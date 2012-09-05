/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: RTP Receiver.
 *
 * Documents:
 *
 *****************************************************************************/

#include <utils/vam_util.h>
#include "vqec_dp_rtp_receiver.h"
#include "vqec_dp_rtp_input_stream.h"
#include "vqec_pak.h"
#include "vqec_dp_tlm.h"
#include "vqec_dpchan.h"
#include "vqec_dp_api_types.h"
#include "vqec_dp_debug_utils.h"

/**
 * RTP source entry allocation pool.
 */
typedef struct vqe_zone vqec_dp_rtp_src_pool_t;
static vqec_dp_rtp_src_pool_t *s_rtp_src_pool;

typedef struct vqe_zone rtcp_xr_stats_pool_t;
static rtcp_xr_stats_pool_t *s_xr_stats_pool = NULL;

typedef struct vqe_zone post_er_stats_pool_t;
static post_er_stats_pool_t *s_post_er_stats_pool;

#ifdef _VQEC_DP_UTEST
#define UT_STATIC
#else
#define UT_STATIC static
#endif

/****************************************************************************
 * HELPFUL DEFINITIONS: 
 * -- RTP source: The 3-tuple {source IP address, source udp port, SSRC}
 * -- packet-flow (pktflow) source: A RTP source from which the dataplane will
 *    accept and process packets. 
 *     = A primary session will have at-most one packet-flow source at 
 *       any instant in time. The first source seen after channel change is
 *       immediately considered to be such a source. After that point, if the
 *       source times-out, the control-plane decides which source (if any) 
 *       to pick as the next packet-flow source. 
 *     = A repair session may have one than one source which has packet-
 *       flow enabled in this implementation. The repair session sources are
 *       constrained by a SSRC filter though - the value of this filter is
 *       equal to the SSRC of the primary session packet-flow source. The
 *       SSRC filters are installed by the control-plane.
 * -- Activity detection: Primary sources that do not have packet-flow
 *    enabled (i.e., all except the one that is currently the packet-flow
 *    source are automatically timed out after ten seconds of inactivity.  
 *    Repair sources have no such detection.
 */


/**---------------------------------------------------------------------------
 * Get an existing source entry (private scope). 
 *
 * @param[in] src_list Pointer to a RTP source list object.
 * @param[in] key Source key.
 * @param[out] vqec_dp_rtp_src_t* Pointer to the retrieved src entry.
 *---------------------------------------------------------------------------*/
static vqec_dp_rtp_src_t *
vqec_dp_rtp_src_entry_get_internal (vqec_dp_rtp_src_list_t *src_list,
                                    vqec_dp_rtp_src_key_t *key)
{
    vqec_dp_rtp_src_t *rtp_src_entry = NULL;
    
    VQE_TAILQ_FOREACH(rtp_src_entry, 
                   &src_list->lh_known_sources, le_known_sources) {
        if (memcmp(&rtp_src_entry->key, key, sizeof(*key)) == 0) {
            break;
        }
    }

    return (rtp_src_entry);
}


/**---------------------------------------------------------------------------
 * Create a new source entry (private scope).
 *
 * @param[in] src_list Pointer to an RTP source list object.
 * @param[in] key Source key.
 * @param[in] max_xr_rle_size Maximum size of the XR RLE stats block to use.
 * @param[in] max_post_er_rle_size Maximum size of the post ER stats block to
 * use.
 * @param[out] vqec_dp_rtp_src_t* Pointer to the entry that
 * is created.
 *---------------------------------------------------------------------------*/
#define VQEC_DP_RTP_2MANY_SYSLOG_PRINT_INTERVAL SECS(30)
UT_STATIC abs_time_t s_nxt_2many_syslog_time = ABS_TIME_0;

static vqec_dp_rtp_src_t *
vqec_dp_rtp_src_entry_create_internal (
    vqec_dp_rtp_src_list_t *src_list, 
    vqec_dp_rtp_src_key_t *key,
    uint32_t max_xr_rle_size,
    uint32_t max_post_er_rle_size)
{
    vqec_dp_rtp_src_t *rtp_src_entry;
    abs_time_t cur_time;

    if (src_list->present >= VQEC_DP_RTP_MAX_KNOWN_SOURCES) {
        /* Too many sources for the receiver. */
        cur_time = get_sys_time();
        if (IS_ABS_TIME_ZERO(s_nxt_2many_syslog_time) ||
            TIME_CMP_A(gt, cur_time, s_nxt_2many_syslog_time)) {
            VQEC_DP_SYSLOG_PRINT(CHAN_TOO_MANY_SOURCES, 
                                 VQEC_DP_RTP_MAX_KNOWN_SOURCES);
            s_nxt_2many_syslog_time = 
                TIME_ADD_A_R(cur_time,
                             VQEC_DP_RTP_2MANY_SYSLOG_PRINT_INTERVAL);
        } 
        /* Update debug counter on failure. */
        VQEC_DP_TLM_CNT(rtp_src_limit_exceeded, vqec_dp_tlm_get());
        return (NULL);
    }

    rtp_src_entry = zone_acquire(s_rtp_src_pool);
    if (!rtp_src_entry) {
        /* Too many sources across the entire population. */
        cur_time = get_sys_time();
        if (IS_ABS_TIME_ZERO(s_nxt_2many_syslog_time) ||
            TIME_CMP_A(gt, cur_time, s_nxt_2many_syslog_time)) {
            VQEC_DP_SYSLOG_PRINT(CHAN_TOO_MANY_SOURCES_GLOBAL);
            s_nxt_2many_syslog_time = 
                TIME_ADD_A_R(cur_time,
                             VQEC_DP_RTP_2MANY_SYSLOG_PRINT_INTERVAL);
        } 
        VQEC_DP_TLM_CNT(rtp_src_table_full, vqec_dp_tlm_get());        
        return (NULL);

    } else {

        memset(rtp_src_entry, 0, sizeof(*rtp_src_entry));
        memcpy(&rtp_src_entry->key, key, sizeof(rtp_src_entry->key));
        if (max_xr_rle_size) {
            rtp_src_entry->xr_stats = 
                    (rtcp_xr_stats_t *) zone_acquire(s_xr_stats_pool);
            if (!rtp_src_entry->xr_stats) {
                VQEC_DP_TLM_CNT(xr_stats_malloc_fails, vqec_dp_tlm_get());
            } else {
                memset(rtp_src_entry->xr_stats, 0, 
                       sizeof(*rtp_src_entry->xr_stats));
                rtcp_xr_set_size(rtp_src_entry->xr_stats, max_xr_rle_size);
            }
        }
        if (max_post_er_rle_size) {
            rtp_src_entry->post_er_stats = 
                (rtcp_xr_post_rpr_stats_t *)zone_acquire(s_post_er_stats_pool);
            if (!rtp_src_entry->post_er_stats) {
                VQEC_DP_TLM_CNT(xr_stats_malloc_fails, vqec_dp_tlm_get());
            } else {
                memset(rtp_src_entry->post_er_stats, 0, 
                       sizeof(*rtp_src_entry->post_er_stats));
                rtcp_xr_set_size(&rtp_src_entry->post_er_stats->xr_stats, 
                                 max_post_er_rle_size);
            }
        }

        /* Put in RTP source table, at the tail of the queue. */
        VQE_TAILQ_INSERT_TAIL(&src_list->lh_known_sources, 
                          rtp_src_entry, le_known_sources);
        src_list->present++;
        src_list->created++;

        /* All entries are active when created. */
        rtp_src_entry->info.state = VQEC_DP_RTP_SRC_ACTIVE;

        VQEC_DP_TLM_CNT(rtp_src_creates, vqec_dp_tlm_get());
        return (rtp_src_entry);
    }
}


/**---------------------------------------------------------------------------
 * Process a primary stream's failover queue:
 *   - queued packets will either be inserted into the PCM, or discarded
 *     (as requested by the caller)
 *   - the primary stream's failover source will be cleared
 *
 * @param[in] in Pointer to a RTP input stream. 
 * @param[in] accept TRUE to insert packets into the PCM, FALSE to discard
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_process_failoverq (
    vqec_dp_chan_rtp_primary_input_stream_t *in,
    boolean accept)
{
    vqec_pak_t *pak, *pak_next;
    vqec_pak_t *l_pak_array[VQEC_DP_RTP_SRC_FAILOVER_PAKS_MAX];
    uint32_t i = 0, failover_paks_queued, drops;

    if (!in->failover_rtp_src_entry) {
        return;
    }

    /* Empty the failover queue into an array */
    VQE_TAILQ_FOREACH_SAFE(pak, &in->failoverq_lh, inorder_obj, pak_next) {
        VQE_TAILQ_REMOVE(&in->failoverq_lh, pak, inorder_obj);
        l_pak_array[i++] = pak;
    }
    failover_paks_queued = i;
    in->failover_paks_queued = 0;
    if (!failover_paks_queued) {
        goto done;
    }

    /* Process the dequeued packets as requested by the caller. */
    if (accept) {
        /*
         * The discontinuity bit is set on the first packet in case the
         * RTP timestamps are not synchronized w.r.t. packets already in
         * the PCM.
         */
        VQEC_PAK_FLAGS_SET(&l_pak_array[0]->flags, 
                           VQEC_PAK_FLAGS_RX_DISCONTINUITY);
        /*
         * Insert the packets into the PCM.  PCM will increase each pak's
         * ref count for any packets it retains.  If any packets were
         * not given to the PCM due to late stage RTP receiver processing
         * drops, those drops will be counted at the RTP receiver level
         * based on reason code.
         */
        drops =
            vqec_dp_chan_rtp_process_primary_paks_and_insert_to_pcm(
                (struct vqec_dp_chan_rtp_input_stream_ *)in, l_pak_array,
                failover_paks_queued, ABS_TIME_0, in->failover_rtp_src_entry);
        in->failover_rtp_src_entry->info.drops += drops;
        in->in_stats.drops += drops;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_FAILOVER,
                      "Incorporated %lu queued failover packets (excludes %llu"
                      " source drops [%lu from PCM]) from "
                      "source %s on channel %s\n",
                      failover_paks_queued - drops,
                      in->failover_rtp_src_entry->info.drops,
                      drops,
                      vqec_dp_rtp_key_to_str(&in->failover_rtp_src_entry->key),
                      vqec_dpchan_print_name(in->chan));
    } else {
        in->failover_rtp_src_entry->info.drops += failover_paks_queued;
        in->rtp_in_stats.rtp_parse_drops += failover_paks_queued;
        in->in_stats.drops += failover_paks_queued;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_FAILOVER,
                      "Discarded %lu queued failover packets from "
                      "source %s on channel %s\n",
                      failover_paks_queued,
                      vqec_dp_rtp_key_to_str(&in->failover_rtp_src_entry->key),
                      vqec_dpchan_print_name(in->chan));
    }
    
    /* 
     * Decrement the ref count (any packets not retained by the PCM 
     * should be freed here)
     */
    for (i=0; i < failover_paks_queued; i++) {
        vqec_pak_free(l_pak_array[i]);
    }

done:
    /* Clear failover status on this source */
    VQEC_DP_DEBUG(
        VQEC_DP_DEBUG_FAILOVER,
        "Removed failover status from source %s on channel %s\n",
        vqec_dp_rtp_key_to_str(&in->failover_rtp_src_entry->key),
        vqec_dpchan_print_name(in->chan));
    in->failover_rtp_src_entry->info.buffer_for_failover = FALSE;
    in->failover_rtp_src_entry = NULL;
}


/**---------------------------------------------------------------------------
 * Sets the failover buffering source for a primary session.
 * Callers MUST ensure that no failover source already exists,
 * or else its packets will be discarded and an error will be logged.
 *
 * @param[in] primary_is Primary RTP input stream.
 * @param[in] rtp_src_entry Source for which failover buffering should
 * be enabled (may be NULL if no failover source should be used).
 *---------------------------------------------------------------------------*/
