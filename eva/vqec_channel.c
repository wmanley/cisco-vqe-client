/*
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2009-2010, 2012 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_channel.c
 *
 * Description: VQEC channel module implementation.  The channel module
 * implements a database of all active channels (all channels for which at
 * least one tuner is bound), and provides APIs for creating and deleting
 * channels, binding them to a tuner, etc.
 *
 * Documents:
 *
 *****************************************************************************/

#include "id_manager.h"
#include "utils/queue_plus.h"
#include "utils/vam_types.h"
#include "utils/vqe_token_bucket.h"
#include "utils/mp_mpeg.h"
#include "vam_util.h"
#include "vqe_hash.h"
#include "vqec_assert_macros.h"
#include "vqec_gap_reporter.h"
#include "vqec_syscfg.h"
#include "vqec_dp_api.h"
#include "vqec_channel_api.h"
#include "vqec_channel_private.h"
#include "vqec_drop.h"
#include "vqec_debug.h"
#include "vqec_ifclient.h"
#include "vqec_ifclient_private.h"
#include "vqec_url.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif // _VQEC_UTEST_INTERPOSERS

vqec_channel_module_t *g_channel_module = NULL;

/**
 * For PCM to allocate FEC buffer 
 * If there are no fec information in the channel DB when box bootup,
 * we will use this parameter to allocate FEC buffer, given FEC is 
 * enabled in channel. 100 is default max size for MPEG COP#3 
 */
#define FEC_DEFAULT_BLOCK_SIZE 100;

/**
 * Define max fec block size, this means that VQE-C will only 
 * support FEC block size no larger than this value.
 * The reason we define this value here is to avoid very large 
 * burst request at RCC. 
 */
#define FEC_MAX_BLOCK_SIZE 256

/**
 * Store the max fec_block_size value while systems is on.
 * Whenever a larger block size is found from a certain channel
 * this value is updated. 
 */
uint16_t max_fec_block_size_observed = FEC_DEFAULT_BLOCK_SIZE;

uint16_t vqec_chan_get_max_fec_block_size_observed (void)
{
    return max_fec_block_size_observed;
}

#define VQEC_CHAN_DEBUG(arg...) VQEC_DEBUG(VQEC_DEBUG_CPCHAN, arg)

/**---------------------------------------------------------------------------
 * Init / deinit / bind / unbind / counters.
 *---------------------------------------------------------------------------*/ 

#if HAVE_FCC
static inline boolean
vqec_chan_is_rcc_aborted (vqec_chan_t *chan)
{
    return (chan->rcc_in_abort);
}

static inline void
vqec_chan_set_rcc_aborted (vqec_chan_t *chan)
{
    chan->rcc_in_abort = TRUE;
}

/**---------------------------------------------------------------------------
 * Return the abort reason for a channel's RCC operation.
 * If the channel did not experience an RCC abort, then the "abort_reason"
 * is left unchanged, and the function returns FALSE.
 *
 * @param[in] chan                         Channel for which RCC operation
 *                                          is performed
 * @param[in] rcc_data                     RCC data collected from the data
 *                                          plane
 * @param[out] abort_reason                Abort code for RCC operation
 * @param[out] boolean                     Did the channel experience an
 *                                          an RCC abort?
 *---------------------------------------------------------------------------*/
static boolean
vqec_chan_rcc_abort_reason (vqec_ifclient_rcc_abort_t *abort_reason,
                            vqec_chan_t *chan,
                            vqec_dp_rcc_data_t *rcc_data)
{

    if (!chan->rcc_enabled ||
        rcc_data->rcc_success ||
        !rcc_data->rcc_in_abort) {
        /* RCC either wasn't enabled, succeeded, or is in progress */
        return FALSE;
    }

    if (chan->event_mask & (1 << VQEC_EVENT_RECEIVE_INVALID_APP)) {
        *abort_reason = VQEC_IFCLIENT_RCC_ABORT_RESPONSE_INVALID;
    } else if (chan->event_mask & (1 << VQEC_EVENT_RECEIVE_NULL_APP)) {
        *abort_reason = VQEC_IFCLIENT_RCC_ABORT_SERVER_REJECT;
    } else if (chan->event_mask & (1 << VQEC_EVENT_RCC_START_TIMEOUT)) {
        if (chan->nakpli_sent) {
            *abort_reason = VQEC_IFCLIENT_RCC_ABORT_RESPONSE_TIMEOUT;
        } else {
            *abort_reason = VQEC_IFCLIENT_RCC_ABORT_STUN_TIMEOUT;
        }
    } else if (rcc_data->event_mask & (1 << VQEC_DP_SM_EVENT_INTERNAL_ERR)) {
        *abort_reason = VQEC_IFCLIENT_RCC_ABORT_OTHER;
    } else if (rcc_data->event_mask & (1 << VQEC_DP_SM_EVENT_TIME_FIRST_SEQ)) {
        *abort_reason = VQEC_IFCLIENT_RCC_ABORT_BURST_START;
    } else if (rcc_data->event_mask &
               (1 << VQEC_DP_SM_EVENT_ACTIVITY_TIMEOUT)) {
        *abort_reason = VQEC_IFCLIENT_RCC_ABORT_BURST_ACTIVITY;
    }  else {
        *abort_reason = VQEC_IFCLIENT_RCC_ABORT_OTHER;
    }

    return (TRUE);
}
#endif

/*
 * vqec_chanid_to_chan()
 *
 * @param[in]  chanid    channel ID value
 * @return               pointer to corresponding channel object, or
 *                        NULL if no associated channel object
 */
vqec_chan_t *
vqec_chanid_to_chan (const vqec_chanid_t chanid)
{
    uint ret_code;
    
    if (!g_channel_module) {
        return (NULL);
    } else {
        return (id_to_ptr(chanid, &ret_code, g_channel_module->id_table_key));
    }
}

/*
 * Searches the active channel database for the given channel, which
 * is identified by its (primary_dest_addr, primary_dest_port) pair.
 *
 * @param[in]  ip            destination IP address of desired channel.
 * @param[in]  port          destination port of desired channel
 * @param[out] vqec_chan_t * Returns a pointer to the found channel, or
 *                           NULL if channel does not exist in the database.
 */
vqec_chan_t *
vqec_chan_find (in_addr_t ip, in_port_t port)
{
    vqec_chan_t *curr_chan, *matching_chan = NULL;

    if (!g_channel_module) {
        return NULL;
    }
    VQE_LIST_FOREACH(curr_chan, &g_channel_module->channel_list, list_obj) {
        if ((curr_chan->cfg.primary_dest_addr.s_addr == ip) &&
            (curr_chan->cfg.primary_dest_port == port)) {
            matching_chan = curr_chan;
            break;
        }
    }
    return (matching_chan);
}

/**
 * Returns a descriptive error string corresponding to an vqec_chan_err_t
 * error code.
 *
 * @param[in] err           Error code
 * @param[out] const char * Descriptive string corresponding to error code
 */ 
const char *
vqec_chan_err2str (vqec_chan_err_t err)
{
    switch (err) {
    case VQEC_CHAN_ERR_OK:
        return "Operation succeeded";
    case VQEC_CHAN_ERR_INVALIDARGS:
        return "Invalid arguments supplied";
    case VQEC_CHAN_ERR_NOMORECHANNELS:
        return "Channel limit has been reached";
    case VQEC_CHAN_ERR_NOTINITIALIZED:
        return "Channel module not initialized";
    case VQEC_CHAN_ERR_NOSUCHCHAN:
        return "Requested channel does not exist";
    case VQEC_CHAN_ERR_NOMEM:
        return "Memory not available";
    case VQEC_CHAN_ERR_NOROOM:
        return "Insufficient buffer size to copy requested info";
    case VQEC_CHAN_ERR_INTERNAL:
        return "Internal error";
    default:
        return "Operation failed";
    }
}

/**
 * Update the FEC receive bandwidth value for the channel, based on the cached
 * FEC L and D values.
 */
static void vqec_chan_update_fec_bw (vqec_chan_t *chan)
{
    vqec_chan_cfg_t *cfg = &chan->cfg;

    /*
     * To calculate the FEC receive bandwidth, the following equations
     * are used:
     *
     *   For the 1-D case (L = 1):
     *     fec_bw = (prim_bw / D)
     *   For the 2-D case (L >= 4):
     *     fec_bw = (prim_bw / D) + (prim_bw / L)
     */
    VQEC_DEBUG(VQEC_DEBUG_CPCHAN,
               "fec_recv_bw updated; oldval = %d\n", chan->fec_recv_bw);
    if (cfg && cfg->fec_d_value) {
        /* both 1-D and 2-D cases */
        chan->fec_recv_bw = cfg->primary_bit_rate / cfg->fec_d_value;
        if (cfg->fec_l_value >= 4) {
            /* 2-D case only */
            chan->fec_recv_bw +=
                cfg->primary_bit_rate / cfg->fec_l_value;
        }
    }
    VQEC_DEBUG(VQEC_DEBUG_CPCHAN,
               "fec_recv_bw updated; newval = %d (L=%d, D=%d)\n",
               chan->fec_recv_bw, cfg->fec_l_value, cfg->fec_d_value);
}

/**
 * need to cache FEC information in channel DB if needed
 *
 * @param[in] chan channel to be updated.
 */
static void 
vqec_chan_cache_fec_info (vqec_chan_t *chan)
{
    vqec_fec_info_t fec_info;
    channel_cfg_t *cfg = NULL;
    uint16_t block_size;

    if (!chan) {
        return;
    }

    if (chan->dp_chanid != VQEC_DP_CHANID_INVALID) {
        if (vqec_dpchan_fec_need_update(chan->dp_chanid, &fec_info)) {
            cfg = cfg_get_channel_cfg_from_orig_src_addr (
                chan->cfg.primary_dest_addr,
                chan->cfg.primary_dest_port);
            if (cfg) {
                cfg->fec_info.fec_l_value = fec_info.fec_l_value;
                cfg->fec_info.fec_d_value = fec_info.fec_d_value;
                cfg->fec_info.fec_order = fec_info.fec_order;
                cfg->fec_info.rtp_ts_pkt_time = 
                    TIME_GET_R(usec, fec_info.rtp_ts_pkt_time);
            }

            /* update the max_fec_block_size_observed if necessary */
            block_size = fec_info.fec_l_value * fec_info.fec_d_value;
            if ((block_size > max_fec_block_size_observed) &&
                (block_size <= FEC_MAX_BLOCK_SIZE))  {
                VQEC_DEBUG(VQEC_DEBUG_CPCHAN,
                           "previous max_fec_block_size_observed = %u\n",
                           (uint32_t)max_fec_block_size_observed);           
                max_fec_block_size_observed = block_size;
                VQEC_DEBUG(VQEC_DEBUG_CPCHAN,
                           "updated max_fec_block_size_observed = %u\n",
                           (uint32_t)max_fec_block_size_observed);           
            } else if (block_size > FEC_MAX_BLOCK_SIZE) {
                VQEC_DEBUG(VQEC_DEBUG_CPCHAN,
                           "observed fec_block larger than defined MAX = %u\n",
                           (uint32_t)block_size);           
            }

            /* update the FEC receive bandwidth */
            vqec_chan_update_fec_bw(chan);
        }
    }
}

/**---------------------------------------------------------------------------
 * Deinit all the fields of a channel that are not used in 2nd bye. For 
 * multiple-bye-support, the channel's id is removed from the id-manager
 * table, the dp graph is destroyed, the repair session is destroyed.
 * 
 * @param[in] chan Pointer to the channel.
 * @param[in] multibye If a shutdown operation should be done on
 * the primary session in order to attempt sending multiple-byes.
 *---------------------------------------------------------------------------*/ 
static void
vqec_chan_deinit_multibye (vqec_chan_t *chan, boolean multibye)
{
    if (!chan) {
        return;
    }

    /* stop all polled services. */
    chan->er_poll_active = FALSE;

    if (chan->stb_dcr_stats_ev) {
        vqec_event_destroy(&chan->stb_dcr_stats_ev);
    }

#if HAVE_FCC
    /* destroy all events. */
    if (chan->app_timeout) {
        vqec_event_destroy(&chan->app_timeout);
    }
    if (chan->er_fast_timer) {
        vqec_event_destroy(&chan->er_fast_timer);
    }
#endif
    
    if (chan->mrb_transition_ev) {
        vqec_event_destroy(&chan->mrb_transition_ev);
    }

    /* destroy repair session. */
    if (chan->repair_session) {
        rtp_delete_session_repair_recv(&chan->repair_session,
                                       TRUE);
        chan->bye_count--;
    }

    /* 
     * put primary session into a permanent "shutdown" state: this will
     * also cache the last statistics snapshot from the dataplane for use
     * in the upcoming bye(s). Must be done before destroying the
     * graph in the dataplane.
     */
    if (chan->prim_session && multibye) {
        MCALL(chan->prim_session, shutdown_allow_byes);
    }        
    
    /* destroy NAT bindings. */
    if (chan->repair_rtcp_nat_id != VQEC_NAT_BINDID_INVALID) {
        vqec_nat_close_binding(chan->repair_rtcp_nat_id);
        chan->repair_rtcp_nat_id = VQEC_NAT_BINDID_INVALID;
    }
    if (chan->repair_rtp_nat_id != VQEC_NAT_BINDID_INVALID) {
        vqec_nat_close_binding(chan->repair_rtp_nat_id);
        chan->repair_rtp_nat_id = VQEC_NAT_BINDID_INVALID;
    } 
    /* for vod primary session nat bindings*/
    if (chan->prim_rtcp_nat_id != VQEC_NAT_BINDID_INVALID) {
        vqec_nat_close_binding(chan->prim_rtcp_nat_id);
        chan->prim_rtcp_nat_id = VQEC_NAT_BINDID_INVALID;
    }
    if (chan->prim_rtp_nat_id != VQEC_NAT_BINDID_INVALID) {
        vqec_nat_close_binding(chan->prim_rtp_nat_id);
        chan->prim_rtp_nat_id = VQEC_NAT_BINDID_INVALID;
    } 

    /**
     * Update channel DB to cache the FEC information, if needed
     */
    if (chan->fec_enabled) {
        vqec_chan_cache_fec_info (chan);
    }
    /* destroy dataplane graph. */
    if (chan->graph_id != VQEC_DP_GRAPHID_INVALID) {
        if (vqec_dp_graph_destroy(chan->graph_id) != 
            VQEC_DP_ERR_OK) {        
            syslog_print(VQEC_CHAN_ERROR,
                         vqec_chan_print_name(chan),
                         "Dataplane graph destroy failed");
        }
    }
    
#ifdef HAVE_FCC
    /* 
     * Forced abort: abort notification from the state-machine will
     * take no action, such as IPC to dataplane.
     */
    vqec_chan_set_rcc_aborted(chan);
    vqec_sm_deinit(chan);
#endif /* HAVE_FCC */
}


/**---------------------------------------------------------------------------
 * Final de-initialization and destruction of the channel:
 *    o Destroy the primary session (causing a BYE to be sent)
 *    o Destroy the bye event
 *    o Remove the channel from the database
 *    o Free the channel object
 * 
 * @param[in] chan Pointer to the chan.
 *---------------------------------------------------------------------------*/ 
static void
vqec_chan_deinit_final (vqec_chan_t *chan)
{
    if (chan->prim_session) {
        rtp_delete_session_era_recv(&chan->prim_session, TRUE); 
    }

    if (chan->bye_ev) {
        vqec_event_destroy(&chan->bye_ev);
    }

    VQE_LIST_REMOVE(chan, list_obj);
    free(chan);
}


/**---------------------------------------------------------------------------
 * 2nd Bye event handler.
 * 
 * @param[in] arg Pointer to the chan.
 *---------------------------------------------------------------------------*/ 
static void 
vqec_chan_bye_event_handler (const vqec_event_t *const unused_evptr,
                             int unused_fd,
                             short unused_event,
                             void *arg)
{
    vqec_chan_t *chan;

    chan = (vqec_chan_t *)arg;
    VQEC_ASSERT(chan);

    
    if (--chan->bye_count) {

        /* more bye(s) to send after this one. */
        rtp_session_era_recv_send_bye(chan->prim_session); 
    } else {

        /*
         * Delete the channel's remaining state, including its primary
         * session, and remove it from the channel database.
         * This will trigger one BYE to be sent.
         */
        vqec_chan_deinit_final(chan);

        /* 
         * If the channel list is now empty, stop the poll timer.
         */
        if (VQE_LIST_EMPTY(&g_channel_module->channel_list)) {
            /* sa_ignore {stop event} IGNORE_RETURN(1) */
            vqec_event_stop(g_channel_module->poll_ev);
        }
    }
}


/**---------------------------------------------------------------------------
 * Destroy an existing channel.
 *
 * If "deinit_final" is FALSE, then the channel is kept within the channel
 * database, but put in a shutdown state, rather than destroyed.  This means
 * that any channel resources and state not needed for transmission of BYEs
 * are de-initialized, and its external ID is destroyed.  A recurring event
 * is created for sending BYE messages while the channel is in this shutdown
 * state.  Once all BYE messages have been sent, the event callback will
 * destroy the event, remove the shutdown channel from the database, and 
 * free it.
 *
 * If an attempt is made to bind a channel that is in the shutdown state, 
 * then it will be detected during channel lookup via vqec_chan_get_chanid().
 * In this case, the channel is immediately destroyed (without sending any 
 * remaining BYEs) and removed from the database.  A new channel structure 
 * is then allocated, initialized, and inserted into the database.
 *
 * @param[in] chan channel to be destroyed.
 * @param[in] deinit_final Force immediate finial deinit - do not do
 * multi-bye(s) on session(s).
 *---------------------------------------------------------------------------*/ 
static void
vqec_chan_destroy (vqec_chan_t *chan, boolean deinit_final)
{
    struct timeval tv;

    if (!chan) {
        return;
    }
    if (chan->shutdown) {
        /* Already in shutdown state - destroy. */
        goto destroy;
    }

    /* If this channel did RCC, decrement the number of active RCCs */
    if (chan->rcc_enabled && !chan->rcc_in_abort) {
        g_channel_module->num_current_rccs--;
    }

    /* Deinit all state that is not used for multi-bye support. */
    vqec_chan_deinit_multibye(chan, !deinit_final);

    /* Delete the channel ID: no external references possible */
    id_delete(chan->chanid, g_channel_module->id_table_key);

    if (!deinit_final &&
        (chan->cfg.er_enable || 
         chan->rcc_enabled) && 
        (chan->bye_count >= 1) &&
        !TIME_CMP_R(eq, chan->bye_delay, REL_TIME_0)) {

        tv = rel_time_to_timeval(chan->bye_delay);

        /* transmit time-delayed bye(s) on primary. */
        if (vqec_event_create(&chan->bye_ev,
                              VQEC_EVTYPE_TIMER,
                              VQEC_EV_RECURRING,
                              vqec_chan_bye_event_handler,
                              VQEC_EVDESC_TIMER,
                              chan) &&
            vqec_event_start(chan->bye_ev, &tv)) {
            
            /* 
             * put the channel into a shutdown state, so that in case a new 
             * bind turns up for this channel, we'll immediately destroy it.
             */
            chan->shutdown = TRUE;
            return;
        }
    }

destroy:
    vqec_chan_deinit_final(chan);
}

/**---------------------------------------------------------------------------
 * Invoke the gap reporter if the channel's repair trigger time has arrived.
 * The method "rounds down" the poll time, i.e., if repair trigger time is
 * 30 msecs, and the poll internal is 20 msecs, the channel will be polled for
 * gaps every 20 msecs. The computation of next_er_sched_time reflects
 * this by subtracting 1 poll interval.
 * 
 * @param[in] chan Pointer to the channel.
 * @param[in] cur_time Current system time.
 *---------------------------------------------------------------------------*/ 
#define VQEC_CHANNEL_POLL_INTERVAL MSECS(20)

static void
vqec_chan_poll_gap_report (vqec_chan_t *chan, abs_time_t cur_time)
{
    if (TIME_CMP_A(ge, cur_time, chan->next_er_sched_time)) {
        chan->next_er_sched_time = TIME_ADD_A_R(cur_time,
                                                chan->repair_trigger_time);
        vqec_gap_reporter_report(NULL, 
                                 0, 
                                 0, 
                                 (void *)chan->chanid);
    }
}

/**---------------------------------------------------------------------------
 * Channel poll handler. This handler is invoked periodically to check if any
 * operations need to be performed on active channels. If there are no 
 * active channels, the poll event is stopped. All input arguments are unused.
 *---------------------------------------------------------------------------*/ 
void vqec_chan_poll_ev_handler (const vqec_event_t * const unused_evptr,
                                int32_t unused_fd, 
                                int16_t unused_event, 
                                void *unused_arg)
{
    vqec_chan_t *chan;
    abs_time_t cur_time;

    if (g_channel_module && !VQE_LIST_EMPTY(&g_channel_module->channel_list)) {
        cur_time = get_sys_time();
        
        VQE_LIST_FOREACH(chan,
                      &g_channel_module->channel_list, 
                      list_obj) {

            if (chan->shutdown) {
                continue;       /* channel is marked for deletion */
            }
            if (chan->er_enabled && chan->er_poll_active) {
                vqec_chan_poll_gap_report(chan, cur_time);
            }
        }
    }
}


/**---------------------------------------------------------------------------
 * Initialize the channel module, and allocate any static resources.
 *
 * @param[out] vqec_err_t Returns VQEC_CHAN_ERR_OK on success. If 
 * resources cannot be allocated returns an error code.
 *---------------------------------------------------------------------------*/ 
vqec_chan_err_t
vqec_chan_module_init (void)
{
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
#define VQEC_CHAN_MIN_CHANNELS 2  /* Id manager limitation */
#define VQEC_CHAN_FLOOR 4         /* Supports unbound channels (see below) */
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();

    /* Nothing to do if channel module is already initialized */
    if (g_channel_module) {
        goto done;
    }

    /* Otherwise, allocate global channel module storage */
    g_channel_module = malloc(sizeof(vqec_channel_module_t));
    if (!g_channel_module) {
        status = VQEC_CHAN_ERR_NOMEM;
        goto done;
    }
    memset(g_channel_module, 0, sizeof(vqec_channel_module_t));
    
    /*
     * Define the maximum number of active channels.  This value is used
     * to limit the memory consumed by channels (including associated 
     * memory, such as their RTP sessions).
     *
     * This value is set based on the maximum number of tuners, subject
     * to a floor, to define a reasonable maximum.  Although only
     * one channel can be assigned to a tuner at a time, the 
     * following additional non-active (non-bound) channels may exist:
     *     1. one channel may be allocated but not yet assigned to 
     *        a tuner (in the context of changing the channel of a 
     *        tuner that is already bound to an active channel), and
     *     2. other channels could exist outside of the active
     *        channel database following an unbind but before their
     *        deletion (upon sending the last BYE).
     */
    g_channel_module->max_channels = 
        max(syscfg->max_tuners * 2, VQEC_CHAN_FLOOR);
    if (g_channel_module->max_channels < VQEC_CHAN_MIN_CHANNELS) {
        /* keep id mgr happy */
        g_channel_module->max_channels = VQEC_CHAN_MIN_CHANNELS;  
    }

    g_channel_module->fec_default_block_size = max_fec_block_size_observed;

    g_channel_module->rtcp_iobuf = malloc(RTCP_PAK_SIZE);
    if (!g_channel_module->rtcp_iobuf) {
        status = VQEC_CHAN_ERR_NOMEM;
        goto done;
    }
    g_channel_module->rtcp_iobuf_len = RTCP_PAK_SIZE;

    /* Prepare an ID Manager table for allocating channel IDs */
    g_channel_module->id_table_key =
        id_create_new_table(1, g_channel_module->max_channels);
    if (g_channel_module->id_table_key == ID_MGR_TABLE_KEY_ILLEGAL) {
        status = VQEC_CHAN_ERR_INTERNAL;
        goto done;
    }
    
    /* Initialize a linked list for storing active channels */
    VQE_LIST_INIT(&g_channel_module->channel_list);

    /* Initialize child RTP module(s). */
    if (!rtp_repair_recv_init_module(g_channel_module->max_channels) ||
        !rtp_era_recv_init_module(g_channel_module->max_channels)) {
        status = VQEC_CHAN_ERR_INTERNAL;
        goto done;
    }
    
    /*
     * All activities inclusive of error-repair polls and STB polls are
     * triggered from a single timer, which runs at a fixed interval.
     */
    if (!vqec_event_create(&g_channel_module->poll_ev,
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_RECURRING,
                           vqec_chan_poll_ev_handler,
                           VQEC_EVDESC_TIMER,
                           NULL)) {
        syslog_print(VQEC_ERROR, 
                     "failed to create poll timer");
        status = VQEC_CHAN_ERR_INTERNAL;
        goto done;        
    }

done:
    if (status != VQEC_CHAN_ERR_OK) {
        if (g_channel_module) {
            if (g_channel_module->rtcp_iobuf) {
                free(g_channel_module->rtcp_iobuf);
            }
            if (g_channel_module->id_table_key != ID_MGR_TABLE_KEY_ILLEGAL) {
                id_destroy_table(g_channel_module->id_table_key);
            }
            if (g_channel_module->poll_ev) {
                vqec_event_destroy(&g_channel_module->poll_ev);
            }
            free(g_channel_module);
            g_channel_module = NULL;
        }

        /* 
         * NOTE: There are no de-init handlers for RTP in 3.0 release;
         * If RTP memory pools are to be deallocated, these handlers should
         * be added and called here / in deinit method.
         */

        snprintf(s_debug_str, DEBUG_STR_LEN, ": %s",
                 vqec_chan_err2str(status));        
    } else {
        s_debug_str[0] = '\0';
    }
    VQEC_CHAN_DEBUG("\n%s()%s", __FUNCTION__, s_debug_str);
    return (status);
}


