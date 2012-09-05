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
 * Description: VQEC channel module private definitions.
 *
 * Documents:
 *
 *****************************************************************************/
#ifndef __VQEC_CHANNEL_PRIVATE_H__
#define __VQEC_CHANNEL_PRIVATE_H__

#include "vqec_channel_api.h"
#include "rtp_era_recv.h"
#include "rtp_repair_recv.h"
#include "vqe_token_bucket.h"
#include "vqec_nat_interface.h"
#include "vqec_sm.h"
#include <utils/mp_tlv_decode.h>
#if HAVE_FCC
#include <rtp/rcc_tlv.h>
#endif
#include "vqec_ifclient_fcc.h"

/**
 * Storage for all global data within the channel module.
 */
typedef struct vqec_channel_module_t_ {
    id_table_key_t  id_table_key;        /*!< ID manager table key */

    /**
     * FEC default block size (LxD) in packets.
     */
    uint16_t fec_default_block_size;
    /**
     * Maximum channels to be supported.
     */
    uint32_t max_channels;
    /**
     * Number of tuners currently bound with RCC resources (i.e., in-progress
     * or previously successful RCC since the last bind on the channel).
     */
    uint32_t num_current_rccs;

    /*
     * Entire channel module statistics. 
     */
    uint64_t total_app_timeout_counter;    /*!< total app timeout counter */
    uint64_t total_null_app_paks;          /*!< total null app packets */
    uint64_t total_app_paks;               /*!< total app packets received */
    uint64_t total_app_parse_errors;       /*!< total app parse errors */
    vqec_ifclient_stats_t historical_stats_cumulative;
    vqec_ifclient_stats_t historical_stats;
                                           /*!<
                                            *!< Stats collected from all
                                            *!< previously-tuned channels.
                                            *!< These get bumped only upon
                                            *!< a channel going inactive.
                                            */
    vqec_event_t *poll_ev;                 /*!< channel polling event  */
    char *rtcp_iobuf;                      /*!< RTCP packet buffer */
    uint32_t rtcp_iobuf_len;               /*!< RTCP packet buffer length */
    VQE_LIST_HEAD(,vqec_chan_) channel_list; /*!< List of active channels */
} vqec_channel_module_t;

extern vqec_channel_module_t *g_channel_module;

/*
 * Tuners associated with a channel are stored in a list,
 * with each element having type vqec_chan_tuner_entry_t.
 */
typedef struct vqec_chan_tuner_entry_ {
    VQE_TAILQ_ENTRY(vqec_chan_tuner_entry_) list_obj;
    vqec_dp_tunerid_t tid;
} vqec_chan_tuner_entry_t;

/**
 * Channel statistics.
 */
typedef struct vqec_chan_stats_
{
    /*
     * Error repair stats
     */
    uint64_t generic_nack_counter;      /*!< total generic nack packets sent */
    uint64_t total_repairs_requested;   /*!< total repair packets (contents
                                             of nacks) requested */
    uint64_t total_repairs_policed;     /*!< total repairs policed (unsent) */
    uint64_t total_repairs_unrequested; /*!< total repair packets (contents
                                             of nacks) unrequested */
    uint64_t fail_to_send_rtcp_counter; /*!< counter for fail to send 
                                             rtp/rtcp pak */
    uint64_t suppressed_jumbo_gap_counter; /*<! fail to report error if the 
                                                gap is too big. */
    uint32_t first_nack_repair_cnt;     /*!< repairs requested in first nack */

    /*
     * Nat injection / ejection statistics. 
     */
    uint64_t nat_injects_rtp;           /* Injected packets for rtp port */
    uint64_t nat_injects_rtcp;          /* Injected packets for rtcp port */
    uint64_t nat_inject_err_drops;      /* Injection error drops. */
    uint64_t nat_inject_input_drops;    /* Injection socket or ipc drops. */

    uint64_t nat_ejects_rtp;            /* Ejected packets for rtp port */
    uint64_t nat_ejects_rtcp;           /* Ejected packets for rtcp port */
    uint64_t nat_eject_err_drops;       /* Eject error drops. */

    /*
     * Active channel update stats
     */
    uint64_t active_cfg_updates;        /* Active channel config updates */

    /*
     * RCC admission control stats
     */
    uint64_t concurrent_rccs_limited;   /*
                                         * Number of RCCs limited because
                                         * of too many other active RCC
                                         * channels
                                         */
    
} vqec_chan_stats_t;


