/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Event.
 *
 * Documents: 
 *
 *****************************************************************************/

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/sched.h>
#include "vqec_event.h"
#include "add-ons/include/tree.h"
#include "vqec_lock.h"
#include "utils/queue_plus.h"
#include "utils/vam_time.h"
#include "utils/zone_mgr.h"
#include "utils/vam_hist.h"

#define VQEC_EVENT_LOG(err, format,...)                         \
    printk(err "<vqec-event>" format "\n", ##__VA_ARGS__);


/*****************************************************************************
 * Event base structure.
 *****************************************************************************/
typedef
struct vqec_event_base_
{    
    uint32_t num_events;        /* events in the event list */
    uint32_t active_events;     /* active events on the timer list */
    struct vqe_zone *event_pool;
                                /* event allocation pool */
    vam_hist_type_t *hist;      /* event-jitter histogram */
    VQE_TAILQ_HEAD(vqec_event_queue, vqec_event_) event_queue;
                                /* event tail queue */
    VQE_RB_HEAD(vqec_time_tree, vqec_event_) time_tree;
                                /* timer RB tree for active timers */
} vqec_event_base_t;


/*****************************************************************************
 * Event meta structure.
 *****************************************************************************/
enum 
{
    VQEC_EVENT_STATE_NONE,      /* newly created */
    VQEC_EVENT_STATE_INACTIVE,  /* inactive state, i.e., not on time-tree */
    VQEC_EVENT_STATE_ACTIVE     /* on time-tree */
};

struct vqec_event_ 
{ 
    int32_t refcnt;             /* reference count */
    int32_t type;               /* type of event [only timers are supported] */
    boolean persist;            /* if true, recurring event */
    void (*userfunc)(const vqec_event_t * const, int32_t, 
                     int16_t, void *);
                                /* user callback function */
    void *dptr;                 /* opaque user-supplied data pointer */
    struct timeval timeout;     /* timeout [relative] for the event */
    abs_time_t exp_time;        /* event's absolute expiration time. */
    uint32_t state;          /* if the event is inactive or active */
    VQE_TAILQ_ENTRY(vqec_event_) event_queue_entry;
                                /* Event queue entry */
    VQE_RB_ENTRY(vqec_event_) tree_entry;
                                /* Red-black timer tree entry */
};

static vqec_event_state_t s_vqec_event_state = VQEC_EVENT_STATE_STOPPED;
static vqec_event_base_t s_event_base_mem;
static vqec_event_base_t *s_event_base_ptr;
#define VQEC_EVENT_JITTER_HISTOGRAM_BUCKETS 8
static char s_event_jitter_hist_mem[
                    sizeof(vam_hist_type_t) + 
                    sizeof(vam_hist_bucket_t) * 
                    VQEC_EVENT_JITTER_HISTOGRAM_BUCKETS];
static int32_t s_event_task_tid;
static struct task_struct *s_vqec_event_task = NULL;
static boolean s_task_run, s_task_stop; 
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,15)
DEFINE_MUTEX(g_vqec_dp_lock);
#else
DECLARE_MUTEX(g_vqec_dp_lock);
#endif
static const char s_task_is_invalid[] = "event loop task not found"; 

/* Internal benchmarking functions */
static inline void benchmark_start_sample (void);
static inline void benchmark_stop_sample (void);
static inline void benchmark_init (void);

#define VQEC_EV_COMPARE_GT 1
#define VQEC_EV_COMPARE_LT -1
#define VQEC_EV_COMPARE_EQ 0
/**---------------------------------------------------------------------------
 * Compare event timetamps for RB tree ordered walk.
 *
 * @param[in] a Pointer to the 1st event.
 * @param[in] b Pointer to the 2nd event.
 * @param[out] int32_t Returns GT, LT or EQ based on comparison of
 * timestamps between 2 different events. If timestamps are equal, then the
 * RB-tree order is determined by the pointer values themselves.
 *---------------------------------------------------------------------------*/
