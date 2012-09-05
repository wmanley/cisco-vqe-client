/********************************************************************
 * stunsvr_debug.h
 *
 * STUN Server debug flags
 *
 * October 2007, Donghai Ma
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef __STUNSVR_DEBUG_H__
#define __STUNSVR_DEBUG_H__

#include <log/vqe_id.h>
#include "stunsvr_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "stunsvr_debug_flags.h"

/* Macros to do STUN Server debug logging */

#define STUNSVR_SET_DEBUG_FLAG(type)                                    \
    debug_set_flag(stunsvr_debug_arr, type, VQENAME_STUN_SERVER, NULL)

#define STUNSVR_GET_DEBUG_FLAG_STATE(type, p_bool, p_filter)            \
    debug_get_flag_state(stunsvr_debug_arr, type, p_bool, p_filter)

#define STUNSVR_RESET_DEBUG_FLAG(type)                                  \
    debug_reset_flag(stunsvr_debug_arr, type, VQENAME_STUN_SERVER, NULL)

#define STUNSVR_SET_FILTER(type, p_filter)              \
    debug_set_filter(stunsvr_debug_arr, type, p_filter)

#define STUNSVR_RESET_FILTER(type)              \
    debug_reset_filter(stunsvr_debug_arr, type)

#define IS_STUNSVR_DEBUG_FLAG_ON(type)          \
    (*(stunsvr_debug_arr[type].var) == TRUE)

#define STUNSVR_DEBUG(type, p_filter, fmt, arg...)                      \
    do {                                                                \
        if (debug_check_element(stunsvr_debug_arr, type, p_filter)) {   \
            buginf(TRUE, VQENAME_STUN_SERVER, FALSE, fmt, ##arg);       \
        }                                                               \
    } while (0)

#define STUNSVR_FLTD_DEBUG(type, stb_ip, fmt, arg...)                   \
    do {                                                                \
        debug_filter_item_t stb_filter  = {DEBUG_FILTER_TYPE_STB_IP,  0}; \
        stb_filter.val = stb_ip;                                        \
        if (debug_check_element(stunsvr_debug_arr, type, &stb_filter)) { \
            buginf(TRUE, VQENAME_STUN_SERVER, FALSE, fmt, ##arg);       \
        }                                                               \
    } while (0)

#endif /* __STUNSVR_DEBUG_H__ */

