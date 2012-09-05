/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: RTP unit tests.
 *
 * Documents:
 *
 *****************************************************************************/
#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "test_vqec_interposers_common.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
#include "rtp_era_recv.h"
#include "vqec_channel_private.h"
#include "vqec_debug.h"
#include <malloc.h>
#include "vqec_nat_interface.h"

#define TEST_RTP_MAX_SESSIONS 4

int32_t
test_vqec_rtp_init (void)
{
    (void)vqec_event_init();
    VQEC_SET_DEBUG_FLAG(VQEC_DEBUG_UPCALL);
    VQEC_SET_DEBUG_FLAG(VQEC_DEBUG_RTCP);
    return (0);
}

int32_t
test_vqec_rtp_clean (void)
{
    VQEC_RESET_DEBUG_FLAG(VQEC_DEBUG_UPCALL);
    VQEC_RESET_DEBUG_FLAG(VQEC_DEBUG_RTCP);
    return (0);
}

/**---------------------------------------------------------------------------
 * Initialize ERA module.
 *---------------------------------------------------------------------------*/ 
static  void
test_rtp_era_recv_module_init (void)
{
    boolean init;

    init = rtp_era_recv_init_module(TEST_RTP_MAX_SESSIONS);
    CU_ASSERT(init);
}


/**---------------------------------------------------------------------------
 * ERA session creation.
 *---------------------------------------------------------------------------*/ 
#define TEST_RTP_DP_STREAMID 1
#define TEST_RTP_SRC_ADDR "127.0.0.1"
#define TEST_RTP_MCAST_ADDR "229.100.101.102"
#define TEST_RTP_ERA_RTCP_PORT 0x2237
#define TEST_RTP_FBT_IP_ADDR "127.0.0.120"
#define TEST_RTP_ORIG_SRC_ADDR "127.0.0.121"
#define TEST_RTP_ORIG_SRC_PORT 0x2239
#define TEST_RTP_MAX_CHANNELS 32

static  void
test_rtp_era_create_session (void)
{
    int32_t i;
    rtp_era_recv_t *s_ptr, *ptr_a[TEST_RTP_MAX_CHANNELS];
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;
    vqec_chan_t chan;
    
    bzero(&chan, sizeof(chan));
    bzero(&rtcp_bw_cfg, sizeof(rtcp_bw_cfg));
    bzero(&rtcp_xr_cfg, sizeof(rtcp_xr_cfg));
    chan.cfg.primary_rtcp_per_rcvr_bw = 100;
    chan.cfg.primary_rtcp_sndr_bw = 100;
    chan.cfg.primary_rtcp_rcvr_bw = 100;
    chan.cfg.primary_bit_rate = 1000000;    
    vqec_set_primary_rtcp_bw(&chan.cfg, &rtcp_bw_cfg);
    vqec_set_primary_rtcp_xr_options(&chan.cfg, &rtcp_xr_cfg);

    s_ptr = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                        inet_network(TEST_RTP_SRC_ADDR),
                                        "vqe_client",
                                        inet_network(TEST_RTP_MCAST_ADDR),  
                                /* htonl() missing - non-multicast IP; bind failure */
                                        TEST_RTP_ERA_RTCP_PORT,
                                        TEST_RTP_ERA_RTCP_PORT,
                                        htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                        &rtcp_bw_cfg,
                                        &rtcp_xr_cfg,
                                        htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                        TEST_RTP_ORIG_SRC_PORT,
                                        512,
                                        0,
                                        0,
                                        &chan);  
    CU_ASSERT(s_ptr == NULL);

    s_ptr = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                        htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                        "vqe_client",
                                        htonl(inet_network(TEST_RTP_MCAST_ADDR)),
                                        TEST_RTP_ERA_RTCP_PORT,
                                        TEST_RTP_ERA_RTCP_PORT,
                                        htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                        &rtcp_bw_cfg,
                                        &rtcp_xr_cfg,
                                        htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                        TEST_RTP_ORIG_SRC_PORT,
                                        512,
                                        0,
                                        0,
                                        NULL);  /* channel pointer is null */
    CU_ASSERT(s_ptr == NULL);

    s_ptr = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                        htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                        "vqe_client",
                                        htonl(inet_network(TEST_RTP_MCAST_ADDR)),
                                        TEST_RTP_ERA_RTCP_PORT,
                                        TEST_RTP_ERA_RTCP_PORT,
                                        htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                        NULL,  /* bandwidth config is null */
                                        &rtcp_xr_cfg,
                                        htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                        TEST_RTP_ORIG_SRC_PORT,
                                        512,
                                        0,
                                        0,
                                        &chan);
    CU_ASSERT(s_ptr == NULL);

                                /* create a session */
    s_ptr = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                        htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                        "vqe_client",
                                        htonl(inet_network(TEST_RTP_MCAST_ADDR)),
                                        TEST_RTP_ERA_RTCP_PORT,
                                        TEST_RTP_ERA_RTCP_PORT,
                                        htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                        &rtcp_bw_cfg,
                                        &rtcp_xr_cfg,
                                        htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                        TEST_RTP_ORIG_SRC_PORT,
                                        512,
                                        0,
                                        0,
                                        &chan);
    CU_ASSERT(s_ptr != NULL);

                                /* exhaust the pool */
    for (i = 0; i < TEST_RTP_MAX_CHANNELS; i++) {
        ptr_a[i] = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                               htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                               "vqe_client",
                                               htonl(inet_network(TEST_RTP_MCAST_ADDR)),
                                               TEST_RTP_ERA_RTCP_PORT,
                                               TEST_RTP_ERA_RTCP_PORT,
                                               htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                               &rtcp_bw_cfg,
                                               &rtcp_xr_cfg,
                                               htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                               TEST_RTP_ORIG_SRC_PORT,
                                               512,
                                               0,
                                               0,
                                               &chan);
        if (!ptr_a[i]) {
            printf("Allocation failed at %d session\n", i);
            CU_ASSERT(TRUE);
            break;
        }
    }
    i--;
    for (; i >= 0; i--) {
        if (ptr_a[i]) {
            rtp_delete_session_era_recv(&ptr_a[i], TRUE);
        }
    }
    if (s_ptr) {
        rtp_delete_session_era_recv(&s_ptr, TRUE);
    }

    s_ptr = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                        htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                        "vqe_client",
                                        htonl(inet_network(TEST_RTP_MCAST_ADDR)),
                                        0, 0, /* disable RTCP */
                                        htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                        &rtcp_bw_cfg,
                                        &rtcp_xr_cfg,
                                        htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                        TEST_RTP_ORIG_SRC_PORT,
                                        512,
                                        0,
                                        0,
                                        &chan);
    CU_ASSERT(s_ptr != NULL);
    CU_ASSERT(s_ptr->era_rtcp_xmit_sock == -1);
    rtp_delete_session_era_recv(NULL, TRUE);  /* bad inputs */
    ptr_a[0] = s_ptr;
    s_ptr = NULL;
    rtp_delete_session_era_recv(&s_ptr, TRUE);  /* bad inputs */
    rtp_delete_session_era_recv(&ptr_a[0], TRUE);
}


/**---------------------------------------------------------------------------
 * ERA upcall event.
 *---------------------------------------------------------------------------*/ 
static  void
test_rtp_era_upcall_ev (void)
{
    rtp_era_recv_t *s_ptr;
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;
    vqec_chan_t chan;
    vqec_dp_rtp_src_table_t table;
    int32_t res, i;
    vqec_dp_rtp_src_key_t *key;
    struct in_addr src_addr;
    rtp_source_id_t z_sid, sid;
    rtp_error_t status;
    rtp_member_id_t member_id;
    rtp_member_t *p_member;

    bzero(&chan, sizeof(chan));
    bzero(&rtcp_bw_cfg, sizeof(rtcp_bw_cfg));
    bzero(&rtcp_xr_cfg, sizeof(rtcp_xr_cfg));
    chan.cfg.primary_rtcp_per_rcvr_bw = 100;
    chan.cfg.primary_rtcp_sndr_bw = 100;
    chan.cfg.primary_rtcp_rcvr_bw = 100;
    chan.cfg.primary_bit_rate = 1000000;    
    vqec_set_primary_rtcp_bw(&chan.cfg, &rtcp_bw_cfg);
    vqec_set_primary_rtcp_xr_options(&chan.cfg, &rtcp_xr_cfg);

    s_ptr = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                        htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                        "vqe_client",
                                        htonl(inet_network(TEST_RTP_MCAST_ADDR)),
                                        TEST_RTP_ERA_RTCP_PORT,
                                        TEST_RTP_ERA_RTCP_PORT,
                                        htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                        &rtcp_bw_cfg,
                                        &rtcp_xr_cfg,
                                        htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                        TEST_RTP_ORIG_SRC_PORT,
                                        512,
                                        0,
                                        0,
                                        &chan);
    CU_ASSERT(s_ptr != NULL);

    bzero(&table, sizeof(table));
    MCALL(s_ptr, process_upcall_event, NULL);  /* faulty input */
    s_ptr->state = VQEC_RTP_SESSION_STATE_SHUTDOWN;
    MCALL(s_ptr, process_upcall_event, &table);  /* input in shutdown */
    s_ptr->state = VQEC_RTP_SESSION_STATE_INACTIVE_WAITFIRST;


