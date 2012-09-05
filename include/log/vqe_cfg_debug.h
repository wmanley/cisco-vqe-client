/********************************************************************
 * vqe_cfg_debug.h
 *
 * VQE CFG Library debug flags
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef __VQE_CFG_DEBUG_H__
#define __VQE_CFG_DEBUG_H__

#include <log/vqe_id.h>
#include "vqe_cfg_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "vqe_cfg_debug_flags.h"

/* Macros to do VQE CFG Library debug logging */

#define VQE_CFG_SET_DEBUG_FLAG(type)                                 \
    debug_set_flag(vqe_cfg_debug_arr, type, VQENAME_LIBCFG, NULL)

#define VQE_CFG_RESET_DEBUG_FLAG(type)                               \
    debug_reset_flag(vqe_cfg_debug_arr, type, VQENAME_LIBCFG, NULL)

#define VQE_CFG_GET_DEBUG_FLAG_STATE(type, p_bool, p_filter)            \
    debug_get_flag_state(vqe_cfg_debug_arr, type, p_bool, p_filter)

#define VQE_CFG_SET_FILTER(type, p_filter)                              \
    debug_set_filter(vqe_cfg_debug_arr, type, p_filter)

#define VQE_CFG_DEBUG(type, p_filter, fmt, arg...)                     \
    do {                                                              \
        if (debug_check_element(vqe_cfg_debug_arr, type, p_filter)) {  \
            buginf(TRUE, VQENAME_LIBCFG, FALSE, fmt, ##arg);            \
        }                                                             \
    } while (0)


#endif /* __VQE_CFG_DEBUG_H__ */
