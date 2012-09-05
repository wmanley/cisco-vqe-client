/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008-2011 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: DP API types that are shared across the second-level API files.
 *
 * Documents: 
 *
 *****************************************************************************/

#ifndef __VQEC_DP_API_TYPES_H__
#define __VQEC_DP_API_TYPES_H__

#include "vqec_dp_common.h"
#include <vqec_seq_num.h>
#include <rtp_header.h>

#include "mp_mpeg.h"  /* for length of PSI section */
#include "../vqec_version.h"    /* included for version number support */

#include "vqec_ifclient_read.h"
#include "../vqec_tr135_defs.h"

/**
 * An API exported via IPC mechanisms.  
 */
#define RPC
/**
 * Input parameter passed by reference.  
 */
#define INR
/**
 * Input parameter passed by value.  
 */
#define INV
/**
 * Output parameter, always passed by reference. 
 */
#define OUT
/**
 * Optional output parameter, always passed by reference. 
 */
#define OUT_OPT
/**
 * Input and output parameters, passed by reference.
 */
#define INOUT

/**
 * Input array type.
 *
 * NOTE:  The current RPC implementation permits only one instance of
 *        INARRAY() per function prototype.
 */
#define INARRAY(type, maxlimit)                 \
    type *__fil, uint32_t __count

/**
 * Output array type.
 *
 * NOTE:  The current RPC implementation permits only one instance of
 *        OUTARRAY() per function prototype.
 */
#define OUTARRAY(type, maxlimit)                        \
    type *_fil, uint32_t _count, uint32_t *_used


/**
 * API version.
 * Note: This assumes that VQEC_PRODUCT_VERSION, VQEC_MAJOR_VERSION,
 *       and VQEC_MINOR_VERSION will never exceed 127 (7 bits each). 
 *       VQEC_BUILD_NUMBER should never exceed 2048 (11 bits). 
 */
#define VQEC_DP_API_VERSION                                     \
    ((VQEC_PRODUCT_VERSION<<25) | (VQEC_MAJOR_VERSION<<18) |    \
     (VQEC_MINOR_VERSION<<11)   | (VQEC_VAR_BUILD_NUMBER))

/**
 * API version type.
 */
typedef uint32_t vqec_dp_api_version_t;

/**
 * TunerID type.
 */
typedef int32_t vqec_dp_tunerid_t;

/**
 * Input or Output stream identifiers (may be passed to the control plane).
 */
#define VQEC_DP_STREAMID_INVALID (0)
typedef int32_t vqec_dp_streamid_t;

/**
 * Graph identifiers
 */
#define VQEC_DP_GRAPHID_INVALID (0)
typedef int32_t vqec_dp_graphid_t;

/**
 * Data plane channel identifiers
 */
#define VQEC_DP_CHANID_INVALID (0)
typedef uint32_t vqec_dp_chanid_t;

/**
 * Data plane operating modes.
 */
typedef
enum vqec_dp_op_mode_
{
    /**
     * User space operating mode.
     */
    VQEC_DP_OP_MODE_USERSPACE = 0,
    /**
     * Kernel space operating mode.
     */
    VQEC_DP_OP_MODE_KERNELSPACE
} vqec_dp_op_mode_t;

/**
 * Dataplane errors. All control-to-dataplane API's must return these
 * error types.
 */
typedef 
enum vqec_dp_error_
{
    /**
     * No error.
     */
    VQEC_DP_ERR_OK = 0,
    /**
     * No memory resources.
     */
    VQEC_DP_ERR_NOMEM,
    /**
     * Bad input argument.
     */
    VQEC_DP_ERR_INVALIDARGS,
    /**
     * Module already initialied.
     */
    VQEC_DP_ERR_ALREADY_INITIALIZED,
    /**
     * Internal error.  Typically, this means that additional information
     * should be available by turning on debugs or looking at syslogs.
     */
    VQEC_DP_ERR_INTERNAL,
    /**
     * Resource already exists.
     */
    VQEC_DP_ERR_EXISTS,
    /**
     * Resource cannot be allocated.
     */
    VQEC_DP_ERR_NO_RESOURCE,
    /**
     * Resource cannot be found.
     */
    VQEC_DP_ERR_NOT_FOUND,
    /**
     * Problem in connecting graph.
     */
    VQEC_DP_ERR_GRAPHCONNECT,
    /**
     * Module is in shutdown state.
     */
    VQEC_DP_ERR_SHUTDOWN,
    /**
     * No such stream.
     */
    VQEC_DP_ERR_NOSUCHSTREAM,
    /**
     * Stream limit reached.
     */
    VQEC_DP_ERR_NOMORESTREAMS,
    /**
     * No such tuner.
     */
    VQEC_DP_ERR_NOSUCHTUNER,
    /**
     * There is an IS mapped to the tuner.
     */
    VQEC_DP_ERR_ISMAPPEDTOTUNER,
    /**
     * No IRQ is pending.
     */
    VQEC_DP_ERR_NOPENDINGIRQ,
    /**
     * Bad RTP header.
     */
    VQEC_DP_ERR_BADRTPHDR,
    /**
     * No resource available to add RTP Header
     */
    VQEC_DP_ERR_NO_RESOURCE_FOR_RTP_HEADER,
    /**
     * No RPC support for this function call.
     */
    VQEC_DP_ERR_NO_RPC_SUPPORT,
    /**
     * Invalid APP packet has been provided.
     */
    VQEC_DP_ERR_INVALID_APP,
    /**
     * Add more error types prior to VQEC_DP_ERR_MAX below.
     */
    VQEC_DP_ERR_MAX   /* MUST BE LAST */
} vqec_dp_error_t;

/**
 * Private TR-135 Data Structures
 */
typedef enum vqec_tr135_stats_error_state_ {
    VQEC_STATS_STATE_OK = 0,
    VQEC_STATS_STATE_ENTER_LOSS_EVENT,
}  vqec_tr135_stats_error_state_t;

typedef enum vqec_tr135_stats_ec_ {
    VQEC_TR135_STATS_BEFORE_EC = 0,
    VQEC_TR135_STATS_AFTER_EC,
} vqec_tr135_stats_ec_t;

/* 
 * Data structure to maintain counters for TR-135 sample statistics. 
 * The reason for creating a data variables for these counters is because
 * the behavior of "sample" statistics is different than "total counters". 
 * Sample stats start being updated only between the start and end of a 
 * 'sample interval'. 
 */
typedef struct vqec_tr135_stream_sample_stats_t_ {
    uint64_t maximum_loss_period;
    uint64_t minimum_loss_distance;
} vqec_tr135_stream_sample_stats_t;

typedef struct vqec_tr135_stream_total_stats_t_ {
    uint64_t mld_low_water_mark;
    uint64_t minimum_loss_distance;     /*!< min. number of packets between  >*/
                                        /*!< two consecutive loss events     >*/
    uint64_t maximum_loss_period;       /*!< max. number of rtp packets of   >*/
                                        /*!< the longest loss event          >*/
    uint64_t mlp_high_water_mark;       /*!< Not a TR135 parameter. For      >*/
                                        /*!< CLI 'cumulative' command        >*/

    uint64_t packets_received;          /*!< total packets received          >*/
    uint64_t packets_lost;              /*!< packets lost                    >*/
    uint64_t packets_expected;          /*!< expected = recvd + lost         >*/
    uint64_t loss_events;
    uint64_t severe_loss_index_count;

    vqec_tr135_stats_error_state_t state;
                                        /*!< state machine variable used in  >*/
                                        /*!<vqec_pcm_update_tr135_stats>*/
    uint64_t num_good_consec_packets;   /*!< number of consecutive good pkts >*/
    uint64_t num_bad_consec_packets;    /*!< number of consecutive bad pkts  >*/
    uint32_t last_pak_seq;
    uint8_t start_flag;
    uint64_t loss_period;
   
} vqec_tr135_stream_total_stats_t;

typedef struct vqec_tr135_mainstream_stats_ {
    vqec_ifclient_tr135_params_t tr135_params;

    /* ServiceMonitoring.Mainstream.Total.Dejittering stats */
    uint64_t overruns;
    uint64_t underruns;

    /* ServiceMonitoring.Mainstream.Total Stats */
    vqec_tr135_stream_total_stats_t total_stats_before_ec;
    vqec_tr135_stream_total_stats_t total_stats_after_ec;

    /* ServiceMonitoring.Mainstream.Sample_Stats */
    vqec_tr135_stream_sample_stats_t sample_stats_before_ec;
    vqec_tr135_stream_sample_stats_t sample_stats_after_ec;

} vqec_tr135_mainstream_stats_t;