#define TEST_CHAN_RTP_PAK_SSRC 0x1234
#define TEST_CHAN_RTP_PAK_SRCPORT 0x5103
#define TEST_CHAN_RTP_PAK_SRCIP "66.66.66.66"

    /******************************************/
                                /* Inactive waitfirst state */
                                /* events: inactive_source, active_source */

                                /* format a new source key from dataplane */
    table.num_entries = 1;
    key = &table.sources[0].key;
    key->ssrc = TEST_CHAN_RTP_PAK_SSRC;
    key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
    table.sources[0].info.pktflow_permitted = TRUE;
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;

                                /* let the command fail: invalidargs */
    test_vqec_dp_permit_pktflow_set_hook(TRUE, VQEC_DP_ERR_INVALIDARGS);
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);
    CU_ASSERT(s_ptr->pktflow_src.thresh_cnt == 0);
    bzero(&z_sid, sizeof(z_sid));
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) == 0);
    s_ptr->state = 0;

    test_vqec_dp_permit_pktflow_set_hook(TRUE, VQEC_DP_ERR_NOT_FOUND);
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);
    CU_ASSERT(s_ptr->pktflow_src.thresh_cnt == 0);
    bzero(&z_sid, sizeof(z_sid));
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) == 0);
    s_ptr->state = 0;

                                /* source is inactive -> session becomes inactive */
    table.sources[0].info.state = VQEC_DP_RTP_SRC_INACTIVE;
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_INACTIVE);
    CU_ASSERT(s_ptr->pktflow_src.thresh_cnt == 0);
    bzero(&z_sid, sizeof(z_sid));
                                /* no local source added to the control-plane */
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) == 0);

                                /* reset state; active source -> active state */
    bzero(&s_ptr->pktflow_src, sizeof(s_ptr->pktflow_src));
    s_ptr->state = 0;    
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    test_vqec_dp_permit_pktflow_set_hook(TRUE, VQEC_DP_ERR_OK);
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    CU_ASSERT(s_ptr->pktflow_src.thresh_cnt == 0);
    bzero(&z_sid, sizeof(z_sid));
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) != 0);
    
    bzero(&sid, sizeof(sid));
    sid.ssrc = TEST_CHAN_RTP_PAK_SSRC;
    sid.src_addr = key->ipv4.src_addr.s_addr;
    sid.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &sid, sizeof(sid)) == 0);

                                /* let source become inactive */
    table.sources[0].info.state = VQEC_DP_RTP_SRC_INACTIVE;
    table.sources[0].info.thresh_cnt++;
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_INACTIVE);
                                /* threshold count is updated */
    CU_ASSERT(s_ptr->pktflow_src.thresh_cnt == 1);
    bzero(&z_sid, sizeof(z_sid));
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) != 0);

    bzero(&sid, sizeof(sid));
    sid.ssrc = TEST_CHAN_RTP_PAK_SSRC;
    sid.src_addr = key->ipv4.src_addr.s_addr;
    sid.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &sid, sizeof(sid)) == 0);    


                                /* let source become active again - 
                                   must have had a threshold transition since it
                                   is going from inactive-to-active. */
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    table.sources[0].info.thresh_cnt++;
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
                                /* threshold count is updated */
    CU_ASSERT(s_ptr->pktflow_src.thresh_cnt == 2);
    bzero(&z_sid, sizeof(z_sid));
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) != 0);

    bzero(&sid, sizeof(sid));
    sid.ssrc = TEST_CHAN_RTP_PAK_SSRC;
    sid.src_addr = key->ipv4.src_addr.s_addr;
    sid.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &sid, sizeof(sid)) == 0);    

                                /* make the current source inactive;
                                   introduce a new source which active */
    table.sources[0].info.state = VQEC_DP_RTP_SRC_INACTIVE;
    table.sources[0].info.thresh_cnt++;
    table.num_entries = 2;
    key = &table.sources[1].key;
    key->ssrc = TEST_CHAN_RTP_PAK_SSRC + 1;
    key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
                                /* not the packetflow source */
    table.sources[1].info.pktflow_permitted = FALSE;
    table.sources[1].info.state = VQEC_DP_RTP_SRC_ACTIVE;

                                /* cause the source(s) to swap */
    test_vqec_dp_src_delete_set_hook(TRUE, VQEC_DP_ERR_OK, NULL);
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    CU_ASSERT(s_ptr->pktflow_src.thresh_cnt == 0);
    bzero(&z_sid, sizeof(z_sid));
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) != 0);

    bzero(&sid, sizeof(sid));
    sid.ssrc = TEST_CHAN_RTP_PAK_SSRC + 1;
    sid.src_addr = key->ipv4.src_addr.s_addr;
    sid.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &sid, sizeof(sid)) == 0);    

                                /* lookup the member(s) */
    memset(&member_id, 0, sizeof(member_id));
    member_id.type = RTP_SMEMBER_ID_RTP_DATA;
    member_id.ssrc = s_ptr->pktflow_src.id.ssrc;
    member_id.src_addr = s_ptr->pktflow_src.id.src_addr;
    member_id.src_port = s_ptr->pktflow_src.id.src_port;
    member_id.cname = NULL;
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_SUCCESS);

                                /* put the 1st source back to active  */
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    table.sources[0].info.thresh_cnt++;
                                /* previously modified dataplane state */
    table.sources[0].info.pktflow_permitted = FALSE;
    table.sources[1].info.pktflow_permitted = TRUE;
    
                                /* no change in the active source */
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    CU_ASSERT(s_ptr->pktflow_src.thresh_cnt == 0);
    bzero(&z_sid, sizeof(z_sid));
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) != 0);

    bzero(&sid, sizeof(sid));
    sid.ssrc = TEST_CHAN_RTP_PAK_SSRC + 1;
    sid.src_addr = key->ipv4.src_addr.s_addr;
    sid.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &sid, sizeof(sid)) == 0);    

                                /* cause a threshold transition on the active */
    table.sources[1].info.thresh_cnt++;
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    CU_ASSERT(s_ptr->pktflow_src.thresh_cnt == 1);
    bzero(&z_sid, sizeof(z_sid));
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) != 0);

    bzero(&sid, sizeof(sid));
    sid.ssrc = TEST_CHAN_RTP_PAK_SSRC + 1;
    sid.src_addr = key->ipv4.src_addr.s_addr;
    sid.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &sid, sizeof(sid)) == 0);    

                                /* add a 3rd active source */
    CU_ASSERT(VQEC_DP_RTP_MAX_KNOWN_SOURCES >= 3);
    table.num_entries = 3;
    key = &table.sources[2].key;
    key->ssrc = TEST_CHAN_RTP_PAK_SSRC + 2;
    key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
    table.sources[2].info.pktflow_permitted = FALSE;
    table.sources[2].info.state = VQEC_DP_RTP_SRC_ACTIVE;
                                /* make 3rd source most recent active */
    table.sources[0].info.last_rx_time = get_sys_time();
    table.sources[2].info.last_rx_time = TIME_ADD_A_R(get_sys_time(), MSECS(1));

                                /* make pktflow source inactive */
    table.sources[1].info.state = VQEC_DP_RTP_SRC_INACTIVE;
    table.sources[1].info.thresh_cnt++;

                                /* ensure the most-recent active 
                                   source is selected next */
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    CU_ASSERT(s_ptr->pktflow_src.thresh_cnt == 0);
    bzero(&z_sid, sizeof(z_sid));
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) != 0);

    bzero(&sid, sizeof(sid));
    sid.ssrc = TEST_CHAN_RTP_PAK_SSRC + 2;
    sid.src_addr = key->ipv4.src_addr.s_addr;
    sid.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &sid, sizeof(sid)) == 0);    

                                /* lookup the member(s) */
    memset(&member_id, 0, sizeof(member_id));
    member_id.type = RTP_SMEMBER_ID_RTP_DATA;
    member_id.ssrc = s_ptr->pktflow_src.id.ssrc;
    member_id.src_addr = s_ptr->pktflow_src.id.src_addr;
    member_id.src_port = s_ptr->pktflow_src.id.src_port;
    member_id.cname = NULL;
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_SUCCESS);

                                /* test error / infrequent paths */

                                /* infrequent: no change is state of
                                   active source. */
    table.sources[0].info.pktflow_permitted =
        table.sources[1].info.pktflow_permitted = FALSE;        
    table.sources[2].info.pktflow_permitted = TRUE;
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    bzero(&z_sid, sizeof(z_sid));
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) != 0);
                                /* lookup the member(s) */
    memset(&member_id, 0, sizeof(member_id));
    member_id.type = RTP_SMEMBER_ID_RTP_DATA;
    member_id.ssrc = s_ptr->pktflow_src.id.ssrc;
    member_id.src_addr = s_ptr->pktflow_src.id.src_addr;
    member_id.src_port = s_ptr->pktflow_src.id.src_port;
    member_id.cname = NULL;
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_SUCCESS);

                                 /* delete member(s) */
    for (i = 0; i < 3; i++) {
        memset(&member_id, 0, sizeof(member_id));
        member_id.type = RTP_SMEMBER_ID_RTP_DATA;
        member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        member_id.src_addr = src_addr.s_addr;
        member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        member_id.cname = NULL;
        status = MCALL((rtp_session_t *)s_ptr, 
                       rtp_lookup_member, &member_id, &p_member, NULL); 
        CU_ASSERT(status == RTP_SUCCESS);
        MCALL((rtp_session_t *)s_ptr,
              rtp_delete_member, &p_member, TRUE);
    }

                                /* improbable: first packetflow src 
                                   is inactive. */
    s_ptr->state = 0;
    memset(&s_ptr->pktflow_src.id, 0, sizeof(s_ptr->pktflow_src.id));
    test_vqec_dp_src_delete_set_hook(TRUE, VQEC_DP_ERR_OK, NULL);
    table.sources[0].info.state = VQEC_DP_RTP_SRC_INACTIVE;
    table.sources[0].info.pktflow_permitted = TRUE;
    table.num_entries = 1;
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_INACTIVE);
    CU_ASSERT(memcmp(&s_ptr->pktflow_src.id, &z_sid, sizeof(z_sid)) == 0);

                                /* error: mismatch in pktflow sources 
                                   between control & dataplane. */
    test_vqec_dp_src_table_set_hook(TRUE, VQEC_DP_ERR_OK, NULL);
    table.num_entries = 3;
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    table.sources[0].info.pktflow_permitted = FALSE;
    table.sources[1].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    table.sources[1].info.pktflow_permitted = TRUE;
    table.sources[2].info.state = VQEC_DP_RTP_SRC_INACTIVE;
    table.sources[2].info.pktflow_permitted = FALSE;    
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    CU_ASSERT(s_ptr->pktflow_src.id.ssrc == TEST_CHAN_RTP_PAK_SSRC + 1);
    table.sources[1].info.pktflow_permitted = FALSE;
    table.sources[1].info.state = VQEC_DP_RTP_SRC_INACTIVE;
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    CU_ASSERT(s_ptr->pktflow_src.id.ssrc == TEST_CHAN_RTP_PAK_SSRC);

                                /* add item with same ssrc but different port
                                   as the next source to be promoted. */
    memset(&sid, 0, sizeof(sid));
    sid.ssrc = TEST_CHAN_RTP_PAK_SSRC + 1;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);    
    sid.src_addr = src_addr.s_addr;
    sid.src_port = TEST_CHAN_RTP_PAK_SRCPORT + 3;
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_new_data_source, &sid);
    CU_ASSERT(status == RTP_SSRC_EXISTS);

                                /* promote a member with the same ssrc */
    table.num_entries = 2;
    table.sources[0].info.state = VQEC_DP_RTP_SRC_INACTIVE;
    table.sources[0].info.pktflow_permitted = TRUE;
    table.sources[1].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    table.sources[1].info.pktflow_permitted = FALSE;
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);

    rtp_show_session_era_recv(s_ptr);
    rtp_show_drop_stats((rtp_session_t *)s_ptr);

    memset(&member_id, 0, sizeof(member_id));
    member_id.type = RTP_SMEMBER_ID_RTP_DATA;
    member_id.ssrc = s_ptr->pktflow_src.id.ssrc;
    member_id.src_addr = s_ptr->pktflow_src.id.src_addr;
    member_id.src_port = s_ptr->pktflow_src.id.src_port;
    member_id.cname = NULL;
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_SUCCESS);

                                /* cause RTP to fail */
                                /* exhaust the pool - this MUST be the
                                   last test, since we'll delete the session to
                                 recover the memory. */
    for (i = 0; i < RTP_MAX_SENDERS; i++) {
        memset(&sid, 0, sizeof(sid));
        sid.ssrc = TEST_CHAN_RTP_PAK_SSRC + 3 + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);    
        sid.src_addr = src_addr.s_addr;
        sid.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        status = MCALL((rtp_session_t *)s_ptr, 
                       rtp_new_data_source, &sid);
        CU_ASSERT(status == RTP_SUCCESS || 
                  status == RTP_MEMBER_RESOURCE_FAIL);
    }
    rtp_show_session_era_recv(s_ptr);

    bzero(&s_ptr->pktflow_src, sizeof(s_ptr->pktflow_src));
    s_ptr->state = 0;
                                /* promote another member
                                   must make IP address + 1 to cause
                                   a duplicate error. */
    table.num_entries = 1;
    table.sources[0].key.ssrc = TEST_CHAN_RTP_PAK_SSRC + 3 + (i - 1);
    table.sources[0].key.ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP+1, &src_addr); 
    CU_ASSERT(res != 0); 
    memcpy(&table.sources[0].key.ipv4.src_addr, 
           &src_addr, sizeof(struct in_addr));
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    table.sources[0].info.pktflow_permitted = TRUE;
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);

    test_vqec_dp_src_delete_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_src_table_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_permit_pktflow_set_hook(FALSE, VQEC_DP_ERR_OK);
    rtp_delete_session_era_recv(&s_ptr, TRUE); 
}


