/*------------------------------------------------------------------
 * VQEC Events
 *
 * May 2006, Rob Drisko
 *
 * Copyright (c) 2006-2009 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __VQEC_EVENT_H__
#define __VQEC_EVENT_H__

#include <utils/vam_types.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/// @defgroup eventapi Libevent wrapper API (internal).
/// @{

typedef enum vqec_event_state_ {
    VQEC_EVENT_STATE_STOPPED = 0,   /* event loop is not running */
    VQEC_EVENT_STATE_EXITING,       /* event loop is exiting */
    VQEC_EVENT_STATE_RUNNING        /* event loop is running */
} vqec_event_state_t;

typedef struct vqec_event_ vqec_event_t;
                                //!< - User-opaque event structure 
#define VQEC_EV_READ            (0x1)
                                //!< - Report "reads" pending on FD
#define VQEC_EV_WRITE           (0x2)
                                //!< - Report "write" possible on FD
#define VQEC_EV_TIMEOUT         (0x4)
                                //!< - Status only; indicates timeout occurred
#define VQEC_EV_RECURRING       (0x8)
                                //!< - Recurring event, both FD and timer types
#define VQEC_EV_ONESHOT         (0x10)
                                //!< - One-shot event, both FD and timer types
#define VQEC_EVDESC_INVALID     (-1)
                                //!< - Invalid descriptor
#define VQEC_EVDESC_TIMER       VQEC_EVDESC_INVALID
                                //!< - Timer's do not have a FD; therefore 
                                //!< - the FD arg is defined to be invalid

//----------------------------------------------------------------------------
/// Event object types which make explicit the differences between creation 
/// for timer-based events, and descriptor-based events. The types <BR>
/// <B>VQEC_EVTYPE_TIMER</B><BR>
/// <B>VQEC_EVTYPE_FD</B><BR>
/// must be used for creation. For timers, a valid event-type must be
/// one of <B>VQEC_EV_RECURRING</B> or <B>VQEC_EV_ONESHOT</B>, while for 
/// file-descriptors, one of <B>VQEC_EV_READ</B>, <B>VQEC_EV_WRITE</B> 
/// must be specified, in addition to the recurring or one-shot property.
//----------------------------------------------------------------------------
typedef
enum vqec_event_evtypes_
{
    VQEC_EVTYPE_TIMER = 1,      //!< - Object for timers
    VQEC_EVTYPE_FD,             //!< - Object for File-descriptors
} vqec_event_evtypes_t;

//----------------------------------------------------------------------------
/// Initializes the event library. 
/// @param[out]  boolean Returns FALSE if the library initialization
/// fails because of an error, TRUE otherwise.
//----------------------------------------------------------------------------
boolean vqec_event_init(void);

//----------------------------------------------------------------------------
/// Run the event dispatch loop. This call *does not* return, until the
/// event loop exits. The loop will exit immediately returning FALSE
/// if there are no enQ'ed events.
/// @param[out]  boolean Returns FALSE if the loop was terminated because
/// of an error condition, or if there are no events to be scanned,
/// and TRUE otherwise.
//----------------------------------------------------------------------------
boolean vqec_event_dispatch(void);

//----------------------------------------------------------------------------
/// Exit the event loop after the timeout given in tv. If tv is NULL, is 0
/// the loop exits immediately. 
/// @param[in]   tv      Timeout after which the loop will exit. If tv is
/// is NULL or 0, the loops exits immediately.
/// @param[out]  boolean Returns TRUE on success, false on failure.
//----------------------------------------------------------------------------
boolean vqec_event_loopexit(struct timeval *tv);

