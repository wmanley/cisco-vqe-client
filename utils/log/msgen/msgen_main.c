/**-----------------------------------------------------------------
 * @brief
 * Test tool: generate syslog messages at the specified rate
 *
 * @file
 * msgen_main.c
 *
 * November 2007, Donghai Ma
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#define _GNU_SOURCE
#include <getopt.h>

#include "utils/vam_types.h"
#include "msgen_xmlrpc.h"
#include "msgen_para.h"

/*------------------------------------------------------------------------*/
/* Among all the C files including it, only ONE C file (say the one where 
 * main() is defined) should instantiate the message definitions described in 
 * XXX_syslog_def.h.   To do so, the designated C file should define the symbol
 * SYSLOG_DEFINITION before including the file.  
 */
#define SYSLOG_DEFINITION
#include "log/vqes_cp_syslog_def.h"
#include "msgen_syslog_def.h"

#undef SYSLOG_DEFINITION

#include "log/vqes_cp_debug.h"
/*------------------------------------------------------------------------*/


/* Only use long options except for 'h' and 'v' */
static struct option long_opt[] = {
    {"xmlrpc-port", 1, NULL, 0},
    {"msg-rate", 1, NULL, 0},
    {"threads", 1, NULL, 0},
    {"version", 0, NULL, 'v'},
    {"help", 0, NULL, 'h'},
    {0, 0, 0, 0}
};


/* Print out the usage on STDOUT */
static void print_usage (void)
{
    const char *usage = 
        "Options:\n"
        "\t--xmlrpc-port <xmlrpc_port>   Port used for xmlrpc server\n"
        "\t                              Default port is 9000\n"
        "\t--threads <n>                 Start n (upto 10) logging threads.\n"
        "\t                              Default value is 2\n"
        "\t--msg-rate <msgs per sec>     Send messages at this rate\n"
        "\t-v, --version                 Print the version number\n"
        "\t-h, --help                    Print the command line options\n";
    printf("%s", usage);
}



/* Globals */
uint16_t xmlrpc_port = DEFAULT_XMLRPC_PORT;

/* Keep track of each thread */
typedef struct thread_tag {
    pthread_t thread_id;
    int number;
} thread_t;

#define MAX_THREADS 10
static thread_t thread[MAX_THREADS];


/* Send out a few syslog messages */
static void send_syslog_msgs (int thread_num)
{
    char msg_buf[256], *s;
    time_t now = time(NULL);

    s = ctime(&now);
    s[strlen(s) - 1] = '\0';   /* remove newline */

    snprintf(msg_buf, 256, "thread %d at %s", thread_num, s);
    syslog_print(MSGEN_ERR_MSG_RL_1, msg_buf);
    inc_msg_sent();

    snprintf(msg_buf, 256, "thread %d at %s", thread_num, s);
    syslog_print(MSGEN_INFO_MSG_RL_1, msg_buf);
    inc_msg_sent();

    snprintf(msg_buf, 256, "thread %d at %s", thread_num, s);
    syslog_print(MSGEN_INFO_MSG_1, msg_buf);
    inc_msg_sent();
}


/*
 * Start routine for msgen logging threads
 */
void *logging_thread_routine (void *arg)
{
    int i;
    uint32_t usec_wait = 0;
    boolean do_not_send = FALSE;
    time_t last_check_time, now;

    uint64_t msg_sent;
    uint32_t msg_rate, msg_burst;
    boolean  oneshot_burst;

    thread_t *self = (thread_t*)arg;

    /* Main loop to send out syslog messages at the specified rate. When 
     * the do_burst flag is set, we will send out a burst of <msg_bust> number
     * of messages.
     */
    last_check_time = time(NULL);
    while (1) {
        get_msgen_paras(&msg_rate, &msg_burst, &oneshot_burst, &msg_sent);

        if (msg_rate == 0) {
            usec_wait = 1000000;  /* 1 second */
            do_not_send = TRUE;
        } else {
            usec_wait = 1./(double)msg_rate * 1000000;
            do_not_send = FALSE;
        }
        
        if (oneshot_burst) {
            for (i = 0; i < msg_burst; i++) {
                send_syslog_msgs(self->number);
            }
            /* Can't reset the flag now as there may be multiple logging 
             * threads.
             */
            //set_oneshot_burst_flag(FALSE);
        }

        if (!do_not_send) {
            send_syslog_msgs(self->number);
        }
        
        usleep(usec_wait);

        /* Check the SML periodically */
        now = time(NULL);
        if (now - last_check_time > 30) {
            check_sml(ABS_TIME_0);
            last_check_time = now;
        }
    }

    return NULL;
}


