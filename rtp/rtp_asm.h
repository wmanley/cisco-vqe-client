/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_asm.h - defines an RTP session for the ASM (any source
 *             multicast) environment, derived from the RTP
 *             base session "class".
 * 
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

/*
 * Prevent multiple inclusion.
 */
#ifndef __rtp_asm_h__
#define __rtp_asm_h__

#include "rtp_session.h"
/*
 * The ASM session is derived from the base RTP session.
 *
 * The methods table will be overridden by any methods that are unique to
 * this type of session, and may also be augmented to include more
 * ASM type-specific methods.
 *
 * The member cache contains all the rtp_member_t elements for           
 * all the members(sources and receivers) of a session. The number and the  
 * access method may be different for the different types of sessions.     
 * While the cache/data struct of members may be different, currently the
 * member structure itself is the same for all types of sessions.
 */                                                                         

#define RTP_ASM_METHODS     \
    RTP_SESSION_METHODS     
    /* 
     * Insert any session methods here that are unique to the ASM session 
     * type 
     */
  
#define RTP_ASM_INFO        \
    RTP_SESSION_INFO        
    /* Insert any session fields here that are unique to the 
     * ASM session type */
    /* Calling this "info" instead of "members"; the RTP term "members" 
     * refers to session participants, not fields in struct.
     */

typedef struct rtp_asm_methods_t_ {
    RTP_ASM_METHODS
} rtp_asm_methods_t;

#define RTP_ASM_SESSIONS_MAX    1000
#define RTP_ASM_SESSION_CHUNKS  RTP_ASM_SESSIONS_MAX

/*
 * Function to populate member function pointer table
 */
void rtp_asm_set_methods(rtp_asm_methods_t * table);

typedef struct rtp_asm_t_ {
    rtp_asm_methods_t * __func_table;
    RTP_ASM_INFO
} rtp_asm_t;


#define RTP_ASM_SESSIONS   1000
#define RTP_ASM_MEMBERS_PER_SESSION  100

void rtp_asm_set_rtcp_handlers(rtcp_handlers_t *table);
void rtp_asm_init_memory(rtcp_memory_t *memory);
void rtp_asm_init_module(void);

/* Prototypes...  create, plus all the overloading functions... */

boolean rtp_create_session_asm (rtp_config_t *p_config, rtp_asm_t **p_sess, 
                                boolean alloc);

void rtp_delete_session_asm (rtp_asm_t **p_asm_sess, boolean deallocate);

#endif
