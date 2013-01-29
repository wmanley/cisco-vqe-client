/*
 * vqec_rtp.c - module containing RTP helper routines.
 *
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <rtp/rtp_session.h>
#include <rtp/rtcp_stats_api.h>
#include <rtp/rtcp_xr.h>
#include "vqec_rtp.h"
#include "rtp_era_recv.h"
#include "rtp_repair_recv.h"
#include "vqec_debug.h"
#include "vqe_port_macros.h"
#include "vqec_assert_macros.h"
#include "vqec_event.h"
#include "vqec_sm.h"
#include "vqec_channel_private.h"
#include <utils/vam_util.h>
#include "vqec_nat_interface.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

#define MAX_SENDERS_SSM 1   /*for SSM*/

#define TMEVT_ERR_STRLEN 256
static const char s_arg_is_null[]  = "- Event arg ptr is null; stopping evt -";
static const char s_src_is_null[]  = "- Local src ptr is null; stopping evt -";
static const char s_sess_is_null[] = "- Session ptr is null -";
static const char s_time_is_null[] = "- Time value ptr is null -";
static const char s_type_is_bad[]  = "- Type is out of bounds -";

typedef void (*timeout_callback)(const vqec_event_t *const, int32_t, int16_t, void *);
UT_STATIC void rtcp_receive_report_timeout_cb(const vqec_event_t * const evptr,
                                              int32_t fd, int16_t event, void *arg);
UT_STATIC void rtcp_member_timeout_cb(const vqec_event_t * const evptr,
                                      int32_t fd, int16_t event, void *arg);
static timeout_callback s_timeout_cb_array[VQEC_RTCP_LAST_TIMEOUT] = 
{ 
    rtcp_member_timeout_cb,
    rtcp_receive_report_timeout_cb 
};

/*
 * rtcp_construct_report_pubports
 *
 * Construct a report including the PUBPORTS message.
 * This is an overriding implementation of the rtcp_construct_report method,
 * for the "repair-recv" and "era-recv" application-level classes.
 *
 * @param[in] p_sess Pointer to session object
 * @param[in] p_source Pointer to local source member
 * @param[in] p_rtcp Pointer to start of RTCP packet in the buffer
 * @param[in] bufflen Length (in bytes) of RTCP packet buffer
 * @param[in] p_pkt_info Pointer to additional packet info 
 * (if any; may be NULL)
 * @param[in[] primary Indicates whether this is the primary or repair
 * rtp session
 * @param[in] reset_xr_stats Flag for whether to reset the RTCP XR stats
 * @param[in] rtp_port External rtp port for pubports.
 * @param[in] rtcp_port External rtcp port for pubports.
 * @param[out] uint32_t Returns length of the packet including RTCP header;
 * zero indicates a construction failure.
 */
uint32_t rtcp_construct_report_pubports (rtp_session_t   *p_sess,
                                         rtp_member_t    *p_source, 
                                         rtcptype        *p_rtcp,
                                         uint32_t         bufflen,
                                         rtcp_pkt_info_t *p_pkt_info,
                                         boolean          primary,
                                         boolean          reset_xr_stats,
                                         in_port_t        rtp_port,
                                         in_port_t        rtcp_port)
{
    rtcp_pkt_info_t *p_new_pkt_info = p_pkt_info;
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t pubports;
    rtcp_pubports_info_t *p_pubports_info = &(pubports.pubports_info);

    rtcp_sender_info_t sender_info[MAX_SENDERS_SSM];
    int num_senders;

    if (vqec_nat_is_natmode_en()) {
        if (rtp_port || rtcp_port) {
        
            /* 
             * we have valid PUBPORTS info:
             * if there's already additional packet info specified,
             * start with that by copying it into our packet info;
             * else just initialize our packet info.
             */
            if (p_pkt_info) {
                pkt_info = *p_pkt_info;
            } else {
                rtcp_init_pkt_info(&pkt_info);
            }
            p_new_pkt_info = &pkt_info;
            /*
             * specify the PUBPORTS info and add it to the packet info
             */
            rtcp_init_msg_info(&pubports);
            
            rtcp_get_sender_info(p_sess,MAX_SENDERS_SSM, 
                                 &num_senders, &sender_info[0]);
            if (num_senders != 0) {
                p_pubports_info->ssrc_media_sender = sender_info[0].ssrc;
            } else {
                p_pubports_info->ssrc_media_sender = 0; 
            }
            p_pubports_info->rtp_port = ntohs(rtp_port);
            p_pubports_info->rtcp_port = ntohs(rtcp_port);
            rtcp_set_pkt_info(p_new_pkt_info, RTCP_PUBPORTS, &pubports);
        }
    }

    return (rtcp_construct_report_base(p_sess, p_source, 
                                       p_rtcp, bufflen,
                                       p_new_pkt_info,
                                       reset_xr_stats));
}

