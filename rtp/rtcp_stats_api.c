/**-----------------------------------------------------------------
 * @brief 
 * Functions for mgmt and internal APIs to statistics.
 *
 * @file
 * rtcp_stats_api.h
 *
 * March 2007, Mike Lague.
 *
 * Copyright (c) 2007-2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <math.h>

#include "rtcp_stats_api.h"

/*
 * management API
 *
 * rtcp_get_session_info    -- get per-session information
 * rtcp_get_session_stats   -- get per-session statistics
 * rtcp_get_sender_info     -- get per-sender information
 * rtcp_get_sender_stats    -- get per-sender statistics
 */

/*
 * rtcp_get_session_info
 *
 * Parameters:
 * sess           -- (input)  ptr to RTP/RTCP per-session data
 * info           -- (input/output) ptr to session information;
 */
void
rtcp_get_session_info (rtp_session_t *sess, 
                       rtcp_session_info_t *info)
{
    rtp_envelope_t *send_addrs;

    if (!(sess && info)) {
        return;
    }
    /*
     * Local member information 
     */
    if (sess->rtp_local_source) {
        info->ssrc = sess->rtp_local_source->ssrc;
        if (sess->rtp_local_source->sdes[RTCP_SDES_CNAME]) {
            strncpy(info->cname, 
                    sess->rtp_local_source->sdes[RTCP_SDES_CNAME],
                    RTCP_MAX_CNAME);
            info->cname[RTCP_MAX_CNAME] = '\0';
        } else {
            info->cname[0] = '\0';
        }
    } else {
        /* 
         * note that since there are no unused SSRC values,
         * zero is a possible value, but statistically unlikely 
         */
        info->ssrc = 0;
        info->cname[0] = '\0';
    }
    /* 
     * IP/UDP information for RTCP messages sent by the local member 
     */
    send_addrs = &sess->send_addrs;
    info->src_addr = send_addrs->src_addr;
    info->src_port = send_addrs->src_port;
    info->dst_addr = send_addrs->dst_addr;
    info->dst_port = send_addrs->dst_port;
    /*
     * Session membership information
     */
    info->rtcp_nmembers = sess->rtcp_nmembers;
    info->rtcp_nmembers_learned = sess->rtcp_nmembers_learned;
    info->rtcp_nsenders = sess->rtcp_nsenders;
    /*
     * RTCP report information
     */
    info->next_send_ts = sess->next_send_ts;
    info->prev_send_ts = sess->prev_send_ts;
    info->rtcp_pmembers = sess->rtcp_pmembers;
    info->rtcp_bw = sess->rtcp_bw;

    info->child_stats_present = sess->rtcp_child_stats ? TRUE : FALSE;
}

/*
 * rtcp_get_session_stats
 *
 * Parameters:
 * sess           -- (input)  ptr to RTP/RTCP per-session data
 * stats          -- (input/output) ptr to session stats: 
 * child_stats    -- (input/output) ptr to aggregated stats for
 *                   child sessions.
 */
void
rtcp_get_session_stats (rtp_session_t *sess,
                        rtcp_session_stats_t *stats,
                        rtcp_session_stats_t *child_stats)
{
    if (stats) {
        memset(stats, 0, sizeof(*stats));
        if (sess) {
            *stats = sess->rtcp_stats;
        }
    }
    if (child_stats) {
        memset(child_stats, 0, sizeof(*child_stats));
        if (sess && sess->rtcp_child_stats) {
            *child_stats = *(sess->rtcp_child_stats);
        }
    }
}

/*
 * rtcp_get_sender_info
 *
 * Parameters:
 * sess           -- (input)  ptr to RTP/RTCP per-session data
 * max_senders    -- (input) max. no. of senders for which to return info.
 * num_senders    -- (output) actual no. of senders for which info is returned.
 * sender_info    -- (input/output) ptr to array of max_senders
 *                   rtcp_session_info_t blocks.
 */
