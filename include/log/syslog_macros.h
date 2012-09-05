/********************************************************************
 * syslog_macros.h
 *
 * Derived from DC-OS include/isan/syslog_macros.h
 * 
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

/* This file defines the following macros:
 *
 * syslog_facilitydef(facility_name, facility_number, default_priority_filter)
 * Used to define a syslog facility with a default priority filter.
 *
 * syslog_msgdef(facility_number, msg_name, priority, output_format)
 * Used to define syslog messages and assign a specific priority and
 * output to them.
 *
 * syslog_open()
 * Used to call openlog() with the right set of parameters.
 *
 * syslog_facility_filter_set()
 * Used to change (at runtime) the priority filter level
 *
 * syslog_print()
 * Used to print a syslog.
 *
 * The objective of these macros is to have a clear definition of
 * syslog messages, and allow the application to filter them based
 * on the priority level; filtering means that some messages won't
 * be sent to the syslog server, reducing its workload.
 *
 * Create a XXX_syslog_def.h file including all the definitions for the
 * syslog messages of application XXX, using the LOG_XXX name and
 * assigning to it a unique number N (you can find all the numbers in
 * use in syslog_facility_num.h).
 *
 * syslog_facilitydef("XXX", LOG_XXX, LOG_CRIT);
 *
 * syslog_msgdef(LOG_XXX, XXX_INIT_FAILED, LOG_CRIT,
 *               "first message: %s");
 * syslog_msgdef(LOG_XXX, XXX_INIT_CONTINUE, LOG_DEBUG,
 *               "second message: %d %d");

 * syslog_msgdef_limit(LOG_XXX, XXX_INVALID_PKT, LOG_ERR,
 *                     "third: %d %s",
 *                     MSGDEF_LIMIT_MEDIUM);
 *
 * The message string doesn't need to be terminated by a '\n', as syslogd
 * will anyway take care of that.
 * This XXX_syslog_def.h file should be included wherever a syslog message
 * is printed. Among all the C files including it, only ONE C file should
 * instantiate the message definitions described in XXX_syslog_def.h;
 * To do so, the designated C file should define the symbol
 * SYSLOG_DEFINITION before including the file, like in the following example:
 *
 * #define SYSLOG_DEFINITION
 * #include "XXX_syslog_def.h"
 * #undef SYSLOG_DEFINITION
 *
 * To print syslog messages, any C file should then do something like this:
 *
 * syslog_open(LOG_XXX, 0); // In the initialization code only
 * syslog_print(XXX_INIT_FAILED, "any_text_param");
 * syslog_print(XXX_INIT_CONTINUE, 3, 4);
 *
 * Depending on the current filter level, the message will be sent to
 * syslogd or not. Every application should define an interaction with the
 * CLI to change the filter level.
 */

/* A note about the implementation: to make sure anybody can include
 * these macros in the same file in both the declarative version and
 * the "definitional" version (the one with SYSLOG_DEFINITION) without
 * compile issues, the macros are always undefined before the
 * redefinition. In this way we don't have to put the magic formula
 * #ifndef _A -> #define _A -> #endif, whilch wouldn't work for this
 * case. Thanks to Chandan for proposing this solution.
 */

/* First, let's clean up, in case this file has been already included
 * earlier (potentially with a different set of options)
 */
#undef syslog_facilitydef
#undef syslog_open
#undef syslog_facility_filter_set
#undef syslog_facility_filter_set_default
#undef syslog_facility_filter_get
#undef syslog_facility_default_filter_get
#undef is_syslog_facility_filter_default
#undef syslog_msgdef
#undef syslog_msgdef_limit
#undef syslog_print

#undef msgdef_explanation
#undef msgdef_recommended_action
#undef msgdef_ddts_component
#undef SYSLOGDOC_NO_ACTION
#undef SYSLOGDOC_NO_DDTS

#include <log/syslog.h>
#include <log/syslog_facility_num.h>
#include <log/vqe_id.h>

#if defined(SYSLOG_DOCGEN)

#include <log/syslog_docgen.h>

#else /* SYSLOG_DOCGEN */

#define _CAN_INCLUDE_SYSLOG_MACROS_TYPES_H
#include <log/syslog_macros_types.h>
#undef _CAN_INCLUDE_SYSLOG_MACROS_TYPES_H


/*
 * Useful default time for rate limited messages:
 *     FAST: 10 messages per second
 *   MEDIUM: 1 messages per second
 *     SLOW: 1 msg per 10 seconds
 *  GLACIAL: 1 msg per 60 seconds
 */
#define MSGDEF_LIMIT_GLACIAL (60000)
#define MSGDEF_LIMIT_SLOW    (10000)
#define MSGDEF_LIMIT_MEDIUM  (1000)
#define MSGDEF_LIMIT_FAST    (100)

#include "log/vqes_syslog_limit.h"


/*
 * Macro to define a "syslog facility".
 */
