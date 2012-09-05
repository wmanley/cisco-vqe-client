/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: VQE-C Dataplane RCC state-machine.
 *
 * Documents:
 *
 *****************************************************************************/

#if HAVE_FCC

#include <utils/vam_types.h>
#include <utils/vam_time.h>
#include "vqec_dp_sm.h"
#include "vqec_dpchan.h"

#ifdef _VQEC_DP_UTEST
#define UT_STATIC
#else
#define UT_STATIC static
#endif

typedef vqec_dp_sm_state_t (*vqec_dp_sm_state_transition_fcn_t)(
    vqec_dp_sm_event_t event);
typedef boolean (*vqec_dp_sm_state_action_fcn_t)(vqec_dpchan_t *chan,
                                                 vqec_dp_sm_state_t state,
                                                 vqec_dp_sm_event_t event,
                                                 void *arg);
typedef vqec_dp_sm_state_action_fcn_t (*vqec_dp_sm_state_map_action_t)(
    vqec_dp_sm_event_t event);

typedef
struct vqec_dp_sm_state_fcns_t_
{
    /**
     * Entry action handler.
     */
    vqec_dp_sm_state_action_fcn_t entry_fcn;
    const char *entry_fcn_name;
    /**
     * Exit action handler.
     */
    vqec_dp_sm_state_action_fcn_t exit_fcn;
    const char *exit_fcn_name;
    /**
     * Per-event action function map handler.
     */
    vqec_dp_sm_state_map_action_t action_map_fcn;
    /**
     * Transition function.
     */
    vqec_dp_sm_state_transition_fcn_t transition_fcn;

} vqec_dp_sm_state_fcns_t;


/**---------------------------------------------------------------------------
 * From event type to character representation.
 *
 * @param[in] e Event type.
 * @param[out] char* Pointer to a character representation for the event.
 *---------------------------------------------------------------------------*/ 
const char * 
vqec_dp_sm_event_to_string (vqec_dp_sm_event_t e)
{
    switch(e) {        
    
    case VQEC_DP_SM_EVENT_START_RCC:
        return "PROCEED_RCC";
    case VQEC_DP_SM_EVENT_INTERNAL_ERR:
        return "DP_INTERNAL_ERR";
    case VQEC_DP_SM_EVENT_ABORT:
        return "CP_ABORT";
    case VQEC_DP_SM_EVENT_TIME_TO_JOIN:
        return "TIME_TO_JOIN";
    case VQEC_DP_SM_EVENT_TIME_TO_EN_ER:
        return "TIME_TO_EN_ER";
    case VQEC_DP_SM_EVENT_TIME_END_BURST:
        return "END_OF_BURST";
    case VQEC_DP_SM_EVENT_REPAIR:
        return "RX_REPAIR";
    case VQEC_DP_SM_EVENT_PRIMARY:
        return "RX_PRIMARY";
    case VQEC_DP_SM_EVENT_ACTIVITY_TIMEOUT:
        return "ACT_TIMEOUT";
    case VQEC_DP_SM_EVENT_TIME_FIRST_SEQ:
        return "1st_SEQ_TIMEOUT";
    default:
        return "ERR";
    }
}


/**---------------------------------------------------------------------------
 * From state to character representation.
 *
 * @param[in] state State.
 * @param[out] char* Pointer to a character representation for the state.
 *---------------------------------------------------------------------------*/ 
const char * 
vqec_dp_sm_state_to_string (vqec_dp_sm_state_t state)
{
    switch (state) {
    case VQEC_DP_SM_STATE_INVALID:
        return "STATE_INVALID";
    case VQEC_DP_SM_STATE_INIT:
        return "STATE_INIT";
    case VQEC_DP_SM_STATE_WAIT_FIRST_SEQ:
        return "STATE_WAIT_FIRSTSEQ";
    case VQEC_DP_SM_STATE_WAIT_JOIN:
        return "STATE_WAIT_JOIN";
    case VQEC_DP_SM_STATE_WAIT_EN_ER:
        return "STATE_WAIT_EN_ER";
    case VQEC_DP_SM_STATE_WAIT_END_BURST:
        return "STATE_WAIT_ENDBURST";
    case VQEC_DP_SM_STATE_ABORT:
        return "STATE_ABORT";
    case VQEC_DP_SM_STATE_FIN_SUCCESS:
        return "STATE_SUCCESS";
    default:
        return "ERR";
    }
}


enum
{
    VQEC_DP_SM_LOG_STATE_EVENT,
    VQEC_DP_SM_LOG_STATE_ENTER,
    VQEC_DP_SM_LOG_STATE_EXIT,
    VQEC_DP_SM_LOG_FIRST_REPAIR,
    VQEC_DP_SM_LOG_ISSUED_JOIN,
    VQEC_DP_SM_LOG_FIRST_PRIMARY,
    VQEC_DP_SM_LOG_ER_EN,
    VQEC_DP_SM_LOG_NUM_VAL
};

#define SM_LOG_MIN_EVENT VQEC_DP_SM_LOG_STATE_EVENT
#define SM_LOG_MAX_EVENT VQEC_DP_SM_LOG_NUM_VAL - 1

const char *
vqec_dp_sm_log_evt_to_string (uint32_t evt)
{
    switch (evt) {
    case VQEC_DP_SM_LOG_STATE_EVENT:
        return "INPUT_EVENT";
    case VQEC_DP_SM_LOG_STATE_ENTER:
        return "STATE_ENTER";
    case VQEC_DP_SM_LOG_STATE_EXIT:
        return "STATE_EXIT";
    case VQEC_DP_SM_LOG_FIRST_REPAIR:
        return "FIRST_REPAIR";
    case VQEC_DP_SM_LOG_ISSUED_JOIN:
        return "ISSUED_JOIN";
    case VQEC_DP_SM_LOG_FIRST_PRIMARY:
        return "FIRST_PRIMARY";
    case VQEC_DP_SM_LOG_ER_EN:
        return "ENABLED_ER";
    default:
        return "ERR";
    }
}


