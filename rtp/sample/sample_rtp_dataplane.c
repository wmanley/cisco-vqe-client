/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * sample_rtp_dataplane.c
 * 
 * Copyright (c) 2006-2009 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "../../include/sys/event.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include "../../include/utils/vam_types.h"
#include <netinet/ip.h>
#include "sample_rtp_application.h"
#include "sample_rtp_channel.h"
#include "sample_rtp_session.h"
#include "sample_rtp_dataplane.h"
#include "sample_rtp_utils.h"
#include "sample_rtp_script.h"

#define BUF_LEN      RTCP_PAK_SIZE
#define DATA_LEN     100
#define TIMEBUF_LEN   80

/*
 * Data and functions for a sample sender
 */

static sample_sender_t *sample_sender = NULL;

/*
 * Format an RTP packet, based on sender data and current time
 */
static uint32_t format_rtp_packet (sample_sender_t *sender, char* buf,
                                   abs_time_t now, uint32_t *rtp_timestamp)
{
    rtpfasttype_t *rtpheader;    

    rtpheader = (rtpfasttype_t *) buf;
    rtpheader->combined_bits = htons(RTPVERSION << VERSION_SHIFT);
    SET_RTP_PAYLOAD(rtpheader,  RTP_H261);
    rtpheader->sequence = htons(sender->seq);

    if (IS_ABS_TIME_ZERO(sender->send_time)) {
        *rtp_timestamp = rtp_random32(RANDOM_TIMESTAMP_TYPE);
    } else {
        *rtp_timestamp = sender->rtp_timestamp + 
            (uint32_t)(rel_time_to_pcr(TIME_SUB_A_A(now, sender->send_time)));
    }
    rtpheader->timestamp = htonl(*rtp_timestamp);

    rtpheader->ssrc = htonl(sender->ssrc);
    
    return (sizeof(rtpfasttype_t));
}


/*
 * send_rtp_packet
 *
 * Used by a "sender" and by a "translator"
 */
static send_status_t send_rtp_packet (sample_sender_t *sender,
                                      char *buf,
                                      ssize_t buflen,
                                      abs_time_t now,
                                      ssize_t *len_sent)
{
    struct sockaddr_in dest_addr;
    ssize_t len = 0;
    ssize_t sndlen = 0;
    uint32_t hdr_octets = 0;
    char cmdbuf[TOKENSIZ];
    rtp_script_cmd_t cmd;
    rtpfasttype_t *rtpheader = (rtpfasttype_t *)buf;    
    send_status_t status = SEND_FAILED;

    sndlen = buflen;
    if (sender->rtp_script) {
        if (next_rtp_script_cmd(sender->rtp_script, TRUE, /* wrap */
                                cmdbuf, sizeof(cmdbuf)) &&
            parse_rtp_script_cmd(cmdbuf, &cmd)) {
            sndlen = process_rtp_script_cmd((uint8_t *)buf, 
                                            buflen, 
                                            &cmd,
                                            &hdr_octets);
            if (sndlen > RTCP_PAK_SIZE) {
                sndlen = RTCP_PAK_SIZE;
            }
        }
    } 
    if (sndlen) {
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = sender->rtp_addr;
        dest_addr.sin_port = sender->rtp_port;
        len = sendto(sender->fd, buf, sndlen, 0,
                     (struct sockaddr *)&dest_addr,
                     sizeof(struct sockaddr_in));
        if (len == sndlen) {
            sender->packets++;
            /* count only payload octets */        
            sender->bytes += (sndlen - RTPHEADERBYTES(rtpheader));
            sender->rtp_timestamp = ntohl(rtpheader->timestamp);
            sender->seq = ntohs(rtpheader->sequence);
            sender->send_time = now;
            status = SEND_OK;
        } else {
            status = SEND_FAILED;
        }
    } else {
        status = SEND_SKIPPED;
    }

    *len_sent = sndlen;
    return (status);
}

/*
 * Handle expiration of "data send" timer
 */
