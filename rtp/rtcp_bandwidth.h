/**-----------------------------------------------------------------
 * @brief
 * Declarations/definitions for RTCP bandwidth/interval data/functions.
 *
 * @file
 * rtcp_bandwidth.h
 *
 * March 2007, Mike Lague.
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __RTCP_BANDWIDTH_H__
#define __RTCP_BANDWIDTH_H__

#include "rtp_types.h"

typedef uint32_t rtcp_bw_t;      /* bandwidth in bits/sec */
typedef uint32_t rtcp_bw_kbps_t; /* bandwidth in kbits/sec */
typedef uint32_t rtcp_bw_bi_t;   /* a 16:16 fixed point value, in kbps:
                                    used by the RTCP Bandwidth Indication
                                    sub-report */

#define RTCP_BW_UNSPECIFIED  0xffffffff  /* used (instead of zero)
                                            to indicate an unspecifed value
                                            for rtcp_bw_t or rtcp_bw_kbps_t */
#define RTCP_MAX_BW          0xfffffffe  /* since we've reserved the highest
                                            32-bit unsigned value to
                                            to indicate "unspecified" */
#define D_RTCP_MAX_BW        ((double)(RTCP_MAX_BW))

/*
 * rtcp_bw_bi_to_dbps
 *
 * Convert a 16:16 fixed point value, in kbps,
 * to a double precision value, in bps.
 */

static inline double
rtcp_bw_bi_to_dbps (rtcp_bw_bi_t bi_val) 
{
    return ((double)(bi_val) / 65.536);
}

/* 
 * rtcp_bw_dbps_to_bi
 *
 * Convert a double precision value in bps
 * to a 16:16 fixed point value, in kbps.
 */

static inline rtcp_bw_bi_t
rtcp_bw_dbps_to_bi (double dbps_val)
{
    return ((rtcp_bw_bi_t)(dbps_val * 65.536));
}

/* 
 * default RTCP bandwidth values
 * expressed as a fraction of session bandwidth.
 */
#define D_RTCP_DFLT_BW_PCT       .05

/*
 * default share of RTCP bandwidth for receivers/senders
 * expressed as fractions of total RTCP bandwidth
 */

#define D_RTCP_DFLT_RCVR_BW_PCT  .75
#define D_RTCP_DFLT_SNDR_BW_PCT  .25

/*
 * rtcp_bw_cfg_t 
 *
 * Defines the quantities that may be configured to specify RTCP bandwidth. 
 * If an element was not configured, the reserved value RTCP_BW_UNSPECIFIED 
 * should be the value of the element, not zero, as zero may be explicitly
 * configured for most elements.
 */

typedef struct rtcp_bw_cfg_t_ {
    /*
     * The SDP source for the indicated elements is shown.
     * Note that per-rcvr-bw and per-sndr-bw are media level only
     */
    rtcp_bw_t      per_rcvr_bw;  /* a=fmtp:<pt> per-rcvr-bw <value> */
    rtcp_bw_t      per_sndr_bw;  /* a=fmtp:<pt> per-sndr-bw <value> */
    rtcp_bw_t      media_rs_bw;  /* media level: b=RS:<value> */
    rtcp_bw_t      sess_rs_bw;   /* session level: b=RS:<value> */
    rtcp_bw_t      media_rr_bw;  /* media level: b=RR:<value> */
    rtcp_bw_t      sess_rr_bw;   /* session level: b=RR:<value> */
    rtcp_bw_kbps_t media_as_bw;  /* media level: b=AS:<value> */
    rtcp_bw_kbps_t sess_as_bw;   /* session level: b=AS:<value> */
} rtcp_bw_cfg_t;

#define RTCP_DFLT_PER_RCVR_BW  160  /* in bps: enough to send a 100-byte
                                       report every 5 seconds (min intvl) */
#define RTCP_DFLT_PER_SNDR_BW  160

#define RTCP_DFLT_AS_BW  5500       /* in kbps */

/*
 * rtcp_init_bw_cfg
 *
 * Initialize an rtcp_bw_cfg_t structure.
 */

static inline void 
rtcp_init_bw_cfg (rtcp_bw_cfg_t *cfg)
{
    cfg->per_rcvr_bw = RTCP_BW_UNSPECIFIED;
    cfg->per_sndr_bw = RTCP_BW_UNSPECIFIED;
    cfg->media_rs_bw = RTCP_BW_UNSPECIFIED;
    cfg->sess_rs_bw = RTCP_BW_UNSPECIFIED;
    cfg->media_rr_bw = RTCP_BW_UNSPECIFIED;
    cfg->sess_rr_bw = RTCP_BW_UNSPECIFIED;
    cfg->media_as_bw = RTCP_BW_UNSPECIFIED;
    cfg->sess_as_bw = RTCP_BW_UNSPECIFIED;
}

