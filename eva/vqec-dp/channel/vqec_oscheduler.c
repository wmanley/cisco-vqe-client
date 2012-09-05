/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: VQEC dp output scheduler definitions.
 *
 * Documents:
 *
 *****************************************************************************/
#include <vqec_dpchan.h>
#include "vqec_pcm.h"
#include "vqec_pak.h"
#include "vqec_pak_seq.h"
#include "vam_time.h"
#include <vqec_dp_io_stream.h>
#include "vqec_dp_tlm.h"
#include <utils/zone_mgr.h>
#include "vqec_dp_rtp_input_stream.h"
#include "vqec_dp_rtp_receiver.h"

/*
 * Interpolate a pak's (reordered primary or repair) receive timestamp from
 * neighboring in-order primary paks.  If such interpolation is not possible
 * the pak's existing receive timestamp is returned - otherwise the 
 * interpolated value is returned.
 */
abs_time_t vqec_dp_oscheduler_rx_interpolate (vqec_dp_oscheduler_t *osched,
                                              vqec_pak_t *pak)
{
    vqec_pak_t *next_inorder_pak;
    vqec_seq_num_t seq_delta1, seq_delta2;
    rel_time_t offset;
    abs_time_t interpolate;
    int64_t t_d;

    /* 
     * if proper previous or next timestamp references are not
     * available, return the existing rx timestamp. 
     */
    if (TIME_CMP_A(eq, ABS_TIME_0, osched->last_inorder_pak_ts) ||
        (!osched->last_inorder_pak_seq_valid)) {
        return (pak->rcv_ts);
    }
    next_inorder_pak = vqec_pcm_inorder_pak_list_get_next(osched->pcm);
    if (!next_inorder_pak) {
        return (pak->rcv_ts);
    }
    
    /* d1 = "next inorder seq" - "prev inorder seq" (d1 > 0) */
    if (vqec_seq_num_le(next_inorder_pak->seq_num,
                        osched->last_inorder_pak_seq)) {
        return (pak->rcv_ts); 
    }
    seq_delta1 = vqec_seq_num_sub(next_inorder_pak->seq_num, 
                                  osched->last_inorder_pak_seq);

    /* d2 = "to-be-interpolated seq" - "prev inorder seq" (d2 > 0) */
    if (vqec_seq_num_le(pak->seq_num,
                        osched->last_inorder_pak_seq)) {
        return (pak->rcv_ts); 
    }
    seq_delta2 = vqec_seq_num_sub(pak->seq_num, 
                                  osched->last_inorder_pak_seq); 

    /* a necessary condition is that (d1 > d2) */
    if (vqec_seq_num_le(seq_delta1, 
                        seq_delta2)) {
        return (pak->rcv_ts);         
    }
    
    /* 
     * per pak time = ("next inorder ts" - "prev inorder ts") / d1
     * interpolated ts = "prev inorder ts" + ("per_pak_time * d2) 
     */
    VQEC_DP_ASSERT_FATAL(seq_delta1 != 0, "sched");
    t_d = TIME_GET_R(usec, 
                     TIME_SUB_A_A(next_inorder_pak->rcv_ts, 
                                  osched->last_inorder_pak_ts));    
    offset = TIME_MK_R(usec, VQE_DIV64(t_d * seq_delta2, seq_delta1));
    interpolate = 
        TIME_ADD_A_R(osched->last_inorder_pak_ts, offset);
    
    return (interpolate);
}

/* PCM output session progression states */
enum
{
    PCM_SESSION_STATE_RCC_APP = 0, 
    PCM_SESSION_STATE_RCC_REPAIR,
    PCM_SESSION_STATE_NORMAL,
};

/**
 * Initialize the output scheduler module, then start it.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[in] pcm  Pointer to the associated PCM module.
 * @param[in] max_post_er_rle_size  Maximum RLE size for RTCP post-ER XR.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_init (vqec_dp_oscheduler_t *osched,
                         vqec_pcm_t *pcm,
                         uint32_t max_post_er_rle_size)
{
    if (!osched || !pcm) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    osched->pcm = (struct vqec_pcm_ *)pcm;

    osched->max_post_er_rle_size = max_post_er_rle_size;

    /* initialize nll */
    vqec_nll_init(&osched->nll);

    osched->run_osched = TRUE;

    return VQEC_DP_ERR_OK;
}

