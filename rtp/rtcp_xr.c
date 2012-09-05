/* $Id$
 * $Source$
 *------------------------------------------------------------------------------
 * rtcp_xr.c - Functions for creating / processing RTCP XR packets
 * 
 * April 2010
 *
 * Copyright (c) 2008-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#include "rtcp_xr.h"

       
/*
 * rtcp_xr_set_size
 *
 * This function sets up the maximum chunks to be used.
 *
 * Parameters:  xr_stats        ptr to rtcp_xr_stats info
 *              max_size        maximum bytes used for RLE chunks
 * Returns:     None
 */
void rtcp_xr_set_size (rtcp_xr_stats_t *xr_stats, 
                       uint16_t max_size)
{
    uint16_t max_chunks;

    if (xr_stats == NULL) {
        return;
    }

    max_chunks = max_size / sizeof(uint16_t);
    if (max_chunks > MAX_CHUNKS) {
        xr_stats->max_chunks_allow = MAX_CHUNKS;
    } else {
        xr_stats->max_chunks_allow = max_chunks;
    }
}


/*
 * rtcp_xr_init_seq
 *
 * This function initializes rtcp_xr_stats data whenever a "base
 * sequence number" is established (or re-established) for a 
 * sending source.
 *
 * Parameters:  xr_stats        ptr to rtcp_xr_stats info
 *              sequence        extended base sequence number to establish
 *              re_init_mode    mode of how to initialize xr_stats
 * Returns:     None
 */
void rtcp_xr_init_seq (rtcp_xr_stats_t *xr_stats, 
                       uint32_t sequence,
                       boolean re_init_mode)
{
    if (xr_stats == NULL) {
        return;
    }

    xr_stats->eseq_start = sequence;
    /* Depend on the condition, re_init counter will be either the total
       counts from previous unreported interval or zero. */
    if (re_init_mode) {
        xr_stats->re_init = xr_stats->totals 
                          + xr_stats->not_reported
                          + xr_stats->before_intvl;
    } else {
        xr_stats->re_init = 0;
    }

    xr_stats->totals = 0;
    xr_stats->not_reported = 0;
    xr_stats->before_intvl = 0;            

    xr_stats->exceed_limit = FALSE;

    xr_stats->lost_packets = 0;
    xr_stats->dup_packets = 0;
    xr_stats->late_arrivals = 0;

    xr_stats->min_jitter = 0;
    xr_stats->max_jitter = 0;
    xr_stats->cum_jitter = 0;
    xr_stats->sqd_jitter = 0;
    xr_stats->num_jitters = 0;

    xr_stats->cur_chunk_in_use = 0;
    xr_stats->next_exp_eseq = sequence;
    xr_stats->bit_idx = MAX_BIT_IDX;

    memset(xr_stats->chunk, 0, sizeof(uint16_t)*(MAX_CHUNKS+2));

    /* Make current one as a bit vector */
    xr_stats->chunk[0] = INITIAL_BIT_VECTOR;
}

/*
 * fill_up_zeros
 *
 * Function to fill up the run length encoding for 0's
 *
 * Parameters:  xr_stats    ptr to rtcp_xr_stats info
 *              zero_length number of zeros
 * Returns:     None
 */
static void fill_up_zeros (rtcp_xr_stats_t *xr_stats,  
                           uint16_t zeros_length)
{
    uint16_t multiples;
    uint8_t remainder;
    
    /*
     * Note: We do not need to worry about the total number of 
     * zeros being more than MAX_RUN_LENGTH here because of the 
     * rule which is used for detecting restart of the sequence.
     */

    /*
     * Compute multiples and remainder in zeros_length as
     *
     * zeros_length = multiples * 15 + remainder;
     *
     * so for multiples, it will stored in a run length of 0's
     * and remainder is in a bit vector.
     */
    multiples = zeros_length / MAX_BIT_IDX;
    remainder = zeros_length % MAX_BIT_IDX;
    if (multiples) {
        xr_stats->chunk[xr_stats->cur_chunk_in_use] = multiples * MAX_BIT_IDX;
        xr_stats->cur_chunk_in_use++;

        /* Check whether we have reached the limit or not */
        if (xr_stats->cur_chunk_in_use == xr_stats->max_chunks_allow) {
            xr_stats->exceed_limit = TRUE;
            xr_stats->cur_chunk_in_use--;
            xr_stats->bit_idx = 0;

            /* Re-adjust the totals including the last packet 
               because we can't store the remainder */
            xr_stats->totals -= (remainder + 1);
            xr_stats->not_reported += (remainder + 1);
            return;
        }
    }

    /* Handle the remainder of zeros */
    xr_stats->chunk[xr_stats->cur_chunk_in_use] = INITIAL_BIT_VECTOR;
    xr_stats->bit_idx -= (remainder + 1);
    xr_stats->chunk[xr_stats->cur_chunk_in_use] |= 1 << xr_stats->bit_idx;

    if (xr_stats->bit_idx == 0) {
        xr_stats->cur_chunk_in_use++;
        xr_stats->bit_idx = MAX_BIT_IDX;

        /* Always current one as a bit vector */
        xr_stats->chunk[xr_stats->cur_chunk_in_use] = INITIAL_BIT_VECTOR;
    }
}

/* Compare two uint32_t numbers and determine which one is ahead */
#define U32_GT(a, b) ((int32_t)(a - b) > 0)

/* Compare two uint32_t numbers and determine which one is ahead or equal */
#define U32_GTE(a, b) ((int32_t)(a - b) >= 0)

