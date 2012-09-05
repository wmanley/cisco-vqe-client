/********************************************************************
 * vqes_rpc_debug.h
 *
 * VQE RPC debug flags
 *
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef __VQE_RPC_DEBUG_H__
#define __VQE_RPC_DEBUG_H__

#include <log/vqe_id.h>
#include "vqe_rpc_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "vqe_rpc_debug_flags.h"

/* Macros to do VQE RPC debug logging */
#define VQE_RPC_SET_FILTER(type, p_filter)              \
    debug_set_filter(vqe_rpc_debug_arr, type, p_filter)

#define VQE_RPC_RESET_FILTER(type)              \
    debug_reset_filter(vqe_rpc_debug_arr, type)

#define VQE_RPC_SET_DEBUG_FLAG(type)                                    \
    debug_set_flag(vqe_rpc_debug_arr, type, VQENAME_RPC, NULL)

#define VQE_RPC_GET_DEBUG_FLAG_STATE(type, p_bool, p_filter)            \
    debug_get_flag_state(vqe_rpc_debug_arr, type, p_bool, p_filter)

#define VQE_RPC_RESET_DEBUG_FLAG(type)                                  \
    debug_reset_flag(vqe_rpc_debug_arr, type, VQENAME_RPC, NULL)


#define IS_VQE_RPC_DEBUG_FLAG_ON(type)          \
    (*(vqe_rpc_debug_arr[type].var) == TRUE)

#define VQE_RPC_DEBUG(type, arg...)                         \
    do {                                                    \
        if (IS_VQE_RPC_DEBUG_FLAG_ON(type)) {               \
            buginf(TRUE, VQENAME_RPC, FALSE, arg);          \
        }                                                   \
    } while (0)


#endif /* __VQE_RPC_DEBUG_H__ */
