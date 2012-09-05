/* 
 * RTP unit test program. Test scaffolding sets up RTP session object, 
 * uses RTP methods. This file holds all of the basic RTP member
 * function tests.
 *
 * Copyright (c) 2007-2008, 2010 by cisco Systems, Inc.
 * All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/utils/vam_types.h"
#include "rtp_session.h"
#include "rtp_database.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
/* CU_ASSERT_* return values are not used */
/* sa_ignore DISABLE_RETURNS CU_assertImplementation */


/* Globals for suite initialization and cleanup */
rtp_session_t  *p_test_sess = NULL;
rtp_session_t  *p_receiver_sess = NULL; /* Used for sender report tests. */
rtp_session_t  *p_sender_sess = NULL; /* Used for sender report tests. */

/* Check at known points for memory leaks. */
uint32_t       memblocks_suite;
uint32_t       memblocks_members;

/* Globals for cross-test validation */
/* Member 1 */
uint32_t ssrc1 = 0x01020301;
ipaddrtype src_addr1 = 0x03030201;
uint16_t src_port1 = 0x0100;
char cname1[] = "STB no. 1";
/* Member 2 */
uint32_t ssrc2 = 0x01020302;
ipaddrtype src_addr2 = 0x03030202;
uint16_t src_port2 = 0x0200;
char cname2[] = "STB no. 2";
/* Member 3 */
uint32_t ssrc3 = 0x01020303;
ipaddrtype src_addr3 = 0x03030203;
uint16_t src_port3 = 0x0300;
char cname3[] = "STB no. 3";

/***********************************************************************
 *
 *     BASIC MULTI MEMBER TESTS START HERE
 *
 * These are tests which exercise the basic member functions (add, 
 * lookup, lookup or create, delete, local source (gets ssrc value)). 
 *
 * These tests should only be run for sessions which can have multiple
 * (more than TWO) members. 
 *
 * These tests assume that the suite initialization has already
 * created a session, and the session pointer is the global
 * p_test_sess. Also, the suite cleanup should clean up the session.
 *
 * This is a continuous suite of tests, that is, the results expected
 * in the ASSERTs depend on running the tests in the proper order.
 *
 **********************************************************************/

void test_base_mul_member_add(void)
{
    rtp_member_t *p_member = NULL;
    rtp_member_id_t member_id;
    rtp_source_id_t source_id;
    int allocations;
    rtp_error_t result = RTP_MEMBER_DOES_NOT_EXIST;

    /* 
     * First checkpoint the memory blocks, so can detect leaks in member adds
     * or deletes. Check after all members deleted.
     */
    memblocks_members = rtcp_memory_allocated(p_test_sess->rtcp_mem, 
                                              &allocations);
    /* 
     * Create some members to play with.
     */
    /* Member 1 */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CHANNEL_MEMBER;
    member_id.ssrc = ssrc1;
    member_id.src_addr = src_addr1;
    member_id.src_port = src_port1;
    member_id.cname = cname1;

    result = MCALL(p_test_sess, rtp_create_member, 
                   &member_id, &p_member);
    CU_ASSERT(RTP_SUCCESS == result);

    /* Member 2 */
    p_member = NULL;
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CLIENT_MEMBER;
    member_id.ssrc = ssrc2;
    member_id.src_addr = src_addr2;
    member_id.src_port = src_port2;
    member_id.cname = cname2;
    result = MCALL(p_test_sess, rtp_create_member, 
                   &member_id, &p_member);
    CU_ASSERT(RTP_SUCCESS == result);

    /* Member 3 */
    /* Note: add third member, without a cname, and
       via the rtp_new_data_source_method, to test whether that works. */
    p_member = NULL;
    source_id.ssrc = ssrc3;
    source_id.src_addr = src_addr3;
    source_id.src_port = src_port3;
    result = MCALL(p_test_sess, rtp_new_data_source, &source_id); 
    CU_ASSERT(RTP_SUCCESS == result);

    /* There should now be 3 members, 1 sender */
    CU_ASSERT(3 == p_test_sess->rtcp_nmembers);
    CU_ASSERT(1 == p_test_sess->rtcp_nsenders);

    /* KK!! Add the check members in list test here. */
}

/* 
 * IMPORTANT! This test relies on running test_base_mul_member_add first;
 * that test leaves 3 members in the session for this test to lookup. 
 */
