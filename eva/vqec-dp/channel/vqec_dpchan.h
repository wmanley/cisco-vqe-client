/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: VQE-C Dataplane Channel internal definitions.
 *
 * Documents: For use within the channel module.
 *
 *****************************************************************************/

#ifndef __VQEC_DPCHAN_H__
#define __VQEC_DPCHAN_H__

#include <vqec_pcm.h>
#include <vqec_oscheduler.h>
#include <vqec_fec.h>
#include <vqec_dp_io_stream.h>
#include <vqec_dp_refcnt.h>
#include <vqec_dp_api_types.h>
#include "vqec_dp_sm.h"
#include "vqec_dpchan_api.h"
#include "mp_mpeg.h"

#define VQEC_DPCHAN_MAX_DPCHAN_OSES 64

/**
 * IRQ descriptor.
 */
typedef
struct vqec_dp_irq_desc_
{
    /**
     * True when an IRQ is pending, and hasn't been acknowledged.
     */
    boolean pending;
    /**
     * Cause register - contains a bitmask of the events that are 
     * active in the device.
     */
    uint32_t cause;
    /**
     * Input event count.
     */
    uint32_t in_events;
    /**
     * IRQ's successfully sent.
     */
    uint32_t sent_irq;
    /**
     * IRQ's dropped.
     */
    uint32_t dropped_irq;
    /**
     * IRQ's acknowledged.
     */
    uint32_t ack;
    /**
     * Spurious IRQs acknowledgements.
     */
    uint32_t spurious_ack;

} vqec_dp_irq_desc_t;


/*----
 * RCC fields.
 *----*/
#if HAVE_FCC

#define VQEC_DPCHAN_RCC_FIELDS                                          \
    vqec_dp_sm_state_t state;        /* Current sm state */             \
    uint32_t event_mask;             /* Mask of all events delivered */ \
    vqec_dp_sm_event_q_t event_q[VQEC_DP_SM_MAX_EVENTQ_DEPTH];          \
                                     /* Event Q */                      \
    uint32_t event_q_depth;          /* Instantaneous event Q depth */  \
    uint32_t event_q_idx;            /* Next unused index in event Q */ \
    vqec_dp_sm_log_t sm_log[VQEC_DP_SM_LOG_MAX_SIZE];                   \
                                     /* Log circular buffer */          \
    uint32_t sm_log_tail;            /* Tail of log buffer */           \
    uint32_t sm_log_head;            /* Head of log buffer */           \
                                                                        \
    rel_time_t act_min_backfill;     /* minimum backfill expected */    \
    rel_time_t act_backfill_at_join; /* backfill expected at join */    \
    rel_time_t max_backfill;         /* maximum backfill expected */    \
    rel_time_t min_backfill;         /* minimum backfill allowed */     \
    rel_time_t max_fastfill;         /* maximum fastfill allowed */     \
    rel_time_t dt_earliest_join;     /* earliest join time */           \
    rel_time_t dt_repair_end;        /* end-of-repair time */           \
    rel_time_t er_holdoff_time;      /* ER holdoff time */              \
    abs_time_t first_repair_deadline;                                   \
                                    /* Abs time limit for the */        \
                                    /* arrival offirst repair packet */ \
                                                                        \
    boolean joined_primary;         /* Join issued on primary? */       \
    boolean joined_fec;             /* Join issues on FEC? */           \
    boolean rcc_in_abort;           /* Has RCC been aborted? */         \
                                    /* APP TS packets list */           \
    VQE_TAILQ_HEAD(app_pkt_list_head_,vqec_pak_) app_paks;              \
    uint32_t start_seq_num;         /* RTP start sequence number */     \
    vqec_event_t *activity_timer;   /* Data activity timer */           \
    vqec_event_t *er_timer;         /* Timer for enabling ER */         \
    vqec_event_t *wait_first_timer; /* Wait-for-first sequence timer */ \
    vqec_event_t *join_timer;       /* Join timer */                    \
    vqec_event_t *burst_end_timer;  /* End-of-burst timer */            \
                                                                        \
    abs_time_t last_primary_pak_ts; /* Rx time of the last primary */   \
                                    /* packet, updated during burst */  \
                                                                        \
    boolean rx_repair;              /* First required seqnum rxd' */    \
    vqec_seq_num_t first_repair_seq;                                    \
                                    /* First repair sequence */         \
    abs_time_t first_repair_ts;     /* First repair rx time (socket) */ \
    abs_time_t first_repair_ev_ts;  /* First repair event time */       \
    abs_time_t burst_end_time;      /* Burst end time */                \
    abs_time_t last_repair_pak_ts;  /* Rx time of the last repair */    \
                                    /* packet, updated during burst */  \
    abs_time_t er_enable_time;      /* Time at which ER enabled */      \
    vqec_dp_pcm_snapshot_t pcm_join_snap;                               \
                                    /* Pcm snapshot at join-time  */    \
    vqec_dp_pcm_snapshot_t pcm_er_en_snap;                              \
                                    /* Pcm snapshot at ER-en-time  */   \
    vqec_dp_pcm_snapshot_t pcm_endburst_snap;                           \
                                    /* Pcm snapshot at end-of-burst  */ \
    vqec_dp_pcm_snapshot_t pcm_firstprim_snap;                          \
                                    /* Pcm snapshot at first-primary */ \
                                                                        \
    uint8_t pat_buf[MP_PSISECTION_LEN];                                 \
                                    /* PAT buffer */                    \
    uint16_t pat_len;               /* length of the parsed PAT data */ \
    uint8_t pmt_buf[MP_PSISECTION_LEN];                                 \
                                    /* PMT buffer */                    \
    uint16_t pmt_len;               /* length of the parsed PMT data */ \
    uint16_t pmt_pid;               /* PMT PID */                       \
    uint64_t pcr_val;               /* PCR value from the APP packet */ \
    uint64_t pts_val;               /* PTS value from the APP packet */ \
    boolean process_first_repair;   /* first repair arrived */ \

