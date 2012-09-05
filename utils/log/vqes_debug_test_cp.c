/********************************************************************
 * vqes_debug_test.c
 *
 * Test VQES debug and syslog message logging
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utils/vam_util.h"   /* for uint32_ntoa_r() */


/* Don't #define SYSLOG_DEFINITION when including xxxx_syslog_def.h file 
 * because it can only be instantiated ONCE, as in vqes_debug_test_main.c file.
 */
#include <log/vqes_cp_syslog_def.h>
#include <log/vqes_cp_debug.h>

/* Test debug logging filtered on Channel IP:Port */
void test_vqes_cp_chan_log(uint32_t chan_ip, uint16_t chan_port)
{
    char str[INET_ADDRSTRLEN];
    int32_t count = 0;
    debug_filter_item_t chan_filter;
    chan_filter.type = DEBUG_FILTER_TYPE_CHANNEL;
    chan_filter.val = CONCAT_U32_U16_TO_U64(chan_ip, chan_port);

    count = 1;

    /* Do some syslog message logging first */
    syslog_print(CP_INIT_FAILURE_CRIT, "Data Plane not being up yet");
    syslog_print(CP_INIT_FAILURE_CRIT, "failing to create a RTP session");

    /* Debug messages are filtered, depending on how the filter is setup for
     * this debug flag.
     */
    VQES_CP_DEBUG(ERA_DEBUG_ER, &chan_filter, 
                  "Control Plane Error Repair Test Message %d: chan=%s:%u\n", 
                  count++, uint32_ntoa_r(chan_ip, str, sizeof(str)), 
                  ntohs(chan_port));

    /* No filtering, i.e. log debug messages for all channels */
    VQES_CP_DEBUG(ERA_DEBUG_ER, NULL,
                  "Unfiltered debug message %d: chan=%s:%u\n", 
                  count++, uint32_ntoa_r(chan_ip, str, sizeof(str)), 
                  ntohs(chan_port));

    VQES_CP_DEBUG(ERA_DEBUG_FCC, &chan_filter,
                  "Control Plane FCC debug message %d, chan=%s:%u\n",
                  count++, uint32_ntoa_r(chan_ip, str, sizeof(str)), 
                  ntohs(chan_port));

}

/* Test debug logging filtered on STB IP address */
void test_vqes_cp_stb_log(uint32_t stb_ip)
{
    char str[INET_ADDRSTRLEN];
    int32_t count = 0;
    debug_filter_item_t chan_filter;
    chan_filter.type = DEBUG_FILTER_TYPE_STB_IP;
    chan_filter.val = stb_ip;

    count = 1;

    /* Do some syslog message logging first */
    syslog_print(CP_INIT_FAILURE_CRIT, "Data Plane not being up yet");
    syslog_print(CP_INIT_FAILURE_CRIT, "failing to create a RTP session");

    /* Debug messages are filtered, depending on how the filter is setup for
     * this debug flag.
     */
    VQES_CP_DEBUG(ERA_DEBUG_ER, &chan_filter, 
                  "Control Plane Error Repair Test Message %d: STB=%s\n", 
                  count++, uint32_ntoa_r(stb_ip, str, sizeof(str)));

    /* No filtering, i.e. log debug messages for all channels */
    VQES_CP_DEBUG(ERA_DEBUG_ER, NULL,
                  "Unfiltered debug message %d: STB=%s\n",
                  count++, uint32_ntoa_r(stb_ip, str, sizeof(str)));

    VQES_CP_DEBUG(ERA_DEBUG_FCC, &chan_filter,
                  "Control Plane FCC debug message %d, STB=%s\n",
                  count++, uint32_ntoa_r(stb_ip, str, sizeof(str)));

}