/**---------------------------------------------------------------------------
 * Deallocate all resources allocated for the channel manager. All
 * channels that are currently cached are destroyed.  Also, the
 * flowgraphs for these channels are also destroyed in the dataplane.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_module_deinit (void)
{
    vqec_chan_t *chan, *tchan;

    if (!g_channel_module) {
        /* Channel module not initialized, so nothing to do */
        goto done;
    }

    /* Destroy all the active channels */
    VQE_LIST_FOREACH_SAFE(chan, &g_channel_module->channel_list, 
			  list_obj, tchan) {
        vqec_chan_destroy(chan, TRUE);
    }

    /* Destroy the channel module's ID Manager table */
    if (g_channel_module->id_table_key != ID_MGR_TABLE_KEY_ILLEGAL) {
        id_destroy_table(g_channel_module->id_table_key);
    }
    /* Free RTCP packet buffer. */
    if (g_channel_module->rtcp_iobuf) {
        free(g_channel_module->rtcp_iobuf);
    }

    /* Free the channel poll event. */
    if (g_channel_module->poll_ev) {
        vqec_event_destroy(&g_channel_module->poll_ev);
    }
    /* Free the global channel module state */
    free(g_channel_module);
    g_channel_module = NULL;

done:
    VQEC_CHAN_DEBUG("\n%s()", __FUNCTION__);
    return;
}

/*
 * Within the definitions below, the default MPEG packet consists of:
 *     = 188 (bytes/MPEG TS pkt) * 7 (MPEG TS pkts/IP packet) +
 *        12 (bytes/RTP hdr) + 8 (bytes/UDP hdr) + 20 (bytes/IP hdr)
 */
#define VQEC_CHAN_MPEG_TS_PAKSIZE_BITS (MP_MPEG_TSPKT_LEN*7*8)
#define VQEC_CHAN_MPEG_PAKSIZE_BITS \
    (VQEC_CHAN_MPEG_TS_PAKSIZE_BITS + (12 + 8 + 20)*8)
#define VQEC_CHAN_AVG_PKT_TIME(bitrate)                                 \
    bitrate ? TIME_MK_R(usec, (VQEC_CHAN_MPEG_PAKSIZE_BITS * 1000) /    \
                        (bitrate/1000))                                 \
        : REL_TIME_0
/**
 * Retrieves a channel manager ID for the supplied active channel,
 * as specified by its destination (ip addr, port) pair.  
 * If no active channel exists with the given (ip addr, port) pair,
 * then VQEC_ERR_CHANNOTACTIVE will be returned.
 *
 * @param[out] chanid   Retrieved channel's ID upon success, or
 *                      VQEC_CHANID_INVALID upon failure.
 * @param[in] ip        requested channel's destination IP address
 * @param[in] port      requested channel's destination port
 * @param[out] vqec_chan_err_t 
 *                      Returns VQEC_CHAN_ERR_OK upon success,
 *                      or a failure code indicating why a channel ID 
 *                      could not be retrieved. 
 */
vqec_chan_err_t
vqec_chan_find_chanid (vqec_chanid_t *chanid,
                       in_addr_t ip,
                       in_port_t port)
{
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    vqec_chan_t *chan;

    if (!chanid) {
        status = VQEC_CHAN_ERR_INVALIDARGS;
        goto done;
    } else {
        *chanid = VQEC_CHANID_INVALID;
    }
    chan = vqec_chan_find(ip, port);
    if (!chan) {
        status = VQEC_CHAN_ERR_NOTFOUND;
        goto done;
    }
    *chanid = chan->chanid;
done:
    return (status);
}

/**
 * Converts a channel_cfg_t structure (based on SDP)
 * into a vqec_chan_cfg_t structure (internal VQE-C
 * representation of a channel configuration). If
 * sdp is NULL, cfg is set to defaults.
 *
 * @param[in]  sdp        sdp channel configuration
 * @param[in]  chan_type  channel type (linear or vod)
 * @param[out] cfg        target channel cfg
 */
void
vqec_chan_convert_sdp_to_cfg(channel_cfg_t *sdp,
                             vqec_chan_type_t chan_type,
                             vqec_chan_cfg_t *cfg)
{
    if (!cfg) {
        return;
    }

    /* Set configuration defaults */
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->name, VQEC_MAX_CHANNEL_NAME_LENGTH, "temp channel");
    cfg->primary_rtcp_sndr_bw = VQEC_RTCP_BW_UNSPECIFIED;
    cfg->primary_rtcp_rcvr_bw = VQEC_RTCP_BW_UNSPECIFIED;
    cfg->primary_rtcp_per_rcvr_bw = VQEC_RTCP_BW_UNSPECIFIED;
    cfg->primary_rtcp_xr_loss_rle = VQEC_RTCP_XR_RLE_UNSPECIFIED;
    cfg->primary_rtcp_xr_per_loss_rle = VQEC_RTCP_XR_RLE_UNSPECIFIED;
    cfg->rtx_rtcp_sndr_bw = VQEC_RTCP_BW_UNSPECIFIED;
    cfg->rtx_rtcp_rcvr_bw = VQEC_RTCP_BW_UNSPECIFIED;
    cfg->rtx_rtcp_xr_loss_rle = VQEC_RTCP_XR_RLE_UNSPECIFIED;

    if (!sdp) {
        return;
    }

    memcpy(cfg->name, sdp->name, sizeof(cfg->name));

    /* Set Primary Stream information including transport parameters
     * for unicast/multicast source and destination, payload type
     * and RTCP bandwidth.
     */
    switch (chan_type) {
    case VQEC_CHAN_TYPE_LINEAR:
        switch (sdp->mode) {
        case SOURCE_MODE:
            cfg->primary_dest_addr = sdp->re_sourced_addr;
            cfg->primary_dest_port = sdp->re_sourced_rtp_port;
            cfg->primary_dest_rtcp_port = sdp->re_sourced_rtcp_port;
            cfg->primary_source_addr = sdp->src_addr_for_re_sourced;
            cfg->primary_source_rtcp_port = sdp->re_sourced_rtcp_port;
            cfg->primary_payload_type = sdp->re_sourced_payload_type;
            cfg->primary_rtcp_sndr_bw = sdp->re_sourced_rtcp_sndr_bw;
            cfg->primary_rtcp_rcvr_bw = sdp->re_sourced_rtcp_rcvr_bw;
            /* inherit per rcvr bw from original stream */
            cfg->primary_rtcp_per_rcvr_bw = sdp->original_rtcp_per_rcvr_bw;
            break;
        case LOOKASIDE_MODE:
        case RECV_ONLY_MODE:
            cfg->primary_dest_addr = sdp->original_source_addr;
            cfg->primary_dest_port = sdp->original_source_port;
            cfg->primary_dest_rtcp_port = sdp->original_source_rtcp_port;
            cfg->primary_source_addr = sdp->src_addr_for_original_source;
            cfg->primary_source_rtcp_port = sdp->original_source_rtcp_port;
            cfg->primary_payload_type = sdp->original_source_payload_type;
            cfg->primary_rtcp_sndr_bw = sdp->original_rtcp_sndr_bw;
            cfg->primary_rtcp_rcvr_bw = sdp->original_rtcp_rcvr_bw;
            cfg->primary_rtcp_per_rcvr_bw = sdp->original_rtcp_per_rcvr_bw;
            break;
        case DISTRIBUTION_MODE:
        case UNSPECIFIED_MODE:
            break;
        }
        break;
    case VQEC_CHAN_TYPE_VOD:
        cfg->primary_source_addr = sdp->original_source_addr;
        cfg->primary_source_port = sdp->original_source_port;
        cfg->primary_source_rtcp_port = sdp->original_source_rtcp_port;
        cfg->primary_payload_type = sdp->original_source_payload_type;
        cfg->primary_rtcp_sndr_bw = sdp->original_rtcp_sndr_bw;
        cfg->primary_rtcp_rcvr_bw = sdp->original_rtcp_rcvr_bw;
        cfg->primary_rtcp_per_rcvr_bw = VQEC_RTCP_BW_UNSPECIFIED;
        break;
    }
    cfg->primary_bit_rate = sdp->bit_rate;

    /* Primary Stream Extended Reports */
    /* inherit loss rle and per loss rle from original when in source mode */
    cfg->primary_rtcp_xr_loss_rle = sdp->original_rtcp_xr_loss_rle;
    cfg->primary_rtcp_xr_per_loss_rle = sdp->original_rtcp_xr_per_loss_rle;
    if (sdp->original_rtcp_xr_stat_flags & RTCP_XR_LOSS_BIT_MASK) {
        cfg->primary_rtcp_xr_stat_flags |= VQEC_RTCP_XR_STAT_LOSS;
    }
    if (sdp->original_rtcp_xr_stat_flags & RTCP_XR_DUP_BIT_MASK) {
        cfg->primary_rtcp_xr_stat_flags |= VQEC_RTCP_XR_STAT_DUP;
    }
    if (sdp->original_rtcp_xr_stat_flags & RTCP_XR_JITT_BIT_MASK) {
        cfg->primary_rtcp_xr_stat_flags |= VQEC_RTCP_XR_STAT_JITT;
    }
    if (sdp->original_rtcp_xr_multicast_acq) {
        cfg->primary_rtcp_xr_multicast_acq = TRUE;
    }
    if (sdp->original_rtcp_rsize) {
        cfg->primary_rtcp_rsize = TRUE;
    }

    if (sdp->original_rtcp_xr_diagnostic_counters) {
        cfg->primary_rtcp_xr_diagnostic_counters = TRUE;
    }

    /* Feedback Target Address */
    cfg->fbt_addr = sdp->fbt_address;

    /* Retransmission Enable Flags */
    cfg->er_enable = sdp->er_enable;
    cfg->rcc_enable = sdp->fcc_enable;

    /* Retransmission Transport Parameters */
    cfg->rtx_source_addr = sdp->rtx_addr;
    cfg->rtx_source_port = sdp->rtx_rtp_port;
    cfg->rtx_source_rtcp_port = sdp->rtx_rtcp_port;

    /* Retransmission Payload and RTCP Bandwidth */
    cfg->rtx_payload_type = sdp->rtx_payload_type;
    cfg->rtx_rtcp_sndr_bw = sdp->repair_rtcp_sndr_bw;
    cfg->rtx_rtcp_rcvr_bw = sdp->repair_rtcp_rcvr_bw;

    /* Retransmission Extended Reports */
    cfg->rtx_rtcp_xr_loss_rle = sdp->repair_rtcp_xr_loss_rle;
    if (sdp->repair_rtcp_xr_stat_flags & RTCP_XR_LOSS_BIT_MASK) {
        cfg->rtx_rtcp_xr_stat_flags |= VQEC_RTCP_XR_STAT_LOSS;
    }
    if (sdp->repair_rtcp_xr_stat_flags & RTCP_XR_DUP_BIT_MASK) {
        cfg->rtx_rtcp_xr_stat_flags |= VQEC_RTCP_XR_STAT_DUP;
    }
    if (sdp->repair_rtcp_xr_stat_flags & RTCP_XR_JITT_BIT_MASK) {
        cfg->rtx_rtcp_xr_stat_flags |= VQEC_RTCP_XR_STAT_JITT;
    }

    /* FEC Enable Flags */
    cfg->fec_enable = sdp->fec_enable;
    switch (sdp->fec_mode) {
    case FEC_1D_MODE:
        cfg->fec_mode = VQEC_FEC_1D_MODE;
        break;
    case FEC_2D_MODE:
        cfg->fec_mode = VQEC_FEC_2D_MODE;
        break;
    }

    /* FEC 1 Transport */
    cfg->fec1_mcast_addr = sdp->fec_stream1.multicast_addr;
    cfg->fec1_mcast_port = sdp->fec_stream1.rtp_port;
    cfg->fec1_mcast_rtcp_port = sdp->fec_stream1.rtcp_port;
    cfg->fec1_source_addr = sdp->fec_stream1.src_addr;

    /* FEC 1 payload type and RTCP bandwidth */
    cfg->fec1_payload_type = sdp->fec_stream1.payload_type;
    cfg->fec1_rtcp_sndr_bw = sdp->fec_stream1.rtcp_sndr_bw;
    cfg->fec1_rtcp_rcvr_bw = sdp->fec_stream1.rtcp_rcvr_bw;


    /* FEC 2 Transport */
    cfg->fec2_mcast_addr = sdp->fec_stream2.multicast_addr;
    cfg->fec2_mcast_port = sdp->fec_stream2.rtp_port;
    cfg->fec2_mcast_rtcp_port = sdp->fec_stream2.rtcp_port;
    cfg->fec2_source_addr = sdp->fec_stream2.src_addr;

    /* FEC 2 payload type and RTCP bandwidth */
    cfg->fec2_payload_type = sdp->fec_stream2.payload_type;
    cfg->fec2_rtcp_sndr_bw = sdp->fec_stream2.rtcp_sndr_bw;
    cfg->fec2_rtcp_rcvr_bw = sdp->fec_stream2.rtcp_rcvr_bw;

    /* FEC common parameters */
    cfg->fec_l_value = sdp->fec_info.fec_l_value;
    cfg->fec_d_value = sdp->fec_info.fec_d_value;
    cfg->fec_order = sdp->fec_info.fec_order;
    cfg->fec_ts_pkt_time = sdp->fec_info.rtp_ts_pkt_time;

}

/* Validation Helper Functions */
/**
 * Validate a payload type number. Payload types
 * must either be within the dynamic range or equal
 * to the static payload type number for MP2T. Payload
 * type values are defined by RFC3551.
 *
 * @param[in]  pt           payload type number
 * @param[in]  allow_static allow static payload type
 * @param[out] boolean      TRUE if pt is valid
 */
static inline boolean
vqec_chan_validate_pt (uint8_t pt, boolean allow_static)
{
#define MIN_PT 96
#define MAX_PT 127
#define STATIC_PT 33
    return ((pt >= MIN_PT && pt <= MAX_PT) || 
            (allow_static && pt == STATIC_PT));
}

/**
 * Validates a set of channel configuration
 * parameters.
 *
 * @param[in]  cfg  channel cfg to validate
 * @param[out] boolean  TRUE if validation succeeds, FALSE otherwise
 */
static boolean
vqec_chan_validate_cfg (vqec_chan_cfg_t *cfg)
{
    boolean rv = TRUE;

    /* Primary dest addr and port must be specified */
    if (!cfg->primary_dest_addr.s_addr || !cfg->primary_dest_port) {
        snprintf(s_debug_str, DEBUG_STR_LEN, 
                 "invalid primary destination in chan cfg");
        rv = FALSE;
        goto done;
    }
    /* If this is a pass-thru channel, skip the rest of the validation */
    if (cfg->passthru) {
        goto done;
    }
    /* Primary bit rate must be specified */
    if (cfg->primary_bit_rate <= 0) {
        snprintf(s_debug_str, DEBUG_STR_LEN,
                 "invalid primary bit rate in chan cfg");
        rv = FALSE;
        goto done;
    }
    /* FBT address and rtx source address must match */
    if ((cfg->er_enable || cfg->rcc_enable) && 
        (cfg->fbt_addr.s_addr != cfg->rtx_source_addr.s_addr)) {
        snprintf(s_debug_str, DEBUG_STR_LEN,
                 "FBT and rtx src addr mismatch in chan cfg");
        rv = FALSE;
        goto done;
    }
    /* ER or RCC enable necessitates FBT addr */
    if ((cfg->er_enable || cfg->rcc_enable) && !cfg->fbt_addr.s_addr) {
        snprintf(s_debug_str, DEBUG_STR_LEN,
                 "missing FBT address in chan cfg");
        rv = FALSE;
        goto done;
    }
    /* Payload types must be valid */
    if (!vqec_chan_validate_pt(cfg->primary_payload_type, TRUE)) {
        snprintf(s_debug_str, DEBUG_STR_LEN,
                 "invalid primary payload type in chan cfg");
        rv = FALSE;
        goto done;
    }
    if ((cfg->er_enable || cfg->rcc_enable) && 
        !vqec_chan_validate_pt(cfg->rtx_payload_type, FALSE)) {
        snprintf(s_debug_str, DEBUG_STR_LEN,
                 "invalid rtx payload type in chan cfg");
        rv = FALSE;
        goto done;
    }
    /* RTP and RTCP port numbers must be unique for each stream */
    if (cfg->primary_dest_rtcp_port && 
        (cfg->primary_dest_rtcp_port == cfg->primary_dest_port)) {
        snprintf(s_debug_str, DEBUG_STR_LEN,
                 "primary dest rtcp port matches primary dest (rtp) port");
        rv = FALSE;
        goto done;
    }
    if (cfg->rtx_dest_rtcp_port &&
        (cfg->rtx_dest_rtcp_port == cfg->rtx_dest_port)) {
        snprintf(s_debug_str, DEBUG_STR_LEN,
                 "rtx dest rtcp port matches rtx dest (rtp) port");
        rv = FALSE;
        goto done;
    }
    /* 
     * All port numbers must be unique if:
     *  a. primary dest addr and rtx dest addr match -or-
     *  b. primary stream is unicast and rtx dest addr is undefined
     */
    if ((cfg->primary_dest_addr.s_addr == cfg->rtx_dest_addr.s_addr) ||
        (!IN_MULTICAST(ntohl(cfg->primary_dest_addr.s_addr)) && 
         !cfg->rtx_dest_addr.s_addr)) {
        if (cfg->rtx_dest_port &&
            ((cfg->rtx_dest_port == cfg->primary_dest_port) ||
             (cfg->rtx_dest_port == cfg->primary_dest_rtcp_port))) {
            snprintf(s_debug_str, DEBUG_STR_LEN,
                     "rtx dest port matches a primary port number");
            rv = FALSE;
            goto done;
        }
        if (cfg->rtx_dest_rtcp_port &&
            ((cfg->rtx_dest_rtcp_port == cfg->primary_dest_port) ||
             (cfg->rtx_dest_rtcp_port == cfg->primary_dest_rtcp_port))) {
            snprintf(s_debug_str, DEBUG_STR_LEN,
                     "rtx dest rtcp port matches a primary port number");
            rv = FALSE;
            goto done;
        }
    }
    /* Source ports can only be specified if source IPs are too */
    if (cfg->primary_source_port && !cfg->primary_source_addr.s_addr) {
        snprintf(s_debug_str, DEBUG_STR_LEN,
                 "invalid primary source address");
        rv = FALSE;
        goto done;
    }
    if (cfg->rtx_source_port && !cfg->rtx_source_addr.s_addr) {
        snprintf(s_debug_str, DEBUG_STR_LEN,
                 "invalid rtx source address");
        rv = FALSE;
        goto done;
    }
        
  done:
    if (!rv) {
        syslog_print(VQEC_ERROR, s_debug_str);
    }
    return rv;
}

/**
 * Retrieves a channel manager ID for the supplied channel.
 *
 * If the supplied channel exists in the channel manager database
 * (implying that a tuner is already associated with it), then its ID
 * will be returned.  If the supplied channel does not exist, then it
 * will be created and inserted into the channel database, and its ID
 * returned.
 *
 * Channels are uniquely identified by their destination IP and port
 * pair (cfg->primary_dest_addr.s_addr, cfg->primary_dest_port).
 * The other fields of the cfg structure are used only when the channel
 * does not already exist in the channel database.  Note that this
 * channel configuration is currently NOT validated within this
 * function--the supplied configuration is assumed to be valid.
 *
 * @param[out] chanid Retrieved channel's ID upon success, or
 * VQEC_CHANID_INVALID upon failure.
 * @param[in] cfg Channel configuration. A copy of this configuration
 * will be cached, and used to construct various dynamic components
 * of the channel.  The configuration is assumed valid.
 * @param[out] vqec_chan_err_t Returns VQEC_CHAN_ERR_OK upon success,
 * or a failure code indicating why a channel ID could not be retrieved. 
 */
vqec_chan_err_t
vqec_chan_get_chanid (vqec_chanid_t *chanid, vqec_chan_cfg_t *cfg)
{
    vqec_chan_t *chan = NULL;
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    boolean chan_allocated = FALSE;
    struct timeval tv;

    /* Validate parameters and channel module state */
    if (!chanid) {
        status = VQEC_CHAN_ERR_INVALIDARGS;
        goto done;
    } else {
        *chanid = VQEC_CHANID_INVALID;
    }
    if (!cfg) {
        status = VQEC_CHAN_ERR_INVALIDARGS;
        goto done;
    }
    if (!g_channel_module) {
        status = VQEC_CHAN_ERR_NOTINITIALIZED;
        goto done;
    }

    /* Lookup the channel, return success if found */
    chan = vqec_chan_find(cfg->primary_dest_addr.s_addr,
                          cfg->primary_dest_port);
    if (chan) {
        if (chan->shutdown) {
            VQEC_CHAN_DEBUG("channel %s in shutdown state for "
                            "multiple-bye-tx, channel immediate destroy\n",
                            vqec_chan_print_name(chan));            
            vqec_chan_destroy(chan, TRUE);
            chan = NULL;
        } else {
            *chanid = chan->chanid;
            goto done;
        }
    }

    /* New channel to be allocated, check cfg parameters first */
    if (!vqec_chan_validate_cfg(cfg)) {
        status = VQEC_CHAN_ERR_INVALIDARGS;
        goto done;
    }

    /* Not found, so we must allocate a new channel and its ID */
    chan = malloc(sizeof(vqec_chan_t));
    if (!chan) {
        status = VQEC_CHAN_ERR_NOMEM;
        goto done;
    }
    chan_allocated=TRUE;
    memset(chan, 0, sizeof(vqec_chan_t));
    VQE_TAILQ_INIT(&chan->tuner_list);
    *chanid = id_get(chan, g_channel_module->id_table_key);
    if (*chanid == VQEC_CHANID_INVALID) {
        status = VQEC_CHAN_ERR_NOMORECHANNELS;
        goto done;
    }
    chan->chanid = *chanid;
    memcpy(&chan->cfg, cfg, sizeof(vqec_chan_cfg_t));

    /* if FEC is enabled, check and fix the L and D values if necessary */
    if (chan->cfg.fec_enable && !chan->cfg.fec_l_value && !chan->cfg.fec_d_value) {
        /* FEC is enabled but no cached L and D values; assume worst-case */
        switch (chan->cfg.fec_mode) {
            case FEC_1D_MODE:
                chan->cfg.fec_l_value = 1;  /* minimum L-value in 1-D mode */
                break;
            case FEC_2D_MODE:
                chan->cfg.fec_l_value = 4;  /* minimum L-value in 2-D mode */
                break;
        }
        chan->cfg.fec_d_value = 4;  /* minimum D-value in 1-D and 2-D modes */
    }

    /* calculate the FEC bandwidth associated with the channel */
    vqec_chan_update_fec_bw(chan);

    /* Insert the channel into the database */
    if (VQE_LIST_EMPTY(&g_channel_module->channel_list)) {
        tv = rel_time_to_timeval(VQEC_CHANNEL_POLL_INTERVAL);
        if (!vqec_event_start(g_channel_module->poll_ev, &tv)) {
            status = VQEC_CHAN_ERR_NOMEM;
            goto done;
        }
    }
    VQE_LIST_INSERT_HEAD(&g_channel_module->channel_list, chan, list_obj);
    
done:
    if (status != VQEC_CHAN_ERR_OK) {
        if (g_channel_module && chanid && *chanid != VQEC_CHANID_INVALID) {
            id_delete(*chanid, g_channel_module->id_table_key);
            *chanid = VQEC_CHANID_INVALID;
        }
        if (chan) {
            free(chan);
        }
        snprintf(s_debug_str, DEBUG_STR_LEN, "%s",
                 vqec_chan_err2str(status));
    } else {
        snprintf(s_debug_str, DEBUG_STR_LEN, "chanid 0x%08x %s",
                 *chanid, chan_allocated ? "allocated" : "retrieved");
    }
    if (cfg) {
        snprintf(s_debug_str2, DEBUG_STR_LEN, "ip=%s,port=%u",
                 inet_ntop_static_buf(cfg->primary_dest_addr.s_addr, 0),
                 cfg->primary_dest_port);
    } else {
        snprintf(s_debug_str2, DEBUG_STR_LEN, "cfg=NULL");
    }
    VQEC_CHAN_DEBUG("\n%s(%s): %s", __FUNCTION__, s_debug_str2, s_debug_str);
    return (status);
}

