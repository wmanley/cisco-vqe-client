/**-----------------------------------------------------------------------
 * @brief
 * Functions for constructing/processing RSI messages.
 * (RSI is the RTCP Receiver Summary Information message)
 *
 * @file
 * rtp_ssm_rsi.c
 *
 * February 2007, Mike Lague -- consolidate existing RSI functions here.
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------------
 */

#include "rtp_ssm_rsi.h"

/*
 * rtcp_construct_report_ssm_rsi
 *
 * Construct an RTCP report with a Receiver Summary Information (RSI) packet
 *
 * Parameters:  p_sess     --pointer to session object
 *              p_source   --pointer to local source member
 *              p_rtcp     --pointer to start of RTCP packet in the buffer
 *              bufflen    --length (in bytes) of RTCP packet buffer
 *              p_pkt_info --pointer to additional packet info 
 *                           (if any; may be NULL)
 *              p_rsi_info --pointer to RSI information
 *              reset_xr_stats --flag for whether to reset RTCP XR stats
 * Returns:     length of the packet including RTCP header;
 *              zero indicates a construction failure.
 */
uint32_t rtcp_construct_report_ssm_rsi (rtp_session_t   *p_sess,
                                        rtp_member_t    *p_source, 
                                        rtcptype        *p_rtcp,
                                        uint32_t         bufflen,
                                        rtcp_pkt_info_t *p_pkt_info,
                                        rtcp_rsi_info_t *p_rsi_info,
                                        boolean          reset_xr_stats)
{
    rtcp_pkt_info_t new_pkt_info;
    rtcp_msg_info_t msg;
    uint32_t len = 0;
    boolean added_rsi_info = FALSE;

    /*
     * if there's pre-specified additional (non-default) content
     * for the compound packet, copy it: we'll add to that copy;
     * else just initialize our additional content.
     */
    if (p_pkt_info) {
        new_pkt_info = *p_pkt_info;
    } else {
        rtcp_init_pkt_info(&new_pkt_info);
    }

    /*
     * Set up to add an RSI message (with a Group and Average Packet Size 
     * subreport) to the pre-specified content of the compound packet.
     */
    rtcp_init_msg_info(&msg);
    msg.rsi_info = *p_rsi_info;
    /* don't set RSI info if it has been set earlier */
    if (!rtcp_get_pkt_info(&new_pkt_info, RTCP_RSI)) {
        rtcp_set_pkt_info(&new_pkt_info, RTCP_RSI, &msg);
        added_rsi_info = TRUE;
    }

    if (added_rsi_info) {
        /* Call the base class version */
        len = rtcp_construct_report_base(p_sess, p_source, 
                                         p_rtcp, bufflen,
                                         &new_pkt_info,
                                         reset_xr_stats);
    } 
    return(len);
}

/*
 * Function:    rtcp_process_rsi
 * Description: Process a rsi packet.
 * Parameters:  p_sess         ptr to session object
 *              p_pak_source   ptr to RTP (sender) member
 *              src_addr       src ip address of sender
 *              src_port       src UDP port 
 *              p_rtcp         ptr to start of RTCP header in packet
 *              count          count of the rtcp packet in compound packet
 *              p_end_of_frame end of RTCP frame

 * Returns:     RTCP_PARSE_OK or RTCP_PARSE_BADLEN
 */
