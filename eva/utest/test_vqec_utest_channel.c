/*
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include <stdint.h>
#include "vqec_pthread.h"
#include "vqec_channel_api.h"
#include "vqec_channel_private.h"
#include "vqec_tuner.h"

int test_vqec_channel_init (void) {
    return 0;
}

int test_vqec_channel_clean (void) {
    return 0;
}

/*
 * Callback test function supplied during a tuner bind operation.
 */
int32_t
test_get_dcr_stats (int32_t context_id,
                            vqec_ifclient_dcr_stats_t *stats)
{
    static boolean picture_decoded = FALSE;

    /* Ensure no frames have been decoded upon first poll */
    if (!picture_decoded) {
        stats->dec_picture_cnt = 0;
        picture_decoded = TRUE;
        goto done;
    }

    stats->dec_picture_cnt = 1;
done:
    return 0;
}

/*
 * Dispatches an event loop to handle events triggered during unit tests.
 *
 * @param[in] arg     - not used
 * @param[out] void * - not used
 */
void *
utest_vqec_utest_channel_dispatch(void *arg)
{
    vqec_event_dispatch();
    return NULL;
}

static void test_vqec_channel_tests (void) {
    vqec_chan_err_t status;
    channel_cfg_t *sdp_cfg1 = NULL, *sdp_cfg2 = NULL, *sdp_cfg3 = NULL;
    vqec_chan_cfg_t cfg1, cfg2, cfg3;
    vqec_chan_t *chan1, *chan2, *chan3, *chan_temp;
    vqec_chanid_t chanid1, chanid2, chanid3, chanid_temp;
    struct in_addr source_addr;
    in_port_t port;
    vqec_bind_params_t *bp;
    vqec_ifclient_get_dcr_info_ops_t dcr_info_ops;
    vqec_tunerid_t tunerid1, tunerid2, tunerid3, tunerid4;
    int chan2_bye_count_before_unbind;
    vqec_nat_bind_data_t bind_data;
    in_port_t rtp_port, rtcp_port;
//    boolean status_bool;
    char *nat_eject_pkt;
    int32_t nat_eject_pkt_len;
    int32_t chan1_nat_ejects_rtp,/* chan1_nat_ejects_rtcp,*/
        chan1_nat_eject_err_drops;
    pthread_t tid;
    boolean chan2_found;
#if HAVE_FCC
    int32_t total_app_parse_errors, total_app_paks;
#endif /* HAVE_FCC */
    channel_cfg_t *cfg_p;
//    in_addr_t source_ip;
//    in_port_t source_port;

    /*
     * Explicily test channel module initialization
     */
    CU_ASSERT(!g_channel_module);

    test_malloc_make_fail(1);
    status = vqec_chan_module_init();
    CU_ASSERT(status != VQEC_CHAN_ERR_OK);
    CU_ASSERT(!g_channel_module);

    test_malloc_make_fail(0);
    status = vqec_chan_module_init();
    CU_ASSERT(status == VQEC_CHAN_ERR_OK);
    CU_ASSERT((int)g_channel_module);
    
    vqec_chan_module_deinit();
    CU_ASSERT(!g_channel_module);

    /*
     * Initialize VQE-C with a valid system config file and an
     * SDP channel file having 3 channels.  The channel module
     * should be initialized as a result.
     */
    vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT((int)g_channel_module);
    
    /*
     * Test active channel database functionality...
     *
     * The initial state that will be built up is as follows:
     *
     *   channels      associated tuners
     *   --------      -----------------
     *   chanid1       tunerid1(bp=rcc), tunerid2(bp=NULL)
     *   chanid2       tunerid3(bp=NULL)
     *   chanid3       tunerid4(bp=NULL)
     */
    /* Get a new channel */
    source_addr.s_addr = htonl((224 << 24) | (1 << 16) | (1 << 8) | 1 );
    port = htons(50000);
    sdp_cfg1 = cfg_get_channel_cfg_from_orig_src_addr(source_addr, port);
    CU_ASSERT((int)sdp_cfg1);
    chanid1 = VQEC_CHANID_INVALID;
    vqec_chan_convert_sdp_to_cfg(sdp_cfg1, VQEC_CHAN_TYPE_LINEAR, &cfg1);
    status = vqec_chan_get_chanid(&chanid1, &cfg1);
    CU_ASSERT(status == VQEC_CHAN_ERR_OK);
    CU_ASSERT(chanid1 != VQEC_CHANID_INVALID);

    /* Find an existing channel */
    chanid_temp = VQEC_CHANID_INVALID;
    status = vqec_chan_get_chanid(&chanid_temp, &cfg1);
    CU_ASSERT(status == VQEC_CHAN_ERR_OK);
    CU_ASSERT(chanid_temp == chanid1);

    /* Add a second active channel */
    chanid2 = VQEC_CHANID_INVALID;
    source_addr.s_addr = htonl((224 << 24) | (1 << 16) | (1 << 8) | 2 );
    port = htons(50000);
    sdp_cfg2 = cfg_get_channel_cfg_from_orig_src_addr(source_addr, port);
    CU_ASSERT((int)sdp_cfg2);
    chanid2 = VQEC_CHANID_INVALID;
    vqec_chan_convert_sdp_to_cfg(sdp_cfg2, VQEC_CHAN_TYPE_LINEAR, &cfg2);
    status = vqec_chan_get_chanid(&chanid2, &cfg2);
    CU_ASSERT(status == VQEC_CHAN_ERR_OK)
    CU_ASSERT(chanid2 != VQEC_CHANID_INVALID);
    
    /* Add a third active channel */
    chanid3 = VQEC_CHANID_INVALID;
    source_addr.s_addr = htonl((224 << 24) | (1 << 16) | (1 << 8) | 3 );
    port = htons(50000);
    sdp_cfg3 = cfg_get_channel_cfg_from_orig_src_addr(source_addr, port);
    CU_ASSERT((int)sdp_cfg3);
    chanid3 = VQEC_CHANID_INVALID;
    vqec_chan_convert_sdp_to_cfg(sdp_cfg3, VQEC_CHAN_TYPE_LINEAR, &cfg3);
    status = vqec_chan_get_chanid(&chanid3, &cfg3);
    CU_ASSERT(status == VQEC_CHAN_ERR_OK)
    CU_ASSERT(chanid3 != VQEC_CHANID_INVALID);

    /* Create a tuner, and bind it to the first channel */
    CU_ASSERT(vqec_tuner_create(&tunerid1, "tuner1") == VQEC_OK);
    bp = vqec_ifclient_bind_params_create();
    vqec_ifclient_bind_params_set_context_id(bp, 0);
    vqec_ifclient_bind_params_enable_rcc(bp);
    vqec_ifclient_bind_params_set_dcr_info_ops(bp, &dcr_info_ops);
    dcr_info_ops.get_dcr_stats = test_get_dcr_stats;
    status = vqec_chan_bind(chanid1, vqec_tuner_get_dptuner(tunerid1), 
                            bp, NULL);
    CU_ASSERT(status == VQEC_CHAN_ERR_OK);

    /* Create a second tuner, and bind it to the first channel */
    CU_ASSERT(vqec_tuner_create(&tunerid2, "tuner2") == VQEC_OK);
    status = vqec_chan_bind(chanid1, vqec_tuner_get_dptuner(tunerid2), 
                            NULL, NULL);
    CU_ASSERT(status == VQEC_CHAN_ERR_OK);

    /* Create a third tuner, and bind it to the second channel */
    CU_ASSERT(vqec_tuner_create(&tunerid3, "tuner3") == VQEC_OK);
    status = vqec_chan_bind(chanid2, vqec_tuner_get_dptuner(tunerid3),
                            NULL, NULL);
    CU_ASSERT(status == VQEC_CHAN_ERR_OK);
    
    /* Create a fourth tuner, and bind it to the third channel */
    CU_ASSERT(vqec_tuner_create(&tunerid4, "tuner4") == VQEC_OK);
    status = vqec_chan_bind(chanid3, vqec_tuner_get_dptuner(tunerid4), 
                            NULL, NULL);
    CU_ASSERT(status == VQEC_CHAN_ERR_OK);

    /*
     * Verify some aspects of the channels' state is as expected.
     * To do this, we pick one of the channels, assume access to the
     * channels' internal data structure, and check some fields.
     */
    chan1 = vqec_chanid_to_chan(chanid1);
    CU_ASSERT((int)chan1);
    chan2 = vqec_chanid_to_chan(chanid2);
    CU_ASSERT((int)chan2);
    chan3 = vqec_chanid_to_chan(chanid3);
    CU_ASSERT((int)chan3);
    CU_ASSERT(chan1->bye_count > 1);
    CU_ASSERT((int)vqec_chan_get_primary_session(chan1));
    CU_ASSERT((int)vqec_chan_get_repair_session(chan1));
    CU_ASSERT(chan1->graph_id != VQEC_DP_GRAPHID_INVALID);
    CU_ASSERT(!chan1->shutdown);
    CU_ASSERT(vqec_chan_id_to_dpchanid(chanid1) != VQEC_DP_CHANID_INVALID);

    /*
     * Verify NAT binding for chanid1 is incomplete, and then
     * issue updates for both RTP & RTCP streams to complete it.
     * Verify the RCC state machine given the NAT_BINDING_COMPLETE
     * event after both streams have completed NAT updates.
     */
    CU_ASSERT(!chan1->rtp_nat_complete && !chan1->rtcp_nat_complete);
    memset(&bind_data, 0, sizeof(bind_data));
    
#if HAVE_FCC
    test_clear_vqec_sm_deliver_event();
    test_vqec_sm_deliver_event_use_interposer(1);
    vqec_chan_nat_bind_update(chanid1, chan1->repair_rtp_nat_id, &bind_data);
    CU_ASSERT(test_validate_vqec_sm_deliver_event(0, NULL, 0, NULL));
    vqec_chan_nat_bind_update(chanid1, chan1->repair_rtcp_nat_id, &bind_data);
    CU_ASSERT(
        test_validate_vqec_sm_deliver_event(1, chan1, 
                                            VQEC_EVENT_NAT_BINDING_COMPLETE,
                                            NULL)); 
    test_vqec_sm_deliver_event_use_interposer(0);
#endif /* HAVE_FCC */    
    /*
     * Verify that the channel mode can retrieve the external bindings for
     * ephemeral ports learned via NAT.
     */
    test_clear_vqec_nat_eject_to_binding();
    test_vqec_nat_query_binding_use_interposer(1,
                                               chan1->repair_rtp_nat_id, 1024,
                                               chan1->repair_rtcp_nat_id, 1028);
    vqec_chan_get_rtx_nat_ports(chan1, &rtp_port, &rtcp_port);
    CU_ASSERT(rtp_port == 1024);
    CU_ASSERT(rtcp_port == 1028);
    test_vqec_nat_query_binding_use_interposer(0, 0, 0, 0, 0);

    /*
     * Verify the channel module passes along NAT eject requests to a
     * NAT module.
     */
    chan1_nat_ejects_rtp = chan1->stats.nat_ejects_rtp;
    chan1_nat_eject_err_drops = chan1->stats.nat_eject_err_drops;
    nat_eject_pkt = "NAT packet response";
    nat_eject_pkt_len = strlen(nat_eject_pkt);
/*    test_vqec_nat_eject_to_binding_use_interposer(1);
    source_ip = 1234;
    source_port = 1234;
    status_bool = vqec_chan_eject_rtp_nat_pak(chanid1,
                                              chan1->dp_chanid,
                                              nat_eject_pkt,
                                              nat_eject_pkt_len,
                                              VQEC_CHAN_RTP_SESSION_REPAIR,
                                              source_ip, source_port);
    CU_ASSERT(status_bool);
    CU_ASSERT(test_validate_vqec_nat_eject_to_binding(1,
                                                      chan1->repair_rtp_nat_id,
                                                      nat_eject_pkt,
                                                      nat_eject_pkt_len));
    CU_ASSERT(chan1->stats.nat_ejects_rtp == chan1_nat_ejects_rtp + 1);
    CU_ASSERT(chan1->stats.nat_eject_err_drops == chan1_nat_eject_err_drops);
    chan1_nat_ejects_rtcp = chan1->stats.nat_ejects_rtcp;
    status_bool = vqec_chan_eject_rtcp_nat_pak(chanid1, nat_eject_pkt,
                                               nat_eject_pkt_len,
                                               VQEC_CHAN_RTP_SESSION_REPAIR,
                                               source_ip, source_port);
    
    CU_ASSERT(status_bool);
    CU_ASSERT(test_validate_vqec_nat_eject_to_binding(2,
                                                      chan1->repair_rtcp_nat_id,
                                                      nat_eject_pkt,
                                                      nat_eject_pkt_len));
    CU_ASSERT(chan1->stats.nat_ejects_rtcp == chan1_nat_ejects_rtcp + 1);
    CU_ASSERT(chan1->stats.nat_eject_err_drops == chan1_nat_eject_err_drops);
*/    test_vqec_nat_eject_to_binding_use_interposer(0);

    /*
     * Test receipt of APP packets
     */
#if HAVE_FCC
    char buf_app[sizeof(ppdd_tlv_t) + 128];
    uint8_t buf_tlvdata[] = {0x00, 0x01, 0x00, 0x5a, 0x00, 0x02, 0x00, 0x14,
                             0x00, 0x00, 0x00, 0x10, 0x00, 0xb0, 0x0d, 0x00,
                             0x01, 0xcf, 0x00, 0x00, 0x00, 0x01, 0xe0, 0x21,
                             0x72, 0x09, 0x22, 0xc1, 0x00, 0x03, 0x00, 0x1e,
                             0x00, 0x21, 0x00, 0x1a, 0x02, 0xb0, 0x17, 0x00,
                             0x01, 0xc9, 0x00, 0x00, 0xe0, 0x20, 0xf0, 0x00,
                             0x02, 0xe0, 0x20, 0xf0, 0x00, 0x04, 0xe0, 0x23,
                             0xf0, 0x00, 0xd4, 0x28, 0x5a, 0x75, 0x00, 0x04,
                             0x00, 0x0c, 0x00, 0x20, 0x00, 0x00, 0x25, 0x04,
                             0xb5, 0x40, 0x80, 0x3d, 0x09, 0x00, 0x00, 0x05,
                             0x00, 0x0c, 0x00, 0x00, 0x08, 0x00, 0x00, 0x21,
                             0x07, 0x00, 0x00, 0x20, 0x04, 0x00};
                             /* a valid APP TLV captured from the wire */
    ppdd_tlv_t *p_rcc_tlv_app = (ppdd_tlv_t *)&buf_app;
    test_vqec_sm_deliver_event_use_interposer(1);

    /* Use an APP which is valid */
    test_clear_vqec_sm_deliver_event();
    memset(p_rcc_tlv_app, 0, sizeof(ppdd_tlv_t) + 128);
    p_rcc_tlv_app->start_seq_number = 1;
    p_rcc_tlv_app->start_rtp_time = 1;
    p_rcc_tlv_app->dt_earliest_join = 1;
    p_rcc_tlv_app->dt_repair_end = 1;
    p_rcc_tlv_app->act_rcc_fill =
        TIME_GET_R(msec, chan1->rcc_max_fill) - 1;
    p_rcc_tlv_app->act_rcc_fill_at_join = 1;
    p_rcc_tlv_app->er_holdoff_time = 1;
    p_rcc_tlv_app->tsrap_len = sizeof(buf_tlvdata);
    memcpy(&p_rcc_tlv_app->tsrap, buf_tlvdata, sizeof(buf_tlvdata));
    vqec_chan_process_app(chan1, p_rcc_tlv_app);

    CU_ASSERT(g_channel_module->total_app_paks == 1);
    CU_ASSERT(g_channel_module->total_app_parse_errors == 0);
    CU_ASSERT(g_channel_module->total_null_app_paks == 0);
    CU_ASSERT(test_validate_vqec_sm_deliver_event(
                  1, chan1, VQEC_EVENT_RECEIVE_VALID_APP, NULL));

    /* Use an APP with actual fill exceeding max fill */
    test_clear_vqec_sm_deliver_event();
    memset(p_rcc_tlv_app, 0, sizeof(ppdd_tlv_t) + 128);
    p_rcc_tlv_app->start_seq_number = 1;
    p_rcc_tlv_app->start_rtp_time = 1;
    p_rcc_tlv_app->dt_earliest_join = 1;
    p_rcc_tlv_app->dt_repair_end = 1;
    p_rcc_tlv_app->act_rcc_fill =
        TIME_GET_R(msec, chan1->rcc_max_fill) + 1;
    p_rcc_tlv_app->act_rcc_fill_at_join = 1;
    p_rcc_tlv_app->er_holdoff_time = 1;
    memcpy(&p_rcc_tlv_app->tsrap, buf_tlvdata, sizeof(buf_tlvdata));
    p_rcc_tlv_app->tsrap_len = sizeof(buf_tlvdata);
    vqec_chan_process_app(chan2, p_rcc_tlv_app);

    CU_ASSERT(g_channel_module->total_app_paks == 2);
    CU_ASSERT(g_channel_module->total_app_parse_errors == 1);
    CU_ASSERT(g_channel_module->total_null_app_paks == 0);
    CU_ASSERT(test_validate_vqec_sm_deliver_event(
                  1, chan2, VQEC_EVENT_RECEIVE_INVALID_APP, NULL));

    /* Use a NULL APP */
    test_clear_vqec_sm_deliver_event();
    memset(p_rcc_tlv_app, 0, sizeof(ppdd_tlv_t) + 128);
    vqec_chan_process_app(chan3, p_rcc_tlv_app);

    CU_ASSERT(g_channel_module->total_app_paks == 3);
    CU_ASSERT(g_channel_module->total_app_parse_errors == 1);
    CU_ASSERT(g_channel_module->total_null_app_paks == 1);
    CU_ASSERT(test_validate_vqec_sm_deliver_event(
                  1, chan3, VQEC_EVENT_RECEIVE_NULL_APP, NULL));

    test_vqec_sm_deliver_event_use_interposer(0);

    /*
     * Test handling of upcall event indicating that an NCSI packet
     * should be sent to VQE-S.  Within NCSI data,
     *   1. set the ER enable timestamp to be 10 ms in the past,
     *      and verify channel ER polling is turned on
     *   2. set the ER enable timestamp to be 10ms in the future,
     *      and verify channel ER polling is not yet turned on
     */
    vqec_dp_upc_ncsi_data_t ncsi_data;
    ncsi_data.first_primary_seq = 0;
    ncsi_data.first_primary_rx_ts = TIME_MK_A(msec, 0);
    ncsi_data.join_latency = TIME_MK_R(msec, 0);
    ncsi_data.er_enable_ts = TIME_SUB_A_R(get_sys_time(), TIME_MK_R(msec, 10));
    chan1->er_poll_active = FALSE;
    vqec_chan_upcall_ncsi_event(chan1, &ncsi_data);
    CU_ASSERT(chan1->er_poll_active);
    ncsi_data.er_enable_ts = TIME_ADD_A_R(get_sys_time(), TIME_MK_R(msec, 10));
    chan1->er_poll_active = FALSE;
    vqec_chan_upcall_ncsi_event(chan1, &ncsi_data);
    CU_ASSERT(!chan1->er_poll_active);
#endif /* HAVE_FCC*/
    /*
     * Unbind chanid2, and
     *   1. validate that the channel is no longer accessible via ID
     *   2. validate that the channel still exists internally (chan ptr),
     *       has sent at least one BYE, and has remaining BYEs to be sent,
     *       and is in the shutdown state
     */
    chan2_bye_count_before_unbind = chan2->bye_count;
    status = vqec_chan_unbind(chanid2, vqec_tuner_get_dptuner(tunerid3));
    CU_ASSERT(status == VQEC_CHAN_ERR_OK);
    CU_ASSERT(!vqec_chanid_to_chan(chanid2));
    CU_ASSERT(chan2->bye_count < chan2_bye_count_before_unbind);
    CU_ASSERT(chan2->bye_count > 0);
    CU_ASSERT(chan2->shutdown);

    /*
     * Create a thread that will execute events that were created and
     * scheduled within the execution of the tests above.  Allow the
     * thread to handle 5 seconds worth of events, then terminate the
     * event loop and wait for it to exit.
     */
    CU_ASSERT(
        vqec_pthread_create(&tid, utest_vqec_utest_channel_dispatch, NULL)==0);
    sleep(5);
    CU_ASSERT(vqec_event_loopexit(NULL));
    CU_ASSERT(pthread_join(tid, NULL) == 0);
    /*
     * Now, perform some simple verifications that the callbacks performed
     * their tasks:
     *   1. chan2 should no longer be in the channel database
     */
    chan2_found = FALSE;
    VQE_LIST_FOREACH(chan_temp, &g_channel_module->channel_list, list_obj) {
        if (chan_temp == chan2) {
            chan2_found = TRUE;
        }
    }
    CU_ASSERT(!chan2_found);
    
    /*
     * Now add chan2 back, and bind it to tuner3 with do_rcc = FALSE
     * for some upcoming slow channel change tests.
     */
    chanid2 = VQEC_CHANID_INVALID;
    source_addr.s_addr = htonl((224 << 24) | (1 << 16) | (1 << 8) | 2 );
    port = htons(50000);
    sdp_cfg2 = cfg_get_channel_cfg_from_orig_src_addr(source_addr, port);
    CU_ASSERT((int)sdp_cfg2);
    chanid2 = VQEC_CHANID_INVALID;
    vqec_chan_convert_sdp_to_cfg(sdp_cfg2, VQEC_CHAN_TYPE_LINEAR, &cfg2);
    status = vqec_chan_get_chanid(&chanid2, &cfg2);
    CU_ASSERT(status == VQEC_CHAN_ERR_OK)
    CU_ASSERT(chanid2 != VQEC_CHANID_INVALID);
    chan2 = vqec_chanid_to_chan(chanid2);
    CU_ASSERT((int)chan2);
    /*
     * Verify
     *   1. ER is turned on immediately w/o waiting for an APP packet.
     *   2. the RCC state machine has been given the SLOW_CHANNEL_CHANGE event
     */
#if HAVE_FCC
    CU_ASSERT(!chan2->er_poll_active);
    test_clear_vqec_sm_deliver_event();
    test_vqec_sm_deliver_event_use_interposer(1);
    vqec_ifclient_bind_params_disable_rcc(bp);
    status = vqec_chan_bind(chanid2, vqec_tuner_get_dptuner(tunerid3), 
                            bp, NULL);
    CU_ASSERT(status == VQEC_CHAN_ERR_OK);
    CU_ASSERT(chan2->er_poll_active);
    CU_ASSERT(test_validate_vqec_sm_deliver_event(
                  1, chan2, VQEC_EVENT_SLOW_CHANNEL_CHANGE, NULL));
   
    /*
     * Validate an APP pkt parse failure would issue a RECEIVE_INVALID_APP
     * event to the state machine, and increment the APP pkt countes.
     */
    test_clear_vqec_sm_deliver_event();
    total_app_parse_errors = g_channel_module->total_app_parse_errors;
    total_app_paks = g_channel_module->total_app_paks;
    vqec_chan_app_parse_failure(chan2);
    CU_ASSERT(test_validate_vqec_sm_deliver_event(
                  1, chan2, VQEC_EVENT_RECEIVE_INVALID_APP, NULL));
    CU_ASSERT(g_channel_module->total_app_parse_errors ==
              total_app_parse_errors + 1);
    CU_ASSERT(g_channel_module->total_app_paks == total_app_paks + 1);

    test_vqec_sm_deliver_event_use_interposer(0);

    /*
     * Validate that an RCC upcall abort event turns on ER immediately
     */   
    chan2->er_poll_active = FALSE;
    vqec_chan_upcall_abort_event(chan2);
    CU_ASSERT(chan2->er_poll_active);
    
#endif /* HAVE_FCC*/

    /* Exercise display functions */
    cfg_p = cfg_get_channel_cfg_from_idx(cfg_get_total_num_channels() - 1);
    CU_ASSERT_NOT_EQUAL(cfg_p, NULL);
    if (cfg_p) {
        vqec_channel_cfg_printf(cfg_p);
    }

    /* Clean up */
    vqec_ifclient_bind_params_destroy(bp);
    vqec_ifclient_deinit();
}

CU_TestInfo test_array_channel[] = {
    {"test vqec_channel_tests", test_vqec_channel_tests},
    CU_TEST_INFO_NULL,
};