#ifdef SYSLOG_DEFINITION
#define syslog_facilitydef(fac_name, fac_sym, default_pri_filter)      \
         syslog_facility_def_t syslog_fac_##fac_sym =                  \
                 { fac_name,                                           \
                   fac_sym,                                            \
                   default_pri_filter,                                 \
                   default_pri_filter,                                 \
                   &syslog_fac_##fac_sym.priority_filter }

#else /* SYSLOG_DEFINITION */
#define syslog_facilitydef(fac_name, fac_sym,                          \
                           default_priority_filter)                    \
         extern syslog_facility_def_t syslog_fac_##fac_sym

#endif /* SYSLOG_DEFINITION */


/*
 * Macro to define a syslog message for a given facility. Each message has a 
 * name an associated format string, and a priority. If message names collide 
 * across two facilities, then the corresponding message definitions should not
 * get included in the same C file.
 */
#ifdef SYSLOG_DEFINITION

#define syslog_msgdef(fac_sym, msg_name, priority, output_fmt)          \
    syslog_msg_def_t syslog_msg_##msg_name =                            \
    { &syslog_fac_##fac_sym,                                            \
      #msg_name,                                                        \
      "<%s-%d-" #msg_name "> " output_fmt "\n",                     \
      output_fmt,                                                       \
      priority,                                                         \
      NULL, FALSE, {NULL} }
