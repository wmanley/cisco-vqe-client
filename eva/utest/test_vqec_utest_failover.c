/*
 * Copyright (c) 2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include "vqec_ifclient_defs.h"
#include "vqec_ifclient.h"


/*
 * Timeline events debugging:
 * prints detailed information about timeline events, including
 *    - events generated for VQE-C (packets, API calls)
 *    - events coming from VQE-C (packets read out at each tuner read)
 */
boolean utf_debug_flag_events;
#define VQEC_UTF_DEBUG_EVENTS(arg...)       \
    do {                                    \
        if (utf_debug_flag_events) {        \
            printf(arg);                    \
        }                                   \
    } while (0)

/* 
 * Packet stream debugging:
 * prints a list of all packets (ssrc & seq num) for all packets
 *    - as sent into VQE-C
 *    - as read out from VQE-C
 */
boolean utf_debug_flag_packets;
#define VQEC_UTF_DEBUG_PACKETS(arg...)       \
    do {                                    \
        if (utf_debug_flag_packets) {       \
            printf(arg);                    \
        }                                   \
    } while (0)


extern
void vqec_chan_cfg_printf (vqec_chan_cfg_t *channel_p);

int test_vqec_failover_init (void) {
    return 0;
}

int test_vqec_failover_clean (void) {
    return 0;
}

void *
test_vqec_start (void *arg)
{
    vqec_ifclient_start();
    return (NULL);
}

/* Unit test channel */
vqec_chan_cfg_t chan_cfg;

/*
 * Defines the type of event, i.e.
 *   - arrival of a primary packet, or
 *   - arrival of an RTSP Announce (new source) message
 */
typedef enum utf_event_type_ {
    UTF_EVENT_NONE = 0,
    UTF_EVENT_PAK,
    UTF_EVENT_NEW_SOURCE,
} utf_event_type_t;

typedef enum utf_stream_type_ {
    UTF_STREAM_PRIMARY,
    UTF_STREAM_REPAIR,
} utf_stream_type_t;


/*
 * Repair delay scheduling:
 *
 * In order to test VQE-C's handling of incoming repair packets,
 * these unit tests schedule incoming repairs as part of the test itself,
 * to mimic a repair sent by a VQE-S.  Each unit test verifies the number
 * of repair packets requested during the test.
 *
 * In order for VQE-C to process the received repairs as expected, repairs
 * should be scheduled in a test at the right time (after being requested, 
 * but before the successor packet is scheduled out of the jitter buffer).
 * So some timing constants are defined below for use when determining 
 * how far into the future after a drop a repair should be sent.
 */

/*
 * Default amount of time (in milliseconds) between when a test
 * packet is dropped on the primary stream and when its repair is sent.
 * Incorporates a typical reorder delay and repair trigger point within
 * the jitter buffer (20ms each), as well as 12ms round trip delay. 
 */
#define UTF_REPAIR_DELAY_DEFAULT 52

/*
 * Worst case scenario for VQE-C to recognize and execute a failover.
 * This is derived from 2 * VQEC_DP_DEFAULT_POLL_INTERVAL_MSECS = 40.
 * I.e., VQE-C may not detect that the packetflow source has gone
 * inactive until up to 40ms after its last packet was received.
 */
#define UTF_REPAIR_DELAY_FAILOVER_DETECTION 40


/* 
 * Size of the buffer within unit test packets.
 * The RTP payload includes:
 *   - the original RTP sequence number (2 bytes) [for repair stream packets]
 *   - an source's ssrc (4 bytes)
 *   - the RTP sequence number (2 bytes)
 */
#define UTF_PAK_BUF_LEN_PRIMARY   6
#define UTF_PAK_BUF_LEN_REPAIR    8
#define UTF_PAK_BUF_LEN_OUTPUT    (UTF_PAK_BUF_LEN_PRIMARY)

/*
 * Maximum size of a unit test packet.
 * Includes everything above the UDP layer (RTP header and payload).
 */
#define UTF_PAK_LEN_MAX  \
   (MINRTPHEADERBYTES + max(UTF_PAK_BUF_LEN_PRIMARY, UTF_PAK_BUF_LEN_REPAIR))

typedef struct utf_session_info_ {
    int                   fd;
    struct sockaddr_in    saddr;    
    uint32_t              paks_sent;     /* paks sent on this session */
} utf_session_info_t;

typedef struct utf_stream_ {
    utf_session_info_t    rtp;
    utf_session_info_t    rtcp;
    uint16_t              rtp_seq_num;   /* current RTP seq num (host order) */
} utf_stream_t;

typedef struct utf_src_t_ {
    uint32_t              ssrc;
    utf_stream_t          primary;
    utf_stream_t          repair;
    vqec_chan_cfg_t       chan_cfg;      /* channel cfg specifying this src */
} utf_src_t;

/*
 * Encapsulates the contents of a packet.
 */
typedef struct utf_pak_data_ {
    utf_src_t          *src;             /* source of packet */
    utf_stream_type_t  stream;           /* stream type of packet */
    uint16_t           rtp_seq_num;      /*
                                          * seq num of packet
                                          * (host byte order)
                                          */
    uint32_t           timestamp;        /*
                                          * rtp timestamp
                                          * (host byte order)
                                          */
    uint16_t           rtp_seq_num_orig; /*
                                          * original seq num of packet
                                          * (relevant only for repair paks)
                                          * (host byte order)
                                          */
} utf_pak_data_t;

/*
 * Encapsulates the information associated with an event.
 */
typedef struct utf_event_ {
    utf_event_type_t utf_type;
    union {
        utf_pak_data_t  utf_pak;
        vqec_chan_cfg_t *new_source;
    } utf_data;
} utf_event_t;

/*
 * An event set has a maximum size of 4 events.
 * I.e. no more than 4 unit test events can occur upon a single clock tick.
 */
#define UTF_EVENTS_PER_SET_MAX  4

/*
 * Encapsulates a set of events,
 * where a set is defined as all the events that happen for a given clock
 * tick on the timeline.
 */
typedef struct utf_event_set_ {
    uint32_t num_events;
    utf_event_t utf_event[UTF_EVENTS_PER_SET_MAX];
} utf_event_set_t;


/*
 * Maximum duration of an event timeline and a single timeline clock tick,
 * in milliseconds.
 *
 * For simplicity, keep UTF_TIMELINE_DURATION evenly divisible by 
 * UTF_TICK_DURATION.
 */
#define UTF_TIMELINE_DURATION    6000
#define UTF_TICK_DURATION        4
#define UTF_TIMELINE_TICKS       (UTF_TIMELINE_DURATION/UTF_TICK_DURATION)

/*
 * Defines the increment in RTP timestamp units for each consecutive packet.
 * Assumes a sampling rate of 90kHz and that each RTP packet contains
 * UTF_TICK_DURATION worth of data.
 */
#define UTF_RTP_TIMESTAMP_INCR   (UTF_TICK_DURATION*90000/1000)
 
/*
 * Define an output delay (in milliseconds) to account for the maximum
 * amount of time that could be needed for a packet sent by a source to
 * be read from the sink.
 *
 * It accounts for various delays, such as:
 *    - transmission/network delay (getting to VQE-C's receive socket)
 *    - arrival buffering delay (sitting in VQE-C's receive socket)
 *    - failover buffering delay (sitting in the failover queue,
 *       awaiting an active source to become inactive)
 *    - jitter buffer delay (sitting in the jitter buffer)
 *    - output queue delay (sitting in the tuner's sink)
 *
 * Unit tests which read packets from a tuner queue will continue to read
 * from the tuner queue for this amount of time after an event timeline ends.
 */
#define UTF_TIMELINE_OUTPUT_LAG 2000