/*
 * Function:    rtcp_get_local_source_ssrc
 * Description: Get the ssrc of the local source.
 *
 * @param[in] rtp_session Pointer to an rtp_session
 * @param[out] ssrc SSRC of the local source.
 * @param[out] boolean TRUE if we were able to get a local_source else FALSE.
 */
boolean rtp_get_local_source_ssrc (const rtp_session_t *rtp_session,
                                   uint32_t *ssrc)
{
    boolean rv=FALSE;
    
    if (rtp_session && rtp_session->rtp_local_source && ssrc) {
        *ssrc = rtp_session->rtp_local_source->ssrc;
        rv = TRUE;
    } else {
        if (ssrc) {
            *ssrc = 0;
        }
        syslog_print(VQEC_ERROR, "Unable to get local ssrc");
    }
    return rv;
}

void 
rtp_show_drop_stats (rtp_session_t *p_sess)
{
    if (!p_sess) {
        return;
    }

    rtp_hdr_session_t *p_stats = &p_sess->rtp_stats;
    CONSOLE_PRINTF(" runts:                     %d\n"
                   " badver:                    %d\n"
                   " badlen:                    %d\n"
                   " badcreate:                 %d\n"
                   " seqjumps:                  %d\n"
                   " initseq_count:             %d\n",
                   p_stats->runts,
                   p_stats->badver,
                   p_stats->badlen,
                   p_sess->badcreate,
                   p_stats->seqjumps,
                   p_stats->initseq_count);
}

static void
vqec_rtcp_item_stats_show (const rtcp_item_stats_t *item_stats) 
{
    if (!item_stats) {
        return;
    }
    CONSOLE_PRINTF("  sent:                     %d\n",
                   (int)(item_stats->sent));
    CONSOLE_PRINTF("  send_errors:              %d\n",
                   (int)(item_stats->send_errors));
    CONSOLE_PRINTF("  rcvd:                     %d\n",
                   (int)(item_stats->rcvd));
    CONSOLE_PRINTF("  rcvd_errors:              %d\n",
                   (int)(item_stats->rcvd_errors));
    CONSOLE_PRINTF("  badver:                   %d\n",
                   (int)(item_stats->badver));
    CONSOLE_PRINTF("  runts:                    %d\n",
                   (int)(item_stats->runts));
    CONSOLE_PRINTF("  badlen:                   %d\n",
                   (int)(item_stats->badlen));
    CONSOLE_PRINTF("  unexp:                    %d\n",
                   (int)(item_stats->unexp));
}
    
static void
vqec_rtcp_stats_show (const rtcp_session_stats_t *stats) 
{
    if (!stats) {
        return;
    }
    CONSOLE_PRINTF(" RTCP compound packet stats\n");
    vqec_rtcp_item_stats_show(&stats->pkts);
    CONSOLE_PRINTF("  avg_pkt_size:             %lf\n",
                   stats->avg_pkt_size);
    CONSOLE_PRINTF("  avg_pkt_size_sent         %lf\n",
                   stats->avg_pkt_size_sent);
}

#define MAX_SENDERS 3   /* 
                         * Artificially limit.  We expect only one
                         * sender unless we are transitioning.
                         * We add a couple for diagnostics
                         * purposes.
                         */

