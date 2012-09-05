/********************************************************************
 * vqes_debug_test_rtp.c
 *
 * Test VQE LIBTP debug and syslog message logging
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

#include <log/vqe_rtp_syslog_def.h>
#include <log/vqe_rtp_debug.h>

void test_librtp_log(uint32_t stb_ip)
{
    int32_t count = 0;
    char str[INET_ADDRSTRLEN];
    debug_filter_item_t ssrc_filter;
    ssrc_filter.type = DEBUG_FILTER_TYPE_STB_IP;
    ssrc_filter.val = stb_ip;

    count = 10;

    /* Do some syslog message logging first */
    syslog_print(RTCP_MEM_INIT_FAILURE, "ERA-LA-MB");
    syslog_print(RTCP_LOCAL_SSRC_UPDATE_FAILURE, 
                 "ERA-LA", "1.2.3.4", 1234, 0x12341234, 0x56785678);


    /*************************************************************/
    /* Debug messages are filtered, depending on how the filter is setup for
     * this debug flag.
     */
    VQE_RTP_DEBUG (RTP_DEBUG_PACKETS, &ssrc_filter,
                   "Filtered LIBRTP debug message %d: STB IP=%s\n", 
                   count++, uint32_ntoa_r(stb_ip, str, sizeof(str)));

    /* No filtering: i.e. log debug messages for all SSRCs */
    VQE_RTP_DEBUG (RTP_DEBUG_PACKETS, NULL, 
                   "Unfiltered LIBRTP debug message %d: STB IP=%s\n",
                   count++, uint32_ntoa_r(stb_ip, str, sizeof(str)));
    

}
