/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_pthread.h
 *
 * Description: Creation of threads for VQEC, including real-time threads.
 *
 * Documents:
 *
 *****************************************************************************/
#ifndef __VQEC_PTHREAD_H__
#define __VQEC_PTHREAD_H__

#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define VQEC_SMALL_STACK 32768
#define VQEC_NORMAL_STACK 65536

int32_t vqec_pthread_set_priosched(pthread_t tid);
int32_t vqec_pthread_create(pthread_t *tid, 
                            void *(*start_routine)(void *), void *arg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VQEC_PTHREAD_H__ */
