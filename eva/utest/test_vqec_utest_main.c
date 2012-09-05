/*
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"

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
/** 
* TODO: Add back in unit-tests.
*/
#if 0 
    {"VQEC_LOG", test_vqec_log_init, 
     test_vqec_log_clean, test_array_log},
#endif
    {"VQEC_FAILOVER", test_vqec_failover_init, test_vqec_failover_clean,
     test_array_failover},
    {"VQEC_CFG", test_vqec_cfg_init, test_vqec_cfg_clean,
     test_array_cfg},
    {"VQEC_IFCLIENT", test_vqec_ifclient_init,
     test_vqec_ifclient_clean, test_array_ifclient},
    {"VQEC_UPDATER", test_vqec_updater_init,
     test_vqec_updater_clean, test_array_updater},
    {"VQEC_DROP", test_vqec_drop_init, test_vqec_drop_clean,
     test_array_drop},
    {"VQEC_PORT", test_vqec_port_init, test_vqec_port_clean,
     test_array_port},
    {"VQEC_DEBUG", test_vqec_debug_init, test_vqec_debug_clean,
     test_array_debug},
    {"VQEC_TUNER", test_vqec_tuner_init, test_vqec_tuner_clean,
     test_array_tuner},
    {"VQEC_CHANNEL", test_vqec_channel_init, test_vqec_channel_clean,
     test_array_channel},
    {"VQEC_GAP_REPORTER", test_vqec_gap_reporter_init, 
     test_vqec_gap_reporter_clean, test_array_gap_reporter},
    {"VQEC_RECV_SOCKET", test_vqec_recv_socket_init, test_vqec_recv_socket_clean,
     test_array_recv_sock},
    {"VQEC_NAT", test_vqec_nat_init, test_vqec_nat_clean, test_array_nat},
    {"VQEC_PAK", test_vqec_pak_init, test_vqec_pak_clean,
     test_array_pak},
    {"VQEC_PAK_SEQ", test_vqec_pak_seq_init, test_vqec_pak_seq_clean,
     test_array_pak_seq},
    {"VQEC_SEQ_NUM", test_vqec_seq_num_init, test_vqec_seq_num_clean,
     test_array_seq_num},
    {"VQE_HASH", test_vqe_hash_init, test_vqe_hash_clean,
     test_array_hash},
    {"VQEC_URL", test_vqec_url_init, test_vqec_url_clean,
     test_array_url},
    {"VQEC_RTP", test_vqec_rtp_init, 
     test_vqec_rtp_clean, test_array_rtp}, 
    {"VQEC_CLI", test_vqec_cli_init, test_vqec_cli_clean,
     test_array_cli},
    {"VQEC_GAPTREE", test_vqec_gaptree_init, 
     test_vqec_gaptree_clean, test_array_gaptree},
    {"VQEC_HEAP", test_vqec_heap_init, 
     test_vqec_heap_clean, test_array_heap},
    {"VQEC_BITMAP", test_vqec_bitmap_init, 
     test_vqec_bitmap_clean, test_array_bitmap}, 
    {"VQEC_IGMP", test_vqec_igmp_init, test_vqec_igmp_clean,
     test_array_igmp},
    {"VQEC_STREAM_OUTPUT", test_vqec_stream_output_init, 
     test_vqec_stream_output_clean, test_array_stream_output},
#if HAVE_FCC
    {"VQEC_SM", test_vqec_sm_init, 
     test_vqec_sm_clean, test_array_sm},
#endif /* HAVE_FCC */
    {"VQEC_UPCALL", test_vqec_upcall_init, 
     test_vqec_upcall_clean, test_array_upcall},
    {"VQEC_EVENT", test_vqec_event_init, test_vqec_event_clean,
     test_array_event},
    {"VQEC_CONFIGPARSER", test_vqec_configparser_init,
     test_vqec_configparser_clean, test_array_configparser},
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


