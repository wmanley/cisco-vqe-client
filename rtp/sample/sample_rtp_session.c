/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * sample_rtp_session.c 
 * 
 * Copyright (c) 2006-2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#include "../../include/sys/event.h"
#include "../../include/utils/zone_mgr.h"
#include "../rtp_util.h"
#include "rtp_exp_api.h"
#include "../rtcp_stats_api.h"
#include "../rtp_database.h"
#include "../rcc_tlv.h"
#include "sample_rtp_application.h"
#include "sample_rtp_channel.h"
#include "sample_rtp_session.h"
#include "sample_rtp_dataplane.h"
#include "sample_rtp_utils.h"
#include "sample_rtcp_script.h"

static sample_config_t      rtp_sample_config;
static rtcp_memory_t        rtp_sample_memory;
static rtp_sample_methods_t rtp_sample_methods_table;
static rtcp_handlers_t      rtp_sample_rtcp_handler;

static rtp_sample_t       **rtp_sample_session = NULL;
static rtp_sample_t       **rtp_repair_session = NULL;

/*------------RTP functions specific to Sample session---------------*/

/*
 * Function:    rtp_sample_set_methods
 * Description: Sets the function pointers in the func ptr table
 * Parameters:  table pointer to a function ptr table which gets populated by 
 *              this funciton
 * Returns:     None
 */
static void 
rtp_sample_set_methods (rtp_sample_methods_t * table,
                        sample_config_t *config)
{
  /* First set the the base method functions */ 
  rtp_ptp_set_methods((rtp_ptp_methods_t *)table);

  /* Now override and/or populate derived class functions */
  if (config->sender) {
      table->rtp_update_sender_stats = rtp_update_sender_stats_sample;
  } else {
      table->rtp_update_receiver_stats = rtp_update_receiver_stats_sample;
  }
  if (config->extra_messages) {
      if (config->sender) {
          table->rtcp_construct_report = rtcp_construct_report_sender_sample;
      } else {
          if (config->rpr_port) {
              table->rtcp_construct_report = rtcp_construct_report_repair;
          } else {
              table->rtcp_construct_report = 
                  rtcp_construct_report_receiver_sample;
          }
      }
  }

  if (!config->sender && config->rpr_port) {
      table->rtcp_construct_report = rtcp_construct_report_repair;
  }
  if (config->rtcp_script) {
      table->rtcp_construct_report = rtcp_construct_report_script;
  }

  table->rtp_data_source_demoted = rtp_data_source_demoted_sample;

  table->rtcp_packet_sent = rtcp_packet_sent_sample;
  table->rtcp_packet_rcvd = rtcp_packet_rcvd_sample;
}


/* Function:    rtp_sample_set_rtcp_handler
 * Description: Sets the function pointers for RTCP handlers
 * Parameters:  None
 * Returns:     None
 */
static void rtp_sample_set_rtcp_handlers(rtcp_handlers_t *table)
{
    /* Set base rtp session handler first */
    rtp_session_set_rtcp_handlers(table);

    /* Now override or add new handlers */

}

/*
 * rtp_sample_init_memory
 *
 * Initializes RTCP memory used by the SAMPLE class.
 *
 * Parameters:
 * memory           -- ptr to memory info struct.
 */
static boolean
rtp_sample_init_memory (rtcp_memory_t *memory,
                        sample_config_t *config)
{
    int num_sessions = 0;

    if (config->sender) {
        num_sessions = RTP_SAMPLE_SESSIONS;
    } else if (config->rpr_port) {
        num_sessions = config->num_sessions * 2;
    } else {
        num_sessions = config->num_sessions;
    }

    /*
     * Allocate all members as channel members (essentially upstream 
     * members on a per channel basis)
     */
    rtcp_cfg_memory(memory,
                    "SAMPLE",
                    sizeof(rtp_sample_t), /* session size */
                    num_sessions,
                    0,
                    0,
                    sizeof(rtp_member_t),
                    num_sessions * RTP_SAMPLE_MAX_MEMBERS_PER_SESSION);
    return (rtcp_init_memory(memory));
}

/* Function:    rtp_sample_init_module
 * Description: One time (class level) initialization of various function tables
 *              This should be call one time at system initialization
 * Parameters:  None
 * Returns:     None
*/
boolean rtp_sample_init_module (sample_config_t *config)
{    
    rtp_sample_config = *config;

    rtp_sample_session = malloc(sizeof(rtp_sample_t *) * config->num_sessions);
    if (rtp_sample_session) {
        memset(rtp_sample_session, 0, 
               sizeof(rtp_sample_t *) * config->num_sessions);
    } else {
        return (FALSE);
    }

    if (config->rpr_port) {
        rtp_repair_session = 
            malloc(sizeof(rtp_sample_t *) * config->num_sessions);
        if (rtp_repair_session) {
            memset(rtp_repair_session, 0, 
                   sizeof(rtp_sample_t *) * config->num_sessions);
        } else {
            return (FALSE);
        }
    }

    if (!rtp_sample_init_memory(&rtp_sample_memory, config)) {
        free (rtp_sample_session);
        return (FALSE);
    }        

    rtp_sample_set_methods(&rtp_sample_methods_table, config);
    rtp_sample_set_rtcp_handlers(&rtp_sample_rtcp_handler);

    return (TRUE);
}

/*
 * gap_timeout_cb
 *
 * Callback invoked by libevent when it's time to check for gaps
 * and request repairs.
 *
 * Parameters:
 * fd          -- recv socket file description
 * event       -- event code: should be EV_TIMEOUT
 * arg         -- context ptr: should be to a sample_event_t struct
 */
 
