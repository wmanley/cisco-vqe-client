/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: RTP input stream classes.
 *
 * Documents:
 *
 *****************************************************************************/

#ifndef __VQEC_DP_RTP_INPUT_STREAM_H__
#define __VQEC_DP_RTP_INPUT_STREAM_H__

#include "vqec_dp_api_types.h"
#include "vqec_dp_refcnt.h"
#include "vqec_dp_io_stream.h"
#include "vqec_dp_rtp_receiver.h"
#include "vqec_seq_num.h"
#include <utils/queue_plus.h>
#include <utils/vam_time.h>

struct vqec_dpchan_;
struct vqec_pak_;

/****************************************************************************
 * Dataplane base input stream data structure.
 */

#define VQEC_DP_CHAN_INPUT_STREAM_MEMBERS                               \
    /**                                                                 \
     * Id of this stream.                                               \
     */                                                                 \
    vqec_dp_isid_t id;                                                  \
    /**                                                                 \
     * Opaque handle provided at creation (used for FEC row or column). \
     */                                                                 \
    int32_t handle;                                                     \
    /**                                                                 \
     * Reference count.                                                 \
     */                                                                 \
    VQEC_DP_REFCNT_FIELD(struct vqec_dp_chan_input_stream_);            \
    /**                                                                 \
     * Parent channel object.                                           \
     */                                                                 \
    struct vqec_dpchan_ *chan;                                          \
    /**                                                                 \
     * Instance of the OS connected to this IS.                         \
     */                                                                 \
    vqec_dp_os_instance_t os_instance;                                  \
    /**                                                                 \
     * The operations vector function table for this input-stream.      \
     */                                                                 \
    vqec_dp_isops_t *ops;                                               \
    /**                                                                 \
     * Capabilities.                                                    \
     */                                                                 \
    int32_t capa;                                                       \
    /**                                                                 \
     * Active or inactive.                                              \
     */                                                                 \
    boolean act;                                                        \
    /**                                                                 \
     * Type of the input stream (primary, repair, or fec)               \
     */                                                                 \
    vqec_dp_input_stream_type_t type;                                   \
    /**                                                                 \
     * Detected encapsulation of stream (RTP or UDP)                    \
     */                                                                 \
    vqec_dp_encap_type_t encap;                                         \
    /**                                                                 \
     * Last processed packet timestamp.                                 \
     */                                                                 \
    abs_time_t last_pak_ts;                                             \
    /**                                                                 \
     * Base statistics snapshot.                                        \
     */                                                                 \
    vqec_dp_stream_stats_t in_stats_snapshot;                           \
    /**                                                                 \
     * Base statistics.                                                 \
     * Keep at end of structure for optimization reasons.               \
     */                                                                 \
    vqec_dp_stream_stats_t in_stats;