/**
 * Negates a vqec_chan_get_chanid call. 
 *
 * The effects of a vqec_chan_get_chanid call can be undone
 * by calling vqec_chan_put_chanid. If no tuners are bound to the
 * channel referenced by the supplied chanid, the channel is
 * destroyed. 
 *
 * For example, if a vqec_chan_get_chanid is called before binding
 * a tuner but the tuner bind call fails, this function may be used
 * to ensure that no memory is leaked.
 *
 * @param[in]  chanid  channel ID of an existing channel
 * @param[out] vqec_chan_err_t  Returns VQEC_CHAN_ERR_OK on success
 */
vqec_chan_err_t
vqec_chan_put_chanid (vqec_chanid_t chanid)
{
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    vqec_chan_t *chan;

    if (!g_channel_module) {
        status = VQEC_CHAN_ERR_NOTINITIALIZED;
        goto done;
    }

    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        status = VQEC_CHAN_ERR_NOSUCHCHAN;
        goto done;
    }

    if (VQE_TAILQ_EMPTY(&chan->tuner_list)) {
        vqec_chan_destroy(chan, TRUE);
    }
    
  done:
    return status;
}

/**
 * Populates a source filter data structure based on filter values and
 * conditions supplied by the caller.
 *
 * A returned source filter takes one of the following forms, listed
 * in order of least granular to most granular:
 *   none                - no filtering on source address or source port
 *    (addr)             - filtering on source address only
 *      (addr, port)     - filtering on source address and source port
 *
 * Callers invoke this function with "addr_condition" and "port_condition"
 * settings to indicate their preference for the maximum granularity of
 * the filter they want built:
 *   - "addr_condition" of TRUE results in this function trying to 
 *      build a filter of the form (addr) or (addr, port)
 *   - "port_condition" is relevant only when "addr_condition" is TRUE.
 *      An "addr_condition" of TRUE and "port_condition" of TRUE results
 *      in this function trying to build a filter of the form (addr, port).
 *
 * The granularity of the returned filter may also be limited by the
 * values supplied for "addr" and "port":
 *   If "addr" is zero, then the returned filter will be of the form "none".
 *   If "port" is zero, then the returned filter will be of the form "none" 
 *     or (addr).
 *
 * @param[out] filter                - generated source filter.
 *                                     Only the following fields are modified;
 *                                     the remainder are left untouched:
 *                                       u.ipv4.src_ip_filter
 *                                       u.ipv4.src_ip
 *                                       u.ipv4.src_port_filter
 *                                       u.ipv4.src_port
 * @param[in]  addr                  - filter address (network byte order)
 * @param[in]  port                  - filter port (network byte order)
 * @param[in]  addr_condition        - must be TRUE in order for a filter
 *                                     to include "addr"
 * @param[in]  port_condition        - must be TRUE in order for a filter
 *                                     to include "port"
 */
void
vqec_chan_assign_source_filter (vqec_dp_input_filter_t *filter,
			        in_addr_t addr,
				uint16_t port,
				boolean addr_condition,
				boolean port_condition)

{
    /*
     * Initialize caller's filter to "none".
     * We will then increase its granularity as appropriate.
     */
    filter->u.ipv4.src_ip_filter = FALSE;
    filter->u.ipv4.src_ip = htonl(0);
    filter->u.ipv4.src_port_filter = FALSE;
    filter->u.ipv4.src_port = htons(0);

    /*
     * assign source IP address filter if filtering is enabled in
     * system configuration and a source address is supplied.
     */
    if (addr_condition && ntohl(addr)) {
        filter->proto = INPUT_FILTER_PROTO_UDP;
        filter->u.ipv4.src_ip_filter = TRUE;
        filter->u.ipv4.src_ip = addr;
	if (port_condition && ntohs(port)) {
	    filter->u.ipv4.src_port_filter = TRUE;
            filter->u.ipv4.src_port = port;
	}
    }
}

/**
 * Initializes a dataplane channel object (vqec_dp_chan_desc_t) from
 * merged global and per-channel (vqec_chan_cfg_t) configuration in the
 * control plane.
 *
 * @param[in]  chan      Control plane channel object
 * @param[out] chan_desc Dataplane channel description
 * @param[out] boolean   TRUE if success, FALSE upon error
 */
boolean
vqec_chan_load_dp_chan_desc (vqec_chan_t *chan,
                             vqec_dp_chan_desc_t *chan_desc)
{
    vqec_chan_cfg_t *cfg;
    boolean status = TRUE;
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();

    if (!chan || !chan_desc || !g_channel_module) {
        status = FALSE;
        goto done;
    }
    cfg = &chan->cfg;
 
    /*
     * stream-independent info
     */
    if (chan->cfg.protocol == VQEC_PROTOCOL_UDP) {
        chan_desc->fallback = TRUE;
    } else {
        chan_desc->fallback = FALSE;
    }
    chan_desc->reorder_time = TIME_MK_R(msec, syscfg->reorder_delay_abs);
    chan_desc->jitter_buffer = TIME_MK_R(msec, syscfg->jitter_buff_size);
    chan_desc->avg_pkt_time =     
        VQEC_CHAN_AVG_PKT_TIME(chan->cfg.primary_bit_rate);
    chan_desc->repair_trigger_time = chan->repair_trigger_time;
    chan_desc->en_rcc = chan->rcc_enabled; 
    chan_desc->passthru = cfg->passthru;
    chan_desc->max_rate = cfg->primary_bit_rate;
    chan_desc->strip_rtp = syscfg->strip_rtp;
    chan_desc->sch_policy = VQEC_DP_CHAN_SCHED_WITH_NLL;
    chan_desc->max_backfill = chan->rcc_max_fill;
    chan_desc->min_backfill = chan->rcc_min_fill;
    chan_desc->max_fastfill = chan->max_fastfill;
    /* copy FEC info for PCM to allocate FEC buffer */
    chan_desc->fec_info.fec_l_value = cfg->fec_l_value;
    chan_desc->fec_info.fec_d_value = cfg->fec_d_value;
    chan_desc->fec_info.fec_order = cfg->fec_order;
    chan_desc->fec_info.rtp_ts_pkt_time = 
        TIME_MK_R(usec, cfg->fec_ts_pkt_time);
    chan_desc->fec_default_block_size = 
        g_channel_module->fec_default_block_size;
    /*
     * primary stream info
     */
    chan_desc->primary.encap = VQEC_DP_ENCAP_RTP;
    /* filter: */
    chan_desc->primary.filter.proto = INPUT_FILTER_PROTO_UDP;
    chan_desc->primary.filter.u.ipv4.dst_ip = cfg->primary_dest_addr.s_addr;
    chan_desc->primary.filter.u.ipv4.dst_ifc_ip = chan->input_if_addr;
    chan_desc->primary.filter.u.ipv4.dst_port = cfg->primary_dest_port;
    vqec_chan_assign_source_filter(
        &chan_desc->primary.filter,
        cfg->primary_source_addr.s_addr,
        cfg->primary_source_port,
        chan->src_ip_filter_enable,
        !IN_MULTICAST(ntohl(cfg->primary_dest_addr.s_addr)));
#if HAVE_FCC
    /* extra igmp ip: */
    chan_desc->primary.filter.u.ipv4.rcc_extra_igmp_ip =
        syscfg->rcc_extra_igmp_ip;
#endif  /* HAVE_FCC */
    /* so_rcvbuf: */
    chan_desc->primary.so_rcvbuf = syscfg->so_rcvbuf;
    /* scheduling_class: */
    chan_desc->primary.scheduling_class = VQEC_DP_FAST_SCHEDULE;
    /* payload type: */
    chan_desc->primary.rtp_payload_type = cfg->primary_payload_type;
    /* datagram_size: */
    chan_desc->primary.datagram_size = syscfg->max_paksize;

    /*
     * Set the RTCP XR max RLE size.  If the original_rtcp_xr_loss_rle is
     * 0xffff (unspecified), it means pkt-loss-rle is not specified in
     * SDP. So set rtcp_xr_max_rle_size to 0.
     */
    if (cfg->primary_rtcp_xr_loss_rle == RTCP_XR_LOSS_RLE_UNSPECIFIED) {
        chan_desc->primary.rtcp_xr_max_rle_size = 0;
    } else {
        chan_desc->primary.rtcp_xr_max_rle_size =
            cfg->primary_rtcp_xr_loss_rle;
    }
    /*
     * However, set rtcp_xr_max_rle_size to MAX_RLE_SIZE if stats summary 
     * is specified, even if no pkt-loss-rle is enabled.
     */
    if (chan_desc->primary.rtcp_xr_max_rle_size == 0 &&
        cfg->primary_rtcp_xr_stat_flags) {
        chan_desc->primary.rtcp_xr_max_rle_size = MAX_RLE_SIZE;
    }

    /*
     * Set the RTCP post-ER XR max RLE size.  If the
     * original_rtcp_xr_per_loss_rle is 0xffff (unspecified), it means
     * pkt-loss-rle is not specified in SDP. So set rtcp_xr_post_er_rle_size
     * to 0.
     */
    if (cfg->primary_rtcp_xr_per_loss_rle == RTCP_XR_LOSS_RLE_UNSPECIFIED) {
        chan_desc->primary.rtcp_xr_post_er_rle_size = 0;
    } else {
        chan_desc->primary.rtcp_xr_post_er_rle_size =
            cfg->primary_rtcp_xr_per_loss_rle;
    }
    /*
     * However, set rtcp_xr_post_er_rle_size to MAX_RLE_SIZE if stats summary
     * is specified, even if no pkt-loss-rle is enabled.
     */
    if (chan_desc->primary.rtcp_xr_post_er_rle_size == 0 &&
        cfg->primary_rtcp_xr_stat_flags) {
        chan_desc->primary.rtcp_xr_post_er_rle_size = MAX_RLE_SIZE;
    }

    /*
     * Repair stream info (a repair stream is needed either if ER or RCC).
     */
    chan_desc->en_repair = chan->er_enabled;
    if (chan_desc->en_repair || chan_desc->en_rcc) {
        chan_desc->repair.encap = VQEC_DP_ENCAP_RTP;
        /*
         * filter:
         * 
         * Includes protocol, destination, and source for the repair stream.
         *
         * For a non-NAT mode multicast channel, the local repair dest port 
         * is assigned to be the repair stream's source port.  The dest
         * port supplied in the channel configuration is NOT used in this case.
         */
        chan_desc->repair.filter.proto = INPUT_FILTER_PROTO_UDP;
        chan_desc->repair.filter.u.ipv4.dst_ip = chan->input_if_addr;
        chan_desc->repair.filter.u.ipv4.dst_port = cfg->rtx_dest_port;
        vqec_chan_assign_source_filter(
            &chan_desc->repair.filter,
            cfg->rtx_source_addr.s_addr,
            cfg->rtx_source_port,
            chan->src_ip_filter_enable,
            TRUE);
        /* so_rcvbuf: */
        chan_desc->repair.so_rcvbuf = syscfg->so_rcvbuf;
        /* scheduling_class: */
        chan_desc->repair.scheduling_class = VQEC_DP_FAST_SCHEDULE;
        /* payload type: */
        chan_desc->repair.rtp_payload_type = cfg->rtx_payload_type;
        /* datagram_size: */
        chan_desc->repair.datagram_size = syscfg->max_paksize;
        /* rtcp_dscp_value: */
        chan_desc->repair.rtcp_dscp_value = syscfg->rtcp_dscp_value;
    }


    /*
     * Set the RTCP XR max RLE size.  If the repair_rtcp_xr_loss_rle is
     * 0xffff (unspecified), it means pkt-loss-rle is not specified in
     * SDP. So set rtcp_xr_max_rle_size to 0.
     */
    if (cfg->rtx_rtcp_xr_loss_rle == RTCP_XR_LOSS_RLE_UNSPECIFIED) {
        chan_desc->repair.rtcp_xr_max_rle_size = 0;
    } else {
        chan_desc->repair.rtcp_xr_max_rle_size =
            cfg->rtx_rtcp_xr_loss_rle;
    }
    /*
     * However, set rtcp_xr_max_rle_size to MAX_RLE_SIZE if stats summary 
     * is specified, even if no pkt-loss-rle is enabled.
     */
    if (chan_desc->repair.rtcp_xr_max_rle_size == 0 &&
        cfg->rtx_rtcp_xr_stat_flags) {
        chan_desc->repair.rtcp_xr_max_rle_size = MAX_RLE_SIZE;
    }
    
    /*
     * fec0 stream info
     */
    chan_desc->en_fec0 = chan->fec_enabled;
    if (chan_desc->en_fec0) {
        chan_desc->fec_0.encap = VQEC_DP_ENCAP_RTP;
        /* filter: */
        chan_desc->fec_0.filter.proto = INPUT_FILTER_PROTO_UDP;
        chan_desc->fec_0.filter.u.ipv4.dst_ip =
            cfg->fec1_mcast_addr.s_addr;
        chan_desc->fec_0.filter.u.ipv4.dst_ifc_ip = chan->input_if_addr;
        chan_desc->fec_0.filter.u.ipv4.dst_port =
            cfg->fec1_mcast_port;
        vqec_chan_assign_source_filter(
            &chan_desc->fec_0.filter,
            cfg->fec1_source_addr.s_addr,
            htons(0),
            chan->src_ip_filter_enable,
            FALSE);
        /* so_rcvbuf: */
        chan_desc->fec_0.so_rcvbuf = syscfg->so_rcvbuf;
        /* scheduling_class: */
        chan_desc->fec_0.scheduling_class = VQEC_DP_SLOW_SCHEDULE;
        /* payload type: */
        chan_desc->fec_0.rtp_payload_type = cfg->fec1_payload_type;
        /* datagram_size: */
        chan_desc->fec_0.datagram_size = syscfg->max_paksize;
    }
    
    /*
     * fec1 stream info
     */
    chan_desc->en_fec1 = 
        chan->fec_enabled && (cfg->fec_mode == VQEC_FEC_2D_MODE);
    if (chan_desc->en_fec1) {
        chan_desc->fec_1.encap = VQEC_DP_ENCAP_RTP;
        /* filter: */
        chan_desc->fec_1.filter.proto = INPUT_FILTER_PROTO_UDP;
        chan_desc->fec_1.filter.u.ipv4.dst_ip =
            cfg->fec2_mcast_addr.s_addr;
        chan_desc->fec_1.filter.u.ipv4.dst_ifc_ip = chan->input_if_addr;
        chan_desc->fec_1.filter.u.ipv4.dst_port =
            cfg->fec2_mcast_port;
        vqec_chan_assign_source_filter(
            &chan_desc->fec_1.filter,
            cfg->fec1_source_addr.s_addr,
            htons(0),
            chan->src_ip_filter_enable,
            FALSE);
        /* so_rcvbuf: */
        chan_desc->fec_1.so_rcvbuf = syscfg->so_rcvbuf;
        /* scheduling_class: */
        chan_desc->fec_1.scheduling_class = VQEC_DP_SLOW_SCHEDULE;
        /* payload type: */
        chan_desc->fec_1.rtp_payload_type = cfg->fec2_payload_type;
        /* datagram_size: */
        chan_desc->fec_1.datagram_size = syscfg->max_paksize;
    }
done:
    return (status);
}


/* RTP session initialization attributes derived from system configuration. */
typedef
struct vqec_chan_rtp_attrib_
{
    in_addr_t src_addr;         /* input interface IP */
    char p_cname[VQEC_MAX_NAME_LEN]; 
                                /* cname */
    uint32_t sock_buf;          /* socket buffer depth */
    uint8_t rtcp_dscp_value;    /* DSCP value for xmitted RTCP packets */ 
    boolean nat_mode;           /* true if in nat mode */
} vqec_chan_rtp_attrib_t;


/**---------------------------------------------------------------------------
 * Helper to setup system configuration attributes used for RTP sessions.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] attrib Pointer to an attrib structure to be initialized.
 *---------------------------------------------------------------------------*/ 
static void
vqec_chan_copy_rtp_attrib (vqec_chan_t *chan,
                            vqec_chan_rtp_attrib_t *attrib)
{
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();

    memset(attrib, 0, sizeof(*attrib));

    attrib->src_addr = chan->input_if_addr;
    memcpy(attrib->p_cname, vqec_get_cname(), sizeof(attrib->p_cname));
    attrib->sock_buf = syscfg->so_rcvbuf;
    attrib->rtcp_dscp_value = syscfg->rtcp_dscp_value;
    attrib->nat_mode = vqec_nat_is_natmode_en();
}


/**---------------------------------------------------------------------------
 * Add a ERA primary session to the channel.
 *
 * @param[in] chan Pointer to the channel to which the session is added.
 * @param[in] id Identifier of the dataplane IS for the primary session.
 * @param[in] attrib Pointer to the system configuration attributes used 
 * during creation of the session.
 * @param[out]boolean Returns true if creation succeeds, false otherwise.
 *---------------------------------------------------------------------------*/ 
static boolean 
vqec_chan_add_era_session (vqec_chan_t *chan, 
                           vqec_dp_streamid_t id, 
                           vqec_chan_rtp_attrib_t *attrib)
{
    vqec_chan_cfg_t *cfg;
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;

    cfg = &chan->cfg;

    /* initialize the XR options */
    vqec_set_primary_rtcp_xr_options(cfg,  &rtcp_xr_cfg);

    vqec_set_primary_rtcp_bw(cfg, &rtcp_bw_cfg);
    chan->prim_session = 
        rtp_create_session_era_recv(
            id,
            attrib->src_addr,
            attrib->p_cname,
            cfg->primary_dest_addr.s_addr,
            cfg->primary_dest_rtcp_port,
            cfg->primary_source_rtcp_port,
            cfg->fbt_addr.s_addr,
            &rtcp_bw_cfg,
            &rtcp_xr_cfg,
            cfg->primary_dest_addr.s_addr,
            cfg->primary_dest_port,
            attrib->sock_buf,
            attrib->rtcp_dscp_value,
            cfg->primary_rtcp_rsize, 
            chan);

    return (chan->prim_session != NULL);    
}


/**---------------------------------------------------------------------------
 * Update an ERA primary session on a channel.
 *
 * @param[in] chan Pointer to the channel to which the session should be
 * updated.
 * @param[in] fbt_addr                 ip address of feedback target
 * @param[in] primary_source_rtcp_port Server port to which RTCP packets 
 *                                     are sent.  May be zero to disable 
 *                                     RTCP transmission.
 * @param[out]boolean Returns true if the update succeeds, false otherwise.
 *--------------------------------------------------------------------------*/ 
static boolean 
vqec_chan_update_era_session (vqec_chan_t *chan,
                              in_addr_t fbt_addr,
                              uint16_t primary_source_rtcp_port)
{
    return (rtp_update_session_era_recv(chan->prim_session,
                                        fbt_addr,
                                        primary_source_rtcp_port));
}


/**---------------------------------------------------------------------------
 * Add a repair session to the channel.
 *
 * @param[in] chan Pointer to the channel to which the session is added.
 * @param[in] id Identifier of the dataplane IS for the reapir session.
 * @param[in] attrib Pointer to the system configuration attributes used 
 * during creation of the session.
 * @param[out]boolean Returns true if creation succeeds, false otherwise.
 *---------------------------------------------------------------------------*/ 
static boolean
vqec_chan_add_repair_session (vqec_chan_t *chan, 
                              vqec_dp_streamid_t id, 
                              vqec_chan_rtp_attrib_t *attrib)
{
    vqec_chan_cfg_t *cfg;
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;
    uint32_t local_ssrc;

    /* Verify channel supplied with no repair session */
    VQEC_ASSERT(chan && !chan->repair_session);

    /* nothing to do if neither error-repair nor rcc is enabled. */
    /*
     * NOTE: The RTCP repair port will be opened if either ER or RCC is enabled
     *       in the channel configuration, regardless of whether ER and RCC are
     *       enabled via the command-line or VQE-C system configuration.  The
     *       reason for this is that, when error repair is configured for a
     *       channel in the lineup, the VQE-S will send RTCP Sender Reports
     *       on the repair stream whether or not the VQE-C is listening.  In
     *       the event that the VQE-C has not opened the port to which these
     *       Sender Reports are going, ICMP Port Unreachable packets will be
     *       sent back to the VQE-S creating extra network load.
     */
    if (!chan->cfg.er_enable && !chan->rcc_enabled) {
        return TRUE;
    }

    cfg = &chan->cfg;

    vqec_set_repair_rtcp_xr_options(cfg,  &rtcp_xr_cfg);
    vqec_set_repair_rtcp_bw(cfg, &rtcp_bw_cfg);
    
    /* 
     * We are using rtp session multiplexing, i.e. the primary and
     * repair session must share the same ssrc.  
     */
    if (!rtp_get_local_source_ssrc((rtp_session_t *)(chan->prim_session),
                                   &local_ssrc)) {
        return (FALSE);
    }

    chan->rtcp_rtx_port = cfg->rtx_dest_rtcp_port;

    chan->repair_session =
        rtp_create_session_repair_recv(
            id,
            attrib->src_addr,
            attrib->p_cname,
            cfg->rtx_source_addr.s_addr,
            &chan->rtcp_rtx_port,
            cfg->rtx_source_rtcp_port,
            local_ssrc,
            &rtcp_bw_cfg,
            &rtcp_xr_cfg,
            cfg->primary_dest_addr.s_addr,
            cfg->primary_dest_port,
            attrib->sock_buf,
            attrib->rtcp_dscp_value,
            cfg->primary_rtcp_rsize,
            chan);

    return (chan->repair_session != NULL);
}


/**---------------------------------------------------------------------------
 * Update repair session on a channel.
 *
 * @param[in] chan Pointer to the channel to which the session should be
 * updated.
 * @param[in] rtx_source_addr       ip address of feedback target   
 * @param[in] rtx_source_rtcp_port  Server port to which RTCP packets 
 *                                  are sent.  May be zero to disable 
 *                                  RTCP transmission.
 * @param[out]boolean Returns true if the update succeeds, false otherwise.
 *--------------------------------------------------------------------------*/ 
static boolean 
vqec_chan_update_repair_session (vqec_chan_t *chan,
                                 in_addr_t rtx_source_addr,
                                 uint16_t rtx_source_rtcp_port)
{
    return (rtp_update_session_repair_recv(chan->repair_session,
                                           rtx_source_addr,
                                           rtx_source_rtcp_port));
}


/**---------------------------------------------------------------------------
 * Open NAT bindings for a channel.
 *
 * @param[in] name String name of the binding.  May be up to 16 characters.
 * @param[in] id Identifies the channel.
 * @param[in] internal_addr Local addr on which pkts will be received
 * @param[in] internal_port Local port on which pkts will be received
 * @param[in] remote_addr Remote addr
 * @param[in] remote_port Remote port
 * @param[in] allow_update always do update no matter behind nat or not
 *                         used to support vod ICE session.
 * @param[out] Returns true on success, false on failure.
 *---------------------------------------------------------------------------*/ 
