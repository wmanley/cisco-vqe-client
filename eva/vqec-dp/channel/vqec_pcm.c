/*
 * Copyright (c) 2006-2011 by Cisco Systems, Inc.
 * All rights reserved.
 */

/*
 * vqec_pcm.c - defines packet cache manager for vqec module.
 *
 * Packets are inserted in by seqence number, pcm has an event to
 * send out the packet with the smallest seqence number.
 */

#include "vqec_pcm.h"
#include "vqec_pak.h"
#include "vqec_pak_seq.h"
#include <rtp/rtp.h>
#include "vqec_log.h"
#include "vam_time.h"
#include "vqe_bitmap.h"
#include <vqec_oscheduler.h>

#ifdef _VQEC_DP_UTEST
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

static vqec_pak_seq_pool_t * s_vqec_pak_seq_pool = NULL;

UT_STATIC vqec_seq_num_t vqec_pcm_get_seq_num (const vqec_pak_t * pak) 
{    
    VQEC_DP_ASSERT_FATAL(pak, "pcm");
    return pak->seq_num;
}

/*
 * HELPER FUNCTIONS FOR CANDIDATE ARRAY
 */

/**
 * vqec_pcm_flush_all_candidates
 * @brief
 * Flushes the candidates array for the pcm
 * @param[in] pcm - Pointer to PCM
 * @return none
 */
static inline void
vqec_pcm_flush_all_candidates (vqec_pcm_t * pcm)
{
    pcm->first_candidate = 0;
    pcm->next_candidate = 0;
    pcm->last_candidate = 0;
    memset(pcm->candidates, 0, 
           sizeof(vqec_pcm_candidate_t) * VQEC_PCM_MAX_CANDIDATES);
}

/**
 * vqec_pcm_add_new_candidate
 * @brief
 * This function adds a candidate packet to the array of candidates
 * for next sequence number that has passed the timeout period for
 * retransmission error repair.  These candidates are not made
 * available for retransmission error repair until they have passed the 
 * error repair hold time.
 * @param[in] pcm - Pointer to PCM
 * @param[in] seq_num - Sequence number of candidate packet
 * @param[in] rcv_ts - Receive timestamp of candidate packet
 * @return none
 */
static inline void
vqec_pcm_add_new_candidate (vqec_pcm_t * pcm,
                      vqec_seq_num_t seq_num,
                      abs_time_t rcv_ts)
{
    uint16_t * first = &pcm->first_candidate;
    uint16_t * last = &pcm->last_candidate;
    uint16_t * next = &pcm->next_candidate;

    /* Add new candidate to queue */
    pcm->candidates[*next].seq_num = seq_num;
    pcm->candidates[*next].rcv_ts = rcv_ts;

    /* Update first, if necessary */
    if (*first == *next && *first != *last) {
        *first = 
            (*first == VQEC_PCM_MAX_CANDIDATES - 1) ? 0 : *first + 1;
    }

    /* Update last */
    *last = *next;

    /* Update next */
    *next = (*next == VQEC_PCM_MAX_CANDIDATES - 1) ? 0 : *next + 1;
}

/**
 * vqec_pcm_get_first_candidate
 * @brief
 * This function returns the first (oldest) candidate in the candidate
 * array
 * @param[in] pcm - Pointer to PCM
 * @return pointer to first candidate
 */
static inline vqec_pcm_candidate_t *
vqec_pcm_get_first_candidate (vqec_pcm_t * pcm)
{
    vqec_pcm_candidate_t * first = NULL;

    if (!(pcm->first_candidate == pcm->last_candidate &&
          pcm->first_candidate == pcm->next_candidate))
        first = &pcm->candidates[pcm->first_candidate];

    return first;
}

/**
 * vqec_pcm_remove_first_candidate
 * @brief
 * Removes the first (oldest) candidate from the candidate array
 * @param[in] pcm - Pointer to PCM
 * @return none
 */
static inline void
vqec_pcm_remove_first_candidate (vqec_pcm_t *pcm)
{
    uint16_t * first = &pcm->first_candidate;
    uint16_t * last = &pcm->last_candidate;

    /* Update first */
    if (*first != *last) {
        *first = 
            (*first == VQEC_PCM_MAX_CANDIDATES - 1) ? 0 : *first + 1;
    } else {
        /* first is the last; reset state of candidate array */
        vqec_pcm_flush_all_candidates(pcm);
    }
}

/**
 * vqec_pcm_get_last_candidate
 * @brief
 * This function returns the last (newest) candidate in the candidate
 * array
 * @param[in] pcm - Pointer to PCM
 * @return pointer to last candidate
 */
static inline vqec_pcm_candidate_t *
vqec_pcm_get_last_candidate (vqec_pcm_t * pcm)
{
    vqec_pcm_candidate_t * last = NULL;

    if (pcm->last_candidate != pcm->next_candidate)
        last = &pcm->candidates[pcm->last_candidate];

    return last;
}

/**
 * vqec_pcm_timeout_old_candidates
 * @brief
 * This function times out (removes) all candidates that are older than the
 * gap_hold_time minus the cur_time.
 *
 * Every time the output scheduler wakes up, it'll check to see if the
 * candidate array has been recently updated (i.e. a packet has been inserted
 * since the last time the output scheduler checked this), and if not, then
 * time out all packets that should have timed out already.  Then, the output
 * scheduler will reset the candidates_recently_checked flag.
 */
inline void
vqec_pcm_timeout_old_candidates (vqec_pcm_t *pcm,
                                 abs_time_t cur_time)
{
    vqec_pcm_candidate_t *first_candidate;

    while (TRUE) {
        first_candidate = vqec_pcm_get_first_candidate(pcm);
        if (!first_candidate) {
            break;
        }
        pcm->highest_er_seq_num = first_candidate->seq_num;
        if (TIME_CMP_A(gt,
                       cur_time,
                       TIME_ADD_A_R(first_candidate->rcv_ts,
                                    pcm->gap_hold_time))) {
            /* need to time out first (oldest) candidate */
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_PCM_PAK,"timeout_old_cands: set "
                       "highest_er_seq to first_cand (%u); now removing it\n",
                first_candidate->seq_num);
            vqec_pcm_remove_first_candidate(pcm);
        } else {
            /* first (oldest) candidate is within gap_hold_time */
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_PCM_PAK,
                          "timeout_old_cands: first_cand OK;"
                          " set highest_er_seq = first_cand = %u\n",
                          first_candidate->seq_num);
            break;
        }
    }
}

/*
 * At gap_hold_time change, update the candidate array bucket width, then walk
 * through the candidate array and time out any older candidates that are
 * beyond the new gap_hold_time value.
 */
inline
void vqec_pcm_update_gap_hold_time (vqec_pcm_t *pcm,
                                    rel_time_t new_hold_time)
{
    vqec_pcm_candidate_t *last_cand;

    pcm->gap_hold_time = new_hold_time;
    pcm->candidate_bucket_width = TIME_DIV_R_I(pcm->gap_hold_time,
                                               VQEC_PCM_MAX_CANDIDATES);
    /* remove all candidates that were just timed out by the holdtime change */
    last_cand = vqec_pcm_get_last_candidate(pcm);
    if (last_cand) {
        vqec_pcm_timeout_old_candidates(pcm,
                                        last_cand->rcv_ts);
    }
}

/* check if we need to update the highest_er_seq_num candidate array */
UT_STATIC
void vqec_pcm_update_candidates (vqec_pcm_t *pcm, vqec_pak_t *pak)
{
    vqec_pcm_candidate_t *first_candidate, *last_candidate;

    first_candidate = vqec_pcm_get_first_candidate(pcm);
    last_candidate = vqec_pcm_get_last_candidate(pcm);
    if (!first_candidate) {
        /* insert the packet as the candidate array is empty */
        vqec_pcm_add_new_candidate(pcm, pak->seq_num, pak->rcv_ts);
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_PCM,
                      "ARRAY EMPTY; add firstcand = {%u, %llu}\n",
                   pak->seq_num, pak->rcv_ts.usec);
    }
    if (last_candidate &&
        TIME_CMP_A(gt,
                   pak->rcv_ts,
                   TIME_ADD_A_R(last_candidate->rcv_ts,
                                pcm->candidate_bucket_width))) {
        /* timeout oldest cand and update highest_er_seq_num */
        vqec_pcm_timeout_old_candidates(pcm, pak->rcv_ts);

        vqec_pcm_add_new_candidate(pcm, pak->seq_num, pak->rcv_ts);

        first_candidate = vqec_pcm_get_first_candidate(pcm);
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_PCM_PAK, "added new cand {%u, %llu} (tail);"
                   " ts diff from first_cand %u (highest_er_seq_num) = %llu\n",
                   pak->seq_num, pak->rcv_ts.usec, first_candidate->seq_num,
                   pak->rcv_ts.usec - first_candidate->rcv_ts.usec);
    }
    /*
     * assert recently checked flag so the output_event_handler doesn't try to
     * time out old candidates when it wakes up
     */
    pcm->candidates_recently_checked = TRUE;
}

/*
 * END OF CANDIDATE ARRAY HELPERS
 */

/*
 * FOLLOWING FUNCTIONS ARE HELPERS FOR THE GAPMAP
 */

/**
 * Walks the gapmap data and assembles an array of vqec_gap_t's for a given
 * sequence number range.
 */
