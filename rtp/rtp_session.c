/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_session.c - Base functions for the base RTP session.
 * 
 * April 2006
 *
 * Copyright (c) 2006-2011 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#include <math.h>

#include "utils/vam_util.h"

#include "rtp_util.h"
#include "rtp_session.h"
#include "rtp_database.h"
#include "rtcp_stats_api.h"

/*
 * Extern the global instantiation of the memory mgr object.
 */
//extern rtp_memory_mgr_t rtp_memory_mgr;
static void rtp_timeout_conflict_elements (rtp_session_t *p_sess);
static rtp_session_methods_t rtp_session_methods_table;
static rtcp_handlers_t rtp_session_rtcp_handler;

/*
 * --------------------------------------------------------------------
 * Functions that are common to all types of RTP sessions. In other
 * words, these functions are part of the base RTP session class.
 * These functions are only called from the derived session objects, not
 * from outside. So for example, the rtp_create_session() here is called
 * from rtp_create_asm_session() (and other types).
 * 
 * There is one "stretch" of the base class. There is both an
 * add_hash_member function and an add_list_member function, for the 
 * two different ways of managing member caches.
 * --------------------------------------------------------------------
 */



/* Function:    rtp_sesssion_set_method
 * Description: One time (class level) initialization of various function tables
 *              This should be call one time at system initialization
 * Parameters:  None
 * Returns:     None
*/
void rtp_session_init_module()
{    
    rtp_session_set_methods(&rtp_session_methods_table);
    rtp_session_set_rtcp_handlers(&rtp_session_rtcp_handler);
    /* Initialize Memory Allocation Zone */
    /* Not done for base class */

}


/* Function:    rtp_sesssion_set_method
 * Description: Sets the function pointers in the func ptr table
 * Parameters:  table pointer to a function ptr table which gets populated by 
                this funciton
 * Returns:     None
*/
void rtp_session_set_methods(rtp_session_methods_t * table)
{
  
    table->rtp_create_member =  rtp_create_member_base;
    table->rtp_lookup_member = rtp_lookup_member_base;
    table->rtp_lookup_or_create_member =  rtp_lookup_or_create_member_base;
    table->rtp_choose_local_ssrc = rtp_choose_local_ssrc_base;
    table->rtp_delete_member = rtp_delete_member_base;
    table->rtp_remove_member_by_id = rtp_remove_member_by_id_base;
    table->rtp_cleanup_session = rtp_cleanup_session_base;
    table->rtp_get_parent_session = rtp_get_parent_session_base;
    table->rtp_create_local_source = rtp_create_local_source_base;
    table->rtp_resolve_collision_or_loop = rtp_resolve_collision_or_loop_base;
    table->rtp_update_local_ssrc = rtp_update_local_ssrc_base;
    table->rtp_new_data_source = rtp_new_data_source_base;
    table->rtp_data_source_demoted = rtp_data_source_demoted_base;
    table->rtp_recv_packet = rtp_recv_packet_base;
    table->rtcp_recv_packet = rtcp_recv_packet_base;
    table->rtcp_packet_rcvd = rtcp_packet_rcvd_base;
    table->rtcp_process_packet = rtcp_process_packet_base;
    table->rtcp_report_interval = rtcp_report_interval_base;
    table->rtcp_construct_report = rtcp_construct_report_base;
    table->rtp_update_sender_stats = rtp_update_sender_stats_base;
    table->rtp_update_receiver_stats = rtp_update_receiver_stats_base;
    table->rtp_update_stats = rtp_update_stats_base;
    table->rtcp_send_report = rtcp_send_report_base;
    table->rtcp_packet_sent = rtcp_packet_sent_base;
    table->rtp_session_timeout_slist = rtp_session_timeout_slist_base;
    table->rtp_session_timeout_glist = rtp_session_timeout_glist_base;
    table->rtp_session_timeout_transmit_report = 
        rtp_session_timeout_transmit_report_base;
 
}

/*
 * Function: rtp_create_session_base
 * Description: Create the session structure for an RTP/RTCP session.
 *              Initializes the common, base session fields. Sets up the 
 *              common, base session methods.
 * Parameters:  config   RTP configuration parameters
 *              p_sess   pointer to session being created
 * Returns:  FALSE if anything went wrong, TRUE otherwise
 * KK !! Replace boolean with specific error status.
 */
boolean rtp_create_session_base (rtp_config_t *config, 
                                 rtp_session_t *p_sess)
{
    rtp_envelope_t *env;
    rtcp_bw_cfg_t *cfg;
    rtcp_bw_info_t *info;

    p_sess->session_id = config->session_id;

    if (config->senders_cached > RTP_MAX_SENDERS_CACHED) {
        RTP_LOG_ERROR_F(p_sess, NULL,
                        "Failed to create session: "
                        "max senders (%d) too large\n",
                        config->senders_cached);
        return(FALSE);
    }

    p_sess->max_senders_cached = RTP_MAX_SENDERS_CACHED;
    p_sess->app_type = config->app_type;
    p_sess->rtp_socket = config->rtp_sock;
    p_sess->rtcp_socket = config->rtcp_sock;
    p_sess->rtcp_rsize = config->rtcp_rsize;
    env = &p_sess->send_addrs;
    env->dst_addr = config->rtcp_dst_addr;
    env->dst_port = config->rtcp_dst_port;
    if (!rtp_get_local_addr(p_sess->rtcp_socket, 
                            &env->src_addr,
                            &env->src_port)) {
        RTP_LOG_DEBUG_F(p_sess, NULL,
                        "Create session warning: "
                        "missing info for rtcp socket %d\n",
                        config->rtcp_sock);
    }
    p_sess->app_ref = config->app_ref;

    if (!p_sess->rtcp_mem) {
        RTP_LOG_ERROR_F(p_sess, NULL,
                        "Failed to create session: "
                        "RTCP memory not specified\n");
        return (FALSE);
    }

    VQE_RB_INIT(&p_sess->member_tree);
    
    cfg = &config->rtcp_bw_cfg;
    info = &p_sess->rtcp_bw;
    rtcp_set_bw_info(cfg, info);
    rtcp_jitter_init(&p_sess->intvl_jitter);

    p_sess->rtcp_xr_cfg = config->rtcp_xr_cfg;

    p_sess->rtcp_handler = &rtp_session_rtcp_handler;
    VQE_TAILQ_INIT(&(p_sess->senders_list));
    VQE_TAILQ_INIT(&(p_sess->garbage_list));
    RTP_LOG_DEBUG_FV(p_sess, NULL, 
                     "Initialized base session members\n");
    return(TRUE);
}



/*
 * RTP SESSION BASE CLASS MEMBER FUNCTIONS
 */

/*
 * Function:    rtp_delete_session_base
 * Description: Frees a session.  The session will be cleaned first using
 *              rtp_cleanup_session.
 * Parameters:  p_sess  ptr to session object
 * Returns:     None 
*/
void rtp_delete_session_base (rtp_session_t **pp_sess)
{
    if ((pp_sess == NULL) || (*pp_sess == NULL)) {
        RTP_LOG_ERROR("Failed to delete session: "
                      "NULL pointer (0x%08p or 0x%08p\n",
                      pp_sess, *pp_sess ? *pp_sess : 0);
        return;
    }

    RTP_LOG_DEBUG_F((*pp_sess), NULL, "Deleted session\n");

    /*
     * Store in a temporary variable for interupt protection.
     */
    VQE_TAILQ_INIT(&((*pp_sess)->senders_list));
    VQE_TAILQ_INIT(&((*pp_sess)->garbage_list));
    /* Clear the member list */
    if ((*pp_sess)->__func_table) {
        MCALL((*pp_sess),rtp_cleanup_session);
    }
}

/*
 * Function:    rtp_cleanup_session_base
 * Description: Remove all rtp members from a session.
 * Parameters:  p_sess   ptr to session object
 * Returns:     None
 */
void rtp_cleanup_session_base (rtp_session_t *p_sess) 
{
    rtp_member_t *p_source, *p_next;

    for (p_source = rtp_first_member(p_sess); p_source; p_source = p_next) {
        p_next = rtp_next_member(p_sess, p_source);
        RTP_LOG_DEBUG_FV(p_sess, p_source, "Cleanup session: delete member\n");
        MCALL(p_sess, rtp_delete_member, &p_source, TRUE);
    }
}

/*
 * rtp_get_parent_session_base
 *
 * Returns a pointer to the "parent" session, if the input session is 
 * a "child" session. 
 *
 * Note that statistics for "child" sessions are maintained both
 * in each child session, and (in the aggregate) in a structure allocated 
 * by the parent session.
 *
 * In the base class, there is no parent/child relationship between
 * sessions. Application-level classes may set up such a relationship
 * between sessions as follows:
 *
 * For "parent" sessions: 
 * . include an rtcp_session_stats_t structure in the extended session class, 
 *   for the purpose of holding aggregated statistics of all child sessions. 
 * . pass a pointer to this structure at session creation time, in the
 *   rtp_config_t structure.
 * For "child" sessions:
 * . override the rtp_get_parent_session method, and have it return the
 *   parent session associated with an input child session.
 *
 */
rtp_session_t *rtp_get_parent_session_base (rtp_session_t *p_sess)
{
    return (NULL);
}

/*
 * Function:    rtp_member_move_to_head
 * Description: Move a member to head of garbage list.
 * Parameters:  p_sess     pointer to session object
 *              p_member   pointer to member object
 * Returns:     None
 */
static void rtp_member_move_to_head (rtp_session_t *p_sess,
                                     rtp_member_t  *p_member)
{

  VQE_TAILQ_REMOVE(&(p_sess->garbage_list),p_member,p_member_chain);
  VQE_TAILQ_INSERT_HEAD(&(p_sess->garbage_list),p_member,p_member_chain);
}

/*
 * Function:    rtp_create_member_base
 * Description: Fill in and add new member into hash table (the base 
 *              memory cache).
 * Parameters:  p_sess     pointer to sesison object
 *              ssrc       ssrc of member
 *              src_addr   src IP address of the member
 *              p_cname    CNAME of the member
 *              p_member   **pointer to member being created
 * Returns:     RTP_SUCCESS,
 *              RTP_SSRC_EXISTS
 *              RTP_MEMBER_RESOURCE_FAIL
 */

uint32_t rtp_create_member_base (rtp_session_t *p_sess,
                                 rtp_member_id_t *p_member_id,
                                 rtp_member_t **pp_member)  
{
    char *s;
    rtp_member_t *p_source;
    char srcaddr[INET_ADDRSTRLEN];

    /* If *p_member is valid, we assume that the member has been allocated before,
     * presumably in a child constructor. Only do member allocation when *pp_member is NULL.
     */
    if (*pp_member == NULL) {
        /*
         * Allocate and initialize the data for this member.
         */
        p_source = rtcp_new_member(p_sess->rtcp_mem, p_member_id->subtype);
        if (!p_source) {
            RTP_LOG_ERROR_F(p_sess, NULL, 
                            "Failed to create member for CNAME %s: "
                            "no member resource\n",
                            p_member_id->cname ? 
                            p_member_id->cname : "<none>");
            return(RTP_MEMBER_RESOURCE_FAIL);
        }
        memset(p_source, 0, sizeof(rtp_member_t));
        *pp_member = p_source;
    }

    (*pp_member)->sender_info = rtcp_new_object(p_sess->rtcp_mem,
                                                RTCP_OBJ_SENDER_INFO);
    if (!(*pp_member)->sender_info) {
        rtcp_delete_member(p_sess->rtcp_mem, (*pp_member)->subtype, *pp_member);
        *pp_member = NULL;
        RTP_LOG_ERROR_F(p_sess, NULL, 
                        "Failed to create member for CNAME %s: "
                        "no sender info resource\n",
                        p_member_id->cname ? p_member_id->cname : "<none>");
        return(RTP_MEMBER_RESOURCE_FAIL);
    }

    GET_TIMESTAMP((*pp_member)->rcv_ctrl_ts);
    (*pp_member)->ssrc = p_member_id->ssrc;
    (*pp_member)->subtype = p_member_id->subtype;

    if (p_member_id->type == RTP_SMEMBER_ID_RTP_DATA) {
        (*pp_member)->rtp_src_addr = p_member_id->src_addr;
        (*pp_member)->rtp_src_port = p_member_id->src_port;
    } else {
        (*pp_member)->rtcp_src_addr = p_member_id->src_addr;
        (*pp_member)->rtcp_src_port = p_member_id->src_port;
        if (!p_member_id->cname) {
            rtcp_delete_member(p_sess->rtcp_mem, 
                               (*pp_member)->subtype, 
                               *pp_member);
            *pp_member = NULL;
            RTP_LOG_ERROR_F(p_sess, NULL,
                            "Failed to create member: "
                            "No CNAME for RTCP candidate "
                            "from %s:%u with SSRC 0x%08x\n",
                            uint32_ntoa_r(p_member_id->src_addr,
                                          srcaddr, sizeof(srcaddr)),
                            ntohs(p_member_id->src_port),
                            p_member_id->ssrc);
            return(RTP_MEMBER_RESOURCE_FAIL);
        }    
    }
    if (p_member_id->type == RTP_RMEMBER_ID_RTCP_DATA) {
        (*pp_member)->flags = RTP_MEMBER_FLAG_RCVR_ONLY;
    } else {
        (*pp_member)->flags = 0;
    }

    (*pp_member)->pos = -1;     /* Any new entry is assumed to be a receiver */
    if (p_member_id->cname != NULL) {
        (*pp_member)->sdes[RTCP_SDES_CNAME] = rtcp_new_object(p_sess->rtcp_mem,
                                                              RTCP_OBJ_SDES);
        if (!((*pp_member)->sdes[RTCP_SDES_CNAME])) {
            rtcp_delete_object(p_sess->rtcp_mem, 
                               RTCP_OBJ_SENDER_INFO, 
                               (*pp_member)->sender_info);
            rtcp_delete_member(p_sess->rtcp_mem, 
                               (*pp_member)->subtype,
                               *pp_member);
            *pp_member = NULL;
            RTP_LOG_ERROR_F(p_sess, NULL,
                            "Failed to create member for CNAME %s: "
                            "no SDES resource\n",
                            p_member_id->cname ? 
                            p_member_id->cname : "<none>");
            return(RTP_MEMBER_RESOURCE_FAIL);
        } else {
            s = (*pp_member)->sdes[RTCP_SDES_CNAME];
            strncpy(s, p_member_id->cname, RTCP_MAX_CNAME + 1);
            s[RTCP_MAX_CNAME] = '\0';
        }
    }

    RTP_LOG_DEBUG_FV(p_sess, (*pp_member),
                     "Initialized member: CNAME %s\n",
                     p_member_id->cname ? p_member_id->cname : "<none>");

    if (!rtp_add_member(p_sess, *pp_member)) {
        
        /* 
         * Fail if asked to add same member (SSRC+CNAME) twice. There's no 
         * duplicate resolution here, it's supposed to already have 
         * been done.
         */
        if ((*pp_member)->sdes[RTCP_SDES_CNAME]) {
            rtcp_delete_object(p_sess->rtcp_mem, 
                               RTCP_OBJ_SDES,
                               (*pp_member)->sdes[RTCP_SDES_CNAME]);
        }
        rtcp_delete_object(p_sess->rtcp_mem,
                           RTCP_OBJ_SENDER_INFO, 
                           (*pp_member)->sender_info);
        rtcp_delete_member(p_sess->rtcp_mem,
                           (*pp_member)->subtype,
                           *pp_member);
        *pp_member = NULL;
        return (RTP_SSRC_EXISTS);
    }
    /* Member is also added to the head of garbage list */
    VQE_TAILQ_INSERT_HEAD(&(p_sess->garbage_list),*pp_member,p_member_chain);

    /* Keep running total for bw calcs, debugging, etc */
    p_sess->rtcp_nmembers++;

    RTP_LOG_DEBUG_F(p_sess, (*pp_member),
                    "Added member: SSRC 0x%08x CNAME %s\n",
                    (*pp_member)->ssrc,
                    (*pp_member)->sdes[RTCP_SDES_CNAME] ?
                    (*pp_member)->sdes[RTCP_SDES_CNAME] : "<none>");
    RTP_LOG_DEBUG_F(p_sess, (*pp_member),
                    "Session has %u members, %u senders\n",
                    p_sess->rtcp_nmembers, p_sess->rtcp_nsenders);

    /*
     * Always check to timeout conflict addresses list, whenever a new
     * member is created
     */
    rtp_timeout_conflict_elements(p_sess); 


    return(RTP_SUCCESS);
}

/*
 * rtp_member_cname_match
 *
 * Determine if a member's CNAME matches an input CNAME.
 *
 * Parameters:  cname       -> specified CNAME (null-terminated string)
 *              member      -> ptr to per-member data
 *
 * Returns:     TRUE if CNAMEs match, else FALSE.
 */

static inline boolean 
rtp_member_cname_match (char *cname, rtp_member_t *member) 
{
    return (rtp_cmp_cname(cname, member->sdes[RTCP_SDES_CNAME]) == 0);
}

/* 
 * rtp_set_conflict_info
 *
 * Put a member into the conflict info.
 *
 * Parameters:  conflict     -> ptr to conflict info
 *              cname          -> CNAME (null-terminated string 
 *                                       from RTCP packet, or NULL)
 *              member         -> ptr to per-member data
 */

static void 
rtp_set_conflict_info (rtp_session_t *sess,
                       rtp_conflict_info_t *conflict,
                       char *cname,
                       rtp_member_t *member)
{
    rtp_conflict_type_t type;
    boolean full;   /* if TRUE, both SSRC and CNAME match, 
                       but member is "receiver only" and packet info is not,
                       or vice versa; if FALSE, only SSRCs match */
    boolean local;  /* if TRUE, member is the local participant;
                       if FALSE, member is a remote participant */
    boolean rcvr;   /* if TRUE, member may only receive data;
                       if FALSE, member may send and/or receive */

    if (!conflict) {
        return;
    }

    full = rtp_member_cname_match(cname, member);
    local = (member == sess->rtp_local_source) ;
    rcvr = (member->flags & RTP_MEMBER_FLAG_RCVR_ONLY) ? TRUE : FALSE;

    if (full) {
        /* full conflict, i.e., both SSRC and CNAME match */
        if (local) {
            type = rcvr ? 
                RTP_CONFLICT_FULL_LOCAL_RCVR : 
                RTP_CONFLICT_FULL_LOCAL_SNDR;
        } else {
            type = rcvr ? 
                RTP_CONFLICT_FULL_REMOTE_RCVR : 
                RTP_CONFLICT_FULL_REMOTE_SNDR;
        }
    } else {
        /* partial conflict, i.e., only SSRC matches */
        if (local) {
            type = rcvr ? 
                RTP_CONFLICT_PARTIAL_LOCAL_RCVR : 
                RTP_CONFLICT_PARTIAL_LOCAL_SNDR;
        } else {
            type = rcvr ? 
                RTP_CONFLICT_PARTIAL_REMOTE_RCVR : 
                RTP_CONFLICT_PARTIAL_REMOTE_SNDR;
        }
    }

    if (!conflict->cmember[type]) {
        conflict->cmember[type] = member;
        conflict->num_conflicts++;
    }
}

/*
 * rtp_lookup_smember_by_rtp_data
 *
 * Look up a (sending) member with information from an RTP packet.
 *
 * Parameters:  p_sess      -> ptr to per-session data.
 *              p_member_id -> member identification data, 
 *                             chiefly from an RTP packet.
 *              p_conflict  -> information on conflicting members, if any
 *
 * Returns:     a ptr to the matching member (if found), else NULL.
 */
static rtp_member_t *
rtp_lookup_smember_by_rtp_data (rtp_session_t *p_sess,
                                rtp_member_id_t *p_member_id,
                                rtp_conflict_info_t *p_conflict)
{
    rtp_member_t *member = NULL;
    rtp_member_t *match = NULL;
    boolean conflict = FALSE;

    /*
     * iterate over all members with the specified SSRC;
     * nearly all of the time, there will be at most one such member.
     */
    for (member = rtp_first_member_with_ssrc(p_sess, p_member_id->ssrc) ;
         member ;
         member = rtp_next_member_with_ssrc(p_sess, member)) {
        if (!(member->flags & RTP_MEMBER_FLAG_RCVR_ONLY)) {
            /*
             * This is NOT a "receiver only" member, so it's a potential match;
             * note that we maintain only one sending member per SSRC value.
             */
            if (!member->rtp_src_addr) {
                /*
                 * This member doesn't have RTP source transport address info;
                 * This can happen if an RTCP message arrives from the member,
                 * before RTP data arrives; just save the information.
                 */
                member->rtp_src_addr = p_member_id->src_addr;
                member->rtp_src_port = p_member_id->src_port;
            }
            if (p_member_id->src_addr == member->rtp_src_addr) {
                /* 
                 * The RTP source address from the packet matches that
                 * of the member: we have a match.
                 */
                match = member;
                /*
                 * $$$ in the future, we should only declare a match
                 * if both source address and port from the RTP data
                 * match that of the member; however, because of current
                 * limitations in how the application deals with a source
                 * change, we assume a change in source port is a restart
                 * at the sender, on a new source port, and we simply record
                 * the latest source port seen. But if there are in fact two
                 * senders at the same source address and the same SSRC,
                 * but different source ports, things won't work.
                 */
                if (p_member_id->src_port != member->rtp_src_port) {
                    /* $$$ log something */
                    member->rtp_src_port = p_member_id->src_port;
                }
            } else {
                conflict = TRUE;
            }
        } else {
            /* this is a "receiver only" member */
            conflict = TRUE;
        }
        if (conflict) {
            rtp_set_conflict_info(p_sess, p_conflict, 
                                  NULL /* no CNAME */, member);
            conflict = FALSE;
        }
    }

    return (match);
}

/*
 * rtp_lookup_smember_by_rtcp_data
 *
 * Look up a (sending) member with information from an RTCP packet.
 *
 * Parameters:  p_sess      -> The current RTP session.
 *              p_member_id -> Member identification data, from an RTCP packet.
 *              p_conflict  -> information on conflicting members, if any
 *
 * Returns:     a ptr to the matching member, else NULL if none found.
 */