/**---------------------------------------------------------------------------
 * ERA update statistics.
 *---------------------------------------------------------------------------*/ 
static  void
test_rtp_era_update_stats (void)
{
    rtp_era_recv_t *s_ptr;
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;
    vqec_chan_t chan;
    vqec_dp_rtp_src_table_t table;
    int32_t res;
    vqec_dp_rtp_src_key_t *key;
    struct in_addr src_addr;
    era_recv_member_t member;
    rtp_hdr_session_t zrtpstats;
    rtp_hdr_source_t zrcvstats;
    rtcp_xr_stats_t zxrstats;

    bzero(&chan, sizeof(chan));
    bzero(&rtcp_bw_cfg, sizeof(rtcp_bw_cfg));
    bzero(&rtcp_xr_cfg, sizeof(rtcp_xr_cfg));
    chan.cfg.primary_rtcp_per_rcvr_bw = 100;
    chan.cfg.primary_rtcp_sndr_bw = 100;
    chan.cfg.primary_rtcp_rcvr_bw = 100;
    chan.cfg.primary_bit_rate = 1000000;    
    chan.cfg.primary_rtcp_xr_loss_rle = 128;
    vqec_set_primary_rtcp_bw(&chan.cfg, &rtcp_bw_cfg);
    vqec_set_primary_rtcp_xr_options(&chan.cfg, &rtcp_xr_cfg);

    s_ptr = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                        htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                        "vqe_client",
                                        htonl(inet_network(TEST_RTP_MCAST_ADDR)),
                                        TEST_RTP_ERA_RTCP_PORT,
                                        TEST_RTP_ERA_RTCP_PORT,
                                        htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                        &rtcp_bw_cfg,
                                        &rtcp_xr_cfg,
                                        htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                        TEST_RTP_ORIG_SRC_PORT,
                                        512,
                                        0,
                                        0,
                                        &chan);
    CU_ASSERT(s_ptr != NULL);

    MCALL((rtp_session_t *)s_ptr, rtp_update_receiver_stats, NULL, FALSE);    
    bzero(&member, sizeof(member));
    MCALL((rtp_session_t *)s_ptr, rtp_update_receiver_stats, 
          (rtp_member_t *)&member, FALSE);
    member.ssrc = TEST_CHAN_RTP_PAK_SSRC;
    member.rtp_src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);    
    member.rtp_src_addr = src_addr.s_addr;
    memset(&member.rcv_stats, 1, sizeof(member.rcv_stats));
    memset(&s_ptr->rtp_stats, 1, sizeof(s_ptr->rtp_stats));

    MCALL((rtp_session_t *)s_ptr, rtp_update_receiver_stats, 
          (rtp_member_t *)&member, FALSE);
    bzero(&zrcvstats, sizeof(zrcvstats));
    bzero(&zrtpstats, sizeof(zrtpstats));
    CU_ASSERT(memcmp(&member.rcv_stats, &zrcvstats, sizeof(zrcvstats)) == 0);
    CU_ASSERT(memcmp(&s_ptr->rtp_stats, &zrtpstats, sizeof(zrtpstats)) == 0);

    s_ptr->dp_is_id = 1;
    table.num_entries = 1;
    key = &table.sources[0].key;
    key->ssrc = TEST_CHAN_RTP_PAK_SSRC;
    key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
    table.sources[0].info.pktflow_permitted = TRUE;
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    test_vqec_dp_permit_pktflow_set_hook(TRUE, VQEC_DP_ERR_OK);
    test_vqec_dp_src_info_set_hook(TRUE, VQEC_DP_ERR_OK, 0);

    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);

    memset(&member.rcv_stats, 1, sizeof(member.rcv_stats));
    memset(&s_ptr->rtp_stats, 1, sizeof(s_ptr->rtp_stats));

    MCALL((rtp_session_t *)s_ptr, rtp_update_receiver_stats,
          (rtp_member_t *)&member, FALSE);
    CU_ASSERT(s_ptr->rtp_stats.initseq_count == 1);
    CU_ASSERT(member.rcv_stats.received == 1);
    CU_ASSERT(member.rcv_stats.cycles == 1);
    CU_ASSERT(member.xr_stats == NULL);
    s_ptr->rtp_stats.initseq_count = 0;
    member.rcv_stats.received = 0;
    member.rcv_stats.cycles = 0;
    CU_ASSERT(memcmp(&member.rcv_stats, &zrcvstats, sizeof(zrcvstats)) == 0);
    CU_ASSERT(memcmp(&s_ptr->rtp_stats, &zrtpstats, sizeof(zrtpstats)) == 0);

    memset(&member.rcv_stats, 1, sizeof(member.rcv_stats));
    memset(&s_ptr->rtp_stats, 1, sizeof(s_ptr->rtp_stats));
    test_vqec_dp_src_info_set_hook(TRUE, VQEC_DP_ERR_INVALIDARGS, 0);
    MCALL((rtp_session_t *)s_ptr, rtp_update_receiver_stats, 
          (rtp_member_t *)&member, FALSE);
    CU_ASSERT(s_ptr->rtp_stats.initseq_count == 0);
    CU_ASSERT(member.rcv_stats.received == 0);
    CU_ASSERT(member.rcv_stats.cycles == 0);
    CU_ASSERT(member.xr_stats == NULL);

                                /* get XR block */
    memset(&member.rcv_stats, 1, sizeof(member.rcv_stats));
    memset(&s_ptr->rtp_stats, 1, sizeof(s_ptr->rtp_stats));

    test_vqec_dp_src_info_set_hook(TRUE, VQEC_DP_ERR_OK, 0);
    s_ptr->dp_is_id = 1;
    MCALL((rtp_session_t *)s_ptr, rtp_update_receiver_stats, 
          (rtp_member_t *)&member, TRUE);
    CU_ASSERT(member.xr_stats != NULL);
    bzero(&zxrstats, sizeof(zxrstats));
    CU_ASSERT(member.xr_stats->chunk[0] == 0xFFFF);
    member.xr_stats->chunk[0] = 0;
    CU_ASSERT(memcmp(member.xr_stats, &zxrstats, sizeof(zxrstats)) == 0);

    test_vqec_dp_src_info_set_hook(TRUE, VQEC_DP_ERR_INVALIDARGS, 0);
    MCALL((rtp_session_t *)s_ptr, rtp_update_receiver_stats, 
          (rtp_member_t *)&member, TRUE);
    CU_ASSERT(member.xr_stats == NULL);

    test_vqec_dp_src_delete_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_src_table_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_permit_pktflow_set_hook(FALSE, VQEC_DP_ERR_OK);
    test_vqec_dp_src_info_set_hook(FALSE, VQEC_DP_ERR_OK, 0);
    rtp_delete_session_era_recv(&s_ptr, TRUE); 

}


/**---------------------------------------------------------------------------
 * ERA shutdown / cache statistics.
 *---------------------------------------------------------------------------*/ 
