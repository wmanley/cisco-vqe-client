/* $Id$
 * $Source$
 *------------------------------------------------------------------------------
 * rtp_header.c - Functions for sequence number and other RTP header processing.
 * 
 * November 2006.
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#include "rtp_header.h"

/* RTP Header Functions */

/*
 * rtp_validate_hdr
 *
 * Performs basic checks on the RTP header.
 *
 * Parameters:  buf      pointer to start of RTP header in the packet
 *              len      length of RTP packet (including RTP header)
 * Returns:     RTP_HDR_OK        -- no errors
 *              RTP_HDR_RUNT      -- too short to be a valid RTP pkt
 *              RTP_HDR_BADVER    -- unexpected RTP version
 *              RTP_HDR_BADLEN    -- actual length inconsistent w/ hdr values
 *              RTP_HDR_EXTENSION -- the extension bit is set in the hdr
 */
rtp_hdr_status_t rtp_validate_hdr (rtp_hdr_session_t *session,
                                   char *buf, 
                                   uint16_t len)
{
    rtpfasttype_t *rtp = (rtpfasttype_t *)buf;
    uint32_t     minlen;

    /*
     * perform sanity check of RTP length
     */
    minlen = MINRTPHEADERBYTES;
    if (len < minlen) {
        session->runts++;
        return(RTP_HDR_RUNT);
    }

    /*
     * Report other unexpected hdr values
     */
    if (RTP_VERSION(rtp) != RTPVERSION) {
        session->badver++;
        return(RTP_HDR_BADVER);
    }

    /*
     * check for length inconsistent with CSRC count
     */
    minlen += RTP_CSRC_LEN(rtp);
    if (len < minlen) {
        session->badlen++;
        return(RTP_HDR_BADLEN);
    }

    /*
     * if an extension header is present,
     * make sure the packet length is consistent with it
     */
    if (RTP_EXT(rtp)) {
        rtpexttype_t *ext;

        if (len < minlen + MINRTPEXTHDRBYTES) {
            session->badlen++;
            return(RTP_HDR_BADLEN);
        }
        ext = (rtpexttype_t *)((uint8_t *)rtp + minlen);
        minlen += RTPEXTHDRBYTES(ext);
        if (len < minlen) {
            session->badlen++;
            return(RTP_HDR_BADLEN);
        } else {
            return (RTP_HDR_EXTENSION);
        }
    }
    return (RTP_HDR_OK);
}

/*
 * rtp_init_seq
 *
 * This function initializes per-source data whenever a "base
 * sequence number" is established (or re-established) for a 
 * sending source.
 *
 * Parameters:  session         ptr to per-session info
 *              source          ptr to per-source info
 *              xr_stats        ptr to rtcp xr stats info
 *              sequence        base sequence number to establish
 *              re_init_xr_mode mode of how the xr stats should be initialized
 * Returns:     None
 */
static void rtp_init_seq (rtp_hdr_session_t *session,
                          rtp_hdr_source_t  *source, 
                          rtcp_xr_stats_t *xr_stats,
                          uint16_t sequence,
                          boolean re_init_xr_mode)
{
    source->base_seq = sequence;
    source->bad_seq = RTP_SEQ_MOD +1;
    source->max_seq = sequence;
    source->cycles = 0;
    source->transit = 0;
    source->jitter = 0;
    source->received = 0;

    source->received_prior = 0;
    source->received_penult = 0;
    source->expected_prior = 0;

    source->last_arr_ts_abs = ABS_TIME_0;
    source->last_arr_ts_media = 0;

    source->out_of_order = 0;
    source->dups = 0;
    source->lost = 0;

    source->initseq_count++;
    session->initseq_count++;
    /* leave seqjumps counters as they are */

    /* Only initialize the xr_stats when the option is on */
    if (xr_stats) {
        /* Initialize rtcp_xr_stats */
        rtcp_xr_init_seq(xr_stats, sequence, re_init_xr_mode);
    }
}