UT_STATIC inline int32_t
vqec_pcm_gap_get_gaps_internal (vqec_pcm_t *pcm,
                                vqec_seq_num_t seq1,
                                vqec_seq_num_t seq2,
                                vqec_dp_gap_t *gap_array,
                                uint32_t array_len,
                                vqec_seq_num_t *highest_seq,
                                boolean *more)
{
    uint16_t buf_idx = 0, gap_count = 0;
    uint32_t gap_idx;
    uint32_t cur_block;
    boolean first_block = TRUE;
    uint8_t bit_offset;
    boolean cur_bit;
    boolean midgap = FALSE;

    *highest_seq = 0;
   
    for (gap_idx = seq1;
         vqec_seq_num_le(gap_idx, seq2);
         gap_idx = vqec_next_seq_num(gap_idx)) {
        bit_offset = gap_idx & 0x1f;  /* bit offset in 32-bit block */
        if (first_block || !bit_offset) {
            if (VQE_BITMAP_OK !=
                vqe_bitmap_get_block(pcm->gapmap, &cur_block, gap_idx)) {
                return (-1);    /* error condition - unlikely except a bug */
            }
            first_block = FALSE;
        }
        cur_bit = (cur_block >> (31 - bit_offset)) & 1;
        if (!cur_bit) {  /* we have a gap at this gap_idx */
            if (!midgap) {  /* start a new vqec_gap_t */
                /*
                 * if at beginning of block, and the rest of the block is within
                 * the requested range, and the rest of the block is all gaps,
                 * then we can process the whole block at once
                 */
                if (!bit_offset &&
                    vqec_seq_num_le(vqec_seq_num_add(gap_idx, 31), seq2) && 
                    cur_block == 0) {
                    /* process the whole block at once */
                    gap_array[buf_idx].start_seq = gap_idx;
                    gap_array[buf_idx].extent = 31;
                    *highest_seq = vqec_seq_num_add(gap_idx, 
                                                    gap_array[buf_idx].extent);
                    gap_count++;
                    midgap = TRUE;
                    /* skip to next block */
                    gap_idx = vqec_seq_num_add(gap_idx, 31); /* loop will
                                                                increment */
                } else {
                    /* process just this single bit */
                    gap_array[buf_idx].start_seq = gap_idx;
                    gap_array[buf_idx].extent = 0;
                    *highest_seq = gap_idx;
                    gap_count++;
                    midgap = TRUE;
                }
            } else {  /* keep going in current vqec_gap_t */
                /*
                 * if at beginning of block, and the rest of the block is within
                 * the requested range, and the rest of the block is all gaps,
                 * then we can process the whole block at once
                 */
                if (!bit_offset &&
                    vqec_seq_num_le(vqec_seq_num_add(gap_idx, 31), seq2) && 
                    cur_block == 0) {
                    /* process the whole block at once */
                    gap_array[buf_idx].extent += 32;
                    *highest_seq = vqec_seq_num_add(*highest_seq, 32);
                    /* skip to next block */
                    gap_idx = vqec_seq_num_add(gap_idx, 31); /* loop will
                                                                increment */
                } else {
                    /* process just this single bit */
                    gap_array[buf_idx].extent++;
                    *highest_seq = vqec_seq_num_add(*highest_seq, 1);
                                /* gap.start + gap.extent may go
                                 * greater than seq_num space,
                                 * which should be okay */
                }
            }
        }
        if (midgap && cur_bit) {
            /* finalize current vqec_gap_t */
            midgap = FALSE;
            buf_idx++;
            if (buf_idx == array_len) {
                *more = TRUE;
                return (buf_idx);
            }
        }

        /*
         * if at beginning of block, and the rest of the block is within the
         * requested range, and no gaps for the rest of the block, then we can
         * skip ahead to the next block
         */
        if (!bit_offset &&
            vqec_seq_num_le(vqec_seq_num_add(gap_idx, 31), seq2) && 
            cur_block == (uint32_t)(-1)) {
            /* skip ahead to next block */
            VQEC_DP_ASSERT_FATAL(!midgap, "pcm");
            gap_idx = vqec_seq_num_add(gap_idx, 31); /* loop will incrmnt */
        }
    } /* end-of-for-loop */
    /* 
     * if the for-loop exits normally, that means there are no more gaps in
     * the range, so return gap_count in *buf_count
     */
    *more = FALSE;

    return (gap_count);
}

boolean 
vqec_pcm_gap_get_gaps (vqec_pcm_t *pcm,
                       vqec_dp_gap_buffer_t *gapbuf,
                       boolean *more)
{
    vqec_seq_num_t high_seq, last_seq, head_seq;
    vqec_seq_num_t seq1, seq2;
    vqec_seq_num_t highest_seq_collected;
    int i;

    if (!pcm || !gapbuf || !more) {
        return (FALSE);
    }

    /*
     * There is no explicit need to memzero the buffer, since
     * num_gaps indexes will be explicity written-to.
     */ 
    gapbuf->num_gaps = 0;
    *more = FALSE;

    if (vqec_pak_seq_get_num_paks(pcm->pak_seq) == 0) {
        return (TRUE);
    }

/* 
 * Some terms used here:
 *  highest_seq -  the sequence number with the highest value that is eligible
 *                 for retransmission error repair
 *  last_req_seq - the last (highest) sequence number that was processed in the
 *                 most recent call to vqec_pcm_gap_get_gaps()
 *  head -         the sequence number at the head of the pcm (smallest value)
 *  highest_seq_collected -
 *                 the highest sequence number collected in the gaps returned
 *                 in gapbuff
 *  
 * So the sequence numbers that are being saved in the state here will have an
 * order like one of the following two cases:
 *
 *     <-- greater seq nums ... lesser seq nums -->
 *  case 1:
 *    highest_seq  ...  last_req_seq ... head
 *  case 2:
 *    highest_seq  ...  head         ... last_req_seq
 *
 * Logic: 1.  We want to request the range from the highest_seq through
 *             the greater(last_req_seq, head).
 *        2.  Then, we want to clear the gaps in the gapmap for the range
 *                [highest_seq_collected, last_req_seq].
 *        3.  Lastly, we need to update last_req_seq:
 *             if 'more' is set to true:  highest_seq_collected.
 *             if 'more' is set to false:  highest_seq.
 */

 
    /* 
     * Timeout old candidates if this is the 1st poll after ER enable. This
     * will ensure that the 1st gap is reported immediately, which will
     * benefit aggressive mode RCC.
     */
    if (!pcm->first_er_poll_done) {
        vqec_pcm_timeout_old_candidates(pcm, get_sys_time());
        pcm->first_er_poll_done = TRUE;
    }
 
    high_seq = pcm->highest_er_seq_num;
    last_seq = pcm->last_requested_er_seq_num;
    head_seq = pcm->head;

    /* step 1 */
    if (vqec_seq_num_gt(last_seq, head_seq)) {  /* case 1 */
        seq1 = vqec_next_seq_num(last_seq);  /* next_seq so we don't request the
                                              * same seq_num twice */
    } else {  /* case 2 */
        seq1 = head_seq;
        pcm->stats.head_ge_last_seq++;
    }
    seq2 = high_seq;

    /*
     * For unicast channels, the (seq2..seq1) range may also be
     * restricted on the low end to exclude gaps within a non-packetflow
     * source's transmission (i.e. gaps in the transmission of a previous
     * source).
     * 
     * Such gaps are not reported to the packetflow source, since the
     * assumption is that the new repair source cannot handle such requests,
     * nor can VQE-C reliably know how to map gaps in such requests to
     * to sequence numbers used by the current packetflow source.
     */
    if (pcm->pktflow_src_seq_num_start_set) {
        if (vqec_seq_num_lt(seq1, pcm->pktflow_src_seq_num_start)) {
            seq1 = pcm->pktflow_src_seq_num_start;
        }
        vqec_pcm_clear_pktflow_src_seq_num_start(pcm);
    }
    
    highest_seq_collected = 0;
    gapbuf->num_gaps = 
        vqec_pcm_gap_get_gaps_internal(pcm, 
                                       seq1, 
                                       seq2, 
                                       gapbuf->gap_list,
                                       VQEC_DP_GAP_BUFFER_MAX_GAPS,
                                       &highest_seq_collected,
                                       more);

    /* Update highest sequence number for next pass. */
    if (gapbuf->num_gaps > 0) {           

        VQEC_DP_DEBUG(VQEC_DP_DEBUG_ERROR_REPAIR,
                   "number of gaps reported: %d, more: %s, "
                   "highest_seq_col:  %u\n",
                   gapbuf->num_gaps, 
                   *more ? "true" : "false",                   
                   highest_seq_collected);

        for(i = 0; i < gapbuf->num_gaps; i++) {
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_ERROR_REPAIR,
                       "  %d - %d\n", 
                       gapbuf->gap_list[i].start_seq,
                       gapbuf->gap_list[i].start_seq + 
                          gapbuf->gap_list[i].extent);            
        }

    } else {
        /* no gaps in range, so set highest_seq_collected to high_seq */
        highest_seq_collected = high_seq;
    }

    /* step 3 */
    if (*more) {
        pcm->last_requested_er_seq_num = highest_seq_collected;
    } else {
        pcm->last_requested_er_seq_num = high_seq;
    }
    return (TRUE);
}

/**
 * Walks the gapmap with a stride looking for set bits (gaps) and returns the
 * number and values of them.
 */
uint16_t vqec_pcm_gap_search (vqec_pcm_t *pcm,
                              vqec_seq_num_t start,
                              uint8_t stride,
                              uint8_t num_paks,
                              vqec_seq_num_t *gap_seq_buf)
{
    vqec_seq_num_t gap_idx = start;
    uint16_t buf_idx = 0;
    uint32_t cur_block;
    uint16_t cur_block_idx;
    uint16_t last_block_idx = 0;
    boolean first_block = TRUE;
    boolean cur_bit;

    if (!pcm || !stride || !gap_seq_buf) {
        return 0;
    }

    while (num_paks) {
        cur_block_idx = gap_idx >> 5;
        if (first_block || last_block_idx != cur_block_idx) {
            if (VQE_BITMAP_OK !=
                vqe_bitmap_get_block(pcm->gapmap, &cur_block, gap_idx)) {
                return FALSE;
            }
            last_block_idx = cur_block_idx;
            first_block = FALSE;
        }
        cur_bit = (cur_block >> (31 - (gap_idx & 0x1f))) & 1;
        if (!cur_bit) {
            gap_seq_buf[buf_idx] = gap_idx;
            buf_idx++;
        }
        gap_idx = vqec_seq_num_add(gap_idx, (vqec_seq_num_t)stride);
        num_paks--;
    }

    return buf_idx;
}

/*
 * END OF GAPMAP HELPERS
 */


void vqec_pcm_gapmap_flush (vqec_pcm_t *pcm) 
{
    VQEC_DP_ASSERT_FATAL(pcm && pcm->gapmap, "pcm");
    if (VQE_BITMAP_OK != vqe_bitmap_flush(pcm->gapmap)) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "bitmap flush failed");
    }
}

/*
 * Check if we should generate gap for the packet.
 * We only generate gap for primary stream since we don't want to 
 * repair stream.
 * Also we only generate gap when error_repair is enabled.
 * Note that error_repair is controlled by two parts. 
 * 1. from channel cfg file, SDP file, it's per channel. er_enable will
 * be populated to pcm in vqec_pcm_init.
 * 2. global error_repair is controlled by cli. vqec_get/set_error_repair()
 * is the API to call.
 */
UT_STATIC
boolean vqec_pcm_gap_list_insert_check (vqec_pcm_t * pcm, 
                                        vqec_pak_t *pak,
                                        boolean is_primary_session) 
{
    /* 
     * We only generate gap for primary stream, since we don't want to 
     * repair repair stream.
     */
    if (is_primary_session && !pcm->primary_received) {
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_PCM,
                      "Received first primary packet after channel "
                      "change, don't generate gap. PCM Tail is %u, "
                      "first primary packet is %u\n",
                      pcm->tail, pak->seq_num);
    }
    
    return (is_primary_session && pcm->primary_received);
}


#define INFINITY_U64 (-1)
/**
 * Update the tr135 state machine.
 *
 * @param[in] tr135 Pointer to a tr135 total statistics data structure.
 * @param[in] tr135_s Pointer to a tr135 sample statistics data structure.
 * @param[in] seq_num The current RTP packet sequence number. 
 * @param[in] gmin to be used for TR-135 stats collection.
 * @param[in] severe_loss_min_distance to be used for TR-135 stats collection. 
 */
