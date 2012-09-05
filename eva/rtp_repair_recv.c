/* 
 * rtp_repair_recv.c - Functions to handle RTP repair receiver session
 *                     for VQEC.  An RTP repair receiver session is a 
 *                     point-to-point style RTP session that receives
 *                     all unicast repair data and control messages.
 * 
 * Copyright (c) 2006-2010 by cisco Systems, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <utils/vam_util.h>
#include <rtp/rtp_util.h>
#if HAVE_FCC
#include <rtp/rcc_tlv.h>
#endif
#include <utils/mp_tlv_decode.h>
#include "rtp_repair_recv.h"
#include "vqec_debug.h"
#include "vqec_rtp.h"
#include "vqec_assert_macros.h"
#include "vqec_event.h"
#include "vqec_channel_private.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif

static rtp_repair_recv_methods_t rtp_repair_recv_methods_table;
static rtcp_handlers_t          rtp_repair_recv_rtcp_handler;
static rtcp_memory_t             rtp_repair_recv_memory;


static boolean
s_rtp_repair_recv_initted = FALSE;  /* Set to TRUE when initialized */

static boolean s_rtp_repair_del_lock;
#define RTP_REPAIR_LOCK_RECURSIVE_DEL()         \
    ({                                          \
        s_rtp_repair_del_lock = TRUE;           \
    })

#define RTP_REPAIR_UNLOCK_RECURSIVE_DEL()       \
    ({                                          \
        s_rtp_repair_del_lock = FALSE;           \
    })

#define RTP_REPAIR_IS_DEL_RECURSIVE()           \
        ({                                          \
            s_rtp_repair_del_lock;                  \
        })


/*
 * rtcp_construct_report_repair_recv
 *
 * Construct a report including the PUBPORTS message.
 * This is an overriding implementation of the rtcp_construct_report method,
 * for the "repair-recv" application-level class.
 *
 * @param[in] p_sess Pointer to session object
 * @param[in] p_source Pointer to local source member
 * @param[in] p_rtcp Pointer to start of RTCP packet in the buffer
 * @param[in] bufflen Length (in bytes) of RTCP packet buffer
 * @param[in] p_pkt_info Pointer to additional packet info 
 * (if any; may be NULL)
 * @param[in] reset_xr_stats flag for whether to reset the RTCP XR stats
 * @param[out] uint32_t Returns length of the packet including RTCP header;
 * zero indicates a construction failure.
 */
uint32_t rtcp_construct_report_repair_recv (rtp_session_t   *p_sess,
                                            rtp_member_t    *p_source, 
                                            rtcptype        *p_rtcp,
                                            uint32_t         bufflen,
                                            rtcp_pkt_info_t *p_pkt_info,
                                            boolean          reset_xr_stats)
{
    boolean primary = FALSE;  /* repair session */
    in_port_t rtp_port;
    in_port_t rtcp_port;
    vqec_chan_t *chan;

    chan = ((rtp_repair_recv_t *)p_sess)->chan;

    /* 
     * Only send the report if ER is enabled in both the channel lineup and
     * the system configuration, or RCC is enabled.
     */
    if (!chan->er_enabled && !chan->rcc_enabled) {
       return (0);
    }

    if (!vqec_chan_get_rtx_nat_ports(chan, &rtp_port, &rtcp_port)) {
        rtp_port =
            rtcp_port = 0;
    }
            
    return rtcp_construct_report_pubports(p_sess,
                                          p_source,
                                          p_rtcp,
                                          bufflen,
                                          p_pkt_info,
                                          primary,
                                          reset_xr_stats,
                                          rtp_port,
                                          rtcp_port);
}


/**---------------------------------------------------------------------------
 * Function:    rtcp_process_app
 * Description: Deal with incoming APP RTCP packet
 * @param[in] p_sess         ptr to repair session object
 * @param[in] p_pak_source   ptr to RTP (sender) member
 * @param[in] src_addr       src ip address of sender
 * @param[in] src_port       src UDP port 
 * @param[in] p_rtcp         ptr to start of RTCP header in packet
 * @param[in] count          count of the rtcp packet in compound packet
 * @param[in] p_end_of_frame end of RTCP frame
 *---------------------------------------------------------------------------*/ 
static rtcp_parse_status_t 
rtcp_process_app (rtp_session_t  *p_sess,
                  rtp_member_t   *p_pak_source,
                  ipaddrtype      src_addr,
                  uint16_t        src_port,
                  rtcptype       *p_rtcp,
                  uint16_t        count,
                  uint8_t         *p_end_of_frame)
{
#if HAVE_FCC
    rtcp_app_t *p_app;
    uint32_t len;
    uint16_t subtype;
    ppdd_tlv_t *p_rcc_tlv;
    vqec_chan_t *chan;
    rtp_repair_recv_t *p_repair_sess;
    uint8_t name[APP_NAME_LENGTH];
 
    p_repair_sess = (rtp_repair_recv_t *)p_sess;
    chan = p_repair_sess->chan;

    if (!chan) {
        syslog_print(VQEC_ERROR, "No channel for the session.");
        /* we don't know if it will parse, asssume yes */
        return (RTCP_PARSE_OK);
    }
    
    p_app = (rtcp_app_t *)(p_rtcp + 1);

    VQEC_DEBUG(VQEC_DEBUG_RCC,
               "RTCP: Received app packet length %d for channel %s\n", 
               ntohs(p_rtcp->len),
               vqec_chan_print_name(chan));

    if ((uint8_t *)(p_app + 1) > p_end_of_frame) {
        vqec_chan_app_parse_failure(chan);
        return (RTCP_PARSE_BADLEN);
    }

    memcpy(name, &(p_app->name), APP_NAME_LENGTH);
    if (memcmp(name, PPDD_APP_NAME, APP_NAME_LENGTH) != 0) {
        VQEC_DEBUG(VQEC_DEBUG_RCC,         
                   "invalid app packet- discarding\n");
        vqec_chan_app_parse_failure(chan);
        return (RTCP_PARSE_UNEXP);
    }

    /*
     * Note subtype is an overload of the count field.
     */
    subtype = count;

    len =  ((ntohs(p_rtcp->len) + 1) << 2) - 4 - sizeof(rtcptype);

    /*
     * Validate the app packet data.
     */
    p_rcc_tlv = ppdd_tlv_decode_allocate(p_app->data, len);
    if (!p_rcc_tlv) {
        syslog_print(VQEC_ERROR, "Unable to decode tlv data.");
        vqec_chan_app_parse_failure(chan);
        return (RTCP_PARSE_BADLEN);
    }

    /* 
     * At this point the packet has been successfully parsed; forward
     * the APP attributes to the channel for further processing.
     */
    chan->app_rx_ts = p_sess->last_rtcp_pkt_ts;
    vqec_chan_process_app(chan, p_rcc_tlv);
    rcc_tlv_decode_destroy(p_rcc_tlv);

#endif /* HAVE_FCC */

    return (RTCP_PARSE_OK);
}


/**---------------------------------------------------------------------------
 * Repair RTCP event handler for libevent.
 * NOTE: All other arguments except dptr are unused.
 *
 * @param[in] dptr Pointer to the repair session object.
 *---------------------------------------------------------------------------*/ 
void
repair_rtcp_event_handler (const vqec_event_t *const evptr, 
                           int fd, short event, void *dptr)
{
    rtp_repair_recv_t *p_repair_sess = dptr;

    VQEC_ASSERT(p_repair_sess != NULL);
    VQEC_ASSERT(p_repair_sess->chan != NULL);
    VQEC_ASSERT((g_channel_module != NULL) && 
                (g_channel_module->rtcp_iobuf != NULL));

    rtcp_event_handler_internal((rtp_session_t *)p_repair_sess,
                                p_repair_sess->repair_rtcp_sock,
                                g_channel_module->rtcp_iobuf,
                                g_channel_module->rtcp_iobuf_len, 
                                FALSE);
}


