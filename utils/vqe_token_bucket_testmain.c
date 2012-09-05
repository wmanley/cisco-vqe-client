/*
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file contains unit tests for the VQE token bucket APIs
 */


/* Included via vqe_token_bucket_tests.h:
#include "../add-ons/include/CUnit/CUnit.h"
*/
#include "../add-ons/include/CUnit/Basic.h"

/*
 * Declaration of test suites.
 * See vqe_token_bucket_tests.c for definition.
 */ 
extern CU_SuiteInfo suites_vqe_token_bucket[];

/* 
 * main()
 *
 * Sets up and runs unit tests for VQE token bucket functions.
 *
 * Parameters
 *    Not used.
 *
 * Returns 
 *    CUE_SUCCESS on successful running, 
 *    another CUnit error code on failure.
 */
int main (int argc, char *argv[])
{

    /* initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return (CU_get_error());
    }

    if (CUE_SUCCESS != CU_register_suites(suites_vqe_token_bucket)) {
        return (CU_get_error());
    }

    /* Run all tests using the CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    (void)CU_basic_run_tests();

    CU_cleanup_registry();

    return (CU_get_error());
}