#endif   /* HAVE_FCC */

#define MAX_DPCHAN_IPV4_PRINT_NAME_SIZE \
    (sizeof("XXX.XXX.XXX.XXX:65535")) 
/* (just a sample string to measure the max length for IPv4 addresses) */

typedef struct vqec_dpchan_ {
    vqec_dp_chanid_t id;        /* Identifier of this channel */
    uint32_t cp_handle;         /* Opaque identifier for the CP */
    char url[MAX_DPCHAN_IPV4_PRINT_NAME_SIZE+1];
                                /* channel URL */
    vqec_pcm_t pcm;             /* pcm for this channel */
    vqec_fec_t *fec;            /* pointer to the fec mgr for this channel */
    vqec_dp_os_instance_t os;   /* output stream for this channel */
    boolean is_multicast;       /* Is the channel dest a multicast addr? */
    boolean er_enabled;         /* If ER is enabled on the channel */
    boolean rcc_enabled;        /* If RCC is enabled on the channel */
    boolean fec_enabled;        /* If FEC is enabled on the channel */
    boolean passthru;           /* Indicates if this is a non-VQE channel */
    boolean prim_inactive;      /* primary is not active */
    rel_time_t reorder_time;    /* Used to detect underrun without a PCM */
    uint32_t msg_generation_num;    
                                /* Per channel upcall message sequence number */
    uint32_t generation_id;     /* Channel generation id */

    vqec_dp_isid_t prim_is;     /* primary input stream */
    vqec_dp_isid_t repair_is;   /* repair input stream */
    vqec_dp_isid_t fec0_is;     /* fec 0 input stream (row or col) */
    vqec_dp_isid_t fec1_is;     /* fec 1 input stream (row or col) */

    vqec_dp_irq_desc_t irq_desc[VQEC_DP_UPCALL_DEV_MAX];
                                /* IRQ descriptor for all devices */
    uint32_t pak_ejects;        /* Packets ejected on the channel */
    uint32_t pak_eject_errors;  /* Ejects failed */

    VQEC_DP_REFCNT_FIELD(struct vqec_dpchan_);
                                /* Reference count field */
    VQE_TAILQ_ENTRY(vqec_dpchan_) le;
                                /* List chaining element */

    abs_time_t join_issue_time; /* Join issue time */
    boolean rx_primary;         /* First primary pak rxd' */
    abs_time_t first_primary_ts;
                                /* First primary rx time (socket) */
    vqec_seq_num_t first_primary_seq;                                   
                                    /* 
                                     * First primary sequence
                                     * Note:  although this is an extended
                                     * sequence number, its lower 16 bits
                                     * represent the rtp sequence number
                                     * because it came from the initial
                                     * source.
                                     */
    abs_time_t first_primary_ev_ts; /* First primary event time */
    abs_time_t prev_src_last_rcv_ts;
                                /*
                                 * rcv timestamp of previous pktflow source's
                                 * last pak, or ABS_TIME_0 if no previous
                                 * pktflow source existed
                                 */

#if HAVE_FCC
    VQEC_DPCHAN_RCC_FIELDS
                                /* RCC fields */
#endif  /* HAVE_FCC */
} vqec_dpchan_t;

