/***********************************************
 * vqm_debug.h
 *
 * VQM debug flags
 *
 * November 2007
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ***********************************************/

#ifndef __VQM_DEBUG_H__
#define __VQM_DEBUG_H__

#include <log/vqe_id.h>
#include "vqm_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "vqm_debug_flags.h"

/* Macros to do VQM debug logging */

#define VQM_SET_DEBUG_FLAG(type)                                    \
    debug_set_flag(vqm_debug_arr, type, VQENAME_VQM, NULL)

#define VQM_GET_DEBUG_FLAG_STATE(type, p_bool, p_filter)            \
    debug_get_flag_state(vqm_debug_arr, type, p_bool, p_filter)

#define VQM_DEBUG_FLAG(type)                                  \
    debug_reset_flag(vqm_debug_arr, type, VQENAME_VQM, NULL)

#define VQE_CFG_SET_FILTER(type, p_filter)              \
    debug_set_filter(vqm_debug_arr, type, p_filter)

#define IS_VQM_DEBUG_FLAG_ON(type)          \
    (*(vqm_debug_arr[type].var) == TRUE)

#define VQM_DEBUG(type, p_filter, fmt, arg...)                      \
    do {                                                                \
        if (debug_check_element(vqm_debug_arr, type, p_filter)) {   \
            buginf(TRUE, VQENAME_VQM, FALSE, fmt, ##arg);       \
        }                                                               \
    } while (0)


#endif /* __VQM_DEBUG_H__ */

