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
 * Description: Locks.
 *
 * Documents: 
 *
 *****************************************************************************/

#ifndef __VQEC_LOCK_API_H__
#define __VQEC_LOCK_API_H__

#include <utils/vam_types.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,15)
#include <linux/mutex.h>
#else
#include <asm/semaphore.h>
#endif

/**---------------------------------------------------------------------------
 * Lock defines.
 *---------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,15)
typedef struct mutex vqec_lock_t;
#define vqec_lock_lock(x) mutex_lock(&(x))
#define vqec_lock_unlock(x) mutex_unlock(&(x))
#define vqec_lock_init(x) mutex_init(&(x))
#else
typedef struct semaphore vqec_lock_t;
#define vqec_lock_lock(x) down_interruptible(&(x))
#define vqec_lock_unlock(x) up(&(x))
#define vqec_lock_init(x) init_MUTEX(&(x))
#endif

extern vqec_lock_t g_vqec_dp_lock;

#endif // __VQEC_LOCK_API_H__