#define VQEC_DP_CHAN_INPUT_STREAM_FCNS                                  \
    /**                                                                 \
     * Process one packet and deliver to a packet cache.                \
     * The current_time may be passed in by the caller as an            \
     * input argument. If the time specified is 0, the current system   \
     * time will be internally read.                                    \
     *                                                                  \
     * @param[in] in_stream Object pointer.                             \
     * @param[in] pak Pointer to the input packet.                      \
     * @param[in] current_time Current system time.                     \
     * @param[out] uint32_t Returns the number of successfully          \
     * processed by this method (= one).                                \
     */                                                                 \
    uint32_t (*process_one)(                                            \
        struct vqec_dp_chan_input_stream_ *in_stream,                   \
        struct vqec_pak_ *pak,                                          \
        abs_time_t current_time);                                       \
                                                                        \
    /**                                                                 \
     * Process & deliver a packet array to the packet cache.            \
     * The current_time may be passed in by the caller as an            \
     * input argument. If the time specified is 0, the current system   \
     * time will be internally read.                                    \
     *                                                                  \
     * @param[in] in_stream Object pointer.                             \
     * @param[in] pak_array Packet pointer array.                       \
     * @param[in] array_len Number of packets in the array.             \
     * @param[in] current_time Current system time.                     \
     * @param[out] uint32_t Returns the number of successfully          \
     * processed by this method (= one).                                \
     */                                                                 \
    uint32_t (*process_vector)(                                         \
        struct vqec_dp_chan_input_stream_ *in_stream,                   \
        struct vqec_pak_ *pak_array[],                                  \
        uint32_t array_len,                                             \
        abs_time_t current_time);                                       \
                                                                        \
    /**                                                                 \
     * Destroy the input stream.                                        \
     *                                                                  \
     * @param[in] in_stream Object pointer.                             \
     */                                                                 \
    void (*destroy)(struct vqec_dp_chan_input_stream_ *in_stream);      \
                                                                        \
    /**                                                                 \
     * Connect an OS to the input stream.                               \
     *                                                                  \
     * @param[in] in_stream Object pointer.                             \
     * @param[in] os The output stream identifier.                      \
     * @param[in] ops Pointer to the OS operations function vector.     \
     * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK   \
     * on success.                                                      \
     */                                                                 \
    vqec_dp_stream_err_t (*connect_os)(                                 \
        struct vqec_dp_chan_input_stream_ *in_stream,                   \
        vqec_dp_osid_t os,                                              \
        const struct vqec_dp_osops_ *ops);                              \
                                                                        \
    /**                                                                 \
     * Issue a bind "commit" on the OS connected to this IS. This is    \
     * used for OS's with multicast inputs whereby the join hasn't      \
     * been issued.                                                     \
     *                                                                  \
     * @param[in] in_stream Object pointer.                             \
     * @param[boolean] Returns true if the operation succeeds.          \
     */                                                                 \
    boolean (*bind_commit)(                                             \
        struct vqec_dp_chan_input_stream_ *in_stream);                  \
                                                                        \
    /**                                                                 \
     * Issue a bind "update" on the OS connected to this IS.            \
     * This updates the source filter of the connected OS for unicast   \
     * streams only.
     *                                                                  \
     * @param[in] in_stream Object pointer.                             \
     * @param[in] fil New filter for connected OS.                      \
     * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK   \
     * on success.                                                      \
     */                                                                 \
    vqec_dp_stream_err_t(*bind_update)(                                 \
        struct vqec_dp_chan_input_stream_ *in_stream,                   \
        vqec_dp_input_filter_t *fil);                                   \
                                                                        \
    /**                                                                 \
     * Issue a data "poll" on the OS connected to this IS. This will    \
     * cause any packets that are pending on the OS's queue to be       \
     * delivered to the IS within the context of this call.             \
     *                                                                  \
     * @param[in] in_stream Object pointer.                             \
     * @param[boolean] Returns true if the operation succeeds.          \
     */                                                                 \
    boolean (*poll_data)(                                               \
        struct vqec_dp_chan_input_stream_ *in_stream);                  \



#define __vqec_dp_chan_input_stream_fcns    VQEC_DP_CHAN_INPUT_STREAM_FCNS
#define __vqec_dp_chan_input_stream_members VQEC_DP_CHAN_INPUT_STREAM_MEMBERS



DEFCLASS(vqec_dp_chan_input_stream);


/**
 * RTP statistics maintained by a RTP IS.
 */
typedef
struct vqec_dp_chan_rtp_is_stats_
{
    /**
     * UDP packets.
     */
    uint64_t udp_paks;
    /**
     * RTP packets.
     */
    uint64_t rtp_paks;
    /**
     * Packets input to the successor (fec or pcm pakseq cache(s)).
     */
    uint64_t pakseq_inserts;
    /**
     * Packets dropped on input to the successor (fec or pcm pakseq cache(s)).
     */
    uint64_t pakseq_drops;    
    /**
     * Drop stat counters for packets dropped by PCM (if sent to PCM)
     */
    vqec_drop_stats_t ses_stats;
    /**
     * Packets dropped upon behest of the RCC state machine. 
     */
    uint64_t sm_drops;
    /**
     * Packets dropped by drop-simulation.
     */
    uint64_t sim_drops;
    /**
     * Packets that failed UDP MPEG sync.
     */
    uint64_t udp_mpeg_sync_drops;
    /**
     * Packets that failed RTP parse.
     */
    uint64_t rtp_parse_drops;
    /**
     * Any packets that were filtered based on sequence number.
     */
    uint64_t fil_drops;

} vqec_dp_chan_rtp_is_stats_t;



/****************************************************************************
 * Dataplane RTP input stream data structure.
 */