void test_base_mul_member_lookup(void)
{
    rtp_member_t *p_member = NULL;
    rtp_member_t *cmember = NULL;
    char *conflict_cname = NULL;
    rtp_member_id_t member_id;
    rtp_conflict_info_t info;

    rtp_error_t result = RTP_MEMBER_DOES_NOT_EXIST;

    /* Lookup the second member */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc2;
    member_id.src_addr = src_addr2;
    member_id.src_port = src_port2;
    member_id.cname = cname2;

    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_SUCCESS == result);
    CU_ASSERT_STRING_EQUAL(p_member ? p_member->sdes[RTCP_SDES_CNAME] : "", 
                           cname2);

    /* Lookup the first member, but with second member's cname, 
     * so should find first member but complain DUPLICATE */
    p_member = NULL;
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc1;
    member_id.src_addr = src_addr1;
    member_id.src_port = src_port1;
    member_id.cname = cname2;
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_MEMBER_IS_DUP == result);
    cmember = info.cmember[RTP_CONFLICT_PARTIAL_REMOTE_SNDR];
    conflict_cname = cmember ? cmember->sdes[RTCP_SDES_CNAME] : "";
    CU_ASSERT_STRING_EQUAL(conflict_cname, cname1);

    /* lookup the third member, but this time via RTCP data,
       specifying previously unspecified RTCP addr/port, and CNAME;
       this should cause the member to be updated */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc3;
    member_id.src_addr = src_addr3+1;
    member_id.src_port = src_port3+1;
    member_id.cname = cname3;
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_SUCCESS == result);
    CU_ASSERT_STRING_EQUAL(p_member ? p_member->sdes[RTCP_SDES_CNAME] : "", 
                           cname3);
    CU_ASSERT((p_member ? p_member->rtcp_src_addr : 0) == (src_addr3 + 1));
    CU_ASSERT((p_member ? p_member->rtcp_src_port : 0) == (src_port3 + 1));

    /* lookup the first member, by SSRC only */
    p_member = NULL;
    p_member = rtp_sending_member_with_ssrc(p_test_sess, ssrc1);
    CU_ASSERT_PTR_NOT_NULL(p_member);

    /* lookup the third member, by SSRC and CNAME (full key) */
    p_member = NULL;
    p_member = rtp_find_member(p_test_sess, ssrc3, cname3);
    CU_ASSERT_PTR_NOT_NULL(p_member);

}

/* 
 * IMPORTANT! This test relies on running test_base_mul_member_add first;
 * that test leaves 3 members in the session for this test to lookup. 
 */
void test_base_mul_member_lookup_or_create(void)
{

    /* KK!! finish this!! */

}


/* 
 * IMPORTANT! This test relies on running test_base_mul_member_add first.
 * That test creates the session, this test creates the local source, 
 * which is a member in the session.
 */
void test_base_mul_local_source(void)
{
    /* Local source */
    uint32_t local_ssrc1;
    uint32_t local_ssrc2;
    uint32_t local_ssrc3;

    /*
     * Test the local source functions. Generate SSRC.
     */
    local_ssrc1 = MCALL(p_test_sess, rtp_choose_local_ssrc);
    local_ssrc2 = MCALL(p_test_sess, rtp_choose_local_ssrc);
    local_ssrc3 = MCALL(p_test_sess, rtp_choose_local_ssrc);
    /*
     * choose_local_ssrc doesn't add a member, so there's no 
     * prevention in the routine itself against getting the 
     * same number twice. That means we can call it more than once
     * to do some tests to check randomness. 
     * Beware: there are assumptions in our rtp code that there will
     * only be a single sender in a session; if we actually added more 
     * than one sender, there might be problems.
     *
     * These tests are pretty brain-dead; will only detect same number
     * or strict sequence. Oh well, it's probably worth doing.
     */
    CU_ASSERT_NOT_EQUAL(local_ssrc1, local_ssrc2);
    CU_ASSERT_NOT_EQUAL(local_ssrc1, local_ssrc3);
    CU_ASSERT_NOT_EQUAL(local_ssrc2, (local_ssrc1 + 1));
    CU_ASSERT_NOT_EQUAL(local_ssrc3, (local_ssrc2 + 1));
}

/* 
 * IMPORTANT! This test relies on running test_base_mul_member_add first;
 * that test leaves 3 members in the session for this test to delete. 
 */
void test_base_mul_member_delete(void)
{
    rtp_member_t *p_member = NULL;
    rtp_member_id_t member_id;
    rtp_conflict_info_t info;
    rtp_member_t *p_tmp = NULL;
    rtp_error_t result = RTP_SUCCESS;


    /* Remove Member 1 */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc1;
    member_id.src_addr = src_addr1;
    member_id.src_port = src_port1;
    member_id.cname = cname1;
    p_member = MCALL(p_test_sess, rtp_remove_member_by_id,
                     &member_id, TRUE);
    /* This assert only tests that the action of the bool arg "TRUE" works.
     * That TRUE forces freeing the memory, presumably after taking the
     * member out of all relevant lists. */
    CU_ASSERT_PTR_NULL(p_member);
    /* Now try looking it up to see if it's really gone. */
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_MEMBER_DOES_NOT_EXIST == result);
    CU_ASSERT(2 == p_test_sess->rtcp_nmembers);
    CU_ASSERT(1 == p_test_sess->rtcp_nsenders);

    /* Remove Member 2, but don't ask for memory free */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc2;
    member_id.src_addr = src_addr2;
    member_id.src_port = src_port2;
    member_id.cname = cname2;
    p_member = MCALL(p_test_sess, rtp_remove_member_by_id,
                     &member_id, FALSE);
    CU_ASSERT_PTR_NOT_NULL(p_member);
    /* Now try looking it up to see if it's really gone. */
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_tmp, &info);
    CU_ASSERT(RTP_MEMBER_DOES_NOT_EXIST == result);
    CU_ASSERT(1 == p_test_sess->rtcp_nmembers);
    CU_ASSERT(1 == p_test_sess->rtcp_nsenders);
    /* Now have to free it ourselves */
    if (p_member == p_test_sess->rtp_local_source) {
        p_test_sess->rtp_local_source = NULL;
    }
    rtcp_delete_member(p_test_sess->rtcp_mem, p_member->subtype, p_member);

    /* Remove Member 3 */
    member_id.type = RTP_SMEMBER_ID_RTP_DATA;
    member_id.ssrc = ssrc3;
    member_id.src_addr = src_addr3;
    member_id.src_port = src_port3;
    member_id.cname = NULL;
    p_member = MCALL(p_test_sess, rtp_remove_member_by_id,
                     &member_id, TRUE);
    CU_ASSERT_PTR_NULL(p_member);
    /* Now try looking it up to see if it's really gone. */
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_MEMBER_DOES_NOT_EXIST == result);
    CU_ASSERT(0 == p_test_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_test_sess->rtcp_nsenders);

