/*
 * rtp_era_recv.c - Functions to handle ERA_RECV (error repair aware
 *                  receiver RTP session).  This session is
 *                  also called the "primary" session.
 * 
 * Copyright (c) 2006-2010 by cisco Systems, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <sys/time.h>
#include "rtp_repair_recv.h"
#include "rtp_era_recv.h"
#include "rtcp_stats_api.h"
#include "vqec_debug.h"
#include "vqec_event.h"
#include "vqec_assert_macros.h"
#include "vqec_channel_private.h"
#include <utils/vam_util.h>
#include <rtp/rtp_util.h>

// YouView changes
#include "vqec_dp_api.h"


#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

static rtp_era_recv_methods_t rtp_era_recv_ptp_methods_table;
static rtp_era_recv_methods_t rtp_era_recv_ssm_methods_table;
static rtcp_handlers_t   rtp_era_recv_ssm_rtcp_handler;
static rtcp_handlers_t   rtp_era_recv_ptp_rtcp_handler;
static rtcp_memory_t     rtp_era_recv_memory;
static uint32_t s_rtp_era_recv_initted=FALSE;  /* Set to TRUE when init'd */

/*
 * rtp_era_recv_init_memory
 *
 * Initializes the memory used by RTCP for ERA RECV.
 *
 * Parameters:
 *                 memory: ptr to memory info struct.
 *                 max_session: maximum number of sessions
 *
 * Returns:        TRUE on success, FALSE on failure.
 */
boolean
rtp_era_recv_init_memory (rtcp_memory_t *memory, int max_sessions)
{
    /* 
     * max_members over all sessions. These are all allocated
     * as channel members (essentially upstream members on a 
     * per channel basis)
     */
    int max_channel_members = max_sessions * VQEC_RTP_MEMBERS_MAX;
    
    rtcp_cfg_memory(memory,
		    "ERA-RECV",
		    /* session size */
		    sizeof(rtp_era_recv_t),  
		    max_sessions,
                    0,
                    0,
		    sizeof(era_recv_member_t),
                    max_channel_members);

    /* 
     * Note that if it fails, rtcp_init_memory will clean up
     * any partially allocated memory, before it returns 
     */
    return (rtcp_init_memory(memory));
}

/*
 * rtcp_construct_report_era_recv
 *
 * Construct a report including the PUBPORTS message.
 * This is an overriding implementation of the rtcp_construct_report method,
 * for the "era-recv" application-level class.
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
uint32_t rtcp_construct_report_era_recv (rtp_session_t   *p_sess,
                                         rtp_member_t    *p_source, 
                                         rtcptype        *p_rtcp,
                                         uint32_t         bufflen,
                                         rtcp_pkt_info_t *p_pkt_info,
                                         boolean          reset_xr_stats)
{
    boolean primary = TRUE;
    in_port_t rtp_port;
    in_port_t rtcp_port;
    vqec_chan_t *chan;    

    if (!p_sess) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return (0);
    }

    chan = ((rtp_era_recv_t *)p_sess)->chan;
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
 * Primary session's RTCP event handler.
 *
 * @param[in] dptr Pointer to the primary session object. 
 * All other arguments are unused.
 *---------------------------------------------------------------------------*/ 
void
primary_rtcp_event_handler (const vqec_event_t * const evptr, 
                            int fd, short event, void *dptr) 
{
    rtp_era_recv_t *p_era_sess = dptr;

    VQEC_ASSERT(p_era_sess != NULL);
    VQEC_ASSERT(p_era_sess->chan != NULL);
    VQEC_ASSERT((g_channel_module != NULL) && 
                (g_channel_module->rtcp_iobuf != NULL));
    
    rtcp_event_handler_internal((rtp_session_t *)p_era_sess,
                                p_era_sess->era_rtcp_sock, 
                                g_channel_module->rtcp_iobuf,
                                g_channel_module->rtcp_iobuf_len,
                                TRUE);
}


/** Function:    rtp_create_session_era_recv 
 * Description: Create the session structure for an RTP/RTCP session for 
 *              era_recv environment.
 * Parameters:  
 * @param[in] src_addr - sessions src address to use for transmitting packets.
 * @param[in] p_cname  - name 
 * @param[in] era_dest_addr - destination address of primary session
 * @param[in] recv_rtcp_port  - Client port on which RTCP packets are recvd.
                                May be zero to disable RTCP reception.
 * @param[in] send_rtcp_port  - Server port to which RTCP packets are sent.
                                May be zero to disable RTCP transmission.
 * @param[in] fbt_ip_addr    - ip address of feedback target
 * @param[in] rtcp_bw_cfg    - ptr to RTCP bandwidth config info
 * @param[in] rtcp_xr_cfg    - ptr to RTCP XR config info
 * @param[in] orig_source_addr - Original primary stream's source address.
 * @param[in] orig_source_port - Original primary stream's source port.
 * @param[in] sock_buf_depth - The rx socket buffer depth to be used.
 * @param[in] rtcp_dscp_value - DSCP value to use for RTCP packets sent
 * @param[in] rtcp_rsize  - Reduced size RTCP setting for this session
 * @param[in] parent_chan - Pointer to parent channel.
 *
 * @return pointer to rtp_era_recv_t session that is allocated,
 *         or NULL on failure.
 */
