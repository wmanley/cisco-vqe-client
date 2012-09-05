/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Upcall unit tests.
 *
 * Documents: 
 *
 *****************************************************************************/
#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "test_vqec_interposers_common.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
#include "vqec_debug.h"
#include "vqec_channel_private.h"
#include "vqec_upcall_event.h"
#include "vqec_pthread.h"
#include <sys/wait.h>


/**---------------------------------------------------------------------------
 * Initialize subsystem.
 *---------------------------------------------------------------------------*/ 
int32_t
test_vqec_upcall_init (void)
{
    (void)vqec_event_init();
    VQEC_SET_DEBUG_FLAG(VQEC_DEBUG_UPCALL);
    return (0);
}


/**---------------------------------------------------------------------------
 * Clean subsystem.
 *---------------------------------------------------------------------------*/ 
int32_t
test_vqec_upcall_clean (void)
{
    VQEC_RESET_DEBUG_FLAG(VQEC_DEBUG_UPCALL);
    return (0);
}

/**---------------------------------------------------------------------------
 * Libevent thread handler. 
 *---------------------------------------------------------------------------*/  
static void *
test_vqec_upcall_run_libevent (void *in)
{
    printf("== start libevent dispatch ==\n");
    vqec_event_dispatch();    
    printf("== end libevent dispatch ==\n");
    return ((void *)0);
}


/**---------------------------------------------------------------------------
 * Test the packet eject path.
 *---------------------------------------------------------------------------*/ 
#define TEST_UPCALL_SETUP_PAKSIZE 128
#define TEST_UPCALL_EJECT_IOV_CNT 3
static void
test_vqec_upcall_punt_path (int32_t fd, struct sockaddr_un *addr)
{
    int32_t bytes_sent, iov_cnt = 0;
    struct msghdr msg;
    struct iovec iov[TEST_UPCALL_EJECT_IOV_CNT];
    vqec_dp_api_version_t ver = VQEC_DP_API_VERSION;
    vqec_dp_pak_hdr_t hdr;
    char bufp[TEST_UPCALL_SETUP_PAKSIZE];
    uint32_t buf_len = sizeof(bufp);

    bzero(&msg, sizeof(msg));
    bzero(&hdr, sizeof(hdr));
    bzero(bufp, sizeof(bufp));

    iov[iov_cnt].iov_base = &ver;  /* version sub-block */
    iov[iov_cnt].iov_len = sizeof(ver);
    iov_cnt++;

    iov[iov_cnt].iov_base = &hdr;  /* packet header sub-block */
    iov[iov_cnt].iov_len = sizeof(hdr);
    iov_cnt++;
    
    iov[iov_cnt].iov_base = bufp;  /* packet buffer sub-block */
    iov[iov_cnt].iov_len = buf_len;
    iov_cnt++;

                                /* bad content length */
    msg.msg_iov = iov;
    msg.msg_iovlen = iov_cnt;
    
    msg.msg_name = (void *)addr;
    msg.msg_namelen = SUN_LEN(addr);
    
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);

                                /* bad version */
    ver = -1;
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    ver = VQEC_DP_API_VERSION;
    
                                /* bad length */
    msg.msg_iovlen = 1;
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    msg.msg_iovlen = iov_cnt;

    hdr.length = buf_len;
    hdr.cp_handle = 0;          /* invalid handle */
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    sleep(2);

}


/**---------------------------------------------------------------------------
 * Test the IRQ path.
 *---------------------------------------------------------------------------*/ 