/**
 * Per-packet actions that allow a packet to be dropped, Q'ed or accepted
 * based on the RCC state-machine state.
 */
typedef
enum vqec_dpchan_pak_actions_
{
    /**
     * Drop the packet.
     */
    VQEC_DP_CHAN_PAK_ACTION_DROP,
    /**
     * Accept the packet, and deliver it to the next element.
     */
    VQEC_DP_CHAN_PAK_ACTION_ACCEPT,
    /**
     * Queue the packet locally, until further notice.
     */
    VQEC_DP_CHAN_PAK_ACTION_QUEUE,

} vqec_dpchan_pak_actions_t;

/**
 * Packet input event types.
 */
typedef
enum vqec_dpchan_rx_pak_ev_
{
    /**
     * Primary packet received.
     */
    VQEC_DP_CHAN_RX_PRIMARY_PAK,
    /**
     * Repair packet received.
     */
    VQEC_DP_CHAN_RX_REPAIR_PAK,
    /**
     * FEC packet received.
     */
    VQEC_DP_CHAN_RX_FEC_PAK,

} vqec_dpchan_rx_pak_ev_t;

/**
 * This method is used to process / deliver a packet event to the RCC 
 * state machine during the time when a RCC burst is active. It allows the
 * state machine to filter incoming packets and transition it's state. The
 * method is invoked from the RTP input stream(s) receive method.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] ev Packet event type, i.e., primary / repair.
 * @param[in] pak Pointer to the packet.
 * @param[in] cur_time Current absolute time.
 * @param[out] boolean True if the packet if the should be processed by
 * the RTP IS, false if it should be dropped. In thus current implementation
 * true is the only returned value.
 */
boolean
vqec_dpchan_pak_event_update_state(vqec_dpchan_t *chan, 
                                   vqec_dpchan_rx_pak_ev_t ev, 
                                   vqec_pak_t *pak, abs_time_t cur_time); 

/**
 * Eject a (nat) packet on the repair session to the control-plane via a upcall
 * socket. A packet header is built which will be appended to the packet.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] pak Pointer to the packet.
 * @param[in] id Input stream id on which the packet is received.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if the packet
 * is successfully enqueued to the socket.
 */
vqec_dp_error_t
vqec_dpchan_eject_pak(vqec_dpchan_t *chan, 
                      vqec_dp_isid_t id, vqec_pak_t *pak);

/**
 * HELPER method to display a channel's name: the contents
 * are printed into a static buffer, and a pointer to that buffer
 * is returned -  do not "cache" the return value!
 */
char *
vqec_dpchan_print_name(vqec_dpchan_t *chan);

/**
 * Assert an IRQ for an upcall event.
 *
 * @param[in] Pointer to the channel.
 * @param[in] dev Type of the device generating the event. At present
 * primary, repair IS and dpchan natively can generate events.
 * @param[in] id Id of the input stream which generates the event.
 * @param[in] event The local event which is asserting the IRQ. 
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if either a
 * previous interrupt for the same device is pending, or if a new IRQ is
 * successfully sent to the control-plane. Even if sending the IRQ to the
 * control-plane fails, the event is still cached in the reason register 
 * associated with the IRQ which can be polled by the control-plane at
 * a subsequent stage. Therefore, callers may ignore the return code.
 */
vqec_dp_error_t
vqec_dpchan_tx_upcall_ev(vqec_dpchan_t *chan, 
                         vqec_dp_upcall_device_t dev, 
                         vqec_dp_isid_t id,
                         vqec_dp_upcall_irq_reason_code_t event);

#if HAVE_FCC 
/**
 * Notification from FSM-to-Channel: RCC burst has completed successfully,
 * and the channel can take appropriate actions.
 *
 * @param[in] chan Pointer to the dp channel.
 */
void 
vqec_dpchan_rcc_success_notify(vqec_dpchan_t *chan);

/**
 * Notification from FSM-to-Channel: RCC burst has been aborted, and
 * and the channel can take appropriate actions.
 *
 * @param[in] chan Pointer to the dp channel.
 */
void
vqec_dpchan_rcc_abort_notify(vqec_dpchan_t *chan);

/**
 * Notification from FSM-to-Channel: Time-to-join epoch has arrived;
 * the channel can perform join-time actions.
 *
 * @param[in] chan Pointer to the dp channel.
 */
void
vqec_dpchan_rcc_join_notify(vqec_dpchan_t *chan);

/**
 * Notification from FSM-to-Channel: send NCSI now.
 *
 * @param[in] chan Pointer to the dp channel.
 */