static void gap_timeout_cb(int fd, short event, void *arg)
{
    sample_event_t *timeout = arg;
    rtp_sample_t *sess = (rtp_sample_t *)(timeout->p_session);
    rtcp_pkt_info_t new_pkt_info;
    rtcp_msg_info_t rtpfb;
    rtcp_rtpfb_generic_nack_t nack[DROP_BUFSIZ];
    int num_nacks = 0;
    int num_missing = 0;
    uint16_t seqno;
    uint16_t base_seqno;

    memset(nack, 0, sizeof(nack));

    while (sample_getnext_drop(sess->channel->pri_rcvr,
                               &sess->next_read_idx,
                               &seqno)) {
        if (num_nacks) {
            base_seqno = nack[num_nacks - 1].pid;
        }
        if (num_nacks && seqno > base_seqno && seqno <= base_seqno + 16) {
            nack[num_nacks - 1].bitmask |= (1 << (seqno - base_seqno - 1));
            num_missing++;
        } else if (num_nacks < DROP_BUFSIZ) {
            num_nacks++;
            nack[num_nacks - 1].pid = seqno;
            nack[num_nacks - 1].bitmask = 0;
            num_missing++;
        } else {
            break;
        }
    }
    if (num_nacks) {
        rtcp_init_pkt_info(&new_pkt_info);
        rtcp_init_msg_info(&rtpfb);

        rtcp_set_rtcp_rsize(&new_pkt_info, rtp_sample_config.rtcp_rsize);
        rtpfb.rtpfb_info.fmt = RTCP_RTPFB_GENERIC_NACK;
        rtpfb.rtpfb_info.ssrc_media_sender = 0;
        rtpfb.rtpfb_info.num_nacks = num_nacks;
        rtpfb.rtpfb_info.nack_data = nack;
        rtcp_set_pkt_info(&new_pkt_info, RTCP_RTPFB, &rtpfb);

        MCALL((rtp_session_t *)sess,
              rtcp_send_report, 
              sess->rtp_local_source, 
              &new_pkt_info,
              FALSE,  /* not re-schedule next RTCP report */
              FALSE); /* not reset RTCP XR stats */
    }

    /* restart the timer */
    struct timeval tv = TIME_GET_R(timeval, TIME_MK_R(msec, 100));
    (void)event_add(&timeout->ev, &tv);
}

/*
 * rcc_timeout_cb
 *
 * Callback invoked by libevent when it's time to "change the channel".
 *
 * Parameters:
 * fd          -- recv socket file description
 * event       -- event code: should be EV_TIMEOUT
 * arg         -- context ptr: should be to a sample_event_t struct
 */
 
static void rcc_timeout_cb(int fd, short event, void *arg)
{
    sample_event_t *timeout = arg;
    rtp_sample_t *sess = (rtp_sample_t *)(timeout->p_session);
    rtp_sample_t *new_sess = NULL;
    rtcp_pkt_info_t new_pkt_info;
    rtcp_msg_info_t psfb;
    rtcp_msg_info_t app;
    uint32_t data_len;
    plii_tlv_t tlv;
    int curr_session_number;
    sample_channel_t *curr_channel;
    sample_channel_t *new_channel;

    curr_session_number = sess->session_number;
    curr_channel = sess->channel;

    /* delete_session will send BYEs for the repair and primary sessions */
    (void)delete_session(curr_session_number);

    new_channel = get_other_channel(curr_channel->channel_number);
    if (!new_channel) {
        new_channel = curr_channel;
    }
    new_sess = rtp_sample_session[curr_session_number] =
        create_session(curr_session_number, TRUE, 
                       &rtp_sample_config, new_channel);
    if (rtp_repair_session) {
        rtp_repair_session[curr_session_number] = 
            create_session(curr_session_number, FALSE, 
                           &rtp_sample_config, new_channel);
    }

    rtcp_init_pkt_info(&new_pkt_info);
    rtcp_init_msg_info(&psfb);

    psfb.psfb_info.fmt = RTCP_PSFB_PLI;
    psfb.rtpfb_info.ssrc_media_sender = 0;
    rtcp_set_pkt_info(&new_pkt_info, RTCP_PSFB, &psfb);

    /*  Added for late instantiation */
    rtcp_init_msg_info(&app);
    memcpy(&app.app_info.name, "PLII", sizeof(uint32_t));

    /* Prepare TLV */
    memset(&tlv, 0, sizeof(plii_tlv_t));
    tlv.min_rcc_fill = rtp_sample_config.min_rcc_fill;
    tlv.max_rcc_fill = rtp_sample_config.max_rcc_fill;
    tlv.do_fastfill = rtp_sample_config.do_fastfill;
    tlv.maximum_fastfill_time = rtp_sample_config.maximum_fastfill_time;
    tlv.maximum_recv_bw = rtp_sample_config.maximum_recv_bw;

    app.app_info.data =
        (uint8_t *) plii_tlv_encode_allocate (tlv, &data_len);
    app.app_info.len = data_len;
    rtcp_set_pkt_info(&new_pkt_info, RTCP_APP, &app);

    if (MCALL((rtp_session_t *)new_sess,
              rtcp_send_report, 
              new_sess->rtp_local_source, 
              &new_pkt_info,
              FALSE, /* not re-schedule next RTCP report */
              FALSE)) /* not reset RTCP XR stats */ { 
        new_sess->channel->psfbs_sent++;
    }

    /* destroy TLV */
    rcc_tlv_encode_destroy(app.app_info.data);

    /* 
     * we don't need to restart the rcc timer,
     * because the create_session code has started the timer,
     * for the new primary session
     */
}