/*
 * rtcp_xr_update_seq
 *
 * Function to encode the sequence based on run length encoding algorithm.
 *
 * The essential of the RLE is to encode a sequence of lost and received
 * packets using a sequence of chunks. Each chunk is aseries of 16 bit
 * units, which specifies either a run length, or a bit vector. A run length
 * describes between 1 and 16,383 events that are all the same (either
 * all receipts or all losses). A bit vector describes 15 events that
 * may be mixed receipts and losses. For the details of RLE, see RFC 3611.
 *
 * In this function, the chunks are pre-allocated to MAX_CHUNKS+2. However,
 * we will only use the chunks up to max_chunks_allow, which is set via
 * the configuration parameter of max-size for pkt-loss-rle in SDP.
 *
 * Important parameters for RLE:
 *
 *    cur_chunk_in_use - index for current chunk in use
 *    max_chunks_allow - the water mark of maximum chunks allowed to use in
 *                       computation
 *    bit_idx          - index for last bit being used in current chunk.
 *                       When bit_idx is 0, which means next chunk is needed,
 *                       and when bit_idx is MAX_BIT_IDX (15), current chunk
 *                       is empty.
 *    next_exp_eseq    - next expected extended sequence number
 *    eseq             - current extended sequence number
 *    exceed_limit     - flag to indicate either all the chunks being used
 *                       or the total interval reaching the MAX_SEQ_ALLOWED
 *    totals           - the total number of packets in the reporting
 *                       interval
 *
 * For a bit vector, the least significant bit is represented by 0 and the
 * most significant bit is represented by 15.
 *
 *      _________________________________
 *      |1| | | | | | | | | | | | | | | |
 *      ---------------------------------
 *       |                             |
 *       -- 15th bit                   -- 0th bit
 * 
 *
 * The high level of RLE algorithm is as follow:
 *
 * if (eseq == next_exp_eseq) {
 *     next_exp_eseq++;
 *     Add the bit to current chunk.
 *     if (current chunk is full) {
 *         if (current chunk is all 1's) {
 *             Create a run length of 1 with 15 and start a new chunk
 *             if the previous chunk is not a run length of 1, or add 
 *             15 to previous chunk and re-use current chunk.
 *         } else {
 *             Start a new chunk and keep this one as a bit vector
 *         }
 *     }
 * } else {
 *     if (eseq > next_exp_eseq) {
 *         // We have missing packet(s)
 *         if (number of 0's is more than current chunk allowed) {
 *             Fill up 0's in current chunk.
 *
 *             Create a run length of 0 with multiple 15's and a bit vector
 *             with the remainder zeros and current sequence.
 *         } else {
 *             Add number of 0's and current sequence in current chunk
 *         }
 *     } else {
 *         Find out which chunk this sequence belongs to
 *         if (the existing bit for eseq == 1) {
 *             Increment the counter for dup_packets by one
 *         } else {
 *             If (the chunk this sequence belongs to is bit vector) {
 *                 Add the bit to the right position
 *             } else {
 *                 Split the run length of 0's to either two chunks or
 *                 three chunks depending on the position; Push down
 *                 the chunks by either 2 or 3 position, and insert
 *                 a bit to the right position of the bit vector.
 *             }
 *         }
 *     }
 * }
 * 
 *
 * This function also updates other summary stats based on RFC 3611.
 *
 * Parameters:  xr_stats   ptr to rtcp_xr_stats info
 *              sequence   extended sequence number
 * Returns:     None
 */