typedef struct vqec_tr135_stats_ {
    /* Frontend.dejittering stats */
    uint64_t buffer_size;
    /** 
     * TR-135 Mainstream Stats, refering to the statistics for a AV stream
     * being sent to a main stream
     */
    vqec_tr135_mainstream_stats_t mainstream_stats;
} vqec_tr135_stats_t;

/**
 * Returns a descriptive error string corresponding to an vqec_dp_err_t
 * error code.  If the error code is invalid, an empty string is returned.
 *
 * @param[in] err           Error code
 * @param[out] const char * Descriptive string corresponding to error code
 */ 
static inline const char
 *vqec_dp_err2str (vqec_dp_error_t err)
{
    switch (err) {
        case VQEC_DP_ERR_OK:
            return "Success";
        case VQEC_DP_ERR_NOMEM:
            return "Memory resources not available";
        case VQEC_DP_ERR_INVALIDARGS:
            return "Invalid input arguments";
        case VQEC_DP_ERR_ALREADY_INITIALIZED:
            return "Module already initialized";
        case VQEC_DP_ERR_INTERNAL:
            return "Internal error";
        case VQEC_DP_ERR_EXISTS:
            return "Resource already exists";
        case VQEC_DP_ERR_NO_RESOURCE:
            return "Resource cannot be allocated";
        case VQEC_DP_ERR_NOT_FOUND:
            return "Resource cannot be found";
        case VQEC_DP_ERR_GRAPHCONNECT:
            return "Problem in connecting graph";
        case VQEC_DP_ERR_SHUTDOWN:
            return "Service is not running";
        case VQEC_DP_ERR_NOSUCHSTREAM:
            return "Stream not found";
        case VQEC_DP_ERR_NOMORESTREAMS:
            return "Stream limit reached";
        case VQEC_DP_ERR_NOSUCHTUNER:
            return "Tuner not found";
        case VQEC_DP_ERR_ISMAPPEDTOTUNER:
            return "There is an input stream mapped to the tuner";
        case VQEC_DP_ERR_NOPENDINGIRQ:
            return "No IRQ event is pending"; 
        case VQEC_DP_ERR_BADRTPHDR:
            return "Bad RTP header";
        case VQEC_DP_ERR_NO_RESOURCE_FOR_RTP_HEADER:
            return "Resource cannot be allocated for adding a RTP Header";
        case VQEC_DP_ERR_NO_RPC_SUPPORT:
            return "No RPC support for this function call";
        case VQEC_DP_ERR_INVALID_APP:
            return "Invalid APP information has been given";
        case VQEC_DP_ERR_MAX:
            return "";
    }

    /* error code is invalid */
    return "";
}

/**
 * Dataplane encapsulation types - used to specify input / output 
 * encapsulation on input / output streams.
 */
typedef
enum vqec_dp_encap_type_
{
    /**
     * Unknown encapsulation.
     */
    VQEC_DP_ENCAP_UNKNOWN,
    /**
     * RTP encapsulation.
     */
    VQEC_DP_ENCAP_RTP,
    /**
     * UDP encapsulation.
     */
    VQEC_DP_ENCAP_UDP,
    
} vqec_dp_encap_type_t;


/**
 * Input stream types.
 */
typedef
enum vqec_dp_input_stream_type_
{
    /**
     * Primary.
     */
    VQEC_DP_INPUT_STREAM_TYPE_PRIMARY = 0,
    /**
     * Repair.
     */
    VQEC_DP_INPUT_STREAM_TYPE_REPAIR,
    /**
     * FEC (both column and row).
     */
    VQEC_DP_INPUT_STREAM_TYPE_FEC,
    
} vqec_dp_input_stream_type_t;

/**
 * Input filter protocol type. A single protocol, i.e. UDP is currently defined.
 */
typedef
enum vqec_dp_input_filter_proto_
{
    /**
     * User Datagram Protocol.
     */
    INPUT_FILTER_PROTO_UDP,  
    /**
     * Must be last.
     */
    INPUT_FILTER_PROTO_MAX,

} vqec_dp_input_filter_proto_t;

/**
 * Input filter specification.
 */
typedef
struct vqec_dp_input_filter_
{
    /**
     * Protocol type - UDP is the only supported type. DCCP support may be
     * added in the future.
     */
    vqec_dp_input_filter_proto_t proto;

    union {

        /**
         * IPv4 filter address specification.
         */
        struct {

            /**
             * Destination IP address.
             * If INADDR_ANY, then behavior will be implementation dependent.
             */
            in_addr_t dst_ip;

            /**
             * Destination interface's IP address.
             * Used ONLY when dst_ip is multicast.
             */
            in_addr_t dst_ifc_ip;

            /**
             * Destination IP port. The following rules apply:
             * (a) Destination IP port must be non-0 if the destination 
             *     IP address a multicast address.
             * (b) If the destination IP address is unicast, and the port is 0,
             *     the caller expects the input shim to allocate an unused 
             *     local port for the requested protocol, which is then 
             *     reported back to the caller. This would in all probability
             *     force an implementation of the input shim to use a socket
             *     abstraction since port assignments for IP protocols are 
             *     integrally linked to the socket abstraction.
             * (c) This port number should be in network byte-order.
             */
            in_port_t dst_port;

            /**
             * If TRUE, the source IP address field (next field), is used to
             * filter inbound traffic, i.e., that source IP address in the 
             * IP header must match the src_ip field.
             */
            boolean src_ip_filter;
            
            /**
             * Source IP address. The following rules apply:
             * (a) The argument is considered valid, when src_ip_filter
             *     (prev field) is set to TRUE.
             * (b) Unicast only.
             * (c) A value of INADDR_ANY or 0.0.0.0 is ignored.
             */
            in_addr_t src_ip;

            /**
             * If TRUE, the source port field (next field),  is used to filter
             * inbound traffic, i.e., that source port in the IP header must
             * match the src_port field.
             */
            boolean src_port_filter;
            
            /**
             * Source IP port. The following rules apply:
             * (a) The argument is considered valid when src_port_filter
             *     (prev field) is set to TRUE.
             * (b) A value of 0 is ignored.
             */
            uint16_t src_port;

            /**
             * Dummy destination IP address.  Must be a multicast address.
             */
            in_addr_t rcc_extra_igmp_ip;
            
        } ipv4;

    } u;

} vqec_dp_input_filter_t;

#define VQEC_DP_SCHEDULING_CLASSES_MAX  32
#define VQEC_DP_DEFAULT_POLL_INTERVAL_MSECS 20
#define VQEC_DP_DEFAULT_POLL_INTERVAL \
    MSECS(VQEC_DP_DEFAULT_POLL_INTERVAL_MSECS)
#define VQEC_DP_NUM_SCHEDULING_CLASSES 2
#define VQEC_DP_FAST_SCHEDULE 0
#define VQEC_DP_FAST_SCHEDULE_INTERVAL_MSEC 20
#define VQEC_DP_SLOW_SCHEDULE 1
#define VQEC_DP_SLOW_SCHEDULE_INTERVAL_MSEC 40
/**
 * Scheduling policy for all classes of streams in the dataplane, 
 * e.g. the class of all primary RTP streams, FEC RTP streams, etc.
 * The structure below defines:
 *    1) max number of supported scheduling classes
 *    2) polling interval for each scheduling class.  This interval
 *       indicates the amount of time desired between instances in which
 *       a class is serviced, in milliseconds.  (Array index 0 represents
 *       the first scheduling class, and all scheduling classes up to the
 *       max must be given a non-zero scheduling interval.)
 */
typedef struct vqec_dp_class_sched_policy_ {
    uint8_t    max_classes;
    uint16_t   polling_interval[VQEC_DP_SCHEDULING_CLASSES_MAX];
} vqec_dp_class_sched_policy_t;

typedef
enum vqec_dp_chan_sched_policy_
{
    /**
     * Free-run based on RTP timestmaps, i.e., no NLL.
     */
    VQEC_DP_CHAN_SCHED_FREE_RUN,
    /**
     * Run with RTP timestmaps and NLL tracking loop.
     */
    VQEC_DP_CHAN_SCHED_WITH_NLL,

} vqec_dp_chan_sched_policy_t;

/**
 * Stream description structure.
 */