#if 0
    /* KK!! incorporate this */
    check_elements_in_lists(p_test_sess, p_test_sess->rtcp_nsenders,
                            p_test_sess->rtcp_nmembers);
#endif

}

/*
 * u32compar
 *
 * Helper function for qsort, used by test_base_mul_member_randomizer.
 * It counts the number of times it detects "equal" elements,
 * in the static "compared_equal" counter, which should be
 * zeroed before the qsort.
 */
static int compared_equal = 0;

static int u32compar (const void *p1, const void *p2) 
{
    uint32_t u1 = *((uint32_t *)p1);
    uint32_t u2 = *((uint32_t *)p2);

    if (u1 < u2) {
        return (-1);
    } else if (u1 > u2) {
        return (1);
    } else {
        compared_equal++;
        return (0);
    }
}

/*
 * test_base_mul_member_randomizer
 *
 * This test exercises the random number generator,
 * checking that a set of random numbers contains no duplicates.
 * This check is done during the process of sorting the set of 
 * numbers.
 *
 * This test does not create multiple members, but the
 * correct behavior of the randomizer is important to 
 * multiple member scenarios.
 */

#define NUM_RANDOMS 500
static uint32_t random_arr[NUM_RANDOMS];

void test_base_mul_member_randomizer (void)
{
    int i;

    for (i = 0; i < NUM_RANDOMS ; i++) {
        random_arr[i] = rtp_random32(i);
    }
    compared_equal = 0;
    qsort(random_arr, NUM_RANDOMS, sizeof(uint32_t), u32compar);

    CU_ASSERT(0 == compared_equal);
}

/***********************************************************************
 *
 * The base multi member "partial collision" test adds "receive only" 
 * members that "partly" collide, i.e., they share the same SSRC, but
 * differ in CNAME. This is unlikely, but possible, in sessions where
 * the headend is not RTCP-capable, and thus the resolution of SSRC 
 * collisions betweeen receiving members cannot take place, as there's
 * no way to notify a receiving member that its SSRC has collided with
 * another receiving member's SSRC.
 *
 **********************************************************************/

