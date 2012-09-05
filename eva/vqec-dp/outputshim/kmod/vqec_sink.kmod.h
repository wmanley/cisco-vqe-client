/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
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
#include "linux/net.h"
#include "linux/wait.h"

/*
 *This structure has 2 main purposes. It allows a tx thread to write into the
 * buffers of a reader thread within the context of the tx thread. In addition,
 * it also allows sleeping reader threads to be woken up once their buffers
 * are full or in case there is an important event.
 */
typedef 
struct vqec_sink_waiter_
{
    vqec_iobuf_t *iobuf;                /* IO buffer array */
    int32_t         buf_cnt;
                                        /* Number of buffers in the array. The
                                           reader thread will be woken up after
                                           the array is full. */
    int32_t         i_cur;              /* Current index into the array */
    int32_t         len;                
                                        /* Total received data length 
                                           in bytes across all buffers */
    wait_queue_head_t *waitq;
                                        /* Wait queue head pointer. This is
                                         used to wake up the reader. */
} vqec_sink_waiter_t;

/*
 * Extend the sink object with methods to add / remove the waiter element. 
 */
#define VQEC_SINK_WAITER_METHODS                                    \
    /**                                                             \
     *  Add a new waiter to the sink.                               \
     */                                                             \
    void (*vqec_sink_add_waiter)(VQEC_SINK_INSTANCE,                \
                                 struct vqec_sink_waiter_ *waiter); \
    /**                                                             \
     *  Renmove a new waiter from the sink.                         \
     */                                                             \
    void (*vqec_sink_del_waiter)(VQEC_SINK_INSTANCE);               \

#define VQEC_SINK_WAITER_MEMBERS                                    \
    /**                                                             \
     * Pointer to a waiter instance.                                \
     */                                                             \
    struct vqec_sink_waiter_ *waiter;                               \
    /**                                                             \
     * Output socket for user space reads.                          \
     */                                                             \
    struct socket *output_sock;                                     \

#define __vqec_sink_fcns            \
    VQEC_SINK_METHODS_COMMON        \
    VQEC_SINK_WAITER_METHODS        \
    
#define __vqec_sink_members         \
    VQEC_SINK_MEMBERS_COMMON        \
    VQEC_SINK_WAITER_MEMBERS        \

DEFCLASS(vqec_sink);

#endif /* __VQEC_SINK_H__ */