rtp_era_recv_t *rtp_create_session_era_recv (vqec_dp_streamid_t dp_id,
                                             in_addr_t src_addr,
                                             char      *p_cname,
                                             in_addr_t era_dest_addr,
                                             uint16_t  recv_rtcp_port,
                                             uint16_t  send_rtcp_port,
                                             in_addr_t fbt_ip_addr,
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
    rtp_era_recv_t *p_era_recv = NULL;
    struct timeval tv;
    rtp_config_t config;
    rtp_member_id_t member_id;
    uint32_t interval = 0;

    if (!parent_chan || !rtcp_bw_cfg) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return (NULL);
    }

    /*
     * Allocate according to the derived class size, not base size
     * The memory allocated is enough not only for the sess but also the
     * methods and the infrastructure for the member cache.
     * Set up a pointer to per-class memory, for future allocations.
     */
    p_era_recv = rtcp_new_session(&rtp_era_recv_memory);
    if (!p_era_recv) {
        syslog_print(VQEC_ERROR, "failed to create new rtcp era session");
        return (NULL);
    } 
    memset(p_era_recv, 0, sizeof(rtp_era_recv_t));    
    p_era_recv->rtcp_mem = &rtp_era_recv_memory;

    if (recv_rtcp_port) {
        /* 
         * Only create the RTCP receive socket if the 
         * recv_rtcp_port is non-zero.
         */
    
        p_era_recv->era_rtcp_sock = 
            vqec_recv_sock_create("era rtcp sock",
                                  src_addr,      
                                  recv_rtcp_port,
                                  era_dest_addr,
                                  FALSE,
                                  sock_buf_depth,
                                  rtcp_dscp_value);
        if (!p_era_recv->era_rtcp_sock) {
            syslog_print(VQEC_ERROR, "unable to open recv rtcp socket\n");
            goto fail;
        }

        /*
         * Note data packets are processed on a per packet event
         */
        if (!vqec_event_create(&p_era_recv->era_rtcp_sock->vqec_ev,
                               VQEC_EVTYPE_FD, 
                               VQEC_EV_READ | VQEC_EV_RECURRING,
                               primary_rtcp_event_handler, 
                               p_era_recv->era_rtcp_sock->fd,
                               p_era_recv) ||
            !vqec_event_start(p_era_recv->era_rtcp_sock->vqec_ev, NULL)) {
            vqec_event_destroy(&p_era_recv->era_rtcp_sock->vqec_ev);
            syslog_print(VQEC_ERROR, "failed to add event");
            goto fail;
        }
    }

    /* Set multicast flag */
    p_era_recv->is_ssm = IN_MULTICAST(ntohl(era_dest_addr));

    if (send_rtcp_port) {
        /*
         * Create a transmit socket for sending rtcp messages.
         * Only create if the server-side RTCP port is known.
         */
        if (!p_era_recv->is_ssm &&
            p_era_recv->era_rtcp_sock != NULL) {
            /* for unicast share one socket for rx and tx */
            p_era_recv->era_rtcp_xmit_sock =
                vqec_recv_sock_get_fd(p_era_recv->era_rtcp_sock);
        } else {
            /* for multicast, must create separate tx socket */
            p_era_recv->era_rtcp_xmit_sock =
                vqec_send_socket_create(src_addr, 0, rtcp_dscp_value);
        }
        if (p_era_recv->era_rtcp_xmit_sock < 0) {
            syslog_print(VQEC_ERROR,
                         "failed to create an era rtcp xmit sock");
            goto fail;
        }
    } else {
        /* send_rtcp_port is 0 */

        /* Assign an invalid fd to the xmit_sock. */
        p_era_recv->era_rtcp_xmit_sock = -1;
        
    }

    rtp_init_config(&config, RTPAPP_STBRTP);
    config.session_id = 
        rtp_taddr_to_session_id(orig_source_addr, orig_source_port);

    config.rtcp_sock = p_era_recv->era_rtcp_xmit_sock;
    config.rtcp_dst_addr = fbt_ip_addr;
    config.rtcp_dst_port = send_rtcp_port;
    config.rtcp_bw_cfg = *rtcp_bw_cfg;
    config.rtcp_xr_cfg = *rtcp_xr_cfg;
    config.rtcp_rsize = rtcp_rsize;
    config.senders_cached = 1;

    if (p_era_recv->is_ssm) {
        /*
         * Construct the "base" ssm_rsi_rcvr session from
         * which this "class" derives.
         * This also will set up the ssm_rsi session methods.
         */
        f_init_successful = 
            rtp_create_session_ssm_rsi_rcvr(&config,
                                            (rtp_ssm_rsi_rcvr_t **)&p_era_recv,
                                            FALSE);
    } else {
        /*
         * First perform all the parent session checks and initializations.
         * This also will set up the parent session methods.
         */
        f_init_successful =
            rtp_create_session_ptp(&config,
                                   (rtp_ptp_t **)&p_era_recv,
                                   FALSE);
    }
    
    if (f_init_successful == FALSE) {
        syslog_print(VQEC_ERROR,
                     "Failed to create rtcp session\n");
        goto fail;
    }     
    
    /* Set the methods table */
    p_era_recv->__func_table = p_era_recv->is_ssm ?
        &rtp_era_recv_ssm_methods_table : &rtp_era_recv_ptp_methods_table;
    
    /* Set RTCP handlers */
    p_era_recv->rtcp_handler = p_era_recv->is_ssm ?
        &rtp_era_recv_ssm_rtcp_handler : &rtp_era_recv_ptp_rtcp_handler;
    
    /* 
     * This is where any ERA_RECV type-specific methods would be
     * set to override the base session methods. So far there 
     * are none.
     */
    
    /* 
     * Add ourselves as a member of the session.
     */
    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    /* 
     * the actual SSRC value will be randomly selected
     * and written here by the rtp_create_local_source method
     */     
    member_id.ssrc = 0;
    member_id.subtype = RTCP_CHANNEL_MEMBER;
    member_id.src_addr = p_era_recv->send_addrs.src_addr;
    member_id.src_port = p_era_recv->send_addrs.src_port;
    member_id.cname = p_cname;

    MCALL((rtp_session_t *)p_era_recv,
          rtp_create_local_source,
          &member_id,
          RTP_SELECT_RANDOM_SSRC);

    /* Timer events to send RTCP reports, managing lists */
    
    /* Member timeout */
    timerclear(&tv);
    interval = MCALL((rtp_session_t *)p_era_recv,
                     rtcp_report_interval,
                     FALSE, TRUE /* jitter the interval */);
    tv = TIME_GET_R(timeval,TIME_MK_R(usec, (int64_t)interval)) ;
    p_era_recv->member_timeout_event = 
        rtcp_timer_event_create(VQEC_RTCP_MEMBER_TIMEOUT, &tv,
                                p_era_recv);
    if (p_era_recv->member_timeout_event == NULL) {
        syslog_print(VQEC_ERROR,
                     "Failed to setup "
                     "member_timeout_event for era session\n");
        goto fail;
    }
     
     
    if (send_rtcp_port) {
        /*
         * send report timeout
         * Only enable sender report timeout when RTCP 
         * is enabled (i.e. send_rtcp_port is non-zero).
         */
        
        timerclear(&tv);
        abs_time_t now = get_sys_time();
        rel_time_t diff = TIME_SUB_A_A(p_era_recv->next_send_ts, now);
        tv = TIME_GET_R(timeval, diff);
        p_era_recv->send_report_timeout_event = 
            rtcp_timer_event_create(VQEC_RTCP_SEND_REPORT_TIMEOUT, 
                                    &tv, p_era_recv);
        if (p_era_recv->send_report_timeout_event == NULL) {
            syslog_print(VQEC_ERROR, "Failed to setup "
                         "send_report_timeout_event for era session\n");
            goto fail;
        }
    }
    
    p_era_recv->dp_is_id = dp_id;
    p_era_recv->chan = parent_chan;

// YouView changes BEGIN
    if(vqec_syscfg_get_ptr()->sig_mode == VQEC_SM_MUX)
    {
        rtp_session_t * p_sess = (rtp_session_t *)p_era_recv;
        p_sess->rtcp_socket = vqec_dp_graph_filter_fd(parent_chan->graph_id);
    }
// YouView changes END

    return (p_era_recv);
    
fail:
    rtp_delete_session_era_recv(&p_era_recv, TRUE);
    return NULL;
}

/** Function:    rtp_update_session_era_recv 
 * Description:  Updates the session structure for an RTP/RTCP session for 
 *               era_recv environment.
 * Parameters:  
 * @param[in] p_era_recv     session to be updated
 * @param[in] fbt_ip_addr    ip address of feedback target
 * @param[in] send_rtcp_port server port to which RTCP packets are sent.
 * @param[out] boolean       TRUE upon successful update, 
 *                           FALSE upon failure
 */
boolean
rtp_update_session_era_recv (rtp_era_recv_t *p_era_recv,
                             in_addr_t fbt_ip_addr,
                             uint16_t send_rtcp_port)
{
    boolean success = TRUE;
    
    if ((!p_era_recv->send_addrs.dst_port && send_rtcp_port) ||
        (p_era_recv->send_addrs.dst_port && !send_rtcp_port)) {
        /*
         * Enabling/Disabling of RTCP messages during a source change
         * is currently not supported.
         */
        syslog_print(VQEC_ERROR, "Enabling/Disabling RTCP reports during "
                     "a source switch is unsupported\n");
        success = FALSE;
        goto done;
    }
    p_era_recv->send_addrs.dst_addr = fbt_ip_addr;
    p_era_recv->send_addrs.dst_port = send_rtcp_port;

done:
    return (success);
}

/*
 * Function:    rtp_delete_session_era_recv
 * Description: Frees a session.  The session will be cleaned first using
 *              rtp_cleanup_session.
 * @param[in]  pp_sess  ptr ptr to session object
 * @param[in] deallocate boolean to check if the object should be destroyed
 */