/*-----
 * Upcall generation number management.
 *-----*/
typedef
struct vqec_chan_upcall_data_
{
    /**
     * Last received generation number.
     */
    uint32_t upcall_last_generation_num;
    /**
     * Upcall event counter.
     */
    uint32_t upcall_events;
    /**
     * Lost event count.  
     */
    uint32_t upcall_lost_events;
    /**
     * Repeated or out-of-order event count.  
     */
    uint32_t upcall_repeat_events;
    /**
     * Error event count.  
     */
    uint32_t upcall_error_events;
    /**
     * IRQ data acknowledgments received.   
     */
    uint32_t upcall_ack_inputs;
    /**
     * IRQ data acknowledgments that were null.   
     */
    uint32_t upcall_ack_nulls;
    /**
     * Data acknowledgment messages that were malformed / in-error.   
     */
    uint32_t upcall_ack_errors;
    /**
     * Dataplane generation Id for channel (last cached value).  
     */
    uint32_t dp_generation_id;

} vqec_chan_upcall_data_t;


/*----
 * Channel change stats fields from STB.
 *----*/
#define VQEC_CC_STB_STATS_FIELDS                          \
    boolean ir_time_valid; /* true if ir_time field is valid */ \
    abs_time_t ir_time;   /* ir channel change time */\
    abs_time_t pat_time;  /* pat found time */\
    abs_time_t pmt_time;  /* pmt found time */\
    abs_time_t fst_decode_time;  /* First packet decode time at STB */\
    uint64_t last_decoded_pts;   /* last decoded PTS.*/\
    uint32_t dec_picture_cnt;    /* decoded picture count */\ 
    abs_time_t display_time; /* first frame display time at STB */\
    uint64_t display_pts;    /* first frame display PTS at STB */\

/**
 * VQEC channel data structure.
 */
typedef
struct vqec_chan_
{
    /**
     * Allows channel to be linked in a list.
     */
    VQE_LIST_ENTRY(vqec_chan_) list_obj;
    /**
     * Opaque channel id.
     */
    vqec_chanid_t chanid;
    /**
     * Channel configuration.
     */
    vqec_chan_cfg_t cfg;
    /**
     * Time of last channel configuration update (after initial bind).
     * If the channel's config has not been updated, this will be zero'd.
     */
    abs_time_t last_cfg_update_ts;
    /**
     * Is channel shutdown, in a transition period waiting for tx
     * of multiple-bye(s).
     */
    boolean shutdown;                               
    /**
     * List of tuners associated with this channel.
     */
    VQE_TAILQ_HEAD(vqec_chan_tuner_head_,vqec_chan_tuner_entry_) tuner_list;
    /**
     * Control plane statistics
     */
    vqec_chan_stats_t stats;
    vqec_chan_stats_t stats_snapshot;
    /**
     * Input interface address which is to be used for all data transfers.
     */
    in_addr_t input_if_addr;
    /**
     * Initial channel bind time
     */
     abs_time_t bind_time;
    /**
     * Context-id for the decoder 
     */
     int32_t context_id;
    /**
     * The function pointers passed by STB
     */ 
     vqec_ifclient_get_dcr_info_ops_t  dcr_ops;
    /**
     * The function pointer for chan_event_cb
     */ 
    vqec_ifclient_chan_event_cb_t chan_event_cb; 
    /**
     * Byes to send at channel destroy.
     */
     int32_t bye_count;
    /**
     * per Bye-delay.
     */
     rel_time_t bye_delay;
    /**
     * Bye-timer event..
     */
     vqec_event_t *bye_ev;
                
    /*-----
     * Identifiers that correspond to sub-entities in the dataplane.
     *-----*/

    /**
     * Dataplane graph identifier associated with this channel.
     */
    vqec_dp_graphid_t graph_id;
    /**
     * Dataplane channel identifier associated with this channel.
     */
    vqec_dp_chanid_t dp_chanid;
    /**
     * Primary session's identifier in the dataplane.
     */
    vqec_dp_streamid_t prim_rtp_id;
    /**
     * Repair session's identifier in the dataplane.
     */
    vqec_dp_streamid_t repair_rtp_id;
    /**
     * FEC session 0 identifier in the dataplane.
     */
    vqec_dp_streamid_t fec0_rtp_id;
    /**
     * FEC session 1 identifier in the dataplane.
     */
    vqec_dp_streamid_t fec1_rtp_id;
    /**
     * stream identifiers in the input shim
     */
    struct {
        vqec_dp_streamid_t prim_rtp_id;
        vqec_dp_streamid_t repair_rtp_id;
        vqec_dp_streamid_t fec0_rtp_id;
        vqec_dp_streamid_t fec1_rtp_id;
    } inputshim;
    /**
     * stream identifiers in the output shim
     */
    struct {
        vqec_dp_streamid_t postrepair_id;
    } outputshim;

