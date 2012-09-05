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
 * Description: Generation of DP debug macros.
 *
 * Documents: Debug flags for use throughout the dataplane. It should be noted
 * that turning on any per-packet debug in the dataplane (especially in the
 * kernel) may significantly impact performance. Hence keep the granularity 
 * of a debug, i.e.,  the number of things it may turn on, small, and also 
 * remember that in many cases they may be a last resort debug tool, rather 
 * than a preferred one.
 *
 *****************************************************************************/

#ifndef __VQEC_DP_DEBUG_H__
#define __VQEC_DP_DEBUG_H__

#include <log/vqe_id.h>

#define __DECLARE_DEBUG_EXTERN_VARS__
#include "vqec_dp_debug_flags.h"        /* Inclusion of boolean flags */

#define __DECLARE_DEBUG_NUMS__
#include "vqec_dp_debug_flags.h"        /* Inclusion of the enumeration */

/**
 * Set a dataplane debug flag.
 */
#define VQEC_DP_SET_DEBUG_FLAG(type)                                    \
    debug_set_flag(vqec_dp_debug_arr, type,  VQENAME_VQEC_DP, NULL)

/**
 * Clear or reset a dataplane debug flag.
 */
#define VQEC_DP_RESET_DEBUG_FLAG(type)                                  \
    debug_reset_flag(vqec_dp_debug_arr, type,  VQENAME_VQEC_DP, NULL)

/**
 * Get the value of a dataplane debug flag.
 */
#define VQEC_DP_GET_DEBUG_FLAG(type)                    \
    debug_check_element(vqec_dp_debug_arr, type, NULL)

/**
 * Log a debug message.
 */ 
#define VQEC_DP_DEBUG(type, arg...)                               \
    do {                                                          \
        if (debug_check_element(vqec_dp_debug_arr, type, NULL)) { \
            buginf(TRUE, VQENAME_VQEC_DP, FALSE, arg);            \
        }                                                         \
    } while (0)

#endif /* __VQEC_DP_DEBUG_H__ */