void rtcp_xr_update_seq (rtcp_xr_stats_t *xr_stats,  
                         uint32_t eseq)
{
    uint16_t max_chunks;
    uint16_t cidx;
    uint8_t  bidx;
    int      zeros_length;
    uint32_t begin_seq;
    uint16_t num_chunks;
    uint16_t total_length;
    int      lost_chunks = 0;
    uint16_t lost_length;

    if (xr_stats == NULL || xr_stats->max_chunks_allow == 0) {
        return;
    }

    /* Check whether this is a late arrival for previous reporting intervals */
    if (U32_GT(xr_stats->eseq_start, eseq)) {
        /* If so, increment the counter for before interval by one */
        xr_stats->before_intvl++;
        return;
    }

    /* Check whether we have reached the limit or not. If so,
       only increment the debug counters for internal tracking purpose */
    if (xr_stats->exceed_limit) {
        /* Check if the new sequence number is greater than next expected 
           sequence number */
        if (U32_GTE(eseq, xr_stats->next_exp_eseq)) {
            xr_stats->not_reported += (eseq - xr_stats->next_exp_eseq + 1);
            xr_stats->next_exp_eseq = eseq + 1;
            return;
        } else { 
            /* Check for the sequence number being outside of the 
               reporting interval or not */
            if (U32_GT(eseq, (xr_stats->eseq_start+xr_stats->totals))) {
                /* This is outside of the reporting interval */
                xr_stats->not_reported++;
                return;
            } else {
                /* We need to add this to the RLE even we reach the limit
                   because it is within the reporting interval */
            }
        }
    }

    /* Check whether the packet is in sequence */
    cidx = xr_stats->cur_chunk_in_use;
    max_chunks = xr_stats->max_chunks_allow;
    if (eseq == xr_stats->next_exp_eseq) {
        /*
         * Case: the packet is as expected
         */

        /* Set a 1 to the position next to the one pointed by bit_idx */
        xr_stats->bit_idx--;  /* Need to substract 1 first before setting the
                                 bit. This is because the bit_idx is always
                                 pointing to last used position. */
        xr_stats->chunk[cidx] |= 1 << xr_stats->bit_idx;
        xr_stats->next_exp_eseq++;
        xr_stats->totals++;

        /* Check to see whether need to add the total to previous chunk 
           and reuse the current one, or use next chunk */
        if (xr_stats->bit_idx == 0) {
            if (xr_stats->chunk[cidx] == ALL_ONE_VECTOR) {

                /* Check whether previous chunk is run length chunk for 1's */
                if (cidx && 
                    ((xr_stats->chunk[cidx-1] & 
                      RUN_LENGTH_FOR_ONE_MASK) == RUN_LENGTH_FOR_ONE) &&
                    ((xr_stats->chunk[cidx-1] & RUN_LENGTH_MASK) 
                     < MAX_RUN_LENGTH)) {

                    /* Add MAX_BIT_IDX to previous run length chunk */
                    xr_stats->chunk[cidx-1] += MAX_BIT_IDX;

                    /* Clean up the current chunk and reuse it */
                    xr_stats->chunk[cidx] = INITIAL_BIT_VECTOR;
                } else {
                    /* Make it as run length chunk  */
                    xr_stats->chunk[cidx] = RUN_LENGTH_FOR_ONE | MAX_BIT_IDX;
                    xr_stats->cur_chunk_in_use++;

                    /* Always current one as a bit vector */
                    xr_stats->chunk[xr_stats->cur_chunk_in_use] = 
                        INITIAL_BIT_VECTOR;
                }
            } else {
                /* Keep it as a bit vector and increment the chunk in use */
                xr_stats->cur_chunk_in_use++;

                /* Always current one as a bit vector */
                xr_stats->chunk[xr_stats->cur_chunk_in_use] = 
                    INITIAL_BIT_VECTOR;
            }

            xr_stats->bit_idx = MAX_BIT_IDX;
        }
    } else { /* case of eseq != xr_stats->next_exp_eseq */
        if (U32_GT(eseq, xr_stats->next_exp_eseq)) {
            /*
             * Case: there is(are) packet(s) missing
             */
            zeros_length = eseq - xr_stats->next_exp_eseq;
            xr_stats->next_exp_eseq = eseq + 1;
            xr_stats->totals = eseq - xr_stats->eseq_start + 1;
            xr_stats->lost_packets += zeros_length;
            if (xr_stats->bit_idx == MAX_BIT_IDX) {
                /* This is a empty bit vector */
                fill_up_zeros(xr_stats, zeros_length);
            } else {
                /* Fill up the remainder of the bit vector first */
                if (zeros_length >= xr_stats->bit_idx ) {
                    zeros_length -= xr_stats->bit_idx;
                    xr_stats->bit_idx = MAX_BIT_IDX;
                    xr_stats->cur_chunk_in_use++;

                    /* Check whether we have reached the limit or not */
                    if (xr_stats->cur_chunk_in_use == max_chunks) {
                        xr_stats->exceed_limit = TRUE;
                        xr_stats->cur_chunk_in_use--;
                        xr_stats->bit_idx = 0;

                        /* Adjust the totals including the last packet */
                        xr_stats->totals -= (zeros_length + 1);
                        xr_stats->not_reported += (zeros_length + 1);
                       
                        return;
                    }
                    
                    fill_up_zeros(xr_stats, zeros_length);
                } else {
                    /* No need to use another chunk */
                    xr_stats->bit_idx -= (zeros_length + 1);
                    xr_stats->chunk[cidx] |= 1 << xr_stats->bit_idx;

                    if (xr_stats->bit_idx == 0) {
                        xr_stats->bit_idx = MAX_BIT_IDX;
                        xr_stats->cur_chunk_in_use++;

                        /* Always current one as a bit vector */
                        xr_stats->chunk[xr_stats->cur_chunk_in_use] 
                            = INITIAL_BIT_VECTOR;
                    }
                }
            }
        } else {
            /*
             * Case: there is a late arrival or duplicate packet
             */

            /* First, figure out in which chunk this packet belongs to 
               and start from the last one in use */
            if (xr_stats->chunk[cidx] & INITIAL_BIT_VECTOR) {
                begin_seq = xr_stats->eseq_start + xr_stats->totals
                          - (MAX_BIT_IDX - xr_stats->bit_idx);
            } else {
                /* This means that last chunk is not a bit vector */
                begin_seq = xr_stats->eseq_start + xr_stats->totals
                          - (xr_stats->chunk[cidx] & RUN_LENGTH_MASK);
            }

            while (eseq < begin_seq) {
                cidx--;
                if (xr_stats->chunk[cidx] & INITIAL_BIT_VECTOR) {
                    begin_seq -= MAX_BIT_IDX;
                } else {
                    begin_seq -= (xr_stats->chunk[cidx] & RUN_LENGTH_MASK);
                }
            }

            /* Now we found which chunk the packet belongs to */
            /* Check whether this chunk is the bit vector or not */
            if (xr_stats->chunk[cidx] & INITIAL_BIT_VECTOR) {
                bidx = MAX_BIT_IDX - 1 - (eseq - begin_seq);

                /* Check whether this is duplicated packet */
                if ((xr_stats->chunk[cidx] >> bidx) & 0x1) {
                    xr_stats->dup_packets++;
                } else {
                    xr_stats->chunk[cidx] |= 1 << bidx;
                    xr_stats->lost_packets--;
                    xr_stats->late_arrivals++;
                }
            } else { /* Not a bit vector */
                /* Check whether this chunk is a run length chunk for 1's */
                if ((xr_stats->chunk[cidx] & RUN_LENGTH_FOR_ONE_MASK) 
                    == RUN_LENGTH_FOR_ONE) {
                    xr_stats->dup_packets++;
                } else {
                    /* This is a run length chunk for 0's */
                    /* Need to push down the chunks in order to insert 1 */
                    total_length = xr_stats->cur_chunk_in_use - cidx + 1;

                    /* Depend on whether 1 need to be inserted at the 
                       beginning, middle or the end of the 0 run length 
                       chunk,  we need to push down either zero, one or 
                       two chunks */
                    bidx = eseq - begin_seq;
                    if (xr_stats->chunk[cidx] == MAX_BIT_IDX) {
                        /* No need to push down, and just
                           change it to a bit vector */
                        xr_stats->chunk[cidx] = INITIAL_BIT_VECTOR;
                        xr_stats->chunk[cidx] |= 1 << (MAX_BIT_IDX - bidx - 1);
                    }
                    else if (bidx < MAX_BIT_IDX) {
                        /* This is the case of the packet being at the 
                           beginning portion of 0 run length */

                        /* Need to push down by one chunk */
                        memmove(&xr_stats->chunk[cidx+1], 
                                &xr_stats->chunk[cidx],
                                sizeof(uint16_t) * total_length);

                        /* Insert a 1 in the first chunk */
                        xr_stats->chunk[cidx] = INITIAL_BIT_VECTOR;
                        xr_stats->chunk[cidx] |= 1 << (MAX_BIT_IDX - bidx - 1);

                        /* Adjust the length in original run length */
                        xr_stats->chunk[cidx+1] -= MAX_BIT_IDX;

                        if (xr_stats->cur_chunk_in_use < max_chunks) {
                            xr_stats->cur_chunk_in_use++;

                            /* If exceeded the limit, need to adjust the totals
                               later */
                            if (xr_stats->cur_chunk_in_use == max_chunks) {
                                lost_chunks = 1;
                            }
                        }
                    } else {
                        bidx = begin_seq + xr_stats->chunk[cidx] - eseq;
                        if (bidx < MAX_BIT_IDX) {
                            /* This is the case of the packet being at the 
                               ending portion of 0 run length */

                            /* Need to push down by one chunk */
                            memmove(&xr_stats->chunk[cidx+1], 
                                    &xr_stats->chunk[cidx],
                                    sizeof(uint16_t) * total_length);

                            /* Copy the original run length here and adjust 
                               the length accordingly */
                            xr_stats->chunk[cidx] = 
                                xr_stats->chunk[cidx+1] - MAX_BIT_IDX;

                            /* Insert a 1 in the second chunk */
                            xr_stats->chunk[cidx+1] = INITIAL_BIT_VECTOR;
                            xr_stats->chunk[cidx+1] |= 1 << (bidx - 1);

                            if (xr_stats->cur_chunk_in_use < max_chunks) {
                                xr_stats->cur_chunk_in_use++;

                                /* If exceeded the limit, need to adjust the
                                   totals later */
                                if (xr_stats->cur_chunk_in_use == max_chunks) {
                                    lost_chunks = 1;
                                }
                            }
                        } else {
                            /* This is the case of the packet being at the 
                               middle portion of 0 run length */

                            /* Need to push down by two chunks */
                            memmove(&xr_stats->chunk[cidx+2], 
                                    &xr_stats->chunk[cidx],
                                    sizeof(uint16_t) * total_length);

                            /* Figure out the first portion of 0 run length */
                            num_chunks = (eseq - begin_seq) / MAX_BIT_IDX;
                            xr_stats->chunk[cidx] = num_chunks * MAX_BIT_IDX;

                            /* Insert a 1 in the second chunk */
                            bidx = eseq - (begin_seq + xr_stats->chunk[cidx]);
                            xr_stats->chunk[cidx+1] = INITIAL_BIT_VECTOR;
                            xr_stats->chunk[cidx+1] |= 1<<(MAX_BIT_IDX-bidx-1);

                            /* Adjust the length in third chunk */
                            num_chunks = xr_stats->chunk[cidx+2]/MAX_BIT_IDX 
                                - num_chunks - 1;
                            xr_stats->chunk[cidx+2] = num_chunks * MAX_BIT_IDX;

                            if (xr_stats->cur_chunk_in_use < max_chunks) {
                                xr_stats->cur_chunk_in_use += 2;
                            }

                            lost_chunks = xr_stats->cur_chunk_in_use
                                        - max_chunks + 1;

                            if (lost_chunks < 0) {
                                lost_chunks = 0;
                            }
                        }
                    }
                
                    /* Reset the cur_chunk_in_use */
                    if (lost_chunks > 0) {
                        /* Current chunk in use should always point to
                           a valid one */
                        xr_stats->cur_chunk_in_use -= lost_chunks;
                        xr_stats->exceed_limit = TRUE;
                    }

                    /* Adjust the totals based on lost_chunks */
                    while (lost_chunks) {
                        /* Get rid of the last chunk */
                        lost_chunks--;
                        if (xr_stats->chunk[max_chunks+lost_chunks] 
                            & INITIAL_BIT_VECTOR) {
                            lost_length = MAX_BIT_IDX 
                                - xr_stats->bit_idx;
                            xr_stats->totals -= lost_length;
                            xr_stats->not_reported += lost_length;
                            xr_stats->bit_idx = 0;
                        } else {
                            lost_length = 
                                xr_stats->chunk[max_chunks+lost_chunks]
                                & RUN_LENGTH_MASK;
                            xr_stats->totals -= lost_length;
                            xr_stats->not_reported += lost_length;
                        }
                    }

                    /* Re-adjust the total lost packet counter and 
                       late arrival counter */
                    xr_stats->lost_packets--;
                    xr_stats->late_arrivals++;
                }
            }
        }
    }

    /* Check whether we have reached the limit or not */
    if (!xr_stats->exceed_limit &&
        ((xr_stats->cur_chunk_in_use == xr_stats->max_chunks_allow) ||
         (xr_stats->totals >= MAX_SEQ_ALLOWED))) {
        /* If so, make exceed_limit to be TRUE */
        xr_stats->exceed_limit = TRUE;
        /* The current chunk in use should always point to last valid one */
        if (xr_stats->cur_chunk_in_use == xr_stats->max_chunks_allow) {
            xr_stats->cur_chunk_in_use--;
        }
        xr_stats->bit_idx = 0;
    }
}