#define VQEC_DP_CHAN_RTP_INPUT_STREAM_MEMBERS                           \
    /**                                                                 \
     * Base class members.                                              \
     */                                                                 \
    __vqec_dp_chan_input_stream_members                                 \
    /**                                                                 \
     * Statistics.                                                      \
     * Keep at this location for optimization.                          \
     */                                                                 \
    vqec_dp_chan_rtp_is_stats_t rtp_in_stats;                           \
    /**                                                                 \
     * Statistics snapshot                                              \
     */                                                                 \
    vqec_dp_chan_rtp_is_stats_t rtp_in_stats_snapshot;                  \
    /**                                                                 \
     * RTP receiver structure.                                          \
     */                                                                 \
    vqec_dp_rtp_recv_t rtp_recv;                                        \

    

#define VQEC_DP_CHAN_RTP_INPUT_STREAM_FCNS                              \
    /**                                                                 \
     * Base class methods.                                              \
     */                                                                 \
    __vqec_dp_chan_input_stream_fcns                                    \
                                                                        \
    /**                                                                 \
     * Get RTP input stream statistics.                                 \
     *                                                                  \
     * @param[in] in_stream Object pointer.                             \
     * @param[out] stats Output RTP statistics pointer.                 \
     * @param[out] vqec_dp_error_t Returns VQEC_DP__ERR_OK              \
     * on success.                                                      \
     */                                                                 \
    vqec_dp_error_t (*get_rtp_stats)(                                   \
        struct vqec_dp_chan_rtp_input_stream_ *in,                      \
        vqec_dp_chan_rtp_is_stats_t *stats,                             \
        boolean cumulative);                                          \
                                                                        \
    /**                                                                 \
     * Clear RTP input stream statistics.                               \
     *                                                                  \
     * @param[in] in_stream Object pointer.                             \
     */                                                                 \
    void (*clear_rtp_stats)(                                            \
        struct vqec_dp_chan_rtp_input_stream_ *in);                     \


    
#define __vqec_dp_chan_rtp_input_stream_fcns    \
    VQEC_DP_CHAN_RTP_INPUT_STREAM_FCNS
#define __vqec_dp_chan_rtp_input_stream_members \
    VQEC_DP_CHAN_RTP_INPUT_STREAM_MEMBERS


DEFCLASS(vqec_dp_chan_rtp_input_stream);


/**
 * The FEC input stream type is simply typedef'd in terms of RTP input stream.
 * It can be expanded into a full class hierarchy if needed at any later point.
 */
typedef vqec_dp_chan_rtp_input_stream_t vqec_dp_chan_rtp_fec_input_stream_t;

/*
 * Maximum number of packets to allow into the failover queue.
 *
 * This is currently just a constant, sized with the goals of:
 * 1) Supporting failover of 2 HD streams (1 pkt/ms) that are aligned
 *    in time, plus a minimal overlap.
 * 2) Preventing the packet pool from being fully drained by an unwanted
 *    source (when source filtering is disabled).
 *
 * The value is intended to handle this worst case scenario:
 *
 *                    inactivity         inactivity
 *                       poll               poll                    failover
 *                        |                   |                      point
 *         | overlap (ms) v   POLL_INTERVAL   v   POLL_INTERVAL  |  / 
 *   A  --------------------                                      /
 *   B     ------------------------------------------------------X----
 *         ^                                                     ^
 *         |                                                     |
 *         |<------ total failover buffering required ---------->|
 *
 *   where             
 *      overlap (ms)     = 80 ms (maximum overlap assumed)
 *      POLL_INTERVAL    = VQEC_DP_DEFAULT_POLL_INTERVAL_MSECS
 */
#define VQEC_DP_RTP_SRC_FAILOVER_PAKS_MAX  \
    (80 + 2 * VQEC_DP_DEFAULT_POLL_INTERVAL_MSECS)


/****************************************************************************
 * Dataplane RTP primary input stream data structure.
 */