//----------------------------------------------------------------------------
/// Instantiate a event object, which is either strictly time-driven, or
/// strictly by read & write events on descriptors. The types 
/// EVTYPE_TIMER(), and EVTYPE_FILEDESC() should be used to explicitly specify
/// the type. Below are a few examples on how to use creation call (each line
/// for create is a separate example!): <BR><BR>
/// <TT>
/// struct anystruct_ <BR>
/// { <BR>
/// &nbsp; vqec_event_t *ev; <BR>
/// }; <BR>
/// struct anystruct mystruct; <BR>
/// int32_t d; <BR>
/// int32_t fd; <BR>
/// void *evh(int32_t fd, int16_t event, void *data); <BR>
/// <BR>
/// vqec_event_create(&mystruct.ev, VQEC_EVTYPE_TIMER,
/// VQEC_EV_RECURRING, evh, VQEC_EVDESC_TIMER, &d);<BR>
/// vqec_event_create(&mystruct.ev, VQEC_EVTYPE_TIMER, VQEC_EV_ONESHOT, evh,
/// VQEC_EVDESC_TIMER, &d);<BR>
/// vqec_event_create(&mystruct.ev, VQEC_EVTYPE_FD,
/// VQEC_EV_READ|VQEC_EV_ONESHOT, evh, fd, &d);<BR>
/// vqec_event_create(&mystruct.ev, VQEC_EVTYPE_FD,
/// VQEC_EV_READ|VQEC_EV_WRITE|VQEC_EV_ONESHOT, evh, fd, &d);<BR>
/// vqec_event_create(&mystruct.ev, VQEC_EVTYPE_FD,
/// VQEC_EV_READ|VQEC_EV_WRITE|VQEC_EV_RECURRING, evh, fd, &d);<BR>
/// </TT>
///
/// The handle evptr is opaque to the user, and it's type is incomplete,
/// i.e., not defined for the users of this API. 
/// @param[in]  evptrptr A reference to the event object is created, 
/// stored in *vqec_ev.
/// @param[in]  type    The type specifier for the object.
/// @param[in]  events  Event specifiers from the list {READ, WRITE,
/// ONESHOT, RECURRING}. For timers one of ONESHOT, RECURRING must
/// be specified. File descriptors in addition should specify at least
/// one of READ, WRITE or both.
/// @param[in]  evh     Event handler, invoked when a requested event occurs. 
/// The descriptor with which the object is associated is the 1st
/// parameter in the handler, and is -1 for timers. The 2nd parameter is
/// the set of events that caused the handler to be invoked, i..e, 
/// EV_READ, EV_WRITE or EV_TIMEOUT.  The 3rd parameter is a pointer to
/// the object which was provided by the user as the "data" argument 
/// of the create call.
/// @param[in]  fd      Must the a valid & open-ed file descriptor for
/// descriptor-based events. In the case of timers, this value must
/// be set to VQEC_EVDESC_TIMER.
/// @param[in]  dataptr Some opaque object pointer, which will be returned
/// to the user as a parameter, when the associated handler is invoked
/// upon detection of a requested event. 
/// @param[out] boolean Returns TRUE on success, FALSE on failure.
//----------------------------------------------------------------------------
boolean vqec_event_create(vqec_event_t **evptrptr,
                          vqec_event_evtypes_t type,
                          int32_t events,
                          void (*evh)(const vqec_event_t * const, int32_t, 
                                      int16_t, void *),
                          int32_t fd,
                          void *dataptr);

//----------------------------------------------------------------------------
/// Activate the timer or descriptor-based event-object. For timers
/// a timeout <B>must</B> be specified via the timeval structure. For
/// descriptors this parameter <B>must</B> be null. <BR>
/// If the object was created with EV_ONESHOT, timers will pop only once
/// at the specified interval, and only the first read and write event
/// will be reported on the descriptor. <BR>
/// If the object was created with EV_RECURRING, then timers will repeatedly
/// pop at the specified interval, and descriptors will remain on the
/// read, write event-lists across events. 
/// @param[in]  evptr   Reference to event object.
/// @param[in]  tv      Timeout which must be non-null for timers,
/// and null for descriptors. The timeout is specified by a linux timeval
/// structure. Both secs and usecs cannot be 0 for recurring timers.
/// @param[out] boolean Returns TRUE on success, FALSE on failure.
//----------------------------------------------------------------------------
boolean vqec_event_start(const vqec_event_t *const evptr,
                         struct timeval *tv);