/*
 * rtcp_xr_update_jitter
 *
 * Updates jitter stats for rtcp xr report.
 * 
 * Parameters:  xr_stats   -- ptr to rtcp xr stats
 *              jitter     -- jitter value
 *
 * Returns:     None
 */
void rtcp_xr_update_jitter (rtcp_xr_stats_t *xr_stats,
                            uint32_t jitter)
{
    if (!xr_stats->exceed_limit) {
        if (jitter < xr_stats->min_jitter) {
            xr_stats->min_jitter = jitter;
        }

        if (jitter > xr_stats->max_jitter) {
            xr_stats->max_jitter = jitter;
        }

        xr_stats->cum_jitter += jitter;
        xr_stats->sqd_jitter += jitter * jitter;
        xr_stats->num_jitters++;
    }
}

/*
 * rtcp_xr_get_mean_jitter
 *
 * Compute the mean of jitters for rtcp xr report.
 * 
 * Parameters:  xr_stats   -- ptr to rtcp xr stats
 *
 * Returns:     mean of the jitters
 */
uint32_t rtcp_xr_get_mean_jitter (rtcp_xr_stats_t *xr_stats)
{
    if (xr_stats->num_jitters) {
        return (VQE_DIV64(xr_stats->cum_jitter, xr_stats->num_jitters));
    } else {
        return 0;
    }
}
    
/*
 * rtcp_xr_get_std_jitter
 *
 * Compute the standard deviation of jitters for rtcp xr report.
 * 
 * Parameters:  xr_stats   -- ptr to rtcp xr stats
 *
 * Returns:     standard deviation of the jitters
 */
uint32_t rtcp_xr_get_std_dev_jitter (rtcp_xr_stats_t *xr_stats)
{
    uint32_t dev_jitter = 0;

    if (xr_stats->num_jitters > 1) {
        dev_jitter = VQE_DIV64((xr_stats->sqd_jitter - xr_stats->cum_jitter 
                                * rtcp_xr_get_mean_jitter(xr_stats)),
                               (xr_stats->num_jitters - 1));
                
        dev_jitter = (uint32_t)VQE_SQRT(dev_jitter);
    }

    return dev_jitter;
}

/*
 * TLV types and length for XR MA block. 
 * Must have same index values as rtcp_xr_ma_tlv_types_t enum in rtcp.h
 * Due to types starting in 200's must compensate with XR_MA_TLV_T_OFFSET
 */
static struct tl {
    rtcp_xr_ma_tlv_types_t type;
    uint16_t len;
    boolean vendor_spec; /* True if TLV block is vendor specific */
} ma_xr_tl[] = {
    {0, 0, 0},  /* XR_MA_TLV_T_OFFSET */
    {FIRST_MCAST_EXT_SEQ, 4, FALSE},
    {SFGMP_JOIN_TIME,     4, FALSE},
    {APP_REQ_TO_RTCP_REQ, 4, FALSE},
    {RTCP_REQ_TO_BURST,   4, FALSE},
    {RTCP_REQ_TO_BURST_END, 4, FALSE},
    {APP_REQ_TO_MCAST,   4, FALSE},
    {APP_REQ_TO_PRES,    4, FALSE},
    {NUM_DUP_PKTS,        4, FALSE},
    {NUM_GAP_PKTS,        4, FALSE},
    {TOTAL_CC_TIME,       4, TRUE},
    {RCC_EXPECTED_PTS,    8, TRUE},
    {RCC_ACTUAL_PTS,      8, TRUE},
};

/**
 * Number of bytes to which the beginning of every TLV element must
 * be aligned
 */
#define RTCP_XR_MA_TLV_ALIGNMENT 4

/* Compute number of bytes occupied by a XR MA TLV Set.
 *
 * Parameters: tlv_types  -- TLV Types present in this set
 *             num_tlvs   -- Number of entries in tlv_types arrary
 *
 * Returns: Number of bytes in set given by tlv_types.
 */
static inline 
uint16_t rtcp_xr_ma_tlv_set_size (const rtcp_xr_ma_tlv_types_t *tlv_types,
                                   uint8_t num_tlvs)
{
    uint8_t i = 0;
    uint16_t tlv_length = 0;
    
    for (i = 0; i < num_tlvs; i++) {
        uint8_t tlv_type_idx = tlv_types[i] - XR_MA_TLV_T_OFFSET;

        if (ma_xr_tl[tlv_type_idx].vendor_spec) {
            tlv_length += sizeof(rtcp_xr_ma_vs_tlv_t);
        } else {
            tlv_length += sizeof(rtcp_xr_ma_tlv_t);
        }

        tlv_length += ma_xr_tl[tlv_type_idx].len;

        /* Pad every TLV element to the nearest 32-bit word boundary */
        tlv_length = (((tlv_length / RTCP_XR_MA_TLV_ALIGNMENT) + 1) * 
                      RTCP_XR_MA_TLV_ALIGNMENT);
    }

    return (tlv_length);
}