rtcp_parse_status_t 
rtcp_process_rsi (rtp_session_t *p_sess,
                  rtp_member_t  *p_pak_source,
                  ipaddrtype     src_addr,
                  uint16_t       src_port,
                  rtcptype      *p_rtcp,
                  uint16_t       count,
                  uint8_t       *p_end_of_frame)
{
    rtcp_rsi_t *p_rsi;
    rtcp_rsi_gen_subrpt_t *p_gensb;
    rtcp_rsi_gaps_subrpt_t *p_gapsb;
    rtcp_rsi_rtcpbi_subrpt_t *p_bisb;
    uint16_t bi_role;  
    uint32_t bi_bw;
    uint8_t *p_end_of_subrpt;
    
    RTP_LOG_PACKET_F(p_sess, p_pak_source,
                     "Received RSI packet\n");

    p_rsi = (rtcp_rsi_t *)(p_rtcp + 1);
    if ((uint8_t *)(p_rsi + 1) > p_end_of_frame) {
        return (RTCP_PARSE_BADLEN);
    }
    p_gensb = (rtcp_rsi_gen_subrpt_t *)p_rsi->data;
    
    while ((uint8_t *)(p_gensb + 1) <= p_end_of_frame) {
        p_end_of_subrpt = (uint8_t *)(p_gensb) + (p_gensb->length * 4);
        if (p_gensb->length == 0 || p_end_of_subrpt > p_end_of_frame) {
            RTP_LOG_ERROR_F(p_sess, p_pak_source,
                            "RTCP RSI OSB length in words (%u) is invalid\n",
                            p_gensb->length);
            return (RTCP_PARSE_BADLEN);
        }
        switch (p_gensb->srbt) {
        case RTCP_RSI_GAPSB:
            p_gapsb = (rtcp_rsi_gaps_subrpt_t *)p_gensb;
            if ((uint8_t *)(p_gapsb + 1) > p_end_of_frame) {
                return (RTCP_PARSE_BADLEN);
            }
            RTP_LOG_PACKET_FV(p_sess, p_pak_source,
                              "GAPSB group size %d, avg pkt size = %d\n",
                              ntohl(p_gapsb->group_size), 
                              ntohs(p_gapsb->average_packet_size));
            /*
             * Process "group size" learned from a remote member
             * 1. reduce the total of (other) members learned from all members
             *    by the (old) contribution from this member (if any);
             *    (which doesn't include this member, or the local member)
             * 2. record the view of group size from this member.
             * 3. add to the total of (other) members learned
             *    by the (new) contribution from this member
             *    (which doesn't include this member, or the local member)
             * $$$ this works for a Distribution Source and a Receiver,
             *     but not a (disjoint) Feedback Target!, or put another
             *     way, this use of rtcp_nmembers_learned results in
             *     rtcp_nmembers + rtcp_nmembers_learned being the correct
             *     membership size for all cases except a disjoint FBT.
             */
            if (p_pak_source->rtcp_nmembers_reported) {
                p_sess->rtcp_nmembers_learned -= 
                    (p_pak_source->rtcp_nmembers_reported - 2);
            }
            p_pak_source->rtcp_nmembers_reported = ntohl(p_gapsb->group_size);
            p_sess->rtcp_nmembers_learned += 
                    (p_pak_source->rtcp_nmembers_reported - 2);

            /* 
             * Process "average packet size" learned from a remote member.
             * 1. record the view of average packet size from this member.
             * 2. update this session's view of average packet size, 
             *    by averaging in the remote member's value, as if
             *    it were an observed packet size ($$$ is this valid?)
             */
            p_pak_source->rtcp_avg_size_reported = 
                ntohs(p_gapsb->average_packet_size);
            p_sess->rtcp_stats.avg_pkt_size = 
                rtcp_upd_avg_pkt_size(p_sess->rtcp_stats.avg_pkt_size,
                                      p_pak_source->rtcp_avg_size_reported);
            break;

        case RTCP_RSI_BISB:
            p_bisb = (rtcp_rsi_rtcpbi_subrpt_t *)p_gensb;
            if ((uint8_t *)(p_bisb + 1) > p_end_of_frame) {
                return (RTCP_PARSE_BADLEN);
            }
            bi_role = ntohs(p_bisb->role);
            bi_bw = ntohl(p_bisb->rtcp_bandwidth);
            if (bi_role & RTCP_RSI_BI_SENDER) {
                p_sess->rtcp_bw.sndr.rpt_per_member_bw = bi_bw;
            }
            if (bi_role & RTCP_RSI_BI_RECEIVERS) {
                p_sess->rtcp_bw.rcvr.rpt_per_member_bw = bi_bw;
            }
            break;

        default:
            RTP_LOG_ERROR_F(p_sess, p_pak_source,
                            "Unknown or unsupported RSI subreport block %d\n",
                            p_gensb->srbt);
            break;
        }
        p_gensb = (rtcp_rsi_gen_subrpt_t *)((uint8_t *)p_gensb 
                                            + (p_gensb->length * 4));
    }
    if ((uint8_t *)p_gensb != p_end_of_frame) {
        RTP_LOG_ERROR_F(p_sess, p_pak_source,
                        "RTCP RSI packet with inconsistent length\n");
        return (RTCP_PARSE_BADLEN);
    }
   
    return (RTCP_PARSE_OK);
}