typedef struct vqec_dp_input_stream_ {
    /**
     * Encapsulation type for this stream (RTP only).
     */
    vqec_dp_encap_type_t encap;   
    /**
     * Input filter for the stream.
     */
    vqec_dp_input_filter_t filter;
    /**
     * RTP payload type.
     */
    int32_t rtp_payload_type;
    /**
     * Expected datagram size (in bytes)
     */
    uint16_t datagram_size;
    /**
     * Size of the socket receive buffer.
     */
    uint32_t so_rcvbuf;
    /**
     * Scheduling class for the socket polling.
     */
    uint32_t scheduling_class;
    /**
     * Maximum RLE size for RTCP XR.
     */
    uint32_t rtcp_xr_max_rle_size;
    /**
     * Maximum RLE size for RTCP post-ER XR.
     */
    uint32_t rtcp_xr_post_er_rle_size;
    /**
     * DSCP value to be used for transmissions to the stream's source.
     * In practice, this only applies when sending STUN messages
     * to the remote peer (source) for the repair stream on the VQE-S.
     * The field name is chosen to match the system configuration
     * parameter--no RTCP packets are actually sent by the data plane.
     */
    uint8_t rtcp_dscp_value;

} vqec_dp_input_stream_t;

/* define FEC sending order */

typedef enum vqec_fec_sending_order_ {
    VQEC_DP_FEC_SENDING_ORDER_NOT_DECIDED = 0,
    VQEC_DP_FEC_SENDING_ORDER_ANNEXA,
    VQEC_DP_FEC_SENDING_ORDER_ANNEXB,
    VQEC_DP_FEC_SENDING_ORDER_OTHER,
} vqec_fec_sending_order_t;

typedef struct vqec_fec_info_ {
    uint8_t                  fec_l_value;
    uint8_t                  fec_d_value;
    vqec_fec_sending_order_t fec_order;
    rel_time_t               rtp_ts_pkt_time;   
                            /* per pkt time in usec based on RTP Timestamp*/
} vqec_fec_info_t;

/**
 * Dataplane channel configuration descriptor.
 * [More parameters may be needed in this structure]
 */
typedef struct vqec_dp_chan_desc_
{   
    /**
     * Opaque Control-plane channel handle.
     */
    uint32_t cp_handle;

    /**
     * If TRUE, directly connect the input shim OS's 
     * to the output shim IS's  - used for fallback mode.
     */
    boolean fallback;
    /**
     * If true, then this is a non-VQE channel that should be passed through
     * the client, and reordering is performed if RTP.
     */
    boolean passthru;
    /**
     * Average time elapsed between receiving each single packet.
     */
    rel_time_t avg_pkt_time;
    /**
     * Time interval for triggering repairs.
     */
    rel_time_t repair_trigger_time;
    /**
     * Reorder hold-time for primary packets.
     */
    rel_time_t reorder_time;
    /**
     * Network jitter buffer which absorbs the effect of
     * of reorders, and retranmission latency.
     */
    rel_time_t jitter_buffer;
    /**
     * Maximum amount of the jitter buffer that can be filled during and after
     * an RCC burst.
     */
    rel_time_t max_backfill;
    /**
     * Minimum amount of the jitter buffer that can be filled during and after
     * an RCC burst, in order to allow other services (FEC, ER) to work.
     */
    rel_time_t min_backfill;
    /**
     * Maximum amount of data that can be fast filled during a memory optimized
     * RCC burst.
     */
    rel_time_t max_fastfill;
    /**
     * Default bound on the FEC block size in packets, before
     * repair is requested.  
     */ 
     uint16_t fec_default_block_size; 
    /**
     * L, D and sending order information from channel CFG/DB.  
     */
    vqec_fec_info_t fec_info;
    /**
     * TRUE, if RCC is enabled on this channel.
     */
    boolean en_rcc;
    /**
     * Maximum rate (in bps) of the channel. For FEC enabled
     * channels, the stream must be CBR.
     */
    int32_t max_rate;    
    /**
     * TRUE, if output stream should be UDP (strip RTP headers).
     */
    int32_t strip_rtp;
    /**
     * Channel's scheduling policy.
     */
    vqec_dp_chan_sched_policy_t sch_policy;

    /**
     * Primary stream's description - this must be present.
     */
    vqec_dp_input_stream_t primary;

    /**
     * TRUE, if the repair stream is valid / to-be constructed.
     */
    boolean en_repair;
    /**
     * Repair stream's description.
     */
    vqec_dp_input_stream_t repair;
    /**
     * TRUE, if the FEC 0 stream (column or row) is valid / to-be constructed.
     */
    boolean en_fec0;
    /**
     * FEC column stream's description.
     */
    vqec_dp_input_stream_t fec_0;
    /**
     * TRUE, if the FEC 1 stream (row or column) is valid / to-be constructed.
     */
    boolean en_fec1;
    /**
     * FEC repair stream's description.
     */
    vqec_dp_input_stream_t fec_1;
    /** 
     * TR-135 control parameters. 
     */
    vqec_ifclient_tr135_params_t tr135_params;

} vqec_dp_chan_desc_t;

/**
 * Buffer contains an APP packet, set in the iobuf.buf_flags field.
 */
#define VQEC_DP_BUF_FLAGS_APP (0x2)

/**
 * Limit on the number of sources allowed per RTP session.
 */
#define VQEC_DP_RTP_MAX_KNOWN_SOURCES 3

/**
 * Sequence number history buffer size.
 */
#define VQEC_DP_RTP_SRC_SEQ_NUM_HIST_ENTRIES 16

/**
 * RTP receiver source key.
 */
typedef 
struct vqec_dp_rtp_src_key_ 
{
    /**
     * SSRC. 
     */
    uint32_t ssrc;

    union
    {
        /**
         * IPv4 attributes.
         */
        struct 
        {
            /**
             * Source IP address.
             */
            struct in_addr src_addr;                
            /**
             * Source port.
             */
            uint16_t src_port;
        } ipv4;
    } ;
        
} vqec_dp_rtp_src_key_t;

/**
 * RTP receiver source states: applicable only for primary sessions.
 */
typedef
enum vqec_dp_rtp_src_state_
{
    /**
     * Source is receiving data traffic, i.e., at least one packet
     * has been received in the last activity timeout period. 
     */
    VQEC_DP_RTP_SRC_ACTIVE = 1,
    /**
     * Source is not receiving data traffic, i.e.,  no packets
     * have been received in the last activity timeout period.
     * Inactive state is used only for the case of primary sources.
     */
    VQEC_DP_RTP_SRC_INACTIVE,
 
} vqec_dp_rtp_src_state_t;

/**
 * Dynamic information about an RTP source.
 */
typedef
struct vqec_dp_rtp_src_entry_info_
{
    /**
     * RTP source statistics - a structure exported by the RTP library.
     */
    rtp_hdr_source_t src_stats;
    /**
     * Source state - will be set to stationary for non-primary sessions.
     */
    vqec_dp_rtp_src_state_t state;
    /**
     * Allow packetflow on this source. Will be true if the SSRC for the 
     * source is allowed to accept packets and tx them to succeeding elements.
     * The concept of such a source will apply to both primary and non-primary
     * sessions. In the case of non-primary sessions this field
     * will simply indicate if received packets are / will be accepted 
     * from this source.
     */
    boolean pktflow_permitted;
    /**
     * Queue/buffer the most recent packets from this source for use
     * in the event that the "pktflow_permitted" source becomes inactive.
     * Packets are buffered for failover from at most one source at a time,
     * and only for primary sessions.
     */
    boolean buffer_for_failover;
    /**
     * Threshold crossings experienced, i.e., active-inactive-active transitions.
     */
    uint32_t thresh_cnt;
    /**
     * Time at which first packet received.
     */
    abs_time_t first_rx_time;
    /**
     * Time at which last packet received.
     */
    abs_time_t last_rx_time;
    /**
     * Number of CSRC entries for this source.
     */
    uint8_t csrc_count;
    /**
     * CSRC entry array for the source.
     */
    uint32_t csrcs[MAX_CSRCC];
    /**
     * Packets received.
     */
    uint64_t packets;
    /**
     * Bytes received.
     */
    uint64_t bytes;
    /**
     * Packets dropped.
     */
    uint64_t drops;
    /**
     * Session RTP sequence number offset.
     *
     * This delta relates a packet's external (source-supplied) RTP sequence
     * number with a virtual, session-based RTP sequence number that is
     * source independent.  As the sources of a channel changes, the offset
     * is used to map packets from a new source such that it immediately
     * follows the highest sequence number received from the old source. 
     */
    int16_t session_rtp_seq_num_offset;
    /**
     * Received sequence number history buffer - debug only.
     */
    uint16_t seq_num_hist[VQEC_DP_RTP_SRC_SEQ_NUM_HIST_ENTRIES];
    /**
     * Index in history buffer of earliest entry.
     */
    uint16_t seq_num_hist_start;
    /**
     * Index of the next new entry in the history buffer.
     */    
    uint16_t seq_num_hist_next;


} vqec_dp_rtp_src_entry_info_t;