void
vqec_pcm_update_tr135_stats (vqec_tr135_stream_total_stats_t *tr135, 
                             vqec_tr135_stream_sample_stats_t *tr135_s,
                             vqec_seq_num_t seq_num,
                             uint32_t gmin, 
                             uint32_t severe_loss_min_distance)
{
    /* 
     * difference of 1 means you got a consecutive good pkt, delta of > 1 means 
     * there has been a gap. 
     */
    int32_t num_missing_pkts;
    boolean error_event;

    if(gmin > 0) {
        if (!tr135->start_flag) {
            tr135->last_pak_seq = seq_num - 1;
            tr135->packets_received = 0;
            tr135->packets_expected = 0;
            tr135->packets_lost = 0;
            tr135->start_flag =  TRUE;
            tr135->minimum_loss_distance = INFINITY_U64;
            tr135->maximum_loss_period = 0;
            tr135->mld_low_water_mark = INFINITY_U64;
            tr135->mlp_high_water_mark = 0;
            tr135_s->minimum_loss_distance = INFINITY_U64;
            tr135_s->maximum_loss_period = 0;
            tr135->loss_period = 0;
        }
        
        num_missing_pkts = vqec_seq_num_sub(seq_num, tr135->last_pak_seq) - 1;
        
        error_event = (num_missing_pkts >= 1) ? TRUE : FALSE;
        tr135->packets_received++;
        tr135->packets_lost += num_missing_pkts;
        tr135->packets_expected += (num_missing_pkts + 1);

        switch(tr135->state) {
            case VQEC_STATS_STATE_OK:
                if (error_event == TRUE) {
                    tr135->state = VQEC_STATS_STATE_ENTER_LOSS_EVENT;
                    if ((tr135->num_good_consec_packets <= 
                         severe_loss_min_distance) && 
                        (tr135->loss_events > 0)) {
                        
                        /* We experienced a severe loss event */
                        tr135->severe_loss_index_count++;
                    }
    
                    /* Keep up with minimum loss distance */
                    if (tr135->num_good_consec_packets < 
                        tr135->minimum_loss_distance) {
                       tr135->minimum_loss_distance = 
                           tr135->num_good_consec_packets;
                    }
                    if (tr135->minimum_loss_distance <
                        tr135->mld_low_water_mark) {
                        tr135->mld_low_water_mark = 
                            tr135->minimum_loss_distance;
                    }
                    /* Update sample stats */
                    if (tr135->num_good_consec_packets < 
                        tr135_s->minimum_loss_distance) {
                       tr135_s->minimum_loss_distance = 
                           tr135->num_good_consec_packets;
                    }
                   
                    tr135->loss_period += num_missing_pkts;
                } else {
                    tr135->num_good_consec_packets++;
                }
                break;
    
            case VQEC_STATS_STATE_ENTER_LOSS_EVENT:
                if (error_event == TRUE) {
                    tr135->num_good_consec_packets = 0;
                    tr135->num_bad_consec_packets += num_missing_pkts;
                    tr135->loss_period += num_missing_pkts;
                } else {
                    tr135->num_good_consec_packets++;
                    if (tr135->num_good_consec_packets >= gmin) {
                        tr135->loss_events++;
                        tr135->state = VQEC_STATS_STATE_OK;
                    
                        /* Calculate Maximum Loss Event Period */
                        if (tr135->loss_period > 
                            tr135->maximum_loss_period) {
                            tr135->maximum_loss_period = 
                                tr135->loss_period;
                            tr135->num_bad_consec_packets = 0;
                        }
                        if (tr135->maximum_loss_period >
                            tr135->mlp_high_water_mark) {
                            tr135->mlp_high_water_mark = 
                                tr135->maximum_loss_period;
                        }

                        /* Update sample stats */
                        if (tr135->loss_period > 
                            tr135_s->maximum_loss_period) {
                            tr135_s->maximum_loss_period = 
                                tr135->loss_period;
                        }
                        tr135->loss_period = 0;
                    } else {
                        tr135->loss_period++;
                    }
                }
                break;

            default:
                break;
        }

        tr135->last_pak_seq = seq_num;
    }

}

vqec_dp_error_t vqec_pcm_module_init(uint32_t max_pcms)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (s_vqec_pak_seq_pool) {
        VQEC_DP_SYSLOG_PRINT(INIT_FAILURE,
                             "PCM module already initialized");
        err = VQEC_DP_ERR_ALREADY_INITIALIZED;
        goto done;
    } 

    s_vqec_pak_seq_pool = vqec_pak_seq_pool_create(
                                "pak_seq_pcm",
                                max_pcms,
                                VQEC_PCM_RING_BUFFER_BUCKET_BITS);
    if (!s_vqec_pak_seq_pool) {
        VQEC_DP_SYSLOG_PRINT(INIT_FAILURE,
                             "PCM pak seq pool creation failed");
        err = VQEC_DP_ERR_NOMEM;
        goto done;
    }

  done:
    if (err != VQEC_DP_ERR_OK &&
        err != VQEC_DP_ERR_ALREADY_INITIALIZED) {
        (void)vqec_pcm_module_deinit();
    }
    return err;
}

vqec_dp_error_t vqec_pcm_module_deinit(void)
{
    if (s_vqec_pak_seq_pool) {
        vqec_pak_seq_pool_destroy(s_vqec_pak_seq_pool);
        s_vqec_pak_seq_pool = NULL;
    }
    return VQEC_DP_ERR_OK;
}

boolean vqec_pcm_init (vqec_pcm_t *pcm, vqec_pcm_params_t *vqec_pcm_params)
{

    int factor = 0;  
    vqec_fec_info_t *info = NULL;
    rel_time_t pkt_time;

    if (!pcm || !vqec_pcm_params) {
        VQEC_DP_SYSLOG_PRINT(INVALIDARGS, __FUNCTION__);
        return FALSE;
    }

    memset(pcm, 0, sizeof(vqec_pcm_t));
    pcm->dpchan = vqec_pcm_params->dpchan;

    VQE_TAILQ_INIT(&pcm->inorder_q);
        
    pcm->pak_seq = vqec_pak_seq_create_in_pool(
                        s_vqec_pak_seq_pool,
                        VQEC_PCM_RING_BUFFER_BUCKET_BITS);
    if (!pcm->pak_seq) {
        return FALSE;
    }

    pcm->gapmap = vqe_bitmap_create(VQEC_PCM_GAPMAP_SIZE);

    /*
     * Want to be able to support the following cases (see below for segment
     * definitions/descriptions):
     *  1.  FEC and ER
     *      This is the most complex case, where all of the segments below are
     *      used and non-zero.  In this case, any gaps in the primary stream
     *      will first be attempted to be fixed by FEC, until they pass point
     *      C.  Once past point C, then retransmission-based error-repair will
     *      attempt to fix them.  The retransmission-based error-repair is
     *      attempted once every interval (defined by the time length of (BD)),
     *      and attempts to fix all gaps in the range (CE).
     *  2.  ER Only
     *      In this case, the fec_delay (AB) is 0 in length, and all other
     *      segments will be used and non-zero.  Gaps will be held until point
     *      C, after which point they will be attempted to be fixed by
     *      retransmission-based error-repair once per every interval as
     *      described above.
     *  3.  FEC Only
     *      Here, the segment (BE) will have a length equal to the length of
     *      (BC), and the reorder delay (formerly BC) will have a length of 0.
     *      Only gaps in the range (AB) may have the opportunity to be
     *      repaired by FEC.  If FEC fails to repair them, and they aren't
     *      filled by out-of-order arriving primary packets, then the gaps will
     *      be played out to the decoder as packet loss.
     *  4.  None of Above
     *      This is the simplest case.  Here, the only non-zero segment is
     *      (BC).  This case supports only reordering of out-of-order arriving
     *      primary packets.  There are no error-repair services in this case.
     *
     * Sequence number space diagram (sequence numbers increase to the left
     * (newest packets), and decrease to the right (oldest packets)):
     *
     *  input paks ->|-------------|-----|-----|----------|-> paks to decoder
     *               A             B     C     D          E
     *
     * So, size the default_delay and gap_hold_time as follows:
     *   default_delay = fec_delay + jitter_buffer_size
     *   gap_hold_time = fec_delay + reorder_delay
     *  where
     *   default_delay (AE):  sum total delay of the PCM propagation.
     *   gap_hold_time (AC):  time during which gaps may NOT be requested in
     *                        G-NACKs for retransmission.
     *   fec_delay (AB):  time to allow only FEC to fix any gaps.  This will be
     *                    0 when FEC is disabled; when FEC is enabled, this
     *                    will start as max(2*L*D), and be trimmed down to
     *                    2*L*D when an FEC packet is received.
     *   jitter_buffer_size (BE):  time during which gaps will be requested in
     *                             G-NACKs for retransmission.  This is equal
     *                             to (BC) when ER is disabled.
     *   reorder_delay (BC):  delay to allow reordered packets to enter the PCM
     *                        in the correct place before ER interprets this
     *                        occurrence as a gap and attempts to repair it.
     *                        This has a value of 0 when ER is disabled.
     *
     *   repair_trigger_time (BD):  time interval used to set up the independent
     *                              timer for triggering retransmission error-
     *                              repair event.  This event has no dependence
     *                              on packet reception or any events in the
     *                              system other than the timer set up with this
     *                              interval.
     *
     * Constraint:
     *   The sum of the reorder_delay (BC), the repair_trigger_time (BD), and
     *   the round-trip time between the VQE-C and VQE-S MUST be less than the
     *   jitter_buffer_size (BE), else some gaps may not be fixed before being
     *   played out to the decoder.
     */

    /*
     * if FEC is not enabled, PCM is initted with delay from jitter_buff_size 
     * if FEC is enabled, use a larger size 
     */
    pcm->cfg_er_enable = vqec_pcm_params->er_enable; 
    pcm->rcc_enable = vqec_pcm_params->rcc_enable;
    pcm->fec_enable = vqec_pcm_params->fec_enable;
    
    /* Update the pcm tr135 stats as well as tr135 control params */
    vqec_pcm_set_tr135_params(pcm, &vqec_pcm_params->tr135_params);
    /* Also update tr135 buffersize stats parameter */
    pcm->stats.tr135.buffer_size =
        rel_time_to_usec(vqec_pcm_params->default_delay);
    
    if (pcm->fec_enable) {
        info = &pcm->fec_info;
        memcpy(info, &vqec_pcm_params->fec_info, sizeof(vqec_fec_info_t));
        pcm->fec_default_block_size = vqec_pcm_params->fec_default_block_size;
        /**
         * If the channel has no FEC cached information,
         * use max_block value, otherwise, use use the 
         * cached information
         */
        if (info->fec_l_value != 0 && info->fec_d_value != 0
            && info->fec_order !=  VQEC_DP_FEC_SENDING_ORDER_NOT_DECIDED) {
            switch (info->fec_order)
            {
                case VQEC_DP_FEC_SENDING_ORDER_ANNEXA:
                    factor = info->fec_l_value * info->fec_d_value 
                        + info->fec_l_value;
                    break;

                default:
                    factor = 2 * info->fec_l_value * info->fec_d_value;
                    break;
            }
        } else {
            /* use default block size define at CP */
            factor = pcm->fec_default_block_size * 2;
        }

        /**
         * Choose the per packet time.
         * Here, we use the larger value of the cached rtp_ts_pkt_time
         * and the avg_pkt_time. If there is no cached value, the 
         * rtp_ts_pkt_time should be zero and avg_pkt_time will be used.
         *
         * The reason we use the larger value is that, sometimes, 
         * the value in SDP bitrate may not be set correctly. 
         * If the SDP bitrate was set too large, VQE-C FEC buffering 
         * could have some issue and late_fec_packets could be seen.  
         */

        if (TIME_CMP_R(ge, vqec_pcm_params->avg_pkt_time,
                       info->rtp_ts_pkt_time)) {
            pkt_time = vqec_pcm_params->avg_pkt_time;
        } else {
            pkt_time = info->rtp_ts_pkt_time;
        }

        VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC, 
                      "avg_pkt_time = %llu (us), "
                      "rtp_ts_pkt_time = %llu (us), "
                      "pkt_time used = %llu (us);",
                      TIME_GET_R(usec, vqec_pcm_params->avg_pkt_time),
                      info->rtp_ts_pkt_time,
                      TIME_GET_R(usec, pkt_time));

        pcm->fec_delay = TIME_MULT_R_I(pkt_time, factor);
    } else {
        pcm->fec_delay = REL_TIME_0;
    }

    /* 
     * This is the configured jitter buffer size, excluding FEC, and 
     * is held constant throughout the lifetime of pcm.
     */
    pcm->cfg_delay = vqec_pcm_params->default_delay;

    /* gap_hold_time = fec_delay + reorder_delay */
    pcm->gap_hold_time =
        TIME_ADD_R_R(pcm->fec_delay, vqec_pcm_params->reorder_delay);        

    pcm->reorder_delay = vqec_pcm_params->reorder_delay;
    pcm->repair_trigger_time = vqec_pcm_params->repair_trigger_time;
    pcm->avg_pkt_time = vqec_pcm_params->avg_pkt_time;
    pcm->strip_rtp = vqec_pcm_params->strip_rtp;

    /*
     * If RCC is enabled, we rely on RCC late instantiation 
     * to fill up the VQE-C buffer: the original value of the default_delay
     * parameter excluding FEC delay is saved in cfg_delay. For RCC we
     * disable under-runs and also enable dynamic buffer growth using
     * late-instantiation.
     */
    if (pcm->rcc_enable) {
        /**
         * Set the default delay to reorder delay to 
         * mitigate the side effect of the 20 ms VQE-C timer. 
         */
        pcm->default_delay = pcm->reorder_delay;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC, 
                      "PCM default delay before start of RCC = %llu (ms).",
                      TIME_GET_R(msec, pcm->default_delay));
        pcm->dyn_jitter_buf_act = 
            pcm->underrun_reset_dis = TRUE;
        pcm->er_enable = FALSE; /* local ER state is disabled */
    } else {
        /* default_delay = fec_delay + jitter_buffer_size */
        pcm->default_delay  = 
            TIME_ADD_R_R(pcm->fec_delay, vqec_pcm_params->default_delay);
        /* local ER state is equal to the configured state */
        pcm->er_enable = pcm->cfg_er_enable;  
    }
    pcm->candidate_bucket_width = TIME_DIV_R_I(pcm->gap_hold_time,
                                               VQEC_PCM_MAX_CANDIDATES);

    VQEC_DP_ASSERT_FATAL(vqec_pak_seq_get_num_paks(pcm->pak_seq) == 0, "pcm");
        
    pcm->stats.total_tx_paks = 0;

    pcm->primary_received = FALSE;
    pcm->repair_received = FALSE;

    /*
     * Initialize the last_rx_seq_num base to 32768 (the mid-point of the
     * 16-bit sequence space) to guarantee that the first packet's VQEC
     * sequence number will be in the first 16-bit generation (i.e., in the
     * range [0, 65535]).
     */
    pcm->last_rx_seq_num = 0x8000;

