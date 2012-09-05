/* 
 * RTP unit test program. Test scaffolding sets up RTP session object, 
 * uses RTP methods. This file holds all of the RTP PTP tests.
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/utils/vam_types.h"
#include "rtp_session.h"
#include "rtp_ptp.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
/* CU_ASSERT_* return values are not used */
/* sa_ignore DISABLE_RETURNS CU_assertImplementation */


/* Globals for cross-test validation or cleanup */
extern rtp_session_t  *p_test_sess;
extern rtp_session_t  *p_receiver_sess; /* Used for sender report tests. */
extern rtp_session_t  *p_sender_sess; /* Used for sender report tests. */

rtp_ptp_t      *p_rtp_ptp = NULL; /* Used for creation and then deletion. */

/* Check at known points for memory leaks. */
extern uint32_t       memblocks_suite;
extern uint32_t       memblocks_members;


/***********************************************************************
 *
 *     PTP SESSION SUITE TESTS START HERE
 *
 * PTP Session Suite tests the basic session functions (create and delete).
 *
 **********************************************************************/

int init_ptp_sess_suite(void)
{
//  memblocks_suite = alloc_blocks;
    return(0);
}
int clean_ptp_sess_suite(void)
{
    return(0);
}

void test_ptp_sess_create(void)
{
    rtp_config_t config;

    rtp_init_config(&config, RTPAPP_DEFAULT);
    config.senders_cached = 1;
    /* assume only one sender and one receiver */
    config.rtcp_bw_cfg.media_rr_bw = RTCP_DFLT_PER_RCVR_BW;
    config.rtcp_bw_cfg.media_rs_bw = RTCP_DFLT_PER_SNDR_BW;

    /* Use the fatal form; no use proceeding if we can't create a session. */
    CU_ASSERT_TRUE_FATAL(rtp_create_session_ptp(&config, &p_rtp_ptp, TRUE));

    /* KK!! Should beat on the session creation some more! */

}

void test_ptp_sess_delete(void)
{
    /* KK!! Must change delete to return error if bad */
    rtp_delete_session_ptp(&p_rtp_ptp, TRUE);
}

/***********************************************************************
 *
 *     PTP MEMBER FUNCTIONS SUITE TESTS START HERE
 *
 * PTP Member Functions Suite tests the basic member functions using
 * the base_two_member tests, the tests for sessions which can only have
 * two members. It's important to create the session at init_suite
 * time and store the pointer in the p_test_sess global. Also clean up
 * the session afterwards.
 *
 **********************************************************************/

int init_ptp_member_suite(void)
{
    rtp_config_t config;

    rtp_init_config(&config, RTPAPP_DEFAULT);
    config.senders_cached = 1;
    /* assume just one sender and one receiver */
    config.rtcp_bw_cfg.media_rs_bw = RTCP_DFLT_PER_SNDR_BW;
    config.rtcp_bw_cfg.media_rr_bw = RTCP_DFLT_PER_RCVR_BW;

//  memblocks_suite = alloc_blocks;
    /* Note that you can't assert from within an init or cleanup function. */
    if (TRUE != (rtp_create_session_ptp(&config, &p_rtp_ptp, TRUE))) {
        return (-1);
    }

    p_test_sess = (rtp_session_t *)p_rtp_ptp;
    return(0);
}
int clean_ptp_member_suite(void)
{
    rtp_delete_session_ptp(&p_rtp_ptp, TRUE);
    return(0);
}

/***********************************************************************
 *
 *     PTP SENDER REPORT SUITE TESTS START HERE
 *
 * PTP Sender Report Suite tests the basic sender report functions using
 * the base sender report tests. It's important to create two sessions
 * at init_suite time and store the pointers in the p_receiver_sess and
 * p_sender_sess globals. Also clean up the sessions afterwards.
 *
 **********************************************************************/

int init_ptp_sender_report_suite(void)
{
    rtp_config_t config;

    rtp_init_config(&config, RTPAPP_DEFAULT);
    config.senders_cached = 1;
    /* assume just one sender and one receiver */
    config.rtcp_bw_cfg.media_rr_bw = RTCP_DFLT_PER_RCVR_BW;
    config.rtcp_bw_cfg.media_rs_bw = RTCP_DFLT_PER_SNDR_BW;

//  memblocks_suite = alloc_blocks;
    /* Note that you can't assert from within an init or cleanup function. */
    if (TRUE != (rtp_create_session_ptp(&config, 
                                        (rtp_ptp_t **)&p_receiver_sess, 
                                        TRUE))) {
        return (-1);
    }

    if (TRUE != (rtp_create_session_ptp(&config, 
                                        (rtp_ptp_t **)&p_sender_sess, 
                                        TRUE))) {
        return (-1);
    }

    return(0);
}
int clean_ptp_sender_report_suite(void)
{
    rtp_delete_session_ptp((rtp_ptp_t **)&p_sender_sess, TRUE);
    rtp_delete_session_ptp((rtp_ptp_t **)&p_receiver_sess, TRUE);
    return(0);
}