/**---------------------------------------------------------------------------
 * Create the session structure for an repair-recv RTP/RTCP session.
 *
 * @param[in] dp_id            - Dataplane session identifier.
 * @param[in] src_addr         - Sessions src address to use for 
 *                               transmitting packets.
 * @param[in] cname            - SDES cname for this session.
 * @param[in] fbt_ip_addr      - IP address of feedback target.
 * @param[in] recv_rtcp_port   - RTCP port number of recv sock repair session.
 *                               If the port number is 0, an ephemeral port
 *                               will be allocated.
 * @param[in] send_rtcp_port   - Remote rtcp port number of repair session
 *                               (may be 0, which means don't do RTCP)
 * @param[in] local_ssrc       - SSRC of the local source
 * @param[in] rtcp_bw_cfg      - Ptr to RTCP bandwidth config info
 * @param[in] rtcp_xr_cfg      - Ptr to RTCP XR config info
 * @param[in] orig_source_addr - Original channel source address.
 * @param[in] orig_source_port - Original channel source port.
 * @param[in] sock_buf_depth   - Socket input buffer depth in bytes.
 * @param[in] rtcp_dscp_value  - DSCP class for RTCP packets. 
 * @param[in] rtcp_rsize       - Boolean flag for setting Reduced Size RTCP
 * @param[in] parent_chan      - Parent channel ptr.
 * @return Pointer to a repair session.
 *---------------------------------------------------------------------------*/ 
rtp_repair_recv_t *
rtp_create_session_repair_recv (vqec_dp_streamid_t dp_id,
                                in_addr_t src_addr,
                                char *cname,
                                in_addr_t fbt_ip_addr,
                                uint16_t  *recv_rtcp_port,
                                uint16_t  send_rtcp_port,
                                uint32_t  local_ssrc,
                                rtcp_bw_cfg_t *rtcp_bw_cfg,
                                rtcp_xr_cfg_t *rtcp_xr_cfg,
                                in_addr_t orig_source_addr,
                                uint16_t orig_source_port, 
                                uint32_t sock_buf_depth,
                                uint8_t rtcp_dscp_value,
                                uint8_t rtcp_rsize,
                                vqec_chan_t *parent_chan)
{
    boolean f_init_successful = FALSE;
    struct timeval tv;
    rtp_config_t config;
    rtp_member_id_t member_id;
    rtp_repair_recv_t *p_repair_recv_sess = NULL;
    vqec_recv_sock_t *rtcp_sock;
    abs_time_t now;
    rel_time_t diff;
    uint32_t interval = 0;

    if (!rtcp_bw_cfg || !parent_chan || !recv_rtcp_port) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return (NULL);
    }

    /*
     * Allocate according to the derived class size, not base size
     * The memory allocated is enough not only for the sess but also the
     * methods and the infrastructure for the member cache.
     * Set up a pointer to per-class memory, for future allocations,
     * and for member hash table init (done in the base class).
     */
    p_repair_recv_sess = rtcp_new_session(&rtp_repair_recv_memory);
    if (!p_repair_recv_sess) {
        syslog_print(VQEC_ERROR, "failed to create new rtcp repair session\n");
        return (NULL);
    } 
    memset(p_repair_recv_sess, 0, sizeof(rtp_repair_recv_t));
    
    p_repair_recv_sess->rtcp_mem = &rtp_repair_recv_memory;
    
    /* 
     * Create the repair rtcp session receive socket. If the rx rtcp
     * port is specified as 0, a port will be dynamically selected.
     */
    p_repair_recv_sess->repair_rtcp_sock = 
        rtcp_sock =  
        vqec_recv_sock_create("repair rtcp sock",
                              src_addr,      
                              *recv_rtcp_port,
                              src_addr,
                              FALSE,
                              sock_buf_depth,
                              rtcp_dscp_value);
    if (!rtcp_sock) {
        syslog_print(VQEC_ERROR, "unable to create unicast rtcp socket\n");
        goto fail;
    }
    if (!*recv_rtcp_port) {
        /* If the port is ephemeral, return it's value to the caller. */
        *recv_rtcp_port = rtcp_sock->port;
        VQEC_DEBUG(VQEC_DEBUG_RTCP,
                   "Ephemeral rtcp port %d assigned for channel %s",
                   ntohs(*recv_rtcp_port), vqec_chan_print_name(parent_chan));
    }

    /* the event invoked when packets are received on the repair session. */ 
    if (!vqec_event_create(&rtcp_sock->vqec_ev,
                           VQEC_EVTYPE_FD, 
                           VQEC_EV_READ | VQEC_EV_RECURRING,
                           repair_rtcp_event_handler, 
                           rtcp_sock->fd,
                           p_repair_recv_sess) ||
        !vqec_event_start(rtcp_sock->vqec_ev, NULL)) {

        vqec_event_destroy(&rtcp_sock->vqec_ev);
        syslog_print(VQEC_ERROR, "failed to add event");
        goto fail;
    }
    
    /*
     * Use the same socket for sending rtcp messages if we have
     * non-0 transmit port from the caller.
     */    
    if (send_rtcp_port) {
        p_repair_recv_sess->repair_rtcp_xmit_sock = 
            vqec_recv_sock_get_fd(rtcp_sock);
    } else {
        /* send_rtcp_port = 0, i.e. don't do rtcp */
        
        /* Mark repair_rtcp_xmit_sock invalid (-1) */
        p_repair_recv_sess->repair_rtcp_xmit_sock = -1;
    }
    
    rtp_init_config(&config, RTPAPP_STBRTP);
    config.session_id = 
        rtp_taddr_to_session_id(orig_source_addr, orig_source_port);
    config.senders_cached = 1;
    config.rtcp_sock = p_repair_recv_sess->repair_rtcp_xmit_sock;
    config.rtcp_dst_addr = fbt_ip_addr;
    config.rtcp_dst_port = send_rtcp_port;
    config.rtcp_bw_cfg = *rtcp_bw_cfg;
    config.rtcp_xr_cfg = *rtcp_xr_cfg;
    config.rtcp_rsize = rtcp_rsize;
    
    /*
     * First perform all the parent session checks and initializations.
     * This also will set up the parent session methods.
     */
    /* Pass in FALSE as it's already been allocated earlier */
    f_init_successful =
        rtp_create_session_ptp(&config,
                               (rtp_ptp_t **)&p_repair_recv_sess, 
                               FALSE);

    if (f_init_successful == FALSE) {
        syslog_print(VQEC_ERROR,
                     "Failed to create ptp session\n");
        goto fail;
    }     

    /* Set the methods table */
    p_repair_recv_sess->__func_table = &rtp_repair_recv_methods_table;

     /* Set RTCP handlers */
    p_repair_recv_sess->rtcp_handler = &rtp_repair_recv_rtcp_handler;

    /* 
     * Add ourselves as a member of the session.
     */
    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CHANNEL_MEMBER;
    member_id.ssrc = local_ssrc;
    member_id.src_addr = p_repair_recv_sess->send_addrs.src_addr;
    member_id.src_port = p_repair_recv_sess->send_addrs.src_port;
    member_id.cname = cname;
    (void) MCALL((rtp_session_t *)p_repair_recv_sess,
                 rtp_create_local_source,
                 &member_id,
                 RTP_USE_SPECIFIED_SSRC);
    
     /* Timer events to send RTCP reports, managing lists */

     /* Member timeout */
     timerclear(&tv);
     interval = MCALL((rtp_session_t *)p_repair_recv_sess,
                      rtcp_report_interval,
                      FALSE, TRUE /* jitter the interval */ );
     tv = TIME_GET_R(timeval,TIME_MK_R(usec, (int64_t)interval)) ;
     p_repair_recv_sess->member_timeout_event = 
         rtcp_timer_event_create(VQEC_RTCP_MEMBER_TIMEOUT, &tv,
                                 p_repair_recv_sess);
     if (p_repair_recv_sess->member_timeout_event == NULL) {
         syslog_print(VQEC_ERROR,
                      "Failed to setup "
                      "member_timeout_event for repair session\n");
         goto fail;
     }
     
     if (send_rtcp_port) {
         /*
          * send report timeout
          * Only enable sender report timeout when RTCP 
          * is enabled (i.e. send_rtcp_port is non-zero).
          */
         
         timerclear(&tv);
         now = get_sys_time();
         diff = TIME_SUB_A_A(p_repair_recv_sess->next_send_ts, now);
         tv = TIME_GET_R(timeval, diff);
         p_repair_recv_sess->send_report_timeout_event = 
             rtcp_timer_event_create(VQEC_RTCP_SEND_REPORT_TIMEOUT, &tv, 
                                     p_repair_recv_sess);
         if (p_repair_recv_sess->send_report_timeout_event == NULL) {
             VQEC_DEBUG(VQEC_DEBUG_RTCP,
                        "Failed to setup send_report_timeout_event "
                        "for repair session\n");
             goto fail;
         }
     }
     
     p_repair_recv_sess->chan = parent_chan;
     p_repair_recv_sess->dp_is_id = dp_id;
     
     VQEC_DEBUG(VQEC_DEBUG_RTCP, 
                "RTP: channel %s, created PTP Repair-Recv session\n", 
                vqec_chan_print_name(parent_chan));
     
     return (p_repair_recv_sess);

  fail:
     rtp_delete_session_repair_recv(&p_repair_recv_sess, TRUE);
     return (NULL);
}


