/* 
 * RTP unit test program. Test scaffolding sets up RTP session object, 
 * uses RTP methods. This file holds all of the RTP SSM RSI RECEIVER tests.
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/utils/vam_types.h"
#include "rtp_session.h"
#include "rtp_ssm_rsi_rcvr.h"
#include "rtp_ssm_rsi_source.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
/* CU_ASSERT_* return values are not used */
/* sa_ignore DISABLE_RETURNS CU_assertImplementation */


/* Globals for cross-test validation or cleanup */
extern rtp_session_t  *p_test_sess;
extern rtp_session_t  *p_receiver_sess; /* Used for sender report tests. */
extern rtp_session_t  *p_sender_sess; /* Used for sender report tests. */

rtp_ssm_rsi_rcvr_t  *p_rtp_ssm_rcv = NULL; /* Used for creation and later 
                                       * deletion. */


/* Check at known points for memory leaks. */
extern uint32_t       memblocks_suite;
extern uint32_t       memblocks_members;


/***********************************************************************
 *
 *     SSM RSI RECEIVER SESSION SUITE TESTS START HERE
 *
 * SSM RSI RECEIVER Session Suite tests the basic session functions (create 
 * and delete).
 *
 **********************************************************************/

int init_ssm_rcv_sess_suite(void)
{
//  memblocks_suite = alloc_blocks;
    return(0);
}
int clean_ssm_rcv_sess_suite(void)
{
    return(0);
}

void test_ssm_rcv_sess_create(void)
{
    rtp_config_t config;

    rtp_init_config(&config, RTPAPP_STBRTP);
    config.senders_cached = 1;
    config.rtcp_bw_cfg.media_as_bw = RTCP_DFLT_AS_BW;

    /* Use the fatal form; no use proceeding if we can't create a session. */
    CU_ASSERT_TRUE_FATAL(rtp_create_session_ssm_rsi_rcvr(&config, 
                                                         &p_rtp_ssm_rcv, 
                                                         TRUE));

    /* KK!! Should beat on the session creation some more! */

}

void test_ssm_rcv_sess_delete(void)
{
    /* KK!! Must change delete to return error if bad */
    rtp_delete_session_ssm_rsi_rcvr(&p_rtp_ssm_rcv, TRUE);
}

/***********************************************************************
 *
 *     SSM RSI RECEIVER MEMBER FUNCTIONS SUITE TESTS START HERE
 *
 * SSM RSI RECEIVER Member Functions Suite tests the basic member functions
 * using
 * the base_multi tests. Important to create the session at init_suite
 * time and store the pointer in the p_test_sess global. Also clean up
 * the session afterwards.
 *
 **********************************************************************/

int init_ssm_rcv_member_suite(void)
{
    rtp_config_t config;

    rtp_init_config(&config, RTPAPP_STBRTP);
    config.senders_cached = 1;
    config.rtcp_bw_cfg.media_as_bw = RTCP_DFLT_AS_BW;

//  memblocks_suite = alloc_blocks;
    /* Note that you can't assert from within an init or cleanup function. */
    if (TRUE != (rtp_create_session_ssm_rsi_rcvr(&config, 
                                                 &p_rtp_ssm_rcv, 
                                                 TRUE))) {
        return (-1);
    }

    p_test_sess = (rtp_session_t *)p_rtp_ssm_rcv;
    return(0);
}
int clean_ssm_rcv_member_suite(void)
{
    rtp_delete_session_ssm_rsi_rcvr(&p_rtp_ssm_rcv, TRUE);
    return(0);
}

/***********************************************************************
 *
 *     SSM RSI RECEIVER SENDER REPORT SUITE TESTS START HERE
 *
 * SSM RSI RECEIVER Sender Report Suite tests the basic sender report 
 * functions using
 * the base sender report tests. It's important to create two sessions
 * at init_suite time and store the pointers in the p_receiver_sess and
 * p_sender_sess globals. Also clean up the sessions afterwards.
 *
 * IMPORTANT! Note that the SSM "sessions" (rsi source and rsi receiver)
 * are really two members of the same SSM session. This is important to
 * this test because the rsi source expects to send the rsi as part of
 * the Sender Report, and the rsi receiver expects to receive and
 * parse it. In the current code, the source CANNOT parse a Sender 
 * Report with an RSI and the receiver CANNOT send one. I think this
 * may be a problem with the source -- what if there's a loop and you
 * get your own packet in? Possibly it's not a problem, and this 
 * situation is prevented earlier in the data path.
 * Anyway, for now this test creates two different types of sessions,
 * the proper ones for the sender and receiver.
 *
 **********************************************************************/

int init_ssm_rcv_sender_report_suite(void)
{
    rtp_config_t config;

    rtp_init_config(&config, RTPAPP_STBRTP);
    config.senders_cached = 1;
    config.rtcp_bw_cfg.media_as_bw = RTCP_DFLT_AS_BW;

//  memblocks_suite = alloc_blocks;
    /* Note that you can't assert from within an init or cleanup function. */
    if (TRUE != (rtp_create_session_ssm_rsi_rcvr(
                     &config, 
                     (rtp_ssm_rsi_rcvr_t **)&p_receiver_sess, 
                     TRUE))) {
        return (-1);
    }

    config.senders_cached = 1;
    config.app_type = RTPAPP_VAMRTP;
    config.app_ref = NULL;

    if (TRUE != (rtp_create_session_ssm_rsi_source(
                     &config, 
                     (rtp_ssm_rsi_source_t **)&p_sender_sess, 
                     TRUE))) {
        return (-1);
    }

    return(0);
}
int clean_ssm_rcv_sender_report_suite(void)
{
    rtp_delete_session_ssm_rsi_source(
        (rtp_ssm_rsi_source_t **)&p_sender_sess, TRUE);
    rtp_delete_session_ssm_rsi_rcvr(
        (rtp_ssm_rsi_rcvr_t **)&p_receiver_sess, TRUE);
    return(0);
}

