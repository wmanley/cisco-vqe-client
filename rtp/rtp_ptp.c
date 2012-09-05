/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_ptp.c - Functions to handle Point-to-Point RTP session.
 * 
 * Copyright (c) 2006-2011 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#include <stdio.h>
#include "rtp_ptp.h"

static rtcp_memory_t     rtp_ptp_memory;
static rtp_ptp_methods_t rtp_ptp_methods_table;
static rtcp_handlers_t   rtp_ptp_rtcp_handler;


/*------------RTP functions specific to Point-to-Point type sessions---------------*/

/*
 * Function:    rtp_ptp_set_methods
 * Description: Sets the function pointers in the func ptr table
 * Parameters:  table pointer to a function ptr table which gets populated by 
 *              this funciton
 * Returns:     None
 */
void rtp_ptp_set_methods(rtp_ptp_methods_t * table)
{
  /* First set the the base method functions */ 
  rtp_session_set_methods((rtp_session_methods_t *)table);

  /* Now override and/or populate derived class functions */

}


/* Function:    rtp_ptp_set_rtcp_handler
 * Description: Sets the function pointers for RTCP handlers
 * Parameters:  None
 * Returns:     None
 */
void rtp_ptp_set_rtcp_handlers(rtcp_handlers_t *table)
{
    /* Set base rtp session handler first */
    rtp_session_set_rtcp_handlers(table);

    /* Now override or add new handlers */

}

/*
 * rtp_ptp_init_memory
 *
 * Initializes RTCP memory used by PTP.
 *
 * Parameters:
 * memory           -- ptr to memory info struct.
 */
void
rtp_ptp_init_memory (rtcp_memory_t *memory)
{
    rtcp_cfg_memory(memory,
                    "PTP",
                    sizeof(rtp_ptp_t), /* session size, excluding hash tbl */
                    RTP_PTP_SESSIONS,
                    sizeof(rtp_member_t),
                    RTP_PTP_SESSIONS * (RTP_PTP_MAX_MEMBERS_PER_SESSION - 1),
                    sizeof(rtp_member_t),
                    RTP_PTP_SESSIONS * 1);

    (void)rtcp_init_memory(memory);
}


/* Function:    rtp_ptp_init_module
 * Description: One time (class level) initialization of various function tables
 *              This should be call one time at system initialization
 * Parameters:  None
 * Returns:     None
*/
void rtp_ptp_init_module(void)
{    
    rtp_ptp_set_methods(&rtp_ptp_methods_table);
    rtp_ptp_set_rtcp_handlers(&rtp_ptp_rtcp_handler);
    rtp_ptp_init_memory(&rtp_ptp_memory);
}



/* Function:    rtp_create_session_ptp
 * Description: Create the session structure for an repair-sender RTP/RTCP session.
 * Parameters:  p_config: RTP session config parameters
 *              pp_ptp_ses:  ptr ptr to the repair-sender session being created
 *              allocate: boolen to check if the session should be allocated or not
 * 
 * Returns:     TRUE or FALSE
*/
boolean rtp_create_session_ptp (rtp_config_t *p_config,
                                rtp_ptp_t **pp_ptp_sess, 
                                boolean allocate)
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
        *pp_ptp_sess = rtcp_new_session(&rtp_ptp_memory);
        if (!*pp_ptp_sess) {
            return(FALSE);
        } 
        memset(*pp_ptp_sess, 0, sizeof(rtp_ptp_t));
    
        (*pp_ptp_sess)->rtcp_mem = &rtp_ptp_memory;
    }

    /*
     * First perform all the base session checks and initializations.
     * This also will set up the base session methods.
     */
     f_init_successful = rtp_create_session_base(p_config,
                                                 (rtp_session_t *)*pp_ptp_sess);
    
     if (f_init_successful == FALSE && allocate) {
         rtcp_delete_session(&rtp_ptp_memory, *pp_ptp_sess);
         return FALSE;
     }     

     /* Set the methods table */
     (*pp_ptp_sess)->__func_table = &rtp_ptp_methods_table;

     /* Set RTCP handlers */
     (*pp_ptp_sess)->rtcp_handler = &rtp_ptp_rtcp_handler;

     RTP_LOG_DEBUG_F((rtp_session_t *)(*pp_ptp_sess), NULL,
                     "Created PTP session\n");

     return(TRUE);
}

/*
 * Function:    rtp_delete_session_ptp
 * Description: Frees a session.  The session will be cleaned first using
 *              rtp_cleanup_session.
 * Parameters:  pp_sess  ptr ptr to session object
 *              deallocate boolen to check if the object should be destoryed
 * Returns:     None 
*/
void rtp_delete_session_ptp (rtp_ptp_t **pp_ptp_sess, boolean deallocate)
{

    if ((pp_ptp_sess == NULL) || (*pp_ptp_sess == NULL)) {
        RTP_LOG_ERROR("Failed to delete Point-to-Point Session: "
                      "NULL pointer (0x%08p or 0x%08p)\n",
                      pp_ptp_sess, 
                      *pp_ptp_sess ? *pp_ptp_sess : 0);
        return;
    }

    RTP_LOG_DEBUG_F((rtp_session_t *)(*pp_ptp_sess), NULL,
                    "Deleted Point-to-Point Session\n");

    rtp_delete_session_base ((rtp_session_t **)pp_ptp_sess);

    if (deallocate) {
        rtcp_delete_session(&rtp_ptp_memory, *pp_ptp_sess);
        *pp_ptp_sess = NULL;
    }

}