    /*-----
     * ERA session(s) for the channel.
     *-----*/

    /**
     * Primary session for the channel.
     */
    rtp_era_recv_t *prim_session;
    /**
     * Reapair session for the channel
     */
    rtp_repair_recv_t *repair_session;

    /*-----
     * Repair ports for rtp, rtcp used by the channel.
     *-----*/

    /**
     * Retransmission RTP port, which may be ephemeral (dp). 
     */
    uint16_t rtp_rtx_port;
    /**
     * Retransmission RTCP port, which may be ephemeral (cp).
     */
    uint16_t rtcp_rtx_port;
    /**
     * NAT traversal enabled
     */
    boolean nat_enabled;
    /** 
     * NAT id for the retransmission RTP port.
     */
    vqec_nat_bindid_t repair_rtp_nat_id;
    /** 
     * Has binding completed?
     */
    boolean rtp_nat_complete;                                                             
    /**
     * NAT id for the retransmission RTCP port.
     */
    vqec_nat_bindid_t repair_rtcp_nat_id;
    /** 
     * Has binding completed?
     */
    boolean rtcp_nat_complete;                                                             

    char nat_username[VQEC_NAT_API_NAME_MAX_LEN];

    /** 
     * NAT id for the primary RTP port.
     */
    vqec_nat_bindid_t prim_rtp_nat_id;
    /** 
     * Has binding completed?
     */
    boolean prim_rtp_nat_complete;                                                             
    /**
     * NAT id for the primary RTCP port.
     */
    vqec_nat_bindid_t prim_rtcp_nat_id;
    /** 
     * Has binding completed?
     */
    boolean prim_rtcp_nat_complete;                      

    /*-----
     * Source Filtering Attributes.
     *-----*/
    
    /**
     * Is a source filter configured for the channel streams?
     * This field caches the syscfg value of the same name upon channel bind.
     */
    boolean src_ip_filter_enable;

    /*-----
     * ER Attributes.
     *-----*/

    /**
     * ER: Is Error-repair enabled?    
     */
    boolean er_enabled;
    /**
     * ER: Repair trigger time?    
     */
    rel_time_t repair_trigger_time; 
    /**
     * ER: has polling been enabled?
     */
    boolean er_poll_active;
    /**
     * ER: repair policer enabled?
     */
    boolean er_policer_enabled;
    /**
     * ER: repair policer token bucket
     */
    token_bucket_info_t er_policer_tb;
    /**
     * ER: Absolute time at which next gap-report is scheduled:
     */
    abs_time_t next_er_sched_time;

    /*-----
     * FEC Attributes.
     *-----*/

    /**
     * Is FEC enabled?
     */
    boolean fec_enabled;


    /*-----
     * RCC Attributes.
     *-----*/

    /**
     * Is RCC enabled?
     */
    boolean rcc_enabled;

    /**
     * Is fast fill enabled?
     */
    boolean fast_fill_enabled;
    /**
     * fast fill time.
     */
    rel_time_t fast_fill_time;
    /**
     * total fast fill bytes
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
    /**
     * STB fast fill vectors.
     */
    vqec_ifclient_fastfill_cb_ops_t fastfill_ops;
    /**
     * Event for polling decoder stats
     */
#define STB_DCR_STATS_POLL_INTERVAL MSECS(250)
    vqec_event_t *stb_dcr_stats_ev;

#ifdef HAVE_FCC
    /**
     * Minimum RCC backfill requested in PLI-NAK.
     */
    rel_time_t rcc_min_fill;
    /**
     * Maximum RCC backfill requested in PLI-NAK.
     */
    rel_time_t rcc_max_fill;
    /**
     * Maximum amount of data (in msec) that can be fast filled during a memory
     * optimized RCC burst.
     */
    rel_time_t max_fastfill;
    /**
     * Actual backfill at IGMP join time.
     */
    rel_time_t act_backfill_at_join;
    /**
     * RCC state.
     */
    vqec_sm_state_t state;
    /**
     * RCC state-machine event mask (OR of all events generated).
     */
    uint32_t event_mask;
    /**
     * Rapid channel change start time.
     */
    abs_time_t rcc_start_time;
    /**
     * Has RCC been aborted.
     */
    boolean rcc_in_abort;
    /**
     * Event Q.
     */
    vqec_sm_event_q_t event_q[VQEC_SM_MAX_EVENTQ_DEPTH];
    /**
     * Instantaneous event Q depth.
     */
    uint32_t event_q_depth;
    /**
     * Next available index in event Q.
     */
    uint32_t event_q_idx;
    /**
     * Log circular buffer.
     */
    vqec_sm_log_t sm_log[VQEC_SM_LOG_MAX_SIZE];
    /**
     * Next index in circular buffer.
     */
    uint32_t sm_log_tail;
    /**
     * Last displayed log index in circular buffer.
     */
    uint32_t sm_log_head;

