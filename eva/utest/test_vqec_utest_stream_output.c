/*
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include <assert.h>
#include <arpa/inet.h>
#include "vqec_stream_output_thread_mgr.h"
#include "vqec_ifclient.h"
#include "vqec_syscfg.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

/*
 * Unit tests for vqec_stream_output
 */
static vqec_tunerid_t tuner_id1, tuner_id2, tuner_id3;
static in_addr_t dest1, dest2, dest3;
static in_port_t port1, port2, port3;
static in_addr_t output_if;
static vqec_stream_output_thread_t *output1;
static char *url = "udp://224.1.1.1:50000";

UT_STATIC vqec_stream_output_thread_t * 
vqec_stream_output_thread_get_by_tuner_id (vqec_tunerid_t tuner_id);

UT_STATIC int vqec_stream_output_get_ip_address_by_if (char * ifname, 
                                                        char * name, 
                                                        int size);

UT_STATIC void vqec_stream_output_thread_create (vqec_tunerid_t tuner_id,
                                                 short output_prot,
                                                 in_addr_t output_dest,
                                                 in_port_t output_port,
                                                 in_addr_t output_if);

UT_STATIC void vqec_stream_output_thread_destroy (vqec_tunerid_t tuner_id);

int test_vqec_stream_output_init (void) {
    vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    vqec_stream_output_thread_mgr_module_init();
    return 0;
}

int test_vqec_stream_output_clean (void) {
    vqec_stream_output_thread_mgr_module_deinit();
    vqec_ifclient_deinit();
    return 0;
}

static void test_vqec_stream_output_create (void) {

    char name[ INET_ADDRSTRLEN ];

    CU_ASSERT(vqec_ifclient_tuner_create(&tuner_id1, NULL) == VQEC_OK);
    CU_ASSERT(vqec_ifclient_tuner_create(&tuner_id2, NULL) == VQEC_OK);
    CU_ASSERT(vqec_ifclient_tuner_create(&tuner_id3, NULL) == VQEC_OK);

    dest1 = inet_addr("5.1.1.1");
    port1 = htons(5000);

    dest2 = inet_addr("5.1.1.2");
    port2 = htons(5001);

    dest3 = inet_addr("5.1.1.3");
    port3 = htons(5002);

    CU_ASSERT(vqec_stream_output_get_ip_address_by_if("lo", 
                                                        name, 
                                                        INET_ADDRSTRLEN) ==
               1);

    output_if = inet_addr(name);
}

static void test_vqec_stream_output_thread_create (void) {
    vqec_stream_output_thread_create(tuner_id1,
                                     0, 
                                     dest1,
                                     port1,
                                     output_if);
    vqec_stream_output_thread_create(tuner_id2,
                                     0, 
                                     dest1,
                                     port1,
                                     output_if);
    vqec_stream_output_thread_create(tuner_id2,
                                     0, 
                                     dest2,
                                     port2,
                                     output_if);
    vqec_stream_output_thread_create(tuner_id3,
                                     0, 
                                     dest3,
                                     port3,
                                     output_if);

    output1 = vqec_stream_output_thread_get_by_tuner_id(tuner_id1);
    CU_ASSERT(output1->thread_id != -1);
    CU_ASSERT(output1->output_sock == NULL);
    CU_ASSERT(output1->changed == 1);
    CU_ASSERT(output1->exit == 0);


    sleep(1);
    

    CU_ASSERT(output1->thread_id != -1);
    CU_ASSERT(output1->output_sock != NULL);
    CU_ASSERT(output1->changed == 0);
    CU_ASSERT(output1->exit == 0);
}

static void test_vqec_stream_output_thread_destroy (void) {
    CU_ASSERT(vqec_stream_output_thread_get_by_tuner_id(tuner_id1) != NULL);
    vqec_stream_output_thread_destroy(tuner_id1);
    CU_ASSERT(vqec_stream_output_thread_get_by_tuner_id(tuner_id1) == NULL);
}

static void test_vqec_stream_output (void) {
    char name[ VQEC_MAX_NAME_LEN ];
    vqec_error_t ret;

    CU_ASSERT(vqec_ifclient_tuner_get_name_by_id(tuner_id1, name, 
                                                   VQEC_MAX_NAME_LEN) ==
               VQEC_OK);
    ret = vqec_stream_output(name,
                             "lo",
                             url);
    
    CU_ASSERT(ret == VQEC_OK);
}

static void test_vqec_stream_output_channel_leave_call_back_func (void) {
    vqec_stream_output_channel_leave_call_back_func(url, 
                                                    tuner_id1,
                                                    output_if,
                                                    dest1,
                                                    port1,
                                                    NULL);

    sleep(1);
    output1 = vqec_stream_output_thread_get_by_tuner_id(tuner_id1);
    CU_ASSERT(output1->thread_id != -1);
    //CU_ASSERT(output1->output_sock == NULL);
    CU_ASSERT(output1->changed == 0);
    CU_ASSERT(output1->exit == 0);
}

static void test_vqec_stream_output_channel_join_call_back_func (void) {
    vqec_stream_output_channel_join_call_back_func(url, 
                                                   tuner_id1,
                                                   output_if,
                                                   dest1,
                                                   port1,
                                                   NULL);
    output1 = vqec_stream_output_thread_get_by_tuner_id(tuner_id1);
    CU_ASSERT(output1->thread_id != -1);

    sleep(1);
    

    CU_ASSERT(output1->thread_id != -1);
    CU_ASSERT(output1->output_sock != NULL);
    CU_ASSERT(output1->changed == 0);
    CU_ASSERT(output1->exit == 0);
}

CU_TestInfo test_array_stream_output[] = {
    {"test vqec_stream_output_create",test_vqec_stream_output_create},
    {"test vqec_stream_output_thread_create",
     test_vqec_stream_output_thread_create},
    {"test vqec_stream_output_thread_destroy",
     test_vqec_stream_output_thread_destroy},
    {"test vqec_stream_output",
     test_vqec_stream_output},
    {"test vqec_stream_output_channel_leave_call_back_func",
     test_vqec_stream_output_channel_leave_call_back_func},
    {"test vqec_stream_output_channel_join_call_back_func",
     test_vqec_stream_output_channel_join_call_back_func},
    CU_TEST_INFO_NULL,
};