/*
 * Write a set of XR MA TLVs to the packet buffer.
 *
 * It is assumed that the buffer has enough space to fit the entire
 * TLV. This should have been checked with rtcp_xr_ma_tlv_set_size above.
 * 
 * Parameters: tlv_types  -- TLV Types present in this set
 *             num_tlvs   -- Number of entries in tlv_types arrary
 *             vendor_spec -- TRUE if TLV entries should be encoded as 
 *                            vendor specific. FALSE otherwise.
 *             p_data     -- Pointer to packet buffer
 *
 * Returns: Number of bytes written to p_data (i.e. xr_ma_tlv_set_size())
 */
static inline
uint16_t rtcp_xr_ma_write_tlv_set (const rtcp_xr_ma_tlv_types_t *tlv_types,
                                   uint8_t num_tlvs,
                                   rtcp_xr_ma_stats_t *xr_ma,
                                   uint8_t *p_data) 
{
    uint8_t i = 0;
    uint8_t *ptr = p_data; 

    for (i = 0; i < num_tlvs; i++) {
        uint32_t u32_nl = 0;
        uint64_t u64_nll = 0;
        uint16_t tlv_length = 0;
        uint8_t tlv_type_idx = tlv_types[i] - XR_MA_TLV_T_OFFSET;

        /* Create separate TLV header based for vendor specific TLVs than
         * for generic TLVs. Pad TLV to 32-bit boundary. Payload is all 0's
         * from the earlier memset which clears entire MA payload. Calculate
         * the length as a multiple of 32bits & then subtract out length of
         * TLV header. Even for the VS TLV, only subtract out base TLV
         * header length, because length field includes the enterprise # len
         */
        if (ma_xr_tl[tlv_type_idx].vendor_spec == FALSE) {
            rtcp_xr_ma_tlv_t *ma_tlv = (rtcp_xr_ma_tlv_t*)ptr;
            ma_tlv->type = tlv_types[i];
            tlv_length = ((((sizeof(rtcp_xr_ma_tlv_t) + 
                             ma_xr_tl[tlv_type_idx].len) / 
                            RTCP_XR_MA_TLV_ALIGNMENT) + 1) * 
                          RTCP_XR_MA_TLV_ALIGNMENT);
            ma_tlv->length =  htons(tlv_length - sizeof(rtcp_xr_ma_tlv_t));
            ptr += sizeof(rtcp_xr_ma_tlv_t);
            
        } else {
            rtcp_xr_ma_vs_tlv_t *ma_vs_tlv = (rtcp_xr_ma_vs_tlv_t*)ptr;
            ma_vs_tlv->type = tlv_types[i];
            tlv_length = ((((sizeof(rtcp_xr_ma_vs_tlv_t) + 
                             ma_xr_tl[tlv_type_idx].len) / 
                            RTCP_XR_MA_TLV_ALIGNMENT) + 1) * 
                          RTCP_XR_MA_TLV_ALIGNMENT);
            ma_vs_tlv->length = htons(tlv_length - sizeof(rtcp_xr_ma_tlv_t));
            ma_vs_tlv->ent_num = htonl(RTCP_XR_ENTERPRISE_NUM_CISCO);
            ptr += sizeof(rtcp_xr_ma_vs_tlv_t);
        }

        switch (tlv_types[i]) {
        case FIRST_MCAST_EXT_SEQ:
            u32_nl = htonl(xr_ma->first_mcast_ext_seq);
            break;

        case SFGMP_JOIN_TIME:
            u32_nl = htonl(xr_ma->sfgmp_join_time);
            break;
            
        case APP_REQ_TO_RTCP_REQ:
            u32_nl = htonl(xr_ma->app_req_to_rtcp_req);
            break;

        case RTCP_REQ_TO_BURST:
            u32_nl = htonl(xr_ma->rtcp_req_to_burst);
            break;

        case RTCP_REQ_TO_BURST_END:
            u32_nl = htonl(xr_ma->rtcp_req_to_burst_end);
            break;

        case APP_REQ_TO_MCAST:
            u32_nl = htonl(xr_ma->app_req_to_mcast);
            break;

        case APP_REQ_TO_PRES:
            u32_nl = htonl(xr_ma->app_req_to_pres);
            break;

        case NUM_DUP_PKTS:
            u32_nl = htonl(xr_ma->num_dup_pkts);
            break;

        case NUM_GAP_PKTS:
            u32_nl = htonl(xr_ma->num_gap_pkts);
            break;

        case TOTAL_CC_TIME:
            u32_nl = htonl(xr_ma->total_cc_time);
            break;

        case RCC_EXPECTED_PTS:
            u64_nll = htonll(xr_ma->rcc_expected_pts);
            break;

        case RCC_ACTUAL_PTS:
            u64_nll = htonll(xr_ma->rcc_actual_pts);
            break;
                
        default:
            /* No other types, need this to silence compiler warning */
            break;
        }

        if (u32_nl > 0) {
            memcpy(ptr, &u32_nl, sizeof(u32_nl));
        } else if (u64_nll > 0){
            memcpy(ptr, &u64_nll, sizeof(u64_nll));
        }

        if (ma_xr_tl[tlv_type_idx].vendor_spec == FALSE) {
            ptr += tlv_length - sizeof(rtcp_xr_ma_tlv_t);
        } else {
            ptr += tlv_length - sizeof(rtcp_xr_ma_vs_tlv_t);
        }
    }
    
    return (ptr - p_data);
}

/*
 * Write a set of XR DC TLVs to the packet buffer.
 *
 * It is assumed that the buffer has enough space to fit the entire TLV
 * 
 * Parameters: xr_dc_stats(in)  -- ptr to xr_dc_stats structure
 *             p_tlv(out) -- Ptr to packet into which stats would be populated
 *             tlv_type(in) -- Type of the data to be written.
 *             bytes_left(in)     -- Bytes left in the packet buffer
 *
 * Returns: Number of bytes written to p_data. Zero if unsuccessful
 */
static inline
uint16_t rtcp_xr_dc_write_tlv_set (rtcp_xr_dc_stats_t *xr_dc_stats,
                                   uint8_t *p_tlv,                              
                                   const rtcp_xr_dc_types_t tlv_type,
                                   uint16_t bytes_left)
{
    uint16_t len = sizeof(xr_dc_stats->type[tlv_type]);
    uint16_t temp16;
    uint32_t temp32; 
    uint64_t temp64;
    
    if (len > 0 && len < 4) {
        len = 4;
    }
    if (len > 8) {
        /* unsupported */
        return 0;
    } 
    if ((int)(bytes_left - len) < 0) {
        return 0;
    }

    p_tlv[0] = tlv_type;
    temp16 = htons(len);
    memcpy(&p_tlv[2], &temp16, 2);
    if (len == 4) {
        temp32 = htonl(xr_dc_stats->type[tlv_type]);
        memcpy(&p_tlv[4], &temp32, 4);
    } else {
        temp64 = htonll(xr_dc_stats->type[tlv_type]);
        memcpy(&p_tlv[4], &temp64, 8);
    }
    return(len + 4);
}