/**---------------------------------------------------------------------------
 * Log of events / state transitions.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dp_sm_log (uint32_t log_evt, vqec_dpchan_t *chan, vqec_dp_sm_state_t state,
                vqec_dp_sm_event_t event)
{
    vqec_dp_sm_log_t *log;

    if (log_evt < SM_LOG_MIN_EVENT || log_evt > SM_LOG_MAX_EVENT) {
        return;
    }
    
    log = &chan->sm_log[chan->sm_log_tail];
    chan->sm_log_tail = (chan->sm_log_tail + 1) % VQEC_DP_SM_LOG_MAX_SIZE;
    
    log->state = state;
    log->event = event;
    log->log_event = log_evt;
    log->timestamp = get_sys_time();
}


/**---------------------------------------------------------------------------
 * Copy log for cp-dp API.
 *
 * @param[in] chan Pointer to the channel; must be null-checked prior
 * to invocation of this method.
 * @param[out] dplog Output log buffer - must be null-checked prior
 * to invocation.
 *---------------------------------------------------------------------------*/ 
void
vqec_dp_sm_copy_log (vqec_dpchan_t *chan, vqec_dp_rcc_fsm_log_t *dplog)
{
    uint32_t i, j;
    vqec_dp_sm_log_t *log;
    
    for (i = chan->sm_log_head, j = 0; 
         (i != chan->sm_log_tail) && (j < VQEC_DP_RCC_FSMLOG_ARRAY_SIZE);
         i = (i + 1) % VQEC_DP_SM_LOG_MAX_SIZE, j++) {

        log = &chan->sm_log[i];

        dplog->logentry[j].log_event = log->log_event;
        dplog->logentry[j].state = log->state;
        dplog->logentry[j].event = log->event;
        dplog->logentry[j].timestamp = log->timestamp;
    }

    dplog->num_entries = j;
}


/**---------------------------------------------------------------------------
 * Empty enter() handler.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_enter_def (vqec_dpchan_t *chan, 
                      vqec_dp_sm_state_t state, 
                      vqec_dp_sm_event_t event, void *arg) 
{
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Empty exit() handler.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_exit_def (vqec_dpchan_t *chan, 
                     vqec_dp_sm_state_t state, 
                     vqec_dp_sm_event_t event, void *arg) 
{
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Null action handler.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_null_action (vqec_dpchan_t *chan, 
                        vqec_dp_sm_state_t state, 
                        vqec_dp_sm_event_t event, void *arg) 
{
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Action routine(s) to stop / destroy the:
 * 
 *  -- wait-first-sequence timer.
 *  -- join timer.
 *  -- ER timer.
 *  -- data activity timer.
 *  -- end-of-burst timer.
 *
 * @param[in] chan Pointer to the channel
 * @param[in] timer Double indirection pointer to the timer which is to 
 * be destroyed.
 * @param[in] fcn_name The function name that invoked this method
 * (for debug).
 *---------------------------------------------------------------------------*/ 
UT_STATIC inline void
vqec_dp_sm_timer_stop (vqec_dpchan_t *chan, vqec_event_t **timer, 
                       const char *fcn_name)
{
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC, 
                  "Channel %s: %s\n",
                  vqec_dpchan_print_name(chan), fcn_name);

    if (*timer) {
        vqec_event_destroy(timer); 
    }
}
    
/**---------------------------------------------------------------------------
 * state, event and arg are unused in all of the methods below.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean 
vqec_dp_sm_waitfirst_timer_stop (vqec_dpchan_t *chan,
                                 vqec_dp_sm_state_t state, 
                                 vqec_dp_sm_event_t event, void *arg)
{
    vqec_dp_sm_timer_stop(chan, &chan->wait_first_timer, __FUNCTION__);
    return (TRUE);
}

UT_STATIC boolean 
vqec_dp_sm_join_timer_stop (vqec_dpchan_t *chan,
                            vqec_dp_sm_state_t state, 
                            vqec_dp_sm_event_t event, void *arg)
{
    vqec_dp_sm_timer_stop(chan, &chan->join_timer, __FUNCTION__);
    return (TRUE);
}
        
UT_STATIC boolean 
vqec_dp_sm_er_timer_stop (vqec_dpchan_t *chan,
                          vqec_dp_sm_state_t state, 
                          vqec_dp_sm_event_t event, void *arg)
{
    vqec_dp_sm_timer_stop(chan, &chan->er_timer, __FUNCTION__);
    return (TRUE);
}

UT_STATIC boolean 
vqec_dp_sm_waitburst_timer_stop (vqec_dpchan_t *chan,
                                 vqec_dp_sm_state_t state, 
                                 vqec_dp_sm_event_t event, void *arg)
{
    vqec_dp_sm_timer_stop(chan, &chan->burst_end_timer, __FUNCTION__);
    return (TRUE);
}

UT_STATIC boolean 
vqec_dp_sm_activity_timer_stop (vqec_dpchan_t *chan,
                                vqec_dp_sm_state_t state, 
                                vqec_dp_sm_event_t event, void *arg)
{
    vqec_dp_sm_timer_stop(chan, &chan->activity_timer, __FUNCTION__);
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * WAIT_FIRST timer expiration handler.
 * 
 * @param[in] arg Untyped pointer to the channel which owns the event.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void 
vqec_dp_sm_waitfirst_timeout_handler (const vqec_event_t * const evptr,
                                      int fd, short event, void *arg) 
{
    vqec_dpchan_t *chan = (vqec_dpchan_t *)arg;
    VQEC_DP_ASSERT_FATAL(chan, "%s", __FUNCTION__);

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC, 
               "Channel %s, [%s]\n",
               vqec_dpchan_print_name(chan), __FUNCTION__); 

    /* sa_ignore {faults are internally handled} IGNORE_RETURN(2) */
    (void)vqec_dp_sm_deliver_event(chan, 
                                   VQEC_DP_SM_EVENT_TIME_FIRST_SEQ, NULL);
} 


