/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "vqec_log.h"

void vqec_log_init (vqec_log_t *log) 
{
    memset(&log->last_n_repair_seq, 0, sizeof(vqec_log_seq_t));
    memset(&log->last_n_fec_repair_seq, 0, sizeof(vqec_log_seq_t));
    memset(&log->first_n_primary_seq, 0, sizeof(vqec_log_seq_t));
    memset(&log->output_gaps, 0, sizeof(vqec_log_gap_t));
    memset(&log->input_gaps, 0, sizeof(vqec_log_gap_t));
    log->proxy.join_time.usec = 0;
    log->vam.desired_join_time.usec = 0;
    log->vam.actual_join_time.usec = 0;
}

void vqec_log_clear (vqec_log_t *log) 
{
    vqec_log_init(log);
}

/*
 * We want keep the last repair seq
 */
void vqec_log_insert_repair_seq (vqec_log_t *log,
                                 vqec_seq_num_t seq) 
{
    int i = log->last_n_repair_seq.cur_pos;
    log->last_n_repair_seq.seq_num[ i ] = seq;
    log->last_n_repair_seq.cur_pos = (i + 1) % VQEC_LOG_SIZE;
}

/**
 * added for use by FEC
 * need to keep the last fec repaired packets
 */

void vqec_log_insert_fec_repair_seq (vqec_log_t *log,
                                     vqec_seq_num_t seq) 
{
    int i = log->last_n_fec_repair_seq.cur_pos;
    log->last_n_fec_repair_seq.seq_num[ i ] = seq;
    log->last_n_fec_repair_seq.cur_pos = (i + 1) % VQEC_LOG_SIZE;
}

/*
 * We just need the first primary seq.
 */
void vqec_log_insert_primary_seq (vqec_log_t *log,
                                  vqec_seq_num_t seq) 
{
    int i = log->first_n_primary_seq.cur_pos;
    if (i == VQEC_LOG_SIZE) {
        return;
    }
    log->first_n_primary_seq.seq_num[ i ] = seq;
    log->first_n_primary_seq.cur_pos = i + 1;
}

void vqec_log_insert_output_gap_direct (vqec_log_gaps_t *output_gaps,
                                        vqec_seq_num_t start,
                                        vqec_seq_num_t end,
                                        abs_time_t time) 
{
    int i = output_gaps->cur_pos;
    if (i == VQEC_LOG_SIZE) {
        return;
    }
    output_gaps->seq_num[ i ].start = start;
    output_gaps->seq_num[ i ].end = end;
    output_gaps->seq_num[ i ].time = time;
    output_gaps->cur_pos = (i + 1) % VQEC_LOG_SIZE;
}

void vqec_log_insert_output_gap (vqec_log_t *log,
                                 vqec_seq_num_t start,
                                 vqec_seq_num_t end,
                                 abs_time_t time) 
{
    vqec_log_insert_output_gap_direct(&log->output_gaps,
                                      start,
                                      end,
                                      time);
}

void vqec_log_insert_input_gap (vqec_log_t *log,
                                vqec_seq_num_t start,
                                vqec_seq_num_t end) 
{
    int i = log->input_gaps.cur_pos;
    if (i >= VQEC_LOG_SIZE) {
        return;
    }
    log->input_gaps.seq_num[ i ].start = start;
    log->input_gaps.seq_num[ i ].end = end;
    log->input_gaps.cur_pos = (i + 1) % VQEC_LOG_SIZE;
}

void vqec_log_proxy_join_time (vqec_log_t *log,
                               abs_time_t time) 
{
    ASSERT(log, "null log pointer");
    if (IS_ABS_TIME_ZERO(log->proxy.join_time)) {
        log->proxy.join_time = time;
    }
}

void vqec_log_vam_desired_join_time (vqec_log_t *log,
                                     abs_time_t time) 
{
    ASSERT(log, "null log pointer");

    if (IS_ABS_TIME_ZERO(log->vam.desired_join_time)) {
        log->vam.desired_join_time = time;
    }
}

void vqec_log_vam_actual_join_time (vqec_log_t * log,
                                    abs_time_t time) 
{
    ASSERT(log, "null log pointer");

    if (IS_ABS_TIME_ZERO(log->vam.actual_join_time)) {
        log->vam.actual_join_time = time;
    }
}


/**---------------------------------------------------------------------------
 * Copy the internal log to it's cp-dp ipc counterpart.
 *---------------------------------------------------------------------------*/ 
void 
vqec_log_to_dp_gaplog (vqec_log_gaps_t *gap_log, 
                       vqec_dp_gaplog_t *dp_gap_log)
{
    uint32_t i, j, gap_entries;

    if (!gap_log || !dp_gap_log) {
        return;
    }

    memset(dp_gap_log, 0, sizeof(*dp_gap_log));

    i = gap_log->cur_pos;
    if (i == VQEC_LOG_SIZE) {
        return;
    }

    gap_entries = VQEC_DP_GAPLOG_ARRAY_SIZE;
    if (VQEC_DP_GAPLOG_ARRAY_SIZE > VQEC_LOG_SIZE) {
        gap_entries = VQEC_LOG_SIZE;
    }

    for (j = 0; j < gap_entries; j++) {
        dp_gap_log->gaps[j].start_seq = gap_log->seq_num[i].start;
        dp_gap_log->gaps[j].end_seq = gap_log->seq_num[i].end;
        dp_gap_log->gaps[j].time = gap_log->seq_num[i].time;
        
        i = (i + 1) % VQEC_LOG_SIZE;
    }
    dp_gap_log->num_entries = gap_entries;
}


/**---------------------------------------------------------------------------
 * Copy the primary, repair and FEC repair sequence logs to the
 * cp-dp ipc structure.
 *---------------------------------------------------------------------------*/ 
void 
vqec_log_to_dp_seqlog (vqec_log_t *log, 
                       vqec_dp_seqlog_t *dp_seq_log)
{
    uint32_t seq_entries;

    if (!log || !dp_seq_log) {
        return;
    }

    memset(dp_seq_log, 0, sizeof(*dp_seq_log));

    seq_entries = VQEC_DP_SEQLOG_ARRAY_SIZE;
    if (VQEC_DP_SEQLOG_ARRAY_SIZE > VQEC_LOG_SIZE) {
        seq_entries = VQEC_LOG_SIZE;
    }

    memcpy(dp_seq_log->prim, 
           log->first_n_primary_seq.seq_num,
           sizeof(dp_seq_log->prim[0]) * seq_entries);
    memcpy(dp_seq_log->repair, 
           log->last_n_repair_seq.seq_num,
           sizeof(dp_seq_log->repair[0]) * seq_entries);
    memcpy(dp_seq_log->fec, 
           log->last_n_fec_repair_seq.seq_num,
           sizeof(dp_seq_log->fec[0]) * seq_entries);
}