static rtp_member_t *
rtp_lookup_smember_by_rtcp_data (rtp_session_t *p_sess,
                                 rtp_member_id_t *p_member_id,
                                 rtp_conflict_info_t *p_conflict)
{
    rtp_member_t *member = NULL;
    rtp_member_t *match = NULL;
    boolean conflict = FALSE;

    /*
     * iterate over all members with the specified SSRC;
     * nearly all of the time, there will be at most one such member.
     */
    for (member = rtp_first_member_with_ssrc(p_sess, p_member_id->ssrc) ;
         member ;
         member = rtp_next_member_with_ssrc(p_sess, member)) {
        if (!(member->flags & RTP_MEMBER_FLAG_RCVR_ONLY)) {
            /*
             * This is NOT a "receiver only" member, so it's a potential match;
             * note that we maintain only one sending member with this SSRC.
             */
            if (!member->rtcp_src_addr) {
                /*
                 * This member doesn't have RTCP source transport address info;
                 * This happens if an RTP packet arrives from the member,
                 * before an RTCP packet arrives. 
                 * Save the source transport address, and update the member
                 * with the CNAME.
                 */
                member->rtcp_src_addr = p_member_id->src_addr;
                member->rtcp_src_port = p_member_id->src_port;
                (void)rtp_update_member_cname(p_sess, member, p_member_id->cname);
            }
            if (p_member_id->src_addr == member->rtcp_src_addr &&
                rtp_member_cname_match(p_member_id->cname, member)) {
                /* 
                 * The RTCP source address and CNAME from the packet matches 
                 * that of the member: we have a match.
                 */
                match = member;
                /*
                 * $$$ in the future, we should only declare a match
                 * if source port from the RTCP packet matches that
                 * of the member, as well; however, because of current
                 * limitations in how the application deals with a source
                 * change, we assume a change in source port is a restart
                 * at the sender, on a new source port, and we simply record
                 * the latest source port seen. But if there are in fact two
                 * senders at the same source address with the same SSRC and
                 * CNAME, but different source ports, things won't work.
                 */
                if (p_member_id->src_port != member->rtcp_src_port) {
                    /* $$$ log something */
                    member->rtcp_src_port = p_member_id->src_port;
                }
            } else {
                conflict = TRUE;
            }
        } else {
            /* 
             * there is an SSRC conflict between the sending participant
             * represented by the input RTCP information, and an existing
             * "receiver only" member.
             */
            conflict = TRUE;
        }
        if (conflict) {
            rtp_set_conflict_info(p_sess, p_conflict, 
                                  p_member_id->cname, member);
            conflict = FALSE;
        }
    }

    return (match);
}

/*
 * rtp_lookup_rmember_by_rtcp_data
 *
 * Look up a "receiver only" member with information from an RTCP packet.
 *
 * Parameters:  p_sess      -> The current RTP session.
 *              p_member_id -> Member identification data, 
 *                             chiefly from an RTCP packet.
 *              pp_member   -> the matching member, if found;
 *                             else *pp_member is NULL.
 *              p_conflict  -> information on conflicting members, if any
 *
 * Returns:     a ptr to the matching member (if found), else NULL.
 */
static rtp_member_t *
rtp_lookup_rmember_by_rtcp_data (rtp_session_t *p_sess,
                                 rtp_member_id_t *p_member_id,
                                 rtp_conflict_info_t *p_conflict)
{
    rtp_member_t *member = NULL;
    rtp_member_t *match = NULL;
    boolean conflict = FALSE;

    /*
     * iterate over all members with the specified SSRC;
     * nearly all of the time, there will be at most one such member.
     */
    for (member = rtp_first_member_with_ssrc(p_sess, p_member_id->ssrc) ;
         member ;
         member = rtp_next_member_with_ssrc(p_sess, member)) {
        if (member->flags & RTP_MEMBER_FLAG_RCVR_ONLY &&
            rtp_member_cname_match(p_member_id->cname, member)) {
            /* 
             * This is a "receive only" member, and the CNAMEs match:
             * we have a match. We don't require a match on source transport
             * address, since if the source is behind a NAT device, the 
             * publicly visible source transport address may change due to 
             * a change in NAT bindings; we just record the last source 
             * transport address seen.
             */
            match = member;
            if (p_member_id->src_addr != member->rtcp_src_addr ||
                p_member_id->src_port != member->rtcp_src_port) {
                /* $$$ log something? */
                member->rtcp_src_addr = p_member_id->src_addr;
                member->rtcp_src_port = p_member_id->src_port;
            }
        } else {
            conflict = TRUE;
        } 
        if (conflict) {
            rtp_set_conflict_info(p_sess, p_conflict, 
                                  p_member_id->cname, member);
            conflict = FALSE;
        }
    }

    return (match);
}

/*
 * Function:    rtp_lookup_member_base
 * Description: Find the member (if any) associated with the 
 *              input member id information.
 *              Return information on any "conflicting" members, 
 *              i.e., those with the same SSRC, but which may differ in
 *              CNAME or other attributes.
 *
 * Parameters:  p_sess      -> The current RTP session.
 *              p_member_id -> Member identification data, 
 *                             chiefly from an RTP or RTCP packet.
 *              pp_member   -> the matching member, if found;
 *                             else *pp_member is NULL.
 *              p_conflict  -> information on conflicting members, if any
 *
 * Returns:     RTP_SUCCESS               -> matching member is *pp_member
 *              RTP_MEMBER_DOES_NOT_EXIST -> matching member not found
 *              RTP_MEMBER_IS_DUP         -> conflicting members found
 *
 * Conflicts should be resolved by passing the member id and conflict 
 * information to the rtp_resolve_collision_or_loop method.
 * 
 */
rtp_error_t rtp_lookup_member_base (rtp_session_t *p_sess,
                                    rtp_member_id_t *p_member_id,
                                    rtp_member_t **pp_member,
                                    rtp_conflict_info_t *p_conflict)
{
    rtp_error_t status;
    rtp_member_t *match = NULL;
    rtp_conflict_info_t info;

    if (!(p_member_id && p_sess)) {
        return (RTP_MEMBER_DOES_NOT_EXIST);
    }

    (void)memset(&info, 0, sizeof(rtp_conflict_info_t));

    switch (p_member_id->type) {
    case RTP_SMEMBER_ID_RTP_DATA:
        match = rtp_lookup_smember_by_rtp_data(p_sess, p_member_id, &info);
        break;
    case RTP_RMEMBER_ID_RTCP_DATA:
        match = rtp_lookup_rmember_by_rtcp_data(p_sess, p_member_id, &info);
        break;
    case RTP_SMEMBER_ID_RTCP_DATA:
    default:
        match = rtp_lookup_smember_by_rtcp_data(p_sess, p_member_id, &info);
        break;
    }

    if (match) {
        status = RTP_SUCCESS;
    } else if (info.num_conflicts) {
        status = RTP_MEMBER_IS_DUP;
    } else {
        status = RTP_MEMBER_DOES_NOT_EXIST;
    }

    if (pp_member) {
        *pp_member = match;
    }
    if (p_conflict) {
        *p_conflict = info;
    }

    return (status);
}

/*
 * rtp_lookup_or_create_member_base
 *
 * Look up a member via information from an RTP or RTCP packet.
 * If found, return the member, otherwise create a new member.
 *
 * Parameters:  p_sess      -> ptr to per-session data.
 *              p_member_id -> id info from an RTP or RTCP packet.
 *              pp_member   -> the found (or created) member;
 *                             NULL if not found and could not be created.
 * Returns:     RTP_SUCCESS       -> member was found or created.
 *              RTP_SSRC_EXISTS   -> conflicting member exists
 *              <other rtp_error_t value>  -> specifies creation failure
 */
rtp_error_t rtp_lookup_or_create_member_base (rtp_session_t *p_sess,
                                              rtp_member_id_t *p_member_id,
                                              rtp_member_t  **pp_member)
{
     rtp_error_t status = RTP_MEMBER_DOES_NOT_EXIST;
     boolean ok_to_create = FALSE;
     rtp_conflict_info_t info;

     if (!(p_sess && p_member_id && pp_member)) {
         return (status);
     }

     /* 
      * See if the member already exists:
      * detect conflicting members during the search.
      */
     status = MCALL(p_sess, rtp_lookup_member, p_member_id, pp_member, &info);

     switch (status) {
     case RTP_SUCCESS:
         /* member already exists, return it. */
         break;
     case RTP_MEMBER_DOES_NOT_EXIST:
         /* matching member does not exist; create a new one */
         ok_to_create = TRUE;
         break;
     case RTP_MEMBER_IS_DUP:
         /* conflicting members already exist: try to resolve the collision. */
         ok_to_create = MCALL(p_sess, rtp_resolve_collision_or_loop, 
                              p_member_id, &info);
         if (!ok_to_create) {
             status = RTP_SSRC_EXISTS;
         }
         break;
     default:
         /* just return other errors */
         break;
     }

     if (ok_to_create) {
         status = MCALL(p_sess, rtp_create_member, p_member_id, pp_member);
     }
     
     return (status);
}

#ifdef USE_CONFLICT_ELEMENT
/*
 * $$$ conlict element code will be removed when we're certain
 * there's no need to maintain this info (currently, it's not)
 */

/*
 * Function:     rtp_lookup_conflict_element
 * Description:  Find conflict entry for loop/collisions.
 * Parameters:   p_sess  ptr to session object
 *               src_addr src address of member
 * Returns;      ptr to conflict entry if match found
 */
static rtp_conflict_t *rtp_lookup_conflict_element (rtp_session_t *p_sess,
                                                    ipaddrtype    src_addr)
{
    int i;
    rtp_conflict_t *cflt = NULL;
    
    for (i = 0; i < RTP_MAX_CONFLICT_ITEMS; i++) {
        if (p_sess->rtp_conflict_table[i].addr == src_addr) {
            cflt = (rtp_conflict_t *) &(p_sess->rtp_conflict_table[i]);
            break;
        }
    }
    return(cflt);
}

/*
 * Function:     rtp_create_conflict_element
 * Description:  Create conflict entry for loop/collisions.
 * Parameters:   p_sess  ptr to session object
 *               src_addr src address of member
 * Returns;      None
 */
static void rtp_create_conflict_element (rtp_session_t *p_sess,
                                         ipaddrtype    src_addr)
{
    rtp_conflict_t *cflt;
    int i;
    
    cflt = rtp_lookup_conflict_element(p_sess, src_addr);
    if (cflt) {
        cflt->addr = src_addr;
        GET_TIMESTAMP(cflt->ts);
    } else {

        /*
         * New entry with src_addr and timestamp.
         */
        for (i = 0; i < RTP_MAX_CONFLICT_ITEMS; i++) {
            if (!p_sess->rtp_conflict_table[i].addr) {
                p_sess->rtp_conflict_table[i].addr = src_addr;
                GET_TIMESTAMP(p_sess->rtp_conflict_table[i].ts);
                break;
            }
        }
    }    
}
#endif /* ifdef USE_CONFLICT_ELEMENTS */

/*
 * Function:     rtp_timeout_conflict_elements
 * Description:  Remove timed out conflict entries.
 * Parameters:   p_sess  ptr to session object
 * Returns:      None
 */
static void rtp_timeout_conflict_elements (rtp_session_t *p_sess)
{
    int i;
    
    for (i = 0; i < RTP_MAX_CONFLICT_ITEMS; i++) {

        /*    
         * if (ELAPSED_TIME(p_sess->rtp_conflict_table[i].ts) > ONEMIN) {
         */
        if (TIME_CMP_R(gt,
                       TIME_SUB_A_A(p_sess->rtp_conflict_table[i].ts, 
                                    get_sys_time()),
                       SECS(60))) {


            p_sess->rtp_conflict_table[i].addr = 0;
        }
    }
}

/*
 * rtp_change_local_ssrc
 *
 * Resolve a conflict with the local member by changing its SSRC.
 *
 * Parameters:  p_sess        -> ptr to per-session data
 *              p_member_id   -> ptr to info from RTP or RTCP packet
 *              member        -> ptr to per-member data
 */
static void
rtp_change_local_ssrc (rtp_session_t *p_sess,
                       rtp_member_id_t *p_member_id,
                       rtp_member_t *member)
{
    uint32_t new_ssrc;
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t bye;

    RTP_LOG_ERROR_F(p_sess, member, 
                    "Collision with local SSRC 0x%08x\n", 
                    p_member_id->ssrc);

    p_sess->collision_own++;

    /* send a BYE with the current SSRC */
    rtcp_init_pkt_info(&pkt_info);
    rtcp_init_msg_info(&bye);
    rtcp_set_pkt_info(&pkt_info, RTCP_BYE, &bye);
    MCALL(p_sess, 
          rtcp_send_report,
          member, 
          &pkt_info, 
          TRUE,  /* re-schedule next RTCP report */
          TRUE); /* reset RTCP XR stats */
            
    /* get a new SSRC that's not in use, and use it for the local member */
    new_ssrc = MCALL(p_sess, rtp_choose_local_ssrc);
    if (!rtp_update_member_ssrc(p_sess, member, new_ssrc)) {
        RTP_LOG_ERROR_F(p_sess, member,
                        "Failed to update local SSRC to 0x%08x\n", 
                        new_ssrc);
    }
}

/*
 * rtp_count_3rd_party_conflict
 *
 * Record a conflict between a potential member and a remote member,
 * as a third-party collision or "loop".
 *
 * Parameters:  p_sess        -> ptr to per-session data
 *              p_member_id   -> ptr to info from RTP or RTCP packet
 *              full_conflict -> TRUE if both SSRC and CNAME match,
 *                               else FALSE.
 *              member        -> ptr to per-member data
 */
static void
rtp_count_3rd_party_conflict (rtp_session_t *p_sess,
                              rtp_member_id_t *p_member_id,
                              boolean full_conflict,
                              rtp_member_t *member)
{
    if (p_member_id->type != RTP_SMEMBER_ID_RTP_DATA) {
        /* 
         * only log something if it's NOT triggered by an RTP packet,
         * i.e., only if the conflict is triggered by an RTCP packet,
         * since logging something for each RTP conflict might overload
         * the logging system.
         */
        RTP_LOG_ERROR_F(p_sess, member,
                        "Third-party collision with SSRC 0x%08x\n",
                        p_member_id->ssrc);
    }
    if (full_conflict) {
        /*
         * in an SSM context, this is unlikely to be a "loop",
         * but we track full vs. partial conflicts in different
         * counters, anyways.
         */
        p_sess->loop_third_party++;
    } else {
        p_sess->collision_third_party++;
    }
}

/*
 * rtp_report_conflict
 *
 * Report an SSRC collision by adding the SSRC to a list for the
 * SSRC Collision subreport of an RSI message that will be sent
 * in the next RTCP report.
 *
 * Parameters:  p_sess        -> ptr to per-session data
 *              p_member_id   -> ptr to info from RTP or RTCP packet
 *              member        -> ptr to per-member data
 */
static void
rtp_report_conflict (rtp_session_t *p_sess,
                     rtp_member_id_t *p_member_id,
                     rtp_member_t *member)
{
    /* $$$ this is for future development: this will be done only
       by some application-level classes, so this needs to be,
       or to invoke, a method whose base class implementation will
       do nothing, but which will be overridden by those classes
       that need to send the collision info, namely ssm_rsi_fbt and
       ssm_rsi_source */
}

/*
 * rtp_delete_conflicting_member
 *
 * Resolve a conflict between a (potentially) sending member's SSRC,
 * and that of a "receive only" member, by deleting knowledge of the
 * "receive only" member. This allows us to proceed with processing
 * data from the sender; the "receive only" member should soon be changing
 * its SSRC, anyways, when it detects the conflict with the sending
 * member.
 *
 * Parameters:  p_sess        -> ptr to per-session data
 *              p_member_id   -> ptr to info from RTP or RTCP packet
 *              member        -> ptr to per-member data
 */
static void
rtp_delete_conflicting_member (rtp_session_t *p_sess,
                               rtp_member_id_t *p_member_id,
                               rtp_member_t *member)
{
    rtp_member_t *member_to_delete = member;

    RTP_LOG_ERROR_F(p_sess, member,
                    "Third-party SSRC collision between sending "
                    "and receive-only members: SSRC=0x%08x\n",
                    p_member_id->ssrc);

    RTP_LOG_ERROR_F(p_sess, member,
                    "Deleting receive-only member with CNAME %s\n",
                    member->sdes[RTCP_SDES_CNAME] ? 
                    member->sdes[RTCP_SDES_CNAME] : "<none>");

    MCALL(p_sess, rtp_delete_member, &member_to_delete, TRUE);
}

/*
 * rtp_resolve_rtp_conflict
 *
 * Resolve a conflict between a potential member represented by RTP data,
 * and an existing member.
 *
 * Parameters:  p_sess        -> ptr to per-session data
 *              type          -> conflict type
 *              p_member_id   -> ptr to info from RTP or RTCP packet
 *              p_conflict    -> info on conflicting members.
 *
 * Returns:     TRUE, if the collision was resolved in such a way as to
 *                    allow creation of a member for the p_member_id info.
 *              FALSE, if member creation for p_member_id should NOT occur.
 */
static boolean 
rtp_resolve_rtp_conflict (rtp_session_t *p_sess,
                          rtp_conflict_type_t type,
                          rtp_member_id_t *p_member_id,
                          rtp_member_t *member)
{
    boolean ok_to_create = FALSE;

    switch (type) {
    case RTP_CONFLICT_PARTIAL_LOCAL_SNDR:
    case RTP_CONFLICT_FULL_LOCAL_SNDR:
        /* 
         * these aren't possible in an SSM context:
         * if we have a local sending member, it's the only sender;
         * we're not listening for any other RTP data 
         */
        /* $$$ log something? */
        ok_to_create = FALSE;
        break;

    case RTP_CONFLICT_FULL_LOCAL_RCVR:
    case RTP_CONFLICT_FULL_REMOTE_RCVR:
        /* 
         * these aren't possible: a "receive only" member
         * will always have a CNAME, and our RTP data will not,
         * so these "full" conflicts aren't possible
         */
        /* $$$ log something? */
        ok_to_create = FALSE;
        break;

    case RTP_CONFLICT_PARTIAL_LOCAL_RCVR:
        rtp_change_local_ssrc(p_sess, p_member_id, member);
        ok_to_create = TRUE;
        break;

    case RTP_CONFLICT_PARTIAL_REMOTE_SNDR:
    case RTP_CONFLICT_FULL_REMOTE_SNDR:
        rtp_count_3rd_party_conflict(p_sess,
                                     p_member_id, 
                                     type == RTP_CONFLICT_FULL_REMOTE_SNDR ?
                                     TRUE : FALSE, /* full or partial */
                                     member);
        rtp_report_conflict(p_sess, p_member_id, member);
        ok_to_create = FALSE;
        break;

    case RTP_CONFLICT_PARTIAL_REMOTE_RCVR:
        rtp_count_3rd_party_conflict(p_sess,
                                     p_member_id, 
                                     FALSE, /* partial conflict */
                                     member);
        rtp_report_conflict(p_sess, p_member_id, member);
        ok_to_create = TRUE;
        break;
    }
     
    return (ok_to_create);
}

/*
 * rtp_resolve_ro_rtcp_conflict
 *
 * Resolve a conflict between a potential "receive only" member represented
 * by RTCP data, and an existing member.
 *
 * Parameters:  p_sess        -> ptr to per-session data
 *              p_member_id   -> ptr to info from RTP or RTCP packet
 *              p_conflict    -> info on conflicting members.
 *
 * Returns:     TRUE, if the collision was resolved in such a way as to
 *                    allow creation of a member for the p_member_id info.
 *              FALSE, if member creation for p_member_id should NOT occur.
 */
static boolean 
rtp_resolve_ro_rtcp_conflict (rtp_session_t *p_sess,
                              rtp_conflict_type_t type,
                              rtp_member_id_t *p_member_id,
                              rtp_member_t *member)
{
    boolean ok_to_create = FALSE;

    switch (type) {
    case RTP_CONFLICT_PARTIAL_LOCAL_SNDR:
        rtp_report_conflict(p_sess, p_member_id, member);
        /*
         * a potential "receive only" member is using the same SSRC
         * as a sender. Since the conflict is only partial (the CNAMEs
         * of the members are different), we can and will add the
         * new "receive only" member to the database. However,
         * this receiver should detect the conflict, since it 
         * is receiving from the sender, so this "receive only" 
         * member may be short-lived (the receiver should send a BYE, and 
         * choose another SSRC).
         */
        ok_to_create = TRUE;
        break;

    case RTP_CONFLICT_FULL_LOCAL_SNDR:
        rtp_report_conflict(p_sess, p_member_id, member);
        ok_to_create = FALSE;
        break;

    case RTP_CONFLICT_PARTIAL_REMOTE_SNDR:
    case RTP_CONFLICT_PARTIAL_REMOTE_RCVR:
        rtp_count_3rd_party_conflict(p_sess,
                                     p_member_id, 
                                     FALSE, /* partial conflict */
                                     member);
        rtp_report_conflict(p_sess, p_member_id, member);
        ok_to_create = TRUE;
        break;

    case RTP_CONFLICT_FULL_REMOTE_RCVR:
    case RTP_CONFLICT_FULL_REMOTE_SNDR:
        rtp_count_3rd_party_conflict(p_sess,
                                     p_member_id, 
                                     TRUE, /* full conflict */
                                     member);
        rtp_report_conflict(p_sess, p_member_id, member);
        ok_to_create = FALSE;
        break;

    case RTP_CONFLICT_FULL_LOCAL_RCVR:
    case RTP_CONFLICT_PARTIAL_LOCAL_RCVR:
        rtp_change_local_ssrc(p_sess, p_member_id, member);
        ok_to_create = TRUE;
        break;
    }
     
    return (ok_to_create);
}

/*
 * rtp_resolve_snd_rtcp_conflict
 *
 * Resolve a conflict between a potentially sending member represented
 * by RTCP data, and an existing member or members.
 *
 * Parameters:  p_sess        -> ptr to per-session data
 *              p_member_id   -> ptr to info from RTP or RTCP packet
 *              p_conflict    -> info on conflicting members.
 *
 * Returns:     TRUE, if the collision was resolved in such a way as to
 *                    allow creation of a member for the p_member_id info.
 *              FALSE, if member creation for p_member_id should NOT occur.
 */
