/**-----------------------------------------------------------------
 * @brief 
 * Declarations/definitions for APIs for RTCP packet construction.
 *
 * @file
 * rtcp_pkt_api.h
 *
 * June 2007, Mike Lague.
 *
 * Copyright (c) 2007-2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __RTCP_PKT_API_H__
#define __RTCP_PKT_API_H__

#include "rtcp.h"

/*
 * The structures and functions defined here allow an 
 * application-level class to add to the default content
 * of an RTCP report (compound packet).
 * 
 * There are two categories of compound packets which may be modified:
 * 1. regularly "scheduled" RTCP reports, sent every RTCP interval.
 * 2. "unscheduled" RTCP reports, sent when a participant needs to
 *    send information immediately, e.g., for feedback purposes, 
 *    or to leave the session.
 *   
 * By default, RTCP compound packets take on one of two forms:
 * 1. RR + SDES
 * 2. SR + SDES
 * 3. SDES only (this is if rtcp-rsize is enabled)
 *
 * The choice of Receiver Report (RR) or Sender Report (SR) is determined
 * by RTCP rules. At minimum, the compound packet also contains a Source
 * Description (SDES) packet with a CNAME (canonical name) item.
 * 
 * To add to these defaults, there are two techniques for specifying 
 * additional content.
 * 1. To add to the content of every report, scheduled or unscheduled,
 *    the application-level class should override the rtcp_construct_report
 *    method. Typically, such an override function will:
 *    - make a copy of the rtcp_pkt_info_t struct (if any) 
 *      that is passed to the method;
 *    - specify additional content, by applying rtcp_set_msg_info (defined
 *      here) to the rtcp_pkt_info_copy; for proper operation of packet
 *      formamtting, information should only  be added, not overridden or 
 *      deleted.
 *    - invoke the parent version of the method (e.g., 
 *      rtcp_construct_report_base) with the modified rtcp_pkt_info_t data.
 * 2. To add to the content of an unscheduled report, application-level code
 *    should invoke the rtcp_send_report method, passing an rtcp_pkt_info_t 
 *    struct with the additional packet information.
 * 
 * Examples:
 * a. An application-level class for a session participant behind a NAT
 *    may wish to include a PUBPORTS message in every report; technique
 *    1. applies.
 * b. A participant wishing to leave a session should invoke the 
 *    rtcp_send_report method with an rtcp_pkt_info_t struct specifying
 *    BYE information.
 * c. A participant wishing to send feedback should invoke the 
 *    rtcp_send_report method with an rtcp_pkt_info_t struct specifying
 *    either RTPFB or PSFB information.
 * d. A participant wishing to send RTPFB without RR should invoke the 
 *    rtcp_send_report method with the boolean rtcp-rsize flag enabled
 */

/* BYE info */

typedef struct rtcp_bye_info_t_ {
    uint8_t reason_len;  /* zero means that no reason data should be added 
                            currently, non-zero reasons are ignored */
    uint8_t *reason;     /* reason data: for future use */
} rtcp_bye_info_t;

/* APP info */

typedef struct rtcp_app_info_t_ {
    uint32_t name;
    uint32_t len;
    uint8_t *data;
} rtcp_app_info_t;

/* RTPFB info */

typedef struct rtcp_rtpfb_info_t_ {
    rtcp_rtpfb_fmt_t fmt;        /* only "generic NACK" is supported */
    uint32_t ssrc_media_sender;
    uint32_t         num_nacks;  /* no. of pid/bitmask pairs specified */
    /* ptr to array of 'num_nacks' pid/bitmask pairs in host order */    
    rtcp_rtpfb_generic_nack_t *nack_data; 
} rtcp_rtpfb_info_t;

/* PSFB info */

typedef struct rtcp_psb_info_t_ {
    rtcp_psfb_fmt_t fmt;   /* only "PLI" is supported */
    uint32_t ssrc_media_sender;
} rtcp_psfb_info_t;

/* XR info */

typedef struct rtcp_xr_info_t_ {
    boolean rtcp_xr_enabled;    /* indicates when the rtcp xr stats being
                                   collected or not */
} rtcp_xr_info_t;

/* RSI info */

typedef struct rtcp_rsi_info_t_ {
    rtcp_rsi_subrpt_mask_t subrpt_mask;  /* indicates which subreports
                                            should be sent */
} rtcp_rsi_info_t;

/* PUBPORTS info -- in the same format as in the msg, but in host order */

typedef rtcp_pubports_t rtcp_pubports_info_t ;
    
/*
 * rtcp_msg_info_t 
 *
 * Info for one packet (message) type
 */