static  void
test_rtp_era_shutdown_stats (void)
{
    rtp_era_recv_t *s_ptr;
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;
    vqec_chan_t chan;
    vqec_dp_rtp_src_table_t table;
    int32_t res;
    vqec_dp_rtp_src_key_t *key;
    struct in_addr src_addr;
    rtp_hdr_session_t zrtpstats;
    rtp_hdr_source_t zrcvstats;
    rtp_error_t status;
    rtp_member_id_t member_id;
    rtp_member_t *p_member;

    bzero(&chan, sizeof(chan));
    bzero(&rtcp_bw_cfg, sizeof(rtcp_bw_cfg));
    bzero(&rtcp_xr_cfg, sizeof(rtcp_xr_cfg));
    chan.cfg.primary_rtcp_per_rcvr_bw = 100;
    chan.cfg.primary_rtcp_sndr_bw = 100;
    chan.cfg.primary_rtcp_rcvr_bw = 100;
    chan.cfg.primary_bit_rate = 1000000;    
    chan.cfg.primary_rtcp_xr_loss_rle = 128;
    vqec_set_primary_rtcp_bw(&chan.cfg, &rtcp_bw_cfg);
    vqec_set_primary_rtcp_xr_options(&chan.cfg, &rtcp_xr_cfg);

    s_ptr = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                        htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                        "vqe_client",
                                        htonl(inet_network(TEST_RTP_MCAST_ADDR)),
                                        TEST_RTP_ERA_RTCP_PORT,
                                        TEST_RTP_ERA_RTCP_PORT,
                                        htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                        &rtcp_bw_cfg,
                                        &rtcp_xr_cfg,
                                        htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                        TEST_RTP_ORIG_SRC_PORT,
                                        512,
                                        0,
                                        0,
                                        &chan);
    CU_ASSERT(s_ptr != NULL);

    /* Inform CP of a DP source table with one entry */
    s_ptr->dp_is_id = 1;
    table.num_entries = 1;
    key = &table.sources[0].key;
    key->ssrc = TEST_CHAN_RTP_PAK_SSRC;
    key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
    table.sources[0].info.pktflow_permitted = TRUE;
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    test_vqec_dp_permit_pktflow_set_hook(TRUE, VQEC_DP_ERR_OK);
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    test_vqec_dp_src_info_set_hook(TRUE, VQEC_DP_ERR_OK, 0);

    /* Shut channel down */
    MCALL(s_ptr, shutdown_allow_byes);

    /* Lookup stats for the DP sender, values should be cached */
    memset(&member_id, 0, sizeof(member_id));
    member_id.type = RTP_SMEMBER_ID_RTP_DATA;
    member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC;
    member_id.src_addr = src_addr.s_addr;
    member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    member_id.cname = NULL;
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_SUCCESS);
    CU_ASSERT(s_ptr->rtp_stats.initseq_count == 1);
    CU_ASSERT(p_member->rcv_stats.received == 1);
    CU_ASSERT(p_member->rcv_stats.cycles == 1);
    CU_ASSERT(p_member->xr_stats->chunk[0] == 0xFFFF);

    /* Check error handling with DP stats lookup failuire, stats should be 0 */
    s_ptr->state = VQEC_RTP_SESSION_STATE_ACTIVE;
    s_ptr->dp_is_id = 1;
    test_vqec_dp_src_info_set_hook(TRUE, VQEC_DP_ERR_INVALIDARGS, 0);
    MCALL(s_ptr, shutdown_allow_byes);
    bzero(&zrcvstats, sizeof(zrcvstats));
    bzero(&zrtpstats, sizeof(zrtpstats));
    CU_ASSERT(memcmp(&s_ptr->rtp_stats, &zrtpstats, sizeof(zrtpstats)) == 0);
    CU_ASSERT(memcmp(&p_member->rcv_stats, &zrcvstats, 
                     sizeof(zrcvstats)) == 0);
    CU_ASSERT(p_member->xr_stats == NULL);    

    test_vqec_dp_src_delete_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_src_table_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_permit_pktflow_set_hook(FALSE, VQEC_DP_ERR_OK);
    test_vqec_dp_src_info_set_hook(FALSE, VQEC_DP_ERR_OK, 0);
    rtp_delete_session_era_recv(&s_ptr, TRUE); 

}


/**---------------------------------------------------------------------------
 * ERA RR / other remaining method invocations.
 *---------------------------------------------------------------------------*/ 
static  void
test_rtp_era_rr (void)
{
    rtp_era_recv_t *s_ptr;
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;
    vqec_chan_t chan;

    bzero(&chan, sizeof(chan));
    bzero(&rtcp_bw_cfg, sizeof(rtcp_bw_cfg));
    bzero(&rtcp_xr_cfg, sizeof(rtcp_xr_cfg));
    chan.cfg.primary_rtcp_per_rcvr_bw = 100;
    chan.cfg.primary_rtcp_sndr_bw = 100;
    chan.cfg.primary_rtcp_rcvr_bw = 100;
    chan.cfg.primary_bit_rate = 1000000;    
    chan.cfg.primary_rtcp_xr_loss_rle = 128;
    vqec_set_primary_rtcp_bw(&chan.cfg, &rtcp_bw_cfg);
    vqec_set_primary_rtcp_xr_options(&chan.cfg, &rtcp_xr_cfg);

    s_ptr = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                        htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                        "vqe_client",
                                        htonl(inet_network(TEST_RTP_MCAST_ADDR)),
                                        TEST_RTP_ERA_RTCP_PORT,
                                        TEST_RTP_ERA_RTCP_PORT,
                                        htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                        &rtcp_bw_cfg,
                                        &rtcp_xr_cfg,
                                        htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                        TEST_RTP_ORIG_SRC_PORT,
                                        512,
                                        0,
                                        0,
                                        &chan);
    CU_ASSERT(s_ptr != NULL);

#if HAVE_FCC
    CU_ASSERT(rtp_era_send_ncsi(s_ptr, 100, MSECS(20)) == TRUE);
    CU_ASSERT(rtp_era_send_nakpli(s_ptr, MSECS(20), MSECS(20), FALSE, 234, REL_TIME_0) == TRUE);
#endif   /* HAVE_FCC */
    rtp_session_era_recv_send_bye(s_ptr);
    CU_ASSERT(TRUE);

    rtp_delete_session_era_recv(&s_ptr, TRUE); 
}


/**---------------------------------------------------------------------------
 * Repair session module init.
 *---------------------------------------------------------------------------*/ 
static  void
test_rtp_repair_module_init (void)
{
    boolean init;

    init = rtp_repair_recv_init_module(TEST_RTP_MAX_SESSIONS);
    CU_ASSERT(init);    
}


/**---------------------------------------------------------------------------
 * Repair session creation / destruction.
 *---------------------------------------------------------------------------*/ 
#define TEST_RTP_REPAIR_RTCP_PORT 0x2339
#define TEST_RTP_LOCAL_SSRC 0xCAFEBEEF

static  void
test_rtp_repair_create_valid_session (rtp_repair_recv_t **s_pptr)
{
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;
    vqec_chan_t *chan;
    uint16_t rtcp_port = 0;

    chan = malloc(sizeof(*chan));
    CU_ASSERT(chan != NULL);
    bzero(chan, sizeof(*chan));
    bzero(&rtcp_bw_cfg, sizeof(rtcp_bw_cfg));
    chan->cfg.primary_rtcp_per_rcvr_bw = 100;
    chan->cfg.primary_rtcp_sndr_bw = 100;
    chan->cfg.primary_rtcp_rcvr_bw = 100;
    chan->cfg.primary_bit_rate = 1000000; 
    chan->cfg.rtx_rtcp_sndr_bw = 100;
    chan->cfg.rtx_rtcp_rcvr_bw = 100;
    vqec_set_repair_rtcp_bw(&chan->cfg, &rtcp_bw_cfg);
    vqec_set_repair_rtcp_xr_options(&chan->cfg,  &rtcp_xr_cfg);


    *s_pptr = rtp_create_session_repair_recv(TEST_RTP_DP_STREAMID,
                                             htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                             "vqe_client",
                                             htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                             &rtcp_port,                                           
                                             TEST_RTP_REPAIR_RTCP_PORT,
                                             TEST_RTP_LOCAL_SSRC,
                                             &rtcp_bw_cfg,        
                                             &rtcp_xr_cfg,
                                             htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                             TEST_RTP_ORIG_SRC_PORT,
                                             512,
                                             0,
                                             0,
                                             chan);
}

