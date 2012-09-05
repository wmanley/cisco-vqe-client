/**-----------------------------------------------------------------
 * @brief
 * RTP/RTCP Library Syslog Definitions 
 *
 * @file
 * rtp_syslog.c
 *
 * February 2007, Donghai Ma
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

/* LIBRTP */
/*------------------------------------------------------------------------*/
/* Among all the C files including it, only ONE C file should instantiate the 
 * message definitions described in XXX_syslog_def.h.   To do so, 
 * the designated C file should define the symbol SYSLOG_DEFINITION before 
 * including the file.
 */
#define SYSLOG_DEFINITION
#include <log/vqe_rtp_syslog_def.h>
#undef SYSLOG_DEFINITION

/* Include base definitions: boolean flags, enum codes */
#include <log/vqe_rtp_debug.h>
/* Declare the debug array */
#define __DECLARE_DEBUG_ARR__
#include <log/vqe_rtp_debug_flags.h>
/*------------------------------------------------------------------------*/

#include "utils/vam_util.h"

#include "rtp_session.h"
#include "rtp_util.h"

/*
 * rtp_check_debug
 *
 * Determine if an RTP debug message should be generated.
 */
boolean 
rtp_debug_check (int debug_flag, rtp_session_t *session, rtp_member_t *member)
{
    boolean flag_state;
    debug_filter_item_t *p_filter;
    debug_filter_item_t my_filter;
    rtp_member_t *p_member;
    int32_t status; 

    status = debug_get_flag_state(vqe_rtp_debug_arr, debug_flag,
                                  &flag_state, &p_filter);
    if (!flag_state) {
        return (FALSE);
    } else if (status == DEBUG_FILTER_NOT_SET) {
        return (TRUE);
    } else {
        my_filter.type = p_filter->type;
        switch (p_filter->type) {
        case DEBUG_FILTER_TYPE_CHANNEL:
            if (session == NULL) {
                return (TRUE);
            } else {
                my_filter.val = session->session_id;
            }
            break;
        case DEBUG_FILTER_TYPE_STB_IP:
            if (member) {
                p_member = member;
            } else if (session) {
                p_member = session->rtp_local_source;
            } else {
                p_member = NULL;
            }
            if (p_member == NULL) {
                return (TRUE);
            } else {
                my_filter.val = (uint64_t)(p_member->rtcp_src_addr);
            }
            break;
        default:
            return (FALSE);
        }
        return (debug_check_element(vqe_rtp_debug_arr,
                                    debug_flag,
                                    &my_filter));
    }
}

/*
 * rtp_debug_prefix
 *
 * Generate the prefix for an RTP/RTCP debug message,
 * based on the session and member data.
 */
char *
rtp_debug_prefix (rtp_session_t *session,
                  rtp_member_t *member,
                  char *buffer,
                  int buflen)
{
    char *name = NULL;
    char *addr1 = NULL;
    char *addr2 = NULL;
    uint16_t port = 0;
    rtp_member_t *p_member = NULL;
    char addr1buf[INET_ADDRSTRLEN];
    char addr2buf[INET_ADDRSTRLEN];

    if (session) {
        name = session->rtcp_mem && session->rtcp_mem->name ?
            session->rtcp_mem->name : "";
        addr1 = uint32_ntoa_r(rtp_get_session_id_addr(session->session_id),
                              addr1buf, sizeof(addr1buf));
        port = rtp_get_session_id_port(session->session_id);
    } 
    if (member) {
        p_member = member;
    } else if (session) {
        p_member = session->rtp_local_source;
    } else {
        p_member = NULL;
    }
    if (p_member) {
        addr2 = uint32_ntoa_r(p_member->rtcp_src_addr,
                              addr2buf, sizeof(addr2buf));
    } 

    if (session) {
        snprintf(buffer, buflen, " [%s %s:%u %s]",
                 name, 
                 addr1 ? addr1 : "", 
                 ntohs(port), 
                 addr2 ? addr2 : "");
    } else if (member) {
        snprintf(buffer, buflen, " [%s]", addr2 ? addr2 : "");
    } else {
        snprintf(buffer, buflen, " ");
    }

    return (buffer);
}



