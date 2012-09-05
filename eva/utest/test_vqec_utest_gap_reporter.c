/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 *
 ******************************************************************************
 *
 * File: test_vqec_utest_gap_reporter.c
 *
 * Description: Unit tests for the vqec_gap_reporter.c module.
 *
 * Documents:
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "vqec_ifclient.h"
#include "vqec_tuner.h"
#include "vqec_gap_reporter.h"
#include "vqec_channel_private.h"
#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

extern uint32_t 
vqec_gap_reporter_construct_generic_nack(vqec_chan_t *chan,
                                         rtcp_rtpfb_generic_nack_t *nd,
                                         int max_fci_count,
                                         uint32_t *num_repairs);

int test_vqec_gap_reporter_init (void)
{
    return 0;
}
int test_vqec_gap_reporter_clean (void)
{
    return 0;
}

/*
 * A burst parameter value of 0 implies police all repair requests
 */
static void test_vqec_gap_reporter_construct_generic_nack_internal (
    vqec_chan_t *chan,
    rtcp_rtpfb_generic_nack_t *nd,
    int *fci_count,
    uint32_t *num_repairs,
    boolean  policer_enabled,
    uint32_t burst) 
{
    vqec_dp_gap_buffer_t gapbuf;

    /* Set up the interposer to return a gaplist */
    gapbuf.num_gaps = 5;
    gapbuf.gap_list[0].start_seq = 4294967288UL;
    gapbuf.gap_list[0].extent = 0;
    gapbuf.gap_list[1].start_seq = 0;
    gapbuf.gap_list[1].extent = 3;
    gapbuf.gap_list[2].start_seq = 5;
    gapbuf.gap_list[2].extent = 0;
    gapbuf.gap_list[3].start_seq = 20;
    gapbuf.gap_list[3].extent = 0;
    gapbuf.gap_list[4].start_seq = 36;
    gapbuf.gap_list[4].extent = 0;
    test_vqec_dp_chan_get_gap_report_use_interposer(
        TRUE, VQEC_DP_ERR_OK, &gapbuf);
    /* Override channel's "er_policer_enabled" setting for this test */
    chan->er_policer_enabled = policer_enabled;
    if (policer_enabled) {
        tb_init(&chan->er_policer_tb,
                1, burst ? burst : TB_BURST_MAX, TB_QUANTUM_MAX, ABS_TIME_0);
        if (!burst) {
            /*
             * Empty the bucket.  Since it'll take TB_QUANTUM_MAX seconds
             * before the bucket is credited again, the code under test should
             * experience an empty token bucket.
             */
            tb_drain_tokens(&chan->er_policer_tb, TB_BURST_MAX);
        }
    }

    /* Perform the test.  The results are checked in the caller */
    *fci_count =
        vqec_gap_reporter_construct_generic_nack(chan, nd,
                                                 VQEC_GAP_REPORTER_FCI_MAX,
                                                 num_repairs);
    /*
     * so if all repair requests were included, we should have...
     *
     * -8         -7  -6  -5  -4  -3  -2  -1  0 1 2 3 4 5 6 7 8 ...
     * 4294967288 289 290 291 292 293 294 295 0 1 2 3 4 5 6 7 8 ...
     * xx         0   0   0   0   0   0   0   1 1 1 1 0 1 0 0 0
     *  ==> 65528, 0001 0111 1000 0000 (=0x1780)
     *
     * 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 ...
     * xx 0  0  0  0  0  0  0  0  0  0  0  0  0  0  0  1
     *  ==> 20, 1000 0000 0000 0000 (=0x8000)
     */

    /* Turn off the interposer */
    test_vqec_dp_chan_get_gap_report_use_interposer(
        FALSE, VQEC_CHANID_INVALID, NULL);

}