static inline vqec_nat_bindid_t
vqec_chan_nat_open (char *name,
                    vqec_chanid_t id,
                    in_addr_t internal_addr,
                    in_port_t internal_port,
                    in_addr_t remote_addr,
                    in_port_t remote_port,
                    boolean allow_update)
{
    vqec_nat_bind_desc_t desc;

    memset(&desc, 0, sizeof(desc));

    strncpy(desc.name, name, VQEC_NAT_API_NAME_MAX_LEN);
    desc.internal_addr = internal_addr;
    desc.internal_port = internal_port;
    desc.remote_addr = remote_addr;
    desc.remote_port = remote_port;
    desc.caller_id = id;
    desc.allow_update = allow_update;

    return (vqec_nat_open_binding(&desc));
}


static boolean
vqec_chan_open_nat_bindings (vqec_chan_t *chan) 
{
    char ufrag[2*VQEC_MAX_ICE_UFRAG_LENGTH];
    boolean allow_update = FALSE;

    if (chan->cfg.prim_nat_binding || !chan->cfg.stun_optimization) {
        /**
         * to support vod ICE session, need to do STUN update even if 
         * is_not_behind_nat flag is true;
         * If stun_optimization is false, need to do STUN update for
         * each channel change. 
         */
        allow_update = TRUE;
    }

    chan->repair_rtcp_nat_id = VQEC_NAT_BINDID_INVALID;
    chan->repair_rtp_nat_id = VQEC_NAT_BINDID_INVALID;
    chan->prim_rtcp_nat_id = VQEC_NAT_BINDID_INVALID;
    chan->prim_rtp_nat_id = VQEC_NAT_BINDID_INVALID;

    memset(ufrag, 0, 2*VQEC_MAX_ICE_UFRAG_LENGTH);
    strlcat(ufrag, chan->cfg.server_ufrag, VQEC_MAX_ICE_UFRAG_LENGTH);
    strlcat(ufrag, ":", VQEC_MAX_ICE_UFRAG_LENGTH);
    strlcat(ufrag, chan->cfg.client_ufrag,VQEC_MAX_ICE_UFRAG_LENGTH);

    VQEC_DEBUG(VQEC_DEBUG_NAT, "ICE ufrag = %s, allow_update = %d\n", 
               ufrag, allow_update);

    /*
     * Open the NAT binding for RTCP if ER is enabled in the channel
     * configuration, regardless of the system configuration setting.
     */
    if(chan->nat_enabled && 
       (chan->rcc_enabled || chan->cfg.er_enable)) {
        chan->repair_rtcp_nat_id = 
            vqec_chan_nat_open(ufrag,
                               chan->chanid,
                               chan->input_if_addr, 
                               chan->rtcp_rtx_port,
                               chan->cfg.rtx_source_addr.s_addr,
                               chan->cfg.rtx_source_rtcp_port,
                               allow_update);
        VQEC_DEBUG(VQEC_DEBUG_NAT, "repair rtcp nat id  = %u\n", 
                   chan->repair_rtcp_nat_id);
        if (chan->repair_rtcp_nat_id == VQEC_NAT_BINDID_INVALID) {
            return (FALSE);
        }
    }

    /* 
     * Only open NAT binding for RTP if ER is enabled in both channel and
     * system configurations.
     */
    if (chan->nat_enabled &&
        (chan->rcc_enabled || chan->er_enabled)) {
        chan->repair_rtp_nat_id = 
            vqec_chan_nat_open(ufrag,
                               chan->chanid,
                               chan->input_if_addr,
                               chan->rtp_rtx_port,
                               chan->cfg.rtx_source_addr.s_addr,
                               chan->cfg.rtx_source_port,
                               allow_update);
        VQEC_DEBUG(VQEC_DEBUG_NAT, "repair rtp nat id  = %u\n", 
                   chan->repair_rtp_nat_id);
        if (chan->repair_rtp_nat_id == VQEC_NAT_BINDID_INVALID) {
            return (FALSE);
        }
    }

    /* To support vod primary session and ICE*/
    if (chan->nat_enabled && chan->cfg.prim_nat_binding) {
        if (chan->cfg.primary_source_rtcp_port) {
            /* if RTCP source port is provided */
            chan->prim_rtcp_nat_id = 
                vqec_chan_nat_open(ufrag,
                                   chan->chanid,
                                   chan->input_if_addr, 
                                   chan->cfg.primary_dest_rtcp_port,
                                   chan->cfg.primary_source_addr.s_addr,
                                   chan->cfg.primary_source_rtcp_port,
                                   allow_update);
            VQEC_DEBUG(VQEC_DEBUG_NAT, "primary rtcp nat id  = %u\n", 
                       chan->prim_rtcp_nat_id);
            if (chan->prim_rtcp_nat_id == VQEC_NAT_BINDID_INVALID) {
                return (FALSE);
            }
        }
        
        chan->prim_rtp_nat_id = 
            vqec_chan_nat_open(ufrag,
                               chan->chanid,
                               chan->input_if_addr,
                               chan->cfg.primary_dest_port,
                               chan->cfg.primary_source_addr.s_addr,
                               chan->cfg.primary_source_port,
                               allow_update);
        VQEC_DEBUG(VQEC_DEBUG_NAT, "primary rtp nat id  = %u\n", 
                   chan->prim_rtp_nat_id);
        if (chan->prim_rtp_nat_id == VQEC_NAT_BINDID_INVALID) {
            return (FALSE);
        }
    }

    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Initialize error-repair gap report trigger for a channel.
 *
 * @param[in] chan Pointer to the channel.
 * @param[out] Returns true on success, false on failure.
 *---------------------------------------------------------------------------*/ 
static boolean
vqec_chan_init_er_poll (vqec_chan_t *chan) 
{
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();

    /*
     * Create and start an error repair event for this channel.
     * All channels currently use the same repair trigger time
     * based on global configuration. 
     */
#define MIN_REPAIR_TRIGGER_TIME VQEC_CHANNEL_POLL_INTERVAL
#define MIN_REPAIR_TRIGGER_BIAS_FACTOR 20

    if (TIME_CMP_R(ge,
                   TIME_ADD_R_R(
                       TIME_MK_R(msec, syscfg->repair_trigger_point_abs),
                       TIME_MK_R(msec, syscfg->reorder_delay_abs)),
                   TIME_MK_R(msec, syscfg->jitter_buff_size))) {
        snprintf(s_debug_str, DEBUG_STR_LEN,
                 "vqec_chan_init_er_poll:: WARNING: repair_trigger_point_abs "
                 "+ reorder_delay_abs is >= jitter_buff_size; some gaps may "
                 "not be repaired");
        syslog_print(VQEC_ERROR, s_debug_str);
    }
    chan->repair_trigger_time = 
        TIME_MK_R(msec, syscfg->repair_trigger_point_abs);
    if (TIME_CMP_R(lt, chan->repair_trigger_time, 
                   MIN_REPAIR_TRIGGER_TIME)) {
        chan->repair_trigger_time = MIN_REPAIR_TRIGGER_TIME; 
    }

    /* round repair_trigger_time */
    if (TIME_CMP_R(ne,
                   REL_TIME_0,
                   TIME_MOD_R_R(chan->repair_trigger_time, 
                                MIN_REPAIR_TRIGGER_TIME))) {

        chan->repair_trigger_time = 
            TIME_ADD_R_R(chan->repair_trigger_time, 
                         TIME_DIV_R_I(MIN_REPAIR_TRIGGER_TIME, 2));
        chan->repair_trigger_time =
            TIME_MULT_R_I(MIN_REPAIR_TRIGGER_TIME, 
                          TIME_DIV_R_R(chan->repair_trigger_time,
                                       MIN_REPAIR_TRIGGER_TIME));
    }

    /* 
     * Event code re-schedules a recurring timer prior to invoking
     * the handler - therefore with this inaccurancy it remains possible
     * that the gap report may not trigger at the next reporting interval,
     * although if there wasn't such an inaccurancy the full repair-trigger 
     * interval has expired. We thus bias the repair trigger time by a 
     * small fudge to address this issue.
     */
    chan->repair_trigger_time = 
        TIME_SUB_R_R(chan->repair_trigger_time,
                     TIME_DIV_R_I(MIN_REPAIR_TRIGGER_TIME,
                                  MIN_REPAIR_TRIGGER_BIAS_FACTOR));

    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Enable polling for error-repair.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
static void
vqec_chan_enable_er_poll (vqec_chan_t *chan) 
{
    vqec_dp_error_t status_dp;
    if (chan->er_enabled && !chan->er_poll_active) {        
        chan->er_poll_active = TRUE;
        chan->next_er_sched_time = get_sys_time();
        status_dp = vqec_dp_set_pcm_er_en_flag(chan->dp_chanid);
    }
}


/**---------------------------------------------------------------------------
 * Enable polling for error-repair with an immediate gap-report-trigger.
 *
 * @param[in] arg Channel identifier.
 *---------------------------------------------------------------------------*/
#if HAVE_FCC
static void 
vqec_chan_en_er_fastpoll (const vqec_event_t *const unused_evptr,
                          int unused_fd,
                          short unused_event,
                          void *arg)
{
    vqec_chan_t *chan;

    chan = vqec_chanid_to_chan((vqec_chanid_t)arg);
    if (!chan) {
        return;
    }

    vqec_gap_reporter_report(unused_evptr, 
                             unused_fd, 
                             unused_event, 
                             (void *)chan->chanid);

    vqec_chan_enable_er_poll(chan); 
}
#endif /* HAVE_FCC */

/**---------------------------------------------------------------------------
 * Compute and cache backfill parameters.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_upd_backfill (vqec_chan_t *chan)
{
#ifdef HAVE_FCC
    uint32_t rcc_max_concurrent;
    uint32_t npaks;
    rel_time_t fecfill, pkt_time;
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();
    vqec_chan_cfg_t *cfg = NULL;
    
    VQEC_ASSERT(chan);

    /*
     *                           pakpool_size            rcc_pakpool_max_pct
     * Num paks for RCC = -------------------------- * -----------------------
     *                        rcc_max_concurrent                 100
     *
     *                             rcc_pakpool_max_pct * pakpool_size
     *                        = ----------------------------------------
     *                                  100 * rcc_max_concurrent
     *
     * If rcc_max_concurrent is 0, then set it to 1, so that the RCC may
     * have access to the entire pakpool for the operation.
     */

    rcc_max_concurrent = syscfg->rcc_max_concurrent;
    if (!rcc_max_concurrent) {
        rcc_max_concurrent = 1;
    }
    npaks = (syscfg->rcc_pakpool_max_pct * syscfg->pakpool_size) /
        (100 * rcc_max_concurrent);
    chan->rcc_max_fill = 
        TIME_MULT_R_I(
            VQEC_CHAN_AVG_PKT_TIME(chan->cfg.primary_bit_rate), npaks);
    
    /*
     * This computation reflects the jitter buffer size computation
     * in the dataplane which is requested for minimum fill.
     *
     * ER             FEC               Min-Fill
     * TRUE        FALSE         jitter_buffer 
     * FALSE       TRUE          TIME(fec_block_size)
     * TRUE        TRUE          jitter_buffer +TIME(fec_block_size)
     * FALSE       FALSE         reorder_delay
     */
    chan->rcc_min_fill = REL_TIME_0;
    if (!chan->er_enabled && !chan->fec_enabled) {
        chan->rcc_min_fill = TIME_MK_R(msec, syscfg->reorder_delay_abs);
    } else {
        if (chan->er_enabled) {
            chan->rcc_min_fill = 
                TIME_MK_R(msec, syscfg->jitter_buff_size);
        } 
        if (chan->fec_enabled) {
            cfg = &chan->cfg;
            if (cfg->fec_l_value != 0 && 
                cfg->fec_d_value != 0 && 
                cfg->fec_order != VQEC_FEC_SENDING_ORDER_NOT_DECIDED) {
                switch (cfg->fec_order)
                {
                    case VQEC_FEC_SENDING_ORDER_ANNEXA:
                        npaks = cfg->fec_l_value * 
                            cfg->fec_d_value + 
                            cfg->fec_l_value;
                        break;

                    case VQEC_FEC_SENDING_ORDER_ANNEXB:
                        npaks = 2 * cfg->fec_l_value * 
                            cfg->fec_d_value;
                        break;
                    default:
                        npaks = 2 * cfg->fec_l_value * 
                            cfg->fec_d_value;
                        break;
                }
            } else {
                /* use default block size define at CP */
                npaks = g_channel_module->fec_default_block_size * 2;
            }

            /**
             * Choose the per packet time.
             * Here, we use the larger value of the cached rtp_ts_pkt_time
             * and the avg_pkt_time. If there is no cached value, the 
             * rtp_ts_pkt_time should be zero and avg_pkt_time will be used.
             *
             * The reason we use the larger value is that, sometimes, 
             * the value in SDP bitrate may not be set correctly. 
             * If the SDP bitrate was set too large, VQE-C FEC buffer 
             * may be smaller than required and late_fec_pkts could be seen.  
             */

            if (TIME_CMP_R(ge, 
                           VQEC_CHAN_AVG_PKT_TIME(chan->cfg.primary_bit_rate),
                           TIME_MK_R(usec, cfg->fec_ts_pkt_time))) {
                pkt_time =  VQEC_CHAN_AVG_PKT_TIME(chan->cfg.primary_bit_rate);
            } else {
                pkt_time =  
                    TIME_MK_R(usec, cfg->fec_ts_pkt_time);
            }

            fecfill = TIME_MULT_R_I(pkt_time, npaks);
            chan->rcc_min_fill = 
                TIME_ADD_R_R(chan->rcc_min_fill, fecfill);
        }
    }
    
    VQEC_DEBUG(VQEC_DEBUG_RCC,
               "Channel %s, setup min backfill %llu, max backfill %llu\n",
               vqec_chan_print_name(chan),
               TIME_GET_R(msec, chan->rcc_min_fill), 
               TIME_GET_R(msec, chan->rcc_max_fill));

#endif /* HAVE_FCC */
}


/**---------------------------------------------------------------------------
 * Binds the given channel to a tuner.
 *
 * Note that channels may be bound to more than one tuner.
 *
 * Upon a successful binding, the following actions are taken:
 *   o The tuner ID is stored with the channel object
 *   o A flowgraph is built (if the channel is not already bound to a tuner)
 *     or augmented (if the channel is already bound) in the dataplane for 
 *     the channel
 *   o A PLI-NACK is sent to initiate RCC (if the channel is not already 
 *     bound to a tuner)
 *
 * @param[in] chanid Channel to be bound
 * @param[in] tid Tuner to which the channel should be bound (specified
 * by its dataplane tuner ID).
 * @param[in] bp Bind parameters
 * @param[in] chan_event_cb: chan event cb function
 * @param[out] vqec_chan_err_t Returns VQEC_CHAN_ERR_OK on success,
 * and an error code on failure.
 *---------------------------------------------------------------------------*/ 
vqec_chan_err_t
vqec_chan_bind (vqec_chanid_t chanid,
                vqec_dp_tunerid_t tid,
                const vqec_bind_params_t *bp,
                const vqec_ifclient_chan_event_cb_t *chan_event_cb)
{
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    vqec_chan_t *chan = NULL;
    vqec_chan_tuner_entry_t *curr_tuner, *new_tuner_entry=NULL;
    boolean requested_binding_exists = FALSE, no_other_bindings = FALSE;
    vqec_dp_chan_desc_t vqec_dp_chan_desc;
    vqec_dp_graphinfo_t ginfo;
    vqec_dp_error_t status_dp;
    vqec_chan_rtp_attrib_t attrib;
    char temp[INET_ADDRSTRLEN];
    vqec_nat_bind_data_t data;
    vqec_ifclient_tr135_params_t *tr135_params;
    boolean policer_enabled;
    float temp_er_policer_rate, temp_er_policer_burst;
    uint32_t rate_percent, burst_millisecs;
    tb_retval_t tb_retval;
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();
    boolean ir_time_valid = FALSE;
    uint64_t ir_time = 0;
    boolean fec = FALSE, fec_set = FALSE;

    /* Validate state and parameters */
    if (!g_channel_module) {
        status = VQEC_CHAN_ERR_NOTINITIALIZED;
        goto done;
    }
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        status = VQEC_CHAN_ERR_NOSUCHCHAN;
        goto done;
    }

    /* If tuner is already bound to the channel, then nothing more to do */
    VQE_TAILQ_FOREACH(curr_tuner, &chan->tuner_list, list_obj) {
        if (curr_tuner->tid == tid) {
            requested_binding_exists = TRUE;
            goto done;
        }
    }

    /* Tuner not bound to channel, so bind it */
    new_tuner_entry = malloc(sizeof(vqec_chan_tuner_entry_t));
    if (!new_tuner_entry) {
        status = VQEC_CHAN_ERR_NOMEM;
        goto done;
    }
    new_tuner_entry->tid = tid;

    no_other_bindings = VQE_TAILQ_EMPTY(&chan->tuner_list);
    /* Perform tasks necessary for a previously-untuned channel */
    if (no_other_bindings) {

        /*
         * Source IP filtering is enabled for primary and FEC streams
         * based on system configuration.
         */
        chan->src_ip_filter_enable = syscfg->src_ip_filter_enable;
        
        if (!chan->cfg.prim_nat_binding) {
            /* nat traversal only enabled for multicast sessions */
            chan->nat_enabled = 
                IN_MULTICAST(ntohl(chan->cfg.primary_dest_addr.s_addr));
        } else {
            /* nat enable for both primary and repair session */
            chan->nat_enabled = TRUE;
        }

        /*
         * FEC is enabled if enabled 
         *  - on the channel, and
         *  - via CLI knob (if CLI knob is set), or
         *    via system config (if CLI knob is not set).
         */
        (void)vqec_dp_get_fec(&fec, &fec_set);
        chan->fec_enabled = chan->cfg.fec_enable && 
            (fec_set ? fec : syscfg->fec_enable);

        /*
         * ER is enabled if enabled
         *  - on the channel, and
         *  - via CLI knob (if CLI knob is set), or
         *    via system config (if CLI knob is not set).
         */
        chan->er_enabled = 
            chan->cfg.er_enable && vqec_get_error_repair(NULL);

        if (chan->er_enabled) {
            vqec_error_repair_policer_parameters_get(&policer_enabled, NULL,
                                                     &rate_percent, NULL,
                                                     &burst_millisecs, NULL);
            chan->er_policer_enabled = policer_enabled;
            if (chan->er_policer_enabled) {
                 temp_er_policer_rate = (float)rate_percent / 100 * 
                    chan->cfg.primary_bit_rate / VQEC_CHAN_MPEG_PAKSIZE_BITS;
                temp_er_policer_burst = temp_er_policer_rate *
                    burst_millisecs / 1000;
                if (temp_er_policer_rate < 1) {
                    /* 
                     * define the token bucket rate as at least one repair
                     * pkt/s
                     */
                    temp_er_policer_rate = 1;
                }
                if (temp_er_policer_burst < 1) {
                    /*
                     * define the token bucket burst as at least one repair
                     * pkt
                     */
                    temp_er_policer_burst = 1;
                }
                tb_retval = tb_init_simple(&chan->er_policer_tb,
                                           temp_er_policer_rate,
                                           temp_er_policer_burst);
                if (tb_retval != TB_RETVAL_OK) {
                    /*
                     * Force the error-repair policer to be disabled
                     * for this chan
                     */
                    chan->er_policer_enabled = FALSE;
                    snprintf(s_debug_str, DEBUG_STR_LEN,
                             "Failed to init repair policer:  %s",
                             tb_retval_to_str(tb_retval));
                    syslog_print(VQEC_ERROR, s_debug_str);
                }
            }
        }

        /*
         * RCC and fast-fill are enabled if enabled
         *  - on the channel, and
         *  - via CLI knob (if CLI knob is set), or
         *    via system config (if CLI knob is not set).
         */
#if HAVE_FCC
        chan->rcc_enabled = chan->cfg.rcc_enable && vqec_get_rcc(NULL);
        chan->fast_fill_enabled = 
            chan->rcc_enabled && vqec_get_fast_fill(NULL);
#else
        chan->rcc_enabled = FALSE;
        chan->fast_fill_enabled = FALSE;
#endif

        /* Check to make sure there aren't too many RCCs active */
        if (chan->rcc_enabled &&
            syscfg->rcc_max_concurrent &&
            (g_channel_module->num_current_rccs >=
             syscfg->rcc_max_concurrent)) {
            chan->rcc_enabled = FALSE;
            syslog_print(VQEC_CHAN_ERROR,
                         vqec_chan_print_name(chan),
                         "RCC operation rejected due to too many current RCCs "
                         "(check setting of \"rcc_max_concurrent\")");
            chan->stats.concurrent_rccs_limited++;
        }

        /* Qualify RCC & fastfill using bind parameters. */
        if (bp) {
            chan->rcc_enabled = chan->rcc_enabled && 
                vqec_ifclient_bind_params_is_rcc_enabled(bp);
            chan->fast_fill_enabled = chan->fast_fill_enabled &&
                vqec_ifclient_bind_params_is_fastfill_enabled(bp);
            if (vqec_ifclient_bind_params_get_fastfill_ops(bp)) {
                chan->fastfill_ops = 
                    *vqec_ifclient_bind_params_get_fastfill_ops(bp);
            }
            /*
             * Convert max_fastfill from bytes to msec:
             *  max_fastfill (msec) = (max_fastfill (bytes) * 8 (bits/byte)
             *                        * 1000 (msec/sec)) / bitrate (bits/sec)
             */
            if (chan->cfg.primary_bit_rate == 0) {
                /* don't know how much we can fastfill without a bitrate */
                chan->max_fastfill = REL_TIME_0;
            } else {
                chan->max_fastfill =
                    TIME_MK_R(msec,
                              ((uint64_t)
                               vqec_ifclient_bind_params_get_max_fastfill(bp)
                               * 8 * 1000)
                              / (uint64_t)chan->cfg.primary_bit_rate);
            }
            VQEC_DEBUG(VQEC_DEBUG_RCC,
                       "max_fastfill = %d bytes at %d bps = %lld msec\n",
                       vqec_ifclient_bind_params_get_max_fastfill(bp),
                       chan->cfg.primary_bit_rate,
                       TIME_GET_R(msec, chan->max_fastfill));
            chan->max_recv_bw_rcc =
                vqec_ifclient_bind_params_get_max_recv_bw_rcc(bp);
            chan->max_recv_bw_er =
                vqec_ifclient_bind_params_get_max_recv_bw_er(bp);
            /* decide which values to use for these */
            if (!chan->max_recv_bw_rcc && !chan->max_recv_bw_er) {
                /*
                 * both ER & RCC values from bind params are zero; fall back to
                 * SD/HD values
                 */
                if (chan->cfg.primary_bit_rate <
                    syscfg->min_hd_stream_bitrate) {
                    /* stream is SD */
                    chan->max_recv_bw_rcc = chan->max_recv_bw_er =
                        syscfg->max_receive_bandwidth_sd;
                    if (syscfg->max_receive_bandwidth_sd_rcc) {
                        chan->max_recv_bw_rcc = 
                            syscfg->max_receive_bandwidth_sd_rcc;
                    }
                } else {
                    /* stream is HD */
                    chan->max_recv_bw_rcc = chan->max_recv_bw_er =
                        syscfg->max_receive_bandwidth_hd;
                    if (syscfg->max_receive_bandwidth_hd_rcc) {
                        chan->max_recv_bw_rcc =
                            syscfg->max_receive_bandwidth_hd_rcc;
                    }
                }                    
            }
            chan->mrb_transition_cb =
                vqec_ifclient_bind_params_get_mrb_transition_cb(bp);
            if (chan->rcc_enabled &&
                (chan->max_recv_bw_rcc || chan->max_recv_bw_er)) {
                /*
                 * This variable will be transitioned to FALSE after some
                 * amount of time (defined by end-of-burst (as obtained from
                 * the APP packet) + a fudge factor time), and the
                 * mrb_transition_cb() function will be called at that time as
                 * well.  This time should be long enough so that the burst (in
                 * aggressive RCC mode) has finished coming into VQEC, and
                 * therefore it's safe to transition further error repairs to
                 * the max_recv_bw for ER instead of RCC.
                 */
                chan->use_rcc_bw_for_er = TRUE;
            }
            
            ir_time = vqec_ifclient_bind_params_get_ir_time(bp, 
                                                            &ir_time_valid);
            if (ir_time_valid) {
                chan->ir_time_valid = TRUE;
                chan->ir_time = TIME_MK_A(usec, ir_time);
            }
            
        }

        /* Update bind time. */
        chan->bind_time = get_sys_time();

        /* cache the context id */
        chan->context_id = vqec_ifclient_bind_params_get_context_id(bp);

        /* cache the function pointers */
        if (bp && vqec_ifclient_bind_params_get_dcr_info_ops(bp)
            && vqec_ifclient_bind_params_get_dcr_info_ops(bp)->get_dcr_stats) {
            chan->dcr_ops.get_dcr_stats = 
                vqec_ifclient_bind_params_get_dcr_info_ops(bp)->get_dcr_stats;
        }

        /* Update if address. */
        if (get_ip_address_by_if((char *)syscfg->input_ifname, 
                                 temp, 
                                 INET_ADDRSTRLEN)) {
            chan->input_if_addr = inet_addr(temp);
        } else {
            chan->input_if_addr = INADDR_ANY;
        }      
 
        /* Validate destination addresses against input_if_addr */
        if (chan->input_if_addr != INADDR_ANY) {
            if (!IN_MULTICAST(ntohl(chan->cfg.primary_dest_addr.s_addr)) &&
                chan->cfg.primary_dest_addr.s_addr != chan->input_if_addr) {
                snprintf(s_debug_str, DEBUG_STR_LEN, 
                         "primary dest addr does not match addr "
                         "of input_ifname configuration param");
                syslog_print(VQEC_ERROR, s_debug_str);
                status = VQEC_CHAN_ERR_INTERNAL;
                goto done;
            }
            if (chan->cfg.rtx_dest_addr.s_addr &&
                chan->cfg.rtx_dest_addr.s_addr != chan->input_if_addr) {
                snprintf(s_debug_str, DEBUG_STR_LEN,
                         "rtx dest addr does not match addr "
                         "of input_ifname configuration param");
                syslog_print(VQEC_ERROR, s_debug_str);
                status = VQEC_CHAN_ERR_INTERNAL;
                goto done;
            }
        }

        /* Create ER poll event - initializes repair trigger time. */
        if (chan->er_enabled && 
            !vqec_chan_init_er_poll(chan)) {
            status = VQEC_CHAN_ERR_INTERNAL;
            goto done;
        } 
        
        /* Create poll event for decoder stats */
        if (chan->dcr_ops.get_dcr_stats) {
            struct timeval tv;
                
            tv = rel_time_to_timeval(STB_DCR_STATS_POLL_INTERVAL);
            if (!vqec_event_create(&chan->stb_dcr_stats_ev,
                                   VQEC_EVTYPE_TIMER,
                                   VQEC_EV_RECURRING,
                                   vqec_chan_dcr_stats_event_handler,
                                   VQEC_EVDESC_TIMER,
                                   chan) ||
                !vqec_event_start(chan->stb_dcr_stats_ev, &tv)) {
                syslog_print(VQEC_CHAN_ERROR,
                             vqec_chan_print_name(chan),
                             "cannot start event to poll decoder stats");
            }            
        }
        
        /*
         * Initialize RCC backfill parameters.
         */
#if HAVE_FCC
        if (chan->rcc_enabled) {
            vqec_chan_upd_backfill(chan);
        }
#endif /* HAVE_FCC */
 
        /*
         * Must create a new graph for the channel in the data plane,
         * so load the data plane representation of a channel.
         */
        memset(&vqec_dp_chan_desc, 0, sizeof(vqec_dp_chan_desc_t));
        vqec_dp_chan_desc.cp_handle = chanid;

        if (!vqec_chan_load_dp_chan_desc(chan, &vqec_dp_chan_desc)) {
            status = VQEC_CHAN_ERR_INTERNAL;
            goto done;
        }

        /* Populate dp channel description for TR-135 bind params */
        if (bp) {
            tr135_params = 
                vqec_ifclient_bind_params_get_tr135_params(bp);

            if (tr135_params) {
               memcpy(&vqec_dp_chan_desc.tr135_params, tr135_params, 
                      sizeof(vqec_ifclient_tr135_params_t));
            }
        }

        /* Create the channel in the data plane. */
        status_dp =
            vqec_dp_graph_create(&vqec_dp_chan_desc, tid,
                                 VQEC_DP_ENCAP_RTP,
                                 &ginfo);
        if (status_dp != VQEC_DP_ERR_OK) {
            status = VQEC_CHAN_ERR_INTERNAL;
            goto done;
        }
        chan->graph_id = ginfo.id;
        chan->dp_chanid = ginfo.dpchan.dpchan_id;
        chan->prim_rtp_id = ginfo.dpchan.prim_rtp_id;
        chan->repair_rtp_id = ginfo.dpchan.repair_rtp_id;
        chan->fec0_rtp_id = ginfo.dpchan.fec0_rtp_id;
        chan->fec1_rtp_id = ginfo.dpchan.fec1_rtp_id;
        chan->inputshim.prim_rtp_id = ginfo.inputshim.prim_rtp_id;
        chan->inputshim.repair_rtp_id = ginfo.inputshim.repair_rtp_id;
        chan->inputshim.fec0_rtp_id = ginfo.inputshim.fec0_rtp_id;
        chan->inputshim.fec1_rtp_id = ginfo.inputshim.fec1_rtp_id;
        chan->outputshim.postrepair_id = ginfo.outputshim.postrepair_id;

        /* cache the chan event cb */
        if (chan_event_cb) {
            memcpy(&(chan->chan_event_cb), chan_event_cb, 
                   sizeof(vqec_ifclient_chan_event_cb_t));
        }

        /*
         * For a non-NAT mode multicast channel, the local repair dest port 
         * is assigned to be the repair stream's source port.  The dest
         * port supplied in the channel configuration is NOT used in this case.
         */
        if (!vqec_nat_is_natmode_en() &&
            IN_MULTICAST(ntohl(chan->cfg.primary_dest_addr.s_addr))) {
            chan->rtp_rtx_port = chan->cfg.rtx_source_port;
        }  else {
            chan->rtp_rtx_port = ginfo.inputshim.rtp_eph_rtx_port;
        }
    
        vqec_chan_copy_rtp_attrib(chan, &attrib);
        /* Add primary session. */
        if (!vqec_chan_add_era_session(chan, chan->prim_rtp_id, &attrib)) {
            status = VQEC_CHAN_ERR_INTERNAL;
            goto done;
        }

        /* Add repair session. */
        if (!vqec_chan_add_repair_session(chan, chan->repair_rtp_id, &attrib)) {
            status = VQEC_CHAN_ERR_INTERNAL;
            goto done;
        }

        /* Open NAT bindings for repair and primary(vod) rtcp & rtp ports */ 
        if (!vqec_chan_open_nat_bindings(chan)) {
            status = VQEC_CHAN_ERR_INTERNAL;
            goto done; 
        }

        /* Invoke immediate binding refresh for NAT. */
        if (chan->repair_rtcp_nat_id != VQEC_NAT_BINDID_INVALID) {
            (void)vqec_nat_query_binding(chan->repair_rtcp_nat_id,
                                         &data, TRUE);
        }
        if (chan->repair_rtp_nat_id != VQEC_NAT_BINDID_INVALID) {
            (void)vqec_nat_query_binding(chan->repair_rtp_nat_id,
                                         &data, TRUE);
        }

        /* added for vod and ICE support, immediatelly refresh */
        if (chan->prim_rtcp_nat_id != VQEC_NAT_BINDID_INVALID) {
            (void)vqec_nat_query_binding(chan->prim_rtcp_nat_id,
                                         &data, TRUE);
        }
        if (chan->prim_rtp_nat_id != VQEC_NAT_BINDID_INVALID) {
            (void)vqec_nat_query_binding(chan->prim_rtp_nat_id,
                                         &data, TRUE);
        }

#ifdef HAVE_FCC
        chan->app_timeout_interval = 
            TIME_MK_R(msec, syscfg->rcc_start_timeout);

        /* Initialize the finite state machine */
        vqec_sm_init(chan);

        if (chan->rcc_enabled) {
            if (!vqec_sm_deliver_event(chan,
                                       VQEC_EVENT_RAPID_CHANNEL_CHANGE, NULL)) {
                status = VQEC_CHAN_ERR_INTERNAL;
                goto done;
            }
            g_channel_module->num_current_rccs++;

            if (!chan->nat_enabled || 
                !vqec_nat_is_natmode_en() || 
                !vqec_nat_is_behind_nat()) {
                /* For non-NAT mode immediately send APP. */
                chan->rtp_nat_complete = 
                    chan->rtcp_nat_complete = TRUE;
                if (!vqec_sm_deliver_event(chan, 
                                           VQEC_EVENT_NAT_BINDING_COMPLETE, NULL)) {
                    status = VQEC_CHAN_ERR_INTERNAL;
                    goto done;
                }
            }
            
        } else {
            if (!vqec_sm_deliver_event(chan,
                                       VQEC_EVENT_SLOW_CHANNEL_CHANGE, NULL)) {
                status = VQEC_CHAN_ERR_INTERNAL;
                goto done;
            }

            /* Run ER poll event immediately. */
            if (chan->er_enabled) {
                vqec_chan_enable_er_poll(chan);
            }            
        } 
   
#else

        /* Run ER poll event. */
        if (chan->er_enabled) {
            vqec_chan_enable_er_poll(chan);
        }
#endif /* HAVE_FCC */

        /* Update bye counts / delay for a subsequent destroy. */
        chan->bye_count = syscfg->num_byes;
        chan->bye_delay = TIME_MK_R(msec, syscfg->bye_delay);

    } else {
        /* Update the channel's existing graph */
        status_dp = vqec_dp_graph_add_tuner(chan->graph_id, tid);
        if (status_dp != VQEC_DP_ERR_OK) {
            status = VQEC_CHAN_ERR_INTERNAL;
            goto done;
        }
    }

    /* Tuner bind operations succeeded, so add the new tuner to the channel */
    VQE_TAILQ_INSERT_HEAD(&chan->tuner_list, new_tuner_entry, list_obj);

done:
    if (status != VQEC_CHAN_ERR_OK) {
        if (new_tuner_entry) {
            free(new_tuner_entry);
        }
        if (no_other_bindings && chan) {
            vqec_chan_destroy(chan, TRUE);
        }

        snprintf(s_debug_str, DEBUG_STR_LEN, ": %s",
                 vqec_chan_err2str(status));
    } else {
        snprintf(s_debug_str, DEBUG_STR_LEN, ": %s",
                 (requested_binding_exists ? 
                  "requested binding already exists" :
                  (no_other_bindings ? "created channel binding" : 
                   "updated channel binding")));
    };
    VQEC_CHAN_DEBUG("\n%s(chanid=0x%08x,tid=%u)%s", __FUNCTION__, chanid, tid,
                    s_debug_str);
    return (status);
}

