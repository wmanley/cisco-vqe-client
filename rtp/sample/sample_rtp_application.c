/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * sample_rtp_application.c
 * 
 * A sample rtp application built using p2p rtp session. The sample
 * control plane consists of an RTP session  (sample_rtp_session.c)
 * which is derived from the rtp p2p. The data plane (sample_rtp_dataplane.c)
 * creates and sends a 100byte RTP packet every 2 seconds.
 * To build: Do "make" in the sample directory
 * To run: Run on 2 separate machines by using following command line:
 *        sample_session <peer's IP address> 
 * The application will ask if we are the sender. One out of 
 * two machince must be selected as sender. 
 * Make sure to turn on the debugs to see all the useful messages floating
 * by as RTCP packets are sent/received, member are created/destroyed.
 * 
 * Copyright (c) 2006-2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "../../include/sys/event.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <netinet/ip.h>
#include "../../include/utils/vam_types.h"
#include "rtp_exp_api.h"
#include "exp.h"
#include "rtp_database.h"
#include "sample_rtp_application.h"
#include "sample_rtp_channel.h"
#include "sample_rtp_session.h"
#include "sample_rtp_dataplane.h"
#include "sample_rtp_utils.h"

#define BUF_LEN  150
#define ADDR_STRLEN 16
#define PORT_STRLEN  6


static rtp_sample_t **pp_rtp_sample = NULL;
static rtp_sample_t **pp_rtp_repair = NULL;
static sample_config_t config;

static void
sigint_handler (int fd, short event, void *arg) 
{
    int byes = 0;

    byes = rtp_delete_session_sample(pp_rtp_sample, pp_rtp_repair,
                                     config.num_sessions);
    fprintf(stderr, "\nExiting ... %d BYEs sent for %d sessions\n", 
            byes, config.num_sessions);
    exit(0);
}

static void
sigusr1_handler (int fd, short event, void *arg) 
{
    rtp_session_t *p_sess = (rtp_session_t *)pp_rtp_sample[0];
    rtp_member_t  *p_source = NULL;

    if (!p_sess) {
        return;
    }

    fprintf(stderr, "Primary session: %d members, %d senders\n",
            p_sess->rtcp_nmembers, p_sess->rtcp_nsenders);

    for (p_source = rtp_first_member(p_sess);
         p_source;
         p_source = rtp_next_member(p_sess, p_source)) {
        
        fprintf(stderr, "SSRC=0x%08x CNAME=%s\n",
                p_source->ssrc,
                p_source->sdes[RTCP_SDES_CNAME] ?
                p_source->sdes[RTCP_SDES_CNAME] : "<null>");
    }
}

static void
sigusr2_handler (int fd, short event, void *arg) 
{
    int i = 0;
    int j = 0;
    sample_channel_t *channel = NULL;
#define RECEIVERS_PER_CHANNEL 3
    sample_receiver_t *receivers[RECEIVERS_PER_CHANNEL] = {NULL, NULL, NULL};
    sample_receiver_t *receiver = NULL;
    sample_source_t *source = NULL;
    rtp_source_id_t *id = NULL;
    char buf[ADDR_STRLEN];
    int num_channels = get_num_channels();

    for (i = 0; i < num_channels; i++) {
        channel = get_sample_channel(i);
        if (!channel) {
            continue;
        }

        fprintf(stderr, "Channel %d:\n", i);
        receivers[0] = channel->pri_rcvr;
        receivers[1] = channel->rpr_rcvr;
        receivers[2] = channel->morerpr_rcvr;
        for (j = 0; j < RECEIVERS_PER_CHANNEL; j++) {
            receiver = receivers[j];
            if (!receiver) {
                continue;
            }
            fprintf(stderr, "%s receiver:\n", sample_receiver_type(receiver->type));
            if (VQE_LIST_EMPTY(&receiver->sources)) {
                fprintf(stderr, "No sources\n") ;
                continue;
            }
            VQE_LIST_FOREACH(source, &receiver->sources, source_link) {
                id = &source->id;
                fprintf(stderr, "[%s:%u ssrc 0x%08x]\n",
                        uint32_ntoa_r(id->src_addr, buf, sizeof(buf)),
                        ntohs(id->src_port),
                        id->ssrc);
            }
        }
    }
}