typedef union rtcp_msg_info_t_ {
    rtcp_bye_info_t      bye_info;
    rtcp_app_info_t      app_info;   
    rtcp_rtpfb_info_t    rtpfb_info;
    rtcp_psfb_info_t     psfb_info;
    rtcp_rsi_info_t      rsi_info;
    rtcp_pubports_info_t pubports_info;
    rtcp_xr_info_t       xr_info;
} rtcp_msg_info_t;
    
/*
 * rtcp_init_msg_info
 *
 * Initialize an rtcp_msg_info_t union.
 *
 * Parameters:
 * msg_info    -- ptr to rtcp_msg_info_t union
 */
static inline
void rtcp_init_msg_info (rtcp_msg_info_t *msg_info) 
{
    memset(msg_info, 0, sizeof(rtcp_msg_info_t));
}

/*
 * rtcp_pkt_info_t
 *
 * Specifies additional compound packet content
 * (beyond the default of [{ SR | RR }] + SDES(w/CNAME) )
 */

typedef struct rtcp_pkt_info_t_ {
    rtcp_msg_mask_t msg_mask;  /* indicates what per-message type
                                  information is present */
    /* 
     * if msg_info[RTCP_MSGTYPE_IDX(msgtype)] is set (i.e., non-NULL), 
     * it's a pointer to info for the corresponding msgtype (e.g., RTCP_APP)
     */
    rtcp_msg_info_t *msg_info[RTCP_NUM_MSGTYPES];
    /* 
     * Note that application-level control of SDES packet is currently 
     * not provided (though SR/RR may be controlled by the application) 
     * - an SR or RR packet will be present as the first packet
     *   unless the application decides to drop the RR packet if rtcp-rsize
     *   is enabled.
     * - an SDES packet will always be present, and will contain a CNAME item 
     *   
     *  Thus msg info for RTCP_SR, RTCP_RR, and RTCP_SDES (e.g.,
     *  msg_info[RTCP_MSGTYPE_IDX(RTCP_SR)], etc.) will be ignored,
     *  if present. 
     */
    uint8_t rtcp_rsize; /* Disabled by default */
} rtcp_pkt_info_t;

/*
 * rtcp_init_pkt_info
 * 
 * Initialize an rtcp_pkt_info_t struct.
 *
 * Parameters:
 * pkt_info    -- ptr to rtcp_pkt_info_t struct
 */
static inline
void rtcp_init_pkt_info (rtcp_pkt_info_t *pkt_info) 
{
    memset(pkt_info, 0, sizeof(rtcp_pkt_info_t));
}

/*
 * rtcp_set_pkt_info
 *
 * Add per-message (per-packet type) info to a
 * rtcp_pkt_info_t struct. 
 *
 * Parameters:
 * pkt_info    -- ptr to rtcp_pkt_info_t struct
 * type        -- RTCP message type
 * msg_info    -- ptr to rtcp_msg_info_t struct, set up for
 *                the message type, e.g., if type is RTCP_APP
 *                then msg_info->app_info should be set up.
 */
static inline
void rtcp_set_pkt_info (rtcp_pkt_info_t *pkt_info,
                        rtcp_type_t type,
                        rtcp_msg_info_t *msg_info) 
{
    if (pkt_info && RTCP_MSGTYPE_OK(type)) {
        int idx = RTCP_MSGTYPE_IDX(type);
        pkt_info->msg_info[idx] = msg_info;
        rtcp_set_msg_mask(&pkt_info->msg_mask, type);
    }
}

/*
 * rtcp_get_pkt_info
 *
 * Retrieve per-message (per-packet type) info from a
 * rtcp_pkt_info_t struct.
 *
 * Parameters:
 * pkt_info    -- ptr to rtcp_pkt_info_t struct
 * type        -- RTCP message type
 * Returns:    -- ptr to corresponding rtcp_msg_info_t struct,
 *                or NULL if not specified, or type is invalid.
 */

static inline
rtcp_msg_info_t *rtcp_get_pkt_info (rtcp_pkt_info_t *pkt_info,
                                    rtcp_type_t type)
{
    return (pkt_info && RTCP_MSGTYPE_OK(type) ? 
            pkt_info->msg_info[RTCP_MSGTYPE_IDX(type)] : NULL);
}

static inline
uint8_t rtcp_set_rtcp_rsize (rtcp_pkt_info_t *pkt_info, uint8_t value)
{
    if (pkt_info) {
        pkt_info->rtcp_rsize = value; 
        return TRUE;
    }
    return FALSE;
}

static inline
uint8_t rtcp_get_rtcp_rsize (rtcp_pkt_info_t *pkt_info, uint8_t *value)
{
    if (pkt_info) {
        *value = pkt_info->rtcp_rsize;
        return TRUE;
    }
    return FALSE;
}

#endif /* __RTCP_PKT_API_H__ */