/**---------------------------------------------------------------------------
 * WAIT_JOIN timer expiration handler.
 *
 * @param[in] arg Untyped pointer to the channel which owns the event.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void 
vqec_dp_sm_join_timeout_handler (const vqec_event_t * const evptr,
                                 int fd, short event, void *arg) 
{
    vqec_dpchan_t *chan = (vqec_dpchan_t *)arg;
    VQEC_DP_ASSERT_FATAL(chan, "%s", __FUNCTION__);
    
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC, 
                  "Channel %s, [%s]\n",
                  vqec_dpchan_print_name(chan), __FUNCTION__);

    /* sa_ignore {faults are internally handled} IGNORE_RETURN(2) */
    (void)vqec_dp_sm_deliver_event(chan, 
                                   VQEC_DP_SM_EVENT_TIME_TO_JOIN, NULL);
} 


/**---------------------------------------------------------------------------
 * WAIT_EN_ER timer expiration handler.
 *
 * @param[in] arg Untyped pointer to the channel which owns the event.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void 
vqec_dp_sm_er_timeout_handler (const vqec_event_t * const evptr,
                               int fd, short event, void *arg) 
{
    vqec_dpchan_t *chan = (vqec_dpchan_t *)arg;
    VQEC_DP_ASSERT_FATAL(chan, "%s", __FUNCTION__);
    
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC, 
                  "Channel %s, [%s]\n",
                  vqec_dpchan_print_name(chan), __FUNCTION__);

    /* sa_ignore {faults are internally handled} IGNORE_RETURN(2) */
    (void)vqec_dp_sm_deliver_event(chan, 
                                   VQEC_DP_SM_EVENT_TIME_TO_EN_ER, NULL);
} 


/**---------------------------------------------------------------------------
 * END_BURST timer expiration handler.
 *
 * @param[in] arg Untyped pointer to the channel which owns the event.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void 
vqec_dp_sm_waitburst_timeout_handler (const vqec_event_t * const evptr,
                                      int fd, short event, void *arg) 
{
    vqec_dpchan_t *chan = (vqec_dpchan_t *)arg;
    VQEC_DP_ASSERT_FATAL(chan, "%s", __FUNCTION__);
    
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC, 
                  "Channel %s, [%s]\n",
                  vqec_dpchan_print_name(chan), __FUNCTION__);

    chan->burst_end_time = get_sys_time();
    /* sa_ignore {faults are internally handled} IGNORE_RETURN(2) */
    (void)vqec_dp_sm_deliver_event(chan, 
                                   VQEC_DP_SM_EVENT_TIME_END_BURST, NULL);
} 


/**---------------------------------------------------------------------------
 * Data activity timer expiration handler.
 *
 * @param[in] arg Untyped pointer to the channel which owns the event.
 *---------------------------------------------------------------------------*/ 
#define VQEC_DP_REPAIR_ACTIVITY_TIMEOUT MSECS(200)
#define VQEC_DP_REPAIR_ACTIVITY_POLL_INTERVAL MSECS(50)

void
vqec_dp_sm_activity_timer_handler (const vqec_event_t * const evptr,
                                   int fd, short event, void *arg) 
{
    abs_time_t cur_time;
    vqec_dpchan_t *chan;

    chan = (vqec_dpchan_t *)arg;
    VQEC_DP_ASSERT_FATAL(chan, "%s", __FUNCTION__);
    cur_time = get_sys_time();

    if (TIME_CMP_R(gt, 
                   TIME_SUB_A_A(cur_time, chan->last_repair_pak_ts),
                   VQEC_DP_REPAIR_ACTIVITY_TIMEOUT)) {

        VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                      "Channel %s, repair activity timer has timed-out\n",
                      vqec_dpchan_print_name(chan));

        /* sa_ignore {faults are internally handled} IGNORE_RETURN(2) */
        (void)vqec_dp_sm_deliver_event(chan, 
                                       VQEC_DP_SM_EVENT_ACTIVITY_TIMEOUT, NULL);
    }
}


/**---------------------------------------------------------------------------
 * Action routine(s) to startup the:
 * 
 *  -- wait-first-sequence timer.
 *  -- join timer.
 *  -- ER timer.
 *  -- data activity timer.
 *  -- end-of-burst timer.
 *
 * @param[in] chan Pointer to the channel
 * @param[in] event Double indirection pointer to the timer event.
 * be destroyed.
 * @param[in] handler Handler which will be called when the timer expires.
 * @param[in] timeout Timeout period.
 * @param[in] fcn_name The function name that invoked this method
 * (for debug).
 * @param[out] boolean Returns true if there are no errors during
 * the installation of the timer.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean 
vqec_dp_sm_timer_start (vqec_dpchan_t *chan, 
                        vqec_event_t **event, 
                        void (handler)(const vqec_event_t * const, int32_t, 
                                       int16_t, void *),
                        rel_time_t timeout,                         
                        const char *fcn_name) 
{    
    struct timeval tv;

    tv = rel_time_to_timeval(timeout);

    VQEC_DP_ASSERT_FATAL(!(*event), "%s", __FUNCTION__);
    if (!vqec_event_create(event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_ONESHOT,
                           handler, 
                           VQEC_EVDESC_TIMER,
                           chan) ||
        !vqec_event_start(*event, &tv)) {
        vqec_event_destroy(event);

        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             fcn_name, 
                             "event creation"); 

        /* sa_ignore {faults are internally handled} IGNORE_RETURN(2) */
        (void)vqec_dp_sm_deliver_event(chan, 
                                       VQEC_DP_SM_EVENT_INTERNAL_ERR, NULL);
        return (FALSE);
    }  

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                  "Channel %s, [%s] started timer timeout %llu\n",
                  vqec_dpchan_print_name(chan), 
                  fcn_name, TIME_GET_R(usec, timeout));

    return (TRUE);
}

