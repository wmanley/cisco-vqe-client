/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_ptp.h - defines a Point-to-Point RTP session.  This is a 
 *    derived "class" from the RTP base session "class".
 * 
 * Copyright (c) 2006-2009 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#ifndef __rtp_ptp_h__
#define __rtp_ptp_h__

#include "rtp_session.h"
  

/*
 * The PTP session is derived from the base RTP session.
 *
 * The methods table will be overridden by any methods that are unique to
 * this type of session.
 *
 * The member cache contains all the rtp_member_t elements for           
 * all the members(sources and receivers) of a session. The number and the  
 * access method may be different for the different types of sessions.     
 * While the cache/data struct of members may be different, currently the
 * member structure itself is the same for all types of sessions.
 */                                                                         

#define RTP_PTP_METHODS     \
    RTP_SESSION_METHODS     
    /* 
     * Insert any session methods here that are unique to the SSM-RSI
     * session type
     */


#define RTP_PTP_INFO        \
    RTP_SESSION_INFO        
    /* Insert any session fields here that are unique to the 
     * SSM-RSI session type */
    /* Calling this "info" instead of "members"; the RTP term "members" 
     * refers to session participants, not fields in struct.
     */

typedef struct rtp_ptp_methods_t_ {
    RTP_PTP_METHODS     
} rtp_ptp_methods_t;

/*
 * Function to populate member function pointer table
 */
void rtp_ptp_init_module(void);

typedef struct rtp_ptp_t_ {
    rtp_ptp_methods_t * __func_table;
    RTP_PTP_INFO
} rtp_ptp_t;

/* for the RTCP memory API */
#define RTP_PTP_SESSIONS                1000
#define RTP_PTP_MAX_MEMBERS_PER_SESSION    4


/* Prototypes...  create, plus all the overloading functions... */
boolean rtp_create_session_ptp(rtp_config_t *p_config,
                              rtp_ptp_t **pp_ptp_sess, 
                              boolean allocate);

void rtp_delete_session_ptp(rtp_ptp_t **pp_ptp_sess, boolean deallocate);
void rtp_ptp_set_rtcp_handlers(rtcp_handlers_t * table);
void rtp_ptp_set_methods(rtp_ptp_methods_t * table);

#endif /*__rtp_ptp_h__ */
