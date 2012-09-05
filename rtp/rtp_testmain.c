/* 
 * RTP unit test program. Test scaffolding uses CUnit, an OpenSource GPL
 * library. This is the main test file, which registers and runs all the
 * RTP unit tests.
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 */
#include "rtp_testmain.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

extern void rtp_asm_init_module(void);
extern void rtp_ptp_init_module(void);
extern void rtp_ssm_rsi_source_init_module(void);
extern void rtp_ssm_rsi_rcvr_init_module(void);

/***********************************************************************
 *
 *     CUnit FRAMEWORK STARTS HERE
 *
 **********************************************************************/

CU_TestInfo test_array_rtp_base_multi[] = {
    /* This suite can be used by all sessions that can have more than
     * two members. It tests the base member functions. It assumes
     * that the suite initialization function creates your special
     * kind of session, and the suite cleanup function deletes it. You
     * should unit test the create and delete of your sessions before
     * running this suite. 
     * In this suite the tests are interrelated, that is, the tests
     * depend on what has gone on in previous tests. Be careful when
     * adding new tests. */
    { "test base multimember add", test_base_mul_member_add },
    { "test base multimember lookup", test_base_mul_member_lookup },
    { "test base multimember lookup or create", 
      test_base_mul_member_lookup_or_create },
    { "test base multimember local source", test_base_mul_local_source },
    { "test base multimember delete", test_base_mul_member_delete },
    { "test base multimember randomizer", test_base_mul_member_randomizer },
    { "test base multimember partial collision", 
      test_base_mul_member_partcoll },
    /* The memory test should be the last in the member suite. */
    { "test base multimember memory", test_base_mul_member_memory },
    CU_TEST_INFO_NULL,
};
CU_TestInfo test_array_rtp_base_twomem[] = {
    /* This suite can be used by all sessions that can only have
     * two members. It tests the base member functions. It assumes
     * that the suite initialization function creates your special
     * kind of session, and the suite cleanup function deletes it. You
     * should unit test the create and delete of your sessions before
     * running this suite.
     * In this suite the tests are interrelated, that is, the tests
     * depend on what has gone on in previous tests. Be careful when
     * adding new tests. */
    { "test base twomember add", test_base_two_member_add },
    { "test base twomember lookup", test_base_two_member_lookup },
    { "test base twomember lookup or create", 
      test_base_two_member_lookup_or_create },
    { "test base twomember local source", test_base_two_local_source },
    { "test base twomember delete", test_base_two_member_delete },
    /* The memory test should be the last in the member suite. */
    { "test base twomember memory", test_base_two_member_memory },
    CU_TEST_INFO_NULL,
};
CU_TestInfo test_array_rtp_base_sender_report[] = {
    /* This suite can be used by all sessions. It tests constructing
     * and parsing sender reports. It requires that the suite 
     * initialization function create two of your special
     * kind of session, and the suite cleanup function delete them. You
     * should unit test the create and delete of your sessions before
     * running this suite. */
    { "test base construct, parse sender report", test_base_sender_report },
    { "test base construct generic NACK", test_base_construct_gnack },
    { "test base construct PUBPORTS message", test_base_construct_pubports },
    /* The memory test should be the last in the member suite. */
    { "test base sender report memory", test_base_sender_report_memory },
    CU_TEST_INFO_NULL,
};
CU_TestInfo test_array_rtp_asm_sess[] = {
    { "test asm session create", test_asm_sess_create },
    { "test asm session delete", test_asm_sess_delete },
    CU_TEST_INFO_NULL,
};
CU_TestInfo test_array_rtp_ptp_sess[] = {
    { "test asm session create", test_ptp_sess_create },
    { "test asm session delete", test_ptp_sess_delete },
    CU_TEST_INFO_NULL,
};
CU_TestInfo test_array_rtp_ssm_src_sess[] = {
    { "test ssm src session create", test_ssm_src_sess_create },
    { "test ssm src session delete", test_ssm_src_sess_delete },
    CU_TEST_INFO_NULL,
};
CU_TestInfo test_array_rtp_ssm_rcv_sess[] = {
    { "test ssm rcv session create", test_ssm_rcv_sess_create },
    { "test ssm rcv session delete", test_ssm_rcv_sess_delete },
    CU_TEST_INFO_NULL,
};
CU_TestInfo test_array_rtcp_xr[] = {
    { "test rtcp xr update without any loss", test_rtcp_xr_no_loss },
    { "test rtcp xr update with over limit", test_rtcp_xr_over_limit },
    { "test rtcp xr update with missing seq", test_rtcp_xr_missing_seq },
    { "test rtcp xr update with duplicated seq", test_rtcp_xr_dup_seq },
    { "test rtcp xr update with late arrival", test_rtcp_xr_late_arrival },
    CU_TEST_INFO_NULL,
};