static void
vqec_dp_rtp_set_failover_buffering_internal (
    vqec_dp_chan_rtp_primary_input_stream_t *primary_is, 
    vqec_dp_rtp_src_t *rtp_src_entry)
{
    /*
     * Verify that if a new failover source is supplied,
     * it isn't also the packetflow source.
     */
    VQEC_DP_ASSERT_FATAL(
        (!rtp_src_entry || !rtp_src_entry->info.pktflow_permitted),
        __FUNCTION__);

    /*
     * If the supplied failover source matches the current failover source,
     * nothing to do.
     */
    if (primary_is->failover_rtp_src_entry == rtp_src_entry) {
        return;
    }
    
    /* 
     * If another failover source is already assigned,
     * log an error and discard any queued packets.
     */
    if (primary_is->failover_rtp_src_entry) {
        VQEC_DP_SYSLOG_PRINT(
            CHAN_RTP_GENERAL_FAILURE, 
            vqec_dpchan_print_name(primary_is->chan),
            "Failover source assigned when one already exists, "
            "discarding packets.");
        vqec_dp_chan_rtp_process_failoverq(primary_is, FALSE);
    }

    /* Assign the caller's source as the failover source */
    primary_is->failover_rtp_src_entry = rtp_src_entry;
    rtp_src_entry->info.buffer_for_failover = TRUE;
    VQEC_DP_DEBUG(
        VQEC_DP_DEBUG_FAILOVER,
        "Assigned source %s on channel %s as the failover source\n",
        vqec_dp_rtp_key_to_str(&rtp_src_entry->key),
        vqec_dpchan_print_name(primary_is->chan));
    
}

/**---------------------------------------------------------------------------
 * Selects a non-packetflow source and promotes it to the failover source.  
 * The selection is based on the most recently active source.
 * If no such source can be found, no failover source is assigned.
 * 
 * @param[in] primary_is Pointer to the primary input stream.
 *---------------------------------------------------------------------------*/ 
static void
vqec_dp_rtp_src_elect_failover (
    vqec_dp_chan_rtp_primary_input_stream_t *primary_is)
{
    vqec_dp_rtp_recv_t *rtp_recv;
    vqec_dp_rtp_src_t *most_recent = NULL, *rtp_src_entry;

    VQEC_DP_ASSERT_FATAL(primary_is, __FUNCTION__);

    rtp_recv = &primary_is->rtp_recv;
    
    /* 
     * Iterate through the source list, and find the most recent
     * active non-packetflow source.
     */
    VQE_TAILQ_FOREACH(rtp_src_entry, 
                      &rtp_recv->src_list.lh_known_sources, le_known_sources) {
        if (rtp_src_entry->info.pktflow_permitted ||
            rtp_src_entry->info.state != VQEC_DP_RTP_SRC_ACTIVE) {
            continue;
        }
        if (most_recent) {
            if (TIME_CMP_A(ge, rtp_src_entry->info.last_rx_time, 
                           most_recent->info.last_rx_time)) {
                most_recent = rtp_src_entry;
            }
        } else {
            most_recent = rtp_src_entry;
        }
    }

    if (most_recent) {
        vqec_dp_rtp_set_failover_buffering_internal(primary_is, most_recent);
    }
}

/**---------------------------------------------------------------------------
 * Enable pktflow for a src entry.
 *
 * pktflow status for a source is recorded in two places:
 *    - the info.pktflow_permitted field for the RTP source, and
 *    - a pointer to the pktflow source is cached in the pktflow source entry
 * Upon being elected as the packetflow source, the source is also promoted
 * to the head of the source list (private scope).
 *
 * @param[in] in Parent RTP input stream.
 * @param[in] rtp_src_entry Source for which pktflow is enabled. 
 * @param[in] session_rtp_seq_num_offset 
 *              new source's RTP sequence space offset from old source 
 *              (see vqec_dp_rtp_src_promote_permit_pktflow() function header
 *              for more information).
 *---------------------------------------------------------------------------*/
static void
vqec_dp_rtp_src_entry_ena_pktflow_internal (
    vqec_dp_chan_rtp_input_stream_t *in, vqec_dp_rtp_src_t *rtp_src_entry,
    int16_t session_rtp_seq_num_offset)
{
    vqec_dp_rtp_recv_t *rtp_recv;
    vqec_dp_chan_rtp_primary_input_stream_t *primary_is = NULL;
    boolean failover_src_promoted = FALSE;

    VQEC_DP_ASSERT_FATAL(in && rtp_src_entry, __FUNCTION__);

    /* 
     * If an existing pktflow source will be going away, 
     * 1) store the time of its last pkt reception (primary stream only)
     * 2) remove pktflow status from the old pktflow source.
     */
    rtp_recv = &in->rtp_recv;
    if (rtp_recv->src_list.pktflow_src &&
        (rtp_recv->src_list.pktflow_src != rtp_src_entry)) {
        if (in->type == VQEC_DP_INPUT_STREAM_TYPE_PRIMARY) {
            in->chan->prev_src_last_rcv_ts = 
                rtp_recv->src_list.pktflow_src->info.last_rx_time;
        }
        rtp_recv->src_list.pktflow_src->info.pktflow_permitted = FALSE;
                                /* demote existing entry */
    }

    /*
     * Assign the sequence number offset to be used when inserting
     * packets from this source into the PCM.
     */
    rtp_src_entry->info.session_rtp_seq_num_offset = 
        session_rtp_seq_num_offset;

    /*
     * If the new packetflow source is also the failover source, process
     * its queued packets.  This will only apply for primary streams.
     */
    if (in->type == VQEC_DP_INPUT_STREAM_TYPE_PRIMARY) {
        primary_is = (vqec_dp_chan_rtp_primary_input_stream_t *)in;
        if (rtp_src_entry == primary_is->failover_rtp_src_entry) {
            failover_src_promoted = TRUE;
            vqec_dp_chan_rtp_process_failoverq(primary_is, TRUE);
        }
    }
        
    /*
     * Promote the supplied source to be the pktflow source.
     * Also move it to the head of the source list for lookup efficiency.
     */
    rtp_src_entry->info.pktflow_permitted = TRUE;
    rtp_recv->src_list.pktflow_src = rtp_src_entry;
    if (VQE_TAILQ_FIRST(&rtp_recv->src_list.lh_known_sources) != 
	rtp_src_entry) {
        VQE_TAILQ_REMOVE(&rtp_recv->src_list.lh_known_sources, 
                         rtp_src_entry, le_known_sources);
        VQE_TAILQ_INSERT_HEAD(&in->rtp_recv.src_list.lh_known_sources, 
                              rtp_src_entry, le_known_sources);
    }
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_FAILOVER,
                  "Assigned source %s as "
                  "new packetflow source for chan %s %s session\n",
                  vqec_dp_rtp_key_to_str(&rtp_src_entry->key),
                  vqec_dpchan_print_name(in->chan),
                  vqec_dp_input_stream_type_to_str(in->type));
    
    /*
     * If the new pktflow source was the old failover source,
     * attempt to select a new failover source.
     */
    if (failover_src_promoted) {
        vqec_dp_rtp_src_elect_failover(primary_is);
    }

}

/**---------------------------------------------------------------------------
 * Disable the most recent pktflow source entry. This method is used to
 * disable the current pktflow source for primary sessions. 
 *
 * @param[in] in Parent RTP input stream. 
 * @param[in] rtp_src_entry Source for which pktflow is enabled. 
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.
 *---------------------------------------------------------------------------*/
static vqec_dp_error_t
vqec_dp_rtp_src_entry_dis_current_pktflow_internal (
    vqec_dp_chan_rtp_input_stream_t *in, vqec_dp_rtp_src_t *rtp_src_entry)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_rtp_recv_t *rtp_recv;

    if (!in || !rtp_src_entry) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    rtp_recv = &in->rtp_recv;
    if (rtp_recv->src_list.pktflow_src != rtp_src_entry) {
        err = VQEC_DP_ERR_NOT_FOUND;
        return (err);
    }

    rtp_recv->src_list.pktflow_src->info.pktflow_permitted = FALSE;
    rtp_recv->src_list.pktflow_src = NULL;

    return (err);
}


/**---------------------------------------------------------------------------
 * Activate a source entry - an IRQ event is sent to the control-plane 
 * indicating a change in the source's state. A source is activated if it
 * was previously inactive, and packets are received from it.
 * 
 * @param[in] in Pointer to a RTP input stream. 
 * @param[in] rtp_src_entry Source to be activated.
 *---------------------------------------------------------------------------*/
static void
vqec_dp_rtp_src_entry_set_state_active (vqec_dp_chan_rtp_input_stream_t *in,
                                        vqec_dp_rtp_src_t *rtp_src_entry)
{
    vqec_dp_chan_rtp_primary_input_stream_t *primary_is;

    rtp_src_entry->info.state = VQEC_DP_RTP_SRC_ACTIVE;
    rtp_src_entry->info.thresh_cnt++; /* inactive-2-active transition count */

    if (in->type == VQEC_DP_INPUT_STREAM_TYPE_PRIMARY) {
        primary_is = (vqec_dp_chan_rtp_primary_input_stream_t *)in;
        if (!rtp_src_entry->info.pktflow_permitted &&
            !primary_is->failover_rtp_src_entry) {
            /*
             * A non-packet flow source has become active
             * while no active failover source exists.
             * Assign this newly active source as the failover source.
             */
            vqec_dp_rtp_set_failover_buffering_internal(primary_is, 
                                                        rtp_src_entry);
        }
    }

    /* Interrupt control-plane. */
    /* sa_ignore {IRQ bit set even if call fails.} IGNORE_RETURN(4) */
    vqec_dp_chan_rtp_input_stream_src_ev(
        in, 
        rtp_src_entry,
        VQEC_DP_UPCALL_REASON_RTP_SRC_ISACTIVE);
}


/**---------------------------------------------------------------------------
 * Deactivate a source entry - an IRQ event is set to control-plane.
 *
 * @param[in] in Pointer to a RTP input stream. 
 * @param[in] rtp_src_entry Source to be deactivated.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.
 *---------------------------------------------------------------------------*/
static vqec_dp_error_t
vqec_dp_rtp_src_entry_set_state_inactive (vqec_dp_chan_rtp_input_stream_t *in,
                                          vqec_dp_rtp_src_t *rtp_src_entry)
{
    vqec_dp_error_t ret = VQEC_DP_ERR_OK;
    vqec_dp_chan_rtp_primary_input_stream_t *primary_is;

    if (!rtp_src_entry) {
        ret = VQEC_DP_ERR_INVALIDARGS;
        return (ret);
    }

    rtp_src_entry->info.state = VQEC_DP_RTP_SRC_INACTIVE;
    rtp_src_entry->info.thresh_cnt++;  /* active-2-inactive transition count */

    if (in->type == VQEC_DP_INPUT_STREAM_TYPE_PRIMARY) {
        primary_is = (vqec_dp_chan_rtp_primary_input_stream_t *)in;
        if (primary_is->failover_rtp_src_entry == rtp_src_entry) {
            /* 
             * Since the failover source has become inactive,
             * discard its queued packets and attempt to elect a new one.
             */
            vqec_dp_chan_rtp_process_failoverq(primary_is, FALSE);
            vqec_dp_rtp_src_elect_failover(primary_is);
        }
    }

    /* Interrupt control-plane. */
    /* sa_ignore {IRQ bit set even if call fails.} IGNORE_RETURN(4) */
    vqec_dp_chan_rtp_input_stream_src_ev(
        in, 
        rtp_src_entry, 
        VQEC_DP_UPCALL_REASON_RTP_SRC_ISINACTIVE);

    return (ret);
}


/**---------------------------------------------------------------------------
 * Delete an existing source entry. 
 *
 * @param[in] rtp_recv      Pointer to RTP receiver instance for which a source
 *                           is to be deleted.
 * @param[in] rtp_src_entry Source to be deleted.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.
 *---------------------------------------------------------------------------*/
static vqec_dp_error_t
vqec_dp_rtp_src_entry_delete_internal (
    vqec_dp_rtp_recv_t *rtp_recv,
    vqec_dp_rtp_src_t *rtp_src_entry)
{
    vqec_dp_rtp_src_list_t *src_list;
    vqec_dp_error_t ret = VQEC_DP_ERR_OK;

    if (!rtp_recv || !rtp_src_entry) {
        ret = VQEC_DP_ERR_INVALIDARGS;
        return (ret);
    }
    
    src_list = &rtp_recv->src_list;
    
    VQE_TAILQ_REMOVE(&src_list->lh_known_sources, 
                 rtp_src_entry, le_known_sources);
    if (rtp_src_entry->xr_stats) {
        zone_release(s_xr_stats_pool, rtp_src_entry->xr_stats);
        rtp_src_entry->xr_stats = NULL;
    }
    if (rtp_src_entry->post_er_stats) {
        zone_release(s_post_er_stats_pool, rtp_src_entry->post_er_stats);
        rtp_src_entry->post_er_stats = NULL;
    }

    if (rtp_recv->src_list.pktflow_src == rtp_src_entry) {
        rtp_recv->src_list.pktflow_src = NULL;
    }

    zone_release(s_rtp_src_pool, rtp_src_entry);
    src_list->present--; 
    src_list->destroyed++; 
    
    VQEC_DP_TLM_CNT(rtp_src_destroys, vqec_dp_tlm_get());
    return (ret);
}