/**---------------------------------------------------------------------------
 * Update the session structure for an repair-recv RTP/RTCP session.
 *
 * @param[in] fbt_ip_addr IP address of feedback target.
 * @param[in] send_rtcp_port Remote rtcp port number of repair session
 *                           (may be 0, which means don't do RTCP)
 * @param[out] boolean       TRUE upon successful update, FALSE otherwise  
 *---------------------------------------------------------------------------*/ boolean
rtp_update_session_repair_recv (rtp_repair_recv_t *p_repair_recv,
                                in_addr_t fbt_ip_addr,
                                uint16_t  send_rtcp_port)
{
    boolean success = TRUE;
    
    if ((!p_repair_recv->send_addrs.dst_port && send_rtcp_port) ||
        (p_repair_recv->send_addrs.dst_port && !send_rtcp_port)) {
        /*
         * Enabling/Disabling of RTCP messages during a source change
         * is currently not supported.
         */
        syslog_print(VQEC_ERROR, "Enabling/Disabling RTCP reports during "
                     "a source switch is unsupported\n");
        success = FALSE;
        goto done;
    }
    p_repair_recv->send_addrs.dst_addr = fbt_ip_addr;
    p_repair_recv->send_addrs.dst_port = send_rtcp_port;
done:
    return (success);
}


/**---------------------------------------------------------------------------
 * Frees a session.  The session will be cleaned first using rtp_cleanup_session.
 *
 * @param[in] pp_sess  ptr ptr to session object
 * @param[in] deallocate boolen to check if the object should be destroyed 
 *---------------------------------------------------------------------------*/ 
void 
rtp_delete_session_repair_recv (rtp_repair_recv_t **pp_repair_recv_sess, 
                                boolean deallocate)
{    
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t bye;
    rtcp_msg_info_t xr;
    uint32_t send_result;
    rtp_session_t *p_sess;

   if ((pp_repair_recv_sess == NULL) || (*pp_repair_recv_sess == NULL)) {
       VQEC_DEBUG(VQEC_DEBUG_RTCP,
                  "RTP: Delete ERA-Source Session: Failed "
                  "- NULL pointer\n");
       return;
    }
   
   VQEC_DEBUG(VQEC_DEBUG_RTCP,
              "RTP: Delete ERA-Source Session "
              "for APP(%d): Success (%x)\n",
              (*pp_repair_recv_sess)->app_type,
              (uint32_t)*pp_repair_recv_sess);
   
   
   /*
    * Send a BYE packet every time we delete a repair session,
    * except if we may not send RTCP (e.g., due to zero RTCP bw),
    * or if error repair is not enabled in both the channel and
    * system configurations.
    */
   if (rtcp_may_send((rtp_session_t *)(*pp_repair_recv_sess)) &&
       ((rtp_repair_recv_t *)(*pp_repair_recv_sess))->chan->er_enabled) {
       rtcp_init_pkt_info(&pkt_info);
       rtcp_init_msg_info(&bye);
       rtcp_set_pkt_info(&pkt_info, RTCP_BYE, &bye);

        /* Depending on the configuration, include RTCP XR or not */
        p_sess = (rtp_session_t *)(*pp_repair_recv_sess);
        if (is_rtcp_xr_enabled(&p_sess->rtcp_xr_cfg)) {
            rtcp_init_msg_info(&xr);
            rtcp_set_pkt_info(&pkt_info, RTCP_XR, &xr);
        }

       send_result = MCALL((rtp_session_t *)(*pp_repair_recv_sess),
                           rtcp_send_report,
                           (*pp_repair_recv_sess)->rtp_local_source,
                           &pkt_info,
                           TRUE,  /* re-schedule next RTCP report */
                           TRUE); /* reset RTCP XR stats */
       if (send_result == 0) {
           syslog_print(VQEC_ERROR, 
                        "RTP: Delete Repair-Recv Session: Failed "
                        "to send rtcp packet\n");
       }
   }
   
   /* delete send report timer. */
   if ((*pp_repair_recv_sess)->send_report_timeout_event) {
        vqec_event_destroy(&(*pp_repair_recv_sess)->send_report_timeout_event);
   }

   /* delete member timeout timer. */
   if ((*pp_repair_recv_sess)->member_timeout_event) {
       vqec_event_destroy(&(*pp_repair_recv_sess)->member_timeout_event);
   }

   /* delete the event associated with the repair rtcp socket. */
   if ((*pp_repair_recv_sess)->repair_rtcp_sock) {
       if ((*pp_repair_recv_sess)->repair_rtcp_sock->vqec_ev) {
           (void)vqec_event_stop(
               (*pp_repair_recv_sess)->repair_rtcp_sock->vqec_ev);
           vqec_event_destroy(
               &((*pp_repair_recv_sess)->repair_rtcp_sock->vqec_ev));
       }
   }

   /* null the rtcp tx socket. closed by vqec_recv_sock_destroy below. */
   if ((*pp_repair_recv_sess)->repair_rtcp_xmit_sock >= 0) {
       (*pp_repair_recv_sess)->repair_rtcp_xmit_sock = -1;
   }

   /* delete rtcp rx socket. */
   if ((*pp_repair_recv_sess)->repair_rtcp_sock) {
       vqec_recv_sock_destroy((*pp_repair_recv_sess)->repair_rtcp_sock);
   }
   
    rtp_delete_session_ptp((rtp_ptp_t **)pp_repair_recv_sess, FALSE);
    
    if (deallocate) {
        rtcp_delete_session(&rtp_repair_recv_memory, *pp_repair_recv_sess);
        *pp_repair_recv_sess = NULL;
    }
}


/**---------------------------------------------------------------------------
 * Show session statistics for a repair session. 
 *
 * @param[in] p_sess Pointer to the repair session.
 *---------------------------------------------------------------------------*/ 
#define REPAIR_SESSION_NAMESTR "Repair " /* must be 7 characters */
void 
rtp_show_session_repair_recv (rtp_repair_recv_t *p_sess)
{
    if (!p_sess) {
        return;
    }

    vqec_rtp_session_show(p_sess->dp_is_id, 
                          (rtp_session_t *)p_sess, 
                          REPAIR_SESSION_NAMESTR,
                          (p_sess->state != 
                           VQEC_RTP_SESSION_STATE_INACTIVE_WAITFIRST));
}