#define VQEC_DP_CHAN_RTP_PRIMARY_INPUT_STREAM_MEMBERS                   \
    /**                                                                 \
     * Base class members.                                              \
     */                                                                 \
    __vqec_dp_chan_rtp_input_stream_members                             \
    /**                                                                 \
     * Alternate source selected for failover recovery.                 \
     */                                                                 \
    vqec_dp_rtp_src_t *failover_rtp_src_entry;                          \
    /**                                                                 \
     * Queue of packets from an alternate source.                       \
     */                                                                 \
    VQE_TAILQ_HEAD(vqe_failoverq_head_, vqec_pak_) failoverq_lh;        \
    /**                                                                 \
     * Number of packets in the failover queue.                         \
     */                                                                 \
    uint32_t failover_paks_queued;                                      \


#define VQEC_DP_CHAN_RTP_PRIMARY_INPUT_STREAM_FCNS                      \
    /**                                                                 \
     * Base class methods.                                              \
     */                                                                 \
    __vqec_dp_chan_rtp_input_stream_fcns                                \

                                                  
#define __vqec_dp_chan_rtp_primary_input_stream_fcns     \
    VQEC_DP_CHAN_RTP_PRIMARY_INPUT_STREAM_FCNS
#define __vqec_dp_chan_rtp_primary_input_stream_members  \
    VQEC_DP_CHAN_RTP_PRIMARY_INPUT_STREAM_MEMBERS


DEFCLASS(vqec_dp_chan_rtp_primary_input_stream);


/****************************************************************************
 * Dataplane RTP repair input stream data structure.
 */

#define VQEC_DP_CHAN_RTP_REPAIR_INPUT_STREAM_MEMBERS                    \
    /**                                                                 \
     * Base class members.                                              \
     */                                                                 \
    __vqec_dp_chan_rtp_input_stream_members                             \
    /**                                                                 \
     * Hold queue list head.                                            \
     */                                                                 \
    VQE_TAILQ_HEAD(, vqec_pak_) holdq_lh;                               \
    /**                                                                 \
     * First sequence number filter.                                    \
     */                                                                 \
    vqec_seq_num_t first_seqnum_fil;                                    \
    /**                                                                 \
     * Is first sequence filter active.                                 \
     */                                                                 \
    boolean first_seqnum_fil_act;                                       \



#define VQEC_DP_CHAN_RTP_REPAIR_INPUT_STREAM_FCNS                       \
    /**                                                                 \
     * Base class methods.                                              \
     */                                                                 \
    __vqec_dp_chan_rtp_input_stream_fcns                                \

                                                  
#define __vqec_dp_chan_rtp_repair_input_stream_fcns     \
    VQEC_DP_CHAN_RTP_REPAIR_INPUT_STREAM_FCNS
#define __vqec_dp_chan_rtp_repair_input_stream_members  \
    VQEC_DP_CHAN_RTP_REPAIR_INPUT_STREAM_MEMBERS


DEFCLASS(vqec_dp_chan_rtp_repair_input_stream);


/**
 * Create a new fec input stream. Returns invalid id on failure.
 *
 * @param[in] parent Pointer to the parent channel.
 * @param[in] handle An opaque handle which is used by FEC to determine 
 *  the source of packets delivered to it.
 * @param[out] vqec_dp_isid_t Identifier of the input stream created,
 * or the invalid identifier on failure. 
 */
vqec_dp_isid_t
vqec_dp_chan_rtp_fec_input_stream_create(struct vqec_dpchan_ *parent, 
                                         int32_t handle,
                                         vqec_dp_input_stream_t *s_desc);

/**
 * Create a new repair input stream. Returns invalid id on failure.
 *
 * @param[in] parent Pointer to the parent channel.
 * @param[out] vqec_dp_isid_t Identifier of the input stream created,
 * or the invalid identifier on failure. 
 */
vqec_dp_isid_t
vqec_dp_chan_rtp_repair_input_stream_create(struct vqec_dpchan_ *parent,
                                            vqec_dp_input_stream_t *s_desc);

/**
 * Create a new primary input stream. Returns invalid id on failure.
 *
 * @param[in] parent Pointer to the parent channel.
 * @param[out] vqec_dp_isid_t Identifier of the input stream created,
 * or the invalid identifier on failure. 
 */
vqec_dp_isid_t
vqec_dp_chan_rtp_primary_input_stream_create(struct vqec_dpchan_ *parent,
                                             vqec_dp_input_stream_t *s_desc);

/**
 * Initialize the RTP subsystem.
 * 
 * @param[in] params Initialization parameters.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_chan_rtp_module_init(vqec_dp_module_init_params_t *params);

/**
 * Deinitialize the RTP subsystem.
 */
