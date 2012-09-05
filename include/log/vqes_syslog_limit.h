/********************************************************************
 * vqes_syslog_limit.h
 * 
 * Helper functions for VQE-S syslog rate limiting
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef __VQES_SYSLOG_LIMIT_H__
#define __VQES_SYSLOG_LIMIT_H__

#include "log/syslog_macros.h"
#include "utils/queue_plus.h"

typedef struct sml_t_
{
    VQE_TAILQ_HEAD(sml_head_, syslog_msg_def_s) head;
    uint32_t num_entries;
} sml_t;


/* Global mutex to */

extern void add_to_sml_ul(syslog_msg_def_t *p_msg);
extern void remove_from_sml_ul(syslog_msg_def_t *p_msg);
extern void check_sml_ul(abs_time_t ts_now);
extern void check_sml(abs_time_t ts_now);

/* Interface to the SML */
extern syslog_msg_def_t *get_first_sup_msg(void);
extern syslog_msg_def_t *get_next_sup_msg(syslog_msg_def_t *msg);
extern uint32_t sml_num_entries(void);

#endif  /* __VQES_SYSLOG_LIMIT_H__ */
