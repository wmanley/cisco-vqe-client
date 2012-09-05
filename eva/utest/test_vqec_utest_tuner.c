/*
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vqec_ifclient_defs.h"
#include "vqec_tuner.h"
#include <assert.h>
#include <arpa/inet.h>
#include "vqec_recv_socket.h"
#include "../rtp/rtp_session.h"
#include "vqec_debug.h"
#include "../cfg/cfgapi.h"
#include "vqec_error.h"
#include "vqec_ifclient.h"


/*
 * Unit tests for vqec_tuner
 */

int test_vqec_tuner_init (void) {

    test_malloc_make_fail(0);

    /* Start vqec tuner manager to own the test_tuner */
    vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");

    return 0;
}

int test_vqec_tuner_clean (void) {

    vqec_ifclient_deinit();
    return 0;
}

static void test_vqec_tuner_tests (void) {
    vqec_tunerid_t tuner_id, tuner_id1, tuner_id2, tuner_id3;
    vqec_chan_cfg_t cfg1, cfg2, cfg3;
    vqec_chanid_t chan_id, chan_id1, chan_id2, chan_id3;
    const vqec_bind_params_t *bp=NULL;
    vqec_error_t err;
    vqec_chan_err_t err_chan;
    const char *name;
    int i;
#define PAT_BUF_LEN 64
    uint8_t pat_buf[PAT_BUF_LEN];
    uint32_t pat_len;
    uint16_t pid;
#define PMT_BUF_LEN 64
    uint8_t pmt_buf[PMT_BUF_LEN];
    uint32_t pmt_len;

    /* Create a tuner, but have malloc fail */
    test_malloc_make_fail(1);
    err = vqec_tuner_create(&tuner_id, NULL);
    test_malloc_make_fail(0);
    CU_ASSERT(tuner_id == VQEC_TUNERID_INVALID);

    /*
     * Attempt to create tuners with illegal names, and verify rejection:
     *   1) for a name having a non-alphanumeric char, and
     *   2) for a name having empty string ("")
     */
    err = vqec_tuner_create(&tuner_id1, "tuner?");
    CU_ASSERT(tuner_id1 == VQEC_TUNERID_INVALID);
    CU_ASSERT(err == VQEC_ERR_INVALIDARGS);
    err = vqec_tuner_create(&tuner_id1, "");
    CU_ASSERT(tuner_id1 == VQEC_TUNERID_INVALID);
    CU_ASSERT(err == VQEC_ERR_INVALIDARGS);
    
    /*
     * Create a tuner w/o supplying a name, and verify success.
     * Then try to create a tuner with the name chosen for the first tuner,
     * and verify failure.
     */
    err = vqec_tuner_create(&tuner_id1, NULL);
    CU_ASSERT(err == VQEC_OK);
    name = vqec_tuner_get_name(tuner_id1);    
    err = vqec_tuner_create(&tuner_id2, name);
    CU_ASSERT(err == VQEC_ERR_EXISTTUNER);

    /* Verify the created tuner has an associated dataplane tuner ID */
    CU_ASSERT(vqec_tuner_get_dptuner(tuner_id1) != 
              VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID);

    /*
     * Create a tuner whose name will be truncated
     * because its name length is > VQEC_MAX_TUNER_NAMESTR_LEN (16).
     * Verify the truncation, and that it is retrievable by its truncated name.
     */
    char *name_long = "0123456789abcdefgh";
    char *name_truncated = "0123456789abcdef";
    err = vqec_tuner_create(&tuner_id2, name_long);
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(tuner_id2 != VQEC_TUNERID_INVALID);
    if (vqec_tuner_get_name(tuner_id2)) {
        CU_ASSERT(!strcmp(vqec_tuner_get_name(tuner_id2), "0123456789abcdef"));
    } else {
        CU_ASSERT(FALSE);
    }
    CU_ASSERT(vqec_tuner_get_id_by_name(name_long) == tuner_id2);
    CU_ASSERT(vqec_tuner_get_id_by_name(name_truncated) == tuner_id2);

    /*
     * Create a third tuner, verify success.
     */
    err = vqec_tuner_create(&tuner_id3, "foo");
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(tuner_id3 != VQEC_TUNERID_INVALID);
    
    /* Allocate channels 1-3 */
    vqec_chan_convert_sdp_to_cfg(cfg_get_channel_cfg_from_idx(0),
                                 VQEC_CHAN_TYPE_LINEAR,
                                 &cfg1);
    err_chan = vqec_chan_get_chanid(&chan_id1, &cfg1);
    CU_ASSERT(err_chan == VQEC_CHAN_ERR_OK);

    vqec_chan_convert_sdp_to_cfg(cfg_get_channel_cfg_from_idx(1),
                                 VQEC_CHAN_TYPE_LINEAR,
                                 &cfg2);
    err_chan = vqec_chan_get_chanid(&chan_id2, &cfg2);
    CU_ASSERT(err_chan == VQEC_CHAN_ERR_OK);

    vqec_chan_convert_sdp_to_cfg(cfg_get_channel_cfg_from_idx(2),
                                 VQEC_CHAN_TYPE_LINEAR,
                                 &cfg3);
    err_chan = vqec_chan_get_chanid(&chan_id3, &cfg3);
    CU_ASSERT(err_chan == VQEC_CHAN_ERR_OK);

    /* Ensure some invalid requests are rejected */
    err = vqec_tuner_bind_chan(VQEC_TUNERID_INVALID, chan_id1, bp, NULL);
    CU_ASSERT(err == VQEC_ERR_NOSUCHTUNER);
    err = vqec_tuner_bind_chan(tuner_id1, VQEC_CHANID_INVALID, bp, NULL);
    CU_ASSERT(err != VQEC_OK);
    err = vqec_tuner_get_chan(VQEC_TUNERID_INVALID, &chan_id);
    CU_ASSERT(err != VQEC_OK);

    /* Unbind an unbound tuner, verify it's a NO-OP */
    err = vqec_tuner_get_chan(tuner_id1, &chan_id);
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(chan_id == VQEC_CHANID_INVALID);
    err = vqec_tuner_unbind_chan(tuner_id1);
    CU_ASSERT(err == VQEC_OK);
    err = vqec_tuner_get_chan(tuner_id1, &chan_id);
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(chan_id == VQEC_CHANID_INVALID);

    /* Bind tuner 1&2 to channels 1&2, verify success */
    err = vqec_tuner_bind_chan(tuner_id1, chan_id1, bp, NULL);
    CU_ASSERT(err == VQEC_OK);
    err = vqec_tuner_bind_chan(tuner_id2, chan_id2, bp, NULL);
    CU_ASSERT(err == VQEC_OK);
    err = vqec_tuner_get_chan(tuner_id1, &chan_id);
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(chan_id == chan_id1);
    err = vqec_tuner_get_chan(tuner_id2, &chan_id);
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(chan_id == chan_id2);
    
    /* Bind a tuner to its current channel, verify it's a NO-OP */
    err = vqec_tuner_bind_chan(tuner_id2, chan_id2, bp, NULL);
    CU_ASSERT(err == VQEC_OK);
    err = vqec_tuner_get_chan(tuner_id2, &chan_id);
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(chan_id == chan_id2);
    
    /* Iterate through the tuners, verify there are 3 */
    i = 0;
    tuner_id = vqec_tuner_get_first_tuner_id();
    while (tuner_id != VQEC_TUNERID_INVALID) {
        i++;
        tuner_id = vqec_tuner_get_next_tuner_id(tuner_id);
    }
    CU_ASSERT(i == 3);

    /* Unbind tuner 2, then bind it again */
    err = vqec_tuner_unbind_chan(tuner_id2);
    CU_ASSERT(err == VQEC_OK);
    err_chan = vqec_chan_get_chanid(&chan_id2, &cfg2);
    CU_ASSERT(err_chan == VQEC_CHAN_ERR_OK);
    err = vqec_tuner_bind_chan(tuner_id2, chan_id2, bp, NULL);
    CU_ASSERT(err == VQEC_OK);

    /*
     * Dump stats on tuner1
     */
    err = vqec_tuner_dump(tuner_id1, DETAIL_MASK);
    CU_ASSERT(err == VQEC_OK);

    /*
     * Get PAT & PMT info
     */
    err = vqec_tuner_get_pat(tuner_id1, pat_buf, &pat_len, PAT_BUF_LEN);
    CU_ASSERT(err == VQEC_OK);    
    err = vqec_tuner_get_pmt(tuner_id1, &pid, pmt_buf, &pmt_len, PMT_BUF_LEN);
    CU_ASSERT(err == VQEC_OK);

    /* Destroy (currently tuned) tuner2, verify there are now 2 tuners */
    err = vqec_tuner_destroy(tuner_id2);
    CU_ASSERT(err == VQEC_OK);
    i = 0;
    tuner_id = vqec_tuner_get_first_tuner_id();
    while (tuner_id != VQEC_TUNERID_INVALID) {
        i++;
        tuner_id = vqec_tuner_get_next_tuner_id(tuner_id);
    }
    CU_ASSERT(i == 2);
    
    /* Destroy (currently untuned) tuner3, ensure it's gone */
    err = vqec_tuner_destroy(tuner_id3);
    CU_ASSERT(err == VQEC_OK);
    CU_ASSERT(vqec_tuner_get_name(tuner_id3) == NULL);
}


CU_TestInfo test_array_tuner[] = {
    {"test vqec_tuner_tuner_tests",test_vqec_tuner_tests},
    CU_TEST_INFO_NULL,
};