static void datasend_timeout_cb(int fd, short event, void *arg)
{
    sample_sender_t *sender;
    uint32_t hdr_octets;
    char buf[BUF_LEN];
    ssize_t sndlen = 0;
    abs_time_t now;
    uint32_t rtp_timestamp;
    char timebuf[TIMEBUF_LEN];
    struct timeval tv;
    char *prefix = NULL;
    send_status_t status = SEND_FAILED;
    rtpfasttype_t *rtphdr = (rtpfasttype_t *)buf;

    sender = arg;
    now = get_sys_time();
    hdr_octets = format_rtp_packet(sender, buf, now, &rtp_timestamp);
    memset(buf + hdr_octets, 0, sizeof(fecfasttype_t));

    status = send_rtp_packet(sender, buf, hdr_octets + DATA_LEN, now,
                             &sndlen);

    switch (status) {
    case SEND_OK:
        sender->seq++;
        prefix = "SENT";
        break;
    case SEND_FAILED:
        prefix = "FAILED to send";
        break;
    case SEND_SKIPPED:
        prefix = "SKIPPED sending";
        break;
    }

    if (sender->verbose) {
        printf("%s RTP pkt (%d bytes) at %s:\n",
               prefix, sndlen,
               abs_time_to_str(now, timebuf, sizeof(timebuf)));
        if (sndlen) {
            rtp_print_header((rtptype *)buf, sndlen);
            hdr_octets =  RTPHEADERBYTES(rtphdr);
            rtp_print_fec((fecfasttype_t *)(buf + hdr_octets),
                          sndlen - hdr_octets);
        }
    }

    timerclear(&tv);
    tv = TIME_GET_R(timeval,TIME_MK_R(msec, sender->interval));
    (void)event_add(&sender->timer, &tv);
}

/*
 * Create data sender
 */
void create_sample_sender(sample_config_t *config, uint32_t ssrc, uint16_t seq)
{
    sample_sender_t *sender = NULL;
    struct timeval tv;
    char addr_str[INET_ADDRSTRLEN];

    sender = malloc(sizeof(sample_sender_t));
    if (!sender) {
        sample_sender = NULL;
        return;
    }
    memset(sender, 0, sizeof(sample_sender_t));
    in_port_t src_port = htons(ntohs(config->fbt_port) - 1);
    sender->fd = sample_socket_setup(config->fbt_addr, src_port);
    if (IN_MULTICAST(ntohl(config->rtp_addr))) {
        (void)sample_mcast_output_setup(sender->fd,
                                        config->rtp_addr,
                                        config->if_addr);
    }
    sender->rtp_addr = config->rtp_addr;
    sender->rtp_port = config->rtp_port;
    printf("Sending RTP to %s:%u ...\n",
           uint32_ntoa_r(sender->rtp_addr, addr_str, sizeof(addr_str)),
           ntohs(sender->rtp_port));

    sender->ssrc = ssrc; 
    sender->seq = seq;
    sender->send_time = ABS_TIME_0;
    sender->interval = config->send_interval;

    sender->rtp_script = config->rtp_script;
    sender->verbose = config->verbose;

    evtimer_set(&sender->timer, datasend_timeout_cb, sender);
    timerclear(&tv);
    tv = TIME_GET_R(timeval,TIME_MK_R(msec, sender->interval)) ;
    (void)event_add(&sender->timer, &tv);

    sample_sender = sender;
}

/*
 * Return the data sender
 */
sample_sender_t *get_sample_sender (void) 
{
    return (sample_sender);
}

/*
 * ------------------------------------------
 * Data and functions for a sample translator
 * ------------------------------------------
 */

static sample_sender_t *sample_translator = NULL;

/*
 * Forward ("translate") a data packet.
 */
