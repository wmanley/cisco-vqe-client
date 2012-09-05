/*------------------------------------------------------------------
 * Ifclient unit tests
 *
 * April 2007, Atif Faheem
 *
 * Copyright (c) 2007-2009 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vqec_ifclient.h"
#include "vqec_tuner.h"
#include "vqec_pthread.h"
#include "vqec_syscfg.h"
#include "vqec_updater.h"

#define VQEC_IFCLIENT_STARTED 2
#define VQEC_IFCLIENT_STOPPED 3

#define LINEAR_SDP "v=0\n\
o=- 1224602055634 1241720866185 IN IP4 vqe-dev-76\n\
s=HD Discovery\n\
i=Channel configuration for HD Discovery ( Unicast Retransmission Only )\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
m=video 53198 RTP/AVPF 96\n\
i=Original Source Stream\n\
c=IN IP4 229.1.1.8/255\n\
b=AS:14910\n\
b=RS:53\n\
b=RR:530000\n\
a=fmtp:96 rtcp-per-rcvr-bw=53\n\
a=recvonly\n\
a=source-filter: incl IN IP4 229.1.1.8 9.3.13.2\n\
a=rtpmap:96 MP2T/90000\n\
a=rtcp:53199 IN IP4 5.3.19.100\n\
a=rtcp-fb:96 nack\n\
a=rtcp-fb:96 nack pli\n\
a=rtcp-xr:pkt-loss-rle stat-summary=loss,dup,jitt post-repair-loss-rle\n\
a=mid:1\n\
m=video 53198 RTP/AVPF 98\n\
i=Re-sourced Stream\n\
c=IN IP4 229.1.1.8/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 229.1.1.8 5.3.19.100\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:53199 IN IP4 5.3.19.100\n\
a=mid:2\n\
m=video 50000 RTP/AVPF 99\n\
i=Unicast Retransmission Stream\n\
c=IN IP4 5.3.19.100\n\
b=RS:53\n\
b=RR:53\n\
a=recvonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:50001\n\
a=fmtp:99 apt=96\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define VOD_SDP "v=0\n\
o=- 4262738225 1158953264 IN IP4 vam.vam-cluster.com\n\
s=VoD 1\n\
i=VoD Session Description\n\
t=0 0\n\
a=group:FID 1 2\n\
m=video 0 RTP/AVPF 96\n\
i=Primary stream\n\
c=IN IP4 0.0.0.0\n\
b=AS:14910\n\
b=RS:55\n\
b=RR:55\n\
a=rtpmap:96 MP2T/90000\n\
a=rtcp-fb:96 nack\n\
a=rtcp-xr:pkt-loss-rle per-loss-rle stat-summary=loss,dupp,jitt\n\
a=mid:1\n\
m=video 0 RTP/AVPF 99\n\
i=Retransmission stream\n\
c=IN IP4 0.0.0.0\n\
b=RS:55\n\
b=RR:55\n\
a=rtpmap:99 rtx/90000\n\
a=fmtp:99 apt=96\n\
a=mid:2\n"

extern int32_t s_vqec_ifclient_state;

int test_vqec_ifclient_init (void)
{
    return 0;
}

int test_vqec_ifclient_clean (void)
{
    /* Leave VQE-C services in an ininitialized state for subsequent tests */
    vqec_ifclient_deinit();
    return 0;
}

static void *test_vqec_run_start_loop (void *arg)
{
    vqec_ifclient_start();
    return (NULL);
}

void test_vqec_ifclient_init_test (void)
{
    vqec_error_t err;
    vqec_tunerid_t id;
    pthread_t tid;
    vqec_updater_status_t status;

    /* de-init prior to initializing -- make sure no crashes */
    vqec_ifclient_deinit();
    CU_PASS("");

    /*
     * Attempt an init where the data plane initialization will fail,
     * then verify that a tuner cannot be created when VQE-C is not
     * initialized.
     */
    test_vqec_dp_init_module_use_interposer(VQEC_DP_ERR_NOMEM);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid_cdi.cfg");
    CU_ASSERT(err != VQEC_OK);
    err = vqec_ifclient_tuner_create(&id, "0");
    CU_ASSERT(err != VQEC_OK);
    test_vqec_dp_init_module_use_interposer(0);    

    /*
     * Test initialization of VQE-C with various errors from reading
     *  a locally stored channel SDP config file
     */
    test_vqec_cfg_init_make_fail(CFG_FAILURE);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_deinit();

    test_vqec_cfg_init_make_fail(CFG_OPEN_FAILED);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_deinit();

    test_vqec_cfg_init_make_fail(CFG_INVALID_HANDLE);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_deinit();

    test_vqec_cfg_init_make_fail(CFG_GET_FAILED);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_deinit();

    test_vqec_cfg_init_make_fail(CFG_ALREADY_INIT);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_deinit();

    test_vqec_cfg_init_make_fail(CFG_UNINITIALIZED);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_deinit();

    test_vqec_cfg_init_make_fail(CFG_MALLOC_REQ);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_deinit();

    test_vqec_cfg_init_make_fail(10000);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_deinit();

    test_vqec_cfg_init_make_fail(0);

    /*
     * Verify that a valid config file defining "tuner1" can be read,
     * and that "tuner1" is created.
     */
    err = vqec_ifclient_init("data/cfg_test_all_params_valid_with_tuners.cfg");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(vqec_ifclient_check_tuner_validity_by_name("tuner1"));
    vqec_ifclient_deinit();

    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_deinit();
    
    /*
     * Verify that the updater service is started during initialization,
     * and stopped after deinitialization.
     */
    err = vqec_ifclient_init("data/cfg_test_all_params_valid_cdi.cfg");
    CU_ASSERT(err == VQEC_OK);
    err = vqec_updater_get_status(&status, NULL, NULL, NULL, NULL, NULL, NULL);
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(status == VQEC_UPDATER_STATUS_RUNNING);
    vqec_ifclient_deinit();
    err = vqec_updater_get_status(&status, NULL, NULL, NULL, NULL, NULL, NULL);
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT((status == VQEC_UPDATER_STATUS_UNINITIALIZED) ||
              (status == VQEC_UPDATER_STATUS_DYING));
    sleep(5);
    err = vqec_updater_get_status(&status, NULL, NULL, NULL, NULL, NULL, NULL);
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(status == VQEC_UPDATER_STATUS_UNINITIALIZED);

    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);

    /*
     * Verify no crashes if VQE-C services are started and stopped between
     * initialization and deinitialization.
     */
    vqec_pthread_create(&tid, test_vqec_run_start_loop, NULL);
    sleep(5);
    vqec_ifclient_stop();
    pthread_join(tid, NULL);
    
    vqec_ifclient_deinit();
    CU_PASS("");
}