void rtp_delete_session_era_recv (rtp_era_recv_t **pp_era_recv, 
                                  boolean deallocate)
{
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t bye;
    rtcp_msg_info_t xr;
    uint32_t send_result;
    rtp_session_t *p_sess;

    if ((pp_era_recv == NULL) || (*pp_era_recv == NULL)) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return;
    }
    
    if ((*pp_era_recv)->chan) {
        VQEC_DEBUG(VQEC_DEBUG_RTCP,
                   "Channel %s, primary session destroyed\n",
                   vqec_chan_print_name((*pp_era_recv)->chan));
    }

    /*
     * Send a BYE packet every time we delete an ERA session,
     * except if we may not send RTCP (e.g., due to zero RTCP bw).
     */
    if (rtcp_may_send((rtp_session_t *)(*pp_era_recv))) {
        rtcp_init_pkt_info(&pkt_info);
        rtcp_init_msg_info(&bye);
        rtcp_set_pkt_info(&pkt_info, RTCP_BYE, &bye);

        /* Depending on the configuration, include RTCP XR or not */
        p_sess = (rtp_session_t *)(*pp_era_recv);
        if (is_rtcp_xr_enabled(&p_sess->rtcp_xr_cfg)) {
            rtcp_init_msg_info(&xr);
            rtcp_set_pkt_info(&pkt_info, RTCP_XR, &xr);
        }

        send_result = MCALL((rtp_session_t *)(*pp_era_recv),
                            rtcp_send_report,
                            (*pp_era_recv)->rtp_local_source,
                            &pkt_info,
                            TRUE,  /* re-schedule next RTCP report */
                            TRUE); /* reset RTCP XR stats */
        if (send_result == 0) {
            syslog_print(VQEC_ERROR, "RTP: Delete ERA-Source Session: Failed "
                         "to send rtcp packet\n");
        }
    }
       
    /* RTCP session timers */
    if ((*pp_era_recv)->send_report_timeout_event) {
        vqec_event_destroy(&(*pp_era_recv)->send_report_timeout_event);
    }

    if ((*pp_era_recv)->member_timeout_event){
        vqec_event_destroy(&(*pp_era_recv)->member_timeout_event);
    }

    /* 
     * delete old sockets if they exist 
     */
    if ((*pp_era_recv)->era_rtcp_xmit_sock >= 0) {
        /* close tx socket if it differs from rx socket */
        if ((*pp_era_recv)->era_rtcp_sock == NULL ||
            (*pp_era_recv)->era_rtcp_xmit_sock != 
            vqec_recv_sock_get_fd((*pp_era_recv)->era_rtcp_sock)) {
            close((*pp_era_recv)->era_rtcp_xmit_sock);
        }
        (*pp_era_recv)->era_rtcp_xmit_sock = -1;
    }
    if ((*pp_era_recv)->era_rtcp_sock) {
        /* remove the event from the handler */
        vqec_event_destroy(&((*pp_era_recv)->era_rtcp_sock)->vqec_ev);
        vqec_recv_sock_destroy((*pp_era_recv)->era_rtcp_sock);
        (*pp_era_recv)->era_rtcp_sock = 0;
    }

    if ((*pp_era_recv)->is_ssm) {
        rtp_delete_session_ssm_rsi_rcvr((rtp_ssm_rsi_rcvr_t **)pp_era_recv, 
                                        FALSE);
    } else {
        rtp_delete_session_ptp((rtp_ptp_t **)pp_era_recv, FALSE);
    }

    if (deallocate) {
        rtcp_delete_session(&rtp_era_recv_memory, *pp_era_recv);
        *pp_era_recv = NULL;
    }

}


/**---------------------------------------------------------------------------
 * Show session statistics for an ERA session. 
 *
 * @param[in] p_era_sess Pointer to the ERA session.
 *---------------------------------------------------------------------------*/ 
#define ERA_SESSION_NAMESTR "Primary" /* must be 7 characters */
void 
rtp_show_session_era_recv (rtp_era_recv_t *p_era_sess)
{
    if (!p_era_sess) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return;
    }

    vqec_rtp_session_show(p_era_sess->dp_is_id, 
                          (rtp_session_t *)p_era_sess, 
                          ERA_SESSION_NAMESTR,
                          (p_era_sess->state != 
                           VQEC_RTP_SESSION_STATE_INACTIVE_WAITFIRST));    
}


/**--------------------------------------------------------------------------
 * Update RTP Diagnostic Counter statistics in the channel's control
 * plane cache.
 *
 * @param[in] p_session Pointer to the ERA session.
 *---------------------------------------------------------------------------
 */
static void 
rtp_era_update_receiver_xr_dc_stats (rtp_era_recv_t *p_era_sess)
{
    if (!is_rtcp_xr_dc_enabled(&p_era_sess->rtcp_xr_cfg)) {
        return;
    }
    
    memset(&p_era_sess->xr_dc_stats, 0, sizeof(p_era_sess->xr_dc_stats));
    vqec_chan_collect_xr_dc_stats(p_era_sess->chan,
                                  &p_era_sess->xr_dc_stats);
}

/**---------------------------------------------------------------------------
 * Update RTP multicast acquisition statistics in the channel's control
 * plane cache.
 *
 * @param[in] p_session Pointer to the ERA session.
 *---------------------------------------------------------------------------*/ 
static void 
rtp_era_update_receiver_xr_ma_stats (rtp_era_recv_t *p_era_sess)
{
    vqec_dp_error_t err;
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    
    if (!is_rtcp_xr_ma_enabled(&p_era_sess->rtcp_xr_cfg)) {
        return;
    }
    
    memset(&p_era_sess->xr_ma_stats, 0, sizeof(p_era_sess->xr_ma_stats));
            
    err = vqec_dp_chan_rtp_input_stream_xr_ma_get_info(
        p_era_sess->dp_is_id,
        &p_era_sess->xr_ma_stats);
            
    if (err != VQEC_DP_ERR_OK) {                
        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s: get xr ma info failed ",
                 vqec_chan_print_name(p_era_sess->chan));
        syslog_print(VQEC_ERROR, msg_buf);          
    } else {                
        VQEC_DEBUG(VQEC_DEBUG_RTCP,
                   "channel %s, got xr ma info ",
                   vqec_chan_print_name(p_era_sess->chan));
    }
    vqec_chan_collect_xr_ma_stats(p_era_sess->chan,
                                  TRUE,
                                  &p_era_sess->xr_ma_stats);
}


/**---------------------------------------------------------------------------
 * Update RTP statistics for the channel's senders, as the channel state
 * will be destroyed in the dataplane, but kept alive in the control-plane
 * to allow transmit of multiple byes.
 *
 * @param[in] p_session Pointer to the ERA session.
 *---------------------------------------------------------------------------*/ 
static void 
rtp_era_shutdown_cache_receiver_stats (rtp_era_recv_t *p_era_sess)
{
    rtp_member_t    *p_source;
    rtp_session_t   *p_sess = (rtp_session_t *)p_era_sess;
    
    /* Update the cache of the XR MA stats */
    rtp_era_update_receiver_xr_ma_stats(p_era_sess);

    /* Update the cache of the XR DC stats */
    rtp_era_update_receiver_xr_dc_stats(p_era_sess);

    /* Update the cache for each sender in the session */
    VQE_TAILQ_FOREACH(p_source, &(p_era_sess->senders_list),p_member_chain) {
        MCALL(p_sess, rtp_update_receiver_stats, p_source, TRUE);
    }
}


/**---------------------------------------------------------------------------
 * Update RTP statistics of an ERA session. 
 * This updates receiver stats for the session which are independent of
 * the remote sender.  Stats that are specific to a remote sender are
 * updated in the "rtp_update_receiver_stats" method.
 *
 * @param[in] p_session Pointer to the ERA session.
 * @param[in] reset_xr_stats [ignored] Caller should set to TRUE
 *---------------------------------------------------------------------------*/ 