/*
 * rtcp_report_timeout_cb
 *
 * Callback invoked by libevent when the RTCP interval has expired,
 * and it's time to send an RTCP report.
 *
 * Parameters:
 * fd          -- recv socket file description
 * event       -- event code: should be EV_TIMEOUT
 * arg         -- context ptr: should be to a sample_event_t struct
 */
 
static void rtcp_report_timeout_cb(int fd, short event, void *arg)
{
      sample_event_t *timeout = arg;
      rtp_member_t *local_member = timeout->p_session->rtp_local_source;
      uint64_t time = TIME_GET_A(msec, get_sys_time());

      /* Update the timestamps */
      local_member->rtp_timestamp = (uint32_t)time;
      local_member->ntp_timestamp = get_ntp_time();

      MCALL(timeout->p_session, rtp_session_timeout_transmit_report);

      abs_time_t now = get_sys_time();
      rel_time_t diff = TIME_SUB_A_A(timeout->p_session->next_send_ts, now);
      struct timeval tval = TIME_GET_R(timeval, diff);

      (void)event_add(&timeout->ev, &tval);
}

/*
 * member_timeout_cb
 *
 * Callback invoked by libevent when the "member timeout" expires,
 * and it's time to a) check for idle senders, and demote those 
 * that are inactive (to receivers), and b) check for idle members,
 * and delete those that are inactive.
 *
 * Parameters:
 * fd          -- recv socket file description
 * event       -- event code: should be EV_TIMEOUT
 * arg         -- context ptr: should be to a sample_event_t struct
 */
 
static void member_timeout_cb (int fd, short event, void *arg) {
    struct timeval tv;
    sample_event_t *ev = arg;
    uint32_t interval;

    /* check for idle senders */
    MCALL(ev->p_session, rtp_session_timeout_slist);
    /* check for idle members */
    MCALL(ev->p_session, rtp_session_timeout_glist);

    /* restart the timer */
    timerclear(&tv);
    interval = MCALL(ev->p_session, rtcp_report_interval,
                     ev->p_session->we_sent,
                     TRUE /* jitter the interval */);
    tv = TIME_GET_R(timeval,TIME_MK_R(usec, (int64_t)interval)) ;

    (void)event_add(&ev->ev, &tv);
}

/*
 * rtcp_report_notify
 *
 * Print some receiver statistics
 */

void rtcp_report_notify (rtp_session_t *p_sess)
{
    rtp_sample_t *sess = (rtp_sample_t *)p_sess;
    sample_receiver_t *receiver = sess->primary ? 
        sess->channel->pri_rcvr : sess->channel->rpr_rcvr;
    rtcp_sender_info_t *sender_info = NULL;
    rtp_member_t *p_source;
    int num_senders = 0;
    char buf[INET_ADDRSTRLEN];
#define TIMEBUFLEN 80
#define TIMEIDX    11
    char timebuf[TIMEBUFLEN];
    const char *p_timebuf = NULL;
    int timeidx = 0;
    char *description = NULL;
    int i, j;

    if (rtp_sample_config.sender) {
        return;
    }

    p_timebuf = abs_time_to_str(get_sys_time(), timebuf, sizeof(timebuf));
    if (p_timebuf && (strlen(p_timebuf) > TIMEIDX)) {
        timeidx = TIMEIDX;
    } 

    switch (receiver->type) {
    case SAMPLE_EXTERNAL_RECEIVER:
    case SAMPLE_INTERNAL_RECEIVER:
    case SAMPLE_TRANS_RECEIVER:
        description = "pri";
        break;
    default:
        description = "rpr";
        break;
    }

    rtcp_get_sender_info(p_sess, 
                         1,
                         &num_senders,
                         NULL);
    if (num_senders) {
        sender_info = malloc(num_senders * sizeof(rtcp_sender_info_t));
        if (!sender_info) {
            goto done;
        }
        j = num_senders;
        rtcp_get_sender_info(p_sess, 
                             j,
                             &num_senders,
                             sender_info);
        for (i = 0; i < j; i++) {
            p_source = rtp_sending_member_with_ssrc(p_sess, sender_info[i].ssrc);
            if (p_source) {
                rtp_update_receiver_stats_sample(p_sess, p_source, FALSE);
            }
        }
        rtcp_get_sender_info(p_sess, 
                             j,
                             &num_senders,
                             sender_info);
        for (i = 0; i < j; i++) {
            fprintf(stderr,
                    "%s [%s:%u ssrc 0x%08x] %s pkts: %u cum_loss: %d\n",
                    &timebuf[timeidx],
                    uint32_ntoa_r(sender_info[i].rtp_src_ip_addr, 
                                  buf, sizeof(buf)),
                    ntohs(sender_info[i].rtp_src_udp_port),
                    sender_info[i].ssrc,
                    description,
                    sender_info[i].received,
                    sender_info[i].cum_loss);
        }
        free(sender_info);
    } else {
        fprintf(stderr, 
                "%s No active RTP %s source\n",
                &timebuf[timeidx],
                description);
    }

 done:
    return;
}


