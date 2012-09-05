/********************************************************************
 * vqes_rtp_debug.h
 *
 * VQE RTP/RTCP debug flags
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef __VQE_RTP_DEBUG_H__
#define __VQE_RTP_DEBUG_H__

#include <log/vqe_id.h>
#include "vqe_rtp_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "vqe_rtp_debug_flags.h"

/* Macros to do VQE RTP debug logging */
#define VQE_RTP_SET_FILTER(type, p_filter)              \
    debug_set_filter(vqe_rtp_debug_arr, type, p_filter)

#define VQE_RTP_RESET_FILTER(type)              \
    debug_reset_filter(vqe_rtp_debug_arr, type)

#define VQE_RTP_SET_DEBUG_FLAG(type)                                    \
    debug_set_flag(vqe_rtp_debug_arr, type, VQENAME_RTP, NULL)

#define VQE_RTP_GET_DEBUG_FLAG_STATE(type, p_bool, p_filter)            \
    debug_get_flag_state(vqe_rtp_debug_arr, type, p_bool, p_filter)

#define VQE_RTP_RESET_DEBUG_FLAG(type)                                  \
    debug_reset_flag(vqe_rtp_debug_arr, type, VQENAME_RTP, NULL)


#define IS_VQE_RTP_DEBUG_FLAG_ON(type)          \
    (*(vqe_rtp_debug_arr[type].var) == TRUE)

#define VQE_RTP_DEBUG(type, p_filter, arg...)                         \
    do {                                                              \
        if (debug_check_element(vqe_rtp_debug_arr, type, p_filter)) { \
            buginf(TRUE, VQENAME_RTP, FALSE, arg);                 \
        }                                                             \
    } while (0)


#endif /* __VQE_RTP_DEBUG_H__ */