void test_base_mul_member_partcoll(void)
{
    /* Member 1 */
    uint32_t ssrc1;
    /* Member 2 */
    uint32_t ssrc2;
    /* Member 3 */
    uint32_t ssrc3;
    /* Member 4 */
    uint32_t ssrc4;
    ipaddrtype src_addr4 = 0x03030204;
    uint16_t src_port4 = 0x0400;
    char cname4[] = "STB no. 4";

    rtp_member_t *p_member = NULL;
    rtp_member_id_t member_id;
    rtp_conflict_info_t info;
    rtp_error_t result = RTP_MEMBER_DOES_NOT_EXIST;

    ssrc1 = 0x01020301;
    ssrc2 = ssrc1;
    ssrc3 = 0x01020303;
    ssrc4 = ssrc3;
    
    /* 
     * Create the partly colliding members.
     */
    /* Member 2 */
    p_member = NULL;
    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CLIENT_MEMBER;
    member_id.ssrc = ssrc2;
    member_id.src_addr = src_addr2;
    member_id.src_port = src_port2;
    member_id.cname = cname2;
    result = MCALL(p_test_sess, rtp_create_member, 
                   &member_id, &p_member);
    CU_ASSERT(RTP_SUCCESS == result);
    /* Member 1 */
    p_member = NULL;
    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CLIENT_MEMBER;
    member_id.ssrc = ssrc1;
    member_id.src_addr = src_addr1;
    member_id.src_port = src_port1;
    member_id.cname = cname1;
    result = MCALL(p_test_sess, rtp_lookup_or_create_member, 
                   &member_id, &p_member);
    CU_ASSERT(RTP_SUCCESS == result);
    /* Member 4 */
    p_member = NULL;
    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CLIENT_MEMBER;
    member_id.ssrc = ssrc4;
    member_id.src_addr = src_addr4;
    member_id.src_port = src_port4;
    member_id.cname = cname4;
    result = MCALL(p_test_sess, rtp_create_member, 
                   &member_id, &p_member);
    CU_ASSERT(RTP_SUCCESS == result);
    /* Member 3 */
    p_member = NULL;
    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CLIENT_MEMBER;
    member_id.ssrc = ssrc3;
    member_id.src_addr = src_addr3;
    member_id.src_port = src_port3;
    member_id.cname = cname3;
    result = MCALL(p_test_sess, rtp_lookup_or_create_member, 
                   &member_id, &p_member);
    CU_ASSERT(RTP_SUCCESS == result);

    CU_ASSERT(4 == p_test_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_test_sess->rtcp_nsenders);

    /* Now look up the members to see whether the creations
     * really worked. Use 4, 1, 2, 3 for no good reason. */
    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CLIENT_MEMBER;
    member_id.ssrc = ssrc4;
    member_id.src_addr = src_addr4;
    member_id.src_port = src_port4;
    member_id.cname = cname4;
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_SUCCESS == result);
    CU_ASSERT_STRING_EQUAL(p_member? p_member->sdes[RTCP_SDES_CNAME] : "", 
                           cname4);

    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CLIENT_MEMBER;
    member_id.ssrc = ssrc1;
    member_id.src_addr = src_addr1;
    member_id.src_port = src_port1;
    member_id.cname = cname1;
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_SUCCESS == result);
    CU_ASSERT_STRING_EQUAL(p_member ? p_member->sdes[RTCP_SDES_CNAME] : "", 
                           cname1);

    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CLIENT_MEMBER;
    member_id.ssrc = ssrc2;
    member_id.src_addr = src_addr2;
    member_id.src_port = src_port2;
    member_id.cname = cname2;
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_SUCCESS == result);
    CU_ASSERT_STRING_EQUAL(p_member ? p_member->sdes[RTCP_SDES_CNAME] : "", 
                           cname2);

    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CLIENT_MEMBER;
    member_id.ssrc = ssrc3;
    member_id.src_addr = src_addr3;
    member_id.src_port = src_port3;
    member_id.cname = cname3;
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_SUCCESS == result);
    CU_ASSERT_STRING_EQUAL(p_member ? p_member->sdes[RTCP_SDES_CNAME] : "", 
                           cname3);

    /* Check member deletions. Use order 2,4,1,3. */
    /* 2 */
    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc2;
    member_id.src_addr = src_addr2;
    member_id.src_port = src_port2;
    member_id.cname = cname2;
    p_member = MCALL(p_test_sess, rtp_remove_member_by_id,
                     &member_id, TRUE);
    CU_ASSERT_PTR_NULL(p_member);
    /* Now try looking it up to see if it's really gone;
       this info is still a "duplicate" of 2 */
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_MEMBER_IS_DUP == result);
    CU_ASSERT(3 == p_test_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_test_sess->rtcp_nsenders);
    /* 4 */
    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc4;
    member_id.src_addr = src_addr4;
    member_id.src_port = src_port4;
    member_id.cname = cname4;
    p_member = MCALL(p_test_sess, rtp_remove_member_by_id,
                     &member_id, TRUE);
    CU_ASSERT_PTR_NULL(p_member);
    /* Now try looking it up to see if it's really gone; 
       this info is still a "duplicate" of 2 */
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_MEMBER_IS_DUP == result);
    CU_ASSERT(2 == p_test_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_test_sess->rtcp_nsenders);
    /* 1 */
    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc1;
    member_id.src_addr = src_addr1;
    member_id.src_port = src_port1;
    member_id.cname = cname1;
    p_member = MCALL(p_test_sess, rtp_remove_member_by_id,
                     &member_id, TRUE);
    CU_ASSERT_PTR_NULL(p_member);
    /* Now try looking it up to see if it's really gone. */
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_MEMBER_DOES_NOT_EXIST == result);
    CU_ASSERT(1 == p_test_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_test_sess->rtcp_nsenders);
    /* 3 */
    member_id.type = RTP_RMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc3;
    member_id.src_addr = src_addr3;
    member_id.src_port = src_port3;
    member_id.cname = cname3;
    p_member = MCALL(p_test_sess, rtp_remove_member_by_id,
                     &member_id, TRUE);
    CU_ASSERT_PTR_NULL(p_member);
    /* Now try looking it up to see if it's really gone. */
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_MEMBER_DOES_NOT_EXIST == result);
    CU_ASSERT(0 == p_test_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_test_sess->rtcp_nsenders);

#if 0
    /* KK!! reactivate this */
    check_elements_in_lists(p_rtp_sess, p_rtp_sess->rtcp_nsenders,
                            p_rtp_sess->rtcp_nmembers);

#endif
}

/* KK!! need to actually test cleanup_session. */
/*    MCALL(p_rtp_sess, rtp_cleanup_session); */

/* 
 * Memory test checks remembered values of alloc_blocks against
 * current value.
 */
void test_base_mul_member_memory(void)
{
    int allocations;

    CU_ASSERT_EQUAL(rtcp_memory_allocated(p_test_sess->rtcp_mem, 
                                          &allocations),
                    memblocks_members);

    /* Clean up the session just in case. */
    MCALL(p_test_sess, rtp_cleanup_session);
}


/***********************************************************************
 *
 *     BASIC TWO MEMBER TESTS START HERE
 *
 * These are tests which exercise the basic member functions (add, 
 * lookup, lookup or create, delete, local source (gets ssrc value)). 
 *
 * These tests should only be run for sessions which can only have
 * TWO members (e.g., ptp, ssm_rcvr). 
 *
 * These tests assume that the suite initialization has already
 * created a session, and the session pointer is the global
 * p_test_sess. Also, the suite cleanup should clean up the session.
 *
 * This is a continuous suite of tests, that is, the results expected
 * in the ASSERTs depend on running the tests in the proper order.
 *
 **********************************************************************/

void test_base_two_member_add(void)
{
    rtp_member_t *p_member = NULL;
    rtp_member_id_t member_id;
    int allocations;
    rtp_error_t result = RTP_MEMBER_DOES_NOT_EXIST;

    /* 
     * First checkpoint the memory blocks, so can detect leaks in member adds
     * or deletes. Check after all members deleted.
     */
    memblocks_members = rtcp_memory_allocated(p_test_sess->rtcp_mem, 
                                              &allocations);
    /* 
     * Create a single member. (The second is local source, tested later.)
     */
    /* Member 1 */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.subtype = RTCP_CHANNEL_MEMBER;
    member_id.ssrc = ssrc1;
    member_id.src_addr = src_addr1;
    member_id.src_port = src_port1;
    member_id.cname = cname1;
    result = MCALL(p_test_sess, rtp_create_member, 
                   &member_id, &p_member);
    CU_ASSERT(RTP_SUCCESS == result);

    /* There should now be 1 member, no senders */
    CU_ASSERT(1 == p_test_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_test_sess->rtcp_nsenders);

    /* KK!! Add the check members in list test here. */
}