/* Function:    create_session
 * Description: Create the session structure for an RTP/RTCP session.
 * Parameters:  session_number: from 0 to N-1, 
 *                              where N is the number of sessions.
 *                              0 indicates the master session, 
 *                              which MUST be created first.
 *              primary: TRUE if a primary session, 
 *                       FALSE for a repair session.
 *              config: RTP session config parameters
 * Returns:     pointer to session created, or NULL.
*/
rtp_sample_t *create_session (int session_number,
                              boolean primary,
                              sample_config_t *config,
                              sample_channel_t *channel)
{
    rtp_sample_t *sample_session = NULL;
    boolean f_init_successful = FALSE;
    struct timeval tv;
    rtp_member_t *p_local_source = NULL;
    rtp_member_id_t member_id;
    rtp_config_t rtp_config;
    in_addr_t send_addr = INADDR_NONE;
    in_addr_t send_port = 0;
#define CNAMEBUFLEN 80
    char cname_str[CNAMEBUFLEN];
    abs_time_t now;
    rel_time_t diff;

    if (!config || !channel) {
        return (NULL);
    }

    /*
     * allocate session memory;
     * set pointer to per-class memory, for allocation of objects
     * (e.g., members) for this session.
     */
    sample_session = rtcp_new_session(&rtp_sample_memory);
    if (!sample_session) {
        return (NULL);
    } 
    memset(sample_session, 0, sizeof(rtp_sample_t));
    sample_session->rtcp_mem = &rtp_sample_memory;
    sample_session->session_number = session_number;
    sample_session->primary = primary;
    sample_session->channel = channel;

    /* 
     * set up sockets for sending and receiving RTCP 
     */

    if (config->sender) {
        /* we don't support repair sessions for the sender simulation */
        send_addr = channel->pri_rtcp_addr;
        send_port = channel->pri_rtcp_port;
    } else { /* we're a receiver */
        if (primary) {
            send_addr = channel->fbt_addr;
            send_port = channel->fbt_port;
        } else {
            send_addr = channel->fbt_addr;
            send_port = channel->rpr_port;
        }
    }

    /*
     * First perform all the base session checks and initializations.
     * This also will set up the base session methods.
     */

    rtp_init_config(&rtp_config, RTPAPP_DEFAULT);
    rtp_config.session_id = rtp_taddr_to_session_id(config->rtp_addr,
                                                    config->rtp_port);
    rtp_config.senders_cached = 1;
    rtp_config.rtp_sock = -1;
    rtp_config.rtcp_sock = primary ? 
        sample_session->channel->pri_send_socket : 
        sample_session->channel->rpr_send_socket;
    rtp_config.rtcp_dst_addr = send_addr;
    rtp_config.rtcp_dst_port = send_port;
    rtp_config.rtcp_bw_cfg.per_rcvr_bw = config->per_rcvr_bw;
    rtp_config.rtcp_bw_cfg.per_sndr_bw = config->per_rcvr_bw;
    rtp_config.rtcp_rsize = rtp_sample_config.rtcp_rsize;
 
    if (config->rtcp_xr_enabled) {
        rtp_config.rtcp_xr_cfg.max_loss_rle = 1000;
        rtp_config.rtcp_xr_cfg.max_post_rpr_loss_rle = 400;
        rtp_config.rtcp_xr_cfg.ss_loss = TRUE;
        rtp_config.rtcp_xr_cfg.ss_dup = TRUE;
        rtp_config.rtcp_xr_cfg.ss_jitt = TRUE;
        rtp_config.rtcp_xr_cfg.ma_xr = TRUE;
        rtp_config.rtcp_xr_cfg.xr_dc = TRUE;
    }

    f_init_successful = rtp_create_session_ptp(&rtp_config,
                                               (rtp_ptp_t **)(&sample_session),
                                               FALSE);
    
     if (f_init_successful == FALSE) {
         goto fail;
     }     

     /* Set the methods table */
     sample_session->__func_table = &rtp_sample_methods_table;

     /* Set RTCP handlers */
     sample_session->rtcp_handler = &rtp_sample_rtcp_handler;

     /* create local member */
     member_id.type = config->sender ?
         RTP_SMEMBER_ID_RTCP_DATA : RTP_RMEMBER_ID_RTCP_DATA;
     /* 
      * in the "primary" case, the actual SSRC value will be randomly selected
      * and written here by the rtp_create_local_source method
      */     
     member_id.ssrc = primary ? 
         0 : rtp_sample_session[session_number]->rtp_local_source->ssrc;
     member_id.subtype = RTCP_CHANNEL_MEMBER;
     member_id.src_addr = sample_session->send_addrs.src_addr;
     member_id.src_port = sample_session->send_addrs.src_port;
     snprintf(cname_str, sizeof(cname_str), "%s member %d", 
             config->sender ? "Sending" : "Receiving", 
             session_number);
     member_id.cname = cname_str;
     p_local_source = MCALL((rtp_session_t *)sample_session, 
                            rtp_create_local_source, 
                            &member_id,
                            primary ? 
                            RTP_SELECT_RANDOM_SSRC : RTP_USE_SPECIFIED_SSRC);
     if (!p_local_source) {
         RTP_LOG_DEBUG_F((rtp_session_t *)sample_session, NULL,
                         "Failed to create local member\n");
         goto fail;
     }
  
     if (primary && config->drop_count) {
         /* 
          * if we're doing drop simulation,
          * primary sessions look for gaps and request repairs 
          */
         sample_event_t  *gap_timeout = &(sample_session->gap_timeout);
         gap_timeout->p_session = (rtp_session_t *)sample_session;
         evtimer_set(&gap_timeout->ev, gap_timeout_cb, gap_timeout);
         /* 
          * by default, initial timeout is jittered between 
          * 100 and 200 ms, to spread out the processing; 
          * subsequent timeouts will be at 100 msec intervals 
          */
         int usec = config->drop_jitter ? 
             (int)(100000 * (1.0 + (rand() / (RAND_MAX + 1.0)))) : 100000;
         timerclear(&tv);
         tv = TIME_GET_R(timeval, TIME_MK_R(usec, usec));
         (void)event_add(&gap_timeout->ev, &tv);
     }

     if (primary && config->min_chg_secs) {
         /* 
          * if we're doing RCC simulation,
          * primary sessions send PLI NACKs periodically
          */
         sample_event_t  *rcc_timeout = &(sample_session->rcc_timeout);
         rcc_timeout->p_session = (rtp_session_t *)sample_session;
         evtimer_set(&rcc_timeout->ev, rcc_timeout_cb, rcc_timeout);

         /* 
          * generate a random next change time,
          * between min_chg_secs and max_chg_secs 
          */
         int chg_time = (config->min_chg_secs * 1000) + 
             (int)((double)((config->max_chg_secs - 
                             config->min_chg_secs) * 1000)
                   * (rand() / (RAND_MAX + 1.0)));

         /* start the timer */
         timerclear(&tv);
         tv = TIME_GET_R(timeval, TIME_MK_R(msec, chg_time));
         (void)event_add(&rcc_timeout->ev, &tv);
     }

     /* Initalize rtcp report send event */
     sample_event_t  *rtcp_timeout = &(sample_session->report_timeout);
     rtcp_timeout->p_session = (rtp_session_t *)sample_session;
     evtimer_set(&rtcp_timeout->ev, rtcp_report_timeout_cb, rtcp_timeout);
     now = get_sys_time();
     diff = TIME_SUB_A_A(sample_session->next_send_ts, now);
     timerclear(&tv);
     tv = TIME_GET_R(timeval, diff);
     (void)event_add(&rtcp_timeout->ev, &tv);

     /* Initalize rtcp member timeout */
     sample_event_t *member_timeout = &(sample_session->member_timeout);
     member_timeout->p_session = (rtp_session_t *)(sample_session);
     evtimer_set(&member_timeout->ev, member_timeout_cb, member_timeout);    
     timerclear(&tv);
     uint32_t interval = MCALL(member_timeout->p_session,
                               rtcp_report_interval,
                               FALSE, TRUE /* jitter the interval */);
     tv = TIME_GET_R(timeval,TIME_MK_R(usec, (int64_t)interval)) ;
     (void)event_add(&member_timeout->ev, &tv);

     /* put the session on the appropriate per-channel list */
     if (primary) {
         VQE_LIST_INSERT_HEAD(&channel->pri_sessions, sample_session, link);
     } else {
         VQE_LIST_INSERT_HEAD(&channel->rpr_sessions, sample_session, link);
     }

     return (sample_session);

 fail:
     rtcp_delete_session(&rtp_sample_memory, sample_session);
     return (NULL);
}

