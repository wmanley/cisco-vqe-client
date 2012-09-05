/**-----------------------------------------------------------------
 * @brief
 * Declarations/definitions for RTCP XR data/functions.
 *
 * @file
 * rtcp_xr.h
 *
 * March 2008, Dong Hsu.
 *
 * Copyright (c) 2008-2011 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __RTCP_XR_H__
#define __RTCP_XR_H__

#include "rtp_types.h"
#include "rtcp.h"

#define RTCP_XR_LOSS_RLE_UNSPECIFIED 0xffff
#define MAX_RLE_SIZE 0xfffe

/*
 * RTCP XR statistics
 */
#define MAX_CHUNKS 700
#define MAX_BIT_IDX 15
#define MAX_RUN_LENGTH 16380
#define MAX_SEQ_ALLOWED 65534

#define INITIAL_BIT_VECTOR 0x8000
#define ALL_ONE_VECTOR 0xffff
#define RUN_LENGTH_FOR_ONE 0x4000
#define RUN_LENGTH_MASK 0x3fff
#define RUN_LENGTH_FOR_ONE_MASK 0xC000

#pragma pack(4)

typedef struct rtcp_xr_stats_t_ {
    uint32_t eseq_start;    /* extended starting seq. number */
    uint16_t totals;        /* total numbers of seq. in the interval */

    uint32_t not_reported;  /* counters after exceeding the limits */
    uint32_t before_intvl;  /* counters before eseq_start */
    uint32_t re_init;       /* counters before re-initialized */

    boolean  exceed_limit;  /* indicator of exceeding the limits */

    uint32_t lost_packets;  /* no. of lost packets during this interval */
    uint32_t dup_packets;   /* no. of duplicate packets during this interval */
    uint32_t late_arrivals; /* no. of late arrivals during this interval */

    uint32_t min_jitter;    /* min jitter during this interval */
    uint32_t max_jitter;    /* max jitter during this interval */
    uint64_t cum_jitter;    /* accumulated jitter during this interval */
    uint64_t sqd_jitter;    /* squared jitter during this interval */
    uint32_t num_jitters;   /* total number of jitters accumulated */

    uint32_t next_exp_eseq; /* next expected extended sequence number */   
    uint16_t chunk[MAX_CHUNKS+2]; /* make it 2 chunks bigger for
                                     efficient moving */
    uint16_t cur_chunk_in_use; /* current chunk in use */
    uint16_t max_chunks_allow; /* maximum number of chunks allowed to use */
    uint8_t  bit_idx;          /* index for last bit being used in current
                                  chunk */
} rtcp_xr_stats_t;

/*
 * RTCP XR Media Acquisition (MA) report 
 */
typedef struct rtcp_xr_ma_stats_t_ {
    /* Raw time values, must be collected from VQE-C DP */
    abs_time_t first_repair_ts;  /* Time of first repair pkt */
    abs_time_t join_issue_time;  /* Time join is issued */
    abs_time_t first_primary_ts; /* Time of first primary packet */
    abs_time_t burst_end_time;   /* Time of burst end */
    int32_t first_prim_to_last_rcc_seq_diff; /* Diff between last repair
                                              * and first primary packets */

    /* Computed values */
    boolean  to_be_sent;        /* True when XR report containing this data
                                 * is ready to be sent */
    uint8_t  status;            /* Status (Type Specific) */
    uint32_t xr_ssrc;           /* SSRC of sender (only used on decode) */
    uint32_t first_mcast_ext_seq;/* Extended RTP Seq # of 1st mcast pkt*/
    uint32_t rtcp_req_to_burst; /* Request to Burst Start Delta Time */
    uint32_t rtcp_req_to_burst_end; /* Request to Burst End Delta Time */
    uint32_t sfgmp_join_time;   /* SFGMP (IGMP) Join Time */
    uint32_t app_req_to_mcast; /* App Request To First Multicast Delta Time */
    uint32_t app_req_to_pres;   /* App Request To Presentation Delta Time */
    uint32_t app_req_to_rtcp_req;/* Application Request to RTCP Request */
    uint32_t num_dup_pkts;      /* Number of Duplicate Packets */
    uint32_t num_gap_pkts;      /* Number of pkts in burst->mcast gap*/

    /* Vendor-Specific extensions */
    uint32_t total_cc_time;     /* Total channel change time */
    uint64_t rcc_expected_pts;  /* PTS of Expected First I-Frame */
    uint64_t rcc_actual_pts;    /* PTS of Actual First I-Frame */

} rtcp_xr_ma_stats_t;

#define RTCP_XR_DC_VERSION       (1)
#define RTCP_XR_DC_VERSION_SHIFT (4)
#define RTCP_XR_DC_FLAGS         (0)

typedef enum rtcp_xr_dc_types_ {
    XR_DC_UNDERRUNS = 0,
    XR_DC_OVERRUNS,
    XR_DC_POST_REPAIR_LOSSES,
    XR_DC_PRIMARY_RTP_DROPS_LATE,
    XR_DC_REPAIR_RTP_DROPS_LATE,
    XR_DC_OUTPUT_QUEUE_DROPS,
    MAX_RTCP_XR_DC_TYPES
} rtcp_xr_dc_types_t;

/* 
 * RTCP XR Diagnostic Counter (DC) report
 */