static boolean 
rtp_resolve_snd_rtcp_conflict (rtp_session_t *p_sess,
                               rtp_conflict_type_t type,
                               rtp_member_id_t *p_member_id,
                               rtp_member_t *member)
{
    boolean ok_to_create = FALSE;

    switch (type) {
    case RTP_CONFLICT_PARTIAL_LOCAL_SNDR:
    case RTP_CONFLICT_FULL_LOCAL_SNDR:
        /* 
         * these aren't possible in an SSM context:
         * if we have a local sending member, it's the only sender;
         * we're not listening for any other RTP data 
         */
        /* $$$ log something? */
        ok_to_create = FALSE;
        break;

    case RTP_CONFLICT_PARTIAL_REMOTE_SNDR:
        rtp_count_3rd_party_conflict(p_sess, 
                                     p_member_id, 
                                     FALSE, /* partial conflict */
                                     member);
        rtp_report_conflict(p_sess, p_member_id, member);
        ok_to_create = FALSE;
        break;

    case RTP_CONFLICT_FULL_REMOTE_SNDR:
        rtp_count_3rd_party_conflict(p_sess, 
                                     p_member_id, 
                                     TRUE, /* full conflict */
                                     member);
        rtp_report_conflict(p_sess, p_member_id, member);
        ok_to_create = FALSE;
        break;

    case RTP_CONFLICT_PARTIAL_REMOTE_RCVR:
        rtp_count_3rd_party_conflict(p_sess,
                                     p_member_id, 
                                     FALSE, /* partial conflict */
                                     member);
        rtp_report_conflict(p_sess, p_member_id, member);
        ok_to_create = TRUE;
        break;

    case RTP_CONFLICT_FULL_REMOTE_RCVR:
        rtp_delete_conflicting_member(p_sess, p_member_id, member);
        rtp_report_conflict(p_sess, p_member_id, member);
        ok_to_create = FALSE;
        break;

    case RTP_CONFLICT_FULL_LOCAL_RCVR:
    case RTP_CONFLICT_PARTIAL_LOCAL_RCVR:
        rtp_change_local_ssrc(p_sess, p_member_id, member);
        ok_to_create = TRUE;
        break;
    }
     
    return (ok_to_create);
}

/*
 * rtp_resolve_collision_or_loop_base
 *
 * Resolve SSRC collisions/loops, based on conflicting member information
 * collected by the rtp_lookup_member method. The algorithm is based on 
 * RFC 3550.
 *
 * Parameters:  p_sess        -> ptr to per-session data
 *              p_member_id   -> ptr to info from RTP or RTCP packet
 *              p_conflict    -> info on conflicting members.
 *
 * Returns:     TRUE, if the collision was resolved in such a way as to
 *                    allow creation of a member for the p_member_id info.
 *              FALSE, if member creation for p_member_id should NOT occur.
 */

boolean rtp_resolve_collision_or_loop_base (rtp_session_t *p_sess,
                                            rtp_member_id_t *p_member_id,
                                            rtp_conflict_info_t *p_conflict)
{
    int i = 0;
    rtp_member_t *member = NULL;
    boolean ok_to_create = TRUE;
    boolean (*resolve_function)(rtp_session_t *sess,
                                rtp_conflict_type_t type,
                                rtp_member_id_t *member_id,
                                rtp_member_t *member);

    if (!(p_sess && p_member_id)) {
        return (FALSE);
    }
    if (!p_conflict) {
        return (TRUE);
    }

    switch (p_member_id->type) {
    case RTP_SMEMBER_ID_RTP_DATA:
        resolve_function = rtp_resolve_rtp_conflict;
        break;
    case RTP_RMEMBER_ID_RTCP_DATA:
        resolve_function = rtp_resolve_ro_rtcp_conflict;
        break;
    case RTP_SMEMBER_ID_RTCP_DATA:
    default:
        resolve_function = rtp_resolve_snd_rtcp_conflict;
        break;
    }

    for (i = RTP_CONFLICT_MIN ; i <= RTP_CONFLICT_MAX ; i++) {
        member = p_conflict->cmember[i];
        if (member) {
            ok_to_create = ok_to_create && 
                (*resolve_function)(p_sess, i, p_member_id, member);
        }
    }
        
    return (ok_to_create);
}

/* Function:    rtp_delete_member_base
 * Description: Cleans up the member and memory allocated for the member
 * Parameters:  p_sess   pointer to session object
 *              p_source pointer to member object
 *              destroy_flag boolean to check if memory should be de-allocated
 * Returns:      None
 */
void rtp_delete_member_base ( rtp_session_t *p_sess,
                               rtp_member_t  **pp_source,
                               boolean      destroy_flag) 
{
    uint32_t i;
    rtp_member_t *p_member = *pp_source;
    rtp_source_id_t source_id;

    /* 
     * Remove member from the RB tree of members, 
     * and from the "garbage" or sender list 
     */
    rtp_remove_member(p_sess, p_member);
    p_sess->rtcp_nmembers--;
    if (p_member->rtcp_nmembers_reported) {
        p_sess->rtcp_nmembers_learned -= 
            (p_member->rtcp_nmembers_reported - 2);
    }
    if ((p_member->pos == -1) || ((p_member->pos == 0) 
                                  && (p_sess->we_sent == FALSE))) {
        VQE_TAILQ_REMOVE(&(p_sess->garbage_list),p_member,p_member_chain);
        
    } else {
        p_sess->rtp_source_bitmask &= ~(1 << p_member->pos);
        VQE_TAILQ_REMOVE(&(p_sess->senders_list),p_member,p_member_chain);
        p_sess->rtcp_nsenders--;
        source_id.ssrc = p_member->ssrc;
        source_id.src_addr = p_member->rtp_src_addr;
        source_id.src_port = p_member->rtp_src_port;
        MCALL(p_sess, rtp_data_source_demoted, &source_id, TRUE);
    }

    RTP_LOG_DEBUG_F(p_sess, p_member,
                    "Removed member: SSRC 0x%08x CNAME %s\n",
                    p_member->ssrc,
                    p_member->sdes[RTCP_SDES_CNAME] ?
                    p_member->sdes[RTCP_SDES_CNAME] : "<none>");
    RTP_LOG_DEBUG_F(p_sess, p_member,
                    "Session now has %u members, %u senders\n",
                    p_sess->rtcp_nmembers, p_sess->rtcp_nsenders);

    for (i = 1; i <= RTCP_SDES_MAX; i++) {
        if (p_member->sdes[i]) {
            rtcp_delete_object(p_sess->rtcp_mem, 
                               RTCP_OBJ_SDES,
                               p_member->sdes[i]);
            p_member->sdes[i] = NULL;
        }
    }
    rtcp_delete_object(p_sess->rtcp_mem, 
                       RTCP_OBJ_SENDER_INFO, 
                       p_member->sender_info);
    p_member->sender_info = NULL;  

    /* $$$Add reverse reconsideration per RFC 3550*/

    /*
     * Free memory for the entry if asked to.
     */
    if (destroy_flag) {
        if (p_member == p_sess->rtp_local_source) {
            p_sess->rtp_local_source = NULL;
        }
        RTP_LOG_DEBUG_F(p_sess, p_member, "Free member\n");
        rtcp_delete_member(p_sess->rtcp_mem, p_member->subtype, p_member);
        *pp_source = NULL;
    } 
}



/*
 * Function:    rtp_remove_member_by_id_base
 * Description: Remove an RTP member (by member id) from the session.
 *              Free the element if the destroy flag is set, otherwise
 *              return the element.
 * Parameters:  p_sess   pointer to session object
 *              ssrc     ssrc of the member
 *              destroy_flag boolen to check if member should be deallocated
 * Returns:     Member, if found and destroy_flag not set. Null otherwise
 */
rtp_member_t *rtp_remove_member_by_id_base (rtp_session_t *p_sess,
                                            rtp_member_id_t *p_member_id,
                                            boolean destroy_flag)
{
    rtp_member_t *p_tmp = NULL;
    rtp_error_t result;

    result = MCALL(p_sess, rtp_lookup_member,
                   p_member_id, &p_tmp, NULL);
    if (!p_tmp) {
        return NULL;
    }
    MCALL(p_sess, rtp_delete_member, &p_tmp, destroy_flag);
    return p_tmp;
}


/*
 * Function:    rtp_choose_local_ssrc_base 
 * Description: Choose a random new ssrc for the given rtp session.
 * Parameters:  p_sess   pointer to session object
 *              src_addr source IP address
 * Returns   :  Randomly computed SSRC 
 */
uint32_t rtp_choose_local_ssrc_base (rtp_session_t *p_sess)
{
    uint32_t ssrc = 0;
    rtp_member_t *member = NULL;

    do {
        ssrc = rtp_random32(RANDOM_SSRC_TYPE);
        /*
         * Check if there is a valid source present already for this ssrc.
         */
        member = rtp_first_member_with_ssrc(p_sess, ssrc);
    } while (member);  
    
    return(ssrc);
}

/*
 * Function:    rtp_create_local_source base
 * Description: This function must be called to choose an SSRC once the 
 *              rtp session is created, if we are a local sender. If we are a 
 *              monitor, then this function need not be called. This function 
 *              also creates a source element for the local member, updates the 
 *              local member variable and assigns the position for the local 
 *              sender's rr stats.
 * Parameters:  p_sess   ptr to session object
 *              src_addr src_ip address
 *              p_cname  CNAME to be used for the source
 *              local_ssrc SSRC to be used for the local member, a new one 
 *              is  allocated if this param is zero
 * Returns:     pointer to local member created
 */
rtp_member_t *rtp_create_local_source_base (rtp_session_t   *p_sess,
                                            rtp_member_id_t *p_member_id,
                                            rtp_ssrc_selection_t ssrc_type)
{
    uint32_t   status;
    rtp_member_t *p_source = NULL;
    uint32_t   interval;

    switch (ssrc_type) {
    case RTP_USE_SPECIFIED_SSRC:
        /* nothing to do: SSRC value to use is already in p_member_id->ssrc */
        break;
    case RTP_SELECT_RANDOM_SSRC:
    default:
        p_member_id->ssrc = MCALL(p_sess, rtp_choose_local_ssrc);
        break;
    }

    /*
     * Once we choose the SSRC, make sure we reserve the source bitfield for
     * sender and update the local_source variable.
     */
    status = MCALL(p_sess, rtp_create_member, p_member_id, &p_source);
    if (status != RTP_SUCCESS) {
        RTP_LOG_ERROR_F(p_sess, NULL,
                        "Failed to create local source with SSRC 0x%08x\n",
                        p_member_id->ssrc);
    }
    
    p_sess->rtp_local_source = p_source;
    /* mark the local source as initial, as it has not sent any RTCP packet
     */
    p_sess->initial = TRUE;
    p_sess->we_sent = FALSE;
    p_sess->rtcp_pmembers = 1;
    /* Note nmembers is incremented when member is created */
    /* 
     * $$$ the initial avg pkt sizes should come from rtp_config_t
     * at session creation time, since as the "probable size of the 
     * first RTCP packet that the application will later construct",
     * [RFC 3550, section 6.3.2] this value is application-specific.
     * Also, the above init, along with pkt size init and perhaps
     * initial RTCP interval calculation, probably 
     * should happen at session creation, not here.
     */
    p_sess->rtcp_stats.avg_pkt_size = D_RTCP_DFLT_AVG_PKT_SIZE;
    p_sess->rtcp_stats.avg_pkt_size_sent = D_RTCP_DFLT_AVG_PKT_SIZE;

    interval = MCALL(p_sess, rtcp_report_interval, 
                     p_sess->we_sent,
                     TRUE /* jitter the interval */ );
    
    GET_TIMESTAMP(p_sess->prev_send_ts);
   /* 
    * p_sess->next_send_ts = p_sess->prev_send_ts + interval;
    */
    p_sess->next_send_ts = TIME_ADD_A_R(p_sess->prev_send_ts,
                                        TIME_MK_R(usec,(int64_t)interval));
  
    /* $$$ Do we need to inform anybody of SSRC change ? */

    /*
     * Set the bitfield for the local source in the source-bitmask.
     */
    if (p_source) {
        p_sess->rtp_source_bitmask |= 1;
        p_source->pos = 0;
    }
    return(p_source);
}


/* Function:    rtp_update_local_ssrc - 
 * Description: Replace old_ssrc with new_ssrc for the local source.
 *              Remove the source from the hash list and add it to its
 *              new list. 
 * Parameters:  p_ses    ptr to session object
 *              old_ssrc old SSRC of local src member
 *              new_ssrc new SSRC of local src member
 * Returns:     None
 */
void rtp_update_local_ssrc_base (rtp_session_t *p_sess, uint32_t old_ssrc,
                            uint32_t new_ssrc)
{
    rtp_member_t  *p_member;
    char addr[INET_ADDRSTRLEN];

    if (!p_sess || !p_sess->rtp_local_source) {
        return;
    }

    p_member = p_sess->rtp_local_source;
    if (p_member->ssrc != old_ssrc) {
        /* old ssrc not found */
        RTP_LOG_ERROR_F(p_sess, p_member,
                        "Failed to update local SSRC: "
                        "old SSRC is 0x%08x but local SSRC is 0x%08x\n",
                        old_ssrc, new_ssrc);
        goto update_failed;
    }

    if (!rtp_update_member_ssrc(p_sess, p_member, new_ssrc)) {
        RTP_LOG_ERROR_F(p_sess, p_member,
                        "Failed to update local SSRC to 0x%08x\n",
                        new_ssrc);
        goto update_failed;
    }

    RTP_LOG_DEBUG_F(p_sess, p_member,
                    "Local SSRC updated to 0x%08x\n",
                    new_ssrc);
    return;

 update_failed:
    RTP_SYSLOG(RTCP_LOCAL_SSRC_UPDATE_FAILURE,
               p_sess->rtcp_mem && p_sess->rtcp_mem->name ?
               p_sess->rtcp_mem->name : "",
               uint32_ntoa_r(rtp_get_session_id_addr(p_sess->session_id),
                             addr, sizeof(addr)),
               ntohs(rtp_get_session_id_port(p_sess->session_id)),
               new_ssrc,
               p_member->ssrc);
    return;
}

/*
 * Function: rtp_session_add_sender
 */
static void rtp_session_add_sender(rtp_session_t *p_sess, rtp_member_t *p_member)
{
    uint32_t i;
    if (p_member->pos == -1) {
        for (i = 1; i < RTP_MAX_SENDERS; i++) {
            if (!((p_sess->rtp_source_bitmask >> i) & 1)) {
                p_sess->rtp_source_bitmask |= (1 << i);
                p_member->pos = i;
                /* This sender is marked receiver (i.e. pos=-1). So remove from 
                 * the garbage list and add to sender list */
                VQE_TAILQ_REMOVE(&(p_sess->garbage_list),
				 p_member,p_member_chain);
                VQE_TAILQ_INSERT_HEAD(&(p_sess->senders_list),
				      p_member,p_member_chain);
                p_sess->rtcp_nsenders++;
                break;
            }
        }
        RTP_LOG_DEBUG_F(p_sess, p_member,
                        "%s remote member as sender: ",
                        p_member->pos > 0 ? "Marked" : "Failed to mark",
                        "SSRC 0x%08x CNAME %s\n",
                        p_member->ssrc, 
                        p_member->sdes[RTCP_SDES_CNAME] ?
                        p_member->sdes[RTCP_SDES_CNAME] : "");
    } else if ((p_member->pos == 0) && (p_sess->we_sent == FALSE)){
        /* Changing local rcvr to local member 
         */
        RTP_LOG_DEBUG_F(p_sess, p_member,
                        "Local member is now marked as sender\n");
        VQE_TAILQ_REMOVE(&(p_sess->garbage_list),p_member,p_member_chain);
        VQE_TAILQ_INSERT_HEAD(&(p_sess->senders_list),p_member,p_member_chain);
        p_sess->rtcp_nsenders++;

    } else {
        /* member already a sender */
        return;
    }
}

/*
 * rtp_new_data_source_base
 *
 * Create a member (i.e. a remote sending member) corresponding to a new 
 * RTP data source.
 *
 * Parameters:  p_sess     ptr to session object
 *              p_srcid    ptr to 3-tuple identifying the data source
 * Returns:     RTP_SUCCESS       -- member successfully created.
 *              RTP_SSRC_EXISTS   -- member with this srcid already exists.
 *              RTP_MEMBER_IS_DUP -- conflicting member (with this SSRC) 
 *                                   already exists: member is not created.
 *              <other errors>    -- member is not created.
 * 
 * SSRC collisions will be detected and processed.
 * Some collisions in such a way that the new remote sending member can
 * be created; if this is the case, RTP_SUCCESS will be returned. 
 * However, other collisions prevent the creation of the member:
 * in this case RTP_MEMBER_IS_DUP will be returned, 
 * and no new member will be created.
 *
 * Note that this method is similar to rtp_lookup_or_create_member, but
 * returns RTP_SSRC_EXISTS when the specified member already exists.
 */
rtp_error_t rtp_new_data_source_base (rtp_session_t   *p_sess,
                                      rtp_source_id_t *p_srcid)
{
     rtp_error_t  status = RTP_MEMBER_DOES_NOT_EXIST;
     rtp_member_t *p_source = NULL;
     rtp_member_id_t member_id;
     rtp_conflict_info_t cinfo;
     boolean ok_to_create = FALSE;

     /* 
      * New data sources are channel members because they correspond
      * to a channel being received 
      */
     member_id.type = RTP_SMEMBER_ID_RTP_DATA;
     member_id.subtype = RTCP_CHANNEL_MEMBER;
     member_id.ssrc = p_srcid->ssrc;
     member_id.src_addr = p_srcid->src_addr;
     member_id.src_port = p_srcid->src_port;
     member_id.cname = NULL;

     /* 
      * First check to see if we already have a member with this SSRC 
      */
     status = MCALL(p_sess, rtp_lookup_member, 
                    &member_id, &p_source, &cinfo);

     switch (status) {
     case RTP_MEMBER_DOES_NOT_EXIST:
         /* No other member exist with same SSRC, create a new one */
         ok_to_create = TRUE;
         break;
     case RTP_MEMBER_IS_DUP:
         /* 
          * A conflicting member (with the same SSRC) is in the member list,
          * resolve the collision 
          */
         ok_to_create = MCALL(p_sess, rtp_resolve_collision_or_loop, 
                              &member_id, &cinfo);
         break;
     case RTP_SUCCESS:
         status = RTP_SSRC_EXISTS;
         break;
     default:
         break;
     }

     if (ok_to_create) {
         status = MCALL(p_sess, rtp_create_member, &member_id, &p_source);
     }

     if (status == RTP_SUCCESS || status == RTP_SSRC_EXISTS) {
         rtp_session_add_sender(p_sess, p_source);
     }
     return (status);
}

/*
 * rtp_data_source_demoted_base
 *
 * Performs post-processing on a "sending" member that has just been
 * demoted to a receiving member, or deleted, due to inactivity.
 *
 * There is no default post-processing of this event, so an
 * an application-level class must override this method
 * if post-processing is required. This is typically done if
 * the "split" RTP/RTCP processing model is used, to mark the data source
 * in such a way that if it becomes active again, RTCP will be
 * notified (through re-invocation of the rtp_new_data_source method)
 * that the source is once again a "sender".
 *
 * Applications that override both the rtp_delete_member and 
 * rtp_data_source_demoted method should be aware that 
 * rtp_data_sourced_demoted will be invoked during the deletion
 * of a sending member.
 *
 * Parameters:  p_sess     ptr to session object
 *              p_srcid    ptr to 3-tuple identifying the data source
 *              deleted    TRUE if the member is being deleted,
 *                         FALSE if the member is just being "demoted".
 */

void rtp_data_source_demoted_base (rtp_session_t   *p_sess,
                                   rtp_source_id_t *p_srcid,
                                   boolean          deleted)
{
    return;
}

/*
 * Function:   rtp_session_timeout_slist_base
 * Description: Traverse the senders list and remove senders that have been
 *              timeout. This should be called at regular intervals by the 
 *              appliation code
 * Parameters:  p_sess   ptr to session object
 * Returns:     None
 */

void rtp_session_timeout_slist_base (rtp_session_t *p_sess)
{
    rtp_member_t *p_member, *p_temp;
    abs_time_t curr_time; 
    abs_time_t      tempts;
    GET_TIMESTAMP(curr_time);
    boolean recent_rtcp_activity;
    boolean recent_rtp_activity;
    uint32_t interval;
    rtp_source_id_t source_id;

    /* 
     * Note each member in slist is traversted to check timeout. It is
     * assumed that number of senders are small enough to not cause
     * any potential hogs
     */
    VQE_TAILQ_FOREACH_REVERSE_SAFE(p_member, 
                               &(p_sess->senders_list),senders_list_t_,
                               p_member_chain, p_temp) {
        if (p_member == p_sess->rtp_local_source) {
            /* $$$ add timeout for local sender based on the timestamp of last
             * rtp packet sent */
            continue;
        }

        /* 
         * If there's been no RTP or RTCP packets received within 
         * two RTCP intervals, time out the sender. 
         */
        interval = MCALL(p_sess, rtcp_report_interval,
                         p_sess->we_sent, FALSE /* don't add jitter */);
        tempts = TIME_SUB_A_R(curr_time, 
                              TIME_MK_R(usec, (int64_t)(2 * interval)));
        recent_rtcp_activity = TIME_CMP_A(ge, p_member->rcv_ctrl_ts, tempts);
        recent_rtp_activity = TIME_CMP_A(ge, 
                                         p_member->rcv_stats.last_arr_ts_abs,
                                         tempts);
        if (!(recent_rtcp_activity || recent_rtp_activity)) {
            /*
             * Remove from senders list and add to the garbage list. $$$ Note that
             * we add it to the head of the garbage list, need to confirm if that
             * is okay?
             */
            if (p_member->pos != -1) {
                p_sess->rtp_source_bitmask &= ~(1 << p_member->pos);
                p_member->pos = -1;
            }
            RTP_LOG_DEBUG_F(p_sess, p_member,
                            "Sender timeout: SSRC 0x%08x CNAME %s\n",
                            p_member->ssrc, 
                            p_member->sdes[RTCP_SDES_CNAME] ?
                            p_member->sdes[RTCP_SDES_CNAME] : "<none>");
            p_sess->rtcp_nsenders--;
            VQE_TAILQ_REMOVE(&(p_sess->senders_list),p_member,p_member_chain);
            VQE_TAILQ_INSERT_HEAD(&(p_sess->garbage_list),p_member,p_member_chain);
            source_id.ssrc = p_member->ssrc;
            source_id.src_addr = p_member->rtp_src_addr;
            source_id.src_port = p_member->rtp_src_port;
            MCALL(p_sess, rtp_data_source_demoted, &source_id, FALSE);
        }
    }

}