void
vqec_dp_chan_rtp_module_deinit(void);

/**
 * HELPER: Wrapper to forward RTP source upcall IRQ event requests to
 * the parent dataplane channel. Invoked by the repair and primary RTP
 * receivers.
 *
 * @param[in] in Object pointer.
 * @param[in] src The RTP source whose state-change has generated the event.
 * @param[in] ev Reason code for the event.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if an IRQ request
 * is already pending for the control-plane, or if a IRQ message is 
 * successfully enqueued for the control-plane.
 */
vqec_dp_error_t
vqec_dp_chan_rtp_input_stream_src_ev(vqec_dp_chan_rtp_input_stream_t *in,
                                     vqec_dp_rtp_src_t *src, 
                                     vqec_dp_upcall_irq_reason_code_t ev);

/**
 * Get the IS (input-stream) instance - the instance consists of the IS 
 * identifier and it's operations function vector.
 *
 * @param[in] in Object pointer.
 * @param[out] instance Instance structure into which the id and the 
 * operations function vector are copied.
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK on success.
 */
vqec_dp_stream_err_t
vqec_dp_chan_input_stream_get_instance(vqec_dp_chan_input_stream_t *in, 
                                       vqec_dp_is_instance_t *instance);

/**
 * Get the status data structure for the IS.
 *
 * @param[in] in Object pointer.
 * @param[out] status Output structure into which the IS status is written.
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK on success.
 */
vqec_dp_stream_err_t 
vqec_dp_chan_input_stream_get_status(vqec_dp_chan_input_stream_t *in,
                                     vqec_dp_is_status_t *status, 
                                     boolean cumulative);

/**
 * HELPER: From stream-id to stream-pointer.
 */
vqec_dp_chan_input_stream_t *
vqec_dp_chan_input_stream_id_to_ptr(vqec_dp_isid_t id);

/****************************************************************************
 * Inlines
 */

/**
 * Input stream type.
 *
 * @param[in] in Object pointer.
 * @param[out] vqec_dp_input_stream_type_t Returns type of the input stream.
 */
static inline vqec_dp_input_stream_type_t  
vqec_dp_chan_input_stream_type (vqec_dp_chan_input_stream_t *in)
{
    return (in->type);
}

/**
 * Allow dataflow (packets to flow) on the IS.
 *
 * @param[in] in Object pointer.
 */
static inline void
vqec_dp_chan_input_stream_run (vqec_dp_chan_input_stream_t *in)
{
    if (in) {
        in->act = TRUE;
    }
}

/**
 * Stop dataflow on the IS.
 *
 * @param[in] in Object pointer.
 */
static inline void
vqec_dp_chan_input_stream_stop (vqec_dp_chan_input_stream_t *in)
{
    if (in) {
        in->act = FALSE;
    }
}


/****************************************************************************
 * Repair-stream specific methods.
 */

/**
 * For RCC, the repair IS will queue input packets until it sees the first 
 * sequence number for the unicast burst defined by the APP packet. The
 * dataplane channel invokes this method to set the first sequence number.
 * (see the implementation for further notes).
 *
 * @param[in] in Object pointer.
 * @param[in] start_seq_num The 16-bit sequence number specified by the
 * APP packet.
 */
void
vqec_dp_chan_rtp_repair_input_stream_set_firstseq_fil(
    vqec_dp_chan_rtp_repair_input_stream_t *in, 
    vqec_seq_num_t start_seq_num);

/**
 * Remove any first sequence number filter that was previously set. If the
 * first sequence number was received, the filter is autmatically disabled, 
 * otherwise the caller must disable it.
 *
 * @param[in] in Object pointer.
 */
void
vqec_dp_chan_rtp_repair_input_stream_clear_firstseq_fil(
    vqec_dp_chan_rtp_repair_input_stream_t *in);

/**
 * For RCC, if there are packets on the IS queue and RCC is aborted,
 * this method must be called to flush the queue.
 *
 * @param[in] in Object pointer.
 */
void
vqec_dp_chan_rtp_repair_input_stream_flush_holdq(
    vqec_dp_chan_rtp_repair_input_stream_t *in);

#endif /* __VQEC_DP_RTP_INPUT_STREAM_H__ */