/**---------------------------------------------------------------------------
 * Check for the age of a RTP source, and delete it if has been inactive
 * for more than a certain time threshold.  Note that the threshold MUST
 * be no less than the maximum jitter buffer size, as the RTP source
 * must be present when scheduling out its packets.
 *
 * The method will never remove sources that are marked as "packet-flow" 
 * sources.
 *
 * @param[in] in Pointer to an RTP input stream.
 * @param[in] key Key of the source for which the age should be checked.
 * @param[in] current_time Current system time.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.
 *---------------------------------------------------------------------------*/
#define VQEC_DP_RTP_SRC_AGE_THRESHOLD_SECS (20)

static vqec_dp_error_t
vqec_dp_rtp_src_entry_check_age (vqec_dp_chan_rtp_input_stream_t *in,
                                 vqec_dp_rtp_src_key_t *key, 
                                 abs_time_t current_time)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_rtp_src_t *rtp_src_entry;
    rel_time_t age;

    if (!in || !key) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    rtp_src_entry = 
        vqec_dp_rtp_src_entry_get_internal(&in->rtp_recv.src_list, key);
    if (rtp_src_entry) {
        if (!rtp_src_entry->info.pktflow_permitted &&
            (rtp_src_entry->info.state == VQEC_DP_RTP_SRC_INACTIVE)) {

            /* Find out how long since we have heard from the source. */
            age = 
                TIME_SUB_A_A(current_time, rtp_src_entry->info.last_rx_time);
            if (TIME_CMP_R(gt, age,
                           SECS(VQEC_DP_RTP_SRC_AGE_THRESHOLD_SECS))) {

                /* Non-pktflow source, and older than threshold so delete */
                err = vqec_dp_rtp_src_entry_delete_internal(&in->rtp_recv,
                                                            rtp_src_entry);
                if (err != VQEC_DP_ERR_OK) {
                    VQEC_DP_SYSLOG_PRINT(CHAN_RTP_GENERAL_FAILURE, 
                                         vqec_dpchan_print_name(in->chan),
                                       "Failed to remove aged-out RTP source.");
                } else {
                    /* increment debug counter. */
                    VQEC_DP_TLM_CNT(rtp_src_aged_out, vqec_dp_tlm_get());
                }
            }
        }
    } else {
        err = VQEC_DP_ERR_INVALIDARGS;
    }

    return (err);
}


/**---------------------------------------------------------------------------
 * Check if the RTP source, as specified by the key, has received any
 * traffic since the last check. If no traffic has been received, set the
 * source's state to inactive.
 *
 * @param[in] in Pointer to an RTP input stream.
 * @param[in] key Key of the source for which activity should be checked.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.
 *---------------------------------------------------------------------------*/
static vqec_dp_error_t
vqec_dp_rtp_src_entry_check_throughput (vqec_dp_chan_rtp_input_stream_t *in,
                                        vqec_dp_rtp_src_key_t *key)
{
    vqec_dp_rtp_src_t *rtp_src_entry;
    vqec_dp_error_t ret = VQEC_DP_ERR_OK;
    
    if (!in || !key) {
        ret = VQEC_DP_ERR_INVALIDARGS;
        return (ret);
    }
    
    rtp_src_entry = 
        vqec_dp_rtp_src_entry_get_internal(&in->rtp_recv.src_list, key);
    if (rtp_src_entry &&
        rtp_src_entry->info.state == VQEC_DP_RTP_SRC_ACTIVE) {
        if (!rtp_src_entry->rcv_flag) {
            /* No activity on source since last check. */
            ret = 
                vqec_dp_rtp_src_entry_set_state_inactive(in, rtp_src_entry);
        } else {
            /* Clear marker for next check. */
            rtp_src_entry->rcv_flag = FALSE;
        }
    }

    return (ret);
}


/**---------------------------------------------------------------------------
 * Iterator for source entries: if the current key's 3-tuple 
 * (ssrc, address, port) is specified as 0, the key of the first source entry
 *  in the source list is returned in *next. If the key is non-0, then the 
 * successor of the list entry corresponding to the key is returned.
 *
 * @param[in] in Pointer to an RTP input stream.
 * @param[in] cur Key of the source whose successor is to be returned.
 * @param[out] next Key of the successor element.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.
 *---------------------------------------------------------------------------*/
vqec_dp_error_t 
vqec_dp_rtp_src_entry_get_next (vqec_dp_chan_rtp_input_stream_t *in, 
                                vqec_dp_rtp_src_key_t *cur, 
                                vqec_dp_rtp_src_key_t *next_key)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_rtp_src_key_t zero_key;
    vqec_dp_rtp_src_t *rtp_src_entry, *next;
    vqec_dp_rtp_recv_t *rtp_recv;

    if (!in || !cur || !next_key) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    rtp_recv = &in->rtp_recv;
    next = NULL;
    memset(&zero_key, 0, sizeof(zero_key));
    if (memcmp(cur, &zero_key, sizeof(*cur)) == 0) {
        rtp_src_entry = VQE_TAILQ_FIRST(&rtp_recv->src_list.lh_known_sources);
        if (rtp_src_entry) {
            memcpy(next_key, &rtp_src_entry->key, sizeof(*next_key));
        } else {
            err = VQEC_DP_ERR_NOT_FOUND;
        }
    } else {
        rtp_src_entry = 
            vqec_dp_rtp_src_entry_get_internal(&rtp_recv->src_list, cur);
        if (rtp_src_entry) {
            next = VQE_TAILQ_NEXT(rtp_src_entry, le_known_sources);
            if (next) {
                memcpy(next_key, &next->key, sizeof(*next_key));
            }
        } else {
            err = VQEC_DP_ERR_NOT_FOUND;
        }
    }
    
    return (err);
}


/**---------------------------------------------------------------------------
 * Method which will scan a single RTP input stream's RTP receivers, and 
 * check all sources for age and activity detection. The method must be
 * invoked for primary streams at this point.
 * 
 * @param[in] in Pointer to an RTP input stream.
 * @param[in] cur_time Current system time.
 *---------------------------------------------------------------------------*/
void 
vqec_dp_chan_rtp_scan_one_input_stream (vqec_dp_chan_rtp_input_stream_t *in, 
                                        abs_time_t cur_time)
{
    vqec_dp_rtp_src_t *rtp_src_entry;
    vqec_dp_rtp_src_list_t *src_list;
    vqec_dp_error_t err;

    if (in) {
        VQEC_DP_ASSERT_FATAL(in->type == VQEC_DP_INPUT_STREAM_TYPE_PRIMARY,
                             __FUNCTION__);

        src_list = &in->rtp_recv.src_list;

        VQE_TAILQ_FOREACH(rtp_src_entry, 
                      &src_list->lh_known_sources, le_known_sources) {
            
            err = vqec_dp_rtp_src_entry_check_throughput(in, 
                                                     &rtp_src_entry->key);
            if (err != VQEC_DP_ERR_OK) {
                VQEC_DP_SYSLOG_PRINT(CHAN_RTP_GENERAL_FAILURE, 
                                     vqec_dpchan_print_name(in->chan), 
                                     "Failed to check entry for throughput.");
            }

            err = vqec_dp_rtp_src_entry_check_age(in, 
                                                  &rtp_src_entry->key,
                                                  cur_time);
            if (err != VQEC_DP_ERR_OK) {
                VQEC_DP_SYSLOG_PRINT(CHAN_RTP_GENERAL_FAILURE, 
                                     vqec_dpchan_print_name(in->chan), 
                                     "Failed to check entry for age.");
            }
        }
    }
}



/**---------------------------------------------------------------------------
 *
 * Wrappers (also useful for mutual exclusion).
 *
 *---------------------------------------------------------------------------*/


/**---------------------------------------------------------------------------
 * Wrapper for the internal delete method.
 *
 * @param[in] in Pointer to parent input stream.
 * @param[in] key Source key.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_rtp_src_entry_delete (vqec_dp_chan_rtp_input_stream_t *in, 
                              vqec_dp_rtp_src_key_t *key)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_rtp_src_t *rtp_src_entry;

    if (!in || !key) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    rtp_src_entry = 
        vqec_dp_rtp_src_entry_get_internal(&in->rtp_recv.src_list, key);
    if (!rtp_src_entry) {
        err = VQEC_DP_ERR_NOT_FOUND;
        return (err);
    }
    
    return (vqec_dp_rtp_src_entry_delete_internal(&in->rtp_recv,
                                                  rtp_src_entry));
}


/**---------------------------------------------------------------------------
 * Retrieve the entire source table for a particular input stream.
 *
 * @param[in] in Pointer to an RTP input stream.
 * @param[out] table Pointer to the RTP source table.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.  
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_rtp_src_get_table (vqec_dp_chan_rtp_input_stream_t *in,
                           vqec_dp_rtp_src_table_t *table) 
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_rtp_src_t *rtp_src_entry;
    vqec_dp_rtp_src_list_t *src_list;
    uint32_t i = 0;

    if (!in || !table) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    memset(table, 0, sizeof(*table));

    src_list = &in->rtp_recv.src_list;
    VQE_TAILQ_FOREACH(rtp_src_entry, 
                  &src_list->lh_known_sources, le_known_sources) {
        if (i < VQEC_DP_RTP_MAX_KNOWN_SOURCES) {
            memcpy(&table->sources[i].key, &rtp_src_entry->key, 
                   sizeof(table->sources[i].key));
            
            memcpy(&table->sources[i].info, &rtp_src_entry->info,
                   sizeof(table->sources[i].info));
            i++;  
        }
    }
    
    table->num_entries = i;
    return (err);
}


/**---------------------------------------------------------------------------
 * Promote a particular source as a packet-flow source. The method is called
 * for primary sessions only. For primary sessions, only one source at any
 * given time will have packetflow enabled. This method will displace any
 * previous source that had packeflow enabled, and disable packetflow on it.
 *
 * When the source of a unicast (only) channel changes, an effort is
 * made to splice the streams from different sources together smoothly.
 * The term "session_rtp_seq_num_offset" defines the offset between the
 *    1) session's RTP sequence number space (based on existing packets in 
 *        the PCM), and
 *    2) a source's RTP sequence number space
 * The offset is applied to packets from a source upon their insertion
 * into the PCM.  The offset of the first source of a session is zero, 
 * while the offset of subsequent sources is chosen such that their 
 * RTP sequence spaces map sequentially into the PCM.
 *
 * E.g. assume three sources send packets of a channel's primary stream 
 * in the order listed below, and that their sequence numbers spaces are
 * uncoordinated.  The offsets shown below are used such that their
 * packets will be sequential when inserted into the PCM:
 *
 *     source      RTP seq num range    PCM seq num range     offset
 *       A             0 ...  99            0 ...  99            0
 *       B           200 ... 299          100 ... 199         -100
 *       C           500 ... 599          200 ... 299         -300
 *
 *  o When the RTP sequence space offset is known by the caller (e.g it 
 *    was signalled with the new source's information), a non-NULL value
 *    should be passed in for session_rtp_seq_num_offset_in.
 *  o When the RTP sequence space offset is NOT known by the caller,   
 *    a NULL value should be passed in for session_rtp_seq_num_offset_in.
 *    The offset will then be computed by the dataplane (with the assumption
 *    that the first packet received by the new packetflow source 
 *    follows the last packet of the old).  In this case only, the caller
 *    may supply a non-NULL value of session_rtp_seq_num_offset_out,
 *    which will be used to return the offset computed by the dataplane.
 *    In the situation that session_rtp_seq_num_offset_in is NULL and 
 *    cannot be computed by the dataplane (i.e. no packets from the new 
 *    source have been received yet), then an error is logged and no 
 *    offset will be assumed for the new source.
 *
 * When the source of a multicast channel changes, no attempt is made
 * to splice the streams smoothly.  An error will be returned if a
 * value for session_rtp_set_num_offset_in is supplied.
 *
 * @param[in] in Pointer to an RTP input stream.
 * @param[in] key Key of the source on which packetflow should be enabled.
 * @param[in] session_rtp_seq_num_offset_in 
 *            [optional] new source's offset if known by the caller.
 *            Must only be supplied if the channel is unicast.
 * @param[out] session_rtp_seq_num_offset_out
 *            [optional] new source's offset computed by the dataplane.
 *            May only be supplied if session_rtp_seq_num_offset_in is NULL.
 *            Will be non-zero only if the channel is unicast.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.
 *---------------------------------------------------------------------------*/
