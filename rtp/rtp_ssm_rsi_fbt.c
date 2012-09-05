/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_ssm_rsi_fbt.c - Support for an RTP session of the SSM_RSI_FBT
 * class (the Feedback Target role in Source-Specific Multicast session
 * using the Distribution Source Feedback Summary Model, where the
 * participant is NOT also the Distribution Source)
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
#include "rtp_ssm_rsi_fbt.h"

static rtcp_memory_t              rtp_ssm_rsi_fbt_memory;
static rtp_ssm_rsi_fbt_methods_t  rtp_ssm_rsi_fbt_methods_table;
static rtcp_handlers_t            rtp_ssm_rsi_fbt_rtcp_handler;

/*
 * rtp_ssm_rsi_fbt_init_module
 *
 * One-time initialization of the methods table, the message handler
 * table (function table), and data structure memory, for the 
 * RTP_SSM_RSI_FBT core class.
 */
void rtp_ssm_rsi_fbt_init_module(void)
{    
    rtp_ssm_rsi_fbt_set_methods(&rtp_ssm_rsi_fbt_methods_table);
    rtp_ssm_rsi_fbt_set_rtcp_handlers(&rtp_ssm_rsi_fbt_rtcp_handler);
    rtp_ssm_rsi_fbt_init_memory(&rtp_ssm_rsi_fbt_memory);
}

/*
 * rtp_ssm_rsi_fbt_set_methods
 *
 * Sets the function pointers in the methods table for the SSM_RSI_FBT class.
 *
 * Parameters:  table  -- pointer to the methods table (function ptr table) 
 * Returns:     None
*/
void rtp_ssm_rsi_fbt_set_methods(rtp_ssm_rsi_fbt_methods_t * table)
{
  /* First set the the base method functions */ 
  rtp_session_set_methods((rtp_session_methods_t *)table);

  /* Now override and/or populate derived class functions */
  table->rtcp_construct_report = rtcp_construct_report_ssm_rsi_fbt;
}

/* 
 * rtp_ssm_rsi_fbt_set_rtcp_handler
 *
 * Sets the function pointers in the RTCP message handler table
 * for the SSM_RSI_FBT class.
 *
 * Parameters:  None
 * Returns:     None
 */
void rtp_ssm_rsi_fbt_set_rtcp_handlers(rtcp_handlers_t *table)
{
  /* Set base rtp session handler first */
  rtp_session_set_rtcp_handlers(table);

  /* Now override or add new handlers */
  table->proc_handler[RTCP_RSI]  = rtcp_process_rsi;
}

/*
 * rtp_ssm_rsi_fbt_init_memory
 *
 * Initialize RTCP memory used by SSM RSI FBT.
 *
 * Parameters:
 * memory          -- ptr to memory info struct.
 */
void 
rtp_ssm_rsi_fbt_init_memory (rtcp_memory_t *memory)
{
    rtcp_cfg_memory(memory,
                    "SSM-FBT",
                    /* session size, excluding hash table */
                    sizeof(rtp_ssm_rsi_fbt_t), 
                    RTP_SSM_RSI_FBT_SESSIONS,
                    sizeof(rtp_member_t),
                    RTP_SSM_RSI_FBT_RCVRS,
                    sizeof(rtp_member_t),
                    2 * RTP_SSM_RSI_FBT_SESSIONS);

    (void)rtcp_init_memory(memory);
}

/* 
 * rtp_create_session_ssm_rsi_fbt 
 *
 * Create the session structure for an RTP/RTCP session 
 * in the SSM_RSI_FBT class.
 *
 * Parameters:  p_config:        RTP session config parameters
 *              pp_ssm_rsi_fbt:  ptr to a ptr to the ssm_rsi_fbt session data
 *              allocate:        if true, allocate session memory
 * Returns:     TRUE or FALSE
 */
