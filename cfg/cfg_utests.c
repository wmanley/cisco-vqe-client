/* 
 * CFG unit test program.
 *
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utils/vam_types.h>
#include "cfgapi.h"
#include "cfg_database.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
#include <sdp.h>
#include <arpa/inet.h>

#define TEST_DB "vam.cfg"
#define DB_IN_DOS_FMT "sdp-in-dos-fmt.cfg"
#define DATA_SIZE 2048
#define NEW_DB "new.cfg"
#define TOTAL_CHANNELS 7

static channel_mgr_t channel_manager;
static void *sdp_cfg_p;

/* sa_ignore DISABLE_RETURNS CU_assertImplementation */

#define NEW_CHANNEL "v=0\n\
o=- 4262738225 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
b=RS:68750\n\
b=RR:206250\n\
a=fmtp:96 rtcp-per-rcvr-bw=55\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"


#define SAME_FBT_CHANNEL "v=0\n\
o=- 26273822384 11589520464 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.9/255\n\
b=AS:5500\n\
b=RS:68750\n\
b=RR:206250\n\
a=fmtp:96 rtcp-per-rcvr-bw=55\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.9 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.9/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.9 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"


#define SAME_RTX_CHANNEL "v=0\n\
o=- 26273422385 11583520464 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.13/255\n\
b=AS:5500\n\
b=RS:68750\n\
b=RR:206250\n\
a=fmtp:96 rtcp-per-rcvr-bw=55\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.13 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.9/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.9 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45010\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:45010\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

/***********************************************************************
 *
 *     CHANNEL SUITE TESTS START HERE
 *
 **********************************************************************/
int channel_testsuite_init (void)
{
    if (cfg_init(TEST_DB) != CFG_SUCCESS) {
        return -1;
    }

    /* Set up a default SDP configuration for all the sessions */
    sdp_cfg_p = sdp_init_config();

    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_TRACE, FALSE);
    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_WARNINGS, FALSE);
    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_ERRORS, TRUE);

    sdp_require_version(sdp_cfg_p, TRUE);
    sdp_require_owner(sdp_cfg_p, TRUE);
    sdp_require_session_name(sdp_cfg_p, TRUE);
    sdp_require_timespec(sdp_cfg_p, TRUE);

    sdp_media_supported(sdp_cfg_p, SDP_MEDIA_VIDEO, TRUE);

    sdp_nettype_supported(sdp_cfg_p, SDP_NT_INTERNET, TRUE);    
    sdp_addrtype_supported(sdp_cfg_p, SDP_AT_IP4, TRUE);
   
    sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_UDP, TRUE);
    sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_RTPAVP, TRUE);
    sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_RTPAVPF, TRUE);
    
    sdp_allow_choose(sdp_cfg_p, SDP_CHOOSE_CONN_ADDR, TRUE);
    sdp_allow_choose(sdp_cfg_p, SDP_CHOOSE_PORTNUM, TRUE);
    
    return 0;
}


int channel_testsuite_cleanup (void)
{
    if (cfg_shutdown() != CFG_SUCCESS) {
        return -1;
    }

    my_free(sdp_cfg_p);
    return 0;
}


void test_channel_add (void)
{
    cfg_db_ret_e status;
    channel_mgr_t *mgr_p;
    channel_cfg_t *sdp_p;
    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    CU_ASSERT_PTR_NOT_NULL(mgr_p);

    if (mgr_p) {
        CU_ASSERT_EQUAL(mgr_p->total_num_channels,
                        cfg_get_total_num_channels());
        CU_ASSERT_EQUAL(mgr_p->total_num_channels,
                        cfg_get_total_num_active_channels());

        /* Test case 1: Add the same channel */
        if (cfg_get_total_num_channels() > 0) {
            sdp_p = cfg_get_channel_cfg_from_idx(0);
            CU_ASSERT_PTR_NOT_NULL(sdp_p);

            if (sdp_p) {
                status = cfg_db_create_SDP(sdp_p, data_buffer, DATA_SIZE);
                CU_ASSERT_EQUAL(status, CFG_DB_SUCCESS);

                data_p = data_buffer;
                sdp_in_p = sdp_init_description(sdp_cfg_p);
                result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
                CU_ASSERT_EQUAL(result, SDP_SUCCESS);

                chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
                CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p,
                                                    &handle, chksum),
                                    CFG_CHANNEL_SUCCESS);
            }
        }

        /* Test case 2: Add a new channel */
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, NEW_CHANNEL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                        CFG_CHANNEL_FAILED_ADD_MAP);

        /* Test case 3: Add a channel with the same fbt address and port */
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_FBT_CHANNEL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                        CFG_CHANNEL_FAILED_ADD_MAP);

        /* Test case 4: Add a channel with the same rtx address and port */
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_RTX_CHANNEL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                        CFG_CHANNEL_FAILED_ADD_MAP);
    }
}


void test_channel_delete (void)
{
    channel_mgr_t *mgr_p;

    mgr_p = cfg_get_channel_mgr();
    CU_ASSERT_PTR_NOT_NULL(mgr_p);

    if (mgr_p) {
        CU_ASSERT_EQUAL(mgr_p->total_num_channels, 
                        cfg_get_total_num_channels());

        if (cfg_get_total_num_channels() > 0) {
            CU_ASSERT_EQUAL(cfg_channel_delete(mgr_p, mgr_p->handles[0], TRUE),
                            CFG_CHANNEL_SUCCESS);

            CU_ASSERT_EQUAL(mgr_p->handles[0], ILLEGAL_ID);
        }
    }
}

/* Validation test case 1: validate the SDP version number */
void test_channel_valid1 (void)
{
#define BAD_VERSION "v=1\n\
o=- 115689375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;

    memset(data_buffer, 0, DATA_SIZE);
    strncpy(data_buffer, BAD_VERSION, DATA_SIZE);
    data_p = data_buffer;
    sdp_in_p = sdp_init_description(sdp_cfg_p);
    result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
    CU_ASSERT_NOT_EQUAL(result, SDP_SUCCESS);
}