void test_vqec_ifclient_sdp_set (void)
{
    vqec_error_t err;
    pthread_t tid;

    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);

    char *bfs_contents = NULL;
    int size, readSize;
    FILE *fp = fopen("data/utest_tuner.cfg", "read");
    CU_ASSERT(fp != NULL);

    size = fseek(fp, 0L, SEEK_END);
    bfs_contents = (char *) malloc(size);
    CU_ASSERT(bfs_contents != NULL);

    rewind(fp);
    readSize = fread(bfs_contents, sizeof(char), size, fp);
    err = vqec_ifclient_cfg_channel_lineup_set(bfs_contents);
    CU_ASSERT(err == VQEC_OK);

    /*  must be able to set config in either started or stopped state */
    vqec_pthread_create(&tid, test_vqec_run_start_loop, NULL);
    sleep(5);
    rewind(fp);
    readSize = fread(bfs_contents, sizeof(char), size, fp);
    /* state = STARTED */
    CU_ASSERT(s_vqec_ifclient_state == VQEC_IFCLIENT_STARTED);
    err = vqec_ifclient_cfg_channel_lineup_set(bfs_contents);    
    CU_ASSERT(err == VQEC_OK);    
    vqec_ifclient_stop();
    pthread_join(tid, NULL);

    rewind(fp);
    readSize = fread(bfs_contents, sizeof(char), size, fp);
    /* state = STOPPED */
    CU_ASSERT(s_vqec_ifclient_state == VQEC_IFCLIENT_STOPPED);
    err = vqec_ifclient_cfg_channel_lineup_set(bfs_contents);
    CU_ASSERT(err == VQEC_OK);    

    free(bfs_contents);
    fclose(fp);

    vqec_ifclient_deinit();
    CU_PASS("");
}


void test_vqec_ifclient_dyn_chan_tests (void)
{
    vqec_error_t err;
    vqec_tunerid_t tid;
    vqec_sdp_handle_t hdl1, hdl2, hdl3;

    err = vqec_ifclient_init("data/cfg_test_dynamic_chans.cfg");
    CU_ASSERT(err == VQEC_OK);

    err = vqec_ifclient_tuner_create(&tid, "0");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(tid != VQEC_TUNERID_INVALID);

    /*
     * Test some attempts to bind the tuner to a channel specified by an
     * SDP handle.  The tuner ID must be valid, and SDP handle must map to
     * a channel in the channel database for the binding to be successful.
     */

    /* Bind valid tuner to invalid dynamic channel */
    err = vqec_ifclient_tuner_bind_chan(tid, "o=tmpchan234", NULL);
    CU_ASSERT(err != VQEC_OK);

    /* Alloc dynamic channel by URL, bind it to tuner */
    hdl1 = 
        vqec_ifclient_alloc_sdp_handle_from_url("udp://235.12.13.14:50000");
    err = vqec_ifclient_tuner_bind_chan(tid, hdl1, NULL);
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_free_sdp_handle(hdl1);

    /* Alloc too many dynamic channels (dependent on max_tuners = 2) */
    hdl1 = 
        vqec_ifclient_alloc_sdp_handle_from_url("udp://235.12.13.24:50000");
    CU_ASSERT(hdl1 != NULL);
    hdl2 = 
        vqec_ifclient_alloc_sdp_handle_from_url("udp://235.12.13.34:50000");
    CU_ASSERT(hdl2 != NULL);
    hdl3 = 
        vqec_ifclient_alloc_sdp_handle_from_url("udp://235.12.13.44:50000");
    CU_ASSERT(hdl3 == NULL);

    /* free one to make room for another */
    vqec_ifclient_free_sdp_handle(hdl2);

    hdl3 = 
        vqec_ifclient_alloc_sdp_handle_from_url("udp://235.12.13.44:50000");
    CU_ASSERT(hdl3 != NULL);

    /* bind to first one */
    err = vqec_ifclient_tuner_bind_chan(tid, hdl1, NULL);
    printf("calling tuner_bind for allocated sdp handle \"%s\"...\n", hdl1);
    CU_ASSERT(err == VQEC_OK);

    /* free sdp handles */
    printf("freeing sdp handles\n");
    vqec_ifclient_free_sdp_handle(hdl1);
    vqec_ifclient_free_sdp_handle(hdl2);
    vqec_ifclient_free_sdp_handle(hdl3);

    vqec_ifclient_deinit();
}

