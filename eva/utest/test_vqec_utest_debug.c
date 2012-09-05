/*
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vqec_debug.h"

/*
 * Unit tests for vqec_debug
 */

int count = 1;

int test_vqec_debug_init (void) {
    /* First thing is to openlog */
    int fds[] = {2};
    int num_fds = 1;
    syslog_facility_open_fds(LOG_VQEC, LOG_CONS, fds, num_fds);
    return 0;
}

int test_vqec_debug_clean (void) {
    /* Close log in your close() routine */
    syslog_facility_close();
    return 0;
}

static void test_vqec_debug_priority (void) {
    /* Set the facility priority level */
    uint8_t prio_filter = LOG_DEBUG;
    uint8_t got_filter = 0;
    syslog_facility_filter_set(LOG_VQEC, prio_filter);
    printf("Syslog facility filter set to %d\n", prio_filter);

    syslog_facility_filter_get(LOG_VQEC, got_filter);
    printf("Got syslog facility filter = %d\n", got_filter);
    CU_ASSERT(got_filter == prio_filter);

    syslog_facility_default_filter_get(LOG_VQEC, got_filter);
    printf("Got the default syslog facility filter = %d\n", 
            prio_filter);
    CU_ASSERT(got_filter == 3);
}

static void test_vqec_debug_log_messages (void) {
    boolean test_flag;
    /* Turn on the VQEC_DEBUG_RCC flag */
    VQEC_SET_DEBUG_FLAG(VQEC_DEBUG_RCC);
    VQEC_DEBUG(VQEC_DEBUG_RCC, "vqe-c debug error for rcc #%d\n", count++);
    test_flag = VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC);
    CU_ASSERT(test_flag == TRUE);

    /* Check all debug flags */
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CHANNEL));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_PCM));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_INPUT));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_OUTPUT));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_EVENT));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_IGMP));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TUNER));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RECV_SOCKET));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TIMER));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RTCP));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_ERROR_REPAIR));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC));

    /* Turn off the VQEC_DEBUG_RCC flag */
    VQEC_RESET_DEBUG_FLAG(VQEC_DEBUG_RCC);
    VQEC_DEBUG(VQEC_DEBUG_RCC,
                  "Should not print as the VQEC_DEBUG_RCC flag is off: %d\n", count++);
    test_flag = VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC);
    CU_ASSERT(test_flag == FALSE);

    /* Try printing general vqec_error */
    VQEC_LOG_ERROR("error that should always print\n");
}

CU_TestInfo test_array_debug[] = {
    {"test vqec_debug_priority",test_vqec_debug_priority},
    {"test vqec_debug_log_messages",test_vqec_debug_log_messages},
    CU_TEST_INFO_NULL,
};