/**
 * Dynamic info about an RTP session.
 */
typedef
struct vqec_dp_rtp_sess_info_
{
    /**
     * Session statistics - the structure is exported by the RTP 
     * library API.
     */
    rtp_hdr_session_t sess_stats;

} vqec_dp_rtp_sess_info_t;

/**
 * Subset of the rtp source which is exported to the control
 * plane in an array. 
 */
typedef
struct vqec_dp_rtp_src_table_entry_
{
    /**
     * Key.
     */
    vqec_dp_rtp_src_key_t key;
    /**
     * Dynamic Info.
     */    
    vqec_dp_rtp_src_entry_info_t info;
} vqec_dp_rtp_src_table_entry_t;

/**
 * Source table, used to export the state of all known sources
 * to the control-plane, for one RTP receiver. 
 */
typedef
struct vqec_dp_rtp_src_table_
{
    /**
     * Num of entries in the array.
     */
    uint32_t num_entries;
    /**
     * The source table entry array.
     */
    vqec_dp_rtp_src_table_entry_t sources[VQEC_DP_RTP_MAX_KNOWN_SOURCES];

} vqec_dp_rtp_src_table_t;


/**
 * Drop interval or ratio specifications.
 */
typedef
struct vqec_dp_drop_desc_
{
    /**
     * Percentage
     * When <drop_percent> is non-zero, randomly selected packets are 
     * dropped such that this percentage <drop_percent> is equal to an
     * approximate cumulative percentage of dropped packets.
     */
    uint32_t drop_percent;
    /**
     * Continuous/Interval
     * These parameters are ignored when <drop_percent> is non-zero.  
     * In this mode, for every <interval_span> packets that are received, 
     * the first <num_cont_drops> of these packets are dropped.
     */
    uint32_t num_cont_drops;
    /**
     * Window or span of drip interval.
     */
    uint32_t interval_span;

} vqec_dp_drop_desc_t;


/**
 * Drop interval settings that are in-use in the dataplane.
 */
typedef
struct vqec_dp_drop_sim_state_
{
    /**
     * Is simulation enabled.
     */
    boolean en;
    /**
     * Drop descriptor.
     */
    vqec_dp_drop_desc_t desc;

} vqec_dp_drop_sim_state_t;


/**
 * Devices that can generate upcall events.
 */
typedef 
enum vqec_dp_upcall_device_
{
    /**
     * Invalid device handle.
     */
    VQEC_DP_UPCALL_DEV_INVALID,
    /**
     * A channel's primary rtp receiver device.
     */
    VQEC_DP_UPCALL_DEV_RTP_PRIMARY,
    /**
     * A channel's repair rtp receiver device.
     */
    VQEC_DP_UPCALL_DEV_RTP_REPAIR,
    /**
     * Events on the channel itself.
     */
    VQEC_DP_UPCALL_DEV_DPCHAN,

} vqec_dp_upcall_device_t;

#define VQEC_DP_UPCALL_DEV_MIN VQEC_DP_UPCALL_DEV_RTP_PRIMARY
#define VQEC_DP_UPCALL_DEV_MAX VQEC_DP_UPCALL_DEV_DPCHAN

/**
 * Upcall IRQ event descriptor.
 */
typedef
struct vqec_dp_upcall_irq_event_
{
    /**
     * Generation or sequence number for the upcall event.
     */
    uint32_t upcall_generation_num;
    /**
     * Channel identifier of the channel which owns this IRQ.
     */
    vqec_dp_chanid_t chid;
    /**
     * A control-plane speciifc opaque handle for the channel.
     */
    uint32_t cp_handle;
    /**
     * A channel generation number. This is helpful because the same IRQ
     * number / same vqe channel number may be reused after a channel
     * change, in which case there will be no way to disambiguate an IRQ
     * for the two "sources", i.e., the vqe channel just destroyed, and the 
     * new vqe channel created.
     */
    uint32_t chan_generation_num;
    /**
     * A channel may have multiple IRQ lines, in which case when the 
     * a device-id may be associated with the IRQ. The device is
     * specified as either "channel", "rtp primary" or "rtp repair". 
     */
    vqec_dp_upcall_device_t device;
    /**
     * This field is specific to "rtp primary" or "rtp repair" devices,
     * and contains their dataplane identifiers.
     */
    vqec_dp_streamid_t device_id;

} vqec_dp_upcall_irq_event_t;

/**
 * Upcall event reason codes. Reason registers are maintained per device,
 * and thus the "reason code" space can be defined on a per device basis.
 * However, since we have a few reason codes, the reason code space is
 * defined on a global basis. The reason register is a bitmask of multiple
 * reason codes that may have been asserted by the device.
 *
 * NOTE: With the current API, upto 32 reason codes are possible.
 */
typedef
enum vqec_dp_upcall_irq_reason_code_
{    
    /**
     * Invalid reason code.
     */
    VQEC_DP_UPCALL_REASON_INVALID,
    /**
     * RTP source transition from inactive-to-active.
     */
    VQEC_DP_UPCALL_REASON_RTP_SRC_ISACTIVE,
    /**
     * RTP source transition from active-to-inactive.
     */
    VQEC_DP_UPCALL_REASON_RTP_SRC_ISINACTIVE,
    /**
     * New RTP source.
     */
    VQEC_DP_UPCALL_REASON_RTP_SRC_NEW,
    /**
     * CSRC update on existing source.
     */
    VQEC_DP_UPCALL_REASON_RTP_SRC_CSRC_UPDATE,
    /**
     * A RCC NCSI message needs to be sent via RTCP.
     */
    VQEC_DP_UPCALL_REASON_CHAN_RCC_NCSI,
    /**
     * A RCC abort event.
     */
    VQEC_DP_UPCALL_REASON_CHAN_RCC_ABORT,
    /**
     * A fast fill done event.
     */
    VQEC_DP_UPCALL_REASON_CHAN_FAST_FILL_DONE,
    /**
     * A burst done event.
     */
    VQEC_DP_UPCALL_REASON_CHAN_BURST_DONE,
    /**
     * A FEC update event.
     */
    VQEC_DP_UPCALL_REASON_CHAN_FEC_UPDATE,
   /**
     * Primary inactive event.
     */
    VQEC_DP_UPCALL_REASON_CHAN_PRIM_INACTIVE,

    /**
     * Event used to periodically synchronize generation (sequence) numbers.
     */
    VQEC_DP_UPCALL_REASON_CHAN_GEN_NUM_SYNC,
    /**
     * Must be last.
     */
    VQEC_DP_UPCALL_REASON_MAX
    
} vqec_dp_upcall_irq_reason_code_t;

#define VQEC_DP_UPCALL_MAKE_REASON(code) (1 << ((code) - 1))
#define VQEC_DP_UPCALL_REASON_ISSET(reason, code) \
    ((reason) & (1 << ((code) -1)))

/**
 * NCSI upcall event data structure.
 */
typedef 
struct vqec_dp_upc_ncsi_data_
{
    /**
     * First primary seq on the channel.
     */
    uint32_t first_primary_seq;
    /**
     * First primary receive timestamp.
     */
    abs_time_t first_primary_rx_ts;
    /**
     * Join latency.  
     */
    rel_time_t join_latency;
    /**
     * Time at which ER is to be enabled.
     */
    abs_time_t er_enable_ts;    

} vqec_dp_upc_ncsi_data_t;

/**
 * fast fill upcall event data structure.
 */
typedef 
struct vqec_dp_upc_fast_fill_
{
    /**
     * total number of fast fill bytes scheduled
     * out to decoder
     */
    uint32_t total_fast_fill_bytes;

    /**
     * Time at which fast fill started.
     */
    abs_time_t fast_fill_start_time;
    /**
     * Time at which fast fill ended.
     */
    abs_time_t fast_fill_end_time;    

} vqec_dp_upc_fast_fill_data_t;

/**
 * FEC update upcall event data structure
 */
typedef
struct vqec_dp_upc_fec_update_data_
{
    /*
     * L-value of FEC
     */
    uint32_t fec_l_value;
    /*
     * D-value of FEC
     */
    uint32_t fec_d_value;
} vqec_dp_upc_fec_update_data_t;

/**
 * IRQ event acknowledgement response.
 */
