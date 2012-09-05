/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_ssm_rsi_source.c - Functions to handle SSM_RSI_SOURCE (any source multicast) RTP
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
#include "rtp_ssm_rsi_source.h"

static rtcp_memory_t     rtp_ssm_rsi_source_memory;
static rtp_ssm_rsi_source_methods_t rtp_ssm_rsi_source_methods_table;
static rtcp_handlers_t   rtp_ssm_rsi_source_rtcp_handler;

/*------------RTP functions specific to SSM_RSI_SOURCE type sessions---------------*/
/* Function:    rtp_sesssion_set_method
 * Description: One time (class level) initialization of various function tables
 *              This should be call one time at system initialization
 * Parameters:  None
 * Returns:     None
*/
void rtp_ssm_rsi_source_init_module()
{    
    rtp_ssm_rsi_source_set_methods(&rtp_ssm_rsi_source_methods_table);
    rtp_ssm_rsi_source_set_rtcp_handlers(&rtp_ssm_rsi_source_rtcp_handler);
    rtp_ssm_rsi_source_init_memory(&rtp_ssm_rsi_source_memory);
}

/*
 * Function:    rtp_sesssion_set_method
 * Description: Sets the function pointers in the func ptr table
 * Parameters:  table pointer to a function ptr table which gets populated by 
                this funciton
 * Returns:     None
*/
void rtp_ssm_rsi_source_set_methods(rtp_ssm_rsi_source_methods_t * table)
{
  /* First set the the base method functions */ 
  rtp_session_set_methods((rtp_session_methods_t *)table);

  /* Now override and/or populate derived class functions */
  table->rtcp_construct_report = rtcp_construct_report_ssm_rsi_source;
}

/* Function:    rtp_ssm_rsi_source_set_rtcp_handler
 * Description: Sets the function pointers for RTCP handlers
 * Parameters:  None
 * Returns:     None
 */
void rtp_ssm_rsi_source_set_rtcp_handlers(rtcp_handlers_t *table)
{
  /* Set base rtp session handler first */
  rtp_session_set_rtcp_handlers(table);

  /* Now override or add new handlers */
  table->proc_handler[RTCP_RSI] = rtcp_process_rsi;
}

/*
 * rtp_ssm_rsi_source_init_memory
 *
 * Initialize RTCP memory used by SSM RSI SOURCE.
 *
 * Parameters:
 * memory          -- ptr to memory info struct.
 */
void
rtp_ssm_rsi_source_init_memory (rtcp_memory_t *memory)
{
    rtcp_cfg_memory(memory,
                    "SSM-SOURCE",
                    /* session size, excluding hash table */
                    sizeof(rtp_ssm_rsi_source_t),
                    RTP_SSM_RSI_SOURCE_SESSIONS,
                    sizeof(rtp_member_t),
                    RTP_SSM_RSI_SOURCE_RCVRS,
                    sizeof(rtp_member_t),
                    RTP_SSM_RSI_SOURCE_SESSIONS);

    (void)rtcp_init_memory(memory);
}