/**---------------------------------------------------------------------------
 * Update RTP statistics for an repair session. 
 *
 * @param[in] p_session Pointer to the repair session.
 * @param[in] p_source Pointer to the per-member data for a remote source.
 * @param[in] reset_xr_stats True if XR should be reset.
 *---------------------------------------------------------------------------*/ 
static void 
rtp_repair_update_receiver_stats (rtp_session_t *p_sess,
                                  rtp_member_t *p_source,
                                  boolean reset_xr_stats)
{
    vqec_dp_rtp_src_entry_info_t src_info;
    vqec_dp_rtp_sess_info_t sess_info;
    vqec_dp_rtp_src_key_t src_key;
    rtp_repair_recv_t *p_repair_sess;
    char str[INET_ADDRSTRLEN];
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    vqec_dp_error_t err;
    boolean do_xr_stats;

    if (!p_sess || 
        !p_source) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return;
    }

    p_repair_sess = (rtp_repair_recv_t *)p_sess;

    do_xr_stats = (is_rtcp_xr_enabled(&p_repair_sess->rtcp_xr_cfg) &&
                   reset_xr_stats);

    memset(&sess_info, 0, sizeof(sess_info));
    memset(&src_key, 0, sizeof(src_key));
    memset(&src_info, 0, sizeof(src_info));

    if (p_repair_sess->dp_is_id == VQEC_DP_STREAMID_INVALID) {

        VQEC_DEBUG(VQEC_DEBUG_RTCP,
                   "channel %s, repair input stream id is zero\n",
                   vqec_chan_print_name(p_repair_sess->chan)); 
    } else {

        src_key.ssrc = p_source->ssrc;
        src_key.ipv4.src_addr.s_addr = p_source->rtp_src_addr;
        src_key.ipv4.src_port = p_source->rtp_src_port;

        if (do_xr_stats) {
            p_source->xr_stats = &p_repair_sess->xr_stats;
            memset(p_source->xr_stats, 0, sizeof(*p_source->xr_stats));
        }

        err = 
            vqec_dp_chan_rtp_input_stream_src_get_info(p_repair_sess->dp_is_id,
                                                       &src_key,
                                                       TRUE,
                                                       reset_xr_stats,
                                                       &src_info,
                                                       (do_xr_stats ? 
                                                        p_source->xr_stats : 
                                                        NULL),
                                                       NULL,  /* post-ER stats*/
                                                       NULL,  /* XR MA stats*/
                                                       &sess_info);

        if (err != VQEC_DP_ERR_OK) {
            
            snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                     "channel %s: get info for RTP source %u:%s:%d: "
                     "repair RTCP session failed\n",
                     vqec_chan_print_name(p_repair_sess->chan),
                     src_key.ssrc,
                     uint32_ntoa_r(src_key.ipv4.src_addr.s_addr, 
                                   str, sizeof(str)), 
                     ntohs(src_key.ipv4.src_port));
            syslog_print(VQEC_ERROR, msg_buf);  

        } else {

            VQEC_DEBUG(VQEC_DEBUG_RTCP,
                       "channel %s, got input stream info - "
                       "key=0x%x:%s:%d(%d)\n",
                       vqec_chan_print_name(p_repair_sess->chan),
                       src_key.ssrc, 
                       uint32_ntoa_r(src_key.ipv4.src_addr.s_addr, 
                                     str, sizeof(str)), 
                       ntohs(src_key.ipv4.src_port), 
                       ntohs(p_source->rtp_src_port)); 
        }

    }

    p_source->rcv_stats = src_info.src_stats;
    p_sess->rtp_stats = sess_info.sess_stats;
    
    VQEC_DEBUG(VQEC_DEBUG_RTCP,
               "channel %s: repair session RTCP statistics: "
               "rcvd = %u ehsr = %0x08x\n",
               vqec_chan_print_name(p_repair_sess->chan),
               p_source->rcv_stats.received,
               p_source->rcv_stats.cycles + 
               p_source->rcv_stats.max_seq);    
}



typedef 
enum rtp_repair_error_cause_
{
    RTP_REPAIR_ERROR_CAUSE_RTPDATASOURCE = 1,   /* RTP new data source  */
    RTP_REPAIR_ERROR_CAUSE_IPCDELETE,           /* IPC source delete  */
    RTP_REPAIR_ERROR_CAUSE_MAXSOURCES,          /* Max source limit */
    RTP_REPAIR_ERROR_CAUSE_IPCSSRC,             /* IPC ssrc filter  */
    RTP_REPAIR_ERROR_CAUSE_OUTOFSYNC,           /* dp-cp state out-of-sync  */
    RTP_REPAIR_ERROR_CAUSE_BADTABLE,            /*Bad dp source table  */

} rtp_repair_error_cause_t;
static rtp_repair_error_cause_t s_rtp_repair_error_cause;

/**---------------------------------------------------------------------------
 * Cleanup a session's state prior to entering error state.
 *
 * @param[in] p_session Pointer to the repair session.
 *---------------------------------------------------------------------------*/ 
static void
rtp_repair_enter_error_state (rtp_repair_recv_t *p_session)
{
    rtp_member_id_t member_id;
    rtp_source_id_t source_id;
    vqec_dp_error_t err;
    vqec_dp_rtp_src_table_t table;
    uint32_t i;

    VQEC_DEBUG(VQEC_DEBUG_UPCALL,
               "channel %s, repair session, FSM entering error state, "
               "error cause %u\n",
               vqec_chan_print_name(p_session->chan),
               s_rtp_repair_error_cause);

    /*
     * -- locally delete all cached sources from control-plane.
     * -- fetch all source entries in the dataplane, and delete all of them.
     * -- remove ssrc filter in dataplane.
     */
    memset(&table, 0, sizeof(table));
    for (i = 0; i < VQEC_DP_RTP_MAX_KNOWN_SOURCES; i++) {
        if (!is_null_rtp_source_id(&p_session->src_ids[i].id)) {
            memset(&member_id, 0, sizeof(member_id));
            member_id.type = RTP_SMEMBER_ID_RTP_DATA;
            member_id.ssrc = p_session->src_ids[i].id.ssrc;
            member_id.src_addr = p_session->src_ids[i].id.src_addr;
            member_id.src_port = p_session->src_ids[i].id.src_port;
            member_id.cname = NULL;
            /*
             * The remove_member vector will invoke the derived class member
             * delete vector - the lock prevents execution of the vector.
             */
            RTP_REPAIR_LOCK_RECURSIVE_DEL();
            MCALL((rtp_session_t *)p_session, 
                  rtp_remove_member_by_id, &member_id, TRUE); 
            RTP_REPAIR_UNLOCK_RECURSIVE_DEL();
        }
    }
    memset(&p_session->src_ids, 0, sizeof(p_session->src_ids));
    p_session->num_src_ids = 0;
    p_session->src_ssrc_filter = 0;
    p_session->ssrc_filter_en = FALSE;

    err = vqec_dp_chan_rtp_input_stream_src_get_table(p_session->dp_is_id,
                                                      &table);
    if (err == VQEC_DP_ERR_OK) {
        for (i = 0; i < table.num_entries; i++) {
            memset(&source_id, 0, sizeof(source_id));
            source_id.ssrc = table.sources[i].key.ssrc;
            source_id.src_addr = table.sources[i].key.ipv4.src_addr.s_addr;
            source_id.src_port = table.sources[i].key.ipv4.src_port;
            
            (void)
                vqec_rtp_session_dp_src_delete((rtp_session_t *)p_session,
                                               p_session->dp_is_id, 
                                               &source_id, NULL);
        }
    }
    (void)
        vqec_dp_chan_input_stream_rtp_session_del_ssrc_filter(
            p_session->dp_is_id);
}