/* Max number of iobufs to hold all pak events */
#define UTF_RUN_IOBUFS_MAX  (UTF_TIMELINE_TICKS*UTF_EVENTS_PER_SET_MAX)
typedef struct utf_run_ {
    struct timeval   start_time;     /* start time of test run */
    uint32_t         duration;       /* duration of timeline in use */
    uint32_t         paks_sent;      /* total paks sent in test case */
    utf_event_set_t  timeline[UTF_TIMELINE_TICKS];
                                     /* timeline events */
    vqec_iobuf_t     iobuf[UTF_RUN_IOBUFS_MAX];
                                     /* post-repair stream from VQE-C */
    uint32_t         iobufs_filled;  /* number of iobufs currently filled */
} utf_run_t;

/* Unit test data for a single test run */ 
utf_run_t run;

/* Tuner for unit tests */
vqec_tunerid_t tuner1;


/*
 * Initializes source-specific fields within a channel config.
 *
 * @param[out] chan_cfg      - channel config to initialize
 * @param[in]  src           - source to be used when initializing 
 *                             channel config
 */
void
utf_chan_init_src (vqec_chan_cfg_t *chan_cfg, utf_src_t *src)
{
    /*
     * Primary Stream Source Transport
     */
    chan_cfg->primary_source_addr.s_addr = 
        src->primary.rtp.saddr.sin_addr.s_addr;
    chan_cfg->primary_source_port = 
        (vqec_in_port_t)src->primary.rtp.saddr.sin_port;
    chan_cfg->primary_source_rtcp_port = 
        (vqec_in_port_t)src->primary.rtcp.saddr.sin_port;

    /* Primary Stream Feedback Target */
    chan_cfg->fbt_addr.s_addr = src->primary.rtcp.saddr.sin_addr.s_addr;

    /* Retransmission Stream Source Transport */
    chan_cfg->rtx_source_addr.s_addr =
        src->repair.rtp.saddr.sin_addr.s_addr;
    chan_cfg->rtx_source_port = 
        (vqec_in_port_t)src->repair.rtp.saddr.sin_port;
    chan_cfg->rtx_source_rtcp_port = 
        (vqec_in_port_t)src->repair.rtcp.saddr.sin_port;    
}


/*
 * Initializes a test channel for failover tests.
 *
 * @param[out] chan_cfg  - test channel configuration
 * @param[in]  src       - source for test channel
 */
void
utf_chan_init (vqec_chan_cfg_t *chan_cfg)
{
    int res;

    /*
     * Create a test channel.
     */
    memset(chan_cfg, 0, sizeof(vqec_chan_cfg_t));

    snprintf(chan_cfg->name, VQEC_MAX_CHANNEL_NAME_LENGTH,
             "Failover test channel");

    /* Primary Stream Destination Transport */
    res = vqec_ifclient_socket_open(&chan_cfg->primary_dest_addr,
                                    &chan_cfg->primary_dest_port,
                                    &chan_cfg->primary_dest_rtcp_port);
    CU_ASSERT(res != 0);
    
    /* Retransmission Enable Flags */
    chan_cfg->er_enable = TRUE;

    /* Primary Stream Parameters */
#define MIN_PT 96
    chan_cfg->primary_payload_type = MIN_PT;
    chan_cfg->primary_bit_rate = 4000000;
    chan_cfg->primary_rtcp_sndr_bw = VQEC_RTCP_BW_UNSPECIFIED;
    chan_cfg->primary_rtcp_rcvr_bw = VQEC_RTCP_BW_UNSPECIFIED;
    chan_cfg->primary_rtcp_per_rcvr_bw = VQEC_RTCP_BW_UNSPECIFIED;

    /* Primary Extended Reports */
    chan_cfg->primary_rtcp_xr_loss_rle = VQEC_RTCP_XR_RLE_ENABLE;
    chan_cfg->primary_rtcp_xr_per_loss_rle = VQEC_RTCP_XR_RLE_ENABLE;
    chan_cfg->primary_rtcp_xr_stat_flags = 
        (VQEC_RTCP_XR_STAT_LOSS | 
         VQEC_RTCP_XR_STAT_DUP | 
         VQEC_RTCP_XR_STAT_JITT );

    /* Retransmission Stream Destination Transport */
    res = vqec_ifclient_socket_open(&chan_cfg->rtx_dest_addr,
                                    &chan_cfg->rtx_dest_port,
                                    &chan_cfg->rtx_dest_rtcp_port);
    CU_ASSERT(res != 0);

    /* Retransmission Stream Parameters */
    chan_cfg->rtx_payload_type = MIN_PT;
    chan_cfg->rtx_rtcp_sndr_bw = VQEC_RTCP_BW_UNSPECIFIED;
    chan_cfg->rtx_rtcp_rcvr_bw = VQEC_RTCP_BW_UNSPECIFIED;

    /* Retransmission Extended Reports */
    chan_cfg->rtx_rtcp_xr_loss_rle = VQEC_RTCP_XR_RLE_ENABLE;
}


/*
 * Generates a copy of a channel config.
 *
 * @param[out] chan_cfg_out  - target channel config
 * @param[in]  chan_cfg_in   - source channel config
 */
void
utf_chan_copy (vqec_chan_cfg_t *chan_cfg_out, vqec_chan_cfg_t *chan_cfg_in)
{
    /*
     * Copy all test channel fields.
     */
    memcpy(chan_cfg_out, chan_cfg_in, sizeof(vqec_chan_cfg_t));
}


/*
 * Destroys resources associated with a test channel.
 *
 * @param[in] chan_cfg  - test channel configuration
 */
void
utf_chan_done (vqec_chan_cfg_t *chan_cfg)
{
    /* Free primary stream resources */
    vqec_ifclient_socket_close(chan_cfg->primary_dest_addr,
                               chan_cfg->primary_dest_port,
                               chan_cfg->primary_dest_rtcp_port);
    
    /* Free repair stream resources */
    vqec_ifclient_socket_close(chan_cfg->rtx_dest_addr,
                               chan_cfg->rtx_dest_port,
                               chan_cfg->rtx_dest_rtcp_port);
}

/*
 * Resets a source in preparation for a unit test:
 *  - source's starting sequence numbers for primary and repair sessions
 *     are set to the values provided.
 *  - source's packet counters are reset
 *
 * @param[out]  src                          - source to be reset
 * @param[in]   initial_primary_rtp_seq_num  - starting sequence number 
 *                                              for source's primary stream
 *                                              (host network order)
 * @param[in]   initial_repair_rtp_seq_num   - starting sequence number 
 *                                              for source's repair stream
 *                                              (host network order)
 */
void
utf_src_reset (utf_src_t *src,
               uint16_t initial_primary_rtp_seq_num,
               uint16_t initial_repair_rtp_seq_num)
{
    src->primary.rtp_seq_num = initial_primary_rtp_seq_num;
    src->repair.rtp_seq_num = initial_repair_rtp_seq_num;
    src->primary.rtp.paks_sent = 0;
    src->primary.rtcp.paks_sent = 0;
    src->repair.rtp.paks_sent = 0;
    src->repair.rtcp.paks_sent = 0;
}

/*
 * Creates a socket for sending, binds it to an internally chosen
 * (loopback addr, ephemeral port).
 *
 * returns <int>  fd of the created socket, or -1 if error
 *          addr  address chosen of local binding (network order)
 *          port  port chosen local binding (network order)
 */