static void 
rtp_era_update_stats (rtp_session_t *p_sess,
                      boolean reset_xr_stats)
{
    /* Update the cache of the XR MA stats */
    rtp_era_update_receiver_xr_ma_stats((rtp_era_recv_t *)p_sess);

    /* Update the cache of the XR DC stats */
    rtp_era_update_receiver_xr_dc_stats((rtp_era_recv_t *)p_sess);
}


/**---------------------------------------------------------------------------
 * Update RTP receiver statistics of an ERA session for a given remote sender.
 *
 * If XR stats are configured for the session and were successfully updated,
 * they will be referenced in these locations:
 *   p_era_recv_source->xr_stats
 *   p_era_recv_source->post_er_stats
 * If XR stats are not configured for the session (or could not be updated),
 * then the above fields will be NULL.
 *
 * @param[in] p_session Pointer to the ERA session.
 * @param[in] p_source Pointer to the per-member data for a remote source.
 * @param[in] reset_xr_stats True if XR stats should be reset.  If this is
 *                          set to TRUE, and RTCP XR is enabled on the session,
 *                          then the XR stats will be collected from the DP and
 *                          stored in the session structure (with a pointer to
 *                          it being stored in the source structure).
 *---------------------------------------------------------------------------*/ 
static void 
rtp_era_update_receiver_stats (rtp_session_t *p_sess,
                               rtp_member_t *p_source,
                               boolean reset_xr_stats)
{
    vqec_dp_rtp_src_entry_info_t src_info;
    vqec_dp_rtp_sess_info_t sess_info;
    vqec_dp_rtp_src_key_t src_key;
    rtp_era_recv_t *p_era_sess;
    era_recv_member_t *p_era_recv_source;
    char str[INET_ADDRSTRLEN];
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    vqec_dp_error_t err;
    boolean do_xr_stats, do_post_er;

    if (!p_sess || 
        !p_source) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return;
    }

    if (((rtp_era_recv_t *)p_sess)->dp_is_id == VQEC_DP_STREAMID_INVALID) {
        /* 
         * Stats were cached at time of channel shutdown; 
         * no additional updates needed.
         */
        return;
    }

    p_era_sess = (rtp_era_recv_t *)p_sess;
    p_era_recv_source = (era_recv_member_t *)p_source;
    do_xr_stats = (is_rtcp_xr_enabled(&p_era_sess->rtcp_xr_cfg) &&
                   reset_xr_stats);
    do_post_er =  (is_post_er_loss_rle_enabled(&p_era_sess->rtcp_xr_cfg) &&
                   reset_xr_stats);

    memset(&sess_info, 0, sizeof(sess_info));
    memset(&src_key, 0, sizeof(src_key));
    memset(&src_info, 0, sizeof(src_info));
    
    src_key.ssrc = p_era_recv_source->ssrc;
    src_key.ipv4.src_addr.s_addr = p_era_recv_source->rtp_src_addr;
    src_key.ipv4.src_port = p_era_recv_source->rtp_src_port;

    if (do_xr_stats) {
        p_era_recv_source->xr_stats = &p_era_recv_source->xr_stats_data;
        memset(p_era_recv_source->xr_stats, 0, 
               sizeof(*p_era_recv_source->xr_stats));
    } else {
        p_era_recv_source->xr_stats = NULL;
    }
    if (do_post_er) {
        p_era_recv_source->post_er_stats = 
            &p_era_recv_source->post_er_stats_data;
        memset(p_era_recv_source->post_er_stats, 0,
               sizeof(*p_era_recv_source->post_er_stats));
    } else {
        p_era_recv_source->post_er_stats = NULL;
    }

    err = 
        vqec_dp_chan_rtp_input_stream_src_get_info(
            p_era_sess->dp_is_id,
            &src_key,
            TRUE,
            reset_xr_stats,
            &src_info,
            (do_xr_stats ? p_era_recv_source->xr_stats : NULL),
            (do_post_er ? p_era_recv_source->post_er_stats : NULL),
            NULL,
            &sess_info);

    if (err != VQEC_DP_ERR_OK) {

        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s: get info for RTP source %u:%s:%d: "
                 "primary RTCP session failed\n",
                 vqec_chan_print_name(p_era_sess->chan),
                 src_key.ssrc,
                 uint32_ntoa_r(src_key.ipv4.src_addr.s_addr, 
                               str, sizeof(str)), 
                 ntohs(src_key.ipv4.src_port));
        syslog_print(VQEC_WARNING, msg_buf);  
        p_era_recv_source->xr_stats = NULL;
        p_era_recv_source->post_er_stats = NULL;
    
    } else {

        VQEC_DEBUG(VQEC_DEBUG_RTCP,
                   "channel %s, got input stream info - "
                   "key=0x%x:%s:%d(%d)\n",
                   vqec_chan_print_name(p_era_sess->chan),
                   src_key.ssrc, 
                   uint32_ntoa_r(src_key.ipv4.src_addr.s_addr, 
                                 str, sizeof(str)), 
                   ntohs(src_key.ipv4.src_port), 
                   ntohs(p_era_recv_source->rtp_src_port)); 

    }
    
    p_era_recv_source->rcv_stats = src_info.src_stats;
    p_era_sess->rtp_stats = sess_info.sess_stats; 

    VQEC_DEBUG(VQEC_DEBUG_RTCP,
               "channel %s: primary session RTCP statistics: "
               "rcvd = %u ehsr = %0x08x\n",
               vqec_chan_print_name(p_era_sess->chan),
               p_era_recv_source->rcv_stats.received,
               p_era_recv_source->rcv_stats.cycles + 
               p_era_recv_source->rcv_stats.max_seq);    
}



typedef 
enum rtp_era_error_cause_
{
    RTP_ERA_ERROR_CAUSE_RTPDATASOURCE = 1,  /* RTP new data source  */
    RTP_ERA_ERROR_CAUSE_IPCDELETE,          /* IPC source delete  */
    RTP_ERA_ERROR_CAUSE_IPCPERMITFLOW,      /* IPC permit pktflow  */

} rtp_era_error_cause_t;
static rtp_era_error_cause_t s_rtp_era_error_cause;

/**---------------------------------------------------------------------------
 * Cleanup session's dataplane source state prior to entering error state.
 *
 * @param[in] p_session Pointer to the primary ERA session.
 *---------------------------------------------------------------------------*/ 
