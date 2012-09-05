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
 * Description:  DP debug utilities.
 *
 *****************************************************************************/

#include <utils/vam_util.h>
#include "vqec_dp_api_types.h"
#include "vqec_dp_io_stream.h"
#include "vqec_recv_socket.h" /* for inet_ntop_static_buf() */

#define DEBUG_STR_LEN 200

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
vqec_dp_err2str_complain_only (vqec_dp_error_t status)
{
    static char s_debug_str[DEBUG_STR_LEN];

    if (status != VQEC_DP_ERR_OK) {
        snprintf(s_debug_str, DEBUG_STR_LEN, ": %s", vqec_dp_err2str(status));
    } else {
        s_debug_str[0] = '\0';
    }
    return (s_debug_str);
}

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
vqec_dp_stream_err2str_complain_only (vqec_dp_stream_err_t status)
{
    static char s_debug_str[DEBUG_STR_LEN];

    if (status != VQEC_DP_STREAM_ERR_OK) {
        snprintf(s_debug_str, DEBUG_STR_LEN, ": %s",
                 vqec_dp_stream_err2str(status));
    } else {
        s_debug_str[0] = '\0';
    }
    return (s_debug_str);
}

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
vqec_dp_input_filter_to_str (vqec_dp_input_filter_t *fil,
                             boolean proto,
                             boolean src,
                             boolean dst)
{
    static char s_debug_str[DEBUG_STR_LEN];
    static char s_debug_str_proto[DEBUG_STR_LEN];
    static char s_debug_str_src[DEBUG_STR_LEN];
#define DEBUG_STR_LEN_PORT 8
    static char s_debug_str_src_port[DEBUG_STR_LEN_PORT];
    static char s_debug_str_dst[DEBUG_STR_LEN];

    if (!fil || !(proto || src || dst)) {
        snprintf(s_debug_str, DEBUG_STR_LEN, "{NULL}");
        goto done;
    }

    /* build proto substring */
    if (proto) {
        snprintf(s_debug_str_proto, DEBUG_STR_LEN, "prot=%s,",
                 fil->proto == INPUT_FILTER_PROTO_UDP ? "udp" : "unknown");
    } else {
        s_debug_str_proto[0] = '\0';
    }

    /* build source substring */
    if (src && (fil->u.ipv4.src_ip_filter || fil->u.ipv4.src_port_filter)) {
        if (fil->u.ipv4.src_port_filter) {
            snprintf(s_debug_str_src_port, DEBUG_STR_LEN_PORT,
                     "%u", ntohs(fil->u.ipv4.src_port));
        }
        snprintf(s_debug_str_src, DEBUG_STR_LEN,
                 "src=(%s:%s),",
                 (fil->u.ipv4.src_ip_filter ? 
                  inet_ntop_static_buf(fil->u.ipv4.src_ip,
                                       INET_NTOP_STATIC_BUF0) : "<any>"),
                 (fil->u.ipv4.src_port_filter ?
                  s_debug_str_src_port : "<any>"));
    } else {
        s_debug_str_src[0] = '\0';
    }

    /* build destination substring */
    if (dst && IN_MULTICAST(ntohl(fil->u.ipv4.dst_ip))) {
        /* multicast dst filter */
        snprintf(s_debug_str_dst, DEBUG_STR_LEN,
                 "dst=(%s:%u,ifaddr=%s)",
                 inet_ntop_static_buf(fil->u.ipv4.dst_ip, 
                                      INET_NTOP_STATIC_BUF0),
                 ntohs(fil->u.ipv4.dst_port),
                 inet_ntop_static_buf(fil->u.ipv4.dst_ifc_ip,
                                      INET_NTOP_STATIC_BUF1));
    } else if (dst) {
        /* unicast dst filter */
        snprintf(s_debug_str_dst, DEBUG_STR_LEN,
                 "dst=(%s:%u)",
                 inet_ntop_static_buf(fil->u.ipv4.dst_ip, 
                                      INET_NTOP_STATIC_BUF0),
                 ntohs(fil->u.ipv4.dst_port));
    } else {
        s_debug_str_dst[0] = '\0';
    }

    /* generate the string */
    snprintf(s_debug_str, DEBUG_STR_LEN, "{%s%s%s}",
             s_debug_str_proto, s_debug_str_src, s_debug_str_dst);
done:
    return (s_debug_str);
}


/**
 * Generates a string that describes the contents of a vqec_dp_rtp_src_key_t
 * structure.
 *
 * NOTE:  the contents of the returned string should be immediately
 *        used or copied, as subsequent calls to this function will
 *        overwrite the contents of the string.
 *
 * @param[in] key     - RTP source key to be described
 * @param[out] char * - descriptive string
 */
char *
vqec_dp_rtp_key_to_str(vqec_dp_rtp_src_key_t *key)
{
#define VQEC_RTP_KEY_STR_LEN (44+INET_ADDRSTRLEN)
    static char s_str[VQEC_RTP_KEY_STR_LEN];
    char str_ip[INET_ADDRSTRLEN];
    
    if (key) {
        snprintf(s_str, VQEC_RTP_KEY_STR_LEN, "(ssrc=%8x, src ip:%s port:%d)",
                 key->ssrc,
                 uint32_ntoa_r(key->ipv4.src_addr.s_addr, 
                               str_ip, sizeof(str_ip)),
                 ntohs(key->ipv4.src_port));
    } else {
        snprintf(s_str, VQEC_RTP_KEY_STR_LEN, "(NULL)");
    }
    return (s_str);
}

/**
 * Returns a string that describes the type of a stream.
 *
 * @param[in] type           - stream type to be described
 * @param[out] const char *  - descriptive string
 */
const char *
vqec_dp_input_stream_type_to_str(vqec_dp_input_stream_type_t type)
{
    switch (type) {
    case VQEC_DP_INPUT_STREAM_TYPE_PRIMARY:
        return ("primary");
    case VQEC_DP_INPUT_STREAM_TYPE_REPAIR:
        return ("repair");
    case VQEC_DP_INPUT_STREAM_TYPE_FEC:
        return ("FEC");
    default:
        return ("unknown");
    }
}
