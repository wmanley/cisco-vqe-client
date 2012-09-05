/* 
 * Unit test program that based on CUnit, an OpenSource GPL
 * library. This is the main test file, which registers and runs all the
 * CFG unit tests.
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
#include "rtspclient_utests.h"

/***********************************************************************
 *
 *     CUnit FRAMEWORK STARTS HERE
 *
 **********************************************************************/

CU_TestInfo test_rtsp_client[] = {
    { "RTSP client initialization", test_rtsp_client_init },
    { "RTSP client DESCRIBE request", test_rtsp_client_request },
    CU_TEST_INFO_NULL,
};

CU_SuiteInfo test_suites[] = {
    { "RTSP Client unit tests", rtspclient_testsuite_init,
      rtspclient_testsuite_cleanup, test_rtsp_client },
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

    /* Run all tests using the CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    /* sa_ignore IGNORE_RETURN */
    CU_basic_run_tests();

    CU_cleanup_registry();
    return CU_get_error();
}