void
vqec_rtp_session_show (vqec_dp_streamid_t id,
                       rtp_session_t *sess,
                       const char *namestr,
                       boolean first_pak_rx)
{
    rtcp_session_info_t rtcp_info;
    rtcp_session_stats_t rtcp_stats;
    rtcp_sender_info_t rtcp_sender_info[MAX_SENDERS];
    rtp_hdr_source_t rtp_hdr_src[MAX_SENDERS];
    int num_senders;
    int i;
    vqec_dp_rtp_src_table_t table;
    char str[INET_ADDRSTRLEN];

    if (!sess || !namestr) {
        return;
    }

    CONSOLE_PRINTF("%7s data received:     %s", namestr, 
                   first_pak_rx ? "TRUE": "FALSE");
    CONSOLE_PRINTF("RTP %s session\n", namestr);

    /* Call low level function to get rtcp_session_info_t */
    rtcp_get_session_info(sess, &rtcp_info);
    CONSOLE_PRINTF("  ssrc:                     %x\n",
                   rtcp_info.ssrc);
    CONSOLE_PRINTF("  cname:                    %s\n",
                   rtcp_info.cname);
    CONSOLE_PRINTF("  nmembers:                 %u\n",
                   rtcp_info.rtcp_nmembers);
    CONSOLE_PRINTF("  nsenders:                 %u\n",
                   rtcp_info.rtcp_nsenders);
    rtcp_get_session_stats (sess, &rtcp_stats, NULL);
    vqec_rtcp_stats_show(&rtcp_stats);

    rtcp_get_sender_info(sess, MAX_SENDERS, &num_senders,
                         rtcp_sender_info);

    rtcp_get_sender_stats(sess, MAX_SENDERS, &num_senders,
                          rtp_hdr_src);
    /* Only display upto MAX_SENDER */
    if (num_senders > MAX_SENDERS) {
        num_senders = MAX_SENDERS;
    }
    for (i = 0; i < num_senders; i++) {
        CONSOLE_PRINTF(" Sender info for sender %d\n",
                       i+1);
        CONSOLE_PRINTF("  ssrc:                     %x\n",
                       rtcp_sender_info[i].ssrc);
        CONSOLE_PRINTF("  cname:                    %s\n",
                       rtcp_sender_info[i].cname);
        CONSOLE_PRINTF("  received:                 %u\n",
                       rtcp_sender_info[i].received);
        CONSOLE_PRINTF("  cum_loss:                 %d\n",
                       rtcp_sender_info[i].cum_loss);
        CONSOLE_PRINTF("  ehsr:                     %u\n",
                       rtcp_sender_info[i].ehsr);
        CONSOLE_PRINTF("  jitter (us):              %u\n",
                       TIME_GET_R(usec, 
                          TIME_MK_R(pcr, rtcp_sender_info[i].jitter)));
        CONSOLE_PRINTF(" Sender stats for sender %d\n",
                       i+1);
        CONSOLE_PRINTF("  max_seq:                  %u\n",
                       rtp_hdr_src[i].max_seq);
        CONSOLE_PRINTF("  cycles:                   %u\n",
                       rtp_hdr_src[i].cycles);
        CONSOLE_PRINTF("  bad_seq:                  %u\n",
                       rtp_hdr_src[i].bad_seq);
        CONSOLE_PRINTF("  base_seq:                 %u\n",
                       rtp_hdr_src[i].base_seq);
        CONSOLE_PRINTF("  transit:                  %u\n",
                       rtp_hdr_src[i].transit);
        /*
         * Don't display sender stats.jitter as it is
         * a scaled up version of sender_info jiter,
         * i.e. redundant.
         */
        CONSOLE_PRINTF("  received:                 %u\n",
                       rtp_hdr_src[i].received);
        CONSOLE_PRINTF("  last_arr_ts_media:        %u\n",
                       rtp_hdr_src[i].last_arr_ts_media);
        CONSOLE_PRINTF("  seqjumps:                 %u\n",
                       rtp_hdr_src[i].seqjumps);
        CONSOLE_PRINTF("  initseq_count:            %u\n",
                       rtp_hdr_src[i].initseq_count);
        CONSOLE_PRINTF("  out_of_order:             %u\n",
                       rtp_hdr_src[i].out_of_order);

        /* RTCP XR Stats */
        if (rtcp_sender_info[i].xr_info_avail) {
            CONSOLE_PRINTF(" RTCP XR info\n");
            CONSOLE_PRINTF("  begin seq:                %u\n",
                           (uint16_t)rtcp_sender_info[i].begin_seq);
            CONSOLE_PRINTF("  end seq:                  %u\n",
                           (uint16_t)rtcp_sender_info[i].end_seq);
            CONSOLE_PRINTF("  lost pkts:                %u\n",
                           rtcp_sender_info[i].lost_packets);
            CONSOLE_PRINTF("  duplicated pkts:          %u\n",
                           rtcp_sender_info[i].dup_packets);
            CONSOLE_PRINTF("  late arrival pkts:        %u\n",
                           rtcp_sender_info[i].late_arrivals);
            CONSOLE_PRINTF("  min jitter (us):          %u\n",
                           TIME_GET_R(usec, 
                             TIME_MK_R(pcr, rtcp_sender_info[i].min_jitter)));
            CONSOLE_PRINTF("  max jitter (us):          %u\n",
                           TIME_GET_R(usec, 
                             TIME_MK_R(pcr, rtcp_sender_info[i].max_jitter)));
            CONSOLE_PRINTF("  mean jitter (us):         %u\n",
                           TIME_GET_R(usec, 
                             TIME_MK_R(pcr, rtcp_sender_info[i].mean_jitter)));
            CONSOLE_PRINTF("  std dev jitter (us):      %u\n",
                           TIME_GET_R(usec, 
                             TIME_MK_R(pcr, rtcp_sender_info[i].dev_jitter)));
            CONSOLE_PRINTF("  before interval:          %u\n",
                           rtcp_sender_info[i].before_intvl);
            CONSOLE_PRINTF("  not reported:             %u\n",
                           rtcp_sender_info[i].not_reported);
            CONSOLE_PRINTF("  reinit:                   %u\n",
                           rtcp_sender_info[i].re_init);
        }
    }

    CONSOLE_PRINTF("%7s RTCP RRs sent:      %d\n", 
                   namestr, (int)(rtcp_stats.pkts.sent));

    memset(&table, 0, sizeof(table));
    if (vqec_rtp_session_dp_get_src_table(sess,
                                          id,
                                          &table)) {
        CONSOLE_PRINTF(" DP sources:                %u\n",
                       table.num_entries);
        for (i = 0; i < table.num_entries; i++) {
#define RTP_DP_TABLE_DISPLAY_ENTRIES_PER_LINE 4
            CONSOLE_PRINTF("  ssrc=%8x, src ip:%s port:%d\n"
                           "     source status:               (%s%s%s)\n"
                           "     source PCM seq num offset:   %d\n",
                           table.sources[i].key.ssrc,
                           inaddr_ntoa_r(
                               table.sources[i].key.ipv4.src_addr,
                               str, sizeof(str)),
                           ntohs(table.sources[i].key.ipv4.src_port),
                           (table.sources[i].info.state == 
                            VQEC_DP_RTP_SRC_ACTIVE ? "active" : "inactive"),
                           (table.sources[i].info.pktflow_permitted ? 
                            ", pktflow" : ""),
                           (table.sources[i].info.buffer_for_failover ?
                            ", failover" : ""),
                           table.sources[i].info.session_rtp_seq_num_offset);
            if (i &&
                !(i % RTP_DP_TABLE_DISPLAY_ENTRIES_PER_LINE)) {
                CONSOLE_PRINTF("\n");
            }
        }
    }    
}