static int32_t
vqec_compare_time (vqec_event_t *a, vqec_event_t *b)
{
    if (TIME_CMP_A(lt, a->exp_time, b->exp_time)) {
        return (VQEC_EV_COMPARE_LT);
    } else if (TIME_CMP_A(gt, a->exp_time, b->exp_time)) {
        return (VQEC_EV_COMPARE_GT);
    } else if (a < b) {
        return (VQEC_EV_COMPARE_LT);
    } else if (a > b) {
        return (VQEC_EV_COMPARE_GT);
    }

    return (VQEC_EV_COMPARE_EQ);
}


/**---------------------------------------------------------------------------
 * Generate RB tree function definitions for the timer-tree.
 *---------------------------------------------------------------------------*/
VQE_RB_GENERATE(vqec_time_tree, vqec_event_, tree_entry, vqec_compare_time);


/**---------------------------------------------------------------------------
 * Insert an event into the event list. The event is not inserted into the
 * time-tree at this point.
 *
 * @param[in] baseptr Pointer to the event base. 
 * @param[in] evptr Pointer to the event.
 *---------------------------------------------------------------------------*/
static inline void
vqec_event_base_insert (vqec_event_base_t *baseptr, vqec_event_t *evptr) 
{
    ASSERT(baseptr != NULL);
    ASSERT(evptr->state == VQEC_EVENT_STATE_NONE);

    VQE_TAILQ_INSERT_TAIL(&baseptr->event_queue, 
                          evptr, 
                          event_queue_entry);
    evptr->state = VQEC_EVENT_STATE_INACTIVE;
    baseptr->num_events++;
}


/**---------------------------------------------------------------------------
 * Insert an event into the RB tree. The event must be on the tail queue
 * prior to insertion.
 *
 * @param[in] baseptr Pointer to the event base. 
 * @param[in] evptr Pointer to the event.
 *---------------------------------------------------------------------------*/
static inline void
vqec_event_base_activate (vqec_event_base_t *baseptr, vqec_event_t *evptr)
{
    vqec_event_t *tmp;
    
    ASSERT(baseptr != NULL);
    ASSERT(evptr->state == VQEC_EVENT_STATE_INACTIVE);

    tmp = VQE_RB_INSERT(vqec_time_tree, &baseptr->time_tree, evptr);
    ASSERT(tmp == NULL);  
    evptr->state = VQEC_EVENT_STATE_ACTIVE;
    baseptr->active_events++;
}


/**---------------------------------------------------------------------------
 * If an event is on the RB tree, remove it from the tree. If the event 
 * was not activated prior to invocation of this API, the method returns
 * false, otherwise returns true.
 *
 * @param[in] baseptr Pointer to the event base. 
 * @param[in] evptr Pointer to the event.
 * @param[out] boolean Returns true if the event is present on the time-tree.
 *---------------------------------------------------------------------------*/
static inline boolean
vqec_event_base_deactivate (vqec_event_base_t *baseptr, vqec_event_t *evptr)
{
    vqec_event_t *tmp;
    boolean rv = TRUE;

    ASSERT(baseptr != NULL);

    if (likely(evptr->state == VQEC_EVENT_STATE_ACTIVE)) {
        tmp = VQE_RB_REMOVE(vqec_time_tree, &baseptr->time_tree, evptr);
        ASSERT(tmp != NULL);  
        evptr->state = VQEC_EVENT_STATE_INACTIVE;
        baseptr->active_events--;
    } else {
        rv = FALSE;
   }

    return (rv);
}


/**---------------------------------------------------------------------------
 * Remove an event from the event list. The event must be deactivated
 * prior to invocation of this method.
 *
 * @param[in] baseptr Pointer to the event base. 
 * @param[in] evptr Pointer to the event.
 *---------------------------------------------------------------------------*/
static inline void
vqec_event_base_remove (vqec_event_base_t *baseptr, vqec_event_t *evptr) 
{
    ASSERT(baseptr != NULL);
    ASSERT(evptr->state == VQEC_EVENT_STATE_INACTIVE);

    VQE_TAILQ_REMOVE(&baseptr->event_queue, 
                     evptr, 
                     event_queue_entry);
    evptr->state = VQEC_EVENT_STATE_NONE;
    baseptr->num_events--;     
}


