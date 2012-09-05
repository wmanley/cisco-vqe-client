/********************************************************************
 * vqes_dp_debug.h
 *
 * VQE Server Data Plane debug flags
 *
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef __VQES_DP_DEBUG_H__
#define __VQES_DP_DEBUG_H__

#include <log/vqe_id.h>
#include "vqes_dp_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "vqes_dp_debug_flags.h"

/* Macros to do VQES Data Plane debug logging */
#define VQES_DP_SET_FILTER(type, p_filter)                              \
    debug_set_filter(vqes_dp_debug_arr, type, p_filter)

#define VQES_DP_SET_DEBUG_FLAG(type)                                    \
    debug_set_flag(vqes_dp_debug_arr, type, VQENAME_VQES_DP, NULL)

#define VQES_DP_RESET_DEBUG_FLAG(type)                                  \
    debug_reset_flag(vqes_dp_debug_arr, type, VQENAME_VQES_DP, NULL)

#define VQES_DP_DEBUG(type, p_filter, arg...)                           \
    do {                                                                \
        if (debug_check_element(vqes_dp_debug_arr, type, p_filter)) {   \
            buginf(TRUE, VQENAME_VQES_DP, FALSE, arg);                  \
        }                                                               \
    } while (0)


#endif /* __VQES_DP_DEBUG_H__ */