#if HAVE_FCC
    /*
     * Set up the callback to be called when the last repair packet prior to
     * the first primary packet has been received.
     */
    pcm->pre_primary_repairs_done_cb =
        vqec_pcm_params->pre_primary_repairs_done_cb;
    pcm->pre_primary_repairs_done_data =
        vqec_pcm_params->pre_primary_repairs_done_data;
#endif  /* HAVE_FCC */

    /* set up the output scheduler */
    vqec_dp_oscheduler_outp_init(&pcm->osched);

    return TRUE;
}

/**
 * Validate that we can insert this packet into our ring buffer(pcm)
 *
 * Note if pak->seq_num is less than our last_pak_seq, that
 * means the packet is out of date, doesn't make sense to insert it.
 *
 */
UT_STATIC vqec_pcm_insert_error_t 
vqec_pcm_insert_packet_check_dupe (vqec_pcm_t *pcm, vqec_pak_t *pak) 
{
    vqec_pak_t *old_pak;

    old_pak = vqec_pak_seq_find(pcm->pak_seq, pak->seq_num);

    if (old_pak) {

        if ((pak->type == VQEC_PAK_TYPE_REPAIR) &&
            (old_pak->type == VQEC_PAK_TYPE_REPAIR)) {
            /* Repaired by both ER and FEC */ 
            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                pcm->stats.duplicate_repairs++;
            }
        }

        if ((pak->type == VQEC_PAK_TYPE_PRIMARY) &&
            (old_pak->type == VQEC_PAK_TYPE_REPAIR)) {
            /*
             * A primary packet with the same sequence number 
             * disqualifies the old packet to be called "After_EC" packet
             */
            VQEC_PAK_FLAGS_RESET(&old_pak->flags, VQEC_PAK_FLAGS_AFTER_EC);
        }

        return VQEC_PCM_INSERT_ERROR_DUPLICATE_PAK;    
    }

    return VQEC_PCM_INSERT_OK;
}

/**
 * Validate that we can insert this packet into our ring buffer(pcm)
 *
 * Note if pak->seq_num is less than our last_pak_seq, that
 * means the packet is out of date, doesn't make sense to insert it.
 *
 */
UT_STATIC vqec_pcm_insert_error_t 
vqec_pcm_insert_packet_check_range (vqec_pcm_t *pcm,
                                    vqec_seq_num_t low_seq,
                                    vqec_seq_num_t high_seq) 
{
    const int seq_num_gap_max = VQEC_PCM_MAX_GAP_SIZE;
    const int head_tail_dist_max = VQEC_PCM_MAX_HEAD_TAIL_SPREAD;

    /* In the same sequence number space ? */
    if (vqec_pak_seq_get_num_paks(pcm->pak_seq) > 0) {

        /* keep distance b/w head and tail less than head_tail_dist_max */
        if (vqec_seq_num_ge(high_seq, 
                            vqec_seq_num_add(pcm->head, head_tail_dist_max))) {
            /* some are out of range */
            if (vqec_seq_num_ge(low_seq, 
                                vqec_seq_num_add(pcm->head,
                                                 head_tail_dist_max))) {
                /* all are out of range */
                return VQEC_PCM_INSERT_ERROR_BAD_RANGE_PAK;
            }
            return VQEC_PCM_INSERT_ERROR_BAD_RANGE_SOME;
        }

        if (vqec_seq_num_ge(low_seq,
                            vqec_seq_num_add(pcm->tail, seq_num_gap_max))) {
            /* gap from tail to lowest inserted is too large; all invalid */
            return VQEC_PCM_INSERT_ERROR_BAD_RANGE_PAK;
        }

        if (vqec_seq_num_le(high_seq, 
                            vqec_seq_num_sub(pcm->head, seq_num_gap_max))) {
            /* gap from highest inserted to head is too large; all invalid */
            return VQEC_PCM_INSERT_ERROR_BAD_RANGE_PAK;
        }
    }

    /* 
     * can't insert after we already displayed. check is valid only if the pcm
     * is non-empty. If pcm is empty this is a possible wrap around / 
     * stop, restart case and we should allow insertion despite sequence
     * discontinuity in the stream.
     */
    if (vqec_pak_seq_get_num_paks(pcm->pak_seq) &&
        vqec_dp_oscheduler_last_pak_seq_valid(&pcm->osched)) {
        if (vqec_seq_num_le(low_seq,
                            vqec_dp_oscheduler_last_pak_seq_get(
                                &pcm->osched))) {
            /* some are late */
            if (vqec_seq_num_le(high_seq,
                            vqec_dp_oscheduler_last_pak_seq_get(
                                &pcm->osched))) {
                /* all are late */
                return VQEC_PCM_INSERT_ERROR_LATE_PAK;
            }
            return VQEC_PCM_INSERT_ERROR_LATE_SOME;
        }
    }
    
    return VQEC_PCM_INSERT_OK;
}

const char * 
vqec_pcm_insert_error_to_str (vqec_pcm_insert_error_t error)
{
    switch (error) {
    case VQEC_PCM_INSERT_ERROR_DUPLICATE_PAK:
        return "Duplicate pak";
    case VQEC_PCM_INSERT_ERROR_BAD_RANGE_PAK:
        return "Bad range pak";
    case VQEC_PCM_INSERT_ERROR_BAD_RANGE_SOME:
        return "Some bad range paks in vector";
    case VQEC_PCM_INSERT_ERROR_LATE_PAK:
        return "Late pak";
    case VQEC_PCM_INSERT_ERROR_LATE_SOME:
        return "Some late paks in vector";
    case VQEC_PCM_INSERT_PAK_SEQ_INSERT_FAIL:
        return "Pak_seq_insert failure pak";
    default:
        VQEC_DP_ASSERT_FATAL(0, "pcm");
        return "No error";
    }
}

UT_STATIC
void vqec_pcm_insert_packet_update_stats (vqec_pcm_t *pcm, 
                                          vqec_drop_stats_t *ses_stats,
                                          vqec_pak_t *pak,
                                          boolean is_primary_session,
                                          vqec_pcm_insert_error_t error) {
    if (!pcm || !pak) {
        return;
    }

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_PCM,
               "Drop %s packet, from %s seq=%u, "
               "srcadd = 0x%x, src_port = %d, t=%llu, head is %u, "
               "tail is %u\n",
               vqec_pcm_insert_error_to_str(error),
               is_primary_session ? "primary" : "repair",
               pak->seq_num,
               ntohl(pak->src_addr.s_addr),
               ntohs(pak->src_port),
               pak->rcv_ts.usec,
               pcm->head,
               pcm->tail); 

    /* global pcm_insert drop counter */
    pcm->stats.pcm_insert_drops++;

    switch (error) {
    case VQEC_PCM_INSERT_ERROR_DUPLICATE_PAK:
        pcm->stats.duplicate_packet_counter++;
        if (ses_stats) {
            ses_stats->duplicate_packet_counter++;
        }
        break;
    case VQEC_PCM_INSERT_ERROR_BAD_RANGE_PAK:
        pcm->stats.seq_bad_range_packet_counter++;
        break;
    case VQEC_PCM_INSERT_ERROR_LATE_PAK:
        pcm->stats.late_packet_counter++;
        if (ses_stats) {
            ses_stats->late_packet_counter++;
        }
        break;
    case VQEC_PCM_INSERT_PAK_SEQ_INSERT_FAIL:
        pcm->stats.pak_seq_insert_fail_counter++;
        break;
    default:
        VQEC_DP_ASSERT_FATAL(0, "pcm");
    }    
}