/**
 * Deinitialize the output scheduler module.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_deinit (vqec_dp_oscheduler_t *osched)
{
    if (!osched) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    osched->run_osched = FALSE;

    if (osched->pak_pend) {
        vqec_pak_free(osched->pak_pend);
        osched->pak_pend = NULL;
    }

    return VQEC_DP_ERR_OK;
}

/**
 * Add the relevant stream information to the output scheduler module.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[in] streams  Input and output streams info for the DPChan module.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_add_streams (vqec_dp_oscheduler_t *osched,
                                vqec_dp_oscheduler_streams_t streams)
{
    if (!osched) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    osched->streams = streams;

    return VQEC_DP_ERR_OK;
}

/**
 * Notify the output scheduler that the RCC repair burst is finished.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_rcc_burst_done_notify (vqec_dp_oscheduler_t *osched)
{
    if (!osched) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    osched->rcc_burst_done = TRUE;

    return VQEC_DP_ERR_OK;
}

/**
 * Indicates whether or not the RCC repair burst is finished.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[out] boolean  Returns TRUE if the RCC burst is finished;
 *                      FALSE otherwise.
 */
static boolean
vqec_dp_oscheduler_is_rcc_burst_done (vqec_dp_oscheduler_t *osched,
                                      abs_time_t cur_time)
{
    /* check if we were explicitly notified */
    if (osched->rcc_burst_done) {
        return (TRUE);
    }

    /* if dt_repair_end has passed, then we can consider the burst finished */
    if (TIME_CMP_A(ne,
                   osched->outp_log.first_pak_rcv_ts,
                   ABS_TIME_0) &&
        TIME_CMP_A(ge,
                   cur_time,
                   TIME_ADD_A_R(osched->outp_log.first_pak_rcv_ts,
                                osched->pcm->dt_repair_end))) {
        return (TRUE);
    }

    return (FALSE);
}

/**
 * This returns the amount of fast fill that has already been played out.
 */
static inline rel_time_t vqec_dp_fastfill_played (vqec_dp_oscheduler_t *osched,
                                                  abs_time_t cur_ts)
{
    rel_time_t fast_fill_amount = REL_TIME_0;

    if (osched->outp_log.first_pak_sent) {
        if (TIME_CMP_A(ge, cur_ts,
                       osched->outp_log.first_pak_ts)) {
            fast_fill_amount = 
                TIME_SUB_A_A(cur_ts, 
                             osched->outp_log.first_pak_ts);
        }
    }

    return (fast_fill_amount);
}

/**
 * This inline function is used to schedule packets during the fast-fill
 * phase to the decoder.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[in] pak  Pointer to the packet that will be scheduled. 
 * @param[in] cur_time  The current system time.
 * @param[out] done_with_fastfill  flag indicates if fastfill is done.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */

static inline void vqec_dp_fastfill_dcr (vqec_dp_oscheduler_t *osched,
                                         vqec_pak_t *pak,
                                         abs_time_t cur_time,
                                         boolean *done_with_fastfill)
{

    vqec_pcm_t *pcm = osched->pcm;
    rel_time_t fast_fill_amount;

    fast_fill_amount = vqec_dp_fastfill_played(osched, pak->pred_ts);

    /**
     * get the total number of bytes sent during 
     * fast fill time period
     */
    if (pcm->strip_rtp) {
        pcm->total_fast_fill_bytes += 
            vqec_pak_get_content_len(pak) - sizeof(rtpfasttype_t);
    } else {
        pcm->total_fast_fill_bytes += 
            vqec_pak_get_content_len(pak);
    }

    osched->last_pak_pred_ts = pak->pred_ts;
    /**
     * When you are here, that means fast fill is not done yet
     * 
     * During fastfill period, we schedule out packets immediately.
     * So the pred_ts should be the rcv_ts.
     */ 
    if (TIME_CMP_A(gt, pak->rcv_ts, cur_time)) {
        /**
         * recv timestamp is in the future, 
         * not right, set pred_ts to cur_time
         */
        pak->pred_ts = cur_time;
    } else {
        pak->pred_ts = pak->rcv_ts;
    }

    *done_with_fastfill = FALSE;

    if (TIME_CMP_R(le, pcm->fast_fill_time, fast_fill_amount)) {
        boolean disc;
        abs_time_t dummy_ts;

        /**
         * fast fill is done, this is the last packet sent
         * set fast_fill_time to zero and 
         * notify CP to do appropriate actions.
         */
        pcm->fast_fill_time = REL_TIME_0;  
        pcm->fast_fill_end_time = get_sys_time();
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                      "[FASTFILL] fastfill is done, pcm head = %u, tail = %u, "
                      "pak seq# = %u, rtp_ts = %u\n",
                      pcm->head, pcm->tail, pak->seq_num, pak->rtp_ts); 

        /*
         * Reset the NLL, but immediately provide a first sample so that it can
         * continue to predict timestamps from the time of this first sample,
         * which corresponds to the last fast-filled packet.
         */
        vqec_nll_reset(&osched->nll);
        vqec_nll_adjust(&osched->nll, pak->pred_ts, pak->rtp_ts, 
                        REL_TIME_0, &disc, &dummy_ts);

        *done_with_fastfill = TRUE;
    }
}

