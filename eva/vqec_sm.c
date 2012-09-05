/*
 * VQE-C Fast Channel Change state machine
 * 
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#if HAVE_FCC
#include <utils/vam_types.h>
#include <utils/vam_time.h>
#include "vqec_sm.h"
#include "vqec_channel_private.h"
#include "vqec_debug.h"
#include "vqec_assert_macros.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

typedef vqec_sm_state_t (*vqec_sm_state_transition_fcn_t)(
    vqec_sm_event_t event);
typedef boolean (*vqec_sm_state_action_fcn_t)(vqec_chan_t *chan,
                                              vqec_sm_state_t state,
                                              vqec_sm_event_t event,
                                              void *arg);
typedef vqec_sm_state_action_fcn_t (*vqec_sm_state_map_action_t)(
    vqec_sm_event_t event);

typedef
struct vqec_sm_state_fcns_t_
{
    /**
     * Entry action handler.
     */
    vqec_sm_state_action_fcn_t entry_fcn;
    const char *entry_fcn_name;
    /**
     * Exit action handler.
     */
    vqec_sm_state_action_fcn_t exit_fcn;
    const char *exit_fcn_name;
    /**
     * Per-event action function map handler.
     */
    vqec_sm_state_map_action_t action_map_fcn;
    /**
     * Transition function.
     */
    vqec_sm_state_transition_fcn_t transition_fcn;

} vqec_sm_state_fcns_t;


/**---------------------------------------------------------------------------
 * From event type to character representation.
 *
 * @param[in] e Event type.
 * @param[out] char* Pointer to a character representation for the event.
 *---------------------------------------------------------------------------*/ 
const char * 
vqec_sm_event_to_string (vqec_sm_event_t e)
{
    switch(e) {        
    
    case VQEC_EVENT_RAPID_CHANNEL_CHANGE:
        return "RAPID_CHANNEL_CHANGE";
    case VQEC_EVENT_SLOW_CHANNEL_CHANGE:
        return "SLOW_CHANNEL_CHANGE";
    case VQEC_EVENT_NAT_BINDING_COMPLETE:
        return "NAT_BINDING_COMPLETE";
    case VQEC_EVENT_RECEIVE_VALID_APP:
        return "RECEIVE_VALID_APP";
    case VQEC_EVENT_RECEIVE_INVALID_APP:
        return "RECEIVE_INVALID_APP";
    case VQEC_EVENT_RECEIVE_NULL_APP:
        return "RECEIVE_NULL_APP";
    case VQEC_EVENT_RCC_START_TIMEOUT:
        return "RCC_START_TIMEOUT";
    case VQEC_EVENT_RCC_IPC_ERR:
        return "RCC_IPC_ERR";
    case VQEC_EVENT_RCC_INTERNAL_ERR:
        return "RCC_INTERNAL_ERROR";
    case VQEC_EVENT_CHAN_DEINIT:
        return "CHAN_DEINIT";
    default:
        return "UNKNOWN_ERROR";
    }
}


/**---------------------------------------------------------------------------
 * From state to character representation.
 *
 * @param[in] state State.
 * @param[out] char* Pointer to a character representation for the state.
 *---------------------------------------------------------------------------*/ 
const char * 
vqec_sm_state_to_string (vqec_sm_state_t state)
{
    switch (state) {
    case VQEC_SM_STATE_INVALID:
        return "STATE_INVALID";
    case VQEC_SM_STATE_INIT:
        return "STATE_INIT";
    case VQEC_SM_STATE_WAIT_APP:
        return "STATE_WAIT_APP";
    case VQEC_SM_STATE_ABORT:
        return "STATE_ABORT";
    case VQEC_SM_STATE_FIN_SUCCESS:
        return "STATE_SUCCESS";
    default:
        return "UNKNOWN_STATE";
    }
}

/**---------------------------------------------------------------------------
 * From event type to character representation.
 *
 * @param[in] evt Log event type.
 * @param[out] char* Pointer to a character representation for the event type.
 *---------------------------------------------------------------------------*/
UT_STATIC const char *
vqec_sm_log_evt_to_string (vqec_sm_log_event_t evt)
{
    switch (evt) {
    case VQEC_SM_LOG_STATE_EVENT:
        return "FSM_EVENT";
    case VQEC_SM_LOG_STATE_ENTER:
        return "STATE_ENTER";
    case VQEC_SM_LOG_STATE_EXIT:
        return "STATE_EXIT";
    default:
        return "UKNOWN_EVENT";
    }
}