/**
 * Unbinds the given channel from the specified tuner.
 *
 * Upon a successful unbinding, the following actions are taken:
 *   o The tuner ID is removed from the channel object
 *   o The channel's flowgraph is modified to exclude the tuner (if other
 *     tuners remain bound to the channel) or destroyed (if no other tuners
 *     remain bound to the channel)
 *   o All its other control and dataplane resources are destroyed
 *
 * @param[in] chanid channel to be unbound
 * @param[in] tid tuner to be unbound from the given channel
 * @param[out] vqec_chan_err_t Returns VQEC_CHAN_ERR_OK if the unbind
 * was successful, and an error code on failure.
 */
vqec_chan_err_t
vqec_chan_unbind (vqec_chanid_t chanid, vqec_dp_tunerid_t tid) 
{
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    vqec_chan_t *chan;
    vqec_chan_tuner_entry_t *curr_tuner, *matching_tuner = NULL;
    static vqec_ifclient_stats_channel_t chan_stats;

    /* Validate state and parameters */
    if (!g_channel_module) {
        status = VQEC_CHAN_ERR_NOTINITIALIZED;
        goto done;
    }
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        status = VQEC_CHAN_ERR_NOSUCHCHAN;
        goto done;
    }

    /* If the given tuner is not bound to the channel, nothing more to do */
    VQE_TAILQ_FOREACH(curr_tuner, &chan->tuner_list, list_obj) {
        if (curr_tuner->tid == tid) {
            matching_tuner = curr_tuner;
            break;
        }
    }
    if (!matching_tuner) {
        goto done;
    }
    
    /*
     * If the channel has only one tuner associated with it, then it will be 
     * destroyed after the tuner is unbound from it.  So query for its stats 
     * just prior to the unbind, and roll them into the the global/historical
     * stats for inclusion in the "show counters" display.
     */
    if (VQE_TAILQ_FIRST(&chan->tuner_list) == 
        VQE_TAILQ_LAST(&chan->tuner_list, vqec_chan_tuner_head_)) {
        status = vqec_chan_get_counters_channel(chan->chanid, &chan_stats, 
                                                FALSE);
        if (status == VQEC_CHAN_ERR_OK) {
            vqec_ifclient_add_stats(&g_channel_module->historical_stats,
                                    &chan_stats);
        }
        status = vqec_chan_get_counters_channel(chan->chanid, &chan_stats, 
                                                TRUE);
        if (status == VQEC_CHAN_ERR_OK) {
            vqec_ifclient_add_stats(
                &g_channel_module->historical_stats_cumulative,
                &chan_stats);
        }

    }

    /* Unbind the channel's tuner */
    VQE_TAILQ_REMOVE(&chan->tuner_list, matching_tuner, list_obj);
    free(matching_tuner);
    (void)vqec_dp_graph_del_tuner(tid);
    
    /* If the channel has no tuners still associated, destroy it */
    if (VQE_TAILQ_EMPTY(&chan->tuner_list)) {
        vqec_chan_destroy(chan, FALSE);
    }

done:
    if (status != VQEC_CHAN_ERR_OK) {
        snprintf(s_debug_str, DEBUG_STR_LEN, ": %s",
                 vqec_chan_err2str(status));
    } else {
        s_debug_str[0] = '\0';
    }
    VQEC_CHAN_DEBUG("\n%s(chanid=0x%08x,tid=%u)%s", __FUNCTION__, chanid, tid,
                    s_debug_str);
    return (status);
}


/*
 * Updates the source-related configuration of an active channel, for the 
 * lifetime of the active channel.  All tuners bound to this active channel
 * will use the updated configuration.
 *
 * Only the source-related fields of the "cfg" structure are relevant, 
 * and the rest are ignored (not updated).  Fields related to the repair
 * stream are only updated if error repair is enabled on the channel.
 *
 * Relevant fields are listed below:
 *  - primary_source_addr
 *  - primary_source_port
 *  - fbt_addr
 *  - primary_source_rtcp_port
 *  - rtx_source_addr
 *  - rtx_source_port
 *  - rtx_source_rtcp_port
 *
 * @param[in] chanid             Identifies channel to update
 * @param[in] cfg                Contains updated config (see fields above)
 * @param[out] vqec_chan_err_t   Results of update:
 *    VQEC_CHAN_ERR_OK           - success
 *    VQEC_CHAN_ERR_NOSUCHCHAN   - channel to update not found
 *    VQEC_CHAN_ERR_INVALIDARGS  - invalid updated channel config
 *    VQEC_CHAN_ERR_INTERNAL     - update failed, info written to syslog/debugs
 *                                 The channel is left in an unspecified state.
 */
vqec_chan_err_t 
vqec_chan_update_source (vqec_chanid_t chanid,
                         const vqec_chan_cfg_t *cfg)
{
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    vqec_chan_t *chan = NULL;
    vqec_chan_cfg_t proposed_cfg;
    vqec_dp_input_filter_t primary_fil, repair_fil;
    
    /* Caller MUST supply new channel config */
    if (!cfg) {
        status = VQEC_CHAN_ERR_INTERNAL;
        goto done;
    }

    /* Look up caller's channel */
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        status = VQEC_CHAN_ERR_NOSUCHCHAN;
        goto done;
    }

    /* 
     * Create a channel config which represents the channel's complete
     * proposed configuration and validate it.
     */
    memcpy(&proposed_cfg, &chan->cfg, sizeof(vqec_chan_cfg_t));
    proposed_cfg.primary_source_addr = cfg->primary_source_addr;
    proposed_cfg.primary_source_port = cfg->primary_source_port;
    proposed_cfg.fbt_addr = cfg->fbt_addr;
    proposed_cfg.primary_source_rtcp_port = cfg->primary_source_rtcp_port;
    proposed_cfg.rtx_source_addr = cfg->rtx_source_addr;
    proposed_cfg.rtx_source_port = cfg->rtx_source_port;
    proposed_cfg.rtx_source_rtcp_port = cfg->rtx_source_rtcp_port;
    if (!vqec_chan_validate_cfg(&proposed_cfg)) {
        status = VQEC_CHAN_ERR_INVALIDARGS;
        goto done;
    }

    /* Verify active channel is bound to at least one tuner */
    if (VQE_TAILQ_EMPTY(&chan->tuner_list)) {
        status = VQEC_CHAN_ERR_INTERNAL;
        goto done;
    }

    /*
     * Update the destination for primary and repair RTCP reports
     * so they are sent to the new sources.
     */
    if (!vqec_chan_update_era_session(chan,
                                      proposed_cfg.fbt_addr.s_addr,
                                      proposed_cfg.primary_source_rtcp_port)) {
        status = VQEC_CHAN_ERR_INTERNAL;
        goto done;
    }
    if (chan->cfg.er_enable &&
        !vqec_chan_update_repair_session(chan,
                                         proposed_cfg.rtx_source_addr.s_addr,
                                         proposed_cfg.rtx_source_rtcp_port)) {
        status = VQEC_CHAN_ERR_INTERNAL;
        goto done;
    }
    
    /*
     * If source filtering is in effect for the channel,
     * update the dataplane source filters for the primary and repair traffic.
     */
    if (chan->src_ip_filter_enable) {
        vqec_chan_assign_source_filter(
            &primary_fil,
            proposed_cfg.primary_source_addr.s_addr,
            proposed_cfg.primary_source_port,
            chan->src_ip_filter_enable,
            !IN_MULTICAST(ntohl(proposed_cfg.primary_dest_addr.s_addr)));
        if (chan->er_enabled) {
            vqec_chan_assign_source_filter(
                &repair_fil,
                proposed_cfg.rtx_source_addr.s_addr,
                proposed_cfg.rtx_source_port,
                chan->src_ip_filter_enable,
                TRUE);
        }
        if (vqec_dp_chan_update_source(chan->dp_chanid,
                                       &primary_fil, 
                                       chan->er_enabled ? &repair_fil : NULL)
            != VQEC_DP_ERR_OK) {
            status = VQEC_CHAN_ERR_INTERNAL;
            goto done;
        }
    }
      
    /* If update succeeded, update the channel's cached config */
    memcpy(&chan->cfg, &proposed_cfg, sizeof(vqec_chan_cfg_t));

    chan->stats.active_cfg_updates++;
    chan->last_cfg_update_ts = get_sys_time();

done:
    VQEC_CHAN_DEBUG("%s(chanid=0x%08x)%s", __FUNCTION__, chanid,
                    vqec_chan_err2str(status));
    return (status);
}

/**
 * Update the nat binding of a channel. This function close 
 * the previous nat binding of the channel and restart a 
 * new nat binding process. It is mainly use for VOD failover
 * support.
 * 
 * @param[in] chanid  Identifies channel to update.
 * @param[out] vqec_chan_err_t VQEC_CHAN_ERR_OK upon success,
 * or a failure code indicating why the channel could not be updated.
 */
vqec_chan_err_t 
vqec_chan_update_nat_binding (vqec_chanid_t chanid)
{
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    vqec_chan_t *chan = NULL;
    vqec_nat_bind_data_t data;
 
    /* Look up caller's channel */
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        status = VQEC_CHAN_ERR_NOSUCHCHAN;
        goto done;
    }

    /* destroy previous NAT bindings. */
    if (chan->repair_rtcp_nat_id != VQEC_NAT_BINDID_INVALID) {
        vqec_nat_close_binding(chan->repair_rtcp_nat_id);
        chan->repair_rtcp_nat_id = VQEC_NAT_BINDID_INVALID;
    }
    if (chan->repair_rtp_nat_id != VQEC_NAT_BINDID_INVALID) {
        vqec_nat_close_binding(chan->repair_rtp_nat_id);
        chan->repair_rtp_nat_id = VQEC_NAT_BINDID_INVALID;
    } 
    /* for vod primary session nat bindings*/
    if (chan->prim_rtcp_nat_id != VQEC_NAT_BINDID_INVALID) {
        vqec_nat_close_binding(chan->prim_rtcp_nat_id);
        chan->prim_rtcp_nat_id = VQEC_NAT_BINDID_INVALID;
    }
    if (chan->prim_rtp_nat_id != VQEC_NAT_BINDID_INVALID) {
        vqec_nat_close_binding(chan->prim_rtp_nat_id);
        chan->prim_rtp_nat_id = VQEC_NAT_BINDID_INVALID;
    } 

    /* Open NAT bindings for the repair and primary (vod) rtcp and rtp ports.*/ 
    if (!vqec_chan_open_nat_bindings(chan)) {
        status = VQEC_CHAN_ERR_INTERNAL;
        goto done; 
    }

    memset(&data, 0, sizeof(vqec_nat_bind_data_t));
    /* Invoke immediate binding refresh for NAT. */
    if (chan->repair_rtcp_nat_id != VQEC_NAT_BINDID_INVALID) {
        (void)vqec_nat_query_binding(chan->repair_rtcp_nat_id,
                                     &data, TRUE);
    }
    if (chan->repair_rtp_nat_id != VQEC_NAT_BINDID_INVALID) {
        (void)vqec_nat_query_binding(chan->repair_rtp_nat_id,
                                     &data, TRUE);
    }

    /* added for vod and ICE support, immediatelly refresh */
    if (chan->prim_rtcp_nat_id != VQEC_NAT_BINDID_INVALID) {
        (void)vqec_nat_query_binding(chan->prim_rtcp_nat_id,
                                     &data, TRUE);
    }
    if (chan->prim_rtp_nat_id != VQEC_NAT_BINDID_INVALID) {
        (void)vqec_nat_query_binding(chan->prim_rtp_nat_id,
                                     &data, TRUE);
    }

done: 
    return (status);
}


/**
 * This API accepts a vqec_ifclient_stats_t structure, which is the
 * public data structure for exporting aggregated data across ALL
 * channels.
 *
 * @param[out] stats Pointer to the aggregate/global statistics structure.
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
 * @param[in] cumulative Flag indicating that Cumulative counters are desired.
 */
vqec_chan_err_t
vqec_chan_get_counters (vqec_ifclient_stats_t *stats, boolean cumulative)
{
    vqec_chan_t *chan;
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    static vqec_ifclient_stats_channel_t per_chan_stats;

    /* Validate state and parameters */
    if (!g_channel_module) {
        status = VQEC_CHAN_ERR_NOTINITIALIZED;
        goto done;
    }
    if (!stats) {
        status = VQEC_CHAN_ERR_INVALIDARGS;
        goto done;
    }
    
    if (cumulative) {
        /* combined stats requested for all active and historical channels */
        memcpy(stats, &g_channel_module->historical_stats_cumulative,
               sizeof(vqec_ifclient_stats_t));
    } else {
        /* combined stats requested for all active and historical channels */
        memcpy(stats, &g_channel_module->historical_stats,
               sizeof(vqec_ifclient_stats_t));
    }
    VQE_LIST_FOREACH(chan, &g_channel_module->channel_list, list_obj) {
        if (chan->shutdown) {
            continue;
        }
        status = vqec_chan_get_counters_channel(chan->chanid, 
                                                &per_chan_stats, 
                                                cumulative);
        if (status != VQEC_CHAN_ERR_OK) {
            goto done;
        }
        vqec_ifclient_add_stats(stats, &per_chan_stats);
    }
done:
    return (status);
}