/**---------------------------------------------------------------------------
 * Create a new event object; a pointer to the created object is returned
 * in *evptrptr.  The event is *not* put on the active timer list.
 *
 * @param[out] *evptrptr Pointer to the newly created event.
 * @param[in] type Type of event to be created: *only* timer events are
 * supported in the kernel abstraction.
 * @param[in] events Event flags: *ignored* for the kernel abstraction.
 * @param[in] evh Event callback function.
 * @param[in] fd File descriptor for the event: *must* be equal to the
 * constant file-descriptor used for timers.
 * @param[in] dataptr Object pointer used as an input argument when
 * the callback is invoked.
 * @param[out] boolean Returns true if the event creation succeeds,
 * false if it fails. 
 *---------------------------------------------------------------------------*/
boolean 
vqec_event_create (vqec_event_t **evptrptr,
                   vqec_event_evtypes_t type,
                   int32_t events,
                   void (*evh)(const vqec_event_t * const, int32_t,
                               int16_t, void *),
                   int32_t fd,
                   void *dataptr)
{
    int32_t persist = events & (VQEC_EV_ONESHOT | VQEC_EV_RECURRING);

    if (unlikely(!evptrptr || 
                 !evh ||
                 type != VQEC_EVTYPE_TIMER ||
                 (persist != VQEC_EV_ONESHOT &&
                  persist != VQEC_EV_RECURRING) ||
                 fd != VQEC_EVDESC_TIMER)) { 
        VQEC_EVENT_LOG(KERN_ERR,
                       "%s Invalid arguments (%p %p %d %d %d)",
                       __FUNCTION__, 
                       evptrptr, evh, type, events, fd);
        return (FALSE);
    }
    
    *evptrptr = (vqec_event_t *)zone_acquire(s_event_base_ptr->event_pool);
    if (unlikely(! *evptrptr)) {
        VQEC_EVENT_LOG(KERN_ERR,
                       "%s Allocation from zone failed",
                       __FUNCTION__);
        return (FALSE);
    }
   
    memset(*evptrptr, 0, sizeof(**evptrptr));
    (*evptrptr)->refcnt++;
    (*evptrptr)->type = type;
    (*evptrptr)->persist = 
        events & VQEC_EV_RECURRING ? TRUE : FALSE;
    (*evptrptr)->dptr = dataptr; 
    (*evptrptr)->userfunc = evh;
    vqec_event_base_insert(s_event_base_ptr, *evptrptr);
    
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Helper to activate an event.
 *
 * @param[in] baseptr Pointer to the event base. 
 * @param[in] evptr Pointer to the event.
 * @param[in] tv Timeout for the timer event.
 *---------------------------------------------------------------------------*/
static inline void
vqec_event_start_internal (vqec_event_base_t *baseptr, 
                           vqec_event_t *evptr, struct timeval *tv)
{
    evptr->timeout = *tv;
    evptr->exp_time = TIME_ADD_A_R(get_sys_time(), 
                                   timeval_to_rel_time(&evptr->timeout));
    vqec_event_base_activate(baseptr, evptr);
}


/**---------------------------------------------------------------------------
 * Put at event on the active list. For kernel, only timer events are
 * supported thus a time-period must be specified. A timeout of 0 is allowed
 * only in one-shot mode to prevent unnecessary thrashing of events.
 *
 * @param[in] evptr Pointer to the event
 * @param[in] tv Timeout for the timer event.
 * @param[out] Returns true if the event was successfully added to the
 * active event list, false otherwise.
 *---------------------------------------------------------------------------*/
#define TIMERISSET(x) ((x)->tv_sec || (x)->tv_usec)
boolean 
vqec_event_start (const vqec_event_t *const evptr,
                  struct timeval * tv) 
{
    if (unlikely(!evptr || 
                 evptr->refcnt != 1 ||
                 evptr->type != VQEC_EVTYPE_TIMER ||
                 !tv ||
                 (!TIMERISSET(tv) && evptr->persist))) {
        VQEC_EVENT_LOG(KERN_ERR,
                       "%s Invalid input arguments (%d %d %ld %ld)",
                       __FUNCTION__, 
                       evptr ? evptr->refcnt : -1, evptr ? evptr->type : -1,
                       tv ? tv->tv_sec : -1, tv ? tv->tv_usec : -1);
        return (FALSE);
    }

    vqec_event_start_internal(s_event_base_ptr, (vqec_event_t *)evptr, tv);
                              
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Stop a previously started event. Applies only to timers for the kernel.
 * If the object was not started the call will indicate failure. This is not
 * considered fatal in this implementation; a message is logged.
 *
 * @param[in] evptr Pointer to the event
 * @param[out] Returns true if the event had been started prior to
 * invoking stop; false otherwise.
 *---------------------------------------------------------------------------*/
boolean
vqec_event_stop (const vqec_event_t *const evptr)
{
    if (unlikely(!evptr || 
                 evptr->refcnt != 1 ||
                 evptr->type != VQEC_EVTYPE_TIMER)) {
        VQEC_EVENT_LOG(KERN_ERR,
                       "%s Invalid input arguments (%d %d)",
                       __FUNCTION__,
                       evptr ? evptr->refcnt : -1, evptr ? evptr->type : -1);
        return (FALSE);
    }
    
    return (vqec_event_base_deactivate(s_event_base_ptr, 
                                       (vqec_event_t *)evptr));
}


/**---------------------------------------------------------------------------
 * Destroy an existing event object in *evptrptr. The memory allocated
 * for the object is returned, and *evptrptr is set to NULL upon return.
 * If the object is NULL this is not considered fatal in this implementation;
 * a message is logged.
 *
 * @param[in] *evptrptr Pointer to the event
 *---------------------------------------------------------------------------*/
void
vqec_event_destroy (vqec_event_t **evptrptr) 
{
    if (unlikely(!evptrptr || 
                 !*evptrptr  ||
                 (*evptrptr)->refcnt != 1)) {
        VQEC_EVENT_LOG(KERN_ERR,
                       "%s Invalid input arguments (%d %d)",
                       __FUNCTION__, 
                       (evptrptr && *evptrptr) ? (*evptrptr)->refcnt : -1, 
                       (evptrptr && *evptrptr) ? (*evptrptr)->type : -1);
        return;
    }
        
    (void)vqec_event_stop(*evptrptr);
    vqec_event_base_remove(s_event_base_ptr, *evptrptr);
    zone_release(s_event_base_ptr->event_pool, *evptrptr);
    *evptrptr = NULL; 
}

/**---------------------------------------------------------------------------
 * Adjust the expire time for every event in the timer tree.  The expire time
 * for each event is adjusted by adding the value of the offset.
 *
 * @param[in] baseptr Pointer to the event base.
 * @param[in] offset The relative time offset by which to adjust each event.
 *---------------------------------------------------------------------------*/
static void
vqec_event_tree_adjust_timeouts (vqec_event_base_t *baseptr,
                                 rel_time_t offset)
{
    vqec_event_t *evptr;

    ASSERT(baseptr != NULL);

    /* add offset to the exp_time of each event in the tree */
    for (evptr = VQE_RB_MIN(vqec_time_tree, &baseptr->time_tree);
         evptr;
         evptr = VQE_RB_NEXT(vqec_time_tree, &baseptr->time_tree, evptr)) {
        evptr->exp_time = TIME_ADD_A_R(evptr->exp_time, offset);
    }
}

#define VQEC_EVENT_DEFAULT_WAKEUP MSECS(20)
#define VQEC_EV_TYPE_TIMEOUT 0x1
/**---------------------------------------------------------------------------
 * Process active events on the timer tree. All events which have expired
 * are removed from the tree. If the events are recurring, they are added
 * back onto the timer tree. The callback function is invoked for all
 * expired events.
 *
 * @param[in] baseptr Pointer to the event base. 
 * @param[out] rel_time_t The relative time after which the next event
 * with the smallest deadline will expire.
 *---------------------------------------------------------------------------*/
static rel_time_t
vqec_event_tree_process (vqec_event_base_t *baseptr)
{
    vqec_event_t *evptr, *next;
    static abs_time_t prev_wakeup = ABS_TIME_0;
    static boolean prev_wakeup_valid = FALSE;
    abs_time_t now;
    rel_time_t next_wakeup = VQEC_EVENT_DEFAULT_WAKEUP;
    rel_time_t offset;  /* negative when time jumps backwards */

    ASSERT(baseptr != NULL);
    now = get_sys_time();

    /* detect if time has gone backwards */
    if (TIME_CMP_A(gt, prev_wakeup, now)) {
        /* time going backwards; adjust events accordingly */
        offset = TIME_SUB_A_A(now, prev_wakeup);
        vqec_event_tree_adjust_timeouts(baseptr, offset);
    }
    prev_wakeup = now;
    prev_wakeup_valid = TRUE;

    for (evptr = VQE_RB_MIN(vqec_time_tree, &baseptr->time_tree);
         evptr; evptr = next) {
        
        if (TIME_CMP_A(gt, evptr->exp_time, now)) {
            next_wakeup = TIME_SUB_A_A(evptr->exp_time, now);
            break;
        }
        next = VQE_RB_NEXT(vqec_time_tree, &baseptr->time_tree, evptr);
        (void)vqec_event_base_deactivate(baseptr, evptr);
        if (likely(baseptr->hist)) {
            (void)vam_hist_add(baseptr->hist, 
                               TIME_GET_R(msec, 
                                          TIME_SUB_A_A(now, evptr->exp_time)));
        }
        if (evptr->persist && (evptr->state == VQEC_EVENT_STATE_INACTIVE)) {
            vqec_event_start_internal(baseptr, evptr, &evptr->timeout);
        }
        if (evptr->userfunc) {
            (*evptr->userfunc)(evptr, VQEC_EVDESC_TIMER, 
                               VQEC_EV_TYPE_TIMEOUT, evptr->dptr); 
        }
    }
    
    return (next_wakeup);
}



/**---------------------------------------------------------------------------
 * A helper to determine if the event task loop should exit. It checks to see
 * if a SIGKILL has been posted for the task or if the user has explicitly
 * decided to stop the task.
 *
 * @param[out] boolean Returns true if the task should exit, false otherwise.
 *---------------------------------------------------------------------------*/
static inline boolean
vqec_event_loop_should_exit (void)
{
    boolean rv = FALSE;

    if (signal_pending(current)) {
        /* SIGKILL. */
        VQEC_EVENT_LOG(KERN_INFO,
                       "Received SIGKILL - terminating input loop");
        flush_signals(current);
        rv = TRUE;
    } else if (s_task_stop) {
        /* Instructed to stop the thread. */
        VQEC_EVENT_LOG(KERN_INFO,
                       "Explicit stop received - terminating input loop");
        rv = TRUE;
    } 

    return (rv);
}


/**---------------------------------------------------------------------------
 * Main timer task. The default wakeup for the task is DEFAULT_WAKEUP
 * msecs. After wakeup the task detects which events have expired
 * and invokes their callback methods.
 * 
 * @param[in] complete_arg Pointer to struct completion to signal that the
 * task has been successfully created.
 * @param[out] int32_t Exit status code [0].
 *---------------------------------------------------------------------------*/
static int32_t
vqec_event_loop (void *complete_arg)
{
    rel_time_t next_wakeup = VQEC_EVENT_DEFAULT_WAKEUP;
    struct completion *sig_complete = (struct completion *)complete_arg;

    /* 
     * -- reparent task to init().
     * -- accept SIGKILL.
     * -- signal parent to notify of thread startup. 
     */
    daemonize("vqec_event_loop");
    allow_signal(SIGKILL);
    __set_current_state(TASK_INTERRUPTIBLE);
    s_vqec_event_state = VQEC_EVENT_STATE_RUNNING;
    s_vqec_event_task = current;
    complete(sig_complete);
    

    for (; ;) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
        msleep(TIME_GET_R(msec, next_wakeup));
#else
        msleep_interruptible(TIME_GET_R(msec, next_wakeup));
#endif

        if (vqec_event_loop_should_exit()) {
            break;
        } else if (!s_task_run) {
            continue;
        }

        /* Acquire and release global lock around the processing loop. */
        vqec_lock_lock(g_vqec_dp_lock);

        benchmark_start_sample();

        /* check again after acquiring lock to be sure nothing has changed */
        if (vqec_event_loop_should_exit()) {
            vqec_lock_unlock(g_vqec_dp_lock);
            break;
        } else if (!s_task_run) {
            vqec_lock_unlock(g_vqec_dp_lock);
            continue;
        }

        next_wakeup = vqec_event_tree_process(s_event_base_ptr);
        if (TIME_CMP_R(gt, next_wakeup, 
                       VQEC_EVENT_DEFAULT_WAKEUP)) {
            next_wakeup = VQEC_EVENT_DEFAULT_WAKEUP;
        }

        benchmark_stop_sample();

        vqec_lock_unlock(g_vqec_dp_lock);
    }

    s_event_task_tid = -1;      /* invalidate the tid */
    s_vqec_event_task = NULL;   /* clear the cached task pointer */
    s_vqec_event_state = VQEC_EVENT_STATE_STOPPED;

    return (0);
}


/**---------------------------------------------------------------------------
 * De-initialize the event library. All resource acquired by the library
 * are released; this method must be called AFTER the event loop task
 * has been stopped.
 *---------------------------------------------------------------------------*/
void
vqec_dp_event_deinit (void)
{
    if (!s_event_base_ptr) {
        return;
    }

    if (s_event_base_ptr->num_events) {
        VQEC_EVENT_LOG(KERN_ERR,
                       "%s event list is not empty", __FUNCTION__);
    } else {
        if (s_event_base_ptr->hist) {
            s_event_base_ptr->hist = NULL;
        }
        if (s_event_base_ptr->event_pool) {
            zone_instance_put(s_event_base_ptr->event_pool);
        }
        s_event_base_ptr = NULL;
    }
}



/**---------------------------------------------------------------------------
 * Initialize the event library with an event pool, i.e., events will be 
 * allocated from the event pool [supported only for kernel compiles].
 * 
 * @param[in] max Maximum number of events to be supported.
 * @param[out] boolean Returns true if the event API is successfully 
 * initialized, otherwise returns false.
 *---------------------------------------------------------------------------*/
boolean 
vqec_dp_event_init (uint32_t max)
{ 
    boolean rv = FALSE;

    if (s_event_base_ptr != NULL) {
        rv = TRUE;
        goto done;
    }
    s_event_base_ptr = &s_event_base_mem;
    if (!s_event_base_ptr) {
        VQEC_EVENT_LOG(KERN_ERR,
                       "%s event base memory failure", __FUNCTION__); 
        goto done;
    }
    memset(s_event_base_ptr, 0, sizeof(*s_event_base_ptr));

    s_event_base_ptr->hist = (vam_hist_type_t *)s_event_jitter_hist_mem; 
    if (!s_event_base_ptr->hist) {
        VQEC_EVENT_LOG(KERN_ERR,
                       "%s histogram memory failure", __FUNCTION__);
        goto done; 
    }
    if (vam_hist_create(s_event_base_ptr->hist, 
                        VQEC_EVENT_JITTER_HISTOGRAM_BUCKETS,
                        VQEC_EVENT_JITTER_HISTOGRAM_BUCKETS - 1,
                        0,
                        "event-loop-hist",
                        FALSE)) {
        VQEC_EVENT_LOG(KERN_ERR,
                       "%s histogram creation failure", __FUNCTION__);
        goto done;
    }

    s_event_base_ptr->event_pool = zone_instance_get_loc("vqec_evtpool",
                                                         0,
                                                         sizeof(vqec_event_t),
                                                         max,
                                                         NULL,
                                                         NULL);
    if (!s_event_base_ptr->event_pool) {
        VQEC_EVENT_LOG(KERN_ERR,
                       "%s zone creation failure", __FUNCTION__);
        goto done;
    }

    VQE_TAILQ_INIT(&s_event_base_ptr->event_queue);
    VQE_RB_INIT(&s_event_base_ptr->time_tree);
    rv = TRUE;

    benchmark_init();

  done:
    if (!rv) {
        vqec_dp_event_deinit();
    }

    return (rv);    
}


/**---------------------------------------------------------------------------
 * Create the event loop task. The method waits for the task to startup,
 * and reparent, then returns. The task will not process any events until
 * dispatch() is invoked by the user.
 *
 * @param[out] boolean Returns true if the thread is created successfully,
 * otherwise returns false.
 *---------------------------------------------------------------------------*/
boolean 
vqec_dp_event_loop_enter (void) 
{
    struct completion wait_start_complete;

    if (s_vqec_event_state != VQEC_EVENT_STATE_STOPPED) {
        VQEC_EVENT_LOG(KERN_ERR, "event loop is not stopped; cannot start\n");
        return (FALSE);
    }

    s_task_run = 
        s_task_stop = FALSE;
    init_completion(&wait_start_complete);
    s_event_task_tid = kernel_thread(vqec_event_loop, 
                                       &wait_start_complete, SIGCHLD);
    if (s_event_task_tid < 0) {
        VQEC_EVENT_LOG(KERN_ERR,
                       "kernel timer task creation failed");
        return (FALSE);
    } 
    wait_for_completion(&wait_start_complete);
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Stop the event loop task.  This is asynchronous.
 *
 * @param[out] boolean If the task was successfully signalled to stop, returns
 *                     true; otherwise returns false.
 *---------------------------------------------------------------------------*/
boolean 
vqec_dp_event_loop_exit (void) 
{
    if (s_event_task_tid < 0) {
        VQEC_EVENT_LOG(KERN_ERR, "%s", s_task_is_invalid);
        return (FALSE);
    }
    if (s_vqec_event_state != VQEC_EVENT_STATE_RUNNING) {
        VQEC_EVENT_LOG(KERN_ERR, "event loop is not running; cannot stop\n");
        return (FALSE);
    }
    s_vqec_event_state = VQEC_EVENT_STATE_EXITING;
    s_task_stop = TRUE;

    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Run the timer loop of the event loop task.  This method must be
 * invoked *AFTER* vqec_event_loop_enter has been called.
 *
 * @param[out] boolean Returns true if a wakeup was successfully posted
 * to the event loop task, false otherwise. 
 *---------------------------------------------------------------------------*/
boolean 
vqec_dp_event_loop_dispatch (void) 
{
    boolean rv = TRUE;

    if (s_event_task_tid < 0) {
        VQEC_EVENT_LOG(KERN_ERR, "%s", s_task_is_invalid);
        return (FALSE);
    }
    if (s_vqec_event_task && !IS_ERR(s_vqec_event_task)) {
        s_task_run = TRUE;
        wake_up_process(s_vqec_event_task);
    } else {
        VQEC_EVENT_LOG(KERN_ERR, "%s", s_task_is_invalid);
        rv = FALSE;
    }

    return (rv);
}


/**---------------------------------------------------------------------------
 * Helper: Display the event jitter histogram to the console.
 *---------------------------------------------------------------------------*/
void
vqec_dp_event_display_hist (void)
{
    if (s_event_base_ptr &&
        s_event_base_ptr->hist) {
        vam_hist_display_nonzero_hits(s_event_base_ptr->hist);
    }
}

/*----------------------------------------------------------------------------
 * Get the current state of the vqec_dp_event library.
 *----------------------------------------------------------------------------*/
vqec_event_state_t vqec_dp_event_get_state (void)
{
    return (s_vqec_event_state);
}

/*----------------------------------------------------------------------------
 * Sleep for a given number of milliseconds.
 *----------------------------------------------------------------------------*/
void vqec_dp_event_sleep (uint32_t sleepms)
{
    msleep(sleepms);
}

/**--------------------------------------------------------------------------
 *  Benchmarking Logic
 *--------------------------------------------------------------------------*/
#include <linux/time.h>
#include <asm/div64.h>

#define MICROS_PER_SEC 1000000
#define SAMPLE_TIME 5           /* 5 Second Sample  */
#define MAX_SAMPLES 60          /* Max 5 minutes    */
#define PRECISION 1000          /* .001 precision   */
#define CALIBRATION_TIME 10     /* 10ms calibration */

#define MIN(_x,_y) ((_x) < (_y) ? (_x) : (_y))
#define MAX(_x,_y) ((_x) > (_y) ? (_x) : (_y))

/* Benchmarking statics */
static uint64_t start_time, start_cycles, sample_cycles;
static uint32_t samples[MAX_SAMPLES];
static uint32_t sample_index;
static uint64_t clk_rate_MHz;

static inline uint64_t udiv64(uint64_t x, uint64_t y)
{
    uint64_t rv;
    rv = x;
    do_div(rv, MAX(y,1));
    return rv;
}

static inline uint64_t get_time_in_micros(void) 
{
    uint64_t micros_time;
    struct timeval tv;
    do_gettimeofday(&tv);
    micros_time = tv.tv_sec;
    micros_time *= MICROS_PER_SEC;
    micros_time += tv.tv_usec;
    return micros_time;
}

static inline uint64_t get_current_cycle_count (void) 
{
#ifdef CONFIG_MIPS
    return read_c0_count();
#else
    return get_time_in_micros();
#endif 
}

static inline void benchmark_calibrate (void) 
{
    uint64_t start_time = 0, end_time = 0;
    uint64_t start_cycles, end_cycles;

    start_time = get_time_in_micros();
    start_cycles = get_current_cycle_count();
    msleep(CALIBRATION_TIME);
    end_time = get_time_in_micros();
    end_cycles = get_current_cycle_count();

    clk_rate_MHz = udiv64(end_cycles - start_cycles, end_time - start_time);
}

static inline void benchmark_start_sample (void) 
{
    uint64_t total_cycles, curr_time;
    curr_time = get_time_in_micros();
    if (!start_time) {
        start_time = curr_time;
    } else if ((curr_time - start_time) >= (SAMPLE_TIME * MICROS_PER_SEC)) {
        total_cycles = (curr_time - start_time) * clk_rate_MHz;
        if (sample_cycles < total_cycles) {
            sample_index = (sample_index + 1) % MAX_SAMPLES;
            samples[sample_index] = udiv64(sample_cycles, 
                                        udiv64(total_cycles, PRECISION));
        }
        start_time = curr_time;
        sample_cycles = 0;
    }
    start_cycles = get_current_cycle_count();
}

static inline void benchmark_stop_sample (void) 
{
    if (start_time) {
        sample_cycles += (get_current_cycle_count() - start_cycles);
    }
}

static inline void benchmark_init (void) 
{
    start_time = 0;
    start_cycles = 0;
    sample_cycles = 0;
    memset(samples, 0, sizeof(samples));
    sample_index = 0;
    clk_rate_MHz = 1;

    benchmark_calibrate();
}

uint32_t vqec_dp_event_get_benchmark (uint32_t interval) 
{
    uint32_t start_index, i, num_samples, total;

    start_index = sample_index;
    num_samples = MAX(1, MIN(interval / SAMPLE_TIME, MAX_SAMPLES));
    total = 0;

    for (i = 0; i < num_samples; i++) {
        total += samples[(start_index - i) % MAX_SAMPLES];
    }

    /* Note: For intervals greater than the sample size, this
     *       function's return value is marginally inprecise.
     *       We assume all samples to be of equal weight.
     */
    return total / num_samples;
}

