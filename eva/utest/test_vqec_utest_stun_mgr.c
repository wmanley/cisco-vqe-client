/*
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vqec_stun_mgr.h"
#include <stdint.h>
#include "../include/utils/vam_util.h"
#include "vqec_repair_port_mgr.h"
#include "vqec_recv_socket.h"
#include "vqec_stun_mgr.h"
#include "vqec_error.h"
#include "vqec_debug.h"
#include <arpa/inet.h>
#include "vqec_src_mgr.h"
#include <stdlib.h>

/*
 * Unit tests for vqec_stun_mgr
 */
static vqec_recv_sock_t * test_sock1, * test_sock2, * test_sock3;
static vqec_stun_server_t test_stun_server;
static in_addr_t server_addr;
static in_port_t server_port;
static boolean retval;

vqec_stun_mgr_id 
vqec_stun_mgr_find (vqec_stun_mapped_address_t internal_addr);

int test_vqec_stun_mgr_init (void) {
    test_malloc_make_fail(0);
    vqec_ifclient_init("data/cfg_test_all_params_valid2.cfg");
    vqec_stun_cfg_module_init();
    /* start stun server - currently test exe path set to x86 arch build path*/
    system("../../../stun/x86-utrun/test_stun_server &");
    sleep(5);
    server_addr = inet_addr("127.0.0.1");
    server_port = htons(3478);
    return 0;
}

int test_vqec_stun_mgr_clean (void) {
    vqec_ifclient_deinit();
    vqec_stun_cfg_module_deinit();
    system("killall test_stun_server &");
    return 0;
}

static void test_vqec_stun_mgr_create (void) {

    /* working malloc */
    test_malloc_make_cache(1);
    vqec_stun_mgr_create(32, 30);
    test_malloc_make_cache(0);
}

static void test_vqec_stun_server_set (void) {
    vqec_stun_server_t server;
    memset(&server, 0, sizeof(vqec_stun_server_t));

    vqec_stun_server_get_random(&server);

    CU_ASSERT(server.addr == 0 && server.port == 0);
        
    CU_ASSERT(vqec_stun_server_add(inet_addr("127.0.0.1"),
                                   htons(3478)) == VQEC_OK);
    vqec_stun_server_get_random(&test_stun_server);
    CU_ASSERT(test_stun_server.addr == inet_addr("127.0.0.1") &&
              test_stun_server.port ==htons(3478));
}

static void test_vqec_stun_mgr_add_socket (void) {

    // struct sockaddr_in saddr;
    vqec_stun_mapped_address_t internal_addr;
    uint8_t rtcp_dscp_value = 24;

    struct sockaddr_in socketAddress;
    socklen_t socketAddressLength = sizeof(struct sockaddr_in);

    vqec_syscfg_t cfg;

    vqec_syscfg_get(&cfg);

    /* working malloc */
    test_sock1 = vqec_recv_sock_create("TEST_SOCK1",
                                       INADDR_ANY,
                                       0,              /* random port */
                                       INADDR_ANY,
                                       FALSE,
                                       cfg.so_rcvbuf,
                                       rtcp_dscp_value);
    if(getsockname(test_sock1->fd, (struct sockaddr *)&socketAddress, 
                   &socketAddressLength) < 0) {        
        perror("can't getsockname");
        CU_ASSERT(0);
        return;
    } else {
        test_sock1->port = socketAddress.sin_port;
    }

    

    test_sock2 = vqec_recv_sock_create("TEST_SOCK2",
                                       INADDR_ANY,
                                       0,              /* random port */
                                       INADDR_ANY,
                                       FALSE,
                                       cfg.so_rcvbuf,
                                       rtcp_dscp_value);
    if(getsockname(test_sock2->fd, (struct sockaddr *)&socketAddress, 
                   &socketAddressLength) < 0) {        
        perror("can't getsockname");
        CU_ASSERT(0);
        return;
    } else {
        test_sock2->port = socketAddress.sin_port;
    }

    test_sock3 = vqec_recv_sock_create("TEST_SOCK3",
                                       INADDR_ANY,
                                       0,              /* random port */
                                       INADDR_ANY,
                                       FALSE,
                                       cfg.so_rcvbuf,
                                       rtcp_dscp_value);
    if(getsockname(test_sock3->fd, (struct sockaddr *)&socketAddress, 
                   &socketAddressLength) < 0) {        
        perror("can't getsockname");
        CU_ASSERT(0);
        return;
    } else {
        test_sock3->port = socketAddress.sin_port;
    }

    internal_addr.addr = server_addr;
    internal_addr.port = test_sock1->port;
    
    retval = vqec_stun_mgr_add(internal_addr,
                               server_addr,
                               server_port);
    CU_ASSERT(retval == TRUE);


    internal_addr.addr = server_addr;
    internal_addr.port = test_sock2->port;
    retval = vqec_stun_mgr_add(internal_addr,
                               server_addr,
                               server_port);
    CU_ASSERT(retval == TRUE);

    internal_addr.addr = server_addr;
    internal_addr.port = test_sock3->port;
    retval = vqec_stun_mgr_add(internal_addr,
                               server_addr,
                               server_port);
    CU_ASSERT(retval == TRUE);
}

static void test_vqec_stun_mgr_find_socket (void) {

    vqec_stun_mapped_address_t internal_addr;

    /*
     * the vqec_stun_mgr_id's here for test_socks 1 and 2 will be 0 and 1, as
     * the vqec_stun_mgr assigns id's sequentially (as available) on add
     */

    internal_addr.addr = server_addr;
    internal_addr.port = test_sock1->port;
    CU_ASSERT(vqec_stun_mgr_find(internal_addr) == 0);

    internal_addr.port = test_sock2->port;
    CU_ASSERT(vqec_stun_mgr_find(internal_addr) == 1);

    internal_addr.port = 0;
    CU_ASSERT(vqec_stun_mgr_find(internal_addr) == VQEC_STUN_MGR_INVALID_ID);
}

static void test_vqec_stun_mgr_delete_socket (void) {

    vqec_stun_mapped_address_t internal_addr;

    internal_addr.addr = server_addr;
    internal_addr.port = test_sock1->port;
    CU_ASSERT(vqec_stun_mgr_delete(test_sock1->port) == TRUE);
    CU_ASSERT(vqec_stun_mgr_find(internal_addr) == 
              VQEC_STUN_MGR_INVALID_ID);
}

static void test_vqec_stun_mgr_destroy (void) {
    test_malloc_make_cache(0);
    /* can't destroy here, otherwise repair port mgr can't be destroyed */
    /* vqec_stun_mgr_destroy();*/
}

CU_TestInfo test_array_stun_mgr[] = {
    {"test vqec_stun_mgr_create",test_vqec_stun_mgr_create},
    {"test vqec_stun_server_set",test_vqec_stun_server_set},
    {"test vqec_stun_mgr_add_socket",test_vqec_stun_mgr_add_socket},
    {"test vqec_stun_mgr_find_socket",test_vqec_stun_mgr_find_socket},
    {"test vqec_stun_mgr_delete_socket",test_vqec_stun_mgr_delete_socket},
    {"test vqec_stun_mgr_destroy",test_vqec_stun_mgr_destroy},
    CU_TEST_INFO_NULL,
};

