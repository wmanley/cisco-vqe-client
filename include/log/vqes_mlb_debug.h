/********************************************************************
 * vqes_mlb_debug.h
 *
 * VQE-S Multicast Load Balancer debug flags
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef __VQES_MLB_DEBUG_H__
#define __VQES_MLB_DEBUG_H__

#include <log/vqe_id.h>
#include "vqes_mlb_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "vqes_mlb_debug_flags.h"

/* Macros to do VQE-S MLB debug logging */
#define VQES_MLB_SET_FILTER(type, p_filter)                             \
    debug_set_filter(vqes_mlb_debug_arr, type, p_filter)

#define VQES_MLB_RESET_FILTER(type)              \
    debug_reset_filter(vqes_cp_debug_arr, type)

#define VQES_MLB_SET_DEBUG_FLAG(type)                                   \
    debug_set_flag(vqes_mlb_debug_arr, type, VQENAME_VQES_MLB, NULL)

#define VQES_MLB_GET_DEBUG_FLAG_STATE(type, p_bool, p_filter)            \
    debug_get_flag_state(vqes_mlb_debug_arr, type, p_bool, p_filter)

#define VQES_MLB_RESET_DEBUG_FLAG(type)                                 \
    debug_reset_flag(vqes_mlb_debug_arr, type, VQENAME_VQES_MLB, NULL)

#define IS_VQES_MLB_DEBUG_FLAG_ON(type)          \
    (*(vqes_mlb_debug_arr[type].var) == TRUE)

#define VQES_MLB_DEBUG(type, p_filter, arg...)                          \
    do {                                                                \
        if (debug_check_element(vqes_mlb_debug_arr, type, p_filter)) {  \
            buginf(TRUE, VQENAME_VQES_MLB, FALSE, arg);                 \
        }                                                               \
    } while (0)

#endif /* __VQES_MLB_DEBUG_H__ */