static void forward_rtp_packet(char *buf, ssize_t buflen)
{
    sample_sender_t *sender;
    abs_time_t now;
    char timebuf[TIMEBUF_LEN];
    char *prefix = NULL;
    ssize_t sndlen = 0;
    uint32_t hdr_octets = 0;
    send_status_t status = SEND_FAILED;
    rtpfasttype_t *rtphdr = (rtpfasttype_t *)buf;

    sender = get_sample_translator();
    now = get_sys_time();

    status = send_rtp_packet(sender, buf, buflen, now, &sndlen);

    switch (status) {
    case SEND_OK:
        sender->seq++;
        prefix = "FWDed";
        break;
    case SEND_FAILED:
        prefix = "FAILED to forward";
        break;
    case SEND_SKIPPED:
        prefix = "SKIPPED forwarding";
        break;
    }

    if (sender->verbose) {
        printf("%s RTP pkt (%d bytes) at %s:\n",
               prefix, sndlen,
               abs_time_to_str(now, timebuf, sizeof(timebuf)));
        if (sndlen) {
            rtp_print_header((rtptype *)buf, sndlen);
            hdr_octets =  RTPHEADERBYTES(rtphdr);
            rtp_print_fec((fecfasttype_t *)(buf + hdr_octets),
                          sndlen - hdr_octets);
        }
    }
}

/*
 * Create data "translator"
 */
void create_sample_translator(sample_config_t *config)
{
    sample_sender_t *sender = NULL;
    char addr_str[INET_ADDRSTRLEN];

    sender = malloc(sizeof(sample_sender_t));
    if (!sender) {
        sample_translator = NULL;
        return;
    }
    memset(sender, 0, sizeof(sample_sender_t));
    in_port_t src_port = 0;
    sender->fd = sample_socket_setup(config->if_addr,
                                     src_port);
    if (IN_MULTICAST(ntohl(config->out_addr))) {
        (void)sample_mcast_output_setup(sender->fd,
                                        config->out_addr,
                                        config->if_addr);
    }
    sender->rtp_addr = config->out_addr;
    sender->rtp_port = config->out_port;
    printf("Forwarding RTP to %s:%u ...\n",
           uint32_ntoa_r(sender->rtp_addr, addr_str, sizeof(addr_str)),
           ntohs(sender->rtp_port));

    sender->ssrc = 0;
    sender->seq = 0;
    sender->send_time = ABS_TIME_0;

    sender->rtp_script = config->rtp_script;
    sender->verbose = config->verbose;

    sample_translator = sender;
}

/*
 * Return the data translator
 */
sample_sender_t *get_sample_translator (void) 
{
    return (sample_translator);
}


/* 
 * ----------------------------------------
 * Data and functions for a sample receiver
 * ----------------------------------------
 */

/*
 * Find per-source data
 */
sample_source_t *get_sample_source (sample_receiver_t *receiver,
                                    rtp_source_id_t *src_id)
{
    sample_source_t *source = NULL;
    rtp_source_id_t *id;

    if (receiver) {
        VQE_LIST_FOREACH(source, &receiver->sources, source_link) {
            id = &source->id;
            if (id->ssrc == src_id->ssrc &&
                id->src_addr == src_id->src_addr &&
                id->src_port == src_id->src_port) {
                break;
            }
        }
    }
    return (source);
}

/*
 * Create per-source data
 */
static sample_source_t *create_sample_source (sample_receiver_t *receiver,
                                              rtp_source_id_t *src_id)
{
    sample_source_t *source;
    
    source = malloc(sizeof(sample_source_t));
    if (!source) {
        return (NULL);
    }
    memset(source, 0, sizeof(sample_source_t));
    source->id = *src_id; /* struct copy */
    source->status = RTP_SUCCESS;
    if (receiver->rtcp_xr_enabled) {
        rtcp_xr_set_size(&source->xr_stats, 1000);
    }
    VQE_LIST_INSERT_HEAD(&receiver->sources, source, source_link);

    return (source);
}

/*
 * Delete per-source data
 */

void delete_sample_source (sample_receiver_t *receiver,
                           rtp_source_id_t *src_id)
{
    sample_source_t *source = get_sample_source(receiver, src_id);

    if (!source) {
        return;
    }
    VQE_LIST_REMOVE(source, source_link);
    free(source);
}

/*
 * Notify control plane of significant events
 */
