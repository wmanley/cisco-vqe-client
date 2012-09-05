//------------------------------------------------------------------
// VQEC. Locking API.
//
//
// Copyright (c) 2007-2008 by cisco Systems, Inc.
// All rights reserved.
//------------------------------------------------------------------

#ifndef __VQEC_LOCK_API_H__
#define __VQEC_LOCK_API_H__

#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include "vqec_assert_macros.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

//----------------------------------------------------------------------------
// Lock data structure.
//----------------------------------------------------------------------------  
typedef struct vqec_lock_
{
    pthread_mutex_t     mutex;          //!< Pthread mutex
} vqec_lock_t;


#define VQEC_LOCK_DEF_NATIVE(name)                                      \
    vqec_lock_def(name)                 //!< - Native pthread lock

//----------------------------------------------------------------------------
// Define a lock.
//----------------------------------------------------------------------------  
#ifdef VQEC_LOCK_DEFINITION
#define vqec_lock_def(name)                                             \
    vqec_lock_t vqec_lockdef_##name =                                   \
        { PTHREAD_MUTEX_INITIALIZER }

#else // VQEC_LOCK_DEFINITION
#define vqec_lock_def(name)                                             \
    extern vqec_lock_t vqec_lockdef_##name
#endif // VQEC_LOCK_DEFINITION

//----------------------------------------------------------------------------
// Internal implementation to release a lock.
//----------------------------------------------------------------------------  
static inline 
int32_t vqec_lock_unlock_internal (vqec_lock_t *lp)
{
    int32_t _rv = 0;

    _rv = pthread_mutex_unlock(&lp->mutex);
    VQEC_ASSERT(_rv == 0);

    return (_rv);
}

//----------------------------------------------------------------------------
// Internal implementation to acquire a lock.
//----------------------------------------------------------------------------
static inline 
int32_t vqec_lock_lock_internal (vqec_lock_t *lp)
{
    int32_t _rv = 0;

    _rv = pthread_mutex_lock(&lp->mutex);
    VQEC_ASSERT(_rv == 0);

    return (0);
}

//----------------------------------------------------------------------------
// Acquire a lock. The rvalue of this macro is 0.
//----------------------------------------------------------------------------  
#define vqec_lock_lock(name)                                            \
    ({                                                                  \
        int32_t _rv = vqec_lock_lock_internal(&(vqec_lockdef_##name));  \
        _rv;                                                            \
    })                                                                  \




//----------------------------------------------------------------------------
// Release a lock.  The rvalue of this macro is 0.
//----------------------------------------------------------------------------  
#define vqec_lock_unlock(name)                                           \
    ({                                                                   \
        int32_t _rv = vqec_lock_unlock_internal(&(vqec_lockdef_##name)); \
        _rv;                                                             \
    })

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __VQEC_LOCK_API_H__