#define TEST_UPCALL_IRQ_IOV_CNT 2
static void
test_vqec_upcall_irq_path (int32_t fd, struct sockaddr_un *addr)
{
    int32_t i, bytes_sent, iov_cnt = 0;
    struct msghdr msg;
    struct iovec iov[TEST_UPCALL_IRQ_IOV_CNT];
    vqec_dp_api_version_t ver = VQEC_DP_API_VERSION;
    vqec_dp_upcall_irq_event_t ev;
    vqec_chan_t chan;
    vqec_chanid_t chanid;
    vqec_channel_module_t *m_cache;
    vqec_dp_irq_poll_t poll;
    vqec_dp_irq_ack_resp_t ack;
    uint32_t upc_events = 0, upc_lost_events = 0, upc_error_events = 0, upc_ack_errors = 0;
    vqec_upcall_ev_stats_t stats;

    bzero(&ev, sizeof(ev));
    bzero(&msg, sizeof(msg));
    bzero(&chan, sizeof(chan));
    bzero(&poll, sizeof(poll));
    bzero(&ack, sizeof(ack));
    bzero(&stats, sizeof(stats));

    iov[iov_cnt].iov_base = &ver;  /* version sub-block */
    iov[iov_cnt].iov_len = sizeof(ver);
    iov_cnt++;
        
    iov[iov_cnt].iov_base = &ev;  /* event sub-block */
    iov[iov_cnt].iov_len = sizeof(ev);
    iov_cnt++;
        
    msg.msg_iov = iov;
    msg.msg_iovlen = iov_cnt;
        
    msg.msg_name = (void *)addr;
    msg.msg_namelen = SUN_LEN(addr);

                                /* bad cp id */
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    upc_error_events++;

                                /* bad version */
    ver = -1;
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    ver = VQEC_DP_API_VERSION;
    upc_events++;
    upc_error_events++;
    
                                /* bad length */
    msg.msg_iovlen = 1;
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    msg.msg_iovlen = iov_cnt;
    upc_events++;
    upc_error_events++;

    m_cache = g_channel_module;
    g_channel_module = malloc(sizeof(*g_channel_module));
    CU_ASSERT(g_channel_module != NULL);
    g_channel_module->id_table_key =
        id_create_new_table(1, 2);
    CU_ASSERT(g_channel_module->id_table_key);
    chanid = id_get(&chan, g_channel_module->id_table_key);
    CU_ASSERT(chanid != VQEC_CHANID_INVALID);

                                /* bad device (=0) */
    msg.msg_iovlen = iov_cnt;
    ev.cp_handle = chanid;
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    upc_error_events++;

                                /* bad device (=MAX) */
    ev.device = VQEC_DP_UPCALL_DEV_MAX+1;
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    upc_error_events++;

                                /* bad sequence number: 
                                   the 1st seq must be = 1 - 
                                   command will issue a poll., and the
                                   poll will return invalidargs */
    test_vqec_dp_chan_poll_upcall_irq_set_hook(TRUE, 
                                               VQEC_DP_ERR_INVALIDARGS, NULL);  
    ev.device = VQEC_DP_UPCALL_DEV_DPCHAN;
    ev.chan_generation_num = 1;
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    upc_lost_events++;
    upc_ack_errors++;
    sleep(1);                   /* allow poll() to occur */

                                /* no data in the poll [empty] */
    test_vqec_dp_chan_poll_upcall_irq_set_hook(TRUE, VQEC_DP_ERR_OK, &poll); 
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    upc_lost_events++;
    sleep(1);

                                /* cause errors for devices in the poll */
    for (i = 0; i < VQEC_DP_UPCALL_DEV_MAX; i++) {
        poll.response[i].ack_error = VQEC_DP_ERR_INVALIDARGS;
    }
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    upc_lost_events++;
    sleep(1);                   /* allow poll() to occur */

                                /* deliver messages in the poll */
    for (i = 0; i < VQEC_DP_UPCALL_DEV_MAX; i++) {
        poll.response[i].ack_error = VQEC_DP_ERR_OK;
        if (i != (VQEC_DP_UPCALL_DEV_DPCHAN - 1)) {
            poll.response[i].irq_reason_code = 
                VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_RTP_SRC_NEW);
        } else {
            poll.response[i].irq_reason_code = 
                VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_CHAN_RCC_ABORT) |
                VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_CHAN_RCC_NCSI) |
                VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_CHAN_FAST_FILL_DONE) |
                VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_CHAN_GEN_NUM_SYNC);
        }
    }
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    upc_lost_events++;
    sleep(1);                   /* allow poll() to occur */

                                /* bad generation id: the channel
                                   generation id is 0. */
    ev.cp_handle = chanid;
    ev.chan_generation_num = 2;
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    ev.chan_generation_num = 1;
    upc_events++;
    upc_error_events++;
    sleep(1);

    ev.upcall_generation_num = 1;  /* good sequence number */
    test_vqec_dp_chan_ack_upcall_irq_set_hook(TRUE, 
                                              VQEC_DP_ERR_INVALIDARGS,
                                              VQEC_DP_UPCALL_DEV_DPCHAN, 
                                              NULL); 
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    upc_ack_errors++;
    sleep(1);
    ev.upcall_generation_num++;
    test_vqec_dp_chan_ack_upcall_irq_set_hook(TRUE, 
                                              VQEC_DP_ERR_INVALIDARGS,
                                              VQEC_DP_UPCALL_DEV_RTP_PRIMARY, 
                                              NULL); 
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    upc_ack_errors++;
    sleep(1);
    ev.upcall_generation_num++;
    test_vqec_dp_chan_ack_upcall_irq_set_hook(TRUE, 
                                              VQEC_DP_ERR_INVALIDARGS,
                                              VQEC_DP_UPCALL_DEV_RTP_REPAIR, 
                                              NULL); 
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    upc_events++; 
    upc_ack_errors++;
    sleep(1);

                                /* an Ack with reason code null */
    ev.upcall_generation_num++;
    test_vqec_dp_chan_ack_upcall_irq_set_hook(TRUE, 
                                              VQEC_DP_ERR_OK,
                                              VQEC_DP_UPCALL_DEV_DPCHAN, 
                                              &ack); 
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    sleep(1);

    ev.upcall_generation_num++;
    ev.device = VQEC_DP_UPCALL_DEV_DPCHAN;  /* chan device */
    ack.irq_reason_code = 
        VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_CHAN_RCC_ABORT) |
        VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_CHAN_RCC_NCSI) |
        VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_CHAN_FAST_FILL_DONE) |
        VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_CHAN_GEN_NUM_SYNC);
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);    
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    sleep(1);
    
    ev.upcall_generation_num++;    
    ev.device = VQEC_DP_UPCALL_DEV_RTP_PRIMARY;  /* primary device */
    ack.irq_reason_code = 
        VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_RTP_SRC_NEW);
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);    
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    sleep(1);

    ev.upcall_generation_num++;
    ev.device = VQEC_DP_UPCALL_DEV_RTP_REPAIR;  /* repair device */
    ack.irq_reason_code = 
        VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_RTP_SRC_ISINACTIVE) |
        VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_RTP_SRC_CSRC_UPDATE);
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);    
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    sleep(1);

                                /* event loss - will poll the dataplane 
                                   for events */
    ev.upcall_generation_num += 2;
    ev.device = VQEC_DP_UPCALL_DEV_RTP_REPAIR;  /* repair device */
    ack.irq_reason_code = 
        VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_RTP_SRC_ISINACTIVE) |
        VQEC_DP_UPCALL_MAKE_REASON(VQEC_DP_UPCALL_REASON_RTP_SRC_CSRC_UPDATE);
    bytes_sent = sendmsg(fd, 
                         &msg,
                         MSG_DONTWAIT);    
    CU_ASSERT(bytes_sent > 0);
    upc_events++;
    upc_lost_events++;
    sleep(1);
    
    printf(" Total Events:              %u\n",
          upc_events);
    printf(" Lost Events:               %u\n",
           upc_lost_events);
    printf(" Error Events:              %u\n",
           upc_error_events);
    printf(" Ack Errors:                %u\n",
           upc_ack_errors);

    vqec_upcall_get_irq_stats(&stats);

    CU_ASSERT(stats.upcall_events == upc_events);
    CU_ASSERT(stats.upcall_lost_events == upc_lost_events);
    CU_ASSERT(stats.upcall_error_events == upc_error_events);
    CU_ASSERT(stats.upcall_ack_errors == upc_ack_errors);
}