/**
 * Populate counters for an individual channel.
 *
 * This API accepts a vqec_ifclient_stats_channel_t structure, which is
 * the public data structure for exporting data on an individual channel.
 *
 * Note that
 *   1) Additional stats/fields may be added to vqec_ifclient_stats_channel_t
 *      in the future, including ones which match the names of those defined
 *      in vqec_ifclient_stats_t, as requirements for retrieving these stats 
 *      appear.
 *   2) The stats retrieved by this API start accumulating when the channel
 *      becomes active, and are NOT cleared by the "clear counters"
 *      command or vqec_ifclient_clear_stats() API since there is no current
 *      requirement for this behavior.
 *   3) This API does not populate stats collected at the tuner(s) 
 *      connected to this channel (e.g. tuner_queue_drops will not be populated
 *      by this API) 
 * 
 * @param[in] chanid           ID of channel whose counters are requested
 * @param[out] stats           Pointer to the channel statistics structure.
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
 */
vqec_chan_err_t
vqec_chan_get_counters_channel (vqec_chanid_t chanid, 
                                vqec_ifclient_stats_channel_t *stats, 
                                boolean cumulative)
{

    vqec_chan_t *chan;
    vqec_dp_rtp_src_table_t table;
    rtp_hdr_source_t *sender_stats;
    int i;
    vqec_dp_chan_stats_t dpchan_stats;
    vqec_dp_pcm_status_t pcm_stats;
    vqec_dp_fec_status_t fec_stats;
#ifdef HAVE_FCC
    vqec_dp_rcc_data_t rcc_data;
#endif
    vqec_dp_error_t err;
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    rtp_era_recv_t *prim_session;
    rtp_repair_recv_t *repair_session;
    vqec_ifclient_rcc_abort_t rcc_abort_reason = VQEC_IFCLIENT_RCC_ABORT_MAX;

    /* Validate state and parameters */
    if (!g_channel_module) {
        status = VQEC_CHAN_ERR_NOTINITIALIZED;
        goto done;
    }
    if (!stats) {
        status = VQEC_CHAN_ERR_INVALIDARGS;
        goto done;
    }
    memset(stats, 0, sizeof(vqec_ifclient_stats_channel_t));
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        status = VQEC_CHAN_ERR_NOSUCHCHAN;
        goto done;
    }
    
    /* retrieve stats table for for channel's primary stream from DP */
    if (vqec_dp_chan_rtp_input_stream_src_get_table(chan->prim_rtp_id, &table)
        != VQEC_DP_ERR_OK) {
        status = VQEC_CHAN_ERR_INTERNAL;
        goto done;
    }
    /* 
     * Walk through table sources, and look for the sender/source with
     * pktflow_permitted set to TRUE.  There will only be at most one of
     * these, and it's this sender whose stats will be reported.
     * If no sender exists as the packet-flow source, then the stats
     * reflfect no activity by having zero'd values.
     */
    for (i=0; i < table.num_entries; i++) {
        if (table.sources[i].info.pktflow_permitted) {
            sender_stats = &table.sources[i].info.src_stats;
            /* expected pkts = (highest seq# rcvd) - (base seq# rcvd) + 1 */
            stats->primary_rtp_expected = 
                ((sender_stats->cycles + sender_stats->max_seq) 
                 - sender_stats->base_seq + 1);
            /* lost pkts = (expected pkts) - (received pkts) */
            stats->primary_rtp_lost =
                stats->primary_rtp_expected - sender_stats->received;
            break;
        }
    }

    if (!cumulative) {
        stats->primary_rtp_expected -=
            chan->primary_rtp_expected_snapshot;
        stats->primary_rtp_lost -= 
            chan->primary_rtp_lost_snapshot;    
    }

    bzero(&dpchan_stats, sizeof(dpchan_stats));
    bzero(&pcm_stats, sizeof(pcm_stats));
    bzero(&fec_stats, sizeof(fec_stats));
#ifdef HAVE_FCC
    bzero(&rcc_data, sizeof(rcc_data));
#endif
        
    if (chan->shutdown || (chan->dp_chanid == VQEC_DP_CHANID_INVALID)) {
        /* Inactive channel */
        goto done;
    }
    
    err = vqec_dp_chan_get_status(chan->dp_chanid, 
                                  &pcm_stats,
                                  NULL, 
                                  &fec_stats,
                                  &dpchan_stats,
                                  NULL,
                                  cumulative);
    if (err != VQEC_DP_ERR_OK) {
        status = VQEC_CHAN_ERR_INTERNAL;
        goto done;
    }
#ifdef HAVE_FCC
    err = vqec_dp_chan_get_rcc_status(chan->dp_chanid, &rcc_data);
    if (err != VQEC_DP_ERR_OK) {
        status = VQEC_CHAN_ERR_INTERNAL;
        goto done;
    }
#endif
        
    /* stream counters */
    prim_session = vqec_chan_get_primary_session(chan);
    if (prim_session) {
        stats->primary_rtcp_inputs = 
            ((rtp_session_t *)prim_session)->rtcp_stats.pkts.rcvd;
        stats->primary_rtcp_outputs = 
            ((rtp_session_t *)prim_session)->rtcp_stats.pkts.sent;
        if (!cumulative) {
            stats->primary_rtcp_inputs -= chan->primary_rtcp_inputs_snapshot;
            stats->primary_rtcp_outputs -= chan->primary_rtcp_outputs_snapshot;
        }
    }
    repair_session = vqec_chan_get_repair_session(chan);
    if (repair_session) {
        stats->repair_rtcp_inputs =
            ((rtp_session_t *)repair_session)->rtcp_stats.pkts.rcvd;
        if (!cumulative) {
            stats->repair_rtcp_inputs -=
                chan->repair_rtcp_inputs_snapshot;
        }
    }       

    stats->primary_udp_inputs = dpchan_stats.udp_rx_paks;
    stats->primary_rtp_inputs = dpchan_stats.primary_rx_paks;
    stats->primary_udp_drops = dpchan_stats.primary_udp_rx_drops;
    stats->primary_rtp_drops =
        (dpchan_stats.primary_pcm_drops_total 
         - dpchan_stats.primary_pcm_drops_duplicate)
        + dpchan_stats.primary_sm_early_drops
        + dpchan_stats.primary_rtp_rx_drops
        + dpchan_stats.primary_sim_drops;
    stats->primary_rtp_drops_late = dpchan_stats.primary_pcm_drops_late;
      
    stats->repair_rtp_inputs = dpchan_stats.repair_rx_paks;
    stats->repair_rtp_drops =
        (dpchan_stats.repair_pcm_drops_total
         - dpchan_stats.repair_pcm_drops_duplicate)
         + dpchan_stats.repair_sm_early_drops
         + dpchan_stats.repair_rtp_rx_drops
         + dpchan_stats.repair_sim_drops;
    stats->repair_rtp_drops_late = dpchan_stats.repair_pcm_drops_late;
        
    stats->fec_inputs = fec_stats.fec_total_paks;
    stats->fec_drops =
       fec_stats.fec_rtp_hdr_invalid +
       fec_stats.fec_hdr_invalid + 
       fec_stats.fec_late_paks +
       fec_stats.fec_drops_other;
    stats->fec_drops_late = fec_stats.fec_late_paks;

    stats->repair_rtp_stun_inputs = chan->stats.nat_ejects_rtp;
    stats->repair_rtcp_stun_inputs = chan->stats.nat_ejects_rtcp;
    stats->repair_rtp_stun_outputs = chan->stats.nat_injects_rtp;
    stats->repair_rtcp_stun_outputs = chan->stats.nat_injects_rtcp;
    if (!cumulative) {
        stats->repair_rtp_stun_inputs -= 
            chan->stats_snapshot.nat_ejects_rtp;
        stats->repair_rtcp_stun_inputs -= 
            chan->stats_snapshot.nat_ejects_rtcp;
        stats->repair_rtp_stun_outputs -= 
            chan->stats_snapshot.nat_injects_rtp;
        stats->repair_rtcp_stun_outputs -= 
            chan->stats_snapshot.nat_injects_rtcp;
    }

    stats->post_repair_outputs = pcm_stats.total_tx_paks;
    stats->underruns = pcm_stats.under_run_counter;
        
    /* error repair counters */
    stats->pre_repair_losses = pcm_stats.input_loss_pak_counter;
    stats->post_repair_losses = pcm_stats.output_loss_pak_counter +
        rcc_data.outp_data.output_loss_paks_in_rcc;
    stats->post_repair_losses_rcc = rcc_data.outp_data.output_loss_paks_in_rcc;
    if (chan->er_enabled) {
        stats->repairs_requested = chan->stats.total_repairs_requested;
        stats->repairs_policed = chan->stats.total_repairs_policed;
        if (!cumulative) {
            stats->repairs_requested -= 
                chan->stats_snapshot.total_repairs_requested;
            stats->repairs_policed -= 
                chan->stats_snapshot.total_repairs_policed;
        }
    }
    stats->fec_recovered_paks = fec_stats.fec_recovered_paks;

    /* TR-135 Stats */
    stats->tr135_overruns = 
        pcm_stats.tr135.mainstream_stats.overruns;
    stats->tr135_underruns = 
        pcm_stats.tr135.mainstream_stats.underruns;
    stats->tr135_packets_expected = 
        pcm_stats.tr135.mainstream_stats.total_stats_after_ec.packets_expected;
    stats->tr135_packets_received = 
        pcm_stats.tr135.mainstream_stats.total_stats_after_ec.packets_received;
    stats->tr135_packets_lost = 
        pcm_stats.tr135.mainstream_stats.total_stats_after_ec.packets_lost;
    stats->tr135_packets_lost_before_ec =
        pcm_stats.tr135.mainstream_stats.total_stats_before_ec.packets_lost;
    stats->tr135_loss_events =
        pcm_stats.tr135.mainstream_stats.total_stats_after_ec.loss_events;
    stats->tr135_loss_events_before_ec =
        pcm_stats.tr135.mainstream_stats.total_stats_before_ec.loss_events;
    stats->tr135_severe_loss_index_count =
  pcm_stats.tr135.mainstream_stats.total_stats_after_ec.severe_loss_index_count;
    stats->tr135_minimum_loss_distance =
    pcm_stats.tr135.mainstream_stats.total_stats_after_ec.minimum_loss_distance;
    stats->tr135_maximum_loss_period =
      pcm_stats.tr135.mainstream_stats.total_stats_after_ec.maximum_loss_period;
    stats->tr135_buffer_size =
        pcm_stats.tr135.buffer_size;
    stats->tr135_gmin = 
        pcm_stats.tr135.mainstream_stats.tr135_params.gmin;
    stats->tr135_severe_loss_min_distance =
        pcm_stats.tr135.mainstream_stats.tr135_params.severe_loss_min_distance;

    /* rcc */
    if (!chan->cc_cleared || cumulative) {
        stats->channel_change_requests = 1;
        stats->concurrent_rccs_limited = chan->stats.concurrent_rccs_limited;
        if (chan->rcc_enabled) {
#ifdef HAVE_FCC
            stats->rcc_requests = 1;
            if (vqec_chan_rcc_abort_reason(&rcc_abort_reason,
                                           chan, &rcc_data)) {
                stats->rcc_aborts_total = 1;
                stats->rcc_abort_stats[rcc_abort_reason] = 1;
            } else if (rcc_data.outp_data.output_loss_paks_in_rcc) {
                stats->rcc_with_loss = 1;
            }
#endif
        }
    }

done:
    return (status);    
}

/**
 * API to set TR-135 parameters for a channel
 *
 * @param[in] chanid ID of channel whose counters are requested
 * @param[in] params TR-135 writable parameters for this channel 
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
 */
vqec_chan_err_t
vqec_chan_set_tr135_params (vqec_chanid_t chanid, 
                            vqec_ifclient_tr135_params_t *params)
{
    vqec_chan_t *chan;
    vqec_dp_error_t err;
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;

    /* Validate state and parameters */
    if (!g_channel_module) {
        status = VQEC_CHAN_ERR_NOTINITIALIZED;
        goto done;
    }
    if (!params) {
        status = VQEC_CHAN_ERR_INVALIDARGS;
        goto done;
    }
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        status = VQEC_CHAN_ERR_NOSUCHCHAN;
        goto done;
    }
    
    if (chan->shutdown || (chan->dp_chanid == VQEC_DP_CHANID_INVALID)) {
        /* Inactive channel */
        status = VQEC_CHAN_ERR_NOSUCHCHAN;
        goto done;
    }

    err = vqec_dp_chan_set_tr135_params(chan->dp_chanid, params);
    if (err != VQEC_DP_ERR_OK) {
        status = VQEC_CHAN_ERR_INTERNAL;
    }
    
done:
    return (status);    
}

/*
 * Populate TR-135 sample counters for an individual channel.
 * @param[in] chanid           ID of channel whose counters are requested
 * @param[out] stats           Pointer to the tr135 sample statistics structure.
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
 */
vqec_chan_err_t
vqec_chan_get_counters_channel_tr135_sample (
            vqec_chanid_t chanid, 
            vqec_ifclient_stats_channel_tr135_sample_t *stats)
{
    vqec_chan_t *chan;
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    vqec_dp_error_t err;

    /* Validate state and parameters */
    if (!g_channel_module) {
        status = VQEC_CHAN_ERR_NOTINITIALIZED;
        goto done;
    }
    if (!stats) {
        status = VQEC_CHAN_ERR_INVALIDARGS;
        goto done;
    }
    memset(stats, 0, sizeof(vqec_ifclient_stats_channel_tr135_sample_t));
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        status = VQEC_CHAN_ERR_NOSUCHCHAN;
        goto done;
    }

    /* 
     * Get tr135 sample params, and reset the stats, start the new sample 
     * interval
     */
    err = vqec_dp_chan_get_stats_tr135_sample(chan->dp_chanid, stats);
    if (err != VQEC_DP_ERR_OK) {
        status = VQEC_CHAN_ERR_INTERNAL;
    }
done:
    return (status);
}


/**
 * Clears the channel counters.
 *
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
 */
vqec_chan_err_t
vqec_chan_clear_counters (void)
{
    vqec_chan_t *chan;
    rtp_era_recv_t *prim_session;
    rtp_repair_recv_t *repair_session;
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    vqec_dp_rtp_src_table_t table;
    rtp_hdr_source_t *sender_stats;
    int i;

    if (!g_channel_module) {
        goto done;
    }

    /* Clear historical channel stats */
    memset(&g_channel_module->historical_stats, 0,
           sizeof(vqec_ifclient_stats_t));

    /* Clear active channel stats */
    VQE_LIST_FOREACH(chan,
                     &g_channel_module->channel_list,
                     list_obj) {
        if (chan->shutdown || (chan->dp_chanid == VQEC_DP_CHANID_INVALID)) {
            continue;
        }
 
        /* retrieve stats table for for channel's primary stream from DP */
        if (vqec_dp_chan_rtp_input_stream_src_get_table(chan->prim_rtp_id,
                                                        &table)
            != VQEC_DP_ERR_OK) {
            status = VQEC_CHAN_ERR_INTERNAL;
            continue;
        }

        /*
         * Walk through table sources, and look for the sender/source with
         * pktflow_permitted set to TRUE.  There will only be at most one of
         * these, and it's this sender whose stats will be reported.
         * If no sender exists as the packet-flow source, then the stats
         * reflfect no activity by having zero'd values.
         */
         for (i=0; i < table.num_entries; i++) {
             if (table.sources[i].info.pktflow_permitted) {
                 sender_stats = &table.sources[i].info.src_stats;
                 /*
                  * expected pkts = (highest seq# rcvd) -
                  * (base seq# rcvd) + 1
                  */
                 chan->primary_rtp_expected_snapshot =
                     ((sender_stats->cycles + sender_stats->max_seq)
                      - sender_stats->base_seq + 1);
                 /* lost pkts = (expected pkts) - (received pkts) */
                 chan->primary_rtp_lost_snapshot =
                     chan->primary_rtp_expected_snapshot -
                     sender_stats->received;
                 break;
            }
        }

        /* Take a snapshot of the RTCP parameters */
        prim_session = vqec_chan_get_primary_session(chan);
        if (prim_session) {
            chan->primary_rtcp_inputs_snapshot =
                ((rtp_session_t *)prim_session)->rtcp_stats.pkts.rcvd;
             chan->primary_rtcp_outputs_snapshot =
                ((rtp_session_t *)prim_session)->rtcp_stats.pkts.sent;
         }
        repair_session = vqec_chan_get_repair_session(chan);
        if (repair_session) {
            chan->repair_rtcp_inputs_snapshot=
                ((rtp_session_t *)repair_session)->rtcp_stats.pkts.rcvd;
        }


        /*
         * Counters were cleared *after* this channel became active,
         * so exclude this channel's effect on channel change stats
         */
        chan->cc_cleared = TRUE;

        /* Take a snapshot of CP stats */
        memcpy(&chan->stats_snapshot, &chan->stats, sizeof(vqec_chan_stats_t));

        if (vqec_dp_chan_clear_stats(chan->dp_chanid) != VQEC_DP_ERR_OK) {
            syslog_print(VQEC_CHAN_ERROR,
                         vqec_chan_print_name(chan),
                         "clear stats IPC failure");
            status = VQEC_CHAN_ERR_INTERNAL;
        }
    }
done:
    return (status);
}

/**---------------------------------------------------------------------------
 * From CP channel id to DP channel id. (only use for CLI interface).
 *
 * @param[in] chanid CP channel identifier.
 * @param[out] vqec_dp_chanid_t Dataplane channel identifier.  
 *---------------------------------------------------------------------------*/ 
vqec_dp_chanid_t
vqec_chan_id_to_dpchanid (vqec_chanid_t chanid)
{
    vqec_chan_t *chan;
    
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        return (VQEC_DP_CHANID_INVALID);
    }
    
    return (chan->dp_chanid);
}



/**---------------------------------------------------------------------------
 * NAT support.
 *---------------------------------------------------------------------------*/ 


/**---------------------------------------------------------------------------
 * Inject a NAT packet into the repair RTP socket. The RTP socket is
 * owned by the dataplane, hence the packet is sent to the dataplane via
 * IPC, and will be queued to the socket in the dataplane.
 * 
 * @param[in] chan Pointer to the channel.
 * @param[in] remote_addr Specifies NAT packet's destination
 * @param[in] remote_port Specifies NAT packet's destination
 * @param[in] buf Pointer to the packet contents.
 * @param[in] len Length of the contents.
 * @param[out] boolean Returns true if the packet was successfully enQd
 * in the socket.
 *---------------------------------------------------------------------------*/ 
static boolean
vqec_chan_send_to_rtp_socket (vqec_chan_t *chan,
                              in_addr_t remote_addr, 
                              in_port_t remote_port,
                              char *buf, 
                              uint16_t len) 
{
    vqec_dp_error_t err;

    err = 
        vqec_dp_graph_repair_inject(chan->graph_id,
                                    remote_addr,
                                    remote_port,
                                    buf,
                                    len);
    if (err != VQEC_DP_ERR_OK) {
        return (FALSE);
    }
            
    return (TRUE);
}

/**---------------------------------------------------------------------------
 * Inject a NAT packet into the repair RTP socket. The RTP socket is
 * owned by the dataplane, hence the packet is sent to the dataplane via
 * IPC, and will be queued to the socket in the dataplane.
 * 
 * @param[in] chan Pointer to the channel.
 * @param[in] remote_addr Specifies NAT packet's destination
 * @param[in] remote_port Specifies NAT packet's destination
 * @param[in] buf Pointer to the packet contents.
 * @param[in] len Length of the contents.
 * @param[out] boolean Returns true if the packet was successfully enQd
 * in the socket.
 *---------------------------------------------------------------------------*/ 
static boolean
vqec_chan_send_to_primary_rtp_socket (vqec_chan_t *chan,
                                      in_addr_t remote_addr, 
                                      in_port_t remote_port,
                                      char *buf, 
                                      uint16_t len) 
{
    vqec_dp_error_t err;

    err = 
        vqec_dp_graph_primary_inject(chan->graph_id,
                                    remote_addr,
                                    remote_port,
                                    buf,
                                    len);
    if (err != VQEC_DP_ERR_OK) {
        return (FALSE);
    }
            
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Inject a packet into the repair RTP or RTCP sockets.
 * 
 * @param[in] chanid Identifier of the channel.
 * @param[in] bindid Identifier of the binding.
 * @param[in] desc Attributes of the binding.
 * @param[in] buf Pointer to the packet contents.
 * @param[in] len Length of the contents.
 * @param[out] boolean Returns true if the packet was successfully enQd
 * in the socket (RTCP) or delivered to the dataplane.
 *---------------------------------------------------------------------------*/ 
boolean 
vqec_chan_nat_inject_pak (vqec_chanid_t chanid, vqec_nat_bindid_t bindid, 
                          vqec_nat_bind_desc_t *desc, char *buf, uint16_t len)
{
    vqec_chan_t *chan;
    boolean ret = TRUE;
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();

    if (!g_channel_module || !desc || !buf || !len) {
        return (FALSE);
    }

    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        return (FALSE);
    }

    if (bindid == chan->repair_rtcp_nat_id) {
        chan->stats.nat_injects_rtcp++;
    } else if (bindid == chan->repair_rtp_nat_id) {
        chan->stats.nat_injects_rtp++;
    }

    if (len > syscfg->max_paksize) {
        chan->stats.nat_inject_err_drops++;
        return (FALSE);
    }

    if (bindid == chan->repair_rtcp_nat_id) {
        /* Inject packet into rtcp session. */
        if (!MCALL(chan->repair_session, send_to_rtcp_socket, 
                   desc->remote_addr, desc->remote_port, buf, len)) {
            chan->stats.nat_inject_input_drops++;
            ret = FALSE;
            VQEC_DEBUG(VQEC_DEBUG_NAT, 
                       "Inject STUN pak to repair rtcp fail: bindid = %u\n",
                       bindid);
        }

    } else if (bindid == chan->repair_rtp_nat_id) {
        /* Inject packet through the dataplane graph ... */
        if (!vqec_chan_send_to_rtp_socket(chan,
                                          desc->remote_addr, 
                                          desc->remote_port, 
                                          buf, 
                                          len)) {
            chan->stats.nat_inject_input_drops++;
            ret = FALSE;
            VQEC_DEBUG(VQEC_DEBUG_NAT, 
                       "Inject STUN pak to repair rtp fail: bindid = %u\n",
                       bindid);
        }
        
    } else if (bindid == chan->prim_rtcp_nat_id) {
        /* Inject packet into primary rtcp session. */
        if (!rtp_era_send_to_rtcp_socket(chan->prim_session,
                                         desc->remote_addr, 
                                         desc->remote_port, 
                                         buf, len)) {
            chan->stats.nat_inject_input_drops++;
            ret = FALSE;
            VQEC_DEBUG(VQEC_DEBUG_NAT, 
                       "Inject STUN pak to primary rtp fail: bindid = %u\n",
                       bindid);
        }

    } else if (bindid == chan->prim_rtp_nat_id) {
        /* Inject packet through the dataplane graph ... */
        if (!vqec_chan_send_to_primary_rtp_socket(chan,
                                                  desc->remote_addr, 
                                                  desc->remote_port, 
                                                  buf, 
                                                  len)) {
            chan->stats.nat_inject_input_drops++;
            ret = FALSE;
            VQEC_DEBUG(VQEC_DEBUG_NAT, 
                       "Inject STUN pak to primary rtp fail: bindid = %u\n",
                       bindid);
        }
    } else {
        chan->stats.nat_inject_err_drops++;
        ret = FALSE;
    }

    return (ret);
}


/**---------------------------------------------------------------------------
 * A NAT binding update - propagate to RCC state machine.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[in] bindid Identifier of the binding.
 * @param[in] data Attributes and current mapping for the binding.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_nat_bind_update (vqec_chanid_t chanid, vqec_nat_bindid_t bindid, 
                           vqec_nat_bind_data_t *data)
{
#ifdef HAVE_FCC   
    vqec_chan_t *chan;

    if (!g_channel_module || !data) {
        return;
    }

    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        return;
    }

    if (chan->repair_rtp_nat_id == bindid && !chan->rtp_nat_complete) {
        chan->rtp_nat_complete = TRUE;
        if (chan->rtcp_nat_complete) {
            /* sa_ignore {failure handled in state-machine} IGNORE_RETURN(2) */            
            (void)vqec_sm_deliver_event(chan, 
                                        VQEC_EVENT_NAT_BINDING_COMPLETE, NULL);
        }
    } else if (chan->repair_rtcp_nat_id == bindid && !chan->rtcp_nat_complete) {
        chan->rtcp_nat_complete = TRUE;
        if (chan->rtp_nat_complete) {
            /* sa_ignore {failure handled in state-machine} IGNORE_RETURN(2) */
            (void)vqec_sm_deliver_event(chan, 
                                        VQEC_EVENT_NAT_BINDING_COMPLETE, NULL);
        }
    }