void test_vqec_ifclient_tuner_tests (void)
{
    vqec_error_t err;
    vqec_tunerid_t id[2];
    vqec_tunerid_t id1, id2, id3, id_tmp;
    char name[VQEC_MAX_TUNER_NAMESTR_LEN];
    vqec_sdp_handle_t sdp_handle;
    vqec_tuner_iter_t iter;
    int i;
    vqec_iobuf_t iobuf[2];
    int32_t bytes_read;
    char test_pakbuf[VQEC_MSG_MAX_DATAGRAM_LEN],
        test_pakbuf1[VQEC_MSG_MAX_DATAGRAM_LEN];
    vqec_ifclient_updater_stats_t updater_stats;
    vqec_ifclient_stats_t stats, stats_zeroed;
#define VQEC_RCC_ABORT_STRLEN 40
    char rcc_abort_str[VQEC_RCC_ABORT_STRLEN];
    boolean after_clear, before_clear, after_clear_cumulative, rv;
    vqec_chan_cfg_t cfg;

    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);

    /* 
     * Perform some basic tuner creation tests.
     * Since the ifclient layer for these functions is just a wrapper for
     * the tuner module APIs, which are tested more thoroughly, the tests
     * for these are not exhaustive.
     */
    err = vqec_ifclient_tuner_create(NULL, NULL);
    CU_ASSERT(err == VQEC_ERR_INVALIDARGS);
    
    err = vqec_ifclient_tuner_create(&id1, "0");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(id1 != VQEC_TUNERID_INVALID);

    err = vqec_ifclient_tuner_create(&id2, "0");
    CU_ASSERT(err == VQEC_ERR_EXISTTUNER);
    CU_ASSERT(id2 == VQEC_TUNERID_INVALID);

    err = vqec_ifclient_tuner_create(&id2, "1");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(id2 != VQEC_TUNERID_INVALID);

    err = vqec_ifclient_tuner_create(&id3, "2");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(id3 != VQEC_TUNERID_INVALID);

    /* Make sure tuner's name matches what was given upon creation */
    err = vqec_ifclient_tuner_get_name_by_id(id1, name,
                                             VQEC_MAX_TUNER_NAMESTR_LEN);
    CU_ASSERT(!strcmp(name, "0"));
    CU_ASSERT(err == VQEC_OK);
    err = vqec_ifclient_tuner_get_name_by_id(id2, name,
                                             VQEC_MAX_TUNER_NAMESTR_LEN);
    CU_ASSERT(!strcmp(name, "1"));
    CU_ASSERT(err == VQEC_OK);
    err = vqec_ifclient_tuner_get_name_by_id(id3, name,
                                             VQEC_MAX_TUNER_NAMESTR_LEN);
    CU_ASSERT(!strcmp(name, "2"));
    CU_ASSERT(err == VQEC_OK);

    /* Make sure tuners can be looked up by name */
    CU_ASSERT(id1 == vqec_ifclient_tuner_get_id_by_name("0"));
    CU_ASSERT(id2 == vqec_ifclient_tuner_get_id_by_name("1"));
    CU_ASSERT(id3 == vqec_ifclient_tuner_get_id_by_name("2"));

    /*
     * Test some attempts to bind the tuner to a channel specified by an
     * SDP handle.  The tuner ID must be valid, and SDP handle must map to
     * a channel in the channel database for the binding to be successful.
     */

    /* Bind invalid tuner to an invalid/non-existent channel */
    err = vqec_ifclient_tuner_bind_chan(VQEC_TUNERID_INVALID,
                                        "invalid SDP handle",
                                        NULL);
    CU_ASSERT(err != VQEC_OK);

    /* Bind valid tuner to invalid channel */
    err = vqec_ifclient_tuner_bind_chan(id1, "invalid SDP handle", NULL);
    CU_ASSERT(err != VQEC_OK);

    /* Bind invalid tuner to valid channel */
    err = vqec_ifclient_tuner_bind_chan(
        VQEC_TUNERID_INVALID,
        "- 1168369302 1168369301 IN IP4 vam-cluster.cisco.com",
        NULL);
    CU_ASSERT(err != VQEC_OK);

    /*
     * Bind tuner to a valid channel, but with no memory to create channel.
     */
    test_malloc_make_fail(1);
    err = vqec_ifclient_tuner_bind_chan(
        id1,
        "- 1168369302 1168369301 IN IP4 vam-cluster.cisco.com",
        NULL);
    CU_ASSERT(err != VQEC_OK);
    test_malloc_make_fail(0);

    /* Bind tuner to a valid channel, should succeed */
    err = vqec_ifclient_tuner_bind_chan(
        id1,
        "- 1168369302 1168369301 IN IP4 vam-cluster.cisco.com",
        NULL);
    CU_ASSERT(err != VQEC_OK);

    /* Bind tuner to a the same channel, should fail */
    err = vqec_ifclient_tuner_bind_chan(
        id1,
        "- 1168369302 1168369301 IN IP4 vam-cluster.cisco.com",
        NULL);
    CU_ASSERT(err != VQEC_OK);

    /* Look-up channel by URL, bind it to second tuner */
    sdp_handle = 
        vqec_ifclient_alloc_sdp_handle_from_url("udp://224.1.1.1:50000");
    err = vqec_ifclient_tuner_bind_chan(id2, sdp_handle, NULL);
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_free_sdp_handle(sdp_handle);

    /* 
     * Test attempts to bind a tuner to a channel specified via a
     * vqec_chan_cfg_t structure. 
     */
    
    /* Bind invalid tuner to NULL cfg */
    err = vqec_ifclient_tuner_bind_chan_cfg(VQEC_TUNERID_INVALID,
                                            NULL, NULL);
    CU_ASSERT(err != VQEC_OK);
    
    /* Construct a valid channel cfg */
    rv = vqec_ifclient_chan_cfg_parse_sdp(&cfg, LINEAR_SDP, 
                                          VQEC_CHAN_TYPE_LINEAR);
    CU_ASSERT_EQUAL(rv, TRUE);
    
    /* Bind invalid tuner to valid cfg */
    err = vqec_ifclient_tuner_bind_chan_cfg(VQEC_TUNERID_INVALID,
                                            &cfg, NULL);
    CU_ASSERT(err != VQEC_OK);

    /* Bind tuner to valid cfg, with no memory */
    test_malloc_make_fail(1);
    err = vqec_ifclient_tuner_bind_chan_cfg(id3, &cfg, NULL);
    test_malloc_make_fail(0);

    /* Bind valid tuner to valid chan cfg */
    err = vqec_ifclient_tuner_bind_chan_cfg(id3, &cfg, NULL);
    CU_ASSERT(err == VQEC_OK);

    /* Bind tuner to the same chan cfg */
    err = vqec_ifclient_tuner_bind_chan_cfg(id3, &cfg, NULL);
    CU_ASSERT(err != VQEC_OK);

    /* Unbind and destroy tuner "2" */
    err = vqec_ifclient_tuner_unbind_chan(id3);
    CU_ASSERT(err == VQEC_OK);
    err = vqec_ifclient_tuner_destroy(id3);
    CU_ASSERT(err == VQEC_OK);

    /*
     * Iterate over tuners, verify
     *   - invalid parameters are handled properly
     *   - all tuner IDs can be retrieved
     */
    id_tmp = vqec_ifclient_tuner_iter_init_first(NULL);
    CU_ASSERT(id_tmp == VQEC_TUNERID_INVALID);

    id_tmp = vqec_ifclient_tuner_iter_init_first(&iter);
    CU_ASSERT((id_tmp == id1) || (id_tmp == id2) || (id_tmp == id3));
    i = 0;
    FOR_ALL_TUNERS_IN_LIST(iter, id_tmp) {
        id[i] = id_tmp;
        i++;
    }
    CU_ASSERT(((id[0] == id1) && (id[1] == id2)) ||
              ((id[0] == id2) && (id[1] == id1)));
    

    /*
     * Try to read data which has arrived on a tuner,
     * and verify the function calls the output shim as expected.
     */
    iobuf[0].buf_ptr = test_pakbuf;
    iobuf[0].buf_len = VQEC_MSG_MAX_DATAGRAM_LEN;
    iobuf[1].buf_ptr = test_pakbuf1;
    iobuf[1].buf_len = VQEC_MSG_MAX_DATAGRAM_LEN;

    test_clear_vqec_dp_output_shim_tuner_read();
    test_vqec_dp_output_shim_tuner_read_use_interposer(
        VQEC_DP_ERR_NOSUCHTUNER);
    CU_ASSERT(vqec_ifclient_tuner_recvmsg(id1, iobuf, 2, &bytes_read, 1) ==
              VQEC_ERR_NOSUCHTUNER);
    CU_ASSERT(
        test_validate_vqec_dp_output_shim_tuner_read(
            1, vqec_tuner_get_dptuner(id1), (vqec_iobuf_t *)iobuf, 2,
            (uint32_t *)&bytes_read, 1));

    test_clear_vqec_dp_output_shim_tuner_read();
    test_vqec_dp_output_shim_tuner_read_use_interposer(
        VQEC_DP_ERR_NOSUCHSTREAM);
    CU_ASSERT(vqec_ifclient_tuner_recvmsg(id1, iobuf, 2, &bytes_read, 1) ==
              VQEC_ERR_NOBOUNDCHAN);

    test_clear_vqec_dp_output_shim_tuner_read();
    test_vqec_dp_output_shim_tuner_read_use_interposer(
        VQEC_DP_ERR_INVALIDARGS);
    CU_ASSERT(vqec_ifclient_tuner_recvmsg(id1, iobuf, 2, &bytes_read, 1) ==
              VQEC_ERR_INVALIDARGS);
        
    test_clear_vqec_dp_output_shim_tuner_read();
    test_vqec_dp_output_shim_tuner_read_use_interposer(
        VQEC_DP_ERR_INTERNAL);
    CU_ASSERT(vqec_ifclient_tuner_recvmsg(id1, iobuf, 2, &bytes_read, 1) ==
              VQEC_ERR_SYSCALL);

    test_clear_vqec_dp_output_shim_tuner_read();
    test_vqec_dp_output_shim_tuner_read_use_interposer(
        VQEC_DP_ERR_NOMEM);
    CU_ASSERT(vqec_ifclient_tuner_recvmsg(id1, iobuf, 2, &bytes_read, 1) ==
              VQEC_ERR_MALLOC);

    test_clear_vqec_dp_output_shim_tuner_read();
    test_vqec_dp_output_shim_tuner_read_use_interposer(
        VQEC_DP_ERR_NOMEM);
    CU_ASSERT(vqec_ifclient_tuner_recvmsg(id1, iobuf, 2, &bytes_read, 1) ==
              VQEC_ERR_MALLOC);

    test_clear_vqec_dp_output_shim_tuner_read();
    test_vqec_dp_output_shim_tuner_read_use_interposer(
        VQEC_DP_ERR_MAX);
    CU_ASSERT(vqec_ifclient_tuner_recvmsg(id1, iobuf, 2, &bytes_read, 1) ==
              VQEC_ERR_UNKNOWN);

    /*
     * Trigger update of channel config stats,
     * and verify the number of channels is retrievable.
     */
    memset(&updater_stats, 0, sizeof(updater_stats));
    vqec_ifclient_trigger_update();
    CU_ASSERT(vqec_ifclient_updater_get_stats(&updater_stats) == VQEC_OK);
    CU_ASSERT(updater_stats.chan_cfg_total_channels == 3);

    /*
     * Get stats, verify success.
     * Clear stats, and verify the packet counters are zero.
     */
    err = vqec_ifclient_get_stats(&stats);
    CU_ASSERT(err == VQEC_OK);
    memset(&stats_zeroed, 0, sizeof(stats_zeroed));
    before_clear = memcmp(&stats, &stats_zeroed, sizeof(stats));
    vqec_ifclient_clear_stats();
    err = vqec_ifclient_get_stats(&stats);
    CU_ASSERT(err == VQEC_OK);
    memset(&stats_zeroed, 0, sizeof(stats_zeroed));
    after_clear = memcmp(&stats, &stats_zeroed, sizeof(stats));
    err = vqec_ifclient_get_stats_cumulative(&stats);
    CU_ASSERT(err == VQEC_OK);
    memset(&stats_zeroed, 0, sizeof(stats_zeroed));
    after_clear_cumulative = memcmp(&stats, &stats_zeroed, sizeof(stats));

    if (before_clear) {
        /* Assert that all stats are zero after clearing */
        CU_ASSERT(!after_clear);
        CU_ASSERT(after_clear_cumulative);
    } else if (!before_clear) {
        CU_ASSERT(!after_clear);
        CU_ASSERT(!after_clear_cumulative);
    }

    /* test bad string pointer */
    CU_ASSERT(!vqec_ifclient_tuner_rccabort2str(
                  NULL, VQEC_RCC_ABORT_STRLEN, 
                  VQEC_IFCLIENT_RCC_ABORT_SERVER_REJECT));
    /* test bad string length */
    CU_ASSERT(!vqec_ifclient_tuner_rccabort2str(
                  rcc_abort_str, 0, 
                  VQEC_IFCLIENT_RCC_ABORT_SERVER_REJECT));
    /* test valid case */
    CU_ASSERT(vqec_ifclient_tuner_rccabort2str(
                  rcc_abort_str, VQEC_RCC_ABORT_STRLEN,
                  VQEC_IFCLIENT_RCC_ABORT_SERVER_REJECT) != NULL);

    /* test bad string pointer */
    CU_ASSERT(!vqec_ifclient_tuner_rccabort2str(
                  NULL, VQEC_RCC_ABORT_STRLEN, 
                  VQEC_IFCLIENT_RCC_ABORT_SERVER_REJECT));
    /* test bad string length */
    CU_ASSERT(!vqec_ifclient_tuner_rccabort2str(
                  rcc_abort_str, 0, 
                  VQEC_IFCLIENT_RCC_ABORT_SERVER_REJECT));
    /* test valid case */
    CU_ASSERT(vqec_ifclient_tuner_rccabort2str(
                  rcc_abort_str, VQEC_RCC_ABORT_STRLEN,
                  VQEC_IFCLIENT_RCC_ABORT_SERVER_REJECT) != NULL);
    /*
     * Perform some tests related to tuner unbind and destroy:
     *   1. unbind a bound tuner
     *   2. unbind an unbound tuner, verify no error given
     *   3. destroy an unbound tuner
     *   4. unbind a non-existent tuner
     *   5. destroy a non-existent tuner
     *   6. destroy a bound tuner
     */
    err = vqec_ifclient_tuner_unbind_chan(id2);
    CU_ASSERT(err == VQEC_OK);
    err = vqec_ifclient_tuner_unbind_chan(id2);
    CU_ASSERT(err == VQEC_OK);
    err = vqec_ifclient_tuner_destroy(id2);
    CU_ASSERT(err == VQEC_OK);
    err = vqec_ifclient_tuner_unbind_chan(id2);
    CU_ASSERT(err != VQEC_OK);
    err = vqec_ifclient_tuner_destroy(id2);
    CU_ASSERT(err != VQEC_OK);
    err = vqec_ifclient_tuner_destroy(id1);
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(vqec_ifclient_tuner_recvmsg(id1, iobuf, 2, &bytes_read, 1) !=
              VQEC_ERR_NOSUCHTUNER);
}