void
rtcp_get_sender_info (rtp_session_t *sess,
                      int max_senders,
                      int *num_senders,
                      rtcp_sender_info_t *info)
{
    rtp_member_t *sender = NULL;
    rtp_hdr_source_t *sender_stats = NULL;
    int sender_count = 0;
    ntp64_t sender_ntp;
    rtcp_xr_stats_t *xr_stats = NULL;

    if (!sess) {
        return;
    }

    VQE_TAILQ_FOREACH(sender, &(sess->senders_list), p_member_chain) {
        /*
         * count all the senders, even if there's no room
         * to report on all of them
         */
        sender_count++;
        if (!info || sender_count > max_senders) {
            continue;
        }

        memset(info, 0, sizeof(*info));
        info->ssrc = sender->ssrc;
        if (sender->sdes[RTCP_SDES_CNAME]) {
            strncpy(info->cname, 
                    sender->sdes[RTCP_SDES_CNAME],
                    RTCP_MAX_CNAME);
            info->cname[RTCP_MAX_CNAME] = '\0';
        } else {
            info->cname[0] ='\0';
        }

        info->rtcp_src_ip_addr = sender->rtcp_src_addr;
        info->rtcp_src_udp_port = sender->rtcp_src_port;
        info->rtp_src_ip_addr = sender->rtp_src_addr;
        info->rtp_src_udp_port = sender->rtp_src_port;

        /*
         * from the Group and Average Packet Size subreport
         * of an RSI message (if any) received from this member
         */
        info->rtcp_nmembers_reported = sender->rtcp_nmembers_reported;
        info->rtcp_avg_size_reported = sender->rtcp_avg_size_reported;

        if (sender != sess->rtp_local_source) {
            /*
             * from the last sender report (SR) from the member (if any)
             */
            sender_ntp.upper = sender->sr_ntp_h;
            sender_ntp.lower = sender->sr_ntp_l;
            info->sr_ntp = ntp_to_abs_time(sender_ntp);
            info->sr_timestamp = sender->sr_timestamp;
            info->sr_npackets = sender->sr_npackets;
            info->sr_nbytes = sender->sr_nbytes;
            /*
             * Re-derive some values that were sent in the last RR
             * plus total received 
             */
            sender_stats = &sender->rcv_stats;
            info->received = sender_stats->received;
            /* ehsr: extended highest sequence number received */
            info->ehsr = sender_stats->cycles + sender_stats->max_seq;
            /* cumulative loss = (total_expected) - received */
            info->cum_loss = (info->ehsr - sender_stats->base_seq + 1) 
                - info->received;
            info->jitter = rtp_get_jitter(sender_stats);

            /* 
             * Re-derive some values for RTCP XR
             */
            xr_stats = sender->xr_stats;
            if (xr_stats) {
                info->xr_info_avail = TRUE;
                info->begin_seq = xr_stats->eseq_start;
                info->end_seq = xr_stats->eseq_start + xr_stats->totals;

                info->lost_packets = xr_stats->lost_packets;
                info->dup_packets = xr_stats->dup_packets;
                info->min_jitter = xr_stats->min_jitter;
                info->max_jitter = xr_stats->max_jitter;
                info->mean_jitter = rtcp_xr_get_mean_jitter(xr_stats);
                info->dev_jitter = rtcp_xr_get_std_dev_jitter(xr_stats);

                info->not_reported = xr_stats->not_reported;
                info->before_intvl = xr_stats->before_intvl;
                info->re_init = xr_stats->re_init;
                info->late_arrivals = xr_stats->late_arrivals;
            }
        } else {
            /*
             * this is the local source, which has actually sent RTP data;
             * we haven't really received from ourselves,
             * but we can supply similar information:
             * the NTP/RTP timestamps are actually for the last
             * data packet sent before we sent an SR.
             */
            info->sr_ntp = ntp_to_abs_time(sender->ntp_timestamp);
            info->sr_timestamp = sender->rtp_timestamp;
            info->sr_npackets = sender->packets;
            info->sr_nbytes = sender->bytes;
            /*
             * we don't have this information readily at hand,
             * since we don't receive from ourselves.
             */
            info->received = 0;
            info->ehsr = 0;
            info->cum_loss = 0;
            info->jitter = 0;
        }
        
        info++;
    }
    if (num_senders) {
        *num_senders = sender_count;
    }
}

/*
 * rtcp_get_sender_stats
 *
 * Parameters:
 * sess           -- (input)  ptr to RTP/RTCP per-session data
 * max_senders    -- (input) max. no. of senders for which to return stats.
 * num_senders    -- (output) actual no. of sndrs for which stats are returned.
 * sender_info    -- (input/output) ptr to array of max_senders
 *                   rtcp_session_stats_t blocks.
 */