/**---------------------------------------------------------------------------
 * Lookup a rtp source in the local cache of dataplane sources. 
 * 
 * @param[in] p_sess Pointer to the repair session.
 * @param[in] lookup Attributes of the source to be looked up.
 * @param[out] int32_t Returns the index of the source if a match is found,
 * -1 otherwise.
 *---------------------------------------------------------------------------*/ 
#define RTP_REPAIR_INVALID_SRC_INDEX -1
static int32_t
rtp_repair_lookup_local_src (rtp_repair_recv_t *p_sess, 
                             rtp_source_id_t *lookup)
{
    int32_t i, match = RTP_REPAIR_INVALID_SRC_INDEX;

    if (!p_sess->num_src_ids) {
        return (match);
    }

    for (i = 0; i < VQEC_DP_RTP_MAX_KNOWN_SOURCES; i++) {

        if (is_null_rtp_source_id(&p_sess->src_ids[i].id)) {
            continue;
        }

        if (same_rtp_source_id(&p_sess->src_ids[i].id, lookup)) {
            match = i;
            break;
        }
    }

    return (match);
}


/**---------------------------------------------------------------------------
 * Compare a dataplane source table against the local source cache. It
 * is assumed that there are no duplicates.
 * 
 * CAUTION: O(N^2) loop for search. Fortunately, N will be 3 or 4 so
 * no issues will arise.
 *  
 * @param[in] p_sess Pointer to the repair session.
 * @param[in] table Dataplane source table.
 * @param[out] boolean Returns true, if there are no mismatches, false
 * otherwise.
 *---------------------------------------------------------------------------*/ 
static boolean
rtp_repair_compare_src_table (rtp_repair_recv_t *p_sess, 
                              vqec_dp_rtp_src_table_t *table)
{   
    int32_t i, match_cnt = 0;
    rtp_source_id_t source_id;
    boolean ret = TRUE;

    if (table->num_entries != p_sess->num_src_ids) {
        ret = FALSE;
        return (ret);        
    }

    for (i = 0; i < table->num_entries; i++) {

        memset(&source_id, 0, sizeof(source_id));
        source_id.ssrc = table->sources[i].key.ssrc;
        source_id.src_addr = table->sources[i].key.ipv4.src_addr.s_addr;
        source_id.src_port = table->sources[i].key.ipv4.src_port;
        
        if (is_null_rtp_source_id(&source_id)) {
            continue;
        }
        
        if (rtp_repair_lookup_local_src(p_sess, &source_id) == 
            RTP_REPAIR_INVALID_SRC_INDEX) {
            ret = FALSE;
            break;
        }
        match_cnt++;
    }
    
    return (ret && (match_cnt == p_sess->num_src_ids));
}


/**---------------------------------------------------------------------------
 * Verify that all entries in the source table:
 *  -- pass the SSRC filter check if one is installed.
 *  -- no two sources with the same SSRC, can have the same 
 *   <IP source, port> tuple. [such a sender will cause rtp to throw a
 *   duplicate member error. 
 *  [The dataplane inputshim has a source filter for the repair
 *  session IP address, port thus the condition cannot happen under
 *  normal operation and is considered an error case].
 * 
 * @param[in] p_sess Pointer to the repair session.
 * @param[in] table Dataplane source table.
 * @param[out] boolean Returns true, if all entries are verified, false if
 * there is an error condition.
 *---------------------------------------------------------------------------*/ 
static boolean
rtp_repair_check_src_table (rtp_repair_recv_t *p_sess, 
                            vqec_dp_rtp_src_table_t *table)
{
    int32_t i, j;
    boolean ret = TRUE;
    vqec_dp_rtp_src_table_entry_t *entry;

    if (p_sess->ssrc_filter_en) {

        for (i = 0; i < table->num_entries; i++) {
            if (p_sess->src_ssrc_filter != table->sources[i].key.ssrc) {
                ret = FALSE;
                break;
            }
        }
    }

    for (i = 0; i < table->num_entries; i++) { 

        entry = &table->sources[i];
        for (j = 0; j < table->num_entries; j++) {
            if ((i != j)  && 
                (table->sources[j].key.ssrc == entry->key.ssrc)) {
                
                ret = FALSE;
                break;
            }
        }
    }

    return (ret);
}


/**---------------------------------------------------------------------------
 * Delete a data member - IPC to dataplane to cleanup. 
 *
 * @param[in] p_sess Pointer to the repair session.
 * @param[in] pp_source Pointer to a pointer to a rtp member.
 * @param[in] destroy_flag If memory for the member is to be released.
 *---------------------------------------------------------------------------*/ 
void
rtp_repair_delete_member (rtp_session_t *p_sess,
                          rtp_member_t **pp_source,
                          boolean destroy_flag)
{
    rtp_repair_recv_t *p_repair_sess;
    char str[INET_ADDRSTRLEN];
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    vqec_dp_rtp_src_table_t table;
    rtp_source_id_t source_id;
    int32_t match_idx;
    boolean ret = FALSE;

    if (!p_sess || 
        !pp_source || 
        !*pp_source) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return; 
    }

    p_repair_sess = (rtp_repair_recv_t *)p_sess;
    rtp_member_t *p_source = *pp_source;

    if (p_source &&
        (p_repair_sess->state != VQEC_RTP_SESSION_STATE_INACTIVE_WAITFIRST) &&
        !RTP_REPAIR_IS_DEL_RECURSIVE()) {
        
        memset(&table, 0, sizeof(table));
        memset(&source_id, 0, sizeof(source_id));

        source_id.ssrc = p_source->ssrc;
        source_id.src_addr = p_source->rtp_src_addr;
        source_id.src_port = p_source->rtp_src_port;

        /* Act iff the source is present in the control-plane table. */
        match_idx = rtp_repair_lookup_local_src(p_repair_sess, 
                                                &source_id);
        if (match_idx != RTP_REPAIR_INVALID_SRC_INDEX) {
            memset(&p_repair_sess->src_ids[match_idx], 
                   0, 
                   sizeof(p_repair_sess->src_ids[0]));
            p_repair_sess->num_src_ids--;
            VQEC_ASSERT(p_repair_sess->num_src_ids >= 0);
        
            ret = vqec_rtp_session_dp_src_delete((rtp_session_t *)p_sess,
                                                 p_repair_sess->dp_is_id,
                                                 &source_id,
                                                 &table);
            if (!ret) {

                snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                         "channel %s: delete IPC for RTP source %u:%s:%d: "
                         "repair RTCP session failed - enter error state",
                         vqec_chan_print_name(p_repair_sess->chan),
                         source_id.ssrc,
                         uint32_ntoa_r(source_id.src_addr, 
                                       str, sizeof(str)), 
                         ntohs(source_id.src_port));
                syslog_print(VQEC_ERROR, msg_buf); 

                s_rtp_repair_error_cause = RTP_REPAIR_ERROR_CAUSE_OUTOFSYNC;
                rtp_repair_enter_error_state(p_repair_sess);
                p_repair_sess->state = VQEC_RTP_SESSION_STATE_ERROR;

            } else {
                
                VQEC_DEBUG(VQEC_DEBUG_RTCP,
                           "channel %s, deleted repair session member - "
                           "key=0x%x:%s:%d\n",
                           vqec_chan_print_name(p_repair_sess->chan),
                           source_id.ssrc, 
                           uint32_ntoa_r(source_id.src_addr, 
                                         str, sizeof(str)), 
                           ntohs(source_id.src_port)); 
            }
        }
    }

    /* Invoke base class destructor */
    rtp_delete_member_base(p_sess, pp_source, destroy_flag);
}


/**---------------------------------------------------------------------------
 * Add a new source member to a rtp session.
 *
 * @param[in] p_session Pointer to the repair rtp session.
 * @param[in] p_source_id Pointer to the source member attributes.
 * @param[out] boolean Returns true if the addition of the source member
 * succeeded, false otherwise.
 *---------------------------------------------------------------------------*/ 
