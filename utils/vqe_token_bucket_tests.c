/*
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "../add-ons/include/CUnit/Basic.h"

#include "vqe_token_bucket.h"
#include "limits.h"

#define MICROSECONDS_PER_SECOND 1000000

void
test_tb_init (void)
{
    token_bucket_info_t tb;

    /* invalid token bucket */
    CU_ASSERT(tb_init(NULL, 1, 1, 1, ABS_TIME_0) == TB_RETVAL_TB_INVALID);
    CU_ASSERT(tb_init_simple(NULL, 1, 1) == TB_RETVAL_TB_INVALID);

    /* zero rate */
    CU_ASSERT(tb_init(&tb, 0, 1, 1, ABS_TIME_0) == TB_RETVAL_OK);
    CU_ASSERT(tb_init_simple(&tb, 0, 1) == TB_RETVAL_OK);
    /* invalid rate */
    CU_ASSERT(tb_init(&tb, TB_RATE_MAX+1, 1, 1, ABS_TIME_0) 
              == TB_RETVAL_RATE_INVALID);
    CU_ASSERT(tb_init_simple(&tb, TB_RATE_MAX+1, 1) == TB_RETVAL_RATE_INVALID);

    /* zero burst */
    CU_ASSERT(tb_init(&tb, 1, 0, 1, ABS_TIME_0) == TB_RETVAL_OK);
    CU_ASSERT(tb_init_simple(&tb, 1, 0) == TB_RETVAL_OK);
    CU_ASSERT(tb_init(&tb, 0, 0, 1, ABS_TIME_0) == TB_RETVAL_OK);
    CU_ASSERT(tb_init_simple(&tb, 0, 0) == TB_RETVAL_OK);
    /* invalid burst */
    CU_ASSERT(tb_init(&tb, 1, TB_BURST_MAX+1, 1, ABS_TIME_0) == 
              TB_RETVAL_BURST_INVALID);
    CU_ASSERT(tb_init_simple(&tb, 1, TB_BURST_MAX+1) ==
              TB_RETVAL_BURST_INVALID);

    /* invalid quantum1 */
    CU_ASSERT(tb_init(&tb, 1, 1, 0, ABS_TIME_0) == TB_RETVAL_QUANTUM_INVALID);
    /* invalid quantum2 */
    CU_ASSERT(tb_init(&tb, 1, 1, TB_QUANTUM_MAX+1, ABS_TIME_0) 
              == TB_RETVAL_QUANTUM_INVALID);

    /* valid values */
    CU_ASSERT(tb_init(&tb, 1, 1, 1, ABS_TIME_0) == TB_RETVAL_OK);
    CU_ASSERT(tb_init_simple(&tb, 1, 1) == TB_RETVAL_OK);
}

void
test_tb_retval_to_str (void)
{
    CU_ASSERT((int)tb_retval_to_str(TB_RETVAL_OK-1));
    CU_ASSERT((int)tb_retval_to_str(TB_RETVAL_OK));
    CU_ASSERT((int)tb_retval_to_str(TB_RETVAL_TB_INVALID));
    CU_ASSERT((int)tb_retval_to_str(TB_RETVAL_INTERNAL_ERROR));
    CU_ASSERT((int)tb_retval_to_str(TB_RETVAL_RATE_INVALID));
    CU_ASSERT((int)tb_retval_to_str(TB_RETVAL_BURST_INVALID));
    CU_ASSERT((int)tb_retval_to_str(TB_RETVAL_QUANTUM_INVALID));
    CU_ASSERT((int)tb_retval_to_str(TB_RETVAL_INSUFFICIENT_TOKENS));
}

