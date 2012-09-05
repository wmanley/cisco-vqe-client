/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_ssm_rsi_fbt.h - defines an RTP session for a feedback target
 * which is NOT a source, in a Source-Specific Muliticast session, 
 * using the Distribution Source Feedback Summary Model. 
 *
 * This is a derived "class" from the RTP base session "class".
 * It will typically be the base of a class used on the VAM (VQE-S), 
 * in lookaside mode.
 * 
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#ifndef __rtp_ssm_rsi_fbt_h__
#define __rtp_ssm_rsi_fbt_h__

#include "rtp_session.h"


/*
 * The SSM_RSI_FBT member class is derived from the base RTP member class
 */
#define  SSM_RSI_FBT_MEMBER_INFO                       \
    RTP_MEMBER_INFO

/*
 * The SSM_RSI_FBT session is derived from the base RTP session.
 *
 * The methods table will be overridden by any methods that are unique to
 * this type of session, and may also be augmented to include more
 * SSM_RSI_FBT specific methods.
 *
 * The member cache contains all the rtp_member_t elements for           
 * all the participants (senders and receivers) in a session. 
 * The number of members, and the access method for finding a member,
 * may be different for the different types of sessions.     
 */                                                                         

#define RTP_SSM_RSI_FBT_METHODS     \
    RTP_SESSION_METHODS     
    /* 
     * Insert any session methods here that are unique to the SSM_RSI_FBT
     * session type
     */


/* Insert any session fields that are unique to the SSM-RSI-FVT session type 
 * after RTP_SESSION_INFO */
#define RTP_SSM_RSI_FBT_INFO        \
    RTP_SESSION_INFO        


typedef struct rtp_ssm_rsi_fbt_methods_t_ {
    RTP_SSM_RSI_FBT_METHODS     
} rtp_ssm_rsi_fbt_methods_t;


/*
 * Function to populate member function pointer table
 */
void rtp_ssm_rsi_fbt_set_methods(rtp_ssm_rsi_fbt_methods_t * table);

typedef struct rtp_ssm_rsi_fbt_t_ {
    rtp_ssm_rsi_fbt_methods_t * __func_table;
    RTP_SSM_RSI_FBT_INFO
} rtp_ssm_rsi_fbt_t;

#define RTP_SSM_RSI_FBT_SESSIONS   1000
/* max (non-FBT) receivers, over all sessions */
#define RTP_SSM_RSI_FBT_RCVRS     10000
/* 
 * max members, over all sessions:
 * per-session:
 * . one local (FBT) member
 * . one sending member
 * over all sessions:
 * . a set of potential receivers
 */
#define RTP_SSM_RSI_FBT_MEMBERS   (2 * RTP_SSM_RSI_FBT_SESSIONS) + \
                                  (RTP_SSM_RSI_FBT_RCVRS)
#define RTP_SSM_RSI_FBT_MEMBERS_PER_SESSION  (RTP_SSM_RSI_FBT_RCVRS + 2)

void rtp_ssm_rsi_fbt_init_memory(rtcp_memory_t *memory);
void rtp_ssm_rsi_fbt_set_rtcp_handlers(rtcp_handlers_t *table);
void rtp_ssm_rsi_fbt_init_module(void);

/* Prototypes...  create, plus all the overloading functions... */

boolean rtp_create_session_ssm_rsi_fbt(rtp_config_t *p_config, 
                                       rtp_ssm_rsi_fbt_t **p_sess, 
                                       boolean alloc);

void rtp_delete_session_ssm_rsi_fbt(rtp_ssm_rsi_fbt_t **p_ssm_rsi_fbt, 
                                    boolean deallocate);

/* Overridden member function */
uint32_t rtcp_construct_report_ssm_rsi_fbt (rtp_session_t   *p_ssm_rsi_fbt,
                                            rtp_member_t    *p_fbt, 
                                            rtcptype        *p_rtcp,
                                            uint32_t         bufflen,
                                            rtcp_pkt_info_t *p_pkt_info,
                                            boolean          reset_xr_stats);
#endif /*__rtp_ssm_rsi_fbt_h__ */