/* 
 * IMPORTANT! This test relies on running test_base_two_member_add first;
 * that test leaves 1 member in the session for this test to lookup. 
 */
void test_base_two_member_lookup(void)
{
    rtp_member_t *p_member = NULL;
    rtp_member_t *cmember = NULL;
    char *conflict_cname = NULL;
    rtp_member_id_t member_id;
    rtp_conflict_info_t info;

    rtp_error_t result = RTP_MEMBER_DOES_NOT_EXIST;


    /* Lookup the first member */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc1;
    member_id.src_addr = src_addr1;
    member_id.src_port = src_port1;
    member_id.cname = cname1;
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_SUCCESS == result);
    CU_ASSERT_STRING_EQUAL(p_member ? p_member->sdes[RTCP_SDES_CNAME] : "",
                           cname1);

    /* Lookup the first member, but with a different cname, 
     * so should find first member but complain DUPLICATE */
    p_member = NULL;
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc1;
    member_id.src_addr = src_addr1;
    member_id.src_port = src_port1;
    member_id.cname = cname2;
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_MEMBER_IS_DUP == result);
    cmember = info.cmember[RTP_CONFLICT_PARTIAL_REMOTE_SNDR];
    conflict_cname = cmember ? cmember->sdes[RTCP_SDES_CNAME] : "" ;
    CU_ASSERT_STRING_EQUAL(conflict_cname, cname1);
}

/* 
 * IMPORTANT! This test relies on running test_base_two_member_add first;
 * that test leaves 1 member in the session for this test to lookup. 
 */
void test_base_two_member_lookup_or_create(void)
{

    /* KK!! finish this!! */

}


/* 
 * IMPORTANT! This test relies on running test_base_two_member_add first.
 * That test creates the session, this test creates the local source, 
 * which is a member in the session.
 */
void test_base_two_local_source(void)
{
    /* Local source */
    uint32_t local_ssrc1;
    uint32_t local_ssrc2;
    uint32_t local_ssrc3;

    /*
     * Test the local source functions. Generate SSRC.
     */
    local_ssrc1 = MCALL(p_test_sess, rtp_choose_local_ssrc);
    local_ssrc2 = MCALL(p_test_sess, rtp_choose_local_ssrc);
    local_ssrc3 = MCALL(p_test_sess, rtp_choose_local_ssrc);
    /*
     * choose_local_ssrc doesn't add a member, so there's no 
     * prevention in the routine itself against getting the 
     * same number twice. That means we can call it more than once
     * to do some tests to check randomness. 
     * Beware: there are assumptions in our rtp code that there will
     * only be a single sender in a session; if we actually added more 
     * than one sender, there might be problems.
     *
     * These tests are pretty brain-dead; will only detect same number
     * or strict sequence. Oh well, it's probably worth doing.
     */
    CU_ASSERT_NOT_EQUAL(local_ssrc1, local_ssrc2);
    CU_ASSERT_NOT_EQUAL(local_ssrc1, local_ssrc3);
    CU_ASSERT_NOT_EQUAL(local_ssrc2, (local_ssrc1 + 1));
    CU_ASSERT_NOT_EQUAL(local_ssrc3, (local_ssrc2 + 1));
}

/* 
 * IMPORTANT! This test relies on running test_base_two_member_add first;
 * that test leaves 1 member in the session for this test to delete. 
 */
void test_base_two_member_delete(void)
{
    rtp_member_t *p_member = NULL;
    rtp_member_id_t member_id;
    rtp_conflict_info_t info;

    rtp_error_t result = RTP_SUCCESS;


    /* Remove Member 1 */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.ssrc = ssrc1;
    member_id.src_addr = src_addr1;
    member_id.src_port = src_port1;
    member_id.cname = cname1;
    p_member = MCALL(p_test_sess, rtp_remove_member_by_id,
                     &member_id, TRUE);
    /* This assert only tests that the action of the bool arg "TRUE" works.
     * That TRUE forces freeing the memory, presumably after taking the
     * member out of all relevant lists. */
    CU_ASSERT_PTR_NULL(p_member);
    /* Now try looking it up to see if it's really gone. */
    result = MCALL(p_test_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_MEMBER_DOES_NOT_EXIST == result);
    CU_ASSERT(0 == p_test_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_test_sess->rtcp_nsenders);

    /* KK!! incorporate this */
#if 0
    check_elements_in_lists(p_test_sess, p_test_sess->rtcp_nsenders,
                            p_test_sess->rtcp_nmembers);
#endif

}

/* 
 * Memory test checks remembered values of alloc_blocks against
 * current value.
 */
void test_base_two_member_memory(void)
{
    int allocations;

    CU_ASSERT_EQUAL(rtcp_memory_allocated(p_test_sess->rtcp_mem, 
                                          &allocations),
                    memblocks_members);

    /* Clean up the session just in case. */
    MCALL(p_test_sess, rtp_cleanup_session);
}