static rtp_error_t sample_notify_controlplane (sample_receiver_t *receiver,
                                               sample_source_t   *source,
                                               rtp_event_t        event)
{
    rtp_error_t status = RTP_SUCCESS;
    rtp_sample_t *sess = NULL;

    switch (event) {
    case RTP_EVENT_SEQSTART:
        switch (receiver->type) {
        case SAMPLE_EXTERNAL_RECEIVER:
        case SAMPLE_TRANS_RECEIVER:
            VQE_LIST_FOREACH(sess, &(receiver->channel->pri_sessions), link) {
                status = MCALL((rtp_session_t *)sess,
                               rtp_new_data_source,
                               &source->id);
            }
            break;
        case SAMPLE_REPAIR_RECEIVER:
            VQE_LIST_FOREACH(sess, &(receiver->channel->rpr_sessions), link) {
                status = MCALL((rtp_session_t *)sess,
                               rtp_new_data_source,
                               &source->id);
            }
            break;
        case SAMPLE_INTERNAL_RECEIVER:
        case SAMPLE_DUMMY_RECEIVER:
        default:
            status = RTP_SUCCESS;
            break;
        }
        break;
    default:
        status = RTP_SUCCESS;
        break;
    }
    return (status);
}

/*
 * sample_do_drop
 *
 * Drop simulation for a receiver
 *
 * Returns TRUE if a packet should be considered "dropped"
 */
static boolean sample_do_drop (sample_receiver_t *receiver,
                               char *buf, uint32_t len)
{
    rtptype *rtphdr = (rtptype *)buf;

    if (receiver->type != SAMPLE_EXTERNAL_RECEIVER ||
        receiver->drop_intvl == 0) {
        return (FALSE);
    }

    /* 
     * don't drop the first packet, else RTCP doesn't know any
     * subsequent dropped ones are missing; don't drop any of
     * the first 'intvl-1' packets, so we'll always drop 'count'
     * in a row
     */
    if (receiver->received < receiver->drop_intvl) {
        return (FALSE);
    }

    if (receiver->received % receiver->drop_intvl >= receiver->drop_count) {
        return (FALSE);
    }

    receiver->drop_seq_nos[receiver->drop_next_write_idx] =
        ntohs(rtphdr->sequence);
    receiver->drop_next_write_idx++;
    receiver->dropped++;
    if (receiver->drop_next_write_idx == DROP_BUFSIZ) {
        receiver->drop_next_write_idx = 0;
    }

    return (TRUE);
}

/*
 * sample_getnext_drop
 *
 * Return the next dropped sequence number from the circular drop buffer
 */
boolean sample_getnext_drop (sample_receiver_t *receiver,
                             int *next_read_idx,
                             uint16_t *sequence) 
{
    int next = *next_read_idx;

    if (next != receiver->drop_next_write_idx) {
        *sequence = receiver->drop_seq_nos[next];
        next++;
        *next_read_idx = (next == DROP_BUFSIZ) ? 0 : next;
        return (TRUE);
    } else {
        return (FALSE);
    }
}

/* 
 * Per-packet receive function for a "split" ("external") receiver.
 */
static boolean sample_external_receiver (sample_receiver_t *receiver,
                                         char *buf,
                                         uint32_t len,
                                         abs_time_t rcv_ts,
                                         ipaddrtype src_addr,
                                         uint16_t src_port)
{
    rtptype *rtphdr = (rtptype *)buf;
    sample_source_t *source = NULL;
    rtp_hdr_status_t status = RTP_HDR_OK;
    rtp_event_t event = RTP_EVENT_NONE;
    rtp_source_id_t src_id;
    rtcp_xr_stats_t *p_xr_stats = NULL;
    
    if (sample_do_drop(receiver, buf, len)) {
        return (FALSE);
    }

    /*
     * perform basic checks on the header, 
     * abort further processing if it's invalid
     */
    status = rtp_validate_hdr(&receiver->stats, buf, len);
    if (!rtp_hdr_ok(status)) {
        return (FALSE);
    }

    /*
     * find the per-source data for this packet;
     * create a new per-source record if it's not found.
     */
    src_id.ssrc = ntohl(rtphdr->ssrc);
    src_id.src_addr = src_addr;
    src_id.src_port = src_port;

    source = get_sample_source(receiver, &src_id);
    if (!source) {
        source= create_sample_source(receiver, &src_id);
    }
    if (!source) {
        /* failure to create data for a new source */
        return (FALSE);
    }

    /*
     * something similar to the above logic should be repeated
     * for any CSRC values
     */
    /* Set up xr_stats and per_loss_rle */
    if (receiver->rtcp_xr_enabled) {
        p_xr_stats = &source->xr_stats;
    }

    /*
     * Perform sequence number checks,
     * update "last rcvd" timestamps and jitter estimate
     */
    status = rtp_update_seq(&receiver->stats,
                            &source->stats,
                            p_xr_stats,
                            ntohs(rtphdr->sequence));
    if (!rtp_hdr_ok(status)) {
        return (FALSE);
    }
    rtp_update_jitter(&source->stats,
                      p_xr_stats,
                      ntohl(rtphdr->timestamp),
                      rtp_update_timestamps(&source->stats, rcv_ts));

    /*
     * notify control plane, where necessary
     */
    event = rtp_hdr_event(status);
    if (event != RTP_EVENT_NONE) {
        source->status = sample_notify_controlplane(receiver, source, event);
    }
    return (TRUE);
}

