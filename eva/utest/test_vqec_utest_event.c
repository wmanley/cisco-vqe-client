/*
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"

#undef _VQEC_UTEST_INTERPOSERS
#define _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"

#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include <sys/event.h>
#include "vqec_event.h"
#include <pthread.h>

#include "vqec_lock_defs.h"
#include "vqec_debug.h"

extern struct event_base *vqec_g_evbase;

//----------------------------------------------------------------------------
// Structure repeated! ugh, but don't want to write accessors! 
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

//----------------------------------------------------------------------------
// Unit tests for vqec_event
//----------------------------------------------------------------------------
extern struct event_base *vqec_g_evbase;

static vqec_event_t *test_event;
static int32_t test_data = 0;
static void 
test_vqec_event_handler (const vqec_event_t * const evptr, int32_t fd, 
                         int16_t ev, void *dptr)
{
    
}

#if 0
static struct timeval g_jitter_limit = {0}, g_last_tm = {0};
static int32_t g_timecnt, g_jitter, g_desc_read, g_desc_write, g_desc_timeout, 
    g_mutex_tfail;


static void 
test_vqec_timer_events (const vqec_event_t * const evptr, int32_t fd, 
                        int16_t ev, void *dptr)
{
    struct timeval *st = (struct timeval *)dptr;
    struct timeval tm, de;
    gettimeofday(&tm, NULL);
    if (!g_timecnt) {
        timersub(&tm, st, &de);
    } else {
        timersub(&tm, &g_last_tm, &de);
    }
    g_last_tm = tm;

    if (timercmp(&de, &g_jitter_limit, >)) {
        g_jitter++;
    }
    g_last_tm = tm;
    g_timecnt++;
    fprintf(stderr, "<< tm: %d %d>>\n\n", g_timecnt, g_jitter);
}

static void 
test_vqec_desc_events (const vqec_event_t * const evptr, int32_t fd, 
                       int16_t ev, void *dptr)
{
    char buf[256];
    if (ev & VQEC_EV_READ) {
        g_desc_read++;
        read(fd, buf, 256);
    }
    if (ev & VQEC_EV_WRITE) {
        write(fd, buf, 256);
        g_desc_write++;
    }
    if (ev & VQEC_EV_TIMEOUT) {
        g_desc_timeout++;
    }
    fprintf(stderr, "<< desc: %d %d>>\n\n", g_desc_read, g_desc_write);
}

static void 
test_vqec_event_locking (const vqec_event_t * const evptr, int32_t fd, 
                         int16_t ev, void *dptr)
{
    if (!pthread_mutex_trylock(&vqec_lockdef_vqec_g_lock.mutex)) {
        fprintf(stderr, "<< trylock >>\n\n");
        g_mutex_tfail++;
        return;
    }
    if (pthread_mutex_unlock(&vqec_lockdef_vqec_g_lock.mutex)) {
        fprintf(stderr, "<< unlock >>\n\n");
        g_mutex_tfail++;
        return;
    }
    if (pthread_mutex_lock(&vqec_lockdef_vqec_g_lock.mutex)) {
        fprintf(stderr, "<< lock >>\n\n");        
        g_mutex_tfail++;
        return;
    }
}
#endif

int test_vqec_event_init (void) 
{
    return 0;
}

int test_vqec_event_clean (void) 
{
    return 0;
}

#if 0
static void *test_vqec_evloop (void *d)
{
    vqec_event_dispatch();
    return (NULL);
}
#endif

static void test_vqec_event_libinit (void) 
{
    boolean st;

    if (vqec_g_evbase) {
        st = vqec_event_init();
        CU_ASSERT(st);
    } else {
        test_event_init_make_fail(1);
        st = vqec_event_init();
        test_event_init_make_fail(0);
        CU_ASSERT_FALSE(st);
    }

    st = vqec_event_init();
    CU_ASSERT(st);

    VQEC_SET_DEBUG_FLAG(VQEC_DEBUG_EVENT);
    st = vqec_event_init();
    CU_ASSERT(st);
    VQEC_RESET_DEBUG_FLAG(VQEC_DEBUG_EVENT);
}

static void test_vqec_event_dispatch (void) 
{
    boolean st;
    struct event_base *base;

    VQEC_SET_DEBUG_FLAG(VQEC_DEBUG_EVENT);
    test_event_dispatch_make_fail(1);
    st = vqec_event_dispatch();
    test_event_dispatch_make_fail(0);
    CU_ASSERT_FALSE(st);

    base = vqec_g_evbase;
    vqec_g_evbase = NULL;
    st = vqec_event_dispatch();
    CU_ASSERT_FALSE(st);
    vqec_g_evbase = base;

    st = vqec_event_dispatch();
    CU_ASSERT(st);
    VQEC_RESET_DEBUG_FLAG(VQEC_DEBUG_EVENT);
}

static void test_vqec_event_loopexit (void) 
{
    boolean st;
    struct event_base *base;

    test_event_loopexit_make_fail(1);
    st = vqec_event_loopexit(NULL);
    test_event_loopexit_make_fail(0);
    CU_ASSERT_FALSE(st);

    base = vqec_g_evbase;
    vqec_g_evbase = NULL;
    st = vqec_event_loopexit(NULL);
    CU_ASSERT_FALSE(st);
    vqec_g_evbase = base;

    st = vqec_event_loopexit(NULL);
    CU_ASSERT(st);
}

static void test_vqec_event_create (void) 
{
    boolean st;
    test_event = NULL;

    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_ONESHOT,
                           NULL, 
                           VQEC_EVDESC_TIMER,
                           &test_data);
    CU_ASSERT_FALSE(st);

    st = vqec_event_create(NULL, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_ONESHOT,
                           NULL, 
                           VQEC_EVDESC_TIMER,
                           &test_data);
    CU_ASSERT_FALSE(st);

    st = vqec_event_create(&test_event, 
                           100,
                           VQEC_EV_ONESHOT,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &test_data);
    CU_ASSERT_FALSE(st);

    st = vqec_event_create(&test_event,  
                           100,
                           0,
                           test_vqec_event_handler,  
                           VQEC_EVDESC_TIMER,
                           &test_data);
    CU_ASSERT_FALSE(st);

    st = vqec_event_create(&test_event, 
                           100,
                           VQEC_EV_ONESHOT,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &test_data);
    CU_ASSERT_FALSE(st);

    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           0,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &test_data);
    CU_ASSERT_FALSE(st);

    test_calloc_make_fail(1);
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_ONESHOT,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &test_data);
    test_calloc_make_fail(0);
    CU_ASSERT_FALSE(st);
    
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_ONESHOT,
                           test_vqec_event_handler, 
                           500,
                           &test_data);
    CU_ASSERT_FALSE(st);
    CU_ASSERT(test_event == NULL);

    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_ONESHOT | VQEC_EV_WRITE,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &test_data);
    CU_ASSERT_FALSE(st);
    CU_ASSERT(test_event == NULL);

    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_ONESHOT | VQEC_EV_WRITE | VQEC_EV_READ,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &test_data);
    CU_ASSERT_FALSE(st);
    CU_ASSERT(test_event == NULL);

    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_FD,
                           VQEC_EV_ONESHOT | VQEC_EV_READ,
                           test_vqec_event_handler, 
                           -1,
                           &test_data);
    CU_ASSERT_FALSE(st);
    CU_ASSERT(test_event == NULL);

    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_FD,
                           VQEC_EV_ONESHOT,
                           test_vqec_event_handler, 
                           100,
                           &test_data);
    CU_ASSERT_FALSE(st);
    CU_ASSERT(test_event == NULL);

//----------------------------------------------------------------------------
    test_data++;
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_ONESHOT,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &test_data);
    CU_ASSERT(st);
    CU_ASSERT(test_event != NULL);
    CU_ASSERT(test_event->type == VQEC_EVTYPE_TIMER);
    CU_ASSERT(test_event->events_rw == 0);
    CU_ASSERT(test_event->persist == VQEC_EV_ONESHOT);
    CU_ASSERT(test_event->fd == VQEC_EVDESC_TIMER);
    CU_ASSERT(test_event->userfunc == test_vqec_event_handler);
    CU_ASSERT(test_event->refcnt == 1);
    CU_ASSERT((*(int32_t *)test_event->dptr) == test_data);
    CU_ASSERT(test_event->ev.ev_events == 0);
    vqec_event_destroy(&test_event);

//----------------------------------------------------------------------------
    test_data++;
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_RECURRING,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &test_data);
    CU_ASSERT(st);
    CU_ASSERT(test_event != NULL);
    CU_ASSERT(test_event->type == VQEC_EVTYPE_TIMER);
    CU_ASSERT(test_event->events_rw == 0);
    CU_ASSERT(test_event->persist == VQEC_EV_RECURRING);
    CU_ASSERT(test_event->fd == VQEC_EVDESC_TIMER);
    CU_ASSERT(test_event->userfunc == test_vqec_event_handler);
    CU_ASSERT(test_event->refcnt == 1);
    CU_ASSERT((*(int32_t *)test_event->dptr) == test_data);
    CU_ASSERT(test_event->ev.ev_events == 0);
    CU_ASSERT(test_event->ev.ev_fd == -1);
    vqec_event_destroy(&test_event);

//----------------------------------------------------------------------------
    int32_t g_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    CU_ASSERT(g_sock != -1);
    test_data++;
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_FD,
                           VQEC_EV_ONESHOT | VQEC_EV_READ,
                           test_vqec_event_handler, 
                           g_sock,
                           &test_data);
    CU_ASSERT(st);
    CU_ASSERT(test_event != NULL);
    CU_ASSERT(test_event->type == VQEC_EVTYPE_FD);
    CU_ASSERT(test_event->events_rw == VQEC_EV_READ);
    CU_ASSERT(test_event->persist == VQEC_EV_ONESHOT);
    CU_ASSERT(test_event->fd == g_sock);
    CU_ASSERT(test_event->userfunc == test_vqec_event_handler);
    CU_ASSERT(test_event->refcnt == 1);
    CU_ASSERT((*(int32_t *)test_event->dptr) == test_data);
    CU_ASSERT(test_event->ev.ev_events == EV_READ);
    vqec_event_destroy(&test_event);

//----------------------------------------------------------------------------
    test_data++;
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_FD,
                           VQEC_EV_ONESHOT | VQEC_EV_READ | VQEC_EV_WRITE,
                           test_vqec_event_handler, 
                           g_sock,
                           &test_data);
    CU_ASSERT(st);
    CU_ASSERT(test_event != NULL);
    CU_ASSERT(test_event->type == VQEC_EVTYPE_FD);
    CU_ASSERT(test_event->events_rw == (VQEC_EV_READ | VQEC_EV_WRITE));
    CU_ASSERT(test_event->persist == VQEC_EV_ONESHOT);
    CU_ASSERT(test_event->fd == g_sock);
    CU_ASSERT(test_event->userfunc == test_vqec_event_handler);
    CU_ASSERT(test_event->refcnt == 1);
    CU_ASSERT((*(int32_t *)test_event->dptr) == test_data);
    CU_ASSERT(test_event->ev.ev_events == (EV_READ | EV_WRITE));
    vqec_event_destroy(&test_event);

//----------------------------------------------------------------------------
    test_data++;
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_FD,
                           VQEC_EV_RECURRING | VQEC_EV_READ,
                           test_vqec_event_handler, 
                           g_sock,
                           &test_data);
    CU_ASSERT(st);
    CU_ASSERT(test_event != NULL);
    CU_ASSERT(test_event->type == VQEC_EVTYPE_FD);
    CU_ASSERT(test_event->events_rw == VQEC_EV_READ);
    CU_ASSERT(test_event->persist == VQEC_EV_RECURRING);
    CU_ASSERT(test_event->fd == g_sock);
    CU_ASSERT(test_event->userfunc == test_vqec_event_handler);
    CU_ASSERT(test_event->refcnt == 1);
    CU_ASSERT((*(int32_t *)test_event->dptr) == test_data);
    CU_ASSERT(test_event->ev.ev_events == (EV_READ | EV_PERSIST));
    vqec_event_destroy(&test_event);

//----------------------------------------------------------------------------
    test_data++;
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_FD,
                           VQEC_EV_RECURRING | VQEC_EV_READ | VQEC_EV_WRITE,
                           test_vqec_event_handler, 
                           g_sock,
                           &test_data);
    CU_ASSERT(st);
    CU_ASSERT(test_event != NULL);
    CU_ASSERT(test_event->type == VQEC_EVTYPE_FD);
    CU_ASSERT(test_event->events_rw == (VQEC_EV_READ | VQEC_EV_WRITE));
    CU_ASSERT(test_event->persist == VQEC_EV_RECURRING);
    CU_ASSERT(test_event->fd == g_sock);
    CU_ASSERT(test_event->userfunc == test_vqec_event_handler);
    CU_ASSERT(test_event->refcnt == 1);
    CU_ASSERT((*(int32_t *)test_event->dptr) == test_data);
    CU_ASSERT(test_event->ev.ev_events == (EV_READ | EV_WRITE | EV_PERSIST));
    vqec_event_destroy(&test_event);
}

static void test_vqec_event_start (void) 
{
    boolean st;
    struct timeval tv;

    memset(&tv, 0, sizeof(tv));
    test_event = NULL;
    vqec_event_create(&test_event, 
                      VQEC_EVTYPE_TIMER,
                      VQEC_EV_ONESHOT,
                      test_vqec_event_handler, 
                      VQEC_EVDESC_TIMER,
                      &test_data);
    CU_ASSERT(test_event != NULL);
    
//----------------------------------------------------------------------------
    test_event_add_make_fail(1);
    st = vqec_event_start(test_event, &tv);
    test_event_add_make_fail(0);
    CU_ASSERT_FALSE(st);

    st = vqec_event_start(test_event, NULL);
    CU_ASSERT_FALSE(st);

    st = vqec_event_start(test_event, &tv);
    CU_ASSERT(st);
    CU_ASSERT(test_event->tv.tv_usec == tv.tv_usec);
    CU_ASSERT(test_event->tv.tv_sec == tv.tv_sec);

    vqec_event_destroy(&test_event);

//----------------------------------------------------------------------------
    vqec_event_create(&test_event, 
                      VQEC_EVTYPE_TIMER,
                      VQEC_EV_RECURRING,
                      test_vqec_event_handler, 
                      VQEC_EVDESC_TIMER,
                      &test_data);
    CU_ASSERT(test_event != NULL);
    st = vqec_event_start(test_event, &tv);
    CU_ASSERT_FALSE(st);

    st = vqec_event_start(test_event, NULL);
    CU_ASSERT_FALSE(st);

    tv.tv_sec = 0;
    tv.tv_usec = 1000 * 1000;
    st = vqec_event_start(test_event, &tv);    
    CU_ASSERT(st);

    CU_ASSERT(test_event->tv.tv_usec == tv.tv_usec);
    CU_ASSERT(test_event->tv.tv_sec == tv.tv_sec);
    st = vqec_event_stop(test_event);
    CU_ASSERT(st);

    vqec_event_destroy(&test_event);

//----------------------------------------------------------------------------
    int32_t g_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    CU_ASSERT(g_sock != -1);

    vqec_event_create(&test_event, 
                      VQEC_EVTYPE_FD,
                      VQEC_EV_ONESHOT | VQEC_EV_READ | VQEC_EV_WRITE,
                      test_vqec_event_handler, 
                      g_sock,
                      &test_data);
    CU_ASSERT(test_event != NULL);
    st = vqec_event_start(test_event, NULL);
    CU_ASSERT(st);
    CU_ASSERT(test_event->tv.tv_usec == 0);
    CU_ASSERT(test_event->tv.tv_sec == 0);

    st = vqec_event_start(test_event, &tv);
    CU_ASSERT_FALSE(st);

    vqec_event_destroy(&test_event);
}

static void test_vqec_event_stop (void) 
{
    boolean st;
    struct timeval tv;

    memset(&tv, 0, sizeof(&tv));
    tv.tv_sec = 100;

    vqec_event_create(&test_event, 
                      VQEC_EVTYPE_TIMER,
                      VQEC_EV_RECURRING,
                      test_vqec_event_handler, 
                      VQEC_EVDESC_TIMER,
                      &test_data);
    tv.tv_sec = 0;
    tv.tv_usec = 100 * 1000;
    st = vqec_event_start(test_event, &tv);
    CU_ASSERT(st);

    test_event_del_make_fail(1);
    st = vqec_event_stop(test_event);
    test_event_del_make_fail(0);
    CU_ASSERT_FALSE(st);

    st = vqec_event_stop(test_event);
    CU_ASSERT(st);

    test_event->refcnt = 0;
    st = vqec_event_stop(NULL);
    CU_ASSERT_FALSE(st);
    test_event->refcnt = 1;

    vqec_event_destroy(&test_event);
}

static void test_vqec_event_destroy (void) 
{
    boolean st;
    struct timeval tm, tv;

    VQEC_SET_DEBUG_FLAG(VQEC_DEBUG_EVENT);

    test_event = NULL;
    vqec_event_destroy(&test_event);
    CU_PASS();

    gettimeofday(&tm, NULL);
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_ONESHOT,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &tm);
    CU_ASSERT(st);

    test_event->refcnt = 0;
    vqec_event_destroy(&test_event);
    CU_PASS();
    test_event->refcnt = 1;
    vqec_event_destroy(&test_event);
    CU_ASSERT(test_event == NULL);
    
    gettimeofday(&tm, NULL);
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_ONESHOT,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &tm);
    CU_ASSERT(st);
    vqec_event_destroy(&test_event);
    CU_ASSERT(test_event == NULL);


    tv.tv_sec = 0;
    tv.tv_usec = 100 * 1000;
    gettimeofday(&tm, NULL);
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_RECURRING,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &tm);
    CU_ASSERT(st);
    st = vqec_event_start(test_event, &tv);
    CU_ASSERT(st);
    vqec_event_destroy(&test_event);
    CU_ASSERT(test_event == NULL);



    tv.tv_sec = 0;
    tv.tv_usec = 100 * 1000;
    gettimeofday(&tm, NULL);
    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_RECURRING,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &tm);
    CU_ASSERT(st);
    st = vqec_event_start(test_event, &tv);
    CU_ASSERT(st);
    st = vqec_event_stop(test_event);
    CU_ASSERT(st);
    vqec_event_destroy(&test_event);
    CU_ASSERT(test_event == NULL);

    VQEC_RESET_DEBUG_FLAG(VQEC_DEBUG_EVENT);
}

extern void vqec_g_ev_handler (int32_t fd, int16_t events, void *dptr);
static void test_vqec_g_ev_handler (void) 
{
    boolean st;
    struct timeval tm;

    test_event = NULL;
    gettimeofday(&tm, NULL);

    st = vqec_event_create(&test_event, 
                           VQEC_EVTYPE_TIMER,
                           VQEC_EV_RECURRING,
                           test_vqec_event_handler, 
                           VQEC_EVDESC_TIMER,
                           &tm);
    CU_ASSERT(st);

    vqec_g_ev_handler(-1, 0, NULL);
    CU_PASS();

    test_event->refcnt = 0;
    vqec_g_ev_handler(-1, 0, test_event);
    test_event->refcnt = 1;
    CU_PASS();

    test_event->tv = tm;
    test_event_add_make_fail(1);
    vqec_g_ev_handler(-1, 0, test_event);
    test_event_add_make_fail(0);
    CU_PASS();

    vqec_g_ev_handler(-1, 0, test_event);
    CU_PASS();

    vqec_event_destroy(&test_event);
}

CU_TestInfo test_array_event[] = {
    {"test vqec_event_libinit",test_vqec_event_libinit},
    {"test vqec_event_dispatch",test_vqec_event_dispatch},
    {"test vqec_event_loopexit",test_vqec_event_loopexit},
    {"test vqec_event_create",test_vqec_event_create},
    {"test vqec_event_start",test_vqec_event_start},
    {"test vqec_event_stop",test_vqec_event_stop},
    {"test vqec_event_destroy",test_vqec_event_destroy},
    {"test vqec_g_ev_handler",test_vqec_g_ev_handler},
    CU_TEST_INFO_NULL,
};

