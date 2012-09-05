/*
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vqec_pak.h"

/*
 * Unit tests for vqec_pak
 */

#define INPUT_PAK_SIZE 1500
#define MAX_PAKS_IN_POOL 10000

vqec_pak_t *test_pak = NULL;

//vqec_pak_entry_t *test_pak_hdr = NULL;
//vqec_pak_poolid_t test_pak_hdr_pool;

int test_vqec_pak_init (void) {
    test_malloc_make_fail(0);
    return 0;
}

int test_vqec_pak_clean (void) {
    return 0;
}

static void test_vqec_pak_pool_create (void) {
    vqec_pak_pool_create("Test pak_pool",
                         INPUT_PAK_SIZE,
                         MAX_PAKS_IN_POOL);

    /* pak_hdr_create */
//    test_pak_hdr_pool = vqec_pak_hdr_pool_create("Test pak_hdr_pool",
//                                            MAX_PAKS_IN_POOL);
//    CU_ASSERT_FALSE(test_pak_hdr_pool == NULL);

}

static void test_vqec_pak_alloc_with_particle (void) {
    /* test the error condition when zone_alloc fails */
    test_zone_acquire_make_fail(1);
    test_pak = vqec_pak_alloc_with_particle();
    test_zone_acquire_make_fail(0);
    CU_ASSERT(test_pak == NULL);

    /* now run it normally */
    test_pak = vqec_pak_alloc_with_particle();
    CU_ASSERT_FALSE(test_pak == NULL);
    CU_ASSERT(test_pak->ref_count == 1);
    CU_ASSERT(test_pak->zone_ptr != NULL);

    /* hdr_pak_pool_alloc */
    /* test the error condition when zone_alloc fails */
//    test_zone_acquire_make_fail(1);
//    test_pak_hdr = vqec_pak_hdr_pool_alloc(test_pak_hdr_pool);
//    test_zone_acquire_make_fail(0);
//    CU_ASSERT(test_pak_hdr == NULL);

    /* now run it normally */
//    test_pak_hdr = vqec_pak_hdr_pool_alloc(test_pak_hdr_pool);
//    CU_ASSERT_FALSE(test_pak_hdr == NULL);
//    CU_ASSERT(test_pak_hdr->pool == test_pak_hdr_pool);

}

static void test_vqec_pak_get_buffer (void) {

}

static void test_vqec_pak_get_content_len (void) {
    uint32_t test_len = 23456;
    uint32_t got_len = 0;
    test_pak->buff_len = test_len;
    got_len = vqec_pak_get_content_len(test_pak);
    CU_ASSERT(got_len == test_len);
}

static void test_vqec_pak_get_rtp_hdr (void) {
    rtpfasttype_t *test_rtp = (rtpfasttype_t *)12345;
    rtpfasttype_t *got_rtp;
    test_pak->rtp = test_rtp;
    got_rtp = vqec_pak_get_rtp_hdr(test_pak);
    CU_ASSERT_PTR_EQUAL(got_rtp,test_rtp);
}

static void test_vqec_pak_ref (void) {
    CU_ASSERT(test_pak->ref_count == 1);
    vqec_pak_ref((vqec_pak_t *)test_pak);
    CU_ASSERT(test_pak->ref_count == 2);
    vqec_pak_ref((vqec_pak_t *)test_pak);
    CU_ASSERT(test_pak->ref_count == 3);
}

static void test_vqec_pak_free (void) {
    CU_ASSERT(test_pak->ref_count == 3);
    vqec_pak_free((vqec_pak_t *)test_pak);
    CU_ASSERT(test_pak->ref_count == 2);
    vqec_pak_free((vqec_pak_t *)test_pak);
    CU_ASSERT(test_pak->ref_count == 1);
    vqec_pak_free((vqec_pak_t *)test_pak);
    CU_ASSERT(test_pak->ref_count == 0);

    /* pak_hdr_free */
//    vqec_pak_hdr_pool_free(test_pak_hdr);
}

static void test_vqec_pak_pool_destroy (void) {
    vqec_pak_pool_destroy();
}

CU_TestInfo test_array_pak[] = {
    {"test vqec_pak_pool_create",test_vqec_pak_pool_create},
    {"test vqec_pak_alloc_with_particle",test_vqec_pak_alloc_with_particle},
    {"test vqec_pak_get_buffer",test_vqec_pak_get_buffer},
    {"test vqec_pak_get_content_len",test_vqec_pak_get_content_len},
    {"test vqec_pak_get_rtp_hdr",test_vqec_pak_get_rtp_hdr},
    {"test vqec_pak_ref",test_vqec_pak_ref},
    {"test vqec_pak_free",test_vqec_pak_free},
    {"test vqec_pak_pool_destroy",test_vqec_pak_pool_destroy},
    CU_TEST_INFO_NULL,
};

