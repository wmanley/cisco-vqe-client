/*
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */


#define _GNU_SOURCE 1           /* Include sched affinity API */

#include <time.h>
#include <sched.h>
#include "vqec_ifclient.h"
#include "vqec_event.h"
#include "vqec_stream_output_thread_mgr.h"
#include "vqec_debug.h"
#include "vqec_syscfg.h"
#include "vqec_pthread.h"
#include "vqec_version.h"
#include "vqec_igmp.h"
#if HAVE_CISCO_RTSP_LIBRARY
#include "vqec_vod_ifclient.h"
#endif  /* HAVE_CISCO_RTSP_LIBRARY */

/*
 * Set up some basic signal handling for this test application.
 * When CTRL-C is pressed at the console, or 'kill' is used, now
 * test_vqec_ifclient will exit gracefully.
 *
 * It should be noted that this also affects the behavior of this
 * application when using GDB.  When CTRL-C is pressed in GDB, the
 * signal will no longer be caught by GDB; instead, it will be caught
 * by this signal handler and the program will appear to exit normally
 * in GDB.
 */
void test_vqec_ifclient_sigset (sigset_t *set)
{
    (void)sigemptyset(set);
    (void)sigaddset(set, SIGINT);
    (void)sigaddset(set, SIGTERM);
}

/*--------
* Cleaning up Unix domain sockets:
*   Calling vqec_upcall_close_unsocks() without ifclient layer support is a
*   layering violation - however to clean-up the pid-appended unix domain
*   inodes this code has been added here.
*--------*/
void *test_vqec_ifclient_signal_handler (void *arg)
{
    sigset_t lsig_set;
    int32_t signum;

    test_vqec_ifclient_sigset(&lsig_set);
    while (1) {
        if (!sigwait(&lsig_set, &signum)) {
            switch (signum) {
            case SIGINT:
                printf("SIGINT  ... exiting\n");
#if HAVE_CISCO_RTSP_LIBRARY
                vqec_vod_module_deinit();
#endif  /* HAVE_CISCO_RTSP_LIBRARY */
                vqec_ifclient_deinit();
                exit(EXIT_SUCCESS);
                break;
            case SIGTERM:
                printf("SIGTERM ... exiting\n");
#if HAVE_CISCO_RTSP_LIBRARY
                vqec_vod_module_deinit();
#endif  /* HAVE_CISCO_RTSP_LIBRARY */
                vqec_ifclient_deinit();
                exit(EXIT_SUCCESS);
                break;
            default:
                break;
            }
        }        
    }

    /* sa_ignore {} NOTREACHED(1) */
    return (NULL);
}

static void print_usage (void)
{
    printf("test_vqec_ifclient [-h|--help] [--setcpu <cpu>] [-v|--version] "
           "<config>\n"
           "-h, --help      Display this usage information\n\n"
           "--setcpu <cpu>  Used to taskset test_vqec_ifclient program\n"
           "                to a particular cpu (note, cpu numbers start\n"
           "                at 0, so '--setcpu 0' would taskset to first\n"
           "                cpu in system)\n\n"
           "--cname <cname> Overrides the default cname chosen by VQE-C\n"
           "                Should be unique across all instances of VQE-C\n\n"
           "-v, --version   Displays version information\n\n"           
           "<config>        Name of the configuration file\n\n");
}

/*
 * Parsed command line args.
 */
static int32_t s_cpu = -1;
static const char *s_cname = NULL;
static char *s_cfgfile = NULL;