/* 
 * Per-packet receive function for a "unified" ("internal") receiver.
 */
static boolean sample_internal_receiver (sample_receiver_t *receiver,
                                         char *buf,
                                         uint32_t len,
                                         abs_time_t rcv_ts,
                                         ipaddrtype src_addr,
                                         uint16_t src_port)
{
    boolean pkt_ok = FALSE;
    rtp_sample_t *sess = NULL;

    VQE_LIST_FOREACH(sess, &(receiver->channel->pri_sessions), link) {
        pkt_ok = MCALL((rtp_session_t *)sess, 
                       rtp_recv_packet, 
                       rcv_ts, src_addr, src_port, buf, len);
    }
    return (pkt_ok);
}

/*
 * Callback to process "data available" event
 */
static void datarecv_cb(int fd, short event, void *arg)
{
    uint32_t len;
    char  buf[BUF_LEN];
    boolean pkt_ok = FALSE;
    abs_time_t rcv_ts;
    struct in_addr src_addr;
    uint16_t src_port;
    sample_receiver_t *receiver;

    receiver = arg;
    len = sizeof(buf);

    if (sample_socket_read(receiver->fd, &rcv_ts, 
                           buf, &len,
                           &src_addr, &src_port)) {
        receiver->received++;
        if (receiver->verbose) {
            rtp_print_envelope(len, src_addr, src_port, rcv_ts);
            rtp_print_header((rtptype *)buf, len);
        }

        switch (receiver->type) {
        case SAMPLE_INTERNAL_RECEIVER:
            pkt_ok = sample_internal_receiver(receiver,
                                              buf, len, 
                                              rcv_ts,
                                              src_addr.s_addr,
                                              src_port);
            break;
        case SAMPLE_EXTERNAL_RECEIVER:
        case SAMPLE_REPAIR_RECEIVER:
            pkt_ok = sample_external_receiver(receiver,
                                              buf, len, 
                                              rcv_ts,
                                              src_addr.s_addr,
                                              src_port);
            break;
        case SAMPLE_TRANS_RECEIVER:
            pkt_ok = sample_external_receiver(receiver,
                                              buf, len, 
                                              rcv_ts,
                                              src_addr.s_addr,
                                              src_port);
            forward_rtp_packet(buf, len);
            break;
        default:
            break;
        }
        // printf("RTP: pkt %sok\n", pkt_ok ? "" : "NOT ");
    }

    (void)event_add(&receiver->event, NULL);
}

/*
 * Create a sample receiver
 */
