/*------------------------------------------------------------------
 * VQEC -- Generalized sequence numbers for VQEC packets
 *
 * April 2006, Josh Gahm
 *
 * Copyright (c) 2006-2008, 2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */


#ifndef __VQEC_SEQ_NUM_H__
#define __VQEC_SEQ_NUM_H__

#include "../include/utils/vam_types.h"

typedef uint32_t vqec_seq_num_t;

/* 
 * vqec_seq_num_OP
 * 
 * return TRUE iff s0 OP s1
 */
static inline boolean vqec_seq_num_lt(vqec_seq_num_t s0, vqec_seq_num_t s1)
{
    return ((s0 - s1) >> 31);
}

static inline boolean vqec_seq_num_gt(vqec_seq_num_t s0, vqec_seq_num_t s1)
{
    return vqec_seq_num_lt(s1,s0);
}

static inline boolean vqec_seq_num_eq(vqec_seq_num_t s0, vqec_seq_num_t s1)
{
    return s0 == s1;
}

static inline boolean vqec_seq_num_ge(vqec_seq_num_t s0, vqec_seq_num_t s1)
{
    return (s0 == s1) || vqec_seq_num_gt(s0,s1);
}

static inline boolean vqec_seq_num_le(vqec_seq_num_t s0, vqec_seq_num_t s1)
{
    return (s0 == s1) || vqec_seq_num_lt(s0,s1);
}

static inline boolean vqec_seq_num_ne(vqec_seq_num_t s0, vqec_seq_num_t s1)
{
    return s0 != s1;
}

static inline int32_t vqec_seq_num_add(vqec_seq_num_t s0, vqec_seq_num_t s1)
{
    return (int32_t) (s0 + s1);
}

static inline int32_t vqec_seq_num_sub(vqec_seq_num_t s0, vqec_seq_num_t s1)
{
    return (int32_t) (s0 - s1);
}

static inline vqec_seq_num_t vqec_next_seq_num (vqec_seq_num_t seq_num) 
{
    return (int32_t) (seq_num + 1);
}

static inline vqec_seq_num_t vqec_pre_seq_num (vqec_seq_num_t seq_num) 
{
    return (int32_t) (seq_num - 1);
}

static inline vqec_seq_num_t 
vqec_seq_num_nearest_to_rtp_seq_num(vqec_seq_num_t s, uint16_t rtp_seq_num)
{
    return s + ((int16_t) (rtp_seq_num - (s & 0xffff)));
}
    
/**
 * Maps a vqec_seq_num_t to a session-based RTP sequence number.
 * 
 * The session-based RTP session number is a sequence number space
 * that is common to all sources of a session (each of which may use
 * independent sequence number spaces).
 *
 * When a session sequence number from this space is communicated
 * outside of VQE-C, care must be taken to map it back to an associated
 * source-specific RTP sequence space as appropriate (e.g. RTCP reports,
 * CLI, etc.).
 *
 * @param[in]  s        - PCM-based 32-bit sequence number
 * @param[out] uint16_t - session-based 16-bit RTP sequence number
 */
static inline uint16_t vqec_seq_num_to_rtp_seq_num(vqec_seq_num_t s) 
{
    return s & 0xffff;
}
    
/**@brief
 * RTP sequence number bitmask (low 16 bits) */
#define VQEC_SEQ_NUM_RTP_BIT_MASK     0x0000ffff

/**@brief
 * RTP sequence number discontinuity indication flag */
#define VQEC_SEQ_NUM_RTP_DISCONT_IND  0x00030000

static inline vqec_seq_num_t 
vqec_seq_num_mark_rtp_seq_num_discont (vqec_seq_num_t s)
{
    return ((s & ~VQEC_SEQ_NUM_RTP_BIT_MASK) + VQEC_SEQ_NUM_RTP_DISCONT_IND);
}

/**@brief
 * Bump the 32-bit sequence number up a few generations of 16-bit seq_nums. */
static inline vqec_seq_num_t vqec_seq_num_bump_seq (vqec_seq_num_t s)
{
    return (s + VQEC_SEQ_NUM_RTP_DISCONT_IND);
}


/*
 * The sequence number functions below operate on 16-bit RTP sequence
 * numbers (uint16_t), not 32-bit sequence numbers (vqec_seq_num_t).
 */
static inline uint16_t vqec_next_rtp_seq_num (uint16_t rtp_seq_num) 
{
    return (uint16_t) (rtp_seq_num + 1);
}

static inline uint16_t vqec_pre_rtp_seq_num (uint16_t rtp_seq_num) 
{
    return (uint16_t) (rtp_seq_num - 1);
}

static inline boolean vqec_rtp_seq_num_lt(uint16_t s0, uint16_t s1)
{
    return (((uint16_t)(s0 - s1)) >> 15);
}

static inline boolean vqec_rtp_seq_num_gt(uint16_t s0, uint16_t s1)
{
    return vqec_rtp_seq_num_lt(s1,s0);
}

#endif /*  __VQEC_SEQ_NUM_H__ */
