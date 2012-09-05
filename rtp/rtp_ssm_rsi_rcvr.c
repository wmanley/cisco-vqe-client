/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_ssm_rsi_rcvr.c - Functions to handle SSM_RSI_RCVR (any source multicast) RTP
 *             session.
 * 
 * Copyright (c) 2006-2011 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#include <stdio.h>
#include "rtp_ssm_rsi.h"
#include "rtp_ssm_rsi_rcvr.h"

static rtcp_memory_t     rtp_ssm_rsi_rcvr_memory;
static rtp_ssm_rsi_rcvr_methods_t rtp_ssm_rsi_rcvr_methods_table;
static rtcp_handlers_t   rtp_ssm_rsi_rcvr_rtcp_handler;

/*------------RTP functions specific to SSM_RSI_RCVR type sessions---------------*/
/* Function:    rtp_sesssion_set_method
 * Description: One time (class level) initialization of various function tables
 *              This should be call one time at system initialization
 * Parameters:  None
 * Returns:     None
*/
void rtp_ssm_rsi_rcvr_init_module(void)
{    
    rtp_ssm_rsi_rcvr_set_methods(&rtp_ssm_rsi_rcvr_methods_table);
    rtp_ssm_rsi_rcvr_set_rtcp_handlers(&rtp_ssm_rsi_rcvr_rtcp_handler);
    rtp_ssm_rsi_rcvr_init_memory(&rtp_ssm_rsi_rcvr_memory);
}

/*
 * Function:    rtp_sesssion_set_method
 * Description: Sets the function pointers in the func ptr table
 * Parameters:  table pointer to a function ptr table which gets populated by 
                this funciton
 * Returns:     None
*/
void rtp_ssm_rsi_rcvr_set_methods(rtp_ssm_rsi_rcvr_methods_t * table)
{
  /* First set the the base method functions */ 
  rtp_session_set_methods((rtp_session_methods_t *)table);

  /* Now override and/or populate derived class functions */
}

/* Function:    rtp_ssm_rsi_rcvr_set_rtcp_handler
 * Description: Sets the function pointers for RTCP handlers
 * Parameters:  None
 * Returns:     None
 */
void rtp_ssm_rsi_rcvr_set_rtcp_handlers(rtcp_handlers_t *table)
{
  /* Set base rtp session handler first */
  rtp_session_set_rtcp_handlers(table);

  /* Now override or add new handlers */
  table->proc_handler[RTCP_RSI]  = rtcp_process_rsi;
}

/*
 * rtp_ssm_rsi_rcvr_init_memory
 *
 * Initializes RTCP memory used by SSM RSI RCVR.
 *
 * Parameters:
 * memory           -- ptr to memory info struct
 */
void
rtp_ssm_rsi_rcvr_init_memory (rtcp_memory_t *memory)
{
    rtcp_cfg_memory(memory,
                    "SSM-RCVR",
                    /* session size, excluding hash table */
                    sizeof(rtp_ssm_rsi_rcvr_t),
                    RTP_SSM_RSI_RCVR_SESSIONS,
                    sizeof(rtp_member_t),
                    RTP_SSM_RSI_RCVR_SESSIONS * 
                    (RTP_SSM_RSI_RCVR_MEMBERS_PER_SESSION - 1),
                    sizeof(rtp_member_t),
                    RTP_SSM_RSI_RCVR_SESSIONS);

    (void)rtcp_init_memory(memory);
}

/* Function:    rtp_create_session_ssm_rsi_rcvr 
 * Description: Create the session structure for an RTP/RTCP session for 
 *              ssm_rsi_rcvr environment.
 * Parameters:  p_config: RTP session config parameters
 *              pp_ssm_rsi_rcvr_ses:  ptr ptr to the ssm_rsi_rcvr 
 *                                      session being created
 *              allocate: boolean to check if the session should be 
 *                        allocated or not
 * 
 * Returns:     TRUE or FALSE
*/
boolean rtp_create_session_ssm_rsi_rcvr (rtp_config_t *p_config,
                                           rtp_ssm_rsi_rcvr_t 
                                           **pp_ssm_rsi_rcvr, 
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
        *pp_ssm_rsi_rcvr = rtcp_new_session(&rtp_ssm_rsi_rcvr_memory);
        if (!*pp_ssm_rsi_rcvr) {
            return(FALSE);
        } 
        memset(*pp_ssm_rsi_rcvr, 0, sizeof(rtp_ssm_rsi_rcvr_t));
    
        (*pp_ssm_rsi_rcvr)->rtcp_mem = &rtp_ssm_rsi_rcvr_memory;
    }

    /*
     * First perform all the base session checks and initializations.
     * This also will set up the base session methods.
     */
     f_init_successful = 
       rtp_create_session_base(p_config,
                               (rtp_session_t *)*pp_ssm_rsi_rcvr);
    
     if (f_init_successful == FALSE && allocate) {
        rtcp_delete_session(&rtp_ssm_rsi_rcvr_memory, *pp_ssm_rsi_rcvr);
        return FALSE;
     }     

     /* Set the methods table */
     (*pp_ssm_rsi_rcvr)->__func_table = &rtp_ssm_rsi_rcvr_methods_table;

     /* Set RTCP handlers */
      (*pp_ssm_rsi_rcvr)->rtcp_handler = &rtp_ssm_rsi_rcvr_rtcp_handler;

      RTP_LOG_DEBUG_F((rtp_session_t *)(*pp_ssm_rsi_rcvr), NULL,
                      "Created SSM_RSI_RCVR session\n");

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
void rtp_delete_session_ssm_rsi_rcvr (rtp_ssm_rsi_rcvr_t **pp_ssm_rsi_rcvr, 
                                        boolean deallocate)
{

    if ((pp_ssm_rsi_rcvr == NULL) || (*pp_ssm_rsi_rcvr == NULL)) {
        RTP_LOG_ERROR("Failed to delete SSM_RSI_RCVR session: "
                      "NULL pointer 0x%08p or 0x%08p",
                      pp_ssm_rsi_rcvr, 
                      *pp_ssm_rsi_rcvr ? *pp_ssm_rsi_rcvr : 0);
        return;
    }

    RTP_LOG_DEBUG_F((rtp_session_t *)(*pp_ssm_rsi_rcvr), NULL,
                    "Deleted SSM_RSI_RCVR session\n");

    rtp_delete_session_base ((rtp_session_t **)pp_ssm_rsi_rcvr);

    if (deallocate) {
        rtcp_delete_session(&rtp_ssm_rsi_rcvr_memory, *pp_ssm_rsi_rcvr);
        *pp_ssm_rsi_rcvr = NULL;
    }

}