/*
 * Function:    rtp_session_timeout_glist_base
 * Description: Traverse the garbage list and remove members that have 
 *              timed out. This should be called at regular intervals by the 
 *              appliation code
 *              Note This is not thread safe!
 * Parameters:  p_sess   ptr to session object
 * Returns:     None
 */
void rtp_session_timeout_glist_base (rtp_session_t *p_sess)
{
    rtp_member_t *p_member, *p_temp;
    abs_time_t curr_time;
    uint32_t         interval;
    abs_time_t      tempts;
    GET_TIMESTAMP(curr_time);

    /* This is per RFC 3550 */
    interval = MCALL(p_sess, rtcp_report_interval, 
                     FALSE /* we_sent */,
                     FALSE /* do not jitter */);
    /* 
     * Note each member in slist is traversed to check timeout. It is
     * assumed that number of senders are small enough to not cause
     * any potential hogs
     */
    VQE_TAILQ_FOREACH_REVERSE_SAFE(p_member, &(p_sess->garbage_list),
                               garbage_list_t_,
                               p_member_chain, p_temp) {
        if (p_member == p_sess->rtp_local_source) {
            continue;
        }
        /* RTCP received from the this member within last M intervals ? 
         * check if ((p_member->rcv_ctrl_ts) < (curr_time - 
         *            RTP_MEMBER_TIMEOUT_MULTIPLIER * interval)) 
         */ 
        tempts = TIME_SUB_A_R(curr_time, 
                     TIME_MK_R(usec,(int64_t)RTP_MEMBER_TIMEOUT_MULTIPLIER * interval));
        if TIME_CMP_A(lt,p_member->rcv_ctrl_ts,tempts) {
            /* RTP packets sent by this member since last interval ? This can happen
             * for systems where data path is in different process/hardware. 
             */
            if (p_member->packets != p_member->packets_prior) {
                rtp_session_add_sender(p_sess, p_member);
                continue;
            }
            RTP_LOG_DEBUG_F(p_sess, p_member,
                            "Member timeout: SSRC 0x%08x CNAME %s\n",
                            p_member->ssrc, 
                            p_member->sdes[RTCP_SDES_CNAME] ?
                            p_member->sdes[RTCP_SDES_CNAME] : "");
            /*
             * Remove from member
             */
            MCALL(p_sess, rtp_delete_member, &p_member, TRUE);          
            
        } else {         
           break;
        }
    }

}
/*
 * Function:    rtp_session_timeout_transmit_report_base
 * Description: This should be called by application when the session times
 *              out to send a report. On return the application should put the 
 *              session back to timer based on new calculated p_sess->next_send_ts
 * Parameters:  p_sess   ptr to session object
 * Returns:     None
 */

void rtp_session_timeout_transmit_report_base (rtp_session_t *p_sess)
{
    abs_time_t       curr_time;
    uint32_t         interval;
    abs_time_t      tempts;
    rtcp_pkt_info_t *pkt_info_p = NULL;
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t msg;
    boolean         xr_enabled;

    GET_TIMESTAMP(curr_time); 
   
    interval = MCALL(p_sess, rtcp_report_interval, 
                     p_sess->we_sent,
                     TRUE /* jitter the interval */);

    /* 
     * Check if (curr_time >= p_sess->prev_send_ts + interval)
     */
    tempts = TIME_ADD_A_R(p_sess->prev_send_ts,
                     TIME_MK_R(usec, (int64_t)interval));
    if TIME_CMP_A(ge,curr_time,tempts) {
        /* send a RTCP report */

        //RTP_LOG_DEBUG_FV(p_sess, NULL,
        //                 "Time to send a report\n");
        /*
         * Note that rtcp_send_report will invoke rtcp_report_interval
         * in order to compute the "next send time" (p_sess->next_send_ts)
         */
 
        /* Here we will decide whether to include RTCP XR report or not */
        xr_enabled = is_rtcp_xr_enabled(&p_sess->rtcp_xr_cfg);
        if (xr_enabled) {
            /* Add the RTCP XR message into the packet */
            rtcp_init_pkt_info(&pkt_info);
            rtcp_init_msg_info(&msg);

            msg.xr_info.rtcp_xr_enabled = TRUE;
            rtcp_set_pkt_info(&pkt_info, RTCP_XR, &msg);
            pkt_info_p = &pkt_info;
        }

        MCALL(p_sess, 
              rtcp_send_report, 
              p_sess->rtp_local_source, 
              pkt_info_p,
              TRUE,  /* re-schedule next RTCP report */
              (xr_enabled ? TRUE : FALSE)); /* reset RTCP XR stats */
    } else {
        //RTP_LOG_DEBUG_FV(p_sess, p_member,
        //                 "Wait until next time to send a report\n");
        p_sess->next_send_ts = TIME_ADD_A_R(p_sess->prev_send_ts, 
                                            TIME_MK_R(usec, (int64_t)interval));
    }

}

/*
 * Funciton:    rtp_recv_packet_base
 * Description: Each application may need a separate receive function. This 
 *              function is a template for different steps to be invoked on 
 *              receiving an RTP packet. This one does not provide interrupt
 *              exclusion. If it returnd FALSE, the pak buffer should be freed.
 * Parameters:  p_sess   ptr to session object
 *              src_addr src ip address of the packet
 *              p_buf   pointer to start of rtcp header in the packet buf
 *              len      length of RTP packet (including RTP header)
 * Returns:     TRUE or FALSE  
 */
boolean rtp_recv_packet_base (rtp_session_t *p_sess, 
                              abs_time_t    rcv_ts,
                              ipaddrtype    src_addr,
                              uint16_t      src_port,
                              char          *p_buf,
                              uint16_t      len)
{
    rtp_member_t  *p_source = NULL, *p_member = NULL;
    rtp_member_id_t member_id;
    uint16_t       sequence, i;
    uint32_t       ssrc, *p_csrc;
    rtptype       *p_rtp = (rtptype *)p_buf;

    rtp_hdr_status_t status = RTP_HDR_OK;

    if (!p_sess) {
        return(FALSE);
    }
    
    /*
     * perform basic checks on the header, 
     * abort further processing if it's invalid
     */
    status = rtp_validate_hdr(&p_sess->rtp_stats, p_buf, len);
    if (!rtp_hdr_ok(status)) {
        return (FALSE);
    }

    /*
     * Glean sequence number and ssrc.
     */
    sequence = ntohs(p_rtp->sequence);
    ssrc = ntohl(p_rtp->ssrc);
    
    /* 
     * Lookup member and create one if it is new ssrc.
     */
    member_id.type = RTP_SMEMBER_ID_RTP_DATA;
    member_id.subtype = RTCP_CHANNEL_MEMBER;
    member_id.ssrc = ssrc;
    member_id.src_addr = src_addr;
    member_id.src_port = src_port;
    member_id.cname = NULL;

    if (MCALL(p_sess, rtp_lookup_or_create_member, 
              &member_id, &p_source) != RTP_SUCCESS) {
        p_sess->badcreate++;
        return(FALSE);
    }
// $$$    GET_TIMESTAMP(p_source->rcv_data_ts); 
//    RTP_LOG_DEBUG_FV(p_sess, p_source, "
//                     "Received RTP packet (%u bytes)\n", len);
    /*
     * If the member is not listed as a sender. Remove from the garbage list
     * and add to the sender list
     * $$$ This logic is new addtion - Verify
     */
    rtp_session_add_sender(p_sess, p_source);

    /*
     * Lookup CSRCs for valid members.
     */
    p_csrc = (uint32_t *) (p_rtp + 1);
    for (i = 0; i < p_rtp->csrc_count; i++, p_csrc++) {

/* $$$ Should the following be added as source too??? */
        member_id.ssrc = *p_csrc;
        /* $$$ this is not quite right?
           we should try to find a distinct CNAME for
           this CSRC, but for now use the CNAME of the SSRC */
        if (MCALL(p_sess, rtp_lookup_or_create_member,
                  &member_id, &p_member) != RTP_SUCCESS) {
            p_sess->badcreate++;
            return(FALSE);
        }
// $$$        GET_TIMESTAMP(p_member->rcv_data_ts); 
    }
    
    /*
     * Process and update the sequence number. If it is not within bounds
     * ignore the packet.
     */
    status = rtp_update_seq(&p_sess->rtp_stats, 
                            &p_source->rcv_stats,
                            p_source->xr_stats,
                            sequence);
    if (!rtp_hdr_ok(status)) {
        return (FALSE);
    }

    rtp_update_jitter(&p_source->rcv_stats,
                      p_source->xr_stats,
                      ntohl(p_rtp->timestamp),
                      rtp_update_timestamps(&p_source->rcv_stats, rcv_ts));
    return(TRUE);
}

/*
 * rtcp_find_pkttype
 *
 * Parse RTCP compound packet to find a particular packet type.
 *
 * Parameters:  pkt       --ptr to start of RTCP compound packet.
 *              plen      --length in bytes of RTCP compound packet.
 *              pkttype   --packet type to search for (SDES, BYE, etc.)
 *              outlen    --[out] length of packet from the beginning of
 *                          of the desired packet type, to the end of 
 *                          the compound packet, or zero if type not found.
 * Returns:     ptr to beginning of specified packet type (SDES, BYE, etc.),
 *              or NULL if not found.
 */
rtcptype *rtcp_find_pkttype (rtcptype *pkt, int plen, rtcp_type_t pkttype,
                             int *outlen)
{
    rtcptype *p_rtcp = pkt;
    int len = plen;
    int rtcp_minlen = sizeof(rtcptype);
    int bytelen; /* length in bytes */

    /*
     * walk through the individual RTCP packets, 
     * looking for the specified packet, with at least the minimum length
     */
    while ((len >= rtcp_minlen) && 
           (rtcp_get_type(ntohs(p_rtcp->params)) != pkttype)) {
        bytelen = (ntohs(p_rtcp->len) + 1) * 4;
        p_rtcp = (rtcptype *) ((uint8_t *)p_rtcp + bytelen);
        len -= bytelen;
    }

    if (len < rtcp_minlen) {
        /* 
         * we've come to the end of the compound packet without finding 
         * the specified packet (len == 0), or an individual packet had
         * a bad length (len != 0).
         */
        *outlen = 0;
        return (NULL);
    } else {
        *outlen = len;
        return (p_rtcp);
    }
}

/*
 * Function:    rtcp_find_cname
 * Description: Parse RTCP compound packet to find the CNAME
 *              associated with the given SSRC.
 * Parameters:  p_pkt      --ptr to start of RTCP compound pkt
 *              plen       --length in bytes of RTCP payload (compound pkt)
 *              ssrc       --SSRC value
 *              cname_len  --(output) length in bytes of CNAME data
 * Returns:     ptr to CNAME (non-NULL terminated)
 */
uint8_t *rtcp_find_cname(rtcptype *p_pkt, int plen, uint32_t ssrc,
                         uint32_t *cname_len)
{
    rtcptype *p_rtcp = NULL;
    int len = 0;
    int count;
    int bytelen; /* length in bytes */
    uint8_t *p_end;
    uint8_t *p_item, *p_next_item;
    rtcp_sdes_chunk_t *p_chunk;
    uint32_t chunk_ssrc;

    /*
     * walk through the individual RTCP packets, 
     * looking for an SDES packet with at least the minimum length
     */
    p_rtcp = rtcp_find_pkttype(p_pkt, plen, RTCP_SDES, &len);
    if (!p_rtcp) {
        return (NULL);
    }

    /* 
     * p_rtcp now points to an SDES packet, of at least minimal length
     */
    bytelen = (ntohs(p_rtcp->len) + 1) * 4;
    if (bytelen > len) {
        /* bad packet len, or truncated packet */
        return (NULL);
    }
    
    /* get the number of chunks (one per SSRC) */
    count = rtcp_get_count(ntohs(p_rtcp->params));

    /* 
     * point to the first chunk, 
     * which is actually the SSRC field of the RTCP header
     */
    p_chunk = (rtcp_sdes_chunk_t *)(&(p_rtcp->ssrc));

    /* point to the end of the SDES pkt */
    p_end = ((uint8_t *)p_rtcp) + bytelen;
    
    /*
     * iterate over the chunks, as illustrated in 
     * rtp_read_sdes in Appendix A.5 of RFC 3550
     */
    while (--count >= 0) {
        p_item = &(p_chunk->items[0]);
        if (p_item >= p_end) {
            break;
        }
        chunk_ssrc = ntohl(p_chunk->ssrc);
        /*
         * iterate over the items, looking for CNAME;
         * note that even if the SSRC of this chunk is not
         * the one we're interested in, we still have to iterate
         * over the items, to find the start of the next chunk
         */
        for (; p_item[RTCP_SDES_ITEM_TYPE]; p_item = p_next_item) {
            p_next_item = p_item + p_item[RTCP_SDES_ITEM_LEN] + 2;
            if (p_next_item >= p_end) {
                return (NULL);
            }
            if (chunk_ssrc == ssrc &&
                p_item[RTCP_SDES_ITEM_TYPE] == RTCP_SDES_CNAME) {
                *cname_len = p_item[RTCP_SDES_ITEM_LEN];
                return (&p_item[RTCP_SDES_ITEM_DATA]);
            }
        }
        /*
         * find the next chunk, 
         * taking into account any padding at the end of the items
         */
        p_chunk = (rtcp_sdes_chunk_t *)
            ((uint32_t *)p_chunk + (((p_item - (uint8_t *)p_chunk) >> 2) + 1));
    }

    return (NULL);
}

/*
 * Function:    rtcp_find_bye
 * Description: Parse RTCP compound packet to find a BYE pkt
 * Paramters:   p_pkt  --ptr to start of RTCP compound pkt
 *              plen   --length of RTCP compound pkt (in bytes)
 *              ssrc   --SSRC which must be contained in the BYE
 * Returns:     ptr to BYE packet with matching SSRC, or NULL
 */
rtcptype *rtcp_find_bye (rtcptype *p_pkt, int plen, uint32_t ssrc)
{
    rtcptype *p_rtcp = NULL;
    int len = 0;
    int bytelen = 0; /* length in bytes */
    uint8_t *p_end_of_frame = NULL;
    uint32_t *p_ssrc = NULL;
    int count = 0;

    /*
     * walk through the individual RTCP packets, 
     * looking for a BYE packet with at least the minimum length
     */
    p_rtcp = rtcp_find_pkttype(p_pkt, plen, RTCP_BYE, &len);
    if (!p_rtcp) {
        return (NULL);
    }

    /* 
     * p_rtcp now points to a BYE packet, of at least minimal length
     */
    bytelen = (ntohs(p_rtcp->len) + 1) * 4;
    if (bytelen > len) {
        /* bad packet len, or truncated packet */
        return (NULL);
    } else {
        p_end_of_frame = ((uint8_t *)p_rtcp) + bytelen;
        p_ssrc = (uint32_t *)(&p_rtcp->ssrc);
        count = rtcp_get_count(ntohs(p_rtcp->params));
    }
    
    while (count-- > 0) {
        if ((uint8_t *)p_ssrc > p_end_of_frame) {
            return (NULL);
        }
        if (ntohl(*p_ssrc) == ssrc) {
            return (p_rtcp);
        }
        p_ssrc++;
    }

    return (NULL);
}

/* RTCP pkt handling functions */

/*
 * Function:    rtcp_process_rr_stats
 * Description: Process rr records for loss and jitter for each sender(source),
 *              at this receiver (rr_source).
 * Parameters:  p_sess   ptr to session object
 *              src_addr src ip address of sender
 *              p_rr     ptr to rr record
 *              count    count of the rtcp packet in compound packet
 *              p_rr_source ptr to RTP (sender) member
 *              p_end_of_frame  end of RTCP frame
 * Returns:     RTCP_PARSE_OK or RTCP_PARSE_BADLEN
 */
static rtcp_parse_status_t 
rtcp_process_rr_stats (rtp_session_t *p_sess,
                       ipaddrtype    src_addr,
                       rtcp_rr_t     *p_rr,
                       uint16_t      count,
                       rtp_member_t  *p_rr_source,
                       uint8_t       *p_end_of_frame)
{
    uint32_t ssrc;
    rtp_member_t *p_source=0;
    ntp64_t  ntp;
    uint32_t rtt;

    /*
     * Scan through all the rr records, glean sources for which 
     * this receiver is sending stats, and store them in the 
     * sender_info structure for this receiver.
     */
    while (count) {
        if ((uint8_t *)(p_rr + 1) > p_end_of_frame) {
            return (RTCP_PARSE_BADLEN);
        }
        ssrc = ntohl(p_rr->rr_ssrc);
        
        /*
         * Look up the source specified by this ssrc. 
         */
        p_source = rtp_sending_member_with_ssrc(p_sess, ssrc);
        
        /*
         * Copy the reception stats sent by this receiver for the 
         * sender.
         */
        if (p_source && 
            (p_source->pos >= 0) &&
            (p_source->pos < RTP_MAX_SENDERS_CACHED)) {
            p_rr_source->sender_info[p_source->pos].rr_loss = ntohl(p_rr->rr_loss);
            p_rr_source->sender_info[p_source->pos].rr_ssrc = ssrc;
            p_rr_source->sender_info[p_source->pos].rr_ehsr = ntohl(p_rr->rr_ehsr);
            p_rr_source->sender_info[p_source->pos].rr_jitter = 
                ntohl(p_rr->rr_jitter);
            p_rr_source->sender_info[p_source->pos].rr_lsr = ntohl(p_rr->rr_lsr);
            p_rr_source->sender_info[p_source->pos].rr_dlsr = ntohl(p_rr->rr_dlsr);
        }
        RTP_LOG_PACKET_FV(p_sess, p_rr_source,
                          "Receiving RR for ssrc 0x%08x:"
                          "loss %d, ehsr %d, jitter 0x%08x, "
                          "lsr 0x%08x, dlsr 0x%08x\n",
                          ssrc, ntohl(p_rr->rr_loss) &0x0ffffff, 
                          ntohl(p_rr->rr_ehsr), ntohl(p_rr->rr_jitter), 
                          ntohl(p_rr->rr_lsr), ntohl(p_rr->rr_dlsr));
        /* Calculate Round Trip Delay */
        if (p_sess->we_sent) {
            ntp = get_ntp_time();
            rtt = MFPTOFP(ntp.upper, ntp.lower) - ntohl(p_rr->rr_lsr) 
                - ntohl(p_rr->rr_dlsr);
            RTP_LOG_PACKET_F(p_sess, p_rr_source,
                             "Current round trip delay is %f msecs\n",
                             (float)1000*rtt/65536.0);
        }

        p_rr++;
        count--;
    }

    return (RTCP_PARSE_OK);
}


/*
 * Function:    rtcp_process_sr
 * Description: Parse an RTCP sender report and store the sender specific
 *              info like the total packets sent.
 * Parameters:   p_sess         ptr to session object
 *              p_pak_source   ptr to RTP (sender) member
 *              src_addr       src ip address of sender
 *              src_port       src UDP port 
 *              p_rtcp         ptr to start of RTCP header in packet
 *              count          count of the rtcp packet in compound packet
 *              p_end_of_frame end of RTCP frame
 * Returns:     RTCP_PARSE_OK or RTCP_PARSE_BADLEN.
 */
static rtcp_parse_status_t 
rtcp_process_sr (rtp_session_t  *p_sess,
                 rtp_member_t  *p_pak_source,
                 ipaddrtype    src_addr,
                 uint16_t      src_port,
                 rtcptype      *p_rtcp,
                 uint16_t      count,
                 uint8_t       *p_end_of_frame)
{
    rtcp_sr_t *p_sr;
    uint32_t     ssrc;
    rtp_member_t *p_source;

    RTP_LOG_PACKET_F(p_sess, p_pak_source,
                     "Received Sender Report packet\n");

    p_sr = (rtcp_sr_t *) (p_rtcp + 1);
    if ((uint8_t *)p_sr > p_end_of_frame) {
        return (RTCP_PARSE_BADLEN);
    }
    ssrc = ntohl(p_rtcp->ssrc);     /* sender's ssrc */

    /*
     * Get new source cache state, if different
     * from original pak source (Eg. Mixer)
     */
/* $$$ Doesn't look like the following check makes sense
    if (p_pak_source->ssrc != ssrc) {
        p_source = rtp_lookup_member(p_sess, ssrc, src_addr, NULL, TRUE);
        if (!p_source)
            return;
    } else
*/
    p_source = p_pak_source;

    rtp_session_add_sender(p_sess, p_source);

    /*
     * Get timestamps for the report and the
     * update sender stats for the source.
     */
    GET_TIMESTAMP(p_source->rcv_ctrl_ts);
    GET_TIMESTAMP(p_source->rcv_sr_ts);

    if ((uint8_t *)(p_sr + 1) > p_end_of_frame) {
        return (RTCP_PARSE_BADLEN);
    }

    p_source->sr_ntp_h = ntohl(p_sr->sr_ntp_h);
    p_source->sr_ntp_l = ntohl(p_sr->sr_ntp_l);
    p_source->sr_npackets = ntohl(p_sr->sr_npackets);
    p_source->sr_nbytes   = ntohl(p_sr->sr_nbytes);

    /*
     * Parse RR records if any.
     */
    return (rtcp_process_rr_stats(p_sess, src_addr, (rtcp_rr_t *) (p_sr + 1), 
                                  count, p_source, p_end_of_frame));
}


/*
 * Function:    rtcp_process_rr
 * Description: RTCP reception report parsing. Glean the SSRC of the receiver and 
 *              then parse individual rr records.
 * Parameter:   p_sess         ptr to session object
 *              p_pak_source   ptr to RTP (sender) member
 *              src_addr       src ip address of sender
 *              src_port       src UDP port 
 *              p_rtcp         ptr to start of RTCP header in packet
 *              count          count of the rtcp packet in compound packet
 *              p_end_of_frame end of RTCP frame
 * Returns:     RTCP_PARSE_OK or RTCP_PARSE_BADLEN
 */
