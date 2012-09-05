/* 
 * RTP unit test program. Test scaffolding uses CUnit, an OpenSource GPL
 * library. This is the main test include file, which lists the prototypes
 * for all the RTP unit tests.
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 */

/*
 * Base multiple-member session, member function tests.
 */
void test_base_mul_member_add(void);
void test_base_mul_member_lookup(void);
void test_base_mul_member_lookup_or_create(void);
void test_base_mul_local_source(void);
void test_base_mul_member_delete(void);
void test_base_mul_member_randomizer(void);
void test_base_mul_member_partcoll(void);
void test_base_mul_member_memory(void);

/*
 * Base two-member session, member function tests. (e.g., ptp, ssm rcvr)
 */
void test_base_two_member_add(void);
void test_base_two_member_lookup(void);
void test_base_two_member_lookup_or_create(void);
void test_base_two_local_source(void);
void test_base_two_member_delete(void);
void test_base_two_member_memory(void);

/*
 * Base sender report test may be run by any type of session.
 */
void test_base_sender_report(void);
void test_base_construct_gnack(void);
void test_base_construct_pubports(void);
void test_base_sender_report_memory(void);

/*****************************************************************
 *             ASM tests
 ****************************************************************/
/*
 * Suite initializations and cleanups.
 */
int init_asm_sess_suite(void);
int clean_asm_sess_suite(void);
int init_asm_member_suite(void);
int clean_asm_member_suite(void);
int init_asm_sender_report_suite(void);
int clean_asm_sender_report_suite(void);
/*
 * Actual tests.
 */
void test_asm_sess_create(void);
void test_asm_sess_delete(void);

/*****************************************************************
 *             PTP tests
 ****************************************************************/
/*
 * Suite initializations and cleanups.
 */
int init_ptp_sess_suite(void);
int clean_ptp_sess_suite(void);
int init_ptp_member_suite(void);
int clean_ptp_member_suite(void);
int init_ptp_sender_report_suite(void);
int clean_ptp_sender_report_suite(void);
/*
 * Actual tests.
 */
void test_ptp_sess_create(void);
void test_ptp_sess_delete(void);

/*****************************************************************
 *             SSM RSI SOURCE tests
 ****************************************************************/
/*
 * Suite initializations and cleanups.
 */
int init_ssm_src_sess_suite(void);
int clean_ssm_src_sess_suite(void);
int init_ssm_src_member_suite(void);
int clean_ssm_src_member_suite(void);
int init_ssm_src_sender_report_suite(void);
int clean_ssm_src_sender_report_suite(void);
/*
 * Actual tests.
 */
void test_ssm_src_sess_create(void);
void test_ssm_src_sess_delete(void);

/*****************************************************************
 *             SSM RSI RECEIVER tests
 ****************************************************************/
/*
 * Suite initializations and cleanups.
 */
int init_ssm_rcv_sess_suite(void);
int clean_ssm_rcv_sess_suite(void);
int init_ssm_rcv_member_suite(void);
int clean_ssm_rcv_member_suite(void);
int init_ssm_rcv_sender_report_suite(void);
int clean_ssm_rcv_sender_report_suite(void);
/*
 * Actual tests.
 */
void test_ssm_rcv_sess_create(void);
void test_ssm_rcv_sess_delete(void);

/*****************************************************************
 *             PCTP XR tests
 ****************************************************************/
/*
 * Suite initializations and cleanups.
 */
int init_rtcp_xr_suite(void);
int clean_rtcp_xr_suite(void);

/*
 * Actual tests.
 */
void test_rtcp_xr_no_loss(void);
void test_rtcp_xr_over_limit(void);
void test_rtcp_xr_missing_seq(void);
void test_rtcp_xr_dup_seq(void);
void test_rtcp_xr_late_arrival(void);
