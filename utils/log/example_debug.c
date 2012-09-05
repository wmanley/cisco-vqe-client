/********************************************************************
 * example_debug.c
 *
 * This file contains source code that describes how to use
 * the simple VQE Debug Library.
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

#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/fcntl.h>

#include "example_debug.h"
#include <log/syslog_macros.h>

#define SYSLOG_DEFINITION
#include <log/ex_syslog_def.h>
#undef SYSLOG_DEFINITION

#define __DECLARE_DEBUG_ARR__
#include "example_debug_flags.h"


#define tprintf if (trace_flag) printf

int trace_flag = 0;
#define EX_DEBUG_FLAG_NUM  4
debug_filter_item_t filter_arr[EX_DEBUG_FLAG_NUM], bogus_fil[EX_DEBUG_FLAG_NUM];

#define TEST_FD_NUM  5

int main(int argc, char *argv[])
{
    int32_t ret, count = 0;

    if (argc > 2) {
        printf("Usage :: %s -d\n", argv[0]);
        exit(0);
    }

    if ((argc == 2) && (ret = strcmp(argv[1], "-d")) == 0) {
        trace_flag = 1;
    }

    /* create some array of FDs to log messages to */
    int mainfd[TEST_FD_NUM];
    int mnum_fds;

    int file1 = open("file1.txt", O_WRONLY | O_CREAT);
    int file2 = open("file2.txt", O_WRONLY | O_CREAT);
    mainfd[0] = 1;  //stdout
    mainfd[1] = 2;  //stderr
    mainfd[2] = 0;  //stdin
    mainfd[3] = file1;
    mainfd[4] = file2;
    mnum_fds = TEST_FD_NUM;

    /* First thing is to openlog */

    /* use for the default syslogd/stderr message target */
    //syslog_facility_open(LOG_EX_SYSLOG, LOG_CONS);

    /* use to pass an array of FDs to send the messages to */
    syslog_facility_open_fds(LOG_EX_SYSLOG, LOG_CONS, mainfd, mnum_fds);
    
    /* Set the the facility prioity level */
    uint8_t prio_filter = LOG_DEBUG;
    syslog_facility_filter_set(LOG_EX_SYSLOG, prio_filter);
    tprintf("Syslog facility filter set to %d\n", prio_filter);

    syslog_facility_filter_get(LOG_EX_SYSLOG, prio_filter);
    tprintf("Got syslog facility filter = %d\n", prio_filter);

    syslog_facility_default_filter_get(LOG_EX_SYSLOG, prio_filter);
    tprintf("Got the default syslog facility filter = %d\n", 
            prio_filter);

    count = 1;

    vqes_syslog(LOG_DEBUG, "vqes_syslog: %s: yyy Log with syslog() directly\n", __FILE__);

    /* Turn on the EX_DEBUG_ERRORS flag */
    EX_DEBUG_SET_DEBUG_FLAG(EX_DEBUG_ERRORS);
    EX_DEBUG_DEBUG(EX_DEBUG_ERRORS, "Control Plane Error Repair Test Message %d\n", count++);

    /* Turn off the EX_DEBUG_ERRORS flag */
    EX_DEBUG_RESET_DEBUG_FLAG(EX_DEBUG_ERRORS);
    EX_DEBUG_DEBUG(EX_DEBUG_ERRORS,
                  "Should not print as the flag is off: %d\n", count++);

    /*************************************************************/
    /* Turn on the EX_DEBUG_ERRORS flag */
    int32_t elem = EX_DEBUG_ERRORS;
    debug_set_flag(ex_debug_arr, elem, "ex_debug", NULL);

    buginf(debug_check_element(ex_debug_arr, EX_DEBUG_ERRORS, 
                               NULL),
           //&filter_arr[EX_DEBUG_ERRORS]),
           "ex_debug", FALSE, "Errors Test Message %d\n", count++);

    /* Turn off the EX_DEBUG_ERRORS flag */
    debug_reset_flag(ex_debug_arr, elem, "ex_debug", NULL);

    buginf(debug_check_element(ex_debug_arr, EX_DEBUG_ERRORS, 
                               NULL),
           //&bogus_fil[EX_DEBUG_ERRORS]),
           "ex_debug", FALSE, 
           "Should not print as the flag is off: %d\n", count++);

    /*************************************************************/
    /* Turn on the EX_DEBUG_EVENTS flag */
    elem = EX_DEBUG_EVENTS;
    debug_set_flag(ex_debug_arr, elem, "ex_debug", NULL);
    buginf(debug_check_element(ex_debug_arr, EX_DEBUG_EVENTS, 
                               //&filter_arr[EX_DEBUG_EVENTS]),
                               NULL),
           "ex_debug", FALSE, "Events Test Message %d\n", count++);
    
    /* Turn off the EX_DEBUG_EVENTS flag */
    debug_reset_flag(ex_debug_arr, elem, "ex_debug", NULL);
    buginf(debug_check_element(ex_debug_arr, EX_DEBUG_EVENTS, 
                               //&filter_arr[EX_DEBUG_EVENTS]),
                               NULL),
           "ex_debug", FALSE, 
           "Should not print as the flag is off: %d\n", count++);


    /*************************************************************/
    /* Turn on the EX_DEBUG_REQUESTS flag */
    elem = EX_DEBUG_REQUESTS;
    debug_set_flag(ex_debug_arr, elem, "ex_debug", NULL);
    buginf(debug_check_element(ex_debug_arr, EX_DEBUG_REQUESTS, 
                               //&filter_arr[EX_DEBUG_REQUESTS]),
                               NULL),
           "ex_debug", FALSE, "Requests Test Message %d\n", count++);
    
    /* Turn it off */
    debug_reset_flag(ex_debug_arr, elem, "ex_debug", NULL);
    buginf(debug_check_element(ex_debug_arr, EX_DEBUG_REQUESTS, 
                               //&bogus_fil[EX_DEBUG_REQUESTS]),
                               NULL),
           "ex_debug", FALSE, 
           "Should not print as the flag is off: %d\n", count++);


    /*************************************************************/
    /* Turn on the EX_DEBUG_RESPONSES flag */
    elem = EX_DEBUG_RESPONSES;
    debug_set_flag(ex_debug_arr, elem, "ex_debug", NULL);

    buginf(debug_check_element(ex_debug_arr, EX_DEBUG_RESPONSES, NULL),
           "ex_debug", FALSE, "Responses Test Message %d\n", count++);
    
    /* Turn it off */
    debug_reset_flag(ex_debug_arr, elem, "ex_debug", NULL);
    buginf(debug_check_element(ex_debug_arr, EX_DEBUG_RESPONSES, NULL),
           "ex_debug", FALSE, 
           "Should not print as the flag is off: %d\n", count++);


    /* Close log in your close() routine */
    syslog_facility_close();


    return 0;
}
