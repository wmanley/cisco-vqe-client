/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_asm.c - Functions to handle ASM (any source multicast) RTP
 *             session.
 * 
 * Copyright (c) 2006-2011, 2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#include <stdio.h>
#include "rtp_asm.h"

/*
 * Extern the global instantiation of the memory mgr object.
 */
static rtp_asm_methods_t rtp_asm_methods_table;
static rtcp_handlers_t   rtp_asm_rtcp_handler;
static rtcp_memory_t     rtp_asm_memory;

/*------------RTP functions specific to ASM type sessions---------------*/
/* Function:    rtp_sesssion_set_method
 * Description: One time (class level) initialization of various function tables
 *              This should be call one time at system initialization
 * Parameters:  None
 * Returns:     None
*/
void rtp_asm_init_module(void)
{    
    rtp_asm_set_methods(&rtp_asm_methods_table);
    rtp_asm_set_rtcp_handlers(&rtp_asm_rtcp_handler);
    rtp_asm_init_memory(&rtp_asm_memory);
}

/*
 * Function:    rtp_sesssion_set_method
 * Description: Sets the function pointers in the func ptr table
 * Parameters:  table pointer to a function ptr table which gets populated by 
                this funciton
 * Returns:     None
*/
void rtp_asm_set_methods(rtp_asm_methods_t * table)
{
  /* First set the the base method functions */ 
  rtp_session_set_methods((rtp_session_methods_t *)table);

  /* Now override and/or populate derived class functions */

}

/* Function:    rtp_asm_set_rtcp_handler
 * Description: Sets the function pointers for RTCP handlers
 * Parameters:  None
 * Returns:     None
 */
void rtp_asm_set_rtcp_handlers(rtcp_handlers_t *table)
{
    /* Set base rtp session handler first */
    rtp_session_set_rtcp_handlers(table);
    
    /* Now override or add new handlers */
}

/*
 * rtp_asm_init_memory
 *
 * Initializes RTCP memory used by ASM.
 *
 * Parameters:
 * memory           -- ptr to memory info struct.
 */
void
rtp_asm_init_memory (rtcp_memory_t *memory)
{
    rtcp_cfg_memory(memory,
                    "ASM",
                    sizeof(rtp_asm_t), /* session size, excluding hash tbl */
                    RTP_ASM_SESSIONS,
                    sizeof(rtp_member_t),
                    (RTP_ASM_MEMBERS_PER_SESSION -2) * RTP_ASM_SESSIONS,
                    sizeof(rtp_member_t),
                    2 * RTP_ASM_SESSIONS);


    (void)rtcp_init_memory(memory);
}


/* Function:    rtp_create_session_asm 
 * Description: Create the session structure for an RTP/RTCP session for asm environment.
 * Parameters:  p_config: RTP session config parameters
 *              pp_asm_ses:  ptr ptr to the asm session being created
 *              allocate: boolen to check if the session should be allocated or not
 * 
 * Returns:     TRUE or FALSE
*/
boolean rtp_create_session_asm (rtp_config_t *p_config,
                                rtp_asm_t **pp_asm_sess, boolean allocate)
{
    boolean f_init_successful = FALSE;

    if (allocate) {
        /*
         * Allocate according to the derived class size, not base size
         * The memory allocated is enough not only for the sess but also the
         * methods and the infrastructure for the member cache.
         * Set up a pointer to per-class memory, for future allocations,
         * and for member hash table init (done in the base class).
         */
        *pp_asm_sess = rtcp_new_session(&rtp_asm_memory);
        if (!*pp_asm_sess) {
            return(FALSE);
        } 
        memset(*pp_asm_sess, 0, sizeof(rtp_asm_t));
    
        (*pp_asm_sess)->rtcp_mem = &rtp_asm_memory;
    }

    /*
     * First perform all the base session checks and initializations.
     * This also will set up the base session methods.
     */
     f_init_successful = rtp_create_session_base(p_config,
                                                 (rtp_session_t *)*pp_asm_sess);
    
     if (f_init_successful == FALSE && allocate) {
       rtcp_delete_session(&rtp_asm_memory, *pp_asm_sess);
       return FALSE;
     }     

     /* Set the methods table */
     (*pp_asm_sess)->__func_table = &rtp_asm_methods_table;

     /* Set RTCP handlers */
      (*pp_asm_sess)->rtcp_handler = &rtp_asm_rtcp_handler;

      RTP_LOG_DEBUG_F((rtp_session_t *)(*pp_asm_sess), NULL, 
                      "Created ASM session\n");

     return(TRUE);
}

/*
 * Function:    rtp_delete_session_base
 * Description: Frees a session.  The session will be cleaned first using
 *              rtp_cleanup_session.
 * Parameters:  pp_sess  ptr ptr to session object
 *              deallocate boolen to check if the object should be destoryed
 * Returns:     None 
*/
void rtp_delete_session_asm (rtp_asm_t **pp_asm_sess, boolean deallocate)
{

    if ((pp_asm_sess == NULL) || (*pp_asm_sess == NULL)) {
        RTP_LOG_ERROR("Failed to delete ASM Session: NULL pointer (0x%08p or 0x%08p)\n",
                      pp_asm_sess, *pp_asm_sess ? *pp_asm_sess : 0);
        return;
    }

    RTP_LOG_DEBUG_F((rtp_session_t *)(*pp_asm_sess), NULL, 
                    "Deleted ASM Session\n");

    rtp_delete_session_base ((rtp_session_t **)pp_asm_sess);

    if (deallocate) {
        rtcp_delete_session(&rtp_asm_memory, *pp_asm_sess);
        *pp_asm_sess = NULL;
    }

}
