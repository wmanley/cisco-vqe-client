/*
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "vqec_recv_socket.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
#include <sys/socket.h>
#include <arpa/inet.h>

#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif

/*
 *  Unit tests for vqec_recv_sock
 */

#define PORT 5678
#define IF_ADDR "127.0.0.1"
#define MCAST_GRP "224.1.1.1"
#define VQEC_UTEST_RCV_BUFF_BYTES 1514

static struct in_addr s_any_src_addr;

vqec_recv_sock_t *testsock, *testsock_mcast, *testsock_rsrv;
//int pak_stream;

int test_vqec_recv_socket_init (void) {
    /* make sure that mallocs are working properly at init */
    test_malloc_make_fail(0);
    /* track each malloc to make sure it is freed */
    test_malloc_make_cache(1);
    s_any_src_addr.s_addr = INADDR_ANY;
    return(0);
}

int test_vqec_recv_socket_clean (void) {
    test_malloc_make_cache(0);
    test_malloc_make_fail(0);
    return 0;
}

static void test_vqec_recv_sock_create (void) {
    /* no mcast_group, no mem */
    struct in_addr if_addr;
    uint8_t rtcp_dscp_value = 24;
    inet_pton(AF_INET, IF_ADDR, &if_addr);
    testsock = vqec_recv_sock_create("TEST socket",
  				    if_addr.s_addr,
 				    PORT,
 				    0,
 				    0,
 				    VQEC_UTEST_RCV_BUFF_BYTES,
                                    rtcp_dscp_value);
    CU_ASSERT_NOT_EQUAL(testsock,NULL);
    CU_ASSERT_EQUAL(diff_malloc_free(),2);
    
    /* mcast_group, no mem */
    struct in_addr mcast_grp_addr;
    inet_pton(AF_INET, MCAST_GRP, &mcast_grp_addr);
    testsock_mcast = vqec_recv_sock_create("TEST socket mcast",
  				    0,
 				    PORT,
				    mcast_grp_addr.s_addr,
 				    0,
				    VQEC_UTEST_RCV_BUFF_BYTES,
                                    rtcp_dscp_value);
    CU_ASSERT_NOT_EQUAL(testsock_mcast,NULL);
    CU_ASSERT_EQUAL(diff_malloc_free(),4);

    /* reserve socket */
    boolean rv;
    in_addr_t rsrv_addr = 0;
    in_port_t rsrv_port = 0;
    rv = vqec_recv_sock_open(&rsrv_addr, &rsrv_port, NULL);
    CU_ASSERT_EQUAL(rv, TRUE);
    CU_ASSERT_NOT_EQUAL(rsrv_port, 0);
    CU_ASSERT_EQUAL(diff_malloc_free(), 5);
    testsock_rsrv = vqec_recv_sock_create("TEST socket reserve",
                                          rsrv_addr,
                                          rsrv_port,
                                          0,
                                          0,
                                          VQEC_UTEST_RCV_BUFF_BYTES,
                                          rtcp_dscp_value);
    CU_ASSERT_NOT_EQUAL(testsock_rsrv, NULL);
    CU_ASSERT_EQUAL(diff_malloc_free(), 6);
}

static void test_vqec_recv_sock_get_rcv_if_address (void) {
    struct in_addr got_addr;
    char test_addr[20];
    got_addr = vqec_recv_sock_get_rcv_if_address(testsock);
    inet_ntop(AF_INET, &got_addr, test_addr, 20);
    CU_ASSERT_STRING_EQUAL(IF_ADDR,test_addr);
}

static void test_vqec_recv_sock_get_port (void) {
    in_port_t got_port;
    got_port = vqec_recv_sock_get_port(testsock);
    CU_ASSERT_EQUAL(PORT,got_port);
}

static void test_vqec_recv_sock_get_blocking (void) {
    boolean got_blocking;
    got_blocking = vqec_recv_sock_get_blocking(testsock);
    CU_ASSERT_EQUAL(0,got_blocking);
    
}

