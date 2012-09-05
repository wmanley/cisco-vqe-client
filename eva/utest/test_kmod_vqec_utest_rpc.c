/*
 * Copyright (c) 2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include <vqec_dp_api_types.h>
#include <vqec_dp_api.h>


/*
 * Unit tests for vqec_rpc_client
 */

int test_vqec_rpc_init (void) 
{
    return 0;
}

int test_vqec_rpc_clean (void)
{
    return 0;
}

#define TRUE 1
#define FALSE 0

static vqec_dp_tunerid_t tuner1, tuner2;
static vqec_dp_graphinfo_t graph;

static void
test_vqec_rpc_start (void) 
{
    // Initialize the dataplane module
    vqec_dp_module_init_params_t params;
    vqec_dp_module_init_params_t empty_params;

    memset(&empty_params, 0, sizeof(vqec_dp_module_init_params_t));

    memset(&params, 0, sizeof(vqec_dp_module_init_params_t));
    params.polling_interval = TIME_MK_R(msec, 20);
    params.scheduling_policy.max_classes = 2;
    params.scheduling_policy.polling_interval[0] = 20;
    params.scheduling_policy.polling_interval[1] = 40;
    params.pakpool_size = 5;
    params.max_tuners = 5;
    params.max_paksize = 4096;
    params.max_channels = 4;
    params.max_streams_per_channel = 2;
    params.output_q_limit = 30;
    params.max_iobuf_cnt = 512;
    params.iobuf_recv_timeout = 1000;
    params.fec_enabled = FALSE;

    // Init module before opening module should fail
    CU_ASSERT(vqec_dp_init_module(&params) != VQEC_DP_ERR_OK);

    CU_ASSERT(vqec_dp_open_module() == VQEC_DP_ERR_OK);

    // Initialize DP module
    CU_ASSERT(vqec_dp_init_module(NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_init_module(&empty_params) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_init_module(&params) == VQEC_DP_ERR_OK);

    CU_ASSERT(vqec_dp_init_module(&params) == VQEC_DP_ERR_ALREADY_INITIALIZED);

    // Create upcall sockets - VERIFY THIS CODE
    vqec_dp_upcall_sockaddr_t addr;
    addr.sock.sun_family = AF_UNIX;
    CU_ASSERT(vqec_dp_create_upcall_socket(VQEC_UPCALL_SOCKTYPE_IRQEVENT, &addr, 2048) == VQEC_DP_ERR_OK);

}

static void
test_vqec_rpc_end (void) 
{
    // Deinitialize the dataplane module
    CU_ASSERT(vqec_dp_graph_destroy(graph.id) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_close_upcall_sockets() == VQEC_DP_ERR_OK);  
    CU_ASSERT(vqec_dp_deinit_module() == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_close_module() == VQEC_DP_ERR_OK);
}

static void
test_vqec_rpc_drop (void)
{
    vqec_dp_drop_sim_state_t state, zstate;

    // NULL and empty drop states
    bzero(&state, sizeof(state));
    bzero(&zstate, sizeof(zstate));
    CU_ASSERT(vqec_dp_set_drop_params(NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_set_drop_params(&state) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_get_drop_params(NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_get_drop_params(&state) == VQEC_DP_ERR_OK);
    CU_ASSERT(memcmp(&state, &zstate, sizeof(state)) == 0);

    // Typical interval and continuous drops
    state.en = TRUE;
    state.desc.interval_span = 5;
    state.desc.num_cont_drops = 1;
    CU_ASSERT(vqec_dp_set_drop_params(&state) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_get_drop_params(&state) == VQEC_DP_ERR_OK);
    CU_ASSERT(state.en);
    CU_ASSERT(state.desc.interval_span == 5);
    CU_ASSERT(state.desc.num_cont_drops == 1);

    // Drop Percent
    state.en = TRUE;
    state.desc.interval_span = 0;
    state.desc.num_cont_drops = 0;
    state.desc.drop_percent = 10;
    CU_ASSERT(vqec_dp_set_drop_params(&state) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_get_drop_params(&state) == VQEC_DP_ERR_OK);
    CU_ASSERT(state.en);
    CU_ASSERT(state.desc.drop_percent == 10);
}

static void
test_vqec_rpc_fec (void) 
{
    boolean enabled;

    enabled = TRUE;
    CU_ASSERT(vqec_dp_set_fec(enabled) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_get_fec(NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_get_fec(&enabled) == VQEC_DP_ERR_OK);
    CU_ASSERT(enabled == TRUE);

    enabled = FALSE;
    CU_ASSERT(vqec_dp_set_fec(enabled) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_get_fec(NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_get_fec(&enabled) == VQEC_DP_ERR_OK);
    CU_ASSERT(enabled == FALSE);
}

static void
test_vqec_rpc_tlm (void) 
{
    vqec_pak_pool_status_t status;
    vqec_dp_global_debug_stats_t stats;

    CU_ASSERT(vqec_dp_get_default_pakpool_status(NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_get_default_pakpool_status(&status) == VQEC_DP_ERR_OK);
 
    CU_ASSERT(vqec_dp_get_global_counters(NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_get_global_counters(&stats) == VQEC_DP_ERR_OK);
}

static void
test_vqec_rpc_input_shim (void) 
{
    vqec_dp_input_shim_status_t status;
    vqec_dp_display_ifilter_t filter_array[VQEC_DP_INPUTSHIM_MAX_FILTERS];
    vqec_dp_display_os_t stream_array[VQEC_DP_INPUTSHIM_MAX_STREAMS];
    uint32_t used;

    CU_ASSERT(vqec_dp_input_shim_get_status(NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_input_shim_get_status(&status) == VQEC_DP_ERR_OK);

    CU_ASSERT(vqec_dp_input_shim_get_filters(NULL,0,0) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_input_shim_get_filters(filter_array, VQEC_DP_INPUTSHIM_MAX_FILTERS, &used) == VQEC_DP_ERR_OK);

    CU_ASSERT(vqec_dp_input_shim_get_streams(NULL,0,NULL,0,0) != 
              VQEC_DP_ERR_OK);
    CU_ASSERT(
        vqec_dp_input_shim_get_streams(
            NULL, 0, stream_array,VQEC_DP_INPUTSHIM_MAX_STREAMS, &used)
        == VQEC_DP_ERR_OK);
}

static void
test_vqec_rpc_output_shim (void) 
{
    int count = 0;
    int LOOP_LIMIT = 10;

    // output shim status
    vqec_dp_output_shim_status_t status;
    CU_ASSERT(vqec_dp_output_shim_get_status(NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_output_shim_get_status(&status) == VQEC_DP_ERR_OK);

    // create tuners
    int32_t cp_tuner_id[] = { 1, 2, 3};
    vqec_dp_tunerid_t tuner_id[3];
    char tuner_name[10] = "tuner1";
    CU_ASSERT(vqec_dp_output_shim_create_dptuner(0, NULL, tuner_name, 10) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_output_shim_create_dptuner(cp_tuner_id[0], &tuner_id[0], tuner_name, 10) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_output_shim_create_dptuner(cp_tuner_id[1], &tuner_id[1], tuner_name, 10) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_output_shim_create_dptuner(cp_tuner_id[2], &tuner_id[2], tuner_name, 10) == VQEC_DP_ERR_OK);

    // destroy tuner
    CU_ASSERT(vqec_dp_output_shim_destroy_dptuner(tuner_id[0]) == VQEC_DP_ERR_OK);
    
    // tuner status
    vqec_dp_output_shim_tuner_status_t tuner_status;
    CU_ASSERT(vqec_dp_output_shim_get_tuner_status(tuner_id[0], &tuner_status) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_output_shim_get_tuner_status(tuner_id[1], NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_output_shim_get_tuner_status(tuner_id[1], &tuner_status) == VQEC_DP_ERR_OK);

    // clear counters
    CU_ASSERT(vqec_dp_output_shim_clear_tuner_counters(tuner_id[0]) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_output_shim_clear_tuner_counters(tuner_id[1]) == VQEC_DP_ERR_OK);

    // input streams
    vqec_dp_isid_t isid;
    vqec_dp_display_is_t is_info;
    CU_ASSERT(vqec_dp_output_shim_get_first_is(NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_output_shim_get_first_is(&isid) == VQEC_DP_ERR_OK);
    if (isid != VQEC_DP_INVALID_ISID) {
        // input stream info
        CU_ASSERT(vqec_dp_output_shim_get_is_info(VQEC_DP_INVALID_ISID, &is_info) != VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_output_shim_get_is_info(isid, NULL) != VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_output_shim_get_is_info(isid, &is_info) == VQEC_DP_ERR_OK);

        // print IDs
        char tidstr[1000];
        uint32_t used;
        CU_ASSERT(vqec_dp_output_shim_mappedtidstr(
                VQEC_DP_INVALID_ISID, tidstr, 1000, &used) != VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_output_shim_mappedtidstr(
                isid, NULL, 1000, &used) != VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_output_shim_mappedtidstr(
                isid, tidstr, 1000, &used) == VQEC_DP_ERR_OK);

        CU_ASSERT(vqec_dp_output_shim_get_next_is(VQEC_DP_INVALID_ISID, &isid) != VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_output_shim_get_next_is(isid, NULL) != VQEC_DP_ERR_OK);
        count = 0;
        while (isid != VQEC_DP_INVALID_ISID && count < LOOP_LIMIT) {
            CU_ASSERT(vqec_dp_output_shim_get_next_is(isid, &isid) == VQEC_DP_ERR_OK);
            count++;
        }
    }
    
    // dp tuners
    vqec_dp_tunerid_t tid;
    CU_ASSERT(vqec_dp_output_shim_get_first_dp_tuner(NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_output_shim_get_first_dp_tuner(&tid) == VQEC_DP_ERR_OK);
    count = 0;
    while (tid != VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID && count < LOOP_LIMIT) {
        CU_ASSERT(vqec_dp_output_shim_get_next_dp_tuner(tid, &tid) == VQEC_DP_ERR_OK);
        count++;
    }

    // global tuners for other tests
    tuner1 = tuner_id[1];
    tuner2 = tuner_id[2];
}    

static void
test_vqec_rpc_graph (void) 
{
    vqec_dp_chan_desc_t chan;
    vqec_dp_tunerid_t tid;
    vqec_dp_encap_type_t tuner_encap;

    // initialize primary stream
    vqec_dp_input_stream_t prim;
    bzero(&prim, sizeof(vqec_dp_input_stream_t));
    prim.so_rcvbuf = 0;
    prim.scheduling_class = 0;
    prim.encap = VQEC_DP_ENCAP_RTP;
    // prim.filter = 
    prim.rtp_payload_type = 99;
    prim.datagram_size = 1512;

    // initialize channel
    bzero(&chan, sizeof(vqec_dp_chan_desc_t));
    chan.cp_handle = 2;
    chan.fallback = FALSE;
    chan.avg_pkt_time = TIME_MK_R(msec, 1);
    chan.repair_trigger_time = TIME_MK_R(msec, 100);
    chan.reorder_time = TIME_MK_R(msec, 20);
    chan.jitter_buffer = TIME_MK_R(msec, 200);
    chan.fec_default_block_size = 200;
    chan.en_rcc = FALSE;
    chan.max_rate = 30;
    chan.strip_rtp = FALSE;
    chan.sch_policy = 0;
    chan.primary = prim;
    chan.en_repair = FALSE;
    chan.en_fec0 = FALSE;
    chan.en_fec1 = FALSE;

    tid = tuner1;
    tuner_encap = VQEC_DP_ENCAP_RTP;

    // create graph
    vqec_dp_graphinfo_t graphinfo;
    CU_ASSERT(vqec_dp_graph_create(NULL, tid, tuner_encap, NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_graph_create(&chan, tid, tuner_encap, &graphinfo) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_graph_destroy(graphinfo.id) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_graph_create(&chan, tid, tuner_encap, &graphinfo) == VQEC_DP_ERR_OK);

    // add second tuner
    CU_ASSERT(vqec_dp_graph_add_tuner(graphinfo.id, VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_graph_add_tuner(graphinfo.id, tuner2) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_graph_del_tuner(VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_graph_del_tuner(tuner2) == VQEC_DP_ERR_OK);

    // global graph for other tests
    graph = graphinfo;
}

static void
test_vqec_rpc_rtp (void) 
{
    vqec_dp_streamid_t id;
    vqec_dp_rtp_src_table_t table;
    vqec_dp_rtp_src_key_t key;
    id = graph.inputshim.prim_rtp_id;
    
    // get input stream src table
    CU_ASSERT(vqec_dp_chan_rtp_input_stream_src_get_table(id, NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_rtp_input_stream_src_get_table(id, &table) == VQEC_DP_ERR_OK);
    
    if (table.num_entries > 0) {
        key = table.sources[0].key;

        // enable/disable packet flow
        CU_ASSERT(vqec_dp_chan_rtp_input_stream_src_disable_pktflow(id, NULL) != VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_chan_rtp_input_stream_src_disable_pktflow(id, &key) == VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_chan_rtp_input_stream_src_permit_pktflow(id, NULL, NULL) != VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_chan_rtp_input_stream_src_permit_pktflow(id, &key, NULL) == VQEC_DP_ERR_OK);

        // info
        vqec_dp_rtp_src_entry_info_t info;
        CU_ASSERT(vqec_dp_chan_rtp_input_stream_src_get_info(
                id, NULL, FALSE, FALSE, NULL, NULL, NULL, NULL) != VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_chan_rtp_input_stream_src_get_info(
                id, &key, FALSE, FALSE, NULL, NULL, NULL, NULL) == VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_chan_rtp_input_stream_src_get_info(
                id, &key, FALSE, FALSE, &info, NULL, NULL, NULL) == VQEC_DP_ERR_OK);

        vqec_dp_rtp_sess_info_t sess_info;
        CU_ASSERT(vqec_dp_chan_input_stream_rtp_session_get_info(
                VQEC_DP_STREAMID_INVALID, &sess_info, NULL) != VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_chan_input_stream_rtp_session_get_info(id, NULL, NULL) != VQEC_DP_ERR_OK);
        CU_ASSERT(vqec_dp_chan_input_stream_rtp_session_get_info(id, &sess_info, NULL) == VQEC_DP_ERR_OK);
    }

    // filter
    CU_ASSERT(vqec_dp_chan_input_stream_rtp_session_add_ssrc_filter(
            VQEC_DP_STREAMID_INVALID, 0x12345678, NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_input_stream_rtp_session_add_ssrc_filter(id, 0x12345678, NULL) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_input_stream_rtp_session_del_ssrc_filter(VQEC_DP_STREAMID_INVALID) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_input_stream_rtp_session_del_ssrc_filter(id) == VQEC_DP_ERR_OK);

    // other channel functions
    vqec_dp_chanid_t chanid;
    chanid = graph.dpchan.dpchan_id;

    // upcalls
    vqec_dp_irq_ack_resp_t resp;
    CU_ASSERT(vqec_dp_chan_ack_upcall_irq(
            VQEC_DP_CHANID_INVALID, VQEC_DP_UPCALL_DEV_RTP_PRIMARY, id, &resp) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_ack_upcall_irq(
            chanid, VQEC_DP_UPCALL_DEV_RTP_PRIMARY, VQEC_DP_STREAMID_INVALID, &resp) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_ack_upcall_irq(
            chanid, VQEC_DP_UPCALL_DEV_RTP_PRIMARY, id, NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_ack_upcall_irq(
            chanid, VQEC_DP_UPCALL_DEV_RTP_PRIMARY, id, &resp) != VQEC_DP_ERR_OK);
    

    vqec_dp_irq_poll_t poll;
    CU_ASSERT(vqec_dp_chan_poll_upcall_irq(VQEC_DP_CHANID_INVALID, &poll) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_poll_upcall_irq(chanid, NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_poll_upcall_irq(chanid, &poll) == VQEC_DP_ERR_OK);
    

    // gap report
    vqec_dp_gap_buffer_t gapbuf;
    boolean more;
    CU_ASSERT(vqec_dp_chan_get_gap_report(VQEC_DP_CHANID_INVALID, &gapbuf, &more) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_gap_report(chanid, NULL, NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_gap_report(chanid, &gapbuf, &more) == VQEC_DP_ERR_OK);

    // RCC
#if HAVE_FCC
    vqec_dp_rcc_app_t app;
    memset(&app, 0, sizeof(vqec_dp_rcc_app_t));
    char packet[1024];
    char oversize_packet[8192];
    CU_ASSERT(vqec_dp_chan_process_app(VQEC_DP_CHANID_INVALID, &app, packet, 1024) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_process_app(chanid, NULL, packet, 1024) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_process_app(chanid, &app, NULL, 1024) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_process_app(chanid, &app, packet, 1024) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_process_app(chanid, &app, oversize_packet, 8192) != VQEC_DP_ERR_OK);

    CU_ASSERT(vqec_dp_chan_abort_rcc(VQEC_DP_CHANID_INVALID) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_abort_rcc(chanid) == VQEC_DP_ERR_OK);
#endif

    // miscellaneous
    vqec_dp_pcm_status_t pcm_s;
    vqec_dp_nll_status_t nll_s;
    vqec_dp_fec_status_t fec_s;
    vqec_dp_chan_stats_t chan_s;
    CU_ASSERT(vqec_dp_chan_get_status(VQEC_DP_CHANID_INVALID, NULL, NULL, NULL, NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_status(chanid, NULL, NULL, NULL, NULL) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_status(chanid, &pcm_s, NULL, NULL, NULL) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_status(chanid, NULL, &nll_s, NULL, NULL) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_status(chanid, NULL, NULL, &fec_s, NULL) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_status(chanid, NULL, NULL, NULL, &chan_s) == VQEC_DP_ERR_OK);

    vqec_dp_gaplog_t gap_log;
    vqec_dp_seqlog_t seq_log;
    CU_ASSERT(vqec_dp_chan_get_seqlogs(VQEC_DP_CHANID_INVALID, NULL, NULL, NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_seqlogs(chanid, NULL, NULL, NULL) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_seqlogs(chanid, &gap_log, NULL, NULL) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_seqlogs(chanid, NULL, &gap_log, NULL) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_seqlogs(chanid, NULL, NULL, &seq_log) == VQEC_DP_ERR_OK);

    CU_ASSERT(vqec_dp_chan_clear_stats(VQEC_DP_CHANID_INVALID) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_clear_stats(chanid) == VQEC_DP_ERR_OK);

#if HAVE_FCC
    vqec_dp_rcc_data_t rcc_data;
    CU_ASSERT(vqec_dp_chan_get_rcc_status(VQEC_DP_CHANID_INVALID, &rcc_data) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_rcc_status(chanid, NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_get_rcc_status(chanid, &rcc_data) == VQEC_DP_ERR_OK);
#endif

    // history
    
    boolean enabled;
    CU_ASSERT(vqec_dp_chan_hist_outputsched_enable(TRUE) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_hist_outputsched_is_enabled(NULL != VQEC_DP_ERR_OK));
    CU_ASSERT(vqec_dp_chan_hist_outputsched_is_enabled(&enabled) == VQEC_DP_ERR_OK);
    CU_ASSERT(enabled);
    CU_ASSERT(vqec_dp_chan_hist_outputsched_enable(FALSE) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_chan_hist_outputsched_is_enabled(&enabled) == VQEC_DP_ERR_OK);
    CU_ASSERT(!enabled);
}

static void
test_vqec_rpc_debug (void) 
{
    boolean flag;

    CU_ASSERT(vqec_dp_debug_flag_enable(VQEC_DP_DEBUG_TLM) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_debug_flag_get(VQEC_DP_DEBUG_TLM, NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_debug_flag_get(VQEC_DP_DEBUG_TLM, &flag) == VQEC_DP_ERR_OK);
    CU_ASSERT(flag == TRUE);
    CU_ASSERT(vqec_dp_debug_flag_disable(VQEC_DP_DEBUG_TLM) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_debug_flag_get(VQEC_DP_DEBUG_TLM, &flag) == VQEC_DP_ERR_OK);
    CU_ASSERT(flag == FALSE);

    CU_ASSERT(vqec_dp_debug_flag_enable(VQEC_DP_DEBUG_INPUTSHIM) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_debug_flag_get(VQEC_DP_DEBUG_INPUTSHIM, NULL) != VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_debug_flag_get(VQEC_DP_DEBUG_INPUTSHIM, &flag) == VQEC_DP_ERR_OK);
    CU_ASSERT(flag == TRUE);
    CU_ASSERT(vqec_dp_debug_flag_disable(VQEC_DP_DEBUG_INPUTSHIM) == VQEC_DP_ERR_OK);
    CU_ASSERT(vqec_dp_debug_flag_get(VQEC_DP_DEBUG_INPUTSHIM, &flag) == VQEC_DP_ERR_OK);
    CU_ASSERT(flag == FALSE);
}


CU_TestInfo test_array_rpc[] = {
    {"test vqec_rpc_start", test_vqec_rpc_start},
    {"test vqec_rpc_drop",test_vqec_rpc_drop},
    {"test vqec_rpc_fec",test_vqec_rpc_fec},
    {"test vqec_rpc_tlm",test_vqec_rpc_tlm},
    {"test vqec_rpc_input_shim",test_vqec_rpc_input_shim},
    {"test vqec_rpc_output_shim",test_vqec_rpc_output_shim},
    {"test vqec_rpc_graph",test_vqec_rpc_graph},
    {"test vqec_rpc_rtp",test_vqec_rpc_rtp}, 
    {"test vqec_rpc_debug",test_vqec_rpc_debug},
    {"test vqec_rpc_end", test_vqec_rpc_end},
    CU_TEST_INFO_NULL,
};