void test_vqec_ifclient_rtp_header_strip (void)
{
    /*
     * Valid RTP test packet.  Wireshark packet decode output for test pkt is:
     *
     * Real-Time Transport Protocol
     *     10.. .... = Version: RFC 1889 Version (2)
     *     ..0. .... = Padding: False
     *     ...0 .... = Extension: False
     *     .... 0000 = Contributing source identifiers count: 0
     *     0... .... = Marker: False
     *     Payload type: MPEG-II transport streams (33)
     *     Sequence number: 46774
     *     Timestamp: 11537639
     *     Synchronization Source identifier: 3016504294
     *     Payload: E6D347004CD94BC797860A31FCA51ECEF016AC03B34929C3...
     */
    uint8_t buf_rtp_packet[] = {
        0x80, 0x21, 0xb6, 0xb6, 0x00, 0xb0, 0x0c, 0xe7, 0xb3, 0xcc, 0x33,
        0xe6, 0xe6, 0xd3, 0x47, 0x00, 0x4c, 0xd9, 0x4b, 0xc7, 0x97, 0x86,
        0x0a, 0x31, 0xfc, 0xa5, 0x1e, 0xce, 0xf0, 0x16, 0xac, 0x03, 0xb3,
        0x49, 0x29, 0xc3, 0xa4, 0xd3, 0x78, 0x8e, 0xf0, 0x52, 0xd6, 0xdd,
        0x9a, 0x90, 0x0d, 0x72, 0x2c, 0x35, 0x1d, 0x6c, 0xee, 0xa1, 0x63,
        0x68, 0xa2, 0x2c, 0xfd, 0xe9, 0xc3, 0xfb, 0x1e, 0x44, 0xe9, 0xf9,
        0xd0, 0xd9, 0x18, 0xae, 0xe2, 0xe7, 0xca, 0x6a, 0x66, 0xda, 0x4a,
        0x49, 0xca, 0x5a, 0x17, 0x8f, 0x11, 0x1a, 0x96, 0xe9, 0x85, 0x45,
        0x28, 0x45, 0x67, 0x17, 0x14, 0xa2, 0xba, 0x30, 0xdb, 0x08, 0xb4,
        0x2b, 0x9c, 0x6c, 0x4a, 0xaf, 0x99, 0xba, 0xc9, 0x2f, 0x32, 0x5f,
        0x2a, 0xc0, 0x96, 0x9b, 0xdc, 0x30, 0x6f, 0xbf, 0xd7, 0x93, 0x52,
        0xe2, 0xda, 0xc0, 0x5a, 0x8a, 0x21, 0x01, 0x88, 0xbb, 0xdc, 0x46,
        0x33, 0xed, 0xc3, 0x4e, 0x8e, 0x9a, 0xda, 0x20, 0xfd, 0xa6, 0xed,
        0xd5, 0x5a, 0xef, 0xf6, 0x14, 0xff, 0xbf, 0x27, 0x78, 0xc3, 0xa9,
        0xe3, 0x3a, 0x37, 0x73, 0xe9, 0x32, 0x5f, 0x39, 0x42, 0x30, 0xf5,
        0xa2, 0xc8, 0xd5, 0xd5, 0xb1, 0xed, 0x0b, 0xc4, 0xc7, 0x8a, 0xa5,
        0xcf, 0x23, 0x2d, 0xf3, 0xc0, 0xf9, 0x0b, 0x93, 0xc8, 0xb0, 0x98,
        0x35, 0xa3, 0xa6, 0x69, 0x19, 0xab, 0xc0, 0xfb, 0x94, 0xf4, 0xd1,
        0x40, 0xb3, 0x97, 0x1d, 0x47, 0x00, 0x4c, 0xda, 0x77, 0x35, 0x90,
        0x22, 0xdf, 0xfb, 0x25, 0xa1, 0x37, 0xc8, 0x4a, 0x40, 0x52, 0xa8,
        0xf8, 0xe8, 0xfc, 0xd0, 0x29, 0xfa, 0xeb, 0x19, 0x92, 0xc8, 0xc5,
        0x68, 0xdc, 0x9b, 0x12, 0x27, 0x80, 0x38, 0x4e, 0x83, 0xcc, 0x25,
        0x1d, 0x14, 0xf5, 0x79, 0x20, 0xc7, 0xd5, 0x1e, 0x7f, 0x77, 0x30,
        0xdd, 0xc8, 0x52, 0xa4, 0x76, 0x84, 0xe8, 0x23, 0x33, 0xc0, 0x44,
        0xe0, 0x82, 0xcf, 0x40, 0x17, 0x0b, 0xac, 0xa7, 0x02, 0xbc, 0xfb,
        0x8b, 0xda, 0x05, 0x2b, 0x89, 0xc5, 0xb7, 0x31, 0x3b, 0xb8, 0x8e,
        0xd0, 0x53, 0x4c, 0x09, 0x7c, 0xba, 0xe8, 0xdd, 0x90, 0xb8, 0xda,
        0x84, 0x11, 0x12, 0x3f, 0xf7, 0x87, 0x38, 0xd9, 0xdc, 0x9b, 0xd7,
        0x1c, 0x4c, 0xc0, 0x53, 0x4b, 0x9d, 0x23, 0x55, 0x17, 0x96, 0x5c,
        0xd6, 0xe8, 0x0f, 0x1b, 0x66, 0x50, 0x2f, 0x4a, 0x80, 0x0e, 0x45,
        0xba, 0x86, 0xc4, 0xd0, 0x9f, 0x0b, 0x68, 0x92, 0xae, 0xf5, 0xa2,
        0xb7, 0xf4, 0x7b, 0x1e, 0x2d, 0x13, 0xba, 0xfb, 0x20, 0x20, 0xe7,
        0x14, 0xec, 0x65, 0xfb, 0x52, 0xfc, 0xfe, 0x2d, 0x1e, 0xe2, 0x55,
        0xa4, 0x78, 0xb4, 0xe1, 0x01, 0xda, 0x72, 0xcd, 0xd0, 0xb0, 0x18,
        0xcc, 0x9c, 0x63, 0xc7, 0x29, 0x86, 0x86, 0x7d, 0xba, 0x5e, 0x5a,
        0x37, 0xde, 0x42, 0xf2, 0x90, 0x47, 0x00, 0x4c, 0xdb, 0x12, 0x53,
        0x37, 0x27, 0x3f, 0x76, 0xb2, 0xd0, 0x09, 0xaa, 0x19, 0x23, 0x32,
        0xfa, 0xff, 0xd8, 0x87, 0x2d, 0x5b, 0xcc, 0xc6, 0x15, 0x2d, 0xb3,
        0xd9, 0xd4, 0x29, 0x25, 0x3d, 0x8c, 0x58, 0xf7, 0xcb, 0x4b, 0xb4,
        0xe2, 0xdc, 0x2c, 0xa1, 0x45, 0xe5, 0xb1, 0x6e, 0x05, 0x14, 0x36,
        0x23, 0x8d, 0x0f, 0x3a, 0x42, 0xce, 0x61, 0x52, 0x17, 0x6d, 0x49,
        0xbb, 0x33, 0x58, 0x51, 0x38, 0x51, 0x6f, 0xa0, 0x5b, 0x4d, 0x9b,
        0xf4, 0xfd, 0x51, 0xa3, 0xe7, 0xb8, 0x80, 0x5a, 0x1a, 0xc6, 0xe4,
        0xf1, 0xf8, 0xe9, 0x6c, 0x86, 0xfe, 0xad, 0x29, 0x09, 0x76, 0x62,
        0x88, 0x30, 0xc1, 0x37, 0x9c, 0x20, 0xfc, 0xd3, 0x13, 0xae, 0xc0,
        0x9e, 0x46, 0xdd, 0x23, 0xa2, 0x55, 0xc5, 0xd0, 0x64, 0x69, 0x4b,
        0xb5, 0xce, 0xce, 0xd0, 0xf1, 0xf4, 0xf1, 0x64, 0x64, 0x03, 0x43,
        0x71, 0xb5, 0xc0, 0x7b, 0xce, 0x8c, 0xc3, 0x78, 0x96, 0xa7, 0x6b,
        0xfd, 0xe5, 0xd6, 0x0f, 0x00, 0x7b, 0x88, 0xb5, 0xec, 0xfa, 0x00,
        0xdc, 0x28, 0x1b, 0x77, 0x5b, 0xfb, 0xbd, 0x2e, 0x02, 0xac, 0x19,
        0x6e, 0x9b, 0x85, 0xc4, 0xb7, 0xef, 0x69, 0xfd, 0x82, 0xaa, 0xab,
        0xe7, 0x24, 0xc2, 0x8a, 0x08, 0x65, 0x8a, 0xbd, 0x5b, 0x89, 0x8f,
        0x31, 0x38, 0xaa, 0x76, 0x45, 0x5d, 0x47, 0x00, 0x4c, 0xdc, 0xfa,
        0xd8, 0x81, 0xd0, 0x95, 0xf6, 0x1d, 0x31, 0x9d, 0xe7, 0x93, 0x52,
        0x51, 0x94, 0x2a, 0x21, 0x1d, 0x72, 0xcf, 0xeb, 0xc8, 0x27, 0xa2,
        0x56, 0xe9, 0x7c, 0xe0, 0xb6, 0x05, 0x2c, 0xee, 0x7d, 0xe7, 0x4d,
        0x40, 0x54, 0xfc, 0x33, 0x85, 0x8f, 0x8b, 0x6f, 0x7c, 0x99, 0x0a,
        0xe7, 0xba, 0xc2, 0x8b, 0x85, 0x3c, 0x46, 0x20, 0x1c, 0x7d, 0x7c,
        0x3d, 0xca, 0x35, 0xea, 0x7a, 0xb2, 0xd4, 0x33, 0x16, 0x85, 0x62,
        0xe6, 0x25, 0x97, 0x64, 0x79, 0xc7, 0xce, 0x79, 0xa9, 0x24, 0x93,
        0x4b, 0x78, 0x58, 0xdd, 0x4c, 0x3b, 0xd3, 0x80, 0x78, 0x29, 0xf3,
        0x4b, 0xef, 0xa0, 0x9a, 0xc1, 0xb8, 0xad, 0xae, 0x56, 0xcc, 0x0e,
        0x67, 0xbd, 0x87, 0x0d, 0x87, 0x3f, 0x38, 0x32, 0x98, 0x98, 0x68,
        0xb8, 0x84, 0xc2, 0x38, 0x28, 0x43, 0x1e, 0x4f, 0xde, 0xa7, 0xb1,
        0xb6, 0xde, 0xc9, 0x47, 0x31, 0x68, 0xfb, 0x91, 0x68, 0x9d, 0xb6,
        0x28, 0x30, 0xc4, 0xe6, 0xa7, 0x61, 0xe3, 0xf7, 0xd5, 0x44, 0x05,
        0x05, 0x02, 0x9e, 0x1a, 0x0c, 0xd0, 0xce, 0xfe, 0xb8, 0x9b, 0x49,
        0xaa, 0xcd, 0x73, 0xbe, 0xed, 0xd5, 0x0f, 0x0e, 0x69, 0x61, 0x32,
        0x13, 0xc5, 0x35, 0x7a, 0x26, 0x02, 0xcd, 0x10, 0x22, 0xe2, 0xdc,
        0xe4, 0xad, 0x83, 0x44, 0x69, 0x03, 0xdc, 0x47, 0x00, 0x4c, 0xdd,
        0x76, 0xf4, 0xd9, 0xbc, 0xc3, 0xaa, 0x81, 0xa4, 0x47, 0xd2, 0xaa,
        0x6a, 0x61, 0x00, 0xd9, 0xa6, 0xfc, 0xfd, 0x42, 0x81, 0x07, 0xe8,
        0x64, 0x70, 0xa5, 0x40, 0x68, 0x7c, 0xc8, 0x73, 0x73, 0x2c, 0x62,
        0xcb, 0x60, 0xcb, 0x07, 0x68, 0xcc, 0x9a, 0xe7, 0xc6, 0xc7, 0xcd,
        0x80, 0x77, 0x85, 0xb5, 0x8e, 0x8a, 0x6d, 0x97, 0x72, 0x34, 0x52,
        0xb5, 0x8c, 0x2f, 0x8f, 0xd6, 0x5d, 0x41, 0x8b, 0xf6, 0xbd, 0x99,
        0x72, 0xfc, 0xfd, 0x3d, 0xa8, 0x8a, 0xa8, 0xe0, 0x81, 0x32, 0xa2,
        0xba, 0xe7, 0x85, 0x1b, 0x50, 0xe2, 0xbd, 0x85, 0x0a, 0xb5, 0x62,
        0x09, 0x27, 0xe6, 0x28, 0x0e, 0x68, 0x44, 0x64, 0x7f, 0x10, 0x02,
        0x81, 0x5e, 0x04, 0xde, 0x65, 0x98, 0x04, 0x06, 0x0e, 0xaa, 0xe9,
        0xa7, 0x1d, 0x74, 0x67, 0x9c, 0x53, 0x61, 0xcb, 0x4c, 0xf7, 0x28,
        0x6a, 0x3a, 0x85, 0xd9, 0x4f, 0xea, 0x77, 0x4d, 0xe0, 0x49, 0x34,
        0x0c, 0xf5, 0xd7, 0xa2, 0x81, 0x53, 0xca, 0xb7, 0x7f, 0x36, 0x4e,
        0xa4, 0x1f, 0xea, 0x11, 0x26, 0x36, 0x9e, 0x89, 0x61, 0x6f, 0xa5,
        0x0a, 0xb4, 0xb3, 0x72, 0x9f, 0x2e, 0x91, 0xc2, 0x83, 0x49, 0xa1,
        0xf5, 0xe0, 0xae, 0xdb, 0x58, 0xfa, 0xc4, 0x1d, 0x75, 0x21, 0x04,
        0x76, 0x87, 0xdd, 0x9b, 0xf8, 0x64, 0xf8, 0xc6, 0x47, 0x00, 0x4c,
        0xde, 0x75, 0x28, 0x00, 0x09, 0xde, 0xb7, 0x88, 0x09, 0xb3, 0x5c,
        0xd1, 0xc3, 0x1e, 0x4f, 0x48, 0xd4, 0xc1, 0x5f, 0xae, 0xad, 0x36,
        0xd9, 0xf6, 0x35, 0xb4, 0x0b, 0xf5, 0xb3, 0x49, 0xe5, 0xda, 0xe6,
        0xe4, 0x52, 0xb7, 0x20, 0x3e, 0x95, 0x15, 0xe2, 0x9c, 0xb6, 0x0f,
        0x61, 0xca, 0x87, 0xf2, 0x7b, 0xe8, 0xca, 0x0c, 0x11, 0xc6, 0x36,
        0x6b, 0xe7, 0xd0, 0xd0, 0x11, 0xb8, 0x82, 0x07, 0x64, 0x79, 0xd6,
        0xd4, 0xb8, 0x9a, 0x5b, 0x97, 0x97, 0x7c, 0xb6, 0x32, 0xa2, 0x05,
        0xc0, 0x4c, 0x0c, 0x7a, 0xa2, 0x2c, 0x1c, 0xcf, 0xd4, 0x9d, 0x03,
        0x11, 0x8a, 0xd4, 0x47, 0x9d, 0x61, 0x47, 0x93, 0x88, 0x69, 0xbc,
        0xdb, 0x2c, 0x73, 0xf6, 0x2e, 0x8e, 0xf7, 0xc4, 0xe1, 0x96, 0x63,
        0x78, 0x12, 0x37, 0x66, 0x1b, 0x87, 0x72, 0x3b, 0xb1, 0x3d, 0xb8,
        0x1d, 0x51, 0x02, 0xc9, 0x79, 0xb9, 0x20, 0xfc, 0xa4, 0xe0, 0x0c,
        0x2c, 0x7c, 0xa1, 0xb7, 0x55, 0xf1, 0x8e, 0xb7, 0x17, 0x59, 0xbd,
        0x40, 0xa5, 0xe4, 0x8b, 0x2a, 0xbb, 0xf6, 0x0a, 0x6d, 0x15, 0x81,
        0x9a, 0x37, 0x7a, 0x52, 0xc2, 0x35, 0x22, 0x5a, 0x43, 0xa8, 0xe1,
        0xda, 0xfa, 0x4f, 0xae, 0xda, 0x10, 0x0e, 0x06, 0xbe, 0x03, 0x0d,
        0xb0, 0xf1, 0xf4, 0xd9, 0xe4, 0x79, 0x95, 0x65, 0x42, 0x47, 0x1f,
        0xff, 0x10};
    vqec_rtphdr_t rtp_hdr;

    CU_ASSERT(vqec_ifclient_rtp_hdr_parse((char *)buf_rtp_packet,
                                          sizeof(buf_rtp_packet),
                                          &rtp_hdr) == VQEC_OK);
    /*
     * Verify some fields of the RTP header:
     *
     * uint16_t  combined_bits;     RTP V,P,X,CC,M & PT
     * uint16_t  sequence;          RTP sequence
     * uint32_t  timestamp;         RTP timestamp
     * uint32_t  ssrc;              Syncronization source identifier
     * uint32_t  header_bytes;      Size of RTP header in bytes - decapsulated
     *                              packet buffer starts after this offset
     */
    CU_ASSERT(rtp_hdr.combined_bits == 0x8021);
    CU_ASSERT(rtp_hdr.sequence == 46774);
    CU_ASSERT(rtp_hdr.timestamp == 11537639);
    CU_ASSERT(rtp_hdr.ssrc == 3016504294UL);
    CU_ASSERT(rtp_hdr.header_bytes == 12);
}

