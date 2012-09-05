/********************************************************************
 * ex_syslog.c
 *
 * Derived from DC-OS file: platform/storage/common/syslogd/test/ex_syslog.c
 *
 * This file contains source code that describes how to use the 
 * Syslog Filters to pre-filter syslog messages from Application ex_debug.
 *
 * Copyright (c) 2006-2007 by Cisco Systems, Inc.
 * All rights reserved.
 *********************************************************************/

#include <errno.h>
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <netinet/in.h>

#define SYSLOG_DEFINITION
#include <log/ex_syslog_def.h>
#include <log/vqe_rtp_syslog_def.h>
//#include <log/vqe_rtsp_syslog_def.h>
//#include <log/vqes_cp_syslog_def.h>
//#include <log/vqes_dp_syslog_def.h>
//#include <log/vqes_mp_syslog_def.h>
#include "../../include/log/libdebug.h"

#undef SYSLOG_DEFINITION

#define tprintf if (trace_flag) printf


int main(int argc, char *argv[])
{
    char *prog_name;
    uint8_t prio_filter, default_prio_filter;
    int ret, trace_flag = 0, count = 0;

    prog_name = argv[0];

    if (argc > 2) {
        printf("Usage :: %s -d\n", prog_name);
        exit(0);
    }

    if ((argc == 2) && (ret = strcmp(argv[1], "-d")) == 0) {
        trace_flag = 1;
    }

    /* openlog in your init() routine */
    syslog_facility_open(LOG_EX_SYSLOG, LOG_CONS);

    /* Setting facility priority should be done from CLI/XML-RPC */
    prio_filter = LOG_ERR;
    syslog_facility_filter_set(LOG_EX_SYSLOG, prio_filter);
    tprintf("Syslog facility filter set to %d\n", prio_filter);

    syslog_facility_filter_get(LOG_EX_SYSLOG, prio_filter);
    tprintf("Got syslog facility filter = %d\n", prio_filter);

    syslog_facility_default_filter_get(LOG_EX_SYSLOG, default_prio_filter);
    tprintf("Got the default syslog facility filter = %d\n", 
            default_prio_filter);

    while (count < 4) {
        //syslog_print(EMER_MSG, count);

        syslog_print(ALERT_MSG, "test alert message", count);
        syslog_print(CRIT_MSG, count);
        syslog_print(ERR_MSG, count);
        syslog_print(WARNING_MSG, count);
        syslog_print(NOTICE_MSG, count);
        syslog_print(INFO_MSG, count);
        syslog_print(DEBUG_MSG, count);
        
        count++;
    }

    /* Log RTP/RTCP library messages */
    syslog_print(RTCP_MEM_INIT_FAILURE, "ERA-LA-MB");
    syslog_print(RTCP_LOCAL_SSRC_UPDATE_FAILURE, 
                 "ERA-LA", "1.2.3.4", 1234, 0x12341234, 0x56785678);

    /* Test logging with a different facility */
    vqes_syslog(LOG_MAKEPRI(LOG_EX_SYSLOG, LOG_CRIT), 
           "Log with LOG_MAKEPRI(): LOG_EX_SYSLOG,LOG_CRIT");
    vqes_syslog(LOG_MAKEPRI(LOG_USER, LOG_CRIT), 
           "Log with LOG_MAKEPRI(): LOG_USER,LOG_CRIT");

    vqes_syslog(LOG_DEBUG, "Log with syslog() directly\n");

    /* Close log in your close() routine */
    syslog_facility_close();

    return 0;
}