static void
rtp_era_enter_error_state (rtp_era_recv_t *p_session)
{
    rtp_member_id_t member_id;
    rtp_source_id_t source_id;
    vqec_dp_error_t err;
    vqec_dp_rtp_src_table_t table;
    uint32_t i;

    VQEC_DEBUG(VQEC_DEBUG_UPCALL,
               "channel %s, primary session, FSM entering error state, "
               "error cause %u\n",
               vqec_chan_print_name(p_session->chan),
               s_rtp_era_error_cause);

    /*
     * -- locally delete any cached pktflow source from control-plane.
     * -- fetch all source entries in the dataplane, delete the pktflow source.
     */
    memset(&table, 0, sizeof(table));
    if (!is_null_rtp_source_id(&p_session->pktflow_src.id)) {
        memset(&member_id, 0, sizeof(member_id));
        member_id.type = RTP_SMEMBER_ID_RTP_DATA;
        member_id.ssrc = p_session->pktflow_src.id.ssrc;
        member_id.src_addr = p_session->pktflow_src.id.src_addr;
        member_id.src_port = p_session->pktflow_src.id.src_port;
        member_id.cname = NULL;
        MCALL((rtp_session_t *)p_session, 
              rtp_remove_member_by_id, &member_id, TRUE); 
        memset(&p_session->pktflow_src, 0, sizeof(p_session->pktflow_src));
    }

    err = vqec_dp_chan_rtp_input_stream_src_get_table(p_session->dp_is_id,
                                                      &table);
    if (err == VQEC_DP_ERR_OK) {
        for (i = 0; i < table.num_entries; i++) {
            if (table.sources[i].info.pktflow_permitted) {
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
    }
}


/**---------------------------------------------------------------------------
 * Add a new source member to a rtp session.
 *
 * @param[in] p_session Pointer to the primary rtp session.
 * @param[in] p_source_id Pointer to the source member attributes.
 * @param[in] pktflow_entry Pointer to the dataplane source entry.
 * @param[out] boolean Returns true if the addition of the source member
 * succeeded, false otherwise.
 *---------------------------------------------------------------------------*/ 
static boolean
rtp_era_add_new_source (rtp_era_recv_t *p_session, 
                        rtp_source_id_t *p_source_id, 
                        vqec_dp_rtp_src_table_entry_t *pktflow_entry)
{
    boolean ret = FALSE;
    rtp_error_t rtp_ret;
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    char str[INET_ADDRSTRLEN];

    rtp_ret = MCALL((rtp_session_t *)p_session, 
                    rtp_new_data_source, p_source_id);

    if (rtp_ret != RTP_SUCCESS && rtp_ret != RTP_SSRC_EXISTS) {
        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s, addition of rtp source 0x%x:%s:%u "
                 "failed, reason (%d)",
                 vqec_chan_print_name(p_session->chan),
                 p_source_id->ssrc,
                 uint32_ntoa_r(p_source_id->src_addr, str, sizeof(str)),
                 ntohs(p_source_id->src_port), rtp_ret); 
        syslog_print(VQEC_ERROR, msg_buf);
        s_rtp_era_error_cause = RTP_ERA_ERROR_CAUSE_RTPDATASOURCE; 

    } else {

        VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                 "channel %s, addition of rtp source 0x%x:%s:%u succeeded\n", 
                   vqec_chan_print_name(p_session->chan),
                   p_source_id->ssrc,
                   uint32_ntoa_r(p_source_id->src_addr, str, sizeof(str)),
                   ntohs(p_source_id->src_port));
        
        memcpy(&p_session->pktflow_src.id, 
               p_source_id, sizeof(rtp_source_id_t));
        p_session->pktflow_src.thresh_cnt = pktflow_entry->info.thresh_cnt;
        ret = TRUE;
    }
    
    return (ret);
}


/**---------------------------------------------------------------------------
 * Promote a source to be the active pktflow source in the control and
 * dataplane.
 *
 * @param[in] p_session Pointer to the primary rtp session.
 * @param[in] pktflow_entry Pointer to the pktflow entry to be promoted.
 * @param[out] boolean Returns true if the promotion of the source member
 * succeeded, false otherwise.
 *---------------------------------------------------------------------------*/ 
static boolean 
rtp_era_promote_src (rtp_era_recv_t *p_session, 
                     vqec_dp_rtp_src_table_entry_t *pktflow_entry)
{
    rtp_source_id_t source_id;
    char str[INET_ADDRSTRLEN];
    boolean ret = FALSE;
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    rtp_repair_recv_t *repair_sess;
    vqec_dp_error_t err;

    memset(&source_id, 0, sizeof(source_id));
    source_id.ssrc = pktflow_entry->key.ssrc;
    source_id.src_addr = pktflow_entry->key.ipv4.src_addr.s_addr;
    source_id.src_port = pktflow_entry->key.ipv4.src_port;

    /* 
     * Inform dataplane of the new sender: this will promote the source 
     * to be designated as the pktflow source in the dataplane. This IPC
     * should not fail, except in cases that point to an error in the state
     * of the system.
     *
     * The dataplane will return the RTP session to source sequence offset
     * that is needed to map the new source's sequence space into that of
     * the old source (i.e. so that the packets are sequential across the
     * source transition).
     */
    err = vqec_dp_chan_rtp_input_stream_src_permit_pktflow(
        p_session->dp_is_id, &pktflow_entry->key, 
        &p_session->session_rtp_seq_num_offset);
    if (err != VQEC_DP_ERR_OK) {
        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s, promote source Ipc (%d) source 0x%x:%s:%u ",
                 vqec_chan_print_name(p_session->chan),
                 err,
                 source_id.ssrc,
                 uint32_ntoa_r(source_id.src_addr, str, sizeof(str)),
                 ntohs(source_id.src_port)); 
        syslog_print(VQEC_ERROR, msg_buf);
        s_rtp_era_error_cause = RTP_ERA_ERROR_CAUSE_IPCPERMITFLOW; 
        return (ret);
    }

    /* 
     * Add new sender to rtp. Failure happens for cases when resources
     * cannot be allocated, or collisions cannot be resolved. 
     */
    ret = rtp_era_add_new_source(p_session, 
                                 &source_id, 
                                 pktflow_entry);
    if (ret && p_session->chan->er_enabled) { 
        
        /*
         * Notify the repair session of update to the pktflow source - the
         * repair session uses this to install a SSRC filter.
         */
        repair_sess = vqec_chan_get_repair_session(p_session->chan);
        if (repair_sess) {
            MCALL(repair_sess, 
                  primary_pktflow_src_update, 
                  &p_session->pktflow_src.id); 
        }
    }

    return (ret);
}


/**---------------------------------------------------------------------------
 * Select and install a new active pktflow source on the primary session. 
 *
 * @param[in] session Pointer to the primary session.
 * @param[in] p_table Pointer to the dataplane RTP source table.
 * @param[out] boolean Returns true if either a new packetflow source 
 * is elected, or no electable packetflow sources are present.
 *---------------------------------------------------------------------------*/ 
static boolean
rtp_era_install_new_pktflow_src (rtp_era_recv_t *p_session, 
                                 vqec_dp_rtp_src_table_t *p_table)
{
    vqec_dp_rtp_src_table_entry_t *pktflow_entry = NULL;
    vqec_dp_rtp_src_table_entry_t *saved_pktflow_entry = NULL;
    char str[INET_ADDRSTRLEN];
    boolean ret = FALSE;
    vqec_chan_t *chan = NULL;
    vqec_ifclient_chan_event_args_t cb_args;

    /* 
     * Find dataplane's pktflow source, and check if it is active. If it is not
     * active elect a new active one, if possible. If no active source can be
     * selected, no new source is installed.
     */
    pktflow_entry = 
        vqec_rtp_src_table_find_pktflow_entry(p_table);
    saved_pktflow_entry = pktflow_entry;

    if (pktflow_entry && 
        (pktflow_entry->info.state != VQEC_DP_RTP_SRC_ACTIVE)) {

        VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                   "The dataplane pktflow source 0x%x:%s:%d for channel %s,  "
                   "primary session is inactive\n",
                   pktflow_entry->key.ssrc,
                   uint32_ntoa_r(
                       pktflow_entry->key.ipv4.src_addr.s_addr, 
                       str, sizeof(str)),
                   ntohs(pktflow_entry->key.ipv4.src_port),
                   vqec_chan_print_name(p_session->chan));

        pktflow_entry = NULL;
    }

    do {
        if (!pktflow_entry) {

            if (p_session->is_ssm) {
                /* Find the most recent active source. */
                pktflow_entry = 
                    vqec_rtp_src_table_most_recent_active_src(p_table);
            } else {
                /* Find the failover source. */
                pktflow_entry = vqec_rtp_src_table_failover_src(p_table);
            }
            if (!pktflow_entry) {
                /* No usable entry can be found - this is not an error. */
                VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                           "No active pktflow source available for channel %s, "
                           "primary session\n",
                           vqec_chan_print_name(p_session->chan)); 
                ret = TRUE;
                break;
            }
        }
   
        /*
         * We have a pktflow_entry to use. The following steps are followed:
         *
         *  -- the source is promoted to be the pktflow source in dataplane.
         *  -- the source member is added to rtp.
         *  -- the members for the repair session which do not have the same
         *      ssrc are invalidated.
         */        
        ret = rtp_era_promote_src(p_session, pktflow_entry);

        /* need to do a chan event callback, if the Ip or port changes */
        if(saved_pktflow_entry && 
           ((pktflow_entry->key.ipv4.src_addr.s_addr != 
             saved_pktflow_entry->key.ipv4.src_addr.s_addr) ||
            (pktflow_entry->key.ipv4.src_port != 
             saved_pktflow_entry->key.ipv4.src_port))){     
            VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                       "New source for channel %s, primary session\n",
                       vqec_chan_print_name(p_session->chan)); 
            chan = p_session->chan;
            if (chan && !chan->shutdown && 
                chan->chan_event_cb.chan_event_cb) {
                cb_args.event = VQEC_IFCLIENT_CHAN_EVENT_NEW_SOURCE;
                VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                           "New source cb, event= %d,\n", cb_args.event);
                (*chan->chan_event_cb.chan_event_cb)(
                    chan->chan_event_cb.chan_event_context,
                    &cb_args);
            }
        }

    } while (0);


    if (ret) {
        if (pktflow_entry) {
            p_session->state = VQEC_RTP_SESSION_STATE_ACTIVE;
        } else if (p_table->num_entries) { /* non-empty table */
            p_session->state = VQEC_RTP_SESSION_STATE_INACTIVE;
        }
    } else {
        rtp_era_enter_error_state(p_session);
        p_session->state = VQEC_RTP_SESSION_STATE_ERROR;         
    }

    return (ret);
}


