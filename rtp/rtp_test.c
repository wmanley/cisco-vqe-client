/* 
 * RTP unit test program. Test scaffolding sets up RTP session object, 
 * uses RTP methods.
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/utils/vam_types.h"
#include "rtp_session.h"
#include "rtp_asm.h"
#include "rtp_ssm_rsi_rcvr.h"
#include "rtp_ssm_rsi_source.h"


#define RTP_UTEST
#ifdef  RTP_UTEST
static inline void check_mem_leak(uint32_t blocks, rtcp_memory_t *mem)
{
    ASSERT(blocks == rtcp_memory_allocated(mem, NULL), 
           "Memory Leak Detected\n");
    return;
}
#endif


uint32_t testnum = 1;
rtp_asm_t *p_remote_asm = NULL;
rtp_asm_t *p_rtp_asm = NULL;
rtp_config_t config;
uint32_t memblocks;
static void check_elements_in_lists(rtp_session_t *p_sess, 
                                        uint32_t nsenders, 
                                        uint32_t nmembers)
{
    uint32_t hash;
    uint32_t members=0, rcvrs=0, senders=0;
    rtp_member_t *p_member,*p_tmp;
    rtp_member_hash_t *p_collis_bucket;
    uint32_t colliding_members = 0;

    for (hash = 0; hash<p_sess->member_hash_size; hash++) {
        p_collis_bucket = RTP_HASH_BUCKET_HEAD(p_sess, hash);
        colliding_members = 0;
        VQE_LIST_FOREACH(p_tmp, &(p_collis_bucket->collision_head), p_collis_list) {
            colliding_members++;
        }
        if (colliding_members > 1) {
          printf("[RTP UTEST] Found Colliding %u members in collision list\n",
                 colliding_members);
        }
          members += colliding_members;
    }    
    ASSERT(nmembers==members,
           "[RTP UTEST] number of members (%d/%d) in list incorrect\n",
           nmembers, members);

    VQE_TAILQ_FOREACH_REVERSE(p_member, &(p_sess->senders_list),senders_list_t_,
                          p_member_chain) {
        senders++;
    }
    ASSERT(senders == nsenders,
           "[RTP UTEST]number of senders (%d/%d) in list incorrect\n",
           nmembers, members);

    VQE_TAILQ_FOREACH_REVERSE(p_member, &(p_sess->garbage_list),garbage_list_t_,
                          p_member_chain) {
        rcvrs++;
    }
    ASSERT((rcvrs == (members-senders)), 
           "[RTP UTEST]number of rcvrs(%d/%d) in garbage list incorrect\n",
           (members-senders), rcvrs);

}


/* Test1
 * Check: Members addition and deltion to  a RTP session
 *        Member lookup, and duplicate member logic
 *        The lists have been updated correctly. 
 *   
 */