/**---------------------------------------------------------------------------
 * First repair packet timer startup.
 *
 * @param[in] chan Pointer to the channel. 
 * All other arguments are unused.
 * @param[out] boolean Returns true if there are no errors during
 * the installation of the timer.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_waitfirst_timer_start (vqec_dpchan_t *chan,
                                  vqec_dp_sm_state_t state, 
                                  vqec_dp_sm_event_t event, void *arg)
    
{    
    abs_time_t cur_time;
    rel_time_t timeout;

    cur_time = get_sys_time();

    if (TIME_CMP_A(gt, chan->first_repair_deadline, cur_time)) {
        timeout = TIME_SUB_A_A(chan->first_repair_deadline, 
                               cur_time);
    } else {
        timeout = REL_TIME_0;
    }

    return
        vqec_dp_sm_timer_start(chan, 
                               &chan->wait_first_timer,
                               vqec_dp_sm_waitfirst_timeout_handler,
                               timeout,
                               __FUNCTION__);

}

/**---------------------------------------------------------------------------
 * Join timer startup.
 *
 * @param[in] chan Pointer to the channel. 
 * All other arguments are unused.
 * @param[out] boolean Returns true if there are no errors during
 * the installation of the timer.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_join_timer_start (vqec_dpchan_t *chan,
                             vqec_dp_sm_state_t state, 
                             vqec_dp_sm_event_t event, void *arg)
    
{    
    abs_time_t cur_time, join_time;
    rel_time_t timeout;

    cur_time = get_sys_time();
    join_time = TIME_ADD_A_R(chan->first_repair_ts, 
                             chan->dt_earliest_join);
    
    if (TIME_CMP_A(gt, join_time, cur_time)) {
        timeout = TIME_SUB_A_A(join_time,
                              cur_time);
    } else {
        timeout = REL_TIME_0;
    }

    return 
        vqec_dp_sm_timer_start(chan, 
                               &chan->join_timer,
                               vqec_dp_sm_join_timeout_handler,
                               timeout,
                               __FUNCTION__);

}

/**---------------------------------------------------------------------------
 * ER enable timer startup.
 *
 * @param[in] chan Pointer to the channel. 
 * All other arguments are unused.
 * @param[out] boolean Returns true if there are no errors during
 * the installation of the timer.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_er_timer_start (vqec_dpchan_t *chan,
                           vqec_dp_sm_state_t state, 
                           vqec_dp_sm_event_t event, void *arg)
    
{    
    abs_time_t cur_time, er_en_time;
    rel_time_t timeout;

    cur_time = get_sys_time();

    er_en_time = TIME_ADD_A_R(chan->first_repair_ts, 
                              chan->dt_earliest_join);
    er_en_time = TIME_ADD_A_R(er_en_time, chan->er_holdoff_time);

    if (TIME_CMP_A(gt, er_en_time, cur_time)) {
        timeout = TIME_SUB_A_A(er_en_time,
                               cur_time);
    } else {
        timeout = REL_TIME_0;
    }

    return
        vqec_dp_sm_timer_start(chan, 
                               &chan->er_timer,
                               vqec_dp_sm_er_timeout_handler,  
                               timeout,
                               __FUNCTION__);

}

/**---------------------------------------------------------------------------
 * End-of-burst wait timer startup.
 *
 * @param[in] chan Pointer to the channel. 
 * All other arguments are unused.
 * @param[out] boolean Returns true if there are no errors during
 * the installation of the timer.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_waitburst_timer_start (vqec_dpchan_t *chan,
                                  vqec_dp_sm_state_t state, 
                                  vqec_dp_sm_event_t event, void *arg)    
{    
    abs_time_t cur_time, waitburst_time;
    rel_time_t timeout;
   
    cur_time = get_sys_time();
    waitburst_time = TIME_ADD_A_R(chan->first_repair_ts, 
                                  chan->dt_repair_end);

    if (TIME_CMP_A(gt, waitburst_time, cur_time)) {
        timeout = TIME_SUB_A_A(waitburst_time,
                               cur_time);
    } else {
        chan->burst_end_time = get_sys_time();
        timeout = REL_TIME_0;
    }

    return 
        vqec_dp_sm_timer_start(chan, 
                               &chan->burst_end_timer,
                               vqec_dp_sm_waitburst_timeout_handler, 
                               timeout,
                               __FUNCTION__);

}


/**---------------------------------------------------------------------------
 * First repair packet is received. Actions include:
 *  -- start a data activity timeout timer, to detect if packets are being
 *  received on the repair session.
 *  -- push the previously cached app packet into Pcm. 
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_first_repair_actions (vqec_dpchan_t *chan,
                                 vqec_dp_sm_state_t state, 
                                 vqec_dp_sm_event_t event, void *arg)
    
{    
    struct timeval tv;
    uint32_t inserts;
    vqec_pak_t * pak_p, *pak_p_next;
    boolean failed_insert = FALSE;

    vqec_dp_sm_log(VQEC_DP_SM_LOG_FIRST_REPAIR, chan, state, event);

    if (VQE_TAILQ_EMPTY(&chan->app_paks)) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             __FUNCTION__, 
                             "No APP packet"); 
        /* sa_ignore {faults are internally handled} IGNORE_RETURN(2) */
        (void)vqec_dp_sm_deliver_event(chan, 
                                       VQEC_DP_SM_EVENT_INTERNAL_ERR, NULL);
        return (FALSE);        
    }

    /* insert APP packets into pcm */
    VQE_TAILQ_FOREACH_SAFE(pak_p, &chan->app_paks, ts_pkts, pak_p_next) {

        VQE_TAILQ_REMOVE(&chan->app_paks, pak_p, ts_pkts);

        if (failed_insert == FALSE) {
            /* insert one (1) packet at a time */

            VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                          "insert one APP to PCM"
                          "seq # = %u, @ time = %llu ms\n",
                          pak_p->seq_num & 0xFFFF, 
                          TIME_GET_A(msec, get_sys_time()));
            inserts = 
                vqec_pcm_insert_packets(&chan->pcm, &pak_p, 1, TRUE, NULL);
            if (inserts == 0) {
                failed_insert = TRUE;
            }
        }

        vqec_pak_free(pak_p);
    }

    if (failed_insert == TRUE) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             __FUNCTION__, 
                             "APP insertion into pcm failed"); 
        /* sa_ignore {faults are internally handled} IGNORE_RETURN(2) */
        (void)vqec_dp_sm_deliver_event(chan, 
                                       VQEC_DP_SM_EVENT_INTERNAL_ERR, NULL);
        return (FALSE); 
    }

    tv = rel_time_to_timeval(VQEC_DP_REPAIR_ACTIVITY_POLL_INTERVAL);
    if (!vqec_event_create(&chan->activity_timer, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_RECURRING,
                           vqec_dp_sm_activity_timer_handler, 
                           VQEC_EVDESC_TIMER,
                           chan) ||
        !vqec_event_start(chan->activity_timer, &tv)) {

        vqec_event_destroy(&chan->activity_timer);        
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             __FUNCTION__, 
                             "event creation"); 
        /* sa_ignore {faults are internally handled} IGNORE_RETURN(2) */
        (void)vqec_dp_sm_deliver_event(chan, 
                                       VQEC_DP_SM_EVENT_INTERNAL_ERR, NULL);
        return (FALSE);
    }  

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                  "Channel %s, [%s] started activity timer timeout %llu\n",
                  vqec_dpchan_print_name(chan), 
                  __FUNCTION__, TIME_GET_R(usec, timeval_to_rel_time(&tv)));

    return (TRUE);    
}