void test_vqec_ifclient_histogram (void)
{
    /* Detect illegal requests */
    CU_ASSERT(vqec_ifclient_histogram_clear(VQEC_HIST_MAX) 
              == VQEC_ERR_INVALIDARGS);
    CU_ASSERT(vqec_ifclient_histogram_display(VQEC_HIST_MAX)
              == VQEC_ERR_INVALIDARGS);

    /* Accept valid requests */
    CU_ASSERT(vqec_ifclient_histogram_clear(VQEC_HIST_OUTPUTSCHED) == VQEC_OK);
    CU_ASSERT(vqec_ifclient_histogram_clear(VQEC_HIST_JOIN_DELAY) == VQEC_OK);
    CU_ASSERT(vqec_ifclient_histogram_display(VQEC_HIST_OUTPUTSCHED)
              == VQEC_OK);
    CU_ASSERT(vqec_ifclient_histogram_display(VQEC_HIST_JOIN_DELAY)
              == VQEC_OK);
}

void test_vqec_ifclient_get_stats_channel (void)
{
    vqec_error_t err;
    vqec_ifclient_stats_channel_t stats;
    vqec_sdp_handle_t sdp_handle;
    vqec_tunerid_t id;

    /* Verify no crashes occur if channel stats polled prior to VQE-C init */
    vqec_ifclient_deinit();
    CU_ASSERT(
        vqec_ifclient_get_stats_channel("udp://224.1.1.1:50000", &stats) ==
        VQEC_ERR_CHANNOTACTIVE);
    
    /* Init VQE-C, try some calls which should fail */
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(vqec_ifclient_get_stats_channel(NULL, NULL) != VQEC_OK);
    CU_ASSERT(vqec_ifclient_get_stats_channel(
                  "udp://224.1.1.1:50000", NULL) != VQEC_OK);
    CU_ASSERT(vqec_ifclient_get_stats_channel(NULL, &stats) != VQEC_OK);
    CU_ASSERT(vqec_ifclient_get_stats_channel("udp://224.bad.url", &stats)
              == VQEC_ERR_CHANNELPARSE);
    CU_ASSERT(vqec_ifclient_get_stats_channel("udp://224.1.1.1:50000", &stats)
              == VQEC_ERR_CHANNOTACTIVE);
    
    /*
     * Create a tuner and bind a channel to it.
     * Verify (w/o traffic received) that stats are 0
     */
    err = vqec_ifclient_tuner_create(&id, "0");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(id != VQEC_TUNERID_INVALID);
    sdp_handle = 
        vqec_ifclient_alloc_sdp_handle_from_url("udp://224.1.1.1:50000");
    err = vqec_ifclient_tuner_bind_chan(id, sdp_handle, NULL);
    CU_ASSERT(err == VQEC_OK);
    vqec_ifclient_free_sdp_handle(sdp_handle);
    CU_ASSERT(
        vqec_ifclient_get_stats_channel("udp://224.1.1.1:50000", &stats) 
        == VQEC_OK);
    CU_ASSERT(stats.primary_rtp_expected == 0);
    CU_ASSERT(stats.primary_rtp_lost == 0);

    /* Counters for TR-135 support */
    CU_ASSERT(stats.tr135_overruns == 0);
    CU_ASSERT(stats.tr135_underruns == 0);
    CU_ASSERT(stats.tr135_packets_expected == 0);
    CU_ASSERT(stats.tr135_packets_received == 0);
    CU_ASSERT(stats.tr135_packets_lost == 0);
    CU_ASSERT(stats.tr135_packets_lost_before_ec == 0);
    CU_ASSERT(stats.tr135_loss_events == 0);
    CU_ASSERT(stats.tr135_loss_events_before_ec == 0);
    CU_ASSERT(stats.tr135_severe_loss_index_count == 0);
    CU_ASSERT(stats.tr135_minimum_loss_distance == 0);
    CU_ASSERT(stats.tr135_maximum_loss_period == 0);
    CU_ASSERT(stats.tr135_gmin == 0);
    CU_ASSERT(stats.tr135_severe_loss_min_distance == 0);
}