/*
 * rtcp_bw_role_t
 *
 * Defines the bandwidth quantities that may be used by
 * either an RTP "sender" or "receiver" to determine its
 * share of RTCP bandwidth.
 */

typedef struct rtcp_bw_role_t_ {
    rtcp_bw_t cfg_per_member_bw;  /* configured per-member bandwidth, via
                                     "a=fmtp:<pt> rtcp-per-rcvr-bw=<value>" or
                                     "a=fmtp:<pt> rtcp-per-sndr-bw=<value>" */
    rtcp_bw_bi_t rpt_per_member_bw; /* reported per-member bandwidth, i.e.,
                                       received via the RTCP Bandwidth 
                                       Indication subreport in the RSI msg */
    rtcp_bw_t tot_role_bw;        /* bandwidth for all senders or receivers,
                                     configured via "b=RS:<value>" or
                                     "b=RR:<value>", or derived from 
                                     "b=AS:<value>" */
} rtcp_bw_role_t ;

/*
 * rtcp_init_bw_role
 *
 * Initialize an rtcp_bw_role_t structure
 */

static inline void 
rtcp_init_bw_role (rtcp_bw_role_t *role)
{
    role->cfg_per_member_bw = RTCP_BW_UNSPECIFIED;
    role->rpt_per_member_bw = RTCP_BW_UNSPECIFIED;
    role->tot_role_bw = RTCP_BW_UNSPECIFIED;
}

/*
 * rtcp_bw_info_t 
 *
 * RTCP bandwidth information.
 * Derived from configuration, or the RSI RTCP Bandwidth Indication.
 */

typedef struct rtcp_bw_info_t_ {
    rtcp_bw_role_t rcvr;   /* bandwidth info for RTP receivers */
    rtcp_bw_role_t sndr;   /* bandwidth info for RTP senders */
} rtcp_bw_info_t ;

/*
 * rtcp_init_bw_info
 *
 * Initialize an rtcp_bw_info_t structure.
 */

static inline void 
rtcp_init_bw_info (rtcp_bw_info_t *info)
{
    rtcp_init_bw_role(&info->rcvr);
    rtcp_init_bw_role(&info->sndr);
}

/*
 * default average packet size, in bytes
 */

#define D_RTCP_DFLT_AVG_PKT_SIZE  100.

/*
 * rtcp_intvl_calc_sess_info
 *
 * Per-session information for RTCP interval calculation
 */

typedef struct rtcp_intvl_calc_info_t_ {
    boolean we_sent;  /* true if this member is a sender */
    boolean initial;  /* true if this member has not yet sent an RTCP packet */
    int members;  /* number of members in the session */
    int senders;  /* number of senders in the session */
    double rtcp_avg_size; /* avg size (in bytes) of all RTCP compound pkts */
    double rtcp_avg_size_sent;  /* avg size of all RTCP sent by this member */
} rtcp_intvl_calc_sess_info_t;

/*
 * rtcp_intvl_calc_params
 *
 * Parameters for RTCP interval calculation
 */

typedef struct rtcp_intvl_calc_params_t_ {
    int members;  /* number of members in the session */
    int senders;  /* number of senders in the session */
    double rtcp_bw; /* total RTCP bandwidth (in bytes/sec) */
    boolean we_sent;  /* true if this member is a sender */
    double rtcp_sender_bw_fraction; /* fraction dedicated to senders */
    double rtcp_avg_size; /* avg size (in bytes) of all RTCP compound pkts */
    boolean initial;  /* true if this member has not yet sent an RTCP packet */
} rtcp_intvl_calc_params_t;

/*
 * rtcp_jitter_data
 *
 * Storage for the randomization function used by rtcp_jitter_interval
 */

typedef unsigned int rtcp_jitter_data_t;


/*
 * Function declarations
 */

extern void rtcp_set_bw_info(rtcp_bw_cfg_t *cfg,
                             rtcp_bw_info_t *info);

extern void rtcp_get_intvl_calc_params(rtcp_bw_info_t *bw_info,
                                       rtcp_intvl_calc_sess_info_t *sess_info,
                                       rtcp_intvl_calc_params_t *params);

extern double rtcp_td_interval(int members,
                               int senders,
                               double rtcp_bw,
                               double rtcp_sender_bw_fraction,
                               boolean we_sent,
                               double rtcp_avg_size,
                               boolean initial,
                               double *act_rtcp_bw,
                               int *act_members);

extern void rtcp_jitter_init(rtcp_jitter_data_t *jdata);

extern double rtcp_jitter_interval(double td_interval, 
                                   rtcp_jitter_data_t *jdata);

#endif /* __RTCP_BANDWIDTH_H__ */

