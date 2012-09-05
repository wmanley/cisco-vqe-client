/********************************************************************
 * vqes_pm_debug.h
 *
 * VQE-S Process Monitor debug flags
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/


#ifndef __VQES_PM_DEBUG_H__
#define __VQES_PM_DEBUG_H__

#include <log/vqe_id.h>
#include "vqes_pm_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "vqes_pm_debug_flags.h"

/* Macros to do VQE-S PM debug logging */
#define VQES_PM_SET_FILTER(type, p_filter)              \
    debug_set_filter(vqe_pm_debug_arr, type, p_filter)

#define VQES_PM_SET_DEBUG_FLAG(type)                                    \
    debug_set_flag(vqes_pm_debug_arr, type, VQENAME_VQES_PM, NULL)

#define VQES_PM_GET_DEBUG_FLAG_STATE(type, p_bool, p_filter)            \
    debug_get_flag_state(vqes_pm_debug_arr, type, p_bool, p_filter)

#define VQES_PM_RESET_DEBUG_FLAG(type)                                  \
    debug_reset_flag(vqes_pm_debug_arr, type, VQENAME_VQES_PM, NULL)

#define VQES_PM_DEBUG(type, p_filter, arg...)                         \
    do {                                                              \
        if (debug_check_element(vqes_pm_debug_arr, type, p_filter)) { \
            buginf(TRUE, VQENAME_VQES_PM, FALSE, arg);                \
        }                                                             \
    } while (0)


#endif /* __VQES_PM_DEBUG_H__ */