CU_SuiteInfo suites_rtp[] = {
    { "RTP ASM test session create, delete", init_asm_sess_suite, 
      clean_asm_sess_suite, test_array_rtp_asm_sess },
    { "RTP ASM test basic member functions", init_asm_member_suite, 
      clean_asm_member_suite, test_array_rtp_base_multi },
    { "RTP ASM test basic sender report", init_asm_sender_report_suite, 
      clean_asm_sender_report_suite, test_array_rtp_base_sender_report },
    { "RTP PTP test session create, delete", init_ptp_sess_suite, 
      clean_ptp_sess_suite, test_array_rtp_ptp_sess },
    { "RTP PTP test basic member functions", init_ptp_member_suite, 
      clean_ptp_member_suite, test_array_rtp_base_twomem },
    { "RTP PTP test basic sender report", init_ptp_sender_report_suite, 
      clean_ptp_sender_report_suite, test_array_rtp_base_sender_report },
    { "RTP SSM src test session create, delete", init_ssm_src_sess_suite, 
      clean_ssm_src_sess_suite, test_array_rtp_ssm_src_sess },
    { "RTP SSM src test basic member functions", init_ssm_src_member_suite, 
      clean_ssm_src_member_suite, test_array_rtp_base_multi },
    { "RTP SSM src test basic sender report", 
      init_ssm_src_sender_report_suite, 
      clean_ssm_src_sender_report_suite, test_array_rtp_base_sender_report },
    { "RTP SSM rcv test session create, delete", init_ssm_rcv_sess_suite, 
      clean_ssm_rcv_sess_suite, test_array_rtp_ssm_rcv_sess },
    { "RTP SSM rcv test basic member functions", init_ssm_rcv_member_suite, 
      clean_ssm_rcv_member_suite, test_array_rtp_base_twomem },
    { "RTP SSM rcv test basic sender report", 
      init_ssm_rcv_sender_report_suite,
      clean_ssm_rcv_sender_report_suite, test_array_rtp_base_sender_report },
    { "RCTP XR test update sequence", init_rtcp_xr_suite, 
      clean_rtcp_xr_suite, test_array_rtcp_xr },
    CU_SUITE_INFO_NULL,
};

/* The main() function for setting up and running the tests.
 * Returns a CUE_SUCCESS on successful running, another
 * CUnit error code on failure.
 */
int main(void)
{
    /* 
     * Special for RTP: Initialize the memory management for 
     * all the different types of RTP sessions. This only
     * needs to be done once at the beginning of time (here).
     */
    rtp_asm_init_module();
    rtp_ptp_init_module();
    rtp_ssm_rsi_source_init_module();
    rtp_ssm_rsi_rcvr_init_module();

    /* initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    if (CUE_SUCCESS != CU_register_suites(suites_rtp)) {
        return CU_get_error();
    }

    /* Run all tests using the CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    (void)CU_basic_run_tests();

    CU_cleanup_registry();
    return CU_get_error();
}
