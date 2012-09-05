/*------------------------------------------------------------------
 * VQEC -- Sequence of packets
 *
 * February 2006, Josh Gahm
 *
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */


#ifndef __VQEC_PAK_SEQ_H__
#define __VQEC_PAK_SEQ_H__

#include <utils/vam_types.h>
#include "vqec_pak.h"

#define VQEC_PAK_SEQ_MAX_BUCKET_BITS 16

#define INVALID_SEQ_NUM_FOR_BUCKET(bucket) ((bucket) ? 0 : 1)


typedef struct vqec_pak_seq_stats_ {
    uint32_t num_dups;
} vqec_pak_seq_stats_t;

typedef struct vqec_pak_seq_bucket_ {
    vqec_pak_t * pak;
    vqec_seq_num_t seq_num;
} vqec_pak_seq_bucket_t;

typedef struct vqec_pak_seq_ {
    uint32_t bucket_mask; /* mask to find bucket for a seq num */
    uint32_t num_buckets;
    uint32_t num_paks;
    uint8_t bucket_bits;
    uint8_t pad[3];
    vqec_pak_seq_stats_t stats;
    vqec_pak_seq_bucket_t buckets[0];
} vqec_pak_seq_t;

typedef struct vqe_zone vqec_pak_seq_pool_t;

static inline uint8_t vqec_pak_seq_get_bucket_bits(const vqec_pak_seq_t * seq)
{
    return seq->bucket_bits;
}


vqec_pak_seq_pool_t * vqec_pak_seq_pool_create(
                                char * pool_name, 
                                uint32_t max_size, 
                                uint8_t bucket_bits);

void vqec_pak_seq_pool_destroy(vqec_pak_seq_pool_t * pool);


vqec_pak_seq_t * vqec_pak_seq_create(uint8_t bucket_bits);
void vqec_pak_seq_destroy(vqec_pak_seq_t * seq);

vqec_pak_seq_t * vqec_pak_seq_create_in_pool(vqec_pak_seq_pool_t * pool,
                                             uint8_t bucket_bits);
void vqec_pak_seq_destroy_in_pool(vqec_pak_seq_pool_t * pool,
                                  vqec_pak_seq_t * seq);


/* vqec_pak_seq_insert
 *
 * Insert a packet into the sequence and return true unless the packet
 * was a duplicate.  Frees the new packet if the packet was a dup,
 * else may free an older packet.
 */
boolean vqec_pak_seq_insert(vqec_pak_seq_t * seq,
                          vqec_pak_t * pak);

/*
 * vqec_pak_seq_find
 *
 * Return a pointer to a packet that is within a sequence, or NULL if the pak
 * cannot be found.  
 */

vqec_pak_t * vqec_pak_seq_find(const vqec_pak_seq_t * seq,
                           vqec_seq_num_t seq_num);

/*
 * vqec_pak_seq_delete
 *
 * Delete a pak from the sequence by sequence number.
 * Returns TRUE if the packet was found.
 */

boolean vqec_pak_seq_delete(vqec_pak_seq_t * seq,
                            vqec_seq_num_t seq_num);

static inline uint32_t vqec_pak_seq_find_bucket (const vqec_pak_seq_t * seq,
                                                 vqec_seq_num_t seq_num) 
{
    return seq_num & seq->bucket_mask;
}

static inline uint32_t vqec_pak_seq_get_num_paks (const vqec_pak_seq_t * seq) 
{
    return seq->num_paks;
}

static inline uint32_t 
vqec_pak_seq_get_num_buckets (const vqec_pak_seq_t * seq) 
{
    return seq->num_buckets;
}

#endif /*  __VQEC_PAK_SEQ_H__ */
