/*
 *------------------------------------------------------------------
 * RTSP Client Test Program
 *
 * November 2006, Dong Hsu
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

/* System Includes */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

/* Application Includes */
#include <utils/vam_debug.h>
#include <utils/vam_time.h>
#include "rtsp_client.h"
#include "vqec_updater.h"

/* Global Variables */
boolean gShutdownFlag = FALSE;
typedef void (*sighandler_t)(int);

const char *applicationUsage = "rtsp_server_name rtsp_server_port num_requests service_name polling_time";
/* sa_ignore DISABLE_RETURNS signal */

static pthread_t client_thread_id;

void *rtsp_client_thread_func(void *arg);

#define SIZE 30

typedef struct rtsp_server_ {
    char name[SIZE];
    int  port;
    int  numRequests;
    char service[SIZE];
    int  polling_time;
} rtsp_server_t;

/**
 * This is the signal handler, When called this sets the
 * global gShutdownFlag allowing the main processing
 * loop to exit cleanly.
 */
void sigHandler (int sig_num)
{
    /* Set a global shutdown flag */
    gShutdownFlag = TRUE;

    /* Unregister interest in the signal to prevent recursive callbacks */
    signal(sig_num, SIG_DFL);

    // TODO:: Do we need to flush the logfile?
    if (SIGTERM == sig_num)
    {
       vam_debug_trace("sigHandler - terminate signal received.\n");
    }
    else
    {
       vam_debug_trace("sigHandler - caught signal: %d\n", sig_num );
    }
}


int main (int argc, char *argv[])
{
    rtsp_server_t server;

    if (argc < 6) {
        printf( "\nUsage: %s %s\n", argv[0], applicationUsage);
        exit(-1);
    }

    strncpy(server.name, argv[1], SIZE);
    server.port = atoi(argv[2]);
    server.numRequests = atoi(argv[3]);
    strncpy(server.service, argv[4], SIZE);
    server.polling_time = atoi(argv[5]);

    /* Register Signal handlers so we can perform graceful shutdown */
    signal(SIGINT,   sigHandler);    /* Trap Ctrl-C on NT */
    signal(SIGILL,   sigHandler);
    signal(SIGABRT,  sigHandler);    /* Abort signal 6 */
    signal(SIGFPE,   sigHandler);    /* Floading Point Exception */
    signal(SIGSEGV,  sigHandler);    /* Address access violations signal 11 */
    signal(SIGTERM,  sigHandler);    /* Trap kill -15 on UNIX */
    signal(SIGHUP,   sigHandler);    /* Hangup */
    signal(SIGQUIT,  sigHandler);
    signal(SIGPIPE,  SIG_IGN);       /* Handle TCP Failure */
    signal(SIGBUS,   sigHandler);
    signal(SIGSYS,   sigHandler);
    signal(SIGXCPU,  sigHandler);
    signal(SIGXFSZ,  sigHandler);
    signal(SIGUSR1,  sigHandler);
    signal(SIGUSR2,  sigHandler);

    if (pthread_create(&client_thread_id,
                       NULL,
                       &rtsp_client_thread_func,
                       &server)) {
        fprintf(stderr, "main:: Unable to create the thread "
                        "to run rtsp client\n");
    }

    /* Wait for exit */
    while (!gShutdownFlag) {
        /* sa_ignore IGNORE_RETURN */
        sleep(5);
    }

    exit(0);
}



void *rtsp_client_thread_func (void *arg)
{
    int i = 0;
    rtsp_server_t *server = (rtsp_server_t *) arg;
    rtsp_ret_e status;
    abs_time_t now;
    int succeeded = 0;
    int wait_for_seconds;
    unsigned random_seed;
    uint64_t start_time;
    uint64_t end_time;

    /* Initialize the random number generator */
    random_seed = getpid();
    srand(random_seed);

    now = get_sys_time();
    start_time = abs_time_to_msec(now);
    printf("Starting time = %llu msec\n", start_time);

    for (i = 0; i < server->numRequests; i++) {
        /* Initialize a rtsp client */
        if (rtsp_client_init(server->name, server->port) == RTSP_SUCCESS) {
            status = rtsp_send_request(
                server->service, MEDIA_TYPE_APP_SDP,
                TIME_ADD_A_R(get_sys_time(), VQEC_UPDATER_TIMEOUT_BACKGROUND));
            if (status == RTSP_SUCCESS) {
                if (rtsp_get_response_code() == MSG_200_OK) {
                    succeeded++;
                }
                else {
                    fprintf(stderr, "Failed to receive a 200OK response %d!\n",
                            rtsp_get_response_code());
                }
            } else {
                fprintf(stderr, "RTSP client failed to send the request.\n");
            }

            /* Close the conntection */
            if (rtsp_client_close() != RTSP_SUCCESS) {
                fprintf(stderr, "Failed to close client!\n");
            }
        } else {
            fprintf(stderr, "Failed to connect to the server %s\n",
                    server->name);
        }

        /* Wait for random number of seconds before sending out next request */
        if (server->polling_time) {
            wait_for_seconds = rand() % server->polling_time;

            /* sa_ignore IGNORE_RETURN */
            sleep(wait_for_seconds);
        }
    }

    now = get_sys_time();
    end_time = abs_time_to_msec(now);
    printf("Ending time with %d requests = %llu msec\n",
           succeeded, end_time);
    if (succeeded) {
        printf("Average request time = %llu nsec\n",
               (end_time - start_time)*1000/succeeded);
    }

    gShutdownFlag = TRUE;
    pthread_exit(NULL);
}


