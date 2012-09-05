/*
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifndef VQEC_DEBUG_H
#define VQEC_DEBUG_H

#include "vam_types.h"
#include "vqe_port_macros.h"

/* set up syslog to work in vqec */
#ifdef SYSLOG_DEFINITION

#include "vqec_syslog_def.h"
/* Include boolean flags */
#include "vqec_debug_flags.h"
/* Include enum codes */
#define __DECLARE_DEBUG_NUMS__
#include "vqec_debug_flags.h"
/* Declare the debug array struct */
#define __DECLARE_DEBUG_ARR__
#include "vqec_debug_flags.h"

#undef SYSLOG_DEFINITION

#else

#include "vqec_syslog_def.h"
/* Include boolean flags */
#define __DECLARE_DEBUG_EXTERN_VARS__
#include "vqec_debug_flags.h"
/* Include enum codes */
#define __DECLARE_DEBUG_NUMS__
#include "vqec_debug_flags.h"

#endif /* SYSLOG_DEFINITION */

#include "vqec_error.h"

/*
 * vqec_err2str_internal()
 *
 * Maps a VQE-C error code to a string.
 *
 * @param[in] err      Error code to be described
 * @param[out] char *  Descriptive string corresponding to error
 */
const char *
vqec_err2str_internal(vqec_error_t err);

/*
 * Returns whether or not info level syslogs are enabled:
 *   TRUE:   info level logs should be printed
 *   FALSE:  info level logs should NOT be printed
 */
#define vqec_info_logging()                                             \
    ((vqec_syscfg_get_ptr() &&                                      \
      vqec_syscfg_get_ptr()->log_level >= LOG_INFO) ? TRUE : FALSE)

/*
 * Declare string buffers that are suitable for logging use
 * (i.e., can be overwritten with the next logging statement).
 */
#define VQEC_LOGMSG_BUFSIZE 128
#define VQEC_LOGMSG_BUFSIZE_LARGE 2048
extern char s_log_str_large[VQEC_LOGMSG_BUFSIZE_LARGE];

/*
 * Declare string buffers that are suitable for debug use
 * (i.e., can be overwritten with the next debug statement).
 */
#define DEBUG_STR_LEN 200
extern char s_debug_str[DEBUG_STR_LEN];
extern char s_debug_str2[DEBUG_STR_LEN];

/**
 * Generates a string that describes the return code of a function.
 * Useful for including the result of an API call within a debug
 * statement.  The generated string is similar to that produced by 
 * vqec_err2str(), except that:
 *  1) the string is empty if the error code is VQEC_OK, and
 *  2) otherwise, the error code is preceded by ": "
 *
 * NOTE:  the contents of the returned string should be immediately
 *        used or copied, as subsequent calls to this function will
 *        overwrite the contents of the string.
 *
 * @param[in] err           Error code
 * @param[out] char *       Descriptive string for error code (if failure)
 */ 
char *
vqec_err2str_complain_only (vqec_error_t status);

/*
 *------------------------------------------------------------------------
 * Macros to do debug logging: wrap around buginf() 
 *------------------------------------------------------------------------
 */
#define VQEC_SET_DEBUG_FLAG(type)                           \
    debug_set_flag(vqec_debug_arr, type,  VQENAME_VQEC, NULL)
#define VQEC_RESET_DEBUG_FLAG(type)                         \
    debug_reset_flag(vqec_debug_arr, type,  VQENAME_VQEC, NULL)
#define VQEC_GET_DEBUG_FLAG(type)                           \
    debug_check_element(vqec_debug_arr, type, NULL)

#define VQEC_DEBUG(type, arg...)                            \
    do {                                                        \
        if (debug_check_element(vqec_debug_arr, type, NULL)) {    \
            buginf(TRUE, VQENAME_VQEC, FALSE, arg);         \
        }                                                       \
    } while (0)
#define VQEC_LOG_ERROR(arg...)                                   \
    buginf(TRUE, VQENAME_VQEC, FALSE, arg);

#define VQEC_DEBUG_CONSOLE(type, format, arg...)                \
    if (debug_check_element(vqec_debug_arr, type, NULL)) {      \
        vqes_syslog(1, format, ##arg);                          \
    }

#endif /* VQEC_DEBUG_H */
