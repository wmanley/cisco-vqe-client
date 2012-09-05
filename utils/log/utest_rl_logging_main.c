/* Rate Limited Logging Unit Test
 *
 * February 2008, Donghai Ma
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "add-ons/include/CUnit/Basic.h"


/*
 * Declaration of test suites.
 * See vqes_rl_logging_tests.c for definition.
 */ 
extern CU_SuiteInfo suites_vqes_rl_logging[];


/*------------------------------------------------------------------------*/
/* Among all the C files including it, only ONE C file (say the one where 
 * main() is defined) should instantiate the message definitions described in 
 * XXX_syslog_def.h.   To do so, the designated C file should define the symbol
 * SYSLOG_DEFINITION before including the file.  
 */
#define SYSLOG_DEFINITION
#include "log/vqes_cp_syslog_def.h"
#include "msgen/msgen_syslog_def.h"
#undef SYSLOG_DEFINITION


/* 
 * main()
 *
 * Sets up and runs unit tests for VQE_S rate limited syslog logging
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

    if (CUE_SUCCESS != CU_register_suites(suites_vqes_rl_logging)) {
        return (CU_get_error());
    }

    /* Initialize the logging facility */
    syslog_facility_open(LOG_VQES_CP, LOG_CONS);
    syslog_facility_filter_set(LOG_VQES_CP, LOG_INFO);

    /* Run all tests using the CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    (void)CU_basic_run_tests();

    CU_cleanup_registry();

    return (CU_get_error());
}
