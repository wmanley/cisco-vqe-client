/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_sink.h
 *
 * Description: VQEC "sink" abstraction implementation.
 *
 * Documents:
 *
 *****************************************************************************/

#ifndef __VQEC_SINK_H__
#define __VQEC_SINK_H__

#include "vqec_sink_api.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" 
{
#endif /* __cplusplus */

/**
 * Allows waiting thread to be woken-up by sinks.
 */
typedef 
struct vqec_sink_waiter_
{
    /**
     * Condition variable used for thread synchronization.
     */
    pthread_cond_t  cond;
    /**
     * IO buffer type.
     */
    vqec_iobuf_t *iobuf;
    /**
     * Number of buffers in array which should be received 
     * before signaling the condition variable.
     */
    int32_t         buf_cnt;
    /**
     * Current buffer index in the array.
     */
    int32_t         i_cur;
    /**
     * Total received byte length.
     */
    int32_t         len;
    /**
     * Waiter list for a particular channel.
     */
    VQE_TAILQ_ENTRY(vqec_sink_waiter_t_) w_chain;

} vqec_sink_waiter_t;

#define VQEC_SINK_WAITER_METHODS                                    \
    /**                                                             \
     *  Add a new waiter to the sink.                               \
     */                                                             \
    void (*vqec_sink_add_waiter)(VQEC_SINK_INSTANCE,                \
                                 struct vqec_sink_waiter_ *waiter); \
    /**                                                             \
     *  Remove a new waiter from the sink.                          \
     */                                                             \
    void (*vqec_sink_del_waiter)(VQEC_SINK_INSTANCE,                \
                                 struct vqec_sink_waiter_ *waiter); \

#define VQEC_SINK_WAITER_MEMBERS                                    \
    /**                                                             \
     * Pointer to a waiter instance.                                \
     */                                                             \
    struct vqec_sink_waiter_ *waiter;                               \


#define __vqec_sink_fcns            \
    VQEC_SINK_METHODS_COMMON        \
    VQEC_SINK_WAITER_METHODS        \
    
#define __vqec_sink_members         \
    VQEC_SINK_MEMBERS_COMMON        \
    VQEC_SINK_WAITER_MEMBERS        \

DEFCLASS(vqec_sink);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __VQEC_SINK_H__
