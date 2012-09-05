/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
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
#ifndef __VQEC_DP_SM_H__
#define __VQEC_DP_SM_H__

#include <utils/vam_time.h>
#include <vqec_dp_api.h>

struct vqec_dpchan_;

/**
 * NOTE: The order of the states cannot be changed without modifying the
 * state-transition table order in the implementation. Also, the position should not
 * be changed due to >= comparison (described below), unless the corresponding
 * dependency is also modified.
 */
typedef
enum vqec_dp_sm_state_
{
    /**
     * Invalid state.
     */
    VQEC_DP_SM_STATE_INVALID,
    /**
     * Initial state.
     */
    VQEC_DP_SM_STATE_INIT,
    /**
     * Wait for first repair-sequence packet.
     */
    VQEC_DP_SM_STATE_WAIT_FIRST_SEQ,
    /**
     * Wait for time-to-join.
     */
    VQEC_DP_SM_STATE_WAIT_JOIN,
    /**
     * Wait for enable ER.
     */
    VQEC_DP_SM_STATE_WAIT_EN_ER,
    /**
     * (KEEP THESE STATE IN ORDER AS THE LAST 3 STATES: see vqec_dpchan.c
     * vqec_dpchan_pak_event FOR THE REASON. 
     */
    /**
     * Wait for end-of-burst.
     */
    VQEC_DP_SM_STATE_WAIT_END_BURST,
    /**
     * Final state: RCC completed successfully. 
     */
    VQEC_DP_SM_STATE_FIN_SUCCESS,
    /**
     * Final state: The operation did not complete successfully, and was aborted.
     */
    VQEC_DP_SM_STATE_ABORT,
    /**
     * Must be last
     */
    VQEC_DP_SM_STATE_NUM_VAL

} vqec_dp_sm_state_t;

#define VQEC_DP_SM_MIN_STATE VQEC_DP_SM_STATE_INIT
#define VQEC_DP_SM_MAX_STATE VQEC_DP_SM_STATE_NUM_VAL - 1

/**
 * Event Q entry.
 */
#define VQEC_DP_SM_MAX_EVENTQ_DEPTH 4

typedef
struct vqec_dp_sm_event_q_
{
    boolean lease;              /* Is Q location leased */
    vqec_dp_sm_event_t event;   /* Event saved at this location */
    void *arg;                  /* Optional argument saved at this location */
} vqec_dp_sm_event_q_t;


/**
 * Log circular buffer entry.
 */
#define VQEC_DP_SM_LOG_MAX_SIZE 16

typedef
struct vqec_dp_sm_log_
{
    uint32_t log_event;         /* Logging event type  */
    vqec_dp_sm_state_t state;   /* Logged state-machine state */
    vqec_dp_sm_event_t event;   /* Logged state-machine event */
    abs_time_t timestamp;       /* Log timestamp */
} vqec_dp_sm_log_t;

/**
 * From event type to character representation.
 *
 * @param[in] e Event type.
 * @param[out] char* Pointer to a character representation for the event.
 */
const char * 
vqec_dp_sm_event_to_string(vqec_dp_sm_event_t e);
/**
 * From state to character representation.
 *
 * @param[in] state State.
 * @param[out] char* Pointer to a character representation for the state.
 */
const char * 
vqec_dp_sm_state_to_string(vqec_dp_sm_state_t state);
/**
 * Deliver an event to the state machine for a given channel.
 * 
 * @param[in] chan Pointer to the channel; must be null-checked prior
 * to invocation of this method.
 * @param[in] event Event type.
 * @param[in] arg Optional opaque argument pointer.
 * @param[out] boolean Returns true if all event-state-transitions succeeded
 * without any resource failures.
 */
boolean 
vqec_dp_sm_deliver_event(struct vqec_dpchan_ *chan,   
                         vqec_dp_sm_event_t event, void *arg);
/**
 * Initialize the state machine.
 * 
 * @param[in] chan Pointer to the channel; must be null-checked prior
 * to invocation of this method.
 */
void 
vqec_dp_sm_init(struct vqec_dpchan_ *chan); 
/**
 * Deinitialize the state machine.
 * 
 * @param[in] chan Pointer to the channel; must be null-checked prior
 * to invocation of this method.
 */
void 
vqec_dp_sm_deinit(struct vqec_dpchan_ *chan); 
/**
 * Copy log for cp-dp API.
 *
 * @param[in] chan Pointer to the channel; must be null-checked prior
 * to invocation of this method.
 * @param[out] dplog Output log buffer - must be null-checked prior
 * to invocation.
 */
#if HAVE_FCC
void
vqec_dp_sm_copy_log(struct vqec_dpchan_ *chan, 
                    vqec_dp_rcc_fsm_log_t *dplog);
#endif /* HAVE_FCC */
/**
 * Return a reason for abort of the state-machine.
 *
 * @param[in] chan Pointer to the channel; must be null-checked prior
 * to invocation of this method.
 * @param[out] char* A character representation for the failure code.
 */
const char *
vqec_dp_sm_fail_reason(struct vqec_dpchan_ *chan);
/**
 * Generate an accurate state-event-transition diagram for the machine.
 */
void
vqec_dp_sm_print(void);

#endif /* __VQEC_DP_SM_H__ */