/* 
 * Encode RTCP XR DC blocks in TLV format. This is the only format
 * supported.
 *
 * Parameters: xr_dc_stats    - ptr to DC stats
 *             xr_ssrc  - SSRC of primary multicast
 *             p_xr     - Buffer to write XR data into
 *             data_len - Number of bytes remaining in p_xr
 * Returns:    Number of bytes written into p_xr. If the packet is not large
 *             enough to hold all data, return 0 and write none.
 */
uint16_t rtcp_xr_encode_dc (rtcp_xr_dc_stats_t *xr_dc_stats,
                            uint32_t xr_ssrc,
                            uint8_t *p_xr,
                            uint16_t data_len)
{
    uint16_t dc_len = 0;
    /* 
     * The RTCP-XR DC block has the following encoding
     *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |     BT=TBD    | Ver=1 | flags |         Block Length          |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |              SSRC of the Primary Multicast Stream             |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |     Type      |   Reserved    |            Length             |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  :                             Value                             :
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */
    rtcp_xr_dc_t *p_dc;
    uint8_t *temp_ptr;
    uint16_t len;
    int i;

    if (data_len == 0) {
        return 0;
    }

    p_dc = (rtcp_xr_dc_t *)p_xr;
    dc_len = sizeof(p_dc->bt) + sizeof(p_dc->ver_flags) + 
             sizeof(p_dc->length) + sizeof(p_dc->xr_ssrc);
    /* Checking if there is space for BT, Ver, Flags, Blen and SSRC */
    if ((int)(data_len - dc_len) < 0) {
        return 0;
    }

    p_dc->bt = RTCP_XR_DC;  /* type */
    p_dc->ver_flags = ((RTCP_XR_DC_VERSION << RTCP_XR_DC_VERSION_SHIFT) | 
                       RTCP_XR_DC_FLAGS);
    /* ssrc */
    p_dc->xr_ssrc = htonl(xr_ssrc);
    data_len -= dc_len;

    temp_ptr = &p_dc->tlv_data[0];
    /* Encode TLVs */
    for (i=0; i< MAX_RTCP_XR_DC_TYPES; i++) {
        if (xr_dc_stats->type[i]) {
            len = rtcp_xr_dc_write_tlv_set(xr_dc_stats, 
                                           temp_ptr, 
                                           i,
                                           data_len);
            if (len != 0) {
                data_len -= len;
                dc_len += len;
                temp_ptr += len;
            } else {
                /* we have run out of space in the buffer */
                return (0); 
            }
        }
    }

    /*
     * The length should be in 32-bit words less one. 
     * All block types, private or fully standardized, need this same 
     * definition of length, per RFC 3611 to allow skipping over unrecognized
     * packets
     */
    p_dc->length = htons((dc_len>>2) - 1);
    return (dc_len);
}

/*
 * Decode an RTCP XR DC report. It is made up of several mandatory fields 
 * followed by TLVs
 *
 * Parameters: p_data     -- ptr to beginning of MA report block
 *             p_len      -- Length of data in p_data
 *             p_xr_ma   -- Ptr to location to store XR MA stats in
 *
 * Returns:    TRUE when decoding succeeds, FALSE otherwise. When FALSE is 
 *             returned, the contents of p_xr_dc is undefined. 
 *             p_xr_dc_stats should be initialized to be populated by 0
 */
boolean rtcp_xr_decode_dc (uint8_t *p_data, uint16_t p_len, 
                           rtcp_xr_dc_stats_t *p_xr_dc_stats)
{
    uint8_t tlv_type;
    uint16_t block_len = 0;
    uint32_t block_len_in_bytes;
    uint32_t i;
    uint16_t tlv_len;

    if (p_len < 8) {
        return FALSE;
    }
    /* Assured that p_len[0] to p_len[7] are addressable */
    /* Decode Block type, version, flags and length and SSRC */
    if (p_data[0] != RTCP_XR_DC) {
        return FALSE;
    }
    if (p_data[1] != ((RTCP_XR_DC_VERSION << RTCP_XR_DC_VERSION_SHIFT) | 
                      RTCP_XR_DC_FLAGS)) {
        return FALSE;
    }
    /* Block length */
    block_len = ntohs(*((uint16_t *)&p_data[2]));
    block_len += 1;
    block_len_in_bytes = block_len << 2;
    if (block_len_in_bytes > p_len) {
        return (FALSE); /* Partial decoding not supported */
    }
    p_xr_dc_stats->xr_ssrc = ntohl(*((uint32_t *)&p_data[4]));
    
    /* Decode TLVs */
    /* block_len_in_bytes is definitely not greater than p_len */
    i=8;
    while (i < block_len_in_bytes) {
        /* 
         * i <block_len_in_bytes <= p_len, and i<block_len_in_bytes 
         * i.e. we are assured that i < p_len
         */
        memcpy(&tlv_type, &p_data[i], 1);
        /* Validate if tlv_type is < MAX_RTCP_XR_DC_TYPES */
        if (tlv_type >=  MAX_RTCP_XR_DC_TYPES) {
            return FALSE;
        }
        
        /* Decode the length */
        if ((i+4) > p_len) {
            return FALSE;
        }
        /*
         * i < p_len and i+4 <= p_len
         * which means that i, i+1, i+2, i+3 are addressable
         */
        tlv_len = ntohs(*((uint16_t *)&p_data[i+2]));

        /* Decode the Value */
        if ((i+4+tlv_len) > p_len) {
            return FALSE;
        }
        /* i+4, i+4+1, i+4+2, i+4+(tlv_len-1) are addressable */
        if (tlv_len == 4) {
            p_xr_dc_stats->type[tlv_type] = ntohl(*((uint32_t *)&p_data[i+4]));
        } else if (tlv_len == 8)  {
            p_xr_dc_stats->type[tlv_type] = ntohll(*((uint64_t *)&p_data[i+4]));
        } else {
            return FALSE;
        }
        i += (tlv_len + 4);
    }
    return TRUE;
}