/*
 * Inorder packet list helpers - Inorder packet list is a simple tail queue
 * which keeps track of all primary packets which are currently in the
 * pcm, and were received in-order, i.e., they were added either when
 * the pcm was empty, or to the tail of pcm. New inorder packets are
 * added to the tail. The tail queue has the nice property that the packet
 * at the head of the list is the "oldest" primary packet still in pcm;
 * Since packets are removed from pcm in increasing sequence-order, 
 * the oldest or "next-of-kin" primary packet for a repair or reordered 
 * packet is found in O(1) time. The abstraction involves
 *   insert, 
 *   remove, 
 *   get_next (returns head without any other side-effects),
 *   flush
 * Flush must be invoked prior to the flush of the sequence  buckets.
 */
UT_STATIC
void vqec_pcm_inorder_pak_list_insert (vqec_pcm_t *pcm, vqec_pak_t *pak)
{ 
    VQEC_DP_ASSERT_FATAL(!VQEC_PAK_FLAGS_ISSET(&pak->flags, 
                                               VQEC_PAK_FLAGS_RX_REORDERED),
                         "pcm");
    VQE_TAILQ_INSERT_TAIL(&pcm->inorder_q, pak, inorder_obj);
    VQEC_PAK_FLAGS_SET(&pak->flags, 
                       VQEC_PAK_FLAGS_ON_INORDER_Q);
}

UT_STATIC inline
void vqec_pcm_inorder_pak_list_remove (vqec_pcm_t *pcm, 
                                       vqec_pak_t *pak)
{ 
    if (VQEC_PAK_FLAGS_ISSET(&pak->flags, 
                             VQEC_PAK_FLAGS_ON_INORDER_Q)) {
        VQE_TAILQ_REMOVE(&pcm->inorder_q, pak, inorder_obj);
    }
}

inline
vqec_pak_t *vqec_pcm_inorder_pak_list_get_next (vqec_pcm_t *pcm)
{ 
    if (!VQE_TAILQ_EMPTY(&pcm->inorder_q)) {
        return (VQE_TAILQ_FIRST(&pcm->inorder_q));
    }

    return (NULL);
}

UT_STATIC 
void vqec_pcm_inorder_pak_list_flush (vqec_pcm_t *pcm)
{
    vqec_pak_t *cur, *nxt;

    if (!VQE_TAILQ_EMPTY(&pcm->inorder_q)) {
        VQE_TAILQ_FOREACH_SAFE(cur, &pcm->inorder_q, inorder_obj, nxt) {
            VQE_TAILQ_REMOVE(&pcm->inorder_q, cur, inorder_obj);
        }
        VQE_TAILQ_INIT(&pcm->inorder_q);
    }
}

/*
 * Insert the seq num range from seq1 till seq2
 * into gap list.
 */
UT_STATIC
void vqec_pcm_gap_update_stats (vqec_pcm_t *pcm,
                                vqec_seq_num_t seq1,
                                vqec_seq_num_t seq2)
{
    if (!pcm) {
        return;
    }

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_ERROR_REPAIR,
               "seeing input gaps, %u - %u\n",
               seq1, seq2);

    pcm->stats.input_loss_pak_counter += vqec_seq_num_sub(seq2, seq1) + 1;
    pcm->stats.input_loss_hole_counter++;

    vqec_log_insert_input_gap(&pcm->log, seq1, seq2);
}


/**
 * get the RTP timestamp diff of two consecutive packets, 
 * this value is used to set up the FEC buffering.
 * We only use primary or repair packet to calculate the 
 * time. 
 */
static
void vqec_pcm_estimate_pkt_time(vqec_pcm_t *pcm, vqec_pak_t *pak)
{

    if (!pak || !pcm) {
        return;
    }

    if (pak->type == VQEC_PAK_TYPE_PRIMARY || 
        pak->type == VQEC_PAK_TYPE_REPAIR) {
        if (pcm->prev_seq_num == 0 && pcm->prev_rtp_timestamp == 0) {
            /* This is the first time here */
            pcm->prev_seq_num = pak->seq_num;
            pcm->prev_rtp_timestamp = pak->rtp_ts;
        } else if ((pak->seq_num == vqec_next_seq_num(pcm->prev_seq_num)) &&
                   !VQEC_PAK_FLAGS_ISSET(&pak->flags,
                                         VQEC_PAK_FLAGS_RX_DISCONTINUITY)) {
            /* make sure no timestamp wrap up */
            if (pak->rtp_ts > pcm->prev_rtp_timestamp) {
                pcm->new_rtp_ts_pkt_time = 
                    TIME_MK_R(pcr, (pak->rtp_ts - pcm->prev_rtp_timestamp));
                pcm->ts_calculation_done = TRUE;
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC, 
                              "first pack ts = %u, seq_num = %u; "
                              "second pack ts = %u, seq_num = %u; "
                              "rtp_ts_pkt_time = %llu (us), "
                              "avg_pkt_time = %llu (us)\n",
                              pcm->prev_rtp_timestamp,pcm->prev_seq_num,
                              pak->rtp_ts,pak->seq_num,
                              TIME_GET_R(usec, pcm->new_rtp_ts_pkt_time), 
                              TIME_GET_R(usec, pcm->avg_pkt_time));
            } else {
                pcm->prev_seq_num = pak->seq_num;
                pcm->prev_rtp_timestamp = pak->rtp_ts;
            }
        } else {
            pcm->prev_seq_num = pak->seq_num;
            pcm->prev_rtp_timestamp = pak->rtp_ts;
        }
    }
}

/**
 * Input: only generate gap list for primary session.
 * Output: Return the number of packets that have been successfully
 * inserted into the pcm pak sequence.
 */
uint32_t
vqec_pcm_insert_packets (vqec_pcm_t *pcm, vqec_pak_t *paks[], 
                         int num_paks, boolean contig,
                         vqec_drop_stats_t *ses_stats)
{
    vqec_pcm_insert_error_t ret = VQEC_PCM_INSERT_OK;
    boolean gap_update = FALSE;
    uint32_t num_paks_in_seq, wr = 0;
    boolean is_primary_session;
    int pak_idx;
    vqec_pak_t *pak;
    boolean bump_seqs = FALSE;
    boolean check_all_ranges = FALSE;

    VQEC_DP_ASSERT_FATAL(pcm, "pcm");
    VQEC_DP_ASSERT_FATAL(paks, "pcm");
    VQEC_DP_ASSERT_FATAL(pcm->pak_seq, "pcm");

    if (num_paks == 0) {
        return (wr);
    }

    /* this is used in debug message for underrun */
    VQEC_DP_ASSERT_FATAL(paks[0] != NULL, "pcm");

    num_paks_in_seq = vqec_pak_seq_get_num_paks(pcm->pak_seq);

    /*
     * Handle overflow in pcm - this is done at the beginning of this
     * function to minimize side-effects of the pcm flush operation on cached
     * state in automatic variables. If the pcm is full (or will fill up during
     * this insertion), simply flush it.
     */
    if (num_paks_in_seq + num_paks >= 
        (1 << VQEC_PCM_RING_BUFFER_BUCKET_BITS)) {
        vqec_pcm_log_tr135_overrun(pcm, 1); /* log TR-135 overrun event */
        if (!vqec_pcm_flush(pcm)) {
            VQEC_DP_SYSLOG_PRINT(ERROR, "pcm flush failure");
        }
        bump_seqs = TRUE;
    }

    /* 
     * Underrun means empty ring buffer and not the first packet to insert
     */
    if (!num_paks_in_seq &&
        vqec_dp_oscheduler_last_pak_seq_valid(&pcm->osched) &&
        !pcm->underrun_reset_dis) {  /* If under-run resets are not disabled */

        VQEC_DP_DEBUG(VQEC_DP_DEBUG_PCM, 
                      "possible under run, seq = %u, reset nll\n",
                      paks[0]->seq_num);

        pcm->stats.under_run_counter++;
        /*
         * Jitter buffer cannot grow dynamically now, enable static delay for
         * jitter buffer accumulation.
         */ 
        if (pcm->dyn_jitter_buf_act) {
            /* If dynamic jitter buffer is in use, set default delay. */ 
            pcm->default_delay = TIME_ADD_R_R(pcm->cfg_delay,
                                              pcm->fec_delay);
            pcm->dyn_jitter_buf_act = FALSE;
        }
        
        /*
         * Reset output block's state including last_pak_seq.
         */        
        vqec_dp_oscheduler_outp_reset(&pcm->osched, FALSE); 

        /*
         * Bump the current pak's seq_num up a few 16-bit generations.
         */
        bump_seqs = TRUE;

        /*
         * Reset the resultant delay from any app paks.
         */
        pcm->delay_from_apps = REL_TIME_0;
    }

    if (contig) {
        /* check seq ranges for just first and last paks */
        ret = vqec_pcm_insert_packet_check_range(pcm,
                                                 paks[0]->seq_num,
                                                 paks[num_paks - 1]->seq_num);
        switch (ret) {
            case VQEC_PCM_INSERT_ERROR_BAD_RANGE_SOME:
            case VQEC_PCM_INSERT_ERROR_LATE_SOME:
                check_all_ranges = TRUE;
                vqec_pcm_log_tr135_overrun(pcm, 1);
                break;
            case VQEC_PCM_INSERT_ERROR_BAD_RANGE_PAK:
            case VQEC_PCM_INSERT_ERROR_LATE_PAK:
                for (pak_idx = 0; pak_idx < num_paks; pak_idx++) {
                    vqec_pcm_insert_packet_update_stats(
                        pcm, ses_stats, paks[pak_idx],
                        (paks[pak_idx]->type == VQEC_PAK_TYPE_PRIMARY), ret);
                }
                vqec_pcm_log_tr135_overrun(pcm, 1);
                return (wr);
            case VQEC_PCM_INSERT_OK:
                break;
            default:
                VQEC_DP_ASSERT_FATAL(0, "pcm");
        }
    }

    for (pak_idx = 0; pak_idx < num_paks; pak_idx++) {
        /* TODO - isn't this assert a bit too late? */
        VQEC_DP_ASSERT_FATAL(paks[pak_idx], "pcm");
        pak = paks[pak_idx];
        is_primary_session = (pak->type == VQEC_PAK_TYPE_PRIMARY);

        if (bump_seqs) {
            pak->seq_num = vqec_seq_num_bump_seq(pak->seq_num);
        }

        /* adjust delay by any delay resultant from APP replication/delay */
        if (pak->type == VQEC_PAK_TYPE_APP) {
            /* APP packet case */

            /* add per-packet delay to cumulative delay */
            pcm->delay_from_apps = TIME_ADD_R_R(pcm->delay_from_apps,
                                                pak->app_cpy_delay);

            /* set current packet delay equal to cumulative delay */
            pak->app_cpy_delay = pcm->delay_from_apps;

        } else if (TIME_CMP_R(ne, pcm->delay_from_apps, REL_TIME_0) &&
                   TIME_CMP_R(eq, pak->app_cpy_delay, REL_TIME_0)) {
            /* non-APP packet case */
            pak->app_cpy_delay = pcm->delay_from_apps;
        }

        gap_update = vqec_pcm_gap_list_insert_check(pcm, pak,
                                                    is_primary_session);

        /* 
         * Always check for duplicates
         */
        ret = vqec_pcm_insert_packet_check_dupe(pcm, pak);

        if (ret == VQEC_PCM_INSERT_OK && (!contig || check_all_ranges)) {
            /* check seq range for pak */
            ret = vqec_pcm_insert_packet_check_range(pcm,
                                                     pak->seq_num,
                                                     pak->seq_num);
            if (ret != VQEC_PCM_INSERT_OK) {
                vqec_pcm_log_tr135_overrun(pcm, 1);
            }
        }

        if (ret != VQEC_PCM_INSERT_OK) {
            vqec_pcm_insert_packet_update_stats(pcm, ses_stats, pak,
                                                is_primary_session, ret);
            continue;
        }

        /* update counters and debugging logs */
        if (!is_primary_session) {
            if (pak->fec_touched == FEC_TOUCHED) {
                if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                    vqec_log_insert_fec_repair_seq(&pcm->log, pak->seq_num);
                }
            } else {
                if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                    pcm->stats.repair_packet_counter++;
                }
                if (!pcm->repair_received) {
                    pcm->repair_received = TRUE;
                }
                if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                    vqec_log_insert_repair_seq(&pcm->log, pak->seq_num);
                }
            }
            /* all non-primary packets are considered "out-of-order" */
            VQEC_PAK_FLAGS_SET(&pak->flags, 
                               VQEC_PAK_FLAGS_RX_REORDERED);
        } else {
            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                pcm->stats.primary_packet_counter++;
            }
            if (!pcm->primary_received) {
                pcm->primary_received = TRUE;
#if HAVE_FCC
                pcm->first_primary_seq = pak->seq_num;
#endif
            }
            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                vqec_log_insert_primary_seq(&pcm->log, pak->seq_num);
            }
        }

        /**
         * get the RTP timestamp diff of two consecutive packets, 
         * this value is used to set up the FEC buffering.
         */
        if (!pcm->ts_calculation_done) {
            vqec_pcm_estimate_pkt_time(pcm, pak);
        }

        if (vqec_pak_seq_insert(pcm->pak_seq, pak)) {
            /* need to update the gapmap and head and tail */
            if (VQE_BITMAP_OK !=
                vqe_bitmap_set_bit(pcm->gapmap, pak->seq_num)) {
                VQEC_DP_SYSLOG_PRINT(ERROR, "bitmap set bit failure");
            }

            if (vqec_pak_seq_get_num_paks(pcm->pak_seq) == 1){
                pcm->head = pak->seq_num;
                pcm->tail = pak->seq_num;

                if (is_primary_session) {
                    vqec_pcm_inorder_pak_list_insert(pcm, pak);
                    vqec_pcm_update_candidates(pcm, pak);
                }
            } else {
                if (vqec_seq_num_lt(pak->seq_num, pcm->head)) {
                    if (pcm->head != vqec_next_seq_num(pak->seq_num) &&
                        gap_update) {
                        vqec_pcm_gap_update_stats(pcm,
                                               vqec_next_seq_num(pak->seq_num),
                                               vqec_pre_seq_num(pcm->head));
                    }
                    pcm->head = pak->seq_num;
                    /* 
                     * This packet is inserted ahead of the head (in the past.
                     * Hence, it is either a repair packet / or was reordered.
                     * The reordered flag is thus set.
                     */
                    VQEC_PAK_FLAGS_SET(&pak->flags, 
                                       VQEC_PAK_FLAGS_RX_REORDERED);
                } else if (vqec_seq_num_gt(pak->seq_num, pcm->tail)) {
                    if (pak->seq_num != vqec_next_seq_num(pcm->tail) &&
                        gap_update) {
                        vqec_pcm_gap_update_stats(pcm,
                                               vqec_next_seq_num(pcm->tail),
                                               vqec_pre_seq_num(pak->seq_num));
                    }
                    pcm->tail = pak->seq_num;
                    if (is_primary_session) {
                        vqec_pcm_inorder_pak_list_insert(pcm, pak);
                        vqec_pcm_update_candidates(pcm, pak);
                    }
                } else {
                    /* 
                     * This packet is inserted between head & tail. Hence it is 
                     * either a repair packet / or was reordered. In either case
                     * set the reordered flag.
                     */
                    VQEC_PAK_FLAGS_SET(&pak->flags, 
                                       VQEC_PAK_FLAGS_RX_REORDERED);
                }            
            }

            pcm->last_rx_seq_num = pak->seq_num;
            wr++;               /* increment paks accepted counter */

            VQEC_DP_DEBUG(VQEC_DP_DEBUG_PCM_PAK,
                       "inserted packet from %s seq=%u, srcadd = 0x%x, "
                       "src_port = %d, t=%llu, head is %u, tail is %u\n",
                       is_primary_session ? "primary" : "repair",
                       pak->seq_num,
                       ntohl(pak->src_addr.s_addr),
                       ntohs(pak->src_port),
                       pak->rcv_ts.usec,
                       pcm->head,
                       pcm->tail);       
        } else {  /* vqec_pak_seq_insert() == FALSE */
            vqec_pcm_insert_packet_update_stats(pcm, ses_stats, pak, 
                                         is_primary_session,
                                         VQEC_PCM_INSERT_PAK_SEQ_INSERT_FAIL);
        }
    }  /* end of big for-loop */