/**---------------------------------------------------------------------------
 * Internal implementation of RTCP packet processing.
 *
 * @param[in] rtp_session Pointer to a repair or primary session.
 * @param[in] pak_src_addr IP source address of the RTCP packet.
 * @param[in] pak_src_port UDP source port of the RTCP packet.
 * @param[in] pak_buff Contents of the RTCP packet.
 * @param[in] pak_buff_len Length of the RTCP packet.
 * @param[in] recv_time Time at which packet is received.
 *---------------------------------------------------------------------------*/ 
void 
rtcp_event_handler_internal_process_pak (rtp_session_t *rtp_session,
                                         struct in_addr pak_src_addr,
                                         uint16_t pak_src_port,
                                         char *pak_buff,
                                         int32_t pak_buff_len,
                                         struct timeval *recv_time)
{
    abs_time_t t;
    rtp_envelope_t addrs;

    addrs.src_addr = pak_src_addr.s_addr;
    addrs.src_port = pak_src_port;
    addrs.dst_addr = 0;
    addrs.dst_port = 0;

    t = timeval_to_abs_time(*recv_time);
    MCALL((rtp_session_t *)rtp_session, 
          rtcp_recv_packet, 
          t,
          FALSE, /* messages are NOT from "receive only" members */
          &addrs, 
          pak_buff, 
          pak_buff_len);

    VQEC_DEBUG(VQEC_DEBUG_RTCP,
               "received rtcp packet srcadd = 0x%x, src_port = %d, ts = %llu\n",
               ntohl(pak_src_addr.s_addr),
               ntohs(pak_src_port),
               t.usec);
}


/**---------------------------------------------------------------------------
 * Front-end RTCP event handler.
 *
 * @param[in] rtp_session Pointer to a repair or primary session.
 * @param[in] rtcp_sock Input socket to be read.
 * @param[in] iobuf An IO buffer to receive RTCP packet data.
 * @param[in] iobuf_len Length of the IO buffer.
 * @param[in] is_primary_session True if the session is the primary session.
 *---------------------------------------------------------------------------*/ 