/**---------------------------------------------------------------------------
 * FIN_SUCCESS state entry action. Stop the data-activity-timer if it is 
 * active,  and inform the channel of success so it joins all channels.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_enter_success_rcc (vqec_dpchan_t *chan,
                              vqec_dp_sm_state_t state, 
                              vqec_dp_sm_event_t event, void *arg)    
{    
    (void)vqec_dp_sm_activity_timer_stop(chan, state, event, arg);
    vqec_dpchan_rcc_success_notify(chan);
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * ABORT state entry action. Stop the data-activity-timer if it is active,
 * and notify the channel of an abort.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_enter_abort_rcc (vqec_dpchan_t *chan,
                            vqec_dp_sm_state_t state, 
                            vqec_dp_sm_event_t event, void *arg)
    
{    
    vqec_pak_t *pak_p, *pak_p_next;

    VQE_TAILQ_FOREACH_SAFE(pak_p, &chan->app_paks, ts_pkts,
                           pak_p_next) {
        VQE_TAILQ_REMOVE(&chan->app_paks, pak_p, ts_pkts);
        vqec_pak_free(pak_p);
    }

    (void)vqec_dp_sm_activity_timer_stop(chan, state, event, arg);
    vqec_dpchan_rcc_abort_notify(chan);
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Action: Join the primary stream.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_primary_join (vqec_dpchan_t *chan,
                         vqec_dp_sm_state_t state, 
                         vqec_dp_sm_event_t event, void *arg)
{
    /* stop the activity timer when issuing join. */
    (void)vqec_dp_sm_activity_timer_stop(chan, state, event, arg);
    vqec_dpchan_rcc_join_notify(chan);
    
    vqec_dp_sm_log(VQEC_DP_SM_LOG_ISSUED_JOIN, chan, state, event);
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Action: Send NCSI.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_send_ncsi (vqec_dpchan_t *chan,
                      vqec_dp_sm_state_t state, 
                      vqec_dp_sm_event_t event, void *arg)
{
    vqec_dpchan_rcc_ncsi_notify(chan);
    vqec_dp_sm_log(VQEC_DP_SM_LOG_FIRST_PRIMARY, chan, state, event);
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Action: Enable ER.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_notify_er_en (vqec_dpchan_t *chan,
                      vqec_dp_sm_state_t state, 
                      vqec_dp_sm_event_t event, void *arg)
{
    vqec_dpchan_rcc_er_en_notify(chan);    
    vqec_dp_sm_log(VQEC_DP_SM_LOG_ER_EN, chan, state, event);
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * INIT state action function map.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_action_fcn_t
vqec_dp_sm_action_fcn_init (vqec_dp_sm_event_t event)
{
    return vqec_dp_sm_null_action;
}


/**---------------------------------------------------------------------------
 * INIT state transition function.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_t
vqec_dp_sm_transition_fcn_init (vqec_dp_sm_event_t event)
{
    switch (event) {
    case VQEC_DP_SM_EVENT_START_RCC:
        return (VQEC_DP_SM_STATE_WAIT_FIRST_SEQ);

    case VQEC_DP_SM_EVENT_ABORT:
    case VQEC_DP_SM_EVENT_INTERNAL_ERR:
        return (VQEC_DP_SM_STATE_ABORT);

    default:
        return (VQEC_DP_SM_STATE_INVALID);
    }
}


/**---------------------------------------------------------------------------
 * WAIT_FIRST_SEQ state action function map.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_action_fcn_t
vqec_dp_sm_action_fcn_waitfirst (vqec_dp_sm_event_t event)
{
    switch (event) {
    case VQEC_DP_SM_EVENT_REPAIR:
        return vqec_dp_sm_first_repair_actions;

    default:
        return vqec_dp_sm_null_action;
    }
}


/**---------------------------------------------------------------------------
 * WAIT_FIRST_SEQ state transition function.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_t
vqec_dp_sm_transition_fcn_waitfirst (vqec_dp_sm_event_t event)
{
    switch (event) {
    case VQEC_DP_SM_EVENT_REPAIR:
        return (VQEC_DP_SM_STATE_WAIT_JOIN);

    case VQEC_DP_SM_EVENT_ABORT:
    case VQEC_DP_SM_EVENT_INTERNAL_ERR:
    case VQEC_DP_SM_EVENT_TIME_FIRST_SEQ:
        return (VQEC_DP_SM_STATE_ABORT);

    default:
        return (VQEC_DP_SM_STATE_INVALID);
    }
}


/**---------------------------------------------------------------------------
 * WAIT_JOIN state action function map.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_action_fcn_t
vqec_dp_sm_action_fcn_waitjoin (vqec_dp_sm_event_t event)
{
    switch (event) {
    case VQEC_DP_SM_EVENT_TIME_TO_JOIN:
        return vqec_dp_sm_primary_join;

    default:
        return vqec_dp_sm_null_action;
    }
}


/**---------------------------------------------------------------------------
 * WAIT_JOIN state transition function.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_t
vqec_dp_sm_transition_fcn_waitjoin (vqec_dp_sm_event_t event)
{
    switch (event) {
    case VQEC_DP_SM_EVENT_TIME_TO_JOIN:
        return (VQEC_DP_SM_STATE_WAIT_EN_ER);

    case VQEC_DP_SM_EVENT_ABORT:
    case VQEC_DP_SM_EVENT_INTERNAL_ERR:
    case VQEC_DP_SM_EVENT_ACTIVITY_TIMEOUT:
        return (VQEC_DP_SM_STATE_ABORT);

    default:
        return (VQEC_DP_SM_STATE_INVALID);
    }
}


/**---------------------------------------------------------------------------
 * WAIT_EN_ER state action function map.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_action_fcn_t
vqec_dp_sm_action_fcn_wait_er (vqec_dp_sm_event_t event)
{
    switch (event) {
    case VQEC_DP_SM_EVENT_PRIMARY:
        return vqec_dp_sm_send_ncsi;

    case VQEC_DP_SM_EVENT_TIME_TO_EN_ER:
        return vqec_dp_sm_notify_er_en;

    default:
        return vqec_dp_sm_null_action;
    }
}


/**---------------------------------------------------------------------------
 * WAIT_EN_ER state transition function.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_t
vqec_dp_sm_transition_fcn_wait_er (vqec_dp_sm_event_t event)
{
    switch (event) {
    case VQEC_DP_SM_EVENT_TIME_TO_EN_ER:
        return (VQEC_DP_SM_STATE_WAIT_END_BURST);

    case VQEC_DP_SM_EVENT_PRIMARY:
        return (VQEC_DP_SM_STATE_WAIT_EN_ER);

    case VQEC_DP_SM_EVENT_ABORT:
    case VQEC_DP_SM_EVENT_INTERNAL_ERR:
        return (VQEC_DP_SM_STATE_ABORT);

    default:
        return (VQEC_DP_SM_STATE_INVALID);
    }
}


/**---------------------------------------------------------------------------
 * WAIT_END_BURST state action function map.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_action_fcn_t
vqec_dp_sm_action_fcn_waitburst (vqec_dp_sm_event_t event)
{
    switch (event) {
    case VQEC_DP_SM_EVENT_PRIMARY:
        return vqec_dp_sm_send_ncsi;

    default:
        return vqec_dp_sm_null_action;
    }
}


/**---------------------------------------------------------------------------
 * WAIT_END_BURST state transition function.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_t
vqec_dp_sm_transition_fcn_waitburst (vqec_dp_sm_event_t event)
{
    switch (event) {
    case VQEC_DP_SM_EVENT_TIME_END_BURST:
        return (VQEC_DP_SM_STATE_FIN_SUCCESS);

    case VQEC_DP_SM_EVENT_PRIMARY:
        return (VQEC_DP_SM_STATE_WAIT_END_BURST);

    case VQEC_DP_SM_EVENT_ABORT:
    case VQEC_DP_SM_EVENT_INTERNAL_ERR:
        return (VQEC_DP_SM_STATE_ABORT);

    default:
        return (VQEC_DP_SM_STATE_INVALID);
    }
}


/**---------------------------------------------------------------------------
 * FIN_SUCCESS state action function map.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_action_fcn_t
vqec_dp_sm_action_fcn_finsuccess (vqec_dp_sm_event_t event)
{
    switch (event) {
    case VQEC_DP_SM_EVENT_PRIMARY:
        return vqec_dp_sm_send_ncsi;

    default:
        return vqec_dp_sm_null_action;
    }
}

/**---------------------------------------------------------------------------
 * FIN_SUCCESS state transition function.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_t
vqec_dp_sm_transition_fcn_finsuccess (vqec_dp_sm_event_t event)
{
    return (VQEC_DP_SM_STATE_FIN_SUCCESS);
}


/**---------------------------------------------------------------------------
 * ABORT state action function map.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_action_fcn_t
vqec_dp_sm_action_fcn_abort (vqec_dp_sm_event_t event)
{
    return vqec_dp_sm_null_action;
}

/**---------------------------------------------------------------------------
 * ABORT state transition function.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_sm_state_t
vqec_dp_sm_transition_fcn_abort (vqec_dp_sm_event_t event)
{
    return (VQEC_DP_SM_STATE_ABORT);
}


/**---------------------------------------------------------------------------
 * State function table: entry, exit methods and transition table.
 *---------------------------------------------------------------------------*/ 
#define DP_SM_ENTER_FCN(x) x, #x
#define DP_SM_EXIT_FCN(x) x, #x

UT_STATIC const
vqec_dp_sm_state_fcns_t s_dp_states[VQEC_DP_SM_STATE_NUM_VAL] =
{
    /* INVALID */
    {NULL, NULL, NULL, NULL, NULL, NULL},

    /* INIT */
    { DP_SM_ENTER_FCN(vqec_dp_sm_enter_def), 
      DP_SM_EXIT_FCN(vqec_dp_sm_exit_def), 
      vqec_dp_sm_action_fcn_init, 
      vqec_dp_sm_transition_fcn_init },

    /* WAIT_FIRST_SEQ */
    { DP_SM_ENTER_FCN(vqec_dp_sm_waitfirst_timer_start), 
      DP_SM_EXIT_FCN(vqec_dp_sm_waitfirst_timer_stop), 
      vqec_dp_sm_action_fcn_waitfirst, 
      vqec_dp_sm_transition_fcn_waitfirst },

    /* WAIT_JOIN */
    { DP_SM_ENTER_FCN(vqec_dp_sm_join_timer_start), 
      DP_SM_EXIT_FCN(vqec_dp_sm_join_timer_stop), 
      vqec_dp_sm_action_fcn_waitjoin, 
      vqec_dp_sm_transition_fcn_waitjoin },

    /* WAIT_EN_ER */
    { DP_SM_ENTER_FCN(vqec_dp_sm_er_timer_start), 
      DP_SM_EXIT_FCN(vqec_dp_sm_er_timer_stop), 
      vqec_dp_sm_action_fcn_wait_er, 
      vqec_dp_sm_transition_fcn_wait_er },

    /* WAIT_END_BURST */
    { DP_SM_ENTER_FCN(vqec_dp_sm_waitburst_timer_start), 
      DP_SM_EXIT_FCN(vqec_dp_sm_waitburst_timer_stop), 
      vqec_dp_sm_action_fcn_waitburst, 
      vqec_dp_sm_transition_fcn_waitburst },

    /* FIN_SUCCESS */
    { DP_SM_ENTER_FCN(vqec_dp_sm_enter_success_rcc), 
      DP_SM_EXIT_FCN(vqec_dp_sm_exit_def), 
      vqec_dp_sm_action_fcn_finsuccess, 
      vqec_dp_sm_transition_fcn_finsuccess },

    /* ABORT */
    { DP_SM_ENTER_FCN(vqec_dp_sm_enter_abort_rcc), 
      DP_SM_EXIT_FCN(vqec_dp_sm_exit_def), 
      vqec_dp_sm_action_fcn_abort, 
      vqec_dp_sm_transition_fcn_abort }
};


/**---------------------------------------------------------------------------
 * A simple guard function that protects actions. This may be made more
 * elaborate (per-state, per-event) later. The argument unused_arg is
 * unused, and is intended to be event-specific data which is provided by
 * the caller / context that delivers the event.
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_dp_sm_guard_fcn (vqec_dpchan_t *chan, 
                      vqec_dp_sm_state_t state, 
                      vqec_dp_sm_event_t event, void *unused_arg) 
{
    vqec_dp_sm_state_t nxt_state;

    nxt_state = (*s_dp_states[state].transition_fcn)(event);
    if (nxt_state == VQEC_DP_SM_STATE_INVALID) {
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                      "[DPSM] UNEXPECTED EVENT "
                      "Channel %s, state %s, event %s\n",
                      vqec_dpchan_print_name(chan),
                      vqec_dp_sm_state_to_string(chan->state),
                      vqec_dp_sm_event_to_string(event));
    }

    return (nxt_state  != VQEC_DP_SM_STATE_INVALID);
}


/**---------------------------------------------------------------------------
 * Deliver an event to the state machine for a given channel.  This method
 * can be recursively invoked by event action routines. A simple event queue
 * of 4 events is maintained, i.e., no more than 4 events may be generated
 * recursively from within action routines. *In this implementation the 
 * recursive call depth will never exceed 2.* The method will flush all pending
 * events before returning, i.e., before the outermost call returns, 
 * the event Q depth will always be 0.
 *
 * The method can return either TRUE or FALSE. It returns TRUE if 
 * an event and all of its subsequent actions generate no internal faults.
 * It returns FALSE if there is an internal resource failure in the
 * state-machine. 
 *
 * The state-machine will automatically transition to the abort state if
 * there is an internal resource allocation failure. 
 * 
 * @param[in] chan Pointer to the channel; must be null-checked prior
 * to invocation of this method.
 * @param[in] event Event type.
 * @param[in] arg Optional opaque argument pointer.
 * @param[out] boolean Returns true if all event-state-transitions succeeded
 * without any resource failures.
 *---------------------------------------------------------------------------*/ 
/*
 * NOTE: Ensure that the action routines do not recursively call deliver event
 * more than 4 times in a single call trace. If your implementation needs that
 * particular feature increase this number.
 */
boolean 
vqec_dp_sm_deliver_event (vqec_dpchan_t *chan,                         
                          vqec_dp_sm_event_t event, void *arg) 
{
    abs_time_t cur_time;
    vqec_dp_sm_event_q_t *event_q;
    boolean ret, ret_act = TRUE;
    vqec_dp_sm_state_action_fcn_t action;
    vqec_dp_sm_state_t next_state;
    uint32_t nxt_q_idx;

    /* Sanity checks on the FSM. */
    VQEC_DP_ASSERT_FATAL(chan, "%s", __FUNCTION__);
    VQEC_DP_ASSERT_FATAL(chan->state >= VQEC_DP_SM_MIN_STATE && 
                         chan->state <= VQEC_DP_SM_MAX_STATE, 
                         "%s", __FUNCTION__);
    VQEC_DP_ASSERT_FATAL(event >= VQEC_DP_SM_EVENT_MIN_EV &&
                         event <= VQEC_DP_SM_EVENT_MAX_EV, 
                         "%s", __FUNCTION__);
    VQEC_DP_ASSERT_FATAL(s_dp_states[chan->state].transition_fcn, 
                         "%s", __FUNCTION__);
    VQEC_DP_ASSERT_FATAL(chan->event_q_depth < 
                         VQEC_DP_SM_MAX_EVENTQ_DEPTH, 
                         "%s", __FUNCTION__);
    
    cur_time = get_sys_time();
    
    /* lease the next empty slot on the event queue. */
    event_q = &chan->event_q[chan->event_q_idx];
    VQEC_DP_ASSERT_FATAL(!event_q->lease, "%s", __FUNCTION__);
    chan->event_q_depth++;
    chan->event_q_idx = (chan->event_q_idx + 1) % VQEC_DP_SM_MAX_EVENTQ_DEPTH;
    nxt_q_idx = chan->event_q_idx;

    event_q->lease = TRUE; 
    event_q->event = event;
    event_q->arg = arg;
    VQEC_DP_ASSERT_FATAL(event < 32, "%s", __FUNCTION__);
    chan->event_mask |= (1 << event);

    if (chan->event_q_depth > 1) {
        /* Another event is already on the Q - enQ this one */
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                      "[DPSM] RECURSIVE_INVOCATION "
                      "%llu msec: Channel %s, state %s, event %s, "
                      "queued event, depth %d\n",
                      TIME_GET_A(msec, cur_time),
                      vqec_dpchan_print_name(chan),
                      vqec_dp_sm_state_to_string(chan->state),
                      vqec_dp_sm_event_to_string(event),
                      chan->event_q_depth);
        return (TRUE);
    }

    do {
        
        ret = vqec_dp_sm_guard_fcn(chan, 
                                   chan->state, event_q->event, event_q->arg); 
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                      "[DPSM] RX_EVENT "
                      "%llu msec: Channel %s, state %s, event %s, "
                      "action %s taken\n",
                      TIME_GET_A(msec, cur_time),
                      vqec_dpchan_print_name(chan),
                      vqec_dp_sm_state_to_string(chan->state),
                      vqec_dp_sm_event_to_string(event_q->event),
                      ret ? "is" : "is not");
        
        vqec_dp_sm_log(VQEC_DP_SM_LOG_STATE_EVENT, 
                       chan, chan->state, event_q->event);
        
        if (ret) {
            /* sa_ignore {"state" bound assert check} ARRAY_BOUNDS(1). */
            action = (*s_dp_states[chan->state].action_map_fcn)(event_q->event);
            ret_act &= 
                (*action)(chan, chan->state, event_q->event, event_q->arg);
            /* sa_ignore {"state" bound assert check} ARRAY_BOUNDS(2). */
            next_state = 
                (*s_dp_states[chan->state].transition_fcn)(event_q->event);
            
            if (next_state != chan->state) { 
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                              "[DPSM] EXIT_STATE "
                              "%llu msec: Channel %s, leaving state %s\n",
                              TIME_GET_A(msec, cur_time),
                              vqec_dpchan_print_name(chan),
                              vqec_dp_sm_state_to_string(chan->state));
                /* sa_ignore {"state" bound assert check} ARRAY_BOUNDS(5). */
                ret_act &= 
                    (*s_dp_states[chan->state].exit_fcn)(chan, 
                                                         chan->state, 
                                                         event_q->event, 
                                                         event_q->arg);
                
                vqec_dp_sm_log(VQEC_DP_SM_LOG_STATE_EXIT, 
                               chan, chan->state, event_q->event);
                
                chan->state = next_state;
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                              "[DPSM] ENTER_STATE "
                              "%llu msec: Channel %s, entering state %s\n",
                              TIME_GET_A(msec, cur_time),
                              vqec_dpchan_print_name(chan),
                              vqec_dp_sm_state_to_string(chan->state));
                /* sa_ignore {"state" bound assert check} ARRAY_BOUNDS(5). */
                ret_act &= 
                    (*s_dp_states[chan->state].entry_fcn)(chan, 
                                                          chan->state, 
                                                          event_q->event, 
                                                          event_q->arg);
                
                vqec_dp_sm_log(VQEC_DP_SM_LOG_STATE_ENTER, 
                               chan, chan->state, event_q->event);
            }
        }
        
        memset(event_q, 0, sizeof(*event_q));
        chan->event_q_depth--;
        event_q = &chan->event_q[nxt_q_idx];
        nxt_q_idx = (nxt_q_idx + 1) % VQEC_DP_SM_MAX_EVENTQ_DEPTH;

    } while (chan->event_q_depth);
    
    return (ret_act);
}

