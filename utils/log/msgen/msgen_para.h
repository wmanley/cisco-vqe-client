/**-----------------------------------------------------------------
 * @brief
 * Test Tool: msgen parameter interface
 *
 * @file
 * msgen_para.h
 *
 * November 2007, Donghai Ma
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _MSGEN_PARA_H_
#define _MSGEN_PARA_H_

#include "utils/vam_types.h"

#define DEFAULT_XMLRPC_PORT  9000
#define DEFAULT_MSG_RATE     1          /* msg per second */
#define MIN_MSG_RATE         0          /* msg per second */
#define MAX_MSG_RATE         1000       /* msg per second */

#define DEFAULT_MSG_BURST    10         /* msgs */


void msgen_lock_lock(void);
void msgen_lock_unlock(void);

void inc_msg_sent(void);

void set_msg_rate(uint32_t msg_rate);
void set_msg_burst(uint32_t msg_burst);
void set_oneshot_burst_flag(boolean bval);

void get_msgen_paras(uint32_t *rate, uint32_t *burst, boolean *ob, 
                     uint64_t *msg_count);

#endif /* _MSGEN_PARA_H_ */