static rtcp_parse_status_t 
rtcp_process_rr (rtp_session_t *p_sess,
                 rtp_member_t  *p_pak_source,
                 ipaddrtype    src_addr,
                 uint16_t      src_port,
                 rtcptype      *p_rtcp,
                 uint16_t      count,
                 uint8_t       *p_end_of_frame)
{
    uint32_t     ssrc;
    rtp_member_t *p_source;
    rtcp_rr_t *p_rr;

    RTP_LOG_PACKET_F(p_sess, p_pak_source,
                     "Received Receiver Report packet\n");

    p_rr = (rtcp_rr_t *)(p_rtcp + 1);
    if ((uint8_t *)p_rr > p_end_of_frame) {
        return (RTCP_PARSE_BADLEN);
    }
    ssrc = ntohl(p_rtcp->ssrc);


/* $$$ Doesn't look like the following check makes sense
    if (p_pak_source->ssrc != ssrc) {
        p_source = rtp_lookup_member(p_sess, ssrc, src_addr, NULL, TRUE);
        if (!p_source)
            return;
    } else
*/
    p_source = p_pak_source;
    
    /*
     * Get timestamp for the report.
     */
    GET_TIMESTAMP(p_source->rcv_ctrl_ts);

    /*
     * Parse rr records if any.
     */
    return (rtcp_process_rr_stats(p_sess, src_addr, p_rr,
                                  count, p_source, p_end_of_frame));
}


/*
 * Function:   rtcp_process_bye
 * Description: Process a bye packet. We do not assume a bye reason in the packet.
 * Parameter:   p_sess         ptr to session object
 *              p_pak_source   ptr to RTP (sender) member
 *              src_addr       src ip address of sender
 *              src_port       src UDP port 
 *              p_rtcp         ptr to start of RTCP header in packet
 *              count          count of the rtcp packet in compound packet
 *              p_end_of_frame end of RTCP frame
 * Returns:     None
*/
static rtcp_parse_status_t 
rtcp_process_bye (rtp_session_t *p_sess,
                  rtp_member_t  *p_pak_source,
                  ipaddrtype    src_addr,
                  uint16_t      src_port,
                  rtcptype      *p_rtcp,
                  uint16_t      count,
                  uint8_t       *p_end_of_frame)
{
    rtp_member_t *p_source;
    uint32_t *p_base;
    uint32_t ssrc;
    
    p_base = (uint32_t *) &p_rtcp->ssrc;

    RTP_LOG_PACKET_F(p_sess, p_pak_source,
                     "Received BYE Packet\n");
    
    while (count-- > 0) {
        if ((uint8_t *)p_base > p_end_of_frame) {
            return (RTCP_PARSE_BADLEN);
        }
        ssrc = ntohl(*p_base);

        p_source = rtp_find_member(p_sess, 
                                   ssrc, 
                                   p_pak_source->sdes[RTCP_SDES_CNAME]);
        if (!p_source) {
            continue;
        }
        MCALL(p_sess, rtp_delete_member, &p_source, TRUE);
        p_base++;
    }
    
    /* cruft at the end of the packet is the reason for the bye packet */
    /* process if necessary. Standard reasons are leaving/collision/loop */
    return (RTCP_PARSE_OK);
}

/*
 * Function:    rtcp_process_one_sdes_item
 * Description: Store info from one SDES item (e.g., CNAME).
 * Parameter:   p_sess         ptr to session object
 *              p_source   ptr to RTP (sender) member
 *              item_type      SDES item type
 *              item_len       SDES item length, in bytes
 *              item_data      ptr to SDES item data
*/
static void rtcp_process_one_sdes_item (rtp_session_t *p_sess,
                                        rtp_member_t  *p_source,
                                        uint8_t        item_type,
                                        uint8_t        item_len,
                                        uint8_t       *item_data)
{
    char          *p_sdes_item;

    /*
     * Copy the SDES item into the appropriate array.
     */
    if (range(RTCP_SDES_MIN, RTCP_SDES_MAX, item_type)) {
        p_sdes_item = p_source->sdes[item_type];
        if (!p_sdes_item) {
            p_sdes_item = rtcp_new_object(p_sess->rtcp_mem, RTCP_OBJ_SDES);
            if (!p_sdes_item) {
                return;
            }
            p_source->sdes[item_type] = p_sdes_item;
        }
        bcopy(item_data, p_sdes_item, item_len);
        p_sdes_item[item_len] = 0;
    }
}


/*
 * Function:    rtcp_process_sdes 
 * Description: Parse SDES items.
 * Parameter:   p_sess         ptr to session object
 *              p_pak_source   ptr to RTP (sender) member
 *              src_addr       src ip address of sender
 *              src_port       src UDP port 
 *              p_rtcp         ptr to start of RTCP header in packet
 *              count          count of the rtcp packet in compound packet
 *              p_end_of_frame end of RTCP frame
 * Returns:     RTCP_PARSE_OK or RTCP_PARSE_BADLEN
 */
static rtcp_parse_status_t 
rtcp_process_sdes (rtp_session_t *p_sess,
                   rtp_member_t  *p_pak_source,
                   ipaddrtype    src_addr,
                   uint16_t      src_port,
                   rtcptype      *p_rtcp,
                   uint16_t      count,
                   uint8_t       *p_end_of_frame)
{
    uint8_t *p_item, *p_next_item;
    rtcp_sdes_chunk_t *p_chunk;
    uint32_t chunk_ssrc;
    int icount = count;

    RTP_LOG_PACKET_F(p_sess, p_pak_source,
                     "Received SDES packet\n");
    /* 
     * Point to the first chunk, 
     * which is actually the SSRC field of the RTCP header.
     */
    p_chunk = (rtcp_sdes_chunk_t *)(&(p_rtcp->ssrc));

    /*
     * Iterate over the chunks, as illustrated in 
     * rtp_read_sdes in Appendix A.5 of RFC 3550
     */
    while (--icount >= 0) {
        p_item = &(p_chunk->items[0]);
        if (p_item >= p_end_of_frame) {
            break;
        }
        chunk_ssrc = ntohl(p_chunk->ssrc);
        /*
         * Iterate over the items, processing only items
         * that belong to the 'p_pak_source' member; i.e.,
         * we don't currently store items for CSRC chunks.
         * Note that even if the SSRC of this chunk is not
         * the one we're interested in, we still have to iterate
         * over the items, to find the start of the next chunk.
         */
        for (; p_item[RTCP_SDES_ITEM_TYPE]; p_item = p_next_item) {
            p_next_item = p_item + p_item[RTCP_SDES_ITEM_LEN] + 2;
            if (p_next_item >= p_end_of_frame) {
                return (RTCP_PARSE_BADLEN);
            }
            if (chunk_ssrc == p_pak_source->ssrc) {
                rtcp_process_one_sdes_item(p_sess,
                                           p_pak_source,
                                           p_item[RTCP_SDES_ITEM_TYPE],
                                           p_item[RTCP_SDES_ITEM_LEN],
                                           &p_item[RTCP_SDES_ITEM_DATA]);
            }
        }
        /*
         * Find the next chunk, 
         * taking into account any padding at the end of the items.
         */
        p_chunk = (rtcp_sdes_chunk_t *)
            ((uint32_t *)p_chunk + (((p_item - (uint8_t *)p_chunk) >> 2) + 1));
    }

    return (icount < 0 ? RTCP_PARSE_OK : RTCP_PARSE_BADLEN);
}


/*
 * Function:    rtcp_process_app
 * Description: Deal with incoming APP RTCP packet
 * Parameters:  p_sess         ptr to session object
 *              p_pak_source   ptr to RTP (sender) member
 *              src_addr       src ip address of sender
 *              src_port       src UDP port 
 *              p_rtcp         ptr to start of RTCP header in packet
 *              count          count of the rtcp packet in compound packet
 *              p_end_of_frame end of RTCP frame
 * Returns:     None
 */

static rtcp_parse_status_t 
rtcp_process_app (rtp_session_t *p_sess,
                  rtp_member_t  *p_pak_source,
                  ipaddrtype    src_addr,
                  uint16_t      src_port,
                  rtcptype      *p_rtcp,
                  uint16_t      count,
                  uint8_t       *p_end_of_frame)
{
    rtcp_app_t *p_app;
    rtcp_app_info_t app_info;

    p_app = (rtcp_app_t *)(p_rtcp + 1);

    RTP_LOG_PACKET_F(p_sess, p_pak_source,
                     "Received APP packet (len field %u)\n", 
                     ntohs(p_rtcp->len));

    if ((uint8_t *)(p_app + 1) > p_end_of_frame) {
        return (RTCP_PARSE_BADLEN);
    }

    /* call registry for receipt of app packets */
    app_info.name = p_app->name;
    app_info.data = p_app->data;
    app_info.len =  ((p_rtcp->len + 1) << 2) - 4 - sizeof(rtcptype);

    /* 
     * Note users are expected to override this function
     */
    return (RTCP_PARSE_OK);
}


/*
 * Function:    rtcp_process_xr
 * Description: Deal with incoming RTCP XR packet
 * Parameters:  p_sess         ptr to session object
 *              p_pak_source   ptr to RTP (sender) member
 *              src_addr       src ip address of sender
 *              src_port       src UDP port 
 *              p_rtcp         ptr to start of RTCP header in packet
 *              count          count of the rtcp packet in compound packet
 *              p_end_of_frame end of RTCP frame
 * Returns:     None
 */

static rtcp_parse_status_t 
rtcp_process_xr (rtp_session_t *p_sess,
                 rtp_member_t  *p_pak_source,
                 ipaddrtype    src_addr,
                 uint16_t      src_port,
                 rtcptype      *p_rtcp,
                 uint16_t      count,
                 uint8_t       *p_end_of_frame)
{
    rtcp_xr_gen_t *p_xr;
    rtcp_xr_report_type_t type;

    RTP_LOG_PACKET_F(p_sess, p_pak_source,
                     "Received RTCP XR packet\n");

    p_xr = (rtcp_xr_gen_t *) (p_rtcp + 1);
    while ((uint8_t *)(p_xr + 1) <= p_end_of_frame) {
        type = p_xr->bt;
        switch (type) {
            case RTCP_XR_LOSS_RLE:

                break;

            case RTCP_XR_STAT_SUMMARY:

                break;

            default:
                break;
        }

        p_xr = (rtcp_xr_gen_t *)((uint8_t *)p_xr + 
                                 ((ntohs(p_xr->length) + 1) << 2));
    }

    if ((uint8_t *)p_xr != p_end_of_frame) {
        RTP_LOG_ERROR_F(p_sess, p_pak_source,
                        "Failed to process RTCP XR in RTCP compound pkt\n");
        return (RTCP_PARSE_BADLEN);
    } else {
        return (RTCP_PARSE_OK);
    }
}


/* Function:    rtp_session_set_rtcp_handler
 * Description: Sets the function pointers for RTCP handlers
 * Parameters:  None
 * Returns:     None
 */
void rtp_session_set_rtcp_handlers(rtcp_handlers_t *table)
{
    uint16_t  i;
    for (i=0;i<=RTCP_MAX_TYPE; i++) {
        table->proc_handler[i] = NULL;
    }
    table->proc_handler[RTCP_SR]   = rtcp_process_sr;
    table->proc_handler[RTCP_RR]   = rtcp_process_rr;
    table->proc_handler[RTCP_SDES] = rtcp_process_sdes;
    table->proc_handler[RTCP_BYE]  = rtcp_process_bye;
    table->proc_handler[RTCP_APP]  = rtcp_process_app;
    table->proc_handler[RTCP_XR]   = rtcp_process_xr;
}

/* 
 * Function:     rtcp_process_packet_base
 * Description:  Receive a compound RTCP packets, and store member 
 *               information rtcp_recv_packet from reports, sdes and byes.
 * Parameters:   p_sess    ptr to session object
 *               recv_only if TRUE, this is from a "receive only" participant
 *               src_addr  src ip address
 *               src_port  src UDP port 
 *               p_buf     ptr to start of rtcp header in the packet buffer
 *               len        length of RTCP packet (including header)
 * Returns:      TRUE if the packet passed basic parsing checks, else FALSE.
 *
 */
boolean rtcp_process_packet_base (rtp_session_t *p_sess,
                                  boolean       recv_only,
                                  ipaddrtype    src_addr,
                                  uint16_t      src_port,
                                  char           *p_buf,
                                  uint16_t        len)
{
    uint8_t         *p_end_of_pak, *p_end_of_frame, *p_pkt;
    uint16_t        params = 0, count = 0;
    rtp_member_t  *p_source = NULL;
    rtp_member_id_t member_id;
    uint8_t       *cname;
    uint8_t        loc_cname[RTCP_MAX_CNAME+1];
    uint32_t       clen = 0;
    rtcptype      *p_rtcp = (rtcptype *)p_buf;
    rtcp_type_t    msg_type;
    rtcp_parse_status_t parse_status = RTCP_PARSE_OK;
    boolean        parsed_ok = TRUE;
    uint16_t       msg_len;
    int            bytes_remaining;
    rtcp_type_processor msg_handler;
    rtcp_msg_mask_t mask = RTCP_MSG_MASK_0;
    boolean        rsize_pkt = FALSE;

    if (!p_sess) {
        return (FALSE);
    }
    
    /*
     * get the initial message type, if at all possible
     */
    msg_type = (len >= 2) ? 
        rtcp_get_type(ntohs(p_rtcp->params)) : NOT_AN_RTCP_MSGTYPE;

    /*
     * Perform sanity checks on the len.
     */
    if (len < sizeof(rtcptype)) {
        rtcp_upd_msg_rcvd_stats(p_sess, msg_type, RTCP_PARSE_RUNT);
        parsed_ok = FALSE;
        goto upd_pkt_stats;
    }
    
    /*
     * Validate the packet.
     */
    if (rtcp_get_version(ntohs(p_rtcp->params)) != RTPVERSION) {
        rtcp_upd_msg_rcvd_stats(p_sess, msg_type, RTCP_PARSE_BADVER);
        parsed_ok = FALSE;
        goto upd_pkt_stats;
    }

    /* 
     * First packet in RTCP compound packet should be of the following types
     */
    switch (msg_type) {
    case RTCP_SR:
    case RTCP_RR:
    case RTCP_BYE:
        break;
    default:
        /* 
         * This is probably a reduced-size RTCP, which we now accept even if
         * rtcp-rsize is not enabled, since there can be a time delay between
         * disabling the feature on the server and this propagating to the 
         * client. Note that in case of such a temporary config mismatch, and
         * if the packet is a reduced-size RTCP, the initial message type is
         * probably an SDES, and this will be counted as both "unexpected" 
         * (in advanced stats) and as "received" (presuming the message type
         * is otherwise OK). If the packet is not an Reduced size RTCP packet
         * and is something completely malformed, it will be rejected later in
         * the validation process. 
         */
        if (!(p_sess->rtcp_rsize)) {
            rtcp_upd_msg_rcvd_stats(p_sess, msg_type, RTCP_PARSE_UNEXP);
        }
        parsed_ok = TRUE;
    } 
    
    cname = rtcp_find_cname(p_rtcp, len, ntohl(p_rtcp->ssrc), &clen);
    /* RTCP packets should contain CNAME */
    if (!cname) {
        /* $$$ not very precise, but it's the closest existing error type */
        rtcp_upd_msg_rcvd_stats(p_sess, RTCP_SDES, RTCP_PARSE_BADLEN);
        parsed_ok = FALSE;
        goto upd_pkt_stats;
    }
    bcopy (cname, loc_cname, clen);
    loc_cname[clen] = 0;
    
/*
 *  $$$ Do we need the following check for VAM ?
 if ((rtcp->ssrc == p_sess->rtp_local_source->ssrc &&
 !strcmp(loc_cname, p_sess->rtp_local_source->sdes[RTCP_SDES_CNAME])) ||
 ip_ouraddress(IPROUTING_DEF_TABLEID, src_addr)) {
 if (rtp_debug_prot) {
 buginf("\nRTCP: TOSSING packet from ourselves");
 }
 return;
 }
*/
    
    
    
/*
    $$$ Correct the following
    if (p_sess->mode == RTP_SSM_RECEIVER_RSI ||
        p_sess->mode == RTP_SSM_RECEIVER_SIMPLE ) {
        redirect_rtcpsocket(p_sess->rtcp_socket, ip->srcadr, p_udp->sport);
    }
    if (p_sess->mode == RTP_SSM_SOURCE_SIMPLE) {
        rtcp_forward_packet(p_sess, p_pak);
    }
*/
    
    /*
     * Lookup the member, if not found create one 
     *  The choice of client or channel members depends on the context
     *  the request was received in. Requests received that scale with the
     *  number of clients are client requests, while requests that scale
     *  with the number of channels are channel members. 
     */
    
    member_id.type = recv_only ? 
        RTP_RMEMBER_ID_RTCP_DATA : RTP_SMEMBER_ID_RTCP_DATA ;
    member_id.subtype = recv_only ? RTCP_CLIENT_MEMBER : RTCP_CHANNEL_MEMBER;
    member_id.ssrc = ntohl(p_rtcp->ssrc);
    member_id.src_addr = src_addr;
    member_id.src_port = src_port;
    member_id.cname = (char *)loc_cname;

    if (MCALL(p_sess, rtp_lookup_or_create_member,
              &member_id, &p_source) != RTP_SUCCESS) {

        RTP_LOG_ERROR_F(p_sess, NULL,
                        "RTP: Failed to lookup or create a member: "
                        "SSRC 0x%x CNAME %s\n", 
                        member_id.ssrc, 
                        member_id.cname ? member_id.cname : "<none>");
        /* 
         * we don't know if it's okay or not,
         * but we can't process it further, 
         * so we have to assume it's not 
         */
        rtcp_upd_msg_rcvd_stats(p_sess, msg_type, RTCP_PARSE_UNEXP);
        parsed_ok = FALSE;
        goto upd_pkt_stats;
    }

    RTP_LOG_PACKET_F(p_sess, p_source,
                     "Received compound RTCP packet from CNAME %s\n", 
                     member_id.cname ? member_id.cname : "<none>");

    RTP_LOG_DEBUG_FV(p_sess, p_source,
                     "Member created or looked-up for SSRC 0x%08x CNAME %s\n", 
                      member_id.ssrc,
                      member_id.cname ? member_id.cname : "<none>");

    /* Move the member to head of garbage list as activity has been detected */
    if (p_source->pos == -1) {
        rtp_member_move_to_head (p_sess, p_source);
    }

    /*
     * Parse through the compound RTCP packet.
     */
    p_pkt = (uint8_t *)p_rtcp;
    p_end_of_pak = p_pkt + len;

    while (p_pkt < p_end_of_pak) {

        bytes_remaining = p_end_of_pak - p_pkt;
        if (bytes_remaining >= 2) {
            params = ntohs(p_rtcp->params);
            msg_type = rtcp_get_type(params);
        } else {
            params = 0;
            msg_type = NOT_AN_RTCP_MSGTYPE;
        }
        if (bytes_remaining >= 4) {
            msg_len = (ntohs(p_rtcp->len) + 1) * 4;
            p_end_of_frame = p_pkt + msg_len;
        } else {
            /* we're always expecting at least 4 */
            msg_len = 4;
            p_end_of_frame = p_pkt + bytes_remaining;
        }
        if (bytes_remaining < msg_len) {
            RTP_LOG_ERROR_F(p_sess, p_source,
                            "RTCP msg too short: expected %d bytes, got %d\n",
                            msg_len, bytes_remaining);
            rtcp_upd_msg_rcvd_stats(p_sess, msg_type, RTCP_PARSE_RUNT);
            parsed_ok = FALSE;
            goto upd_pkt_stats;
        }
        if (rtcp_get_version(params) != RTPVERSION) {
            RTP_LOG_ERROR_F(p_sess, p_source,
                            "Bad RTCP version %d\n", rtcp_get_version(params));
            rtcp_upd_msg_rcvd_stats(p_sess, msg_type, RTCP_PARSE_RUNT);
            parsed_ok = FALSE;
            goto upd_pkt_stats;
        }
        count = rtcp_get_count(params);

        /*
         * Demux the single RTCP packet.
         */
        if (RTCP_MSGTYPE_OK(msg_type)) {
            msg_handler = p_sess->rtcp_handler->proc_handler[msg_type];
            if (msg_handler) {
                parse_status = (*msg_handler)(p_sess, p_source, 
                                              src_addr, src_port,
                                              p_rtcp, count, p_end_of_frame);
            } else {
                RTP_LOG_ERROR_F(p_sess, p_source,
                                "Unexpected RTCP type %d\n", msg_type);
                parse_status = RTCP_PARSE_UNEXP;
            }
        } else {
            RTP_LOG_ERROR_F(p_sess, p_source, 
                            "Unknown RTCP type %d\n", msg_type);
            parse_status = RTCP_PARSE_UNEXP;
        }
        rtcp_upd_msg_rcvd_stats(p_sess, msg_type, parse_status);
        if (parse_status != RTCP_PARSE_OK) {
            parsed_ok = FALSE;
        } else {
             rtcp_set_msg_mask(&mask, msg_type);
        }
        /* 
         * point to the next message
         */
        p_pkt = p_end_of_frame;
        p_rtcp = (rtcptype *)p_pkt;
    }

    /* Reduced size packets do not have an RR/SR packet.*/
    if(parsed_ok &&
       (!(rtcp_is_msg_set(mask, RTCP_RR) || 
          rtcp_is_msg_set(mask, RTCP_SR)))) {
        rsize_pkt = TRUE;
    }

 upd_pkt_stats:
    rtcp_upd_pkt_rcvd_stats(p_sess, len, parsed_ok, rsize_pkt);
    return (parsed_ok);
}

/*
 * Function:      rtcp_send_time
 * Description:   Estimate the time at which an 
 *                RTCP compound packet was sent.
 * Parameters:    p_buf -- ptr to start of RTCP compound packet
 *                len   -- len of compound packet, in bytes
 * Returns:       An estimate of the send time, in NTP format (host order):
 *                current time is returned, as an approximation.
 */
static ntp64_t rtcp_send_time (uint8_t *buf, uint16_t len)
{
    return (get_ntp_time());
}    


/* 
 * Function:     rtcp_recv_packet
 * Description:  Receive a compound RTCP packets, and store member 
 *               information rtcp_recv_packet from reports, sdes and byes.
 * Parameters:   p_sess   ptr to session object
 *               rcv_ts    Receive timestamp of the packet. May be
 *                         ABS_TIME_0 if application-layer class doesn't 
 *                         use last_rtcp_pkt_ts
 *               from_recv_only_member -- if TRUE, pkt is from a "recv only"
 *                                        participant in the session.
 *               addrs     ptr to struct with src/dest transport addrs
 *               p_buf     ptr to start of rtcp header in the packet buffer
 *               len        length of RTCP packet (including header)
 * Returns:      None
 */