/* Function:    rtp_create_session_sample
 * Description: Create the session structures for "SAMPLE" RTP/RTCP sessions.
 * Parameters:  config: ptr to RTP session config parameters
 *              num_sessions: on output, number of sessions created.
 * Returns:     pointer to array of session ptrs.
*/
rtp_sample_t **rtp_create_session_sample (sample_config_t *config,
                                          sample_channel_t *channel,
                                          boolean primary,
                                          int *num_sessions)
{
    int i;
    int num_created = 0;
    rtp_sample_t **sample_array = NULL;

    sample_array = primary ? rtp_sample_session : rtp_repair_session;

    /* create the master session */
    sample_array[0] = create_session(0, primary, config, channel);
    if (sample_array[0]) {
        num_created++;
    } else {
        return (NULL);
    }        

    /* create slave sessions */
    for (i = 1; i < config->num_sessions ; i++) {
        sample_array[i] = create_session(i, primary, config, channel);
        if (sample_array[i]) {
            num_created++;
        }
        if (num_created % 50 == 0 && config->verbose) {
            printf("Created %d sessions...\n", num_created);
        }
    }

    *num_sessions = num_created;
    return (sample_array);
}

static inline 
void sample_event_del (struct event *ev)
{
    if (event_initialized(ev)) {
        (void)event_del(ev);
    }
}

/*
 * delete_session
 *
 * Delete a single primary session,
 * and its corresponding repair session, if any.
 */
int delete_session (int session_number) 
{
    int i = session_number;
    int j;
    int byes = 0;
    uint32_t send_len;
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t bye;
    rtcp_msg_info_t xr;
#define SESSIONS_PER_CHANNEL 2   /* primary and repair */
    rtp_sample_t *session[SESSIONS_PER_CHANNEL];
    rtp_sample_t *sess = NULL;

    rtcp_init_pkt_info(&pkt_info);
    rtcp_init_msg_info(&bye);
    rtcp_set_pkt_info(&pkt_info, RTCP_BYE, &bye);

    session[0] = rtp_repair_session ? rtp_repair_session[i] : NULL;
    session[1] = rtp_sample_session[i];
    for (j = 0; j < SESSIONS_PER_CHANNEL ; j++) {
        sess = session[j];
        if (sess) {
            if (j == 1) {
                if (is_rtcp_xr_enabled(&sess->rtcp_xr_cfg)) {
                    rtcp_init_msg_info(&xr);
                    rtcp_set_pkt_info(&pkt_info, RTCP_XR, &xr);
                }
            }
            send_len = MCALL((rtp_session_t *)(sess), 
                             rtcp_send_report, 
                             sess->rtp_local_source, 
                             &pkt_info,
                             TRUE,  /* re-schedule next RTCP report */
                             TRUE); /* reset RTCP XR stats */
            if (send_len) {
                byes++;
            }
            rtp_delete_session_ptp ((rtp_ptp_t **)(&sess), FALSE);
            sample_event_del(&(sess->gap_timeout.ev));
            sample_event_del(&(sess->rcc_timeout.ev));
            sample_event_del(&(sess->report_timeout.ev));
            sample_event_del(&(sess->member_timeout.ev));
        }
    }
    for (j = 0; j < SESSIONS_PER_CHANNEL ; j++) {
        sess = session[j];
        if (sess) {
            VQE_LIST_REMOVE(sess, link);
            rtcp_delete_session(&rtp_sample_memory, sess);
        }
    }
    rtp_sample_session[i] = NULL;
    if (rtp_repair_session) {
        rtp_repair_session[i] = NULL;
    }
    return (byes);
}