    /**---
     * Parameters / state derived from the APP. 
     *---*/

    /**
     * APP timer timeout interval (includes NAT, first repair packet time)
     */
    rel_time_t app_timeout_interval;
    /**
     * APP timer event.
     */
    vqec_event_t *app_timeout;
    /**
     * Has NAK-PLI been sent.
     */
    boolean nakpli_sent;
    /**
     * Time at which nakpli was sent.
     */
    abs_time_t nakpli_sent_time;
    /**
     * Sequence number of the first repair packet.
     */
    uint32_t start_seq_num;
    /**
     * The RTP timestamp that should be assigned to the APP packet.
     */
    uint32_t app_rtp_ts;
    /**
     * The system time at which app is received.
     */
    abs_time_t app_rx_ev_ts;
    /**
     * The packet receive time at which app is received
     */
    abs_time_t app_rx_ts;
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
     * ER holdoff time, relative to the earliest-join point.
     */
    rel_time_t er_holdoff_time;
    /**
     * Absolute time deadline for the arrival of the first repair sequence.
     */
    abs_time_t first_repair_deadline;
    /**
     * Has NCSI been sent.
     */
    boolean ncsi_sent;
    /**
     * NCSI send time.
     */
    abs_time_t ncsi_sent_time;
    /**
     * Event to fast-trigger ER.
     */
    vqec_event_t *er_fast_timer;
    /**
     * Time the RCC burst is done at, as indicated by DP upcall. There
     * may be some delay here from when the last packet is actually received.
     */
    abs_time_t burst_end_time;
#endif /* HAVE_FCC */

    vqec_chan_upcall_data_t upc_data;
    /**
     * Bandwidth of the FEC stream(s).
     */
    uint32_t fec_recv_bw;
    /**
     * Maximum receive bandwidth for RCC.
     */
    uint32_t max_recv_bw_rcc;
    /**
     * Maximum receive bandwidth for ER.
     */
    uint32_t max_recv_bw_er;
    /**
     * This specifies whether to use the RCC bandwidth value for ER requests.
     */
    boolean use_rcc_bw_for_er;
    /**
     * Callback function pointer to be called during the MRB transition event.
     */
    void (*mrb_transition_cb)(int32_t context_id);
    /**
     * Event pointer for the MRB transition event.
     */
    vqec_event_t *mrb_transition_ev;
    /**
     * First primary sequence number received on this channel.
     */
    vqec_seq_num_t first_primary_seq;
    /**
     * Indicates whether or not the value of first_primary_seq is valid.
     */
    boolean first_primary_valid;
    /**
     * stats information from STB
     */
    VQEC_CC_STB_STATS_FIELDS;
    /* Placeholder for the statistics snapshot at the time of "clear counters"*/
    uint64_t primary_rtp_expected_snapshot;
    int64_t primary_rtp_lost_snapshot;

    uint64_t primary_rtcp_inputs_snapshot;
    uint64_t primary_rtcp_outputs_snapshot;
    uint64_t repair_rtcp_inputs_snapshot;

    boolean cc_cleared;
} vqec_chan_t;


/*
 * vqec_chanid_to_chan()
 *
 * @param[in]  chanid    channel ID value
 * @return               pointer to corresponding channel object, or
 *                        NULL if no associated channel object
 */
vqec_chan_t *
vqec_chanid_to_chan(const vqec_chanid_t chanid);

