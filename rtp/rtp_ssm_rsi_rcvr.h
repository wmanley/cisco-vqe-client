/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_ssm_rsi_rcvr.h - defines an RTP session for the SSM, RSI mode
 * receiver session.  This is a derived "class" from
 * the RTP base session "class".
 * 
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#ifndef __rtp_ssm_rsi_rcvr_h__
#define __rtp_ssm_rsi_rcvr_h__

#include "rtp_session.h"


/*
 * The SSM-RSI member class is derived from the base RTP member class
 */
#define  SSM_RSI_RCVR_MEMBER_INFO                       \
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

#define RTP_SSM_RSI_RCVR_METHODS     \
    RTP_SESSION_METHODS     
    /* 
     * Insert any session methods here that are unique to the SSM-RSI
     * session type
     */


#define RTP_SSM_RSI_RCVR_INFO        \
    RTP_SESSION_INFO        
    /* Insert any session fields here that are unique to the 
     * SSM-RSI session type */
    /* Calling this "info" instead of "members"; the RTP term "members" 
     * refers to session participants, not fields in struct.
     */

typedef struct rtp_ssm_rsi_rcvr_methods_t_ {
    RTP_SSM_RSI_RCVR_METHODS     
} rtp_ssm_rsi_rcvr_methods_t;


/*
 * Function to populate member function pointer table
 */
void rtp_ssm_rsi_rcvr_set_methods(rtp_ssm_rsi_rcvr_methods_t * table);

typedef struct rtp_ssm_rsi_rcvr_t_ {
    rtp_ssm_rsi_rcvr_methods_t * __func_table;
    RTP_SSM_RSI_RCVR_INFO
} rtp_ssm_rsi_rcvr_t;

#define RTP_SSM_RSI_RCVR_SESSIONS            1000
#define RTP_SSM_RSI_RCVR_MEMBERS_PER_SESSION    4

void rtp_ssm_rsi_rcvr_init_memory(rtcp_memory_t *memory);
void rtp_ssm_rsi_rcvr_set_rtcp_handlers(rtcp_handlers_t *table);
void rtp_ssm_rsi_rcvr_init_module(void);

/* Prototypes...  create, plus all the overloading functions... */

boolean rtp_create_session_ssm_rsi_rcvr (rtp_config_t *p_config, rtp_ssm_rsi_rcvr_t **p_sess, 
                                           boolean alloc);

void rtp_delete_session_ssm_rsi_rcvr (rtp_ssm_rsi_rcvr_t **p_ssm_rsi_rcvr, boolean deallocate);


#endif /*__rtp_ssm_rsi_rcvr_h__ */