void
vqec_dpchan_rcc_ncsi_notify(vqec_dpchan_t *chan);

/**
 * Notification from FSM-to-Channel: Time-to-en-ER has arrived.
 * Update ER state for sub-components.
 *
 * @param[in] chan Pointer to the dp channel.
 */
void
vqec_dpchan_rcc_er_en_notify(vqec_dpchan_t *chan);

#endif /* HAVE_FCC */

/**
 * Notification from the dpchan that the FEC L and D values have been updated.
 * An upcall notification is sent to the control-plane.
 * 
 * @param[in] chanid ID of the channel.
 */ 
void
vqec_dpchan_fec_update_notify(uint32_t chanid);

/**
 * Access the pcm instance: used by the RTP IS to call "deliver" on
 * the instance.
 */
static inline vqec_pcm_t *
vqec_dpchan_pcm_ptr (vqec_dpchan_t *chan)
{
    return (&chan->pcm);
}

/**
 * Access the fec instance: used by the FEC RTP IS to call "deliver" on
 * the FEC instance.
 */
static inline vqec_fec_t *
vqec_dpchan_fec_ptr (vqec_dpchan_t *chan)
{
    return (chan->fec);
}

/**
 * If the channel RCC state machine is in a post-ER enabled state.
 * 
 * @param[in] chan Pointer to the channel - must be null-checked prior to call.
 * @param[out] boolean True if the state-machine is in a state in which ER 
 * should be enabled (wait_end_burst / abort / success states). It always
 * returns true for the non-RCC case including if RCC is not enabled.
 */
boolean
vqec_dpchan_sm_state_post_er_enable(vqec_dpchan_t *chan);

/**
 * Logs a data point for the join-delay histogram.
 *
 * @param[in] msec_delay Value observed for a channel change delay,
 *                       starting with the request of a join and concluding
 *                       upon the receipt of the first primary stream packet
 */
void
vqec_dp_chan_hist_join_delay_add_data(uint32_t msec_delay);

/**
 * Records the arrival of a channel's first primary packet.
 *
 * @param[in] chan - channel which has not (previously) received a primary pkt
 * @param[in] pak  - first primary packet to be accepted for chan.
 *                   It is the caller's responsibility to ensure the packet
 *                    is the first and is acceptable (e.g. it's RTP header
 *                    has been validated, the channel is ready to accept it
 *                    if doing RCC, etc.)
 * @param[in] cur_time - current time 
 */
void
vqec_dpchan_pak_event_record_first_primary(vqec_dpchan_t *chan,
                                           vqec_pak_t *pak,
                                           abs_time_t cur_time);

/**
 * Performs the following functions upon receipt of a packet (inlined for
 * efficiency) from an IS:
 *
 * - Determines (and returns) what action should be taken for the packet
 *    (e.g. accept, drop, queue):
 *      o If RCC is not enabled (or supported), packets are always accepted.
 *      o If RCC is enabled, packets may be accepted, queued or dropped
 *        based on the current state of the RCC state-machine.
 *
 * - For all packets which are accepted:
 *      o If the packet is the first primary packet received by the channel,
 *          - records the channel's first_primary_ts, first_primary_ev_ts,
 *            and first_primary_seq
 *          - logs a data point in the join-delay histogram
 *
 * - For packets which are accepted when RCC is enabled and not 
 *   completed/aborted:
 *      o For primary packets:
 *          - update the channel's last_primary_pak_ts field
 *          - if the packet is the first primary packet received by the chan:
 *             o trigger a "first primary" event in the RCC state machine
 *      o For repair packets:
 *          - update the channel's last_repair_pak_ts field
 *          - if the packet is the first repair packet received by the chan:
 *             o record the channel's first_repair_seq and first_repair_ev_ts
 *             o trigger a "first repair" event in the RCC state machine
 *
 * @param[in] chan Pointer to the channel - must be null-checked prior to call.
 * @param[in] ev Type of packet event, primary, repair or FEC.
 * @param[in] pak Input packet.
 * @param[in] cur_time Current timestamp - may be specified as 0, in which
 * case the method will acquire system time.
 * @param[out] vqec_dpchan_pak_actions_t Output actions, i.e., if the
 * packet is to be accepted, dropped or queued.
 */
