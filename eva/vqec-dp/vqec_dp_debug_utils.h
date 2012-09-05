/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: DP debug utilities.  These may rely on the data plane types.
 *
 *****************************************************************************/

#include "vqec_dp_api_types.h"

#ifndef __VQEC_DP_DEBUG_UTILS_H__
#define __VQEC_DP_DEBUG_UTILS_H__

/**
 * Generates a string that describes the return code of a function.
 * Useful for including the result of a data plane API call within a debug
 * statement.  The generated string is similar to that produced by 
 * vqec_dp_err2str(), except that:
 *  1) the string is empty if the error code is VQEC_DP_ERR_OK, and
 *  2) otherwise, a string consisting of ": " and the error description
 *     is returned
 *
 * NOTE:  the contents of the returned string should be immediately
 *        used or copied, as subsequent calls to this function will
 *        overwrite the contents of the string.
 *
 * @param[in] err           Error code
 * @param[out] char *       Descriptive string for error code (if failure)
 */ 
char *
vqec_dp_err2str_complain_only(vqec_dp_error_t status);

/**
 * Generates a string that describes the return code of a function.
 * Useful for including the result of a DP stream API call within a debug
 * statement.  The generated string is similar to that produced by 
 * vqec_dp_stram_err2str(), except that:
 *  1) the string is empty if the error code is VQEC_DP_STREAM_ERR_OK, and
 *  2) otherwise, the error code is preceded by ": "
 *
 * NOTE:  the contents of the returned string should be immediately
 *        used or copied, as subsequent calls to this function will
 *        overwrite the contents of the string.
 *
 * @param[in] err           Error code
 * @param[out] char *       Descriptive string for error code (if failure)
 */ 
char *
vqec_dp_stream_err2str_complain_only(vqec_dp_stream_err_t status);

/**
 * Generates a string that describes the contents of a vqec_dp_input_filter_t
 * structure.  Useful for logging API invocations.
 *
 * NOTE:  the contents of the returned string should be immediately
 *        used or copied, as subsequent calls to this function will
 *        overwrite the contents of the string.
 *
 * @param[in] fil     - filter to be described
 * @param[in] proto   - TRUE to include protocol in string, FALSE to omit
 * @param[in] src     - TRUE to include source filter in string, FALSE to omit
 * @param[in] dst     - TRUE to include dest filter in string, FALSE to omit
 * @param[out] char * - descriptive string
 */
char *
vqec_dp_input_filter_to_str(vqec_dp_input_filter_t *fil,
                            boolean proto,
                            boolean src,
                            boolean dst);

/**
 * Generates a string that describes the contents of a vqec_dp_rtp_src_key_t
 * structure.  Useful for logging.
 *
 * NOTE:  the contents of the returned string should be immediately
 *        used or copied, as subsequent calls to this function will
 *        overwrite the contents of the string.
 *
 * @param[in] key     - RTP source key to be described
 * @param[out] char * - descriptive string
 */
char *
vqec_dp_rtp_key_to_str(vqec_dp_rtp_src_key_t *key);

/**
 * Returns a string that describes the type of a stream.
 *
 * @param[in] type           - stream type to be described
 * @param[out] const char *  - descriptive string
 */
const char *
vqec_dp_input_stream_type_to_str(vqec_dp_input_stream_type_t type);

#endif /* __VQEC_DP_DEBUG_UTILS_H__ */