typedef
struct vqec_dp_irq_ack_resp_
{
    /**
     * Acknowledgement error - valid *only* when the poll IRQ method is used.
     */
    vqec_dp_error_t ack_error;
    /**
     * Reason code register which is a bitmask composed of all the reasons
     * which casued the IRQ to be triggered.
     */
    uint32_t irq_reason_code;  
    /**
     * This field is specific to "rtp primary" or "rtp repair" devices,
     * and contains their dataplane identifiers. The field is reflected here 
     * since the structure is also used for "poll" of IRQs if they are lost.
     */
    vqec_dp_streamid_t device_id;
    /**
     * Union of all data that may be collected within a single ack over the  
     * different devices - for now this is limited to the source table.
     */
    union 
    {
        /**
         * Source table collected for "rtp primary" and "rtp repair" devices.
         */
        struct 
        {
            vqec_dp_rtp_src_table_t src_table; 
        } rtp_dev;

        /**
         * Attributes collected for a channel device.
         */
        struct
        {            
            vqec_dp_upc_ncsi_data_t ncsi_data;
            vqec_dp_upc_fast_fill_data_t fast_fill_data;
            vqec_dp_upc_fec_update_data_t fec_update_data;
        } chan_dev;

    } response;

} vqec_dp_irq_ack_resp_t;


/**
 * This data structure is used to poll the IRQ's for all devices
 * on a single channel. It is used for the case where some upcall's may be
 * lost to the control-plane.
 */
typedef
struct vqec_dp_irq_poll_
{
    /**
     * Channel identifier of the channel which owns this IRQ.
     */
    vqec_dp_chanid_t chid;
    /**
     * A control-plane speciifc opaque handle for the channel.
     */
    uint32_t cp_handle;
    /**
     * A channel generation number. This is helpful because the same IRQ
     * number / same vqe channel number may be reused after a channel
     * change, in which case there will be no way to disambiguate an IRQ
     * for the two "sources", i.e., the vqe channel just destroyed, and the 
     * new vqe channel created.
     */
    uint32_t chan_generation_num;    
    /**
     * Generation or sequence number in the dataplane when the poll data is 
     * copied / returned.
     */
    uint32_t upcall_generation_num;
    /**
     * Responses captured from all the devices.
     */
    vqec_dp_irq_ack_resp_t response[VQEC_DP_UPCALL_DEV_MAX];

} vqec_dp_irq_poll_t;


/**
 * Report one gap to control-plane.
 */
typedef 
struct vqec_dp_gap_ 
{
    /**
     * a 32-bit sequence number for the start of the gap.
     */
     uint32_t start_seq;
    /**
     * The extent of the gap.
     */
    uint32_t extent;

} vqec_dp_gap_t;

/**
 * Gaps that will fit in a single buffer.
 */
#define VQEC_DP_GAP_BUFFER_MAX_GAPS 32

/**
 * Aggregate gap reporting buffer.
 */
typedef
struct vqec_dp_gap_buffer_
{
    /**
     * Number of gaps reported in the list.
     */
    uint32_t num_gaps;
    /**
     * Gap buffer.
     */
    vqec_dp_gap_t gap_list[VQEC_DP_GAP_BUFFER_MAX_GAPS];

} vqec_dp_gap_buffer_t;

/**
 * Data structure for passing PSI sections (PAT/PMT) across IPC.
 */
typedef
struct vqec_dp_psi_section_
{
    /**
     * Length of PSI section used.
     */
    uint32_t len;
    /**
     * PSI section data.
     */
    uint8_t data[MP_PSISECTION_LEN];
} vqec_dp_psi_section_t;


/**
 * These are defined in a macro-ized fashion so that they have a "flat"
 * structure, and the field list can be extended locally to the dataplane,
 * where more fields can be added for initialization.  
 */
#define VQEC_DP_MODULE_INIT_FIELDS              \
    /**                                         \
     * Packet pool size (number of packets).    \
     */                                                                 \
    uint32_t pakpool_size;                                              \
                                                                        \
    /**                                                                 \
     * Maximum number of tuners to support (i.e. the the max number of  \
     * channels that may need to be tuned simultaneously).              \
     */                                                                 \
    uint32_t max_tuners;                                                \
                                                                        \
    /**                                                                 \
     * Maximum packet payload size, including RTP header and MPEG payload \
     * (but excluding UDP layer and below).                             \
     */                                                                 \
    uint32_t max_paksize;                                               \
                                                                        \
    /*                                                                  \
     * Defines the limits of how many channels and how many output streams \
     * per channel the input shim must support.  Although the total limit of \
     * output streams is bound by only the product of these two numbers, \
     * the limit is accepted as the product of these two factors so that the \
     * implementation may optimize its memory allocation.               \
     */                                                                 \
    uint32_t max_channels;                                              \
    uint32_t max_streams_per_channel;                                   \
                                                                        \
    /**                                                                 \
     * Output packet queue limit (number of packets).                   \
     */                                                                 \
    uint32_t output_q_limit;                                            \
                                                                        \
    /**                                                                 \
     * Maximum iobuf array length for reading.                          \
     */                                                                 \
    uint32_t max_iobuf_cnt;                                             \
                                                                        \
    /**                                                                 \
     * Maximum amount of time (in msecs) before tuner_read times out.   \
     */                                                                 \
    uint32_t iobuf_recv_timeout;                                        \
                                                                        \
    /**                                                                 \
     * Number of APP packets to send out at the beginning of each RCC.  \
     */                                                                 \
    uint32_t app_paks_per_rcc;                                          \
                                                                        \
    /**                                                                 \
     * Interpacket output delay between replicated APP packets.         \
     */                                                                 \
    uint32_t app_cpy_delay;                                             \

/**
 * Initialization parameters for the dataplane: The MODULE_INIT_FIELDS
 * are filled in the by the control-plane while the rest of the fields in this
 * structure are filled in by the dataplane.
 */
typedef
struct vqec_dp_module_init_params_
{
    VQEC_DP_MODULE_INIT_FIELDS
    
    /*----
     * Dataplane written fields.
     *----*/

    /**
     * Overall polling interval for running the input shim, and
     * for running the dataplane channels output schedulers.
     */
    rel_time_t polling_interval;

    /**
     * Class-based polling intervals for servicing classes within the
     * input shim.
     *
     * Note that the ability of the input shim to adhere to these intervals
     * is constrained by the "polling_interval" parameter above.
     * For example, if the polling_interval is 20ms, then class service
     * times will actually align themselves with intervals 20ms (e.g. 20ms,
     * 40ms, etc.).
     */
    vqec_dp_class_sched_policy_t scheduling_policy;

} vqec_dp_module_init_params_t;


/**
 * Packet header for ejected packets to control-plane.
 */
typedef
struct vqec_dp_pak_hdr_
{       
    /**
     * Channel identifier of the channel from which the packet is ejected.
     */
    vqec_dp_chanid_t dp_chanid;
    /** 
     *Corresponding control-plane channel handle.
     */
    uint32_t cp_handle;
    /**
     * Stream identifier of the stream on which the packet was received.
     */
    vqec_dp_streamid_t is_id;
    /**
     * Timestmap at which the packet was received.
     */
    abs_time_t rx_timestamp;
    /**
     * Source address from which pak received.
     */
    in_addr_t src_addr;
    /**
     * Source port from which pak received.
     */
    in_port_t src_port;
    /**
     * Packet content length.
     */
    uint16_t length;

} vqec_dp_pak_hdr_t;

typedef struct vqec_dp_display_ifilter_ {
    vqec_dp_input_filter_t filter;         /* filter values */
    uint32_t scheduling_class;             /* when to process filter */
} vqec_dp_display_ifilter_t;


typedef struct vqec_dp_display_os_ {
    uint32_t os_id;               /* handle ID for this OS */
    vqec_dp_encap_type_t encaps;        /* stream encapsulation type */
    int32_t capa;                       /* capabilities of this OS */
    boolean has_filter;                 /* has associated filter? */
    vqec_dp_display_ifilter_t filter;   /* associated filter */
    uint32_t is_id;                     /* connected IS (if connected) */

    uint64_t packets;                   /* transmitted packets */
    uint64_t bytes;                     /* transmitted bytes */
    uint64_t drops;                     /* output drops */
} vqec_dp_display_os_t;

typedef struct vqec_dp_display_is_ {
    uint32_t id;          /*!< Input stream id */
    uint32_t capa;              /*!< Input stream capabilities */
    vqec_dp_encap_type_t encap; /*!< Input stream encapsulation type */
    uint32_t os_id;                     /* connected OS (if connected) */

    uint64_t packets;                   /* received packets */
    uint64_t bytes;                     /* received bytes */
    uint64_t drops;                     /* input drops */
} vqec_dp_display_is_t;

