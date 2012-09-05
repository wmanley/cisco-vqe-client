/* 
 * Unit test program that based on CUnit, an OpenSource GPL
 * library. This is the main test file, which registers and runs all the
 * CFG unit tests.
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
#include "cfg_utests.h"

/*Include CFG Library syslog header files */
#include <log/vqe_cfg_syslog_def.h>
#include <log/vqe_cfg_debug.h>

/***********************************************************************
 *
 *     CUnit FRAMEWORK STARTS HERE
 *
 **********************************************************************/

CU_TestInfo test_cfg_channel[] = {
    { "channel configuration copy", test_channel_copy },
    { "channel configuration compare", test_channel_compare },
    { "channel configuration get", test_channel_get },
    { "channel configuration parse", test_parse_channels },
    { "channel configuration single channel", test_parse_single_channel },
    { "channel configuration validate version", test_channel_valid1 },
    { "channel configuration validate IP version", test_channel_valid2 },
    { "channel configuration validate non-zero time values",
      test_channel_valid3 },
    { "channel configuration validate rtcp-unicast attribute",
      test_channel_valid4 },
    { "channel configuration validate group attribute", test_channel_valid5 },
    { "channel configuration validate random order of media sessions",
      test_channel_valid6 },
    { "channel configuration validate missing media sessions", 
      test_channel_valid7 },
    { "channel configuration validate encoding name and clock rate", 
      test_channel_valid8 },
    { "channel configuration validate rtcp bandwidth", test_channel_valid9 },
    { "channel configuration validate src ip address in re-sourced session",
      test_channel_valid10 },
    { "channel configuration validate port values", test_channel_valid11 },
    { "channel configuration validate ip addresses", test_channel_valid12 },
    { "channel configuration validate source filter", test_channel_valid13 },
    { "channel configuration validate payload type", test_channel_valid14 },
    { "channel configuration validate FEC channels", test_channel_valid15 },
    { "channel configuration validate mid", test_channel_valid16 },
    { "channel configuration validate overflowed strings", 
      test_channel_valid17 },
    { "channel configuration validate illegal syntax", test_channel_valid18 },
    { "channel configuration validate overlap ports", test_channel_valid19 },
    { "channel configuration validate rtcp xr", test_channel_valid20 },
    { "channel configuration add", test_channel_add },
    { "channel configuration delete", test_channel_delete },
    CU_TEST_INFO_NULL,
};

CU_TestInfo test_cfg_database[] = {
    { "configuration database open", test_db_open },
    { "configuration database close", test_db_close },
    { "configuration database read", test_db_read },
    { "configuration database write", test_db_write },
    CU_TEST_INFO_NULL,
};

CU_TestInfo test_configmgr[] = {
    { "configuration manager initialization", test_cfgmgr_init },
    { "configuration manager shutdown", test_cfgmgr_shutdown },
    { "configuration manager update", test_cfgmgr_update },
    { "configuration manager access functions", test_cfgmgr_access },
    { "configuration manager save", test_cfgmgr_save },
    { "configuration manager remove FBTs", test_cfgmgr_removeFBTs },
    CU_TEST_INFO_NULL,
};

CU_SuiteInfo test_suites[] = {
    { "CFG test channel", channel_testsuite_init,
      channel_testsuite_cleanup, test_cfg_channel },
    { "CFG test database", database_testsuite_init,
      database_testsuite_cleanup, test_cfg_database },
    { "CFG test config manager", cfgmgr_testsuite_init,
      cfgmgr_testsuite_cleanup, test_configmgr },
    CU_SUITE_INFO_NULL,
};

/* The main() function for setting up and running the tests.
 * Returns a CUE_SUCCESS on successful running, another
 * CUnit error code on failure.
 */
int main(void)
{
    /* initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    if (CUE_SUCCESS != CU_register_suites(test_suites)) {
        return CU_get_error();
    }

    /* Initialize the logging facility */
    int loglevel = LOG_WARNING;

    syslog_facility_open(LOG_LIBCFG, LOG_CONS);
    syslog_facility_filter_set(LOG_LIBCFG, loglevel);

    if (loglevel == LOG_DEBUG) {
        VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_MGR);
        VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_DB);
        VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_CHANNEL);
        //VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_SDP);
    }

    /* Run all tests using the CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    if (CUE_SUCCESS != CU_basic_run_tests()) {
        return CU_get_error();
    }

    CU_cleanup_registry();
    syslog_facility_close();

    return CU_get_error();
}