/* Function for checking up the cname */
int compare_cname(char *new_cname, int len)
{
    vqec_syscfg_t cfg;
    vqec_syscfg_get(&cfg);
    printf("Cname to check = %s\n", new_cname);
    printf("cfg cname = %s\n", vqec_get_cname());
    if(strncmp(vqec_get_cname(), new_cname, len)==0) {
        return 0;
    } else {
        return 1;
    }
}


boolean check_membership (char *string, char candidate_char)
{
    int i;
    for (i=0;i<strlen(string);i++) {
       if (string[i] == candidate_char) return TRUE;
    }
    return FALSE;
}

char test_cname[VQEC_MAX_CNAME_LEN + 10];
void test_vqec_ifclient_register_cname (void) 
{
    vqec_error_t err; 
    int i;
    boolean retval;
    strcpy(test_cname, "test_cname09");
    /* If we want to register a CNAME we MUST do it before vqec_ifclient_init
     * This dependency is desired 
     */
    vqec_ifclient_deinit();
    retval = vqec_ifclient_register_cname(test_cname);
    CU_ASSERT(retval == TRUE);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(compare_cname(test_cname, (strlen(test_cname) +1 ))==0) 
    vqec_ifclient_deinit();

    /* Test the rejection of the cname registration because the desired
     * CNAME length is larger than VQEC_MAX_CNAME_LEN
     */
    for (i = 0; i < VQEC_MAX_CNAME_LEN+10; i++) {
        test_cname[ i ] = 'a';
    }
    vqec_ifclient_deinit();
    retval = vqec_ifclient_register_cname(test_cname);
    CU_ASSERT(retval == FALSE);

    /* Test the fringe case where the CNAME string is VQEC_MAX_CNAME_LEN-1
     * bytes and is NULL terminated. This string should be accepted
     */
    for (i = 0; i < VQEC_MAX_CNAME_LEN-1; i++) {
        test_cname[ i ] = 'a';
    }
    test_cname[VQEC_MAX_CNAME_LEN-1] = '\0';
    vqec_ifclient_deinit();
    retval = vqec_ifclient_register_cname(test_cname);
    CU_ASSERT(retval == TRUE);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(compare_cname(test_cname, VQEC_MAX_CNAME_LEN)==0) 
    vqec_ifclient_deinit();
    
    /* Test the behavior when the CNAME is "" */
    strcpy(test_cname, "");
    vqec_ifclient_deinit();
    retval=vqec_ifclient_register_cname(test_cname);
    CU_ASSERT(retval == FALSE);
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(compare_cname(test_cname, (strlen(test_cname) + 1 ))!=0) 
    vqec_ifclient_deinit();

    /* Test when the CNAME contains unacceptable characters */
    strcpy(test_cname, "test_cname%");
    vqec_ifclient_deinit();
    retval=vqec_ifclient_register_cname(test_cname);
    CU_ASSERT(retval == FALSE);
    /* Assert that incase of registration failure, the MAC address 
     * is without the ':' character
     */
    CU_ASSERT(check_membership(test_cname, ':') == FALSE);
}