/**---------------------------------------------------------------------------
 * Update the pktflow source on the primary session. This method is
 * invoked if the session is in the active or inactive states. 
 *
 * @param[in] session Pointer to the primary session.
 * @param[in] p_table Pointer to the dataplane RTP source table.
 * @param[out] boolean Returns true if the existing packetflow source
 * is not changed (including null source), or a new packetflow source 
 * is elected.
 *---------------------------------------------------------------------------*/ 
static boolean
rtp_era_update_pktflow_src (rtp_era_recv_t *p_session, 
                            vqec_dp_rtp_src_table_t *p_table)
{
    vqec_dp_rtp_src_table_entry_t *pktflow_entry;
    rtp_source_id_t source_id;
    char str[INET_ADDRSTRLEN];
    boolean ret = FALSE;

    /* 
     * If the control-plane does not have a pktflow source, elect a new one.
     */
    if (is_null_rtp_source_id(&p_session->pktflow_src.id)) { 
        return (rtp_era_install_new_pktflow_src(p_session,
                                                p_table));
    }


    /* 
     * There is an existing pktflow source:
     *   -- ensure that the source is active in the dataplane;
     *   -- if there have been threshold crossings, re-add the source to rtp.
     *   -- the control and dataplane must have the same pktflow source.
     */
    memset(&source_id, 0, sizeof(source_id));
    pktflow_entry = 
        vqec_rtp_src_table_find_pktflow_entry(p_table);
    if (pktflow_entry) {
        source_id.ssrc = pktflow_entry->key.ssrc;
        source_id.src_addr = pktflow_entry->key.ipv4.src_addr.s_addr;
        source_id.src_port = pktflow_entry->key.ipv4.src_port;
    }

    if (pktflow_entry &&
        same_rtp_source_id(&source_id, &p_session->pktflow_src.id)) {

        if (pktflow_entry->info.state != VQEC_DP_RTP_SRC_ACTIVE) {
            p_session->pktflow_src.thresh_cnt = pktflow_entry->info.thresh_cnt;
            ret = rtp_era_install_new_pktflow_src(p_session,
                                                  p_table);
            
        } else if (p_session->pktflow_src.thresh_cnt != 
                   pktflow_entry->info.thresh_cnt) {
            
            /* 
             * The pktflow source went active->inactive->active in the
             * dataplane; update the source with RTP, and set session
             * state to active.
             */
            VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                       "The pktflow source 0x%x:%s:%d for channel %s,  "
                       "primary session threshold count change %d/%d\n",
                       pktflow_entry->key.ssrc,
                       uint32_ntoa_r(
                           pktflow_entry->key.ipv4.src_addr.s_addr, 
                           str, sizeof(str)),
                       ntohs(pktflow_entry->key.ipv4.src_port),
                       vqec_chan_print_name(p_session->chan),
                       p_session->pktflow_src.thresh_cnt,
                       pktflow_entry->info.thresh_cnt);
            
            ret = rtp_era_add_new_source(p_session, 
                                         &p_session->pktflow_src.id, 
                                         pktflow_entry);
            if (ret) {
                p_session->state = VQEC_RTP_SESSION_STATE_ACTIVE;
            } else {
                rtp_era_enter_error_state(p_session);
                p_session->state = VQEC_RTP_SESSION_STATE_ERROR;
            }

        } else {
                                /* no action needs to be taken */
            ret = TRUE;
        }
        
    } else { /* else-of if (same_rtp_source_id(....)) */

        /* 
         * This is an error condition - the dataplane's pktflow source
         * should not change without explicit intervention from 
         * control-plane after the first-time initialization. To allow the
         * system to run, we generate a syslog, and elect a new pktflow
         * source from the source table.
         */
        syslog_print(VQEC_RTP_SESSION_STATE_ERROR, 
                     vqec_chan_print_name(p_session->chan), 
                     "primary session mismatch between data / control plane "
                     "pktflow source"); 
        
        ret = rtp_era_install_new_pktflow_src(p_session,
                                              p_table);
    }

    return (ret);
}


/**---------------------------------------------------------------------------
 * Process an upcall event for this session from the dataplane.
 *
 * @param[in] p_session Pointer to the primary session.
 * @param[in] p_table Pointer to the dataplane RTP source table.
 *---------------------------------------------------------------------------*/ 
