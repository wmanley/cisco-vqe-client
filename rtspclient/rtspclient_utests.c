/* 
 * RTSP Client unit test program.
 *
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utils/vam_types.h>
#include "rtsp_client.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "vqec_updater.h"

static pthread_t server_thread_id;

void *server_thread_activation(void *arg);

#define NAME_SIZE 40

/* sa_ignore DISABLE_RETURNS CU_assertImplementation */
/* sa_ignore DISABLE_RETURNS sleep */
/* sa_ignore DISABLE_RETURNS system */
/* sa_ignore DISABLE_RETURNS pthread_create */

/***********************************************************************
 *
 *     RTSP CLIENT SUITE TESTS START HERE
 *
 **********************************************************************/
int rtspclient_testsuite_init (void)
{
    return 0;
}


int rtspclient_testsuite_cleanup (void)
{
    return 0;
}

void test_rtsp_client_init (void)
{
    rtsp_ret_e ret;
    char machine_name[NAME_SIZE];
    
    memset(machine_name, 0, NAME_SIZE);
    ret = rtsp_client_init(machine_name, 8554);
    CU_ASSERT_NOT_EQUAL(ret, RTSP_SUCCESS);

    /* Start the RTSP Server */
    pthread_create(&server_thread_id, NULL,
                   &server_thread_activation, NULL);

    sleep(2);
    /* sa_ignore IGNORE_RETURN */
    gethostname(machine_name, NAME_SIZE);
    ret = rtsp_client_init(machine_name, 8554);
    CU_ASSERT_EQUAL(ret, RTSP_SUCCESS);

    ret = rtsp_client_close();
    CU_ASSERT_EQUAL(ret, RTSP_SUCCESS);

    system("kill -9 `pidof VCDServer`");
}

void test_rtsp_client_request (void)
{
    rtsp_ret_e ret;
    char machine_name[NAME_SIZE];
    
    /* Start the RTSP Server */
    pthread_create(&server_thread_id, NULL,
                   &server_thread_activation, NULL);

    sleep(2);
    /* sa_ignore IGNORE_RETURN */    
    gethostname(machine_name, NAME_SIZE);
    ret = rtsp_client_init(machine_name, 8554);
    CU_ASSERT_EQUAL(ret, RTSP_SUCCESS);

    ret = rtsp_send_request(
        "vqe-channels", MEDIA_TYPE_APP_SDP,
        TIME_ADD_A_R(get_sys_time(), VQEC_UPDATER_TIMEOUT_BACKGROUND));
    CU_ASSERT_EQUAL(ret, RTSP_SUCCESS);

    CU_ASSERT_EQUAL(rtsp_get_response_code(), MSG_200_OK);

    ret = rtsp_client_close();
    CU_ASSERT_EQUAL(ret, RTSP_SUCCESS);

    system("kill -9 `pidof VCDServer`");
    sleep(2);
}

void *server_thread_activation (void *arg)
{
    system("../rtsp/obj/dev/VCDServer -f VQEChanCfgServer.cfg");
    pthread_exit(NULL);
}