void create_sample_receiver (sample_config_t *config,
                             sample_receiver_type_t type,
                             sample_channel_t *channel)
{
    sample_receiver_t *receiver;
    in_addr_t recv_addr;
    in_port_t recv_port;
    char addr_str[INET_ADDRSTRLEN];
    char *description = NULL;

    switch (type) {
    case SAMPLE_EXTERNAL_RECEIVER:
    case SAMPLE_INTERNAL_RECEIVER:
    case SAMPLE_TRANS_RECEIVER:
        description = "primary";
        recv_addr = channel->pri_rtp_addr;
        recv_port = channel->pri_rtp_port;
        break;
    case SAMPLE_REPAIR_RECEIVER:
        description = "repair";
        recv_addr = config->if_addr;
        recv_port = 0;
        break;
    case SAMPLE_DUMMY_RECEIVER:
        description = "more rpr";
        recv_addr = config->if_addr;
        recv_port = 0;
        break;
    default:
        return;
    }

    receiver = malloc(sizeof(sample_receiver_t));
    if (!receiver) {
        return;
    }
    memset(receiver, 0, sizeof(sample_receiver_t));
    receiver->type = type;
    receiver->channel = channel;
    receiver->verbose = config->verbose;

    receiver->drop_count = config->drop_count;
    receiver->drop_intvl = config->drop_intvl;

    receiver->fd = sample_socket_setup(recv_addr, recv_port);
    if (IN_MULTICAST(ntohl(recv_addr))) {
        (void)sample_mcast_input_setup(receiver->fd, 
                                       recv_addr,
                                       config->if_addr);
    }
    (void)rtp_get_local_addr(receiver->fd,
                             &receiver->recv_addr,
                             &receiver->recv_port);
    printf("Receiving %s RTP data at %s:%u ...\n",
           description,
           uint32_ntoa_r(receiver->recv_addr, addr_str, sizeof(addr_str)),
           ntohs(receiver->recv_port));

    event_set(&receiver->event, receiver->fd, EV_READ, 
              datarecv_cb, receiver);
    memset(&receiver->stats, 0, sizeof(rtp_hdr_session_t));
    VQE_LIST_INIT(&receiver->sources);

    (void)event_add(&receiver->event, 0);

    switch (type) {
    case SAMPLE_EXTERNAL_RECEIVER:
        if (config->rtcp_xr_enabled) {
            receiver->rtcp_xr_enabled = TRUE;
        }
        /* fall through */
    case SAMPLE_INTERNAL_RECEIVER:
    case SAMPLE_TRANS_RECEIVER:
        if (config->drop_count) {
            fprintf(stderr, "Dropping %d out of %d packets...\n",
                    config->drop_count, config->drop_intvl);
            fprintf(stderr, "Sending %sjittered repair requests...\n",
                    config->drop_jitter ? "" : "un");
        }
        break;
    case SAMPLE_REPAIR_RECEIVER:
        if (config->min_chg_secs) {
            fprintf(stderr, "Min/max RCC backfill: %u-%u msecs ... \n",
                    config->min_rcc_fill, config->max_rcc_fill);
            if (config->maximum_recv_bw) {
                fprintf(stderr, "Maximum recv bandwidth: %u bps ...\n",
                        config->maximum_recv_bw);
            } else {
                fprintf(stderr, "Per-client e-factor is disabled ... \n");
            }
            if (config->do_fastfill) {
                fprintf(stderr, "Fastfill is enabled...\n");
            }
            if (config->maximum_fastfill_time) {
                fprintf(stderr, "Maximum fastfill time: %d.%03d seconds ...\n",
                        config->maximum_fastfill_time / 1000,
                        config->maximum_fastfill_time % 1000);
            }
        }
        break;
    case SAMPLE_DUMMY_RECEIVER:
    default:
        break;
    }

    switch (type) {
    case SAMPLE_EXTERNAL_RECEIVER:
    case SAMPLE_INTERNAL_RECEIVER:
    case SAMPLE_TRANS_RECEIVER:
        channel->pri_rcvr = receiver;
        break;
    case SAMPLE_REPAIR_RECEIVER:
        channel->rpr_rcvr = receiver;
        break;
    case SAMPLE_DUMMY_RECEIVER:
        channel->morerpr_rcvr = receiver;
        break;
    default:
        break;
    }
    return;
}

/*
 * Return a string describing the receiver type
 */
char *sample_receiver_type (sample_receiver_type_t type)
{
    switch (type) {
    case SAMPLE_EXTERNAL_RECEIVER:
        return ("EXTERNAL");
    case SAMPLE_INTERNAL_RECEIVER:
        return ("INTERNAL");
    case SAMPLE_REPAIR_RECEIVER:
        return ("REPAIR");
    case SAMPLE_DUMMY_RECEIVER:
        return ("DUMMY");
    case SAMPLE_TRANS_RECEIVER:
        return ("TRANSLATOR");
    default:
        return ("<unknown>");
    }
}