/*
 * rtp_update_seq
 *
 * Function to check if the sequence falls within range, update 
 * sequence info, make sure packets are processed in order. No 
 * re-ordering or resequencing done. Function similar to update_seq
 * pseudo-code given in  RFC3550, section A.1. 
 *
 * Parameters:  session    ptr to per-session info
 *              source     ptr to per-source info
 *              xr_stats   ptr to rtcp xr stats info
 *              seq        sequence number obtained from RTP header
 *
 * Returns:     RTP_HDR_OK        - pkt ok, acceptable sequence no.
 *              RTP_HDR_SEQSTART  - pkt ok, but sequence number processing
 *                                  has been started; notify RTCP.
 *              RTP_HDR_SEQRESTART  - pkt ok, but sequence number processing
 *                                    has been restarted.
 *              RTP_HDR_SEQJUMP   - pkt seq no. not in acceptable range.
 *              RTP_HDR_SEQDUP    - pkt seq no. is dup of last pkt rcvd.
 *
 * Packets with return values of RTP_HDR_OK, RTP_HDR_SEQSTART
 * or RTP_HDR_SEQRESTART are eligible for further processing; 
 * packets with other return values should be discarded. 
 * . If a RTP_HDR_SEQSTART value is returned, then RTCP should be notified: 
 *   this is a new source (no pkts previously rcvd from this source), 
 * Note also that RTP_HDR_SEQDUP detects only the most trivial of
 * duplicates, i.e., where a packet has the same sequence number
 * as that of the packet last received.
 */
rtp_hdr_status_t rtp_update_seq (rtp_hdr_session_t *session,
                                 rtp_hdr_source_t  *source, 
                                 rtcp_xr_stats_t *xr_stats,
                                 uint16_t seq)
{
    uint16_t udelta = seq - source->max_seq;
    uint32_t eseq;

    if (!source->received) {
        /*
         * There is no probation for sources in this implementation;
         * i.e., we believe in the source when we receive the first 
         * RTP packet from that source.
         */
        rtp_init_seq(session, source, xr_stats, seq, FALSE);
        /* Update RTCP XR stats if required */
        if (xr_stats) {
            rtcp_xr_update_seq(xr_stats, seq);
        }
        source->received++;
        return(RTP_HDR_SEQSTART);
    } else if (udelta < RTP_MAX_DROPOUT) {
        /*
         * In order, with permissible gap.
         */
        if (seq < source->max_seq) {
            /*
             * Sequence number wrapped. Count another 64k cycle.
             */
            source->cycles += RTP_SEQ_MOD;
        }
        source->max_seq = seq;
        if (udelta > 1) {
            /*
             * The delta is always one behind.
             */
            source->lost += udelta - 1;
        }

        /* Only update the xr_stats when the option is on */
        if (xr_stats) {
            eseq = source->cycles + seq;
            rtcp_xr_update_seq(xr_stats, eseq);
        }
    } else if (udelta <= RTP_SEQ_MOD - RTP_MAX_MISORDER) {
        /*
         * A large jump in the sequence number.
         */
        if (seq == source->bad_seq) {
            /*
             * Two sequential packets. Assume the other side restarted.
             * Pretend as though this packet is the first packet.
             */
            rtp_init_seq(session, source, xr_stats, seq, TRUE);
            /* Update RTCP XR stats if required */
            if (xr_stats) {
                rtcp_xr_update_seq(xr_stats, seq);
            }
            source->received++;
            return (RTP_HDR_SEQRESTART);
        } else {
            source->bad_seq = (seq + 1) & (RTP_SEQ_MOD - 1);
            source->seqjumps++;
            session->seqjumps++;
            return (RTP_HDR_SEQJUMP);
        }
    } else {
        /* Only update the xr_stats when the option is on */
        if (xr_stats) {
            /* Compute extended seq based on the position of seq.
               If the difference between max_seq and seq is less than
               0, that means that seq is in previous cycle. */
            if ((int)(source->max_seq - seq) > 0) {
                eseq = source->cycles + seq;
            } else {
                eseq = source->cycles - RTP_SEQ_MOD + seq;
            }
            rtcp_xr_update_seq(xr_stats, eseq);
        }

        /*
         * Duplicate or misordered packet.
         */
        if (udelta == 0) {
            source->received++; 
            source->dups++;
            return(RTP_HDR_SEQDUP);
        }
        source->out_of_order++;
        source->lost--;
    }
    
    source->received++;
    return(RTP_HDR_OK);
}

/* 
 * rtp_hdr_ok
 *
 * Determine if a packet should be processed further,
 * based on the rtp_hdr_status_t value returned by 
 * rtp_validate_hdr or rtp_update_seq.
 *
 * Parameters:  error -- rtp_hdr_status_t value.
 * Returns:     TRUE  -- packet may be processed.
 *              FALSE -- packet should be discarded.
 * Note that this function will return TRUE for a packet
 * with the extension bit set. However, if an application 
 * chooses to discard such packets, it must look for the
 * RTP_HDR_EXTENSION value returned by rtp_validate_hdr.
 */