static  void
test_rtp_repair_create_session (void)
{
    int32_t i;
    rtp_repair_recv_t *s_ptr, *ptr_a[TEST_RTP_MAX_CHANNELS];
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;
    vqec_chan_t chan;
    uint16_t rtcp_port = 0;

    bzero(&chan, sizeof(chan));
    bzero(&rtcp_bw_cfg, sizeof(rtcp_bw_cfg));
    chan.cfg.primary_rtcp_per_rcvr_bw = 100;
    chan.cfg.primary_rtcp_sndr_bw = 100;
    chan.cfg.primary_rtcp_rcvr_bw = 100;
    chan.cfg.primary_bit_rate = 1000000; 
    chan.cfg.rtx_rtcp_sndr_bw = 100;
    chan.cfg.rtx_rtcp_rcvr_bw = 100;
    vqec_set_repair_rtcp_bw(&chan.cfg, &rtcp_bw_cfg);
    vqec_set_repair_rtcp_xr_options(&chan.cfg,  &rtcp_xr_cfg);


    s_ptr = rtp_create_session_repair_recv(TEST_RTP_DP_STREAMID,
                                /* missing htonl on addresses */
                                           inet_network(TEST_RTP_SRC_ADDR),
                                           "vqe_client",
                                           inet_network(TEST_RTP_FBT_IP_ADDR),
                                           &rtcp_port,                                           
                                           TEST_RTP_REPAIR_RTCP_PORT,
                                           TEST_RTP_LOCAL_SSRC,
                                           &rtcp_bw_cfg,   
                                           &rtcp_xr_cfg,
                                           inet_network(TEST_RTP_ORIG_SRC_ADDR),
                                           TEST_RTP_ORIG_SRC_PORT,
                                           512,
                                           0,
                                           0,
                                           &chan);  
    CU_ASSERT(s_ptr == NULL);

    s_ptr = rtp_create_session_repair_recv(TEST_RTP_DP_STREAMID,
                                           htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                           "vqe_client",
                                           htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                           &rtcp_port,                                           
                                           TEST_RTP_REPAIR_RTCP_PORT,
                                           TEST_RTP_LOCAL_SSRC,
                                           &rtcp_bw_cfg,    
                                           &rtcp_xr_cfg,
                                           htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                           TEST_RTP_ORIG_SRC_PORT,
                                           512,
                                           0,
                                           0,
                                           NULL); /* null parent channel */
    CU_ASSERT(s_ptr == NULL);

    s_ptr = rtp_create_session_repair_recv(TEST_RTP_DP_STREAMID,
                                           htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                           "vqe_client",
                                           htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                           &rtcp_port,                                           
                                           TEST_RTP_REPAIR_RTCP_PORT,
                                           TEST_RTP_LOCAL_SSRC,
                                           NULL, /* null bandwidth configuration */
                                           NULL, /* null xr cfg */
                                           htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                           TEST_RTP_ORIG_SRC_PORT,
                                           512,
                                           0,
                                           0,
                                           &chan);  
    CU_ASSERT(s_ptr == NULL);

    s_ptr = rtp_create_session_repair_recv(TEST_RTP_DP_STREAMID,
                                           htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                           "vqe_client",
                                           htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                           &rtcp_port,                                           
                                           TEST_RTP_REPAIR_RTCP_PORT,
                                           TEST_RTP_LOCAL_SSRC,
                                           &rtcp_bw_cfg,   
                                           &rtcp_xr_cfg,
                                           htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                           TEST_RTP_ORIG_SRC_PORT,
                                           512,
                                           0,
                                           0,
                                           &chan);  
    CU_ASSERT(s_ptr != NULL);
    CU_ASSERT(rtcp_port != 0);
    CU_ASSERT(s_ptr->repair_rtcp_sock != NULL);
    CU_ASSERT(s_ptr->repair_rtcp_sock->vqec_ev != NULL);
    CU_ASSERT(s_ptr->send_report_timeout_event != NULL);

    rtp_delete_session_repair_recv(&s_ptr, TRUE); 
    s_ptr = rtp_create_session_repair_recv(TEST_RTP_DP_STREAMID,
                                           htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                           "vqe_client",
                                           htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                           &rtcp_port,                                           
                                           0,  /* no send rtcp port in this session */
                                           TEST_RTP_LOCAL_SSRC,
                                           &rtcp_bw_cfg,
                                           &rtcp_xr_cfg,
                                           htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                           TEST_RTP_ORIG_SRC_PORT,
                                           512,
                                           0,
                                           0,
                                           &chan);  
    CU_ASSERT(s_ptr != NULL);
    CU_ASSERT(rtcp_port != 0);
    CU_ASSERT(s_ptr->repair_rtcp_sock != NULL);
    CU_ASSERT(s_ptr->repair_rtcp_sock->vqec_ev != NULL);
    CU_ASSERT(s_ptr->send_report_timeout_event == NULL);

                                /* exhaust the pool */
    for (i = 0; i < TEST_RTP_MAX_CHANNELS; i++) {
        rtcp_port = 0;
        ptr_a[i] = rtp_create_session_repair_recv(TEST_RTP_DP_STREAMID,
                                                  htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                                  "vqe_client",
                                                  htonl(inet_network(TEST_RTP_FBT_IP_ADDR + i + 1)),
                                                  &rtcp_port,                                           
                                                  0,  /* no send rtcp port in this session */
                                                  TEST_RTP_LOCAL_SSRC,
                                                  &rtcp_bw_cfg,    
                                                  &rtcp_xr_cfg,
                                                  htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                                  TEST_RTP_ORIG_SRC_PORT,
                                                  512,
                                                  0,
                                                  0,
                                                  &chan);  
        if (!ptr_a[i]) {
            printf("Allocation failed at %d session\n", i);
            CU_ASSERT(TRUE);
            break;
        }
    }
    i--;
    for (; i >= 0; i--) {
        if (ptr_a[i]) {
            rtp_delete_session_repair_recv(&ptr_a[i], TRUE);
        }
    }
    if (s_ptr) {
        rtp_delete_session_repair_recv(&s_ptr, TRUE);
    }

                                /* test some other failures */
    test_calloc_make_fail(1);
    s_ptr = rtp_create_session_repair_recv(TEST_RTP_DP_STREAMID,
                                           htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                           "vqe_client",
                                           htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                           &rtcp_port,                                           
                                           TEST_RTP_REPAIR_RTCP_PORT,
                                           TEST_RTP_LOCAL_SSRC,
                                           &rtcp_bw_cfg,
                                           &rtcp_xr_cfg,
                                           htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                           TEST_RTP_ORIG_SRC_PORT,
                                           512,
                                           0,
                                           0,
                                           &chan);  
    CU_ASSERT(s_ptr == NULL);
    test_calloc_make_fail(0);

    rtp_delete_session_repair_recv(&s_ptr, TRUE);
    rtp_delete_session_repair_recv(NULL, TRUE);
    
    test_vqec_dp_src_delete_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_src_table_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_permit_pktflow_set_hook(FALSE, VQEC_DP_ERR_OK);
    test_vqec_dp_src_info_set_hook(FALSE, VQEC_DP_ERR_OK, 0);
 
}


/**---------------------------------------------------------------------------
 * Repair session upcall event handling.
 *---------------------------------------------------------------------------*/ 