/**---------------------------------------------------------------------------
 * Generate an accurate state-event-transition diagram for the machine.
 *---------------------------------------------------------------------------*/ 
void
vqec_dp_sm_print (void)
{

    uint32_t i, j, k;
    
    VQE_FPRINTF(stdout, "digraph G {\n");
    VQE_FPRINTF(stdout, "\tsize = \"7.0,9\"; ratio=\"fill\"; orientation=\"landscape\";\n");
    for (i = VQEC_DP_SM_MIN_STATE; i <= VQEC_DP_SM_MAX_STATE; i++) {
        if (i == VQEC_DP_SM_STATE_FIN_SUCCESS || 
            i == VQEC_DP_SM_STATE_ABORT) {
            continue;
        }
        for (j = VQEC_DP_SM_EVENT_MIN_EV; j <= VQEC_DP_SM_EVENT_MAX_EV; j++) {
            k = (*s_dp_states[i].transition_fcn)(j);	
            if (k != VQEC_DP_SM_STATE_INVALID) {
                if (i == k) {
                    VQE_FPRINTF(stdout, "\t%s -> %s [style=bold, label=%s];\n", 
                           vqec_dp_sm_state_to_string(i),
                           vqec_dp_sm_state_to_string(k),
                           vqec_dp_sm_event_to_string(j));
                } else {
                    VQE_FPRINTF(stdout, "\t%s -> %s [style=bold, label=\"%s\\nEXIT/%s\\nENTER/%s\"];\n", 
                           vqec_dp_sm_state_to_string(i),
                           vqec_dp_sm_state_to_string(k),
                           vqec_dp_sm_event_to_string(j),
                           s_dp_states[i].exit_fcn_name,
                           s_dp_states[k].entry_fcn_name);
                }
            }
        }
    }
    VQE_FPRINTF(stdout, "}\n");
}