#endif /* HAVE_FCC */
}


/**---------------------------------------------------------------------------
 * Display NAT bindings for the channel.
 *
 * @param[in] chanid Identifier of the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_show_nat_bindings (vqec_chanid_t chanid)
{
    vqec_chan_t *chan;

    if (!g_channel_module) {
        return;
    }

    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        return;
    }

    CONSOLE_PRINTF("\n");
    CONSOLE_PRINTF("--- NAT status ---\n");

    if (!chan->cfg.er_enable && !chan->rcc_enabled) {
        vqec_nat_print(" ER and RCC disabled; no NAT bindings!\n");
        return;
    }

    if (chan->repair_rtp_nat_id != VQEC_NAT_BINDID_INVALID) {
        CONSOLE_PRINTF("NAT id:                     %lu\n", 
                       chan->repair_rtp_nat_id);
        vqec_nat_fprint_binding(chan->repair_rtp_nat_id);
    }
    if (chan->repair_rtcp_nat_id != VQEC_NAT_BINDID_INVALID) {
        CONSOLE_PRINTF("NAT id:                     %lu\n", 
                       chan->repair_rtcp_nat_id);
        vqec_nat_fprint_binding(chan->repair_rtcp_nat_id);
    }

}


/**---------------------------------------------------------------------------
 * Retrieve the latest udp-nat port mappings for the channel.
 *
 * @param[in] chan Pointer to the channel instance.
 * @param[out] rtp_port RTP external mapped udp port.
 * @param[out] rtcp_Port RTCP external mapped udp port.
 * @param[out] boolean The method returns true if a valid mapping
 * is returned for the rtp and rtcp ports, otherwise returns false.
 *---------------------------------------------------------------------------*/ 
boolean
vqec_chan_get_rtx_nat_ports (vqec_chan_t *chan, 
                             in_port_t *rtp_port, in_port_t *rtcp_port)
{
    boolean ret;
    vqec_nat_bind_data_t data;

    if (!chan || !rtp_port || !rtcp_port || chan->shutdown) {
        return (FALSE);
    }

    *rtp_port = 
        *rtcp_port = 0;

    if (chan->repair_rtp_nat_id == VQEC_NAT_BINDID_INVALID &&
        chan->repair_rtcp_nat_id == VQEC_NAT_BINDID_INVALID) {
        return (FALSE);
    }

    if (chan->repair_rtp_nat_id != VQEC_NAT_BINDID_INVALID) {
        memset(&data, 0, sizeof(data));
        ret = vqec_nat_query_binding(chan->repair_rtp_nat_id,
                                     &data, FALSE);
        if (!ret) {
            return (FALSE);
        }

        *rtp_port = data.ext_port;
    }

    if (chan->repair_rtcp_nat_id != VQEC_NAT_BINDID_INVALID) {
        memset(&data, 0, sizeof(data));
        ret = vqec_nat_query_binding(chan->repair_rtcp_nat_id,
                                     &data, FALSE);
        if (!ret) {
            return (FALSE);
        }

        *rtcp_port = data.ext_port;
    }
    
    return (TRUE);    
}


/**---------------------------------------------------------------------------
 * Eject a RTCP packet to the NAT protocol.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] buf Pointer to the packet contents.
 * @param[in] len Length of the contents.
 * @param[in] session_t Type of session.  
 * @param[in] source_ip: source ip of the packet
 * @param[in] source_port: source port of the packet
 * @param[out] boolean Returns true if the packet was successfully sent
 * to the NAT protocol, false otherwise.
 *---------------------------------------------------------------------------*/ 
boolean
vqec_chan_eject_rtcp_nat_pak (vqec_chanid_t chanid,
                              char *buf, uint16_t len, 
                              vqec_chan_rtp_session_type_t sess_t,
                              in_addr_t source_ip, in_port_t source_port)
{
    vqec_chan_t *chan;

    if (!g_channel_module) {
        return (FALSE);
    }

    chan = vqec_chanid_to_chan(chanid);
    if (!chan || 
        !buf || 
        !len) {
        return (FALSE);
    }
    
    chan->stats.nat_ejects_rtcp++;
    if (sess_t == VQEC_CHAN_RTP_SESSION_REPAIR) {
        /* eject MAT pak from repair session */
        if (chan->repair_rtcp_nat_id == VQEC_NAT_BINDID_INVALID) {
            chan->stats.nat_eject_err_drops++;
            return (FALSE);
        }
        vqec_nat_eject_to_binding(chan->repair_rtcp_nat_id, buf, 
                                  len, source_ip, source_port);
    } else if (sess_t == VQEC_CHAN_RTP_SESSION_PRIMARY) {
        /* eject MAT pak from primary session */
        if (chan->prim_rtcp_nat_id == VQEC_NAT_BINDID_INVALID) {
            chan->stats.nat_eject_err_drops++;
            return (FALSE);
        }
        vqec_nat_eject_to_binding(chan->prim_rtcp_nat_id, buf, 
                                  len, source_ip, source_port);
    } else {
        return (FALSE);
    }

    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Eject a RTP packet from the dataplane to the NAT protocol.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[in] dp_chanid Identifier of the dataplane channel.
 * @param[in] buf Pointer to the packet contents.
 * @param[in] len Length of the contents.
 * @param[in] session_t Type of session.  
 * @param[in] source_ip: source ip of the packet
 * @param[in] source_port: source port of the packet
 * @param[out] boolean Returns true if the packet was successfully sent
 * to the NAT protocol, false otherwise.
 *---------------------------------------------------------------------------*/ 
boolean
vqec_chan_eject_rtp_nat_pak (vqec_chanid_t chanid,
                             vqec_dp_chanid_t dp_chanid, char *buf, uint16_t len,
                             vqec_dp_streamid_t is_id,
                             in_addr_t source_ip, in_port_t source_port)
    
{
    vqec_chan_t *chan;

    if (!g_channel_module) {
        return (FALSE);
    }

    chan = vqec_chanid_to_chan(chanid);
    if (!chan || 
        !buf || 
        !len) {        
        return (FALSE);
    }
    
    chan->stats.nat_ejects_rtp++;


    if (is_id == chan->repair_rtp_id) {
        /* eject NAT pak from repair session */
        if ((chan->dp_chanid != dp_chanid) ||
            (chan->repair_rtp_nat_id == VQEC_NAT_BINDID_INVALID)) {
            chan->stats.nat_eject_err_drops++;
            return (FALSE);
        } 
        VQEC_DEBUG(VQEC_DEBUG_NAT, 
                   "Proto: STUN recvd pak from repair, bindid = %u\n",
                   chan->repair_rtp_nat_id);

        vqec_nat_eject_to_binding(chan->repair_rtp_nat_id, buf, len, 
                                  source_ip, source_port);
    } else if (is_id == chan->prim_rtp_id) {
        /* eject NAT pak from primary session */
        if ((chan->dp_chanid != dp_chanid) ||
            (chan->prim_rtp_nat_id == VQEC_NAT_BINDID_INVALID)) {
            chan->stats.nat_eject_err_drops++;
            return (FALSE);
        } 
        VQEC_DEBUG(VQEC_DEBUG_NAT, 
                   "Proto: STUN recvd pak from primary, bindid = %u\n",
                   chan->prim_rtp_nat_id);

        vqec_nat_eject_to_binding(chan->prim_rtp_nat_id, buf, len, 
                                  source_ip, source_port);
    } else {
        return FALSE;
    }

    return (TRUE); 
}


/**---------------------------------------------------------------------------
 * Poll the STB to get channel change information. 
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] cur_time Current system time. 
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_update_dcr_stats (vqec_chan_t *chan)
{
    vqec_ifclient_dcr_stats_t dcr_stats;

    memset(&dcr_stats, 0, sizeof(vqec_ifclient_dcr_stats_t));
    (*chan->dcr_ops.get_dcr_stats)(chan->context_id,
                                   &dcr_stats);
    
    if (!chan->ir_time_valid) {
        chan->ir_time = TIME_MK_A(usec, dcr_stats.ir_time);
    }

    /* Don't populate timing info if we are in the middle of a channel
     * change. Specifically, if ir_time has changed, but the new picture
     * has not yet been displayed.  */
    if (TIME_CMP_A(ge, chan->ir_time, 
                   TIME_MK_A(usec, dcr_stats.display_time))) {
        return;
    }
    
    chan->fst_decode_time = TIME_MK_A(usec, dcr_stats.fst_decode_time);
    /* translate 45KHZ PTS value to 90KHZ,  so x2 here */
    chan->last_decoded_pts = dcr_stats.last_decoded_pts * 2;
    chan->dec_picture_cnt = dcr_stats.dec_picture_cnt;
    chan->pat_time = TIME_MK_A(usec, dcr_stats.pat_time);
    chan->pmt_time = TIME_MK_A(usec, dcr_stats.pmt_time);
    chan->display_time = TIME_MK_A(usec, dcr_stats.display_time);
    /* translate 45KHZ PTS value to 90KHZ,  so x2 here */
    chan->display_pts = dcr_stats.display_pts*2;

}

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
vqec_chan_dcr_stats_event_handler (const vqec_event_t *const unused_evptr,
                                   int unused_fd,
                                   short unused_event,
                                   void *arg)
{
    vqec_chan_t *chan;

    chan = (vqec_chan_t *)arg;
    VQEC_ASSERT(chan);

    vqec_chan_update_dcr_stats(chan);
 
    /* Keep polling until we get the display time */
    if (TIME_CMP_A(gt, chan->display_time, ABS_TIME_0)) {
        vqec_event_stop(chan->stb_dcr_stats_ev);
    }
}

/**---------------------------------------------------------------------------
 * Get the tuner Q drops for all the tuners bound to a channel
 *
 * @param[in]  chanid              Channel identifier.
 * @param[out] tuner_queue_drops   Pointer to the uint64_t variable
 *                                that will be populated by this function
 * @param[in]  cumulative          Boolean specifying if cumulative stats
 *                                are desired
 * @returns vqec_chan_err_t
 *---------------------------------------------------------------------------*/
vqec_chan_err_t vqec_chan_get_tuner_q_drops (vqec_chanid_t chanid,
                                             uint64_t *tuner_queue_drops,
                                             boolean cumulative)
{
    vqec_chan_tuner_entry_t *curr_tuner;
    vqec_dp_output_shim_tuner_status_t output_shim_tuner_status;
    vqec_dp_error_t status_dp;
    vqec_chan_t *chan;

    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        return VQEC_CHAN_ERR_NOSUCHCHAN;
    }

    /* Load the "sink" stats for each tuner bound to this channel */
    *tuner_queue_drops = 0;
    VQE_TAILQ_FOREACH(curr_tuner, &chan->tuner_list, list_obj) {
        status_dp =
            vqec_dp_output_shim_get_tuner_status(curr_tuner->tid,
                                                 &output_shim_tuner_status,
                                                 cumulative);
        if (status_dp == VQEC_DP_ERR_OK) {
            *tuner_queue_drops += output_shim_tuner_status.qdrops;
        } else {
            return VQEC_CHAN_ERR_INTERNAL;
        }
    }
    return VQEC_CHAN_ERR_OK;
}

/* 
 * Collect XR DC stats and populate structure. Some of the fields must
 * be retrieved from the DP and should be filled in before this is called
 *
 * @param[in] chan Pointer to a vqec_chan_t structure
 * @param[out] xr_dc_stats Output pointer to location of stats
 */
void 
vqec_chan_collect_xr_dc_stats(vqec_chan_t *chan,
                              rtcp_xr_dc_stats_t *xr_dc_stats)
{
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    static vqec_ifclient_stats_channel_t chan_stats;
    uint64_t tuner_queue_drops;

    /*
     * Get TR-135 underrun and overrun. This is exactly what we want for
     * XR-DC underrun and overrun respectively.
     */
    if (chan == NULL) {
        goto not_updated;
    }
    if (!vqec_chanid_to_chan(chan->chanid)) {
        goto not_updated;
    }
    status = vqec_chan_get_counters_channel(chan->chanid, &chan_stats,
                                            TRUE);
    if (status != VQEC_CHAN_ERR_OK) {
        goto not_updated;
    }

    /* Populate the tuner Q drops for this channel */
    tuner_queue_drops = 0;
    status = vqec_chan_get_tuner_q_drops(chan->chanid, 
                                         &tuner_queue_drops, TRUE);
 
    xr_dc_stats->type[XR_DC_UNDERRUNS] = chan_stats.tr135_underruns;
    xr_dc_stats->type[XR_DC_OVERRUNS] = chan_stats.tr135_overruns;
    xr_dc_stats->type[XR_DC_POST_REPAIR_LOSSES] = chan_stats.post_repair_losses;
    xr_dc_stats->type[XR_DC_PRIMARY_RTP_DROPS_LATE] = 
        chan_stats.primary_rtp_drops_late;
    xr_dc_stats->type[XR_DC_REPAIR_RTP_DROPS_LATE] = 
        chan_stats.repair_rtp_drops_late;
    xr_dc_stats->type[XR_DC_OUTPUT_QUEUE_DROPS] = tuner_queue_drops; 
    xr_dc_stats->is_updated = TRUE;
    return;
not_updated:
    xr_dc_stats->is_updated = FALSE;
    return;
}

/**-----------------------------------------------------------------------
 * Collect XR MA stats and populate structure. Some of the fields must
 * be retrieved from the DP and should be filled in before this is called
 *
 * @param[in] chanid Identifier of the channel
 * @param[in] force Force sending an MA message. This does not wait for all
 *                data to be present, and sends whatever is available.
 * @param[out] xr_ma_stats Output pointer to location of stats
 *-------------------------------------------------------------------------*/
void
vqec_chan_collect_xr_ma_stats (vqec_chan_t *chan,
                               boolean force,
                               rtcp_xr_ma_stats_t *xr_ma_stats) 
{
    boolean burst_data = FALSE, pres_data = FALSE;
    int32_t num_gap_pkts;

    xr_ma_stats->to_be_sent = FALSE;

#ifdef HAVE_FCC
    if (TIME_CMP_A(ge, chan->nakpli_sent_time, chan->bind_time)) {
        xr_ma_stats->app_req_to_rtcp_req =
            TIME_GET_R(msec, TIME_SUB_A_A(chan->nakpli_sent_time,
                                          chan->bind_time));
    }

    num_gap_pkts = xr_ma_stats->first_prim_to_last_rcc_seq_diff - 1;
    xr_ma_stats->num_gap_pkts = (num_gap_pkts >= 0) ? num_gap_pkts : 0;

    /* Use the channel event mask to detect a failure on the client side. */
    xr_ma_stats->status = ERA_RCC_SUCCESS;

    if (!chan->rcc_enabled) {
        xr_ma_stats->status = ERA_RCC_NORCC;

    } else if (chan->rcc_in_abort) {
      if ((chan->event_mask & (1 << VQEC_EVENT_RECEIVE_INVALID_APP)) ||
          (chan->event_mask & (1 << VQEC_EVENT_RCC_START_TIMEOUT)) ||
          (chan->event_mask & (1 << VQEC_EVENT_RCC_IPC_ERR))) {
          xr_ma_stats->status = ERA_RCC_CLIENT_FAIL;
          burst_data = TRUE;

      } else if (chan->event_mask & (1 << VQEC_EVENT_RECEIVE_NULL_APP)) {
          xr_ma_stats->status = ERA_RCC_BAD_REQ;
      }
    }
                
    /* Don't expect burst data to show up when RCC is disabled, 
     * or the server rejected the burst
     */
    switch (xr_ma_stats->status) {
    case ERA_RCC_NORCC:
    case ERA_RCC_BAD_REQ:
    case ERA_RCC_SERVER_RES:
    case ERA_RCC_NETWORK_RES:
    case ERA_RCC_NO_CONTENT:
        burst_data = TRUE;
        break;

    case ERA_RCC_SUCCESS:
    case ERA_RCC_CLIENT_FAIL:  /* Server still sent burst, show those stats */
        if (TIME_CMP_A(ge, xr_ma_stats->first_repair_ts,
                       chan->nakpli_sent_time)) {
            xr_ma_stats->rtcp_req_to_burst =
                TIME_GET_R(msec, TIME_SUB_A_A(xr_ma_stats->first_repair_ts,
                                              chan->nakpli_sent_time));
        }

        if (TIME_CMP_A(ge, xr_ma_stats->burst_end_time,
                       chan->nakpli_sent_time)) {
            xr_ma_stats->rtcp_req_to_burst_end =
                TIME_GET_R(msec, TIME_SUB_A_A(xr_ma_stats->burst_end_time,
                                              chan->nakpli_sent_time));
        }
        
        /* If burst_end is > 0, we are guaranteed burst start is also > 0*/
        if (xr_ma_stats->rtcp_req_to_burst_end > 0) {
            burst_data = TRUE;
        }

        break;
    }

#else /* !HAVE_FCC */

    burst_data = TRUE;
    xr_ma_stats->status = ERA_RCC_NORCC;

#endif /* HAVE_FCC */

    /* These are all related to receiving the multicast burst, and occur
       even when RCC is disabled */
     if (TIME_CMP_A(ge, xr_ma_stats->first_primary_ts,
                   xr_ma_stats->join_issue_time)) {
          xr_ma_stats->sfgmp_join_time = 
             TIME_GET_R(msec, TIME_SUB_A_A(xr_ma_stats->first_primary_ts,
                                           xr_ma_stats->join_issue_time));
      }
  
     if (TIME_CMP_A(ge, xr_ma_stats->first_primary_ts,
                     chan->bind_time)) {
          xr_ma_stats->app_req_to_mcast = 
             TIME_GET_R(msec, TIME_SUB_A_A(xr_ma_stats->first_primary_ts,
                                            chan->bind_time));
      }

    
    /* These are all related to presentation & depend on STB instrumentation */
    if (chan->dcr_ops.get_dcr_stats) {
        /* get the stats first, polling thread has already saved into 
         * chan if display_time > 0 */
        if (TIME_CMP_A(eq, chan->display_time, ABS_TIME_0)) {
            vqec_chan_update_dcr_stats(chan);
        }

        /* Don't send this packet until display_time and time are available */
        if (chan->display_pts > 0 &&
            TIME_CMP_A(gt, chan->display_time, ABS_TIME_0)) {
            pres_data = TRUE;
        }
        
        xr_ma_stats->rcc_actual_pts = chan->display_pts;

        if (TIME_CMP_A(ne, chan->ir_time, ABS_TIME_0) &&
            TIME_CMP_A(gt, chan->display_time, chan->ir_time)) {
            xr_ma_stats->total_cc_time = 
                TIME_GET_R(msec, TIME_SUB_A_A(chan->display_time,
                                              chan->ir_time));
        }

        if (TIME_CMP_A(gt, chan->display_time, chan->bind_time)) {
            xr_ma_stats->app_req_to_pres = 
                TIME_GET_R(msec, TIME_SUB_A_A(chan->display_time, 
                                              chan->bind_time));
        }

    } else {
        pres_data = TRUE;
    }

    /* Mark XR RMA as ready for sending if all data is present. Depending on
     * environment, some of these fields will never be set, so don't wait
     * for them */
    if (force ||
        (burst_data && pres_data && (xr_ma_stats->first_mcast_ext_seq > 0))) {
        xr_ma_stats->to_be_sent = TRUE;
    }
}

/**---------------------------------------------------------------------------
 * RCC support.
 *---------------------------------------------------------------------------*/ 


#ifdef HAVE_FCC

/**
 * Static buffer used for getting a PSI section (PAT/PMT) from the DP.
 */
static vqec_dp_psi_section_t s_psi;

/**---------------------------------------------------------------------------
 * Gather the PAT for a channel.
 *
 * param[in]  chanid    Channel identifier.
 * param[out] pat_buf   Pointer to the PAT buffer
 * param[out] pat_len   Length of the PAT
 * param[in]  buf_len   Length of the buffer.
 *---------------------------------------------------------------------------*/ 
vqec_chan_err_t
vqec_chan_get_pat (vqec_chanid_t chanid,
                  uint8_t *pat_buf,
                  uint32_t *pat_len,
                  uint32_t buf_len)
{
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    vqec_chan_t *chan;

    if (!pat_buf || !pat_len) {
        status = VQEC_CHAN_ERR_INVALIDARGS;
        goto done;
    }

    /* Validate state and parameters */
    if (!g_channel_module) {
        status = VQEC_CHAN_ERR_NOTINITIALIZED;
        goto done;
    }
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        status = VQEC_CHAN_ERR_NOSUCHCHAN;
        goto done;
    }

    if (VQEC_DP_ERR_OK == vqec_dp_chan_get_pat(chan->dp_chanid,
                                               &s_psi)) {
        if (s_psi.len > buf_len) {
            status = VQEC_DP_ERR_INVALIDARGS;
            *pat_len = 0;
            goto done;
        }
        memcpy(pat_buf, &s_psi.data, s_psi.len);
        *pat_len = s_psi.len;
        status = VQEC_CHAN_ERR_OK;
    } else {
        status = VQEC_CHAN_ERR_NOROOM;
    }

done:
    return (status);
}


/**---------------------------------------------------------------------------
 * Gather the PMT for a channel.
 *
 * param[in]  chanid    Channel identifier.
 * param[out] pmt_pid   PID of the PMT.
 * param[out] pmt_buf   Pointer to the PMT buffer
 * param[out] pmt_len   Length of the PMT
 * param[in]  buf_len   Length of the buffer.
 *---------------------------------------------------------------------------*/ 
vqec_chan_err_t
vqec_chan_get_pmt (vqec_chanid_t chanid,
                   uint16_t *pmt_pid,
                   uint8_t *pmt_buf,
                   uint32_t *pmt_len,
                   uint32_t buf_len)
{
    vqec_chan_err_t status = VQEC_CHAN_ERR_OK;
    vqec_chan_t *chan;

    if (!pmt_pid) {
        status = VQEC_CHAN_ERR_INVALIDARGS;
        goto done;
    }

    /* Validate state and parameters */
    if (!g_channel_module) {
        status = VQEC_CHAN_ERR_NOTINITIALIZED;
        goto done;
    }
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        status = VQEC_CHAN_ERR_NOSUCHCHAN;
        goto done;
    }

    if (VQEC_DP_ERR_OK == vqec_dp_chan_get_pmt(chan->dp_chanid,
                                               pmt_pid,
                                               &s_psi)) {
        if (s_psi.len > buf_len) {
            status = VQEC_DP_ERR_INVALIDARGS;
            *pmt_len = 0;
            goto done;
        }
        memcpy(pmt_buf, &s_psi.data, s_psi.len);
        *pmt_len = s_psi.len;
        status = VQEC_CHAN_ERR_OK;
    } else {
        status = VQEC_CHAN_ERR_NOROOM;
    }

done:
    return (status);
}

/**---------------------------------------------------------------------------
 * ER Maximum receive bandwidth transition handler.
 * 
 * @param[in] arg Pointer to the chan.
 *---------------------------------------------------------------------------*/ 
static void 
vqec_chan_mrb_transition_event_handler (const vqec_event_t *const unused_evptr,
                                        int unused_fd,
                                        short unused_event,
                                        void *arg)
{
    vqec_chan_t *chan;

    chan = (vqec_chan_t *)arg;
    VQEC_ASSERT(chan);

    if (chan->use_rcc_bw_for_er) {
        vqec_chan_mrb_transition_actions(chan, TRUE);
    }
}

