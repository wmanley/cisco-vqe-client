/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: DP event counters for instrumentation.
 *
 * Documents: Taken from vqes-cm.
 *
 *****************************************************************************/

#ifndef __VQEC_DP_EVENT_COUNTER_H__
#define  __VQEC_DP_EVENT_COUNTER_H__

struct vqec_dp_ev_cnt_ ; 

/**
 * vqec_dp_ev_cnt_notify_cb_t
 *
 * Signature for callback function that gets called whenever an event
 * counter is bumped and the activity flag was not already set.
 *
 *@param[in] cnt - The event counter that the callback pertains to.
 */
typedef void (*vqec_dp_ev_cnt_notify_cb_t)(struct vqec_dp_ev_cnt_ * cnt);

/*
 * An event counter for keeping track of events that occur in realtime
 * code in the dataplane.
 */
typedef 
struct vqec_dp_ev_cnt_ 
{
    /**
     * Count of occurrences.  Not cleared. 
     */
    uint32_t count;
    /**
     * Optional callback function to call whenever active is first set.
     */
    vqec_dp_ev_cnt_notify_cb_t activity_notify;
    /**
     * User data to help during callback function.
     */
    void *user_ptr;
    /**
     * User data to help during callback function.
     */
    uint32_t user_data;
    /**
     * Flag set each time the counter is bumped.  Optionally cleared
     * when counter is read.
     */
    boolean  active;

} vqec_dp_ev_cnt_t;

/**
 * Initialize an event counter.
 *
 * @param[in] cnt - Pointer to the event counter to initialize
 * @param[in] activity_notify - Optional callback for when event occurs 
 * @param[in] user_ptr - User pointer parameter for use in callback
 * @param[in] user_data - User data value for use in callback
 * @return void
 */
static inline void
vqec_dp_ev_cnt_init (vqec_dp_ev_cnt_t * cnt,
                     vqec_dp_ev_cnt_notify_cb_t activity_notify,
                     void * user_ptr,
                     uint32_t user_data)
{
    memset(cnt, 0, sizeof(vqec_dp_ev_cnt_t));
    cnt->user_ptr = user_ptr;
    cnt->user_data = user_data;
    cnt->activity_notify = activity_notify;
}

/**
 * Increment the count associated with an event counter and set the
 * activity flag to indicate that the count is "dirty".  If the
 * activiy flag is being set afresh, then call the activity_notify
 * function if one was provided.
 *
 * @param[in] cnt - Pointer to the event counter to bump
 * @return void
 */
static inline void
vqec_dp_ev_cnt_cnt (vqec_dp_ev_cnt_t * cnt)
{
    cnt->count++;
    if (! cnt->active && cnt->activity_notify)
        (*cnt->activity_notify)(cnt);
    cnt->active = TRUE;
}

/**
 * vqec_dp_ev_cnt_get
 *
 * Fetch and return the value of an event counter.  Optionally clear
 * the activity flag to indicate that the counter has been read.
 * @param[in] cnt - Pointer to the event counter to read
 * @param[in] clear_active - If set, then clear the active flag.
 * @return - The value of the event counter
 */
static inline uint32_t 
vqec_dp_ev_cnt_get (vqec_dp_ev_cnt_t * cnt,
                    boolean clear_active)
{
    if (clear_active)
        cnt->active = FALSE;
    
    return cnt->count;
}


#endif /*  __VQEC_DP_EVENT_COUNTER_H__ */