#if HAVE_FCC
    /*
     * Check to see if the RCC burst is finished.  This is done by looking to
     * see if we have both the first primary packet, as well as the last repair
     * packet from the burst, which should be the sequence number previous to
     * the first primary sequence number.
     *
     * If the RCC burst is finished, call the pre_primary_repairs_done_cb()
     * callback.
     */
    if (pcm->primary_received &&
        !pcm->pre_primary_repairs_done_notified &&
        vqec_pak_seq_find(pcm->pak_seq,
                          vqec_pre_seq_num(pcm->first_primary_seq))) {

        /* call the callback if available */
        if (pcm->pre_primary_repairs_done_cb) {
            pcm->pre_primary_repairs_done_cb(pcm->pre_primary_repairs_done_data);
            pcm->pre_primary_repairs_done_notified = TRUE;
        }

        /* notify the oscheduler that the burst is done */
        vqec_dp_oscheduler_rcc_burst_done_notify(&pcm->osched);
    }
#endif  /* HAVE_FCC */

    return (wr);
}

/*
 * If pak is NULL, return the first one.
 * Be careful of the deadlock, the caller should hold the lock
 */
UT_STATIC inline
vqec_pak_t * vqec_pcm_find_next_packet_internal (vqec_pcm_t *pcm, 
                                                 const vqec_pak_t *pak) 
{    
    vqec_seq_num_t seq_num;
    vqec_seq_num_t temp_seq_num;
    vqec_pak_t * next_pak = NULL;
    uint16_t cur_block_idx, last_block_idx = 0;
    uint32_t cur_block;
    uint8_t bit_offset;
    boolean first_block = TRUE;

    if (!pcm || !pcm->pak_seq) {
        return NULL;
    }

    if (!vqec_pak_seq_get_num_paks(pcm->pak_seq)) {
        return NULL;
    }

    if (!pak) {
        next_pak = vqec_pak_seq_find(pcm->pak_seq, pcm->head);
    } else if (!vqec_pak_seq_find(pcm->pak_seq, 
                                  vqec_pcm_get_seq_num(pak))) {
        /* This packet is not in our cache manager, don't know what
           caller want */
        next_pak = NULL;
    } else {

        seq_num = vqec_pcm_get_seq_num(pak);
        temp_seq_num = vqec_next_seq_num(seq_num);
        
        /*
         * If this is the last packet, don't need to go through the whole 
         */
        if (seq_num == pcm->tail) {            
            return NULL;
        }


        next_pak = vqec_pak_seq_find(pcm->pak_seq, temp_seq_num);
        if (!next_pak) {
            while (vqec_seq_num_lt(temp_seq_num, pcm->tail)) {
                temp_seq_num = vqec_next_seq_num(temp_seq_num);

                /* get a new block if necessary */
                cur_block_idx = temp_seq_num >> VQE_BITMAP_WORD_SIZE_BITS;
                if (first_block || (last_block_idx != cur_block_idx)) {
                    if (VQE_BITMAP_OK !=
                        vqe_bitmap_get_block(pcm->gapmap,
                                             &cur_block, temp_seq_num)) {
                        return NULL;
                    }
                    last_block_idx = cur_block_idx;
                    first_block = FALSE;
                }

                /*
                 * if the rest of the block is within a valid range, and there
                 * are no paks in the current block, then we can skip
                 * ahead to the next block
                 */
                bit_offset = temp_seq_num & 
                    ((1 << VQE_BITMAP_WORD_SIZE_BITS) - 1);  /* 
                                                              * offset in the
                                                              * 32-bit block
                                                              */
                if (cur_block == 0 &&
                    vqec_seq_num_le(vqec_seq_num_add(temp_seq_num, 
                           ((1 << VQE_BITMAP_WORD_SIZE_BITS) - 1) - bit_offset),
                           pcm->tail)) {
                    /* skip the rest of the current block */
                    temp_seq_num = vqec_seq_num_add(temp_seq_num,
                           ((1 << VQE_BITMAP_WORD_SIZE_BITS) - 1) - bit_offset);
                                         /*
                                          * subtract 1 here because the top of
                                          * the loop will increment
                                          * temp_seq_num, giving an effective
                                          * # of bits of a whole word for the
                                          * next iteration
                                          */
                    continue;
                }
                if ((cur_block >> (((1 << VQE_BITMAP_WORD_SIZE_BITS) - 1) -
                                   bit_offset)) & 1) {
                    next_pak = vqec_pak_seq_find(pcm->pak_seq, temp_seq_num);
                    break;
                }
            }
        }
    }

    return next_pak;
}

/*
 * If pak is NULL, return the last one.
 * Be careful of the deadlock, the caller should hold the lock
 */
UT_STATIC inline
vqec_pak_t * vqec_pcm_find_pre_packet_internal (
    vqec_pcm_t *pcm, const vqec_pak_t *pak) 
{
    vqec_seq_num_t seq_num;
    vqec_seq_num_t temp_seq_num;
    vqec_pak_t * pre_pak = NULL;

    if (!pcm || !pcm->pak_seq) {
        return NULL;
    }

    if (!vqec_pak_seq_get_num_paks(pcm->pak_seq)) {
        return NULL;
    }

    if (!pak) {
        pre_pak = vqec_pak_seq_find(pcm->pak_seq, pcm->tail);
    } else if (!vqec_pak_seq_find(pcm->pak_seq, 
                                   vqec_pcm_get_seq_num(pak))) {
        /* 
         * This packet is not in our cache manager, don't know what
         * caller want
         */
        pre_pak = NULL;
    } else {

        seq_num = vqec_pcm_get_seq_num(pak);
        temp_seq_num = vqec_pre_seq_num(seq_num);

        if (seq_num == pcm->head) {
            return NULL;
        }

        while (TRUE) {

            pre_pak = vqec_pak_seq_find(pcm->pak_seq, temp_seq_num);
            
            if (pre_pak) {
                break;
            } else {
                temp_seq_num = vqec_pre_seq_num(temp_seq_num);
                if (vqec_seq_num_lt(temp_seq_num, pcm->head)) {
                    break;
                }
                if (vqec_pak_seq_find_bucket(pcm->pak_seq, temp_seq_num) ==
                     vqec_pak_seq_find_bucket(pcm->pak_seq, seq_num)) {
                    break;
                }
            }
        }
    }

    return pre_pak;
}

