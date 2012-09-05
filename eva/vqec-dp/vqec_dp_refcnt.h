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
 * Description: Atomic reference counting macros for the dataplane.
 *
 * Documents: In userspace, since the dataplane operations will be invoked
 * with the global lock held, therefore, no locks are to be acquired for
 * atomic operations. (In later implementations we may decided if the
 * lock member needs to be converted to a pointer).
 *
 *****************************************************************************/

#ifndef __VQEC_DP_REFCNT_H__
#define __VQEC_DP_REFCNT_H__

#include <vqec_lock.h>

#define VQEC_DP_REFCNT_MEMBERS(type)            \
    struct {                                    \
        /**                                     \
         * Reference counter.                   \
         */                                     \
        uint32_t count;                         \
        /**                                     \
         * Lock - may not be used in all implementations.       \
         */                                                     \
        vqec_lock_t lock;                                       \
        /**                                                     \
         * Destructor - invoked when the reference count                \
         * drops to 0.                                                  \
         */                                                             \
        void (*destructor)(type *self);         \
    }

/**
 * Defines the members of a reference count field. 
 */
#define VQEC_DP_REFCNT_FIELD(type)  \
    VQEC_DP_REFCNT_MEMBERS(type) __refcnt;


/**
 * Lock acquisition, and release macros. 
 * They are empty in a userspace implementation.
 */
#define VQEC_DP_REFCNT_ACQUIRE_LOCK(lockptr) 
#define VQEC_DP_REFCNT_RELEASE_LOCK(lockptr) 

/**
 * Bump up the reference count.
 */
#define VQEC_DP_REFCNT_REF(objptr)              \
    {                                                           \
        VQEC_DP_REFCNT_ACQUIRE_LOCK(&((objptr)->__refcnt.lock));        \
        (objptr)->__refcnt.count++;                                     \
        VQEC_DP_REFCNT_RELEASE_LOCK(&((objptr)->__refcnt.lock));        \
    }

/**
 * Module-specific deinitialization (if any).
 */
#define __VQEC_DP_REFCNT_DEINIT_INTERNAL(objptr)         \
    {                                           \
    }

/**
 * Module-specific initialization (if any).
 */
#define __VQEC_DP_REFCNT_INIT_INTERNAL(objptr)         \
    {                                                  \
    }

/**
 * Decrement the reference count. If the reference count drops to 0,
 * the object is destroyed by invoking the destructor.
 */
#define VQEC_DP_REFCNT_UNREF(objptr)            \
    {                                                                   \
        int remaining;                                                  \
        VQEC_DP_REFCNT_ACQUIRE_LOCK(&((objptr)->__refcnt.lock));        \
        (objptr)->__refcnt.count--;                                     \
        remaining = (objptr)->__refcnt.count;                           \
        VQEC_DP_REFCNT_RELEASE_LOCK(&((objptr)->__refcnt.lock));        \
        if (remaining <= 0) {                                           \
            __VQEC_DP_REFCNT_DEINIT_INTERNAL(objptr);                   \
            if ((objptr)->__refcnt.destructor) {                        \
                (*((objptr)->__refcnt.destructor))(objptr);             \
            }                                                           \
        }                                                               \
    }

/**
 * Initialize the reference count members.
 */
#define VQEC_DP_REFCNT_INIT(objptr, destruct)                           \
    {                                                                   \
        memset(&(objptr)->__refcnt, 0, sizeof((objptr)->__refcnt));     \
        (objptr)->__refcnt.destructor = (destruct);                     \
        __VQEC_DP_REFCNT_INIT_INTERNAL((objptr));                       \
    }

#endif /* __VQEC_DP_REFCNT_H__ */