/* Validation test case 2: validate the IP version number */
void test_channel_valid2 (void)
{
#define BAD_IP_VERSION "v=0\n\
o=- 115689375 1158953264 IN IP6 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define BAD_SESSION_VERSION "v=0\n\
o=- 115689375 some-number IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define MISSING_TYPE_VERSION "v=0\n\
o=- 115689375 1189273648 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define OVERFLOW_VERSION "v=0\n\
o=- 115689375 9223372036854775807 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    memset(data_buffer, 0, DATA_SIZE);
    strncpy(data_buffer, BAD_IP_VERSION, DATA_SIZE);
    data_p = data_buffer;
    sdp_in_p = sdp_init_description(sdp_cfg_p);
    result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
    CU_ASSERT_NOT_EQUAL(result, SDP_SUCCESS);

    memset(data_buffer, 0, DATA_SIZE);
    strncpy(data_buffer, BAD_SESSION_VERSION, DATA_SIZE);
    data_p = data_buffer;
    sdp_in_p = sdp_init_description(sdp_cfg_p);
    result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
    CU_ASSERT_NOT_EQUAL(result, SDP_SUCCESS);

    memset(data_buffer, 0, DATA_SIZE);
    strncpy(data_buffer, MISSING_TYPE_VERSION, DATA_SIZE);
    data_p = data_buffer;
    sdp_in_p = sdp_init_description(sdp_cfg_p);
    result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
    CU_ASSERT_NOT_EQUAL(result, SDP_SUCCESS);

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, OVERFLOW_VERSION, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 3: validate non-zero time values */
void test_channel_valid3 (void)
{
#define NON_ZERO_START_TIME "v=0\n\
o=- 115689375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=1102928390 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define NON_NUMERIC_TIME "v=0\n\
o=- 115689375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 <time>\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define FUTURE_STOP_TIME "v=0\n\
o=- 115689375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 999999999999999\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, NON_ZERO_START_TIME, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, NON_NUMERIC_TIME, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, FUTURE_STOP_TIME, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 4: validate rtcp-unicast attribute */
void test_channel_valid4 (void)
{
#define MISSING_RTCP_ATTRIBUTE "v=0\n\
o=- 115689375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISSING_RTCP_ATTRIBUTE, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 5: validate group attribute */
void test_channel_valid5 (void)
{
#define MISSING_GROUP_ATTRIBUTE "v=0\n\
o=- 115689375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define MISMATCHED_GROUP_ATTRIBUTE1 "v=0\n\
o=- 115689375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"


#define MISMATCHED_GROUP_ATTRIBUTE2 "v=0\n\
o=- 115689375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=sendonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=rtcp:50001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISSING_GROUP_ATTRIBUTE, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISMATCHED_GROUP_ATTRIBUTE1, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISMATCHED_GROUP_ATTRIBUTE2, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}



/* Validation test case 6: validate random order of media sessions */
void test_channel_valid6 (void)
{
#define RANDOM_ORDER "v=0\n\
o=- 115689375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.4/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.4 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 47000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:47001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45005\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RANDOM_ORDER, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                        CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 7: validate missing media sessions */
void test_channel_valid7 (void)
{
#define MISSING_SESSION "v=0\n\
o=- 11244375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISSING_SESSION, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 8: validate encoding name and clock rate */
void test_channel_valid8 (void)
{
#define BAD_ENCODER_1 "v=0\n\
o=- 115689273 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 h263-1998/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define BAD_ENCODER_2 "v=0\n\
o=- 115689273 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/50000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define BAD_ENCODER_3 "v=0\n\
o=- 115689273 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 ulaw/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define BAD_ENCODER_4 "v=0\n\
o=- 115689273 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/8000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define BAD_ENCODER_5 "v=0\n\
o=- 115689273 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/900es00\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_ENCODER_1, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_ENCODER_2, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_ENCODER_3, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_ENCODER_4, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_ENCODER_5, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_INVALID_CLOCK_IN_RTPMAP_LINE);
    }
}


/* Validation test case 9: validate rtcp bandwidths */
void test_channel_valid9 (void)
{
#define NO_RTCP_BW_CHANNEL "v=0\n\
o=- 11262738123 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.5/255\n\
b=AS:2500\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.5 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45002\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46100 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46101\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define NO_BW_CHANNEL "v=0\n\
o=- 11262738124 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
a=fmtp:96 rtcp-per-rcvr-bw=55\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define MISSING_BW_CHANNEL "v=0\n\
o=- 11262738125 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.6/255\n\
b=AS:2500\n\
b=RR:206250\n\
a=fmtp:96 rtcp-per-rcvr-bw=55\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.6 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45004\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46900 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46901\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define MISSING_AS_CHANNEL "v=0\n\
o=- 11262738126 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.7/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=fmtp:96 rtcp-per-rcvr-bw=55\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.7 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45006\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46200 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46201\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define BIG_AS_CHANNEL "v=0\n\
o=- 11262938126 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.2.1.7/255\n\
b=AS:40001\n\
b=RR:206250\n\
b=RS:68750\n\
a=fmtp:96 rtcp-per-rcvr-bw=55\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.2.1.7 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.2.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.2.1.1 10.11.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45006\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46200 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.11.20.114\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46201\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define BIG_RR_CHANNEL "v=0\n\
o=- 11262938126 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.2.1.7/255\n\
b=AS:4000\n\
b=RR:99999999999999999999\n\
b=RS:68750\n\
a=fmtp:96 rtcp-per-rcvr-bw=55\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.2.1.7 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.2.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.2.1.1 10.11.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45006\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46200 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.11.20.114\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46201\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define ZERO_BW_CHANNEL "v=0\n\
o=- 11262758123 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
m=video 50000 RTP/AVPF 96\n\
i=Original source stream\n\
c=IN IP4 224.11.1.5/255\n\
b=AS:2500\n\
b=RR:0\n\
b=RS:0\n\
a=fmtp:96 rtcp-per-rcvr-bw=0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.11.1.5 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=rtcp-fb:96 nack\n\
a=rtcp-fb:96 nack pli\n\
a=rtcp:50001 IN IP4 10.10.20.214\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45002\n\
a=mid:2\n\
m=video 47100 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RR:0\n\
b=RS:0\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:47101\n\
a=fmtp:99 apt=96\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"


#define ILLEGAL_MODE_CHANNEL "v=0\n\
o=- 11262758129 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
m=video 50000 RTP/AVPF 96\n\
i=Original source stream\n\
c=IN IP4 224.11.1.5/255\n\
b=AS:2500\n\
b=RR:0\n\
b=RS:0\n\
a=fmtp:96 rtcp-per-rcvr-bw=40\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.11.1.5 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=rtcp-fb:96 nack\n\
a=rtcp-fb:96 nack pli\n\
a=rtcp:50001\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45002\n\
a=mid:2\n\
m=video 46100 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RR:0\n\
b=RS:0\n\
a=inactive\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46101\n\
a=fmtp:99 apt=96\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define BAD_RTCP_PER_RCVR_BW "v=0\n\
o=- 11262738123 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.5/255\n\
b=AS:2500\n\
a=fmtp:96 rtcp-per-rcvr-bw=junk\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.5 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45002\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46100 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46101\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"


#define MISMATCH_PT_RTCP_PER_RCVR_BW "v=0\n\
o=- 11262738223 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.5/255\n\
b=AS:2500\n\
a=fmtp:196 rtcp-per-rcvr-bw=40\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.5 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45002\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46100 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46101\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, NO_RTCP_BW_CHANNEL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                        CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, NO_BW_CHANNEL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISSING_BW_CHANNEL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                        CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISSING_AS_CHANNEL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BIG_AS_CHANNEL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BIG_RR_CHANNEL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_FIELD_VALUE_OVERFLOW);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, ZERO_BW_CHANNEL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                        CFG_CHANNEL_SUCCESS);

        channel_cfg_t *channel_p;
        channel_p = cfg_get_channel_cfg_from_hdl(handle);
        if (channel_p) {
            CU_ASSERT_EQUAL(channel_p->original_rtcp_sndr_bw, 0);
            CU_ASSERT_EQUAL(channel_p->original_rtcp_rcvr_bw, 0);
            CU_ASSERT_EQUAL(channel_p->original_rtcp_per_rcvr_bw, 0);
        }

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, ILLEGAL_MODE_CHANNEL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_RTCP_PER_RCVR_BW, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_INVALID_RTCP_PER_RCVR_BW_LINE);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISMATCH_PT_RTCP_PER_RCVR_BW, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 10: validate src IP address in re-sourced session */
void test_channel_valid10 (void)
{
#define BAD_SRC_IP_ADDRESS "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.114\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_SRC_IP_ADDRESS, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 11: validate invalid port */
void test_channel_valid11 (void)
{
#define BAD_RTP_PORT "v=0\n\
o=- 111199375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 128 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.11/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.11 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.11/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.11 10.10.200.116\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.116\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define ZERO_RTCP_PORT "v=0\n\
o=- 111299375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 1300 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.2.11/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.2.11 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.11/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.11 10.10.200.116\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:0 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.116\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define LARGE_RTCP_PORT "v=0\n\
o=- 111293375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 1300 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.3.11/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.3.11 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.11/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.11 10.10.200.116\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:70000 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.116\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define INCORRECT_FBT "v=0\n\
o=- 111293375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 1300 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.3.11/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.3.11 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.11/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.11 10.10.200.116\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.116\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define DUPLICATED_RTCP_PORT "v=0\n\
o=- 117327459095 1173459092 IN IP4 vam-cluster.cisco.com\n\
s=Channel 3\n\
i=Channel configuration for Channel 3\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
m=video 60000 RTP/AVPF 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.3/255\n\
b=AS:5500\n\
b=RS:68750\n\
b=RR:206250\n\
a=fmtp:96 rtcp-per-rcvr-bw=55\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.3 192.168.1.3\n\
a=rtpmap:96 MP2T/90000\n\
a=rtcp:60001\n\
a=mid:1\n\
m=video 45004 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.3/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.3 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=mid:2\n\
m=video 62000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:55\n\
b=RR:55\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:50001\n\
a=fmtp:99 apt=96\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_RTP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, ZERO_RTCP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, LARGE_RTCP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, INCORRECT_FBT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, DUPLICATED_RTCP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 12: validate invalid IP addresses */
void test_channel_valid12 (void)
{
#define BAD_MC_ADDR "v=0\n\
o=- 111199375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 5000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.11/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.11 10.10.200.116\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.116\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define BAD_SRC_IP_ADDR "v=0\n\
o=- 111299375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 1300 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.2.11/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.2.11 192.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.11/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.11 10.10.200.116\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:0 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.116\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define BAD_FBT_ADDR "v=0\n\
o=- 111299375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 1300 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.2.11/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.2.11 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.11/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.11 10.10.1.116\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:20030 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.116\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_MC_ADDR, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_SRC_IP_ADDR, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_FBT_ADDR, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 13: validate missing source filter line */
void test_channel_valid13 (void)
{
#define MISSING_SRC_FILTER "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 48010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:48001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define BAD_SRC_IP "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 233.200.115\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISSING_SRC_FILTER, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                        CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, BAD_SRC_IP, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 14: validate payload type */
void test_channel_valid14 (void)
{
#define ILLEGAL_PAYLOAD_TYPE_1 "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 34\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:34 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define ILLEGAL_PAYLOAD_TYPE_2 "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 94\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:94 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:94 nack\n\
a=rtcp-fb:94 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=94\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, ILLEGAL_PAYLOAD_TYPE_1, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, ILLEGAL_PAYLOAD_TYPE_2, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 15: validate FEC channels */
void test_channel_valid15 (void)
{
#define ILLEGAL_PROTO "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 50000 udp 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"

#define ILLEGAL_ENCODED_NAME "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 46000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=rtcp:46001\n\
a=mid:4\n\
m=video 46002 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46003\n\
a=mid:5\n"

#define ILLEGAL_GROUPING "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 3 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 46000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 2dparityfec/90000\n\
a=rtcp:46001\n\
a=mid:4\n\
m=video 46002 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=rtcp:46003\n\
a=mid:5\n"

#define SAME_PORT_IN_COL "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 2 4 5\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 46000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 2dparityfec/90000\n\
a=rtcp:46000\n\
a=mid:4\n\
m=video 46002 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=rtcp:46003\n\
a=mid:5\n"

#define SAME_PORT_IN_ROW "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 2 4 5\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 46000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 2dparityfec/90000\n\
a=rtcp:46001\n\
a=mid:4\n\
m=video 46002 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=rtcp:46002\n\
a=mid:5\n"

#define MISSING_CONNECTION "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 2 4 5\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 46000 RTP/AVP 100\n\
i=FEC column stream\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 2dparityfec/90000\n\
a=rtcp:46001\n\
a=mid:4\n\
m=video 46002 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=rtcp:46003\n\
a=mid:5\n"

#define MISSING_ATTRIBUTE "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 2 4 5\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 46000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 2dparityfec/90000\n\
a=rtcp:46001\n\
a=mid:4\n\
m=video 46002 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=rtcp:46003\n\
a=mid:5\n"

#define MISSING_SOURCE_FILTER "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 2 4 5\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 46000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 2dparityfec/90000\n\
a=rtcp:46001\n\
a=mid:4\n\
m=video 46002 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
a=recvonly\n\
a=rtpmap:101 2dparityfec/90000\n\
a=rtcp:46003\n\
a=mid:5\n"

#define SAME_RTP_PORT_IN_FEC "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 2 4 5\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 46000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 2dparityfec/90000\n\
a=rtcp:46001\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 230.1.1.10/255\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=rtcp:46003\n\
a=mid:5\n"

#define SAME_RTCP_PORT_IN_FEC "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 2 4 5\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 46000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 2dparityfec/90000\n\
a=rtcp:46001\n\
a=mid:4\n\
m=video 46002 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 230.1.1.10/255\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"


    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, ILLEGAL_PROTO, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, ILLEGAL_ENCODED_NAME, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, ILLEGAL_GROUPING, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_PORT_IN_COL, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_PORT_IN_ROW, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISSING_CONNECTION, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISSING_ATTRIBUTE, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISSING_SOURCE_FILTER, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_RTP_PORT_IN_FEC, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_RTCP_PORT_IN_FEC, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 16: validate missing mid fileds */
void test_channel_valid16 (void)
{
#define MISSING_MID_IN_RTX "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"

#define MISSING_MID_IN_FEC "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4 5\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 46000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=rtcp:46001\n\
m=video 46002 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=rtcp:46003\n\
a=mid:5\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISSING_MID_IN_RTX, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, MISSING_MID_IN_FEC, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 17: validate overflowed strings */
void test_channel_valid17 (void)
{
#define OVERFLOWED_USERNAME "v=0\n\
o=111111111122222222223333333333444444444455555555556666666666777777777788888888889 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1"

#define OVERFLOWED_SESSION_ID "v=0\n\
o=- 111111111122222222223333333333444444444455555555556666666666777777777788888888889 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1"

#define OVERFLOWED_SESSION_VER "v=0\n\
o=- 11589527637 11111111112222222222333333333 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1"

#define OVERFLOWED_SESSION_NAME "v=0\n\
o=- 11589527637 11111111112222222 IN IP4 vam.vam-cluster.com\n\
s=11111111112222222222333333333344444444445555555555666666666677777777778888888888999999\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1"

#define OVERFLOWED_GROUP "v=0\n\
o=- 11589527637 11111111112222222 IN IP4 vam.vam-cluster.com\n\
s=test\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 11111111112222222222333333333344444444445555555555666666666677777777778888888888999999 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1"

#define OVERFLOWED_MID "v=0\n\
o=- 11589527637 11111111112222222 IN IP4 vam.vam-cluster.com\n\
s=test\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:11111111112222222222333333333344444444445555555555666666666677777777778888888888999999\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;

    memset(data_buffer, 0, DATA_SIZE);
    strncpy(data_buffer, OVERFLOWED_USERNAME, DATA_SIZE);
    data_p = data_buffer;
    sdp_in_p = sdp_init_description(sdp_cfg_p);
    result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
    CU_ASSERT_EQUAL(result, SDP_FIELD_VALUE_OVERFLOW);

    memset(data_buffer, 0, DATA_SIZE);
    strncpy(data_buffer, OVERFLOWED_SESSION_ID, DATA_SIZE);
    data_p = data_buffer;
    sdp_in_p = sdp_init_description(sdp_cfg_p);
    result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
    CU_ASSERT_EQUAL(result, SDP_FIELD_VALUE_OVERFLOW);

    memset(data_buffer, 0, DATA_SIZE);
    strncpy(data_buffer, OVERFLOWED_SESSION_VER, DATA_SIZE);
    data_p = data_buffer;
    sdp_in_p = sdp_init_description(sdp_cfg_p);
    result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
    CU_ASSERT_EQUAL(result, SDP_FIELD_VALUE_OVERFLOW);

    memset(data_buffer, 0, DATA_SIZE);
    strncpy(data_buffer, OVERFLOWED_SESSION_NAME, DATA_SIZE);
    data_p = data_buffer;
    sdp_in_p = sdp_init_description(sdp_cfg_p);
    result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
    CU_ASSERT_EQUAL(result, SDP_FIELD_VALUE_OVERFLOW);

    memset(data_buffer, 0, DATA_SIZE);
    strncpy(data_buffer, OVERFLOWED_GROUP, DATA_SIZE);
    data_p = data_buffer;
    sdp_in_p = sdp_init_description(sdp_cfg_p);
    result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
    CU_ASSERT_EQUAL(result, SDP_FIELD_VALUE_OVERFLOW);

    memset(data_buffer, 0, DATA_SIZE);
    strncpy(data_buffer, OVERFLOWED_MID, DATA_SIZE);
    data_p = data_buffer;
    sdp_in_p = sdp_init_description(sdp_cfg_p);
    result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
    CU_ASSERT_EQUAL(result, SDP_FIELD_VALUE_OVERFLOW);
}


/* Validation test case 18: validate illegal SDP syntax */
void test_channel_valid18 (void)
{
#define ILLEGAL_RTCP "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 INIP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;

    memset(data_buffer, 0, DATA_SIZE);
    strncpy(data_buffer, ILLEGAL_RTCP, DATA_SIZE);
    data_p = data_buffer;
    sdp_in_p = sdp_init_description(sdp_cfg_p);
    result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
    CU_ASSERT_EQUAL(result, SDP_INVALID_RTCP_LINE);
}


/* Validation test case 19: validate overlap ports */
void test_channel_valid19 (void)
{
#define SAME_ORIG_RTP_RTX_RTCP_PORT "v=0\n\
o=- 125699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
a=group:FEC 1 4 5\n\
m=video 50000 RTP/AVPF 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:50001\n\
a=fmtp:99 apt=33\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 45000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=mid:5\n"

#define SAME_ORIG_RTCP_FEC1_RTP_PORT "v=0\n\
o=- 125699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
a=group:FEC 1 4 5\n\
m=video 50000 RTP/AVPF 33\n\
i=Original source stream\n\
c=IN IP4 230.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=fmtp:99 apt=33\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 50001 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=mid:5\n"

#define SAME_ORIG_RTP_FEC1_RTP_PORT "v=0\n\
o=- 125699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
a=group:FEC 1 4 5\n\
m=video 50000 RTP/AVPF 33\n\
i=Original source stream\n\
c=IN IP4 230.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=fmtp:99 apt=33\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=mid:5\n"

#define SAME_ORIG_RTCP_FEC2_RTP_PORT "v=0\n\
o=- 125699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
a=group:FEC 1 4 5\n\
m=video 50000 RTP/AVPF 33\n\
i=Original source stream\n\
c=IN IP4 230.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=fmtp:99 apt=33\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 45000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 50001 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=mid:5\n"

#define SAME_ORIG_RTP_FEC2_RTP_PORT "v=0\n\
o=- 125699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
a=group:FEC 1 4 5\n\
m=video 50000 RTP/AVPF 33\n\
i=Original source stream\n\
c=IN IP4 230.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=fmtp:99 apt=33\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 45000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 50000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=mid:5\n"

#define SAME_FEC1_RTP_FEC2_RTP_PORT "v=0\n\
o=- 125699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
a=group:FEC 1 4 5\n\
m=video 50000 RTP/AVPF 33\n\
i=Original source stream\n\
c=IN IP4 230.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=fmtp:99 apt=33\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 45000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 45000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=mid:5\n"

#define SAME_FEC1_RTP_FEC2_RTCP_PORT "v=0\n\
o=- 125699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
a=group:FEC 1 4 5\n\
m=video 50000 RTP/AVPF 33\n\
i=Original source stream\n\
c=IN IP4 230.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=fmtp:99 apt=33\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 45000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=rtcp:45000\n\
a=mid:5\n"

#define SAME_FEC1_RTCP_FEC2_RTP_PORT "v=0\n\
o=- 125699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
a=group:FEC 1 4 5\n\
m=video 50000 RTP/AVPF 33\n\
i=Original source stream\n\
c=IN IP4 230.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=fmtp:99 apt=33\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 45000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=rtcp:46000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=mid:5\n"

#define SAME_FEC1_RTCP_FEC2_RTCP_PORT "v=0\n\
o=- 125699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
a=group:FEC 1 4 5\n\
m=video 50000 RTP/AVPF 33\n\
i=Original source stream\n\
c=IN IP4 230.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=fmtp:99 apt=33\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n\
m=video 45000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=rtcp:46001\n\
a=mid:4\n\
m=video 45001 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:0\n\
b=RS:0\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 10.10.200.115\n\
a=rtpmap:101 2dparityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"


    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_ORIG_RTP_RTX_RTCP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_ORIG_RTCP_FEC1_RTP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_ORIG_RTP_FEC1_RTP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_ORIG_RTCP_FEC2_RTP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_ORIG_RTP_FEC2_RTP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_FEC1_RTP_FEC2_RTP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_FEC1_RTP_FEC2_RTCP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_FEC1_RTCP_FEC2_RTP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SAME_FEC1_RTCP_FEC2_RTCP_PORT, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);
    }
}


/* Validation test case 20: validate rtcp-xr attributes */
void test_channel_valid20 (void)
{
#define RTCP_XR_NO_TYPES "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"

#define RTCP_XR_LOSS_RLE "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:pkt-loss-rle\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"

#define RTCP_XR_LOSS_RLE_WITH_MAX "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:pkt-loss-rle=50\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"


#define RTCP_XR_FOR_R3_0 "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:pkt-loss-rle stat-summary=loss,dup,jitt\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"


#define RTCP_XR_FOR_R3_0_WITH_MAX "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:pkt-loss-rle=30 stat-summary=loss,dup,jitt\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"

#define RTCP_XR_REVERSED_ORDER "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:stat-summary=loss,dup,jitt pkt-loss-rle=20\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"

#define RTCP_XR_FOR_R3_0_WITH_UNKNOWN "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:pkt-loss-rle=30 pkt-dup-rle stat-summary=loss,dup,jitt\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"

#define RTCP_XR_FOR_R3_0_WITH_POST_ER_LOSS_RLE "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:pkt-loss-rle=30 post-repair-loss-rle stat-summary=loss,dup,jitt\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"

#define RECV_ONLY_WITH_POST_ER_LOSS_RLE "v=0\n\
o=- 115699376 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
m=video 50000 RTP/AVPF 98\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:50001 IN IP4 10.10.200.115\n\
a=rtcp-xr:pkt-loss-rle=30 post-repair-loss-rle stat-summary=loss,dup,jitt\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=inactive\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define SMALL_BW_WITH_POST_ER_LOSS_RLE "v=0\n\
o=- 1156911126 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 1 3\n\
m=video 50000 RTP/AVPF 98\n\
i=Original source stream\n\
c=IN IP4 224.1.1.100/255\n\
b=AS:20000\n\
b=RR:206250\n\
b=RS:68750\n\
a=fmtp:98 rtcp-per-rcvr-bw=12\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.100 1.1.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:50001 IN IP4 10.10.200.115\n\
a=rtcp-xr:pkt-loss-rle=30 post-repair-loss-rle stat-summary=loss,dup,jitt\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=inactive\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"


#define RTCP_XR_FOR_R3_4_WITH_MULTICAST_ACQ "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:pkt-loss-rle=30 post-repair-loss-rle stat-summary=loss,dup,jitt \
multicast-acq\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"

#define RTCP_XR_FOR_R3_4_WITH_DIAGNOSTIC_COUNTERS "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:pkt-loss-rle=30 post-repair-loss-rle stat-summary=loss,dup,jitt \
vqe-diagnostic-counters\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;
    idmgr_id_t handle;
    uint32_t chksum;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_XR_NO_TYPES, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_XR_LOSS_RLE, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_XR_LOSS_RLE_WITH_MAX, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_XR_FOR_R3_0, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_XR_FOR_R3_0_WITH_MAX, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);


        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_XR_REVERSED_ORDER, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_XR_FOR_R3_0_WITH_UNKNOWN, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_XR_FOR_R3_0_WITH_POST_ER_LOSS_RLE, 
                DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_XR_FOR_R3_4_WITH_MULTICAST_ACQ, 
                DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_XR_FOR_R3_4_WITH_DIAGNOSTIC_COUNTERS, 
                DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RECV_ONLY_WITH_POST_ER_LOSS_RLE, 
                DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_NOT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                            CFG_CHANNEL_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, SMALL_BW_WITH_POST_ER_LOSS_RLE, 
                DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        chksum = cfg_db_checksum(data_buffer, strlen(data_buffer));
        CU_ASSERT_EQUAL(cfg_channel_add(sdp_in_p, mgr_p, &handle, chksum),
                        CFG_CHANNEL_SUCCESS);
    }
}


/* 
 * Validate attribute rtcp-rsize for a channel to be used for not sending
 * RRs with generic NACKs
 */
void test_channel_valid21 (void)
{
#define RTCP_RSIZE_ENABLED "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtcp-rsize\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n\
a=mid:5\n"


#define RTCP_RSIZE_NOT_ENABLED "v=0\n\
o=- 115699375 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
a=group:FEC 1 4\n\
m=video 50000 udp 33\n\
i=Original source stream\n\
c=IN IP4 224.1.1.10/255\n\
b=AS:5500\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.10 1.1.200.115\n\
a=rtpmap:33 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.10/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.10 10.10.200.115\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001 IN IP4 10.10.200.115\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=rtcp-xr:\n\
a=mid:2\n\
m=video 46010 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.200.115\n\
b=RR:206250\n\
b=RS:68750\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
m=video 50000 RTP/AVP 100\n\
i=FEC column stream\n\
c=IN IP4 230.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 230.1.1.10 1.1.200.115\n\
a=rtpmap:100 parityfec/90000\n\
a=mid:4\n\
m=video 46000 RTP/AVP 101\n\
i=FEC rowstream\n\
c=IN IP4 231.1.1.10/255\n\
b=RR:206250\n\
b=RS:68750\n\
a=recvonly\n\
a=source-filter: incl IN IP4 231.1.1.10 10.10.200.115\n\
a=rtpmap:101 2parityfec/90000\n\
a=rtcp:46001\n"

    char data_buffer[DATA_SIZE];
    char *data_p;
    void *sdp_in_p;
    sdp_result_e result;
    channel_mgr_t *mgr_p;

    mgr_p = cfg_get_channel_mgr();
    if (mgr_p) {
        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_RSIZE_ENABLED, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);

        memset(data_buffer, 0, DATA_SIZE);
        strncpy(data_buffer, RTCP_RSIZE_NOT_ENABLED, DATA_SIZE);
        data_p = data_buffer;
        sdp_in_p = sdp_init_description(sdp_cfg_p);
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_buffer));
        CU_ASSERT_EQUAL(result, SDP_SUCCESS);
    }
}

void test_channel_copy (void)
{
    cfg_channel_ret_e status;
    channel_cfg_t *src_p = NULL;
    channel_cfg_t *dest_p = NULL;
    channel_mgr_t *mgr_p;

    mgr_p = cfg_get_channel_mgr();
    CU_ASSERT_PTR_NOT_NULL(mgr_p);

    if (mgr_p) {
        src_p = cfg_channel_get(mgr_p, mgr_p->handles[0]);
        if (src_p) {
            status = cfg_channel_copy(dest_p, src_p);
            CU_ASSERT_EQUAL(status, CFG_CHANNEL_INVALID_CHANNEL);

            dest_p = (channel_cfg_t *) malloc(sizeof(channel_cfg_t));
            if (dest_p) {
                memset(dest_p, 0, sizeof(channel_cfg_t));
                status = cfg_channel_copy(dest_p, src_p);
                CU_ASSERT_EQUAL(status, CFG_CHANNEL_SUCCESS);

                free(dest_p);
            }
        }
    }
}


void test_channel_compare (void)
{
    cfg_channel_ret_e status;
    channel_cfg_t *src_p = NULL;
    channel_cfg_t *dest_p = NULL;
    channel_mgr_t *mgr_p;

    mgr_p = cfg_get_channel_mgr();
    CU_ASSERT_PTR_NOT_NULL(mgr_p);

    if (mgr_p) {
        src_p = cfg_channel_get(mgr_p, mgr_p->handles[0]);
        if (src_p) {
            dest_p = (channel_cfg_t *) malloc(sizeof(channel_cfg_t));
            if (dest_p) {
                memset(dest_p, 0, sizeof(channel_cfg_t));
                status = cfg_channel_copy(dest_p, src_p);
                CU_ASSERT_EQUAL(status, CFG_CHANNEL_SUCCESS);

                CU_ASSERT_EQUAL(cfg_channel_compare(
                                    dest_p,
                                    cfg_channel_get(mgr_p, mgr_p->handles[0])),
                                TRUE);

                CU_ASSERT_EQUAL(cfg_channel_compare(
                                    dest_p,
                                    cfg_channel_get(mgr_p, mgr_p->handles[1])),
                                FALSE);

                free(dest_p);
            }
        }
    }
}


void test_channel_get (void)
{
    channel_cfg_t *src_p = NULL;
    channel_cfg_t *dest_p = NULL;
    channel_mgr_t *mgr_p;

    mgr_p = cfg_get_channel_mgr();
    CU_ASSERT_PTR_NOT_NULL(mgr_p);

    if (mgr_p) {
        /* Test case for getting channel configuration data based on index */
        src_p = cfg_channel_get(mgr_p, mgr_p->handles[0]);
        dest_p = cfg_get_channel_cfg_from_idx(0);
        CU_ASSERT_EQUAL(src_p, dest_p);

        /* Test case for getting channel configuration data based on handle */
        if (src_p) {
            dest_p = NULL;
            dest_p = cfg_get_channel_cfg_from_hdl(src_p->handle);
            CU_ASSERT_EQUAL(src_p, dest_p);

            /* Test case for getting channel configuration data */
            /* based on source addr and port */
            dest_p = NULL;
            dest_p = cfg_get_channel_cfg_from_orig_src_addr(
                src_p->original_source_addr,
                src_p->original_source_port);
            CU_ASSERT_EQUAL(src_p, dest_p);
        }
    }
}

void test_parse_channels (void)
{
#define NO_CART_RETURN_SDP "v=0\n\
o=- 4262738225 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
b=RS:68750\n\
b=RR:206250\n\
a=fmtp:96 rtcp-per-rcvr-bw=55\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3"

    cfg_ret_e status;
    int total_channels;
    char *sdp_buffer;
    char *empty_buffer;

    total_channels = cfg_get_total_num_channels();
    sdp_buffer = malloc(total_channels * MAX_SESSION_SIZE);
    empty_buffer = malloc(MAX_SESSION_SIZE);

    status = cfg_get_all_channel_data(sdp_buffer,
                                      total_channels*MAX_SESSION_SIZE);
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    /* Test the empty channel content */
    status = cfg_parse_all_channel_data(NULL);
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    status = cfg_commit_update();
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    CU_ASSERT_EQUAL(0, cfg_get_total_num_channels());

    status = cfg_parse_all_channel_data(empty_buffer);
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    status = cfg_parse_all_channel_data(NO_CART_RETURN_SDP);
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    /* Test the new channel content */
    status = cfg_parse_all_channel_data(sdp_buffer);
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    CU_ASSERT_EQUAL(0, cfg_get_total_num_channels());

    status = cfg_commit_update();
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    CU_ASSERT_EQUAL(total_channels,
                    cfg_get_total_num_channels());

    /* Test another parsing */
    status = cfg_parse_all_channel_data(NULL);
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    CU_ASSERT_EQUAL(total_channels,
                    cfg_get_total_num_channels());

    status = cfg_commit_update();
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    CU_ASSERT_EQUAL(0, cfg_get_total_num_channels());

    /* Test the new channel content */
    status = cfg_parse_all_channel_data(sdp_buffer);
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    CU_ASSERT_EQUAL(0, cfg_get_total_num_channels());

    status = cfg_commit_update();
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    CU_ASSERT_EQUAL(total_channels,
                    cfg_get_total_num_channels());

    if (sdp_buffer) {
        free(sdp_buffer);
    }

    if (empty_buffer) {
        free(empty_buffer);
    }
}


void test_parse_single_channel (void)
{
#define LINEAR_SDP "v=0\n\
o=- 4262738225 1158953264 IN IP4 vam.vam-cluster.com\n\
s=Channel 1\n\
i=Channel configuration for Channel 1\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
a=group:FID 2 3\n\
m=video 50000 udp 96\n\
i=Original source stream\n\
c=IN IP4 224.1.1.1/255\n\
b=AS:5500\n\
b=RS:68750\n\
b=RR:206250\n\
a=fmtp:96 rtcp-per-rcvr-bw=55\n\
a=recvonly\n\
a=source-filter: incl IN IP4 224.1.1.1 192.168.1.1\n\
a=rtpmap:96 MP2T/90000\n\
a=mid:1\n\
m=video 45000 RTP/AVPF 98\n\
i=Re-sourced stream\n\
c=IN IP4 232.1.1.1/255\n\
a=sendonly\n\
a=source-filter: incl IN IP4 232.1.1.1 10.10.20.114\n\
a=rtpmap:98 MP2T/90000\n\
a=rtcp:45001\n\
a=rtcp-fb:98 nack\n\
a=rtcp-fb:98 nack pli\n\
a=mid:2\n\
m=video 46000 RTP/AVPF 99\n\
i=Unicast retransmission stream\n\
c=IN IP4 10.10.20.114\n\
b=RS:68750\n\
b=RR:206250\n\
a=sendonly\n\
a=rtpmap:99 rtx/90000\n\
a=rtcp:46001\n\
a=fmtp:99 apt=98\n\
a=fmtp:99 rtx-time=3000\n\
a=mid:3\n"

#define VOD_SDP "v=0\n\
o=- 4262738225 1158953264 IN IP4 vam.vam-cluster.com\n\
s=VoD 1\n\
i=VoD Session Description\n\
t=0 0\n\
a=group:FID 1 2\n\
m=video 0 RTP/AVPF 96\n\
i=Primary stream\n\
c=IN IP4 0.0.0.0\n\
b=AS:14910\n\
b=RS:55\n\
b=RR:55\n\
a=rtpmap:96 MP2T/90000\n\
a=rtcp-fb:96 nack\n\
a=rtcp-xr:pkt-loss-rle per-loss-rle stat-summary=loss,dupp,jitt\n\
a=mid:1\n\
m=video 0 RTP/AVPF 99\n\
i=Retransmission stream\n\
c=IN IP4 0.0.0.0\n\
b=RS:55\n\
b=RR:55\n\
a=rtpmap:99 rtx/90000\n\
a=fmtp:99 apt=96\n\
a=mid:2\n"

    cfg_ret_e status;
    channel_cfg_t chan_cfg;

    /* Test NULL and empty arguments */
    status = cfg_parse_single_channel_data(NULL, NULL, CFG_LINEAR);
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    status = cfg_parse_single_channel_data("", NULL, CFG_LINEAR);
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    status = cfg_parse_single_channel_data("  ", NULL, CFG_LINEAR);
    CU_ASSERT_NOT_EQUAL(CFG_SUCCESS, status);

    status = cfg_parse_single_channel_data("  ", &chan_cfg, CFG_LINEAR);
    CU_ASSERT_NOT_EQUAL(CFG_SUCCESS, status);

    status = cfg_parse_single_channel_data(LINEAR_SDP, NULL, CFG_LINEAR);
    CU_ASSERT_NOT_EQUAL(CFG_SUCCESS, status);

    /* Test linear extraction */
    status = cfg_parse_single_channel_data(LINEAR_SDP, &chan_cfg, CFG_LINEAR);
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    CU_ASSERT(strcmp("Channel 1", chan_cfg.name) == 0);
    CU_ASSERT(SOURCE_MODE == chan_cfg.mode);
    CU_ASSERT(UDP_STREAM == chan_cfg.source_proto);
    CU_ASSERT(inet_addr("224.1.1.1") == chan_cfg.original_source_addr.s_addr);
    CU_ASSERT(inet_addr("192.168.1.1") == 
        chan_cfg.src_addr_for_original_source.s_addr);
    CU_ASSERT(htons(50000) == chan_cfg.original_source_port);
    CU_ASSERT(96 == chan_cfg.original_source_payload_type);
    CU_ASSERT(inet_addr("232.1.1.1") == chan_cfg.re_sourced_addr.s_addr);
    CU_ASSERT(htons(45000) == chan_cfg.re_sourced_rtp_port);
    CU_ASSERT(htons(45001) == chan_cfg.re_sourced_rtcp_port);
    CU_ASSERT(98 == chan_cfg.re_sourced_payload_type);
    CU_ASSERT(inet_addr("10.10.20.114") == chan_cfg.fbt_address.s_addr);
    CU_ASSERT(inet_addr("10.10.20.114") == chan_cfg.rtx_addr.s_addr);
    CU_ASSERT(htons(46000) == chan_cfg.rtx_rtp_port);
    CU_ASSERT(htons(46001) == chan_cfg.rtx_rtcp_port);
    CU_ASSERT(99 == chan_cfg.rtx_payload_type);
    CU_ASSERT(98 == chan_cfg.rtx_apt);
    CU_ASSERT(3000 == chan_cfg.rtx_time);
    CU_ASSERT(chan_cfg.er_enable);
    CU_ASSERT(chan_cfg.fcc_enable);
    CU_ASSERT(RSI == chan_cfg.rtcp_fb_mode);
    CU_ASSERT(68750 == chan_cfg.repair_rtcp_sndr_bw);
    CU_ASSERT(206250 == chan_cfg.repair_rtcp_rcvr_bw);

    /* Test vod extraction */
    status = cfg_parse_single_channel_data(VOD_SDP, &chan_cfg, CFG_VOD);
    CU_ASSERT_EQUAL(CFG_SUCCESS, status);

    CU_ASSERT(strcmp("VoD 1", chan_cfg.name) == 0);
    CU_ASSERT(LOOKASIDE_MODE == chan_cfg.mode);
    CU_ASSERT(RTP_STREAM == chan_cfg.source_proto);
    CU_ASSERT(0 == chan_cfg.original_source_addr.s_addr);
    CU_ASSERT(0 == chan_cfg.src_addr_for_original_source.s_addr);
    CU_ASSERT(0  == chan_cfg.original_source_port);
    CU_ASSERT(96 == chan_cfg.original_source_payload_type);
    CU_ASSERT(14910000 == chan_cfg.bit_rate);
    CU_ASSERT(55 == chan_cfg.original_rtcp_sndr_bw);
    CU_ASSERT(55 == chan_cfg.original_rtcp_rcvr_bw);
    CU_ASSERT(0 == chan_cfg.fbt_address.s_addr);
    CU_ASSERT(0 == chan_cfg.rtx_addr.s_addr);
    CU_ASSERT(0 == chan_cfg.rtx_rtp_port);
    CU_ASSERT(0 == chan_cfg.rtx_rtcp_port);
    CU_ASSERT(99 == chan_cfg.rtx_payload_type);
    CU_ASSERT(96 == chan_cfg.rtx_apt);
    CU_ASSERT(chan_cfg.er_enable);
    CU_ASSERT(!chan_cfg.fcc_enable);
    CU_ASSERT(55 == chan_cfg.repair_rtcp_sndr_bw);
    CU_ASSERT(55 == chan_cfg.repair_rtcp_rcvr_bw);
}

/***********************************************************************
 *
 *     DATABASE SUITE TESTS START HERE
 *
 **********************************************************************/
static uint32_t cfg_session_key_hash_func (uint8_t *key,
                                           int32_t len,
                                           int32_t mask)
{
    int i, acc = 0;
    char *p = (char *)key;
   
    for (i = 0; i < len; i++) {
        acc += *p;
        p++;
        while (acc > 255) {
            acc = (acc & 255) + (acc >> 8);
        }
    }

    return (acc % 256);
}

int database_testsuite_init (void)
{
    channel_manager.handle_mgr = id_create_new_table(100, 100);
    channel_manager.session_keys = 
        vqe_hash_create(256,
                        cfg_session_key_hash_func,
                        NULL);
    return 0;
}


int database_testsuite_cleanup (void)
{
    /* Delete the ID manager */
    id_destroy_table(channel_manager.handle_mgr);

    return 0;
}

void test_db_open (void)
{
    cfg_db_ret_e status;
    char bogus_name[] = "bogus.cfg";
    cfg_database_t db;
    
    /* Make sure to remove the backup first */
    /* sa_ignore IGNORE_RETURN */
    remove(BACKUP_DB_NAME);

    /* Open a bogus database */
    status = cfg_db_open(&db, bogus_name, DB_FILE);
    CU_ASSERT_EQUAL(status, CFG_DB_OPEN_FAILED);
    CU_ASSERT_PTR_NULL(db.db_file_p);

    /* Open a test database */
    status = cfg_db_open(&db, TEST_DB, DB_FILE);
    CU_ASSERT_EQUAL(status, CFG_DB_SUCCESS);
    CU_ASSERT_PTR_NOT_NULL(db.db_file_p);

    fclose(db.db_file_p);
}


void test_db_close (void)
{
    cfg_db_ret_e status;
    cfg_database_t db;

    /* Open a test database */
    status = cfg_db_open(&db, TEST_DB, DB_FILE);
    CU_ASSERT_EQUAL(status, CFG_DB_SUCCESS);

    /* Close the test database */
    status = cfg_db_close(&db);
    CU_ASSERT_EQUAL(status, CFG_DB_SUCCESS);
    CU_ASSERT_PTR_NULL(db.db_file_p);
}


void test_db_read (void)
{
    cfg_db_ret_e status;
    cfg_database_t db;
    int i;

    /* Open a test database */
    status = cfg_db_open(&db, TEST_DB, DB_FILE);
    CU_ASSERT_EQUAL(status, CFG_DB_SUCCESS);

    status = cfg_db_read_channels(&db, &channel_manager);
    CU_ASSERT_EQUAL(status, CFG_DB_SUCCESS);
    CU_ASSERT_EQUAL(channel_manager.total_num_channels,TOTAL_CHANNELS);
    for (i=0; i < TOTAL_CHANNELS; i++) {
        CU_ASSERT_PTR_NOT_NULL(cfg_channel_get(&channel_manager,
                                               channel_manager.handles[i]));
    }

    /* Close the test database */
    status = cfg_db_close(&db);
    CU_ASSERT_EQUAL(status, CFG_DB_SUCCESS);
}


void test_db_write (void)
{
    cfg_db_ret_e status;
    cfg_database_t db;

    /* Open a test database */
    /* sa_ignore IGNORE_RETURN */
    system("cp vam.cfg vam4utest.cfg");
    status = cfg_db_open(&db, "vam4utest.cfg", DB_FILE);
    CU_ASSERT_EQUAL(status, CFG_DB_SUCCESS);

    status = cfg_db_write_channels(&db, &channel_manager);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    /* Check whether the backup file has been made or not */
    CU_ASSERT_EQUAL(access(BACKUP_DB_NAME, F_OK), 0);

    /* Close the test database */
    status = cfg_db_close(&db);
    CU_ASSERT_EQUAL(status, CFG_DB_SUCCESS);
}


/***********************************************************************
 *
 *     CONFIGURATION MANAGER SUITE TESTS START HERE
 *
 **********************************************************************/
int cfgmgr_testsuite_init (void)
{
    return 0;
}


int cfgmgr_testsuite_cleanup (void)
{
    return 0;
}

void test_cfgmgr_init (void)
{
    cfg_ret_e status;
    channel_mgr_t *mgr_p;
    FILE *fp;
    int file_size;
    char *sdp_buffer = NULL;

    mgr_p = cfg_get_channel_mgr();
    CU_ASSERT_PTR_NULL(mgr_p);

    status = cfg_init(TEST_DB);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    CU_ASSERT_EQUAL(cfg_get_total_num_channels(),
                    cfg_get_total_num_input_channels());

    mgr_p = cfg_get_channel_mgr();
    CU_ASSERT_PTR_NOT_NULL(mgr_p);

    if (mgr_p) {
        CU_ASSERT_EQUAL(mgr_p->total_num_channels,
                        cfg_get_total_num_channels());

        status = cfg_init(TEST_DB);
        CU_ASSERT_EQUAL(status, CFG_ALREADY_INIT);

        status = cfg_shutdown();
        CU_ASSERT_EQUAL(status, CFG_SUCCESS);

        status = cfg_init(NULL);
        CU_ASSERT_EQUAL(status, CFG_SUCCESS);

        status = cfg_shutdown();
        CU_ASSERT_EQUAL(status, CFG_SUCCESS);
    }

    /* Open an DOS-formatted SDP file */
    status = cfg_init(DB_IN_DOS_FMT);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    CU_ASSERT_EQUAL(cfg_get_total_num_channels(),
                    cfg_get_total_num_input_channels());

    /* Another test by reading in the entire DOS-formatted SDP file */
    fp = fopen(DB_IN_DOS_FMT, "read");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        file_size = ftell(fp);
        rewind (fp);

        sdp_buffer = malloc(file_size);
        if (sdp_buffer) {
            memset(sdp_buffer, 0, file_size);

            CU_ASSERT_EQUAL(fread(sdp_buffer, 1, file_size, fp), file_size);

            status = cfg_parse_all_channel_data(sdp_buffer);
            CU_ASSERT_EQUAL(CFG_SUCCESS, status);

            status = cfg_commit_update();
            CU_ASSERT_EQUAL(CFG_SUCCESS, status);

            CU_ASSERT_EQUAL(2, cfg_get_total_num_channels());

            free(sdp_buffer);
        }

        fclose(fp);
    }

    status = cfg_shutdown();
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);
}


