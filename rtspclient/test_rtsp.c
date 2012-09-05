/*
 *------------------------------------------------------------------
 * RTSP Client Test Program
 *
 * July 2006, Dong Hsu
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

/* System Includes */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <getopt.h>

/* Application Includes */
#include <utils/vam_debug.h>
#include "rtsp_client.h"
#include "vqec_updater.h"

/*------------------------------------------------------------------------*/
/* Among all the C files including it, only ONE C file (say the one where
 * main() is defined) should instantiate the message definitions described in
 * XXX_syslog_def.h.   To do so, the designated C file should define the symbol
 * SYSLOG_DEFINITION before including the file.
 */
#define SYSLOG_DEFINITION
#include <log/vqe_rtsp_syslog_def.h>
#undef SYSLOG_DEFINITION

/* Include base definitions: boolean flags, enum codes */
#include <log/vqe_rtsp_debug.h>
/* Declare the debug array */
#define __DECLARE_DEBUG_ARR__
#include <log/vqe_rtsp_debug_flags.h>
/*------------------------------------------------------------------------*/

#define NAME_SIZE 100

const char *applicationUsage = "rtsp_server_name rtsp_server_port loglevel request_name";

int main (int argc, char *argv[])
{
    char serverHost[NAME_SIZE];
    int  serverPort;
    int loglevel;

    if (argc < 5) {
        printf( "\nUsage: %s %s\n", argv[0], applicationUsage);
        exit(-1);
    }

    strncpy(serverHost, argv[1], NAME_SIZE);
    serverPort = atoi(argv[2]);
    loglevel = atoi(argv[3]);

    /* Initialize the logging facility */
    syslog_facility_open(LOG_RTSP, LOG_CONS);
    syslog_facility_filter_set(LOG_RTSP, LOG_INFO);

    /* Initialize a rtsp client */
    if (rtsp_client_init(serverHost, serverPort) != RTSP_SUCCESS) {
        printf("Error:: rtsp client initialization failed!\n");
        exit(-1);
    }

    char name[NAME_SIZE];
    strncpy(name, argv[4], NAME_SIZE);

    syslog_print(RTSP_CHANNEL_UPDATE_NOTICE);

    if (rtsp_send_request(
            name, MEDIA_TYPE_APP_SDP,
            TIME_ADD_A_R(get_sys_time(), VQEC_UPDATER_TIMEOUT_BACKGROUND))
        != RTSP_SUCCESS) {
        printf("Failed to send out the request!\n");
        exit(-1);
    }

    if (rtsp_get_response_code() == MSG_200_OK) {
        printf("\n\nResponse body = \n%s\n",
               rtsp_get_response_body());
    }

    if (rtsp_client_close() != RTSP_SUCCESS) {
        printf("Error:: rtsp client close failed!\n");
        exit(-1);
    }

    exit(0);
}