void
rtcp_get_sender_stats (rtp_session_t *sess,
                       int max_senders,
                       int *num_senders,
                       rtp_hdr_source_t *sender_stats)
{
    rtp_member_t *sender = NULL;
    int sender_count = 0;

    if (!sess) {
        return;
    }

    VQE_TAILQ_FOREACH(sender, &(sess->senders_list), p_member_chain) {
        /*
         * count all the senders, even if there's no room
         * to report on all of them
         */
        sender_count++;
        if (sender_stats && sender_count <= max_senders) {
            *sender_stats = sender->rcv_stats;
            sender_stats++;
        }
    }
    if (num_senders) {
        *num_senders = sender_count;
    }
}

/*
 * internal API -- used by RTCP functions to update statistics
 *
 * rtcp_upd_pkt_rcvd_stats  -- called after an RTCP compound packet is received
 * rtcp_upd_msg_rcvd_stats  -- called after an individual RTCP msg is processed
 * rtcp_upd_pkt_sent_stats  -- called after an RTCP compound packet is sent:
 *                             this will also update per-message statistics
 */

/*
 * rtcp_upd_pkt_rcvd_stats
 *
 * Update statistics after a compound packet is received.
 *
 * Parameters: 
 * sess      -- ptr to RTP session data
 * rtcp_len  -- length in bytes of all RTCP data in the compound packet
 *              (but NOT including IP/UDP hdrs)
 * parsed_ok -- TRUE if no parse errors were encountered while processing the
 *              packet; FALSE if any errors were detected, even if some 
 *              individual msgs were without error.
 */
void 
rtcp_upd_pkt_rcvd_stats (rtp_session_t *sess,
                         uint32_t rtcp_len,
                         boolean parsed_ok,
                         boolean rsize_pkt)
{
    rtp_session_t *parent_sess = MCALL(sess, rtp_get_parent_session);

    rtcp_upd_pkt_rcvd_counts(&sess->rtcp_stats, 
                             rtcp_len, parsed_ok, rsize_pkt);
    if (parent_sess && parent_sess->rtcp_child_stats) {
        rtcp_upd_pkt_rcvd_counts(parent_sess->rtcp_child_stats,
                                 rtcp_len, parsed_ok, rsize_pkt);
    }
}

/*
 * rtcp_upd_msg_rcvd_stats
 *
 * Update statistics after an individual RTCP message is processed.
 *
 * Parameters: 
 * sess         -- ptr to RTP session data
 * msg_type     -- message type (RR, SR, etc., 
 *                 or NOT_AN_RTCP_MSGTYPE for runt messages)
 * status       -- parse status of the msg
 */
void
rtcp_upd_msg_rcvd_stats (rtp_session_t *sess,
                         rtcp_type_t msg_type,
                         rtcp_parse_status_t status)
{
    rtp_session_t *parent_sess = MCALL(sess, rtp_get_parent_session);
        
    rtcp_upd_msg_rcvd_counts(&sess->rtcp_stats, msg_type, status);
    if (parent_sess && parent_sess->rtcp_child_stats) {
        rtcp_upd_msg_rcvd_counts(parent_sess->rtcp_child_stats, 
                                 msg_type, status);
    }
}

/*
 * rtcp_upd_pkt_send_stats
 *
 * Update statistics after an RTCP compound packet has been sent;
 * this also updates per-message send statistics.
 *
 * Parameters: 
 * sess      -- ptr to RTP session data
 * rtcp_pkt  -- ptr to start of RTCP data in the compound packet.
 * rtcp_len  -- length in bytes of all RTCP data in the compound packet
 *              (but NOT including IP/UDP hdrs)
 * sent_ok   -- TRUE if the packet was successfully sent, else FALSE.
 */
void
rtcp_upd_pkt_sent_stats(rtp_session_t *sess,
                        uint8_t *rtcp_pkt,
                        uint32_t rtcp_len,
                        boolean sent_ok)
{
    rtp_session_t *parent_sess = MCALL(sess, rtp_get_parent_session);

    if (!rtcp_pkt) {
        return;
    }

    rtcp_upd_pkt_sent_counts(&sess->rtcp_stats,
                             rtcp_pkt, rtcp_len, sent_ok);
    if (parent_sess && parent_sess->rtcp_child_stats) {
        rtcp_upd_pkt_sent_counts(parent_sess->rtcp_child_stats,
                                 rtcp_pkt, rtcp_len, sent_ok);
    }
}