void test_cfgmgr_shutdown (void)
{
    cfg_ret_e status;
    channel_mgr_t *mgr_p;

    status = cfg_init(TEST_DB);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    status = cfg_shutdown();
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    mgr_p = cfg_get_channel_mgr();
    CU_ASSERT_PTR_NULL(mgr_p);

    status = cfg_shutdown();
    CU_ASSERT_EQUAL(status, CFG_UNINITIALIZED);
}


void test_cfgmgr_update (void)
{
    cfg_ret_e status;
    char time_buffer[DATA_SIZE];
    char stats_buffer[DATA_SIZE];
    channel_cfg_t *channel_p;
    channel_cfg_t *new_channel_p;

    /* sa_ignore IGNORE_RETURN */
    system("cp update.cfg new.cfg");

    status = cfg_init(NEW_DB);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    status = cfg_update(NEW_DB);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    /* sa_ignore IGNORE_RETURN */
    cfg_get_update_stats(stats_buffer, DATA_SIZE);
    printf("\nUpdate = %s\n", stats_buffer);

    /* sa_ignore IGNORE_RETURN */
    cfg_get_timestamp(time_buffer, DATA_SIZE);
    printf("Time = %s\n", time_buffer);

    channel_p = cfg_get_channel_cfg_from_idx(0);
    if (channel_p) {
        new_channel_p = cfg_find_in_update(channel_p);
        CU_ASSERT_NOT_EQUAL(NULL, new_channel_p);

        CU_ASSERT_EQUAL(FALSE, cfg_has_changed(channel_p, new_channel_p));

        cfg_remove_channel(channel_p);

        CU_ASSERT_EQUAL(NULL, cfg_get_channel_cfg_from_idx(0));

        channel_p = cfg_insert_channel(new_channel_p);
        CU_ASSERT_EQUAL(channel_p, cfg_get_channel_cfg_from_idx(0));
    }

    cfg_cleanup_update();

    status = cfg_shutdown();
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);
}


