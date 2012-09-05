/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef VQEC_LOG_H
#define VQEC_LOG_H

#include "vqec_dp_common.h"
#include "vqec_dp_api.h"
#include "vqec_pak.h"

#define VQEC_LOG_SIZE 10
#define VQEC_MAX_LOG_STRLEN 500
#define VQEC_LOG_CLI_PRINT_BUF_SIZE 50

typedef struct vqec_log_seq_ {
    vqec_seq_num_t seq_num[ VQEC_LOG_SIZE ];
    int cur_pos;
} vqec_log_seq_t;

typedef struct vqec_log_gap_ {
    vqec_seq_num_t start;
    vqec_seq_num_t end;
    abs_time_t time;
} vqec_log_gap_t;

typedef struct vqec_log_gaps_ {
    vqec_log_gap_t seq_num[ VQEC_LOG_SIZE ];
    int cur_pos;
} vqec_log_gaps_t;

typedef struct vqec_log_ {
    vqec_log_seq_t last_n_repair_seq;
    vqec_log_seq_t last_n_fec_repair_seq;
    vqec_log_seq_t first_n_primary_seq;
    vqec_log_gaps_t output_gaps;
    vqec_log_gaps_t input_gaps;
    struct {
        abs_time_t join_time;
    } proxy;
    struct {
        abs_time_t actual_join_time;
        abs_time_t desired_join_time;
    } vam;
} vqec_log_t;

/* log api */
void vqec_log_init(vqec_log_t *log);
void vqec_log_clear(vqec_log_t *log);
void vqec_log_dump(vqec_log_t *log, boolean brief_flag);
void vqec_log_insert_repair_seq(vqec_log_t *log,
                                vqec_seq_num_t seq);
void vqec_log_insert_fec_repair_seq(vqec_log_t *log,
                                    vqec_seq_num_t seq);
void vqec_log_insert_primary_seq(vqec_log_t *log,
                                 vqec_seq_num_t seq);
void vqec_log_insert_output_gap_direct(vqec_log_gaps_t *log_gaps,
                                       vqec_seq_num_t start,
                                       vqec_seq_num_t end,
                                       abs_time_t time);
void vqec_log_insert_output_gap(vqec_log_t *log,
                                vqec_seq_num_t start,
                                vqec_seq_num_t end,
                                abs_time_t time);
void vqec_log_insert_input_gap(vqec_log_t *log,
                               vqec_seq_num_t start,
                               vqec_seq_num_t end);
void vqec_log_proxy_leave_time(vqec_log_t *log,
                               abs_time_t time);
void vqec_log_proxy_join_time(vqec_log_t *log,
                              abs_time_t time);
void vqec_log_vam_desired_join_time(vqec_log_t *log,
                                    abs_time_t time);
void vqec_log_vam_actual_join_time(vqec_log_t *log,
                                   abs_time_t time);
void vqec_log_to_dp_gaplog(vqec_log_gaps_t *gap_log, 
                           vqec_dp_gaplog_t *dp_gap_log);
void 
vqec_log_to_dp_seqlog(vqec_log_t *log, 
                      vqec_dp_seqlog_t *dp_seq_log);

#endif /* VQEC_LOG_H */
