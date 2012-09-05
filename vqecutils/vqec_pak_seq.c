/*------------------------------------------------------------------
 * VQEC Cache Manager -- Sequence of packets
 *
 * April 2006, Josh Gahm
 *
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

/* included for unit testing purposes */
#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif

#include "vqec_pak_seq.h"
#include "vqec_debug.h"
#include <utils/zone_mgr.h>

vqec_pak_seq_pool_t * vqec_pak_seq_pool_create(
                                char * pool_name,
                                uint32_t max_size,
                                uint8_t bucket_bits) 
{
    uint32_t num_buckets = 1 << bucket_bits;
    uint32_t seq_size = sizeof(vqec_pak_seq_t) +
        sizeof(vqec_pak_seq_bucket_t)*(num_buckets);

    return zone_instance_get_loc(pool_name,
                                 O_CREAT,
                                 seq_size,
                                 max_size,
                                 NULL, NULL);
}

void vqec_pak_seq_pool_destroy(vqec_pak_seq_pool_t * pool)
{
    if (pool) {
        (void) zone_instance_put(pool);
    }
}

vqec_pak_seq_t * vqec_pak_seq_create(uint8_t bucket_bits)
{
    return vqec_pak_seq_create_in_pool(NULL, bucket_bits);
}

vqec_pak_seq_t * vqec_pak_seq_create_in_pool(vqec_pak_seq_pool_t * pool,
                                             uint8_t bucket_bits)
{
    uint32_t num_buckets = 1 << bucket_bits;
    uint32_t seq_size = sizeof(vqec_pak_seq_t) + 
        sizeof(vqec_pak_seq_bucket_t)*(num_buckets);
    vqec_pak_seq_t * seq;
    int bucket_num;

    if (bucket_bits == 0 || bucket_bits > VQEC_PAK_SEQ_MAX_BUCKET_BITS) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return NULL;
    }

    if (pool) {
        seq = (vqec_pak_seq_t *) zone_acquire(pool);
    } else {
        seq  = (void *) VQE_MALLOC(seq_size);
    }
    if (! seq) {
        syslog_print(VQEC_MALLOC_FAILURE, "vqec_pak_seq_create::malloc failed");
        return NULL;
    } 

    memset(seq, 0, seq_size);

    seq->bucket_mask = num_buckets - 1;
    seq->bucket_bits = bucket_bits;
    seq->num_buckets = num_buckets;
    
    /* Set the seq_num associated with each bucket so that it will
     * never match anything associated with that bucket.
     */

    for (bucket_num = 0;bucket_num < num_buckets; bucket_num++)
        seq->buckets[bucket_num].seq_num = 
            INVALID_SEQ_NUM_FOR_BUCKET(bucket_num);

    return seq;
}

void vqec_pak_seq_destroy(vqec_pak_seq_t * seq)
{
    vqec_pak_seq_destroy_in_pool(NULL, seq);
}

void vqec_pak_seq_destroy_in_pool(vqec_pak_seq_pool_t * pool,
                                  vqec_pak_seq_t * seq)
{
    int bucket_num;    
    for (bucket_num = 0;bucket_num < seq->num_buckets; bucket_num++) {
        if (seq->buckets[bucket_num].pak) {
            /* mark the packet invalid with one atomic write *before*
             messing with its contents at all.  That way a snooper can
             tell if a packet was valid up until any particular time
             by reading the sequence number associated with it in the
             seq buffer.
            */
             seq->buckets[bucket_num].seq_num = 
                 INVALID_SEQ_NUM_FOR_BUCKET(bucket_num);
             vqec_pak_free(seq->buckets[bucket_num].pak);
             seq->buckets[bucket_num].pak = NULL;
             seq->num_paks--;             
        }
    }
    ASSERT(seq->num_paks == 0, "pak sequence not empty");
    if (pool) {
        zone_release(pool, seq);
    } else {
        VQE_FREE(seq);
    }
}

boolean vqec_pak_seq_insert(vqec_pak_seq_t * seq,
                          vqec_pak_t * pak)
{
    uint32_t bucket_num = vqec_pak_seq_find_bucket(seq, pak->seq_num);
    vqec_pak_t * old_pak = seq->buckets[bucket_num].pak;
    boolean success = TRUE;
    
    if (old_pak && (pak->seq_num != seq->buckets[bucket_num].seq_num)) {
        /* Unlike vam, we don't want new packet blindly replace old packet */
        success = FALSE;
    } else if (old_pak) {
        /* duplicate */
        seq->stats.num_dups++;
        success = FALSE;
    } else {
        /* position was empty */
         seq->buckets[bucket_num].pak = pak;       
         seq->buckets[bucket_num].seq_num = pak->seq_num;
         seq->num_paks++;
         vqec_pak_ref(pak);
    }       

    return success;
}

vqec_pak_t * vqec_pak_seq_find(const vqec_pak_seq_t * seq,
                           vqec_seq_num_t seq_num)
{
    uint32_t bucket_num = vqec_pak_seq_find_bucket(seq, seq_num);

    if (seq->buckets[bucket_num].seq_num == seq_num)
        return seq->buckets[bucket_num].pak;

    return NULL;
}

boolean vqec_pak_seq_delete(vqec_pak_seq_t * seq,
                          vqec_seq_num_t seq_num)
{
    uint32_t bucket_num = vqec_pak_seq_find_bucket(seq, seq_num);

    if (seq->buckets[bucket_num].seq_num == seq_num) {
        vqec_pak_free(seq->buckets[bucket_num].pak);
        seq->buckets[bucket_num].seq_num = 
            INVALID_SEQ_NUM_FOR_BUCKET(bucket_num);
        seq->buckets[bucket_num].pak = NULL;
        seq->num_paks--;
        return TRUE;
    }
    
    return FALSE;
}

