/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Input/Output stream implementation.
 *
 * Documents:
 *
 *****************************************************************************/

#include <vqec_dp_api_types.h>
#include <vqec_dp_io_stream.h>
#include <strl.h>

/**
 * Returns a descriptive error string corresponding to a vqec_dp_stream_err_t
 * error code.  If the error code is invalid, an empty string is returned.
 *
 * @param[in] err           Error code
 * @param[out] const char * Descriptive string corresponding to error code
 */ 
const char *
vqec_dp_stream_err2str (vqec_dp_stream_err_t err)
{
    switch (err) {
    case VQEC_DP_STREAM_ERR_OK:
        return "Success";
    case VQEC_DP_STREAM_ERR_NACKCAPA:
        return "Capability negotiation failed";
    case VQEC_DP_STREAM_ERR_NOSUCHSTREAM:
        return "Invalid stream ID supplied";
    case VQEC_DP_STREAM_ERR_INVALIDARGS:
        return "Invalid arguments";
    case VQEC_DP_STREAM_ERR_DUPFILTER:
        return "Duplicate stream filter not allowed";
    case VQEC_DP_STREAM_ERR_SERVICESHUT:
        return "Service is shutdown";
    case VQEC_DP_STREAM_ERR_ENCAPSMISMATCH:
        return "Requested encapsulation not available";
    case VQEC_DP_STREAM_ERR_OSALREADYCONNECTED:
        return "Output stream is already connected to an input stream";
    case VQEC_DP_STREAM_ERR_OSALREADYBOUND:
        return "Output stream is already bound to a filter";
    case VQEC_DP_STREAM_ERR_FILTERUNSUPPORTED:
        return "Stream filter is not supported";
    case VQEC_DP_STREAM_ERR_NOMEMORY:
        return "Insufficient memory";
    case VQEC_DP_STREAM_ERR_FILTERISCOMMITTED:
        return "Filter has already been committed";
    case VQEC_DP_STREAM_ERR_FILTERNOTSET:
        return "Filter not previously set";
    case VQEC_DP_STREAM_ERR_FILTERUPDATEUNSUPPORTED:
        return "Filter update request not supported";
    case VQEC_DP_STREAM_ERR_INTERNAL:
        return "Internal Error";
    default:
        return "";
    }
}

/**
 * Return a string representing the given capabilities.
 *
 * @param[in] capa Given set of capabilities.
 * @param[out] buf Buffer to print into.
 * @param[in] buf_len Length of buf.
 * @param[out] return Pointer to buf.
 */
const char *
vqec_dp_stream_capa2str (uint32_t capa,
                         char *buf,
                         int buf_len)
{
    char tmp[VQEC_DP_STREAM_PRINT_BUF_SIZE];
    int tmp_len = VQEC_DP_STREAM_PRINT_BUF_SIZE;
    memset(buf, 0, buf_len);
    memset(tmp, 0, tmp_len);

    if (!capa) {
        snprintf(buf, buf_len, "(none) ");
        return buf;
    }
    if (capa & VQEC_DP_STREAM_CAPA_PUSH) {
        snprintf(tmp, tmp_len, "PUSH ");
        (void)strlcat(buf, tmp, buf_len);
    }
    if (capa & VQEC_DP_STREAM_CAPA_PUSH_VECTORED) {
        snprintf(tmp, tmp_len, "PUSH_VECTORED ");
        (void)strlcat(buf, tmp, buf_len);
    }
    if (capa & VQEC_DP_STREAM_CAPA_PULL) {
        snprintf(tmp, tmp_len, "PULL ");
        (void)strlcat(buf, tmp, buf_len);
    }
    if (capa & VQEC_DP_STREAM_CAPA_BACKPRESSURE) {
        snprintf(tmp, tmp_len, "BACKPRESSURE ");
        (void)strlcat(buf, tmp, buf_len);
    }
    if (capa & VQEC_DP_STREAM_CAPA_RAW) {
        snprintf(tmp, tmp_len, "RAW ");
        (void)strlcat(buf, tmp, buf_len);
    }
    if (capa & VQEC_DP_STREAM_CAPA_PUSH_POLL) {
        snprintf(tmp, tmp_len, "PUSH_POLL ");
        (void)strlcat(buf, tmp, buf_len);
    }

    return buf;
}

/**
 * Return a string representing the given encapsulation.
 *
 * @param[in] capa Given encapsulation.
 * @param[out] buf Buffer to print into.
 * @param[in] buf_len Length of buf.
 * @param[out] return Pointer to buf.
 */
const char *
vqec_dp_stream_encap2str (vqec_dp_encap_type_t encap,
                          char *buf,
                          int buf_len)
{
    memset(buf, 0, buf_len);

    switch (encap) {
        case VQEC_DP_ENCAP_UNKNOWN:
            snprintf(buf, buf_len, "(unknown)");
            break;
        case VQEC_DP_ENCAP_RTP:
            snprintf(buf, buf_len, "RTP");
            break;
        case VQEC_DP_ENCAP_UDP:
            snprintf(buf, buf_len, "UDP");
            break;
    }

    return buf;
}