vqec_dp_error_t 
vqec_dp_rtp_src_promote_permit_pktflow (
    vqec_dp_chan_rtp_primary_input_stream_t *in,
    vqec_dp_rtp_src_key_t *key,
    int16_t *session_rtp_seq_num_offset_in,
    int16_t *session_rtp_seq_num_offset_out)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_rtp_src_t *rtp_src_entry;
    int16_t session_rtp_seq_num_offset = 0;
    uint16_t failoverq_lowest_seq_num;
    vqec_pak_t *pak, *pak_next;
    vqec_seq_num_t session_rtp_seq_num_next, 
        session_rtp_seq_num_highest;
    
    if (!in || !key || 
        (in->chan->is_multicast && session_rtp_seq_num_offset_in) ||
        (session_rtp_seq_num_offset_in && session_rtp_seq_num_offset_out)) {
        err = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    rtp_src_entry = 
        vqec_dp_rtp_src_entry_get_internal(&in->rtp_recv.src_list, key);
    if (!rtp_src_entry) {
        err = VQEC_DP_ERR_NOT_FOUND;
        goto done;
    }

    if (rtp_src_entry == in->rtp_recv.src_list.pktflow_src) {
        /* Caller's source is already the packetflow source */
        goto done;
    }

    /* Determine the new source's sequence number offset, if applicable */
    if (in->chan->is_multicast) {
        /* Multicast channel; no attempt is made to syncrhonize sources */
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_FAILOVER,
                      "Switching to new source %s for multicast chan %s "
                      "primary stream; sources may be unsynchronized.\n",
                      vqec_dp_rtp_key_to_str(&rtp_src_entry->key),
                      vqec_dpchan_print_name(in->chan));
        if (session_rtp_seq_num_offset_out) {
            *session_rtp_seq_num_offset_out = 0;
        }
    } else {
        /* Unicast channel; attempt to synchronize sources */
        if (session_rtp_seq_num_offset_in) {
            /* Use the offset supplied by the caller */
            session_rtp_seq_num_offset = *session_rtp_seq_num_offset_in;
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_FAILOVER,
                          "Using supplied failover offset for chan %s "
                          "primary stream new source %s, "
                          "Session to source offset = %d\n",
                          vqec_dpchan_print_name(in->chan),
                          vqec_dp_rtp_key_to_str(&rtp_src_entry->key),
                          session_rtp_seq_num_offset);
        } else {
            /*
             * Use an offset learned by the data plane.
             *
             * Note that the offset is derived by assuming the highest
             * numbered packet from the old and lowest numbered packet
             * from the new source are to be spliced together.  It is
             * not possible to determine whether this splice is correct
             * w/o additional information.
             */
            pak = VQE_TAILQ_FIRST(&in->failoverq_lh);
            if (pak) {                
                /* Find packet on the queue with the lowest seq num */
                failoverq_lowest_seq_num = ntohs(pak->rtp->sequence);
                VQE_TAILQ_FOREACH_SAFE(pak, &in->failoverq_lh, 
                                       inorder_obj, pak_next) {
                    if (vqec_rtp_seq_num_lt(ntohs(pak->rtp->sequence),
                                            failoverq_lowest_seq_num)) {
                        failoverq_lowest_seq_num = ntohs(pak->rtp->sequence);
                    }
                }
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_FAILOVER,
                              "\nFailover queue lowest seq num is %u..",
                              failoverq_lowest_seq_num);

                /* Calculate the offset from the PCM's highest seq num */ 
                session_rtp_seq_num_highest =
                    vqec_seq_num_to_rtp_seq_num(
                        vqec_pcm_get_highest_rx_seq_num(
                            vqec_dpchan_pcm_ptr(in->chan)));
                session_rtp_seq_num_next = 
                    vqec_next_rtp_seq_num(session_rtp_seq_num_highest);
                session_rtp_seq_num_offset =
                    (int16_t)(session_rtp_seq_num_next -
                              failoverq_lowest_seq_num);
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_FAILOVER,
                              "Computing failover offset for chan %s primary "
                              "stream new source %s, "
                              "Highest rx pak in pcm = seq %lu, "
                              "Highest rx pak in pcm = rtp seq %u, "
                              "Lowest rx pak from new source = rtp seq %u, "
                              "Session to source offset = %d\n",
                              vqec_dpchan_print_name(in->chan),
                              vqec_dp_rtp_key_to_str(&rtp_src_entry->key),
                              vqec_pcm_get_highest_rx_seq_num(
                                  vqec_dpchan_pcm_ptr(in->chan)),
                              session_rtp_seq_num_highest,
                              failoverq_lowest_seq_num,
                              session_rtp_seq_num_offset);
            } else {
                /* 
                 * Proceed with sources unsynchronized, and log an error.
                 */
                VQEC_DP_SYSLOG_PRINT(
                    CHAN_NEW_SOURCE_SYNC_ERROR,
                    vqec_dp_rtp_key_to_str(&rtp_src_entry->key),
                    vqec_dpchan_print_name(in->chan));
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_FAILOVER,
                              "Failover offset for chan %s primary "
                              "stream new source %s could not be "
                              "determined, sources may be unsynchronized\n",
                              vqec_dpchan_print_name(in->chan),
                              vqec_dp_rtp_key_to_str(&rtp_src_entry->key));
            }
            if (session_rtp_seq_num_offset_out) {
                *session_rtp_seq_num_offset_out = 
                    session_rtp_seq_num_offset;
            }
        }
    }
    
    /*
     * If a primary stream's packetflow source is being changed, record
     * a lower bound on the new source's extended sequence number space
     * for error repair purposes.  Future error repair requests will
     * exclude packets less than this bound (from the old source), as 
     * 1) the new source may not be able to repair packets it never sent, and
     * 2) there is no guarantee that VQE-C can correctly map the missing
     *    sequence numbers in the PCM to sequence numbers used by the
     *    new source
     *    (e.g. the new source's offset may have been computed by the
     *          data plane incorrectly due to lost packets between the
     *          end of the old source's transmission and the beginning 
     *          of the new source's transmission).
     */
    vqec_pcm_set_pktflow_src_seq_num_start(vqec_dpchan_pcm_ptr(in->chan));

    /* Assign the new packetflow source */
    vqec_dp_rtp_src_entry_ena_pktflow_internal(
        (vqec_dp_chan_rtp_input_stream_t *)in, rtp_src_entry,
        session_rtp_seq_num_offset);
    
done:
    return (err);
}


/**---------------------------------------------------------------------------
 * Disable packetflow for a particular source. The method will be meaningful
 * only for primary sessions. After this method has completed, there will be
 * no source which has packetflow permitted on the primary session.
 *
 * @param[in] in Pointer to an RTP input stream.
 * @param[in] key Key of the source on which packetflow should be enabled.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.  
 *---------------------------------------------------------------------------*/
vqec_dp_error_t 
vqec_dp_rtp_src_demote_disable_pktflow (vqec_dp_chan_rtp_input_stream_t *in,
                                        vqec_dp_rtp_src_key_t *key)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_rtp_src_t *rtp_src_entry;
    
    if (!in || !key) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    rtp_src_entry = 
        vqec_dp_rtp_src_entry_get_internal(&in->rtp_recv.src_list, key);
    if (rtp_src_entry) {
        err = vqec_dp_rtp_src_entry_dis_current_pktflow_internal(in,
                                                                 rtp_src_entry);
    } else {
        err = VQEC_DP_ERR_NOT_FOUND;
    }
    
    return (err);
}


/**---------------------------------------------------------------------------
 * Retrieves information for a RTP source, and the session in a single call.
 * This will primarily be used to update receiver statistics.
 * 
 * @param[in] in Pointer to an RTP input stream.
 * @param[in] key Key of the source for which information is to be retrieved.
 * @param[in] update_prior If true, prior packet packet counters should
 * be upated as a result of this call.
 * @param[in] reset_xr_stats Indicates whether or not to reset the XR stats 
 * @param[out] src_info Pointer to output RTP source information (optional). 
 * @param[out] xr_stats Pointer to output RTP XR statistics (optional).  If
 *                      this is non-NULL, the XR stats will be collected and
 *                      written to this location.
 * @param[out] post_er_stats Pointer to output RTP XR post repair statistics.
 *                      If this is non-NULL the post repair XR stats will be
 *                      collected and written to this location
 * @param[out] sess_info Pointer to output RTP session information (optional).
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_rtp_src_get_info (vqec_dp_chan_rtp_input_stream_t *in,
                          vqec_dp_rtp_src_key_t *key,
                          boolean update_prior,
                          boolean reset_xr_stats,
                          vqec_dp_rtp_src_entry_info_t *src_info,
                          rtcp_xr_stats_t *xr_stats,
                          rtcp_xr_post_rpr_stats_t *post_er_stats,
                          vqec_dp_rtp_sess_info_t *sess_info)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_rtp_src_t *rtp_src_entry;
    uint32_t eseq;

    if (!in || !key) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    if (src_info) {
        memset(src_info, 0, sizeof(*src_info));
    }
    if (sess_info) {
        memset(sess_info, 0, sizeof(*sess_info));
    }

    rtp_src_entry = 
        vqec_dp_rtp_src_entry_get_internal(&in->rtp_recv.src_list, key);
    if (rtp_src_entry) {
        if (sess_info) {
            memcpy(sess_info, &in->rtp_recv.session, sizeof(*sess_info));
        }

        if (src_info) {
            memcpy(src_info, &rtp_src_entry->info, sizeof(*src_info));
        }

        if (xr_stats && rtp_src_entry->xr_stats) {
            memcpy(xr_stats, rtp_src_entry->xr_stats, sizeof(*xr_stats));
        }

        if (post_er_stats && rtp_src_entry->post_er_stats) {
            memcpy(post_er_stats, rtp_src_entry->post_er_stats, 
                   sizeof(*post_er_stats));
        }

        if (update_prior) {
            /* Update RTP prior counts */
            rtp_update_prior(&rtp_src_entry->info.src_stats);
        }

        if (reset_xr_stats && rtp_src_entry->xr_stats) {
            /* Initialize the RTCP XR stats */
            eseq = rtp_src_entry->info.src_stats.cycles
                + rtp_src_entry->info.src_stats.max_seq + 1;
            rtcp_xr_init_seq(rtp_src_entry->xr_stats,
                             eseq, 
                             FALSE); 
        }

        if (reset_xr_stats && rtp_src_entry->post_er_stats) {
            /* Initialize the RTCP post ER stats */
            eseq = rtp_src_entry->post_er_stats->source.cycles +
                rtp_src_entry->post_er_stats->source.max_seq + 1;
            rtcp_xr_init_seq(&rtp_src_entry->post_er_stats->xr_stats,
                             eseq,
                             FALSE); 
        }

    } else {
        err = VQEC_DP_ERR_NOT_FOUND;
    }

    return (err);
}


/**---------------------------------------------------------------------------
 * Retrieve RTP session information / RTP source table.
 * 
 * @param[in] in Pointer to an RTP input stream.
 * @param[out] sess_info Pointer to output RTP session information.
 * @param[out] table Pointer to the source table for output (optional).
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_rtp_session_get_info (vqec_dp_chan_rtp_input_stream_t *in,
                              vqec_dp_rtp_sess_info_t *sess_info,
                              vqec_dp_rtp_src_table_t *table)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (!in || !sess_info) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    memset(sess_info, 0, sizeof(*sess_info));
    if (table) {
        memset(table, 0, sizeof(*table));
    }
    
    memcpy(sess_info, &in->rtp_recv.session, sizeof(*sess_info));
    if (table) {        
        /* sa_ignore {all inputs are null-checked} IGNORE_RETURN(1) */ 
        vqec_dp_rtp_src_get_table(in, table);
    }

    return (err);
}