typedef struct vqec_dp_pcm_status_ {
    vqec_seq_num_t head;  /*!< the smallest seq num */
    vqec_seq_num_t tail;  /*!< the largest seq num */
    boolean er_enable;    /*!< error repair enable/disable */
    vqec_seq_num_t highest_er_seq_num;  /*!< highest seq_num for gap report */
    vqec_seq_num_t last_requested_er_seq_num; /*!< seq_num last req'd for ER */
    vqec_seq_num_t last_rx_seq_num;    /*!< change 16-bit to 32-bit seqnums */
    uint32_t num_paks_in_pak_seq;  /*!< num paks currently in the pak_seq */
    boolean primary_received; /*!< shows if primary paks have been recvd */
    boolean repair_received;  /*!< shows if repair paks have been recvd */
    rel_time_t repair_trigger_time; /* repair trigger interval */
    rel_time_t reorder_delay;  /*!< wait for reordered paks before doing ER */
    rel_time_t fec_delay;               /* delay induced by FEC */
    rel_time_t cfg_delay;               /* added for FEC */
    rel_time_t default_delay;           /* buffer delay per packet */
    rel_time_t gap_hold_time;           /*!< time to hold pak before er */
    vqec_seq_num_t outp_last_pak_seq;    /*!< last sent packet's sequence */
    abs_time_t outp_last_pak_ts;         /*!< last sent packet's timestamp */
    uint64_t total_tx_paks;   /*!< total transmitted packets */

    uint64_t primary_packet_counter; /*!< total primary packet count */
    uint64_t repair_packet_counter; /*!< total repair packet count */
    uint64_t duplicate_packet_counter; /*!< total duplicate packet count */
    uint64_t input_loss_pak_counter; 
                                /*!< counter for single input seq drop */
    uint64_t input_loss_hole_counter; 
                                /*!< counter for contiguous input seq gap */
    uint64_t output_loss_pak_counter;
                                /*!< counter for single output seq drop */
    uint64_t output_loss_hole_counter;
                                /*!< counter for contiguous output seq gap */
    uint64_t under_run_counter;    /*!< counter for under run */
    uint64_t late_packet_counter; /*!< packets dropped from head */
    uint64_t head_ge_last_seq; /* head was >= last req'd seq_num at ER */
    uint64_t pak_seq_insert_fail_counter; /*!< failed pak_seq_insert() */
    uint64_t pcm_insert_drops; /*!< packets that failed pcm_insert() */
    uint64_t seq_bad_range_packet_counter; /*!< paks in bad seq range */
    uint64_t bad_rcv_ts;            /*!< packets with invalid receive ts  */
    uint64_t duplicate_repairs;     /* num paks repaired by both FEC and ER */
    vqec_tr135_stats_t tr135;
} vqec_dp_pcm_status_t;

typedef struct vqec_dp_nll_status_ 
{
    uint32_t mode;                      /*!< tracking / non-tracking mode */
    abs_time_t pred_base;               /*!< prediction time base */
    uint32_t pcr32_base;                /*!< The PCR value, without the MSB.
                                        use 32 bit rather than 33 bit PCR
                                        values in here so it will work with RTP
                                        timestamps too. */
    abs_time_t last_actual_time;        /*!< last prediction's actual rx time */
    uint32_t num_exp_disc;              /*!< explicit discontinuity count */
    uint32_t num_imp_disc;              /*!< implicit discontinuity count */
    uint32_t num_obs;                   /*!< samples predicted */
    rel_time_t total_adj;               /*!< total adjustment applied (debug) */
    uint32_t resets;                    /*!< forced resets  */
    uint32_t predict_in_past;           /*!< samples predicted in the past */
    rel_time_t primary_offset;          /*!< repair-burst to primary offset */
    uint32_t reset_base_no_act_time;
                                        /*!<  reset_base without actual time  */
} vqec_dp_nll_status_t;

typedef struct vqec_dp_fec_status_
{                                       /*!< stats for FEC module */
    boolean is_fec_enabled;             /*!< indicates if FEC is enabled */
    uint32_t avail_streams;             /*!< number of FEC stream available */
    boolean column_avail;               /*!< indicates column stream avail */
    boolean row_avail;                  /*!< indicates row stream avail */
    uint8_t L;                          /*!< L-value */
    uint8_t D;                          /*!< D-value */
    vqec_seq_num_t fec_column_head;     /*!< smallest column fec seq num */
    vqec_seq_num_t fec_column_tail;     /*!< largest column fec seq num */
    vqec_seq_num_t fec_row_head;        /*!< smallest row fec seq num */
    vqec_seq_num_t fec_row_tail;        /*!< largest row fec seq num */

    uint64_t fec_late_paks;             /*!< late fec paks*/
    uint64_t fec_recovered_paks  ;      /*!< fec recovered pks*/
    uint64_t fec_dec_not_needed;        /*!< no need to decode paks*/
    uint64_t fec_duplicate_paks;        /*!< fec duplicate packet count */
    uint64_t fec_total_paks;            /*!< total fec paks received */
    uint64_t fec_rtp_hdr_invalid;       /*!< # paks with invalid rtp hdr */
    uint64_t fec_hdr_invalid;           /*!< # paks with invalid fec hdr */
    uint64_t fec_unrecoverable_paks;    /*!< # fec paks can not recover video*/
    uint64_t fec_drops_other;           /*!< fec pak drops, other reason*/ 
    uint64_t fec_gap_detected;          /*!< fec gap detected */
} vqec_dp_fec_status_t;

/**
 * vqec_drop_stats_t
 *
 * Holds stats that track the drop reasons for packets of a stream that were
 * dropped.
 */
typedef struct vqec_drop_stats_t_ {
    uint64_t late_packet_counter;
    uint64_t duplicate_packet_counter;
} vqec_drop_stats_t;

/**
 * Per channel statistics.
 */
typedef
struct vqec_dp_chan_stats_
{
    /**
     * Total packets received.
     */
    uint64_t rx_paks;
    /**
     * Total UDP MPEG paks received (after RTP).
     */
    uint64_t udp_rx_paks;
    /**
     * Total RTP paks received (after RTP).
     */
    uint64_t rtp_rx_paks;

    /**
     *  Primary packets received (after RTP).
     */
    uint64_t primary_rx_paks;
    /**
     *  Primary drops due to UDP MPEG sync errors
     */
    uint64_t primary_udp_rx_drops;
    /**
     *  Primary drops due to RTP parse errors
     */
    uint64_t primary_rtp_rx_drops;
    /**
     * Primary drops due to rcc state-machine disallowing packets
     */
    uint64_t primary_sm_early_drops;
    /**
     * Primary drops due to drop simulation.
     */
    uint64_t primary_sim_drops;
    /**
     * Primary drops due to PCM insert failure:  total of all drops
     */
    uint64_t primary_pcm_drops_total;
    /**
     * Primary drops due to PCM insert failure:  packet was late
     */
    uint64_t primary_pcm_drops_late;
    /**
     * Primary drops due to PCM insert failure:  packet was a duplicate
     */
    uint64_t primary_pcm_drops_duplicate;
    
    /**
     * Total repair packets received (after RTP).
     */
    uint64_t repair_rx_paks;
    /**
     * Repair drops due to RTP parse errors
     */
    uint64_t repair_rtp_rx_drops;
    /**
     * Repair drops due to rcc state-machine disallowing packets
     */
    uint64_t repair_sm_early_drops;
    /**
     * Repair drops due to drop simulation.
     */
    uint64_t repair_sim_drops;
    /**
     * Repair drops due to PCM insert failure:  total of all drops
     */
    uint64_t repair_pcm_drops_total;
    /**
     * Repair drops due to PCM insert failure:  packet was late
     */
    uint64_t repair_pcm_drops_late;
    /**
     * Repair drops due to PCM insert failure:  packet was a duplicate
     */
    uint64_t repair_pcm_drops_duplicate;

} vqec_dp_chan_stats_t;


/*
 * Primary session failover state information
 */
typedef
struct vqec_dp_chan_failover_status_ {

    /**
     * Source, if a failover alternate has been identified.
     * Sub-fields are in network byte order.
     */
    vqec_dp_rtp_src_key_t failover_src;
    /**
     * Number of packets currently in failover queue
     */
    uint32_t failover_paks_queued;
    /**
     * Sequence number of packet @ head of failover queue (network byte order)
     */
    uint16_t failover_queue_head_seqnum;
    /**
     * Sequence number of packet @ tail of failover queue (network byte order)
     */
    uint16_t failover_queue_tail_seqnum;
    /**
     * Receive timestamp of last packet from previous source
     * (or ABS_TIME_0 if no previous source).
     */
    abs_time_t prev_src_last_rcv_ts;
    /**
     * Receive timestamp of first packet from current source
     * (or ABS_TIME_0 if no current source).
     */
    abs_time_t curr_src_first_rcv_ts;

} vqec_dp_chan_failover_status_t;

