/* 
 * RTP unit test program. Test scaffolding sets up RTP session object, 
 * uses RTP methods. This file holds all of the RTP ASM tests.
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/utils/vam_types.h"
#include "rtp_session.h"
#include "rtp_asm.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
/* CU_ASSERT_* return values are not used */
/* sa_ignore DISABLE_RETURNS CU_assertImplementation */


/* Globals for cross-test validation or cleanup */
extern rtp_session_t  *p_test_sess;
extern rtp_session_t  *p_receiver_sess; /* Used for sender report tests. */
extern rtp_session_t  *p_sender_sess; /* Used for sender report tests. */

rtp_asm_t      *p_rtp_asm = NULL; /* Used for creation and then deletion. */


/* Check at known points for memory leaks. */
extern uint32_t       memblocks_suite;
extern uint32_t       memblocks_members;


/***********************************************************************
 *
 *     ASM SESSION SUITE TESTS START HERE
 *
 * ASM Session Suite tests the basic session functions (create and delete).
 *
 **********************************************************************/

int init_asm_sess_suite(void)
{
//  memblocks_suite = alloc_blocks;
    return(0);
}
int clean_asm_sess_suite(void)
{
    return(0);
}

void test_asm_sess_create(void)
{
    rtp_config_t config;

    rtp_init_config(&config, RTPAPP_DEFAULT);
    config.senders_cached = 1;
    config.rtcp_bw_cfg.media_as_bw = RTCP_DFLT_AS_BW;

    /* Use the fatal form; no use proceeding if we can't create a session. */
    CU_ASSERT_TRUE_FATAL(rtp_create_session_asm(&config, &p_rtp_asm, TRUE));

    /* KK!! Should beat on the session creation some more! */

}

void test_asm_sess_delete(void)
{
    /* KK!! Must change delete to return error if bad */
    rtp_delete_session_asm(&p_rtp_asm, TRUE);
}

/***********************************************************************
 *
 *     ASM MEMBER FUNCTIONS SUITE TESTS START HERE
 *
 * ASM Member Functions Suite tests the basic member functions using
 * the base_multi tests. Important to create the session at init_suite
 * time and store the pointer in the p_test_sess global. Also clean up
 * the session afterwards.
 *
 **********************************************************************/

int init_asm_member_suite(void)
{
    rtp_config_t config;

    rtp_init_config(&config, RTPAPP_DEFAULT);
    config.senders_cached = 1;
    config.rtcp_bw_cfg.media_as_bw = RTCP_DFLT_AS_BW;

//  memblocks_suite = alloc_blocks;
    /* Note that you can't assert from within an init or cleanup function. */
    if (TRUE != (rtp_create_session_asm(&config, &p_rtp_asm, TRUE))) {
        return (-1);
    }

    p_test_sess = (rtp_session_t *)p_rtp_asm;
    return(0);
}
int clean_asm_member_suite(void)
{
    rtp_delete_session_asm(&p_rtp_asm, TRUE);
    return(0);
}

/***********************************************************************
 *
 *     ASM SENDER REPORT SUITE TESTS START HERE
 *
 * ASM Sender Report Suite tests the basic sender report functions using
 * the base sender report tests. It's important to create two sessions
 * at init_suite time and store the pointers in the p_receiver_sess and
 * p_sender_sess globals. Also clean up the sessions afterwards.
 *
 **********************************************************************/

int init_asm_sender_report_suite(void)
{
    rtp_config_t config;

    rtp_init_config(&config, RTPAPP_DEFAULT);
    config.senders_cached = 1;
    config.rtcp_bw_cfg.media_as_bw = RTCP_DFLT_AS_BW;

//  memblocks_suite = alloc_blocks;
    /* Note that you can't assert from within an init or cleanup function. */
    if (TRUE != (rtp_create_session_asm(&config, 
                                        (rtp_asm_t **)&p_receiver_sess, 
                                        TRUE))) {
        return (-1);
    }

    if (TRUE != (rtp_create_session_asm(&config, 
                                        (rtp_asm_t **)&p_sender_sess, 
                                        TRUE))) {
        return (-1);
    }

    return(0);
}
int clean_asm_sender_report_suite(void)
{
    rtp_delete_session_asm((rtp_asm_t **)&p_sender_sess, TRUE);
    rtp_delete_session_asm((rtp_asm_t **)&p_receiver_sess, TRUE);
    return(0);
}