void test_cfgmgr_access (void)
{
    cfg_ret_e status;
    char o_line[MAX_LINE_LENGTH];

    status = cfg_init(TEST_DB);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    idmgr_id_t *channel_cfg_hdl_p = NULL;
    uint16_t total_channels;

    status = cfg_get_all_channels(&channel_cfg_hdl_p, &total_channels);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);
    CU_ASSERT_EQUAL(total_channels,
                    cfg_get_total_num_channels());

    uint32_t parsed;
    uint32_t validated;
    uint32_t total;
    status = cfg_get_cfg_stats(&parsed, &validated, &total);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);
    CU_ASSERT_EQUAL(total_channels, parsed);
    CU_ASSERT_EQUAL(total_channels, validated);

    uint32_t gop_size = 60;
    uint8_t burst_rate = 20;

    status = cfg_set_system_params(gop_size, burst_rate);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    uint32_t temp1;
    uint8_t temp2;

    status = cfg_get_system_params(&temp1, &temp2);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);
    CU_ASSERT_EQUAL(temp1, gop_size);
    CU_ASSERT_EQUAL(temp2, burst_rate);

    channel_cfg_t *p_channel1, *p_channel2;
    p_channel1 = cfg_get_channel_cfg_from_idx(1);
    if (p_channel1) {
        // o=- 1173459094 1173459092 IN IP4 vam-cluster.cisco.com
        p_channel2 = cfg_get_channel_cfg_from_origin("-",
                                                     "1173459094",
                                                     "IN",
                                                     "IP4",
                                                     "vam-cluster.cisco.com");

        CU_ASSERT_EQUAL(p_channel1, p_channel2);

        p_channel2 = cfg_get_channel_cfg_from_origin("bogus_name",
                                                     "1234567890",
                                                     "IN",
                                                     "IP4",
                                                     "192.168.1.1");

        CU_ASSERT_EQUAL(NULL, p_channel2);

        memset(o_line, 0, MAX_LINE_LENGTH);
        snprintf(o_line, MAX_LINE_LENGTH, "%s",
                 "o=-   1173459094 1173459092 IN IP4 vam-cluster.cisco.com");

        p_channel2 = cfg_get_channel_cfg_from_SDP_hdl(o_line);
        CU_ASSERT_EQUAL(p_channel1, p_channel2);

        snprintf(o_line, MAX_LINE_LENGTH, "%s",
                 "o=- 1173459092 1173459094 IN IP4 vam-cluster.cisco.com");

        p_channel2 = cfg_get_channel_cfg_from_SDP_hdl(o_line);
        CU_ASSERT_EQUAL(NULL, p_channel2);

        snprintf(o_line, MAX_LINE_LENGTH, "o=%s %s %s IN IP4 %s",
                 "bogus",
                 "1234567890",
                 "1234567890",
                 "does not exist");

        p_channel2 = cfg_get_channel_cfg_from_SDP_hdl(o_line);
        CU_ASSERT_EQUAL(NULL, p_channel2);

        snprintf(o_line, MAX_LINE_LENGTH, "o=%s %s IN IP4 %s",
                 "bogus",
                 "1234567890",
                 "does-not-exist");

        p_channel2 = cfg_get_channel_cfg_from_SDP_hdl(o_line);
        CU_ASSERT_EQUAL(NULL, p_channel2);

        snprintf(o_line, MAX_LINE_LENGTH, "o=%s %s %s IN IP6 %s",
                 "b",
                 "1",
                 "2",
                 "d");

        p_channel2 = cfg_get_channel_cfg_from_SDP_hdl(o_line);
        CU_ASSERT_EQUAL(NULL, p_channel2);

        snprintf(o_line, MAX_LINE_LENGTH, "o=%s %s %s IN IP4 %s",
                 "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                 "11111111111111111111111111111111111111111111111111"
                 "22222222222222222222222222222222222222222222222222"
                 "3333333333333333333333333333333333333333333333333333"
                 "4444444444444444444444444444444444444444444444444444"
                 "55555555555555555555555555555555555555555555555555555555",
                 "11111111111111111111111111111111111111111111111111"
                 "22222222222222222222222222222222222222222222222222"
                 "3333333333333333333333333333333333333333333333333333"
                 "4444444444444444444444444444444444444444444444444444"
                 "55555555555555555555555555555555555555555555555555555555",
                 "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

        p_channel2 = cfg_get_channel_cfg_from_SDP_hdl(o_line);
        CU_ASSERT_EQUAL(NULL, p_channel2);

        channel_cfg_sdp_handle_t handle;
        handle = cfg_alloc_SDP_handle_from_orig_src_addr(
            p_channel1->source_proto,
            p_channel1->original_source_addr,
            p_channel1->original_source_port);

        p_channel2 = cfg_get_channel_cfg_from_SDP_hdl(handle);
        CU_ASSERT_EQUAL(p_channel1, p_channel2);

        channel_cfg_sdp_handle_t cloned_handle;
        cloned_handle = cfg_clone_SDP_handle(handle);
        p_channel2 = cfg_get_channel_cfg_from_SDP_hdl(cloned_handle);
        CU_ASSERT_EQUAL(p_channel1, p_channel2);

        CU_ASSERT_EQUAL(TRUE, 
                        cfg_SDP_handle_equivalent(handle, cloned_handle));
        
        cfg_free_SDP_handle(handle);
        cfg_free_SDP_handle(cloned_handle);
    }

    status = cfg_shutdown();
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);
}


void test_cfgmgr_save (void)
{
    cfg_ret_e status;

    status = cfg_init(TEST_DB);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    CU_ASSERT_EQUAL(cfg_is_savable(), TRUE);

    status = cfg_shutdown();
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    status = cfg_init(NEW_DB);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    /* sa_ignore IGNORE_RETURN */
    system("chmod -w new.cfg");

    CU_ASSERT_EQUAL(cfg_is_savable(), FALSE);

    /* sa_ignore IGNORE_RETURN */
    system("chmod +w new.cfg");

    status = cfg_shutdown();
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);
}


void test_cfgmgr_removeFBTs (void)
{
    cfg_ret_e status;

    status = cfg_init(NEW_DB);
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

    cfg_remove_redundant_FBTs();

    status = cfg_shutdown();
    CU_ASSERT_EQUAL(status, CFG_SUCCESS);

}