/**---------------------------------------------------------------------------
 *
 * CP-DP API Methods.
 *
 *---------------------------------------------------------------------------*/


/**---------------------------------------------------------------------------
 * Delete an existing source entry. 
 *
 * @param[in] id Identifier of a RTP input stream.
 * @param[in] key Key of the source to be deleted.
 * @param[out] table Pointer to the source table for output (optional).
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.
 *---------------------------------------------------------------------------*/
vqec_dp_error_t 
vqec_dp_chan_rtp_input_stream_src_delete (vqec_dp_streamid_t id,
                                          vqec_dp_rtp_src_key_t *key,
                                          vqec_dp_rtp_src_table_t *table)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_chan_rtp_input_stream_t *in;

    if (id == VQEC_DP_STREAMID_INVALID || !key) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    in = (vqec_dp_chan_rtp_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(id);
    if (!in) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    if (in->type != VQEC_DP_INPUT_STREAM_TYPE_PRIMARY && 
        in->type != VQEC_DP_INPUT_STREAM_TYPE_REPAIR) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    err = vqec_dp_rtp_src_entry_delete(in, key);
    if (table) {        
        /* sa_ignore {all inputs are null-checked} IGNORE_RETURN(1) */ 
        vqec_dp_rtp_src_get_table(in, table);
    }
    
    return (err);
}


/**---------------------------------------------------------------------------
 * Enable packetflow for a particular source. The method will be meaningful
 * only for primary sessions, and will return a failure code for all other 
 * session types. For primary sessions, only one source at any
 * given time will have packetflow enabled. This method will displace any
 * previous source that had packeflow enabled, and disable packetflow on it.
 *
 * @param[in] id Identifier of a RTP input stream.
 * @param[in] key Key of the source on which packetflow should be enabled.
 * @param[out] session_rtp_seq_num_offset Sequence offset between the
 *                1) session's rtp sequence number space and 
 *                2) source's rtp sequence number space (in use by the
 *                   newly chosen packetflow source)
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.  
 *---------------------------------------------------------------------------*/
vqec_dp_error_t 
vqec_dp_chan_rtp_input_stream_src_permit_pktflow (
    vqec_dp_streamid_t id,
    vqec_dp_rtp_src_key_t *key,
    int16_t *session_rtp_seq_num_offset)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_chan_rtp_input_stream_t *in;
    vqec_dp_rtp_src_t *rtp_src_entry;

    if (id == VQEC_DP_STREAMID_INVALID || !key) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    in = (vqec_dp_chan_rtp_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(id);
    if (!in) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    if (in->type != VQEC_DP_INPUT_STREAM_TYPE_PRIMARY) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    rtp_src_entry = 
        vqec_dp_rtp_src_entry_get_internal(&in->rtp_recv.src_list, key);
    if (rtp_src_entry) {
        err = vqec_dp_rtp_src_promote_permit_pktflow(
            (vqec_dp_chan_rtp_primary_input_stream_t *)in, key,
            NULL, session_rtp_seq_num_offset);
    } else {
        err = VQEC_DP_ERR_NOT_FOUND;
    }

    return (err);
}


/**---------------------------------------------------------------------------
 * Disable packetflow for a particular source. The method will be meaningful
 * only for primary sessions, and will return a failure code for all other 
 * session types. For primary sessions, there will be no sources that have
 * pktflow enabled after this call.
 *
 * @param[in] id Identifier of a RTP input stream.
 * @param[in] key Key of the source on which packetflow should be disabled.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.  
 *---------------------------------------------------------------------------*/
vqec_dp_error_t 
vqec_dp_chan_rtp_input_stream_src_disable_pktflow (
    vqec_dp_streamid_t id,
    vqec_dp_rtp_src_key_t *key)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_chan_rtp_input_stream_t *in;
    vqec_dp_rtp_src_t *rtp_src_entry;
    
    if (id == VQEC_DP_STREAMID_INVALID || !key) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    in = (vqec_dp_chan_rtp_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(id);
    if (!in) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    if (in->type != VQEC_DP_INPUT_STREAM_TYPE_PRIMARY) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    rtp_src_entry = 
        vqec_dp_rtp_src_entry_get_internal(&in->rtp_recv.src_list, key);
    if (rtp_src_entry) {
        err = vqec_dp_rtp_src_demote_disable_pktflow(in, key);
    } else {
        err = VQEC_DP_ERR_NOT_FOUND;
    }

    return (err);
}


/**---------------------------------------------------------------------------
 * Get the source table for a particular input stream. 
 *
 * @param[in] id Identifier of a RTP input stream.
 * @param[out] table Pointer to the source table for output (optional).
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.  
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_chan_rtp_input_stream_src_get_table (vqec_dp_streamid_t id,
                                             vqec_dp_rtp_src_table_t *table)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_chan_rtp_input_stream_t *in;

    if (id == VQEC_DP_STREAMID_INVALID || !table) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    in = (vqec_dp_chan_rtp_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(id);
    if (!in) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    if (in->type != VQEC_DP_INPUT_STREAM_TYPE_PRIMARY &&
        in->type != VQEC_DP_INPUT_STREAM_TYPE_REPAIR) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    /* sa_ignore {all inputs are null-checked} IGNORE_RETURN(1) */ 
    vqec_dp_rtp_src_get_table(in, table);
    
    return (err);
}


/**---------------------------------------------------------------------------
 * Retrieve info for the RTCP XR Media Acquisition report
 * 
 * @param[in] id Identifier of a RTP input stream.
 * @param[out] xr_ma_stats Pointer to the XR RMA stats for the RTP source.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_chan_rtp_input_stream_xr_ma_get_info (vqec_dp_streamid_t id,
                                              rtcp_xr_ma_stats_t *xr_ma_stats)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_chan_rtp_input_stream_t *in;

    if (id == VQEC_DP_STREAMID_INVALID) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    in = (vqec_dp_chan_rtp_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(id);
    if (!in) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    if (in->type != VQEC_DP_INPUT_STREAM_TYPE_PRIMARY && 
        in->type != VQEC_DP_INPUT_STREAM_TYPE_REPAIR) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    if (xr_ma_stats) {
        err = vqec_dpchan_get_xr_ma_stats(in->chan, xr_ma_stats);
    }

    return (err);
}

/**---------------------------------------------------------------------------
 * Update statistics /  retrieve information for a RTP source, as specified 
 * in the key.
 * 
 * @param[in] id Identifier of a RTP input stream.
 * @param[in] key Key of the source for which information is to be retrieved.
 * @param[in] update_prior If true, prior packet packet counters should
 * be upated as a result of this call.
 * @param[in] reset_xr_stats Indicated whether or not to reset XR statistics.
 * @param[out] src_info Pointer to output RTP source information (optional). 
 * @param[out] xr_stats Pointer to the output RTP XR statistics (optional).
 * @param[out] post_er_stats Pointer to the output XR Post Repair stats (opt).
 * @param[out] xr_ma_stats Pointer to the XR RMA stats for the RTP source.
 * @param[out] sess_info Pointer to output RTP session information (optional).
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_chan_rtp_input_stream_src_get_info (vqec_dp_streamid_t id,
                                            vqec_dp_rtp_src_key_t *key,
                                            boolean update_prior,
                                            boolean reset_xr_stats,
                                            vqec_dp_rtp_src_entry_info_t
                                                *src_info,
                                            rtcp_xr_stats_t *xr_stats,
                                            rtcp_xr_post_rpr_stats_t
                                            *post_er_stats,
                                            rtcp_xr_ma_stats_t *xr_ma_stats,
                                            vqec_dp_rtp_sess_info_t *sess_info)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_chan_rtp_input_stream_t *in;

    if (id == VQEC_DP_STREAMID_INVALID) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    in = (vqec_dp_chan_rtp_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(id);
    if (!in || !key) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    if (in->type != VQEC_DP_INPUT_STREAM_TYPE_PRIMARY && 
        in->type != VQEC_DP_INPUT_STREAM_TYPE_REPAIR) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    if (xr_ma_stats) {
        err = vqec_dpchan_get_xr_ma_stats(in->chan, xr_ma_stats);
        if (err != VQEC_DP_ERR_OK) {
            return (err);
        }
    }

    err =
        vqec_dp_rtp_src_get_info(in,
                                 key,
                                 update_prior,
                                 reset_xr_stats,
                                 src_info,
                                 xr_stats,
                                 post_er_stats,
                                 sess_info);

    return (err);
}


/**---------------------------------------------------------------------------
 * Retrieve RTP session information / RTP source table.
 * 
 * @param[in] id Identifier of a RTP input stream.
 * @param[out] sess_info Pointer to output RTP session information.
 * @param[out] table Pointer to the source table for output (optional).
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_chan_input_stream_rtp_session_get_info (
    vqec_dp_streamid_t id,
    vqec_dp_rtp_sess_info_t *sess_info,
    vqec_dp_rtp_src_table_t *table) 
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_chan_rtp_input_stream_t *in;

    if (id == VQEC_DP_STREAMID_INVALID || !sess_info) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    in = (vqec_dp_chan_rtp_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(id);
    if (!in) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    if (in->type != VQEC_DP_INPUT_STREAM_TYPE_PRIMARY && 
        in->type != VQEC_DP_INPUT_STREAM_TYPE_REPAIR) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    err =
        vqec_dp_rtp_session_get_info(in, sess_info, table);

    return (err);
}


/**---------------------------------------------------------------------------
 * Update SSRC filter state, and remove any entry from the source table
 * which does not match the filtered SSRC.
 * 
 * @param[in] in Pointer to the RTP input stream.
 * @param[in] ssrc SSRC filter to be set.
 *---------------------------------------------------------------------------*/
static void
vqec_dp_rtp_session_install_ssrc_filter (vqec_dp_chan_rtp_input_stream_t *in,
                                         uint32_t ssrc)
{   
    vqec_dp_rtp_recv_t *rtp_recv;
    vqec_dp_rtp_src_list_t *src_list;
    vqec_dp_rtp_src_t *rtp_src_entry, *rtp_src_next;
    vqec_dp_error_t err;

    rtp_recv = &in->rtp_recv;
    if (rtp_recv) {  
        rtp_recv->filter.ssrc = ssrc;
        rtp_recv->filter.enable = TRUE;
    
        src_list = &rtp_recv->src_list;

        VQE_TAILQ_FOREACH_SAFE(rtp_src_entry, 
                           &src_list->lh_known_sources, 
                           le_known_sources, 
                           rtp_src_next) {
            
            if (rtp_src_entry->key.ssrc != rtp_recv->filter.ssrc) {
                err = vqec_dp_rtp_src_entry_delete_internal(
                    rtp_recv, rtp_src_entry);
                VQEC_DP_ASSERT_FATAL((err == VQEC_DP_ERR_OK), __FUNCTION__);
            }
        }
    }
}
    

/**---------------------------------------------------------------------------
 * Install a SSRC filter / optionally retrieve RTP source table.
 * 
 * @param[in] id Identifier of a RTP input stream.
 * @param[in] ssrc SSRC filter to be set.
 * @param[out] table Pointer to the source table for output (optional).
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t 
vqec_dp_chan_input_stream_rtp_session_add_ssrc_filter (
    vqec_dp_streamid_t id,
    uint32_t ssrc,
    vqec_dp_rtp_src_table_t *table)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_chan_rtp_input_stream_t *in;

    if (id == VQEC_DP_STREAMID_INVALID) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    in = (vqec_dp_chan_rtp_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(id);
    if (!in) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    if (in->type != VQEC_DP_INPUT_STREAM_TYPE_PRIMARY &&
        in->type != VQEC_DP_INPUT_STREAM_TYPE_REPAIR) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    vqec_dp_rtp_session_install_ssrc_filter(in, ssrc);
    if (table) {
        /* sa_ignore {all inputs are null-checked} IGNORE_RETURN(1) */
        vqec_dp_rtp_src_get_table(in, table);
    }

    return (err);
}