/***********************************************************************
 *
 * The Base Sender Report test may be run by any session: two-members or
 * multiple-members sessions.
 * 
 * Base Sender Report test constructs and parses a Sender 
 * Report. To do this, we create two sessions, a local and a "remote".
 * Session creation must be done (as usual) in the suite initialization
 * function, and the session pointers put into the globals p_receiver_sess
 * and p_sender_sess.
 *
 * The remote will construct a dummy Sender Report, and the local will
 * pretend to receive the constructed Sender Report. We don't actually
 * send any packets.
 *
 * Note that there is nothing in RTP that identifies a session except
 * the RTP/RTCP address/port pairs, and the IP headers are processed
 * outside of this RTP library. Thus it is possible to simulate two
 * different hosts within this library by setting up two different 
 * sessions. We can then accept a Sender Report from a different 
 * session as if it came from a different host within the same session.
 *
 **********************************************************************/

void test_base_sender_report(void)
{
    rtp_member_t   *p_sender = NULL;
    rtp_member_t   *p_member = NULL;
    rtp_member_id_t member_id;
    rtp_conflict_info_t info;
    rtp_error_t    result = RTP_SUCCESS;
    uint32_t       rtcp_len = 0;
    uint8_t        *data_start = NULL;
    uint32_t       sender_ssrc = 0;
    int            allocations;

    /* Member 1, sender */
    ipaddrtype src_addr_sender = 0xc0a83737;
    uint16_t src_port_sender = 0x0100;
    char cname_sender[] = "Sender (VAM)";

    /* 
     * First checkpoint the memory blocks, so can detect leaks in member adds
     * or deletes. Check after all members deleted.
     */
    memblocks_members = rtcp_memory_allocated(p_sender_sess->rtcp_mem, 
                                              &allocations);

    /* Member 1, created in sender sess to be sender and construct report */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    /* 
     * the actual SSRC value will be randomly selected
     * and written here by the rtp_create_local_source method
     */     
    member_id.ssrc = 0;
    member_id.subtype = RTCP_CHANNEL_MEMBER;
    member_id.src_addr = src_addr_sender;
    member_id.src_port = src_port_sender;
    member_id.cname = cname_sender;
    p_sender = MCALL(p_sender_sess, rtp_create_local_source,
                     &member_id, RTP_SELECT_RANDOM_SSRC);
    CU_ASSERT_PTR_NOT_NULL(p_sender);

    /* Save for later checks. */
    sender_ssrc = p_sender->ssrc;

    /* Generate a RTCP Sender report -- Dummy up sender stats */
    data_start = (uint8_t* ) malloc(RTCP_PAK_SIZE);

    p_sender->packets = 100;
    p_sender->packets_prior = 50;
    p_sender->bytes = 10000;
    p_sender->bytes_prior = 5000;
    p_sender->rtp_timestamp =  rtp_random32(RANDOM_TIMESTAMP_TYPE);
    p_sender->ntp_timestamp = get_ntp_time();
    rtcp_len = MCALL(p_sender_sess, rtcp_construct_report, 
                     p_sender,
                     (rtcptype *)data_start, 
                     RTCP_PAK_SIZE,
                     NULL /* no additional (non-default) packet content */,
                     FALSE);
        
    CU_ASSERT(1 == p_sender_sess->rtcp_nmembers);
    CU_ASSERT(1 == p_sender_sess->rtcp_nsenders);

    /* Parse the sender report in the receiver session. */
    MCALL(p_receiver_sess, rtcp_process_packet, 
          FALSE, /* NOT from a "recv only" member */
          src_addr_sender, 0, 
          (char *)data_start, rtcp_len);

    CU_ASSERT(1 == p_receiver_sess->rtcp_nmembers);
    CU_ASSERT(1 == p_receiver_sess->rtcp_nsenders);

    /* Clean up everything. */
    free(data_start);
    /* Interesting to delete a member that's a sender. */
    /* Remove sender from "local" session, use ssrc of the remote sender. */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    member_id.ssrc = sender_ssrc;
    member_id.src_addr = src_addr_sender;
    member_id.src_port = src_port_sender;
    member_id.cname = cname_sender;
    p_member = MCALL(p_receiver_sess, rtp_remove_member_by_id, 
                     &member_id, TRUE);
    CU_ASSERT_PTR_NULL(p_member);
    /* Now try looking it up to see if it's really gone. */
    result = MCALL(p_receiver_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_MEMBER_DOES_NOT_EXIST == result);
    CU_ASSERT(0 == p_receiver_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_receiver_sess->rtcp_nsenders);

    /* Use cleanup_session to delete the "remote" session member. */
    MCALL(p_sender_sess, rtp_cleanup_session);
    /* Now try looking it up to see if it's really gone. */
    result = MCALL(p_sender_sess, rtp_lookup_member, 
                   &member_id, &p_member, &info);
    CU_ASSERT(RTP_MEMBER_DOES_NOT_EXIST == result);
    CU_ASSERT(0 == p_sender_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_sender_sess->rtcp_nsenders);


#if 0
    check_elements_in_lists(p_rtp_sess, p_rtp_sess->rtcp_nsenders,
                            p_rtp_sess->rtcp_nmembers);
#endif

}

