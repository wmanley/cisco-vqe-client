/********************************************************************
 * vqes_cp_debug.h
 *
 * VQE Server Control Plane debug flags
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef __VQES_CP_DEBUG_H__
#define __VQES_CP_DEBUG_H__

#include <log/vqe_id.h>
#include "vqes_cp_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "vqes_cp_debug_flags.h"

/* Macros to do VQES CP debug logging */
#define VQES_CP_SET_FILTER(type, p_filter)              \
    debug_set_filter(vqes_cp_debug_arr, type, p_filter)

#define VQES_CP_RESET_FILTER(type)              \
    debug_reset_filter(vqes_cp_debug_arr, type)

#define VQES_CP_SET_DEBUG_FLAG(type)                                    \
    debug_set_flag(vqes_cp_debug_arr, type, VQENAME_VQES_CP, NULL)

#define VQES_CP_GET_DEBUG_FLAG_STATE(type, p_bool, p_filter)            \
    debug_get_flag_state(vqes_cp_debug_arr, type, p_bool, p_filter)

#define VQES_CP_RESET_DEBUG_FLAG(type)                                  \
    debug_reset_flag(vqes_cp_debug_arr, type, VQENAME_VQES_CP, NULL)

#define IS_VQES_CP_DEBUG_FLAG_ON(type)          \
    (*(vqes_cp_debug_arr[type].var) == TRUE)


#define VQES_CP_DEBUG(type, p_filter, arg...)                           \
    do {                                                                \
        if (debug_check_element(vqes_cp_debug_arr, type, p_filter)) {   \
            buginf(TRUE, VQENAME_VQES_CP, FALSE, arg);                  \
        }                                                               \
    } while (0)


#endif /* __VQES_CP_DEBUG_H__ */