static void 
rtp_era_upcall_ev (rtp_era_recv_t *p_session, 
                   vqec_dp_rtp_src_table_t *p_table)
{        
    vqec_rtp_session_state_t state;

    if (!p_session || 
        !p_table) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__); 
        return;        
    }

    state = p_session->state;
    if (state == VQEC_RTP_SESSION_STATE_SHUTDOWN) {
        syslog_print(VQEC_ERROR, 
                     "Primary session is shutdown prior to deletion"); 
        return; 
    }

    VQEC_DEBUG(VQEC_DEBUG_UPCALL,
               "channel %s, primary session state %s received upcall event\n",
               vqec_rtp_session_state_tostr(state),
               vqec_chan_print_name(p_session->chan));
    
    switch (state) {

    case VQEC_RTP_SESSION_STATE_INACTIVE_WAITFIRST:
        /*
         * The session state is inactive after channel change. It was expected
         * that the next event will be a new source: this must be the default
         * pktflow source in the dataplane.
         */
        VQEC_ASSERT(is_null_rtp_source_id(&p_session->pktflow_src.id));
        if (!rtp_era_install_new_pktflow_src(p_session, 
                                             p_table)) {
            VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                       "channel %s, primary session new pktflow promotion "
                       "error, state machine in error state\n", 
                       vqec_chan_print_name(p_session->chan));
        }
        break;


    case VQEC_RTP_SESSION_STATE_ACTIVE:
        /*
         * The session state is active, i.e., there is a previous pktflow 
         * source for the primary session. In this particular state there 
         * are two explicit cases to deal with:
         *
         *  --  The pktflow source in the dataplane must match the
         * control-plane's pktflow source, otherwise there is an error
         * in the state machine. The pktflow source information from the
         * dataplane consists of a "threshold-crossing" count, which counts
         *  the number of times the source has transitioned from active-
         * to-inactive-to-active states.The control-plane maintains the last
         * known value of this field. If the field has incremented, then 
         * RTP is "re-informed" of the presence of the data source.
         *
         *  -- If the session state is active, and there is a mismatch
         * between the control-plane and dataplane's view of pktflow
         * sources, the session's state is set to error, in which case the
         * session will ignore all other source updates until the next
         * channel change. [this is an explicit error case and points to
         * a software bug].
         */       
        if (!rtp_era_update_pktflow_src(p_session, 
                                        p_table)) {
            VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                       "channel %s, primary session update pktflow promotion "
                       "error, state machine in error state\n", 
                       vqec_chan_print_name(p_session->chan)); 
        }
        break;


    case VQEC_RTP_SESSION_STATE_INACTIVE:
        /*
         * The session state is inactive, there is no previous pktflow
         * source for the primary session. For this case we simply select
         * a new active source, and promote it. It is assumed that any
         * active source will not be deleted for a sufficiently long 
         * time-period after this upcall has been sent (this time-period
         * is set to 10-seconds).
         */
        if (!rtp_era_update_pktflow_src(p_session, 
                                        p_table)) {
            VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                       "channel %s, primary session update pktflow promotion error, "
                       "state machine in error state\n", 
                       vqec_chan_print_name(p_session->chan));
        }
        break;
        
    case VQEC_RTP_SESSION_STATE_ERROR:
        /*
         * The session is in error state, all events are ignored.
         */
        break;

    case VQEC_RTP_SESSION_STATE_SHUTDOWN:
        VQEC_ASSERT(FALSE);     /* not possible to reach this code point */
        break;
    }
    
    if (state != p_session->state) {
        VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                   "Primary session state updated for channel %s, "
                   "previous state %s, new state %s\n",
                   vqec_chan_print_name(p_session->chan),
                   vqec_rtp_session_state_tostr(state),
                   vqec_rtp_session_state_tostr(p_session->state));
    }
}


/**---------------------------------------------------------------------------
 * Send a BYE on the primary session.
 * 
 * @param[in] pp_era_recv Pointer to the primary session.
 *---------------------------------------------------------------------------*/ 
void 
rtp_session_era_recv_send_bye (rtp_era_recv_t *pp_era_recv)
{
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t bye;
    rtcp_msg_info_t xr;
    uint32_t send_result;
    rtp_session_t *p_sess;

    if (pp_era_recv == NULL) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return;
    }

    /*
     * Send a BYE packet every time we call this fucntion,
     * except if we may not send RTCP (e.g., due to zero RTCP bw).
     */
    if (rtcp_may_send((rtp_session_t *)pp_era_recv)) {
        rtcp_init_pkt_info(&pkt_info);
        rtcp_init_msg_info(&bye);
        rtcp_set_pkt_info(&pkt_info, RTCP_BYE, &bye);

        /* Depending on the configuration, include RTCP XR or not */
        p_sess = (rtp_session_t *)(pp_era_recv);
        if (is_rtcp_xr_enabled(&p_sess->rtcp_xr_cfg)) {
            rtcp_init_msg_info(&xr);
            rtcp_set_pkt_info(&pkt_info, RTCP_XR, &xr);
        }

        send_result = MCALL((rtp_session_t *)pp_era_recv,
                            rtcp_send_report,
                            pp_era_recv->rtp_local_source,
                            &pkt_info,
                            TRUE,  /* re-schedule next RTCP report */
                            TRUE); /* reset RTCP XR stats */
        if (send_result == 0) {
            syslog_print(VQEC_ERROR, "RTP: send RTCP BYE: Failed");
        } else {
            VQEC_DEBUG(VQEC_DEBUG_RTCP, 
                       "primary session bye sent "
                       "at time (msec) %llu\n", TIME_GET_A(msec, get_sys_time()));
        }
    }
}


/**---------------------------------------------------------------------------
 * Shutdown the session but allow for transmit of byes.
 * 
 * @param[in] pp_era_recv Pointer to the primary session.
 *---------------------------------------------------------------------------*/ 
static void
rtp_era_shutdown_allow_byes (rtp_era_recv_t *p_era_recv)
{
    if (p_era_recv) {
        if ((p_era_recv->state == VQEC_RTP_SESSION_STATE_ACTIVE) ||
            (p_era_recv->state == VQEC_RTP_SESSION_STATE_INACTIVE) ||
            (p_era_recv->state == VQEC_RTP_SESSION_STATE_INACTIVE_WAITFIRST)) {
            rtp_era_shutdown_cache_receiver_stats(p_era_recv);
        }
        
        /* delete session timers at this time */
        if (p_era_recv->send_report_timeout_event) {
            vqec_event_destroy(&p_era_recv->send_report_timeout_event);
        }
        if (p_era_recv->member_timeout_event) {
            vqec_event_destroy(&p_era_recv->member_timeout_event);
        }        
        p_era_recv->dp_is_id = VQEC_DP_STREAMID_INVALID;
        p_era_recv->state = VQEC_RTP_SESSION_STATE_SHUTDOWN;
    }
}

/**---------------------------------------------------------------------------
 * Send NCSI packet for RCC.
 *
 * @param[in] p_session Pointer to the primary session.
 * @param[in] seq_num First primary sequence number.
 * @param[in] join_latency Join latency.
 * @param[out] boolean Returns true if the NCSI message is enqueued
 * successfully on the tx socket.
 *---------------------------------------------------------------------------*/ 
#if HAVE_FCC
boolean
rtp_era_send_ncsi (rtp_era_recv_t *p_session,
                   uint32_t seq_num,
                   rel_time_t join_latency) 
{
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t app;
    ncsi_tlv_t tlv;
    uint32_t data_len;
    uint16_t seq_num_rtp;
    int send_result = 0;

    if (!p_session) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return (FALSE);
    }

    /* 
     * When we are finished constructing the repair and
     * primary sessions Send a PLI-Nack which signals picture
     * loss for the channel to which we have just tuned. 
     */
    rtcp_init_pkt_info(&pkt_info);
    rtcp_init_msg_info(&app);
    memcpy(&app.app_info.name, "NCSI", sizeof(uint32_t));

    memset(&tlv,0,sizeof(ncsi_tlv_t));
    seq_num_rtp = vqec_seq_num_to_rtp_seq_num(seq_num);
    /* The TLV encoder will change this value to network order */
    tlv.first_mcast_seq_number = (uint32_t)seq_num_rtp;
    tlv.first_mcast_recv_time = (uint32_t)TIME_GET_R(msec, join_latency);
    
    app.app_info.data = 
        (uint8_t *) ncsi_tlv_encode_allocate (tlv, &data_len);
    app.app_info.len = data_len;
    rtcp_set_pkt_info(&pkt_info, RTCP_APP, &app);
    
    send_result = MCALL((rtp_session_t *)p_session,
                        rtcp_send_report,
                        p_session->rtp_local_source,
                        &pkt_info,
                        TRUE,
                        FALSE);
    rcc_tlv_encode_destroy(app.app_info.data);

    return (send_result > 0);
}
#endif

/**---------------------------------------------------------------------------
 * Send a NAK-PLI to start a RCC burst. 
 *
 * @param[in] p_session Pointer to the primary session.
 * @param[in] rcc_min_fill RCC minimum fill TLV value.
 * @param[in] rcc_max_fill RCC maximum fill TLV value.
 * @param[in] do_fast_fill Specifies whether or not to do a fast-fill RCC.
 * @param[in] maximum_recv_bw Amount of receive bandwidth that is available for
 *                            the RCC operation.
 * @param[in] max_fastfill Maximum amount of data that can be fast-filled.
 * @param[out] boolean Returns true if the NAK-PLI message is enqueued
 * successfully on the tx socket.
 *---------------------------------------------------------------------------*/ 