/**---------------------------------------------------------------------------
 * Initialize the state machine.
 *---------------------------------------------------------------------------*/ 
void
vqec_dp_sm_init (vqec_dpchan_t *chan)
{
    chan->state = VQEC_DP_SM_STATE_INIT;  
}


/**---------------------------------------------------------------------------
 * De-initialize the state machine (post a CP abort).
 *---------------------------------------------------------------------------*/ 
void 
vqec_dp_sm_deinit (vqec_dpchan_t *chan)
{
    if (chan->state >= VQEC_DP_SM_MIN_STATE) {
        (void)vqec_dp_sm_deliver_event(chan,
                                       VQEC_DP_SM_EVENT_ABORT,
                                       NULL);
    }
}


/**---------------------------------------------------------------------------
 * Return a reason for abort of the state-machine.
 *---------------------------------------------------------------------------*/ 
const char *
vqec_dp_sm_fail_reason (vqec_dpchan_t *chan)
{
    if (!chan->rcc_enabled) {
        return "RCC_DISABLED";
    }
    if (chan->rcc_in_abort) {
        if (chan->event_mask & 
            (1 << VQEC_DP_SM_EVENT_ABORT)) {
            return "CP_ABORT";
        } else if (chan->event_mask & 
                   (1 << VQEC_DP_SM_EVENT_INTERNAL_ERR)) {
            return "INTERNAL_ERR";
        } else if (chan->event_mask & 
                   (1 << VQEC_DP_SM_EVENT_TIME_FIRST_SEQ)) {
            return "FIRST_REPAIR_TIMEOUT";
        } else if (chan->event_mask & 
                   (1 << VQEC_DP_SM_EVENT_ACTIVITY_TIMEOUT)) {
            return "REPAIR_TIMEOUT";
        }  else {
            return "UNKNOWN";
        }
    } else {
        return "NONE";
    }
}

#endif   /* HAVE_FCC */
