/*
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vqec_pak_seq.h"

/*
 * Unit tests for vqec_pak_seq
 */

/* BITS_PER_BUCKET must be at least 7 for constants in below tests to work */
#define BITS_PER_BUCKET 16
#define INPUT_PAK_SIZE 1500
#define MAX_PAKS_IN_POOL 10000

vqec_pak_seq_t *test_pak_seq;
vqec_pak_t *pak1, *pak2, *pak3, *pak4, *pak5, *pak6;

int test_vqec_pak_seq_init (void) {
    test_malloc_make_fail(0);
    vqec_pak_pool_create("Test pak_seq pak_pool",
                         INPUT_PAK_SIZE,
                         MAX_PAKS_IN_POOL);
    pak1 = vqec_pak_alloc_with_particle();
    pak2 = vqec_pak_alloc_with_particle();
    pak3 = vqec_pak_alloc_with_particle();
    pak4 = vqec_pak_alloc_with_particle();
    pak5 = vqec_pak_alloc_with_particle();
    pak6 = vqec_pak_alloc_with_particle();

    return 0;
}

int test_vqec_pak_seq_clean (void) {
    vqec_pak_pool_destroy();
    return 0;
}

static void test_vqec_pak_seq_create (void) {
    /* max bucket bits should be 16 */
    test_pak_seq = vqec_pak_seq_create(31);
    CU_ASSERT(test_pak_seq == NULL);
    test_pak_seq = vqec_pak_seq_create(17);
    CU_ASSERT(test_pak_seq == NULL);
    test_pak_seq = vqec_pak_seq_create(0);
    CU_ASSERT(test_pak_seq == NULL);

    /* make malloc of pak_seq fail */
    test_malloc_make_fail(1);
    test_pak_seq = vqec_pak_seq_create(16);
    test_malloc_make_fail(0);
    CU_ASSERT(test_pak_seq == NULL);

    /* now make a real one, and check that it works */
    test_malloc_make_cache(1);
    test_pak_seq = vqec_pak_seq_create(BITS_PER_BUCKET);
    test_malloc_make_cache(0);
    CU_ASSERT(test_pak_seq != NULL);
    CU_ASSERT(test_pak_seq->bucket_mask == (1 << BITS_PER_BUCKET) - 1);
    CU_ASSERT(test_pak_seq->bucket_bits == BITS_PER_BUCKET);
    CU_ASSERT(test_pak_seq->num_buckets == 1 << BITS_PER_BUCKET);
}

static void test_vqec_pak_seq_get_bucket_bits (void) {
    uint8_t num_bits;
    num_bits = vqec_pak_seq_get_bucket_bits(test_pak_seq);
    CU_ASSERT(num_bits == BITS_PER_BUCKET);
}

static void test_vqec_pak_seq_insert (void) {
    boolean test_status = 0;
    //pak1 has no set seq, so should default to 0
    pak2->seq_num = 55;
    pak3->seq_num = 74;
    pak4->seq_num = 55 + 
        (1 << BITS_PER_BUCKET);  //same bucket as pak2, should not replace it
    pak5->seq_num = 83;
    pak6->seq_num = 74;  //duplicate of pak3, should fail
    test_status = vqec_pak_seq_insert(test_pak_seq,pak1);
    CU_ASSERT(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 1);
    test_status = vqec_pak_seq_insert(test_pak_seq,pak2);
    CU_ASSERT(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 2);
    test_status = vqec_pak_seq_insert(test_pak_seq,pak3);
    CU_ASSERT(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 3);
    test_status = vqec_pak_seq_insert(test_pak_seq,pak4);
    CU_ASSERT_FALSE(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 3);
    test_status = vqec_pak_seq_insert(test_pak_seq,pak5);
    CU_ASSERT(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 4);
    CU_ASSERT(test_pak_seq->stats.num_dups == 0);
    test_status = vqec_pak_seq_insert(test_pak_seq,pak6);
    CU_ASSERT_FALSE(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 4);
    CU_ASSERT(test_pak_seq->stats.num_dups == 1);

    /* make sure the pak_seq looks how we'd expect it to */
    CU_ASSERT_PTR_EQUAL(test_pak_seq->buckets[0].pak, pak1);
    CU_ASSERT_EQUAL(test_pak_seq->buckets[0].seq_num, 0); 
    CU_ASSERT_PTR_EQUAL(test_pak_seq->buckets[55].pak, pak2);
    CU_ASSERT_EQUAL(test_pak_seq->buckets[55].seq_num, 55);
    CU_ASSERT_PTR_EQUAL(test_pak_seq->buckets[74].pak, pak3);
    CU_ASSERT_EQUAL(test_pak_seq->buckets[74].seq_num, 74);
    CU_ASSERT_PTR_EQUAL(test_pak_seq->buckets[83].pak, pak5);
    CU_ASSERT_EQUAL(test_pak_seq->buckets[83].seq_num, 83);
}