boolean rtp_create_session_ssm_rsi_fbt (rtp_config_t *p_config,
                                        rtp_ssm_rsi_fbt_t **pp_ssm_rsi_fbt, 
                                        boolean allocate)
{
    boolean f_init_successful = FALSE;

    if (allocate) {
        /*
         * Allocate according to the derived class size, not base size
         * The memory allocated from the rtp_ssm_rsi_fbt_zone is enough 
         * not only for the session data but also the method table ptr, 
         * and the infrastructure for the member cache.
         * Set up a pointer to per-class memory, for future allocations,
         * and for member hash table init (done in the base class).
         */
        *pp_ssm_rsi_fbt = rtcp_new_session(&rtp_ssm_rsi_fbt_memory);
        if (!*pp_ssm_rsi_fbt) {
            return(FALSE);
        } 
        memset(*pp_ssm_rsi_fbt, 0, sizeof(rtp_ssm_rsi_fbt_t));

        (*pp_ssm_rsi_fbt)->rtcp_mem = &rtp_ssm_rsi_fbt_memory;
    }

    /*
     * First perform all the base session checks and initializations.
     * This also will set up the base session methods.
     */
     f_init_successful = 
       rtp_create_session_base(p_config,
                               (rtp_session_t *)*pp_ssm_rsi_fbt);
    
     if (f_init_successful == FALSE && allocate) {
        rtcp_delete_session(&rtp_ssm_rsi_fbt_memory, *pp_ssm_rsi_fbt);
        return FALSE;
     }     

     /* Set the methods table */
     (*pp_ssm_rsi_fbt)->__func_table = &rtp_ssm_rsi_fbt_methods_table;

     /* Set RTCP handlers */
      (*pp_ssm_rsi_fbt)->rtcp_handler = &rtp_ssm_rsi_fbt_rtcp_handler;

      RTP_LOG_DEBUG_F((rtp_session_t *)(*pp_ssm_rsi_fbt), NULL,
                      "RTP: Created SSM_RSI_FBT session\n");

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
void rtp_delete_session_ssm_rsi_fbt (rtp_ssm_rsi_fbt_t **pp_ssm_rsi_fbt, 
                                        boolean deallocate)
{

    if ((pp_ssm_rsi_fbt == NULL) || (*pp_ssm_rsi_fbt == NULL)) {
        RTP_LOG_ERROR("Failed to delete SSM_RSI_FBT session: "
                      "NULL pointer 0x%08p or 0x%08p\n",
                      pp_ssm_rsi_fbt, 
                      *pp_ssm_rsi_fbt ? *pp_ssm_rsi_fbt : 0);
        return;
    }

    RTP_LOG_DEBUG_F((rtp_session_t *)(*pp_ssm_rsi_fbt), NULL,
                    "Deleted SSM_RSI_FBT session\n");

    rtp_delete_session_base ((rtp_session_t **)pp_ssm_rsi_fbt);

    if (deallocate) {
        rtcp_delete_session(&rtp_ssm_rsi_fbt_memory, *pp_ssm_rsi_fbt);
        *pp_ssm_rsi_fbt = NULL;
    }

}

/*
 * rtcp_construct_report_ssm_rsi_fbt
 *
 * Construct a report including the Receiver Summary Information (RSI) message.
 * This is an overriding implementation of the rtcp_construct_report method,
 * for the SSM RSI FBT class.
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
uint32_t rtcp_construct_report_ssm_rsi_fbt (rtp_session_t   *p_sess,
                                            rtp_member_t    *p_source, 
                                            rtcptype        *p_rtcp,
                                            uint32_t         bufflen,
                                            rtcp_pkt_info_t *p_pkt_info,
                                            boolean          reset_xr_stats)
{
    rtcp_rsi_info_t rsi_info;
    /*
     * Set up to add an RSI message (with a Group and Average Packet Size 
     * subreport) to the pre-specified content of the compound packet.
     */
    rsi_info.subrpt_mask = RTCP_RSI_SUBRPT_MASK_0;
    rtcp_set_rsi_subrpt_mask(&rsi_info.subrpt_mask, RTCP_RSI_GAPSB);

    return (rtcp_construct_report_ssm_rsi(p_sess, p_source, 
                                          p_rtcp, bufflen,
                                          p_pkt_info, &rsi_info,
                                          reset_xr_stats));
}