void rtcp_recv_packet_base (rtp_session_t *p_sess,
                            abs_time_t    rcv_ts,
                            boolean       from_recv_only_member,
                            rtp_envelope_t *addrs,
                            char           *p_buf,
                            uint16_t       len)
{
    boolean valid = TRUE;

    p_sess->last_rtcp_pkt_ts = rcv_ts;

    valid = MCALL(p_sess, rtcp_process_packet, 
                  from_recv_only_member,
                  addrs->src_addr, addrs->src_port, 
                  p_buf, len);

    if (valid) {
        /* perform received packet post-processing */
        MCALL(p_sess, rtcp_packet_rcvd,
              from_recv_only_member,
              addrs,
              rtcp_send_time((uint8_t *)p_buf, len),
              (uint8_t *)p_buf,
              len);
    }
    return;
}

/* 
 * Function:     rtcp_packet_rcvd
 * Description:  Performs post-processing on an RTCP compound packet
 *               which has just been received and processed.
 *               There is no default post-processing, so an
 *               an application-level class must override this method
 *               if post-processing is required. This is typically
 *               done to "export" the RTCP packet, for VQM purposes.
 * Parameters:   p_sess          ptr to session object
 *               from_recv_only_member -- if TRUE, pkt is from "receive only"
 *                                        participant.
 *               p_env           ptr to address info for the pkt
 *               orig_send_time  the time at which the pkt was originally sent;
 *                               derived from pkt itself (and current time)
 *               p_buf           ptr to start of first RTCP header in the pkt
 *               len             length in bytes of RTCP compound pkt
 * Returns:      None
 */

void rtcp_packet_rcvd_base(rtp_session_t *p_sess,
                           boolean from_recv_only_member,
                           rtp_envelope_t *orig_addrs,
                           ntp64_t orig_send_time,
                           uint8_t *p_buf,
                           uint16_t len)
{
    /*
     * As an example, here's how to export a compound packet:
     *
     * 1. initialize the VQR header via the vqr_report_init() function:
     *    - the values of the subtype, chan_addr, chan_port, 
     *      sndr_role, and rcvr_role arguments must be determined 
     *      by the application-level class.
     *    - the orig_send_time and orig_addrs arguments should be the same
     *      as supplied in the orig_send_time and orig_addrs arguments
     *      to this function.
     * 2. call vqr_export() to export the packet.
     */
}



/*  RTCP Transmission Functions */

/*
 * rtcp_fmt_loss_value
 *
 * Format the 32-bit "loss value" for the RR reception block,
 * from the actual loss, and the loss fraction for the current
 * reporting interval.
 *
 * Paramters
 * loss_fraction    -- value between 0 and 255.
 * actual_loss      -- signed 32-bit cumulative loss.
 * Returns:         -- 32 bit loss value (in host order):
 *                     high order 8 bits: loss_fraction
 *                     low order 24 bits: signed cumulative loss,
 *                       capped at 0x7fffff for positive loss,
 *                       or at 0x800000 for negative loss.
 */
static uint32_t rtcp_fmt_loss_value (uint32_t loss_fraction,
                                     int32_t  actual_loss)
{
#define MAX_LOSS 0x7fffff
    int32_t reported_loss = 0;
    int32_t pos_loss_cap = MAX_LOSS;
    int32_t neg_loss_cap = -(MAX_LOSS + 1);

    if (actual_loss > pos_loss_cap) {
        reported_loss = pos_loss_cap;
    } else if (actual_loss < neg_loss_cap) {
        reported_loss = neg_loss_cap;
    } else {
        reported_loss = actual_loss;
    }

    return ((loss_fraction << 24) | (reported_loss & 0xffffff));
}
               
/*
 * Function:    rtcp_construct_rr_block
 * Descritpion: Make receiver report blocks for senders which did send 
 *              some traffic in the last interval.
 * Parameters:  p_rr   --ptr to start of RTCP RR block in packet
 *              p_sess --ptr to session object
 *              p_end  --ptr to end of buffer
 *              reset_xr_stats --flag for whether to reset RTCP XR stats
 * Returns:     Number of RR records formatted (i.e. number of remote senders)
 *              or -1, if there is insufficient room in the buffer
 */
static int rtcp_construct_rr_block (rtcp_rr_t     *p_rr, 
                                    rtp_session_t *p_sess,
                                    uint8_t       *p_end,
                                    boolean        reset_xr_stats) 
    
{
    rtp_member_t    *p_source;
    rtp_hdr_source_t *p_src_stats;
    int32_t         lost_diff, expected_diff, received_diff, receivedin2_diff;
    int32_t         actual_loss;
    uint32_t        total_expected, loss_fraction, loss_value;
    uint32_t        num_rr_records = 0;    
    /*
     * Scan through the list of senders for which we have to send rr reports.
     * The sender list is scanned in reverse order so that older sources
     * sources are reported earlier in the report.
     */
    VQE_TAILQ_FOREACH_REVERSE(p_source, &(p_sess->senders_list),
                              senders_list_t_, p_member_chain) {

        if (!p_source->rtp_src_addr) {
            /* we haven't seen an RTP packet yet, only RTCP */
            continue;
        }
        MCALL(p_sess, rtp_update_receiver_stats, p_source, reset_xr_stats);
        p_src_stats = &p_source->rcv_stats;
        /*
         * If there was some activity since the last report
         * or the last one, then we need to send a reception report.
         */
        received_diff = p_src_stats->received - p_src_stats->received_prior;
        receivedin2_diff = 
            p_src_stats->received - p_src_stats->received_penult;
        if (receivedin2_diff && (p_source != p_sess->rtp_local_source)) {

            if ((uint8_t *)(p_rr + 1) > p_end) {
                return (-1);
            }

            p_rr->rr_ssrc = htonl(p_source->ssrc);
            
            /*
             * Include cycles in the calculation and find the total loss.
             */
            total_expected = p_src_stats->cycles + p_src_stats->max_seq 
                - p_src_stats->base_seq + 1;
            expected_diff = total_expected - p_src_stats->expected_prior;
            lost_diff = expected_diff - received_diff;
            rtp_update_prior(p_src_stats);

            /*
             * The loss fraction is an 8-bit fixed point.
             */
            if ((expected_diff <= 0) || (lost_diff <= 0)) {
                loss_fraction = 0;
            } else {
                loss_fraction = (lost_diff << 8) / expected_diff;
            }
            actual_loss = total_expected - p_src_stats->received;
            loss_value = rtcp_fmt_loss_value(loss_fraction, actual_loss);
            p_rr->rr_loss = htonl(loss_value);
            
            RTP_LOG_PACKET_FV(p_sess, NULL,
                              "Sending RR for ssrc: 0x%08x\n", 
                              ntohl(p_rr->rr_ssrc));
            RTP_LOG_PACKET_FV(p_sess, NULL,
                              "cycles %d, max_seq %d, base_seq %d\n",
                              p_src_stats->cycles, 
                              p_src_stats->max_seq, 
                              p_src_stats->base_seq);
            RTP_LOG_PACKET_FV(p_sess, NULL,
                              "total_expected %u (%d), expected_diff %d\n",
                              total_expected, total_expected, expected_diff);
            RTP_LOG_PACKET_FV(p_sess, NULL,
                              "received_diff %d, lost_diff %d, "
                              "loss_fraction %u\n", 
                              received_diff, lost_diff, loss_fraction);
            RTP_LOG_PACKET_FV(p_sess, NULL,
                              "actual_loss %d, loss_value %u ((0x%08x)\n",
                              actual_loss, loss_value, loss_value);
            /*
             * Fill in other rr info.
             */
            p_rr->rr_jitter = htonl(rtp_get_jitter(p_src_stats));
            p_rr->rr_ehsr = htonl(p_src_stats->cycles + p_src_stats->max_seq);
            
            /* 
             * fill in the lsr and dlsr info, iff there has been
             * atleast one previous SR from this sender
             */
            if (!IS_ABS_TIME_ZERO(p_source->rcv_sr_ts)) {
                
                uint32_t dlsr;

                p_rr->rr_lsr = htonl(MFPTOFP(p_source->sr_ntp_h, 
                                             p_source->sr_ntp_l));  
                RTP_LOG_PACKET_FV(p_sess, NULL,
                                  "Calculated LSR: 0x%08x\n",
                                  ntohl(p_rr->rr_lsr));

               /*
                * temp_dlsr = (((uint64_t)(ELAPSED_TIME(p_source->rcv_sr_ts)) 
                *                              << 16) / 1000000);
                * Note the following math converts floating point seconds to
                * 32 bit NTP format (upper 16bit as seconds, lower 16 bits as
                * fraction. All it is doing is t * 2^^16/10^^6
                * 
                */              

                dlsr = (TIME_GET_R (usec,
                                    TIME_SUB_A_A(get_sys_time(), 
                                                 p_source->rcv_sr_ts))
                        << 16)/1000000;
                RTP_LOG_PACKET_FV(p_sess, NULL,
                                  "Calculated DLSR: 0x%08x or %f secs\n",
                                  dlsr, (float)dlsr/65536.0);
                p_rr->rr_dlsr = htonl(dlsr);
            } else {
                /* put "now" in lsr, for VQM purposes */
                ntp64_t now = get_ntp_time();
                p_rr->rr_lsr = htonl(MFPTOFP(now.upper, now.lower));
                p_rr->rr_dlsr = 0;
            }
            
            p_rr++;
            num_rr_records++;
            if (num_rr_records >= RTP_MAX_SENDERS) 
            break;
                
            }
        
    }
    return(num_rr_records);
}


/* 
 * Function:    rtcp_construct_one_sdes_element 
 * Description: Construct exactly one sdes type element in the RTCP packet
 * Parameters:  p_data   start of the SDES element in the packet buf
 *              p_source     ptr to rtp member
 *              sdes_type    SDES type
 * Returns:     Length of the SDES element created, or -1 if no room.
 */
static int rtcp_construct_one_sdes_element (uint8_t      *p_data, 
                                            int           bufflen,
                                            rtp_member_t *p_source,
                                            uint32_t     sdes_type) 
{
    uint16_t len = 0;
    
    /*
     * If we have an SDES elelment for the given type, 
     * add it to the data.
     */
    if (p_source->sdes[sdes_type]) {
        len = strlen(p_source->sdes[sdes_type]);
        if (len) {
            if (len + 2 > bufflen) {
                return (-1);
            }
            *p_data++ = sdes_type;
            *p_data++ = len;
            bcopy(p_source->sdes[sdes_type], p_data, len);
            len = len + 2;
        }
    }
    return(len);
}

/* 
 * Function:    rtcp_construct_sdes_report 
 * Description: Construct SDES elements as specified in the bitmask.
 * Parameters:  p_source pointer to local source member
 *              p_rtcp   pointer to start of rtcp packet in the buffer, should
 *                       be passed by the application
 *              sdes_mask the sdes items to be sent
 *                        (Bit 0 for SDES item1..... bit 6 for SDES 7.)
 * Returns:    Length of SDES in bytes, or zero if SDES will not fit.
 */
static uint32_t rtcp_construct_sdes_report (rtcptype     *p_rtcp, 
                                            uint32_t      bufflen,
                                            rtp_member_t *p_source,
                                            uint8_t       sdes_mask)
{
    uint16_t params = 0, total_len = 0, i , padding;
    int ilen = 0;
    uint8_t  *p_data;
    int bytes_remaining = bufflen;
    
    p_rtcp->ssrc = htonl(p_source->ssrc);
    
    params = rtcp_set_version(params);
    params = rtcp_set_count(params, 1);            /* One SDES element */
    params = rtcp_set_type(params, RTCP_SDES);
    p_rtcp->params = htons(params);

    p_data = (uint8_t *) (p_rtcp + 1);
    
    /* 
     * CNAME is compulsory. Bit 0 of sdes_mask denotes CNAME, 
     * bit 1 denotes NAME etc, bit 2 denotes etc......
     */
    for (i = 1; i <= RTCP_SDES_MAX; i++) {
        if (sdes_mask & 0x01) { 
            ilen = rtcp_construct_one_sdes_element(p_data, bytes_remaining,
                                                   p_source, i);
            if (ilen >= 0) {
                p_data += ilen;
                bytes_remaining -= ilen;
            } else {
                return (0);
            }
        }
        sdes_mask >>= 1;
    }
    
    total_len = p_data - (uint8_t *) p_rtcp;
    padding = 4 - (total_len & 3);
    if (bytes_remaining < padding) {
        return (0);
    }

    total_len += padding;               /* Add padding */
    p_rtcp->len = htons((total_len >> 2) - 1);

    while(padding-- > 0) {
        *p_data++ = RTCP_SDES_END;
    }
    
    return(total_len);
}

/*
 * Function:    rtcp_construct_bye
 * Description: Construct a bye packet
 * Parameters:  p_source pointer to local source member
 *              p_rtcp   pointer to start of rtcp packet in the buffer
 *              bufflen  size of buffer in bytes
 * Returns:     Length of bye packet created, or zero if no room.
 */
static uint32_t rtcp_construct_bye (rtcptype     *p_rtcp, 
                                    uint32_t      bufflen,
                                    rtp_member_t *p_source)
{
    uint16_t params = 0;
    if (sizeof(rtcptype) > bufflen) {
        return (0);
    }
    
    p_rtcp->ssrc = htonl(p_source->ssrc);
    
    params = rtcp_set_version(params);
    params = rtcp_set_count(params, 1);            /* One element */
    params = rtcp_set_type(params, RTCP_BYE);
    p_rtcp->params = htons(params);
    p_rtcp->len = htons(1);
    
    /*
     * Add bye reason later if we want.
     */ 
    return(sizeof(rtcptype));   
}


/* round up a length in bytes to the next multiple of 32 bits */
#define ROUNDUP_32(bytelen) (((bytelen) + 3) & ~(0x03))

/*
 * Function:    rtcp_construct_app
 * Description: construct an application specific RTCP packet
 * Parameters:  p_sess pointer to session object
 *              p_source pointer to local source member
 *              p_rtcp   pointer to start of rtcp packet in the buffer
 *              bufflen  length of buffer, in bytes
 *              p_app    pointer to app information 
 * 
 */
static uint32_t rtcp_construct_app (rtp_session_t *p_sess, 
                                    rtp_member_t *p_source,
                                    rtcptype *p_rtcp,
                                    uint32_t bufflen,
                                    rtcp_app_info_t *p_app)
{
    ushort params = 0;
    rtcp_app_t *p_apphdr;

    uint32_t applen = sizeof(rtcptype) + sizeof(rtcp_app_t) 
        + ROUNDUP_32(p_app->len);

    if (applen > bufflen) {
        return (0);
    }

    params = rtcp_set_version(params);
    params = rtcp_set_count(params, 1);
    params = rtcp_set_type(params, RTCP_APP);

    p_rtcp->params = htons(params);
    p_rtcp->ssrc = htonl(p_sess->rtp_local_source->ssrc);

    p_apphdr = (rtcp_app_t *)(p_rtcp + 1);
    p_rtcp->len = htons((applen >> 2) - 1);

    p_apphdr->name = p_app->name;
    bcopy(p_app->data, p_apphdr->data, p_app->len);

    return (applen);
}


/*
 * Function:    rtcp_construct_rtpfb
 * Description: Construct a RTPFB packet.  Arguments are in
 *              host byte order and will be converted to network byte
 *              order within the rtcp buffer.
 *              Only the "generic nack" format is currently supported.
 * Parameters:  p_source  --pointer to local source member
 *              buff      -- pointer to start of rtcp packet in the buffer
 *              bufflen   -- bytes available in the buffer.
 *              rtpfb_info -- pointer to RTPFB information.
 * Returns:     Length of nack packet created, note zero length
 *              indicates a failure.
 */
static uint32_t 
rtcp_construct_rtpfb (rtp_member_t *p_source,
                      uint8_t *buff,
                      uint32_t bufflen,
                      rtcp_rtpfb_info_t *rtpfb_info)
{
    rtcpfbtype_t  *p_rtcpfb=(rtcpfbtype_t *)buff;
    uint16_t params = 0;
    uint32_t i;
    uint32_t num_nacks = 0;
    uint32_t nack_data_len = 0;
    rtcp_rtpfb_generic_nack_t *p_nack_data;

    if (!p_rtcpfb || 
        !p_source ||
        !rtpfb_info ||
        rtpfb_info->fmt != RTCP_RTPFB_GENERIC_NACK ||
        !rtpfb_info->num_nacks ||
        !rtpfb_info->nack_data ||
        (bufflen < (RTCP_GENERIC_NACK_OVERHEAD + sizeof(uint32_t)))) {
        RTP_LOG_ERROR_F(NULL, p_source,
                        "Invalid params constructing RTPFB\n");
        return 0;
    }

    num_nacks = rtpfb_info->num_nacks;
    nack_data_len = 
        RTCP_GENERIC_NACK_OVERHEAD + (num_nacks * sizeof(uint32_t));
    if (nack_data_len > bufflen) {
        /*
         * Requested data won't fit into our remaining buffer space.
         * Debug an error and request what will fit.
         */
        RTP_LOG_ERROR_F(NULL, p_source, 
                        "too many Generic NACKs (%d) requested\n",
                        num_nacks);

        num_nacks = (bufflen - RTCP_GENERIC_NACK_OVERHEAD) / 4;
        nack_data_len = RTCP_GENERIC_NACK_OVERHEAD +
            (num_nacks * sizeof(uint32_t));
    }

    p_nack_data = (rtcp_rtpfb_generic_nack_t *)(p_rtcpfb->fci);
    
    p_rtcpfb->rtcp.ssrc = htonl(p_source->ssrc);
    p_rtcpfb->ssrc_media_sender = htonl(rtpfb_info->ssrc_media_sender);
    params = rtcp_set_version(params);
    params = rtcp_set_fmt(params, RTCP_RTPFB_GENERIC_NACK); 
    params = rtcp_set_type(params, RTCP_RTPFB);
    p_rtcpfb->rtcp.params = htons(params);
    p_rtcpfb->rtcp.len = htons((nack_data_len >> 2) - 1);
    for (i = 0; i < num_nacks; i++) {
        p_nack_data->pid = htons(rtpfb_info->nack_data[i].pid);
        p_nack_data->bitmask = htons(rtpfb_info->nack_data[i].bitmask);
        p_nack_data++;
    }
    
    return (nack_data_len);
}

/*
 * Function:    rtcp_construct_psfb
 * Description: Construct a PSFB packet.  Arguments are in
 *              host byte order and will be converted to network byte
 *              order within the rtcp buffer. 
 *              Only the "PLI Nack" is currently supported.
 * Parameters:  p_source  --pointer to local source member
 *              buff      --pointer to start of rtcp packet in the buffer
 *              bufflen   --buffer length remaining (in bytes)
 *              psfb_info --PSFB information, in host order.
 * Returns:     Length of "PLI nack" packet created
 */
static uint32_t 
rtcp_construct_psfb (rtp_member_t       *p_source,
                     uint8_t            *buff,
                     uint32_t            bufflen,
                     rtcp_psfb_info_t   *psfb_info)
{
    uint16_t params = 0;
    rtcpfbtype_t *p_rtcp=(rtcpfbtype_t *)buff;
    /* size of common feedback packet (RTPFB/PSFB), with no FCI */
    uint32_t pli_nack_len = sizeof(rtcpfbtype_t); 

    if (!p_rtcp || 
        !p_source || 
        !psfb_info ||
        psfb_info->fmt != RTCP_PSFB_PLI ||
        pli_nack_len > bufflen) {
        RTP_LOG_ERROR_F(NULL, p_source,
                        "Invalid params constructing PSFB (PLI NACK)\n");
        return 0;
    }
    
    p_rtcp->rtcp.ssrc = htonl(p_source->ssrc);
    p_rtcp->ssrc_media_sender = htonl(psfb_info->ssrc_media_sender);
    params = rtcp_set_version(params);
    params = rtcp_set_fmt(params, RTCP_PSFB_PLI);
    params = rtcp_set_type(params, RTCP_PSFB);
    p_rtcp->rtcp.params = htons(params);
    p_rtcp->rtcp.len = htons((pli_nack_len >> 2) - 1);

    return pli_nack_len;
}

#define LOSS_REPORT_FLAG 0x80
#define DUPLICATE_REPORT_FLAG 0x40
#define JITTER_FLAG 0x20

/*
 * Function:    trim_xr_chunks
 * Description: Remove N bytes from RLE chunks
 * Parameters:  p_xr    pointer to xr_stats structure
 *              length  Number of bytes to be removed
 * Returns:     Number of chunks after trimming
 * 
 */
static uint32_t trim_xr_chunks (rtcp_xr_stats_t *p_xr,
                                uint32_t length)
{
    uint32_t totals = 0;
    int i;
    int num_chunks;
    int chunks_to_rm = (length+1) / 2;

    num_chunks = p_xr->cur_chunk_in_use + 1;
    if (num_chunks % 2 != 0) {
        num_chunks++;
    }

    if (chunks_to_rm >= num_chunks) {
        /* Asked for more chunks we can trim
           so just return no trimming occurred */
        return 0;
    } else {
        p_xr->cur_chunk_in_use -= chunks_to_rm;
        for (i = 0; i <= p_xr->cur_chunk_in_use; i++) {
            if (p_xr->chunk[i] & INITIAL_BIT_VECTOR) {
                totals += MAX_BIT_IDX;
            } else {
                totals += p_xr->chunk[i] & RUN_LENGTH_MASK;
            }
        }

        p_xr->totals = totals;

        num_chunks = p_xr->cur_chunk_in_use + 1;
        if (num_chunks % 2 != 0) {
            num_chunks++;
        }

        return num_chunks;
    }
}


/*
 * Function:    rtcp_construct_xr
 * Description: construct a RTCP XR packet
 * Parameters:  p_sess pointer to session object
 *              p_source pointer to local source member
 *              p_rtcp   pointer to start of rtcp packet in the buffer
 *              bufflen  length of buffer, in bytes
 * Returns:     Length of "RTCP XR" packet created
 * 
 */