static inline vqec_dpchan_pak_actions_t
vqec_dpchan_pak_event (vqec_dpchan_t *chan, 
                       vqec_dpchan_rx_pak_ev_t ev, 
                       vqec_pak_t *pak, 
                       abs_time_t cur_time)
{

#ifndef HAVE_FCC
    if ((ev == VQEC_DP_CHAN_RX_PRIMARY_PAK) && (!chan->rx_primary)) {
        vqec_dpchan_pak_event_record_first_primary(chan, pak, cur_time);
    }
    return (VQEC_DP_CHAN_PAK_ACTION_ACCEPT);
#else /* HAVE_FCC */

    if (chan->state >= VQEC_DP_SM_STATE_FIN_SUCCESS || !chan->rcc_enabled) {
        /* The state-machine is in a final state. */ 
        if (!chan->rx_primary &&
            (ev == VQEC_DP_CHAN_RX_PRIMARY_PAK)) {
            /* 
             * Is needed to handle the case when the 1st primary is received 
             * after the  end-of the specified rcc-burst interval.
             */
            (void)vqec_dpchan_pak_event_update_state(chan, ev, 
                                                     pak, cur_time);
        }
        return (VQEC_DP_CHAN_PAK_ACTION_ACCEPT);
    }

    if (chan->state == VQEC_DP_SM_STATE_INIT) {
        if (ev == VQEC_DP_CHAN_RX_REPAIR_PAK) {
            /* Have not received an APP from control-plane yet. */ 
            return (VQEC_DP_CHAN_PAK_ACTION_QUEUE);
        } else {
            /* Except repair drop any other packet types. */
            return (VQEC_DP_CHAN_PAK_ACTION_DROP);
        } 
    }

    switch (ev) {
    case VQEC_DP_CHAN_RX_PRIMARY_PAK:
    case VQEC_DP_CHAN_RX_REPAIR_PAK:
        if (!vqec_dpchan_pak_event_update_state(chan, ev, 
                                                pak, cur_time)) {
            return (VQEC_DP_CHAN_PAK_ACTION_DROP);
        }
        break;
        
    default:
        break;
    }

    return (VQEC_DP_CHAN_PAK_ACTION_ACCEPT);
#endif /* HAVE_FCC */
}

/**---------------------------------------------------------------------------
 * Retrieves post-repair XR statistics.
 * 
 * @param[in] dpchan Pointer to dpchan instance.
 * @param[in] reset_xr_stats Specifies whether to reset the post-ER XR stats.
 * @param[out] post_er_stats Pointer to copy the stats info into.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/
static inline vqec_dp_error_t
vqec_dpchan_get_post_er_stats (vqec_dpchan_t *dpchan,
                               boolean reset_xr_stats,
                               rtcp_xr_post_rpr_stats_t *post_er_stats)
{
    if (dpchan) {
        return vqec_oscheduler_get_post_er_stats(
            &vqec_dpchan_pcm_ptr(dpchan)->osched,
            reset_xr_stats,
            post_er_stats);
    }

    return VQEC_DP_ERR_INVALIDARGS;
}

/**---------------------------------------------------------------------------
 * Retrieves XR Media Acquisition statistics.
 * 
 * @param[in] dpchan Pointer to dpchan instance.
 * @param[out] xr_ma_stats Pointer to copy the MA stats info into
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/
static inline vqec_dp_error_t
vqec_dpchan_get_xr_ma_stats (vqec_dpchan_t *dpchan,
                             rtcp_xr_ma_stats_t *xr_ma_stats)
{

#ifdef HAVE_FCC
    if (dpchan->rcc_enabled) {
        xr_ma_stats->first_repair_ts = dpchan->first_repair_ts;
        xr_ma_stats->rcc_expected_pts = dpchan->pts_val;
        xr_ma_stats->num_dup_pkts = dpchan->pcm_endburst_snap.dup_paks;
        xr_ma_stats->first_prim_to_last_rcc_seq_diff = 
            dpchan->pcm.stats.first_prim_to_last_rcc_seq_diff;

        /* Anything less than SUCCESS means the RCC is still in progress. 
         * Don't treat this as an error, user might change channels before
         * the RCC is done. */
        if (dpchan->state < VQEC_DP_SM_STATE_FIN_SUCCESS) {
            xr_ma_stats->status = ERA_RCC_SUCCESS;
        } else {
            xr_ma_stats->status = ERA_RCC_CLIENT_FAIL;
        }
    } else {
        xr_ma_stats->status = ERA_RCC_NORCC;
    }
#endif /* HAVE_FCC */

    xr_ma_stats->first_mcast_ext_seq = dpchan->first_primary_seq;
    xr_ma_stats->join_issue_time = dpchan->join_issue_time;
    xr_ma_stats->first_primary_ts = dpchan->first_primary_ts;
    xr_ma_stats->burst_end_time = dpchan->burst_end_time;

    return (VQEC_DP_ERR_OK);
}

#endif /* __VQEC_DPCHAN_H__ */