static inline void
vqec_dp_rtp_session_uninstall_ssrc_filter (vqec_dp_chan_rtp_input_stream_t *in)
{
    in->rtp_recv.filter.ssrc = 0;
    in->rtp_recv.filter.enable = FALSE;
}

/**---------------------------------------------------------------------------
 * Remove any previously installed a SSRC filter.
 * 
 * @param[in] id Identifier of a RTP input stream.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t 
vqec_dp_chan_input_stream_rtp_session_del_ssrc_filter (vqec_dp_streamid_t id)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_chan_rtp_input_stream_t *in;

    if (id == VQEC_DP_STREAMID_INVALID) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    in = (vqec_dp_chan_rtp_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(id);
    if (!in) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    if (in->type != VQEC_DP_INPUT_STREAM_TYPE_PRIMARY &&
        in->type != VQEC_DP_INPUT_STREAM_TYPE_REPAIR) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    vqec_dp_rtp_session_uninstall_ssrc_filter(in);
    return (err);
}


/**---------------------------------------------------------------------------
 * Update csrc list associated with a RTP source. If the list
 * changes, an interrupt is sent to the control-plane.
 * 
 * @param[in] in Pointer to an RTP input stream.
 * @param[in] rtp_pak Pointer to a rtp packet's header.
 * @param[in] rtp_src_entry Pointer to the rtp source entry for the
 * corresponding rtp packet. 
 *---------------------------------------------------------------------------*/
static void 
vqec_dp_chan_process_rtp_pak_csrc_update (
    vqec_dp_chan_rtp_input_stream_t *in,
    rtptype *rtp_pak,
    vqec_dp_rtp_src_t *rtp_src_entry)
{
    uint32_t *csrc_ptr;
    boolean csrc_chg = FALSE;
                                               
    csrc_ptr = (uint32_t *)(rtp_pak + 1);
    if (rtp_pak->csrc_count == rtp_src_entry->info.csrc_count) {
        if (memcmp(csrc_ptr,
                   rtp_src_entry->info.csrcs,
                   rtp_pak->csrc_count) != 0) {
            csrc_chg = TRUE;
        }
    } else {
        csrc_chg = TRUE;
    }

    if (csrc_chg) {
        int i;
            
        memset(rtp_src_entry->info.csrcs, 0, sizeof(uint32_t) * MAX_CSRCC);
        for (i = 0; i < rtp_pak->csrc_count; i++) {
            rtp_src_entry->info.csrcs[i] = ntohl(csrc_ptr[i]);
        }
        rtp_src_entry->info.csrc_count = rtp_pak->csrc_count;
        
        /* Interrupt control-plane. */
        /* sa_ignore {IRQ bit set even if call fails.} IGNORE_RETURN(4) */
        vqec_dp_chan_rtp_input_stream_src_ev(
            in, 
            rtp_src_entry,
            VQEC_DP_UPCALL_REASON_RTP_SRC_CSRC_UPDATE);
    }
}


/**---------------------------------------------------------------------------
 * RTP-specific processing for a single packet of the primary input stream.
 * If the packet should be accepted / delivered to pcm the method returns 
 * a true, otherwise it returns false.  If a packet represents an alternate
 * source that may be used upon failover, VQE will enqueue it internally
 * and returns false.
 * NOTE: It is assumed that both the primary stream and packet input
 * pointers are null-checked prior to invoking this function.
 *
 * @param[in] in Pointer to the rtp input stream.
 * @param[in] pak Pointer to a RTP packet.
 * @param[in] drop (optional) Set to TRUE if a drop should be counted
 * on the input stream object, FALSE otherwise
 * @param[out] pak_rtp_src_entry (optional) If supplied and the function 
 * returns TRUE, this points to the packet's RTP source entry
 * @param[out] boolean Returns true if the packet should be delivered
 * to the next cache element.
 *---------------------------------------------------------------------------*/
boolean
vqec_dp_chan_rtp_process_primary_pak (
    vqec_dp_chan_rtp_input_stream_t *in, 
    vqec_pak_t *pak,
    boolean *drop,
    vqec_dp_rtp_src_t **pak_rtp_src_entry)
{
    vqec_dp_rtp_recv_t *rtp_recv;
    vqec_pak_t *oldest_pak;
    rtptype *rtp_pak;
    vqec_dp_rtp_src_key_t key;
    rtp_hdr_status_t rtp_status;
    vqec_dp_rtp_src_t *rtp_src_entry = NULL;
    vqec_dp_chan_rtp_primary_input_stream_t *primary_is;
    boolean pcm_insert = FALSE, dropped_pak = FALSE;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    
    
    VQEC_DP_ASSERT_FATAL(in != NULL, __FUNCTION__);
    VQEC_DP_ASSERT_FATAL(pak != NULL, __FUNCTION__);

    rtp_recv = &in->rtp_recv; 
    primary_is = (vqec_dp_chan_rtp_primary_input_stream_t *)in; 

    rtp_status = rtp_validate_hdr(&rtp_recv->session.sess_stats,
                                  vqec_pak_get_head_ptr(pak),
                                  vqec_pak_get_content_len(pak));
    if (!rtp_hdr_ok(rtp_status)) {
        /* Check for stun packet: punt to control plane. */
        if ((vqec_pak_get_head_ptr(pak)[0] & 0xC0) == 0) {
            err = vqec_dpchan_eject_pak(in->chan, in->id, pak);
            if (err == VQEC_DP_ERR_OK) {
                VQEC_DP_TLM_CNT(input_stream_eject_paks, vqec_dp_tlm_get());
            } else {
                VQEC_DP_TLM_CNT(input_stream_eject_paks_failure, 
                                vqec_dp_tlm_get());
            }
            pcm_insert = FALSE;
            goto done;
        }
        in->rtp_in_stats.rtp_parse_drops++;
        dropped_pak = TRUE;
        goto done;
    }

    in->rtp_in_stats.rtp_paks++;
    pak->rtp = (rtpfasttype_t *)vqec_pak_get_head_ptr(pak);
    
    /*
     * Search for an existing rtp source entry. 
     */
    rtp_pak = (rtptype *) pak->rtp;    
    memset(&key, 0, sizeof(vqec_dp_rtp_src_key_t));
    key.ssrc = ntohl(rtp_pak->ssrc);
    memcpy(&key.ipv4.src_addr, &pak->src_addr, sizeof(struct in_addr));
    key.ipv4.src_port = pak->src_port;
    rtp_src_entry = 
        vqec_dp_rtp_src_entry_get_internal(&rtp_recv->src_list, &key);

    if (!rtp_src_entry) {
        /* Build a new RTP source entry. */
        rtp_src_entry = 
            vqec_dp_rtp_src_entry_create_internal(
                &rtp_recv->src_list, &key,
                rtp_recv->max_xr_rle_size,
                rtp_recv->max_post_er_rle_size);
        if (!rtp_src_entry) {
            in->rtp_in_stats.rtp_parse_drops++;
            dropped_pak = TRUE;
            goto done;
        }

        if (rtp_recv->src_list.created == 1) { 
            /*
             * If this is the first source added for the receiver after
             * channel change, enable this source to accept packets.
             *
             * Note that this is the only situation in which the dataplane
             * selects a packetflow source.  Enabling packetflow from the
             * dataplane permits all packets from the source to be accepted,
             * without any loss due to the IPC delay of the control plane
             * being informed of the source creation and assigning packetflow
             * to this source.
             *
             * Packetflow status for this source (as long as it exists) may
             * only be revoked by the control plane as a result of promoting
             * another active source after this one has become inactive.
             */
            vqec_dp_rtp_src_entry_ena_pktflow_internal(in, rtp_src_entry, 0);
        } else if (!primary_is->chan->is_multicast &&
                   !primary_is->failover_rtp_src_entry) {
            /*
             * A new source for a unicast stream has appeared, and no
             * failover source currently exists.  Assign this new source 
             * as the failover source.
             *
             * Failover status for this source (as long as it exists) may
             * only be revoked as a result of this source becoming the
             * packetflow source, or by this source becoming inactive.
             */
            vqec_dp_rtp_set_failover_buffering_internal(
                primary_is, rtp_src_entry);
        }
        
        /* Interrupt control-plane. */
        /* sa_ignore {IRQ bit set even if call fails.} IGNORE_RETURN(3) */
        vqec_dp_chan_rtp_input_stream_src_ev(in, 
                                             rtp_src_entry,
                                             VQEC_DP_UPCALL_REASON_RTP_SRC_NEW);
    }

    rtp_src_entry->info.packets++;
    rtp_src_entry->info.bytes += vqec_pak_get_content_len(pak);
    if (IS_ABS_TIME_ZERO(rtp_src_entry->info.first_rx_time)) {
        rtp_src_entry->info.first_rx_time = pak->rcv_ts;
    }
    rtp_src_entry->info.last_rx_time = pak->rcv_ts;
    rtp_src_entry->rcv_flag = TRUE;

    /* If the state is inactive, set state to active. */
    if (rtp_src_entry->info.state == VQEC_DP_RTP_SRC_INACTIVE) {
        vqec_dp_rtp_src_entry_set_state_active(in, rtp_src_entry);        
    }
    
    /* Determine what to do with the packet... */
    if (rtp_src_entry->info.pktflow_permitted) {
        /*
         * Packet is arriving from the packetflow source,
         * so it may be forwarded to the PCM.
         */
        pcm_insert = TRUE;
        goto done;
    } else if (rtp_src_entry->info.buffer_for_failover) {
        /*
         * Packet could be used if packetflow source becomes inactive,
         * so enqueue it.
         */
        if (primary_is->failover_paks_queued ==
            VQEC_DP_RTP_SRC_FAILOVER_PAKS_MAX) {
            /* If we are at the limit, remove the oldest pak and drop it */
            oldest_pak = VQE_TAILQ_FIRST(&primary_is->failoverq_lh);
            VQE_TAILQ_REMOVE(&primary_is->failoverq_lh, oldest_pak, 
                             inorder_obj);
            primary_is->failover_paks_queued--;
            vqec_pak_free(oldest_pak);
            rtp_src_entry->info.drops++;
            in->rtp_in_stats.rtp_parse_drops++;
            dropped_pak = TRUE;
        }
        vqec_pak_ref(pak);
        VQE_TAILQ_INSERT_TAIL(&primary_is->failoverq_lh, pak, inorder_obj);
        primary_is->failover_paks_queued++;
        goto done;
    } else {
        /*
         * Packet is not from our packetflow or failover source,
         * so it should be dropped.
         */
        rtp_src_entry->info.drops++;
        in->rtp_in_stats.rtp_parse_drops++;
        dropped_pak = TRUE;
        goto done;
    }
done:
    if (pak_rtp_src_entry) {
        *pak_rtp_src_entry = rtp_src_entry;
    }
    if (drop) {
        *drop = dropped_pak;
    }
    return (pcm_insert);
}

/*
 * Defines the largest batch of primary packets processed at once,
 * which is the larger of
 *    - amount of packets read from the input shim at one time
 *    - amount of packets fed from the failover queue at one time
 */
#define VQEC_DP_RTP_PROCESS_PRIMARY_PAKS_MAX                            \
    max(VQEC_DP_STREAM_PUSH_VECTOR_PAKS_MAX, VQEC_DP_RTP_SRC_FAILOVER_PAKS_MAX)

/**---------------------------------------------------------------------------
 * RTP-specific processing for packets of the primary input stream
 * from a single RTP source (SSRC, source IP, source port).
 * Returns the number of packets accepted / delivered to the PCM.
 * NOTE: It is assumed that both the primary stream and packet input
 * pointers are null-checked prior to invoking this function.
 *
 * @param[in] in Pointer to the rtp input stream.
 * @param[in] pak_array Pointer to the packet vector which is to be processed.
 * @param[in] array_len Length of the array.
 * @param[in] current_time The current system time. The value can
 * be left unspecified, i.e., 0, and is essentially used for optimization.
 * @param[in] rtp_src_entry Pointer to the RTP source corresponding to
 * the packets in the pak_array
 * @param[out] uint32_t The method returns the number of packets
 * that were dropped due to failing RTP receive processing.  It will thus 
 * return a value less than or equal to array_len.
 *---------------------------------------------------------------------------*/