/* test fast fill */
void test_vqec_ifclient_bind_params_fast_fill (void)
{

    vqec_bind_params_t *bp = NULL;

    bp = vqec_ifclient_bind_params_create();
    CU_ASSERT(bp != NULL);

    (void)vqec_ifclient_bind_params_enable_fastfill(bp);
    CU_ASSERT(vqec_ifclient_bind_params_is_fastfill_enabled(bp));
    (void)vqec_ifclient_bind_params_disable_fastfill(bp);
    CU_ASSERT(!vqec_ifclient_bind_params_is_fastfill_enabled(bp));
}

void test_vqec_ifclient_bind_tr135_params (void)
{
    vqec_bind_params_t *bp = NULL;
    vqec_ifclient_tr135_params_t tr135_params, *ret_tr135;
    
    tr135_params.gmin = 2;
    tr135_params.severe_loss_min_distance = 3;
    
    bp = vqec_ifclient_bind_params_create();
    CU_ASSERT(bp != NULL);

    (void)vqec_ifclient_bind_params_set_tr135_params(bp, &tr135_params);
    ret_tr135 = vqec_ifclient_bind_params_get_tr135_params(bp);
    CU_ASSERT(ret_tr135->gmin == tr135_params.gmin);
    CU_ASSERT(ret_tr135->severe_loss_min_distance == 
              tr135_params.severe_loss_min_distance);
}