#define VQEC_MAX_CHANNEL_IPV4_PRINT_NAME_SIZE \
(sizeof("rtp://XXX.XXX.XXX.XXX:65535")) 
/* (just a sample string to measure the max length) */

/**
 * Helper method to display a channel's name: the contents
 * are printed into a static buffer, and a pointer to that buffer
 * is returned -  do not "cache" the return value!
 *
 * @params[in] chan Pointer to channel
 * @params[out] char * Buffer containing descriptive name
 */
char *
vqec_chan_print_name(vqec_chan_t *chan);

/*******
 * NOTE: For any API in this category that it is for use by channel-
 * associated sub-components, the "shutdown" flag on the channel
 * must be checked, and no action taken if the flag is set.
 ********/

/*
 * Retrieve the latest udp-nat port mappings for the channel.
 *
 * @param[in] chan Pointer to the channel instance.
 * @param[out] rtp_port RTP external mapped udp port.
 * @param[out] rtcp_Port RTCP external mapped udp port.
 * @param[out] boolean The method returns true if a valid mapping
 * is returned for the rtp and rtcp ports, otherwise returns false.
 */
boolean
vqec_chan_get_rtx_nat_ports(vqec_chan_t *chan, 
                            in_port_t *rtp_port, in_port_t *rtcp_port); 

#if HAVE_FCC
/*
 * Process an APP packet: the packet is received on the repair
 * RTCP session, and after pre-processing is handed to the 
 * channel instance.
 *
 * @param[in] chan Pointer to the channel instance.
 * @param[in] p_rcc_tlv A List of TLVs that are present in the
 * APP packet.
 */
void
vqec_chan_process_app(vqec_chan_t *chan,
                      ppdd_tlv_t *p_rcc_tlv);

/*
 * A notification from a channel's child repair session that
 * rudimentary RTCP APP message parse has failed, and RCC is
 * to be aborted.
 *
 * @param[in] chan Pointer to the channel instance.
 */
void
vqec_chan_app_parse_failure(vqec_chan_t *chan);

/*
 * This method is to be invoked by the CP RCC state-machine
 * when it transitions to the Abort state. The callback is used to
 * perform operations in both control & dataplane that will initiate
 * slow channel changes.
 *
 * @param[in] chan Pointer to the channel instance.
 */
void
vqec_chan_rcc_abort_notify(vqec_chan_t *chan);

/**
 * Process an upcall abort event which is received on the channel
 * from the dataplane. RCC is to be aborted when this event is
 * received. At the moment there is no data associated with this event.
 *
 * @param[in] chan Pointer to the channel instance.
 */
void
vqec_chan_upcall_abort_event(vqec_chan_t *chan);

/**
 * Process an upcall fast fill event which is received on the channel
 * from the dataplane. Fast fill is finished when this event is
 * received. At the moment there is no data associated with this event.
 *
 * @param[in] chan Pointer to the channel instance.
 */
void
vqec_chan_upcall_fast_fill_event(vqec_chan_t *chan,
                                 vqec_dp_upc_fast_fill_data_t *fast_fill_data);

/**
 * Process an upcall NCSI event which is received on the channel
 * from the dataplane. The NCSI packet is to be sent / ER actions
 * taken when this event is received.
 *
 * @param[in] chan Pointer to the channel instance.
 * @param[in] ncsi_data Pointer to the NCSI event data.
 */
void
vqec_chan_upcall_ncsi_event(vqec_chan_t *chan, 
                            vqec_dp_upc_ncsi_data_t *ncsi_data);

/**
 * Process an upcall burst done event which is received on the channel
 * from the dataplane.  When this event is received, the CP knows that the RCC
 * burst (including any associated error repairs) has been completed.
 *
 * @param[in] chan Pointer to the channel instance.
 */
void
vqec_chan_upcall_burst_done_event(vqec_chan_t *chan);

#endif /* HAVE_FCC */

/**
 * Process a primary inactive upcall event received for the channel.
 *
 * @param[in] chan Pointer to the channel.
 */
void
vqec_chan_upcall_prim_inactive_event(vqec_chan_t *chan);

/**
 * Process a FEC update upcall event received for the channel.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] fec_data Pointer to the FEC update event data.
 */ 
void
vqec_chan_upcall_fec_update_event(vqec_chan_t *chan,
                                  vqec_dp_upc_fec_update_data_t *fec_data);