#define syslog_msgdef_limit(fac_sym, msg_name, priority, output_fmt, limit) \
    static msg_limiter msg_limiter_##msg_name =                         \
    { limit, ABS_TIME_0, 0 };                                           \
    syslog_msg_def_t syslog_msg_##msg_name =                            \
    { &syslog_fac_##fac_sym,                                            \
      #msg_name,                                                        \
      "<%s-%d-" #msg_name "> " output_fmt "\n",                     \
      output_fmt,                                                       \
      priority,                                                         \
      &msg_limiter_##msg_name, FALSE, {NULL} }

#else /* SYSLOG_DEFINITION */

#define syslog_msgdef(fac_sym, msg_name, priority, output_fmt)  \
    extern syslog_msg_def_t syslog_msg_##msg_name
#define syslog_msgdef_limit(fac_sym, msg_name, priority, output_fmt, limit) \
    extern syslog_msg_def_t syslog_msg_##msg_name

#endif /* SYSLOG_DEFINITION */


/*
 * Macros to be used by documentation only, and therefore have no bodies.
 */
#define msgdef_explanation(txt)
#define msgdef_recommended_action(txt)
#define msgdef_ddts_component(txt)

/*
 * Logging a message to syslog.
 */

#ifdef HAVE_SYSLOG
/* 
 * When logging a syslog message, do rate limiting if the message is defined
 * with the msgdef_limit() macro. If a message is suppressed after it exceeds 
 * the specified limiting rate, the message struct is added to the 
 * Suppressed Message List (SML), which can be shown with a XMLRPC method.
 *
 * The SML is checked upon the next successful emission of *any* message, 
 * i.e. the triggering message may not be the same as the suppressed message.
 * Or an application may hook up with routine check_sml() to proactively 
*  check the SML. 
 */
#define syslog_print(msg_name, args...)                                 \
    do {                                                                \
        syslog_msg_def_t *__msg;                                        \
        abs_time_t __now;                                               \
        __msg = (syslog_msg_def_t*)&(syslog_msg_##msg_name);            \
        if (__msg->priority > __msg->facility->priority_filter) {       \
            break;                                                      \
        }                                                               \
        if(__msg->facility->use_stderr) {                               \
            VQE_FPRINTF(stderr,                                         \
                    __msg->output_format,                               \
                    syslog_convert_to_upper(__msg->facility->fac_name), \
                    __msg->priority,                                    \
                    ##args);                                            \
            break;                                                      \
        }                                                               \
        __now = get_sys_time();                                         \
        vqes_syslog_lock_lock();                                        \
        if (__msg->limiter) {                                           \
            /* Suppress this message if we have not passed rate_limit   \
             * millisecs since last time when this message was logged   \
             */                                                         \
            if (TIME_CMP_A(gt,                                          \
                           TIME_ADD_A_R(__msg->limiter->last_time,      \
                                        TIME_MK_R(                      \
                                            msec,                       \
                                            __msg->limiter->rate_limit)), \
                           __now)) {                                    \
                __msg->limiter->suppressed ++;                          \
                if (!__msg->on_sup_list) {                              \
                    __msg->limiter->last_time = __now;                  \
                    add_to_sml_ul(__msg);                               \
                }                                                       \
                vqes_syslog_lock_unlock();                              \
                break;                                                  \
            } else {                                                    \
                if (__msg->limiter->suppressed > 0) {                   \
                    vqes_syslog_ul(LOG_WARNING,                         \
                                   "Messages of the form \"<%s-%d-%s> %s\" were " \
                                   "suppressed %llu times\n",           \
                                   syslog_convert_to_upper(             \
                                       __msg->facility->fac_name),      \
                                   __msg->priority,                     \
                                   #msg_name,                           \
                                   __msg->msg_format,                   \
                                   __msg->limiter->suppressed);         \
                }                                                       \
                if (__msg->on_sup_list) {                               \
                    remove_from_sml_ul(__msg);                          \
                }                                                       \
                __msg->limiter->suppressed = 0;                         \
                __msg->limiter->last_time = __now;                      \
            }                                                           \
        }                                                               \
        check_sml_ul(__now);                                            \
        vqes_syslog_ul(__msg->priority,                                 \
                       __msg->output_format,                            \
                       syslog_convert_to_upper(__msg->facility->fac_name), \
                       __msg->priority,                                 \
                       ##args);                                         \
        vqes_syslog_lock_unlock();                                      \
    } while(0)

#else /* #ifdef HAVE_SYSLOG */

/* Note syslog_print() is not rate limited when HAVE_SYSLOG is undefined */
#define syslog_print(msg_name, args...)                                 \
    do {                                                                \
        if((syslog_msg_##msg_name.facility)->priority_filter >=         \
           syslog_msg_##msg_name.priority) {                            \
            VQE_FPRINTF(stderr,                                             \
                    syslog_msg_##msg_name.output_format,                \
                    syslog_convert_to_upper(                            \
                        (syslog_msg_##msg_name.facility)->fac_name),    \
                    syslog_msg_##msg_name.priority,                     \
                    ##args);                                            \
        }                                                               \
    } while(0)
#endif /* HAVE_SYSLOG */

/*
 * Open a syslog facility - LOG_CONS is not set by default. Use this macro
 * instead of using syslog_open() directly.
 */
#ifdef HAVE_SYSLOG
#define syslog_facility_open(fac_sym, extra_flags)                      \
    do {                                                                \
        vqes_openlog(syslog_fac_##fac_sym.fac_name,                     \
                     (LOG_NDELAY) | (extra_flags),                      \
                     syslog_fac_##fac_sym.fac_num,                      \
                     NULL,                                              \
                     0);                     \
        syslog_fac_##fac_sym.use_stderr = (getenv("USE_STDERR")!=NULL)?1:0; \
    } while(0)
#else
#define syslog_facility_open(fac_sym, extra_flags)                           
#endif /* HAVE_SYSLOG */

#define syslog_facility_open_fds(fac_sym, extra_flags, fd_array, num_fds) \
    do {                                                                \
        vqes_openlog(syslog_fac_##fac_sym.fac_name,                     \
                     (LOG_NDELAY) | (extra_flags),                      \
                     syslog_fac_##fac_sym.fac_num,                      \
                     fd_array,                                              \
                     num_fds);                     \
        syslog_fac_##fac_sym.use_stderr = (getenv("USE_STDERR")!=NULL)?1:0; \
    } while(0)

#define syslog_facility_close          \
    vqes_closelog

/*
 * Accessors, Modifiers for a facility's priority filtering level.
 */

/* Combine with setlogmask() so that users only need to call this macro to 
 * set the facility priority. Always set the LOG_DEBUG mask so that debug
 * messages are logged as soon as a debug flag is turned on. This is to be
 * consistent with IOS, DC-OS.
 */
#define syslog_facility_filter_set(fac_sym, new_priority_filter)        \
    (syslog_fac_##fac_sym.priority_filter = new_priority_filter);       \
    vqes_setlogmask(LOG_UPTO(new_priority_filter) | LOG_MASK(LOG_DEBUG))

#define syslog_facility_filter_set_default(fac_sym)                     \
    (syslog_fac_##fac_sym.priority_filter =                             \
     syslog_fac_##fac_sym.default_filter)

#define syslog_facility_filter_get(fac_sym, get_priority_filter)        \
    (get_priority_filter = syslog_fac_##fac_sym.priority_filter)

#define syslog_facility_default_filter_get(fac_sym, get_priority_filter) \
    (get_priority_filter = syslog_fac_##fac_sym.default_filter)

#define is_syslog_facility_filter_default(fac_sym, priority_filter)     \
    (priority_filter == syslog_fac_##fac_sym.default_filter)


/* Normally we have one facility mapped to a process.  But for libraries
 * that are shared by multiple processes, we do want to define their own 
 * facilities (to define syslog messages) for the libraries; also we want
 * their priorities in sync with the priority of the process they reside.
 *
 * This macro sync'es up the facility priority of libraries with that of 
 * the process where the libraries reside.  
 *
 * Right now we only need to handle the RTP/RTCP Library used in the 
 * VQES Control and Data Processes.
 */
#define sync_lib_facility_filter_with_process(process_fac_sym, lib_fac_sym) \
    do {                                                                \
        syslog_fac_##lib_fac_sym.priority_filter =                      \
            syslog_fac_##process_fac_sym.priority_filter;               \
        syslog_fac_##lib_fac_sym.default_filter =                       \
            syslog_fac_##process_fac_sym.default_filter;                \
        syslog_fac_##lib_fac_sym.use_stderr =                           \
            syslog_fac_##process_fac_sym.use_stderr;                    \
    } while(0)

#endif /* SYSLOG_DOCGEN */
