/**-----------------------------------------------------------------
 * @brief
 * Test Tool: msgen - parameter interface
 *
 * @file
 * msgen_para.c
 *
 * November 2007, Donghai Ma
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include <pthread.h>
#include "msgen_para.h"

/* Count the number of messages sent */
static uint64_t g_msg_sent = 0;

/* Settable from the XMLRPC API */
static uint32_t g_msg_rate = DEFAULT_MSG_RATE;
static uint32_t g_msg_burst = DEFAULT_MSG_BURST;

/* Send one burst of <msg_burst> number of messages when the flag is set to 
 * TRUE. Reset to FALSE when the burst is done.
 */
static boolean g_oneshot_burst = FALSE;


/* Define the lock */
static pthread_mutex_t msgen_lock = PTHREAD_MUTEX_INITIALIZER;

void msgen_lock_lock (void)
{
    pthread_mutex_lock(&msgen_lock);
}

void msgen_lock_unlock (void)
{
    pthread_mutex_unlock(&msgen_lock);
}


void inc_msg_sent (void)
{
    msgen_lock_lock();
    g_msg_sent ++;
    msgen_lock_unlock();
}


void set_msg_rate (uint32_t msg_rate)
{
    msgen_lock_lock();
    g_msg_rate = msg_rate;
    msgen_lock_unlock();
}

void set_msg_burst (uint32_t msg_burst)
{
    msgen_lock_lock();
    g_msg_burst = msg_burst;
    msgen_lock_unlock();
}

void set_oneshot_burst_flag (boolean bval)
{
    msgen_lock_lock();
    g_oneshot_burst = bval;
    msgen_lock_unlock();
}

void get_msgen_paras (uint32_t *rate, uint32_t *burst, boolean *ob,
                      uint64_t *msg_count)
{
    msgen_lock_lock();
    *rate = g_msg_rate;
    *burst = g_msg_burst;
    *ob = g_oneshot_burst;
    *msg_count = g_msg_sent;
    msgen_lock_unlock();
}

