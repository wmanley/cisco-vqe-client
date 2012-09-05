/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * sample_rtp_session.h - defines a sample Point-to-Point RTP session.  
 *  This is a derived "class" from the ptp
 * 
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#ifndef __rtp_sample_h__
#define __rtp_sample_h__

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#include "../rtp_ptp.h"
#include "../../include/sys/event.h"
#include "../../include/utils/zone_mgr.h"

#include "sample_rtp_application.h"

/*
 * The sample session is derived from the base session.
 *
 * The methods table will be overridden by any methods that are unique to
 * this type of session.
 *
 */                                                                         

#define RTP_SAMPLE_METHODS     \
    RTP_SESSION_METHODS     
    /* 
     * Insert any session methods here that are unique to the SAMPLE
     * session type
     */


typedef struct sample_event_t_ {
    rtp_session_t  *p_session;
    struct event    ev;
} sample_event_t;

#define RTP_SAMPLE_INFO        \
    RTP_SESSION_INFO           \
    /* class-specific data */  \
    int session_number;        \
    boolean primary;           \
    VQE_LIST_ENTRY(rtp_sample_t_) link; \
    sample_channel_t *channel;     \
    sample_event_t gap_timeout;    \
    sample_event_t rcc_timeout;    \
    sample_event_t report_timeout; \
    sample_event_t member_timeout; \
    rtcp_xr_stats_t xr_stats;  \
    rtcp_xr_post_rpr_stats_t post_er_stats;  \
    int next_read_idx;

typedef struct rtp_sample_methods_t_ {
    RTP_SAMPLE_METHODS     
} rtp_sample_methods_t;

struct rtp_sample_t_ {
    rtp_sample_methods_t * __func_table;
    RTP_SAMPLE_INFO
} ;

/* for RTCP memory initialization */
#define RTP_SAMPLE_SESSIONS                 2
#define RTP_SAMPLE_MAX_MEMBERS_PER_SESSION  4
#define RTP_SAMPLE_MEMBERS  (RTP_SAMPLE_SESSIONS *                \
                             RTP_SAMPLE_MAX_MEMBERS_PER_SESSION)

/*
 * Function to populate member function pointer table
 */
boolean rtp_sample_init_module(sample_config_t *p_config);

/* Prototypes...  create, plus all the overloading functions... */
rtp_sample_t *create_session(int session_number,
                             boolean primary,
                             sample_config_t *config,
                             sample_channel_t *channel);
rtp_sample_t **rtp_create_session_sample(sample_config_t *p_config,
                                         sample_channel_t *p_channel,
                                         boolean primary,
                                         int *num_sessions);
int delete_session(int session_number);
int rtp_delete_session_sample(rtp_sample_t **pp_sample_sess,
                              rtp_sample_t **pp_repair_sess,
                              int num_sessions);

void rtp_update_sender_stats_sample(rtp_session_t *p_sess, rtp_member_t *p_source);
void rtp_update_receiver_stats_sample(rtp_session_t *p_sess, 
                                      rtp_member_t *p_source, 
                                      boolean reset_stats);
uint32_t rtcp_construct_report_sender_sample(rtp_session_t   *p_sess,
                                             rtp_member_t    *p_source, 
                                             rtcptype        *p_rtcp,
                                             uint32_t         bufflen,
                                             rtcp_pkt_info_t *p_pkt_info,
                                             boolean         reset_stats);
uint32_t rtcp_construct_report_receiver_sample(rtp_session_t   *p_sess,
                                               rtp_member_t    *p_source, 
                                               rtcptype        *p_rtcp,
                                               uint32_t         bufflen,
                                               rtcp_pkt_info_t *p_pkt_info,
                                               boolean         reset_stats);
uint32_t rtcp_construct_report_repair(rtp_session_t   *p_sess,
                                      rtp_member_t    *p_source, 
                                      rtcptype        *p_rtcp,
                                      uint32_t         bufflen,
                                      rtcp_pkt_info_t *p_pkt_info,
                                      boolean         reset_stats);
uint32_t rtcp_construct_report_script(rtp_session_t   *p_sess,
                                      rtp_member_t    *p_source, 
                                      rtcptype        *p_rtcp,
                                      uint32_t         bufflen,
                                      rtcp_pkt_info_t *p_pkt_info,
                                      boolean         reset_stats);
void rtcp_report_notify(rtp_session_t *p_sess);
void rtcp_packet_rcvd_sample(rtp_session_t *p_sess,                   
                             boolean from_recv_only_member,
                             rtp_envelope_t *orig_addrs,
                             ntp64_t orig_send_time,
                             uint8_t *p_buf,
                             uint16_t len);
void rtcp_packet_sent_sample(rtp_session_t *p_sess,                   
                             rtp_envelope_t *orig_addrs,
                             ntp64_t orig_send_time,
                             uint8_t *p_buf,
                             uint16_t len);
void rtp_data_source_demoted_sample(rtp_session_t *p_sess,
                                    rtp_source_id_t *p_src_id,
                                    boolean deleted);
#endif /*__rtp_sample_h__ */