/*
 * Encode RTCP XR MA report into TLV blocks. 
 *
 * The MA report consists of both mandatory and optional fields. Several
 * of the optional fields exist only when the primary multicast or repair 
 * burst streams have been received. Some of the fields are grouped together
 * by events that have occured such as receiving the first multicast or 
 * presenting the first frame.  The first set is sent only when the 
 * primary multicast stream has been received (req_to_mcast > 0). This set 
 * includes {FIRST_MCAST_EXT_SEQ, SFGMP_JOIN_TIME, APP_REQ_TO_MCAST,
 * NUM_DUP_PKTS, NUM_GAP_PKTS}. The second set is included after the video
 * has been presented and is made up of {APP_REQ_TO_PRES} and the vendor 
 * specific TLVs below. The other TLV elements are sent only when they hold
 * valid values. 
 *
 * There are also several vendor specific TLV fields {TOTAL_CC_TIME,
 * RCC_EXPECTED_PTS, and RCC_ACTUAL_PTS}. This format includes the IANA
 * enterprise number to ensure uniqueness across vendors. These are included 
 * along with the presentation TLVs
 * 
 * Parameters: xr_ma    -- ptr to MA stats
 *             xr_ssrc   -- SSRC of the primary multicast
 *             p_xr      -- buffer to write XR data into 
 *             data_len  -- amount of data remaining in p_xr
 * 
 * Returns:    Number of bytes written info p_xr. If the packet is not large
 *             enough to hold all data, return 0 and write none.
 */
uint16_t rtcp_xr_encode_ma (rtcp_xr_ma_stats_t *xr_ma,
                            uint32_t xr_ssrc,
                            uint8_t *p_xr,
                            uint16_t data_len)
{
    rtcp_xr_ma_t *p_ma = NULL;
    uint16_t tlv_length = 0;

    const rtcp_xr_ma_tlv_types_t app_types[] = {
        APP_REQ_TO_RTCP_REQ,
    };

    const rtcp_xr_ma_tlv_types_t burst_types[] = {
        RTCP_REQ_TO_BURST,
    };

    const rtcp_xr_ma_tlv_types_t burst_end_types[] = {
        RTCP_REQ_TO_BURST_END,
    };

    const rtcp_xr_ma_tlv_types_t mcast_types[] = {
        FIRST_MCAST_EXT_SEQ, 
        SFGMP_JOIN_TIME, 
        APP_REQ_TO_MCAST,
        NUM_DUP_PKTS,
    };

    const rtcp_xr_ma_tlv_types_t burst_mcast_types[] = {
        NUM_GAP_PKTS,
    };

    const rtcp_xr_ma_tlv_types_t pres_types[] = {
        APP_REQ_TO_PRES,
        TOTAL_CC_TIME,
        RCC_EXPECTED_PTS,
        RCC_ACTUAL_PTS,
    };

    /* Compute total size of XR including TLVs, check to be sure it fits.
     * Some TLVs are linked together depending on reception of mcast stream
     * or rcc burst */
    tlv_length = sizeof(rtcp_xr_ma_t);

    /* Cannot use a >0 check here because the req->app time often is 0 */
    if (xr_ma->status != ERA_RCC_NORCC) {
        tlv_length += rtcp_xr_ma_tlv_set_size(app_types, 
                                              (sizeof(app_types) /
                                               sizeof(app_types[0])));
    }

    if (xr_ma->rtcp_req_to_burst > 0) {
        tlv_length += rtcp_xr_ma_tlv_set_size(burst_types, 
                                              (sizeof(burst_types) /
                                               sizeof(burst_types[0])));
    }

    if (xr_ma->rtcp_req_to_burst_end > 0) {
        tlv_length += rtcp_xr_ma_tlv_set_size(burst_end_types, 
                                              (sizeof(burst_end_types) /
                                               sizeof(burst_end_types[0])));
    }

    if  (xr_ma->app_req_to_mcast > 0) {
        tlv_length += rtcp_xr_ma_tlv_set_size(mcast_types, 
                                              (sizeof(mcast_types) /
                                               sizeof(mcast_types[0])));
    }

    /* Send when unicast burst has ended and multicast has been received. */
    if ((xr_ma->rtcp_req_to_burst_end > 0) &&
        (xr_ma->app_req_to_mcast > 0)) {
             tlv_length += rtcp_xr_ma_tlv_set_size(burst_mcast_types, 
                                              (sizeof(burst_mcast_types) /
                                               sizeof(burst_mcast_types[0])));
    }

    if (xr_ma->app_req_to_pres > 0) {
        tlv_length += rtcp_xr_ma_tlv_set_size(pres_types, 
                                              (sizeof(pres_types) /
                                               sizeof(pres_types[0])));
    }


    if (tlv_length > data_len) {
        return (0);
    }


    /* Begin writing data into packet */
    memset(p_xr, 0, tlv_length);
    p_ma = (rtcp_xr_ma_t*)p_xr;
    p_ma->bt = RTCP_XR_MA;
    p_ma->type_specific = xr_ma->status;
    p_ma->length = htons((tlv_length >> 2) - 1);
    p_ma->xr_ssrc = htonl(xr_ssrc);

    p_xr += sizeof(rtcp_xr_ma_t);

    if (xr_ma->status != ERA_RCC_NORCC) {
        p_xr += rtcp_xr_ma_write_tlv_set(app_types, 
                                         (sizeof(app_types) /
                                          sizeof(app_types[0])),
                                         xr_ma,
                                         p_xr);
    }

    if (xr_ma->rtcp_req_to_burst > 0) {
        p_xr += rtcp_xr_ma_write_tlv_set(burst_types, 
                                         (sizeof(burst_types) /
                                          sizeof(burst_types[0])),
                                         xr_ma,
                                         p_xr);   
    }

    if (xr_ma->rtcp_req_to_burst_end > 0) {
        p_xr += rtcp_xr_ma_write_tlv_set(burst_end_types, 
                                         (sizeof(burst_end_types) /
                                          sizeof(burst_end_types[0])),
                                         xr_ma,
                                         p_xr);   
    }
    
    if (xr_ma->app_req_to_mcast > 0) {
        p_xr += rtcp_xr_ma_write_tlv_set(mcast_types, 
                                         (sizeof(mcast_types) /
                                          sizeof(mcast_types[0])),
                                         xr_ma,
                                         p_xr);
    }

    if ((xr_ma->rtcp_req_to_burst_end > 0) &&
        (xr_ma->app_req_to_mcast > 0)) {
        p_xr += rtcp_xr_ma_write_tlv_set(burst_mcast_types, 
                                         (sizeof(burst_mcast_types) /
                                          sizeof(burst_mcast_types[0])),
                                         xr_ma,
                                         p_xr);

    }
    
    if (xr_ma->app_req_to_pres > 0) {
        p_xr += rtcp_xr_ma_write_tlv_set(pres_types, 
                                         (sizeof(pres_types) /
                                          sizeof(pres_types[0])),
                                         xr_ma,
                                         p_xr);
    }
    
    return (tlv_length); 
}

/*
 * Decode an RTCP XR MA report. It is made up of several mandatory fields
 * followed by optional TLVs. The optional TLVs come in two formats, generic
 * and vendor specific.
 *
 * The calling function should have already verified the bt field, it will
 * not be validated here.
 * 
 * Parameters: p_data     -- ptr to beginning of MA report block
 *             p_len      -- Length of data in p_data
 *             p_xr_ma   -- Ptr to location to store XR MA stats in
 *
 * Returns:    TRUE when decoding succeeds, FALSE otherwise. When FALSE is 
 *             returned, the contents of p_xr_ma is undefined
 *
 */