boolean vqec_pcm_remove_packet (vqec_pcm_t *pcm, vqec_pak_t *pak,
                                abs_time_t remove_time) 
{
    vqec_seq_num_t seq_num;
    boolean ret;    
    const vqec_pak_t * next_pak;
    const vqec_pak_t * pre_pak;   

    if (!pcm || !pak || !pcm->pak_seq) {
        return FALSE;
    }

    seq_num = vqec_pcm_get_seq_num(pak);

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_PCM_PAK,                           
               "Remove packet seq=%u, srcadd = 0x%x, "
               "src_port = %d, pred time = %llu now = %llu "
               "head is %u, tail is %u\n",
               seq_num,
                   ntohl(pak->src_addr.s_addr),
               ntohs(pak->src_port),
               pak->rcv_ts.usec, 
               remove_time.usec,
               pcm->head,
               pcm->tail);

    if (pak == vqec_pak_seq_find(pcm->pak_seq, seq_num)) {

        /* check / remove packet if it is on the reorder list */
        vqec_pcm_inorder_pak_list_remove(pcm, pak);

        if (pcm->head == seq_num && pcm->tail == seq_num) {
            VQEC_DP_ASSERT_FATAL(vqec_pak_seq_get_num_paks(pcm->pak_seq) == 1,
                                 "pcm");
        } else if (pcm->head == seq_num) {
            
            next_pak = vqec_pcm_find_next_packet_internal(pcm, pak);
            VQEC_DP_ASSERT_FATAL(next_pak, "pcm");

            if (next_pak) {
                pcm->head = vqec_pcm_get_seq_num(next_pak);
            }
        } else if (pcm->tail == seq_num) {
            
            pre_pak = vqec_pcm_find_pre_packet_internal(pcm, pak);
            VQEC_DP_ASSERT_FATAL(pre_pak, "pcm");

            if (pre_pak) {
                pcm->tail = vqec_pcm_get_seq_num(pre_pak);
            }
        }

        if(vqec_pak_seq_delete(pcm->pak_seq, seq_num)) {           
            /* remove from gapmap */
            if (VQE_BITMAP_OK != vqe_bitmap_clear_bit(pcm->gapmap, seq_num)) {
                VQEC_DP_SYSLOG_PRINT(ERROR, "bitmap clear bit failure");
            }
            ret = TRUE;
        } else {
            ret = FALSE;
        }
    } else {
        ret = FALSE;
    }
    
    return ret;
}

/**
 * vqec_pcm_get_next_packet_with_lock 
 * Retrieve a handle to the next packet from pcm.  Note this will
 * increase the pak's ref-count. The caller is responsible for calling
 * vqec_pak_free when finished using the returned packet handle.  
 * @param[in] pcm ptr of pcm
 * @param[in] pak ptr of packet to start 
 * @return ptr of next packet, return NULL if not found.
 */
vqec_pak_t * 
vqec_pcm_get_next_packet (vqec_pcm_t *pcm, const vqec_pak_t *pak) 
{
    if (!pcm) {
        return NULL;
    }

    return vqec_pcm_find_next_packet_internal(pcm, pak);
}

void vqec_pcm_deinit (vqec_pcm_t *pcm) 
{
    VQEC_DP_ASSERT_FATAL(pcm, "pcm");
    
    if (!pcm) {
        return;
    }

    /* deQ all paks from the inorder list (for safety) */
    vqec_pcm_inorder_pak_list_flush(pcm);

    /* reset output block's state */
    vqec_dp_oscheduler_outp_reset(&pcm->osched, TRUE);

    if (pcm->gapmap) {
        vqec_pcm_gapmap_flush(pcm);
        vqe_bitmap_destroy(pcm->gapmap);
    }

    if (pcm->pak_seq) {
        vqec_pak_seq_destroy_in_pool(s_vqec_pak_seq_pool,
                                     pcm->pak_seq);
    }
}

boolean vqec_pcm_flush (vqec_pcm_t * pcm) 
{
    if (!pcm) {
        return FALSE;
    }

    /* flush the gap list */
    vqec_pcm_gapmap_flush(pcm);

    /* deQ all paks from the inorder list (for safety) */
    vqec_pcm_inorder_pak_list_flush(pcm);

    /* reset output block's state */
    vqec_dp_oscheduler_outp_reset(&pcm->osched, TRUE);

    /* flush the sequencer */
    if (pcm->pak_seq) {
        vqec_pak_seq_destroy_in_pool(s_vqec_pak_seq_pool,
                                     pcm->pak_seq);
        pcm->pak_seq = NULL;
    }
    pcm->pak_seq = vqec_pak_seq_create_in_pool(
                        s_vqec_pak_seq_pool,
                        VQEC_PCM_RING_BUFFER_BUCKET_BITS);
    if (!pcm->pak_seq) {
        return FALSE;
    }

    VQEC_DP_ASSERT_FATAL(vqec_pak_seq_get_num_paks(pcm->pak_seq) == 0, "pcm");

    pcm->stats.total_tx_paks = 0;

    pcm->primary_received = FALSE;
    pcm->repair_received = FALSE;

    pcm->delay_from_apps = REL_TIME_0;

    return TRUE;
}


/* Helper Function */
void subtract_tr135_total_stats (vqec_tr135_stream_total_stats_t *a, 
                                 vqec_tr135_stream_total_stats_t *b) 
{
    if (a && b) {
        a->severe_loss_index_count -= b->severe_loss_index_count;
        a->packets_received -= b->packets_received;
        a->packets_expected -= b->packets_expected;
        a->packets_lost -= b->packets_lost;
        a->loss_events -= b->loss_events;
    }
}

void vqec_pcm_get_status (vqec_pcm_t *pcm, vqec_dp_pcm_status_t *s, 
                          boolean cumulative)
{
    static uint64_t vqec_dp_input_shim_status_tr135_overrun_last_val = 0;
    uint64_t tr135_overruns;
    vqec_dp_input_shim_status_t input_shim_status;

    if (!pcm || !s) {
        return;
    }

    /* 
     * vqec_pcm_counter_clear() does not touch pcm head and pcm tail, thus
     * clearing is not supposed to reset these counters, and thus, even with 
     * the snapshot, we do not need to worry about offset etc, because this
     * counter was not touched during the reset
     */
    s->head = pcm->head;
    s->tail = pcm->tail;

    s->er_enable = pcm->er_enable;
    s->highest_er_seq_num = pcm->highest_er_seq_num;
    s->last_requested_er_seq_num = pcm->last_requested_er_seq_num;
    s->last_rx_seq_num = pcm->last_rx_seq_num;
    s->num_paks_in_pak_seq = vqec_pak_seq_get_num_paks(pcm->pak_seq);
    s->primary_received = pcm->primary_received;
    s->repair_received = pcm->repair_received;
    s->repair_trigger_time = pcm->repair_trigger_time;
    s->reorder_delay = pcm->reorder_delay;
    s->fec_delay = pcm->fec_delay;
    s->cfg_delay = pcm->cfg_delay;
    s->default_delay = pcm->default_delay;
    s->gap_hold_time = pcm->gap_hold_time;
    s->outp_last_pak_seq = vqec_dp_oscheduler_last_pak_seq_get(&pcm->osched);
    s->outp_last_pak_ts = vqec_dp_oscheduler_last_pak_ts_get(&pcm->osched);

    s->total_tx_paks = pcm->stats.total_tx_paks;
    s->primary_packet_counter = pcm->stats.primary_packet_counter;
    s->repair_packet_counter = pcm->stats.repair_packet_counter;
    s->duplicate_packet_counter = pcm->stats.duplicate_packet_counter;
    s->input_loss_pak_counter = pcm->stats.input_loss_pak_counter;
    s->input_loss_hole_counter = pcm->stats.input_loss_hole_counter; 
    s->output_loss_pak_counter = pcm->stats.output_loss_pak_counter;
    s->output_loss_hole_counter = pcm->stats.output_loss_hole_counter;
    s->under_run_counter = pcm->stats.under_run_counter;
    s->late_packet_counter = pcm->stats.late_packet_counter;
    s->head_ge_last_seq = pcm->stats.head_ge_last_seq;
    s->pak_seq_insert_fail_counter = pcm->stats.pak_seq_insert_fail_counter;
    s->pcm_insert_drops = pcm->stats.pcm_insert_drops;
    s->seq_bad_range_packet_counter = pcm->stats.seq_bad_range_packet_counter;
    s->bad_rcv_ts = pcm->stats.bad_rcv_ts;
    s->duplicate_repairs = pcm->stats.duplicate_repairs;
    
    /* TR-135 related Stats */

    /* Update Overruns from input shim's output stream */
    vqec_dp_input_shim_get_status(&input_shim_status);
    tr135_overruns = input_shim_status.tr135_overruns -
                     vqec_dp_input_shim_status_tr135_overrun_last_val;
    vqec_dp_input_shim_status_tr135_overrun_last_val = 
        input_shim_status.tr135_overruns;
    pcm->stats.tr135.mainstream_stats.overruns += tr135_overruns;

    memcpy(&s->tr135, &pcm->stats.tr135, sizeof(vqec_tr135_stats_t));

    if (cumulative) {
        s->tr135.mainstream_stats.total_stats_before_ec.minimum_loss_distance =
     pcm->stats.tr135.mainstream_stats.total_stats_before_ec.mld_low_water_mark;
        s->tr135.mainstream_stats.total_stats_before_ec.maximum_loss_period = 
    pcm->stats.tr135.mainstream_stats.total_stats_before_ec.mlp_high_water_mark;
        s->tr135.mainstream_stats.total_stats_after_ec.minimum_loss_distance = 
      pcm->stats.tr135.mainstream_stats.total_stats_after_ec.mld_low_water_mark;
        s->tr135.mainstream_stats.total_stats_after_ec.maximum_loss_period = 
     pcm->stats.tr135.mainstream_stats.total_stats_after_ec.mlp_high_water_mark;
    }

    if (!cumulative) { 
        s->total_tx_paks -= pcm->stats_snapshot.total_tx_paks;
        s->primary_packet_counter -= pcm->stats_snapshot.primary_packet_counter;
        s->repair_packet_counter -= pcm->stats_snapshot.repair_packet_counter;
        s->duplicate_packet_counter -= 
            pcm->stats_snapshot.duplicate_packet_counter;
        s->input_loss_pak_counter -= 
            pcm->stats_snapshot.input_loss_pak_counter; 
        s->input_loss_hole_counter -= 
            pcm->stats_snapshot.input_loss_hole_counter;
        s->output_loss_pak_counter -= 
            pcm->stats_snapshot.output_loss_pak_counter;
        s->output_loss_hole_counter -= 
            pcm->stats_snapshot.output_loss_hole_counter;
        s->under_run_counter -= 
            pcm->stats_snapshot.under_run_counter;
        s->late_packet_counter -= 
            pcm->stats_snapshot.late_packet_counter;
        s->head_ge_last_seq -= 
            pcm->stats_snapshot.head_ge_last_seq;
        s->pak_seq_insert_fail_counter -= 
            pcm->stats_snapshot.pak_seq_insert_fail_counter;
        s->pcm_insert_drops -= pcm->stats_snapshot.pcm_insert_drops;
        s->seq_bad_range_packet_counter -= 
            pcm->stats_snapshot.seq_bad_range_packet_counter; 
        s->bad_rcv_ts -=  pcm->stats_snapshot.bad_rcv_ts;
        s->duplicate_repairs -= pcm->stats_snapshot.duplicate_repairs;

        /* TR-135 related Stats */
        subtract_tr135_total_stats(
            &s->tr135.mainstream_stats.total_stats_before_ec,
            &pcm->stats_snapshot.tr135.mainstream_stats.total_stats_before_ec);

        subtract_tr135_total_stats(
            &s->tr135.mainstream_stats.total_stats_after_ec,
            &pcm->stats_snapshot.tr135.mainstream_stats.total_stats_after_ec);

        s->tr135.mainstream_stats.overruns -=
            pcm->stats_snapshot.tr135.mainstream_stats.overruns;

        s->tr135.mainstream_stats.underruns -=
            pcm->stats_snapshot.tr135.mainstream_stats.underruns;
    }
    return;
}

