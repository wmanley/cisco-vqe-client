/**-----------------------------------------------------------------
 * @brief 
 * Declarations/definitions for RTCP statistics data/functions.
 *
 * @file
 * rtcp_statistics.h
 *
 * March 2007, Mike Lague.
 *
 * Copyright (c) 2007, 2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __RTCP_STATISTICS_H__
#define __RTCP_STATISTICS_H__

#include "rtcp.h"
#include "rtcp_bandwidth.h"

/*
 * Sesssion statistics
 */

typedef uint64_t rtcp_counter_t;  /* use 64-bit counters */

/*
 * rtcp_item_stats_t
 *
 * Basic statistics, which may be kept either 
 * . for RTCP compound packets, or 
 * . for individual RTCP message types
 */
typedef struct rtcp_item_stats_t_ {
    rtcp_counter_t sent;          /* items successfully sent */
    rtcp_counter_t send_errors;   /* items which failed to be sent */
    rtcp_counter_t rcvd;          /* items received (w/ or w/o errors) */
    rtcp_counter_t rcvd_errors;   /* items w/ one or more parse errors */
    /*
     * detailed breakdown of parse errors: 
     * note that when kept for compound msgs, these are summed over all 
     * individual message types, so in this case the sum of these
     * counters may exceed rcvd_errors.
     */
    rtcp_counter_t badver; /* version number is incorrect */
    rtcp_counter_t runts;  /* applies to a msg at the end of a compound pkt:
                              either the msg was shorter than the minimum
                              length of a msg (4 bytes), or the msg was 
                              shorter than the length indicated in the hdr */
    rtcp_counter_t badlen; /* length is not valid for the given msg type,
                              e.g., too short or long, or inconsistent with
                              count information in the hdr */
    rtcp_counter_t unexp;  /* the msg type is either: unexpected
                              in its position (only certain types are
                              allowed as the first message in a packet),
                              unexpected by the application-level
                              derived class, or is an unknown msg type */
} rtcp_item_stats_t;

/*
 * rtcp_session_stats_t
 *
 * Statistics kept for RTCP compound packets,
 * as well as for each individual RTCP message type.
 */

typedef struct rtcp_session_stats_t_ {
    /*
     * statistics for RTCP compound packets
     */
    rtcp_item_stats_t  pkts;      /* counters for RTCP compound packets */
    double             avg_pkt_size;   /* avg compound pkt size, 
                                          over all pkts sent and rcvd */
    double             avg_pkt_size_sent;   /* avg compound pkt size,
                                               over all pkts sent */
    /*
     * statistics kept by individual RTCP message type (e.g., RR):
     */
    rtcp_item_stats_t   msgs[RTCP_NUM_MSGTYPES];
    rtcp_item_stats_t   unknown_msgs;  /* for all unknown msg types */

    /* counter for number of RTCP Reduced Size packets 
       received and parsed successfully */
    rtcp_counter_t      rsize_pkts;
    
} rtcp_session_stats_t;

/*
 * functions to update statistics
 */

double rtcp_upd_avg_pkt_size(double avg_pkt_size, 
                             uint32_t rtcp_len);

void rtcp_upd_pkt_rcvd_counts(rtcp_session_stats_t *stats,
                              uint32_t rtcp_len,
                              boolean  parsed_ok,
                              boolean rsize_pkt);
void rtcp_upd_msg_rcvd_counts(rtcp_session_stats_t *stats,
                              rtcp_type_t msg_type,
                              rtcp_parse_status_t status);

void rtcp_upd_pkt_sent_counts(rtcp_session_stats_t *stats,
                              uint8_t *rtcp_pkt,
                              uint32_t rtcp_len,
                              boolean sent_ok);

#endif /* __RTCP_STATISTICS_H__ */