/**---------------------------------------------------------------------------
 * Log of events / state transitions.
 *
 * @param[in] log_evt Type of event which occurred
 * @param[in] chan    Channel on which event occurred
 * @param[in] state   Current state on which event occurred
 * @param[in] event   Event which occurred
 *---------------------------------------------------------------------------*/
UT_STATIC void
vqec_sm_log (vqec_sm_log_event_t log_evt, vqec_chan_t *chan,
             vqec_sm_state_t state, vqec_sm_event_t event)
{
    vqec_sm_log_t *log;

    if (log_evt < VQEC_SM_LOG_MIN_EVENT || 
        log_evt > VQEC_SM_LOG_MAX_EVENT) {
        return;
    }
    
    log = &chan->sm_log[chan->sm_log_tail];
    chan->sm_log_tail = (chan->sm_log_tail + 1) % VQEC_SM_LOG_MAX_SIZE;
    
    log->state = state;
    log->event = event;
    log->log_event = log_evt;
    log->timestamp = get_sys_time();
}


/**---------------------------------------------------------------------------
 * Display event log for the given channel.
 *
 * @param[in] chan  Pointer to channel whose log it to be displayed
 *---------------------------------------------------------------------------*/
void
vqec_sm_display_log (vqec_chan_t *chan)
{
    uint32_t i;
    vqec_sm_log_t *log;

    VQEC_ASSERT(chan);
    for (i = chan->sm_log_head; 
         i != chan->sm_log_tail;
         i = (i + 1) % VQEC_SM_LOG_MAX_SIZE) {

        log = &chan->sm_log[i];

        CONSOLE_PRINTF("%-20s %-20s %-15s %-16llu\n",
                       vqec_sm_state_to_string(log->state),
                       vqec_sm_event_to_string(log->event),
                       vqec_sm_log_evt_to_string(log->log_event),
                       TIME_GET_A(msec, log->timestamp));
    }    
}


/**---------------------------------------------------------------------------
 * Empty enter() handler.
 *
 * @param[in] chan  Pointer to channel on which event occurred
 * @param[in] state State being entered
 * @param[in] event Event which caused entry into "state"
 * @param[in] arg   Event arguments (event-specific)
 *---------------------------------------------------------------------------*/
