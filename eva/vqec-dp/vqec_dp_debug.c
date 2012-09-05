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
 * Description: Implementation for VQE-C DP debug flag functions.
 *
 * Documents: 
 *
 *****************************************************************************/

#include <vqec_dp_api.h>
#include <vqec_dp_debug.h>

/**
 * Enable a DP debug flag.
 *
 * @param[in] type The flag type to enable.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_debug_flag_enable (uint32_t type)
{
    VQEC_DP_SET_DEBUG_FLAG(type);
    return (VQEC_DP_ERR_OK);
}

/**
 * Disable a DP debug flag.
 *
 * @param[in] type The flag type to disable.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_debug_flag_disable (uint32_t type)
{
    VQEC_DP_RESET_DEBUG_FLAG(type);
    return (VQEC_DP_ERR_OK);
}

/**
 * Get the status of a DP debug flag.
 *
 * @param[in] type The flag type to get the status on.
 * @param[out] flag Returns whether or not the flag is enabled.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_debug_flag_get (uint32_t type, boolean *flag)
{
    if (flag) {
        *flag = VQEC_DP_GET_DEBUG_FLAG(type);
        return (VQEC_DP_ERR_OK);
    }

    return (VQEC_DP_ERR_INVALIDARGS);
}