/*
 * Function:    rtp_delete_session_sample
 * Description: Frees a session.  The session will be cleaned first using
 *              rtp_cleanup_session.
 * Parameters:  pp_sample_sess  ptr to array of session pointers
 *              num_sessions    number of sessions
 * Returns:     no. of BYEs successfully sent
*/
int rtp_delete_session_sample (rtp_sample_t **pp_sample_sess,
                               rtp_sample_t **pp_repair_sess,
                               int num_sessions)
{
    int i;
    int byes = 0;

    for (i = 0; i < num_sessions; i++) {
        byes += delete_session(i);
    }
    return (byes);
}

/*
 * Function:    rtp_update_sender_stats_sample
 * Description: Sample function to get sender statistics from data plane
 * Parameters:  p_sess pointer to session object
 *              p_source pointer to local source member
 *              
 */
void rtp_update_sender_stats_sample(rtp_session_t *p_sess, 
                                    rtp_member_t *p_source)
{
    sample_sender_t *sender = NULL;
    boolean updated = FALSE;

    if (p_sess->rtp_local_source == p_source) {
        /*
         * source is local: if we're a sender, update sender stats
         */
        sender = get_sample_sender();
        if (sender) {
            p_source->packets_prior = p_source->packets;
            p_source->packets = sender->packets;
            p_source->bytes_prior = p_source->bytes;
            p_source->bytes = sender->bytes;

            p_source->rtp_timestamp = sender->rtp_timestamp;
            p_source->ntp_timestamp = abs_time_to_ntp(sender->send_time);
            updated = TRUE;
        }
    } 

    RTP_LOG_DEBUG_F(p_sess, p_source,
                    "%s sender stats\n",
                    updated ? "Updated" : "Failed to update");
}


/*
 * Function:    rtp_update_receiver_stats_sample
 * Description: Utility function to get sender statistics from data plane
 * Parameters:  p_sess pointer to session object
 *              p_source pointer to local source member
 *              reset_xr_stats flag for whether to reset RTCP XR stats
 *              
 */
void rtp_update_receiver_stats_sample(rtp_session_t *p_sess, 
                                      rtp_member_t *p_source,
                                      boolean reset_xr_stats)
{
    rtp_sample_t *sess = NULL;
    sample_receiver_t *receiver = NULL;
    sample_source_t *source = NULL;
    rtp_source_id_t src_id;
    boolean updated = FALSE;
    char str[INET_ADDRSTRLEN];
    uint32_t eseq;
    
    if  (!p_sess || !p_source) {
        return;
    }

    /* 
     * source is not local: if we're a receiver, and have seen
     * something from the source, update stats for that source 
     */
    sess = (rtp_sample_t *)p_sess;
    receiver = sess->primary ? 
        sess->channel->pri_rcvr : sess->channel->rpr_rcvr;
    if (receiver) {
        src_id.ssrc = p_source->ssrc;
        src_id.src_addr = p_source->rtp_src_addr;
        src_id.src_port = p_source->rtp_src_port;
        source = get_sample_source(receiver, &src_id);
    }
    if (source) {
        p_source->rcv_stats = source->stats;
        p_sess->rtp_stats = receiver->stats;
        if (p_source->xr_stats) {
            *(p_source->xr_stats) = source->xr_stats;
        }
        else {
            p_source->xr_stats = &((rtp_sample_t *)p_sess)->xr_stats;
            p_source->post_er_stats = &((rtp_sample_t *)p_sess)->post_er_stats;
        }
        //rtp_update_prior(&source->stats);
        updated = TRUE;

        if (reset_xr_stats) {
            eseq = source->stats.cycles + source->stats.max_seq + 1;
            rtcp_xr_init_seq(&source->xr_stats, eseq, FALSE);

            RTP_LOG_DEBUG_F(p_sess, p_source,
                            "Re-init RTCP XR stats for SSRC 0x%08x at seq %d\n",
                            p_source->ssrc, eseq);
        }
    }

    RTP_LOG_DEBUG_F(p_sess, p_source,
                    "%s receiver stats for SSRC 0x%08x RTP %s:%u\n",
                    updated ? "Updated" : "Failed to update",
                    p_source->ssrc, 
                    uint32_ntoa_r(p_source->rtp_src_addr, str, sizeof(str)), 
                    ntohs(p_source->rtp_src_port));
}

/*
 * rtcp_construct_report_sender_sample
 *
 * Illustrate how a sender might add non-default content
 * to RTCP Sender Reports, through this override of the 
 * rtcp_construct_report method.
 */
uint32_t rtcp_construct_report_sender_sample (rtp_session_t   *p_sess,
                                              rtp_member_t    *p_source, 
                                              rtcptype        *p_rtcp,
                                              uint32_t         bufflen,
                                              rtcp_pkt_info_t *p_pkt_info,
                                              boolean          reset_xr_stats)
{
    rtcp_pkt_info_t new_pkt_info;
    rtcp_msg_info_t rsi;
    rtcp_msg_info_t app;
    uint32_t len = 0;
    char *s = "Sender Data";
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
     * Set up to add an RSI message (with an RTCP Bandwidth Indication
     * subreport) to the pre-specified content of the compound packet.
     */
    rtcp_init_msg_info(&rsi);
    rtcp_set_rsi_subrpt_mask(&rsi.rsi_info.subrpt_mask, RTCP_RSI_BISB);
    rtcp_set_pkt_info(&new_pkt_info, RTCP_RSI, &rsi);

    /* 
     * Add an APP message -- typically this wouldn't be done here,
     * for every report, but rather in an "unscheduled" RTCP send
     */

    rtcp_init_msg_info(&app);
    memcpy(&app.app_info.name, "SNDR", sizeof(uint32_t));
    app.app_info.len = strlen(s);
    app.app_info.data = (uint8_t *)(s);
    rtcp_set_pkt_info(&new_pkt_info, RTCP_APP, &app);
       
    /* Call the base class version */
    len = rtcp_construct_report_base(p_sess, p_source, 
                                     p_rtcp, bufflen,
                                     &new_pkt_info,
                                     FALSE);
    return(len);
}