boolean rtp_hdr_ok (rtp_hdr_status_t status)
{
    switch (status) {
    case RTP_HDR_OK:
    case RTP_HDR_EXTENSION:
    case RTP_HDR_SEQSTART:
    case RTP_HDR_SEQRESTART:
        return (TRUE);

    case RTP_HDR_SEQJUMP:
    case RTP_HDR_SEQDUP:
    case RTP_HDR_RUNT:
    case RTP_HDR_BADVER:
    case RTP_HDR_BADLEN:
        break;
    }

    return (FALSE);
}
        
/*
 * rtp_hdr_event
 *
 * Determine if RTCP needs to be notified of an RTP event,
 * based on the rtp_hdr_status_t value returned by rtp_update_seq.
 *
 * Parameters:  error -- rtp_hdr_status_t value.
 * Returns:     RTP_EVENT_NONE -- no need to notify RTCP.
 *              RTP_EVENT_*    -- event code to pass to RTCP
 */
rtp_event_t rtp_hdr_event (rtp_hdr_status_t status)
{
    switch (status) {
    case RTP_HDR_SEQSTART:
        return (RTP_EVENT_SEQSTART);

    case RTP_HDR_OK:
    case RTP_HDR_EXTENSION:
    case RTP_HDR_SEQRESTART:
    case RTP_HDR_SEQJUMP:
    case RTP_HDR_SEQDUP:
    case RTP_HDR_RUNT:
    case RTP_HDR_BADVER:
    case RTP_HDR_BADLEN:
        break;
    }

    return (RTP_EVENT_NONE);
}
    
/*
 * rtp_update_timestamps
 *
 * Maintain "arrival time" timestamps, in "absolute time" and media units.
 *
 * Parameters:  arrival_ts -- Arrival timesamp, in "absolute time" units.
 * Returns:     a corresponding timestamp, in media (RTP) units.
 */
uint32_t rtp_update_timestamps (rtp_hdr_source_t *source, 
                                abs_time_t arrival_ts)
{
    if (!IS_ABS_TIME_ZERO(source->last_arr_ts_abs)) {
        source->last_arr_ts_media += 
            (uint32_t)(rel_time_to_pcr(TIME_SUB_A_A(arrival_ts, 
                                                    source->last_arr_ts_abs)));
        source->last_arr_ts_abs = arrival_ts;
    } else {
        source->last_arr_ts_media = 0;
        source->last_arr_ts_abs = arrival_ts;
    }

    return (source->last_arr_ts_media);
}

/*
 * rtp_update_jitter
 *
 * Updates jitter estimate.
 * 
 * Parameters:  source     -- ptr to per-source info
 *              xr_stats   -- ptr to rtcp xr stats info
 *              pak_ts     -- RTP timestamp from pkt, but in host order.
 *              arrival_ts -- Arrival timestamp, in media (RTP) units. 
 *                            Typically, derived from socket timestamp
 *                            (SO_TIMESTAMP option) by rtp_update_timestamps().
 * Returns:     None
 */
void rtp_update_jitter (rtp_hdr_source_t *source, 
                        rtcp_xr_stats_t *xr_stats,
                        uint32_t pak_ts, 
                        uint32_t arrival_ts)
{
    int32_t transit, jitter;

    transit = arrival_ts - pak_ts;
    if (source->transit) {
        jitter = transit - source->transit;
        if (jitter < 0) {
            jitter = -jitter;
        }
        source->jitter += jitter - ((source->jitter + 8) >> 4);

        /* Only update if the option is enabled */
        if (xr_stats) {
            /* Update the jitter stats */
            rtcp_xr_update_jitter(xr_stats, jitter);
        }
    }
    source->transit = transit;
}

/*
 * rtp_get_jitter
 *
 * Return the jitter estimate for this source
 *
 * Parameters: source -- per-source info
 * Returns:    jitter estimate, for inclusion in RTCP reception reports.
 */
uint32_t rtp_get_jitter (rtp_hdr_source_t *source)
{
    /* 
     * jitter is maintained in a "scaled up" form,
     * to reduce round-off error;
     * return the "scaled down" estimate.
     */
    return (source->jitter >> 4);
}

/*
 * rtp_update_prior
 *
 * Update the "prior" elements of the per-source information.
 * This must be called after RTCP prepares a reception report
 * for the source.
 *
 * Parameters:   source -- ptr to per-source info
 * Returns:      none.
 */
void rtp_update_prior (rtp_hdr_source_t *source)
{
    source->expected_prior = source->cycles 
                             + source->max_seq 
                             - source->base_seq
                             + 1 ;
    source->received_penult = source->received_prior;
    source->received_prior = source->received;
}
 
