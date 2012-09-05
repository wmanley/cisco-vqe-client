/*
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vqec_seq_num.h"

/*
 * Unit tests for vqec_seq_num
 */

boolean result;

int test_vqec_seq_num_init (void) {
    test_malloc_make_fail(0);
    return 0;
}

int test_vqec_seq_num_clean (void) {
    return 0;
}

static void test_vqec_seq_num_lt (void) {
    result = vqec_seq_num_lt(5,4);
    CU_ASSERT_FALSE(result);
    result = vqec_seq_num_lt(5,5);
    CU_ASSERT_FALSE(result);
    result = vqec_seq_num_lt(5,6);
    CU_ASSERT(result);
    result = vqec_seq_num_lt(0xb,0x8000000b);
    CU_ASSERT(result);
    result = vqec_seq_num_lt(0xb,0x8000000c);
    CU_ASSERT_FALSE(result);
}

static void test_vqec_seq_num_gt (void) {
    result = vqec_seq_num_gt(5,4);
    CU_ASSERT(result);
    result = vqec_seq_num_gt(5,5);
    CU_ASSERT_FALSE(result);
    result = vqec_seq_num_gt(5,6);
    CU_ASSERT_FALSE(result);
    result = vqec_seq_num_gt(0xb,0x8000000b);
    CU_ASSERT(result);
    result = vqec_seq_num_gt(0xb,0x8000000c);
    CU_ASSERT(result);
}

static void test_vqec_seq_num_eq (void) {
    result = vqec_seq_num_eq(5,4);
    CU_ASSERT_FALSE(result);
    result = vqec_seq_num_eq(5,5);
    CU_ASSERT(result);
    result = vqec_seq_num_eq(5,6);
    CU_ASSERT_FALSE(result);
    result = vqec_seq_num_eq(0xb,0x8000000b);
    CU_ASSERT_FALSE(result);
    result = vqec_seq_num_eq(0xb,0x8000000c);
    CU_ASSERT_FALSE(result);
}

static void test_vqec_seq_num_ge (void) {
    result = vqec_seq_num_ge(5,4);
    CU_ASSERT(result);
    result = vqec_seq_num_ge(5,5);
    CU_ASSERT(result);
    result = vqec_seq_num_ge(5,6);
    CU_ASSERT_FALSE(result);
    result = vqec_seq_num_ge(0xb,0x8000000b);
    CU_ASSERT(result);
    result = vqec_seq_num_ge(0xb,0x8000000c);
    CU_ASSERT(result);
}

static void test_vqec_seq_num_le (void) {
    result = vqec_seq_num_le(5,4);
    CU_ASSERT_FALSE(result);
    result = vqec_seq_num_le(5,5);
    CU_ASSERT(result);
    result = vqec_seq_num_le(5,6);
    CU_ASSERT(result);
    result = vqec_seq_num_le(0xb,0x8000000b);
    CU_ASSERT(result);
    result = vqec_seq_num_le(0xb,0x8000000c);
    CU_ASSERT_FALSE(result);
}

static void test_vqec_seq_num_ne (void) {
    result = vqec_seq_num_ne(5,4);
    CU_ASSERT(result);
    result = vqec_seq_num_ne(5,5);
    CU_ASSERT_FALSE(result);
    result = vqec_seq_num_ne(5,6);
    CU_ASSERT(result);
    result = vqec_seq_num_ne(0xb,0x8000000b);
    CU_ASSERT(result);
    result = vqec_seq_num_ne(0xb,0x8000000c);
    CU_ASSERT(result);
}

static void test_vqec_seq_num_sub (void) {
    CU_ASSERT(vqec_seq_num_sub(5,4) == (5 - 4));
    CU_ASSERT(vqec_seq_num_sub(5,5) == (5 - 5));
    CU_ASSERT(vqec_seq_num_sub(5,6) == (5 - 6));
    CU_ASSERT(vqec_seq_num_sub(0xb,0x8000000b) == (0xb - 0x8000000b));
    CU_ASSERT(vqec_seq_num_sub(0xb,0x8000000c) == (0xb - 0x8000000c));
}

static void test_vqec_seq_num_nearest_to_rtp_seq_num (void) {
    CU_ASSERT(vqec_seq_num_nearest_to_rtp_seq_num(0x12340000,0x25) == 0x12340025);
    CU_ASSERT(vqec_seq_num_nearest_to_rtp_seq_num(0x12348024,0x25) == 0x12340025);
    CU_ASSERT(vqec_seq_num_nearest_to_rtp_seq_num(0x12348025,0x25) == 0x12340025);
    CU_ASSERT(vqec_seq_num_nearest_to_rtp_seq_num(0x12348026,0x25) == 0x12350025);
    CU_ASSERT(vqec_seq_num_nearest_to_rtp_seq_num(0xffff8025,0x25) == 0xffff0025);
    CU_ASSERT(vqec_seq_num_nearest_to_rtp_seq_num(0xffff8026,0x25) == 0x25);
    CU_ASSERT(vqec_seq_num_nearest_to_rtp_seq_num(0x2,0xdddd) == 0xffffdddd);
}
    
static void test_vqec_seq_num_to_rtp_seq_num (void) {
    CU_ASSERT(vqec_seq_num_to_rtp_seq_num(0x12340000) == 0x0);
    CU_ASSERT(vqec_seq_num_to_rtp_seq_num(0x12340024) == 0x24);
    CU_ASSERT(vqec_seq_num_to_rtp_seq_num(0x12340025) == 0x25);
    CU_ASSERT(vqec_seq_num_to_rtp_seq_num(0x12340026) == 0x26);
    CU_ASSERT(vqec_seq_num_to_rtp_seq_num(0x1234fd) == 0x34fd);
    CU_ASSERT(vqec_seq_num_to_rtp_seq_num(0x0) == 0x0);
    CU_ASSERT(vqec_seq_num_to_rtp_seq_num(0xffffffff) == 0xffff);
}

CU_TestInfo test_array_seq_num[] = {
    {"test vqec_seq_num_lt",test_vqec_seq_num_lt},
    {"test vqec_seq_num_gt",test_vqec_seq_num_gt},
    {"test vqec_seq_num_eq",test_vqec_seq_num_eq},
    {"test vqec_seq_num_ge",test_vqec_seq_num_ge},
    {"test vqec_seq_num_le",test_vqec_seq_num_le},
    {"test vqec_seq_num_ne",test_vqec_seq_num_ne},
    {"test vqec_seq_num_sub",test_vqec_seq_num_sub},
    {"test vqec_seq_num_nearest_to_rtp_seq_num",test_vqec_seq_num_nearest_to_rtp_seq_num},
    {"test vqec_seq_num_to_rtp_seq_num",test_vqec_seq_num_to_rtp_seq_num},
    CU_TEST_INFO_NULL,
};