/*
 * rtcp_construct_report_receiver_sample
 *
 * Illustrate how a sender might add non-default content
 * to RTCP Receiver Reports, through this override of the 
 * rtcp_construct_report method.
 */
uint32_t rtcp_construct_report_receiver_sample (rtp_session_t   *p_sess,
                                                rtp_member_t    *p_source, 
                                                rtcptype        *p_rtcp,
                                                uint32_t         bufflen,
                                                rtcp_pkt_info_t *p_pkt_info,
                                                boolean         reset_xr_stats)
{
    rtcp_pkt_info_t new_pkt_info;
    rtcp_msg_info_t rsi;
    rtcp_msg_info_t pubports;
    rtcp_msg_info_t rtpfb;
    rtcp_msg_info_t psfb;
    uint32_t len = 0;

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
    rtcp_init_msg_info(&rsi);
    rtcp_set_rsi_subrpt_mask(&rsi.rsi_info.subrpt_mask, RTCP_RSI_GAPSB);
    rtcp_set_pkt_info(&new_pkt_info, RTCP_RSI, &rsi);

    /* Add a PUBPORTS message */
    rtcp_init_msg_info(&pubports);
    pubports.pubports_info.ssrc_media_sender = 0x01020304;
    pubports.pubports_info.rtp_port = 24;
    pubports.pubports_info.rtcp_port = 25;
    rtcp_set_pkt_info(&new_pkt_info, RTCP_PUBPORTS, &pubports);
    
    /* 
     * Add RTPFB and PSFB messages -- an artificial example,
     * as only one of these would be sent, and it would be
     * sent in an "unscheduled" send, via the rtcp_pkt_info_t
     * arg to the rtcp_send_report method 
     */

    rtcp_init_msg_info(&rtpfb);
    rtpfb.rtpfb_info.fmt = RTCP_RTPFB_GENERIC_NACK;
    rtpfb.rtpfb_info.ssrc_media_sender = 0x01020304;
    rtpfb.rtpfb_info.num_nacks = 1;
    rtcp_rtpfb_generic_nack_t nack;
    nack.pid = 0x1234;
    nack.bitmask = 0x4321;
    rtpfb.rtpfb_info.nack_data = &nack;
    rtcp_set_pkt_info(&new_pkt_info, RTCP_RTPFB, &rtpfb);
    
    /* Set the packet up for Reduced size RTCP */
    rtcp_set_rtcp_rsize(&new_pkt_info, rtp_sample_config.rtcp_rsize);

    rtcp_init_msg_info(&psfb);
    psfb.psfb_info.fmt = RTCP_PSFB_PLI;
    psfb.psfb_info.ssrc_media_sender = 0x01020304;
    rtcp_set_pkt_info(&new_pkt_info, RTCP_PSFB, &psfb);

    /* Call the base class version */
    len = rtcp_construct_report_base(p_sess, p_source, 
                                     p_rtcp, bufflen,
                                     &new_pkt_info,
                                     reset_xr_stats);
    return(len);
}

/*
 * rtcp_construct_report_repair
 *
 * Override the usual rtcp_construct_report method
 * when doing repair simulation.
 */
uint32_t rtcp_construct_report_repair (rtp_session_t   *p_sess,
                                       rtp_member_t    *p_source, 
                                       rtcptype        *p_rtcp,
                                       uint32_t         bufflen,
                                       rtcp_pkt_info_t *p_pkt_info,
                                       boolean         reset_xr_stats)
{
    rtcp_pkt_info_t new_pkt_info;
    rtcp_msg_info_t pubports;
    uint32_t len = 0;
    rtp_sample_t *sess = (rtp_sample_t *)p_sess;
    rtp_sample_t *rpr_sess = rtp_repair_session[sess->session_number];
    rtp_sample_t *first_rpr_sess = 
        VQE_LIST_FIRST(&rpr_sess->channel->rpr_sessions);

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

    /* Add a PUBPORTS message */
    rtcp_init_msg_info(&pubports);
    pubports.pubports_info.ssrc_media_sender = 0;
    if (rpr_sess == first_rpr_sess) {
        pubports.pubports_info.rtp_port = 
            ntohs(rpr_sess->channel->rpr_rcvr->recv_port);
        pubports.pubports_info.rtcp_port = 
            ntohs(rpr_sess->channel->rpr_rtcp_port);
    } else {
        pubports.pubports_info.rtp_port = 
            ntohs(rpr_sess->channel->morerpr_rcvr->recv_port);
        pubports.pubports_info.rtcp_port = 
            ntohs(rpr_sess->channel->morerpr_rtcp_port);
    }
    rtcp_set_pkt_info(&new_pkt_info, RTCP_PUBPORTS, &pubports);
    
    /* Call the base class version */
    len = rtcp_construct_report_base(p_sess, p_source, 
                                     p_rtcp, bufflen,
                                     &new_pkt_info,
                                     reset_xr_stats);
    return(len);
}

