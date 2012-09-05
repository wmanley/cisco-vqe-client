/**-----------------------------------------------------------------
 * @brief 
 * Declarations/definitions for mgmt and internal APIs to statistics.
 *
 * @file
 * rtcp_stats_api.h
 *
 * March 2007, Mike Lague.
 *
 * Copyright (c) 2007-2008, 2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __RTCP_STATS_API_H__
#define __RTCP_STATS_API_H__

#include "rtp_session.h"

/*
 * rtcp_session_info_t
 *
 * Per-session information:
 * This is generally a subset of rtp_session_t, but with some elements
 * gleaned from the local member structure (sess->rtp_local_source).
 */
typedef struct rtcp_session_info_t_ {
    /*
     * Local member information 
     */
    uint32_t ssrc;                    /* SSRC identifier */
    char     cname[RTCP_MAX_CNAME+1]; /* CNAME: null-terminated string */
    /* 
     * IP/UDP information for RTCP messages sent by the local member 
     */
    in_addr_t  src_addr;             /* IP source address */
    in_port_t  src_port;             /* UDP source port */
    in_addr_t  dst_addr;             /* IP destination address */
    in_port_t  dst_port;             /* UDP destination port */
    /*
     * Session membership information
     */
    uint32_t rtcp_nmembers;          /* Members in this session (observed) */
    uint32_t rtcp_nmembers_learned;  /* Members learned from other sources */
    uint32_t rtcp_nsenders;          /* No. of senders (sending members):
                                        should be 1 or 0 for LA sessions */
    /*
     * RTCP report information
     */
    abs_time_t next_send_ts;     /* time to send next RTCP report */ 
    abs_time_t prev_send_ts;     /* time when prev RTCP report was sent */
    uint32_t   rtcp_pmembers;    /* no. of members (nmembers) 
                                    at time of last RTCP report */
    rtcp_bw_info_t rtcp_bw;      /* RTCP bandwidth information */

    rtcp_xr_cfg_t rtcp_xr_cfg;   /* RTCP XR configuration information */
    /*
     * Parent/child information
     */
    boolean child_stats_present;  /* if TRUE, this session accumulates
                                    stats for child sessions */
} rtcp_session_info_t;

/*
 * rtcp_sender_info_t 
 *
 * Per-sender information:
 * this is gleaned from the rtp_member_t struct for a sending member.
 */

typedef struct rtcp_sender_info_t_ {
    uint32_t   ssrc;               /* synchronization source id */
    char       cname[RTCP_MAX_CNAME+1];  /* CNAME: null-terminated string */

    in_addr_t  rtcp_src_ip_addr;   /* source transport addr for RTCP msgs */
    in_port_t  rtcp_src_udp_port;
    in_addr_t  rtp_src_ip_addr;    /* source transport addr for RTP data */
    in_addr_t  rtp_src_udp_port;
    /*
     * from the Group and Average Packet Size subreport
     * of an RSI message (if any) received from this member
     */
    uint32_t rtcp_nmembers_reported; 
    int32_t  rtcp_avg_size_reported;  
    /*
     * from the last sender report (SR) from the member (if any)
     */
    abs_time_t sr_ntp;        /* derived from NTP timestamp */
    uint32_t   sr_timestamp;  /* media timestamp */
    uint32_t   sr_npackets;   /* number of packets sent */            
    uint32_t   sr_nbytes;     /* number of bytes sent */
    /*
     * Reception statistics:
     *  for this sender at the local receiver, at time of the last 
     *  Receiver Report (RR) sent by the local receiver.
     */
    uint32_t received;      /* no. of packets received */
    int32_t  cum_loss;      /* cumulative no. of packets lost:
                             * may be negative, due to duplicate
                             * or mis-ordered packets 
                             */
    uint32_t ehsr;          /* extended highest seq. no. received */
    uint32_t jitter;        /* jitter (in RTP timestamp units) */

    boolean  xr_info_avail; /* flag to indicate whethere there is XR info */
    uint32_t begin_seq;     /* beginning of seq. number */
    uint32_t end_seq;       /* ending of seq. number */
    uint32_t lost_packets;  /* no. of lost packets during this interval */
    uint32_t dup_packets;   /* no. of duplicate packets during this interval */
    uint32_t min_jitter;    /* min jitter during this interval */
    uint32_t max_jitter;    /* max jitter during this interval */
    uint32_t mean_jitter;   /* mean jitter during this interval */
    uint32_t dev_jitter;    /* std deviation of jitter during this interval */

    /* Debug counters which won't go to RTCP XR */
    uint32_t not_reported;  /* counters after exceeding the limits */
    uint32_t before_intvl;  /* counters before eseq_start */
    uint32_t re_init;       /* counters before re-initialized */
    uint32_t late_arrivals; /* no. of late arrivals during this interval */

} rtcp_sender_info_t;

/*
 * management API
 *
 * rtcp_get_session_info    -- get per-session information
 * rtcp_get_session_stats   -- get per-session statistics
 * rtcp_get_sender_info     -- get per-sender information
 * rtcp_get_sender_stats     -- get per-sender statistics
 */

/*
 * rtcp_get_session_info
 *
 * Parameters:
 * sess           -- (input)  ptr to RTP/RTCP per-session data
 * info           -- (input/output) ptr to session information;
 */
extern void
rtcp_get_session_info(rtp_session_t *sess, 
                      rtcp_session_info_t *info);
/*
 * rtcp_get_session_stats
 *
 * Parameters:
 * sess           -- (input)  ptr to RTP/RTCP per-session data
 * stats          -- (input/output) ptr to session stats: 
 * child_stats    -- (input/output) ptr to aggregated stats for
 *                   child sessions.
 */
extern void
rtcp_get_session_stats(rtp_session_t *sess,
                       rtcp_session_stats_t *stats,
                       rtcp_session_stats_t *child_stats);
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
extern void
rtcp_get_sender_info(rtp_session_t *sess,
                     int max_senders,
                     int *num_senders,
                     rtcp_sender_info_t *info);
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
rtcp_get_sender_stats(rtp_session_t *sess,
                      int max_senders,
                      int *num_senders,
                      rtp_hdr_source_t *sender_stats);

/*
 * internal API -- used by RTCP functions to update statistics
 *
 * rtcp_upd_pkt_rcvd_stats  -- called after an RTCP compound packet is received
 * rtcp_upd_msg_rcvd_stats  -- called after an individual RTCP msg is processed
 * rtcp_upd_pkt_sent_stats  -- called after an RTCP compound packet is sent:
 *                             this will also update per-message statistics
 */

extern void 
rtcp_upd_pkt_rcvd_stats(rtp_session_t *sess,
                        uint32_t rtcp_len,
                        boolean parsed_ok,
                        boolean rsize_pkt);
extern void
rtcp_upd_msg_rcvd_stats(rtp_session_t *sess,
                        rtcp_type_t msg_type,
                        rtcp_parse_status_t status);
extern void
rtcp_upd_pkt_sent_stats(rtp_session_t *sess,
                        uint8_t *rtcp_pkt,
                        uint32_t rtcp_len,
                        boolean sent_ok);

#endif /* __RTCP_STATS_API_H__ */