/**
 * Dataplane APP message.
 */
#if HAVE_FCC 
typedef 
struct vqec_dp_rcc_app_
{
    /**
     * Absolute time at which channel change was initiated.
     */
    abs_time_t rcc_start_time;
    /**
     * Sequence number of the first repair packet.
     */
    uint32_t start_seq_num;
    /**
     * The RTP timestamp that should be assigned to the APP packet.
     */
    uint32_t app_rtp_ts;
    /**
     * Earliest join time - delta from the first repair abs timestamp.
     */
    rel_time_t dt_earliest_join;
    /**
     * Repair burst end time - delta from the first repair abs timestamp.
     */
    rel_time_t dt_repair_end;
    /**
     * Actual minimum backfill.
     */
    rel_time_t act_min_backfill;        
    /**
     * Actual backfill at IGMP join time.
     */
    rel_time_t act_backfill_at_join;
    /**
     * ER holdoff time, relative to the earliest-join point.
     */
    rel_time_t er_holdoff_time;
    /**
     * Absolute time by which the first repair sequence must arrive. 
     */
    abs_time_t first_repair_deadline;
    /**
     * Length of the APP mpeg-ts packet.
     */
    uint16_t tspkt_len;
    /**
     * Number of APP packets (via replication) to insert at channel change.
     */
    uint32_t app_paks_per_rcc;
    /**
     * fast fill amount in ms
     */
    rel_time_t fast_fill_time;

} vqec_dp_rcc_app_t;

#endif /* HAVE_FCC */
/**
 * Upcall socket types.
 */
typedef
enum vqec_dp_upcall_socktype_
{
    /**
     * Invalid type.
     */
    VQEC_UPCALL_SOCKTYPE_INVALID,
    /**
     * Event IRQ socket.
     */
    VQEC_UPCALL_SOCKTYPE_IRQEVENT,
    /**
     * Socket for eject of NAT packets.
     */
    VQEC_UPCALL_SOCKTYPE_PAKEJECT,
    /**
     * Must be last.
     */
    VQEC_UPCALL_SOCKTYPE_NUM_VAL

} vqec_dp_upcall_socktype_t;
#define VQEC_UPCALL_SOCKTYPE_MIN VQEC_UPCALL_SOCKTYPE_IRQEVENT
#define VQEC_UPCALL_SOCKTYPE_MAX VQEC_UPCALL_SOCKTYPE_NUM_VAL - 1

/**
 * A socket "address" - this is the destination to which upcall messages should 
 * be sent by the dataplane. In future, may be used to encapsulate different
 * types of sockets.
 */
typedef
struct vqec_dp_upcall_sockaddr_
{
    struct sockaddr_un sock;

} vqec_dp_upcall_sockaddr_t;


typedef 
struct vqec_dp_global_debug_stats_ 
{

    /* Aggregated top-level debug stats */
    #define VQEC_DP_TLM_CNT_DECL(name,desc) uint32_t name;
    #include "vqec_dp_tlm_cnt_decl.h"
    #undef VQEC_DP_TLM_CNT_DECL

} vqec_dp_global_debug_stats_t;


/****************************************************************************
 * Gap logs.
 */
typedef
struct vqec_dp_gaplog_entry_
{
    /**
     * a 32-bit extended sequence number for the start of the gap.
     */
     uint32_t start_seq;
    /**
     * The extent of the gap.
     */
    uint32_t end_seq;
    /**
     * Timestamp of capture.
     */
    abs_time_t time;

} vqec_dp_gaplog_entry_t;

/**
 * Gap log array.
 */
#define VQEC_DP_GAPLOG_ARRAY_SIZE 10
typedef
struct vqec_dp_gaplog_
{
    /**
     * Number of valid entries in the array.
     */
    uint32_t num_entries;
    /**
     * Actual gap entry array.
     */
    vqec_dp_gaplog_entry_t gaps[VQEC_DP_GAPLOG_ARRAY_SIZE];

} vqec_dp_gaplog_t;

/**
 * Sequence number logs for various streams.
 */
#define VQEC_DP_SEQLOG_ARRAY_SIZE 10
typedef
struct vqec_dp_seqlog_
{
    /**
     * Primary packet sequence log.
     */
    vqec_seq_num_t prim[VQEC_DP_SEQLOG_ARRAY_SIZE];
    /**
     * Repair packet sequence log.
     */
    vqec_seq_num_t repair[VQEC_DP_SEQLOG_ARRAY_SIZE];
    /**
     * FEC repair packet sequence log.
     */
    vqec_seq_num_t fec[VQEC_DP_SEQLOG_ARRAY_SIZE];

} vqec_dp_seqlog_t;


/****************************************************************************
 * RCC state and status.
 */

/**
 * RCC dataplane state machine events
 */
typedef 
enum vqec_dp_sm_event_
{
    /**
     * Continue with RCC.
     */
    VQEC_DP_SM_EVENT_START_RCC,
    /**
     * Abort RCC: forced by control-plane.
     */
    VQEC_DP_SM_EVENT_ABORT,  
    /**
     * Abort RCC: Internal error in dataplane.
     */
    VQEC_DP_SM_EVENT_INTERNAL_ERR,    
    /**
     * Time-to-join.
     */
    VQEC_DP_SM_EVENT_TIME_TO_JOIN,
    /**
     * Time-to-enable ER.
     */
    VQEC_DP_SM_EVENT_TIME_TO_EN_ER,
    /**
     * End-of-burst.
     */
    VQEC_DP_SM_EVENT_TIME_END_BURST,
    /**
     * First repair sequence timeout.
     */
    VQEC_DP_SM_EVENT_TIME_FIRST_SEQ,    
    /**
     * Received repair packet.
     */
    VQEC_DP_SM_EVENT_REPAIR,
    /**
     * Recevied primary packet.
     */
    VQEC_DP_SM_EVENT_PRIMARY,
    /**
     * Repair session has stopped receiving data.
     */
    VQEC_DP_SM_EVENT_ACTIVITY_TIMEOUT,
    /**
     * Must be last.
     */
    VQEC_DP_SM_EVENT_NUM_VAL,

} vqec_dp_sm_event_t;

#define VQEC_DP_SM_EVENT_MIN_EV VQEC_DP_SM_EVENT_START_RCC
#define VQEC_DP_SM_EVENT_MAX_EV VQEC_DP_SM_EVENT_NUM_VAL - 1

/**
 * A brief snapshot of pcm data.
 */ 

#if HAVE_FCC

typedef
struct vqec_dp_pcm_snapshot_
{
    /**
     * Head.
     */
    uint32_t head;
    /**
     * Tail.
     */
    uint32_t tail;
    /**
     * Packets in cache.
     */
    uint32_t num_paks;
    /**
     * Output gap count. 
     */
    uint64_t outp_gaps;
    /**
     * Duplicate packets.
     */
    uint64_t dup_paks;
    /**
     *  Late packets.
     */
    uint64_t late_paks;

} vqec_dp_pcm_snapshot_t;

/**
 * Output scheduler state at beginning & end of RCC burst.
 */ 

typedef
struct vqec_dp_pcm_outp_rcc_data_
{
    /**
     * First packet tx time.
     */
    abs_time_t first_rcc_pak_ts;
    /**
     * Tx time of the first-primary (end-of-burst) packet.
     */
    abs_time_t last_rcc_pak_ts;
    /**
     * Output loss packets during RCC.
     */
    uint32_t output_loss_paks_in_rcc;
    /**
     * Output loss holes during RCC.
     */
    uint32_t output_loss_holes_in_rcc;
    /**
     * Duplicate packets during RCC.
     */
    uint32_t dup_paks_in_rcc;
    /**
     * Difference between last repair & first mcast sequence # */
    uint64_t first_prim_to_last_rcc_seq_diff;
    /**
     * Last x Gaps observed (if any).
     */
    vqec_dp_gaplog_t outp_gaps;
    
}  vqec_dp_pcm_outp_rcc_data_t;

/**
 * Event log entry for dataplane state-machine.
 */
typedef
struct vqec_dp_rcc_fsm_log_entry_
{
    /**
     * Log event type.
     */
    uint32_t log_event;
    /**
     * Fsm state index.
     */
    uint32_t state;
    /**
     * Fsm event index.
     */
    uint32_t event;
    /**
     * Timestamp of log.
     */
    abs_time_t timestamp;

} vqec_dp_rcc_fsm_log_entry_t;

/**
 * FSM log array.
 */
