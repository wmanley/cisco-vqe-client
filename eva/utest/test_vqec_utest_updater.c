/*
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif // _VQEC_UTEST_INTERPOSERS

#include "vqec_ifclient.h"
#include "vqec_ifclient_defs.h"
#include "vqec_error.h"
#include "vqec_syscfg.h"
#include <log/vqe_cfg_syslog_def.h>
#include "utils/vmd5.h"
#include "vqec_updater.h"
#include "vqec_updater_private.h"
#include "vqec_debug.h"

/*
 * Unit tests for VQE-C updater
 */

static vqec_syscfg_t s_cfg;
#define MAX_STRLEN 512
static char cmd[MAX_STRLEN];    

/* md5 hash of an empty file */
#define VQEC_VERSION_EMPTY_FILE "d41d8cd98f00b204e9800998ecf8427e"

void test_vqec_updater_reset_files (void)
{
    /* Restore empty resource files */
    snprintf(cmd, MAX_STRLEN, "rm -f %s", s_cfg.network_cfg_pathname);
    system(cmd);
    snprintf(cmd, MAX_STRLEN, "rm -f %s", s_cfg.channel_lineup);
    system(cmd);
    snprintf(cmd, MAX_STRLEN, "touch %s", s_cfg.network_cfg_pathname);
    system(cmd);
    snprintf(cmd, MAX_STRLEN, "touch %s", s_cfg.channel_lineup);
    system(cmd); 
}

/*
 * Sets the VQE-C updater to have existent but empty
 * attribute and channel config files.
 *
 * Also, resets the RTSP client interposers.
 */
void test_vqec_updater_reset_all (void)
{
    /* Restore empty local attribute and channel files */
    test_vqec_updater_reset_files();

    /*
     * Make updater records of VQE-C resources match content,
     * make updater records of VCDS content to match VQE-C.
     */
    (void)vqe_MD5ComputeChecksumStr(vqec_updater.attrcfg.datafile, TRUE,
                                vqec_updater.attrcfg.vqec_version);
    (void)vqe_MD5ComputeChecksumStr(vqec_updater.chancfg.datafile, TRUE,
                                vqec_updater.chancfg.vqec_version);
    (void)vqe_MD5ComputeChecksumStr(vqec_updater.attrcfg.datafile, TRUE,
                                vqec_updater.attrcfg.vcds_version);
    (void)vqe_MD5ComputeChecksumStr(vqec_updater.chancfg.datafile, TRUE,
                                vqec_updater.chancfg.vcds_version);

    /* Initialize RTSP client interposers to intercept */
    test_rtsp_client_set_intercept(TRUE);
    test_rtsp_client_init_make_fail(RTSP_SUCCESS);
    test_rtsp_send_request_make_fail(RTSP_SUCCESS);
    test_rtsp_get_response_code_make_fail(MSG_200_OK);
    test_rtsp_get_response_body_make_fail("");
    test_rtsp_client_close_make_fail(RTSP_SUCCESS);
}

int test_vqec_updater_init (void)
{
    int loglevel;

    sleep(2);
    /* Load the system configuration from supplied file */
    (void)vqec_syscfg_init(
        "data/cfg_test_all_params_valid_with_local_vcds.cfg");
    vqec_syscfg_get(&s_cfg);
    test_vqec_updater_reset_files();

    /* Initialize the logging facility */
    loglevel = s_cfg.log_level;
    syslog_facility_open(LOG_VQEC, LOG_CONS);
    syslog_facility_filter_set(LOG_VQEC, loglevel);
    sync_lib_facility_filter_with_process(LOG_VQEC, LOG_LIBCFG);
    sync_lib_facility_filter_with_process(LOG_VQEC, LOG_RTP);
    return 0;
}

int test_vqec_updater_clean (void)
{
    snprintf(cmd, MAX_STRLEN, "rm -f %s", s_cfg.network_cfg_pathname);
    system(cmd);
    snprintf(cmd, MAX_STRLEN, "rm -f %s", s_cfg.channel_lineup);
    system(cmd);
    return 0;
}

