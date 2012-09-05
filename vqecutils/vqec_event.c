/*------------------------------------------------------------------
 * vqec events
 *
 * May 2006, Rob Drisko
 *
 * Copyright (c) 2006-2009 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "vqec_event.h"
#include "vqec_debug.h"
#include <sys/event.h>
#include "vqec_syslog_def.h"
#include "vqec_assert_macros.h"

#define VQEC_LOCK_DEFINITION
#include "vqec_lock_defs.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_interposers_common.h"
#endif // _VQEC_UTEST_INTERPOSERS

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

//
// Separate instances of event_base may be utilized to have two separate
// functioning instances of libevent within the same program. There are some
// restrictions vis-a-vis the use of signal event types; however, the vqec
// library does not use signal event type. 
//
UT_STATIC struct event_base *vqec_g_evbase;

//----------------------------------------------------------------------------
// VQEC private event object; encapsulates all user-data for an event.
//----------------------------------------------------------------------------
struct vqec_event_
{
    int32_t     refcnt;
                                //!< Reference count
    int32_t     type;
                                //!< Type of object
    int32_t     events_rw;
                                //!< R/W event requests
    int32_t     persist;
                                //!< Persistence 
    int32_t     fd;
                                //!<  File descriptor (descriptor-events)
    void        *dptr;
                                //!<  Opaque user-supplied data pointer
    void        (*userfunc)(const vqec_event_t * const, int32_t, 
                            int16_t, void *);
                                //!<  Callback function
    struct      timeval tv;
                                //!<  Associated timeout (timers)
    struct      event ev; 
                                //!<  Libevent event object
};

typedef
enum ev_errtype_t_
{
    VQEC_EVENT_ERR_GENERAL,
    VQEC_EVENT_ERR_MALLOC,
} ev_errtype_t;

static const char s_libevt_start_err[]   = "- Starting event via libevt failed -";
static const char s_libevt_add_err[]     = "- Adding event via libevt failed -";
static const char s_libevt_del_err[]     = "- Deleting event via libevt failed -";
static const char s_libevt_baseset_err[] = "- Setting event's base failed -";
static const char s_libevt_exit_err[]    = "- Libevent loop exit failed -";
static const char s_libevt_init_err[]    = "- Initialization of libevent failed -";
static const char s_libevt_is_null[]     = "- Libevent base ptr is null -";
static const char s_args_are_bad[]       = "- Invalid args -";

#define EVENT_ERR_STRLEN 256

static 
void vqec_event_log_err (ev_errtype_t type, const char *format, ...)
{
    char buf[EVENT_ERR_STRLEN];
    va_list ap;

    va_start(ap, format);    
    vsnprintf(buf, EVENT_ERR_STRLEN, format, ap);
    va_end(ap);

    switch (type) {
    case VQEC_EVENT_ERR_GENERAL:
        syslog_print(VQEC_ERROR, buf);
        break;

    case VQEC_EVENT_ERR_MALLOC:
        syslog_print(VQEC_MALLOC_FAILURE, buf);
        break;

    default:
        VQEC_ASSERT(FALSE);
    }
}

static int
vqec_event_lock_get (void)
{
    return (vqec_lock_lock(vqec_g_lock));    
}

static int
vqec_event_lock_put (void)
{
    return (vqec_lock_unlock(vqec_g_lock));
}

//----------------------------------------------------------------------------
// Convert local API event flags to libevent specific flags.
//----------------------------------------------------------------------------
static 
int16_t vqec_event_2libevt (int32_t ev)
{
    int16_t libev = 0;
    if (ev & VQEC_EV_READ) {
        libev |= EV_READ;
    } 
    if (ev & VQEC_EV_WRITE) {
        libev |= EV_WRITE;
    }
    if (ev & VQEC_EV_TIMEOUT) {
        libev |= EV_TIMEOUT;
    }
    return (libev);
}

//----------------------------------------------------------------------------
// Convert flags returned by libevent to local API flags.
//----------------------------------------------------------------------------
static 
int32_t vqec_event_2usr (int16_t ev)
{
    int32_t usrev = 0;
    if (ev & EV_READ) {
        usrev |= VQEC_EV_READ;
    } 
    if (ev & EV_WRITE) {
        usrev |= VQEC_EV_WRITE;
    }
    if (ev & EV_TIMEOUT) {
        usrev |= VQEC_EV_TIMEOUT;
    }
    return (usrev);
}

//----------------------------------------------------------------------------
// Global handler which wraps all registered callbacks, and also provides
// persistence for timers. It acquires and releases a lock around the 
// the entire function, including the user-callback.
//----------------------------------------------------------------------------
UT_STATIC
void vqec_g_ev_handler (int32_t fd, int16_t events, void *dptr)
{
    vqec_event_t *evptr = (vqec_event_t *)dptr;

    if (evptr && evptr->refcnt) {
        if (evptr->type == VQEC_EVTYPE_TIMER && 
            evptr->persist == VQEC_EV_RECURRING) {
            if (!vqec_event_start(evptr, &evptr->tv)) {

                vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, "%s %s",
                                   __FUNCTION__, s_libevt_start_err);
            }
        }
        if (evptr->userfunc) {
            (*(evptr->userfunc))(evptr, fd, 
                                 vqec_event_2usr(events), evptr->dptr);
        }
    } else {	
        if (!evptr) {
            vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, "%s %s (null evptr)", 
                               __FUNCTION__, s_args_are_bad);
        } else {
            vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, "%s %s (refcount is 0)",
                               __FUNCTION__, s_args_are_bad);
        }
    }
}

//----------------------------------------------------------------------------
// Initialize the libevent event library. 
//----------------------------------------------------------------------------
boolean vqec_event_init (void)
{
    boolean rv = TRUE;
    struct event_mutex m = {&vqec_event_lock_get, &vqec_event_lock_put};

    if (vqec_g_evbase) {
        VQEC_DEBUG(VQEC_DEBUG_EVENT,
                   "Libevent is already initialized");
    } else {
        vqec_g_evbase = event_init_with_lock(&m);
        if (!vqec_g_evbase) {
            rv = FALSE;
            vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, "%s %s", 
                               __FUNCTION__, s_libevt_init_err);
        }
    }

    event_init_benchmark();

    return (rv);
}

//----------------------------------------------------------------------------
// Start the event-loop. event_base_dispatch() does not return until there
// is an error or vqec_event_loopexit is invoked. Please remember that if
// there are no events registered with the library, event dispatch will 
// return immediately, with value -1.
//----------------------------------------------------------------------------
boolean vqec_event_dispatch (void) 
{
    boolean rv = TRUE;
    int32_t st;

    if (vqec_g_evbase) {
        if ((st = event_base_dispatch(vqec_g_evbase)) < 0) {
            rv = FALSE;
            VQEC_DEBUG(VQEC_DEBUG_EVENT,
                       "Libevent returned with status (%d)", st);
        }
    } else {
        rv = FALSE;
        vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, "%s %s", 
                           __FUNCTION__, s_libevt_is_null); 
    }

    return (rv);
}

//----------------------------------------------------------------------------
// Exit a running libevent dispatch loop, with timeout specified in tv.
// NULL and 0 tv's are ok.
//----------------------------------------------------------------------------
boolean vqec_event_loopexit (struct timeval *tv) 
{
    boolean rv = TRUE;
    int32_t st;

    if (vqec_g_evbase) {
        if ((st = event_base_loopexit(vqec_g_evbase, tv)) != 0) {
            rv = FALSE;
            vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, "%s %s", 
                               __FUNCTION__, s_libevt_exit_err);
        }
    } else {
        rv = FALSE;
        vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, "%s %s", 
                           __FUNCTION__, s_libevt_is_null);
    }

    return (rv);
}

//----------------------------------------------------------------------------
// Create a new event object; a pointer to the created object is returned
// in *evptrptr.  The event is *not* registered will libevent yet, and has
// a reference count of 1.
//----------------------------------------------------------------------------
boolean 
vqec_event_create (vqec_event_t **evptrptr,
                   vqec_event_evtypes_t type,
                   int32_t events,
                   void (*evh)(const vqec_event_t * const, int32_t, 
                               int16_t, void *),
                   int32_t fd,
                   void *dataptr)
{
    boolean rv = TRUE;
    int16_t flags;
    int32_t persist = events & (VQEC_EV_ONESHOT | VQEC_EV_RECURRING);

    if (evptrptr) {
        *evptrptr = NULL;
    }

    if (evptrptr &&  evh &&
         (type == VQEC_EVTYPE_TIMER || type == VQEC_EVTYPE_FD) && 
         (persist == VQEC_EV_ONESHOT || persist == VQEC_EV_RECURRING)) {
        
        *evptrptr = (vqec_event_t *)calloc(1, sizeof(vqec_event_t));
        if (!(*evptrptr)) {
            rv = FALSE;
            vqec_event_log_err(VQEC_EVENT_ERR_MALLOC, "%s", __FUNCTION__);
        } else {
            switch (type) {
            case VQEC_EVTYPE_TIMER:
                if ((fd != VQEC_EVDESC_TIMER) || 
                    ((events & (VQEC_EV_READ | VQEC_EV_WRITE)) != 0)) {
                    rv = FALSE;
                    vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, 
                                       "%s %s (timer args) (%d, %d)",
                                       __FUNCTION__, s_args_are_bad, fd, events);
                }
                break;
                
            case VQEC_EVTYPE_FD:
                if ((fd == VQEC_EVDESC_INVALID) ||
                    ((events & (VQEC_EV_READ | VQEC_EV_WRITE)) == 0)) {
                    rv = FALSE;
                    vqec_event_log_err(VQEC_EVENT_ERR_GENERAL,
                                       "%s %s (file descriptor args) (%d, %d)",
                                       __FUNCTION__, s_args_are_bad, fd, events);
                }
                break;

            default:
                VQEC_ASSERT(FALSE);
                break;
            }

            if (rv) {
                (*evptrptr)->type = type;
                (*evptrptr)->events_rw = events & (VQEC_EV_READ | VQEC_EV_WRITE);
                (*evptrptr)->persist = 
                    events & (VQEC_EV_ONESHOT | VQEC_EV_RECURRING);
                (*evptrptr)->fd = fd;
                (*evptrptr)->dptr = dataptr; 
                (*evptrptr)->userfunc = evh;   
                (*evptrptr)->refcnt++;
                if (type == VQEC_EVTYPE_FD) {
                    flags = vqec_event_2libevt((*evptrptr)->events_rw);
                    if (persist & VQEC_EV_RECURRING) {
                        flags |= EV_PERSIST;
                    }
                } else {
                    flags = 0;
                }
                event_set(&(*evptrptr)->ev, fd, flags, vqec_g_ev_handler,
                          (*evptrptr));
                if (event_base_set(vqec_g_evbase, &(*evptrptr)->ev) != 0) {
                    rv = FALSE;
                    vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, "%s %s", 
                                       __FUNCTION__, s_libevt_baseset_err);
                }
            }
        }
    } else {
        rv = FALSE;
        vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, 
                           "%s %s (input checking) (%p %p %d %d %d)",
                           __FUNCTION__, s_args_are_bad, 
                           evptrptr, evh, type, events, fd);
    }

    if (!rv && evptrptr && *evptrptr) {
        free(*evptrptr);
        *evptrptr = NULL;
    }

    return (rv);           
}

//----------------------------------------------------------------------------
// Start scanning events for an event objects. The event object is registered
// with libevent as part of this call. Time-periods are specified only 
// in the case of timers.  The cast to struct event is needed to remove 
// const-ness from vqec_event_t. Timeout of 0 is allowed only in one-shot
// mode to prevent unnecessary thrashing of events..
//----------------------------------------------------------------------------
boolean 
vqec_event_start (const vqec_event_t *const evptr,
                  struct timeval * tv) 
{
    boolean rv = TRUE;
    
    if (!evptr || (evptr->refcnt != 1)) {
        rv = FALSE;
        vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, 
                           "%s %s (evptr or refcnt) (%p %d)",
                           __FUNCTION__, s_args_are_bad, 
                           evptr, evptr ? evptr->refcnt : 0);
    } else if (((evptr->type == VQEC_EVTYPE_FD) && !tv) || 
        ((evptr->type == VQEC_EVTYPE_TIMER) &&
         (tv && (timerisset(tv) || 
                 (evptr->persist == VQEC_EV_ONESHOT))))) {

        if (event_add((struct event *)&evptr->ev, tv) == -1) {
            rv = FALSE;
            vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, "%s %s", 
                               __FUNCTION__, s_libevt_add_err);
        } else {
            if (tv) {
                ((vqec_event_t *)evptr)->tv = *tv;
            } else {
                memset(&((vqec_event_t *)evptr)->tv, 0, 
                       sizeof(struct timeval));
            }
        }
    } else {
        rv = FALSE;
        vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, 
                           "%s %s (input checking) (%d %d %p %lld/%lld)",
                           __FUNCTION__, s_args_are_bad, 
                           evptr->type, evptr->persist,
                           tv, tv ? tv->tv_usec : 0LL, tv ? tv->tv_sec : 0LL);
    }
        
    return (rv);
}

//----------------------------------------------------------------------------
// Stop a previously started event object. If the object was not started
// and a stop is  initiated the call will fail. This is not considered fatal
// in this implementation; a message is logged. The cast to struct event
// is needed to remove const-ness from vqec_event_t.
//----------------------------------------------------------------------------
boolean
vqec_event_stop (const vqec_event_t *const evptr)
{
    boolean rv = TRUE;

    if (!evptr || (evptr->refcnt != 1)) {
        rv = FALSE;
        vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, 
                           "%s %s (evptr or refcnt) (%p %d)",
                           __FUNCTION__, s_args_are_bad, 
                           evptr, evptr ? evptr->refcnt : 0);
    } else if (event_del((struct event *)&evptr->ev) == -1) {
        rv = FALSE;
        vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, "%s %s",
                           __FUNCTION__, s_libevt_del_err); 
    }
 
    return (rv);
}

//----------------------------------------------------------------------------
// Destroy an existing event object in *evptrptr. The memory allocated
// for the object is returned, and *evptrptr is set to NULL upon return.
// If the object is NULL this is not considered fatal in this implementation;
// a message is logged.
//----------------------------------------------------------------------------
void
vqec_event_destroy (vqec_event_t **evptrptr) 
{
    if (evptrptr && *evptrptr) {
        if ((*evptrptr)->refcnt == 1) {
            if (event_del(&(*evptrptr)->ev) == -1) {
                VQEC_DEBUG(VQEC_DEBUG_EVENT,
                           "Event not enQ'd");
            }
            free(*evptrptr);
            *evptrptr = NULL;
        } else {            
            vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, "%s %s (refcnt) (%d)",
                               __FUNCTION__, s_args_are_bad, (*evptrptr)->refcnt);
        }
    } else if (!evptrptr) {
        vqec_event_log_err(VQEC_EVENT_ERR_GENERAL, 
                           "%s %s (evptrptr) (%p %p)",
                           __FUNCTION__, s_args_are_bad, 
                           evptrptr, evptrptr ? *evptrptr : NULL);
    }
}


/**---------------------------------------------------------------------------
 * COMPILE STUBS [METHODS USED FOR KERNEL DP]. 
 *---------------------------------------------------------------------------*/
boolean 
vqec_dp_event_init (uint32_t max)
{
    return (TRUE);
}

boolean 
vqec_dp_event_loop_enter (void)
{
    return (TRUE);
}

boolean 
vqec_dp_event_loop_exit (void)
{
    return (TRUE);
}

boolean 
vqec_dp_event_loop_dispatch (void)
{
    return (TRUE);
}

void 
vqec_dp_event_deinit (void)
{
}

vqec_event_state_t vqec_dp_event_get_state (void)
{
    return (VQEC_EVENT_STATE_STOPPED);
}

void vqec_dp_event_sleep (uint32_t sleepms)
{
}

/**--------------------------------------------------------------------------
 * CPU Usage API 
 *-------------------------------------------------------------------------*/
void vqec_event_enable_benchmark (boolean enabled)
{
    if (enabled) {
        event_enable_benchmark();
    } else {
        event_disable_benchmark();
    }
}

uint32_t vqec_event_get_benchmark (uint32_t interval) 
{
    return event_get_benchmark(interval);
}

uint32_t vqec_dp_event_get_benchmark (uint32_t interval) 
{
    return 0;
}