void
test_tb_get_properties (void)
{
    token_bucket_info_t tb0, tb1, tb2, tb3;
    uint32_t rate, burst, quantum_size, tokens_in_bucket;

    tb_init(&tb0,  0, 11, 22, ABS_TIME_0);
    tb_init(&tb1, 10, 20, 30, ABS_TIME_0);
    tb_init(&tb2, 40, 50, 60, ABS_TIME_0);
    tb_init(&tb3,  0,  0, 44, ABS_TIME_0);

    CU_ASSERT(tb_get_properties(NULL, &rate, &burst, &quantum_size, 
                                &tokens_in_bucket)
              == TB_RETVAL_TB_INVALID);
    
    tb_get_properties(&tb0, &rate, &burst, &quantum_size, &tokens_in_bucket);
    CU_ASSERT(rate == 0);
    CU_ASSERT(burst == 11);
    CU_ASSERT(quantum_size == 22);
    CU_ASSERT(tokens_in_bucket == 11);

    tb_get_properties(&tb1, &rate, &burst, &quantum_size, &tokens_in_bucket);
    CU_ASSERT(rate == 10);
    CU_ASSERT(burst == 20);
    CU_ASSERT(quantum_size == 30);
    CU_ASSERT(tokens_in_bucket == 20);
    
    tb_get_properties(&tb2, &rate, &burst, &quantum_size, &tokens_in_bucket);
    CU_ASSERT(rate == 40);
    CU_ASSERT(burst == 50);
    CU_ASSERT(quantum_size == 60);
    CU_ASSERT(tokens_in_bucket == 50);

    tb_get_properties(&tb3, &rate, &burst, &quantum_size, &tokens_in_bucket);
    CU_ASSERT(rate == 0);
    CU_ASSERT(burst == 0);
    CU_ASSERT(quantum_size == 44);
    CU_ASSERT(tokens_in_bucket == 0);
}


/*
 * Tests core token bucket functionality.
 *
 * NOTE:  These tests are sequential, and a failure in one may lead
 *        to failures in later tests.
 */