//----------------------------------------------------------------------------
/// Stop monitoring the timer for timeouts, or a descriptor for
/// read-write activity. The event-object is <B>not</B> destroyed and 
/// can be reused with vqec_event_start, if desired.
/// @param[in]  evptr   Reference to event object.  May be NULL, in which
/// case the function logs an error and FALSE is returned.
/// @param[out] boolean Returns TRUE on success, FALSE on failure.
//----------------------------------------------------------------------------                         
boolean vqec_event_stop(const vqec_event_t *const evptr);

//----------------------------------------------------------------------------
/// Free resources allocated with a event object.
/// @param[in]  evptrptr The reference to the event object in *evptrptr is
/// destroyed, and *evptrptr is set to null; *evptrptr must not be used after
/// this call, since the reference is destroyed.  evptrptr may be NULL, in 
/// case the function merely logs an error.
/// @param[out] void
//----------------------------------------------------------------------------
void vqec_event_destroy(vqec_event_t **evptrptr);

/*----------------------------------------------------------------------------
 * KERNEL-MODE SUPPORT METHODS. 
 * [The methods below are supported only when compiling for the kernel.]
 *----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Initialize the event library with an event pool, i.e., events will be 
 * allocated from the event pool.
 * 
 * @param[in] max Maximum number of events to be supported.
 * @param[out] boolean Returns true if the event API is successfully 
 * initialized, otherwise returns false.
 *----------------------------------------------------------------------------*/
boolean vqec_dp_event_init(uint32_t max);

/*----------------------------------------------------------------------------
 * Create the event loop task. The method waits for the task to startup,
 * and reparent, then returns. The task will not process any events until
 * dispatch() is invoked by the user.
 *
 * @param[out] boolean Returns true if the thread is created successfully,
 * otherwise returns false.
 *----------------------------------------------------------------------------*/
boolean vqec_dp_event_loop_enter(void);

/**---------------------------------------------------------------------------
 * Stop the event loop task.  If the thread cannot be stopped within
 * a fixed timeout, the method will return without further delay.
 *
 * @param[out] boolean If the task was successfully stopped returns true,
 * otherwise returns false.
 *---------------------------------------------------------------------------*/
boolean vqec_dp_event_loop_exit(void);

/**---------------------------------------------------------------------------
 * Run the timer loop of the event loop task.  This method must be
 * invoked *AFTER* vqec_event_loop_enter has been called.
 *
 * @param[out] boolean Returns true if a wakeup was successfully posted
 * to the event loop task, false otherwise. 
 *---------------------------------------------------------------------------*/
boolean vqec_dp_event_loop_dispatch(void);

/*----------------------------------------------------------------------------
 * Deinitialize the event library. 
 *----------------------------------------------------------------------------*/
void vqec_dp_event_deinit(void);

/*----------------------------------------------------------------------------
 * Display event histogram to the console.
 *----------------------------------------------------------------------------*/
void vqec_dp_event_display_hist(void);

/*----------------------------------------------------------------------------
 * Get the current state of the vqec_dp_event library.
 *----------------------------------------------------------------------------*/
vqec_event_state_t vqec_dp_event_get_state(void);

/*----------------------------------------------------------------------------
 * Sleep for a given number of milliseconds.
 *----------------------------------------------------------------------------*/
void vqec_dp_event_sleep(uint32_t sleepms);

/*----------------------------------------------------------------------------
 * Get the CPU usage for the event loop.
 *----------------------------------------------------------------------------*/
uint32_t vqec_event_get_benchmark(uint32_t interval);
void vqec_event_enable_benchmark(boolean enabled);
uint32_t vqec_dp_event_get_benchmark(uint32_t interval);

/// @}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __VQEC_EVENT_H__
