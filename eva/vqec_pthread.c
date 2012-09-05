/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_pthread.c
 *
 * Description: Creation of threads for VQEC, including real-time threads.
 *
 * Documents:
 *
 *****************************************************************************/
#include <stdio.h>
#include <string.h>
#include "vqec_pthread.h"

#define VQEC_PRIO_THREAD_PRIORITY 1
#define VQEC_PRIO_THREAD_POLICY SCHED_RR

/******************************************************************************
 * Elevate a thread to  real-time. Returns 0 if elevation succeeds; otherwise 
 * returns the error from pthread_setschedparam.  The only likely error code
 * is EPERM, i.e., caller did not have sufficient permissions.
 ******************************************************************************/
int32_t vqec_pthread_set_priosched (pthread_t tid)
{
    struct sched_param sp;
    int32_t ret, policy;
    
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = VQEC_PRIO_THREAD_PRIORITY;

    ret = pthread_setschedparam(tid, VQEC_PRIO_THREAD_POLICY, &sp);
    if (ret) { 
        printf("=== Failed to elevate priority: (%s) ===\n", 
               strerror(ret));
    }

    if (!pthread_getschedparam(tid, &policy, &sp)) {
        printf("=== pthread (%u) policy (%s), prio (%d) ===\n",
               (uint32_t)tid, 
               (policy == SCHED_OTHER) ? "low" : "realtime", 
               sp.sched_priority);	
    }
    
    return (ret);
}

/******************************************************************************
 * Create a thread with default priority, without inheriting from the thread 
 * calling this function.
 *
 * Returns 0 on success; otherwise the failure codes are those returned by 
 * either pthread_attr_...() or pthread_create() calls. These are extremely 
 * unlikely, and in most cases signifiy something bad.
 ******************************************************************************/
int32_t vqec_pthread_create (pthread_t *tid, 
                             void *(*start_routine)(void *), void *arg)
{
    pthread_attr_t attr;
    struct sched_param sp;
    int32_t ret = 0, policy;
    size_t vqec_stack_size;

#ifdef USE_SMALL_VQEC_STACK
    vqec_stack_size = VQEC_SMALL_STACK;
#else
    vqec_stack_size = VQEC_NORMAL_STACK;
#endif

    memset(&sp, 0, sizeof(sp));
    if ((ret = pthread_attr_init(&attr)) ||
        (ret = pthread_attr_setschedpolicy(&attr, SCHED_OTHER)) ||
        (ret = pthread_attr_setschedparam(&attr, &sp)) ||
        (ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) ||
        (ret = pthread_attr_setstacksize(&attr, vqec_stack_size))) {        

        printf("=== Unable to set attributes for pthread (%s) ===\n",
               strerror(ret));
    } else if ((ret = pthread_create(tid, &attr, 
                                     start_routine, arg))) {
        
            printf("=== Unable to create pthread (%s) ===\n",
                   strerror(ret));
    } else {
        if (!pthread_getschedparam(*tid, &policy, &sp)) {
        	printf("=== pthread (%u) policy (%s), prio (%d) ===\n",
                       (uint32_t)*tid, 
                       (policy == SCHED_OTHER) ? "low" : "realtime", 
                       sp.sched_priority);	
        }
    }

    return (ret);
}