void
test_tb_operation (void)
{
    token_bucket_info_t tb;
    uint32_t curr_tokens = 1;
    abs_time_t time;

    /*
     * Verify no crash occurs in simple cases of a non-initialized token
     * bucket being passed, or no token buckets being passed.
     */
    memset(&tb, 0, sizeof(token_bucket_info_t));
    (void)tb_credit_tokens(&tb, ABS_TIME_0, &curr_tokens);
    CU_ASSERT(!curr_tokens);
    CU_ASSERT(tb_credit_tokens(NULL, ABS_TIME_0, NULL) == 
              TB_RETVAL_TB_INVALID);
    CU_ASSERT(tb_drain_tokens(NULL, 0) == TB_RETVAL_TB_INVALID);

    /* Init bucket, make sure it's full */
    time = get_sys_time();
    CU_ASSERT(tb_init(&tb, 10, 50, 1, time) == TB_RETVAL_OK);
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 50);

    /* Attempt to drain more tokens than the burst, should be rejected */
    CU_ASSERT(tb_drain_tokens(&tb, 52) == TB_RETVAL_INSUFFICIENT_TOKENS);

    /* Drain an available amount of tokens */
    CU_ASSERT(tb_drain_tokens(&tb, 0) == TB_RETVAL_OK);
    CU_ASSERT(tb_drain_tokens(&tb, 20) == TB_RETVAL_OK);
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 30);

    /* Let 5 replenish periods of go by, make sure 5 tokens were added */
    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 500));
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 35);

    /*
     * Let some arbitrary time elapse, and issue calls to tb_credit_tokens().
     * Ensure the proper credits are made to the token bucket. 
     */
    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 90));  /* t + 90ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 35);  /* same as we started with */

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 90));  /* t + 180ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 36);  /* one more */

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 25));  /* t + 205ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 37);  /* one more */

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 200));  /* t + 405ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 39);  /* two more */

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 2200));  /* t + 2605ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 50);  /* bucket is full */
    
    /*
     * make sure token bucket works when current time not supplied
     */
    CU_ASSERT(tb_init(&tb, 10, 10, 1, ABS_TIME_0) == TB_RETVAL_OK);
    CU_ASSERT(tb_drain_tokens(&tb, 10) == TB_RETVAL_OK);  /* empty bucket */
    sleep(1);  /* let enough time go by to earn 10 tokens */
    CU_ASSERT(tb_credit_tokens(&tb, ABS_TIME_0, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 10);

    /*
     * white box test of operation near extreme values:
     *   - init bucket to rate = max, burst = max, quantum_size = max
     *   - verify that if enough time goes by to earn just over 2^32 tokens,
     *     the bucket is still full
     */ 
#define REPLENISH_PERIOD \
    (uint64_t)TB_QUANTUM_MAX * MICROSECONDS_PER_SECOND / TB_RATE_MAX

    CU_ASSERT(tb_init(&tb, TB_RATE_MAX, TB_BURST_MAX, TB_QUANTUM_MAX, time) ==
              TB_RETVAL_OK);
    CU_ASSERT(tb_drain_tokens(&tb, TB_BURST_MAX) ==
              TB_RETVAL_OK);
    time = TIME_ADD_A_R(time, 
                        TIME_MK_R(usec, UINT_MAX * REPLENISH_PERIOD + 1));
    curr_tokens = 0;
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == TB_BURST_MAX);

    /*
     * drain all tokens, allow just less than one replenish period to elapse
     */
    CU_ASSERT(tb_drain_tokens(&tb, TB_BURST_MAX) == TB_RETVAL_OK);
    time = TIME_ADD_A_R(time, TIME_MK_R(usec, REPLENISH_PERIOD - 1));    
    curr_tokens = 0;
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0);

    /* allow the remainder of a replenish period to elapse, check tokens */
    time = TIME_ADD_A_R(time, TIME_MK_R(usec, 1));    
    curr_tokens = 0;
    tb_credit_tokens(&tb, time, &curr_tokens);
    CU_ASSERT(curr_tokens == TB_QUANTUM_MAX);

    /* Check tb_conform() */
    CU_ASSERT(tb_init_simple(&tb, 10, 10) == TB_RETVAL_OK);
    /* Sure we have more than 8 tokens in the bucket, */
    CU_ASSERT_EQUAL(tb_conform(&tb, ABS_TIME_0, 8), TB_RETVAL_OK);
    /* but not 12 */
    CU_ASSERT_EQUAL(tb_conform(&tb, ABS_TIME_0, 12),
                    TB_RETVAL_INSUFFICIENT_TOKENS);
    /* Use 8 tokens, */
    CU_ASSERT_EQUAL(tb_drain_tokens(&tb, 8), TB_RETVAL_OK);
    /* now we have less than 8 tokens in the bucket. */
    CU_ASSERT_EQUAL(tb_conform(&tb, ABS_TIME_0, 8), 
                    TB_RETVAL_INSUFFICIENT_TOKENS);
    /* Let enough time go by to earn 10 tokens */
    time = TIME_ADD_A_R(get_sys_time(), TIME_MK_R(sec, 1));  /* t + 1s */
    /* Now we should have 10 tokens in the bucket. */
    CU_ASSERT_EQUAL(tb_conform(&tb, time, 8), TB_RETVAL_OK);
    
}

/*
 * Tests core token bucket functionality when rate=0.
 *
 * NOTE:  These tests are sequential, and a failure in one may lead
 *        to failures in later tests.
 */
void
test_tb_operation_zero_rate (void)
{
    token_bucket_info_t tb;
    uint32_t curr_tokens = 1;
    abs_time_t time;

    /* Init bucket, make sure it's full */
    time = get_sys_time();
    CU_ASSERT(tb_init(&tb, 0, 50, 1, time) == TB_RETVAL_OK);
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 50);

    /* Attempt to drain more tokens than the burst, should be rejected */
    CU_ASSERT(tb_drain_tokens(&tb, 51) == TB_RETVAL_INSUFFICIENT_TOKENS);

    /* Drain an available amount of tokens */
    CU_ASSERT(tb_drain_tokens(&tb, 50) == TB_RETVAL_OK);
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0);

    /* Let 5 replenish periods of go by, make sure no tokens were added */
    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 500));
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0);

    /*
     * After a partial drain.
     * Let some arbitrary time elapse, and issue calls to tb_credit_tokens().
     * Ensure that no credits are made to the token bucket. 
     */
    CU_ASSERT(tb_init(&tb, 0, 20, 1, ABS_TIME_0) == TB_RETVAL_OK);
    CU_ASSERT(tb_drain_tokens(&tb, 7) == TB_RETVAL_OK);
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 13);

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 90));  /* t + 90ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 13); 

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 90));  /* t + 180ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 13); 

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 25));  /* t + 205ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 13);  

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 2200));  /* t + 2405ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 13); 

    /*
     * make sure token bucket works when current time not supplied
     */
    CU_ASSERT(tb_init(&tb, 0, 10, 1, ABS_TIME_0) == TB_RETVAL_OK);
    CU_ASSERT(tb_drain_tokens(&tb, 7) == TB_RETVAL_OK);
    sleep(1);  /* let time go by */
    CU_ASSERT(tb_credit_tokens(&tb, ABS_TIME_0, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 3);    
}