uint32_t
vqec_dp_chan_rtp_process_primary_paks_and_insert_to_pcm (
    struct vqec_dp_chan_rtp_input_stream_ *in, 
    vqec_pak_t *pak_array[],
    uint32_t array_len,
    abs_time_t current_time,
    vqec_dp_rtp_src_t *rtp_src_entry)
{
    vqec_dp_rtp_recv_t *rtp_recv;
    rtptype *rtp_pak;
    rtp_hdr_status_t rtp_status;
    rtp_event_t event;
    rtcp_xr_stats_t *rtcp_xr_stats = NULL;
    vqec_pak_t *pak, *l_pak_array[VQEC_DP_RTP_PROCESS_PRIMARY_PAKS_MAX];
  
    uint32_t wr = 0, rtp_drops = 0, i, inserts;
    boolean in_seq = TRUE;
    
    VQEC_DP_ASSERT_FATAL(in != NULL, __FUNCTION__);
    VQEC_DP_ASSERT_FATAL(pak_array != NULL, __FUNCTION__);

    if (IS_ABS_TIME_ZERO(current_time)) {
        current_time = get_sys_time();
    }

    rtp_recv = &in->rtp_recv; 

    for (i = 0; i < array_len; i++) {

        pak = pak_array[i];
        rtp_pak = (rtptype *) pak->rtp;    

        /* Track / update all CSRCs associated with this source. */     
        vqec_dp_chan_process_rtp_pak_csrc_update(in, rtp_pak, rtp_src_entry);

        /*
         * Process and update the sequence number. If it is not within bounds,
         * ignore the packet.
         */
        rtcp_xr_stats = rtp_src_entry->xr_stats;
        rtp_status = rtp_update_seq(&rtp_recv->session.sess_stats, 
                                    &rtp_src_entry->info.src_stats, 
                                    rtcp_xr_stats,
                                    ntohs(rtp_pak->sequence));
        if (!rtp_hdr_ok(rtp_status)) {
            in->rtp_in_stats.rtp_parse_drops++;
            rtp_drops++;
            continue;
        }
        
        /* Update the jitter estimate. */
        rtp_update_jitter(&rtp_src_entry->info.src_stats,
                          rtcp_xr_stats,
                          ntohl(rtp_pak->timestamp),
                          rtp_update_timestamps(&rtp_src_entry->info.src_stats,
                                                pak->rcv_ts));
        event = rtp_hdr_event(rtp_status);
        if (event != RTP_EVENT_NONE) {
            /* IGNORE - we currently take no action based on header event. 
             * In the future, we may need to report certain types of events
             * up to RTP/RTCP, but right now there are no events of interest.
             */
        }
        
        pak->mpeg_payload_offset = RTPHEADERBYTES(pak->rtp);
        pak->rtp_ts = ntohl(rtp_pak->timestamp);        
        pak->type = VQEC_PAK_TYPE_PRIMARY; 
        VQEC_PAK_FLAGS_RESET(&pak->flags, VQEC_PAK_FLAGS_AFTER_EC);

        /*
         * project to extended sequence number space:
         * 1. map per-source RTP sequence number to 
         *    session RTP sequence number
         * 2. map session RTP sequence number to
         *    extended sequence number (32-bit) using historical info.
         */
        pak->seq_num = vqec_seq_num_nearest_to_rtp_seq_num(
            vqec_pcm_get_last_rx_seq_num(vqec_dpchan_pcm_ptr(in->chan)), 
            ntohs(pak->rtp->sequence) + 
            rtp_src_entry->info.session_rtp_seq_num_offset);
        
        /* 
         * Invoke the rcc state machine - it may be possible that
         * the state machine does not accept the packet in it's
         * current state. 
         */
        if (vqec_dpchan_pak_event(in->chan, VQEC_DP_CHAN_RX_PRIMARY_PAK,
                                  pak, current_time) !=
            VQEC_DP_CHAN_PAK_ACTION_ACCEPT) {            
            in->rtp_in_stats.sm_drops++;
            rtp_drops++;
            continue;
        }

        /* determine if the packet is in-order with the previous one. */ 
        if ((wr != 0)  &&
            (pak->seq_num != 
             vqec_next_seq_num(vqec_pcm_get_last_rx_seq_num(
                                   vqec_dpchan_pcm_ptr(in->chan))))) {
            in_seq = FALSE;
        }
        vqec_pcm_set_last_rx_seq_num(vqec_dpchan_pcm_ptr(in->chan),
                                     pak->seq_num);

        l_pak_array[wr] = pak;
        wr++;

    }

    if (wr != 0) {
        /* rtp packets delivered to the next element in the chain. */
        if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
            in->rtp_in_stats.pakseq_inserts += wr;
        }
        inserts = vqec_pcm_insert_packets(vqec_dpchan_pcm_ptr(in->chan), 
                                          (vqec_pak_t **)l_pak_array,
                                          wr,
                                          in_seq,
                                          &in->rtp_in_stats.ses_stats);
        /* some packets may have been dropped on insert */
        in->rtp_in_stats.pakseq_drops += wr - inserts;
    }
    
    return (rtp_drops);
}


/**---------------------------------------------------------------------------
 * RTP-specific processing for a single packet of the repair input stream.
 * If the packet should be accepted / delivered to pcm the method returns 
 * a true, otherwise it returns false. 
 * NOTE: It is assumed that both the repair stream and packet input
 * pointers are null-checked prior to invoking this function.
 *
 * @param[in] in Pointer a rtp input stream object.
 * @param[in] pak Pointer to a RTP packet.
 * @param[in] validate_hdr If true, validate the header.
 * @param[out] session_rtp_seq_num_offset RTP sequence space offset for
 * mapping this receiver's packets into the session's RTP sequence space.
 * @param[out] boolean Returns true if the packet should be delivered
 * to the next cache element.
 *---------------------------------------------------------------------------*/
boolean
vqec_dp_chan_rtp_process_repair_pak (
    vqec_dp_chan_rtp_input_stream_t *in, vqec_pak_t *pak,
    boolean validate_hdr, int16_t *session_rtp_seq_num_offset)
{
    vqec_dp_rtp_recv_t *rtp_recv;
    rtptype *rtp_pak;
    vqec_dp_rtp_src_key_t key;
    rtp_hdr_status_t rtp_status;
    rtp_event_t event;
    vqec_dp_rtp_src_t *rtp_src_entry, *rtp_src_entry_primary = NULL;
    rtcp_xr_stats_t *rtcp_xr_stats = NULL;
    vqec_dp_chan_rtp_primary_input_stream_t *in_primary;

    VQEC_DP_ASSERT_FATAL(in != NULL, __FUNCTION__);
    VQEC_DP_ASSERT_FATAL(pak != NULL, __FUNCTION__);
    VQEC_DP_ASSERT_FATAL(session_rtp_seq_num_offset != NULL, __FUNCTION__);

    rtp_recv = &in->rtp_recv; 
    
    if (validate_hdr) {
        rtp_status = rtp_validate_hdr(&rtp_recv->session.sess_stats,
                                      vqec_pak_get_head_ptr(pak),
                                      vqec_pak_get_content_len(pak));
        if (!rtp_hdr_ok(rtp_status)) {
            return (FALSE);
        }
    }
    
    /*
     * Search for an existing rtp source entry. If ssrc filtering is enabled
     * and the the incoming ssrc is not equal to the ssrc set in the filter
     * drop the packet. This is done prior to src entry lookup.
     */
    rtp_pak = (rtptype *) pak->rtp;    
    memset(&key, 0, sizeof(vqec_dp_rtp_src_key_t));
    key.ssrc = ntohl(rtp_pak->ssrc);
    memcpy(&key.ipv4.src_addr, &pak->src_addr, sizeof(struct in_addr));
    key.ipv4.src_port = pak->src_port;

    if (rtp_recv->filter.enable && 
        rtp_recv->filter.ssrc != key.ssrc) {  
        rtp_recv->filter.drops++;
        VQEC_DP_TLM_CNT(ssrc_filter_drops, vqec_dp_tlm_get());
        return (FALSE);
    }

    rtp_src_entry = 
        vqec_dp_rtp_src_entry_get_internal(&rtp_recv->src_list, &key);
    if (!rtp_src_entry) {

        /* Build a new RTP source entry. */
        rtp_src_entry = 
            vqec_dp_rtp_src_entry_create_internal(
                &rtp_recv->src_list, &key, 
                rtp_recv->max_xr_rle_size,
                0);
        if (!rtp_src_entry) {
            return (FALSE);
        }

        /*
         * We are hearing from a new RTX source.
         *
         * If the channel's primary stream is multicast, then RTP sequence
         * numbers are considered to already map to the session's RTP
         * sequence space, i.e. no offset is needed.
         *
         * If the channel's primary stream is unicast, then RTP sequence
         * numbers from this repair source need to be mapped to the 
         * session's RTP sequence space.  The offset for this RTP source
         * is copied from that used for the primary stream.  If a 
         * matching RTP source cannot be found for the primary stream,
         * the packets are dropped.
         */
        if (in->chan->is_multicast) {
            *session_rtp_seq_num_offset = 0;
        } else {
            in_primary = (vqec_dp_chan_rtp_primary_input_stream_t *)
                vqec_dp_chan_input_stream_id_to_ptr(in->chan->prim_is);
            if (in_primary) {
                rtp_src_entry_primary = 
                    in_primary->rtp_recv.src_list.pktflow_src;
            }
            if (!rtp_src_entry_primary) {
                (void)vqec_dp_rtp_src_entry_delete_internal(
                    rtp_recv, rtp_src_entry);
                return (FALSE);
            }
            *session_rtp_seq_num_offset = 
                rtp_src_entry_primary->info.session_rtp_seq_num_offset;
        }

        /*
         * For the repair session we simply consider all sources to be
         * pktflow sources once they have passed the ssrc filter.
         */
        vqec_dp_rtp_src_entry_ena_pktflow_internal(
            in, rtp_src_entry, *session_rtp_seq_num_offset);

        /* Interrupt control-plane. */
        /* sa_ignore {IRQ bit set even if call fails.} IGNORE_RETURN(3) */
        vqec_dp_chan_rtp_input_stream_src_ev(in, 
                                             rtp_src_entry, 
                                             VQEC_DP_UPCALL_REASON_RTP_SRC_NEW);
    } else {
        *session_rtp_seq_num_offset = 
            rtp_src_entry->info.session_rtp_seq_num_offset;
    }

    rtp_src_entry->info.packets++;
    rtp_src_entry->info.bytes += vqec_pak_get_content_len(pak);
    if (IS_ABS_TIME_ZERO(rtp_src_entry->info.first_rx_time)) {
        rtp_src_entry->info.first_rx_time = pak->rcv_ts;
    }
    rtp_src_entry->info.last_rx_time = pak->rcv_ts;
    rtp_src_entry->rcv_flag = TRUE;

    /* Drop packet if the source does not have packetflow enabled. */
    if (!rtp_src_entry->info.pktflow_permitted) {
        rtp_src_entry->info.drops++;
        return (FALSE);
    }
    
    /* Track / update all CSRCs associated with this source. */     
    vqec_dp_chan_process_rtp_pak_csrc_update(in, rtp_pak, rtp_src_entry);

    /*
     * Process and update the sequence number. If it is not within bounds,
     * ignore the packet.
     */
    rtcp_xr_stats = rtp_src_entry->xr_stats;
    rtp_status = rtp_update_seq(&rtp_recv->session.sess_stats, 
                                &rtp_src_entry->info.src_stats,
                                rtcp_xr_stats,
                                ntohs(rtp_pak->sequence));
    if (!rtp_hdr_ok(rtp_status)) {
        return FALSE;
    }

    /* 
     * Update the jitter estimate.
     */
    rtp_update_jitter(&rtp_src_entry->info.src_stats,
                      rtcp_xr_stats,
                      ntohl(rtp_pak->timestamp),
                      rtp_update_timestamps(&rtp_src_entry->info.src_stats,
                                            pak->rcv_ts));
    event = rtp_hdr_event(rtp_status);
    if (event != RTP_EVENT_NONE) {
        /* IGNORE - we currently take no action based on header event. 
         * In the future, we may need to report certain types of events
         * up to RTP/RTCP, but right now there are no events of interest.
         */
    }

    pak->mpeg_payload_offset = RTPHEADERBYTES(pak->rtp);
    pak->rtp_ts = ntohl(rtp_pak->timestamp);
    
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * RTP-specific processing for a single packet of the fec input stream.
 * If the packet should be accepted / delivered to fec cache the method 
 *  returns a true, otherwise it returns false. 
 * NOTE: It is assumed that both the fec stream and packet input
 * pointers are null-checked prior to invoking this function.
 *
 * @param[in] in Pointer a rtp input stream object.
 * @param[in] pak Pointer to a RTP packet.
 * @param[in] validate_hdr If true, validate the header.
 * @param[out] boolean Returns true if the packet should be delivered
 * to the next cache element.
 *---------------------------------------------------------------------------*/
boolean
vqec_dp_chan_rtp_process_fec_pak (
    vqec_dp_chan_rtp_input_stream_t *in, vqec_pak_t *pak,
    boolean validate_hdr)
{
    vqec_dp_rtp_recv_t *rtp_recv;
    rtp_hdr_status_t rtp_status;
    vqec_dp_rtp_src_t *rtp_src_entry;
    vqec_dp_rtp_src_key_t zero_key;

    VQEC_DP_ASSERT_FATAL(in != NULL, __FUNCTION__);
    VQEC_DP_ASSERT_FATAL(pak != NULL, __FUNCTION__);

    rtp_recv = &in->rtp_recv; 

    if (validate_hdr) {
        rtp_status = rtp_validate_hdr(&rtp_recv->session.sess_stats,
                                      vqec_pak_get_head_ptr(pak),
                                      vqec_pak_get_content_len(pak));
        if (!rtp_hdr_ok(rtp_status)) {
            return (FALSE);
        }
    }

    /*
     * There is one source entry for all sources!
     */
    rtp_src_entry = VQE_TAILQ_FIRST(&rtp_recv->src_list.lh_known_sources);
    if (!rtp_src_entry) {
        /* Build a new RTP source. */
        memset(&zero_key, 0, sizeof(zero_key));
        rtp_src_entry = 
            vqec_dp_rtp_src_entry_create_internal(&rtp_recv->src_list, 
                                                  &zero_key,
                                                  0, 0);
        if (!rtp_src_entry) {
            return (FALSE);
        }       
    }

   rtp_src_entry->info.packets++;
   rtp_src_entry->info.bytes += vqec_pak_get_content_len(pak);
   if (IS_ABS_TIME_ZERO(rtp_src_entry->info.first_rx_time)) {
       rtp_src_entry->info.first_rx_time = pak->rcv_ts;
   }
   rtp_src_entry->info.last_rx_time = pak->rcv_ts;
   rtp_src_entry->rcv_flag = TRUE;

   pak->mpeg_payload_offset = RTPHEADERBYTES(pak->rtp);
   
   return (TRUE);
}


/**---------------------------------------------------------------------------
 * RTP-specific processing for a single packet of the post-repair stream
 * for a channel.  This performs only RTP-related statistic updates.
 *
 * @param[in] in Pointer the rtp input stream object corresponding to the
 * PRIMARY stream of the packet's channel.
 * @param[in] pak Pointer to a RTP packet.
 * @param[in] now Current time.
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_process_post_repair_pak (
    vqec_dp_chan_rtp_input_stream_t *in, vqec_pak_t *pak, abs_time_t now)
{
    vqec_dp_rtp_src_t *rtp_src_entry = NULL;
    rtcp_xr_post_rpr_stats_t *post_er_stats = NULL;

    VQEC_DP_ASSERT_FATAL(in != NULL, __FUNCTION__);
    VQEC_DP_ASSERT_FATAL(pak != NULL, __FUNCTION__);

    /* Lookup the primary source to which this packet corresponds */
    VQE_TAILQ_FOREACH(rtp_src_entry, 
                      &in->rtp_recv.src_list.lh_known_sources, 
                      le_known_sources) {
        if (rtp_src_entry->key.ssrc == ntohl(pak->rtp->ssrc)) {
            break;
        }
    }
        
    if (!rtp_src_entry || !rtp_src_entry->post_er_stats) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                             vqec_dpchan_print_name(in->chan), 
                             __FUNCTION__,
                             "Could not find post repair packet's source:  "
                             "Post Error Repair stats may be incorrect");
        return;
    }

    /* Update the RTCP post repair stats */
    post_er_stats = rtp_src_entry->post_er_stats;

    (void)rtp_update_seq(&post_er_stats->sess, 
                         &post_er_stats->source, 
                         &post_er_stats->xr_stats,
                         vqec_seq_num_to_rtp_seq_num(pak->seq_num) - 
                         rtp_src_entry->info.session_rtp_seq_num_offset);
    
    rtp_update_jitter(&post_er_stats->source,
                      &post_er_stats->xr_stats,
                      pak->rtp_ts,
                      rtp_update_timestamps(&post_er_stats->source, now));
}


