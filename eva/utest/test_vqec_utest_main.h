/*
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifndef __VQEC_UTEST_MAIN_H__
#define __VQEC_UTEST_MAIN_H__

#include <unistd.h>
#include <stdio.h>
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

/* useful macro for printing buffers to screen in hex */
#define PRINT_BUF_HEX(buf_name,buf,buf_len){ \
        printf("\n%s[%u]:", buf_name, buf_len); \
        int i; \
        for (i = 0; i < buf_len; i++) \
        { \
          if (i % 8 == 0) \
            printf(" "); \
          if (i % 24 == 0) \
            printf("\n"); \
          printf("%02hx ",buf[i] & 0x00ff); \
        } \
}

/* unit tests for recv_socket */
int test_vqec_recv_socket_init(void);
int test_vqec_recv_socket_clean(void);
extern CU_TestInfo test_array_recv_sock[];

/* unit tests for url */
int test_vqec_url_init(void);
int test_vqec_url_clean(void);
extern CU_TestInfo test_array_url[];

/* unit tests for tuner */
int test_vqec_tuner_init(void);
int test_vqec_tuner_clean(void);
extern CU_TestInfo test_array_tuner[];

/* unit tests for channel */
int test_vqec_channel_init(void);
int test_vqec_channel_clean(void);
extern CU_TestInfo test_array_channel[];

/* unit tests for pak */
int test_vqec_pak_init(void);
int test_vqec_pak_clean(void);
extern CU_TestInfo test_array_pak[];

/* unit tests for pak_seq */
int test_vqec_pak_seq_init(void);
int test_vqec_pak_seq_clean(void);
extern CU_TestInfo test_array_pak_seq[];

/* unit tests for seq_num */
int test_vqec_seq_num_init(void);
int test_vqec_seq_num_clean(void);
extern CU_TestInfo test_array_seq_num[];

/* unit tests for event */
int test_vqec_event_init(void);
int test_vqec_event_clean(void);
extern CU_TestInfo test_array_event[];

/* unit tests for ev_timer */
int test_vqec_ev_timer_init(void);
int test_vqec_ev_timer_clean(void);
extern CU_TestInfo test_array_ev_timer[];

/* unit tests for hash */
int test_vqe_hash_init(void);
int test_vqe_hash_clean(void);
extern CU_TestInfo test_array_hash[];

/* unit tests for time */
int test_vqec_time_init(void);
int test_vqec_time_clean(void);
extern CU_TestInfo test_array_time[];

/* unit tests for cfg */
int test_vqec_cfg_init(void);
int test_vqec_cfg_clean(void);
extern CU_TestInfo test_array_cfg[];

/* unit tests for stun mgr */
int test_vqec_stun_mgr_init(void);
int test_vqec_stun_mgr_clean(void);
extern CU_TestInfo test_array_stun_mgr[];

/* unit tests for debug */
int test_vqec_debug_init(void);
int test_vqec_debug_clean(void);
extern CU_TestInfo test_array_debug[];

/* unit tests for cli*/
int test_vqec_cli_init(void);
int test_vqec_cli_clean(void);
extern CU_TestInfo test_array_cli[];

/* unit tests for port */
int test_vqec_port_init(void);
int test_vqec_port_clean(void);
extern CU_TestInfo test_array_port[];

/* unit tests for igmp*/
int test_vqec_igmp_init(void);
int test_vqec_igmp_clean(void);
extern CU_TestInfo test_array_igmp[];

/* unit tests for stream_output*/
int test_vqec_stream_output_init(void);
int test_vqec_stream_output_clean(void);
extern CU_TestInfo test_array_stream_output[];

/* unit tests for log*/
int test_vqec_log_init(void);
int test_vqec_log_clean(void);
extern CU_TestInfo test_array_log[];

/* unit tests for ifclient*/
int test_vqec_ifclient_init(void);
int test_vqec_ifclient_clean(void);
extern CU_TestInfo test_array_ifclient[];

/* unit tests for updater */
int test_vqec_updater_init(void);
int test_vqec_updater_clean(void);
extern CU_TestInfo test_array_updater[];

/* unit tests for rtp */
int test_vqec_rtp_init(void);
int test_vqec_rtp_clean(void);
extern CU_TestInfo test_array_rtp[];

/* unit tests for heap */
int test_vqec_heap_init(void);
int test_vqec_heap_clean(void);
extern CU_TestInfo test_array_heap[];

/* unit tests for gaptree */
int test_vqec_gaptree_init(void);
int test_vqec_gaptree_clean(void);
extern CU_TestInfo test_array_gaptree[];

/* unit tests for gap reporter */
int test_vqec_gap_reporter_init(void);
int test_vqec_gap_reporter_clean(void);
extern CU_TestInfo test_array_gap_reporter[];

/* unit tests for bitmap */
int test_vqec_bitmap_init(void);
int test_vqec_bitmap_clean(void);
extern CU_TestInfo test_array_bitmap[];

/* unit tests for nat */
int test_vqec_nat_init(void);
int test_vqec_nat_clean(void);
extern CU_TestInfo test_array_nat[];

#if HAVE_FCC
/* unit tests for CP SM */
int test_vqec_sm_init(void);
int test_vqec_sm_clean(void);
extern CU_TestInfo test_array_sm[];
#endif /* HAVE_FCC */

/* unit tests for upcall */
int test_vqec_upcall_init(void);
int test_vqec_upcall_clean(void);
extern CU_TestInfo test_array_upcall[];

/* unit tests for drop */
int test_vqec_drop_init(void);
int test_vqec_drop_clean(void);
extern CU_TestInfo test_array_drop[];

/* unit tests for configparser */
int test_vqec_configparser_init(void);
int test_vqec_configparser_clean(void);
extern CU_TestInfo test_array_configparser[];

/* unit tests for failover */
int test_vqec_failover_init(void);
int test_vqec_failover_clean(void);
extern CU_TestInfo test_array_failover[];

#endif