static rtcptype *rtcp_find_msg(rtcptype *p_rtcp, int len, rtcp_type_t type)
{
    uint16_t params, len_hs;

    if (len < sizeof(rtcptype)) {
        return (NULL);
    }
    params = ntohs(p_rtcp->params);
    while (rtcp_get_type(params) != type){
        len_hs = ntohs(p_rtcp->len);
        len -= ((len_hs + 1) * 4);
        if (len < sizeof(rtcptype)) {
            break;
        }
        p_rtcp = (rtcptype *) ((uint8_t *)p_rtcp + (len_hs + 1) * 4);
        params = ntohs(p_rtcp->params);
    }

    return (len >= sizeof(rtcptype) ? p_rtcp : NULL);
}

/*
 * test_base_construct_gnack
 *
 * Test the construction of a generic NACK
 * Based on the former VQEC test: test_vqec_rtcp_construct_generic_nack
 */
void test_base_construct_gnack(void)
{
#define RTP_UT_NUMNACKS 3
    rtp_member_t   *p_sender = NULL;
    uint8_t        *buff = NULL;
    uint32_t bufflen = RTCP_PAK_SIZE;
    uint32_t non_nack_len= 0;
    uint32_t        len = 0;
    rtcp_rtpfb_generic_nack_t nack_data[RTP_UT_NUMNACKS];
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t nack;
    rtcpfbtype_t *p_rtcpfb = NULL;
    rtcp_rtpfb_generic_nack_t *p_nack;
    rtp_member_id_t member_id;

    /* Member 1, sender */
    ipaddrtype src_addr_sender = 0xc0a83737;
    uint16_t src_port_sender = 0x0201;
    char cname_sender[] = "Sender (VAM)";

    /* Member 1, created in sender sess to be sender and construct report */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    /* 
     * the actual SSRC value will be randomly selected
     * and written here by the rtp_create_local_source method
     */     
    member_id.ssrc = 0;
    member_id.subtype = RTCP_CHANNEL_MEMBER;
    member_id.src_addr = src_addr_sender;
    member_id.src_port = src_port_sender;
    member_id.cname = cname_sender;
    p_sender = MCALL(p_sender_sess, rtp_create_local_source, 
                     &member_id, RTP_SELECT_RANDOM_SSRC);

    CU_ASSERT_PTR_NOT_NULL(p_sender);

    /* Generate a RTCP Sender report */
    buff = malloc(RTCP_PAK_SIZE);
    CU_ASSERT_PTR_NOT_NULL(buff);
    if (!buff) {
        goto cleanup;
    }

    non_nack_len = MCALL(p_sender_sess, rtcp_construct_report, 
                         p_sender,
                         (rtcptype *)buff, 
                         RTCP_PAK_SIZE,
                         NULL /* no (non-default) packet content */,
                         FALSE);
    CU_ASSERT(non_nack_len > 0);

    memset(&nack_data[0], 0, sizeof(nack_data));
    memset(buff, 0, RTCP_PAK_SIZE);
    rtcp_init_pkt_info(&pkt_info);
    rtcp_init_msg_info(&nack);

    /* test that we can insert a generic NACK */
    nack_data[0].pid = 0x1234;
    nack_data[0].bitmask = 0x5678;
    nack.rtpfb_info.fmt = RTCP_RTPFB_GENERIC_NACK;
    nack.rtpfb_info.num_nacks = 1;
    nack.rtpfb_info.nack_data = nack_data;
    rtcp_set_pkt_info(&pkt_info, RTCP_RTPFB, &nack);
    rtcp_set_rtcp_rsize(&pkt_info, FALSE);

    /* Note 16 additional bytes for buffer length says room for one buffer */
    len = MCALL(p_sender_sess,
                rtcp_construct_report,
                p_sender_sess->rtp_local_source,
                (rtcptype *)buff, 
                non_nack_len + 16,
                &pkt_info,
                FALSE);
    CU_ASSERT((len - non_nack_len) == 16);
    
    /* validate the the length increased appropriately */
    /* verify the data in the bufffer */
    p_rtcpfb = 
        (rtcpfbtype_t *)rtcp_find_msg((rtcptype *)buff, len, RTCP_RTPFB);
    CU_ASSERT_PTR_NOT_NULL(p_rtcpfb);

    if (p_rtcpfb) {
        p_nack = (rtcp_rtpfb_generic_nack_t *)(p_rtcpfb + 1);
        CU_ASSERT(ntohs(p_nack->pid) == 0x1234);
        CU_ASSERT(ntohs(p_nack->bitmask) == 0x5678); 
    }

    /* verify that we put in the sessions sender */

    /* test multiple nacks, using array */
    nack_data[0].pid = 11;
    nack_data[0].bitmask = 0x2222;
    nack_data[1].pid = 22;
    nack_data[1].bitmask = 0x2222;
    nack_data[2].pid = 33;
    nack_data[2].bitmask = 0x3333;
    nack.rtpfb_info.num_nacks = RTP_UT_NUMNACKS;
    len = MCALL(p_sender_sess,
                rtcp_construct_report,
                p_sender_sess->rtp_local_source,
                (rtcptype *)buff, 
                non_nack_len + 24,
                &pkt_info,
                FALSE);
    CU_ASSERT((len - non_nack_len) == 24);

    /* test the exceed max size behavior */

    /* test invalid args */
    
    /* invalid num nacks */
    nack_data[0].pid = 2314;
    nack_data[0].bitmask = 0x0001;
    nack.rtpfb_info.num_nacks = 0;
    len = MCALL(p_sender_sess,
                rtcp_construct_report,
                p_sender_sess->rtp_local_source,
                (rtcptype *)buff, 
                bufflen,
                &pkt_info,
                FALSE);
    CU_ASSERT(len == 0);

    /* null nack data */
    nack_data[0].pid = 2314;
    nack_data[0].bitmask = 0x0001;
    nack.rtpfb_info.num_nacks = 1;
    nack.rtpfb_info.nack_data = NULL;
    len = MCALL(p_sender_sess,
                rtcp_construct_report,
                p_sender_sess->rtp_local_source,
                (rtcptype *)buff, 
                bufflen,
                &pkt_info,
                FALSE);
    CU_ASSERT(len == 0);

    /* Clean up everything. */
    free(buff);
 cleanup:
    MCALL(p_sender_sess, rtp_cleanup_session);
    CU_ASSERT(0 == p_sender_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_sender_sess->rtcp_nsenders);
}