static void test_vqec_recv_sock_get_rcv_buff_bytes (void) {
    int got_buff_bytes;
    got_buff_bytes = vqec_recv_sock_get_rcv_buff_bytes(testsock);
    CU_ASSERT_EQUAL(VQEC_UTEST_RCV_BUFF_BYTES,got_buff_bytes);
}

static void test_vqec_recv_sock_get_mcast_group (void) {
    struct in_addr got_mcast_grp;
    char test_mcast_grp[20];
    got_mcast_grp = vqec_recv_sock_get_mcast_group(testsock_mcast);
    inet_ntop(AF_INET, &got_mcast_grp, test_mcast_grp, 20);
    CU_ASSERT_STRING_EQUAL(MCAST_GRP,test_mcast_grp);
}

static void test_vqec_recv_sock_mcast_join (void) {
   // vqec_recv_sock_mcast_join(vqec_recv_sock_t *sock)
   vqec_recv_sock_mcast_join(testsock, s_any_src_addr);
}

static void test_vqec_recv_sock_mcast_leave (void) {
    vqec_recv_sock_mcast_leave(testsock, s_any_src_addr);
    vqec_recv_sock_mcast_leave(testsock_mcast, s_any_src_addr);
}

#define SRC_PORT 0

void test_vqec_send_socket_create (void) {
    int for_sending_to_peer;
    struct in_addr src_addr;
    uint8_t rtcp_dscp_value = 24;
    inet_pton(AF_INET, IF_ADDR, &src_addr);
    for_sending_to_peer = 
        vqec_send_socket_create(src_addr.s_addr, htons(SRC_PORT), 
                                rtcp_dscp_value);
    CU_ASSERT_NOT_EQUAL(for_sending_to_peer,-1);
    if (for_sending_to_peer != -1) {
        close(for_sending_to_peer);
    }
}

static void test_vqec_recv_sock_ref (void) {
    int lastref;
    lastref = testsock->ref_count;
    vqec_recv_sock_ref(testsock);
    CU_ASSERT_EQUAL(lastref+1,testsock->ref_count);
}

#define BUF_LEN 4096

void test_vqec_recv_sock_read (void) {
    struct timeval recv_time;
    char buf[BUF_LEN];
    //char expected_buf[BUF_LEN];
    uint32_t buf_len = BUF_LEN;
    int recv_len = 0;
    struct in_addr src_addr;
    uint16_t src_port;
    uint8_t rtcp_dscp_value = 24;
    vqec_pak_t pak;

    pak.buff = buf;
    *((uint32_t *)&pak.alloc_len) = buf_len;

    //send a packet to a loopback socket
    /*int pak_stream;
    char pak_buf[1370];
    pak_stream = open("vam_stream.pak", O_RDONLY);
    read(pak_stream, &pak_buf, 1370);
    testsock_mcast->fd = socket(AF_UNIX,SOCK_DGRAM,0);
    fcntl(testsock_mcast->fd, F_SETFL, O_NONBLOCK);
    write(testsock_mcast->fd, &pak_buf, 1370);
    */
    vqec_recv_sock_t * read_sock;
    read_sock = vqec_recv_sock_create("read_sock",
                                     INADDR_ANY,
                                     htons(50000),
                                     htonl((224 << 24) | (1 << 16) | (1 << 8) | 1),
                                     0,
                                     100000,
                                     rtcp_dscp_value);
    vqec_recv_sock_mcast_join(read_sock, s_any_src_addr);

    int num_received, num_failed;
    
    /* test to make sure it works correctly given appropriate circumstances */
    buf_len = BUF_LEN;
    num_received = 0;
    num_failed = 0;
    while(num_received < 1 && num_failed < 10000)
    {
      recv_len = vqec_recv_sock_read_pak(read_sock, &pak);

      recv_time = abs_time_to_timeval(pak.rcv_ts);
      src_addr = pak.src_addr;
      src_port = pak.src_port;

      if (recv_len > 0)
      {
        num_received++;
        //PRINT_BUF_HEX("buf",buf,buf_len);
        //read(pak_stream, &expected_buf, buf_len);
        //PRINT_BUF_HEX("e_buf",expected_buf,buf_len);
      } else {
        num_failed++;
      }  
    }
    CU_ASSERT_EQUAL(buf_len,1316);

    /* make CMSG_FIRSTHDR return NULL, simulating an "extra message" */
/*    buf_len = BUF_LEN;
    num_received = 0;
    num_failed = 0;
    test_recvmsg_make_fail(2);
    while(num_received < 1 && num_failed < 10000)
    {
    recv_len = vqec_recv_sock_read(read_sock, &recv_time, buf, buf_len,
                         &src_addr, &src_port);
      if (recv_len > 0)
      {
        num_received++;
      } else {
        num_failed++;
      }
    }
    CU_ASSERT(recv_len != BUF_LEN && num_received == 0);
*/
    /* make recvmsg() fail with error code EAGAIN */
    buf_len = BUF_LEN;
    num_received = 0;
    num_failed = 0;
    test_recvmsg_make_fail(EAGAIN);
    while(num_received < 1 && num_failed < 10)
    {
      recv_len = vqec_recv_sock_read_pak(read_sock, &pak);

      if (recv_len > 0)
      {
        num_received++;
      } else {
        num_failed++;
      }  
    }
    CU_ASSERT_NOT_EQUAL(recv_len,1316);
    
    /* make recvmsg() fail with other error code */
    buf_len = BUF_LEN;
    num_received = 0;
    num_failed = 0;
    test_recvmsg_make_fail(ENOMEM);
    while(num_received < 1 && num_failed < 10)
    {
      recv_len = vqec_recv_sock_read_pak(read_sock, &pak);

      if (recv_len > 0)
      {
        num_received++;
      } else {
        num_failed++;
      }  
    }
    CU_ASSERT_NOT_EQUAL(recv_len,1316);
    vqec_recv_sock_destroy(read_sock);
}