void
rtcp_event_handler_internal (rtp_session_t *rtp_session,
                             vqec_recv_sock_t *rtcp_sock,
                             char *iobuf,
                             uint32_t iobuf_len,
                             boolean is_primary_session)
{
    int32_t pak_buff_len;
    struct in_addr pak_src_addr;
    uint16_t pak_src_port;
    struct timeval recv_time;    

    VQEC_ASSERT(rtp_session && rtcp_sock && iobuf && iobuf_len);

    while (TRUE) {
        pak_buff_len = vqec_recv_sock_read(rtcp_sock, 
                                           &recv_time, 
                                           iobuf,
                                           iobuf_len,
                                           &pak_src_addr, 
                                           &pak_src_port);
        if (pak_buff_len < 1) {
            break;
        } else { 
            if ((iobuf[0] & 0xC0) == 0) {
                /* NAT packet */
                if (!is_primary_session) {
                    /* send to the NAT protocol for repair session. */
                    if(!vqec_chan_eject_rtcp_nat_pak(
                           rtp_repair_session_chanid(
                               (rtp_repair_recv_t *)rtp_session),
                           iobuf, 
                           pak_buff_len, 
                           VQEC_CHAN_RTP_SESSION_REPAIR,
                           pak_src_addr.s_addr,
                           pak_src_port)) {
                        syslog_print(VQEC_ERROR, 
                                     "Eject of NAT packet failed repair");
                    }
                } else {
                    /* send to the NAT protocol for primary session. */                 
                    if(!vqec_chan_eject_rtcp_nat_pak(
                           rtp_era_session_chanid(
                               (rtp_era_recv_t *)rtp_session),
                           iobuf, 
                           pak_buff_len, 
                           VQEC_CHAN_RTP_SESSION_PRIMARY,
                           pak_src_addr.s_addr,
                           pak_src_port)) {
                        syslog_print(VQEC_ERROR, 
                                     "Eject of NAT packet failed primary");
                    }
                }
                continue;
            }
            
            rtcp_event_handler_internal_process_pak(rtp_session, 
                                                    pak_src_addr,
                                                    pak_src_port,
                                                    iobuf,
                                                    pak_buff_len,
                                                    &recv_time);
        }
    }

    return;
}


static 
void rtcp_tmevt_log_err (const char *format, ...)
{
    char buf[TMEVT_ERR_STRLEN];
    va_list ap;

    va_start(ap, format);    
    vsnprintf(buf, TMEVT_ERR_STRLEN, format, ap);
    va_end(ap);

    syslog_print(VQEC_ERROR, buf);
}

/* 
 * This callback is executed when the time to send the next
 * receiver report is reached.
 */
UT_STATIC
void rtcp_receive_report_timeout_cb (const vqec_event_t * const evptr,
                                     int32_t fd, int16_t event, void *arg)
{
    rtp_session_t *p_sess = NULL;
    rtp_member_t *local_member = NULL;
    uint64_t time;
    struct timeval tv;

    if (!arg) {
        rtcp_tmevt_log_err("%s %s", __FUNCTION__, s_arg_is_null);

        /* ignore return {nothing to be done if stop fails} */
        (void)vqec_event_stop(evptr);    
        return;
    }

    p_sess = (rtp_session_t *)arg;
    local_member = p_sess->rtp_local_source;
    if (!local_member) {
        rtcp_tmevt_log_err("%s %s", __FUNCTION__, s_src_is_null);

        /* ignore return nothing to be done if stop fails */
        (void)vqec_event_stop(evptr);    
        return;
    }

    /* Update the timestamps */
    time = TIME_GET_A(msec, get_sys_time());
    local_member->rtp_timestamp = (uint32_t)time;
    local_member->ntp_timestamp = abs_time_to_ntp(msec_to_abs_time(time));

    MCALL(p_sess, rtp_update_stats, TRUE);
    MCALL(p_sess, rtp_session_timeout_transmit_report);
    timerclear(&tv);
    rel_time_t diff = TIME_SUB_A_A(p_sess->next_send_ts, 
                                   msec_to_abs_time(time));
    /* If the rel-time delta is -ive [lagging behind] use 0. */
    if (TIME_CMP_R(lt, diff, REL_TIME_0)) {
        diff = REL_TIME_0;
    }
    tv = TIME_GET_R(timeval, diff);

    /* ignore return no recourse if start fails*/
    (void)vqec_event_start(evptr, &tv);
}

/* Timeout to check for idle senders/members */
UT_STATIC
void rtcp_member_timeout_cb (const vqec_event_t * const evptr,
                             int32_t fd, int16_t event, void *arg) 
{
    struct timeval tv;
    rtp_session_t *p_sess = NULL;
    uint32_t interval = 0;

    if (!arg) {
        rtcp_tmevt_log_err("%s %s", __FUNCTION__, s_arg_is_null);
        /*sa_ignore {nothing to be done if stop fails} IGNORE_RETURN (1) */
        vqec_event_stop(evptr);    
        return;
    }

    p_sess = (rtp_session_t *)arg;

    /* check for idle senders */
    MCALL(p_sess, rtp_session_timeout_slist);
    /* check for idle members */
    MCALL(p_sess, rtp_session_timeout_glist);

    /* restart the timer */
    timerclear(&tv);
    interval = MCALL(p_sess, rtcp_report_interval,
                     p_sess->we_sent, TRUE /* jitter the interval */);
    tv = TIME_GET_R(timeval,TIME_MK_R(usec, (int64_t)interval)) ;
    /*sa_ignore {no recourse if start fails} IGNORE_RETURN (1) */
    vqec_event_start(evptr, &tv);
}