/* Function:    rtp_create_session_ssm_rsi_source 
 * Description: Create the session structure for an RTP/RTCP session for 
 *              ssm_rsi_source environment.
 * Parameters:  p_config: RTP session config parameters
 *              pp_ssm_rsi_source_ses:  ptr ptr to the ssm_rsi_source 
 *                                      session being created
 *              allocate: boolean to check if the session should be 
 *                        allocated or not
 * 
 * Returns:     TRUE or FALSE
*/
boolean rtp_create_session_ssm_rsi_source (rtp_config_t *p_config,
                                           rtp_ssm_rsi_source_t 
                                           **pp_ssm_rsi_source, 
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
        *pp_ssm_rsi_source = rtcp_new_session(&rtp_ssm_rsi_source_memory);
        if (!*pp_ssm_rsi_source) {
            return(FALSE);
        } 
        memset(*pp_ssm_rsi_source, 0, sizeof(rtp_ssm_rsi_source_t));
    
        (*pp_ssm_rsi_source)->rtcp_mem = &rtp_ssm_rsi_source_memory;
    }

    /*
     * First perform all the base session checks and initializations.
     * This also will set up the base session methods.
     */
     f_init_successful = 
       rtp_create_session_base(p_config,
                               (rtp_session_t *)*pp_ssm_rsi_source);
    
     if (f_init_successful == FALSE && allocate) {
        rtcp_delete_session(&rtp_ssm_rsi_source_memory, *pp_ssm_rsi_source);
        return FALSE;
     }     

     /* Set the methods table */
     (*pp_ssm_rsi_source)->__func_table = &rtp_ssm_rsi_source_methods_table;

     /* Set RTCP handlers */
      (*pp_ssm_rsi_source)->rtcp_handler = &rtp_ssm_rsi_source_rtcp_handler;

      RTP_LOG_DEBUG_F((rtp_session_t *)(*pp_ssm_rsi_source), NULL,
                      "Created SSM_RSI_SOURCE session\n");

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
void rtp_delete_session_ssm_rsi_source (rtp_ssm_rsi_source_t **pp_ssm_rsi_source, 
                                        boolean deallocate)
{

    if ((pp_ssm_rsi_source == NULL) || (*pp_ssm_rsi_source == NULL)) {
        RTP_LOG_ERROR("Failed to delete SSM_RSI_SOURCE session: "
                      "NULL pointer 0x%08p or 0x%08p\n",
                      pp_ssm_rsi_source, 
                      *pp_ssm_rsi_source ? *pp_ssm_rsi_source : 0);
        return;
    }

    RTP_LOG_DEBUG_F((rtp_session_t *)(*pp_ssm_rsi_source), NULL,
                    "Deleted SSM_RSI_SOURCE session\n");

    rtp_delete_session_base ((rtp_session_t **)pp_ssm_rsi_source);

    if (deallocate) {
        rtcp_delete_session(&rtp_ssm_rsi_source_memory, *pp_ssm_rsi_source);
        *pp_ssm_rsi_source = NULL;
    }

}

/*
 * rtcp_construct_report_ssm_rsi_source
 *
 * Construct a report including the Receiver Summary Information (RSI) message.
 * This is an overriding implementation of the rtcp_construct_report method,
 * for the SSM RSI Source class.
 *
 * Parameters:  p_sess     --pointer to session object
 *              p_source   --pointer to local source member
 *              p_rtcp     --pointer to start of RTCP packet in the buffer
 *              bufflen    --length (in bytes) of RTCP packet buffer
 *              p_pkt_info --pointer to additional packet info 
 *                           (if any; may be NULL)
 *              reset_xr_stats --flag for whether to reset RTCP XR stats
 * Returns:     length of the packet including RTCP header;
 *              zero indicates a construction failure.
 */
uint32_t rtcp_construct_report_ssm_rsi_source (rtp_session_t   *p_sess,
                                               rtp_member_t    *p_source, 
                                               rtcptype        *p_rtcp,
                                               uint32_t         bufflen,
                                               rtcp_pkt_info_t *p_pkt_info,
                                               boolean          reset_xr_stats)
{
    rtcp_rsi_info_t rsi_info;
    /*
     * Set up to add an RSI message (with an RTCP Bandwidth Indication
     * subreport) to the pre-specified content of the compound packet.
     */
    rsi_info.subrpt_mask = RTCP_RSI_SUBRPT_MASK_0;
    rtcp_set_rsi_subrpt_mask(&rsi_info.subrpt_mask, RTCP_RSI_BISB);

    return (rtcp_construct_report_ssm_rsi(p_sess, p_source, 
                                          p_rtcp, bufflen,
                                          p_pkt_info, &rsi_info,
                                          reset_xr_stats));
}