static  void
test_rtp_repair_upcall_ev (void)
{
    rtp_repair_recv_t *s_ptr;
    vqec_dp_rtp_src_table_t table, table1;
    vqec_dp_rtp_src_key_t *key;
    struct in_addr src_addr;
    int32_t res, i;
    rtp_source_id_t sid;
    rtp_member_id_t member_id;
    rtp_error_t status;
    rtp_member_t *p_member;

    test_rtp_repair_create_valid_session(&s_ptr);
    CU_ASSERT(s_ptr != NULL);

    bzero(&table, sizeof(table));
    MCALL(s_ptr, process_upcall_event, NULL);
    MCALL(s_ptr, process_upcall_event, &table);  /* an empty table */
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_INACTIVE_WAITFIRST);

                                /* have 1 source in the table */
    table.num_entries = 1;
    key = &table.sources[0].key;
    key->ssrc = TEST_CHAN_RTP_PAK_SSRC;
    key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    MCALL(s_ptr, process_upcall_event, &table);  
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    CU_ASSERT(s_ptr->num_src_ids == 1);

    bzero(&sid, sizeof(sid));
    sid.ssrc = TEST_CHAN_RTP_PAK_SSRC;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);    
    sid.src_addr = src_addr.s_addr;
    sid.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    CU_ASSERT(memcmp(&s_ptr->src_ids[0].id, &sid, sizeof(sid)) == 0);

                                /* deliver same table again; no change in state */
    MCALL(s_ptr, process_upcall_event, &table);  
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    CU_ASSERT(s_ptr->num_src_ids == 1);

                                /* deliver a table with an empty entry;
                                   this will cause a mismatch - should not be
                                   delivering null entries; result is 
                                   error state */
    table.num_entries = 2;
    MCALL(s_ptr, process_upcall_event, &table);  
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);
    CU_ASSERT(s_ptr->num_src_ids == 0);
    s_ptr->state = 0;

                                /* make 2 of the same entry - 
                                   this is in an error in general, since one
                                   entry is the duplicate of the other. */                                   
    table.sources[1].key = table.sources[0].key;
    table.sources[1].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    MCALL(s_ptr, process_upcall_event, &table);  
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);
    CU_ASSERT(s_ptr->num_src_ids == 0);

    s_ptr->state = 0;

                                /* add 3 entries - make sure ssrc 
                                   is different */
    CU_ASSERT(VQEC_DP_RTP_MAX_KNOWN_SOURCES >= 3); 
    for (i = 0; i < 3; i++) {
        table.num_entries = i + 1;
        key = &table.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
        MCALL(s_ptr, process_upcall_event, &table);  
        CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    }

    CU_ASSERT(s_ptr->num_src_ids == 3);
    for (i = 0; i < 3; i++) {
        bzero(&sid, sizeof(sid));
        sid.ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);    
        sid.src_addr = src_addr.s_addr;
        sid.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        CU_ASSERT(memcmp(&s_ptr->src_ids[i].id, &sid, sizeof(sid)) == 0);
    }

                                /* lookup the member(s) */
    for (i = 0; i < 3; i++) {
        memset(&member_id, 0, sizeof(member_id));
        member_id.type = RTP_SMEMBER_ID_RTP_DATA;
        member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        member_id.src_addr = src_addr.s_addr;
        member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        member_id.cname = NULL;
        status = MCALL((rtp_session_t *)s_ptr, 
                       rtp_lookup_member, &member_id, &p_member, NULL); 
        CU_ASSERT(status == RTP_SUCCESS);
    }


    test_vqec_dp_src_table_set_hook(TRUE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_src_delete_set_hook(TRUE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_ssrc_filter_del_set_hook(TRUE, VQEC_DP_ERR_OK);

                                /*  try adding a 4th source -
                                    this would put the session into error
                                    state since the dataplane cannot have
                                    more than 3 entries, and the control
                                    plane deletes those entries.
                                */
    table.num_entries = 1;
    i = 3;                      /* use a different ssrc */
    key = &table.sources[0].key;
    key->ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
    key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;

    MCALL(s_ptr, process_upcall_event, &table);  
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);


                                /* lookup the member(s) in rtp 
                                   they must not exits. */
    bzero(&sid, sizeof(sid));
    for (i = 0; i < 3; i++) {
        memset(&member_id, 0, sizeof(member_id));
        member_id.type = RTP_SMEMBER_ID_RTP_DATA;
        member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        member_id.src_addr = src_addr.s_addr;
        member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        member_id.cname = NULL;
        status = MCALL((rtp_session_t *)s_ptr, 
                       rtp_lookup_member, &member_id, &p_member, NULL); 
        CU_ASSERT(status == RTP_MEMBER_DOES_NOT_EXIST);
        CU_ASSERT(memcmp(&s_ptr->src_ids[i].id, &sid, sizeof(sid)) == 0);         
    }
    CU_ASSERT(s_ptr->num_src_ids == 0);

                                /* now try some other tricks */
                                /* a mismatch between the cp
                                   and dp tables. */
    s_ptr->state = 0;
    for (i = 0; i < 2; i++) {
        table.num_entries = i + 1;
        key = &table.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
        MCALL(s_ptr, process_upcall_event, &table);  
        CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    }
                                /* the dp table suddenly has only 1 entry. */
    table.num_entries = 1;
    key = &table.sources[0].key;
    key->ssrc = TEST_CHAN_RTP_PAK_SSRC + 3;
    key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    MCALL(s_ptr, process_upcall_event, &table);  
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);
    CU_ASSERT(s_ptr->num_src_ids == 0);

    s_ptr->state = 0;
    for (i = 0; i < 1; i++) {
        table.num_entries = i + 1;
        key = &table.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
        MCALL(s_ptr, process_upcall_event, &table);  
        CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    }
                                /* the dp table suddenly has 1 entry
                                   but is still out-of-sync */
    table.num_entries = 1;
    for (i = 0; i < 1; i++) {
        key = &table.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC + 3 + i;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    }
    MCALL(s_ptr, process_upcall_event, &table);  
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);
    CU_ASSERT(s_ptr->num_src_ids == 0);

    
                                /* an IP port collision for the same 
                                   <IP address, ssrc> tuple - this will 
                                   cause recursive fetch of the src table,
                                   but it will fail on cp-dp table
                                   re-sync. */
    s_ptr->state = 0;
    for (i = 0; i < 2; i++) {
        table.num_entries = i + 1;
        key = &table.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
        MCALL(s_ptr, process_upcall_event, &table);
    }

    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);
    CU_ASSERT(s_ptr->num_src_ids == 0);

                                /* again IP port collision but this time
                                   the recursive update fails. */
    test_vqec_dp_src_table_set_hook(TRUE, VQEC_DP_ERR_INVALIDARGS, NULL);
    s_ptr->state = 0;
    for (i = 0; i < 2; i++) {
        table.num_entries = i + 1;
        key = &table.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
        MCALL(s_ptr, process_upcall_event, &table);
    }

                                /* again IP port collision but this time
                                   cause an invalid table to be delivered. */
    bzero(&table1, sizeof(table1));
    for (i = 0; i < 2; i++) {
        table1.num_entries = i + 1;
        key = &table1.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table1.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    }

    test_vqec_dp_src_table_set_hook(TRUE, VQEC_DP_ERR_OK, &table1);
    s_ptr->state = 0;
    for (i = 0; i < 2; i++) {
        table.num_entries = i + 1;
        key = &table.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
        MCALL(s_ptr, process_upcall_event, &table);
    }

    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);
    CU_ASSERT(s_ptr->num_src_ids == 0);


                                /* ssrc collision but this time
                                   have a valid table delivered. */
    bzero(&table1, sizeof(table1));
    for (i = 0; i < 2; i++) {
        table1.num_entries = i + 1;
        key = &table1.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP + i, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table1.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    }

    test_vqec_dp_src_table_set_hook(TRUE, VQEC_DP_ERR_OK, &table1);
    s_ptr->state = 0;
    for (i = 0; i < 2; i++) {
        table.num_entries = i + 1;
        key = &table.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
        MCALL(s_ptr, process_upcall_event, &table);
    }
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    CU_ASSERT(s_ptr->num_src_ids == 2);

                                /* ensure that RTP members are correct */
    for (i = 0; i < 2; i++) {
        memset(&member_id, 0, sizeof(member_id));
        member_id.type = RTP_SMEMBER_ID_RTP_DATA;
        member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        member_id.src_addr = src_addr.s_addr;
        member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        member_id.cname = NULL;
        status = MCALL((rtp_session_t *)s_ptr, 
                       rtp_lookup_member, &member_id, &p_member, NULL); 
        CU_ASSERT(status == RTP_SUCCESS);
    }

                                /* now extend the member list by 1 */
    for (i = 0; i < 3; i++) {
        table.num_entries = i + 1;
        key = &table.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    }
    MCALL(s_ptr, process_upcall_event, &table);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    CU_ASSERT(s_ptr->num_src_ids == 3);
    rtp_show_session_repair_recv(NULL);
    rtp_show_session_repair_recv(s_ptr);

                                /* ensure that RTP members are correct */
    for (i = 0; i < 3; i++) {
        memset(&member_id, 0, sizeof(member_id));
        member_id.type = RTP_SMEMBER_ID_RTP_DATA;
        member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        member_id.src_addr = src_addr.s_addr;
        member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        member_id.cname = NULL;
        status = MCALL((rtp_session_t *)s_ptr, 
                       rtp_lookup_member, &member_id, &p_member, NULL); 
        CU_ASSERT(status == RTP_SUCCESS);
    }
        
    test_vqec_dp_src_table_set_hook(TRUE, VQEC_DP_ERR_OK, NULL);

    /************************** 
     * SSRC FILTER
     **************************/

                                /* install a ssrc filter */
    MCALL(s_ptr, primary_pktflow_src_update, NULL);
    CU_ASSERT(!s_ptr->ssrc_filter_en);

                                /* only one ssrc is left */
    table1.num_entries = 1;
    key = &table1.sources[0].key;
    key->ssrc = TEST_CHAN_RTP_PAK_SSRC + 1;
    key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
    table1.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    test_vqec_dp_ssrc_filter_add_set_hook(TRUE, VQEC_DP_ERR_OK, &table1);

    bzero(&sid, sizeof(sid));
                                /* filter on the 2nd ssrc */
    sid.ssrc = TEST_CHAN_RTP_PAK_SSRC + 1;
    MCALL(s_ptr, primary_pktflow_src_update, &sid);
    CU_ASSERT(s_ptr->ssrc_filter_en);
    CU_ASSERT(s_ptr->src_ssrc_filter == TEST_CHAN_RTP_PAK_SSRC + 1);
    CU_ASSERT(s_ptr->num_src_ids == 1);
                                /* ensure that RTP members are correct */
    for (i = 0; i < 3; i++) {
        memset(&member_id, 0, sizeof(member_id));
        member_id.type = RTP_SMEMBER_ID_RTP_DATA;
        member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        member_id.src_addr = src_addr.s_addr;
        member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        member_id.cname = NULL;
        status = MCALL((rtp_session_t *)s_ptr, 
                       rtp_lookup_member, &member_id, &p_member, NULL); 
        if (i == 1) {
            CU_ASSERT(status == RTP_SUCCESS);
        } else {
            CU_ASSERT(status == RTP_MEMBER_DOES_NOT_EXIST);
        }
    }

                                /* now cause all sources to be deleted */
    table1.num_entries = 0;
    MCALL(s_ptr, primary_pktflow_src_update, &sid);
    CU_ASSERT(s_ptr->ssrc_filter_en);
    CU_ASSERT(s_ptr->src_ssrc_filter == TEST_CHAN_RTP_PAK_SSRC + 1);
    CU_ASSERT(s_ptr->num_src_ids == 0);
    for (i = 0; i < 3; i++) {
        memset(&member_id, 0, sizeof(member_id));
        member_id.type = RTP_SMEMBER_ID_RTP_DATA;
        member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        member_id.src_addr = src_addr.s_addr;
        member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        member_id.cname = NULL;
        status = MCALL((rtp_session_t *)s_ptr, 
                       rtp_lookup_member, &member_id, &p_member, NULL); 
        CU_ASSERT(status == RTP_MEMBER_DOES_NOT_EXIST);
    }
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);

                                /* fail filter IPC  */
    test_vqec_dp_ssrc_filter_add_set_hook(TRUE, VQEC_DP_ERR_INVALIDARGS, NULL);
    MCALL(s_ptr, primary_pktflow_src_update, &sid);
    CU_ASSERT(s_ptr->num_src_ids == 0);
    CU_ASSERT(!s_ptr->ssrc_filter_en);
    CU_ASSERT(s_ptr->src_ssrc_filter == 0);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);

                                /* empty table */
    s_ptr->state = 0;
    table1.num_entries = 0;
    test_vqec_dp_ssrc_filter_add_set_hook(TRUE, VQEC_DP_ERR_OK, &table1);
    MCALL(s_ptr, primary_pktflow_src_update, &sid);
    CU_ASSERT(s_ptr->num_src_ids == 0);
    CU_ASSERT(s_ptr->ssrc_filter_en);
    CU_ASSERT(s_ptr->src_ssrc_filter == TEST_CHAN_RTP_PAK_SSRC + 1);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_INACTIVE_WAITFIRST);

                                /* null entry - should not be allowed */
    table1.num_entries = 1;
    bzero(&table1.sources[0], sizeof(table1.sources[0]));
    test_vqec_dp_ssrc_filter_add_set_hook(TRUE, VQEC_DP_ERR_OK, &table1);
    MCALL(s_ptr, primary_pktflow_src_update, &sid);
    CU_ASSERT(s_ptr->num_src_ids == 0);
    CU_ASSERT(!s_ptr->ssrc_filter_en);
    CU_ASSERT(s_ptr->src_ssrc_filter == 0);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);


                                /* try to create a condition such that
                                   find_add_new_sources() fails. It shouldn't
                                   fail since there will be at most one entry
                                   per good table; however we just hack
                                   a way to test this case. */
    s_ptr->state = 0;
    s_ptr->num_src_ids = 3;
                                /* the null source ids will be ignored */

    table1.num_entries = 1;
    key = &table1.sources[0].key;
    key->ssrc = TEST_CHAN_RTP_PAK_SSRC + 1;
    key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
    table1.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    test_vqec_dp_ssrc_filter_add_set_hook(TRUE, VQEC_DP_ERR_OK, &table1);
    MCALL(s_ptr, primary_pktflow_src_update, &sid);
    CU_ASSERT(s_ptr->num_src_ids == 0);
    CU_ASSERT(!s_ptr->ssrc_filter_en);
    CU_ASSERT(s_ptr->src_ssrc_filter == 0);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);

                                /* create a condition that the source table
                                   comparsion fails. The condition is
                                   not reachable under normal circumstances. */
    s_ptr->state = 0;
    s_ptr->num_src_ids = 2;
    MCALL(s_ptr, primary_pktflow_src_update, &sid);
    CU_ASSERT(s_ptr->num_src_ids == 0);
    CU_ASSERT(!s_ptr->ssrc_filter_en);
    CU_ASSERT(s_ptr->src_ssrc_filter == 0);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);

                                /* now try a different transitional
                                   behavior: the primary event is delivered
                                   before any repair packet is received,
                                   but during the installation of the filter
                                   a valid table entry is returned with the
                                   desired ssrc. */
    s_ptr->state = 0;
                                /* the add_filter hook is still table1 */
    sid.ssrc = TEST_CHAN_RTP_PAK_SSRC + 1;
    MCALL(s_ptr, primary_pktflow_src_update, &sid);
    CU_ASSERT(s_ptr->num_src_ids == 1);
    CU_ASSERT(s_ptr->ssrc_filter_en);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);


    rtp_delete_session_repair_recv(&s_ptr, TRUE);

    test_vqec_dp_src_delete_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_src_table_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_permit_pktflow_set_hook(FALSE, VQEC_DP_ERR_OK);
    test_vqec_dp_src_info_set_hook(FALSE, VQEC_DP_ERR_OK, 0);
    test_vqec_dp_ssrc_filter_add_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_ssrc_filter_del_set_hook(FALSE, VQEC_DP_ERR_OK);
}