/*!
 * Create a timer event and add it to libevent scheduler.
 * @returns null if the event couldn't be added successfully, otherwise
 * returns the pointer to the event. It's the caller's responsibility
 * to free the event ptr.
 */
vqec_event_t *rtcp_timer_event_create (vqec_rtcp_timer_type to_type,
                                       struct timeval *p_tv,
                                       void *p_sess)
{
    vqec_event_t *rtcp_timeout = NULL;

    if (p_sess && p_tv && to_type < VQEC_RTCP_LAST_TIMEOUT) {
        if (!vqec_event_create(&rtcp_timeout, 
                               VQEC_EVTYPE_TIMER, 
                               VQEC_EV_ONESHOT, 
                               s_timeout_cb_array[to_type], 
                               VQEC_EVDESC_TIMER, 
                               p_sess) ||
            !vqec_event_start (rtcp_timeout, p_tv)) {

            vqec_event_destroy(&rtcp_timeout);
            rtcp_timeout = NULL;
        }
    } else {
        if (!p_sess) {
            rtcp_tmevt_log_err("%s %s", __FUNCTION__, s_sess_is_null);
        } else if (!p_tv) {
            rtcp_tmevt_log_err("%s %s", __FUNCTION__, s_time_is_null);
        } else {
            rtcp_tmevt_log_err("%s %s(%d)", 
                               __FUNCTION__, s_type_is_bad, to_type);
        }
    }


    return (rtcp_timeout);
}

/*
 * vqec_set_primary_rtcp_bw
 *
 * Set the RTCP bandwidth for the primary stream,
 * from channel configuration information.
 */
void
vqec_set_primary_rtcp_bw (vqec_chan_cfg_t *chan_cfg,
			  rtcp_bw_cfg_t *rtcp_bw_cfg)
{
    rtcp_init_bw_cfg(rtcp_bw_cfg);
    /* 
     * for now, use the configured per-rcvr value for both
     * the per-rcvr value and the per-sndr value, i.e., treat
     * senders and receivers equally 
     */
    rtcp_bw_cfg->per_rcvr_bw = chan_cfg->primary_rtcp_per_rcvr_bw;
    rtcp_bw_cfg->per_sndr_bw = chan_cfg->primary_rtcp_per_rcvr_bw;

    rtcp_bw_cfg->media_rs_bw = chan_cfg->primary_rtcp_sndr_bw;
    rtcp_bw_cfg->media_rr_bw = chan_cfg->primary_rtcp_rcvr_bw;

    /* convert bps to back to kbps */
    rtcp_bw_cfg->media_as_bw = chan_cfg->primary_bit_rate / 1000 ;
}

/*
 * vqec_set_repair_rtcp_bw
 *
 * Set the RTCP bandwidth for the repair stream,
 * from channel configuration information.
 */
void
vqec_set_repair_rtcp_bw (vqec_chan_cfg_t *chan_cfg,
			 rtcp_bw_cfg_t *rtcp_bw_cfg)
{
    rtcp_init_bw_cfg(rtcp_bw_cfg);

    rtcp_bw_cfg->media_rs_bw = chan_cfg->rtx_rtcp_sndr_bw;
    rtcp_bw_cfg->media_rr_bw = chan_cfg->rtx_rtcp_rcvr_bw;
}

/*
 * vqec_set_primary_rtcp_xr_options
 *
 * Set up RTCP XR config option for the primary stream
 * from the channel configuration.
 *
 * The RTCP XR config should be initialized via rtcp_init_xr_cfg,
 * (or via rtp_init_config, which calls it), prior to calling this function.
 */
void 
vqec_set_primary_rtcp_xr_options (vqec_chan_cfg_t *chan_cfg,
                                  rtcp_xr_cfg_t *rtcp_xr_cfg)
{
    rtcp_init_xr_cfg(rtcp_xr_cfg);

    if (chan_cfg->primary_rtcp_xr_loss_rle == RTCP_XR_LOSS_RLE_UNSPECIFIED) {
        rtcp_xr_cfg->max_loss_rle = 0;
    } else {
        rtcp_xr_cfg->max_loss_rle = chan_cfg->primary_rtcp_xr_loss_rle;
    }

    if (chan_cfg->primary_rtcp_xr_per_loss_rle
        == RTCP_XR_LOSS_RLE_UNSPECIFIED) {
        rtcp_xr_cfg->max_post_rpr_loss_rle = 0;
    } else {
        rtcp_xr_cfg->max_post_rpr_loss_rle =
            chan_cfg->primary_rtcp_xr_per_loss_rle;
    }

    if (chan_cfg->primary_rtcp_xr_stat_flags & RTCP_XR_LOSS_BIT_MASK) {
        rtcp_xr_cfg->ss_loss = TRUE;
    }

    if (chan_cfg->primary_rtcp_xr_stat_flags & RTCP_XR_DUP_BIT_MASK) {
        rtcp_xr_cfg->ss_dup = TRUE;
    }

    if (chan_cfg->primary_rtcp_xr_stat_flags & RTCP_XR_JITT_BIT_MASK) {
        rtcp_xr_cfg->ss_jitt = TRUE;
    }

    if (chan_cfg->primary_rtcp_xr_multicast_acq) {
        rtcp_xr_cfg->ma_xr = TRUE;
    }

    if (chan_cfg->primary_rtcp_xr_diagnostic_counters) {
        rtcp_xr_cfg->xr_dc =  TRUE;
    }
}