static int test_rtp_session1(char *session_type, void *p_session)
{
    rtp_session_t *p_rtp_sess = (rtp_session_t *) p_session;
    rtp_member_t *p_member = NULL,*p_tmp;
    uint32_t status = SUCCESS;

    printf("\n*************** STARTING TEST %d *****************************\n"
           ,testnum);
    /* Member 1 */
    uint32_t ssrc = 0x01020301;
    ipaddrtype src_addr = 0x03030201;
    char cname1[] = "First STB";
    /* Member 2 */
    char cname2[] = "Second STB";

    /* Local source */
    uint32_t local_ssrc;
    uint32_t result;

    /* 
     * Create some members to play with.
     */
    result = MCALL(p_rtp_sess, rtp_create_member, 
                   ssrc, src_addr, cname1, &p_member);
    ASSERT(result == RTP_SUCCESS, "[RTP UTEST]Couldn't create member 1");   

    ssrc++;
    src_addr++;
    p_member = NULL;
    result = MCALL(p_rtp_sess, rtp_create_member, 
                   ssrc, src_addr, cname2, &p_member);
    ASSERT(result == RTP_SUCCESS, "[RTP UTEST]Couldn't create member 2");
    ssrc++;
    src_addr++;
    /* Note: add third member without a cname to test that. */
    p_member = NULL;
    result = MCALL(p_rtp_sess, rtp_create_member, 
                   ssrc, src_addr, NULL, &p_member);
    ASSERT(result == RTP_SUCCESS, "[RTP UTEST]Couldn't create member 3");
    
    /* Lookup the second member */
    ssrc--;
    src_addr--;
    status = MCALL(p_rtp_sess, rtp_lookup_or_create_member, 
                   ssrc, src_addr, cname2, &p_member);
    ASSERT(result == RTP_SUCCESS, 
           "[RTP UTEST]Couldn't find member 2, status was 0x%x\n", result);
    printf("Found member 2, cname is %s\n", p_member->sdes[RTCP_SDES_CNAME]);


    /* Lookup the first member, but with second member's cname, 
     * so should be DUP */
    ssrc--;
    src_addr--;
    status = MCALL(p_rtp_sess, rtp_lookup_member, 
                   ssrc, src_addr, cname2, &p_member);
    ASSERT(status == RTP_MEMBER_IS_DUP, 
           "[RTP UTEST]Lookup of duplicate member failed, status was 0x%x.\n",
           status);
    printf("[RTP UTEST] Found duplicate member 1 gave cname %s, it has cname %s\n", 
           cname2, p_member->sdes[RTCP_SDES_CNAME]);


    /*
     * Test the local source functions. Generate SSRC.
     */
    local_ssrc = MCALL(p_rtp_sess, rtp_choose_local_ssrc);
    printf("[RTP UTEST]Got SSRC 0x%x for local source.\n", local_ssrc);

    printf("[RTP UTEST]Session type=%s: There are now %d members, %d senders in session %x\n",
           session_type, p_rtp_sess->rtcp_nmembers, p_rtp_sess->rtcp_nsenders,
           (uint32_t)p_rtp_sess);

    check_elements_in_lists(p_rtp_sess, p_rtp_sess->rtcp_nsenders,
                            p_rtp_sess->rtcp_nmembers);

    /* Start the removal */
    ssrc = 0x01020301;
    printf("[RTP UTEST] Starting to delete the members ");
    p_tmp = MCALL(p_rtp_sess, rtp_remove_ssrc,ssrc, TRUE);
    ASSERT(p_tmp == NULL, "[RTP UTEST]Member didn't get deleted");
    printf("[RTP UTEST]Session type=%s: There are now %d members, %d senders in session %x.\n",
           session_type, p_rtp_sess->rtcp_nmembers, p_rtp_sess->rtcp_nsenders,
           (uint32_t)p_rtp_sess);
    check_elements_in_lists(p_rtp_sess, p_rtp_sess->rtcp_nsenders,
                            p_rtp_sess->rtcp_nmembers);

    ssrc++;
    p_tmp = MCALL(p_rtp_sess, rtp_remove_ssrc,ssrc, TRUE);
    ASSERT(p_tmp == NULL, "[RTP UTEST]Member didn't get deleted");
    printf("[RTP UTEST]Session type=%s: There are now %d members, %d senders in session %x.\n",
           session_type, p_rtp_sess->rtcp_nmembers, p_rtp_sess->rtcp_nsenders,
           (uint32_t)p_rtp_sess);
    check_elements_in_lists(p_rtp_sess, p_rtp_sess->rtcp_nsenders,
                            p_rtp_sess->rtcp_nmembers);


    ssrc++;
    p_tmp = MCALL(p_rtp_sess, rtp_remove_ssrc,ssrc, TRUE);
    printf("[RTP UTEST] Session type=%s: There are now %d members, %d senders in session %x.\n",
           session_type, p_rtp_sess->rtcp_nmembers, p_rtp_sess->rtcp_nsenders,
           (uint32_t)p_rtp_sess);
    ASSERT(p_tmp == NULL, "[RTP UTEST]Member didn't get deleted");
    ASSERT(p_rtp_sess->rtcp_nmembers == 0, "[RTP UTEST] Member number incorrect");
    ASSERT(p_rtp_sess->rtcp_nsenders == 0, "[RTP UTEST] Sender number incorrect");
    check_elements_in_lists(p_rtp_sess, p_rtp_sess->rtcp_nsenders,
                            p_rtp_sess->rtcp_nmembers);

    check_mem_leak(memblocks, p_rtp_sess->rtcp_mem);
    printf("*************** TEST %d PASSED ********************************\n",
           testnum);
    testnum++;
    return SUCCESS;

}



/* Test2
 * Checks: Addition Members with colliding ssrcs (hash collision)
 *         Member cleanup 
 */
