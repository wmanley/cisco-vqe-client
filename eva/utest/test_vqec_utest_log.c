/*
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vqec_log.h"

/*
 * Unit tests for vqec_pak
 */

vqec_log_t *testlog;
abs_time_t test_time;

int test_vqec_log_init (void) {
    test_malloc_make_fail(0);
    test_malloc_make_cache(0);
    testlog = malloc(sizeof(vqec_log_t));
    vqec_log_init(testlog);
    return 0;
}

int test_vqec_log_clean (void) {
    vqec_log_clear(testlog);
    return 0;
}

static void test_vqec_log_insert_repair_seq (void)
{
    vqec_log_insert_repair_seq(testlog, 3445);
    /* manual check - okay */
    vqec_log_dump(testlog, FALSE);
}

static void test_vqec_log_insert_input_gap (void)
{
    vqec_log_insert_input_gap(testlog, 5546, 6000);
    /* manual check - okay */
    vqec_log_dump(testlog, FALSE);
}

static void test_vqec_log_times (void)
{
    test_time = get_sys_time();
    vqec_log_proxy_join_time(testlog, test_time);
    CU_ASSERT(abs_time_eq(testlog->proxy.join_time,test_time));

    test_time = get_sys_time();
    vqec_log_vam_desired_join_time(testlog, test_time);
    CU_ASSERT(abs_time_eq(testlog->vam.desired_join_time, test_time));

    test_time = get_sys_time();
    vqec_log_vam_actual_join_time(testlog, test_time);
    CU_ASSERT(abs_time_eq(testlog->vam.actual_join_time, test_time));
}

CU_TestInfo test_array_log[] = {
    {"test vqec_log_insert_repair_seq",test_vqec_log_insert_repair_seq},
    {"test vqec_log_insert_input_gap",test_vqec_log_insert_input_gap},
    {"test vqec_log_times",test_vqec_log_times},
    CU_TEST_INFO_NULL,
};