/**---------------------------------------------------------------------------
 * Upcall socket setup.
 *---------------------------------------------------------------------------*/ 
static void
test_upcall_setup_sock (void)
{
    int32_t irq_sock, pak_sock;
    pthread_t tid;
    struct sockaddr_un pak_addr, irq_addr;

    CU_ASSERT(vqec_upcall_setup_unsocks(0) != VQEC_OK);
    test_malloc_make_fail(1);
    CU_ASSERT(vqec_upcall_setup_unsocks(
                  TEST_UPCALL_SETUP_PAKSIZE) != VQEC_OK);
    test_malloc_fail_nth_alloc(1);
    CU_ASSERT(vqec_upcall_setup_unsocks(
                  TEST_UPCALL_SETUP_PAKSIZE) != VQEC_OK);
    test_malloc_fail_nth_alloc(2);
    CU_ASSERT(vqec_upcall_setup_unsocks(
                  TEST_UPCALL_SETUP_PAKSIZE) != VQEC_OK);
    test_malloc_make_fail(0);
    test_malloc_fail_nth_alloc(0);

    test_calloc_make_fail(1);
    CU_ASSERT(vqec_upcall_setup_unsocks(
                  TEST_UPCALL_SETUP_PAKSIZE) != VQEC_OK);
    test_calloc_make_fail(0);

    test_vqec_dp_create_upcall_socket_set_hook(TRUE, VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_upcall_setup_unsocks(
                  TEST_UPCALL_SETUP_PAKSIZE) == VQEC_OK);

    test_vqec_dp_upcall_socket_fd(VQEC_UPCALL_SOCKTYPE_IRQEVENT, 
                                  &irq_sock,
                                  &irq_addr);
    CU_ASSERT(irq_sock > 0);
    test_vqec_dp_upcall_socket_fd(VQEC_UPCALL_SOCKTYPE_PAKEJECT,
                                  &pak_sock,
                                  &pak_addr);
    CU_ASSERT(pak_sock > 0);

                                /* dispatch libevent */
    CU_ASSERT(
        !vqec_pthread_create(&tid, test_vqec_upcall_run_libevent, NULL)); 
    (void)pthread_detach(tid);
    test_vqec_upcall_irq_path(irq_sock, &irq_addr);
    test_vqec_upcall_punt_path(pak_sock, &pak_addr);
    vqec_upcall_display_state();    

    vqec_event_loopexit(NULL);

                                /* clean-up all the state */
    vqec_upcall_close_unsocks();
    test_malloc_make_fail(0);
    test_malloc_fail_nth_alloc(0);
    test_calloc_make_fail(0);
    test_vqec_dp_create_upcall_socket_set_hook(FALSE, 0);
    test_vqec_dp_chan_poll_upcall_irq_set_hook(FALSE, 0, NULL); 
    test_vqec_dp_chan_ack_upcall_irq_set_hook(FALSE, 0, 0, NULL);
}


CU_TestInfo test_array_upcall[] =
{
    { "test_upcall_setup_sock", test_upcall_setup_sock },
    
    CU_TEST_INFO_NULL
};