static int test_rtp_session2(char *session_type, void *p_session)
{
    rtp_session_t *p_rtp_sess = (rtp_session_t *) p_session;
    rtp_member_t *p_member = NULL;

    printf("\n*************** STARTING TEST %d *****************************\n"
           ,testnum);

    /* Member 1 */
    uint32_t ssrc1, ssrc2;
    ipaddrtype src_addr = 0x03030201;
    char cname1[] = "First STB";
    /* Member 2 */
    char cname2[] = "Second STB";
    uint32_t result, status;

    ssrc1 = rtp_random32(RANDOM_SSRC_TYPE);
    ssrc2 = ssrc1+1;
    while (RTP_HASH(ssrc1,p_rtp_sess->member_hash_size) !=
           RTP_HASH(ssrc2,p_rtp_sess->member_hash_size)) {
      ssrc2++;
    }
    
    /* 
     * Create some members to play with.
     */
    result = MCALL(p_rtp_sess, rtp_create_member, 
                   ssrc1, src_addr, cname1, &p_member);
    ASSERT(result == RTP_SUCCESS, "[RTP UTEST]Couldn't create member 1");       
    src_addr++;
    p_member = NULL;
    result = MCALL(p_rtp_sess, rtp_create_member, 
                   ssrc2, src_addr, cname2, &p_member);
    ASSERT(result == RTP_SUCCESS, "[RTP UTEST]Couldn't create member 2");
    
    ASSERT(p_rtp_sess->rtcp_nsenders == 0, "[RTP UTEST]Incorrect # senders");
    ASSERT(p_rtp_sess->rtcp_nmembers == 2, 
           "[RTP UTEST]Incorrect # members = %d\n",
           p_rtp_sess->rtcp_nmembers );

    status = MCALL(p_rtp_sess, rtp_lookup_member, 
                   ssrc1, src_addr, cname2, &p_member);
    ASSERT(result == RTP_SUCCESS, 
           "[RTP UTEST]Couldn't find member 2, status was 0x%x\n", result);
    printf("[RTP UTEST]Found member 2, cname is %s\n", 
           p_member->sdes[RTCP_SDES_CNAME]);    

    status = MCALL(p_rtp_sess, rtp_lookup_member, 
                   ssrc2, src_addr, cname2, &p_member);
    ASSERT(result == RTP_SUCCESS, 
           "[RTP UTEST]Couldn't find member 2, status was 0x%x\n", result);
    printf("[RTP UTEST]Found member 2, cname is %s\n", 
           p_member->sdes[RTCP_SDES_CNAME]);    


    check_elements_in_lists(p_rtp_sess, p_rtp_sess->rtcp_nsenders,
                            p_rtp_sess->rtcp_nmembers);
    
    /* Now cleanup all the member */
    MCALL(p_rtp_sess, rtp_cleanup_session);
    ASSERT(p_rtp_sess->rtcp_nmembers == 0, 
           "[RTP UTEST] Member number incorrect");
    ASSERT(p_rtp_sess->rtcp_nsenders == 0,
           "[RTP UTEST] Sender number incorrect");
    check_elements_in_lists(p_rtp_sess, p_rtp_sess->rtcp_nsenders,
                            p_rtp_sess->rtcp_nmembers);

    check_mem_leak(memblocks, p_rtp_sess->rtcp_mem);
    printf("*************** TEST %d PASSED ********************************\n"
           ,testnum);
    testnum++;
    return SUCCESS;
}


/* Test3
 * Checks: Generation of SR
 *         Processing of SR
 *         Creating a remote sender
 *         Member added to the slist
 */