/*
 * Get a channel's repair session instance. The method may return
 * null if the session does not exist. Callers must check for validity of 
 * the pointer.
 * 
 * @param[in] chan Pointer to the channel instance.
 * @param[out] rtp_repair_recv_t* Pointer to the repair session.
 */
rtp_repair_recv_t *
vqec_chan_get_repair_session(vqec_chan_t *chan);

/*
 * Get a channel's primary session instance. The method may return
 * null if the session does not exist. Callers must check for validity of 
 * the pointer.
 * 
 * @param[in] chan Pointer to the channel instance.
 * @param[out] rtp_era_recv_t* Pointer to the primary session.
 */
rtp_era_recv_t *
vqec_chan_get_primary_session(vqec_chan_t *chan);


/*
 * Event handler to poll decoder for channel change time stats. Once the 
 * stats are retrieved (as indicated by display_pts > 0), the event 
 * will be stopped. 
 *
 * @param unused_evptr - Pointer to the event UNUSED
 * @param unused_fd - File descriptor UNUSED
 * @param unused_event - Event Type UNUSED
 * @param arg - pointer to channel object
 */
void 
vqec_chan_dcr_stats_event_handler(const vqec_event_t *const unused_evptr,
                                  int unused_fd,
                                  short unused_event,
                                  void *arg);

/**
 * Collect XR MA stats and populate structure. Some of the fields must
 * be retrieved from the DP and should be filled in before this is called
 *
 * @param[in] chanid Identifier of the channel
 * @param[in] force Force sending an MA message. This does not wait for all
 *                data to be present, and sends whatever is available.
 * @param[out] xr_ma_stats Output pointer to location of stats
 */
void
vqec_chan_collect_xr_ma_stats(vqec_chan_t *chan,
                              boolean force,
                              rtcp_xr_ma_stats_t *xr_ma_stats);

/*
 * Collect XR DC stats and populate structure. Some of the fields must
 * be retrieved from the DP and should be filled in before this is called
 *
 * @param[in] chanid Identifier of the channel
 * @param[out] xr_dc_stats Output pointer to location of stats
 */
void
vqec_chan_collect_xr_dc_stats(vqec_chan_t *chan,
                              rtcp_xr_dc_stats_t *xr_dc_stats);


/**
 * Inlined accessors for upcall event / session support.
 */ 
static inline vqec_chan_upcall_data_t *
vqec_chan_upc_data_ptr (vqec_chan_t *chan)
{
    return (&chan->upc_data);
}
static inline vqec_dp_streamid_t
vqec_chan_primary_session_dp_id (vqec_chan_t *chan)
{
    return (chan->prim_rtp_id);
}
static inline vqec_dp_streamid_t
vqec_chan_repair_session_dp_id (vqec_chan_t *chan)
{
    return (chan->repair_rtp_id);
}
static inline vqec_chanid_t 
vqec_chan_to_chanid (vqec_chan_t *chan)
{
    return (chan->chanid);
}

/**
 * Inlined actions for the max_receive_bandwidth transition event.
 */
static inline void vqec_chan_mrb_transition_actions (vqec_chan_t *chan,
                                                     boolean timed_out)
{
    if (!timed_out && chan->mrb_transition_ev) {
        vqec_event_stop(chan->mrb_transition_ev);
    }

    chan->use_rcc_bw_for_er = FALSE;
    if (chan->mrb_transition_cb) {
        chan->mrb_transition_cb(chan->context_id);
    }
}

/**
 * Get the maximum receive bandwidth value to send up to the VQE-S for the RCC
 * case.  This number will already have the FEC bandwidth subtracted out from
 * it, if necessary.
 *
 * @param[in] chan Pointer of the channel.
 * @param[out] uint32_t Returns the bandwidth value to be sent to the server.
 */
uint32_t vqec_chan_get_max_recv_bw_rcc(vqec_chan_t *chan);

/**
 * Get the maximum receive bandwidth value to send up to the VQE-S for the ER
 * case.  This number will already have the FEC bandwidth subtracted out from
 * it, if necessary.
 *
 * @param[in] chan Pointer of the channel.
 * @param[out] uint32_t Returns the bandwidth value to be sent to the server.
 */
uint32_t vqec_chan_get_max_recv_bw_er(vqec_chan_t *chan);

/**---------------------------------------------------------------------------
 * Poll the STB to get channel change information. 
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] cur_time Current system time. 
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_update_dcr_stats(vqec_chan_t *chan);


#endif /* __VQEC_CHANNEL_PRIVATE_H__ */