UT_STATIC boolean
vqec_sm_enter_def (vqec_chan_t *chan, 
                   vqec_sm_state_t state, 
                   vqec_sm_event_t event, void *arg) 
{
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Empty exit() handler.
 *
 * @param[in] chan  Pointer to channel on which event occurred
 * @param[in] state State being exited
 * @param[in] event Event which caused exit from "state"
 * @param[in] arg   Event arguments (event-specific)
 *---------------------------------------------------------------------------*/
UT_STATIC boolean
vqec_sm_exit_def (vqec_chan_t *chan, 
                  vqec_sm_state_t state, 
                  vqec_sm_event_t event, void *arg) 
{
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Null action handler.
 *
 * @param[in] chan  Pointer to channel on which event occurred
 * @param[in] state State for which event occurred
 * @param[in] event Event which occurred
 * @param[in] arg   Event arguments (event-specific)
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_sm_null_action (vqec_chan_t *chan, 
                     vqec_sm_state_t state, 
                     vqec_sm_event_t event, void *arg) 
{
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Abort state entry action function. It calls back into the channel to enable
 * error-repair / inform dataplane to abort as well. 
 *
 * @param[in] chan  Pointer to channel on which event occurred
 * @param[in] state State being entered
 * @param[in] event Event which caused entry into "state"
 * @param[in] arg   Event arguments (event-specific)
 *---------------------------------------------------------------------------*/
UT_STATIC boolean
vqec_sm_enter_abort_rcc (vqec_chan_t *chan, 
                         vqec_sm_state_t state, 
                         vqec_sm_event_t event, void *arg) 
{
    vqec_chan_rcc_abort_notify(chan);
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Action routine to stop the APP packet timer.
 *
 * @param[in] chan  Pointer to channel on which event occurred
 * @param[in] state State on which event occurred
 * @param[in] event Event which occurred
 * @param[in] arg   Event arguments (event-specific)
 *---------------------------------------------------------------------------*/
UT_STATIC boolean 
vqec_sm_app_timer_stop (vqec_chan_t *chan,
                        vqec_sm_state_t state, 
                        vqec_sm_event_t event, void *arg)
{
    VQEC_DEBUG(VQEC_DEBUG_RCC, 
               "Channel %s, app timer stop handler\n", 
               vqec_chan_print_name(chan));
               
    if (chan->app_timeout) {
        vqec_event_destroy(&chan->app_timeout);
    } 

    return (TRUE);
}
        

/**---------------------------------------------------------------------------
 * APP timer expiration handler. The RCC operation is aborted if this 
 * timer fires.
 *
 * @param[in] {evptr, fd, event} Event callback parameters
 * @param[in] arg  Pointer to channel on which timeout occurred
 *---------------------------------------------------------------------------*/
UT_STATIC void 
vqec_sm_app_timeout_handler (const vqec_event_t * const evptr,
                             int fd, short event, void *arg) 
{
    vqec_chan_t *chan = (vqec_chan_t *)arg;
    VQEC_ASSERT(chan);

    VQEC_DEBUG(VQEC_DEBUG_RCC, 
               "Channel %s, app timeout handler invoked\n", 
               vqec_chan_print_name(chan));

    g_channel_module->total_app_timeout_counter++; 
    /* sa_ignore {transition to abort state} IGNORE_RETURN(2) */ 
    (void)vqec_sm_deliver_event(chan, 
                                VQEC_EVENT_RCC_START_TIMEOUT, NULL);
} 


/**---------------------------------------------------------------------------
 * APP / initial repair packet timer setup. This includes:
 *  -- NAT binding response.
 *  -- PLI-NAK transmit.
 *  -- APP receipt.
 *
 * @param[in] chan  Pointer to channel on which event occurred
 * @param[in] state State for which event occurred
 * @param[in] event Event which occurred
 * @param[in] arg   Event arguments (event-specific)
 *---------------------------------------------------------------------------*/
UT_STATIC boolean
vqec_sm_app_timer_start (vqec_chan_t *chan,
                         vqec_sm_state_t state, 
                         vqec_sm_event_t event, void *arg)
    
{
    struct timeval tv;

    chan->rcc_start_time = get_sys_time();
    
    /* 
     * Start a timer to wait for completion of NAT resolution, send of
     * the NAK-PLI and the arrival of the APP packet. 
     */
    tv = rel_time_to_timeval(chan->app_timeout_interval);

    VQEC_ASSERT(!chan->app_timeout);
    if (!vqec_event_create(&chan->app_timeout, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_ONESHOT,
                           vqec_sm_app_timeout_handler, 
                           VQEC_EVDESC_TIMER,
                           chan) ||
        !vqec_event_start(chan->app_timeout, &tv)) {
        vqec_event_destroy(&chan->app_timeout);
        syslog_print(VQEC_CHAN_ERROR,
                     vqec_chan_print_name(chan), 
                     "failed to create app timer event");
        /* sa_ignore {transition to abort state} IGNORE_RETURN(2) */ 
        (void)vqec_sm_deliver_event(chan, 
                                    VQEC_EVENT_RCC_INTERNAL_ERR, NULL);
        return (FALSE);
    }

    chan->first_repair_deadline = 
        TIME_ADD_A_R(get_sys_time(), chan->app_timeout_interval);
    VQEC_DEBUG(VQEC_DEBUG_RCC, 
               "Channel %s, APP timer started time %llu msec\n",
               vqec_chan_print_name(chan), 
               TIME_GET_R(msec, chan->app_timeout_interval)); 
    
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Send a NAK-PLI to start a RCC burst. If RCC is disabled on the channel,
 * then simply abort the RCC burst.
 *
 * @param[in] chan  Pointer to channel on which event occurred
 * @param[in] state State for which event occurred
 * @param[in] event Event which occurred
 * @param[in] arg   Event arguments (event-specific)
 *---------------------------------------------------------------------------*/
UT_STATIC boolean
vqec_sm_send_nakpli (vqec_chan_t *chan, 
                     vqec_sm_state_t state, 
                     vqec_sm_event_t event, void *arg) 
{
    boolean ret;
    boolean do_fast_fill = FALSE;
    uint32_t recv_bw;

    if (chan->fast_fill_enabled && chan->fastfill_ops.fastfill_start &&
        chan->fastfill_ops.fastfill_abort && 
        chan->fastfill_ops.fastfill_done) {
        do_fast_fill = TRUE;
    }

    /* compute receive bw and determine if it's enough to do RCC */
    recv_bw = vqec_chan_get_max_recv_bw_rcc(chan);
    if (!recv_bw && chan->max_recv_bw_rcc) {
        /*
         * FEC bw must be greater than max_recv_bw_rcc; cause the RCC not to
         * happen by setting recv_bw to 1 bps, which is too low for an RCC.
         */
        recv_bw = 1;
    }

    ret = rtp_era_send_nakpli(vqec_chan_get_primary_session(chan),
                              chan->rcc_min_fill, 
                              chan->rcc_max_fill,
                              (uint32_t)do_fast_fill,
                              recv_bw,
                              chan->max_fastfill);    
    if (!ret) {
        syslog_print(VQEC_CHAN_ERROR,
                     vqec_chan_print_name(chan), 
                     "failed to send nak pli");
        /* sa_ignore {transition to abort state} IGNORE_RETURN(2) */ 
        (void)vqec_sm_deliver_event(chan, 
                                    VQEC_EVENT_RCC_INTERNAL_ERR, NULL); 
        return (FALSE);
    }

    chan->nakpli_sent = TRUE;
    chan->nakpli_sent_time = get_sys_time();
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * INIT state transition function.
 *
 * @param[in] event  Event which is triggering the transition. 
 *---------------------------------------------------------------------------*/
UT_STATIC vqec_sm_state_t
vqec_sm_transition_fcn_init (vqec_sm_event_t event)
{
    switch (event) {
    case VQEC_EVENT_RAPID_CHANNEL_CHANGE:
        return (VQEC_SM_STATE_WAIT_APP);

    case VQEC_EVENT_SLOW_CHANNEL_CHANGE:
        return (VQEC_SM_STATE_FIN_SUCCESS);

    default:
        return (VQEC_SM_STATE_INVALID);
    }
}


/**---------------------------------------------------------------------------
 * WAIT_APP state transition function.
 *
 * @param[in] event  Event which is triggering the transition. 
 *---------------------------------------------------------------------------*/
UT_STATIC vqec_sm_state_t
vqec_sm_transition_fcn_waitapp (vqec_sm_event_t event)
{
    switch (event) {
    case VQEC_EVENT_NAT_BINDING_COMPLETE:
        return (VQEC_SM_STATE_WAIT_APP);

    case VQEC_EVENT_RECEIVE_VALID_APP:
        return (VQEC_SM_STATE_FIN_SUCCESS);

    case VQEC_EVENT_RECEIVE_INVALID_APP:
    case VQEC_EVENT_RECEIVE_NULL_APP:
    case VQEC_EVENT_RCC_START_TIMEOUT:
    case VQEC_EVENT_RCC_INTERNAL_ERR:
    case VQEC_EVENT_RCC_IPC_ERR:
    case VQEC_EVENT_CHAN_DEINIT:
        return (VQEC_SM_STATE_ABORT);

    default:
        return (VQEC_SM_STATE_INVALID);
    }
}


/**---------------------------------------------------------------------------
 * FIN_SUCCESS state transition function.
 *
 * @param[in] event  Event which is triggering the transition. 
 *---------------------------------------------------------------------------*/
UT_STATIC vqec_sm_state_t
vqec_sm_transition_fcn_finsuccess (vqec_sm_event_t event)
{
    return (VQEC_SM_STATE_FIN_SUCCESS);
}


/**---------------------------------------------------------------------------
 * ABORT state transition function.
 *
 * @param[in] event  Event which is triggering the transition. 
 *---------------------------------------------------------------------------*/
UT_STATIC vqec_sm_state_t
vqec_sm_transition_fcn_abort (vqec_sm_event_t event)
{
    return (VQEC_SM_STATE_ABORT);
}
        

/**---------------------------------------------------------------------------
 * INIT state action function map.
 *
 * @param[in] event  Event which is triggering the action. 
 *---------------------------------------------------------------------------*/
UT_STATIC vqec_sm_state_action_fcn_t
vqec_sm_action_fcn_init (vqec_sm_event_t event)
{
    return vqec_sm_null_action;
}


/**---------------------------------------------------------------------------
 * WAIT_APP state action function map.
 *
 * @param[in] event  Event which is triggering the action. 
 *---------------------------------------------------------------------------*/
UT_STATIC vqec_sm_state_action_fcn_t
vqec_sm_action_fcn_waitapp (vqec_sm_event_t event)
{
    switch (event) {
    case VQEC_EVENT_NAT_BINDING_COMPLETE:
        return vqec_sm_send_nakpli;
        
    default:
        return vqec_sm_null_action;
    }
}


/**---------------------------------------------------------------------------
 * FIN_SUCCESS state action function map.
 *
 * @param[in] event  Event which is triggering the action. 
 *---------------------------------------------------------------------------*/
UT_STATIC vqec_sm_state_action_fcn_t
vqec_sm_action_fcn_finsuccess (vqec_sm_event_t event)
{
    return vqec_sm_null_action;
}


/**---------------------------------------------------------------------------
 * ABORT state action function map.
 *
 * @param[in] event  Event which is triggering the action. 
 *---------------------------------------------------------------------------*/
UT_STATIC vqec_sm_state_action_fcn_t
vqec_sm_action_fcn_abort (vqec_sm_event_t event)
{
    return vqec_sm_null_action;
}


/**---------------------------------------------------------------------------
 * State function table: entry, exit methods and transition table.
 *---------------------------------------------------------------------------*/
#define SM_ENTER_FCN(x) x, #x
#define SM_EXIT_FCN(x) x, #x

UT_STATIC const
vqec_sm_state_fcns_t s_cp_states[VQEC_SM_STATE_NUM_VAL] =
{
    /* INVALID */
    {NULL, NULL, NULL, NULL, NULL, NULL},

    /* INIT */
    { SM_ENTER_FCN(vqec_sm_enter_def),
      SM_EXIT_FCN(vqec_sm_exit_def), 
      vqec_sm_action_fcn_init, 
      vqec_sm_transition_fcn_init },

    /* WAIT_APP */
    { SM_ENTER_FCN(vqec_sm_app_timer_start), 
      SM_EXIT_FCN(vqec_sm_app_timer_stop), 
      vqec_sm_action_fcn_waitapp, 
      vqec_sm_transition_fcn_waitapp },

    /* FIN_SUCCESS */
    { SM_ENTER_FCN(vqec_sm_enter_def), 
      SM_EXIT_FCN(vqec_sm_exit_def), 
      vqec_sm_action_fcn_finsuccess, 
      vqec_sm_transition_fcn_finsuccess },

    /* ABORT */
    { SM_ENTER_FCN(vqec_sm_enter_abort_rcc), 
      SM_EXIT_FCN(vqec_sm_exit_def), 
      vqec_sm_action_fcn_abort, 
      vqec_sm_transition_fcn_abort }
};


/**---------------------------------------------------------------------------
 * A simple guard function that protects actions. This may be made more
 * elaborate (per-state, per-event) later.
 *
 * @param[in] chan  Pointer to channel on which event occurred
 * @param[in] state State for which event occurred
 * @param[in] event Event which occurred
 * @param[in] fil_arg   Event arguments (event-specific)
 *---------------------------------------------------------------------------*/ 
UT_STATIC boolean
vqec_sm_guard_fcn (vqec_chan_t *chan, 
                   vqec_sm_state_t state, 
                   vqec_sm_event_t event, void *fil_arg) 
{
    vqec_sm_state_t nxt_state;

    nxt_state = (*s_cp_states[state].transition_fcn)(event);
    if (nxt_state == VQEC_SM_STATE_INVALID) {
        VQEC_DEBUG(VQEC_DEBUG_RCC,
                   "[SM] UNEXPECTED EVENT "
                   "Channel %s, state %s, event %s\n",
                   vqec_chan_print_name(chan),
                   vqec_sm_state_to_string(chan->state),
                   vqec_sm_event_to_string(event));
    }
    return 
        (nxt_state != VQEC_SM_STATE_INVALID);
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
vqec_sm_deliver_event (vqec_chan_t *chan, 
                       vqec_sm_event_t event, void *arg) 
{
    abs_time_t cur_time;
    vqec_sm_event_q_t *event_q;
    boolean ret, ret_act = TRUE;
    vqec_sm_state_action_fcn_t action;
    vqec_sm_state_t next_state;
    uint32_t nxt_q_idx;

    /* Sanity checks to ensure that the FSM memory is not corrupt. */
    VQEC_ASSERT(chan);
    VQEC_ASSERT(chan->state >= VQEC_SM_MIN_STATE && 
                chan->state <= VQEC_SM_MAX_STATE);
    VQEC_ASSERT(event >= VQEC_EVENT_MIN_EV &&
                event <= VQEC_EVENT_MAX_EV);
    VQEC_ASSERT(s_cp_states[chan->state].transition_fcn);
    VQEC_ASSERT(chan->event_q_depth < VQEC_SM_MAX_EVENTQ_DEPTH);

    cur_time = get_sys_time();
    
    /* lease the next empty slot on the event queue. */
    event_q = &chan->event_q[chan->event_q_idx];
    VQEC_ASSERT(!event_q->lease);
    chan->event_q_depth++;
    chan->event_q_idx = (chan->event_q_idx + 1) % VQEC_SM_MAX_EVENTQ_DEPTH;
    nxt_q_idx = chan->event_q_idx;

    event_q->lease = TRUE; 
    event_q->event = event;
    event_q->arg = arg;
    VQEC_ASSERT(event < sizeof(vqec_sm_event_t)*8);
    chan->event_mask |= (1 << event);
    
    if (chan->event_q_depth > 1) {
        /* Another event is already on the Q - enQ this one */
        VQEC_DEBUG(VQEC_DEBUG_RCC,
                   "[SM] RECURSIVE_INVOCATION "
                   "%llu msec: Channel %s, state %s, event %s, "
                   "queued event, depth %d\n",
                   TIME_GET_A(msec, cur_time),
                   vqec_chan_print_name(chan),
                   vqec_sm_state_to_string(chan->state),
                   vqec_sm_event_to_string(event),
                   chan->event_q_depth);
        return (TRUE);          /* event successfully placed on Q */
    }

    do {

        ret = vqec_sm_guard_fcn(chan, 
                                chan->state, event_q->event, event_q->arg); 
        VQEC_DEBUG(VQEC_DEBUG_RCC,
                   "[SM] RX_EVENT "
                   "%llu msec: Channel %s, state %s, event %s, action %s taken\n",
                   TIME_GET_A(msec, cur_time),
                   vqec_chan_print_name(chan),
                   vqec_sm_state_to_string(chan->state),
                   vqec_sm_event_to_string(event_q->event),
                   ret ? "is" : "is not");

        vqec_sm_log(VQEC_SM_LOG_STATE_EVENT, 
                    chan, chan->state, event_q->event);

        if (ret) {
           /* sa_ignore {array boundary checked} ARRAY_BOUNDS(1) */ 
            action = (*s_cp_states[chan->state].action_map_fcn)(event_q->event);
            ret_act &= 
                (*action)(chan, chan->state, event_q->event, event_q->arg);
           /* sa_ignore {array boundary checked} ARRAY_BOUNDS(2) */ 
            next_state = 
                (*s_cp_states[chan->state].transition_fcn)(event_q->event);
            
            if (next_state != chan->state) { 
                VQEC_DEBUG(VQEC_DEBUG_RCC,
                           "[SM] EXIT_STATE "
                           "%llu msec: Channel %s, leaving state %s\n",
                           TIME_GET_A(msec, cur_time),
                           vqec_chan_print_name(chan),
                           vqec_sm_state_to_string(chan->state));
               /* sa_ignore {array boundary checked} ARRAY_BOUNDS(1) */ 
                ret_act &= (*s_cp_states[chan->state].exit_fcn)(chan, chan->state, 
                                                             event_q->event, event_q->arg);

                vqec_sm_log(VQEC_SM_LOG_STATE_EXIT, 
                            chan, chan->state, event_q->event);

                chan->state = next_state;
                VQEC_DEBUG(VQEC_DEBUG_RCC,
                           "[SM] ENTER_STATE "
                           "%llu msec: Channel %s, entering state %s\n",
                           TIME_GET_A(msec, cur_time),
                           vqec_chan_print_name(chan),
                           vqec_sm_state_to_string(chan->state));
               /* sa_ignore {array boundary checked} ARRAY_BOUNDS(3) */ 
                ret_act &= 
                    (*s_cp_states[chan->state].entry_fcn)(chan, chan->state,
                                             event_q->event, event_q->arg);

                vqec_sm_log(VQEC_SM_LOG_STATE_ENTER, 
                            chan, chan->state, event_q->event);
            }
        }
        
        memset(event_q, 0, sizeof(*event_q));
        chan->event_q_depth--;
        event_q = &chan->event_q[nxt_q_idx];
        nxt_q_idx = (nxt_q_idx + 1) % VQEC_SM_MAX_EVENTQ_DEPTH;

    } while (chan->event_q_depth);  /* event Q is 0 when we return */
    
    return (ret_act);
}


/**---------------------------------------------------------------------------
 * Generate an accurate state-event-transition diagram for the machine. 
 * This diagram can then be input to "dot" to generate a pictorial graph
 * representation of the state-machine.
 *---------------------------------------------------------------------------*/ 
void
vqec_sm_print (void)
{
    uint32_t i, j, k;
    
    printf("digraph G {\n");
    printf("\tsize = \"8.5,11\"; ratio=\"fill\"; orientation=\"landscape\";\n");
    for (i = VQEC_SM_MIN_STATE; i <= VQEC_SM_MAX_STATE; i++) {
        if (i == VQEC_SM_STATE_FIN_SUCCESS || 
            i == VQEC_SM_STATE_ABORT) {
            continue;
        }
        for (j = VQEC_EVENT_MIN_EV; j <= VQEC_EVENT_MAX_EV; j++) {          
            k = (*s_cp_states[i].transition_fcn)(j);
            if (k != VQEC_SM_STATE_INVALID) {
                if (i == k) {
                    printf("\t%s -> %s [style=bold, label=%s];\n", 
                           vqec_sm_state_to_string(i),
                           vqec_sm_state_to_string(k),
                           vqec_sm_event_to_string(j));
                } else {
                    printf("\t%s -> %s [style=bold, label=\"%s\\nEXIT/%s\\nENTER/%s\"];\n", 
                           vqec_sm_state_to_string(i),
                           vqec_sm_state_to_string(k),
                           vqec_sm_event_to_string(j),
                           s_cp_states[i].exit_fcn_name,
                           s_cp_states[k].entry_fcn_name);
                }
            }
        }
    }
    printf("}\n");
}


/**---------------------------------------------------------------------------
 * Initialize the state machine.
 *
 * @param[in] chan Channel whose state machine is to be initializied.
 *---------------------------------------------------------------------------*/
void 
vqec_sm_init (vqec_chan_t *chan)
{
    chan->state = VQEC_SM_STATE_INIT;
}


/**---------------------------------------------------------------------------
 * De-initialize the state machine.
 *
 * @param[in] chan Channel whose state machine is to be de-initializied.
 *---------------------------------------------------------------------------*/
void 
vqec_sm_deinit (vqec_chan_t *chan)
{
    if (chan->state >= VQEC_SM_MIN_STATE) {
        /* sa_ignore {transition to abort state} IGNORE_RETURN(2) */ 
        (void)vqec_sm_deliver_event(chan,
                                    VQEC_EVENT_CHAN_DEINIT,
                                    NULL);
    }
}

/**---------------------------------------------------------------------------
 * Return a reason for abort of the state-machine.
 *
 * @param[in] chan    Channel for which RCC aborted
 * @param[out] char * String which describes failure code
 *---------------------------------------------------------------------------*/
const char *
vqec_sm_fail_reason (vqec_chan_t *chan)
{

    if (!chan->rcc_enabled) {
        return "RCC_DISABLED";
    }
    if (chan->rcc_in_abort) {
        if (chan->event_mask & 
            (1 << VQEC_EVENT_RECEIVE_INVALID_APP)) {
            return "INVALID_APP";
        } else if (chan->event_mask & 
                   (1 << VQEC_EVENT_RECEIVE_NULL_APP)) {
            return "NULL_APP";
        } else if (chan->event_mask & 
                   (1 << VQEC_EVENT_RCC_START_TIMEOUT)) {
            if (chan->nakpli_sent) {
                return "APP_TIMEOUT";
            } else {
                return "NAT_TIMEOUT";
            }
        } else if (chan->event_mask & 
                   (1 << VQEC_EVENT_RCC_IPC_ERR)) {
            return "IPC_ERROR";
        } else if (chan->event_mask &
                   (1 << VQEC_EVENT_CHAN_DEINIT)) {
            return "CHAN_DEINIT";
        } else {
            return "UNKNOWN";
        }
    } else {
        return "NONE";
    }
}
#endif   /* HAVE_FCC */