boolean
utf_src_session_init (utf_session_info_t *session)
{
    struct sockaddr_in saddr;
    socklen_t saddr_length = sizeof(struct sockaddr_in);
    boolean success = FALSE;

    session->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (session->fd == -1) {
        perror("utf_src_session_init:  error creating socket");
        CU_ASSERT(0);
        goto done;
    }
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    saddr.sin_port = htons(0);
    if (bind(session->fd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
        perror("utf_src_session_init:  error binding socket");
        CU_ASSERT(0);
        goto done;
    }
    if (getsockname(session->fd, 
                    (struct sockaddr *)&saddr, &saddr_length) == -1) {
        perror("utf_src_session_init:  error getting allocated port");
        CU_ASSERT(0);
        goto done;
    }
    session->saddr = saddr;
    success = TRUE;
done:
    return (success);
}

/*
 * Destroys resources associated with a session.
 */
void
utf_src_session_done (utf_session_info_t *session)
{
    close(session->fd);
}

/*
 * Initialize a source that transmits from the loopback interface.
 *
 * Allocates file descriptors representing the RTP and RTCP sessions
 * for the source's primary and repair streams.
 *
 * @param[in/out] src  - source to initialize
 * @param[in]     ssrc - ssrc for source
 */
void
utf_src_init (utf_src_t *src, uint32_t ssrc)
{
    boolean success;
    memset(src, 0, sizeof(utf_src_t));
    
    success = utf_src_session_init(&src->primary.rtp);
    CU_ASSERT(success);
    success = utf_src_session_init(&src->primary.rtcp);
    CU_ASSERT(success);
    success = utf_src_session_init(&src->repair.rtp);
    CU_ASSERT(success);
    success = utf_src_session_init(&src->repair.rtcp);
    CU_ASSERT(success);
    utf_chan_copy(&src->chan_cfg, &chan_cfg);
    utf_chan_init_src(&src->chan_cfg, src);
    src->ssrc = ssrc;
    utf_src_reset(src, 0, 0);
}


/*
 * Destroys resources associated with a test source.
 *
 * @param[in] src  - source to initialize
 */
void
utf_src_done (utf_src_t *src)
{
    utf_src_session_done(&src->primary.rtp);
    utf_src_session_done(&src->primary.rtcp);
    utf_src_session_done(&src->repair.rtp);
    utf_src_session_done(&src->repair.rtcp);
}


/*
 * Sends one packet from a source on its primary or repair stream.
 * Packet's source and rtp sequence number(s) are specified in pak_data.
 *
 * @param[in] pak_data  - describes the packet's data
 */
void
utf_pak_send (utf_pak_data_t *pak_data)
{
    struct sockaddr_in *source_addr, dest_addr;
    char pak_buff[UTF_PAK_LEN_MAX], *buff_ptr;
    rtpfasttype_t *rtp;
    char src_str[INET_ADDRSTRLEN], dst_str[INET_ADDRSTRLEN];
#define OSN_STR_LIMIT  40
    char osn_str[OSN_STR_LIMIT];
    uint16_t rtp_seq_num_orig;
    utf_session_info_t *session_info;


    /* Load dest addresses (and source for debugging output) */
    dest_addr.sin_family = AF_INET;        
    switch (pak_data->stream) {
    case UTF_STREAM_PRIMARY:
        dest_addr.sin_addr = chan_cfg.primary_dest_addr;
        dest_addr.sin_port = chan_cfg.primary_dest_port;
        source_addr = &pak_data->src->primary.rtp.saddr;
        break;
    case UTF_STREAM_REPAIR:
        dest_addr.sin_addr = chan_cfg.rtx_dest_addr;
        dest_addr.sin_port = chan_cfg.rtx_dest_port;
        source_addr = &pak_data->src->repair.rtp.saddr;
        break;
    default:
        CU_ASSERT(FALSE);
        return;
    }

    /* Clear the pak's header and payload */
    memset(pak_buff, 0, UTF_PAK_LEN_MAX);

    /* Build pak's header */
    rtp = (rtpfasttype_t *)pak_buff;
    SET_RTP_VERSION(rtp, RTPVERSION);
    SET_RTP_PADDING(rtp, 0);
    SET_RTP_EXT(rtp, 0);
    SET_RTP_CSRCC(rtp, 0);
    SET_RTP_MARKER(rtp, 0);
    SET_RTP_PAYLOAD(rtp, RTP_MP2T);
    rtp->sequence = htons(pak_data->rtp_seq_num);
    rtp->timestamp = htonl(pak_data->timestamp);
    rtp->ssrc = pak_data->src->ssrc;

    /* 
     * Build pak's payload following the RTP header.
     * Format:
     *   original rtp seq num (2 bytes)  [for repair packets only]
     *   ssrc                 (4 bytes)
     *   rtp seq num          (2 bytes)
     */
    buff_ptr = &pak_buff[MINRTPHEADERBYTES];
    if (pak_data->stream == UTF_STREAM_REPAIR) {
        rtp_seq_num_orig = htons(pak_data->rtp_seq_num_orig);
        memcpy(buff_ptr, &rtp_seq_num_orig, sizeof(uint16_t));
        buff_ptr += sizeof(uint16_t);
    } else {
        rtp_seq_num_orig = rtp->sequence;
    }
    memcpy(buff_ptr, &pak_data->src->ssrc, sizeof(uint32_t));
    buff_ptr += sizeof(uint32_t);
    memcpy(buff_ptr, &rtp_seq_num_orig, sizeof(uint16_t));
    buff_ptr += sizeof(uint16_t);
    
    /*
     * Send the packet.
     */
    src_str[0] = '\0';
    dst_str[0] = '\0';
    if (pak_data->stream == UTF_STREAM_PRIMARY) {
        session_info = &pak_data->src->primary.rtp;
        osn_str[0] = '\0';
    } else {
        session_info = &pak_data->src->repair.rtp;
        snprintf(osn_str, OSN_STR_LIMIT, 
                 " (repair OSN = %u) ", pak_data->rtp_seq_num_orig);
    }
    VQEC_UTF_DEBUG_EVENTS(
        "\n    --> Sending %s pak from (%s:%u [%08x]) to (%s:%u), "
        "rtp_seq_num = %u%s... ",
        pak_data->stream == UTF_STREAM_PRIMARY ? "primary" : "repair",
        inet_ntop(AF_INET, &source_addr->sin_addr, src_str, INET_ADDRSTRLEN),
        ntohs(source_addr->sin_port),
        pak_data->src->ssrc,
        inet_ntop(AF_INET, &dest_addr.sin_addr, dst_str, INET_ADDRSTRLEN),
        ntohs(dest_addr.sin_port),
        pak_data->rtp_seq_num,
        osn_str);
    if (sendto(session_info->fd, pak_buff, 
               MINRTPHEADERBYTES + 
               (pak_data->stream == UTF_STREAM_PRIMARY ? 
                UTF_PAK_BUF_LEN_PRIMARY : UTF_PAK_BUF_LEN_REPAIR),
               0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1) {
        perror("utf_pak_send():  error sending to socket");
        CU_ASSERT(FALSE);
    }
    session_info->paks_sent++;
    run.paks_sent++;
}

/*
 * Generates a VQE-C update source API call for the given source.
 *
 * @param[in] chan_cfg  - chan cfg (includes the new source for the channel)
 */
void
utf_chan_update (vqec_chan_cfg_t *chan_cfg)
{
    vqec_error_t err;

    VQEC_UTF_DEBUG_EVENTS(
        "\n    --> Issuing new source API call with channel config:\n");
    if (utf_debug_flag_events) {
        vqec_chan_cfg_printf(chan_cfg);
    }
    err = vqec_ifclient_chan_update(chan_cfg, NULL);
    CU_ASSERT(err == VQEC_OK);
}

uint16_t
utf_src_get_next_seq (utf_src_t *src, utf_stream_type_t type)
{
    utf_stream_t *stream;

    switch (type) {
    case UTF_STREAM_PRIMARY:
        stream = &src->primary;
        break;
    case UTF_STREAM_REPAIR:
        stream = &src->repair;
        break;
    default:
        CU_ASSERT(FALSE);
        return (0);
    }

    return (stream->rtp_seq_num++);
}

/*
 * Reset the unit test timeline.
 */
void
utf_timeline_reset (void)
{
    uint32_t i, j;
    char *buf_ptr;

    run.duration = UTF_TIMELINE_DURATION;
    run.paks_sent = 0;
    memset(&run.timeline, 0, sizeof(run.timeline));
    for (i=0; i<UTF_TIMELINE_TICKS*UTF_EVENTS_PER_SET_MAX; i++) {
        if (i<UTF_TIMELINE_TICKS) {
            run.timeline[i].num_events = 0;
            for (j = 0;
                 j < UTF_EVENTS_PER_SET_MAX;
                 j++) {
                run.timeline[i].utf_event[j].utf_type = UTF_EVENT_NONE;
            }
        }
        if (!run.iobuf[i].buf_ptr) {
            /* First time around, allocate output buffers */
            buf_ptr = (char *)malloc(UTF_PAK_BUF_LEN_OUTPUT);
            if (buf_ptr) {
                run.iobuf[i].buf_ptr = buf_ptr;
                run.iobuf[i].buf_len = UTF_PAK_BUF_LEN_OUTPUT;
            } else {
                CU_ASSERT(FALSE);
                run.iobuf[i].buf_len = 0;
            }
        }
        run.iobuf[i].buf_wrlen = 0;
        run.iobuf[i].buf_flags = 0;
    }
    run.iobufs_filled = 0;
}


/*
 * Finds the first empty event slot within a given event set.
 *
 * @param[in]  evset  - index of event set
 * @param[out] empty_index - index of empty event slot, or
 *                           UTF_EVENTS_PER_SET_MAX if no empty slots found
 */
void
utf_timeline_find_empty_event (utf_event_set_t *evset,
                               uint32_t *empty_index)
{
    int i;

    /* Find the first empty event slot */
    for (i=0; i<UTF_EVENTS_PER_SET_MAX; i++) {
        if (evset->utf_event[i].utf_type == UTF_EVENT_NONE) {
            break;
        }
    }    

    *empty_index = i;
}

/*
 * Insert a repair stream packet into the timeline.
 *
 * @param[in] src               - identifies source of the packets
 * @param[in] time              - time of the packet transmission (in ms)
 * @param[in] rtp_seq_num_orig  - original sequence number of packet to be
 *                                 repaired (host byte order)
 * @param[in] rtp_timestamp     - original RTP timestamp of the packet to be
 *                                 repaired (host byte order)
 */
void
utf_timeline_insert_repair_pak (utf_src_t *src,
                                uint32_t time, 
                                uint16_t rtp_seq_num_orig,
                                uint32_t rtp_timestamp)
{
    utf_event_set_t *evset;
    uint32_t evnum;

    /* Add repair packet event from the given source into the timeline */
    evset = &run.timeline[time/UTF_TICK_DURATION];
    /* Find an empty slot for the new packet event */
    utf_timeline_find_empty_event(evset, &evnum);
    if (evnum == UTF_EVENTS_PER_SET_MAX) {
        /* Unit test bug--exceeded test case event limit */
        CU_ASSERT(FALSE);
        return;
    }
    /* Add a packet event */
    evset->utf_event[evnum].utf_type = UTF_EVENT_PAK;
    evset->utf_event[evnum].utf_data.utf_pak.src = src;
    evset->utf_event[evnum].utf_data.utf_pak.stream = UTF_STREAM_REPAIR;
    evset->utf_event[evnum].utf_data.utf_pak.rtp_seq_num =
        utf_src_get_next_seq(src, UTF_STREAM_REPAIR);
    evset->utf_event[evnum].utf_data.utf_pak.rtp_seq_num_orig =
        rtp_seq_num_orig;
    evset->utf_event[evnum].utf_data.utf_pak.timestamp = rtp_timestamp;
    evset->num_events++;
}


/*
 * Insert a burst of primary stream packets into the timeline.
 *
 * @param[in] src        - identifies source of the packets
 * @param[in] start_time - starting time of the packet burst (in ms)
 * @param[in] end_time   - duration of the packet burst (in ms).
 *                         One packet will be inserted for each 
 *                         UTF_MS_PER_TICK units of time within this value.
 */
void
utf_timeline_insert_primary_paks (utf_src_t *src,
                                  uint32_t start_time, 
                                  uint32_t end_time)
{
    utf_event_set_t *evset;
    uint32_t i, evnum;
    
    /* Add packet events from the given source into specified timeline range */
    for (i=start_time/UTF_TICK_DURATION; i < end_time/UTF_TICK_DURATION; i++) {
        evset = &run.timeline[i];
        /* Find an empty slot for the new packet event */
        utf_timeline_find_empty_event(evset, &evnum);
        if (evnum == UTF_EVENTS_PER_SET_MAX) {
            /* Unit test bug--exceeded test case event limit */
            CU_ASSERT(FALSE);
            return;
        }
        /* Add a packet event */
        evset->utf_event[evnum].utf_type = UTF_EVENT_PAK;
        evset->utf_event[evnum].utf_data.utf_pak.src = src;
        evset->utf_event[evnum].utf_data.utf_pak.stream = UTF_STREAM_PRIMARY;
        evset->utf_event[evnum].utf_data.utf_pak.rtp_seq_num =
            utf_src_get_next_seq(src, UTF_STREAM_PRIMARY);
        evset->utf_event[evnum].utf_data.utf_pak.timestamp =
            i * UTF_RTP_TIMESTAMP_INCR;
        evset->num_events++;
    }
}


/*
 * Drops (removes) a source's primary stream packet which was previously
 * inserted into the timeline.  A failure is logged if a packet from the
 * given source is not found. 
 *
 * If "repair_delay" is non-zero, a transmission of the dropped packet
 * on the repair stream will be added to the time line "repair_delay"
 * milliseconds after the drop.
 *
 * @param[in]  src          - identifies source whose packet is to be dropped
 * @param[in]  drop_time    - point on the timeline that identifies the packet
 *                             to be dropped
 * @param[in]  repair_delay - if non-zero, defines the number of milliseconds
 *                             into the future that a repair of the dropped
 *                             packet should be sent.
 *                             NOTE:  this should be determined with care,
 *                                    so as to ensure the repair is sent
 *                                    1) after VQE-C requests it, but
 *                                    2) before the lost packet's
 *                                       successor is scheduled out
 */
void
utf_timeline_drop_pak (utf_src_t *src,
                       uint32_t drop_time,
                       uint32_t repair_delay)
{
    uint32_t i, rtp_timestamp = 0;
    uint16_t rtp_seq_num = 0;
    utf_event_set_t *evset;
    boolean found = FALSE;

    /* Load the event set at the specified drop time */
    evset = &run.timeline[drop_time/UTF_TICK_DURATION];
    
    /* Search for a primary packet event from the source to be dropped */
    for (i=0; i<UTF_EVENTS_PER_SET_MAX; i++) {
        if ((evset->utf_event[i].utf_type == UTF_EVENT_PAK) &&
            (evset->utf_event[i].utf_data.utf_pak.src == src) &&
            (evset->utf_event[i].utf_data.utf_pak.stream == 
             UTF_STREAM_PRIMARY)) {
            
            /* 
             * Found it--store the rtp seq number and timestamp, and
             * clear the packet event
             */
            rtp_seq_num = evset->utf_event[i].utf_data.utf_pak.rtp_seq_num;
            rtp_timestamp = evset->utf_event[i].utf_data.utf_pak.timestamp;
            evset->utf_event[i].utf_type = UTF_EVENT_NONE;
            evset->num_events--;
            found = TRUE;
        }
    }

    /* 
     * Flag a failure if there was no packet found
     * (most likely an invalid test case is being constructed).
     */
    if (!found) {
        CU_ASSERT(FALSE);
        return;
    }

    /* Add a retransmission of the dropped packet, if requested */
    if (repair_delay) {
        utf_timeline_insert_repair_pak(src, drop_time + repair_delay,
                                       rtp_seq_num,
                                       rtp_timestamp);
    }
}


/*
 * Updates the test channel's source VQE-C API call to the timeline.
 *
 * @param[in] src          - identifies new source of the channel's packets
 * @param[in] change_time  - point on the timeline at which the API call
 *                           is placed
 */
void
utf_timeline_update_source (utf_src_t *src,
                            uint32_t change_time)
{
    uint32_t evnum;
    utf_event_set_t *evset;

    /* Load the event set at the specified API call time */
    evset = &run.timeline[change_time/UTF_TICK_DURATION];
    
    /* Find an empty slot for the update event */
    utf_timeline_find_empty_event(evset, &evnum);
    if (evnum == UTF_EVENTS_PER_SET_MAX) {
        /* Unit test bug--exceeded test case event limit */
        CU_ASSERT(FALSE);
        return;
    }

    /* Add a new source event */
    evset->utf_event[evnum].utf_type = UTF_EVENT_NEW_SOURCE;
    evset->utf_event[evnum].utf_data.new_source = &src->chan_cfg;
    evset->num_events++;
}


/*
 * Executes a single timeline unit test scenario.
 *
 * @param[in]  test_name  - descriptive name of test
 * @param[out] stats      - VQE-C stats from test run
 */
void
utf_timeline_run (char *test_name,
                  vqec_ifclient_stats_t *stats)
{
    uint32_t i, j, events_processed;
    int32_t bytes_read;
    uint32_t iobufs_filled_current;
    vqec_error_t err;
    utf_event_set_t *evset;
    struct timeval now;
    int64_t sleep_time_us;
    utf_pak_data_t *pak_data;

    printf("\nRunning test:  %s...\n", test_name);

    /* Reset VQE-C stats prior to each test */
    vqec_ifclient_clear_stats();

    /* Bind the tuner to the test channel */
    err = vqec_ifclient_tuner_bind_chan_cfg(tuner1, &chan_cfg, NULL);
    CU_ASSERT(err == VQEC_OK);    

    /*
     * Dump the channel configuration so the dynamically allocated
     * ports can be seen in the test log.
     */
    printf("\nTest channel config:\n");
    vqec_chan_cfg_printf(&chan_cfg);
    
    VQEC_UTF_DEBUG_PACKETS(
        "\n Printing sequence numbers from sent packets...");

    /*
     * Run the test for the duration of the timeline plus jitter buff size.
     * This allows time for all timeline events to be issued, and also
     * sufficient time for any packets in the jitter buffer to be drained.
     */
    gettimeofday(&run.start_time, NULL);
    for (i = 0;
         i < (UTF_TIMELINE_TICKS + UTF_TIMELINE_OUTPUT_LAG/UTF_TICK_DURATION);
         i++) {

        VQEC_UTF_DEBUG_EVENTS("\nTime tick = %u ms...", i * UTF_TICK_DURATION);
        if (i < UTF_TIMELINE_TICKS) {
            /* Generate the timeline's events for the current tick */
            VQEC_UTF_DEBUG_EVENTS("\n  Generating events...");
            evset = &run.timeline[i];
            for (j = 0, events_processed = 0; 
                 events_processed < evset->num_events;
                 j++) {
                /* Error in the unit tests if not all events are found */
                if (j == UTF_EVENTS_PER_SET_MAX) {
                    CU_ASSERT(FALSE);
                    break;
                }
                /*
                 * Process the events in the set.
                 * Since the events are not necessarily contiguous in the
                 * set, we keep scanning the set until all events are found.
                 */
                switch (evset->utf_event[j].utf_type) {
                case UTF_EVENT_NONE:
                    break;
                case UTF_EVENT_PAK:
                    /* Send a pak from the given source */
                    utf_pak_send(&evset->utf_event[j].utf_data.utf_pak);
                    events_processed++;
                    break;
                case UTF_EVENT_NEW_SOURCE:
                    /* Invoke a VQE-C API to update the channel's source */
                    utf_chan_update(evset->utf_event[j].utf_data.new_source);
                    events_processed++;
                    break;
                default:
                    /* not yet supported */
                    CU_ASSERT(FALSE);
                    break;
                }
            }
        }
        
        /*
         * Read out any packets from the jitter buffer every 20ms
         * in non-blocking mode.  The read packets are placed into
         * the iobuf[] array and immediately follow any packets
         * previously read.
         */
        if (((i * UTF_TICK_DURATION) % 20) == 0) {
            VQEC_UTF_DEBUG_EVENTS("\n  Reading packets from VQE-C's tuner...");
            vqec_ifclient_tuner_recvmsg(tuner1,
                                        &run.iobuf[run.iobufs_filled],
                                        UTF_RUN_IOBUFS_MAX - run.iobufs_filled,
                                        &bytes_read,
                                        0);
            iobufs_filled_current = bytes_read / UTF_PAK_BUF_LEN_OUTPUT;
            VQEC_UTF_DEBUG_EVENTS("got %u iobufs (%u bytes)...", 
                                  iobufs_filled_current, bytes_read); 
            for (j = 0; j < iobufs_filled_current; j++) {
                VQEC_UTF_DEBUG_EVENTS(
                    "(%x %u) ", 
                    *(uint32_t *)run.iobuf[run.iobufs_filled+j].buf_ptr,
                    ntohs(*((uint16_t *)
                            ((char *)run.iobuf[run.iobufs_filled+j
                                ].buf_ptr+sizeof(uint32_t)))));
            }
            run.iobufs_filled += iobufs_filled_current;
        }

        /* Sleep until it's time to process the next event set */
        gettimeofday(&now, 0);
        sleep_time_us = 
            ((int64_t)run.start_time.tv_sec - now.tv_sec) * 1000000 + 
            ((int64_t)run.start_time.tv_usec - now.tv_usec) +
            UTF_TICK_DURATION * (i+1) * 1000;
        if (sleep_time_us < 0) {
            VQEC_UTF_DEBUG_EVENTS("\nTimeline is falling behind system clock "
                                  "(sleep time is %lld us)...", sleep_time_us);
        } else {
            VQEC_UTF_DEBUG_EVENTS("\nSleeping for %lld us...", sleep_time_us);
            (void)usleep(sleep_time_us);
        }
        
    }

    /* 
     * Log a warning if the timeline fell behind the system time.
     * This means that events could not be generated fast enough to
     * adhere to the timeline.  This may or may not cause issues.
     */
    if (sleep_time_us < 0) {
        printf("\nWARNING:  Timeline fell behind system clock "
               "(delta is %lld us).", sleep_time_us);
        CU_ASSERT(FALSE);
    }

    /* Unbind the tuner from the test channel */
    err = vqec_ifclient_tuner_unbind_chan(tuner1);
    CU_ASSERT(err == VQEC_OK);

    /* Allow time for the channel's BYE packets to be sent */
    sleep(2);

    /* Dump packet traces if packet debugging is enabled */
    if (utf_debug_flag_packets) {

        VQEC_UTF_DEBUG_PACKETS(
            "\n --- Printing sequence numbers from sent packets...");
        for (i = 0; i < UTF_TIMELINE_TICKS; i++) {
            evset = &run.timeline[i];
            for (j = 0, events_processed = 0; 
                 events_processed < evset->num_events;
                 j++) {
                if (evset->utf_event[j].utf_type == UTF_EVENT_PAK) {
                    pak_data = &evset->utf_event[j].utf_data.utf_pak;
                    VQEC_UTF_DEBUG_PACKETS("\n%x ", pak_data->src->ssrc);
                    if (pak_data->stream == UTF_STREAM_PRIMARY) {
                        VQEC_UTF_DEBUG_PACKETS("%u", pak_data->rtp_seq_num);
                    } else {
                        VQEC_UTF_DEBUG_PACKETS("%u", 
                                               pak_data->rtp_seq_num_orig);
                    }
                }
                if (evset->utf_event[j].utf_type != UTF_EVENT_NONE) {
                    events_processed++;
                }
            }
        }
        VQEC_UTF_DEBUG_PACKETS("\n");
        
        VQEC_UTF_DEBUG_PACKETS(
            "\n --- Printing sequence numbers from %u received packets...",
            run.iobufs_filled);
        for (i = 0; i < run.iobufs_filled; i++) {
            VQEC_UTF_DEBUG_PACKETS("\n%x ", 
                                   *(uint32_t *)run.iobuf[i].buf_ptr);
            VQEC_UTF_DEBUG_PACKETS(
                "%u",
                ntohs(*((uint16_t *)
                        ((char *)run.iobuf[i].buf_ptr+sizeof(uint32_t)))));
        }
        VQEC_UTF_DEBUG_PACKETS("\n ---\n");

    }

    /* Retrieve stats for this test run */
    err = vqec_ifclient_get_stats(stats);
    CU_ASSERT(err == VQEC_OK);
    
}


static void test_vqec_failover_tests (void) 
{
    vqec_ifclient_stats_t stats;
    vqec_error_t err;
    pthread_t tid;
    pthread_attr_t attr;
    utf_src_t src_a, src_b, src_c;
    char *test_name;

    /* Initialize the ifclient library */
    err = vqec_ifclient_init("data/cfg_test_failover.cfg");
    CU_ASSERT(err == VQEC_OK);    
    
    /* Create a tuner called "tuner1" */
    err = vqec_ifclient_tuner_create(&tuner1, "tuner1");
    CU_ASSERT(err == VQEC_OK);

    /* Initialize a test channel which EXCLUDES source information. */
    utf_chan_init(&chan_cfg);

    /* Create some sources. */
    utf_src_init(&src_a, 0xAAAAAAAA);
    utf_src_init(&src_b, 0xBBBBBBBB);
    utf_src_init(&src_c, 0xCCCCCCCC);
    
    /*
     * Initialize the test channel for use with source A.   All unit
     * test cases will assume source A as the initially configured source.
     */
    utf_chan_init_src(&chan_cfg, &src_a);

    /* Spawn a thread and run VQE-C */
    pthread_attr_init(&attr);
    pthread_create(&tid, &attr, test_vqec_start, NULL);
    sleep(3);

    /* set debug flags */
    VQE_RTP_SET_DEBUG_FLAG(RTP_DEBUG_VERBOSE);
    VQE_RTP_SET_DEBUG_FLAG(RTP_DEBUG_PACKETS);
    VQEC_DP_SET_DEBUG_FLAG(VQEC_DP_DEBUG_FAILOVER);
    VQEC_DP_SET_DEBUG_FLAG(VQEC_DP_DEBUG_ERROR_REPAIR);

    utf_debug_flag_events = FALSE;
    utf_debug_flag_packets = FALSE;

    /* run tests */
    printf("\nRunning VQE-C failover tests....\n");

    /*
     * Test Case Legend:
     *   Timeline activity proceeds from left to right.
     *   "CP source" refers to source specified via VQE-C API,
     *     either at bind time or via source update API
     *   "DP source" shows activity on the primary and repair streams from
     *     the given source.  Numbers indicate individual drop/repair packets.
     *
     * NOTE:  All test cases assume an 800ms jitter buffer size.
     */

    /*
     * Test 1.1:  non-overlapping sources:
     *              - primary streams are lossless
     *              - primary streams are ordered
     *              - source sequence spaces don't wrap
     *              - no new source messages
     *              - no repair stream packets sent
     *
     *       
     * time(ms):    0           200    600           5000
     * 
     * CP source:   A
     * DP source: 
     *   A(p)       ---------------
     *   A(r)              
     *   B(p)                          ------------------
     *   B(r)              
     *   C(p)
     *   C(r)
     */
    test_name = "Test 1.1";
    utf_src_reset(&src_a, 0, 0);
    utf_src_reset(&src_b, 0, 0);
    utf_src_reset(&src_c, 0, 0);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 200);
    utf_timeline_insert_primary_paks(&src_b, 600, 5000);
    utf_timeline_run(test_name, &stats);
    /* Verify that all packets sent to VQE-C were successfully read out */
    CU_ASSERT(run.iobufs_filled == run.paks_sent);
    CU_ASSERT(stats.post_repair_outputs == run.paks_sent);
    /* Verify streams were stitched together seamlessly */
    CU_ASSERT(stats.post_repair_losses == 0);

    /*
     * Test 1.2:  non-overlapping sources:
     *              - same as Test 1.1, but source sequence spaces wrap
     *
     *       
     * time(ms):    0           200    600           5000
     * 
     * CP source:   A
     * DP source: 
     *   A(p)       ---------------          
     *   A(r)              
     *   B(p)                          ------------------
     *   B(r)              
     *   C(p)
     *   C(r)
     */
    test_name = "Test 1.2";
    utf_src_reset(&src_a, 65528, 0);  /* primary will wrap */
    utf_src_reset(&src_b, 32000, 0);
    utf_src_reset(&src_c, 0, 0);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 200);
    utf_timeline_insert_primary_paks(&src_b, 600, 5000);
    utf_timeline_run(test_name, &stats);
    /* Verify that all packets sent to VQE-C were successfully read out */
    CU_ASSERT(run.iobufs_filled == run.paks_sent);
    CU_ASSERT(stats.post_repair_outputs == run.paks_sent);
    /* Verify streams were stiched together seamlessly */
    CU_ASSERT(stats.post_repair_losses == 0);

    /*
     * Test 1.3:  non-overlapping sources:
     *              - same as Test 1.2, but primary streams have loss
     *
     *       
     * time(ms):    0           200    600           5000
     * 
     * CP source:   A
     * DP source: 
     *   A(p)       --1--2-----3---          
     *   A(r)              
     *   B(p)                          -4-------5--------
     *   B(r)              
     *   C(p)
     *   C(r)
     */
    test_name = "Test 1.3";
    utf_src_reset(&src_a, 65528, 0);  /* primary will wrap */
    utf_src_reset(&src_b, 32000, 0);
    utf_src_reset(&src_c, 0, 0);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 200);
    utf_timeline_drop_pak(&src_a, 40, 0);        /* drop 1 */
    utf_timeline_drop_pak(&src_a, 80, 0);        /* drop 2 */
    utf_timeline_drop_pak(&src_a, 192, 0);       /* drop 3 */
    utf_timeline_insert_primary_paks(&src_b, 600, 5000);
    utf_timeline_drop_pak(&src_b, 608, 0);       /* drop 4 */
    utf_timeline_drop_pak(&src_b, 3400, 0);      /* drop 5 */
    utf_timeline_run(test_name, &stats);
    /* Verify that all packets sent to VQE-C were successfully read out */
    CU_ASSERT(run.iobufs_filled == run.paks_sent);
    CU_ASSERT(stats.post_repair_outputs == run.paks_sent);
    /* no drops repaired */
    CU_ASSERT(stats.post_repair_losses == 5);
    /* 
     * only request repairs when the packet source matches the config
     * (drops 1,2,3)
     */
    CU_ASSERT(stats.repairs_requested == 3);

    /*
     * Test 1.4:  non-overlapping sources:
     *              - same as Test 1.3,
     *                 but source changed during A's transmission
     *
     * time(ms):    0           200    600           5000
     * 
     * CP source:   A        B
     * DP source:             
     *   A(p)       --1--2-----3---         
     *   A(r)              
     *   B(p)                          -4-------5--------
     *   B(r)              
     *   C(p)
     *   C(r)
     */
    test_name = "Test 1.4";
    utf_src_reset(&src_a, 65528, 0);  /* primary will wrap */
    utf_src_reset(&src_b, 32000, 0);
    utf_src_reset(&src_c, 0, 0);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 200);
    utf_timeline_drop_pak(&src_a, 40, 0);        /* drop 1 */
    utf_timeline_drop_pak(&src_a, 80, 0);        /* drop 2 */
    utf_timeline_update_source(&src_b, 160);
    utf_timeline_drop_pak(&src_a, 192, 0);       /* drop 3 */
    utf_timeline_insert_primary_paks(&src_b, 600, 5000);
    utf_timeline_drop_pak(&src_b, 608, 0);       /* drop 4 */
    utf_timeline_drop_pak(&src_b, 3400, 0);      /* drop 5 */
    utf_timeline_run(test_name, &stats);
    /* Verify that all packets sent to VQE-C were successfully read out */
    CU_ASSERT(run.iobufs_filled == run.paks_sent);
    CU_ASSERT(stats.post_repair_outputs == run.paks_sent);
    /* no drops repaired */
    CU_ASSERT(stats.post_repair_losses == 5);
    /* 
     * only request repairs when the packet source matches the cfg
     * (drops 1,2,4,5)
     */
    CU_ASSERT(stats.repairs_requested == 4);

    /*
     * Test 1.5:  non-overlapping sources:
     *              - same as Test 1.4,
     *                 but drops are repaired by the VQE-S
     *
     * time(ms):    0           200    600           5000
     * 
     * CP source:   A            B
     * DP source:                 
     *   A(p)       --1--2-----3---          
     *   A(r)              1  2              
     *   B(p)                          -4-------5--------
     *   B(r)                             4       5
     *   C(p)
     *   C(r)              
     */
    test_name = "Test 1.5";
    utf_src_reset(&src_a, 65528, 0);  /* primary will wrap */
    utf_src_reset(&src_b, 32000, 0);
    utf_src_reset(&src_c, 0, 0);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 200);
    utf_timeline_drop_pak(&src_a, 40, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_drop_pak(&src_a, 80, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_update_source(&src_b, 160);
    utf_timeline_drop_pak(&src_a, 192, 0);
    utf_timeline_insert_primary_paks(&src_b, 600, 5000);
    utf_timeline_drop_pak(&src_b, 608, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_drop_pak(&src_b, 3400, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_run(test_name, &stats);
    /* Verify that all packets sent to VQE-C were successfully read out */
    CU_ASSERT(run.iobufs_filled == run.paks_sent);
    CU_ASSERT(stats.post_repair_outputs == run.paks_sent);
    /* only drop number 3 unrepaired */
    CU_ASSERT(stats.post_repair_losses == 1);
    /* 
     * Only issue repairs when the pkt source matches the config
     * (drops 1,2,4,5)
     */
    CU_ASSERT(stats.repairs_requested == 4);

    /*
     * Test 1.6:  non-overlapping sources:
     *              - same as Test 1.5,
     *                 but new source changed during the gap
     *
     * time(ms):    0           200    600           5000
     *                                
     * CP source:   A                B    
     * DP source:                         
     *   A(p)       --1--2-----3---          
     *   A(r)              1  2  3           
     *   B(p)                          -4-------5--------
     *   B(r)                             4       5
     *   C(p)
     *   C(r)              
     */
    test_name = "Test 1.6";
    utf_src_reset(&src_a, 65528, 0);  /* primary will wrap */
    utf_src_reset(&src_b, 32000, 0);
    utf_src_reset(&src_c, 0, 0);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 200);
    utf_timeline_drop_pak(&src_a, 40, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_drop_pak(&src_a, 80, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_drop_pak(&src_a, 192, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_update_source(&src_b, 500);
    utf_timeline_insert_primary_paks(&src_b, 600, 5000);
    utf_timeline_drop_pak(&src_b, 608, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_drop_pak(&src_b, 3400, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_run(test_name, &stats);
    /* Verify that all packets sent to VQE-C were successfully read out */
    CU_ASSERT(run.iobufs_filled == run.paks_sent);
    CU_ASSERT(stats.post_repair_outputs == run.paks_sent);
    /* all drops repaired */
    CU_ASSERT(stats.post_repair_losses == 0);
    /* repairs requested for all drops */
    CU_ASSERT(stats.repairs_requested == 5);

    /*
     * Test 1.7:  non-overlapping sources:
     *              - same as Test 1.6,
     *                 but new source changed during B's transmission
     *                 (Note:  new source change arrives early enough
     *                         so that its information is available by
     *                         the time the reorder delay has expired
     *                         for drop 4)
     *
     * time(ms):    0           200    600           5000
     *                                      
     * CP source:   A                         B
     * DP source:                               
     *   A(p)       --1--2-----3---          
     *   A(r)              1  2  3           
     *   B(p)                          -4-------5--------
     *   B(r)                             4       5
     *   C(p)
     *   C(r)              
     */
    test_name = "Test 1.7";
    utf_src_reset(&src_a, 65528, 0);  /* primary will wrap */
    utf_src_reset(&src_b, 32000, 0);
    utf_src_reset(&src_c, 0, 0);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 200);
    utf_timeline_drop_pak(&src_a, 40, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_drop_pak(&src_a, 80, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_drop_pak(&src_a, 192, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_insert_primary_paks(&src_b, 600, 5000);
    utf_timeline_drop_pak(&src_b, 608, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_update_source(&src_b, 612);
    utf_timeline_drop_pak(&src_b, 3400, UTF_REPAIR_DELAY_DEFAULT);
    utf_timeline_run(test_name, &stats);
    /* Verify that all packets sent to VQE-C were successfully read out */
    CU_ASSERT(run.iobufs_filled == run.paks_sent);
    CU_ASSERT(stats.post_repair_outputs == run.paks_sent);
    /* All drops repaired */
    CU_ASSERT(stats.post_repair_losses == 0);
    /* Repairs requested for all drops */
    CU_ASSERT(stats.repairs_requested == 5);

    /*
     * Test 2.1:  overlapping sources:
     *              - primary streams are lossless
     *              - primary streams are ordered
     *              - source sequence spaces don't wrap
     *              - no new source messages
     *              - no repair stream packets sent
     *       
     * time(ms):    0      1800   2000               5000
     *                                
     * CP source:   A
     * DP source: 
     *   A(p)       ------------------          
     *   A(r)              
     *   B(p)                ----------------------------
     *   B(r)              
     *   C(p)
     *   C(r)              
     */
    test_name = "Test 2.1";
    utf_src_reset(&src_a, 0, 0);
    utf_src_reset(&src_b, 0, 0);
    utf_src_reset(&src_c, 0, 0);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 2000);
    utf_timeline_insert_primary_paks(&src_b, 1800, 5000);
    utf_timeline_run(test_name, &stats);
    /* Verify that all packets sent to VQE-C were successfully read out */
    CU_ASSERT(run.iobufs_filled == run.paks_sent);
    CU_ASSERT(stats.post_repair_outputs == run.paks_sent);
    /* All drops repaired */
    CU_ASSERT(stats.post_repair_losses == 0);
    /* Repairs requested for all drops */
    CU_ASSERT(stats.repairs_requested == 0);

    /*
     * Test 2.2:  overlapping sources:
     *              - same as Test 2.1, but source sequence spaces wrap,
     *                primary streams are lossy,
     *                source update occurs during the overlap, and
     *                drops are repaired by the VQE-S
     *
     * time(ms):    0      1800   2000               5000
     *                                
     * CP source:   A              B
     * DP source:
     *   A(p)       ---1--------2-----            
     *   A(r)            1        2 
     *   B(p)                --3-------4-------5---------
     *   B(r)                          3 4       5
     *   C(p)
     *   C(r)              
     */
    test_name = "Test 2.2";
    utf_src_reset(&src_a, 65528, 0);  /* primary will wrap */
    utf_src_reset(&src_b, 65528, 0);  /* primary will wrap (in failover q) */
    utf_src_reset(&src_c, 0, 0);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 2000);
    utf_timeline_drop_pak(&src_a, 200, UTF_REPAIR_DELAY_DEFAULT);  /* drop 1 */
    utf_timeline_drop_pak(&src_a, 1900, UTF_REPAIR_DELAY_DEFAULT); /* drop 2 */
    utf_timeline_insert_primary_paks(&src_b, 1800, 5000);
    utf_timeline_drop_pak(&src_b, 1840,                            /* drop 3 */
                          UTF_REPAIR_DELAY_DEFAULT+
                          UTF_REPAIR_DELAY_FAILOVER_DETECTION+
                          200 /* max source overlap from this test case */);
    utf_timeline_update_source(&src_b, 1980);
    utf_timeline_drop_pak(&src_b, 2100,                            /* drop 4 */
                          UTF_REPAIR_DELAY_DEFAULT+
                          UTF_REPAIR_DELAY_FAILOVER_DETECTION+
                          200 /* max source overlap from this test case */);
    utf_timeline_drop_pak(&src_b, 3000,                            /* drop 5 */
                          UTF_REPAIR_DELAY_DEFAULT+
                          UTF_REPAIR_DELAY_FAILOVER_DETECTION+
                          200 /* max source overlap from this test case */);
    utf_timeline_run(test_name, &stats);
    /* Verify that all packets sent to VQE-C were successfully read out */
    CU_ASSERT(run.iobufs_filled == run.paks_sent);
    CU_ASSERT(stats.post_repair_outputs == run.paks_sent);
    /* All drops repaired */
    CU_ASSERT(stats.post_repair_losses == 0);
    /* Repairs requested for all drops */
    CU_ASSERT(stats.repairs_requested == 5);


    /*
     * Test 3.1:  closely aligned sources:
     *              - primary streams are lossy
     *              - primary streams are ordered
     *              - source sequence spaces are unsynchronized/random
     *              - NO new source message
     *                                
     * time(ms):    0             2000               5000
     *
     * CP source:   A
     * DP source:                           
     *   A(p)       ---1--------2-----  
     *   A(r)            1        2 
     *   B(p)                         -3-------4---------
     *   B(r)                                     
     *   C(p)
     *   C(r)              
     */
    test_name = "Test 3.1";
    utf_src_reset(&src_a, 43201, 32004);
    utf_src_reset(&src_b, 12303, 8303);
    utf_src_reset(&src_c, 0, 0);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 2000);
    utf_timeline_drop_pak(&src_a, 520, UTF_REPAIR_DELAY_DEFAULT);  /* drop 1 */
    utf_timeline_drop_pak(&src_a, 1700, UTF_REPAIR_DELAY_DEFAULT); /* drop 2 */
    utf_timeline_insert_primary_paks(&src_b, 2000, 5000);
    utf_timeline_drop_pak(&src_b, 2004, 0);   /* drop 3 */
    utf_timeline_drop_pak(&src_b, 4040, 0);   /* drop 4 */
    utf_timeline_run(test_name, &stats);
    /* Verify that all packets sent to VQE-C were successfully read out */
    CU_ASSERT(run.iobufs_filled == run.paks_sent);
    CU_ASSERT(stats.post_repair_outputs == run.paks_sent);
    /* Two drops repaired */
    CU_ASSERT(stats.post_repair_losses == 2);
    /* Repairs requested for some drops */
    CU_ASSERT(stats.repairs_requested == 2);  /* 
                                               * only for drops 1 and 2,
                                               * others don't match the source
                                               */

    /*
     * Test 3.2:  closely aligned sources:
     *              - same as test 3.1, but with 
     *                 o new source message upon failover
     *                 o stray source C pror to failover
     *                   (should be ignored after period of inactivity)
     *
     * time(ms):    0             2000               5000
     *                                
     * CP source:   A                 B
     * DP source:
     *   A(p)       ---1--------2-----
     *   A(r)            1        2
     *   B(p)                         -3-------4---------
     *   B(r)                            3       4
     *   C(p)          --
     *   C(r)              
     */
    test_name = "Test 3.2";
    utf_src_reset(&src_a, 43201, 32004);
    utf_src_reset(&src_b, 12303, 8303);
    utf_src_reset(&src_c, 0, 0);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 2000);
    utf_timeline_drop_pak(&src_a, 520, UTF_REPAIR_DELAY_DEFAULT);  /* drop 1 */
    utf_timeline_drop_pak(&src_a, 1700, UTF_REPAIR_DELAY_DEFAULT); /* drop 2 */
    utf_timeline_insert_primary_paks(&src_c, 500, 600);
    utf_timeline_update_source(&src_b, 2000);
    utf_timeline_insert_primary_paks(&src_b, 2000, 5000);
    utf_timeline_drop_pak(&src_b, 2004,
                          UTF_REPAIR_DELAY_DEFAULT+
                          UTF_REPAIR_DELAY_FAILOVER_DETECTION);    /* drop 3 */
    utf_timeline_drop_pak(&src_b, 4040,
                          UTF_REPAIR_DELAY_DEFAULT+
                          UTF_REPAIR_DELAY_FAILOVER_DETECTION);    /* drop 4 */
    utf_timeline_run(test_name, &stats);
    /*
     * Verify that all packets sent to VQE-C, except those sent by source C,
     * were successfully read out
     */
    CU_ASSERT(run.iobufs_filled == 
              (run.paks_sent - src_c.primary.rtp.paks_sent));
    CU_ASSERT(stats.post_repair_outputs == 
              (run.paks_sent - src_c.primary.rtp.paks_sent));
    /* All drops repaired */
    CU_ASSERT(stats.post_repair_losses == 0);
    /* Repairs requested for all drops */
    CU_ASSERT(stats.repairs_requested == 4);

    /*
     * Test 4.1:  multiple failovers, overlapping and non-overlapping sources:
     *              - primary streams are lossless
     *              - primary streams are ordered
     *              * DP source: sequence spaces arbitrarily chosen
     *              * new source messages announce each change
     *       
     * time(ms):    0   1000   2000   3000   4000    5000
     *                                
     * CP source:   A          B                  C
     * DP source: 
     *   A(p)       ------1--------
     *   A(r)               1          
     *   B(p)                  ---------2--------
     *   B(r)                             2
     *   C(p)                                    ---3---
     *   C(r)                                         3   
     */
    test_name = "Test 4.1";
    utf_src_reset(&src_a, 16302, 31317);
    utf_src_reset(&src_b, 51441, 11628);
    utf_src_reset(&src_c, 29011, 62921);
    utf_timeline_reset();
    utf_timeline_insert_primary_paks(&src_a, 0, 2000);
    utf_timeline_drop_pak(&src_a, 1000, UTF_REPAIR_DELAY_DEFAULT); /* drop 1 */
    utf_timeline_update_source(&src_b, 1800);
    utf_timeline_insert_primary_paks(&src_b, 1800, 4000);
    utf_timeline_drop_pak(&src_b, 3000, UTF_REPAIR_DELAY_DEFAULT); /* drop 2 */
    utf_timeline_update_source(&src_c, 4000);
    utf_timeline_insert_primary_paks(&src_c, 4000, 5000);
    utf_timeline_drop_pak(&src_c, 4500, UTF_REPAIR_DELAY_DEFAULT); /* drop 3 */
    utf_timeline_run(test_name, &stats);
    /* Verify that all packets sent to VQE-C were successfully read out */
    CU_ASSERT(run.iobufs_filled == run.paks_sent);
    CU_ASSERT(stats.post_repair_outputs == run.paks_sent);
    /* All drops repaired */
    CU_ASSERT(stats.post_repair_losses == 0);
    /* Repairs requested for all drops */
    CU_ASSERT(stats.repairs_requested == 3);

    VQEC_DP_SET_DEBUG_FLAG(VQEC_DP_DEBUG_FAILOVER);

    /* Stop VQE-C */
    vqec_ifclient_stop();
    err = pthread_join(tid, NULL);
    CU_ASSERT(err == 0);
    
    /* Deallocate channel and source resources (e.g. sockets) */
    utf_chan_done(&chan_cfg);
    utf_src_done(&src_a);
    utf_src_done(&src_b);
    utf_src_done(&src_c);

}

CU_TestInfo test_array_failover[] = {
    {"test vqec_failover_tests", test_vqec_failover_tests},
    CU_TEST_INFO_NULL,
};