static void test_vqec_updater_tests (void) 
{
    vqec_updater_status_t status;
    boolean in_progress;
    int32_t seconds_until_next_update;
    struct timeval attrcfg_last_commit_time;
    char *attrcfg_version;
    struct timeval chancfg_last_commit_time;
    char *chancfg_version;
    vqec_error_t err;

    /* Test updater initialization */
    vqec_updater_deinit();
    err = vqec_updater_init(&s_cfg);
    CU_ASSERT(err == VQEC_OK);

    /* Get status prior to any updates:  verify no updates performed */
    vqec_updater_get_status(&status,
                            &in_progress,
                            &seconds_until_next_update,
                            &attrcfg_last_commit_time,
                            &attrcfg_version,
                            &chancfg_last_commit_time,
                            &chancfg_version);
    CU_ASSERT(status == VQEC_UPDATER_STATUS_INITIALIZED);
    CU_ASSERT(in_progress == FALSE);
    CU_ASSERT(seconds_until_next_update == -1);
    CU_ASSERT((attrcfg_last_commit_time.tv_sec == 0) &&
              (attrcfg_last_commit_time.tv_usec == 0));
    CU_ASSERT(!strcmp(attrcfg_version, VQEC_VERSION_EMPTY_FILE));
    CU_ASSERT((chancfg_last_commit_time.tv_sec == 0) &&
              (chancfg_last_commit_time.tv_usec == 0));
    CU_ASSERT(!strcmp(chancfg_version, VQEC_VERSION_EMPTY_FILE));
    
    /*
     * Some tests for parsing index files
     */
    /*
     * Test 1:  NULL body
     *          Make sure both files are deleted
     */
    test_vqec_updater_reset_all();
    test_rtsp_get_response_body_make_fail(NULL);
    vqec_updater_request_update(
        TRUE, FALSE, FALSE,
        TIME_ADD_A_R(get_sys_time(), VQEC_UPDATER_TIMEOUT_BACKGROUND));
    CU_ASSERT(vqec_updater.last_index_request.result
              == VQEC_UPDATER_RESULT_ERR_OK);
    CU_ASSERT(!strcmp(vqec_updater.attrcfg.vcds_version,
                      VQEC_VERSION_FILE_NOT_AVAILABLE));
    CU_ASSERT(!strcmp(vqec_updater.chancfg.vcds_version,
                      VQEC_VERSION_FILE_NOT_AVAILABLE));
    
    /*
     * Test 2:  body with only attribute and unrecognized resource
     *          Make sure new updated versions are recorded for VCDS
     */
    test_vqec_updater_reset_all();
    test_rtsp_get_response_body_make_fail(
        "vqec-network-cfg    f02d77ad5d121f00b124d4aa5651e2f8\r\n"
        "unknown-resource    <unknown version>");
    vqec_updater_request_update(
        TRUE, FALSE, FALSE,
        TIME_ADD_A_R(get_sys_time(), VQEC_UPDATER_TIMEOUT_BACKGROUND));
    CU_ASSERT(vqec_updater.last_index_request.result
              == VQEC_UPDATER_RESULT_ERR_OK);
    CU_ASSERT(!strcmp(vqec_updater.attrcfg.vcds_version,
                      "f02d77ad5d121f00b124d4aa5651e2f8"));
    CU_ASSERT(!strcmp(vqec_updater.chancfg.vcds_version,
                      VQEC_VERSION_FILE_NOT_AVAILABLE));

    /*
     * Test 3:  body with only attribute, unknown, and channel resources
     *          Make sure new updated versions are recorded for VCDS
     */
    test_vqec_updater_reset_all();
    test_rtsp_get_response_body_make_fail(
        "vqe-channels e89ab785efbe3a674655b21e998ab5bd\r\n"
        "unknown-resource    <unknown version>\r\n"
        "vqec-network-cfg    f02d77ad5d121f00b124d4aa5651e2f8\r\n");
    vqec_updater_request_update(
        TRUE, FALSE, FALSE,
        TIME_ADD_A_R(get_sys_time(), VQEC_UPDATER_TIMEOUT_BACKGROUND));
    CU_ASSERT(vqec_updater.last_index_request.result
              == VQEC_UPDATER_RESULT_ERR_OK);
    CU_ASSERT(!strcmp(vqec_updater.attrcfg.vcds_version,
                      "f02d77ad5d121f00b124d4aa5651e2f8"));
    CU_ASSERT(!strcmp(vqec_updater.chancfg.vcds_version,
                      "e89ab785efbe3a674655b21e998ab5bd"));

    /*
     * Test 4:  VCDS version string with attribute and channel config
     *          Make sure new updated versions are recorded for VCDS
     */
    test_vqec_updater_reset_all();
    test_rtsp_get_response_body_make_fail(
        "vcds 3.2.0(61)-development\r\n"
        "vqe-channels e89ab785efbe3a674655b21e998ab5bd\r\n"
        "unknown-resource    <unknown version>\r\n"
        "vqec-network-cfg    f02d77ad5d121f00b124d4aa5651e2f8\r\n");
    vqec_updater_request_update(
        TRUE, FALSE, FALSE,
        TIME_ADD_A_R(get_sys_time(), VQEC_UPDATER_TIMEOUT_BACKGROUND));
    CU_ASSERT(vqec_updater.last_index_request.result
              == VQEC_UPDATER_RESULT_ERR_OK);
    CU_ASSERT(!strcmp(vqec_updater.attrcfg.vcds_version,
                      "f02d77ad5d121f00b124d4aa5651e2f8"));
    CU_ASSERT(!strcmp(vqec_updater.chancfg.vcds_version,
                      "e89ab785efbe3a674655b21e998ab5bd"));
}

CU_TestInfo test_array_updater[] = {
    {"test_vqec_updater_tests",test_vqec_updater_tests},
    CU_TEST_INFO_NULL,
};