/*
 * Tests core token bucket functionality when burst=0.
 *
 * NOTE:  These tests are sequential, and a failure in one may lead
 *        to failures in later tests.
 */
void
test_tb_operation_zero_burst (void)
{
    token_bucket_info_t tb;
    uint32_t curr_tokens = 1;
    abs_time_t time;

  /* Init bucket with rate=0 and burst=0, make sure it's "full" */
    time = get_sys_time();
    CU_ASSERT(tb_init(&tb, 0, 0, 1, time) == TB_RETVAL_OK);
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0);

    /* Init bucket, make sure it's "full" */
    time = get_sys_time();
    CU_ASSERT(tb_init(&tb, 50, 0, 1, time) == TB_RETVAL_OK);
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0);

    /* Attempt to drain more tokens than the burst, should be rejected */
    CU_ASSERT(tb_drain_tokens(&tb, 1) == TB_RETVAL_INSUFFICIENT_TOKENS);

    /* Drain an available amount of tokens */
    CU_ASSERT(tb_drain_tokens(&tb, 0) == TB_RETVAL_OK);
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0);

    /* Let 5 replenish periods of go by, make sure no tokens were added */
    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 500));
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0);

    /*
     * Let some arbitrary time elapse, and issue calls to tb_credit_tokens().
     * Ensure that no credits are made to the token bucket. 
     */
    CU_ASSERT(tb_init(&tb, 10, 0, 1, ABS_TIME_0) == TB_RETVAL_OK);
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0);

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 90));  /* t + 90ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0); 

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 90));  /* t + 180ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0); 

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 25));  /* t + 205ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0);  

    time = TIME_ADD_A_R(time, TIME_MK_R(msec, 2200));  /* t + 2405ms */
    CU_ASSERT(tb_credit_tokens(&tb, time, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0); 

    /*
     * make sure token bucket works when current time not supplied
     */
    CU_ASSERT(tb_init(&tb, 100, 0, 1, ABS_TIME_0) == TB_RETVAL_OK);
    CU_ASSERT(tb_drain_tokens(&tb, 7) == TB_RETVAL_INSUFFICIENT_TOKENS);
    CU_ASSERT(tb_credit_tokens(&tb, ABS_TIME_0, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0); 
    sleep(1);  /* let time go by */
    CU_ASSERT(tb_credit_tokens(&tb, ABS_TIME_0, &curr_tokens) == TB_RETVAL_OK);
    CU_ASSERT(curr_tokens == 0);    
}



CU_TestInfo test_vqe_token_bucket_array[] = {
    { "Token Bucket: initialization\n", test_tb_init },
    { "Token Bucket: convert error code to str", test_tb_retval_to_str },
    { "Token Bucket: read properties", test_tb_get_properties },
    { "Token Bucket: bucket operation", test_tb_operation },
    { "Token Bucket: bucket operation (rate=0)", test_tb_operation_zero_rate },
    { "Token Bucket: bucket operation (burst=0)", test_tb_operation_zero_burst },
    CU_TEST_INFO_NULL,
};

int
test_vqe_token_bucket_suite_init (void)
{
    return 0;
}

int
test_vqe_token_bucket_suite_clean (void)
{
    return 0;
}

CU_SuiteInfo suites_vqe_token_bucket[] = {
    { "VQE Token Bucket suite", 
      test_vqe_token_bucket_suite_init,
      test_vqe_token_bucket_suite_clean,
      test_vqe_token_bucket_array },
    CU_SUITE_INFO_NULL,
};