void test_vqec_ifclient_set_tr135_params_channel (void) 
{
    vqec_ifclient_tr135_params_t tr135_params;
    boolean retval;
    vqec_error_t err;
    vqec_sdp_handle_t sdp_handle;
    vqec_tunerid_t id;
    vqec_ifclient_stats_channel_t channel_stats;
    vqec_ifclient_stats_channel_tr135_sample_t sample_stats;
    
    tr135_params.gmin = 2;
    tr135_params.severe_loss_min_distance = 3;

    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);
    
    /*
     * Create a tuner and bind a channel to it.
     * Verify (w/o traffic received) that stats are 0
     */
    err = vqec_ifclient_tuner_create(&id, "newtuner");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(id != VQEC_TUNERID_INVALID);
    sdp_handle = 
        vqec_ifclient_alloc_sdp_handle_from_url("udp://224.1.1.1:50000");
    err = vqec_ifclient_tuner_bind_chan(id, sdp_handle, NULL);
    CU_ASSERT(err == VQEC_OK);

    retval = vqec_ifclient_set_tr135_params_channel("udp://224.1.1.1:50000", 
                                                     &tr135_params);
    CU_ASSERT(retval == VQEC_OK);
    retval = vqec_ifclient_get_stats_channel("udp://224.1.1.1:50000", 
                                              &channel_stats);
    CU_ASSERT(retval == VQEC_OK);
    CU_ASSERT(channel_stats.tr135_gmin == 
              tr135_params.gmin);
    CU_ASSERT(channel_stats.tr135_severe_loss_min_distance == 
              tr135_params.severe_loss_min_distance);

    /* Unit tests for TR-135 sample statistics */
    retval = vqec_ifclient_get_stats_channel_tr135_sample(
                                        "udp://224.1.1.1:50000", 
                                        &sample_stats);
    CU_ASSERT(retval == VQEC_OK);

    vqec_ifclient_deinit();
}

/* test parsing individual sdp */
void test_vqec_ifclient_parse_sdp (void)
{
    vqec_error_t err;

    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);

    vqec_chan_cfg_t cfg;
    boolean rv;
   
    /* bad arguments */ 
    rv = vqec_ifclient_chan_cfg_parse_sdp(&cfg,
                                          NULL,
                                          VQEC_CHAN_TYPE_LINEAR);
    CU_ASSERT_EQUAL(rv, FALSE);
    rv = vqec_ifclient_chan_cfg_parse_sdp(NULL,
                                          LINEAR_SDP,
                                          VQEC_CHAN_TYPE_LINEAR);
    CU_ASSERT_EQUAL(rv, FALSE);
    rv = vqec_ifclient_chan_cfg_parse_sdp(&cfg, 
                                          VOD_SDP, 
                                          VQEC_CHAN_TYPE_LINEAR);
    CU_ASSERT_EQUAL(rv, FALSE);

    /* linear sdp parsing and extraction */
    rv = vqec_ifclient_chan_cfg_parse_sdp(&cfg,
                                          LINEAR_SDP,
                                          VQEC_CHAN_TYPE_LINEAR);
    CU_ASSERT_EQUAL(rv, TRUE);
    CU_ASSERT(strcmp("HD Discovery", cfg.name) == 0);
    CU_ASSERT(inet_addr("229.1.1.8") == cfg.primary_dest_addr.s_addr);
    CU_ASSERT(htons(53198) == cfg.primary_dest_port);
    CU_ASSERT(htons(53199) == cfg.primary_dest_rtcp_port);
    CU_ASSERT(inet_addr("9.3.13.2") == cfg.primary_source_addr.s_addr);
    CU_ASSERT(96 == cfg.primary_payload_type);
    CU_ASSERT(14910000 == cfg.primary_bit_rate);
    CU_ASSERT(53 == cfg.primary_rtcp_sndr_bw);
    CU_ASSERT(530000 == cfg.primary_rtcp_rcvr_bw);
    CU_ASSERT(53 == cfg.primary_rtcp_per_rcvr_bw);
    CU_ASSERT(inet_addr("5.3.19.100") == cfg.fbt_addr.s_addr);
    CU_ASSERT(0 < cfg.er_enable);
    CU_ASSERT(0 < cfg.rcc_enable);
    CU_ASSERT(inet_addr("5.3.19.100") == cfg.rtx_source_addr.s_addr);
    CU_ASSERT(htons(50000) == cfg.rtx_source_port);
    CU_ASSERT(htons(50001) == cfg.rtx_source_rtcp_port);
    CU_ASSERT(99 == cfg.rtx_payload_type);
    CU_ASSERT(53 == cfg.rtx_rtcp_sndr_bw);
    CU_ASSERT(53 == cfg.rtx_rtcp_rcvr_bw);
    CU_ASSERT(0 == cfg.fec_enable);

    /* vod sdp parsing and extraction */
    rv = vqec_ifclient_chan_cfg_parse_sdp(&cfg,
                                          VOD_SDP,
                                          VQEC_CHAN_TYPE_VOD);
    CU_ASSERT_EQUAL(rv, TRUE);
    CU_ASSERT(strcmp("VoD 1", cfg.name) == 0);
    CU_ASSERT(96 == cfg.primary_payload_type);
    CU_ASSERT(14910000 == cfg.primary_bit_rate);
    CU_ASSERT(55 == cfg.primary_rtcp_sndr_bw);
    CU_ASSERT(55 == cfg.primary_rtcp_rcvr_bw);
    CU_ASSERT(0 < cfg.er_enable);
    CU_ASSERT(99 == cfg.rtx_payload_type);
    CU_ASSERT(55 == cfg.rtx_rtcp_sndr_bw);
    CU_ASSERT(55 == cfg.rtx_rtcp_rcvr_bw);

    vqec_ifclient_deinit();  
}

CU_TestInfo test_array_ifclient[] = {
    {"test_vqec_ifclient_init_test",                
     test_vqec_ifclient_init_test},
    {"test_vqec_ifclient_sdp_set",                  
     test_vqec_ifclient_sdp_set},
    {"test_vqec_ifclient_dyn_chan_tests",              
     test_vqec_ifclient_dyn_chan_tests},
    {"test_vqec_ifclient_tuner_tests",              
     test_vqec_ifclient_tuner_tests},
    {"test_vqec_ifclient_rtp_header_strip",         
     test_vqec_ifclient_rtp_header_strip},
    {"test_vqec_ifclient_histogram",
     test_vqec_ifclient_histogram},
    {"test_vqec_ifclient_get_stats_channel",
     test_vqec_ifclient_get_stats_channel},
    {"test_vqec_ifclient_register_cname",
     test_vqec_ifclient_register_cname},
    {"test_vqec_ifclient_bind_params_fast_fill",
     test_vqec_ifclient_bind_params_fast_fill},
    {"test_vqec_ifclient_bind_tr135_params",
     test_vqec_ifclient_bind_tr135_params},
    {"test_vqec_ifclient_set_tr135_params_channel", 
     test_vqec_ifclient_set_tr135_params_channel},
    {"test_vqec_ifclient_parse_sdp",
     test_vqec_ifclient_parse_sdp},
    CU_TEST_INFO_NULL,
};

