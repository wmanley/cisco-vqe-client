/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_kmod_vqec_utest_main.h"

/*
 * Only include vqec_debug.h with SYSLOG_DEFINITION defined ONCE per process
 * include it without SYSLOG_DEFINITION defined in all other .c files.
 * For the VQE-C library, which is included in all VQE-C processes, the
 * vqec_debug.h is included with SYSLOG_DEFINITION defined ONLY in the file
 * vqec_src_mgr.c and no where else.
 */
#include "vqec_debug.h"

/*
 *  TestInfo initializations
 */

CU_SuiteInfo suites_vqec[] = {
    {"VQEC_RPC", test_vqec_rpc_init,
     test_vqec_rpc_clean, test_array_rpc},
   CU_SUITE_INFO_NULL,
};

/*
 *  Main function for unit tests.
 */

int main (int argc, char* argv[]) {

    /* Set proper working directory */
    if (argc == 1) {
        printf("Usage: test_vqec_utest <directory containing unit_test dir>\n\r");
    }
    else if (argc > 1 && argv[1]) {
        if (chdir(argv[1]) != 0) {
            printf("Could not set working directory");
        }
    }
    /* Initialize CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    if (CUE_SUCCESS != CU_register_suites(suites_vqec)) {
        return CU_get_error();
    }

    /* Run all tests using CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    /* sa_ignore {} IGNORE_RETURN(1) */
    CU_basic_run_tests();

    CU_cleanup_registry();
    return CU_get_error();
}