/**
 * Run the output scheduler module.  If the output scheduler is stopped, this
 * function will not do anything.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[in] cur_time  The current system time.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
#define REORDER_DELAY MSECS(50)
#define MAX_PRED_TS_ERROR MSECS(100)
vqec_dp_error_t
vqec_dp_oscheduler_run (vqec_dp_oscheduler_t *osched,
                        abs_time_t cur_time, boolean *done_with_fastfill)
{
#define OSCHED_ERRSTR_LEN 120
    vqec_pcm_t *pcm;
    vqec_pak_t *pak;
    boolean discontinuity;
    abs_time_t now;
    vqec_seq_num_t last_pak_seq;
    boolean last_pak_seq_valid;
    vqec_dp_isid_t isid;
    const vqec_dp_isops_t *isops;
    vqec_dp_encap_type_t osencap;
    int stream_idx;
    vqec_dp_chan_rtp_input_stream_t *prim_iptr;
    static abs_time_t prev_time = ABS_TIME_0;
    static boolean prev_time_valid = FALSE;
    char errstr[OSCHED_ERRSTR_LEN];

    if (!osched || !done_with_fastfill) {
        return VQEC_DP_ERR_INVALIDARGS;
    } else if (!osched->run_osched) {
        return VQEC_DP_ERR_OK;
    }

    VQEC_DP_ASSERT_FATAL(osched, "sched");
    pcm = osched->pcm;
    VQEC_DP_ASSERT_FATAL(pcm, "sched");

    /* first check to make sure system time hasn't gone backwards */
    if (prev_time_valid && TIME_CMP_A(lt, cur_time, prev_time)) {
        /* system time has gone backwards - reset oscheduler and abort RCC */
        snprintf(errstr, OSCHED_ERRSTR_LEN,
                 "WARNING: system time has gone backwards! "
                 "(prev_time = %llu, cur_time = %llu)\n",
                 TIME_GET_A(usec, prev_time),
                 TIME_GET_A(usec, cur_time));
        VQEC_DP_SYSLOG_PRINT(ERROR, errstr);
        VQEC_DP_TLM_CNT(system_clock_jumpbacks, vqec_dp_tlm_get());
        vqec_pcm_flush(osched->pcm);
        vqec_dp_chan_abort_rcc(osched->pcm->dpchan->id);
        prev_time = cur_time;
        return (VQEC_DP_ERR_OK);
    }

    prev_time = cur_time;
    prev_time_valid = TRUE;

    *done_with_fastfill = FALSE;
    now = cur_time;

    /*
     * check candidate array to see if we need to timeout any old candidates
     * that haven't been timed out by the insertion of new received packets
     */
    if (!vqec_pcm_candidates_recently_checked_get(pcm)) {
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_PCM_PAK,
                      "output_event_handler: candidates not "
                      "recently checked; timing out old candidates\n");
        vqec_pcm_timeout_old_candidates(pcm, now);
    } else {
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_PCM_PAK,"output_event_handler: clearing "
                   "candidates_recently_checked flag\n");
        /*
         * reset flag so that output_handler can timeout old cands next time it
         * runs if no new cands have been added before then
         */
        vqec_pcm_candidates_recently_checked_reset(pcm);
    }

    /*
     * If post ER stats will be updated, 
     * cache the primary input stream pointer.
     */
    if (osched->max_post_er_rle_size) {
        prim_iptr = (vqec_dp_chan_rtp_input_stream_t *) 
            vqec_dp_chan_input_stream_id_to_ptr(pcm->dpchan->prim_is);
        if (!prim_iptr) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                                 vqec_dpchan_print_name(pcm->dpchan), 
                                 __FUNCTION__,
                                 "Could not find channel's input stream:  "
                                 "Post Error Repair stats may be incorrect");
        }
    } else {
        prim_iptr = NULL;
    }

    while (TRUE) {
        if (osched->pak_pend) {       /* output packet is pending */
            if (TIME_CMP_A(le, osched->pak_pend->pred_ts, now)) {

                if (!osched->outp_log.first_pak_sent) {
                    osched->outp_log.first_pak_sent = TRUE;

                    /* 
                     * Use the pred_ts as predicted by the NLL.  During a
                     * fastfill operation, this was stored in last_pak_pred_ts
                     * just before the packet's pred_ts was overwritten.
                     */
                    osched->outp_log.first_pak_ts =
                        (TIME_CMP_A(ne, ABS_TIME_0,
                                    osched->last_pak_pred_ts)) ?
                        osched->last_pak_pred_ts : osched->pak_pend->pred_ts;

                    osched->outp_log.first_pak_rcv_ts = 
                        osched->pak_pend->rcv_ts;
                }

#if HAVE_FCC
                if (!osched->outp_log.first_prim_pak_sent && 
                    (osched->pak_pend->type == VQEC_PAK_TYPE_PRIMARY)) {

                    osched->outp_log.first_prim_pak_sent = TRUE;
                    osched->outp_log.first_prim_pak_ts = now;

                    vqec_pcm_cache_state_first_prim(pcm);
                }
#endif /* HAVE_FCC */

                /* process packet for each connected input stream */
                for (stream_idx = 0;
                     stream_idx < VQEC_DP_OSCHED_STREAMS_MAX;
                     stream_idx++) {
                    isops = osched->streams.isops[stream_idx];
                    isid = osched->streams.isids[stream_idx];
                    osencap = osched->streams.osencap[stream_idx];

                    /* strip RTP headers if necessary */
                    if (osencap == VQEC_DP_ENCAP_UDP) {
                        vqec_pak_adjust_head_ptr(
                            osched->pak_pend,
                            osched->pak_pend->mpeg_payload_offset);

                        /* buff now points to payload, hence zero offset */
                        osched->pak_pend->mpeg_payload_offset = 0;
                    }

                    /* push packet into connected input stream */
                    if (isops && (isid != VQEC_DP_INVALID_ISID)) {
                        isops->receive(isid, osched->pak_pend);
                    }    
                }

                /*
                 * update post-ER XR stats; these stats are collected only when
                 * post-repair-loss-rle is enabled and after the first primary
                 * packet has been scheduled out
                 */
                if (prim_iptr && osched->pcm->pre_prim_outp_stats_done) {
                    vqec_dp_chan_rtp_process_post_repair_pak(prim_iptr,
                                                             osched->pak_pend,
                                                             now);
                }

                vqec_pak_free(osched->pak_pend);
                osched->pak_pend = NULL;
                vqec_pcm_total_tx_paks_bump(pcm);
            } else {
                break;                  /* wait until this packet is sent */
            }
        }

        pak = vqec_pcm_get_next_packet(pcm, NULL);
        if (!pak) {
            /*
             * At the point of output scheduling, no packet was available
             * to be scheduled out. We call this event as a 'tr135_underrun'
             */
            vqec_pcm_log_tr135_underrun(pcm, 1);
            break;              /* no more packets in cache */
        }

        /**
         * Now that we're peeking at the next packet, we need to make sure
         * that we can play it out.  The algorithm for this is as follows:
         *
         *   burst_is_done = (last_repair_seq == first_prim_seq - 1) ||
         *                   (dt_burst_end has passed)
         *
         *   while (played < fastfill_amount) {
         * start:
         *       play repair paks as available;
         *       if (only primary paks available) {
         *           wait until (burst_is_done || repair paks come);
         *           if (burst_is_done) {
         *               play primary paks;
         *           }
         *           if (repair paks come) {
         *               goto start;
         *           }
         *       }
         *   }
         *
         * Note:  The condition (pcm->max_fastfill != 0) is checked to determine
         *        if memory-optimized bursts are enabled.
         */

        /* useful definitions for the conditions tested below */
#define VQEC_OSCHED_MEMOPT_FASTFILL_ENABLED         \
        (TIME_CMP_R(gt,                             \
                    osched->pcm->max_fastfill,      \
                    REL_TIME_0))

#define VQEC_OSCHED_MEMOPT_FASTFILL_IN_PROGRESS                         \
        (TIME_CMP_R(le,                                                 \
                    vqec_dp_fastfill_played(osched,                     \
                                            osched->last_pak_pred_ts),  \
                    osched->pcm->fast_fill_time))

#define VQEC_OSCHED_MEMOPT_FASTFILL_DONE                \
        (TIME_CMP_A(ne,                                 \
                    osched->pcm->fast_fill_end_time,    \
                    ABS_TIME_0))

        /* check to see if we can fastfill this packet out next */
        if (VQEC_OSCHED_MEMOPT_FASTFILL_ENABLED &&
            (VQEC_OSCHED_MEMOPT_FASTFILL_IN_PROGRESS ||
             VQEC_OSCHED_MEMOPT_FASTFILL_DONE)) {
            if (pak->type == VQEC_PAK_TYPE_REPAIR ||
                pak->type == VQEC_PAK_TYPE_APP) {
                /* play the packet out */
            } else if (vqec_dp_oscheduler_is_rcc_burst_done(osched, now)) {
                /* play the packet out */
            } else {
                /* wait for a repair packet to come */
                break;
            }
        }

        /*
         * While doing a fastfill, starting at the time which is
         * minimum_backfill before the end of the fastfill, but after the burst
         * is done, make sure we still maintain the minimum backfill in the
         * jitter buffer.
         *
         * In other words, if a fastfill is in progress or has been done, and
         * the RCC burst is finished, when enough packets have been fastfilled
         * such that:
         *   played >= fastfill_amount - min_backfill
         * enforce the condition:
         *   (pcm->tail - pak->seq_num) * avg_pkt_time >= min_backfill
         */
        if (vqec_dp_oscheduler_is_rcc_burst_done(osched, now)
            &&
            TIME_CMP_R(gt,
                       osched->pcm->fast_fill_time,
                       REL_TIME_0)
            &&
            TIME_CMP_R(ge,
                       vqec_dp_fastfill_played(osched, osched->last_pak_pred_ts),
                       TIME_SUB_R_R(osched->pcm->fast_fill_time,
                                    osched->pcm->min_backfill))
            &&
            TIME_CMP_R(lt,
                       TIME_MULT_R_I(osched->pcm->avg_pkt_time,
                                     vqec_pak_seq_get_num_paks(
                                         osched->pcm->pak_seq)),
                       osched->pcm->min_backfill)) {
            break;
        }

        if (!(VQEC_PAK_FLAGS_ISSET(&pak->flags, 
                                   VQEC_PAK_FLAGS_RX_REORDERED))) {
            /* this case is for primary, in-order packets */
            if (TIME_CMP_A(gt, pak->rcv_ts, now)) {
                /* bad rx ts on packet (> cur time)! - set to "now" */
                pak->rcv_ts = now;
                vqec_pcm_bad_rcv_ts_bump(pcm);
            }
            if (TIME_CMP_A(gt, 
                           TIME_ADD_A_R(pak->rcv_ts, REORDER_DELAY), now)) {
                /* 
                 * primary in-order packet must have been received
                 * REORDER_DELAY in the past to continue.
                 */
                break;                  
            }
        } else {
            /* this case is for repair / primary reordered paks */
            if (TIME_CMP_A(gt, pak->rcv_ts, now)) {
                /* bad rx ts on packet (> cur time)! - set to "now" */
                pak->rcv_ts = now;
                vqec_pcm_bad_rcv_ts_bump(pcm);
            }
        }

        /* 
         * dequeue packet from pcm - also update osched->last_pak_seq, so cache
         * its previous value
         */
        last_pak_seq = osched->last_pak_seq;
        last_pak_seq_valid = osched->last_pak_seq_valid;
        vqec_pak_ref(pak);
        if (vqec_pcm_remove_packet(pcm, pak, now)) {
            if (osched->last_pak_seq_valid && 
                (pak->seq_num != 
                 vqec_next_seq_num(osched->last_pak_seq))) {
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_ERROR_REPAIR,
                           "gap detected on output, seq %u, "
                           "last seq %u\n", 
                           pak->seq_num, osched->last_pak_seq);
                vqec_pcm_output_gap_log(pcm,
                                        osched->last_pak_seq,
                                        pak->seq_num,
                                        now);
                vqec_pcm_output_gap_counter_bump(
                    pcm,
                    vqec_seq_num_sub(pak->seq_num, osched->last_pak_seq) - 1);
            } 

            /* Update TR-135 Statistics */
            if (!VQEC_PAK_FLAGS_ISSET(&pak->flags, 
                                      VQEC_PAK_FLAGS_AFTER_EC)) {
                vqec_pcm_update_tr135_stats(
                    &pcm->stats.tr135.mainstream_stats.total_stats_before_ec,
                    &pcm->stats.tr135.mainstream_stats.sample_stats_before_ec,
                    pak->seq_num,
                    pcm->tr135_params.gmin,
                    pcm->tr135_params.severe_loss_min_distance);
             }
             vqec_pcm_update_tr135_stats(
                &pcm->stats.tr135.mainstream_stats.total_stats_after_ec,
                &pcm->stats.tr135.mainstream_stats.sample_stats_after_ec,
                pak->seq_num,
                pcm->tr135_params.gmin,
                pcm->tr135_params.severe_loss_min_distance);

            osched->last_pak_seq = pak->seq_num;
            osched->last_pak_seq_valid = TRUE;

#if HAVE_FCC
            if (vqec_pcm_get_rcc_post_abort_process(pcm)) {
                vqec_pcm_update_rcc_post_abort_process(pcm);
            }

            if (last_pak_seq_valid && pcm->primary_received &&
                vqec_seq_num_eq(pak->seq_num, pcm->first_primary_seq)) {
                pcm->stats.first_prim_to_last_rcc_seq_diff = 
                    vqec_seq_num_sub(pak->seq_num, last_pak_seq);
            }
#endif
        }

        if (VQEC_PAK_FLAGS_ISSET(&pak->flags, 
                                 VQEC_PAK_FLAGS_RX_DISCONTINUITY)) {
            discontinuity = TRUE;       /* explicit RTP discontinuity */
        } else {
            discontinuity = FALSE;      /* no explicit RTP discontinuity */
        }

        switch (osched->state) {

        case PCM_SESSION_STATE_RCC_APP: /* handle apps' */

            if (pak->type == VQEC_PAK_TYPE_APP) {  
                /* 
                 * For app paks, pred_ts = rcv_ts, and nll is not adjusted. 
                 * Any "time-base-scheduling"  of apps / replication of apps
                 * must be done by the caller. App's could cause head-of-line
                 * blocking, since they are received out-of-order with respect
                 * to the rcc repair burst, and their rcv_ts maybe APP_TIMEOUT
                 * in the future compared to a repair packet. Caller's should
                 * remove this skew in app's to prevent such behavior.
                 */
                pak->pred_ts = pak->rcv_ts;

            } else {

                if (pak->type == VQEC_PAK_TYPE_REPAIR) {
                    /* 
                     * transition from-app-to-repair packet; in this case the 
                     * next state is RCC_REPAIR, and nll will be in the 
                     * non-tracking mode. Explicit discontinuity is forced.
                     */
                    osched->state = PCM_SESSION_STATE_RCC_REPAIR;
                    discontinuity = TRUE;
                    vqec_nll_adjust(&osched->nll, pak->rcv_ts, pak->rtp_ts, 
                                    REL_TIME_0, &discontinuity, &pak->pred_ts);
                } else {                    
                    /*
                     * transition from-app-to-primary packet; in this case the
                     * next state is RCC_PRIMARY, and nll will be in the
                     * tracking mode.  It is possible that this packet was
                     * "reordered", in which case the packet's rcv_ts will be
                     * improper, i.e., delayed by the reorder time. For this
                     * case a better estimate is not available, therefore, no
                     * corrective action is taken. (This is abnormal behavior
                     * with RCC enabled). Explicit discontiunity is forced.
                     */
                    osched->state = PCM_SESSION_STATE_NORMAL;
                    vqec_nll_set_tracking_mode(&osched->nll);
                    discontinuity = TRUE;
                    vqec_nll_adjust(&osched->nll, pak->rcv_ts, pak->rtp_ts, 
                                    REL_TIME_0, &discontinuity, &pak->pred_ts);
                    
                    if (!VQEC_PAK_FLAGS_ISSET(&pak->flags, 
                                              VQEC_PAK_FLAGS_RX_REORDERED)) {
                        /* 
                         * this is an inorder packet; therefore save 
                         * "last_inorder_pak" state for later prediction.
                         */
                        osched->last_inorder_pak_seq = pak->seq_num;
                        osched->last_inorder_pak_seq_valid = TRUE;
                        osched->last_inorder_pak_ts = pak->rcv_ts;
                    }
                }
            } 
            break;                      /* done with APP case */



        case PCM_SESSION_STATE_RCC_REPAIR:  /* handle repairs post-APP */

            if (pak->type == VQEC_PAK_TYPE_REPAIR) {
                /* 
                 * for repair packets prior to receiving a primary packet,  
                 * compute the sequence number delta between the current 
                 * and the last transmitted packet; scale it by the "average
                 * packet time" for use as the estimated rtp time to use in
                 * both explicit and implicit discontinuities. (based on the 
                 * semantics of seq_num_minus(), the result is always >=0.
                 */
                rel_time_t est_rtp_delta = REL_TIME_0;
                        
                if ((osched->last_pak_seq_valid) &&
                    (vqec_seq_num_gt(pak->seq_num, last_pak_seq))) {

                    vqec_seq_num_t seq_delta = 
                        vqec_seq_num_sub(pak->seq_num, 
                                         last_pak_seq);
                    est_rtp_delta = 
                        TIME_MULT_R_I(vqec_pcm_avg_pkt_time_get(pcm),
                                      seq_delta);
                }

                vqec_nll_adjust(&osched->nll, ABS_TIME_0, pak->rtp_ts, 
                                est_rtp_delta, &discontinuity, &pak->pred_ts);
            } else { 

                /*
                 * transition from-repair-to-primary packet - this will be the
                 * normal case with RCC enabled; nll's mode is set to tracking.
                 * It is possible that this packet was "reordered", in which 
                 * case the  packet's rcv_ts will be improper, i.e., delayed by 
                 * the reorder time. For this case a better estimate is not 
                 * available (no previous in-order primary packet), therefore,
                 * no corrective action is taken. Explicit discontinuity 
                 * is forced.
                 */
                osched->state = PCM_SESSION_STATE_NORMAL;
                vqec_nll_set_tracking_mode(&osched->nll);
                discontinuity = TRUE;
                vqec_nll_adjust(&osched->nll, pak->rcv_ts, pak->rtp_ts, 
                                REL_TIME_0, &discontinuity, &pak->pred_ts);

                if (!VQEC_PAK_FLAGS_ISSET(&pak->flags, 
                                          VQEC_PAK_FLAGS_RX_REORDERED)) {
                    /* 
                     * this is an inorder packet; therefore update 
                     * "last_inorder_pak" state for later prediction.
                     */
                    osched->last_inorder_pak_seq = pak->seq_num;
                    osched->last_inorder_pak_seq_valid = TRUE;
                    osched->last_inorder_pak_ts = pak->rcv_ts;
                }
            }
            break;                      /* done with repair-for-rcc  */



        case PCM_SESSION_STATE_NORMAL:  /* normal operation (primary/repair) */

            if (VQEC_PAK_FLAGS_ISSET(&pak->flags, 
                                     VQEC_PAK_FLAGS_RX_REORDERED)) {
                /*
                 * If this is a reordered packet (either primary reorder or 
                 * repairs), interpolate it's receive ts from the last in-order
                 * packet sent, and an in-order packet which has been received 
                 * after (based on RTP sequence numbers) after this packet.
                 */
                pak->rcv_ts = vqec_dp_oscheduler_rx_interpolate(osched, pak); 

            } else {

                /* 
                 * this is an inorder packet; therefore update 
                 * "last_inorder_pak" state for later prediction.
                 */
                osched->last_inorder_pak_seq = pak->seq_num;
                osched->last_inorder_pak_seq_valid = TRUE;
                osched->last_inorder_pak_ts = pak->rcv_ts;
            }

            vqec_nll_adjust(&osched->nll, pak->rcv_ts, pak->rtp_ts, 
                            REL_TIME_0, &discontinuity, &pak->pred_ts);
            break;                      /* done with normal-mode operations */
            


        default:                        /* not reachable */
            VQEC_DP_ASSERT_FATAL(0, "sched");
            break;
        }                               /* end-of-case */

        if (TIME_CMP_R(gt, pcm->fast_fill_time, REL_TIME_0)) {
            /**
             * Need to do fast fill.
             * If fast_fill_time is not zero, fast fill is not done.
             */
            vqec_dp_fastfill_dcr(osched, pak, cur_time, 
                                 done_with_fastfill);
        }

        /* add the PCM default delay */
        pak->pred_ts = TIME_ADD_A_R(pak->pred_ts,
                                    vqec_pcm_default_delay_get(pcm));

        /* add the packet's APP replication delay */
        pak->pred_ts = TIME_ADD_A_R(pak->pred_ts,
                                    pak->app_cpy_delay);

        osched->pak_pend = pak;       /* last_pak_seq was updated in remove */
        osched->last_pak_ts = pak->rcv_ts;

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
        /* reuse rcv_ts for jitter measurement */
        if (pak->type == VQEC_PAK_TYPE_PRIMARY) {
            pak->rcv_ts = TIME_ADD_A_R(pak->rcv_ts,
                                       vqec_pcm_default_delay_get(pcm));
            pak->rcv_ts = TIME_ADD_A_R(pak->rcv_ts,
                                       osched->nll.primary_offset);
            pak->rcv_ts = TIME_ADD_A_R(pak->rcv_ts,
                                       pak->app_cpy_delay);
        }
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
    }                                   /* end-of-while () */

    return VQEC_DP_ERR_OK;
}