/**---------------------------------------------------------------------------
 * Process a RCC APP packet.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] p_rcc_tlv Pointer to the RCC tlv's.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_process_app (vqec_chan_t *chan,
                       ppdd_tlv_t *p_rcc_tlv)
{
    vqec_dp_rcc_app_t app;
    vqec_dp_error_t dp_err;
    vqec_ifclient_fastfill_params_t param;
    vqec_ifclient_fastfill_status_t status;
    rel_time_t burst_duration;
    struct timeval tv;

/*
 * Amount of extra time (in msecs) to be sure the estimate is long enough.
 *
 * This length of time should be chosen such that it is longer than the longest
 * RCC burst durations.  Using the following parameters, the burst lengths in
 * aggressive mode can be calculated.
 *
 * IGMP Join variance = 500 ms
 * e = 10%
 * Gop Size = 2 s
 *
 * earliest RAP = 650 ms, so worst case RAP = 2650 ms
 *
 * Join Time = 25,350 ms
 * Total Length of Burst = 25,850 ms (not including error repairs)
 *
 * Add an additional RTT delay of 1 s to allow for error repairs to return.
 *
 * Total Length with Error Repair = 26,850 ms
 */
#define BURST_DURATION_FUDGE_FACTOR SECS(27)

    if (!g_channel_module || !chan || !p_rcc_tlv || chan->shutdown) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return;
    }

    chan->app_rx_ev_ts = get_sys_time();
    g_channel_module->total_app_paks++;

    if (!p_rcc_tlv->start_rtp_time && 
        !p_rcc_tlv->dt_earliest_join &&
        !p_rcc_tlv->dt_repair_end) {

        /* abort. */
        /* sa_ignore {state-machine to abort} IGNORE_RETURN(2) */
        (void)vqec_sm_deliver_event(chan,
                                    VQEC_EVENT_RECEIVE_NULL_APP, NULL);
        g_channel_module->total_null_app_paks++;
        return;
    }

    /*
     * When max_fastfill is nonzero, memory-optimized bursts are being used, and
     * therefore, the condition of actual_fill exceeding max_fill is acceptable.
     */
    if (TIME_CMP_R(eq, chan->max_fastfill, REL_TIME_0) &&
        TIME_CMP_R(lt, chan->rcc_max_fill,
                   TIME_MK_R(msec, p_rcc_tlv->act_rcc_fill))) {
        /* sa_ignore {state-machine to abort} IGNORE_RETURN(2) */
        (void)vqec_sm_deliver_event(chan,
                                    VQEC_EVENT_RECEIVE_INVALID_APP, NULL);
        g_channel_module->total_app_parse_errors++;
        syslog_print(VQEC_CHAN_ERROR,
                     vqec_chan_print_name(chan),
                     "APP exceeds maximum fill bound - RCC aborted");
        return;
    }

    /* Fill-in cached parameters. */
    chan->start_seq_num = p_rcc_tlv->start_seq_number;
    chan->app_rtp_ts = p_rcc_tlv->start_rtp_time;
    chan->dt_earliest_join = TIME_MK_R(pcr, p_rcc_tlv->dt_earliest_join);
    chan->dt_repair_end = TIME_MK_R(pcr, p_rcc_tlv->dt_repair_end);
    chan->er_holdoff_time = TIME_MK_R(msec, p_rcc_tlv->er_holdoff_time);
    chan->act_min_backfill = TIME_MK_R(msec, p_rcc_tlv->act_rcc_fill);
    chan->act_backfill_at_join = TIME_MK_R(msec, p_rcc_tlv->act_rcc_fill_at_join);
    chan->fast_fill_time = TIME_MK_R(pcr, p_rcc_tlv->actual_fastfill);

    /* start timer to transition ER e-factor just after end of burst */
    if (chan->max_recv_bw_rcc || chan->max_recv_bw_er) {
        burst_duration = TIME_ADD_R_R(chan->dt_repair_end,
                                      BURST_DURATION_FUDGE_FACTOR);
        tv = rel_time_to_timeval(burst_duration);
        if (!vqec_event_create(&chan->mrb_transition_ev,
                               VQEC_EVTYPE_TIMER,
                               VQEC_EV_ONESHOT,
                               vqec_chan_mrb_transition_event_handler,
                               VQEC_EVDESC_TIMER,
                               chan) ||
            !vqec_event_start(chan->mrb_transition_ev, &tv)) {
            syslog_print(VQEC_CHAN_ERROR,
                         vqec_chan_print_name(chan),
                         "cannot start event to transition ER max_recv_bw");
        }
    }

    /* Fill-in parameters for APP processing in DP */
    memset(&app, 0, sizeof(app));
    app.rcc_start_time = chan->rcc_start_time;
    app.start_seq_num = chan->start_seq_num;
    app.app_rtp_ts = chan->app_rtp_ts;
    app.dt_earliest_join = chan->dt_earliest_join;
    app.dt_repair_end = chan->dt_repair_end;
    app.act_min_backfill = chan->act_min_backfill;
    app.act_backfill_at_join = chan->act_backfill_at_join;
    app.first_repair_deadline = chan->first_repair_deadline;
    app.er_holdoff_time = chan->er_holdoff_time;
    app.fast_fill_time = chan->fast_fill_time;

    dp_err = vqec_dp_chan_process_app(chan->dp_chanid,
                                      &app,
                                      p_rcc_tlv->tsrap,
                                      p_rcc_tlv->tsrap_len);
    if (dp_err == VQEC_DP_ERR_OK) {
        /* Transition state-machine. */
        /* sa_ignore {failure handled in state-machine} IGNORE_RETURN(2) */
        (void)vqec_sm_deliver_event(chan,
                                    VQEC_EVENT_RECEIVE_VALID_APP, NULL);
        /**
         * alert external devices if a fastfill is starting 
         * or has been aborted.
         */
        if (chan->fast_fill_enabled && 
            chan->fastfill_ops.fastfill_start) {
            if (TIME_CMP_R(gt, chan->fast_fill_time, REL_TIME_0)) {
                memset(&param, 0, sizeof(param));
                (void)vqec_dp_chan_get_pcr(chan->dp_chanid, &param.first_pcr);
                param.fill_total_time = 
                    (uint32_t)TIME_GET_R(pcr, chan->fast_fill_time);
                (*chan->fastfill_ops.fastfill_start)(chan->context_id, &param);
            } else {
                memset(&status, 0, sizeof(status));
                status.abort_reason =  VQEC_IFCLIENT_FASTFILL_ABORT_SERVER;
                (*chan->fastfill_ops.fastfill_abort)(chan->context_id, &status);
            }
        }
    } else if (dp_err == VQEC_DP_ERR_INVALID_APP) {
        /* sa_ignore {state-machine to abort} IGNORE_RETURN(2) */
        (void)vqec_sm_deliver_event(chan,
                                    VQEC_EVENT_RECEIVE_INVALID_APP, NULL);
        g_channel_module->total_app_parse_errors++;
    } else {
        syslog_print(VQEC_CHAN_ERROR,
                     vqec_chan_print_name(chan),
                     "APP IPC to dataplane failed - RCC aborted");
        /* sa_ignore {state-machine to abort} IGNORE_RETURN(2) */
        (void)vqec_sm_deliver_event(chan,
                                    VQEC_EVENT_RCC_IPC_ERR, NULL);
    }
}


/**---------------------------------------------------------------------------
 * Failure notification of an APP packet parse - invoked from the child 
 * repair session which initially receives the APP packet.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void 
vqec_chan_app_parse_failure (vqec_chan_t *chan)
{
    if (!g_channel_module || !chan) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return;
    }
   
    /* sa_ignore {state-machine to abort} IGNORE_RETURN(2) */
    (void)vqec_sm_deliver_event(chan,
                                VQEC_EVENT_RECEIVE_INVALID_APP, NULL);
    g_channel_module->total_app_parse_errors++;
    g_channel_module->total_app_paks++;
}

#endif /* HAVE_FCC */


/**---------------------------------------------------------------------------
 * This notification is generated by the CP FSM when an event causes it to 
 * transition to the abort state. The channel takes appropriate actions in
 * resposne to the abort.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_rcc_abort_notify (vqec_chan_t *chan)
{
#ifdef HAVE_FCC
    vqec_ifclient_fastfill_status_t status;
  
    if (!g_channel_module || !chan || chan->shutdown) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return;
    }

    /* Has the control-plane already aborted? */ 
    if (!vqec_chan_is_rcc_aborted(chan)) {
        
        /* Kill the dataplane via IPC abort. */
        if (vqec_dp_chan_abort_rcc(chan->dp_chanid) != VQEC_DP_ERR_OK) {
            syslog_print(VQEC_CHAN_ERROR,
                         vqec_chan_print_name(chan),
                         "Abort IPC to dataplane failed");
        }

        /* transition the max recv bandwidth value for ER */
        if (chan->use_rcc_bw_for_er) {
            vqec_chan_mrb_transition_actions(chan, FALSE);
        }

        /* Enable ER / send BYE on repair session */
        vqec_chan_enable_er_poll(chan); 
        rtp_session_repair_recv_send_bye(chan->repair_session);

        /**
         * alert external devices if the rcc abort happens prior to 
         * fastfill being completed.
         */
        if (chan->fast_fill_enabled && 
            chan->fastfill_ops.fastfill_abort &&
            TIME_CMP_A(eq, chan->fast_fill_end_time, ABS_TIME_0)) {
            memset(&status, 0, sizeof(status));
            status.abort_reason =  VQEC_IFCLIENT_FASTFILL_ABORT_CLIENT;
            (*chan->fastfill_ops.fastfill_abort)(chan->context_id, &status);
        }

        /* Decrement the number of current active RCCs */
        g_channel_module->num_current_rccs--;

        vqec_chan_set_rcc_aborted(chan);
    }
#endif /* HAVE_FCC */
}


/**---------------------------------------------------------------------------
 * Send NCSI packet for RCC.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] seq_num The first primary sequence number seen.
 * @param[in] join_latency The join latency for the channel change.
 * @param[out] boolean TRUE if ncsci pkt successfully enqueued for 
 *                      transmission, FALSE otherwise
 *---------------------------------------------------------------------------*/ 
#if HAVE_FCC
static boolean 
vqec_chan_rcc_send_ncsi (vqec_chan_t *chan, uint32_t seq_num, 
                         rel_time_t join_latency)
{
    return rtp_era_send_ncsi(chan->prim_session,
                             seq_num,
                             join_latency);
}
#endif /* HAVE_FCC */


/**---------------------------------------------------------------------------
 * Process an NCSI upcall event received for the channel.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] ncsi_data Pointer to the NCSI data: consists of the join
 * latency, the first primary sequence, and the absolute time at which CP 
 * should enable ER.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_upcall_ncsi_event (vqec_chan_t *chan, 
                             vqec_dp_upc_ncsi_data_t *ncsi_data) 
{
#ifdef HAVE_FCC
    abs_time_t cur_time;
    rel_time_t er_en_time;
    struct timeval tv;

    if (!chan || 
        !ncsi_data || 
        chan->shutdown ||
        vqec_chan_is_rcc_aborted(chan)) {
        return;
    }

    if (chan->er_enabled) { 
        /* 
         * If ER is enabled, and the time-to-en-ER has not yet
         * arrived, setup a timer to startup ER immediately at the end
         * of the burst.
         */
        cur_time = get_sys_time();   
        if (TIME_CMP_A(gt, 
                       ncsi_data->er_enable_ts, cur_time)) {

            er_en_time = 
                TIME_SUB_A_A(ncsi_data->er_enable_ts, cur_time);
            tv = rel_time_to_timeval(er_en_time);

            if (!vqec_event_create(&chan->er_fast_timer,
                                   VQEC_EVTYPE_TIMER,
                                   VQEC_EV_ONESHOT,
                                   vqec_chan_en_er_fastpoll,
                                   VQEC_EVDESC_TIMER,
                                   (void *)chan->chanid) ||
                !vqec_event_start(chan->er_fast_timer, &tv)) {
                        
                syslog_print(VQEC_CHAN_ERROR,
                             vqec_chan_print_name(chan),
                             "Failed to add fast timer to libevent");
                vqec_event_destroy(&chan->er_fast_timer);
            }
        } else {
            /* 
             * Time to enable ER is now; Invoke gap-reporter right away
             * so the first report can be sent immediately.
             */
            vqec_chan_en_er_fastpoll(NULL, 0, 0, (void *)chan->chanid);
        }
    }
    
    /* Send NCSI message. */
    if (!vqec_chan_rcc_send_ncsi(chan, 
                                 ncsi_data->first_primary_seq, 
                                 ncsi_data->join_latency)) {
        syslog_print(VQEC_CHAN_ERROR,
                     vqec_chan_print_name(chan),
                     "Tx of NCSI message failed");
    }

    /* store the first primary sequence number in the channel structure */
    chan->first_primary_seq = ncsi_data->first_primary_seq;
    chan->first_primary_valid = TRUE;
    
    VQEC_DEBUG(VQEC_DEBUG_RCC,
               "Channel %s, enabled ER / sent NCSI\n",
               vqec_chan_print_name(chan));

#endif /* HAVE_FCC */
}


/**---------------------------------------------------------------------------
 * Process a RCC abort upcall event received for the channel.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_upcall_abort_event (vqec_chan_t *chan) 
{
#ifdef HAVE_FCC
    if (!chan || 
        chan->shutdown) {
        return;
    }

    /* transition the max recv bandwidth value for ER */
    if (chan->use_rcc_bw_for_er) {
        vqec_chan_mrb_transition_actions(chan, FALSE);
    }

    /* setup ER timer, and send BYE on repair session. */ 
    vqec_chan_enable_er_poll(chan);
    rtp_session_repair_recv_send_bye(chan->repair_session);
    VQEC_DEBUG(VQEC_DEBUG_RCC,
               "Channel %s, dataplane asserted RCC abort\n",
               vqec_chan_print_name(chan));

#endif /* HAVE_FCC */
} 


/**---------------------------------------------------------------------------
 * Process a Fast fill finish upcall event received for the channel.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_upcall_fast_fill_event (vqec_chan_t *chan, 
                                  vqec_dp_upc_fast_fill_data_t *fast_fill_data)
{
#ifdef HAVE_FCC
    vqec_ifclient_fastfill_status_t status;
 
    if (!chan || chan->shutdown || !fast_fill_data || 
        !chan->fast_fill_enabled) {
        return;
    }

    chan->total_fast_fill_bytes = fast_fill_data->total_fast_fill_bytes;
    chan->fast_fill_start_time = fast_fill_data->fast_fill_start_time;
    chan->fast_fill_end_time = fast_fill_data->fast_fill_end_time;

    /* alert external devices that fastfill is complete. */
    if (chan->fast_fill_enabled && 
        chan->fastfill_ops.fastfill_done) {
        memset(&status, 0, sizeof(status));
        status.bytes_filled = chan->total_fast_fill_bytes;
        status.elapsed_time = 
            TIME_GET_R(pcr, 
                       TIME_SUB_A_A(chan->fast_fill_end_time, 
                                    chan->fast_fill_start_time));
        (*chan->fastfill_ops.fastfill_done)(chan->context_id, &status);

        if (VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC)) {
            CONSOLE_PRINTF("[FASTFILL]: actual fast fill time = %llu ms\n", 
                           TIME_GET_R(msec,
                                      TIME_SUB_A_A(chan->fast_fill_end_time, 
                                              chan->fast_fill_start_time)));
        }
    }     

#endif /* HAVE_FCC */
} 

/**---------------------------------------------------------------------------
 * Process a RCC burst done upcall event received for the channel.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_upcall_burst_done_event (vqec_chan_t *chan) 
{
#ifdef HAVE_FCC
    if (!chan || 
        chan->shutdown) {
        return;
    }

    /* transition the max recv bandwidth value for ER */
    if (chan->use_rcc_bw_for_er) {
        vqec_chan_mrb_transition_actions(chan, FALSE);
    }
    
    VQEC_DEBUG(VQEC_DEBUG_RCC,
               "Channel %s, dataplane signaled RCC burst done.\n",
               vqec_chan_print_name(chan));

#endif /* HAVE_FCC */
} 


/**---------------------------------------------------------------------------
 * Process a primary inactive upcall event received for the channel.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_upcall_prim_inactive_event (vqec_chan_t *chan) 
{
    vqec_ifclient_chan_event_args_t cb_args;

    if (!chan || 
        chan->shutdown) {
        return;
    }
    if (chan->chan_event_cb.chan_event_cb) {
        cb_args.event = VQEC_IFCLIENT_CHAN_EVENT_INACTIVE;
        (*chan->chan_event_cb.chan_event_cb)(
            chan->chan_event_cb.chan_event_context,
            &cb_args);
    }
    
    VQEC_DEBUG(VQEC_DEBUG_VOD,
               "Channel %s, dataplane signaled primary inactive.\n",
               vqec_chan_print_name(chan));

} 


/**---------------------------------------------------------------------------
 * Process a FEC update upcall event received for the channel.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_chan_upcall_fec_update_event (vqec_chan_t *chan, 
                                   vqec_dp_upc_fec_update_data_t *fec_data)
{
    if (!chan || chan->shutdown || !chan->fec_enabled) {
        return;
    }

    /* copy over the new FEC values */
    chan->cfg.fec_l_value = fec_data->fec_l_value;
    chan->cfg.fec_d_value = fec_data->fec_d_value;

    /* update the FEC receive bandwidth value for this channel */
    vqec_chan_update_fec_bw(chan);
} 

/**---------------------------------------------------------------------------
 * Print the name of the channel in a static buffer: to-be-used only for
 * debug purposes: do-not-cache.
 * 
 * @param[in] chan Pointer to the channel.
 * @param[out] char* Pointer to a formatted string for the channel name. 
 *---------------------------------------------------------------------------*/ 
char *
vqec_chan_print_name (vqec_chan_t *chan)
{
    static char name_buf[VQEC_MAX_CHANNEL_IPV4_PRINT_NAME_SIZE+1];
    char tmp[INET_ADDRSTRLEN];

    /*sa_ignore {result ignored} IGNORE_RETURN (5) */
    snprintf(name_buf, sizeof(name_buf),
             "%s:%d",
             inet_ntop(AF_INET, 
                       &chan->cfg.primary_dest_addr, tmp, INET_ADDRSTRLEN),
             ntohs(chan->cfg.primary_dest_port));
    
    return (name_buf);
}


/**---------------------------------------------------------------------------
 * From channel to it's repair session.
 * 
 * @param[in] chan Pointer to the channel.
 * @param[out] rtp_repair_recv_t* Pointer to the repair session. 
 *---------------------------------------------------------------------------*/ 
rtp_repair_recv_t *
vqec_chan_get_repair_session (vqec_chan_t *chan)
{
    if (chan && !chan->shutdown) {
        return (chan->repair_session);
    } 
    
    return (NULL);
}


/**---------------------------------------------------------------------------
 * From channel to it's primary session.
 * 
 * @param[in] chan Pointer to the channel.
 * @param[out] rtp_era_recv_t* Pointer to the primary session. 
 *---------------------------------------------------------------------------*/ 
rtp_era_recv_t *
vqec_chan_get_primary_session (vqec_chan_t *chan)
{
    if (chan && !chan->shutdown) {
        return (chan->prim_session);
    }

    return (NULL);
}

/**
 * Retrieve a channel's input shim stream identifiers.
 *
 * @param[in]  chanid          - specifies channel of interest
 * @param[out] prim_rtp_id     - primary rtp stream ID
 * @param[out] repair_rtp_id   - repair rtp stream ID
 * @param[out] fec0_rtp_id     - fec0 rtp stream ID
 * @param[out] fec1_rtp_id     - fec1 rtp stream ID
 * @param[out] vqec_chan_err_t - VQEC_CHAN_ERR_OK:         success
 *                               VQEC_CHAN_ERR_NOSUCHCHAN: channel not found
 */
vqec_chan_err_t
vqec_chan_get_inputshim_streams (vqec_chanid_t chanid,
                                 vqec_dp_streamid_t *prim_rtp_id,
                                 vqec_dp_streamid_t *repair_rtp_id,
                                 vqec_dp_streamid_t *fec0_rtp_id,
                                 vqec_dp_streamid_t *fec1_rtp_id)
{
    vqec_chan_t *chan;

    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        return (VQEC_CHAN_ERR_NOSUCHCHAN);
    }

    if (prim_rtp_id) {
        *prim_rtp_id = chan->inputshim.prim_rtp_id;
    }
    if (repair_rtp_id) {
        *repair_rtp_id = chan->inputshim.repair_rtp_id;
    }
    if (fec0_rtp_id) {
        *fec0_rtp_id = chan->inputshim.fec0_rtp_id;
    }
    if (fec1_rtp_id) {
        *fec1_rtp_id = chan->inputshim.fec1_rtp_id;
    }

    return (VQEC_CHAN_ERR_OK);
}

/*
 * Retrieve a channel's output shim stream identifier.
 *
 * @param[in]  chanid          - specifies channel of interest
 * @param[out] postrepair_id   - post-repair stream ID
 * @param[out] vqec_chan_err_t - VQEC_CHAN_ERR_OK:         success
 *                               VQEC_CHAN_ERR_NOSUCHCHAN: channel not found
 */
vqec_chan_err_t
vqec_chan_get_outputshim_stream (vqec_chanid_t chanid,
                                 vqec_dp_streamid_t *postrepair_id)
{
    vqec_chan_t *chan;

    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        return (VQEC_CHAN_ERR_NOSUCHCHAN);
    }
    
    if (postrepair_id) {
        *postrepair_id = chan->outputshim.postrepair_id;
    }
    return (VQEC_CHAN_ERR_OK);
}


/**---------------------------------------------------------------------------
 * Has fastfill been requested for this channel from the server. 
 *
 * @param[in] chanid Identifier of the channel.
 * @param[out] uint8_t Returns TRUE if fastfill is enabled, FALSE otherwise.
 *---------------------------------------------------------------------------*/ 
uint8_t 
vqec_chan_is_fastfill_enabled (vqec_chanid_t chanid)
{
    vqec_chan_t *chan;

    /* Validate state and parameters */
    if (!g_channel_module ||
        !(chan = vqec_chanid_to_chan(chanid))) {
        return (FALSE);
    }

    return (chan->fast_fill_enabled);
}

/**---------------------------------------------------------------------------
 * Is RCC enabled for this channel? 
 *
 * @param[in] chanid Identifier of the channel.
 * @param[out] uint8_t Returns TRUE if rcc is enabled, FALSE otherwise.
 *---------------------------------------------------------------------------*/ 
uint8_t 
vqec_chan_is_rcc_enabled (vqec_chanid_t chanid)
{
    vqec_chan_t *chan;

    /* Validate state and parameters */
    if (!g_channel_module ||
        !(chan = vqec_chanid_to_chan(chanid))) {
        return (FALSE);
    }

    return (chan->rcc_enabled);
}

/**
 * Get the maximum receive bandwidth value (for primary stream and VQE
 * services) to send up to the VQE-S for the RCC case.  This number will
 * already have the FEC bandwidth subtracted out from it, if necessary.
 *
 * @param[in] chan Pointer of the channel.
 * @param[out] uint32_t Returns the bandwidth value to be sent to the server.
 */
uint32_t vqec_chan_get_max_recv_bw_rcc (vqec_chan_t *chan)
{
    /*
     * If the multicast addresses for primary and FEC streams are different,
     * then this is the "late join case," in which the FEC stream is joined
     * after the RCC is done, and therefore we don't need to subtract out the
     * bandwidth of the FEC stream.
     */
    if ((chan->cfg.primary_dest_addr.s_addr !=
         chan->cfg.fec1_mcast_addr.s_addr) &&
        (chan->cfg.primary_dest_addr.s_addr !=
         chan->cfg.fec2_mcast_addr.s_addr)) {
        /* late join case */
        return (chan->max_recv_bw_rcc);
    } else {
        if (chan->max_recv_bw_rcc > chan->fec_recv_bw) {
            return (chan->max_recv_bw_rcc - chan->fec_recv_bw);
        } else {
            return (0);
        }
    }
}

/**
 * Get the maximum receive bandwidth value (for primary stream and VQE
 * services) to send up to the VQE-S for the ER case.  This number will
 * already have the FEC bandwidth subtracted out from it, if necessary.
 *
 * @param[in] chan Pointer of the channel.
 * @param[out] uint32_t Returns the bandwidth value to be sent to the server.
 */
uint32_t vqec_chan_get_max_recv_bw_er (vqec_chan_t *chan)
{
    /* always subtract out the FEC bandwidth in this case */
    if (chan->max_recv_bw_er > chan->fec_recv_bw) {
        return (chan->max_recv_bw_er - chan->fec_recv_bw);
    } else {
        return (0);
    }
}