static void test_vqec_gap_reporter_construct_generic_nack (void)
{
    rtcp_rtpfb_generic_nack_t nd[VQEC_GAP_REPORTER_FCI_MAX];
    int fci_count = 0;
    uint32_t num_repairs = 0;
    uint64_t initial_repairs_policed;
    uint64_t initial_repairs_policed_chan;
    vqec_error_t err;
    vqec_tunerid_t tuner_id1;
    channel_cfg_t *cfg1;
    vqec_chan_cfg_t chan_cfg1;
    vqec_chanid_t chan_id1;
    vqec_chan_t *chan1;
    vqec_chan_err_t err_chan;

    /*
     * Prepare for building channel gap reports by:
     *   1. initializing VQE-C
     *   2. creating a tuner
     *   3. creating a channel
     *   4. binding the channel to the tuner
     */
    err = vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT(err == VQEC_OK);    
    err = vqec_tuner_create(&tuner_id1, NULL);
    CU_ASSERT(err == VQEC_OK);
    cfg1 = cfg_get_channel_cfg_from_idx(0);
    vqec_chan_convert_sdp_to_cfg(cfg1, VQEC_CHAN_TYPE_LINEAR, &chan_cfg1);
    err_chan = vqec_chan_get_chanid(&chan_id1, &chan_cfg1);
    CU_ASSERT(err_chan == VQEC_CHAN_ERR_OK);
    err = vqec_tuner_bind_chan(tuner_id1, chan_id1, NULL, NULL);
    CU_ASSERT(err == VQEC_OK);
    chan1 = vqec_chanid_to_chan(chan_id1);

    /*
     * Try to build an FCI payload three times:
     * 
     * Once without error repair policing...
     */
    initial_repairs_policed = g_vqec_error_repair_policed_requests;
    initial_repairs_policed_chan = chan1->stats.total_repairs_policed;

    test_vqec_gap_reporter_construct_generic_nack_internal(
        chan1, nd, &fci_count, &num_repairs, FALSE, 0);

    CU_ASSERT(fci_count == 2);
    CU_ASSERT(num_repairs == 8);
    CU_ASSERT(g_vqec_error_repair_policed_requests == initial_repairs_policed);
    CU_ASSERT(chan1->stats.total_repairs_policed ==
              initial_repairs_policed_chan);

    CU_ASSERT(nd[0].pid == 65528);
    CU_ASSERT(nd[0].bitmask == 0x1780);

    CU_ASSERT(nd[1].pid == 20);
    CU_ASSERT(nd[1].bitmask == 0x8000);

    /*
     * Once with error repair policing (up to 4 repairs allowed)...
     */
    initial_repairs_policed = g_vqec_error_repair_policed_requests;
    initial_repairs_policed_chan = chan1->stats.total_repairs_policed;

    test_vqec_gap_reporter_construct_generic_nack_internal(
        chan1, nd, &fci_count, &num_repairs, TRUE, 4);

    CU_ASSERT(fci_count == 1);
    CU_ASSERT(num_repairs == 4);
    CU_ASSERT(g_vqec_error_repair_policed_requests == 
              initial_repairs_policed + 4);
    CU_ASSERT(chan1->stats.total_repairs_policed ==
              initial_repairs_policed_chan + 4);

    CU_ASSERT(nd[0].pid == 65528);
    CU_ASSERT(nd[0].bitmask == 0x0380);

    /*
     * Once with error repair policing (no repairs allowed)...
     */
    initial_repairs_policed = g_vqec_error_repair_policed_requests;
    initial_repairs_policed_chan = chan1->stats.total_repairs_policed;

    test_vqec_gap_reporter_construct_generic_nack_internal(
        chan1, nd, &fci_count, &num_repairs, TRUE, 0);

    CU_ASSERT(!fci_count);
    CU_ASSERT(!num_repairs);
    CU_ASSERT(g_vqec_error_repair_policed_requests == 
              initial_repairs_policed + 8);
    CU_ASSERT(chan1->stats.total_repairs_policed ==
              initial_repairs_policed_chan + 8);

    vqec_ifclient_deinit();
}


CU_TestInfo test_array_gap_reporter[] = {
    {"test_vqec_gap_reporter_construct_generic_nack",
     test_vqec_gap_reporter_construct_generic_nack},
    CU_TEST_INFO_NULL,
};