boolean rtcp_xr_decode_ma (uint8_t *p_data, uint16_t p_len, 
                           rtcp_xr_ma_stats_t *p_xr_ma)

{
    rtcp_xr_ma_t *p_ma = (rtcp_xr_ma_t*)p_data;
    uint8_t *ptr = NULL;
    uint32_t block_length = 0;
    uint8_t tlv_type = 0;
    uint16_t tlv_length = 0;
    uint32_t u32_nl = 0, ent_num = 0;
    uint64_t u64_nll = 0;
    uint8_t tlv_type_idx = 0;

    memset(p_xr_ma, 0, sizeof(*p_xr_ma));

    p_xr_ma->status = p_ma->type_specific;
    
    block_length = ((ntohs(p_ma->length) + 1) << 2);
    if (block_length > p_len) {
        return (FALSE);
    }

    p_xr_ma->xr_ssrc = ntohl(p_ma->xr_ssrc);
    
    ptr = (uint8_t*)p_ma + sizeof(*p_ma);
    
    while (ptr < p_data + block_length) {
        rtcp_xr_ma_tlv_t *p_ma_tlv = (rtcp_xr_ma_tlv_t*)ptr;
        rtcp_xr_ma_vs_tlv_t *p_ma_vs_tlv = (rtcp_xr_ma_vs_tlv_t*)ptr;
        tlv_type = p_ma_tlv->type;
        tlv_length = ntohs(p_ma_tlv->length);
        tlv_type_idx = tlv_type - XR_MA_TLV_T_OFFSET;

        switch (tlv_type) {
        case FIRST_MCAST_EXT_SEQ:
            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }

            ptr += sizeof(rtcp_xr_ma_tlv_t);
            memcpy(&u32_nl, ptr, sizeof(u32_nl));
            p_xr_ma->first_mcast_ext_seq = ntohl(u32_nl);
            ptr += tlv_length;
            break;

        case SFGMP_JOIN_TIME:
            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }
            ptr += sizeof(rtcp_xr_ma_tlv_t);

            memcpy(&u32_nl, ptr, ma_xr_tl[tlv_type_idx].len);
            p_xr_ma->sfgmp_join_time = ntohl(u32_nl);
            ptr += tlv_length;
            break;
            
        case APP_REQ_TO_RTCP_REQ:
            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }
            ptr += sizeof(rtcp_xr_ma_tlv_t);

            memcpy(&u32_nl, ptr, ma_xr_tl[tlv_type_idx].len);
            p_xr_ma->app_req_to_rtcp_req = ntohl(u32_nl);
            ptr += tlv_length;
            break;
            
        case RTCP_REQ_TO_BURST:
            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }
            ptr += sizeof(rtcp_xr_ma_tlv_t);

            memcpy(&u32_nl, ptr, ma_xr_tl[tlv_type_idx].len);
            p_xr_ma->rtcp_req_to_burst = ntohl(u32_nl);
            ptr += tlv_length;
            break;
            
        case RTCP_REQ_TO_BURST_END:
            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }
            ptr += sizeof(rtcp_xr_ma_tlv_t);

            memcpy(&u32_nl, ptr, ma_xr_tl[tlv_type_idx].len);
            p_xr_ma->rtcp_req_to_burst_end = ntohl(u32_nl);
            ptr += tlv_length;
            break;

        case APP_REQ_TO_MCAST:          
            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }
            ptr += sizeof(rtcp_xr_ma_tlv_t);

            memcpy(&u32_nl, ptr, ma_xr_tl[tlv_type_idx].len);
            p_xr_ma->app_req_to_mcast = ntohl(u32_nl);
            ptr += tlv_length;
            break;

        case APP_REQ_TO_PRES:
            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }
            ptr += sizeof(rtcp_xr_ma_tlv_t);

            memcpy(&u32_nl, ptr, ma_xr_tl[tlv_type_idx].len);
            p_xr_ma->app_req_to_pres = ntohl(u32_nl);
            ptr += tlv_length;
            break;

        case NUM_DUP_PKTS:          
            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }
            ptr += sizeof(rtcp_xr_ma_tlv_t);

            memcpy(&u32_nl, ptr, ma_xr_tl[tlv_type_idx].len);
            p_xr_ma->num_dup_pkts = ntohl(u32_nl);
            ptr += tlv_length;
            break;

        case NUM_GAP_PKTS:          
            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }
            ptr += sizeof(rtcp_xr_ma_tlv_t);

            memcpy(&u32_nl, ptr, ma_xr_tl[tlv_type_idx].len);
            p_xr_ma->num_gap_pkts = ntohl(u32_nl);
            ptr += tlv_length;
            break;

        case TOTAL_CC_TIME:
            /* Vendor Specific TLV, read & validate the enterprise # */
            ent_num = ntohl(p_ma_vs_tlv->ent_num);
            if (ent_num != RTCP_XR_ENTERPRISE_NUM_CISCO) {
                return (FALSE);
            }
            ptr += sizeof(rtcp_xr_ma_vs_tlv_t);

            tlv_length -= sizeof(ent_num);

            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }
            memcpy(&u32_nl, ptr, ma_xr_tl[tlv_type_idx].len);
            p_xr_ma->total_cc_time = ntohl(u32_nl);
            ptr += tlv_length;
            break;

        case RCC_EXPECTED_PTS:
            /* Vendor Specific TLV, read & validate the enterprise # */
            ent_num = ntohl(p_ma_vs_tlv->ent_num);
            if (ent_num != RTCP_XR_ENTERPRISE_NUM_CISCO) {
                return (FALSE);
            }
            ptr += sizeof(rtcp_xr_ma_vs_tlv_t);
            tlv_length -= sizeof(ent_num);

            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }            
            memcpy(&u64_nll, ptr, ma_xr_tl[tlv_type_idx].len);
            p_xr_ma->rcc_expected_pts = ntohll(u64_nll);
            ptr += tlv_length;
            break;

        case RCC_ACTUAL_PTS:
            /* Vendor Specific TLV, read & validate the enterprise # */
            ent_num = ntohl(p_ma_vs_tlv->ent_num);
            if (ent_num != RTCP_XR_ENTERPRISE_NUM_CISCO) {
                return (FALSE);
            }
            ptr += sizeof(rtcp_xr_ma_vs_tlv_t);
            tlv_length -= sizeof(ent_num);

            if (tlv_length < ma_xr_tl[tlv_type_idx].len) {
                return (FALSE);
            }
            memcpy(&u64_nll, ptr, ma_xr_tl[tlv_type_idx].len);
            p_xr_ma->rcc_actual_pts = ntohll(u64_nll);
            ptr += tlv_length;
            break;

        default:
            /* Ignore unknown TLV type, doesn't matter if its vendor-spec
             * because the ent # field is included in tlv_length */
            ptr += sizeof(rtcp_xr_ma_tlv_t) + tlv_length;
            continue;
        }
    }
        
    return (TRUE);
}