static boolean parse_args (int argc, const char * argv[])
{

#define ARG_STRLEN 50

    int32_t cur_arg = 1;
    time_t ltime; /* calendar time */

    if (argc == 1) { 
       return (FALSE);
    }

    /* Scan for -v qualifier */
    if ((strncmp("-v", argv[cur_arg], ARG_STRLEN) == 0) ||
        (strncmp("-version", argv[cur_arg], ARG_STRLEN) == 0) ||
        (strncmp("--v", argv[cur_arg], ARG_STRLEN) == 0) ||
        (strncmp("--version", argv[cur_arg], ARG_STRLEN) == 0)) {
        
        printf("Version: VQE-C %d.%d.%d Build %d\n", 
                VQEC_PRODUCT_VERSION, VQEC_MAJOR_VERSION, 
                VQEC_MINOR_VERSION, VQEC_VAR_BUILD_NUMBER);
        ltime = time(NULL);
        printf("Timestamp: %s %s\n", BUILD_DATE, BUILD_TIME);
        exit(EXIT_SUCCESS);
    } 

    /* Scan for -h qualifier */
    for (cur_arg = 1; cur_arg < argc; cur_arg++)
    {
        if ((strncmp("-h", argv[cur_arg], ARG_STRLEN) == 0) || 
            (strncmp("--h", argv[cur_arg], ARG_STRLEN) == 0) ||
            (strncmp("-help", argv[cur_arg], ARG_STRLEN) == 0) || 
            (strncmp("--help", argv[cur_arg], ARG_STRLEN) == 0)) {
            print_usage();
            exit(EXIT_SUCCESS);
        }
    }

    boolean result = TRUE;

    /* Scan for other command line qualifiers */
    for (cur_arg = 1; cur_arg < (argc - 1); cur_arg++)
    {
        if (strncmp("--setcpu", argv[cur_arg], ARG_STRLEN) == 0) {
            if (cur_arg >= (argc - 2)) {
                result = FALSE;
                break;
            }
            
            s_cpu = atoi(argv[cur_arg + 1]);
            cur_arg++;
        } else if (strncmp("--cname", argv[cur_arg], ARG_STRLEN) == 0) {
            if (cur_arg >= (argc - 2)) {
                result = FALSE;
                break;
            }

            s_cname = argv[cur_arg + 1];
            cur_arg++;
        } else {
            result = FALSE;
            break;
        }
    }

    if (result) {
        if (cur_arg == (argc - 1)) {
            s_cfgfile = (char *)argv[argc - 1];
            printf("Configuration file is %s\n", s_cfgfile);
        } else {
            result = FALSE;
        }
    }

    return (result);
}

int main (int argc, const char **argv)
{

    vqec_error_t err;
    uint8_t err_cname;
    pthread_t tid;
    sigset_t lsig_set;
    int result;

    if (!parse_args(argc, argv)) {
        print_usage();
        exit(EXIT_FAILURE);
    }

#ifdef OBJECT_X86
    /* Set proc affinity, if requested */
    if (s_cpu != -1)
    {
        cpu_set_t cpu_s;
        int ret;
    
        CPU_ZERO(&cpu_s);
        CPU_SET(s_cpu, &cpu_s);
        ret = sched_setaffinity(0, sizeof(cpu_s), &cpu_s);
        if (ret != 0) {
            printf("Could not set process affinity!!\n");
        } else {
            printf("Set process affinity to %d!!\n", s_cpu);
        }
    }    
#endif /* OBJECT_X86 */

    /* 
     * This thread will run the libevent context: elevate priority:
     * An error in elevation due to insufficient privilege
     * is not considered fatal; other errors are fatal.
     */
    result = vqec_pthread_set_priosched(pthread_self());
    if (result != 0) {
        if (result != EPERM) {
            exit(EXIT_FAILURE);
        }
    }

    /* block SIGINT & SIGTERM in parent, all children */
    test_vqec_ifclient_sigset(&lsig_set);
    if (pthread_sigmask(SIG_BLOCK, &lsig_set, NULL)) {
        printf("Signal masking failed - exiting\n");
        exit(EXIT_FAILURE);
    }

    /* create a thread to handle signals */
    if (vqec_pthread_create(&tid,  
                            test_vqec_ifclient_signal_handler, NULL)) {
        printf("Unable to create signal handling thread - exiting\n");
        exit(EXIT_FAILURE);
    }

    /* assign the cname, if one was supplied */
    if (s_cname) {
        err_cname = vqec_ifclient_register_cname((char *)s_cname);
        if (!err_cname) {
            printf("*** VQE-C CNAME registration failed.  "
                   "Default CNAME will be used...\n");
        }
    }
 
    /* initialize the ifclient library */
    err = vqec_ifclient_init(s_cfgfile);    
    if (err != VQEC_OK) {
        printf("VQE-C initialization failed - error (%s)\n",
               vqec_err2str(err));
        exit(EXIT_FAILURE);
    }

#if HAVE_CISCO_RTSP_LIBRARY
    vqec_vod_error_t res = VQEC_VOD_OK;
    if ((res = vqec_vod_module_init(NULL)) != VQEC_VOD_OK) {
        printf("VQE-C VOD session initialization failed\n"); 
        exit(EXIT_FAILURE);
    }
#endif  /* HAVE_CISCO_RTSP_LIBRARY */

    /* look through config file again to setup any stream_output */
    vqec_stream_output_thread_mgr_init_startup_streams(s_cfgfile);

    vqec_igmp_proxy_init(s_cfgfile);

    /* Ignoring return value - ifclient returns if client wasn't properly
       initialized, or if an explicit stop is done. In either case we'll
       terminate the program. */
    (void)vqec_ifclient_start();

    printf("Exit from vqec_ifclient\n");

    return 0;
}
