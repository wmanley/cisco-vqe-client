/* $Id$
 * $Source$
 *----------------------------------------------------------------------
 * rtp_header.h - Definitions and declaration for RTP header processing.
 * 
 * Copyright (c) 2006-2011 by cisco Systems, Inc.
 * All rights reserved.
 *----------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

/*
 * Prevent multiple inclusion.
 */
#ifndef __rtp_header_h__
#define __rtp_header_h__

#include "rtp.h"
#include "../include/utils/vam_time.h"
#include "rtcp_xr.h"

/*
 * RTP header status values. 
 *
 * Note that the following status values should trigger
 * notification of RTCP:
 *
 *  RTP_HDR_SEQSTART      -- sequence no. processing has started 
 *                             for this source: notify RTCP via
 *                             direct or indirect invocation of the
 *                             rtp_new_data_source method.
 */
typedef enum {

    RTP_HDR_OK,           /* acceptable sequence no., no other errors */
    /*
     * sequence number notifications:
     * packets with these status values should be processed,
     * but RTCP must be notified of the (re-)intialization 
     * of sequence numbers.
     */
    RTP_HDR_SEQSTART,     /* sequence no. processing has started 
                             for this source */
    /*
     * sequence number errors:
     * packets with these errors should NOT be processed.
     */
    RTP_HDR_SEQJUMP,      /* either a large jump in sequence number, 
                             or not in sequence after a large jump */
    RTP_HDR_SEQDUP,       /* the sequence no. is a duplicate of 
                             the one in the previous packet */
    /*
     * other notifications:
     */
    RTP_HDR_EXTENSION,    /* an extension hdr is present */
    RTP_HDR_SEQRESTART,   /* sequence no. processing has restarted 
                             for this source (after a previous large jump 
                             in sequence no. */
    /*
     * other errors:
     * packets with these errors should NOT be processed.
     */
    RTP_HDR_RUNT,         /* too short to be an RTP packet */
    RTP_HDR_BADVER,       /* unexpected version number */
    RTP_HDR_BADLEN        /* length inconsistent with hdr values */

} rtp_hdr_status_t;

/*
 * RTP events
 *
 * Events in sequence number processing that need to 
 * be communicated to RTCP, if split RTP/RTCP processing 
 * is in use.
 */

typedef enum {
    RTP_EVENT_NONE,
    RTP_EVENT_SEQSTART     /* sequence number base established 
                              for this source */
} rtp_event_t;

#pragma pack(4)

/*
 * Per-source information
 *
 * A source (for a given session) is identified by a 3-tuple:
 * (SSRC, source address, source port)
 */

typedef struct rtp_hdr_source_t_ {
    /*
     * these statistics are required for RTCP reports
     */
    uint16_t                 max_seq;   /* cur seq */
    uint32_t                 cycles;    /* cycles in seq no */
    uint32_t                 bad_seq;   /* last bad_seq + 1 */
    uint32_t                 base_seq;  /* base sequence */
    uint32_t                 transit;   /* rela. trans time for prev pk */
    uint32_t                 jitter;    /* estimated jitter, scaled up */
    uint32_t                 received;  /* rcvd packet count */              

    uint32_t                 received_prior;  /* "received" value, at last
                                                 RTCP report interval */
    uint32_t                 received_penult; /* "received" value, at 
                                                 penultimate (next-to-last)
                                                 RTCP interval */
    uint32_t                 expected_prior;  /* total pkts expected, at
                                                 last RTCP report interval */
    abs_time_t               last_arr_ts_abs;  /* arrival timestamp
                                                  of last pkt rcvd */
    uint32_t                 last_arr_ts_media; /* arrival timestamp,
                                                   in media (RTP) units */
    /*
     * other statistics:
     */
    uint32_t                 seqjumps;
    uint32_t                 initseq_count;

    uint32_t                 out_of_order; /* out of order paks */
    uint32_t                 dups;      /* duplicates: not all, but
                                           only instances of two packets 
                                           in a row with the same seq. no. */
    uint32_t                 lost;      /* packets lost */

} rtp_hdr_source_t;

/*
 * Per-session statistics
 */

typedef struct rtp_hdr_session_t_ {
    uint32_t runts;
    uint32_t badver;
    uint32_t badlen;
    uint32_t seqjumps;
    uint32_t initseq_count;
} rtp_hdr_session_t;

/*
 * RTCP XR Post Error Repair Stats
 */
typedef struct rtcp_xr_post_rpr_stats_t_ {
    rtp_hdr_session_t sess;
    rtp_hdr_source_t source;
    rtcp_xr_stats_t xr_stats;
} rtcp_xr_post_rpr_stats_t;

#pragma pack()

/*
 * function declarations
 */

extern rtp_hdr_status_t rtp_validate_hdr(rtp_hdr_session_t *session,
                                         char *buf, 
                                         uint16_t len);
extern rtp_hdr_status_t rtp_update_seq(rtp_hdr_session_t *session,
                                       rtp_hdr_source_t *source,
                                       rtcp_xr_stats_t *xr_stats,
                                       uint16_t seq);

extern boolean rtp_hdr_ok(rtp_hdr_status_t error);
extern rtp_event_t rtp_hdr_event(rtp_hdr_status_t error);

uint32_t rtp_update_timestamps(rtp_hdr_source_t *source,
                               abs_time_t arrival_ts);
extern void rtp_update_jitter(rtp_hdr_source_t *source, 
                              rtcp_xr_stats_t *xr_stats,
                              uint32_t pak_ts, 
                              uint32_t arrival_ts);
extern uint32_t rtp_get_jitter(rtp_hdr_source_t *source);

extern void rtp_update_prior(rtp_hdr_source_t *source);

#endif /* to prevent multiple inclusion */