/**---------------------------------------------------------------------------
 * Delete member method.
 *---------------------------------------------------------------------------*/ 
static  void
test_rtp_repair_delete_member (void)
{
    rtp_repair_recv_t *s_ptr;
    vqec_dp_rtp_src_table_t table;
    vqec_dp_rtp_src_key_t *key;
    struct in_addr src_addr;
    int32_t res, i;
    rtp_member_id_t member_id;
    rtp_error_t status;
    rtp_member_t *p_member = NULL;

    test_rtp_repair_create_valid_session(&s_ptr);
    CU_ASSERT(s_ptr != NULL);
    
    MCALL((rtp_session_t *)s_ptr, rtp_delete_member, NULL, FALSE);
    MCALL((rtp_session_t *)s_ptr, rtp_delete_member, &p_member, FALSE);
    CU_ASSERT(TRUE);

                                /* install three member; delete one of them */
    CU_ASSERT(VQEC_DP_RTP_MAX_KNOWN_SOURCES >= 3); 
    for (i = 0; i < 3; i++) {
        table.num_entries = i + 1;
        key = &table.sources[i].key;
        key->ssrc = TEST_CHAN_RTP_PAK_SSRC + i;
        key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
        res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
        CU_ASSERT(res != 0);
        memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
        table.sources[i].info.state = VQEC_DP_RTP_SRC_ACTIVE;
    }

    MCALL(s_ptr, process_upcall_event, &table);  
    CU_ASSERT(s_ptr->num_src_ids == 3);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);

    memset(&member_id, 0, sizeof(member_id));
    member_id.type = RTP_SMEMBER_ID_RTP_DATA;
    member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    member_id.src_addr = src_addr.s_addr;
    member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    member_id.cname = NULL;
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_SUCCESS);

    test_vqec_dp_src_delete_set_hook(TRUE, VQEC_DP_ERR_OK, NULL);

    MCALL((rtp_session_t *)s_ptr, rtp_delete_member, &p_member, TRUE);
    CU_ASSERT(s_ptr->num_src_ids == 2);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_MEMBER_DOES_NOT_EXIST);


                                /* add / delete the same source 
                                   to flush out any stale "state" issues. */
    table.num_entries = 3;
    MCALL(s_ptr, process_upcall_event, &table);  
    CU_ASSERT(s_ptr->num_src_ids == 3);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    memset(&member_id, 0, sizeof(member_id));
    member_id.type = RTP_SMEMBER_ID_RTP_DATA;
    member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    member_id.src_addr = src_addr.s_addr;
    member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    member_id.cname = NULL;
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_SUCCESS);
    MCALL((rtp_session_t *)s_ptr, rtp_delete_member, &p_member, TRUE);
    CU_ASSERT(s_ptr->num_src_ids == 2);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_MEMBER_DOES_NOT_EXIST);

                                /* cause IPC to fail */
    member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC + 1;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    member_id.src_addr = src_addr.s_addr;
    member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    member_id.cname = NULL;
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_SUCCESS);

    test_vqec_dp_src_delete_set_hook(TRUE, VQEC_DP_ERR_INVALIDARGS, NULL);

    MCALL((rtp_session_t *)s_ptr, rtp_delete_member, &p_member, TRUE);
    CU_ASSERT(s_ptr->num_src_ids == 0);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ERROR);
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_MEMBER_DOES_NOT_EXIST);

    rtp_delete_session_repair_recv(&s_ptr, TRUE);

    test_vqec_dp_src_delete_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_src_table_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_permit_pktflow_set_hook(FALSE, VQEC_DP_ERR_OK);
    test_vqec_dp_src_info_set_hook(FALSE, VQEC_DP_ERR_OK, 0);
    test_vqec_dp_ssrc_filter_add_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_ssrc_filter_del_set_hook(FALSE, VQEC_DP_ERR_OK);
}


/**---------------------------------------------------------------------------
 * Update statistics.
 *---------------------------------------------------------------------------*/ 
static  void
test_rtp_repair_update_stats (void)
{
    rtp_repair_recv_t *s_ptr;
    vqec_dp_rtp_src_table_t table;
    vqec_dp_rtp_src_key_t *key;
    struct in_addr src_addr;
    int32_t res;
    rtp_member_id_t member_id;
    rtp_error_t status;
    rtp_member_t *p_member = NULL;

    test_rtp_repair_create_valid_session(&s_ptr);
    CU_ASSERT(s_ptr != NULL);

    MCALL((rtp_session_t *)s_ptr, rtp_update_receiver_stats,
          NULL, TRUE);
    CU_ASSERT(TRUE);

    table.num_entries = 1;
    key = &table.sources[0].key;
    key->ssrc = TEST_CHAN_RTP_PAK_SSRC;
    key->ipv4.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    memcpy(&key->ipv4.src_addr, &src_addr, sizeof(struct in_addr));
    table.sources[0].info.state = VQEC_DP_RTP_SRC_ACTIVE;

    MCALL(s_ptr, process_upcall_event, &table);  
    CU_ASSERT(s_ptr->num_src_ids == 1);
    CU_ASSERT(s_ptr->state == VQEC_RTP_SESSION_STATE_ACTIVE);

    memset(&member_id, 0, sizeof(member_id));
    member_id.type = RTP_SMEMBER_ID_RTP_DATA;
    member_id.ssrc = TEST_CHAN_RTP_PAK_SSRC;
    res = inet_aton(TEST_CHAN_RTP_PAK_SRCIP, &src_addr); 
    CU_ASSERT(res != 0);
    member_id.src_addr = src_addr.s_addr;
    member_id.src_port = TEST_CHAN_RTP_PAK_SRCPORT;
    member_id.cname = NULL;
    status = MCALL((rtp_session_t *)s_ptr, 
                   rtp_lookup_member, &member_id, &p_member, NULL); 
    CU_ASSERT(status == RTP_SUCCESS);

    test_vqec_dp_src_info_set_hook(TRUE, VQEC_DP_ERR_INVALIDARGS, 0);
    MCALL((rtp_session_t *)s_ptr, rtp_update_receiver_stats,
          p_member, TRUE);
    CU_ASSERT(TRUE);

                                /* let the IPC succeed */
    test_vqec_dp_src_info_set_hook(TRUE, VQEC_DP_ERR_OK, 0);
    MCALL((rtp_session_t *)s_ptr, rtp_update_receiver_stats,
          p_member, TRUE);
    CU_ASSERT(TRUE);

    s_ptr->dp_is_id = 0;
    MCALL((rtp_session_t *)s_ptr, rtp_update_receiver_stats,
          p_member, TRUE);
    CU_ASSERT(TRUE);

    rtp_delete_session_repair_recv(&s_ptr, TRUE);

    test_vqec_dp_src_delete_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_src_table_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_permit_pktflow_set_hook(FALSE, VQEC_DP_ERR_OK);
    test_vqec_dp_src_info_set_hook(FALSE, VQEC_DP_ERR_OK, 0);
    test_vqec_dp_ssrc_filter_add_set_hook(FALSE, VQEC_DP_ERR_OK, NULL);
    test_vqec_dp_ssrc_filter_del_set_hook(FALSE, VQEC_DP_ERR_OK);
}


/**---------------------------------------------------------------------------
 * Invoke other methods.
 *---------------------------------------------------------------------------*/ 
static  void
test_rtp_repair_rtcp_methods (void)
{
    rtp_repair_recv_t *s_ptr;
    char buf[80];
    boolean retval;
    struct in_addr src_addr;
    int32_t res;
    in_addr_t sk_src = inet_addr("127.0.0.1");
    in_addr_t sk_mcast_addr = inet_addr("239.216.181.199");
    uint16_t sk_rtp_port = 31672;
    char iobuf[128];
    vqec_recv_sock_t *sk_p;

    test_rtp_repair_create_valid_session(&s_ptr);
    CU_ASSERT(s_ptr != NULL);

    rtp_session_repair_recv_send_bye(NULL);
    rtp_session_repair_recv_send_bye(s_ptr);
    retval = MCALL(s_ptr, send_to_rtcp_socket, 0, 0, NULL, 0);
    CU_ASSERT(!retval);
    retval = MCALL(s_ptr, send_to_rtcp_socket, 0, 0, buf, 0);
    CU_ASSERT(!retval);

    res = inet_aton("127.0.0.1", &src_addr); 
    CU_ASSERT(res != 0);
    retval = MCALL(s_ptr, send_to_rtcp_socket, src_addr.s_addr,
                   TEST_CHAN_RTP_PAK_SRCPORT + 128, buf, sizeof(buf));
    CU_ASSERT(retval);

    (void)rtp_era_recv_init_module(1);

    sk_p = vqec_recv_sock_create("test socket",
                                 sk_src, 
                                 sk_rtp_port,
                                 sk_mcast_addr,
                                 FALSE,
                                 2048,
                                 0);
    CU_ASSERT(sk_p != NULL);

    rtcp_event_handler_internal((rtp_session_t *)s_ptr, 
                                sk_p,
                                iobuf,
                                sizeof(iobuf),
                                FALSE);
    /*
     * If we got here, rtcp_event_handler_internal() accepted the
     * passed valid arguments without failing an assert() check.
     */

    rtp_delete_session_repair_recv(&s_ptr, TRUE);
}


/**---------------------------------------------------------------------------
 * RTCP construct packet.
 *---------------------------------------------------------------------------*/ 