typedef struct rtcp_xr_dc_stats_t_ {
    uint8_t is_updated;
    uint32_t xr_ssrc;           /* SSRC of sender (only used on decode) */
    /* All counters are 64-bit */
    uint64_t type[MAX_RTCP_XR_DC_TYPES];
} rtcp_xr_dc_stats_t;

/*
 * rtcp_xr_cfg_t 
 *

 * Defines the quantities that may be configured to specify RTCP XR reports.
 *
 */

typedef struct rtcp_xr_cfg_t_ {
    /*
     * The SDP source for the indicated elements is shown.
     */
    uint16_t max_loss_rle;           /* a=rtcp-xr:pkt-loss-rle=<value> */
    uint16_t max_post_rpr_loss_rle;  /* a=rtcp-xr:post-repair-loss-rle=<value> */
    /* Note: the values in max_loss_rle and max_post_rpr_loss_rle are
       the maximum bytes used for RLE chunks. */

    boolean  ss_loss;           /* a=rtcp_xr:stat-summary=loss */
    boolean  ss_dup;            /* a=rtcp_xr:stat-summary=dup */
    boolean  ss_jitt;           /* a=rtcp_xr:stat-summary=jitt */
    boolean  ma_xr;             /* a=rtcp_xr:multicast-acq */
    boolean  xr_dc;             /* a=rtcp_xr:vqe-diagnostic-counters */
} rtcp_xr_cfg_t;

#pragma pack()

/*
 * rtcp_init_xr_cfg
 *
 * Initialize an rtcp_xr_cfg_t structure.
 */

static inline void 
rtcp_init_xr_cfg (rtcp_xr_cfg_t *cfg)
{
    cfg->max_loss_rle = 0;
    cfg->max_post_rpr_loss_rle = 0;
    cfg->ss_loss = FALSE;
    cfg->ss_dup = FALSE;
    cfg->ss_jitt = FALSE;
    cfg->ma_xr = FALSE;
    cfg->xr_dc = FALSE;
}

/*
 * rtcp_xr_get_max_size
 *
 * Get the max size for rtcp xr stats collection.
 */
static inline uint16_t 
rtcp_xr_get_max_size (rtcp_xr_cfg_t *cfg)
{
    return cfg->max_loss_rle;
}

/*
 * rtcp_xr_get_max_size_post_rpr_loss_rle
 *
 * Get the max size used for post ER Loss RLE in RTCP XR.
 */

static inline uint16_t 
rtcp_xr_get_max_size_post_rpr_loss_rle (rtcp_xr_cfg_t *cfg)
{
    return cfg->max_post_rpr_loss_rle;
}

/*
 * is_rtcp_xr_enabled
 *
 * Check whether the rtcp_xr is enabled or not
 */
static inline boolean
is_rtcp_xr_enabled (rtcp_xr_cfg_t *cfg)
{
    if (cfg->max_loss_rle > 1 || cfg->ss_loss || cfg->ss_dup || cfg->ss_jitt) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/*
 * is_rtcp_xr_dc_enabled
 * check if rtcp_xr_dc_stats_t carries data of a particular type
 */
static inline
boolean is_rtcp_xr_dc_enabled(rtcp_xr_cfg_t *cfg)
{
    return (cfg->xr_dc);
}

/*
 * is_post_er_loss_rle_enabled
 *
 * Check whether post ER Loss RLE is enabled or not
 */
static inline boolean
is_post_er_loss_rle_enabled (rtcp_xr_cfg_t *cfg)
{
    if (cfg->max_post_rpr_loss_rle > 1) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/*
 * is_rtcp_xr_ma_enabled
 *
 * Check whether the Rtcp XR MA report is enabled or not
 */
static inline boolean 
is_rtcp_xr_ma_enabled (rtcp_xr_cfg_t *cfg) 
{
    return cfg->ma_xr;
}

extern void rtcp_xr_set_size(rtcp_xr_stats_t *xr_stats,
                             uint16_t max_size);

extern void rtcp_xr_init_seq(rtcp_xr_stats_t *xr_stats,
                             uint32_t seq,
                             boolean re_init_mode);

extern void rtcp_xr_update_seq(rtcp_xr_stats_t *xr_stats,
                               uint32_t eseq);

extern void rtcp_xr_update_jitter(rtcp_xr_stats_t *xr_stats,
                                  uint32_t jitter);

extern uint32_t rtcp_xr_get_mean_jitter(rtcp_xr_stats_t *xr_stats);

extern uint32_t rtcp_xr_get_std_dev_jitter(rtcp_xr_stats_t *xr_stats);

extern uint16_t rtcp_xr_encode_ma(rtcp_xr_ma_stats_t *xr_ma,
                                  uint32_t xr_ssrc,
                                  uint8_t *p_xr,
                                  uint16_t data_len);

extern boolean rtcp_xr_decode_ma(uint8_t *p_data, uint16_t p_len, 
                                 rtcp_xr_ma_stats_t *p_xr_ma);

extern uint16_t rtcp_xr_encode_dc(rtcp_xr_dc_stats_t *xr_dc,
                                  uint32_t xr_ssrc,
                                  uint8_t *p_xr,
                                  uint16_t data_len);

extern boolean rtcp_xr_decode_dc(uint8_t *p_data, uint16_t p_len, 
                                 rtcp_xr_dc_stats_t *p_xr_dc);

#endif /* __RTCP_XR_H__ */

