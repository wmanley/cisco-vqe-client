/* 
 *------------------------------------------------------------------
 * Event Logging subsystem
 *
 * Started May 1996, Dave Barach
 * Posix version November 2005, Dave Barach
 *
 * Copyright (c) 1996-2007 by cisco Systems, Inc.
 * All rights reserved.
 *
 *------------------------------------------------------------------
 */

#ifndef __ELOG_H_
#define __ELOG_H_ 1

/*
 * Bisexual logging support
 * The QNX kernel logging mechanism (TraceEvent)
 * is preferable, to the extent that it doesn't
 * produce so much data as to be useless.
 *
 * The following preprocessor symbol controls whether code 
 * instrumentation macros use it, or use the ported IOS Classic 
 * Gist logger.
 *
 * #ifdef USE_INFRA_ELIB_LOGGER
 *
 */

#define USE_INFRA_ELIB_LOGGER 1

/* 
 * Please DO NOT #include anything that might be 
 * unavailable e.g. when compiling offline tools against this
 * header file.
 */
#ifdef USE_INFRA_ELIB_LOGGER
#include <pthread.h>            /* needed by the Gist logger port */
#else
#include <sys/trace.h>          /* needed by QNX TraceEvent */
#endif /* USE_INFRA_ELIB_LOGGER */

/*
 * The following macro should be used by point definition files for 
 * enumerating the subsystem specific points.
 */
#define EV_NUM(n)	(EV_BASE + (n))

/*
 * Each "event numbering domain" needs a top-level header file
 * which assigns EV_BASE, and points the various tools at the set of
 * event definition header files.
 *
 * These top-level header files must contain a line or lines of the
 * form:
 *
 * #define EV_SUB1_BASE  100  / * pointdefs: ../h/sub1_points.h * /
 * #define EV_SUB2_BASE  200  / * pointdefs: ../h/sub2_points.h * /
 *
 * The leaf-level header files define specific sets of events as follows:
 *
 * sub1_points.h:
 *
 * #define EV_BASE EV_SUB1_BASE
 * #define EV_SUB1_FIRST   EV_NUM(0) / * first_event_sub1: %d * /
 * #define EV_SUB1_SECOND  EV_NUM(1) / * second_event_sub1: %d * /
 *
 * sub2_points.h:
 *
 * #define EV_BASE EV_SUB2_BASE
 * #define EV_SUB2_FIRST   EV_NUM(0) / * first_event_sub2: %d * /
 * #define EV_SUB2_SECOND  EV_NUM(1) / * second_event_sub2: %d * /
 */

/*
 * As of this writing, .../infra/elib/src/cpel_gen is willing to 
 * autogenerate <infra/elib/elog_base.h> and .../src/test_cpel.c,
 * effectively auto-numbering the event points.
 */
#include "../../include/utils/elog_base.h"

#ifdef USE_INFRA_ELIB_LOGGER

/**** event record structure ****/
typedef struct _elog_event {
    /* 20 bytes */
    unsigned long time[2];
    unsigned long pid;
    unsigned long code;
    unsigned long datum;
} elog_event;

/**** shared memory header structure ****/

#define ELOG_MAXNAME 128

typedef struct _elog_shmem {
    /* First cache line */
    int enable;
    int flags;
    pthread_mutex_t mutex;
    /* Second cache line */
    int nevents;                /* number of events in buffer */
    int curindex;               /* current index */
    int trigger_state;          /* trigger variables */
    int trigger_index;
    int num_clients;            /* Number of clients hooked up */
    int rnd_pagesize;           /* size of shared region, rounded to 4K */
    int trigger_position;       /* trigger position, in percent of buffer */
    int pad;
    /* 4 lines worth (more or less) of name */
    char name[ELOG_MAXNAME];
    /* The event array */
    elog_event events[0];
} elog_shmem_t;

#define ELOG_FLAG_SHARED	0x00000001 /* shared buffer, need mutex */
#define ELOG_FLAG_RESIZE        0x00000002 /* resize in progress */
#define ELOG_FLAG_WRAPPED       0x00000004 /* buffer has wrapped */

#define ELOG_DEF_BUF_SIZE	100000

elog_shmem_t *elog_get_shmem(void);
void elog_log(unsigned long track, unsigned long code, unsigned long datum);

void elog_trigger(void);
int elog_map_options (const char *name, int nevents, int resize);
int elog_map(const char *name);
void elog_unmap(void);
int elog_not_mapped(void);
elog_shmem_t *elog_shmem;

/*
 * elog_log
 *
 * Prototype for event logging.  Subsystems should not call this function
 * directly.  Instead they should use the macros ELOG and XELOG which
 * wrap the core elog_log facility.
 *
 * Macro parameters:
 *
 * 	do_trace: do_trace indicates whether the subsystem wants this
 *                event logged.  It is evaluated as follows:
 * 		  If non-zero, then log the event.  If the upper-most
 *		  bit is on, then treat the event as a trigger.
 *
 *	track:	  process id, session id or any other piece of information
 *		  which is useful to that subsystem to correlate the event.
 *
 *	code:	  The event code to log.  This number should be a value
 *		  within the defined range for the subsystem.
 *
 *	datum:	  Any arbitrary information that the caller wants to log
 *		  with this event.
 */

#define ELOG(do_trace, code, datum)             \
    if (elog_shmem->enable && (do_trace)) {     \
        elog_log(0,                             \
                 (unsigned long)(code),         \
                 (unsigned long)(datum));       \
}

#define XELOG(do_trace, pid, code, datum)       \
    if (elog_shmem->enable && (do_trace)) {     \
        elog_log((unsigned long)pid,            \
                 (unsigned long)(code),         \
                 (unsigned long)(datum));       \
}

#if defined(OBJECT_4K) || defined(__ARCH_4K__)
static inline unsigned long arch_get_timestamp(void)
{
    unsigned long rv;
#if defined (__OBJ_IN_KERNEL__)
    asm volatile("mfc0 %0, $9" : "=r" (rv));
#else
    #error arch_get_timestamp is undefined in user-mode...
#endif /* __OBJ_IN_KERNEL__ */
    return (rv);
}
#endif /* OBJECT_4K */

#if defined(OBJECT_PPC) || defined(__ARCH_PPC__)
static inline unsigned long arch_get_timestamp(void)
{
    unsigned long rv;

    asm volatile("mftb %0" : "=r" (rv) : );

    return rv;
}
#endif /* OBJECT_PPC */

#ifdef OBJECT_X86
static inline unsigned long arch_get_timestamp(void)
{
    unsigned long low, high;
    asm volatile ("rdtsc" : "=a" (low), "=d" (high));
    return(low);
}

#endif /* OBJECT_X86 */

#if !defined(OBJECT_4K) && !defined(__ARCH_4K__)  \
&& !defined(OBJECT_PPC) && !defined(__ARCH_PPC__) \
&& !defined(OBJECT_X86)

#error arch_get_timestamp is undefined...

#endif /* multiple ways to get a timestamp */

#else /* USE_INFRA_ELIB_LOGGER */

/* 
 * QNX TraceEvent version of the macros 
 */
#define ELOG(do_trace, code, datum)                                     \
    if ((do_trace)) {                                                   \
        TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, (code), (datum));       \
}

#define XELOG(do_trace, pid, code, datum)                               \
    if ((do_trace)) {                                                   \
        TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, (code), (datum));       \
}

#endif /* USE_INFRA_ELIB_LOGGER */

/* Groups of events. 1 => enabled, 2 => disabled */
#define ELOG_DEFAULT_GROUP    1

#endif /* __ELOG_H_ */