/**---------------------------------------------------------------------------
 * Create a RTP source pool for use by all receivers.
 * 
 * @param[in] src_pool_size Number of entries needed in the pool.
 * @param[out] vqec_dp_rtp_src_pool_t* Pointer to the source pool.
 *---------------------------------------------------------------------------*/
vqec_dp_rtp_src_pool_t *
vqec_dp_rtp_src_pool_create (uint32_t src_pool_size)
{
    int flags = O_CREAT;
    return zone_instance_get_loc(
        "dp_rtp_src", 
        flags,
        sizeof(vqec_dp_rtp_src_t),
        src_pool_size,
        NULL, NULL);
}

/**---------------------------------------------------------------------------
 * Create an XR stats pool to preserve resources.
 * 
 * @param[in] pool_size maximum number of xr stats pointers
 * @param[out] rtcp_xr_stats_pool_t* point to xr stats pool.
 *---------------------------------------------------------------------------*/
static rtcp_xr_stats_pool_t *
vqec_dp_rtcp_xr_stats_pool_create (uint32_t pool_size)
{
    int flags = O_CREAT;
    return zone_instance_get_loc(
        "rtcp_xr_stats",
        flags,
        sizeof(rtcp_xr_stats_t),
        pool_size,
        NULL, NULL);
}

/**---------------------------------------------------------------------------
 * Create a Post ER stats pool.
 * 
 * @param[in] pool_size maximum number of Post ER stats pointers
 * @param[out] rtcp_xr_stats_pool_t* point to xr stats pool.
 *---------------------------------------------------------------------------*/
static post_er_stats_pool_t *
vqec_dp_rtcp_post_er_stats_pool_create (uint32_t pool_size)
{
    int flags = O_CREAT;
    return zone_instance_get_loc(
        "post_er_stats",
        flags,
        sizeof(rtcp_xr_post_rpr_stats_t),
        pool_size,
        NULL, NULL);
}

/**---------------------------------------------------------------------------
 * Destroy the RTP source pool.
 *
 * @param[in] pool Pointer to the source pool.
 *---------------------------------------------------------------------------*/
void
vqec_dp_rtp_src_pool_destroy (vqec_dp_rtp_src_pool_t *pool)
{
    if (pool) {
        (void)zone_instance_put(pool);
    }
}

/**---------------------------------------------------------------------------
 * Destroy the XR stats pool.
 *
 * @param[in] pool Pointer to the pool.
 *---------------------------------------------------------------------------*/
void
vqec_dp_rtcp_xr_stats_pool_destroy (rtcp_xr_stats_pool_t *pool)
{
    if (pool) {
        (void)zone_instance_put(pool);
    }
}

/**---------------------------------------------------------------------------
 * Destroy the Post ER stats pool.
 *
 * @param[in] pool Pointer to the pool.
 *---------------------------------------------------------------------------*/
void
vqec_dp_rtcp_post_er_stats_pool_destroy (post_er_stats_pool_t *pool)
{
    if (pool) {
        (void)zone_instance_put(pool);
    }
}

/**---------------------------------------------------------------------------
 * Initialize the RTP receiver for one RTP input stream.
 *
 * @param[in] rtp_recv Pointer to a RTP receiver object.
 * @param[in] par Pointer to parent input stream.
 * @param[in] max_rle_size Maximum memory size for RLE reports.
 * @param[in] max_post_er_rle_size Maximum memory size for post ER RLE reports.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_chan_rtp_receiver_init (
    vqec_dp_rtp_recv_t *rtp_recv,
    struct vqec_dp_chan_rtp_input_stream_ *par,
    uint32_t max_rle_size,
    uint32_t max_post_er_rle_size)
{
    vqec_dp_error_t err = VQEC_DP_ERR_INVALIDARGS;

    if (rtp_recv && par) {
        memset(rtp_recv, 0, sizeof(*rtp_recv));
        
        /* Initialize RTP source list. */
        VQE_TAILQ_INIT(&rtp_recv->src_list.lh_known_sources);
        rtp_recv->input_stream = par;
        rtp_recv->max_xr_rle_size = max_rle_size;
        rtp_recv->max_post_er_rle_size = max_post_er_rle_size;
        
        err = VQEC_DP_ERR_OK;
    }

    return (err);
}

/**---------------------------------------------------------------------------
 * Deinitialize an instance of a RTP receiver.
 *
 * @param[in] in Pointer to a RTP receiver.
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_receiver_deinit (vqec_dp_rtp_recv_t *rtp_recv)
{
    vqec_dp_rtp_src_t *rtp_src_entry, *rtp_src_next;
    vqec_dp_rtp_src_list_t *src_list;
    vqec_dp_error_t err;

    if (rtp_recv) {
        src_list = &rtp_recv->src_list;

        VQE_TAILQ_FOREACH_SAFE(rtp_src_entry, 
                           &src_list->lh_known_sources, 
                           le_known_sources, 
                           rtp_src_next) {
            
            err = vqec_dp_rtp_src_entry_delete_internal(
                rtp_recv, rtp_src_entry);
            VQEC_DP_ASSERT_FATAL((err == VQEC_DP_ERR_OK), __FUNCTION__);
        }
    }
}


/**---------------------------------------------------------------------------
 * Initialize the RTP receiver module.
 *
 * @param[in] params Initialization parameters.
 * @param[out] vqec_dp_error_t VQEC_DP_ERR_OK on success. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_rtp_receiver_module_init (vqec_dp_module_init_params_t *params)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    uint32_t pool_size;

    if (!params ||
        !params->max_channels ||
        !params->max_streams_per_channel) {
        err = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    pool_size = params->max_channels * params->max_streams_per_channel *
                    VQEC_DP_RTP_MAX_KNOWN_SOURCES;

    s_rtp_src_pool = vqec_dp_rtp_src_pool_create(pool_size);

    if (!s_rtp_src_pool) {
        VQEC_DP_SYSLOG_PRINT(INIT_FAILURE, 
                             "RTP source pool creation failed");
	    err = VQEC_DP_ERR_NOMEM;
        goto done;
    }

    s_xr_stats_pool = vqec_dp_rtcp_xr_stats_pool_create(pool_size);

    if (!s_xr_stats_pool) {
        VQEC_DP_SYSLOG_PRINT(INIT_FAILURE,
                             "XR stats pool creation failed in RTP rcvr");
        err = VQEC_DP_ERR_NOMEM;
        goto done;
    }

    s_post_er_stats_pool = 
        vqec_dp_rtcp_post_er_stats_pool_create(
            pool_size / params->max_streams_per_channel);

    if (!s_post_er_stats_pool) {
        VQEC_DP_SYSLOG_PRINT(INIT_FAILURE,
                             "Post ER stats pool creation failed in RTP rcvr");
        err = VQEC_DP_ERR_NOMEM;
        goto done;
    }

done:
    if (err != VQEC_DP_ERR_OK &&
        err != VQEC_DP_ERR_ALREADY_INITIALIZED) {
        vqec_dp_rtp_receiver_module_deinit();
    }
    return (err);
}

/**---------------------------------------------------------------------------
 * Deinitialize the RTP receiver module. No safeguards here
 * if references exist to the zone.
 *---------------------------------------------------------------------------*/
void
vqec_dp_rtp_receiver_module_deinit (void)
{
    vqec_dp_rtp_src_pool_destroy(s_rtp_src_pool);
    s_rtp_src_pool = NULL;
    vqec_dp_rtcp_xr_stats_pool_destroy(s_xr_stats_pool);
    s_xr_stats_pool = NULL;
    vqec_dp_rtcp_post_er_stats_pool_destroy(s_post_er_stats_pool);
    s_post_er_stats_pool = NULL;
}