/*
 * vqec_set_repair_rtcp_xr_options
 *
 * Set up RTCP XR config option for the repair stream
 * from the channel configuration.
 *
 * The RTCP XR config should be initialized via rtcp_init_xr_cfg,
 * (or via rtp_init_config, which calls it), prior to calling this function.
 */
void 
vqec_set_repair_rtcp_xr_options (vqec_chan_cfg_t *chan_cfg,
                                 rtcp_xr_cfg_t *rtcp_xr_cfg)
{
    rtcp_init_xr_cfg(rtcp_xr_cfg);

    if (chan_cfg->rtx_rtcp_xr_loss_rle == RTCP_XR_LOSS_RLE_UNSPECIFIED) {
        rtcp_xr_cfg->max_loss_rle = 0;
    } else {
        rtcp_xr_cfg->max_loss_rle = chan_cfg->rtx_rtcp_xr_loss_rle;
    }

    if (chan_cfg->rtx_rtcp_xr_stat_flags & RTCP_XR_LOSS_BIT_MASK) {
        rtcp_xr_cfg->ss_loss = TRUE;
    }

    if (chan_cfg->rtx_rtcp_xr_stat_flags & RTCP_XR_DUP_BIT_MASK) {
        rtcp_xr_cfg->ss_dup = TRUE;
    }

    if (chan_cfg->rtx_rtcp_xr_stat_flags & RTCP_XR_JITT_BIT_MASK) {
        rtcp_xr_cfg->ss_jitt = TRUE;
    }
}

/**---------------------------------------------------------------------------
 * Session states to constant string name.
 *---------------------------------------------------------------------------*/ 
const char *
vqec_rtp_session_state_tostr (vqec_rtp_session_state_t state)
{
    switch (state) {
    case VQEC_RTP_SESSION_STATE_INACTIVE_WAITFIRST:
        return "Inactive_WaitFirst";
        
    case VQEC_RTP_SESSION_STATE_ACTIVE:
        return "Active";

    case VQEC_RTP_SESSION_STATE_INACTIVE:
        return "Inactive";

    case VQEC_RTP_SESSION_STATE_ERROR:
        return "Error";

    default:
        break;
    }

    return ("INVALID");
}


/**---------------------------------------------------------------------------
 * Find the first entry in the rtp source table that has pktflow status.
 * 
 * @param[in] table Pointer to the RTP source table.
 * @param[out] vqec_dp_rtp_src_table_entry_t* Pointer to the pktflow
 * entry if one exists, null otherwise.
 *---------------------------------------------------------------------------*/ 
vqec_dp_rtp_src_table_entry_t *
vqec_rtp_src_table_find_pktflow_entry (vqec_dp_rtp_src_table_t *table)
{
    uint32_t i;

    if (!table) {
        return (NULL);
    }
    
    /* 
     * Iterate through the source table, and find the pktflow source,
     * of which there may be none.
     */
    for (i = 0; i < table->num_entries; i++) {
        if (table->sources[i].info.pktflow_permitted) {
            return (&table->sources[i]);
        }
    }

    return (NULL);
}


/**---------------------------------------------------------------------------
 * Find the most recent active entry in the rtp source table.
 * 
 * @param[in] table Pointer to the RTP source table.
 * @param[out] vqec_dp_rtp_src_table_entry_t* Pointer to the most recent
 * active entry if there is one, null otherwise.
 *---------------------------------------------------------------------------*/ 
vqec_dp_rtp_src_table_entry_t *
vqec_rtp_src_table_most_recent_active_src (vqec_dp_rtp_src_table_t *table)
{
    uint32_t i;
    vqec_dp_rtp_src_table_entry_t *most_recent = NULL;

    if (!table) {
        return (NULL);
    }
    
    /* 
     * Iterate through the source table, and find the most recent
     * active source.
     */
    for (i = 0; i < table->num_entries; i++) {
        
        if (table->sources[i].info.state == VQEC_DP_RTP_SRC_ACTIVE) {
            if (most_recent) {
                if (TIME_CMP_A(ge, table->sources[i].info.last_rx_time, 
                               most_recent->info.last_rx_time)) {
                    most_recent = &table->sources[i];
                }
            } else {
                most_recent = &table->sources[i];
            }
        }
    }

    return (most_recent);
}


