/********************************************************************
 * syslog_macros_types.h
 *
 * Derived from DC-OS include/isan/syslog_macros_types.h
 * 
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/


/* Internal details
 *
 * This is getting pretty much complicated, let's list here all the flags
 * that are used by these macros and how they work:
 *
 * __KERNEL__: used only to check whether or not SYSLOG_FACILITYDEF_ONLY
 *             is used.
 * SYSLOG_FACILITYDEF_ONLY: when this flag is set, the macros will only
 *             expand the facility part, just to have the LOG_XXX symbol
 *             defined. Whan this flag is set, all the C types and
 *             variables won't be generated. For kernel modules, this
 *             option MUST ALWAYS be set.
 *
 * SYSLOG_DOCGEN: when this flag is set, the macros are used to generate
 *             tagged documentation to be post-processed by a specific
 *             tool.
 *
 * _CAN_INCLUDE_SYSLOG_MACROS_TYPES_H: this flag is used to verify that
 *             syslog_macros_types.h is not included by mistake by a
 *             file that shouldn't include it. Only syslog_macros.h
 *             should include syslog_macros_types.h
 *
 * SYSLOG_DEFINITION: define this flag in one and only one of the C
 *             files that make your application in order to have the
 *             necessary variables defined (any other inclusion of
 *             your xxx_syslog_def.h will only declare them).
 */

/* For any information, please read syslog_macros.h */

#ifndef _CAN_INCLUDE_SYSLOG_MACROS_TYPES_H
#error "Don't include this file directly. Include syslog_macros.h instead"
#endif /* _CAN_INCLUDE_SYSLOG_MACROS_TYPES_H */

#ifndef _SYSLOG_MACROS_TYPES_H
#define _SYSLOG_MACROS_TYPES_H
 
#include "utils/vam_types.h"
#include "utils/vam_time.h"
#include "utils/queue_plus.h"

/* The rate-limit parameter defines the minimum interval in milliseconds 
 * between repetitions of the same syslog message type.
 */
typedef struct msg_limiter_ {
    uint32_t rate_limit;	/* Minimum interval between repetitions of the 
                                 * same message, in msec, or 0 if not limited
                                 */
    abs_time_t last_time;       /* Last time we emitted this message */
    uint64_t suppressed;        /* Count the number of suppressed messages */
} msg_limiter;

/* 
 * Facility definition structure - nominally limited to one per process, since
 * each process can have at most one connection to syslogd.
 */
typedef struct syslog_facility_def_s
{
    char *fac_name;          /* facility name (one per process) */
    int fac_num;             /* facility number */ 
    uint8_t default_filter;  /* This is the default priority level */
    uint8_t priority_filter; /* Any syslog above this level is not sent */
    uint8_t *pri_fil_ptr;    /* pointing to a value in shared mem 
                                (may not need this?!) */
    uint8_t use_stderr;      /* set to TRUE when getenv("USE_STDERR") is set */
} syslog_facility_def_t;

/* Representation of a log message */
typedef struct syslog_msg_def_s
{
    syslog_facility_def_t *facility;   /* parent facility */
    char *msg_name;                    /* message name */
    char *output_format;               /* message header + message format */
    char *msg_format;                  /* message format string */
    uint8_t priority;                  /* message priority */
    msg_limiter *limiter;              /* a limiter or NULL if not limited */
    boolean on_sup_list;               /* if on the sup_msg_list */ 

    VQE_TAILQ_ENTRY(syslog_msg_def_s) list_obj;

} syslog_msg_def_t;



/* Used to convert a facility name to upper cases */
static inline char *syslog_convert_to_upper(char *str) 
{
    int i = 0;
    static char str1[100] = {0};
 
    i = strlen(str);
    while (i--) {
        str1[i] = toupper(str[i]);
    }
	str1[strlen(str)] = '\0';
    return (str1);
}

#endif  /* _SYSLOG_MACROS_TYPES_H */