static void
print_usage (void)
{
    printf("Usage: sample_session [-l <addr>:<port> [-i <addr>]]\n" 
           "                      [-f <addr>:<port>[:<rpr_port>]]\n"
           "                      [-d <count>:<interval>[:[no]jitter]]\n"
           "                      [-L <addr>:<port>]\n"
           "                      [-F <addr>:<port>[:<rpr_port>]]\n"
           "                      [-c <min_secs>:<max_secs>]\n"
           "                      [--fastfill]\n"
           "                      [--max-fastfill-time <msecs>]\n"
           "                      [--rcc-fill <min>:<max>]\n"
           "                      [--max-rcc-bw <bps>]\n"
           "                      [-o <addr>:<port>]\n"
           "                      [-x <addr>:port>]\n"
           "                      [-s | -r <# sessions>]\n"
           "                      [-t <msecs>]\n"
           "                      [-e]\n"
           "                      [-C <rtcp_script_file>]\n"
           "                      [-R <rtp_script_file>]\n"
           "                      [-v]\n"
           "                      [--rtcp-rsize]\n"
           "                      [-b <bps/rcvr>]\n"
           "                      [-m]\n");
}

int main (int argc, char **argv)
{
    int c = 0;
    int option_index = 0;
    char addr_str[ADDR_STRLEN];
    char port_str[PORT_STRLEN];
    char rpr_port_str[PORT_STRLEN];
#define JITTER_STRLEN 9
    char jitter_str[JITTER_STRLEN];
    int drop_count = 0;
    int drop_intvl = 0;
    int min_chg_secs = 0;
    int max_chg_secs = 0;
    unsigned int min_rcc_fill = 0;
    unsigned int max_rcc_fill = 0;
    unsigned int max_rcc_bw = 0;

    in_addr_t       vqm_ip_addr = INADDR_NONE;
    in_port_t       vqm_port = 0;
    char            vqm_ip_addr_str[ADDR_STRLEN];
    char            vqm_port_str[PORT_STRLEN];
    boolean         vqm_ip_addr_configured = FALSE;
    boolean         vqm_port_configured = FALSE;
    exp_status_e    exp_status;

    int num_sessions = 0;
    int num_rpr_sessions = 0;
    int num_channels = 0;
    int rqd_channels = 0;
    sample_channel_t *channel = NULL;

    int fbt_sections = 0;
    int num_drop_params = 0;

    struct event interrupt;
    struct event usr1;
    struct event usr2;

#define OPT_FASTFILL          "fastfill"
#define OPT_MAX_FASTFILL_TIME "max-fastfill-time"
#define OPT_RCC_FILL          "rcc-fill"
#define OPT_MAX_RCC_BW        "max-rcc-bw"
#define OPT_RTCP_RSIZE        "rtcp-rsize"
 
    static struct option long_options[] = {
        {OPT_FASTFILL, 0, NULL, 0},
        {OPT_MAX_FASTFILL_TIME, required_argument, NULL, 0},
        {OPT_RCC_FILL, required_argument, NULL, 0},
        {OPT_MAX_RCC_BW, required_argument, NULL, 0},
        {OPT_RTCP_RSIZE, no_argument, NULL, 0},
        {"help", 0, NULL, 'h'},
        {0, 0, 0, 0}
    };

    config.sender = FALSE;
#define SEND_INTERVAL 2000 /* in msecs */
    config.send_interval = 2000;
    config.num_sessions = 1;
    config.extra_messages = FALSE;
    config.export = FALSE;
    config.per_rcvr_bw = RTCP_DFLT_PER_RCVR_BW / 3;
    config.rtcp_xr_enabled = FALSE;
    config.rtp_addr = INADDR_NONE;
    config.rtp_port = 0;
    config.if_addr = INADDR_ANY;
    config.rtcp_addr = INADDR_NONE;
    config.rtcp_port = 0;
    config.fbt_addr = INADDR_NONE;
    config.fbt_port = 0;
    config.rpr_port = 0;
    config.alt_rtp_addr = INADDR_NONE;
    config.alt_rtp_port = 0;
    config.alt_rtcp_addr = INADDR_NONE;
    config.alt_rtcp_port = 0;
    config.alt_fbt_addr = INADDR_NONE;
    config.alt_fbt_port = 0;
    config.alt_rpr_port = 0;
    config.out_addr = INADDR_NONE;
    config.out_port = 0;
    config.drop_count = 0;
    config.drop_intvl = 0;
    config.drop_jitter = FALSE;
    config.min_chg_secs = 0;
    config.max_chg_secs = 0;
    config.do_fastfill = FALSE;
    config.maximum_fastfill_time = 0;
#define DFLT_MIN_RCC_FILL  200
#define DFLT_MAX_RCC_FILL 5000
#define DFLT_MAX_RCC_BW      0
    config.min_rcc_fill = DFLT_MIN_RCC_FILL;
    config.max_rcc_fill = DFLT_MAX_RCC_FILL;
    config.maximum_recv_bw = DFLT_MAX_RCC_BW;
    config.rtcp_script = NULL;
    config.rtp_script = NULL;
    config.verbose = FALSE;
    config.rtcp_rsize = FALSE; /* By default */

    while ((c = getopt_long(argc, argv, "l:L:i:f:F:x:r:b:C:R:d:c:t:o:smveh",
                            long_options, &option_index)) 
           != EOF) {
        switch (c) {
        case 0:
            if (strcmp(OPT_FASTFILL, long_options[option_index].name) == 0) {
                config.do_fastfill  = TRUE;
            } else if (strcmp(OPT_MAX_FASTFILL_TIME,
                              long_options[option_index].name) == 0) {
                char *endptr = NULL;
                unsigned long int mfft = 0;
                errno = 0;
                mfft = strtoul(optarg, &endptr, 0);
                if (errno != 0 || *endptr != '\0' || mfft > UINT32_MAX) {
                    print_usage();
                    return (-1);
                } else {
                    config.maximum_fastfill_time = (uint32_t)mfft;
                }
            } else if (strcmp(OPT_RCC_FILL,
                              long_options[option_index].name) == 0) {
                if (sscanf(optarg, "%u:%u", &min_rcc_fill, &max_rcc_fill) != 2) {
                    print_usage();
                    return (-1);
                }
                if (min_rcc_fill > UINT32_MAX || max_rcc_fill > UINT32_MAX) {
                    print_usage();
                    return (-1);
                }
                if (max_rcc_fill < min_rcc_fill) {
                    print_usage();
                    return (-1);
                }
                config.min_rcc_fill = min_rcc_fill;
                config.max_rcc_fill = max_rcc_fill;
            } else if (strcmp(OPT_MAX_RCC_BW,
                              long_options[option_index].name) == 0) {
                if (sscanf(optarg, "%u", &max_rcc_bw) != 1) {
                    print_usage();
                    return (-1);
                }
                if (max_rcc_bw > UINT32_MAX) {
                    print_usage();
                    return (-1);
                }
                config.maximum_recv_bw = max_rcc_bw;
            } else if (strcmp(OPT_RTCP_RSIZE, 
                       long_options[option_index].name) == 0) {
                config.rtcp_rsize = TRUE;
                break;
            } else {
                print_usage();
                return (-1);
            }
            break;
        case 'l':
            if (sscanf(optarg, "%15[^:]:%5s", addr_str, port_str) != 2) {
                print_usage();
                return (-1);
            }
            config.rtp_addr = inet_addr(addr_str);
            config.rtp_port = htons((in_port_t)(atoi(port_str)));
            break;
        case 'L':
            if (sscanf(optarg, "%15[^:]:%5s", addr_str, port_str) != 2) {
                print_usage();
                return (-1);
            }
            config.alt_rtp_addr = inet_addr(addr_str);
            config.alt_rtp_port = htons((in_port_t)(atoi(port_str)));
            break;
        case 'i':
            config.if_addr = inet_addr(optarg);
            break;
        case 'f':
            fbt_sections = sscanf(optarg, "%15[^:]:%5[^:]:%5s", 
                                  addr_str, port_str, rpr_port_str);
            if (fbt_sections < 2) {
                print_usage();
                return (-1);
            }
            config.fbt_addr = inet_addr(addr_str);
            config.fbt_port = htons((in_port_t)(atoi(port_str)));
            if (fbt_sections >= 3) {
                config.rpr_port = htons((in_port_t)(atoi(rpr_port_str)));
                if (config.rpr_port == config.fbt_port) {
                    print_usage();
                    return(-1);
                }
            }
            break;
        case 'F':
            fbt_sections = sscanf(optarg, "%15[^:]:%5[^:]:%5s", 
                                  addr_str, port_str, rpr_port_str);
            if (fbt_sections < 2) {
                print_usage();
                return (-1);
            }
            config.alt_fbt_addr = inet_addr(addr_str);
            config.alt_fbt_port = htons((in_port_t)(atoi(port_str)));
            if (fbt_sections >= 3) {
                config.alt_rpr_port = htons((in_port_t)(atoi(rpr_port_str)));
                if (config.alt_rpr_port == config.alt_fbt_port) {
                    print_usage();
                    return(-1);
                }
            }
            break;
        case 'd':
            num_drop_params = sscanf(optarg, "%d:%d:%8s", 
                                     &drop_count, &drop_intvl, jitter_str);
            if (num_drop_params < 2) {
                print_usage();
                return (-1);
            }
            if (drop_count <= 0 || drop_count > LONG_MAX) {
                print_usage();
                return (-1);
            }
            if (drop_intvl < drop_count || drop_intvl > LONG_MAX) {
                print_usage();
                return (-1);
            }
            config.drop_count = drop_count;
            config.drop_intvl = drop_intvl;
            if (num_drop_params >= 3) {
                if (strncmp("nojitter", jitter_str, strlen(jitter_str)) == 0) {
                    config.drop_jitter = FALSE;
                } else if (strncmp("jitter", jitter_str,
                                   strlen(jitter_str)) == 0) {
                    config.drop_jitter = TRUE;
                } else {
                    print_usage();
                    return (-1);
                }
            } else {
                config.drop_jitter = TRUE;
            }
            break;
        case 'c':
            if (sscanf(optarg, "%d:%d", &min_chg_secs, &max_chg_secs) != 2) {
                print_usage();
                return (-1);
            }
            if (min_chg_secs <= 0 ||  max_chg_secs > LONG_MAX) {
                print_usage();
                return (-1);
            }
            if (max_chg_secs < min_chg_secs || max_chg_secs > LONG_MAX) {
                print_usage();
                return (-1);
            }
            config.min_chg_secs = min_chg_secs;
            config.max_chg_secs = max_chg_secs;
            break;
        case 'o':
            if (sscanf(optarg, "%15[^:]:%5s", addr_str, port_str) != 2) {
                print_usage();
                return (-1);
            }
            config.out_addr = inet_addr(addr_str);
            config.out_port = htons((in_port_t)(atoi(port_str)));
            break;
        case 'x':
            if (sscanf(optarg, "%15[^:]:%5s", addr_str, port_str) != 2) {
                print_usage();
                return (-1);
            }
	    strncpy((void*)vqm_ip_addr_str,(void*)addr_str, ADDR_STRLEN);
	    strncpy((void*)vqm_port_str,(void*)port_str, PORT_STRLEN);
	    vqm_ip_addr_configured = TRUE;
	    vqm_port_configured    = TRUE;
	    config.export          = TRUE;
            break;
        case 's':
            config.sender = TRUE;
            break;
        case 't':
            config.send_interval = atoi(optarg);
            if (config.send_interval <= 0 ||
                config.send_interval > LONG_MAX) {
                print_usage();
                return(-1);
            }
            break;
        case 'r':
            config.sender = FALSE;
            config.num_sessions = atoi(optarg);
            if (config.num_sessions <= 0 || 
                config.num_sessions > LONG_MAX) {
                print_usage();
                return (-1);
            }
            break;
        case 'b':
            config.per_rcvr_bw = atoi(optarg);
            if (config.per_rcvr_bw < 0 || 
                config.per_rcvr_bw > LONG_MAX) {
                print_usage();
                return (-1);
            }
            break;
        case 'm':
            config.extra_messages = TRUE;
            break;
        case 'v':
            config.verbose = TRUE;
            break;
        case 'e':
            config.rtcp_xr_enabled = TRUE;
            break;
        case 'C':
            config.rtcp_script = fopen(optarg, "r");
            if (!config.rtcp_script) {
                perror("Failed to open rtcp script file:");
                print_usage();
                return(-1);
            }
            break;
        case 'R':
            config.rtp_script = fopen(optarg, "r");
            if (!config.rtp_script) {
                perror("Failed to open rtp script file:");
                print_usage();
                return(-1);
            }
            break;
        case 'h':
        default:
            print_usage();
            return (-1);
        }
    }

    if (config.rtp_addr == INADDR_NONE) {
        config.rtp_addr = htonl(INADDR_LOOPBACK);
    }
#define SAMPLE_DFLT_RTP_PORT 50000
    if (config.rtp_port == 0) {
        config.rtp_port = htons(SAMPLE_DFLT_RTP_PORT);
    }

    if (config.rtcp_addr == INADDR_NONE) {
        config.rtcp_addr = config.rtp_addr;
    }
    if (config.rtcp_port == 0) {
        config.rtcp_port = htons(ntohs(config.rtp_port) + 1);
    }

    if (config.fbt_addr == INADDR_NONE) {
        config.fbt_addr = htonl(INADDR_LOOPBACK);
    }
    if (config.fbt_port == 0) {
        config.fbt_port = htons(ntohs(config.rtp_port) + 3);
    }

    if (config.alt_rtcp_addr == INADDR_NONE) {
        config.alt_rtcp_addr = config.alt_rtp_addr;
    }
    if (config.alt_rtcp_port == 0 && config.alt_rtcp_addr != INADDR_NONE) {
        config.alt_rtcp_port = htons(ntohs(config.alt_rtp_port) + 1);
    }

    if (config.if_addr == INADDR_ANY) {
        if (IN_MULTICAST(ntohl(config.rtp_addr)) ||
            IN_MULTICAST(ntohl(config.alt_rtp_addr)) ||
            IN_MULTICAST(ntohl(config.out_addr))) {
            /* 
             * in this case, if_addr will be used for
             * multicast join or setup; INADDR_ANY may work,
             * but it probably won't, so force the user
             * to specify an address via -i 
             */
            printf("Please specify an interface address (via -i)\n"
                   "when using multicast addresses with -l, -L, or -o :\n");
            print_usage();
            return (-1);
        }
    }
                         
    /* Initalize the event library */ 
    (void)event_init();

    /* Initialize EXP (RTCP export) */

    exp_status = exp_init(vqm_ip_addr_str, vqm_port_str, config.export, FALSE, 
                          vqm_ip_addr_configured, vqm_port_configured);
    if (exp_status != EXP_SUCCESS) {
        printf("EXP initialization failed: error %d", exp_status);
    } else if (config.export && 
               vqm_ip_addr_configured && 
               vqm_port_configured) {
        printf("Exporting to %s:%u ...\n", 
               uint32_ntoa_r(vqm_ip_addr, addr_str, sizeof(addr_str)),
               ntohs(vqm_port));
    }

    /* Init class methods table, etc. */
    if (!rtp_sample_init_module(&config)) {
        printf("Couldn't initialize SAMPLE module\n");
        return (-1);
    }

    /* create "channels" */
    num_channels = rtp_create_channel_sample(&config);
    rqd_channels = config.alt_rtcp_port ? 2 : 1 ;
    if (num_channels != rqd_channels) {
        printf("Channel creation failure: %d out of %d created\n",
               num_channels, rqd_channels);
        return (-1);
    }

    /* Now create RTCP sessions */
    pp_rtp_sample = 
        rtp_create_session_sample(&config, 
                                  get_sample_channel(0),
                                  TRUE, /* primary sessions */
                                  &num_sessions);
    if (pp_rtp_sample && num_sessions) {
        printf("Created %d primary session(s)\n", num_sessions);
    } else {
        printf("Couldn't create primary session(s)\n");
        return (-1);
    }
    if (config.rpr_port) {
        pp_rtp_repair = 
            rtp_create_session_sample(&config, 
                                      get_sample_channel(0),
                                      FALSE, /* not primary */
                                      &num_rpr_sessions);
        if (pp_rtp_repair && num_rpr_sessions) {
            printf("Created %d repair session(s)\n", num_rpr_sessions);
        } else {
            printf("Couldn't create repair session(s)\n");
            return (-1);
        }
    }

    /* create dataplane objects */
    channel = get_sample_channel(MAIN_CHANNEL);
    if (config.sender) {
        create_sample_sender(&config,
                             pp_rtp_sample[0]->rtp_local_source->ssrc, 
                             100); 
    } else if (config.out_port) {
        create_sample_translator(&config);
        create_sample_receiver(&config, 
                               SAMPLE_TRANS_RECEIVER,
                               channel);
    } else {
        create_sample_receiver(&config, 
                               SAMPLE_EXTERNAL_RECEIVER,
                               channel);
    }

    if (!config.sender) {
        if (config.rpr_port) {
            create_sample_receiver(&config, SAMPLE_REPAIR_RECEIVER,
                                   channel);
            if (config.num_sessions > 1) {
                create_sample_receiver(&config, SAMPLE_DUMMY_RECEIVER,
                                       channel);
            }
        }
    }    

    channel = get_sample_channel(ALT_CHANNEL);
    if (channel) {
        create_sample_receiver(&config, 
                               SAMPLE_EXTERNAL_RECEIVER,
                               channel);
        if (config.alt_rpr_port) {
            create_sample_receiver(&config, SAMPLE_REPAIR_RECEIVER,
                                   channel);
            if (config.num_sessions > 1) {
                create_sample_receiver(&config, SAMPLE_DUMMY_RECEIVER,
                                       channel);
            }
        }
    }

    /* Catch SIGINT, to try to clean up sessions on exit */
    signal_set(&interrupt, SIGINT, sigint_handler, NULL);
    (void)signal_add(&interrupt, NULL);

    /* Catch SIGUSR1, print info on members */
    signal_set(&usr1, SIGUSR1, sigusr1_handler, NULL);
    (void)signal_add(&usr1, NULL);

    /* Catch SIGUSR2, print info on data sources */
    signal_set(&usr2, SIGUSR2, sigusr2_handler, NULL);
    (void)signal_add(&usr2, NULL);

    /* just wait for events */
    (void)event_dispatch();
    return (0);
}
