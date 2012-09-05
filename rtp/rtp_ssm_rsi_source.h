/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_ssm_rsi_source.h - defines an RTP session for a source in 
 *   the SSM, Distribution  Source Feedback Summary Model,.  
 *  This is a derived "class" from the RTP base session "class".
 * 
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#ifndef __rtp_ssm_rsi_source_h__
#define __rtp_ssm_rsi_source_h__

#include "rtp_session.h"


/*
 * The SSM-RSI member class is derived from the base RTP member class
 */
#define  SSM_RSI_SOURCE_MEMBER_INFO                       \
    RTP_MEMBER_INFO

/*
 * The SSM-RSI session is derived from the base RTP session.
 *
 * The methods table will be overridden by any methods that are unique to
 * this type of session, and may also be augmented to include more
 * SSM-RSI specific methods.
 *
 * The member cache contains all the rtp_member_t elements for           
 * all the members(sources and receivers) of a session. The number and the  
 * access method may be different for the different types of sessions.     
 * While the cache/data struct of members may be different, currently the
 * member structure itself is the same for all types of sessions.
 */                                                                         

#define RTP_SSM_RSI_SOURCE_METHODS     \
    RTP_SESSION_METHODS     
    /* 
     * Insert any session methods here that are unique to the SSM-RSI
     * session type
     */


/* Insert any session fields that are unique to the SSM-RSI-SOURCE session type 
 * after RTP_SESSION_INFO */
#define RTP_SSM_RSI_SOURCE_INFO        \
    RTP_SESSION_INFO        


typedef struct rtp_ssm_rsi_source_methods_t_ {
    RTP_SSM_RSI_SOURCE_METHODS     
} rtp_ssm_rsi_source_methods_t;


/*
 * Function to populate member function pointer table
 */
void rtp_ssm_rsi_source_set_methods(rtp_ssm_rsi_source_methods_t * table);

typedef struct rtp_ssm_rsi_source_t_ {
    rtp_ssm_rsi_source_methods_t * __func_table;
    RTP_SSM_RSI_SOURCE_INFO
} rtp_ssm_rsi_source_t;

#define RTP_SSM_RSI_SOURCE_SESSIONS   1000
/* max receivers, over all sessions */
#define RTP_SSM_RSI_SOURCE_RCVRS     10000
/* 
 * max members, over all sessions:
 * per-session:
 * . one local (sending) member
 * over all sessions:
 * . a set of potential receivers
 */
#define RTP_SSM_RSI_SOURCE_MEMBERS   (RTP_SSM_RSI_SOURCE_SESSIONS) + \
                                     (RTP_SSM_RSI_SOURCE_RCVRS)
#define RTP_SSM_RSI_SOURCE_MEMBERS_PER_SESSION  (RTP_SSM_RSI_SOURCE_RCVRS + 2)


void rtp_ssm_rsi_source_init_memory(rtcp_memory_t *memory);
void rtp_ssm_rsi_source_set_rtcp_handlers(rtcp_handlers_t *table);
void rtp_ssm_rsi_source_init_module(void);

/* Prototypes...  create, plus all the overloading functions... */

boolean rtp_create_session_ssm_rsi_source(rtp_config_t *p_config, 
                                          rtp_ssm_rsi_source_t **p_sess, 
                                          boolean alloc);

void rtp_delete_session_ssm_rsi_source(rtp_ssm_rsi_source_t **p_ssm_rsi_source,
                                       boolean deallocate);

/* Overridden member function */
uint32_t rtcp_construct_report_ssm_rsi_source(rtp_session_t   *p_ssm_rsi_source,
                                              rtp_member_t    *p_source, 
                                              rtcptype        *p_rtcp,
                                              uint32_t         bufflen,
                                              rtcp_pkt_info_t *p_pkt_info,
                                              boolean          reset_xr_stats);
#endif /*__rtp_ssm_rsi_source_h__ */