static boolean
rtp_repair_add_new_source (rtp_repair_recv_t *p_session, 
                        rtp_source_id_t *p_source_id)
{
    boolean ret = FALSE;
    rtp_error_t rtp_ret;
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    char str[INET_ADDRSTRLEN];
    uint32_t i;

    if (p_session->num_src_ids == VQEC_DP_RTP_MAX_KNOWN_SOURCES) {
        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s, addition of rtp source 0x%x:%s:%u "
                 "failed - reached the max member count %d",
                 vqec_chan_print_name(p_session->chan),
                 p_source_id->ssrc,
                 uint32_ntoa_r(p_source_id->src_addr, str, sizeof(str)),
                 ntohs(p_source_id->src_port), 
                 VQEC_DP_RTP_MAX_KNOWN_SOURCES);
        syslog_print(VQEC_ERROR, msg_buf);
        s_rtp_repair_error_cause = RTP_REPAIR_ERROR_CAUSE_MAXSOURCES;
        return (ret);
    }

    rtp_ret = MCALL((rtp_session_t *)p_session, 
                    rtp_new_data_source, p_source_id);

    if (rtp_ret != RTP_SUCCESS && 
        rtp_ret != RTP_SSRC_EXISTS) {
        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s, addition of rtp source 0x%x:%s:%u "
                 "failed, reason (%d)",
                 vqec_chan_print_name(p_session->chan),
                 p_source_id->ssrc,
                 uint32_ntoa_r(p_source_id->src_addr, str, sizeof(str)),
                 ntohs(p_source_id->src_port), rtp_ret); 
        syslog_print(VQEC_ERROR, msg_buf);
        s_rtp_repair_error_cause = RTP_REPAIR_ERROR_CAUSE_RTPDATASOURCE;

    } else {
        
        /* find an empty slot in the table. */
        for (i = 0; i < VQEC_DP_RTP_MAX_KNOWN_SOURCES; i++) {
            if (!is_null_rtp_source_id(&p_session->src_ids[i].id)) {
                continue;
            }
            break;
        }

        VQEC_ASSERT(i < VQEC_DP_RTP_MAX_KNOWN_SOURCES);
        memcpy(&p_session->src_ids[i].id, p_source_id, sizeof(rtp_source_id_t));
        p_session->num_src_ids++;

        VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                   "channel %s, addition of rtp source 0x%x:%s:%u succeeded "
                   " source count in cp: %d\n",
                   vqec_chan_print_name(p_session->chan),
                   p_source_id->ssrc,
                   uint32_ntoa_r(p_source_id->src_addr, str, sizeof(str)),
                   ntohs(p_source_id->src_port), p_session->num_src_ids);
        
        ret = TRUE; 
    }
    
    return (ret);    
}


/**---------------------------------------------------------------------------
 * Find and add new sources in the repair table to the repair session.
 *
 * CAUTION: O(N^2) loop for search. Fortunately, N will be 3 or 4 so
 * no issues will arise.
 *
 * ASSUMPTION: The combined dataplane RTP receiver source-pool
 * is less than or equal to the control-plane pool.
 *
 * @param[in] p_session Pointer to the repair rtp session.
 * @param[in] table Pointer to the dataplane source table.
 * @param[out] boolean Returns true if the addition of all the new
 * source members succeeded, false otherwise.
 *---------------------------------------------------------------------------*/ 
static boolean
rtp_repair_find_add_new_sources (rtp_repair_recv_t *p_session, 
                                 vqec_dp_rtp_src_table_t *table)
{
    boolean ret = TRUE;
    rtp_source_id_t source_id;
    uint32_t i;
    
    for (i = 0; i < table->num_entries; i++) {

        memset(&source_id, 0, sizeof(source_id));
        source_id.ssrc = table->sources[i].key.ssrc;
        source_id.src_addr = table->sources[i].key.ipv4.src_addr.s_addr;
        source_id.src_port = table->sources[i].key.ipv4.src_port;
        
        if (is_null_rtp_source_id(&source_id)) {
            continue;
        }
        
        if (rtp_repair_lookup_local_src(p_session, &source_id) ==
            RTP_REPAIR_INVALID_SRC_INDEX) { 
            
            /* source does not exist: add. */
            ret = rtp_repair_add_new_source(p_session, &source_id);
            if (!ret) {
                ret = FALSE;
                break;
            }
        }
    }   

    return (ret);
}


/**---------------------------------------------------------------------------
 * Process the repair session's dataplane source-table, and install new
 * sources in the control-plane.
 * 
 * @param[in] p_session Pointer to the repair session.
 * @param[in] p_table Pointer to the dataplane RTP source table.
 * @param[out] boolean Returns true if the update of the source is 
 * successful, false otherwise.
 *---------------------------------------------------------------------------*/ 
static boolean
rtp_repair_update_sources (rtp_repair_recv_t *p_session, 
                           vqec_dp_rtp_src_table_t *p_table)
{
    boolean ret = FALSE;
    vqec_dp_rtp_src_table_t table;
    char msg_buf[VQEC_LOGMSG_BUFSIZE];

    if (!p_table->num_entries) {
        /* no sources reported - return. */
        ret = TRUE;
        return (ret);
    }

    /* 
     * Ensure that all sources reported are within the IP source and
     * SSRC filters established in the dataplane.
     */
    if (!rtp_repair_check_src_table(p_session,
                                    p_table)) {
        
        /* 
         * It is possible that this rtp source table is stale - fetch a fresh
         * copy of the source table.
         */
        VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                   "channel %s, the source table from the repair session "
                   "appears to be a stale, fetching new copy\n",
                   vqec_chan_print_name(p_session->chan));
        
        memset(&table, 0, sizeof(table));
        if (!vqec_rtp_session_dp_get_src_table((rtp_session_t *)p_session,
                                               p_session->dp_is_id,
                                               &table)) {
            
            snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                     "channel %s, repair session source table Ipc failure",
                     vqec_chan_print_name(p_session->chan));
            syslog_print(VQEC_ERROR, msg_buf);
            goto done;
        }
        
        p_table = &table;
        
        if (!rtp_repair_check_src_table(p_session,
                                        p_table)) {
            
            /* 
             * An invalid condition in accordance with the current ;
             * implementation of the cp-dp semantics.
             */
            snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                     "channel %s, 2nd fetch of source table does not "
                     "pass installed IP / SSRC filter checks",
                     vqec_chan_print_name(p_session->chan));
                syslog_print(VQEC_ERROR, msg_buf);
                s_rtp_repair_error_cause = RTP_REPAIR_ERROR_CAUSE_BADTABLE;
                goto done;
        }
    }
            
    /* All checks have passed - add new sources. */
    ret = rtp_repair_find_add_new_sources(p_session, p_table);
    if (!ret) {
        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s, failure in addition of new rtp sources",
                 vqec_chan_print_name(p_session->chan));
        syslog_print(VQEC_ERROR, msg_buf);
    }

  done:
    if (ret) {
        if (!rtp_repair_compare_src_table(p_session, p_table)) {
            
            snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                     "channel %s: cp / dp source tables out-of-sync",
                     vqec_chan_print_name(p_session->chan));
            syslog_print(VQEC_ERROR, msg_buf);
            s_rtp_repair_error_cause = RTP_REPAIR_ERROR_CAUSE_OUTOFSYNC;
            ret = FALSE;
        }
    }

    return (ret);
}


/**---------------------------------------------------------------------------
 * Process an upcall event for this session from the dataplane.
 *
 * @param[in] session Pointer to the repair session.
 * @param[in] p_table Pointer to the dataplane's source entry table.
 *---------------------------------------------------------------------------*/ 