/**---------------------------------------------------------------------------
 * Find the failover source entry in the rtp source table.
 * The failover source corresponds to an non-packetflow
 * sender whose most-recent packets are being queued by the dataplane.
 * If no such failover source exists, or if the failover source is 
 * not active, NULL will be returned 
 * 
 * @param[in] table Pointer to the RTP source table.
 * @param[out] vqec_dp_rtp_src_table_entry_t* Pointer to the failover
 * entry if there is one, null otherwise.
 *---------------------------------------------------------------------------*/ 
vqec_dp_rtp_src_table_entry_t *
vqec_rtp_src_table_failover_src (vqec_dp_rtp_src_table_t *table)
{
    uint32_t i;
    vqec_dp_rtp_src_table_entry_t *failover = NULL;

    if (!table) {
        return (NULL);
    }
    
    /* 
     * Iterate through the source table, and find the failover source.
     */
    for (i = 0; i < table->num_entries; i++) {
        if (table->sources[i].info.buffer_for_failover) {
            failover = &table->sources[i];
            break;
        }
    }

    return (failover);
}


/**---------------------------------------------------------------------------
 * Delete a dataplane source entry for a RTP session.
 * 
 * @param[in] p_session Pointer to the RTP session
 * @param[in] id Identifier of the RTP IS in the dataplane.
 * @param[in] source_id Attributes of the source.
 * @param[out] table [optional]If the caller needs, the most recent
 * source table will be returned in this parameter.
 * @param[out] boolean Returns true if the delete succeeded, or
 * if the source-entry was not present in the dataplane. All other cases,
 * such as null-pointers, non-existent stream in the dataplane, etc. are
 * considered failures.
 *---------------------------------------------------------------------------*/ 
boolean
vqec_rtp_session_dp_src_delete (rtp_session_t *p_session, 
                                vqec_dp_streamid_t id,
                                rtp_source_id_t *source_id,
                                vqec_dp_rtp_src_table_t *table) 
{
    boolean ret = TRUE;
    vqec_dp_rtp_src_key_t key;
    vqec_dp_error_t err;
    char str[INET_ADDRSTRLEN];

    if (!p_session || !source_id) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        ret = FALSE;
        return (ret);
    }

    memset(&key, 0, sizeof(key));
    key.ssrc = source_id->ssrc;
    key.ipv4.src_addr.s_addr = source_id->src_addr;
    key.ipv4.src_port = source_id->src_port;
        
    err = vqec_dp_chan_rtp_input_stream_src_delete(id,
                                                   &key,
                                                   table);
    if (err == VQEC_DP_ERR_NOT_FOUND) {
        VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                   "Stream source delete, source entry not present in dp: "
                   "IS id: %u, source %u:%s:%d\n",
                   id, 
                   key.ssrc,
                   uint32_ntoa_r(key.ipv4.src_addr.s_addr, str, sizeof(str)), 
                   ntohs(key.ipv4.src_port));

    } else if (err != VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, 
                     "IPC failure for input stream source delete"); 
        VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                   "IPC failed for input stream source delete: "
                   "IS id: %u, source %u:%s:%d\n",
                   id, 
                   key.ssrc,
                   uint32_ntoa_r(key.ipv4.src_addr.s_addr, str, sizeof(str)), 
                   ntohs(key.ipv4.src_port));
        ret = FALSE;
    }
    
    return (ret);
}


/**---------------------------------------------------------------------------
 * Fetch a copy of the source table for a RTP session.
 * 
 * @param[in] p_session Pointer to the RTP session (unused).
 * @param[in] id Identifier of the RTP IS in the dataplane.
 * @param[out] table The most recent source table will be returned
 * in this parameter.
 * @param[out] boolean Returns true if a valid table was returned,
 * false otherwise.
 *---------------------------------------------------------------------------*/ 
boolean
vqec_rtp_session_dp_get_src_table (rtp_session_t *p_session, 
                                   vqec_dp_streamid_t id,
                                   vqec_dp_rtp_src_table_t *table)
{
    boolean ret = TRUE;
    vqec_dp_error_t err;

    if (!p_session || !table) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        ret = FALSE;
        return (ret);
    }

    err = vqec_dp_chan_rtp_input_stream_src_get_table(id, table);

    if (err != VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR,
                     "IPC failure for input stream source permit");
        VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                   "IPC failed for input stream source table fetch: "
                   "IS id %u\n", id);
        ret = FALSE;
    }
    
    return (ret);
}


