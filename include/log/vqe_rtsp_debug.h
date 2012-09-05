/********************************************************************
 * vqe_rtsp_debug.h
 *
 * VQE RTSP Server debug flags
 *
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef __VQE_RTSP_DEBUG_H__
#define __VQE_RTSP_DEBUG_H__

#include <log/vqe_id.h>
#include "vqe_rtsp_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "vqe_rtsp_debug_flags.h"

/* Macros to do VQE RTP Library debug logging */
#define VQES_RTSP_SET_FILTER(type, p_filter)            \
    debug_set_filter(vqe_rtsp_debug_arr, type, p_filter)

#define VQE_RTSP_SET_DEBUG_FLAG(type)                                   \
    debug_set_flag(vqe_rtsp_debug_arr, type, VQENAME_RTSP, NULL)

#define VQE_RTSP_RESET_DEBUG_FLAG(type)                                 \
    debug_reset_flag(vqe_rtsp_debug_arr, type, VQENAME_RTSP, NULL)

#define VQE_RTSP_GET_DEBUG_FLAG_STATE(type, p_bool, p_filter)            \
    debug_get_flag_state(vqe_rtsp_debug_arr, type, p_bool, p_filter)

#define VQE_RTSP_DEBUG(type, p_filter, arg...)                          \
    do {                                                                \
        if (debug_check_element(vqe_rtsp_debug_arr, type, p_filter)) {  \
            buginf(TRUE, VQENAME_RTSP, FALSE, arg);                     \
        }                                                               \
    } while (0)


#endif /* __VQE_RTSP_DEBUG_H__ */
