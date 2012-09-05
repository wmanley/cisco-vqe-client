/*
 * VQE-C Fast Channel Change state machine API
 * 
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef VQEC_SM_H
#define VQEC_SM_H

#include <sys/time.h>

struct vqec_chan_;

typedef 
enum vqec_sm_event_
{
    /**
     * Proceed with rapid channel change.
     */
    VQEC_EVENT_RAPID_CHANNEL_CHANGE,    
    /**
     * Proceed with rapid channel change.
     */
    VQEC_EVENT_SLOW_CHANNEL_CHANGE,
    /**
     * NAT binding is complete.
     */
    VQEC_EVENT_NAT_BINDING_COMPLETE,
    /**
     * Received a valid APP packet.
     */
    VQEC_EVENT_RECEIVE_VALID_APP,
    /**
     * Received invalid APP packet.
     */
    VQEC_EVENT_RECEIVE_INVALID_APP,
    /**
     * Received NULL APP packet.
     */
    VQEC_EVENT_RECEIVE_NULL_APP,
    /**
     * Timeout within nat-app wait mode. 
     */
    VQEC_EVENT_RCC_START_TIMEOUT,
    /**
     * Error in IPC layer.
     */
    VQEC_EVENT_RCC_IPC_ERR,
    /**
     *  Internal resource allocation error. 
     */
    VQEC_EVENT_RCC_INTERNAL_ERR,
    /**
     * Channel deinit event
     */
    VQEC_EVENT_CHAN_DEINIT,
    /**
     * Must be last.
     */
    VQEC_EVENT_NUM_VAL,

} vqec_sm_event_t;

#define VQEC_EVENT_MIN_EV VQEC_EVENT_RAPID_CHANNEL_CHANGE
#define VQEC_EVENT_MAX_EV VQEC_EVENT_NUM_VAL - 1

typedef
enum vqec_sm_state_
{
    /**
     * Invalid state.
     */
    VQEC_SM_STATE_INVALID,
    /**
     * Initial state.
     */
    VQEC_SM_STATE_INIT,
    /**
     * In wait for a NAT binding update and APP.
     */
    VQEC_SM_STATE_WAIT_APP,
    /**
     * Final state: control-plane state machine has terminated.
     */
    VQEC_SM_STATE_FIN_SUCCESS,
    /**
     * Final state: The operation did not complete successfully, and was aborted.
     */
    VQEC_SM_STATE_ABORT,
    /**
     * Must be last
     */
    VQEC_SM_STATE_NUM_VAL

} vqec_sm_state_t;

#define VQEC_SM_MIN_STATE VQEC_SM_STATE_INIT
#define VQEC_SM_MAX_STATE VQEC_SM_STATE_NUM_VAL - 1

/**
 * Event Q entry.
 */
#define VQEC_SM_MAX_EVENTQ_DEPTH 4

typedef
struct vqec_sm_event_q_
{
    boolean lease;              /* Is Q location leased */
    vqec_sm_event_t event;      /* Event saved at this location */
    void *arg;                  /* Optional argument saved at this location */
} vqec_sm_event_q_t;

/**
 * VQE-C state machine log event type.
 * Each entry in the state machine log is categorized as belonging to one
 * of the following event types.
 */
typedef enum vqec_sm_log_event_t_
{
    VQEC_SM_LOG_STATE_EVENT,    /* event occurred */
    VQEC_SM_LOG_STATE_ENTER,    /* state is entered */
    VQEC_SM_LOG_STATE_EXIT,     /* state is exited */
    VQEC_SM_LOG_NUM_VAL
} vqec_sm_log_event_t;
#define VQEC_SM_LOG_MIN_EVENT VQEC_SM_LOG_STATE_EVENT
#define VQEC_SM_LOG_MAX_EVENT VQEC_SM_LOG_NUM_VAL - 1

/**
 * Log circular buffer entry.
 */
#define VQEC_SM_LOG_MAX_SIZE 16

typedef
struct vqec_sm_log_
{
    vqec_sm_log_event_t log_event;    /* Logging event type  */
    vqec_sm_state_t state;            /* Logged state-machine state */
    vqec_sm_event_t event;            /* Logged state-machine event */
    abs_time_t timestamp;             /* Log timestamp */
} vqec_sm_log_t;


const char * 
vqec_sm_event_to_string(vqec_sm_event_t e);
const char * 
vqec_sm_state_to_string(vqec_sm_state_t state);
boolean
vqec_sm_deliver_event(struct vqec_chan_ *chan,   
                      vqec_sm_event_t event, void *arg);
void
vqec_sm_display_log(struct vqec_chan_ *chan);
void 
vqec_sm_init(struct vqec_chan_ *chan); 
void 
vqec_sm_deinit(struct vqec_chan_ *chan); 
const char *
vqec_sm_fail_reason(struct vqec_chan_ *chan);
void
vqec_sm_print(void);

#endif /* VQEC_SM_H */