static uint32_t rtcp_construct_xr (rtp_session_t *p_sess,
                                   rtp_member_t *p_source,
                                   rtcptype *p_rtcp,
                                   uint32_t bufflen)
{
    uint16_t params = 0;
    rtp_member_t *p_src;
    rtcp_xr_report_type_t type;
    rtcp_xr_gen_t *p_xr_block;
    rtcp_xr_loss_rle_t *p_loss;
    rtcp_xr_stat_summary_t *p_stats;
    int total_len = sizeof(rtcptype);
    int loss_rle_len;
    int new_len;
    int trim_len;
    uint8_t report_flags = 0;
    uint32_t i;
    uint16_t *p_chunk;
    rtcp_xr_stats_t *p_xr = NULL;
    uint16_t num_chunks;
    uint16_t length;
    uint16_t begin_seq;
    uint16_t end_seq;
    uint16_t data_len = 0;


    /* check whether we still have the room to create the xr report */
    if (total_len > bufflen) {
        return (0);
    }

    if (!p_source || !p_rtcp) {
        RTP_LOG_ERROR_F(p_sess, p_source,
                        "Invalid params constructing RTCP XR\n");
        return 0;
    }

    params = rtcp_set_version(params);
    params = rtcp_set_count(params, 1);
    params = rtcp_set_type(params, RTCP_XR);
    p_rtcp->params = htons(params);
    p_rtcp->ssrc = htonl(p_source->ssrc);
    p_xr_block = (rtcp_xr_gen_t *) (p_rtcp + 1);

    /*
     * First build the XR MA report. This is not done in the for loop
     * below, because it is not sent "per-sender". It is per repair 
     * session.
     */
    if (is_rtcp_xr_ma_enabled(&p_sess->rtcp_xr_cfg)) {
        if (p_sess->xr_ma_stats.to_be_sent &&
            !p_sess->xr_ma_sent) {
            data_len = 0;
            uint32_t ssrc = 0;

            /* If there are multiple senders, pick the SSRC of the first 
             * remote sender. If there are no senders, include SSRC of 0. If
             * someday the SSRC is provisioned in the SDP, we could use that
             * here instead of dynamically learning the prim multicast SSRC */
            VQE_TAILQ_FOREACH(p_src, &(p_sess->senders_list), p_member_chain) {
                if (p_src != p_sess->rtp_local_source) {		  
                    ssrc = p_src->ssrc;
		    break;
                }
            }

            data_len = rtcp_xr_encode_ma(&p_sess->xr_ma_stats,
                                         ssrc,
                                         (uint8_t*)p_xr_block,
                                         bufflen - total_len);
            if (data_len == 0) {
                RTP_SYSLOG(RTCP_XR_NOT_INCLUDED_WARN, RTCP_XR_MA);
            } else {
                total_len += data_len;
                p_xr_block = (rtcp_xr_gen_t *)((uint8_t*)p_xr_block 
                                               + data_len);
                
                /*Only send each XR MA report once*/ 
                p_sess->xr_ma_sent = TRUE;
            }
        }
    }

    /*
     * Scan through the list of senders for which we have to send rr reports.
     * The sender list is scanned in reverse order so that older sources
     * sources are reported earlier in the report.
     */
    VQE_TAILQ_FOREACH_REVERSE(p_src, &(p_sess->senders_list), 
                              senders_list_t_, p_member_chain) {

        for (type = RTCP_XR_DC;
             type >= RTCP_XR_LOSS_RLE;
             type--) {
            p_xr = p_src->xr_stats;
            switch (type) {
                case RTCP_XR_DC:       
                    if (is_rtcp_xr_dc_enabled(&p_sess->rtcp_xr_cfg) &&
                        (p_sess->xr_dc_stats.is_updated)) {
                        data_len = rtcp_xr_encode_dc(&p_sess->xr_dc_stats,
                                                     htonl(p_src->ssrc),
                                                     (uint8_t*)p_xr_block,
                                                     bufflen - total_len);
              
                        total_len+=data_len;
                        p_xr_block = (rtcp_xr_gen_t *)((uint8_t*)p_xr_block 
                                     + ((ntohs(p_xr_block->length) + 1) << 2));
                    }
                    break;
                case RTCP_XR_POST_RPR_LOSS_RLE:
                    /* Share the same code with construction of Loss RLE */
                    if (p_src->post_er_stats && 
                        is_post_er_loss_rle_enabled(&p_sess->rtcp_xr_cfg)) {
                        p_xr = &p_src->post_er_stats->xr_stats;
                    } else {
                        p_xr = NULL;
                    }

                    /* Let it fall through to share the same code with
                       pre-repair Loss RLE construction */

                case RTCP_XR_LOSS_RLE:

                    /* Reset the pointer to XR stats data if this is for
                       pre-repair */
                    if (type == RTCP_XR_LOSS_RLE) {
                        if(p_sess->rtcp_xr_cfg.max_loss_rle > 1) {
                            p_xr = p_src->xr_stats;
                        } else {
                            p_xr = NULL;
                        }
                    }

                    /* Only construct the report if we have data
                       or XR is not empty.
                       Note: In the case of empty XR data,
                       we will not generate XR report. */
                    if (p_xr && (p_xr->totals != 0)) {
                        /* If the last chunk is empty bit vector, re-adjust
                           the current chunk index */
                        if (p_xr->chunk[p_xr->cur_chunk_in_use] ==
                            INITIAL_BIT_VECTOR) {
                            p_xr->cur_chunk_in_use--;
                        }
                        num_chunks = p_xr->cur_chunk_in_use + 1;

                        /* num_chunks in RLE report must be a even number.
                           If not, a null chunk must be padded per RFC 3611. */
                        if (num_chunks % 2 != 0) {
                            num_chunks++;
                        }
                        RTP_LOG_PACKET_FV(p_sess, p_src,
                                          "Total no. of chunks for RLE %d\n",
                                          num_chunks);
                        loss_rle_len = sizeof(rtcp_xr_loss_rle_t) 
                            + (num_chunks * sizeof(uint16_t));
                        new_len = total_len + loss_rle_len;
                        if (new_len > bufflen) {
                            RTP_SYSLOG(RTCP_XR_EXCEED_LIMIT_INFO,
                                       type, loss_rle_len,
                                       bufflen-total_len);
                            trim_len = new_len - bufflen;
                            /* Trim off trim_len in Loss RLE */
                            num_chunks = trim_xr_chunks(p_xr, trim_len);
                            if (num_chunks == 0) {
                                /* Nothing can be trimmed so skip it */
                                RTP_SYSLOG(RTCP_XR_NOT_INCLUDED_WARN, type);
                                break;
                            } else {
                                loss_rle_len = sizeof(rtcp_xr_loss_rle_t) 
                                    + (num_chunks * sizeof(uint16_t));
                                total_len += loss_rle_len; 
                            }
                        } else {
                            total_len = new_len;
                        }

                        p_loss = (rtcp_xr_loss_rle_t *)p_xr_block;
                        p_loss->bt = type;
                        p_loss->type_specific = htons(0);
                        /* The value of length for RLE report should be
                           num_chunks/2 plus one 32-bit for ssrc and,
                           one 32-bit for [begin_seq, end_seq] */
                        length = ((sizeof(rtcp_xr_loss_rle_t) +
                                   (num_chunks * sizeof(uint16_t))) >> 2) - 1;
                        p_loss->length = htons(length);
                        p_loss->xr_ssrc = htonl(p_src->ssrc);
                        begin_seq = p_xr->eseq_start;
                        p_loss->begin_seq = htons(begin_seq);
                        end_seq = p_xr->eseq_start + p_xr->totals;
                        p_loss->end_seq = htons(end_seq);
                        
                        p_chunk = (uint16_t *) p_loss->chunk;
                        for (i = 0; i <= p_xr->cur_chunk_in_use; i++) {
                            *p_chunk = htons(p_xr->chunk[i]);
                            p_chunk++;
                        }

                        if (p_xr->cur_chunk_in_use % 2 == 0) {
                            /* Add a null chunk if the number of chunks is 
                               odd number */
                            *p_chunk = htons(0);
                        }

                        p_xr_block = (rtcp_xr_gen_t *)((uint8_t *)p_xr_block
                                   + ((ntohs(p_xr_block->length) + 1) << 2));

                        /* Output the unreported counters */
                        if (p_xr->exceed_limit) {
                            RTP_SYSLOG(RTCP_XR_NOT_REPORTED_INFO,
                                       p_xr->not_reported,
                                       "after data exceeding the "
                                       "internal limits");

                            if (p_xr->before_intvl) {
                                RTP_SYSLOG(RTCP_XR_NOT_REPORTED_INFO,
                                           p_xr->before_intvl,
                                           "before the beginning sequence");
                            }

                            if (p_xr->re_init) {
                                RTP_SYSLOG(RTCP_XR_NOT_REPORTED_INFO,
                                           p_xr->re_init,
                                           "from previous report interval");
                            }
                        }
                    }

                    break;

                case RTCP_XR_STAT_SUMMARY:

                    /* Only construct the report if required */
                    if ((p_sess->rtcp_xr_cfg.ss_loss ||
                         p_sess->rtcp_xr_cfg.ss_dup ||
                         p_sess->rtcp_xr_cfg.ss_jitt) && 
                        p_xr &&
                        (p_xr->totals != 0)) {
                        total_len += sizeof(rtcp_xr_stat_summary_t);
                        if (total_len > bufflen) {
                            total_len -= sizeof(rtcp_xr_stat_summary_t);
                            RTP_SYSLOG(RTCP_XR_NOT_INCLUDED_WARN, type);
                            break;
                        }

                        p_stats = (rtcp_xr_stat_summary_t *)p_xr_block;
                        p_stats->bt = type;
                        p_stats->length = htons(9);

                        if (p_sess->rtcp_xr_cfg.ss_loss) {
                            report_flags |= LOSS_REPORT_FLAG;
                            p_stats->lost_packets = htonl(p_xr->lost_packets);
                        } else {
                            p_stats->lost_packets = htonl(0);
                        }
                    

                        if (p_sess->rtcp_xr_cfg.ss_dup) {
                            report_flags |= DUPLICATE_REPORT_FLAG;
                            p_stats->dup_packets = htonl(p_xr->dup_packets);
                        } else {
                            p_stats->dup_packets = htonl(0);
                        }

                        if (p_sess->rtcp_xr_cfg.ss_jitt) {
                            report_flags |= JITTER_FLAG;

                            p_stats->min_jitter = htonl(p_xr->min_jitter);
                            p_stats->max_jitter = htonl(p_xr->max_jitter);
                            p_stats->mean_jitter = 
                                htonl(rtcp_xr_get_mean_jitter(p_xr));
                            p_stats->dev_jitter = 
                                htonl(rtcp_xr_get_std_dev_jitter(p_xr));
                        } else {
                            p_stats->min_jitter = htonl(0);
                            p_stats->max_jitter = htonl(0);
                            p_stats->mean_jitter = htonl(0);
                            p_stats->dev_jitter = htonl(0);
                        }

                        p_stats->xr_ssrc = htonl(p_src->ssrc);
                        begin_seq = p_xr->eseq_start;
                        p_stats->begin_seq = htons(begin_seq);
                        end_seq = p_xr->eseq_start + p_xr->totals;
                        p_stats->end_seq = htons(end_seq);
                        p_stats->type_specific = report_flags;

                        p_stats->min_ttl_or_hl = 0;
                        p_stats->max_ttl_or_hl = 0;
                        p_stats->mean_ttl_or_hl = 0;
                        p_stats->dev_ttl_or_hl = 0;

                        p_xr_block = (rtcp_xr_gen_t *)((uint8_t *)p_xr_block
                                   + ((ntohs(p_xr_block->length) + 1) << 2));
                    }

                    break;
 
            default:
                    break;
            }
        }
    }
    p_rtcp->len = htons((total_len >> 2) - 1);
    return (total_len);
}


/*
 * rtcp_get_rcvr_rtcpbi_value
 *
 * Return a per-receiver bandwidth value for the RTCP Bandwidth Indication
 * subreport, in host order, from per-session information.
 */

static rtcp_bw_bi_t rtcp_get_rcvr_rtcpbi_value (rtp_session_t *p_sess)
{
    rtcp_bw_role_t *role_bw;
    double d_per_rcvr_bw;  /* in bps */
    uint32_t members;     /* total number of members in the session */
    uint32_t receivers;   /* total number of receivers in the session */

    role_bw = &(p_sess->rtcp_bw.rcvr);
    if (role_bw->cfg_per_member_bw != RTCP_BW_UNSPECIFIED) {
        d_per_rcvr_bw = (double)(role_bw->cfg_per_member_bw);
    } else if (role_bw->tot_role_bw != RTCP_BW_UNSPECIFIED) {
        /*
         * Compute per-receiver bandwith by dividing the total
         * receiver bandwidth by the total number of receivers:
         * . the total number of receivers is the total number of
         *   members minus the numbers of senders; but assume there is
         *   least one receiver, to avoid division by zero.
         * . the total number of members in the session is the number
         *   of members observed via RTCP messages (rtcp_nmembers),
         *   plus the numbers of members learned about from disjoint
         *   feedback targets (if any) via Group and Average Packet Size
         *   subreports (rtcp_nmembers_learned).
         * . note that except at a Distribution Source with disjoint 
         *   Feedback Targets, rtcp_nmembers_learned will be zero.
         */
        members = p_sess->rtcp_nmembers + p_sess->rtcp_nmembers_learned;
        if (members > p_sess->rtcp_nsenders) {
            receivers = members - p_sess->rtcp_nsenders;
        } else {
            receivers = 1;
        }
        d_per_rcvr_bw = (double)(role_bw->tot_role_bw) / (double)(receivers);
    } else {
        d_per_rcvr_bw = (double)RTCP_DFLT_PER_RCVR_BW; 
    }
    /*
     * convert a double value in bps
     * to a 16:16 fixed point value, in kbps 
     */
    return (rtcp_bw_dbps_to_bi(d_per_rcvr_bw));
}

/*
 * Function:    rtcp_construct_rsi
 * Description: Construct a RSI report
 *              Note: Currently only Group and Average Packet Size Sub Report
 *              Block (Need to add collision summary also )
 * Parameters:  p_ssm_rsi_source  pointer to session object
 *              p_source          pointer to local source member
 *              subrpt_mask       Mask indicating which subreports to include.
 *              p_rtcp   pointer to start of rtcp packet in the buffer
 * Returns:     Length of RSI packet, or zero if no room in the buffer.
 */

uint32_t rtcp_construct_rsi (rtp_session_t *p_sess,
                             rtp_member_t *p_source,
                             rtcptype *p_rtcp,
                             uint32_t bufflen,
                             rtcp_rsi_subrpt_mask_t subrpt_mask)
{
    ushort params = 0;
    rtcp_rsi_t *p_rsi;
    ntp64_t      ntp_ts;
    rtcp_rsi_gen_subrpt_t *p_subrpt;
    rtcp_rsi_gaps_subrpt_t *p_gaps_subrpt;
    rtcp_rsi_rtcpbi_subrpt_t *p_rtcpbi_subrpt;
    uint32_t length_in_bytes;
    uint32_t subrpt_length_in_bytes;
    rtcp_rsi_subrpt_t subrpt_type;
    uint32_t bytes_remaining = bufflen;

    /* minimum length of RSI packet */
    length_in_bytes = (sizeof(rtcptype) + sizeof(rtcp_rsi_t));
    if (length_in_bytes > bytes_remaining) {
        return (0);
    }

    params = rtcp_set_version(params);
    params = rtcp_set_count(params, 0);
    params = rtcp_set_type(params, RTCP_RSI);
    p_rtcp->params = htons(params);
    /* set p_rtcp->len later, when we've finished adding sub-reports. */

    p_rtcp->ssrc = htonl(p_source->ssrc);

    p_rsi = (rtcp_rsi_t *)(p_rtcp + 1);
    p_subrpt = (rtcp_rsi_gen_subrpt_t *)p_rsi->data;

    p_rsi->summ_ssrc = htonl(p_source->ssrc);
    ntp_ts = get_ntp_time();
    p_rsi->ntp_h = htonl(ntp_ts.upper);
    p_rsi->ntp_l = htonl(ntp_ts.lower);

    for (subrpt_type = rtcp_rsi_get_first_subrpt(subrpt_mask) ;
         subrpt_type != NOT_AN_RSI_SUBRPT ;
         subrpt_type = rtcp_rsi_get_next_subrpt(subrpt_mask, subrpt_type)) {
        switch (subrpt_type) {
        case RTCP_RSI_GAPSB:
            subrpt_length_in_bytes = sizeof(rtcp_rsi_gaps_subrpt_t);
            if (subrpt_length_in_bytes > bytes_remaining) {
                return (0);
            }
            p_gaps_subrpt = (rtcp_rsi_gaps_subrpt_t *)p_subrpt;
            p_gaps_subrpt->average_packet_size = 
                htons((uint16_t)(p_sess->rtcp_stats.avg_pkt_size));
            p_gaps_subrpt->group_size = htonl(p_sess->rtcp_nmembers);
            /* $$$ this doesn't work for a DS with a disjoint FBT,
               where it should be rtcp_nmembers + rtcp_nmembers_learned */
            break;
        case RTCP_RSI_BISB:
            subrpt_length_in_bytes = sizeof(rtcp_rsi_rtcpbi_subrpt_t);
            if (subrpt_length_in_bytes > bytes_remaining) {
                return (0);
            }
            p_rtcpbi_subrpt = (rtcp_rsi_rtcpbi_subrpt_t *)p_subrpt;
            p_rtcpbi_subrpt->role = htons(RTCP_RSI_BI_RECEIVERS);
            p_rtcpbi_subrpt->rtcp_bandwidth =
                htonl(rtcp_get_rcvr_rtcpbi_value(p_sess));
            break;
        default:
            subrpt_length_in_bytes = 0;
            RTP_LOG_ERROR_F(p_sess, p_source,
                            "Attempt to format unsupported RTCP RSI "
                            "sub report block (type %d)\n",
                            subrpt_type);
            break;
        }
        p_subrpt->srbt = subrpt_type;
        p_subrpt->length = subrpt_length_in_bytes / 4;
        p_subrpt = (rtcp_rsi_gen_subrpt_t *)((uint8_t *)p_subrpt 
                                             + subrpt_length_in_bytes);
        length_in_bytes += subrpt_length_in_bytes;
        bytes_remaining -= subrpt_length_in_bytes;
    }
    p_rtcp->len = htons((length_in_bytes / 4) - 1);

    return (length_in_bytes);
}

/*
 * Function:    rtcp_construct_pubports
 * Description: Construct a PUBPORTS msg.  Arguments are in
 *              host byte order and will be converted to network byte
 *              order within the rtcp buffer.
 * Parameters:  p_source:       pointer to local source member
 *              buff:           buffer in which to store the constructed packet
 *              bufflen:        length of buffer
 *              pubports_info:  pointer to PUBPORTS info
 *              p_rtcp   pointer to start of rtcp packet in the buffer
 * Returns:     Length of pubport packet created
 */
static uint32_t 
rtcp_construct_pubports (const rtp_member_t *p_source,
                         uint8_t      *buff,
                         uint32_t     bufflen,
                         rtcp_pubports_info_t *p_pubports_info)
{
    uint16_t params = 0;
    rtcptype *p_rtcp = (rtcptype *)buff;
    rtcp_pubports_t *p_pubports = NULL;
    uint32_t pubports_len = sizeof(rtcptype) + sizeof(rtcp_pubports_t);

    if (!p_rtcp || !p_source || (pubports_len > bufflen)) {
        return (0);
    }
    
    params = rtcp_set_version(params);
    params = rtcp_set_count(params, 1);         /* One element */
    params = rtcp_set_type(params, RTCP_PUBPORTS);
    p_rtcp->params = htons(params);
    p_rtcp->len = htons((pubports_len / 4) - 1);
    p_rtcp->ssrc = htonl(p_source->ssrc);

    p_pubports = (rtcp_pubports_t *)(p_rtcp + 1);
    p_pubports->ssrc_media_sender = htonl(p_pubports_info->ssrc_media_sender);
    p_pubports->rtp_port = htons(p_pubports_info->rtp_port);
    p_pubports->rtcp_port = htons(p_pubports_info->rtcp_port);

    return (pubports_len);
}

/*
 * Function:    rtcp_construct_report_base
 *
 * Description: Construct Report
 *
 * Parameters:  p_sess     --pointer to session object
 *              p_source   --pointer to local source member
 *              p_rtcp     --pointer to start of rtcp packet in the buffer
 *              bufflen    --length of supplied buffer, in bytes
 *              p_pkt_info --pointer to additional packet information
 *              reset_xr_stats --flag for whether to reset RTCP XR stats
 * Returns:     length of the packet including RTCP header
 * NOTES:
 * - Either a sender report or a receive report will be sent depending on 
 *   whether the source (mostly the local source) has sent any data or not.
 * - Note that in case rtcp-rsize is enabled, RR will not be sent. 
 *   (SR may be. Note that its typically SDES+other packets such as PUBPORTS)
 * - The application should control the socket on which the packet is to be
 *   sent
 * - The buffer for packet should be allocated big enough to allow for fitting 
 *   biggest RTCP packet (done by application)
 */