#define VQEC_DP_RCC_FSMLOG_ARRAY_SIZE 10
typedef
struct vqec_dp_rcc_fsm_log_
{
    /**
     * Number of valid entries in the array.
     */
    uint32_t num_entries;
    /**
     * Actual log events.
     */
    vqec_dp_rcc_fsm_log_entry_t logentry[VQEC_DP_RCC_FSMLOG_ARRAY_SIZE];

} vqec_dp_rcc_fsm_log_t;

/**
 * RCC data captured from the dataplane for each RCC burst.
 */  
#define VQEC_DP_RCC_FAILURE_REASON_LEN 24
typedef
struct vqec_dp_rcc_data_
{
    /**
     * Brief pcm snapshot at join-time.
     */
    vqec_dp_pcm_snapshot_t join_snap;
    /**
     * Brief pcm snapshot at first-primary time.
     */
    vqec_dp_pcm_snapshot_t prim_snap;
    /**
     * Brief pcm snapshot at ER enable time.
     */
    vqec_dp_pcm_snapshot_t er_en_snap;
    /**
     * Brief pcm snapshot at RCC burst-end time.
     */
    vqec_dp_pcm_snapshot_t end_burst_snap;
    /**
     * Reflects RCC state in dataplane.
     */
    boolean rcc_enabled;
    /**
     * Was RCC aborted.
     */
    boolean rcc_in_abort;
    /**
     * Was RCC successful.
     */
    boolean rcc_success;
    /**
     * Mask of all RCC events delivered
     */
    uint32_t event_mask;
    /**
     * Actual time at which 1st repair packet processed.
     */
    abs_time_t first_repair_ev_ts;
    /**
     * Actual time at which join sent.
     */
    abs_time_t join_issue_ev_ts;
    /**
     * First primary sequence number.
     */
    uint32_t first_primary_seq;
    /**
     * Actual time at which 1st primary packet processed.
     */
    abs_time_t first_primary_ev_ts; 
    /**
     * Actual time at which ER enabled.
     */
    abs_time_t er_en_ev_ts;
   /**
    * Actual time at which Burst End
    */
    abs_time_t burst_end_time;
    /**
     * Join latency.
     */
    rel_time_t join_latency;
    /**
     * RCC data captured at the output scheduler.
     */
    vqec_dp_pcm_outp_rcc_data_t outp_data; 
    /**
     * State-machine state transition log.
     */
    vqec_dp_rcc_fsm_log_t sm_log;
    /**
     * A Textual reason for error.
     */
    char fail_reason[VQEC_DP_RCC_FAILURE_REASON_LEN];
    /**
     * Packet receive time of first repair packet
     */
    abs_time_t first_repair_ts;
    /**
     * Packet receive time of first primary packet
     */
    abs_time_t first_primary_ts;

} vqec_dp_rcc_data_t;

#endif /* HAVE_FCC */

/**
 * Input shim's state that is available through the public interface.
 */
typedef 
struct vqec_dp_input_shim_status_
{

    boolean is_shutdown;      /*!< TRUE if the shim is shutdown */
    uint32_t os_creates;      /*!< Output streams created since activation */
    uint32_t os_destroys;     /*!< Output streams destroyed since activation */
    uint32_t num_filters;     /*!< Number of unique filters in current use */
    uint64_t num_pkt_errors;  /*!<
                               *!< Number of internal packet errors
                               *!< (pkts received but not forwarded to an
                               *!<  input stream)
                               */
    uint64_t tr135_overruns;   /*!< Overruns because of Tr-135 counters */

} vqec_dp_input_shim_status_t;

/*
 * FIXME:
 * The following APIs will not work if when using a script to autogenerate
 * IPC-related code.
 */
#define VQEC_DP_INPUTSHIM_MAX_FILTERS 16
#define VQEC_DP_INPUTSHIM_MAX_STREAMS VQEC_DP_INPUTSHIM_MAX_FILTERS

/*
 * Identifies histograms supported by the data plane.
 *
 * VQEC_DP_HIST_JOIN_DELAY:
 * Delay intervals between join request and receipt of
 * first primary packet of all channels (past and present).
 *
 * VQEC_DP_HIST_OUTPUTSCHED:
 * Delay intervals between instances of output interval scheduling. 
 */
typedef enum vqec_dp_hist_t_ {
    VQEC_DP_HIST_JOIN_DELAY = 0,
    VQEC_DP_HIST_OUTPUTSCHED,
    VQEC_DP_HIST_MAX
} vqec_dp_hist_t;

/*
 * Per-tuner Histograms supported by outputshim.
 */
typedef enum vqec_dp_outputshim_hist_t_ 
{
    VQEC_DP_HIST_OUTPUTSHIM_READER_JITTER,
    VQEC_DP_HIST_OUTPUTSHIM_INP_DELAY,
    VQEC_DP_HIST_OUTPUTSHIM_MAX
} vqec_dp_outputshim_hist_t;

/* Maximum number of buckets supported by above histograms */
#define VQEC_DP_HIST_MAX_BUCKETS 25

typedef VAM_HIST_STRUCT_DECL(vqec_dp_histogram_data_, VQEC_DP_HIST_MAX_BUCKETS)
    vqec_dp_histogram_data_t;

typedef struct vqec_dp_graphinfo_ {
    vqec_dp_graphid_t id;
    struct {
        vqec_dp_streamid_t prim_rtp_id;
        vqec_dp_streamid_t repair_rtp_id;
        vqec_dp_streamid_t fec0_rtp_id;
        vqec_dp_streamid_t fec1_rtp_id;
        in_port_t rtp_eph_rtx_port;
    } inputshim;
    struct {
        vqec_dp_chanid_t dpchan_id;
        vqec_dp_streamid_t prim_rtp_id;
        vqec_dp_streamid_t repair_rtp_id;
        vqec_dp_streamid_t fec0_rtp_id;
        vqec_dp_streamid_t fec1_rtp_id;
    } dpchan;
    struct {
        vqec_dp_streamid_t postrepair_id;
    } outputshim;
} vqec_dp_graphinfo_t;

/**
 * Invalid output shim tuner ID.
 */
#define VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID 0

/**
 * Invalid output socket FD.
 */
#define VQEC_DP_OUTPUT_SHIM_INVALID_FD (-1)

/**
 * Maximum limit on length of DP tuner name.
 *
 * NOTE:  should align with VQEC_MAX_TUNER_NAMESTR_LEN in vqec_ifclient_defs.h.
 */
#define VQEC_DP_TUNER_MAX_NAME_LEN (16)

/**
 * Maximum limit on iobuf array that can be read.
 */
#define VQEC_DP_MAX_IOBUF_ARRAY_LENGTH  (32)

/**
 * Maximum limit on receive timeout. in msecs.
 */
#define VQEC_DP_MAX_IOBUF_RECV_TIMEOUT  (100)

/**
 * Maximum limit on length of buffer to print an IS's mapped tunerIDs into.
 */
#define VQEC_DP_OUTPUT_SHIM_PRINT_BUF_SIZE  (96)

/**
 * Output shim's state that is available through the public interface.
 */
typedef 
struct vqec_dp_output_shim_status_
{
    boolean shutdown;        /*!< TRUE if the output shim is shutdown */
    uint32_t is_creates;        /*!< Input streams created since activation */
    uint32_t is_destroys;       /*!< Input streams destroyed since activation */
    uint32_t num_tuners;        /*!< Number of active tuners  */
} vqec_dp_output_shim_status_t;

/**
 * Tuner's status in the dataplane.
 */
typedef 
struct vqec_dp_output_shim_tuner_status_
{

    int32_t cp_tid;             /*!< Control-plane tuner id */
    vqec_dp_tunerid_t tid;      /*!< Local dataplane tuner id  */
    int32_t qid;                /*!< Output queue id */
    vqec_dp_streamid_t is;      /*!< IS to which this tuner is connected */
    uint32_t qinputs;           /*!< Total packets enqueued to the queue */
    uint32_t qdrops;            /*!< Total drops on the queue  */
    uint32_t qdepth;            /*!< Instantaneous depth of the queue */
    uint32_t qoutputs;          /*!< Total packets removed from the queue */
} vqec_dp_output_shim_tuner_status_t;

/**
 * Maximum length of a packet that can be exchanged through IPC.
 */
#define VQEC_DP_IPC_MAX_PAKSIZE 3200

/**
 * Test vqec reader params.
 */
typedef struct test_vqec_reader_params_ {
    int tid;
    in_addr_t srcaddr;
    in_addr_t dstaddr;
    uint16_t dstport;
    uint32_t iobufs;
    uint32_t iobuf_size;
    int32_t timeout;  /* in msec */
    boolean disable, use_module_params;
} test_vqec_reader_params_t;

#endif /* __VQEC_DP_API_TYPES_H__ */