#if HAVE_FCC
boolean
rtp_era_send_nakpli (rtp_era_recv_t *p_session,
                     rel_time_t rcc_min_fill,
                     rel_time_t rcc_max_fill,
                     uint32_t do_fast_fill,
                     uint32_t maximum_recv_bw,
                     rel_time_t max_fastfill) 
{
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t pli;
    uint32_t send_result = 0, data_len;
    rtcp_msg_info_t app;
    plii_tlv_t tlv;

    if (!p_session) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return (FALSE);
    }
    
    rtcp_init_pkt_info(&pkt_info);
    rtcp_init_msg_info(&pli);
    pli.psfb_info.fmt = RTCP_PSFB_PLI;
    pli.psfb_info.ssrc_media_sender = 0; /* we don't know this, yet */
    rtcp_set_pkt_info(&pkt_info, RTCP_PSFB, &pli);
    
    /*  Added for late instantiation */
    rtcp_init_msg_info(&app);
    memcpy(&app.app_info.name, "PLII", sizeof(uint32_t));
    /* Prepare TLV */ 
    memset(&tlv,0,sizeof(plii_tlv_t));
    tlv.min_rcc_fill = TIME_GET_R(msec, rcc_min_fill);
    tlv.max_rcc_fill = TIME_GET_R(msec, rcc_max_fill);
    tlv.do_fastfill = do_fast_fill;
    tlv.maximum_recv_bw = maximum_recv_bw;
    tlv.maximum_fastfill_time = TIME_GET_R(msec, max_fastfill);

    app.app_info.data = 
        (uint8_t *) plii_tlv_encode_allocate (tlv, &data_len);
    app.app_info.len = data_len;
    rtcp_set_pkt_info(&pkt_info, RTCP_APP, &app);
    
    send_result = MCALL((rtp_session_t *)p_session,
                        rtcp_send_report,
                        p_session->rtp_local_source,
                        &pkt_info,
                        TRUE,   /* re-schedule next RTCP report */
                        FALSE); /* not reset RTCP XR stats */
    /* destroy TLV */
    rcc_tlv_encode_destroy(app.app_info.data);

    return (send_result > 0);
}
#endif

/* 
 * Function:    rtp_era_recv_set_rtcp_handlers
 * Description: Sets the function pointers for RTCP handlers
 * @param[in] ssm_table  pointer to a ssm function ptr table which gets
 *                       populated by this function
 * @param[in] ptp_table  pointer to a ptp function ptr table which gets
 *                       populated by this function
 */
UT_STATIC
void rtp_era_recv_set_rtcp_handlers(rtcp_handlers_t *ssm_table,
                                    rtcp_handlers_t *ptp_table)
{
  /* 
   * Set rtcp handlers for ssm and ptp
   */
  rtp_ssm_rsi_rcvr_set_rtcp_handlers(ssm_table);
  rtp_ptp_set_rtcp_handlers(ptp_table);
}


/* Function:    rtp_sesssion_init_module
 * Description: One time (class level) initialization of the module.
 *              Initializes various fcn tables and allocates memory.
 *              This should be call one time at system initialization
 * @param[in] max_sessions the maximum number of repair sessions to 
 * be created. MUST BE AT LEAST 1.
 * @param[out[] TRUE(1) if successfully initialized this module
 * else returns FALSE(0).
 */
boolean rtp_era_recv_init_module (uint32_t max_sessions)
{    
    if (s_rtp_era_recv_initted) {
        return TRUE;
    }
    if (max_sessions == 0) {
        return FALSE;  /* check for valid number of sessions */
    }

    rtp_era_recv_set_methods(&rtp_era_recv_ssm_methods_table,
                             &rtp_era_recv_ptp_methods_table);
    rtp_era_recv_set_rtcp_handlers(&rtp_era_recv_ssm_rtcp_handler,
                                   &rtp_era_recv_ptp_rtcp_handler);

    if (rtp_era_recv_init_memory(&rtp_era_recv_memory, (int)max_sessions)) {
	s_rtp_era_recv_initted = 1;
	return TRUE;
    } else {
	syslog_print(VQEC_ERROR, "era session module initialization failed\n");
	return FALSE;
    }
}

/**---------------------------------------------------------------------------
 * Inject a RTCP packet into the primary RTCP socket.
 * 
 * @param[in] p_session Pointer to the primary session.
 * @param[in] remote_addr IP address to which the packet is sent.
 * @param[in] remote_port UDP port to which the packet is sent.
 * @param[in] buf Pointer to the packet contents.
 * @param[in] len Length of the contents.
 * @param[out] boolean Returns true if the packet was successfully enQd
 * in the socket.
 *---------------------------------------------------------------------------*/ 
boolean
rtp_era_send_to_rtcp_socket (rtp_era_recv_t *p_session,
                             in_addr_t remote_addr, 
                             in_port_t remote_port,
                             char *buf, 
                             uint16_t len) 
{
    struct sockaddr_in dest_addr;
    int32_t tx_len;

    if (!p_session || !buf || !len) {
        return (FALSE);
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = remote_addr;
    dest_addr.sin_port = remote_port;
        
    tx_len = sendto(p_session->era_rtcp_sock->fd,
                    buf, 
                    len,
                    0,
                    (struct sockaddr *)&dest_addr, 
                    sizeof(struct sockaddr_in));

    return (len == tx_len);
}

/**---------------------------------------------------------------------------
 * From a primary session to channel identifier of parent channel.
 * 
 * @param[in] p_primary Pointer to the primary session.
 * @param[out] vqec_chanid_t Parent channel's id.
 *---------------------------------------------------------------------------*/ 
vqec_chanid_t
rtp_era_session_chanid (rtp_era_recv_t *p_primary)
{
    VQEC_ASSERT(p_primary != NULL);
    VQEC_ASSERT(p_primary->chan != NULL);

    return (vqec_chan_to_chanid(p_primary->chan));
}

/*
 * Function:    rtp_era_recv_set_methods
 * Description: Sets the function pointers in the func ptr table
 * @param[in] ssm_table  pointer to a ssm function ptr table which gets 
 *                       populated by this function
 * @param[in] ptp_table  pointer to a ptp function ptr table
 */
void rtp_era_recv_set_methods(rtp_era_recv_methods_t * ssm_table,
                              rtp_era_recv_methods_t * ptp_table)
{
  /* First set the the ssm rsi base method functions */ 
  rtp_ssm_rsi_rcvr_set_methods((rtp_ssm_rsi_rcvr_methods_t *)ssm_table);

  /* Set the ptp base method functions */
  rtp_ptp_set_methods((rtp_ptp_methods_t *)ptp_table);

  /* Now override and/or populate derived class functions */
  ssm_table->rtcp_construct_report = rtcp_construct_report_era_recv;
  ptp_table->rtcp_construct_report = rtcp_construct_report_era_recv;

  /* statistics collection methods. */
  ssm_table->rtp_update_stats = rtp_era_update_stats;
  ssm_table->rtp_update_receiver_stats = rtp_era_update_receiver_stats;
  ptp_table->rtp_update_stats = rtp_era_update_stats;
  ptp_table->rtp_update_receiver_stats = rtp_era_update_receiver_stats;

  /* upcall event processing. */
  ssm_table->process_upcall_event = rtp_era_upcall_ev;
  ptp_table->process_upcall_event = rtp_era_upcall_ev;

  /* shutdown with capability to send byes. */
  ssm_table->shutdown_allow_byes = rtp_era_shutdown_allow_byes;
  ptp_table->shutdown_allow_byes = rtp_era_shutdown_allow_byes;

}