/*
 * test_base_construct_pubports
 *
 * Test the construction of a PUBPORTS message
 * Based on the former VQEC test: test_vqec_rtcp_construct_pubport
 */
void test_base_construct_pubports(void)
{
    rtp_member_t   *p_sender = NULL;
    uint32_t        len = 0;
    uint8_t        *buff = NULL;
    uint32_t bufflen = RTCP_PAK_SIZE;
    uint32_t non_pubports_len= 0;
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t msg;
    rtcptype *p_rtcp = NULL;
    rtcp_pubports_t *p_pubports = NULL;
    rtp_member_id_t member_id;

    /* Member 1, sender */
    ipaddrtype src_addr_sender = 0xc0a83737;
    uint16_t src_port_sender = 0x0201;
    char cname_sender[] = "Sender (VAM)";

    /* Member 1, created in sender sess to be sender and construct report */
    member_id.type = RTP_SMEMBER_ID_RTCP_DATA;
    /* 
     * the actual SSRC value will be randomly selected
     * and written here by the rtp_create_local_source method
     */     
    member_id.ssrc = 0;
    member_id.subtype = RTCP_CHANNEL_MEMBER;
    member_id.src_addr = src_addr_sender;
    member_id.src_port = src_port_sender;
    member_id.cname = cname_sender;
    p_sender = MCALL(p_sender_sess, rtp_create_local_source, 
                     &member_id, RTP_SELECT_RANDOM_SSRC);
    CU_ASSERT_PTR_NOT_NULL(p_sender);

    /* Generate a RTCP Sender report */
    buff = malloc(RTCP_PAK_SIZE);
    CU_ASSERT_PTR_NOT_NULL(buff);
    if (!buff) {
        goto cleanup;
    }
    memset(buff, 0, RTCP_PAK_SIZE);
    non_pubports_len = MCALL(p_sender_sess, rtcp_construct_report, 
                             p_sender,
                             (rtcptype *)buff,
                             RTCP_PAK_SIZE,
                             NULL /* no (non-default) packet content */,
                             FALSE);
    CU_ASSERT(non_pubports_len > 0);

    /* now, construct one with PUBPORTS and check the data */
    rtcp_init_pkt_info(&pkt_info);
    rtcp_init_msg_info(&msg);
    msg.pubports_info.ssrc_media_sender = 0x01020304;
    msg.pubports_info.rtp_port = 0x1234;
    msg.pubports_info.rtcp_port = 0x4567;
    rtcp_set_pkt_info(&pkt_info, RTCP_PUBPORTS, &msg);

    len = MCALL(p_sender_sess,
                rtcp_construct_report,
                p_sender_sess->rtp_local_source,
                (rtcptype *)buff, 
                bufflen,
                &pkt_info,
                FALSE);

    CU_ASSERT((len - non_pubports_len) == 16);
    p_rtcp = rtcp_find_msg((rtcptype *)buff, len, RTCP_PUBPORTS);
    CU_ASSERT_PTR_NOT_NULL(p_rtcp);
    if (p_rtcp) {
        p_pubports = (rtcp_pubports_t *)(p_rtcp + 1);
        CU_ASSERT(ntohl(p_pubports->ssrc_media_sender) == 0x01020304);
        CU_ASSERT(ntohs(p_pubports->rtp_port) == 0x1234);
        CU_ASSERT(ntohs(p_pubports->rtcp_port) == 0x4567);
    }

    /* invalid buff too small */

    /* invalid args source NULL */
    len = MCALL(p_sender_sess,
                rtcp_construct_report,
                NULL,
                (rtcptype *)buff, 
                bufflen,
                &pkt_info,
                FALSE);
    CU_ASSERT(len == 0);

    /* invalid args buff NULL */
    len = MCALL(p_sender_sess,
                rtcp_construct_report,
                p_sender_sess->rtp_local_source,
                NULL, 
                bufflen,
                &pkt_info,
                FALSE);
    CU_ASSERT(len == 0);

    /* Clean up everything. */

    free(buff);
 cleanup:
    MCALL(p_sender_sess, rtp_cleanup_session);
    CU_ASSERT(0 == p_sender_sess->rtcp_nmembers);
    CU_ASSERT(0 == p_sender_sess->rtcp_nsenders);
}

/* 
 * Memory test checks remembered values of alloc_blocks against
 * current value.
 */
void test_base_sender_report_memory(void)
{
    int allocations;

    CU_ASSERT_EQUAL(rtcp_memory_allocated(p_sender_sess->rtcp_mem, 
                                          &allocations), 
                    memblocks_members);

    /* Clean up the sessions just in case. */
    MCALL(p_receiver_sess, rtp_cleanup_session);
    MCALL(p_sender_sess, rtp_cleanup_session);
}


