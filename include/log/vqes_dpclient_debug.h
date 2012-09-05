/********************************************************************
 * vqes_dpclient_debug.h
 *
 * VQE Server Data Plane debug flags
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef __VQES_DPCLIENT_DEBUG_H__
#define __VQES_DPCLIENT_DEBUG_H__

#include <log/vqe_id.h>
#include "vqes_dpclient_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "vqes_dpclient_debug_flags.h"

/* Macros to do VQES Data Plane debug logging */
#define VQES_DPCLIENT_SET_FILTER(type, p_filter)                              \
    debug_set_filter(vqes_dpclient_debug_arr, type, p_filter)

#define VQES_DPCLIENT_SET_DEBUG_FLAG(type)                                    \
    debug_set_flag(vqes_dpclient_debug_arr, type, VQENAME_VQES_DPCLIENT, NULL)

#define VQES_DPCLIENT_RESET_DEBUG_FLAG(type)                                  \
    debug_reset_flag(vqes_dpclient_debug_arr, type, VQENAME_VQES_DPCLIENT, NULL)

#define VQES_DPCLIENT_DEBUG(type, p_filter, arg...)                           \
    do {                                                                \
        if (debug_check_element(vqes_dpclient_debug_arr, type, p_filter)) {   \
            buginf(TRUE, VQENAME_VQES_DPCLIENT, FALSE, arg);                  \
        }                                                               \
    } while (0)


#endif /* __VQES_DPCLIENT_DEBUG_H__ */