static int test_rtp_session3(char *session_type, void *p_session)
{
    rtp_session_t *p_rtp_sess = (rtp_session_t *) p_session;
    rtp_session_t *p_remote_sess = (rtp_session_t *) p_remote_asm;
    rtp_member_t *p_remote_source = NULL;
    uint32_t rtcp_len;
    uint8_t *data_start;
    char cname1[] = "We the VAM";
    ipaddrtype src_addr = 0x03030201;

    printf("\n*************** STARTING TEST %d *****************************\n"
           ,testnum);

    /* Generate a RTCP Sender report */
    data_start = (uint8_t* ) malloc(RTCP_PAK_SIZE);
    /* Create a dummy member just to generate a report */
    p_remote_source = MCALL(p_remote_sess, rtp_create_local_source, 
                           src_addr, cname1, 0);
    ASSERT(p_remote_source != NULL, "[RTP UTEST]Couldn't create local source 1");     
    /* Dummy up sender stats */
    p_remote_source->packets = 100;
    p_remote_source->packets_prior = 50;
    p_remote_source->bytes = 10000;
    p_remote_source->bytes = 5000;
    p_remote_source->rtp_timestamp =  rtp_random32(RANDOM_TIMESTAMP_TYPE);
    p_remote_source->ntp_timestamp = get_ntp_time();
    rtcp_len = MCALL(p_remote_sess, rtcp_construct_report, p_remote_source,
                     (rtcptype *)data_start,  
                     1, FALSE, NULL, FALSE);
        
    MCALL(p_rtp_sess, rtcp_process_packet, src_addr, 0, (char *)data_start, rtcp_len);

    ASSERT(p_rtp_sess->rtcp_nmembers == 1, 
           "[RTP UTEST] Member number incorrect");
    ASSERT(p_rtp_sess->rtcp_nsenders == 1, 
           "[RTP UTEST] Sender number incorrect");
    check_elements_in_lists(p_rtp_sess, p_rtp_sess->rtcp_nsenders,
                            p_rtp_sess->rtcp_nmembers);

   

   /* Now cleanup all the member */
    MCALL(p_rtp_sess, rtp_cleanup_session);
    ASSERT(p_rtp_sess->rtcp_nmembers == 0, 
           "[RTP UTEST] Member number incorrect");
    ASSERT(p_rtp_sess->rtcp_nsenders == 0, 
           "[RTP UTEST] Sender number incorrect");
    check_elements_in_lists(p_rtp_sess, p_rtp_sess->rtcp_nsenders,
                            p_rtp_sess->rtcp_nmembers);

    /* Now cleanup all the member */
    MCALL(p_remote_sess, rtp_cleanup_session);
    ASSERT(p_remote_sess->rtcp_nmembers == 0, 
           "[RTP UTEST] Member number incorrect");
    ASSERT(p_remote_sess->rtcp_nsenders == 0, 
           "[RTP UTEST] Sender number incorrect");
    ASSERT(p_remote_sess->rtp_local_source == NULL, 
           "[RTP UTEST] Local Source non zero");
    check_elements_in_lists(p_remote_sess, p_remote_sess->rtcp_nsenders,
                            p_remote_sess->rtcp_nmembers);

    check_mem_leak(memblocks, p_remote_sess->rtcp_mem);

    printf("*************** TEST %d PASSED ********************************\n"
           ,testnum);
    testnum++;
    return SUCCESS;
}




static void setup_tests()
{
    rtp_init_config(&config, RTPAPP_DEFAULT);
    config.senders_cached = 1;
    config.rtcp_bw_cfg.media_as_bw = RTCP_DFLT_AS_BW;
    /* Init class methods table */
    rtp_asm_init_module();

    if (!rtp_create_session_asm(&config, &p_rtp_asm, TRUE)) {
        printf("[RTP UTEST]Couldn't create Local ASM session.\n");
        return;
    } 

    printf("[RTP UTEST]Created Local ASM session. %x \n", 
           (uint32_t)p_rtp_asm);

    /* 
     * Create an ASM session.
     */

    if (!rtp_create_session_asm(&config, &p_remote_asm, TRUE)) {
        printf("[RTP UTEST]Couldn't create Remote ASM session.\n");
        return;
    }

    printf("[RTP UTEST]Created Remote ASM session. %x \n", 
           (uint32_t)p_remote_asm);

    memblocks = rtcp_memory_allocated(p_rtp_asm->rtcp_mem, NULL);
}

int main(int argc, char **argv)
{
    uint32_t status = SUCCESS;
    rtcp_memory_t *mem;

    setup_tests();

    mem = p_rtp_asm->rtcp_mem;

    /* Run Tests */
    status = test_rtp_session1("ASM session", p_rtp_asm);
    status = test_rtp_session2("ASM session", p_rtp_asm);
    status = test_rtp_session3("ASM session", p_rtp_asm);

    check_mem_leak(memblocks, p_rtp_asm->rtcp_mem);

    rtp_delete_session_asm(&p_remote_asm, TRUE);
    rtp_delete_session_asm(&p_rtp_asm, TRUE);

    if (status != SUCCESS) {
        printf("[RTP UTEST]ASM session test failed!!\n");
    }
    check_mem_leak(0, mem);
    return (0);
}