#define RTP_UT_BUFFLEN 1600
#define RTP_UT_NUMNACKS 3
static  void 
test_vqec_rtcp_construct_packet (void)
{
    uint8_t buff[RTP_UT_BUFFLEN];
    uint32_t len;
    rtcp_rtpfb_generic_nack_t nack_data[RTP_UT_NUMNACKS];
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t bye;
    rtcp_msg_info_t rtpfb;
    rtp_era_recv_t *s_ptr;
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;
    vqec_chan_t chan;
    vqec_nat_init_params_t nat_params;
    vqec_error_t err;
    vqec_nat_bind_desc_t desc;

    bzero(&chan, sizeof(chan));
    bzero(&rtcp_bw_cfg, sizeof(rtcp_bw_cfg));
    bzero(&rtcp_xr_cfg, sizeof(rtcp_xr_cfg));
    chan.cfg.primary_rtcp_per_rcvr_bw = 100;
    chan.cfg.primary_rtcp_sndr_bw = 100;
    chan.cfg.primary_rtcp_rcvr_bw = 100;
    chan.cfg.primary_bit_rate = 1000000;    
    vqec_set_primary_rtcp_bw(&chan.cfg, &rtcp_bw_cfg);
    vqec_set_primary_rtcp_xr_options(&chan.cfg, &rtcp_xr_cfg);

    s_ptr = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                        htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                        "vqe_client",
                                        htonl(inet_network(TEST_RTP_MCAST_ADDR)),
                                        0, 0, /* disable RTCP */
                                        htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                        &rtcp_bw_cfg,
                                        &rtcp_xr_cfg,
                                        htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                        TEST_RTP_ORIG_SRC_PORT,
                                        512,
                                        0,
                                        0,
                                        &chan);
    CU_ASSERT(s_ptr != NULL);


    memset(&nack_data[0], 0, sizeof(nack_data));
    memset(buff, 0, RTP_UT_BUFFLEN);
    
    rtcp_init_msg_info(&rtpfb);
    rtpfb.rtpfb_info.fmt = RTCP_RTPFB_GENERIC_NACK;
    rtpfb.rtpfb_info.num_nacks = 1;
    rtpfb.rtpfb_info.nack_data = &nack_data[0];

    /* 
     * the VQEC-specific rtcp_construct_report method
     * now adds a pubports message, in NAT state,
     * so we don't need to specify it here.
     */

    rtcp_init_msg_info(&bye);

    rtcp_init_pkt_info(&pkt_info);
    rtcp_set_pkt_info(&pkt_info, RTCP_RTPFB, &rtpfb);
    rtcp_set_pkt_info(&pkt_info, RTCP_BYE, &bye);

    /* construct a generic nack packet */
    len = MCALL((rtp_session_t *)s_ptr,
                rtcp_construct_report,
                s_ptr->rtp_local_source,
                (rtcptype *)buff, 
                RTP_UT_BUFFLEN,
                &pkt_info,
                FALSE);

    CU_ASSERT(len > 0);

    /* 
     * construct a generic nack packet, 
     * with invalid nack type
     */
    rtpfb.rtpfb_info.fmt = 1345; /* invalid Nack type */
    len = MCALL((rtp_session_t *)s_ptr,
                rtcp_construct_report,
                s_ptr->rtp_local_source,
                (rtcptype *)buff, 
                RTP_UT_BUFFLEN,
                &pkt_info,
                FALSE);

    CU_ASSERT(len == 0);    

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = 10;
    nat_params.refresh_interval = 1; 
    nat_params.max_paksize = 1500; 
    
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_NULL,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);
    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = 1000;
    desc.internal_port = 1000;
    chan.repair_rtp_nat_id = vqec_nat_open_binding(&desc);
    CU_ASSERT(chan.repair_rtp_nat_id != VQEC_NAT_BINDID_INVALID);
    desc.internal_addr = 1001;
    desc.internal_port = 1001;
    chan.repair_rtcp_nat_id = vqec_nat_open_binding(&desc);
    CU_ASSERT(chan.repair_rtcp_nat_id != VQEC_NAT_BINDID_INVALID);
    

    rtpfb.rtpfb_info.fmt = RTCP_RTPFB_GENERIC_NACK;

    /* construct a generic nack packet */
    len = MCALL((rtp_session_t *)s_ptr,
                rtcp_construct_report,
                s_ptr->rtp_local_source,
                (rtcptype *)buff, 
                RTP_UT_BUFFLEN,
                &pkt_info,
                FALSE);

    CU_ASSERT(len > 0);
    vqec_nat_module_deinit();

    rtp_delete_session_era_recv(&s_ptr, TRUE);
}


/**---------------------------------------------------------------------------
 * RTCP receive report timeout callback.
 *---------------------------------------------------------------------------*/ 
void rtcp_receive_report_timeout_cb(const vqec_event_t * const evptr,
                                    int32_t fd, int16_t event, void *arg);
void rtcp_member_timeout_cb(const vqec_event_t * const evptr,
                            int32_t fd, int16_t event, void *arg);
static  void
test_rtcp_receive_report_timeout_cb (void)
{
    rtp_era_recv_t *s_ptr;
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;
    vqec_chan_t chan;
    vqec_event_t *ev_p = NULL;
    struct timeval tv;
    rtp_member_t *l_member;

    bzero(&chan, sizeof(chan));
    bzero(&rtcp_bw_cfg, sizeof(rtcp_bw_cfg));
    bzero(&rtcp_xr_cfg, sizeof(rtcp_xr_cfg));
    chan.cfg.primary_rtcp_per_rcvr_bw = 100;
    chan.cfg.primary_rtcp_sndr_bw = 100;
    chan.cfg.primary_rtcp_rcvr_bw = 100;
    chan.cfg.primary_bit_rate = 1000000;    
    vqec_set_primary_rtcp_bw(&chan.cfg, &rtcp_bw_cfg);
    vqec_set_primary_rtcp_xr_options(&chan.cfg, &rtcp_xr_cfg);

    s_ptr = rtp_create_session_era_recv(TEST_RTP_DP_STREAMID,
                                        htonl(inet_network(TEST_RTP_SRC_ADDR)),
                                        "vqe_client",
                                        htonl(inet_network(TEST_RTP_MCAST_ADDR)),
                                        0, 0, /* disable RTCP */
                                        htonl(inet_network(TEST_RTP_FBT_IP_ADDR)),
                                        &rtcp_bw_cfg,
                                        &rtcp_xr_cfg,
                                        htonl(inet_network(TEST_RTP_ORIG_SRC_ADDR)),
                                        TEST_RTP_ORIG_SRC_PORT,
                                        512,
                                        0,
                                        0,
                                        &chan);
    CU_ASSERT(s_ptr != NULL);

    memset(&tv, 0, sizeof(tv));
    ev_p = rtcp_timer_event_create(VQEC_RTCP_SEND_REPORT_TIMEOUT, 
                                   &tv, 
                                   NULL);
    CU_ASSERT(ev_p == NULL);
#define RTCP_BAD_TIMER_TYPE 1000
    ev_p = rtcp_timer_event_create(RTCP_BAD_TIMER_TYPE, 
                                   &tv, 
                                   NULL);
    CU_ASSERT(ev_p == NULL);
    tv.tv_usec = 1000;
    ev_p = rtcp_timer_event_create(VQEC_RTCP_MEMBER_TIMEOUT, 
                                   &tv, 
                                   s_ptr);
    CU_ASSERT(ev_p != NULL);
    vqec_event_destroy(&ev_p);
    ev_p = rtcp_timer_event_create(VQEC_RTCP_SEND_REPORT_TIMEOUT, 
                                   &tv, 
                                   s_ptr);
    CU_ASSERT(ev_p != NULL);


    rtcp_receive_report_timeout_cb(ev_p, 
                                   -1, 
                                   0, 
                                   NULL);
    l_member = s_ptr->rtp_local_source;
    s_ptr->rtp_local_source = NULL;
    rtcp_receive_report_timeout_cb(ev_p, 
                                   -1, 
                                   0, 
                                   s_ptr);
    s_ptr->rtp_local_source = l_member;
    rtcp_receive_report_timeout_cb(ev_p, 
                                   -1, 
                                   0, 
                                   s_ptr);
    vqec_event_destroy(&ev_p);

    ev_p = rtcp_timer_event_create(VQEC_RTCP_MEMBER_TIMEOUT, 
                                   &tv, 
                                   s_ptr);
    CU_ASSERT(ev_p != NULL);
    rtcp_member_timeout_cb(ev_p, 
                                   -1, 
                                   0, 
                                   NULL);
    l_member = s_ptr->rtp_local_source;
    s_ptr->rtp_local_source = NULL;
    rtcp_member_timeout_cb(ev_p, 
                           -1, 
                           0, 
                           s_ptr);
    s_ptr->rtp_local_source = l_member;
    rtcp_member_timeout_cb(ev_p, 
                           -1, 
                           0, 
                           s_ptr);
    vqec_event_destroy(&ev_p);

    rtp_delete_session_era_recv(&s_ptr, TRUE);
}


CU_TestInfo test_array_rtp[] =
{
    {"test_rtp_era_recv_module_init",  test_rtp_era_recv_module_init},
    {"test_rtp_era_create_session",  test_rtp_era_create_session},
    {"test_rtp_era_upcall_ev",  test_rtp_era_upcall_ev},
    {"test_rtp_era_update_stats",  test_rtp_era_update_stats},
    {"test_rtp_era_shutdown_stats",  test_rtp_era_shutdown_stats},
    {"test_rtp_era_rr",  test_rtp_era_rr},
    {"test_rtp_repair_module_init",  test_rtp_repair_module_init},
    {"test_rtp_repair_create_session",  test_rtp_repair_create_session},
    {"test_rtp_repair_upcall_ev",  test_rtp_repair_upcall_ev},
    {"test_rtp_repair_delete_member",  test_rtp_repair_delete_member},
    {"test_rtp_repair_update",  test_rtp_repair_update_stats},
    {"test_rtp_repair_rtcp_methods",  test_rtp_repair_rtcp_methods},
    {"test_vqec_rtcp_construct_packet", test_vqec_rtcp_construct_packet},
    {"test rtcp_receive_report_timeout_cb", test_rtcp_receive_report_timeout_cb},
    
    CU_TEST_INFO_NULL
};