static void test_vqec_pak_seq_find (void) {
    vqec_pak_t *found_pak;
    found_pak = vqec_pak_seq_find(test_pak_seq,0);
    CU_ASSERT_PTR_EQUAL(found_pak, pak1);
    found_pak = vqec_pak_seq_find(test_pak_seq,55);
    CU_ASSERT_PTR_EQUAL(found_pak, pak2);
    found_pak = vqec_pak_seq_find(test_pak_seq,74);
    CU_ASSERT_PTR_EQUAL(found_pak, pak3);
    found_pak = vqec_pak_seq_find(test_pak_seq,(55 + (1 << BITS_PER_BUCKET)));
    CU_ASSERT_PTR_EQUAL(found_pak, NULL);
    found_pak = vqec_pak_seq_find(test_pak_seq,83);
    CU_ASSERT_PTR_EQUAL(found_pak, pak5);
    found_pak = vqec_pak_seq_find(test_pak_seq,22);
    CU_ASSERT_PTR_EQUAL(found_pak, NULL);
}

static void test_vqec_pak_seq_delete (void) {
    boolean test_status = 0;
    test_status = vqec_pak_seq_delete(test_pak_seq, 5);
    CU_ASSERT_FALSE(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 4);
    test_status = vqec_pak_seq_delete(test_pak_seq, 0);
    CU_ASSERT(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 3);
    test_status = vqec_pak_seq_delete(test_pak_seq, 55);
    CU_ASSERT(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 2);
    test_status = vqec_pak_seq_delete(test_pak_seq, 74);
    CU_ASSERT(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 1);
    test_status = vqec_pak_seq_delete(test_pak_seq,
                                      (55 + (1 << BITS_PER_BUCKET)));
    CU_ASSERT_FALSE(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 1);
    test_status = vqec_pak_seq_delete(test_pak_seq, 83);
    CU_ASSERT(test_status);
    CU_ASSERT(test_pak_seq->num_paks == 0);
}

static void test_vqec_pak_seq_destroy (void) {

    test_malloc_make_cache(1);
    vqec_pak_seq_destroy(test_pak_seq);
    test_malloc_make_cache(0);
    CU_ASSERT(diff_malloc_free() == 0)
}


CU_TestInfo test_array_pak_seq[] = {
    {"test vqec_pak_seq_create",test_vqec_pak_seq_create},
    {"test vqec_pak_seq_get_bucket_bits",test_vqec_pak_seq_get_bucket_bits},
    {"test vqec_pak_seq_insert",test_vqec_pak_seq_insert},
    {"test vqec_pak_seq_find",test_vqec_pak_seq_find},
    {"test vqec_pak_seq_delete",test_vqec_pak_seq_delete},
    {"test vqec_pak_seq_destroy",test_vqec_pak_seq_destroy},
    CU_TEST_INFO_NULL,
};