static void 
rtp_repair_upcall_ev (rtp_repair_recv_t *p_session, 
                      vqec_dp_rtp_src_table_t *p_table)
{        
    if (!p_session || 
        !p_table) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return; 
    }

    VQEC_DEBUG(VQEC_DEBUG_UPCALL,
               "channel %s, repair session state %s received upcall event\n",
               vqec_rtp_session_state_tostr(p_session->state),
               vqec_chan_print_name(p_session->chan));
    
    switch (p_session->state) {

    case VQEC_RTP_SESSION_STATE_INACTIVE_WAITFIRST:
        /*
         * The session state is inactive after channel change. Add the
         * all new data source member(s) to the repair session. If the 
         * control-plane has a ssrc filter, then do not add sources that
         * do not have the correct ssrc - these sources must have
         * been reported prior to an ssrc filter installation operation -
         * we can simply re-update the source table, and then re-do
         * the analysis. 
         */
        VQEC_ASSERT(p_session->num_src_ids == 0);
        
        /* FALLTHRU */
    case VQEC_RTP_SESSION_STATE_ACTIVE: 
                
        if (!rtp_repair_update_sources(p_session, p_table)) {
            /* Failure: enter error state. */
            rtp_repair_enter_error_state(p_session);
            p_session->state = VQEC_RTP_SESSION_STATE_ERROR;
            VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                       "channel %s, error in updating source members, "
                       "state machine in error state\n", 
                       vqec_chan_print_name(p_session->chan));

        } else if ((p_session->state != VQEC_RTP_SESSION_STATE_ACTIVE) &&
                   p_table->num_entries) {
            
            VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                       "Repair session state -to- active for channel %s\n",
                       vqec_chan_print_name(p_session->chan));
                p_session->state = VQEC_RTP_SESSION_STATE_ACTIVE;
        }
        break;

        
    case VQEC_RTP_SESSION_STATE_ERROR:
    default:
        /*
         * The session is in error state, all events are ignored.
         */
        break;
    }
}


/**---------------------------------------------------------------------------
 * Helper to remove all stale source entries from the control-plane.
 *
 * @param[in] p_session Pointer to the repair session.
 * @param[in] table Pointer to the source table.
 *---------------------------------------------------------------------------*/ 
static void 
rtp_repair_del_stale_sources (rtp_repair_recv_t *p_session,   
                              vqec_dp_rtp_src_table_t *table)
{
    int32_t i, j;
    rtp_source_id_t *local_id, remote_id;
    rtp_member_id_t member_id;
    boolean match;
    char str[INET_ADDRSTRLEN];

    for (i = 0; i < VQEC_DP_RTP_MAX_KNOWN_SOURCES; i++) {

        match = FALSE;
        local_id = &p_session->src_ids[i].id;
        if (is_null_rtp_source_id(local_id)) {
            continue;
        }

        for (j = 0; j < table->num_entries; j++) {
            memset(&remote_id, 0, sizeof(remote_id));
            remote_id.ssrc = table->sources[j].key.ssrc;
            remote_id.src_addr = table->sources[j].key.ipv4.src_addr.s_addr;
            remote_id.src_port = table->sources[j].key.ipv4.src_port;
            
            if (same_rtp_source_id(local_id, &remote_id)) {
                match = TRUE;
                break;
            }
        }
        if (!match) {
            memset(&member_id, 0, sizeof(member_id));
            member_id.type = RTP_SMEMBER_ID_RTP_DATA;
            member_id.ssrc = local_id->ssrc;
            member_id.src_addr = local_id->src_addr;
            member_id.src_port = local_id->src_port;
            member_id.cname = NULL;
            /*
             * The remove_member vector will invoke the derived class member
             * delete vector - the lock prevents execution of the vector.
             */
            RTP_REPAIR_LOCK_RECURSIVE_DEL();
            MCALL((rtp_session_t *)p_session, 
                  rtp_remove_member_by_id, &member_id, TRUE);
            RTP_REPAIR_UNLOCK_RECURSIVE_DEL();
            p_session->num_src_ids--;

            VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                       "channel %s, deletion of rtp source 0x%x:%s:%u succeeded "
                       " source count in cp: %d\n",
                       vqec_chan_print_name(p_session->chan),
                       local_id->ssrc,
                       uint32_ntoa_r(local_id->src_addr, str, sizeof(str)),
                       ntohs(local_id->src_port), p_session->num_src_ids);
            
            memset(local_id, 0, sizeof(*local_id)); 
        }        
    }
}


/**---------------------------------------------------------------------------
 * The primary packet flow source has changed - install a new SSRC filter.
 * The sources for the session are updated after the new SSRC filter 
 * is installed.
 *
 * @param[in] session Pointer to the repair session.
 * @param[in] id Attributes of the new source.
 *---------------------------------------------------------------------------*/ 
static void 
rtp_repair_primary_pktflow_src_update (rtp_repair_recv_t *p_session,   
                                       rtp_source_id_t *id)
{    
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    vqec_dp_error_t err;
    vqec_dp_rtp_src_table_t table;

    if (!p_session || 
        !id ||
        is_null_rtp_source_id(id)) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return;        
    }

    VQEC_DEBUG(VQEC_DEBUG_UPCALL,
               "channel %s, repair session primary pktflow update\n",
               vqec_chan_print_name(p_session->chan));

    /* IPC to install a new SSRC filter in the dataplane. */
    memset(&table, 0, sizeof(table));
    err = vqec_dp_chan_input_stream_rtp_session_add_ssrc_filter(
        p_session->dp_is_id,
        id->ssrc,
        &table);
    if (err != VQEC_DP_ERR_OK) {
        /* failure on SSRC filter update in dataplane */
        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s, IPC to install SSRC filter %u in dataplane failed "
                 "enter error state",
                 vqec_chan_print_name(p_session->chan),                 
                 id->ssrc);
        syslog_print(VQEC_ERROR, msg_buf);  

        s_rtp_repair_error_cause = RTP_REPAIR_ERROR_CAUSE_IPCSSRC;
        rtp_repair_enter_error_state(p_session);
        p_session->state = VQEC_RTP_SESSION_STATE_ERROR;
        return;
    }

    /* Ensure that the dataplane source table is sane. */
    if (!rtp_repair_check_src_table(p_session, 
                                    &table)) {

        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s, install SSRC filter %u failed (bad table) "
                 "enter error state",
                 vqec_chan_print_name(p_session->chan), 
                 id->ssrc);
        syslog_print(VQEC_ERROR, msg_buf);  

        s_rtp_repair_error_cause = RTP_REPAIR_ERROR_CAUSE_BADTABLE;
        rtp_repair_enter_error_state(p_session);
        p_session->state = VQEC_RTP_SESSION_STATE_ERROR;
        return;
    }

    /*
     * Remove / re-add all sources in the control-plane from what's
     * present in the received table - again an O(N^2) loop, but for small 
     * value, N=3 this is not an issue.
     */
    rtp_repair_del_stale_sources(p_session, &table);
    if (!rtp_repair_find_add_new_sources(p_session, &table)) {
        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s, install SSRC filter %u failed (source add) "
                 "enter error state",
                 vqec_chan_print_name(p_session->chan),                 
                 id->ssrc);
        syslog_print(VQEC_ERROR, msg_buf);  

        rtp_repair_enter_error_state(p_session);
        p_session->state = VQEC_RTP_SESSION_STATE_ERROR;
        return;        
    } 

    /* Ensure that everything is in sync now */
    if (!rtp_repair_compare_src_table(p_session, &table)) {
        
        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s, install SSRC filter %u failed (out-of-sync) "
                 "enter error state",
                 vqec_chan_print_name(p_session->chan),                 
                 id->ssrc);
        syslog_print(VQEC_ERROR, msg_buf);  

        s_rtp_repair_error_cause = RTP_REPAIR_ERROR_CAUSE_OUTOFSYNC;
        rtp_repair_enter_error_state(p_session);
        p_session->state = VQEC_RTP_SESSION_STATE_ERROR;
        return;        
    }

    p_session->src_ssrc_filter = id->ssrc;
    p_session->ssrc_filter_en = TRUE;

    VQEC_DEBUG(VQEC_DEBUG_UPCALL,
               "channel %s, installed ssrc filter %u on repair session\n",
               vqec_chan_print_name(p_session->chan),
               p_session->src_ssrc_filter);

    if ((p_session->state != VQEC_RTP_SESSION_STATE_ACTIVE) &&
        p_session->num_src_ids && 
        table.num_entries) {
        
        VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                   "Repair session state -to- active for channel %s\n",
                   vqec_chan_print_name(p_session->chan));
        p_session->state = VQEC_RTP_SESSION_STATE_ACTIVE;
    }
}


