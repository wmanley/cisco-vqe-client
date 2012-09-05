/*
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vqec_tuner.h"
#include <assert.h>
#include <netinet/igmp.h>
#include <netinet/ip.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "vqec_debug.h"
#include "../cfg/cfgapi.h"
#include "vqec_error.h"
#include "vqec_igmp.h"
#include "vqec_igmpv3.h"
#include "vqec_url.h"
#include "cfgapi.h"
#include "vqec_ifclient.h"
#include "vqec_syscfg.h"
#include "utils/vam_util.h"
#include "vqec_igmpv3.h"
#include "vqec_syslog_def.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

/*
 * Unit tests for vqec_igmp
 */
static vqec_tunerid_t tuner_id1, tuner_id2, tuner_id3;
static vqec_igmp_t * igmp1, *igmp2, *igmp3;
static in_addr_t stb_addr;

vqec_recv_sock_t *igmp_sock1;

static in_addr_t channel;


/* declarations required for unit tests */
UT_STATIC void vqec_igmp_do_join(vqec_tunerid_t tuner_id,
                               struct in_addr group_ip);

UT_STATIC void vqec_igmp_do_leave(vqec_tunerid_t tuner_id,
                                  struct in_addr group_ip);

UT_STATIC vqec_igmp_t * vqec_igmp_get_entry_by_tuner_id(vqec_tunerid_t id);

UT_STATIC uint32_t vqec_igmp_process_igmp_pak(vqec_tunerid_t tuner_id,
                                              char *buf, int recv_len);

vqec_recv_sock_t * vqec_igmp_sock_create(vqec_tunerid_t tuner_id,
                                          char * name,
                                          in_addr_t rcv_if_address,
                                          boolean blocking,
                                          uint32_t rcv_buff_bytes);
void vqec_igmp_event_handler(const vqec_event_t * const evptr,
                              int fd, short event, void *event_params);
void vqec_igmp_event_handler_internal(int fd, short event,
                                       vqec_tunerid_t tuner_id);

int test_vqec_igmp_init (void) {
    vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    stb_addr = inet_addr("10.1.1.1");
    channel  = inet_addr("224.1.1.1");
    return 0;
}

int test_vqec_igmp_clean (void) {
    vqec_ifclient_deinit();
    return 0;
}

static void test_vqec_igmp_create (void) {
    uint8_t rtcp_dscp_value = 24;
    
    CU_ASSERT(vqec_ifclient_tuner_create(&tuner_id1, NULL) == VQEC_OK);
    CU_ASSERT(vqec_ifclient_tuner_create(&tuner_id2, NULL) == VQEC_OK);
    CU_ASSERT(vqec_ifclient_tuner_create(&tuner_id3, NULL) == VQEC_OK);

    vqec_igmp_create(tuner_id1, "lo", inet_addr("127.0.0.1"));
    vqec_igmp_create(tuner_id2, "lo", inet_addr("127.0.0.1"));

    igmp1 = vqec_igmp_get_entry_by_tuner_id(tuner_id1);
    CU_ASSERT(igmp1 != NULL);
    /* Since we can't run this as root, so don't test here */
    /* CU_ASSERT(igmp1->proxy == TRUE); */
    /* CU_ASSERT(igmp1->igmp_sock != NULL); */

    igmp2 = vqec_igmp_get_entry_by_tuner_id(tuner_id2);
    CU_ASSERT(igmp2 != NULL);
    /* Since we can't run this as root, so don't test here */
    /* CU_ASSERT(igmp2->proxy == TRUE); */
    /* CU_ASSERT(igmp2->igmp_sock != NULL); */

    /* test creating an igmp socket */
    igmp_sock1 = vqec_recv_sock_create("IGMP test sock for tuner 1",
                                       INADDR_ANY,
                                       5500,
                                       INADDR_ANY,
                                       FALSE,
                                       128000, 
                                       rtcp_dscp_value);
}

static void test_vqec_igmp_do_join (void) {
    struct in_addr chan;
    chan.s_addr = channel;

    vqec_igmp_do_join(tuner_id1, 
                       chan);
}

static void test_vqec_igmp_do_leave (void) {
    struct in_addr chan;
    chan.s_addr = channel;

    vqec_igmp_do_leave(tuner_id1, 
                        chan);
}

static void test_vqec_igmp_handler (void)
{
    const vqec_event_t * const ev_ptr = 0;
    vqec_igmp_event_handler(ev_ptr, 0, 0, igmp1);
    vqec_igmp_event_handler_internal(0, 0, tuner_id1);
}

