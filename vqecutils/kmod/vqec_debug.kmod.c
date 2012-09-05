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
 * Description: Kernel debug.
 *
 * Documents: 
 *
 *****************************************************************************/

#include <utils/vam_types.h>

/*
 * This defines the error array.  It is the one .c file in which
 * "vqec_error.h" is included with __DECLARE_ERROR_ARR__ being defined.
 */
#define __DECLARE_ERROR_ARR__ 
#include "vqec_error.h"
#undef __DECLARE_ERROR_ARR__
#include "vqec_error.h"
/*
 * this is the one .c file in all of vqec where this definition
 * exists while including vqec_debug.h
 * if it's defined anywhere else while including vqec_debug.h,
 * it will break everything
 */
#define SYSLOG_DEFINITION
#include "vqec_debug.h"
#undef SYSLOG_DEFINITION

/*
 * String buffers that are suitable for debug use
 * (i.e., can be overwritten with the next debug statement).
 */
char s_debug_str[DEBUG_STR_LEN];
char s_debug_str2[DEBUG_STR_LEN];

const char *
vqec_err2str_internal (vqec_error_t err)
{
    if (err < VQEC_ERR_MAX) {
        return (vqec_error_arr[err].helpstr);
    } else {
        return (vqec_error_arr[VQEC_ERR_MAX].helpstr);
    }
}

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
vqec_err2str_complain_only (vqec_error_t status)
{
    static char s_debug_str[DEBUG_STR_LEN];

    if (status != VQEC_OK) {
        snprintf(s_debug_str, DEBUG_STR_LEN, ": %s",
                 vqec_err2str_internal(status));
    } else {
        s_debug_str[0] = '\0';
    }

    return (s_debug_str);
}