static void test_vqec_recv_sock_destroy (void) {
    CU_ASSERT(testsock->fd != -1);
    
    /* first time brings refcount back to 0, since it was
     * incremented once in the test_ref_count above */
    vqec_recv_sock_destroy(testsock);
    CU_ASSERT_EQUAL(diff_malloc_free(),6);
    /* then it actually gets freed */
    vqec_recv_sock_destroy(testsock);
    CU_ASSERT_EQUAL(diff_malloc_free(),4);
    
    vqec_recv_sock_destroy(testsock_mcast);
    CU_ASSERT_EQUAL(diff_malloc_free(),2);

    /* reserved socket destroy */
    vqec_recv_sock_close(testsock_rsrv->rcv_if_address.s_addr,
                         testsock_rsrv->port);
    CU_ASSERT_EQUAL(diff_malloc_free(),2);
    vqec_recv_sock_destroy(testsock_rsrv);
    CU_ASSERT_EQUAL(diff_malloc_free(), 0);
}


CU_TestInfo test_array_recv_sock[] = {
    {"test vqec_recv_sock_create",test_vqec_recv_sock_create},
    {"test vqec_recv_sock_get_rcv_if_address",test_vqec_recv_sock_get_rcv_if_address},
    {"test vqec_recv_sock_get_port",test_vqec_recv_sock_get_port},
    {"test vqec_recv_sock_get_blocking",test_vqec_recv_sock_get_blocking},
    {"test vqec_recv_sock_get_rcv_buff_bytes",test_vqec_recv_sock_get_rcv_buff_bytes},
    {"test vqec_recv_sock_get_mcast_group",test_vqec_recv_sock_get_mcast_group},
    {"test vqec_recv_sock_mcast_join",test_vqec_recv_sock_mcast_join},
    {"test vqec_recv_sock_mcast_leave",test_vqec_recv_sock_mcast_leave},
    {"test vqec_send_socket_create",test_vqec_send_socket_create},
    {"test vqec_recv_sock_ref",test_vqec_recv_sock_ref},
//    {"test vqec_recv_sock_read",test_vqec_recv_sock_read},
    {"test vqec_recv_sock_destroy",test_vqec_recv_sock_destroy},
    CU_TEST_INFO_NULL,
};

/* test for sock_read requirea a vam to be running, */
/* so it's not a true *unit* test */