uint32_t rtcp_construct_report_base (rtp_session_t   *p_sess,
                                     rtp_member_t    *p_source, 
                                     rtcptype        *p_rtcp,
                                     uint32_t         bufflen,
                                     rtcp_pkt_info_t *p_pkt_info,
                                     boolean          reset_xr_stats)
{
    uint16_t   params = 0, total_len = 0, len=0;
    int num_rr_blocks = 0;
    rtcp_sr_t *p_sr;
    rtcp_rr_t *p_rr;
    rtcp_msg_info_t *p_msg_info;
    uint8_t    sdes_mask;
    uint8_t   *p_data;
    ntp64_t   ntp_now;
    uint8_t   *p_end = NULL;
    char      *p_pkttype = "";
    uint8_t got_pkt_info, rtcp_rsize_enabled;
 
    /*
     * Sanity checks
     */
    if (!p_source || !p_sess || !p_rtcp)
        return(0);

    MCALL(p_sess, rtp_update_sender_stats, p_source);
    
    p_end = ((uint8_t *)p_rtcp) + bufflen;
    if ((uint8_t *)(p_rtcp + 1) > p_end) {
        p_pkttype = "RTCP header";
        goto no_room;
    }

    /*
     * Fill in ssrc.
     */
    params = rtcp_set_version(params);
    p_rtcp->ssrc = htonl(p_source->ssrc);  
    total_len = sizeof(rtcptype);
    
    rtcp_rsize_enabled = FALSE;
    got_pkt_info = rtcp_get_rtcp_rsize(p_pkt_info, &rtcp_rsize_enabled);
    /*
     * If we have sent data packets since the last report,
     * we need to send a sender report. Otherwise, we need to
     * send a receiver report.
     */
    if (p_source->packets != p_source->packets_prior) {
        
        /* 
         * Check if the local member is marked as a sender. If not
         * add to the sender list and mark as sender
         */
        if (!p_sess->we_sent) {
            rtp_session_add_sender(p_sess,p_source);
            p_sess->we_sent = TRUE;
        }

        p_sr = (rtcp_sr_t *) (p_rtcp + 1);
        if ((uint8_t *)(p_sr + 1) > p_end) {
            p_pkttype = "SR";
            goto no_room;
        }

        /*
         * Set the NTP timestamp 
         */
        ntp_now = get_ntp_time();
        p_sr->sr_ntp_h = htonl(ntp_now.upper);
        p_sr->sr_ntp_l = htonl(ntp_now.lower);
        RTP_LOG_PACKET_FV(p_sess, p_source,
                          "Setting NTP time in SR: 0x%08x/0x%08x\n",
                          ntp_now.upper, ntp_now.lower);
        /*
         * Set the media timestamp --
         * this is derived from the NTP/RTP values for the
         * last data pkt sent, supplied by the data plane,
         * and the current time. 
         */
        uint32_t rtp_ts = p_source->rtp_timestamp;
        rel_time_t r = TIME_SUB_A_A(ntp_to_abs_time(ntp_now),
                                    ntp_to_abs_time(p_source->ntp_timestamp));
        rtp_ts += (uint32_t)(rel_time_to_pcr(r));
        p_sr->sr_timestamp = htonl(rtp_ts);

        /*
         * Update packet/byte stats.
         */
        p_sr->sr_npackets = htonl(p_source->packets);
        p_sr->sr_nbytes = htonl(p_source->bytes);
        
        /*
         * Add RTCP SR type to params and set the next data ptr to 
         * add reception stats for other senders.
         */

        params = rtcp_set_type(params, RTCP_SR);
        p_rr = (rtcp_rr_t *) (p_sr + 1);
        
    } else {
        /*
         * It is a reception report.
         * Add RTCP RR type to params and set the next data ptr to 
         * add reception stats for other senders.
         */
        p_rr = (rtcp_rr_t *) p_rtcp;
        if (!(got_pkt_info & (rtcp_rsize_enabled))) {
            params = rtcp_set_type(params, RTCP_RR);
            p_rr = (rtcp_rr_t *) (p_rtcp + 1);
        }
    }
    if (!(got_pkt_info & (rtcp_rsize_enabled))) {
        /*
         * Add in the rr_blocks and update RTCP params.
         */
        num_rr_blocks = rtcp_construct_rr_block(p_rr, p_sess, p_end, 
                                                reset_xr_stats);
        if (num_rr_blocks < 0) {
            p_pkttype = "RR";
            goto no_room;
        }
    }

    params = rtcp_set_count(params, num_rr_blocks);
    p_rtcp->params = htons(params);
    p_data = (uint8_t *) (p_rr + num_rr_blocks);  
    
    total_len = p_data - (uint8_t *) p_rtcp;
    p_rtcp->len = htons((total_len >> 2) - 1);
    
    /*
     * the constraints on the order of packets in a compound packet are:
     * - RR or SR must be first. (and not-present if rtcp-rsize is enabled)
     * - BYE (if present) must be last
     * - PUBPORTS (if present) must precede other feedback packets (RTPFB/PSFB)
     * So we generally put packets in a compound packet in type number order,
     * EXCEPT that BYE is last, and we put PUBPORTS after SDES (i.e., we use
     * use packet number order, except that BYE and PUBPORTS switch places)
     */
     
    /*
     * Add in SDES, with a (mandatory) CNAME item.
     * $$$ temporarily, we're not supporting the formatting of other items.
     */
    sdes_mask = 0x1 << (RTCP_SDES_CNAME - 1);
    if (sdes_mask) {
        len = rtcp_construct_sdes_report((rtcptype *) p_data, 
                                         bufflen - total_len,
                                         p_source, sdes_mask);
        if (len) {
            total_len += len;
            p_data += len;
        } else {
            p_pkttype = "SDES";
            goto no_room;
        }
    }

    /* Add PUBPORTS, if specified */
    p_msg_info = rtcp_get_pkt_info(p_pkt_info, RTCP_PUBPORTS);
    if (p_msg_info) {
        len = rtcp_construct_pubports(p_source, 
                                      p_data, 
                                      bufflen - total_len,
                                      &(p_msg_info->pubports_info));
        if (len) {
            total_len += len;
            p_data += len;
        } else {
            p_pkttype = "PUBPORTS";
            goto no_room;
        }
    }

    /* Add APP, if specified */
    p_msg_info = rtcp_get_pkt_info(p_pkt_info, RTCP_APP);
    if (p_msg_info) {
        len = rtcp_construct_app(p_sess, 
                                 p_source,
                                 (rtcptype *)p_data, 
                                 bufflen - total_len,
                                 &(p_msg_info->app_info));
        if (len) {
            total_len += len;
            p_data += len;
        } else {
            p_pkttype = "APP";
            goto no_room;
        }
    }

    /* Add RTPFB, if specified */
    p_msg_info = rtcp_get_pkt_info(p_pkt_info, RTCP_RTPFB);
    if (p_msg_info) {
        len = rtcp_construct_rtpfb(p_source,
                                   p_data, 
                                   bufflen - total_len,
                                   &(p_msg_info->rtpfb_info));
        if (len) {
            total_len += len;
            p_data += len;
        } else {
            p_pkttype = "RTPFB";
            goto no_room;
        }
    }
        
    /* Add PSFB, if specified */
    p_msg_info = rtcp_get_pkt_info(p_pkt_info, RTCP_PSFB);
    if (p_msg_info) {
        len = rtcp_construct_psfb(p_source,
                                  p_data,
                                  bufflen - total_len,
                                  &(p_msg_info->psfb_info));
        if (len) {
            total_len += len;
            p_data += len;
        } else {
            p_pkttype = "PSFB";
            goto no_room;
        }
    }

    /* Add RSI, if specified. */
    p_msg_info = rtcp_get_pkt_info(p_pkt_info, RTCP_RSI);
    if (p_msg_info) {
        len = rtcp_construct_rsi(p_sess,
                                 p_source,
                                 (rtcptype *)p_data,
                                 bufflen - total_len,
                                 p_msg_info->rsi_info.subrpt_mask);
        if (len) {
            total_len += len;
            p_data += len;
        } else {
            p_pkttype = "RSI";
            goto no_room;
        }
    }

    /* Add RTCP XR, if specified */
    p_msg_info = rtcp_get_pkt_info(p_pkt_info, RTCP_XR);
    if (p_msg_info) {
        /* We will leave some room for possible bye message at the end here */
        len = rtcp_construct_xr(p_sess,
                                p_source,
                                (rtcptype *)p_data, 
                                bufflen - total_len - sizeof(rtcptype));
        if (len) {
            total_len += len;
            p_data += len;
        } else {
            RTP_LOG_ERROR_F(p_sess, p_source,
                            "Failed to add XR to RTCP compound pkt: "
                            "buffer size: %d, available bytes: %d\n",
                            bufflen, bufflen - total_len);
        }
    }

    /* Add BYE, if specified. */
    p_msg_info = rtcp_get_pkt_info(p_pkt_info, RTCP_BYE);
    if (p_msg_info) {
        len = rtcp_construct_bye((rtcptype *) p_data, bufflen - total_len,
                                 p_source);
        if (len) {
            total_len += len;
            p_data += len;
        } else {
            p_pkttype = "BYE";
            goto no_room;
        }
    }
 
    return(total_len);

 no_room:

    RTP_LOG_ERROR_F(p_sess, p_source,
                    "Failed to add %s to RTCP compound pkt: "
                    "buffer size: %d, available bytes: %d\n",
                    p_pkttype, bufflen, bufflen - total_len);
    return (0);
}

/*
 * rtcp_get_td_intvl_params
 *
 * Return Td (the deterministic RTCP interval) and parameters
 * associated with the calculation.
 *
 * Parameters:
 *
 * p_sess                 -- pointer to per-session information
 * we_sent                -- TRUE, if this participant sent RTP data 
 *                           during the past RTCP interval, else FALSE.
 * (the remaining parameters are optional output; output is supplied
 *  if a paramter is non-NULL)
 * calc_td_secs           -- ptr to calculated Td (in secs)
 * calc_rtcp_bw           -- ptr to RTCP bandwidth used in the calculation 
 *                           (in bytes/sec)
 * calc_rtcp_avg_size     -- ptr to RTCP avg pkt size used in the calculation.
 *                           (in bytes)
 * calc_members           -- ptr to no. of members used in the calculation
 */
static void 
rtcp_get_td_intvl_params (rtp_session_t    *p_sess,
                          boolean          we_sent,
                          double           *calc_td_secs,
                          double           *calc_rtcp_bw,
                          double           *calc_rtcp_avg_size,
                          int              *calc_members)
{
    double                      td_secs;      /* Td (deterministic intvl) */
    rtcp_intvl_calc_sess_info_t sinfo;
    rtcp_intvl_calc_params_t    params;

    /*
     * Gather the per-session info we need for the
     * interval computation; if we have received Group and
     * and Average Packet Size subreports, use those values
     * along with our local observations.
     */
    sinfo.we_sent = we_sent;
    sinfo.initial = p_sess->initial;
    sinfo.members = p_sess->rtcp_nmembers + p_sess->rtcp_nmembers_learned;
    sinfo.senders = p_sess->rtcp_nsenders;
    sinfo.rtcp_avg_size = p_sess->rtcp_stats.avg_pkt_size;
    sinfo.rtcp_avg_size_sent = p_sess->rtcp_stats.avg_pkt_size_sent;

    /*
     * Determine the parameters for interval calculation
     * from RTCP bandwidth info and other per-session info.
     */
    rtcp_get_intvl_calc_params(&p_sess->rtcp_bw,
                               &sinfo,
                               &params);
    /*
     * Find Td (the deterministic interval), 
     * and parameters used in the calculation.
     */
    td_secs = rtcp_td_interval(params.members,
                               params.senders,
                               params.rtcp_bw,
                               params.rtcp_sender_bw_fraction,
                               params.we_sent,
                               params.rtcp_avg_size,
                               params.initial,
                               calc_rtcp_bw,
                               calc_members);
    if (calc_td_secs) {
        *calc_td_secs = td_secs;
    }
    if (calc_rtcp_avg_size) {
        *calc_rtcp_avg_size = params.rtcp_avg_size;
    }
}

/*
 * rtcp_get_td_interval
 *
 * Return the deterministic RTCP interval (Td)
 *
 * Parameters:
 * p_sess                 -- ptr to per-session information
 * we_sent                -- TRUE, if this participant sent RTP data 
 *                           during the past RTCP interval, else FALSE.
 * Returns:               -- Td, in seconds
 */
static double
rtcp_get_td_interval (rtp_session_t *p_sess, boolean we_sent)
{
    double td_secs = 0.;

    rtcp_get_td_intvl_params(p_sess, we_sent, &td_secs, NULL, NULL, NULL);

    return (td_secs);
}

/*
 * rtcp_may_send
 *
 * Returns TRUE if the current session participant may send an RTCP message.
 * This is determined by:
 * . checking the RTCP bandwidth available to the participant 
 *   (which depends on its role, and configured and received RTCP bandwidth)
 * . checking the (send) socket descriptor.
 * . checking the destination address and port associated with the socket.
 *
 * Parameters:
 * p_sess                 -- ptr to per-session information
 * Returns:               -- TRUE, if an RTCP packet may be sent, else FALSE.
 */
boolean rtcp_may_send (rtp_session_t *p_sess)
{
    double rtcp_bw = 0.;

    rtcp_get_td_intvl_params(p_sess, p_sess->we_sent, 
                             NULL, &rtcp_bw, NULL, NULL);

    return ((rtcp_bw > 0.) && 
            (p_sess->rtcp_socket != -1) &&
            (p_sess->send_addrs.dst_addr) &&
            (p_sess->send_addrs.dst_port));
}

/*
 * Function:    rtcp_send_report_base
 *
 * Description: Send an RTCP report in an IP/UDP datagram and returns the 
 *              length sans IP and UDP headers. This function calls 
 *              rtcp_construct_report to construct a  compound RTCP packet and 
 *              tries to transmit the packet to the rtcp socket. Look at the 
 *              function rtcp_construct_report for  more details.
 *
 * Parameters:  p_sess      --pointer to session object
 *              p_source    --pointer to local source member
 *              p_pkt_info  --pointer to additional packet info
 *              re_schedule --flag for wheter to re-schedule the timer
 *              reset_xr_stats --flag for whether to reset RTCP XR stats
 * Returns:     -- length of packet sent (RTCP data, not including IP/UDP):
 *                 zero indicates a send failure, or lack of RTCP bandwidth.
 */

uint32_t rtcp_send_report_base (rtp_session_t   *p_sess,
                                rtp_member_t    *p_source, 
                                rtcp_pkt_info_t *p_pkt_info,
                                boolean          re_schedule,
                                boolean          reset_xr_stats)
{
    static uint8_t rtcp_buffer[RTCP_PAK_SIZE];

    ssize_t len = 0;
    uint16_t    rtcp_len = 0;
    uint8_t     *data_start = rtcp_buffer;
    struct sockaddr_in dest_addr;
    abs_time_t  curr_time;
    boolean sent_ok = FALSE;

    /*
     * Sanity checks...
     */
    if (!p_source || !p_sess) {
        return(0);
    }

    /*
     * Note that we must always construct the report,
     * even if we may not be able to send it, since
     * the rtcp_construct_method will cause the update 
     * of RTP send or receive statistics, in order to
     * to maintain accurate counts for mgmt, as well as
     * to keep members from timing out prematurely.
     */
    rtcp_len = MCALL(p_sess, rtcp_construct_report, 
                     p_source, 
                     (rtcptype *)data_start,  
                     RTCP_PAK_SIZE,
                     p_pkt_info,
                     reset_xr_stats);
    /*
     * check for available RTCP bandwidth,
     * and other prerequisites for sending.
     */
    if (!rtcp_may_send(p_sess)) {
        rtcp_len = 0;
        sent_ok = TRUE;
    }

    if (rtcp_len) {
        /* 
         * ASSUMPTION: The socket is already opened by the application 
         * and bound the local address 
         */
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = p_sess->send_addrs.dst_addr;
        dest_addr.sin_port = p_sess->send_addrs.dst_port;

        len = sendto(p_sess->rtcp_socket,
                     data_start, 
                     rtcp_len, 
                     0, /* flags */
                     (struct sockaddr *)&dest_addr,
                     sizeof(struct sockaddr_in));
        sent_ok = !(len < (ssize_t)rtcp_len);

        /* perform transmit packet post-processing */
        MCALL(p_sess, rtcp_packet_sent,
              &p_sess->send_addrs, 
              rtcp_send_time(data_start, rtcp_len),
              data_start,
              rtcp_len);
    }

    /*
     * update "packet sent" statistics, including average packet size
     */
    rtcp_upd_pkt_sent_stats(p_sess, data_start, rtcp_len, sent_ok);

    /* update _prior after all the calculations are done */
    p_source->packets_prior = p_source->packets;
    p_source->bytes_prior = p_source->bytes;
    p_sess->rtcp_pmembers = p_sess->rtcp_nmembers;

    if (re_schedule) {
        /*
         * Find the timeout for scheduling the next RTCP report.
         */
        p_sess->rtcp_interval = MCALL(p_sess, rtcp_report_interval,
                                      p_sess->we_sent,
                                      TRUE /* jitter the interval */);
        p_sess->initial = FALSE;

        /* Set tn = tc + interval */
        GET_TIMESTAMP(curr_time); 
        p_sess->prev_send_ts =  curr_time;
        p_sess->next_send_ts = TIME_ADD_A_R(curr_time, 
                                            TIME_MK_R
                                            (usec, 
                                             (int64_t)p_sess->rtcp_interval));
    }

    return (sent_ok ? len : 0);

}

/*
 * Function:    rtcp_report_interval_base
 * Description: Calculates the timeout for scheduling the next RTCP packet.
 *              Returns interval between 2 rtcp reports in microsecs.
 * Parameters:
 *           p_sess:  pointer to per-session information.
 *           we_sent: Flag that is true if we have sent data during the last two
 *           RTCP intervals. If the flag is true, the compound RTCP packet
 *           just sent contained an SR packet.
 *           add_jitter: if false, returns the deterministic interval (Td);
 *           if true, returns the randomized (jittered) interval (T).
 *
 * Returns:  Calculated Interval in microseconds
 */
uint32_t rtcp_report_interval_base(rtp_session_t    *p_sess,
                                   boolean          we_sent,
                                   boolean          add_jitter)
{
    uint32_t                    intvl_usecs;  /* T or Td interval, in usecs */
    double                      intvl_secs;   /* T or Td interval, in secs */
    double                      td_secs;      /* Td (deterministic intvl) */

    /*
     * Compute Td (the deterministic interval), 
     * then jitter it to produce the randomized interval T), 
     * if requested.
     */
    td_secs = rtcp_get_td_interval(p_sess, we_sent);
    intvl_secs = add_jitter ? 
        rtcp_jitter_interval(td_secs, &p_sess->intvl_jitter) : td_secs;
    intvl_usecs = (uint32_t)(intvl_secs * ONESEC);

    RTP_LOG_DEBUG_FV(p_sess, NULL,
                     "RTCP interval: Td=%f secs, returning %s=%u.%06u secs\n", 
                     td_secs, 
                     add_jitter ? "T" : "Td", 
                     intvl_usecs / ONESEC, intvl_usecs % ONESEC);

    return (intvl_usecs);
}

/* 
 * Function:     rtcp_packet_sent
 * Description:  Performs post-processing on an RTCP compound packet
 *               which has just been transmitted.
 *               There is no default post-processing, so an
 *               an application-level class must override this method
 *               if post-processing is required. This is typically
 *               done to "export" the RTCP packet, for VQM purposes.
 * Parameters:   p_sess          ptr to session object
 *               orig_addrs      ptr to address info for the pkt
 *               orig_send_time  the time at which the pkt was originally sent;
 *                               derived from pkt itself (and current time)
 *               p_buf           ptr to start of first RTCP header in the pkt
 *               len             length in bytes of RTCP compound pkt
 * Returns:      None
 */

void rtcp_packet_sent_base(rtp_session_t *p_sess,
                           rtp_envelope_t *orig_addrs,
                           ntp64_t orig_send_time,
                           uint8_t *p_buf,
                           uint16_t len)
{
    /*
     * As an example, here's how to export a compound packet:
     *
     * 1. initialize the VQR header via the vqr_report_init() function:
     *    - the values of the subtype, chan_addr, chan_port, 
     *      sndr_role, and rcvr_role arguments must be determined 
     *      by the application-level class.
     *    - the orig_send_time and orig_addrs arguments should be the same
     *      as supplied in the orig_send_time and orig_addrs arguments
     *      to this function.
     * 2. call vqr_export() to export the packet.
     */
}

/*
 * Function:    rtp_update_sender_stats_base
 * Description: Dummy Function (pure virtual) to be overridden and called by
 *              application layer to fill in sender stats from data plane
 * Parameters:  p_sess pointer to session object
 *              p_source pointer to local source member
 *              
 */
void rtp_update_sender_stats_base (rtp_session_t *p_sess, rtp_member_t *p_source)
{

/* Application Layer(Derived class) must override this function, in the case the data 
 * plane is in separate process or hardware. This function should get the folowing
 * stats from the data plane:
 * total packets sent
 * total bytes sent
 * RTP timestamp of last packet sent
 * NTP timestamp of last packet sent
 *
 * Then the function should fill the following in the p_source member data if
 * local source is a sender:
 *    uint32_t               packets; 
 *    uint32_t               bytes;       
 *    uint32_t               packets_prior; 
 *    uint32_t               bytes_prior;   
 *    uint32_t               rtp_timestamp; 
 *    ntp64_t                       ntp_timestamp;
 */
}

/*
 * Function:    rtp_update_receiver_stats_base
 * Description: Dummy Function (pure virtual) to be overridden and called by
 *              application layer to fill in receivers stats from data plane
 * Parameters:  p_sess pointer to session object
 *              p_source pointer to local source member
 *              reset_xr_stats flag for whether to reset RTCP XR stats
 *              
 */
void rtp_update_receiver_stats_base (rtp_session_t *p_sess, 
                                     rtp_member_t *p_source,
                                     boolean reset_xr_stats)
{
/* Application Layer(Derived class) must override this function, in the case the data 
 * plane is in separate process or hardware. This function should get the folowing
 * stats from the data plane:
 * 
 * rtp_hdr_source_t information (per-source statistics)
 * rtp_hdr_session_t information (per-session statistics)
 * 
 * Then, if the p_source member represents a known remote sender,
 * the function should 
 * 1) fill in the following p_source member data:
 *    rtp_hdr_source_t  rcv_stats;
 * 2) fill in the following p_sess session data:
 *    rtp_hdr_session_t rtp_stats;
 */
}

/*
 * Function:    rtp_update_stats_base
 * Description: Dummy Function (pure virtual) to be overridden and called by
 *              application layer to fill in stats (receiver or sender) from data plane
 * Parameters:  p_sess pointer to session object
 *              reset_xr_stats flag for whether to reset RTCP XR stats
 *              
 */
void rtp_update_stats_base (rtp_session_t *p_sess, 
                            boolean reset_xr_stats)
{
/* Application Layer(Derived class) must override this function, in the case the data 
 * plane is in separate process or hardware.  
 * 
 * If the session's local source is a receiver, this function should get the folowing
 * stats from the data plane:
 *
 * rtp_hdr_source_t information (per-source statistics, for all sending sources)
 * rtp_hdr_session_t information (per-session statistics)
 * 
 * Then, for all known sending sources,
 * the function should fill in the corresponding per-member data:
 *    rtp_hdr_source_t  rcv_stats; 
 *    rtp_hdr_session_t rtp_stats;
 * 
 * If the session's local source is a sender, this function should get the folowing
 * stats from the data plane:
 * total packets sent
 * total bytes sent
 * RTP timestamp of last packet sent
 * NTP timestamp of last packet sent
 *
 * Then the function should fill in the corresponding p_source member data:
 *    uint32_t               packets; 
 *    uint32_t               bytes;       
 *    uint32_t               packets_prior; 
 *    uint32_t               bytes_prior;   
 *    uint32_t               rtp_timestamp; 
 *    ntp64_t                ntp_timestamp;
 */
}


