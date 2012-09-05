/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: DP common.
 *
 * Documents: Common stuff for use throughout dataplane.
 *
 *****************************************************************************/

#ifndef __VQEC_DP_COMMON_H__
#define __VQEC_DP_COMMON_H__

#include "vam_types.h"
#include "vam_hist.h"
#include "vqec_dp_syslog_def.h"
#include "vqec_dp_debug.h"

/**
 * Dataplane malloc. 
 */
#define VQEC_DP_MALLOC(len)                     \
    ({                                          \
        void *result = VQE_MALLOC(len);                         \
        if (!result) {                                          \
            VQEC_DP_SYSLOG_PRINT(MALLOC_FAILURE, (len), __FUNCTION__);        \
        }                                                       \
        result;                                                 \
    })

/**
 * Dataplane free.
 */
#define VQEC_DP_FREE(ptr)                       \
    VQE_FREE(ptr)

/**
 * Dataplane fatal assertion.
 */
#define VQEC_DP_ASSERT_FATAL(condition, rest...)        \
    ASSERT_FATAL(condition, VQEC_DP_INTERNAL_FORCED_EXIT, rest)

/**
 * Dataplane wrapper for syslog_print().
 */
#define VQEC_DP_SYSLOG_PRINT(msg, rest...)        \
    syslog_print(VQEC_DP_ ## msg, ##rest)

#define VQEC_DP_CONSOLE_PRINTF printf

#endif /* __VQEC_DP_COMMON_H__ */