static void test_vqec_proxy_igmp_join (void) {
    vqec_error_t ret;
    char name[VQEC_MAX_NAME_LEN];

    CU_ASSERT(vqec_ifclient_tuner_get_name_by_id(tuner_id3, name, 
                                                   VQEC_MAX_NAME_LEN) ==
               VQEC_OK);

    ret = vqec_proxy_igmp_join(name,
                               "lo",
                               stb_addr,
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    
    igmp3 = vqec_igmp_get_entry_by_tuner_id(tuner_id3);
    CU_ASSERT(igmp3 != NULL);
    /* Since we can't run this as root, so don't test here */
    /* CU_ASSERT(igmp3->proxy == TRUE); */
    /* CU_ASSERT(igmp3->igmp_sock != NULL); */


    ret = vqec_proxy_igmp_join(name,
                               "lo",
                               0,
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    
    igmp3 = vqec_igmp_get_entry_by_tuner_id(tuner_id3);
    CU_ASSERT(igmp3 != NULL);
    CU_ASSERT(igmp3->proxy == FALSE);
    CU_ASSERT(igmp3->igmp_sock == NULL);    

}

static void construct_igmp_pak (char *buf, int len, uint8_t type, 
                                uint8_t v3_type,
                                in_addr_t group, int *pak_len) {
    struct ip * ip;
    struct igmp *igmp;
    igmp_v3_report_type *igmpv3rp;
    igmp_group_record *igmpv3gr;
    int construct_len = 0;

    *pak_len = 0;
    memset(buf, 0, len);

    if (len < sizeof(struct ip) + sizeof(struct igmp)) {
        return;
    }

    ip = (struct ip *) buf;
    ip->ip_src.s_addr = stb_addr;
    ip->ip_dst.s_addr = inet_addr("127.0.0.1");    
    ip->ip_p = 2;
    ip->ip_hl = sizeof(struct ip) >> 2;
    /* no checksum stuff */

    construct_len += sizeof(struct ip);

    if (type == IGMP_V3_REPORT_TYPE) {
        igmpv3rp = (igmp_v3_report_type *)(buf + sizeof(struct ip));
        igmpv3rp->type = type;
        igmpv3rp->group_count = htons(1);

        igmpv3gr = igmpv3rp->group;
        igmpv3gr->group = group;
        igmpv3gr->type = v3_type;        
        construct_len += sizeof(igmp_v3_report_type);
    } else {
        igmp = (struct igmp *)(buf + sizeof(struct ip));
        igmp->igmp_group.s_addr = group;
        igmp->igmp_type = type;    
        construct_len += sizeof(struct igmp);
    }    
          
    *pak_len = construct_len;
    ip->ip_len = htons(construct_len);
    return;
}

static void test_vqec_igmp_process_igmp_pak (void) {
    char buf[VQEC_SYSCFG_DEFAULT_MAX_PAKSIZE];
    uint8_t type;
    uint8_t v3type;
    int construct_len;
    uint32_t rv;

    vqec_error_t ret;
    char name[VQEC_MAX_NAME_LEN];

    CU_ASSERT(vqec_ifclient_tuner_get_name_by_id(tuner_id3, name, 
                                                 VQEC_MAX_NAME_LEN) ==
              VQEC_OK);

    ret = vqec_proxy_igmp_join(name,
                               "lo",
                               stb_addr,
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    
    igmp3 = vqec_igmp_get_entry_by_tuner_id(tuner_id3);
    CU_ASSERT(igmp3 != NULL);
    /* CU_ASSERT(igmp3->proxy == TRUE); */
    /* Since we can't run under root mode, so fake it here */
    igmp3->proxy = TRUE;

    for (type = IGMP_MEMBERSHIP_QUERY; type <= 0x1f/*IGMP_MTRAC*/; type++) {
        construct_igmp_pak(buf, VQEC_SYSCFG_DEFAULT_MAX_PAKSIZE, type, 0,
                           inet_addr("224.1.1.1"), &construct_len);
        rv = vqec_igmp_process_igmp_pak(tuner_id3, buf, construct_len);
        CU_ASSERT(rv == VQEC_OK);
    }

    for (v3type = MODE_IS_INCLUDE; v3type <= BLOCK_OLD_SOURCES; v3type++) {
        construct_igmp_pak(buf, VQEC_SYSCFG_DEFAULT_MAX_PAKSIZE, 
                           IGMP_V3_REPORT_TYPE, v3type, 
                           inet_addr("224.1.1.1"), 
                           &construct_len);
        rv = vqec_igmp_process_igmp_pak(tuner_id3, buf, construct_len);
        CU_ASSERT(rv == VQEC_OK);
    }
}

static void test_vqec_igmp_destroy (void) {

    vqec_igmp_destroy(tuner_id1);
    vqec_igmp_destroy(tuner_id2);
    vqec_igmp_destroy(tuner_id3);

    igmp1 = vqec_igmp_get_entry_by_tuner_id(tuner_id1);
    CU_ASSERT(igmp1 != NULL);
    CU_ASSERT(igmp1->proxy != TRUE);
    CU_ASSERT(igmp1->igmp_sock == NULL);

    igmp2 = vqec_igmp_get_entry_by_tuner_id(tuner_id2);
    CU_ASSERT(igmp2 != NULL);
    CU_ASSERT(igmp2->proxy != TRUE);
    CU_ASSERT(igmp2->igmp_sock == NULL);

    igmp3 = vqec_igmp_get_entry_by_tuner_id(tuner_id3);
    CU_ASSERT(igmp3 != NULL);
    CU_ASSERT(igmp3->proxy != TRUE);
    CU_ASSERT(igmp3->igmp_sock == NULL);

}

CU_TestInfo test_array_igmp[] = {
    {"test vqec_igmp_create",test_vqec_igmp_create},
    {"test vqec_igmp_do_join",test_vqec_igmp_do_join},
    {"test vqec_igmp_do_leave",test_vqec_igmp_do_leave},
    {"test vqec_igmp_handler",test_vqec_igmp_handler},
    {"test vqec_proxy_igmp_join",
     test_vqec_proxy_igmp_join},
    {"test vqec_igmp_process_igmp_pak", test_vqec_igmp_process_igmp_pak},
    {"test vqec_igmp_destroy",test_vqec_igmp_destroy},
    CU_TEST_INFO_NULL,
};

