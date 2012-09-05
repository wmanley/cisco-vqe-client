/*
 *------------------------------------------------------------------
 * rtp_util.c  -- 
 *
 * June 2006
 *
 * Copyright (c) 2006-2007 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "rtp.h"
#include "rtp_session.h"

/*
 * rtp_get_local_addr
 *
 * Determine the local transport address associated with a socket.
 */

boolean rtp_get_local_addr (ipsocktype socket,
                            ipaddrtype *local_addr,
                            uint16_t   *local_port)
{
    int sname_result;
    boolean rval;
    struct sockaddr_in sin;
    socklen_t socklen = sizeof(sin);

    sname_result = getsockname(socket, (struct sockaddr *)&sin, &socklen);
    if (sname_result == 0) {
        *local_addr = sin.sin_addr.s_addr;
        *local_port = sin.sin_port;
    } else {
        *local_addr = 0;
        *local_port = 0;
    }

    rval = (sname_result == 0);
    return (rval);
}


/*
 * same_rtp_source_id
 *
 * Check if the two RTP source IDs are the same.  Returns TRUE if they are the
 * same; otherwise returns FALSE.
 */
boolean same_rtp_source_id (rtp_source_id_t *s1, rtp_source_id_t *s2)
{
    /* Choose not to do memcmp() for the following reason:
     *   Unless coders are very careful about zeroing out these structs 
     * (via bzero or memset) before filling them in, the memcmp can be prone 
     * to failure, because the total size of the individual elements may not be
     * a multiple of 4 (or whatever the alignment is), so there's some padding,
     * which may or may not be set to zero. 
     */
    if (s1 && s2) {
        return (s1->ssrc != s2->ssrc || 
                s1->src_addr != s2->src_addr || 
                s1->src_port != s2->src_port) ? FALSE : TRUE;
    }

    return FALSE;
} 


/*
 * is_null_rtp_source_id
 *
 * Check if the passed in RTP source ID is NULL.
 */
boolean is_null_rtp_source_id (rtp_source_id_t *s)
{
    if (s) {
        return (s->ssrc || s->src_addr || s->src_port) ? FALSE : TRUE;
    }

    return TRUE;
}

/*
 * rtcp_msgtype_str
 *
 * Return a string corresponding to an RTCP message type.
 */
char *rtcp_msgtype_str (rtcp_type_t msgtype)
{
    switch (msgtype) {
    case RTCP_SR:
        return ("SR");
    case RTCP_RR:
        return ("RR");
    case RTCP_SDES:
        return ("SDES");
    case RTCP_BYE:
        return ("BYE");
    case RTCP_APP:
        return ("APP");
    case RTCP_RTPFB:
        return ("RTPFB");
    case RTCP_PSFB:
        return ("PSFB");
    case RTCP_XR:
        return ("XR");
    case RTCP_RSI:
        return ("RSI");
    case RTCP_PUBPORTS:
        return ("PUBPORTS");
    default:
        return ("Unknown");
    }
}