/*
 * rtcp_construct_report_script
 *
 * Override the usual rtcp_construct_report method,
 * in order to do script-based modification of 
 * outgoing RTCP messages.
 */
uint32_t rtcp_construct_report_script (rtp_session_t   *p_sess,
                                       rtp_member_t    *p_source, 
                                       rtcptype        *p_rtcp,
                                       uint32_t         bufflen,
                                       rtcp_pkt_info_t *p_pkt_info,
                                       boolean         reset_xr_stats)
{
    uint32_t orig_len = 0;
    uint32_t new_len = 0;
#define SCRIPTCMDLEN 1024
    char cmdbuf[SCRIPTCMDLEN];
    rtcp_script_cmd_t cmd;

    orig_len = rtcp_construct_report_base(p_sess, 
                                          p_source,
                                          p_rtcp,
                                          bufflen,
                                          p_pkt_info,
                                          reset_xr_stats);
    if (next_rtcp_script_cmd(rtp_sample_config.rtcp_script, 
                             TRUE, /* wrap when eof is reached */
                             cmdbuf, 
                             sizeof(cmdbuf)) &&
        parse_rtcp_script_cmd(cmdbuf, 
                              &cmd)) {
        new_len = process_rtcp_script_cmd((uint8_t *)p_rtcp, orig_len, &cmd);
    }
    return (new_len);
}

/*
 * rtcp_packet_rcvd_sample
 *
 * Sample post-processing of received RTCP packet.
 */

void rtcp_packet_rcvd_sample (rtp_session_t *p_sess,                   
                              boolean from_recv_only_member,
                              rtp_envelope_t *orig_addrs,
                              ntp64_t orig_send_time,
                              uint8_t *p_buf,
                              uint16_t len)
{
    exp_hdr_t exp_hdr;
    rtp_sample_t *sess = (rtp_sample_t *)p_sess;

    /*
     * As an example, here's how to export a compound packet:
     *
     * 1. initialize the EXP header via the exp_report_init() function:
     *    - the values of the subtype, chan_addr, chan_port, 
     *      sndr_role, and rcvr_role arguments must be determined 
     *      by the application-level class.
     *    - the orig_send_time and orig_addrs arguments should be the same
     *      as supplied in the orig_send_time and orig_addrs arguments
     *      to this function.
     * 2. call exp_export() to export the packet.
     */

    if (rtp_sample_config.export) {
        exp_report_init(&exp_hdr,
                        sess->primary ?
                        EXP_ORIGINAL : EXP_REXMIT,  /* report subtype */
                        rtp_sample_config.rtp_addr, /* channel address */
                        rtp_sample_config.rtp_port, /* channel port */
                        rtp_sample_config.sender ? 
                        EXP_VQEC : EXP_VQES, /* sender role */
                        rtp_sample_config.sender ? 
                        EXP_VQES : EXP_VQEC, /* receiver role */
                        orig_send_time,
                        orig_addrs);
        exp_export(&exp_hdr, p_buf, len);
    }
}

/*
 * rtcp_packet_sent_sample
 *
 * Sample post-processing of a transmitted RTCP packet
 */

void rtcp_packet_sent_sample (rtp_session_t *p_sess,
                              rtp_envelope_t *orig_addrs,
                              ntp64_t orig_send_time,
                              uint8_t *p_buf,
                              uint16_t len)
{
    exp_hdr_t exp_hdr;
    exp_hdr_t *p_hdr = &exp_hdr;
    rtp_sample_t *sess = (rtp_sample_t *)p_sess;

    /*
     * As an example, here's how to export a compound packet:
     *
     * 1. initialize the EXP header via the exp_report_init() function:
     *    - the values of the subtype, chan_addr, chan_port, 
     *      sndr_role, and rcvr_role arguments must be determined 
     *      by the application-level class.
     *    - the orig_send_time and orig_addrs arguments should be the same
     *      as supplied in the orig_send_time and orig_addrs arguments
     *      to this function.
     * 2. call exp_export() to export the packet.
     */

    if (rtp_sample_config.export) {
        exp_report_init(p_hdr,
                        sess->primary ? 
                        EXP_ORIGINAL : EXP_REXMIT,  /* report subtype */
                        rtp_sample_config.rtp_addr, /* channel address */
                        rtp_sample_config.rtp_port, /* channel port */
                        rtp_sample_config.sender ? 
                        EXP_VQES : EXP_VQEC, /* sender role */
                        rtp_sample_config.sender ? 
                        EXP_VQEC : EXP_VQES, /* receiver role */
                        orig_send_time,
                        orig_addrs);
        exp_export(p_hdr, p_buf, len);
    }
}

/*
 * rtp_data_source_demoted_sample
 *
 * Sample post-processing of a "data source demoted" event.
 * Inform the dataplane via a call to delete_sample_source;
 * the next time the source is active, the rtp_new_data_source
 * method will be invoked from the dataplane.
 */
void rtp_data_source_demoted_sample(rtp_session_t *p_sess,
                                    rtp_source_id_t *p_src_id,
                                    boolean deleted)
{
    rtp_sample_t *session = (rtp_sample_t *)p_sess;
    sample_receiver_t *receiver = NULL;

    if (rtp_sample_config.sender) {
        /* 
         * if configured as a "sender", there is no "receiver"
         * of any type, so just return
         */
        return;
    }

    /*
     * This only needs to be done once, but there's no harm in
     * trying to delete a source that's already gone, so do it
     * for any session.
     */
    receiver = session->primary ? 
        session->channel->pri_rcvr : session->channel->rpr_rcvr;
    delete_sample_source(receiver, p_src_id);
}
   