void vqec_pcm_set_tr135_params (vqec_pcm_t *pcm, 
                                vqec_ifclient_tr135_params_t *params)
{
    if (pcm && params) {
        /* Copy tr135 config to pcm's tr135 config */
        memcpy(&pcm->tr135_params,
               params, sizeof(vqec_ifclient_tr135_params_t));
        /* Update pcm tr135 stats's params */
        memcpy(&pcm->stats.tr135.mainstream_stats.tr135_params,
               params, sizeof(vqec_ifclient_tr135_params_t));
        return;
    }
    return;
}

void vqec_pcm_tr135_reinit (vqec_tr135_stats_t *tr135)
{
    if (tr135) {
        tr135->mainstream_stats.total_stats_before_ec.minimum_loss_distance =
            INFINITY_U64;
        tr135->mainstream_stats.total_stats_after_ec.minimum_loss_distance =
            INFINITY_U64;
        tr135->mainstream_stats.total_stats_after_ec.maximum_loss_period = 0;
        tr135->mainstream_stats.total_stats_before_ec.maximum_loss_period = 0;
    }
}

void vqec_pcm_counter_clear (vqec_pcm_t *pcm) 
{
    if (!pcm) {
        return;
    }
    memcpy(&pcm->stats_snapshot, &pcm->stats, sizeof(vqec_pcm_stats_t));
    /* Reinitialise minimum loss distance and maximum loss period for TR-135 */
    vqec_pcm_tr135_reinit(&pcm->stats.tr135); 
}

/* Helper function to set the er_en_ts for the PCM */
void vqec_pcm_set_er_en_ts (vqec_pcm_t *pcm)
{
    if (!pcm) {
        return;
    }

    if (IS_ABS_TIME_ZERO(pcm->er_en_ts)) {
        pcm->er_en_ts = get_sys_time(); 
    }
}

/**
 * Function to update TR-135 overrun counter
 *
 * @param[in] pcm Pcm instance
 * @param[in] increment Integer expressing increment step
 */
void vqec_pcm_log_tr135_overrun (vqec_pcm_t *pcm, int increment) 
{
    if (!pcm) {
        return;
    }
    pcm->stats.tr135.mainstream_stats.overruns += increment;
}

/**
 * Function to update TR-135 underrrun counter
 *
 * @param[in] pcm Pcm instance
 * @param[in] increment Integer expressing increment step
 */
void vqec_pcm_log_tr135_underrun (vqec_pcm_t *pcm, int increment) 
{
    if (!pcm) {
        return;
    }
    pcm->stats.tr135.mainstream_stats.underruns += increment;
}

/**---------------------------------------------------------------------------
 * Enable ER / under-run detection post RCC unicast burst, or after an
 * abort has taken place.
 *
 * @param[in] pcm Pcm instance.
 *---------------------------------------------------------------------------*/ 
#if HAVE_FCC
void
vqec_pcm_notify_rcc_en_er (vqec_pcm_t *pcm)
{
    if (pcm) {
        pcm->underrun_reset_dis = FALSE;
        pcm->er_enable = pcm->cfg_er_enable;

        if (pcm->er_enable) {
            pcm->last_requested_er_seq_num = pcm->head;
            pcm->highest_er_seq_num = pcm->tail;
            vqec_pcm_set_er_en_ts(pcm);
        }
    }
}

/**---------------------------------------------------------------------------
 * RCC has been aborted. The actions that are taken by pcm are:
 *  -- The packet cache is flushed. 
 *  -- Dynamic jitter buffer is no longer active.
 *  -- The default delay is reset to it's original value.
 *
 * @param[in] pcm Pcm instance.
 *---------------------------------------------------------------------------*/ 
void
vqec_pcm_notify_rcc_abort (vqec_pcm_t *pcm)
{
    if (!vqec_pcm_flush(pcm)) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "pcm flush failure");
    }

    pcm->dyn_jitter_buf_act = FALSE;
    pcm->default_delay = TIME_ADD_R_R(pcm->cfg_delay,
                                      pcm->fec_delay);

    /* clear the APP replication delay in the PCM */
    pcm->delay_from_apps = REL_TIME_0;

    /* notify the oscheduler that the burst is done */
    vqec_dp_oscheduler_rcc_burst_done_notify(&pcm->osched);
}

#endif /* HAVE_FCC */
/**---------------------------------------------------------------------------
 * Update FEC delay. 
 *
 * @param[in] pcm Pcm instance.
 * @param[in] fec_delay FEC delay.
 *---------------------------------------------------------------------------*/ 
void
vqec_pcm_update_fec_delay (vqec_pcm_t *pcm, rel_time_t fec_delay)
{
#define VQEC_PCM_DEBUG_BUF_SIZE 80
    char buf[VQEC_PCM_DEBUG_BUF_SIZE];

    if (!pcm) {
        return;
    }
    /**
     * If the new fec_delay is larger than pcm->fec_delay, we do not update
     * the fec_delay and gap_hold_time. Otherwise, ER may have problem.
     */

    if (TIME_CMP_R(le, fec_delay, pcm->fec_delay)) {
        pcm->fec_delay = fec_delay;
    } else {
        snprintf(buf, sizeof(buf),
                 "new fec_delay (%lld) larger than configured max (%lld)",
                 TIME_GET_R(msec, fec_delay),
                 TIME_GET_R(msec, pcm->fec_delay));
        VQEC_DP_SYSLOG_PRINT(ERROR, buf);
    }
    
    if (!pcm->dyn_jitter_buf_act) {
        /* If dynamic jitter buffer is not in use, update default delay. */ 
        pcm->default_delay = TIME_ADD_R_R(pcm->cfg_delay,
                                          pcm->fec_delay);
    }
    
    /* Always update gap hold-time. */
    vqec_pcm_update_gap_hold_time(pcm,
                                  TIME_ADD_R_R(pcm->fec_delay,
                                               pcm->reorder_delay));
}


/**---------------------------------------------------------------------------
 * Capture a snapshot of PCM state 
 *
 * @param[in] pcm Pcm instance.
 * @param[out] snap PCM snapshot attributes.
 *---------------------------------------------------------------------------*/ 
void
vqec_pcm_cap_state_snapshot (vqec_pcm_t *pcm, vqec_dp_pcm_snapshot_t *snap)
{
    if (pcm && snap) {
        memset(snap, 0, sizeof(*snap));
        
        snap->head = pcm->head;
        snap->tail = pcm->tail;
        snap->num_paks = vqec_pak_seq_get_num_paks(pcm->pak_seq);
        snap->outp_gaps = pcm->stats.output_loss_hole_counter;
        snap->dup_paks = pcm->stats.duplicate_packet_counter;
        snap->late_paks = pcm->stats.late_packet_counter;
    }
}


#if HAVE_FCC
/**---------------------------------------------------------------------------
 * Capture RCC related output data.
 *
 * @param[in] pcm Pcm instance.
 * @param[out] outp RCC related output data.
 *---------------------------------------------------------------------------*/ 
void
vqec_pcm_cap_outp_rcc_data (vqec_pcm_t *pcm, 
                            vqec_dp_pcm_outp_rcc_data_t *outp)
{
    if (pcm && outp) {
        memset(outp, 0, sizeof(*outp));
        outp->first_rcc_pak_ts =
            vqec_dp_oscheduler_outp_log_first_pak_ts(&pcm->osched);
        outp->last_rcc_pak_ts =
            vqec_dp_oscheduler_outp_log_first_prim_pak_ts(&pcm->osched);
        outp->output_loss_paks_in_rcc = pcm->stats.output_loss_paks_first_prim;
        outp->output_loss_holes_in_rcc = pcm->stats.output_loss_holes_first_prim;
        outp->dup_paks_in_rcc = pcm->stats.dup_paks_first_prim;
        outp->first_prim_to_last_rcc_seq_diff = 
            pcm->stats.first_prim_to_last_rcc_seq_diff;
        vqec_log_to_dp_gaplog(&pcm->first_prim_log, &outp->outp_gaps);
    }
}

/**--------------------------------------------------------------------------
 * Get the PCM rcc_post_abort_process flag 
 *
 * @param[in] pcm Pcm instance.
 * return the flag if PCM is valid
 *--------------------------------------------------------------------------*/

boolean vqec_pcm_get_rcc_post_abort_process (vqec_pcm_t *pcm)
{
    if (!pcm) {
        return FALSE;
    }

    return pcm->rcc_post_abort_process;
}


/**--------------------------------------------------------------------------
 * Update the PCM rcc_post_abort_process flag if needed.
 *
 * @param[in] pcm Pcm instance.
 * update the flag if the condition meet.
 *--------------------------------------------------------------------------*/

void vqec_pcm_update_rcc_post_abort_process (vqec_pcm_t *pcm)
{
    if (!pcm) {
        return;
    }

    if (pcm->rcc_post_abort_process && pcm->primary_received) {
        if (pcm->osched.last_pak_seq_valid && 
            vqec_seq_num_le(pcm->first_primary_seq, pcm->osched.last_pak_seq)) {
            pcm->rcc_post_abort_process = FALSE;
        }
    }
}


/**---------------------------------------------------------------------------
 * Identify if we need to post process a repair packet if RCC is aborted. 
 *
 * @param[in] pcm:   pcm pointer. 
 * @param[in] pak:   the pak that will be tested. 
 * 
 * @return: TRUE if the packet will be post processed, otherwise, FALSE.
 * 
 * In some very special cases, for instance, when there is a 
 * large amount of network delay, the incoming repair packets 
 * may take a long time to arrive, but when they do and RCC is 
 * aborted,they'll be wrongly inserted into the PCM and cause 
 * large output gaps.
 * If this test is true for a certain packet, the packet should be
 * freed. 
 *---------------------------------------------------------------------------*/

boolean vqec_pcm_rcc_post_abort_process (vqec_pcm_t *pcm, vqec_pak_t *pak)
{
    if (!pcm || !pak) {
        return FALSE;
    }

    if (pcm->rcc_post_abort_process) {
        if (!pcm->primary_received || 
            vqec_seq_num_le(pak->seq_num, pcm->first_primary_seq)){
            return TRUE;
        }
    }

    return FALSE;
}

#endif   /* HAVE_FCC */