int main (int argc, char *argv[])
{
    int thread_count, total_threads = 2, status;

    /* Process the command line arguments */
    int c = 0, tmp;
    int option_index = 0;
    const char *optstr = "vh?";

    while ((c = getopt_long(argc, argv, optstr, long_opt, &option_index))
           != EOF) {
        switch( c ) {   
        case 0:
            /* All options other than help, version go here */
            if (strcmp("xmlrpc-port", long_opt[option_index].name) == 0) {
                tmp = atoi(optarg);
                if (tmp < 0 || tmp > 0xffff) {
                    printf("Invalid value %d for option %s is specified. "
                           "Valid range is [%d %d]. Use value %d.\n", 
                           tmp, long_opt[option_index].name, 0, 0xffff,
                           xmlrpc_port);
                } else {
                    xmlrpc_port = tmp;
                }

            } else if (strcmp("msg-rate", long_opt[option_index].name) == 0) {
                tmp = atoi(optarg);
                if (tmp < MIN_MSG_RATE || tmp > MAX_MSG_RATE) {
                    printf("Invalid value %d for option %s is specified. "
                           "Valid range is [%d %d]. Use default value.\n",
                           tmp, long_opt[option_index].name, 
                           MIN_MSG_RATE, MAX_MSG_RATE);
                } else {
                    set_msg_rate(tmp);
                }
            } else if (strcmp("threads", long_opt[option_index].name) == 0) {
                tmp = atoi(optarg);
                if (tmp < 1 || tmp > MAX_THREADS) {
                    printf("Invalid value %d for option %s is specified. "
                           "Valid range is [%d %d]. Use default value.\n",
                           tmp, long_opt[option_index].name, 
                           1, MAX_THREADS);
                } else {
                    total_threads = tmp;
                }
            }                
            break;

        case 'v':  /* version number */
            printf("\n%s: version 0.1\n", argv[0]);
            return (0);

        case 'h':  /* fall through is intentional */
        case '?':
            printf( "\nUsage: %s [options]\n", argv[0]);
            print_usage();
            return (0);

        default:   /* invalid options or missing arguments */
            printf("Invalid command line option: %s\n", argv[optind-1]);
            break;
        } /* switch (c) */
    } /* while (c=getopt_long() ...) */

    if ((argc - optind) > 0) {
        /* Should log a warning message and continue */
        printf("Invalid command line option: %s\n", argv[optind-1]);
    }


    /* Initialize the logging facility */
    syslog_facility_open(LOG_VQES_CP, LOG_CONS);
    syslog_facility_filter_set(LOG_VQES_CP, LOG_INFO);


    /* Start the XMLRPC server thread */
    if (!msgen_xmlrpc_start(xmlrpc_port, NULL)) {
        fprintf(stderr, "Failed to start XMLRPC server thread. Exiting ...\n");
        return (-1);
    }

    /* Start a few logging threads */
    for (thread_count = 0; thread_count < total_threads; thread_count++) {
        thread[thread_count].number = thread_count;
        status = pthread_create(&thread[thread_count].thread_id, 
                                NULL,
                                logging_thread_routine, 
                                (void*)&thread[thread_count]);
        if (status != 0) {
            fprintf(stderr, "Failed to create logging thread %d. "
                    "Exiting ...\n", thread_count);
            return (-1);            
        }
    }
    
    /* Now join with each of the threads */
    for (thread_count = 0; thread_count < total_threads; thread_count ++) {
        status = pthread_join(thread[thread_count].thread_id, NULL);
        if (status != 0) {
            fprintf(stderr, "Failed to join logging thread %d. "
                    "Exiting ...\n", thread_count);
            return (-1);            
        }
    }
    return 0;
}
