/**-----------------------------------------------------------------
 * @brief 
 * Functions for updating RTCP statistics.
 *
 * @file
 * rtcp_statistics.c
 *
 * March 2007, Mike Lague.
 *
 * Copyright (c) 2007-2008, 2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include "rtcp_statistics.h"

/*
 * rtcp_get_msg_stats
 *
 * Get the pointer to the correct per-messages stats block,
 * given the session stats ptr, and the msg category and/or type.
 */
static inline
rtcp_item_stats_t *rtcp_get_msg_stats (rtcp_session_stats_t *stats,
                                       rtcp_type_t           type)
{
    return ((RTCP_MSGTYPE_OK(type)) ?
            &stats->msgs[RTCP_MSGTYPE_IDX(type)] :
            &stats->unknown_msgs);
}

/*
 * rtcp_upd_avg_pkt_size
 *
 * Update average packet size
 */
double rtcp_upd_avg_pkt_size (double avg_pkt_size, uint32_t rtcp_len)
{
    double pkt_size  = (double)(rtcp_len + UDPHEADERBYTES + IPHEADERSIZE);

    return ((1./16.)*pkt_size + (15./16.)*avg_pkt_size);
}

/*
 * rtcp_upd_pkt_rcvd_counts
 *
 * Updates RTCP compound packet statistics,
 * upon receipt of a packet.
 */
void rtcp_upd_pkt_rcvd_counts (rtcp_session_stats_t *stats,
                               uint32_t rtcp_len,
                               boolean  parsed_ok,
                               boolean rsize_pkt)
{
    if (parsed_ok) {
        stats->pkts.rcvd++;
    } else {
        stats->pkts.rcvd_errors++;
    }
    if(rsize_pkt) {
        stats->rsize_pkts++;
    }
    stats->avg_pkt_size = 
        rtcp_upd_avg_pkt_size(stats->avg_pkt_size, rtcp_len);
}

/*
 * rtcp_upd_msg_rcvd_counts
 *
 * Updates RTCP statistics kept per message type,
 * upon receipt of a message
 */
void rtcp_upd_msg_rcvd_counts (rtcp_session_stats_t *stats,
                               rtcp_type_t msg_type,
                               rtcp_parse_status_t status)
{
    rtcp_item_stats_t *msgs = rtcp_get_msg_stats(stats, msg_type);
    rtcp_item_stats_t *pkts = &stats->pkts;

    if (status == RTCP_PARSE_OK) {
        msgs->rcvd++;
    } else {
        msgs->rcvd_errors++;
        switch (status) {
        case RTCP_PARSE_BADVER:
            msgs->badver++;
            pkts->badver++;
            break;
        case RTCP_PARSE_RUNT:
            msgs->runts++;
            pkts->runts++;
            break;
        case RTCP_PARSE_BADLEN:
            msgs->badlen++;
            pkts->badlen++;
            break;
        case RTCP_PARSE_UNEXP:
            msgs->unexp++;
            pkts->unexp++;
            break;
        default:
            break;
        }
    }
}

/*
 * rtcp_upd_pkt_sent_counts
 *
 * Updates RTCP compound packet statistics,
 * and statistics kept per RTCP message type
 * upon tranmission of a packet, or the failure to transmit.
 */
void rtcp_upd_pkt_sent_counts (rtcp_session_stats_t *stats,
                               uint8_t *rtcp_pkt,
                               uint32_t rtcp_len,
                               boolean sent_ok)
{
    uint8_t *rtcp_end;
    rtcptype *rtcp_hdr;
    rtcp_type_t msg_type;
    rtcp_msg_mask_t mask = RTCP_MSG_MASK_0;
    rtcp_item_stats_t *msgs;

    if (!rtcp_len) {
        /* zero-length packet: */
        if (sent_ok) {
            /*
             * zero-length "ok" packets are due to no RTCP bandwidth:
             * don't increment any counters
             */
        } else {
            /* 
             * zero-length "send failures" are due to 
             * packet construction errors (no room in buffer);
             * increment send errors for compound packets,
             * and for the "unknown" packet type.
             */
            stats->pkts.send_errors++;
            msgs = rtcp_get_msg_stats(stats, NOT_AN_RTCP_MSGTYPE);
            msgs->send_errors++;
        }
        return;
    }

    if (sent_ok) {
        stats->pkts.sent++;
    } else {
        stats->pkts.send_errors++;
    }
    stats->avg_pkt_size = 
        rtcp_upd_avg_pkt_size(stats->avg_pkt_size, rtcp_len);
    stats->avg_pkt_size_sent = 
        rtcp_upd_avg_pkt_size(stats->avg_pkt_size_sent, rtcp_len);

    /*
     * get the set of individual message types in this compound packet
     */
    for (rtcp_end = rtcp_pkt + rtcp_len ; 
         (rtcp_pkt + 4) <= rtcp_end ; 
         rtcp_pkt += ((ntohs(rtcp_hdr->len) + 1) * 4)) {
        rtcp_hdr = (rtcptype *)rtcp_pkt;
        msg_type = rtcp_get_type(ntohs(rtcp_hdr->params));
        rtcp_set_msg_mask(&mask, msg_type);
    }

    for (msg_type = rtcp_get_first_msg(mask); 
         RTCP_MSGTYPE_OK(msg_type);
         msg_type = rtcp_get_next_msg(mask, msg_type)) {
        msgs = rtcp_get_msg_stats(stats, msg_type);
        if (sent_ok) {
            msgs->sent++;
        } else {
            msgs->send_errors++;
        }
    }   
}