/**
 * Start the output scheduler module.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_start (vqec_dp_oscheduler_t *osched)
{
    if (!osched) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    osched->run_osched = TRUE;

    return VQEC_DP_ERR_OK;
}

/**
 * Stop the output scheduler module.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_stop (vqec_dp_oscheduler_t *osched)
{
    if (!osched) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    osched->run_osched = FALSE;

    return VQEC_DP_ERR_OK;
}


/**
 * Reset the output scheduler module.  This will simply reset the NLL module.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_reset (vqec_dp_oscheduler_t *osched)
{
    if (!osched) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    vqec_nll_reset(&osched->nll);

    return VQEC_DP_ERR_OK;
}


/*
 * Init / reset state of output data block inclusive of the NLL.
 */
void vqec_dp_oscheduler_outp_init (vqec_dp_oscheduler_t *osched)
{
    osched->last_pak_seq_valid = FALSE;
    osched->last_inorder_pak_seq_valid = FALSE;

}

void vqec_dp_oscheduler_outp_reset (vqec_dp_oscheduler_t *osched,
                                    boolean free_pend)
{
    if (free_pend && osched->pak_pend) {
        vqec_pak_free(osched->pak_pend);     
        osched->pak_pend = NULL;
    }
    osched->last_pak_ts = ABS_TIME_0;
    osched->last_pak_seq = 0;
    osched->last_pak_seq_valid = FALSE;
    osched->last_inorder_pak_ts = ABS_TIME_0;
    osched->last_inorder_pak_seq = 0;
    osched->last_inorder_pak_seq_valid = FALSE;
    osched->state = 0;

    vqec_nll_reset(&osched->nll);
    vqec_dp_oscheduler_outp_init(osched);
}

void vqec_oscheduler_event_handler (const vqec_event_t *const evptr,
                                    int unused_fd,
                                    short event, 
                                    void *arg)
{
    boolean done_with_fastfill;
    vqec_dp_oscheduler_run((vqec_dp_oscheduler_t *)arg, 
                           get_sys_time(), &done_with_fastfill);
}