/**---------------------------------------------------------------------------
 * Inject a RTCP packet into the repair RTCP socket.
 * 
 * @param[in] p_repair Pointer to the repair session.
 * @param[in] remote_addr IP address to which the packet is sent.
 * @param[in] remote_port UDP port to which the packet is sent.
 * @param[in] buf Pointer to the packet contents.
 * @param[in] len Length of the contents.
 * @param[out] boolean Returns true if the packet was successfully enQd
 * in the socket.
 *---------------------------------------------------------------------------*/ 
static boolean
rtp_repair_send_to_rtcp_socket (rtp_repair_recv_t *p_repair,
                                in_addr_t remote_addr, 
                                in_port_t remote_port,
                                char *buf, 
                                uint16_t len) 
{
    struct sockaddr_in dest_addr;
    int32_t tx_len;

    if (!p_repair || !buf || !len) {
        return (FALSE);
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = remote_addr;
    dest_addr.sin_port = remote_port;
        
    tx_len = sendto(p_repair->repair_rtcp_sock->fd,
                    buf, 
                    len,
                    0,
                    (struct sockaddr *)&dest_addr, 
                    sizeof(struct sockaddr_in));

    return (len == tx_len);
}


/**---------------------------------------------------------------------------
 * Send a BYE on the repair session.
 * 
 * @param[in] pp_repair_recv Pointer to the repair session.
 *---------------------------------------------------------------------------*/ 
void 
rtp_session_repair_recv_send_bye (rtp_repair_recv_t *pp_repair_recv)
{
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t bye;
    rtcp_msg_info_t xr;
    uint32_t send_result;
    rtp_session_t *p_sess;

    if (pp_repair_recv == NULL) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return;
    }

    /*
     * Send a BYE packet every time we call this fucntion,
     * except if we may not send RTCP (e.g., due to zero RTCP bw).
     */
    if (rtcp_may_send((rtp_session_t *)pp_repair_recv)) {
        rtcp_init_pkt_info(&pkt_info);
        rtcp_init_msg_info(&bye);
        rtcp_set_pkt_info(&pkt_info, RTCP_BYE, &bye);

        /* Depending on the configuration, include RTCP XR or not */
        p_sess = (rtp_session_t *)(pp_repair_recv);
        if (is_rtcp_xr_enabled(&p_sess->rtcp_xr_cfg)) {
            rtcp_init_msg_info(&xr);
            rtcp_set_pkt_info(&pkt_info, RTCP_XR, &xr);
        }

        send_result = MCALL((rtp_session_t *)pp_repair_recv,
                            rtcp_send_report,
                            pp_repair_recv->rtp_local_source,
                            &pkt_info,
                            TRUE,  /* re-schedule next RTCP report */
                            TRUE); /* reset RTCP XR stats */
        if (send_result == 0) {
            syslog_print(VQEC_ERROR, "RTP: repair session sends RTCP BYE: Failed \n");
        } else {
            VQEC_DEBUG(VQEC_DEBUG_RTCP, 
                       "repair session bye sent "
                       " at time (msec) %llu\n", 
                       TIME_GET_A(msec, get_sys_time())); 
        }
    }
}


/**---------------------------------------------------------------------------
 * From a repair session to channel identifier of parent channel.
 * 
 * @param[in] p_repair Pointer to the repair session.
 * @param[out] vqec_chanid_t Parent channel's id.
 *---------------------------------------------------------------------------*/ 
vqec_chanid_t
rtp_repair_session_chanid (rtp_repair_recv_t *p_repair)
{
    VQEC_ASSERT(p_repair != NULL);
    VQEC_ASSERT(p_repair->chan != NULL);

    return (vqec_chan_to_chanid(p_repair->chan));
}


/**
 * Function:    rtp_repair_recv_set_methods
 * Description: Sets the function pointers in the func ptr table
 * @param[in] table pointer to a function ptr table which gets populated by 
 * this funciton
 */
static void rtp_repair_recv_set_methods (rtp_repair_recv_methods_t * table)
{
  /* First set the the base method functions */ 
  rtp_ptp_set_methods((rtp_ptp_methods_t *)table);

  /* Now override and/or populate derived class functions */
  table->rtcp_construct_report = rtcp_construct_report_repair_recv;

  /* statistics collection method. */
  table->rtp_update_receiver_stats = rtp_repair_update_receiver_stats;

  /* also override the delete member method for dataplane delete. */
  table->rtp_delete_member = rtp_repair_delete_member;

  /* upcall event processor. */
  table->process_upcall_event = rtp_repair_upcall_ev;

  /* pktflow source update method. */
  table->primary_pktflow_src_update = rtp_repair_primary_pktflow_src_update;

  /* inject packets into rtcp socket. */
  table->send_to_rtcp_socket = rtp_repair_send_to_rtcp_socket;
}


/**
 * Function:    rtp_repair_recv_set_rtcp_handler
 * Description: Sets the function pointers for RTCP handlers
 * @param[in] table Table of rtcp handlers
 */
static void rtp_repair_recv_set_rtcp_handlers(rtcp_handlers_t *table)
{
    /* Set base rtp session handler first */
    rtp_ptp_set_rtcp_handlers(table);

    /* Now override or add new handlers */
    table->proc_handler[RTCP_APP]  = rtcp_process_app;
}

/*
 * rtp_repair_recv_init_memory
 *
 * Initializes the RTCP memory used by repair sessions.
 *
 * @param[in] memory Pointer to the memory info struct.
 * @param[out] boolean TRUE on success, FALSE on failure.
 */
static boolean
rtp_repair_recv_init_memory (rtcp_memory_t *memory, int max_sessions)
{
    /* 
     * max_members over all sessions. These are all allocated
     * as channel members (essentially upstream members on a 
     * per channel basis)
     */
    int max_channel_members = max_sessions * VQEC_RTP_MEMBERS_MAX;

    rtcp_cfg_memory(memory,
		    "RPR-RECV",
		    /* session size */
		    sizeof(rtp_repair_recv_t),  
		    max_sessions,
                    0,
                    0,
		    sizeof(rtp_member_t),
		    max_channel_members);
    /* 
     * Note that if it fails, rtcp_init_memory will clean up
     * any partially allocated memory, before it returns 
     */
    return (rtcp_init_memory(memory));
}


/**
 * Function:    rtp_repair_recv_init_module
 * Description: One time (class level) initialization of various function tables
 * @param[in] max_sessions The maximum number of repair sessions to be created
 * MUST BE AT LEAST 1.
 * @param[out] boolean TRUE(1) if successfully initialized this module
 * else returns FALSE(0).
 */
boolean rtp_repair_recv_init_module (uint32_t max_sessions)
{
    if (s_rtp_repair_recv_initted) {
        return TRUE;
    }
    if (max_sessions == 0) {
        return FALSE;
    }
    
    rtp_repair_recv_set_methods(&rtp_repair_recv_methods_table);
    rtp_repair_recv_set_rtcp_handlers(&rtp_repair_recv_rtcp_handler);
    if (rtp_repair_recv_init_memory(&rtp_repair_recv_memory, 
                                    (int)max_sessions)) {
        s_rtp_repair_recv_initted = 1;
        return TRUE;
    } else {
        syslog_print(VQEC_ERROR, 
                     "repair session module initialization failed\n");
        return FALSE;
    }        
}


