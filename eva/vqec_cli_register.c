/*
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */
/*! 
  @file vqec_cli_register.c
  @brief VQE-C CLI command definition utilizing vqec_cli.
  VQEC uses CLI commands for providing commands for test, debug and diagnostics
  via telnet.
 */

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif
#include <utils/strl.h>
#include <sys/stat.h>

#include "vqec_assert_macros.h"
#include "vqec_ifclient.h"
#include "vqec_ifclient_private.h"
#include "vqec_stream_output_thread_mgr.h"
#include "vqec_cli_register.h"
#include "vqec_igmp.h"
#include "vqec_lock_defs.h"
#include "vqec_gap_reporter.h"
#include "vqec_drop.h"
#include "vqec_version.h"
#include "vqec_igmp.h"
#include "vqe_port_macros.h"
#include "vqec_pthread.h"
#include "vqec_channel_api.h"
#include "vqec_cli.h"
#if HAVE_CISCO_RTSP_LIBRARY
#include "vqec_vod_ifclient.h"
#include "vqec_vod_private.h"
#endif  /* HAVE_CISCO_RTSP_LIBRARY */

/*
 * these includes circumvent the ifclient layer
 * following each one is a list of the functions in the file which 
 * depend on that included file
 */
#include "vqec_debug.h"
/* required for:
 * vqec_str_to_lower()
 * vqec_cmd_show_debug()
 * vqec_debug_func()
 */
#include "vqec_tuner.h"
/* required for:
 * vqec_cmd_show_channel()
 * vqec_cmd_show_drop()
 * vqec_cmd_show_error_repair()
 * vqec_cmd_show_counters()
 * vqec_cmd_show_fec()
 * vqec_cmd_set_global()
 * vqec_cmd_drop_interval()
 * vqec_cmd_drop_percentage()
 * vqec_cmd_drop_enable()
 * vqec_cmd_drop_disable()
 * vqec_cmd_error_repair_enable()
 * vqec_cmd_error_repair_disable()
 * vqec_cmd_proxy_igmp_join()
 */
#include "vqec_updater.h"
/* required for:
 * vqec_cmd_show_channel()
 */

/* For NAT interface control. */
#include "vqec_nat_interface.h"
/* For upcall global statistics display. */
#include "vqec_upcall_event.h"

#ifdef HAVE_ELOG
#include "vqec_elog.h"
/* required for:
 * vqec_cmd_monitor_elog()
 */
#endif

#include "vqec_cli_interface.h"
/* required for:
 * vqec_cli_pak_pool_dump()
 */
#include "vqec_dp_api.h"
/* required for:
 * vqec_debug_func()
 */

#ifdef HAVE_IPLM
#include "iplm_cli_register.h" 
#endif

/* Include CFG Library syslog header files */
#include <log/vqe_cfg_syslog_def.h>
#include <log/vqe_cfg_debug.h>

#define VQEC_PRIVILEGE_INIT_ONLY 100

#define VQEC_MAXCMDLEN  256  /* max cmd line length */
#define VQEC_MAXHELPLEN 512  /* max help line length */
#define VQEC_CNT1 "%-13llu "
#define VQEC_CNT2 "%llu (%llu) "
#define VQEC_CNT3 "%-10d"
#define VQEC_NA "-             "
    
static int32_t g_vqec_cli_tcpsock = -1;
static void *g_cli_handle;

/**
 * this parameter is used to determine whether or not the VQEC_DEBUG() messages
 * will be printed to the CLI or not.  (they are always printed to syslog)
 */
static boolean s_vqec_debugs_goto_cli_enabled = TRUE;

/**
 * function to set the above parameter to TRUE or FALSE.
 */
UT_STATIC void vqec_enable_debugs_goto_cli (boolean ena)
{
    s_vqec_debugs_goto_cli_enabled = ena;
}

/* 
 * vqec_str_to_lower
 * vqec_str_match
 * helper functions for parsing and processing the keywords
 * in each command function
 */
UT_STATIC void vqec_str_to_lower (char *outputString, const char *inString)
{
    int i, slen;
    char inputString[VQEC_MAXCMDLEN];
    memset(inputString, 0, VQEC_MAXCMDLEN);
    (void)strlcpy(inputString, inString, VQEC_MAXCMDLEN);
    inputString[VQEC_MAXCMDLEN - 1] = '\0';

    slen = strlen(inputString);

    for (i = 0; i < slen; i++) {
        if (isupper(inputString[i])) {
            outputString[i] = tolower(inputString[i]);
        } else {
            outputString[i] = inputString[i];
        }
    }
    outputString[i] = '\0';
}

int vqec_str_match (char *s, char *params[])
{
    int i,slen,result = VQEC_CLI_ERROR;
    char str[VQEC_MAXCMDLEN];

    if (!s) {
        return(VQEC_CLI_ERROR);
    }
    
    /* Convert input string to lower case */
    vqec_str_to_lower(str,s);
    /* Search for unique match in params */
    i = 0;
    slen = strlen(str);

    while (params[i]) {
        if (strncmp(str,params[i],slen) == 0) {
            /* Check for exact match */
            if (strlen(str) == strlen(params[i])) {
                return(i);
            }
            /* Allow partial match */
            if (result < 0) {
                result = i;	/* partial match */
            } else {
                return(VQEC_CLI_ERROR);	/* duplicate match */
            }
        }
        i++;
    }
    return(result);
}

int vqec_check_args_for_help_char (char *argv[], int argc)
{
    int i = 0;
    for (i = 0 ; i < argc ; i++) {
        if (strchr(argv[i], '?')) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * vqec_print_callback
 * callback function for vqec_cli_print
 * "\n" is attached as the newline character over telnet sessions
 */
UT_STATIC void vqec_print_callback (struct vqec_cli_def *cli, char *string, 
                                    int string_len)
{
    char *ptr;
    int index = 0;
    int len = 0;
    boolean replace_flag = FALSE;
    char *temp_string;
    if (cli && cli->client) {
        temp_string = calloc(sizeof(char), VQEC_CLI_MAXSIZE_PRINTBUF);
        if (!temp_string) {
            return;
        }
        /* Search for '\n' in the string, and replace it with '\n\r' */
        ptr = string;
        index = 0;
        while(*ptr != '\0' && (len < string_len) && 
              (len < VQEC_CLI_MAXSIZE_PRINTBUF)) {
            if ((index + 1) == VQEC_CLI_MAXSIZE_PRINTBUF) {
                break;
            } else {
                temp_string[index++] = *ptr;
            }
            if (*ptr == '\n') {
                replace_flag = TRUE;
                if ((index + 1) ==  VQEC_CLI_MAXSIZE_PRINTBUF) {
                    break;
                } else {
                    temp_string[index++] = '\r';
                }
            }
            ptr++;
            len++;
        }
        
        temp_string[VQEC_CLI_MAXSIZE_PRINTBUF - 1] = '\0';
        if (replace_flag) {
            fprintf(cli->client, "%s\n\r", temp_string);
        } else {
            fprintf(cli->client, "%s\n\r", string);
        }
        free(temp_string);
    }
}

UT_STATIC unsigned int vqec_set_sc_options_flag (char *argv[], int argc, 
                                                 struct vqec_cli_def *cli)
{   
    unsigned int options_flag = 0; 
    char *PARAMS1[] = { SHOW_C_CLI_ARG_COUNTERS, SHOW_C_CLI_ARG_CONFIG,
                        (char *)0 };
    char *PARAMS2[] = { SHOW_C_CLI_ARG_ALL, 
                        SHOW_C_CLI_ARG_URL, 
                       (char *)0 };
    char *PARAMS3[] = { SHOW_C_CLI_ARG_CUMULATIVE,
                        (char *)0 };
    vqec_protocol_t protocol;
    in_addr_t ip;
    uint16_t port;
    int val, val2, val3;

    if (argc == 0) {
        options_flag |= SC_LIST_MASK;
        goto done;
    }

    val = vqec_str_match(argv[ 0 ], PARAMS1);
    switch (val) {
        case SCCLIARG_COUNTERS: {
            if (argc == 1)  {
                /*
                 *  We do not support the CUMULATIVE counters at this
                 *  point, but may do so in the future. Thus for now,
                 *  we will add the error mask
                 */
                options_flag |= SC_ERROR_MASK;
                goto done;
            } else {
                val2 = vqec_str_match( argv[ 1 ], PARAMS2);
                switch (val2) {
                    case SCCLIARG_ALL: {
                        if (argc == 3) {
                            val3 = vqec_str_match (argv[ 2 ], PARAMS3);
                            if (val3 == SCCLIARG_CUMULATIVE) {
                                options_flag |= SC_COUNTERS_CUMULATIVE_MASK;
                            } else {
                                options_flag |= SC_ERROR_MASK;
                            }
                        }
                        if (argc > 3) {
                            options_flag |= SC_ERROR_MASK;
                        }
                        options_flag |= SC_COUNTERS_ALL_MASK;
                        break;
                    }
                    case SCCLIARG_URL: {
                        if (argc > 4) {
                           options_flag |= SC_ERROR_MASK;
                           goto done;
                        }
                        /* Check if argv[2] is a url */
                        if (vqec_url_parse((vqec_url_t) argv[2], &protocol,
                                           &ip, &port)) {
                            options_flag |= SC_COUNTERS_URL_MASK;
                        } else {
                            options_flag |= SC_ERROR_MASK;
                            goto done;
                        }
                        if (argc == 4) {
                            val3 = vqec_str_match (argv[ 3 ], PARAMS3);
                            if (val3 == SCCLIARG_CUMULATIVE) {
                                options_flag |= SC_COUNTERS_CUMULATIVE_MASK;
                            } else {
                                options_flag |= SC_ERROR_MASK;
                            }
                        }

                        break;
                    }
                    default: {
                        options_flag |= SC_ERROR_MASK;
                        goto done;
                    }
                }
                break;
            }
        }
        case SCCLIARG_CONFIG: {
            if (argc == 1) {
                /* Nothing is specified after the keyword "config" */
                options_flag |= SC_ERROR_MASK;
                goto done;
            }
            val2 = vqec_str_match( argv[ 1 ], PARAMS2);
            switch (val2) {
                case SCCLIARG_ALL:
                    options_flag |= SC_CONFIG_ALL_MASK;
                    break;
                case SCCLIARG_URL: {
                    /* Check if argv[2] is a url */
                    if (argc == 3 && (vqec_url_parse((vqec_url_t) argv[ 2 ],
                                                     &protocol, &ip, &port))) {
                        options_flag |= SC_CONFIG_URL_MASK;
                    } else {
                        options_flag |= SC_ERROR_MASK;
                        return options_flag;
                    }
                    break;
                }
                default: {
                    options_flag |= SC_ERROR_MASK;
                    goto done;
                }
            }
            break;
        }
        default:
            options_flag |= SC_ERROR_MASK;
            break;
    }
done:
    return options_flag;
}

UT_STATIC unsigned int vqec_set_st_options_flag (char *argv[], int argc, 
                                                 struct vqec_cli_def *cli, 
                                                 vqec_tunerid_t *ptr_tuner_id)
{   
    unsigned int options_flag = 0; 
    int argc_count;
    int argc_count_base = 1;
    char *PARAMS1[] = { SHOW_T_CLI_ARG_NAME, SHOW_T_CLI_ARG_ALL, 
                        SHOW_T_CLI_ARG_JOINDELAY, (char *)0 };
    char *PARAMS2[] = { SHOW_T_CLI_ARG_BRIEF, SHOW_T_CLI_ARG_PCM, 
                        SHOW_T_CLI_ARG_CHANNEL, SHOW_T_CLI_ARG_COUNTERS,
                        SHOW_T_CLI_ARG_LOG, SHOW_T_CLI_ARG_FEC,
                        SHOW_T_CLI_ARG_NAT, SHOW_T_CLI_ARG_RCC, 
                        SHOW_T_CLI_ARG_IPC, 
                        SHOW_T_CLI_ARG_DETAIL, (char *)0 };
    if (argc < 1) {
        options_flag |= TUNER_NAME_ERROR_MASK;
        return options_flag;
    }

    /* argc[0] is the argument following "tuner"
     * 'join-delay' specifies the join-delay histogram
     * 'all' specifies all tuners
     */
    switch (vqec_str_match(argv[ 0 ], PARAMS1)) {
        case STCLIARG_ALL:
            options_flag |= ALL_TUNERS_MASK;
            break;
        case STCLIARG_NAME: 
            if (argc == 1) {
                options_flag |= TUNER_NAME_ERROR_MASK;
                return options_flag;
            }
            if (!(vqec_ifclient_check_tuner_validity_by_name(argv[ 1 ]))) {
                options_flag |= TUNER_NAME_ERROR_MASK;
                return options_flag;
            } else {
                *ptr_tuner_id = vqec_ifclient_tuner_get_id_by_name(argv[ 1 ]);
                options_flag |= SPECIFIC_TUNER_MASK;
                argc_count_base += 1;
            }
            break;
        case STCLIARG_JOINDELAY:
            if (argc > 1) {
                return ERROR_MASK;
            }
            return JOINDELAY_MASK;
        default:
            options_flag |= TUNER_NAME_ERROR_MASK;
            return options_flag;
    }

    for (argc_count = argc_count_base; argc_count < argc; argc_count++) {
        switch(vqec_str_match(argv[ argc_count ], PARAMS2)) {
            case STCLIARG_BRIEF:
                options_flag |= BRIEF_MASK;
                break;
            case STCLIARG_DETAIL: 
                options_flag |= DETAIL_MASK;
                break;
            case STCLIARG_PCM:
                options_flag |= PCM_MASK;
                break;
            case STCLIARG_COUNTERS: 
                options_flag |= COUNTERS_MASK;
                break;
            case STCLIARG_CHANNEL: 
                options_flag |= CHANNEL_MASK;
                break;
            case STCLIARG_LOG:
                options_flag |= LOG_MASK;
                break;
            case STCLIARG_FEC:
                options_flag |= FEC_MASK;
                break;
            case STCLIARG_NAT:
                options_flag |= NAT_MASK;
                break;
            case STCLIARG_RCC:
                options_flag |= RCC_MASK;
                break;
            case STCLIARG_IPC:
                options_flag |= IPC_MASK;
                break;
            default:
                options_flag |= ERROR_MASK;       
                return options_flag;
        }
    }
    
    /* if no options are specified brief is assumed.*/
    if ((options_flag & ALL_OPTIONS_MASK)==0) {   
        options_flag |= BRIEF_MASK;
    }
    return options_flag;
}

UT_STATIC void 
vqec_cmd_show_counters_dump (struct vqec_cli_def *cli,
                             const char *name,
                             const vqec_ifclient_common_stats_t *stats)
{       
    if (name) {
        /* Show legacy, per-tuner stats */
        vqec_cli_print(cli, "tuner-name:      %s", name);
        vqec_cli_print(cli, " inputs:         %llu",
                       stats->primary_rtp_inputs + stats->repair_rtp_inputs +
                       stats->primary_rtcp_inputs + stats->repair_rtcp_inputs);
        vqec_cli_print(cli, " drops:          %llu",
                       stats->primary_rtp_drops + stats->repair_rtp_drops);
        vqec_cli_print(cli, " prim udp mpeg:  %llu", stats->primary_udp_inputs);
        vqec_cli_print(cli, " primary rtp:    %llu", stats->primary_rtp_inputs);
        vqec_cli_print(cli, " primary rtcp:   %llu", 
                       stats->primary_rtcp_inputs);
        vqec_cli_print(cli, " repair rtp:     %llu", stats->repair_rtp_inputs);
        vqec_cli_print(cli, " repair rtcp:    %llu", stats->repair_rtcp_inputs);
#ifdef HAVE_FCC
        vqec_cli_print(cli, " app timeouts:   %d",
                       stats->rcc_abort_stats[
                       VQEC_IFCLIENT_RCC_ABORT_RESPONSE_TIMEOUT]);
        vqec_cli_print(cli, " null app:       %d",
                       stats->rcc_abort_stats[
                       VQEC_IFCLIENT_RCC_ABORT_SERVER_REJECT]);
#endif
        /*
         * NOTE:  The value displayed as "outputs" is based on outputs from
         *        the tuner queue.  Since the vqec_ifclient_stats_t
         *        structure does not define a field for such a counter, 
         *        the post_repair_outputs field is used to hold the
         *        tuner queue outputs in this special case of support
         *        for the legacy "show counters <tuner>" display.
         */
        vqec_cli_print(cli, " outputs:        %llu", 
                       stats->post_repair_outputs);
        vqec_cli_print(cli, " output Q drops: %llu", 
                       stats->tuner_queue_drops);
        /* added for use by FEC */
        vqec_cli_print(cli, " fec repairs:    %llu", 
                       stats->fec_recovered_paks);
        return;
    }

    /* Inputs */
    vqec_cli_print(cli, "\n-- Stream Packet Counters --");
    vqec_cli_print(cli,
                 "                   Inputs        Outputs       Drops (Late)");
    vqec_cli_print(cli,
                   "primary udp mpeg:  "
                                        VQEC_CNT1     VQEC_CNT1     VQEC_CNT1,
                   stats->primary_udp_inputs,
                   (stats->primary_udp_inputs - stats->primary_udp_drops),
                   stats->primary_udp_drops);
    vqec_cli_print(cli,
                   "primary rtp:       "
                                        VQEC_CNT1     VQEC_NA       VQEC_CNT2,
                   stats->primary_rtp_inputs,
                   stats->primary_rtp_drops,
                   stats->primary_rtp_drops_late);
    vqec_cli_print(cli,
                   "primary rtcp:      "
                                        VQEC_CNT1     VQEC_CNT1     VQEC_NA,
                   stats->primary_rtcp_inputs,
                   stats->primary_rtcp_outputs);
    vqec_cli_print(cli, "repair/rcc rtp:    "
                                        VQEC_CNT1     VQEC_NA       VQEC_CNT2,
                   stats->repair_rtp_inputs,
                   stats->repair_rtp_drops,
                   stats->repair_rtp_drops_late);
    vqec_cli_print(cli, "repair/rcc rtcp:   "
                                        VQEC_CNT1     VQEC_NA       VQEC_NA,
                   stats->repair_rtcp_inputs);
    vqec_cli_print(cli, "fec:               "
                                        VQEC_CNT1     VQEC_NA       VQEC_CNT2,
                   stats->fec_inputs,
                   stats->fec_drops,
                   stats->fec_drops_late);
    vqec_cli_print(cli, "repair rtp stun:   "
                                        VQEC_CNT1     VQEC_CNT1     VQEC_NA,
                   stats->repair_rtp_stun_inputs,
                   stats->repair_rtp_stun_outputs);
    vqec_cli_print(cli, "repair rtcp stun:  "
                                        VQEC_CNT1     VQEC_CNT1     VQEC_NA,
                   stats->repair_rtcp_stun_inputs,
                   stats->repair_rtcp_stun_outputs);
    vqec_cli_print(cli, "post-repair:       "
                                        VQEC_NA       VQEC_CNT1     VQEC_NA,
                   stats->post_repair_outputs);
    vqec_cli_print(cli, "tuner Q drops:     "
                                        VQEC_NA       VQEC_NA       VQEC_CNT1,
                   stats->tuner_queue_drops);
    vqec_cli_print(cli, "underruns:         "
                                        VQEC_CNT1     VQEC_NA       VQEC_NA,
                   stats->underruns);

    vqec_cli_print(cli, "\n-- Repair Packet Counters --");
    vqec_cli_print(cli, "                   Pre-Repair    Post-Repair");
    vqec_cli_print(cli, "stream loss:       "
                                        VQEC_CNT1     VQEC_CNT1,
                   stats->pre_repair_losses,
                   stats->post_repair_losses);
    vqec_cli_print(cli, " rcc loss:         "
                                        VQEC_NA       VQEC_CNT1,
                   stats->post_repair_losses_rcc);
    vqec_cli_print(cli, "                   Requested     Policed");
    vqec_cli_print(cli, "error repair:      "
                                        VQEC_CNT1     VQEC_CNT1,
                   stats->repairs_requested,
                   stats->repairs_policed);
    vqec_cli_print(cli, "                   Recovered");
    vqec_cli_print(cli, "fec:               "
                                       VQEC_CNT1,
                   stats->fec_recovered_paks);

    vqec_cli_print(cli, "\n-- Channel Change Counters --");
    vqec_cli_print(cli, "requests:          %d", 
                   stats->channel_change_requests);
#ifdef HAVE_FCC
    vqec_cli_print(cli, " rcc requests:     %d", stats->rcc_requests);
    vqec_cli_print(cli, "  rccs limited:    %d", stats->concurrent_rccs_limited);
    vqec_cli_print(cli, "  rcc with loss:   %d", stats->rcc_with_loss);
    vqec_cli_print(cli, "  rcc aborts:      %d", stats->rcc_aborts_total);
    vqec_cli_print(cli,
            "    server    stun      response  response  burst     burst");
    vqec_cli_print(cli,
            "    rejects   timeout   timeout   invalid   start     activity"
            "  other");
    vqec_cli_print(cli, "    "
            VQEC_CNT3 VQEC_CNT3 VQEC_CNT3 VQEC_CNT3 VQEC_CNT3 VQEC_CNT3
            VQEC_CNT3,
            stats->rcc_abort_stats[VQEC_IFCLIENT_RCC_ABORT_SERVER_REJECT],
            stats->rcc_abort_stats[VQEC_IFCLIENT_RCC_ABORT_STUN_TIMEOUT],
            stats->rcc_abort_stats[VQEC_IFCLIENT_RCC_ABORT_RESPONSE_TIMEOUT],
            stats->rcc_abort_stats[VQEC_IFCLIENT_RCC_ABORT_RESPONSE_INVALID],
            stats->rcc_abort_stats[VQEC_IFCLIENT_RCC_ABORT_BURST_START],
            stats->rcc_abort_stats[VQEC_IFCLIENT_RCC_ABORT_BURST_ACTIVITY],
            stats->rcc_abort_stats[VQEC_IFCLIENT_RCC_ABORT_OTHER]);

#endif
}

UT_STATIC int vqec_show_tuner_output (struct vqec_cli_def *cli, 
                                      unsigned int options_flag, 
                                      vqec_tunerid_t vqec_tuner_id)
{
    vqec_tuner_iter_t iter;
    vqec_tunerid_t id = VQEC_TUNERID_INVALID;

    if ((options_flag & ALL_TUNERS_MASK)!=0) {
        FOR_ALL_TUNERS_IN_LIST(iter, id) {
            (void)vqec_ifclient_tuner_dump_state(id, options_flag);
        }
    } else if ((options_flag & SPECIFIC_TUNER_MASK)!=0) {  
        if (vqec_ifclient_tuner_dump_state(vqec_tuner_id, options_flag) 
                != VQEC_OK) {
            vqec_cli_print(cli, "Error in tuner dump state!");
            return VQEC_CLI_ERROR;
        }
    } else if (options_flag & JOINDELAY_MASK) {
        if (vqec_ifclient_histogram_display(VQEC_HIST_JOIN_DELAY) != VQEC_OK) {
            vqec_cli_print(cli, "Error in displaying histogram");
            return VQEC_CLI_ERROR;
        }
    } else {
        /**
         * The command failed to match one in which:
         *   - all tuners was selected
         *   - a specific tuner was selected
         *   - the "join-delay" keyword was added.
         * This is not possible. Thus, we must report an error.
         */
        vqec_cli_print(cli, "ERROR: Ambiguous or improperly specified options");
        return VQEC_CLI_ERROR;
    }
    return VQEC_CLI_OK;
}

/*
 * COMMAND FUNCTIONS FOR 'SHOW' COMMANDS
 */

UT_STATIC int vqec_cmd_show_tuner (struct vqec_cli_def *cli, char *command, 
                                   char *argv[], int argc)
{
    vqec_tunerid_t id = VQEC_TUNERID_INVALID;
    int retVal;
    unsigned int options_flag;

    if (vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli,
                       "Usage: show tuner { {all | name <tuner-name>} "
                       "[[brief] [pcm] [fec] [nat]\n"
                       "                        [counters] [channel] "
                       "[log] [ipc] [rcc] [detail]] |\n"
                       "                    join-delay }");
        return VQEC_CLI_ERROR;
    }
    
    options_flag = vqec_set_st_options_flag(argv, argc, cli, &id);
    if ((options_flag & ERROR_MASK) != 0) {
        if ((options_flag ^ TUNER_NAME_ERROR_MASK) == 0) {
            vqec_cli_print(cli, "Incorrect/Missing Tuner Name");
        } else {
            vqec_cli_print(cli, "Unidentified argument(s)");
        }
        vqec_cli_print(cli,
                       "Usage: show tuner { {all | name <tuner-name>} "
                       "[[brief] [pcm] [fec] [nat]\n"
                       "                        [counters] [channel] "
                       "[log] [ipc] [rcc] [detail]] |\n"
                       "                    join-delay }");
        return VQEC_CLI_ERROR;
    }

    retVal = vqec_show_tuner_output(cli, options_flag, id);
    if (retVal == VQEC_CLI_ERROR) {
        vqec_cli_print(cli, "Error in Show Tuner");
        return VQEC_CLI_ERROR;
    }
    return VQEC_CLI_OK;
}

#define INFINITY_U64 (-1)

/* Helper function that displays the channel stats associated with the url */

int cli_display_chan_stats (char *chan_url, struct vqec_cli_def *cli, 
                            boolean cumulative)
{
    vqec_ifclient_stats_channel_t channel_stats;
    vqec_error_t err;

    if (cumulative) {
        err = vqec_ifclient_get_stats_channel_cumulative(chan_url, 
                                                         &channel_stats);
    } else {
        err = vqec_ifclient_get_stats_channel(chan_url, &channel_stats);
    }
    if (err == VQEC_OK) {
          vqec_cli_print(cli, "primary rtp expected:    %lld\n",
                         channel_stats.primary_rtp_expected);
          vqec_cli_print(cli, "primary rtp lost:        %lld\n",
                         channel_stats.primary_rtp_lost);

          vqec_cli_print(cli, "\n-- TR-135 Packet Counters --");
          vqec_cli_print(cli, "buffersize (usec):              %llu",
                         channel_stats.tr135_buffer_size);
          vqec_cli_print(cli, "underruns (events):             %llu", 
                         channel_stats.tr135_underruns);
          vqec_cli_print(cli, "overruns (events):              %llu",
                         channel_stats.tr135_overruns);
          vqec_cli_print(cli, "gmin:                           %u",
                         channel_stats.tr135_gmin);
          vqec_cli_print(cli, "severe loss minimum distance:   %u ",
                         channel_stats.tr135_severe_loss_min_distance);
          vqec_cli_print(cli, "                            Before-EC     "
                                 "After-EC");
          vqec_cli_print(cli, "packets expected:           "
                                                      VQEC_NA       VQEC_CNT1,
                         channel_stats.tr135_packets_expected);
          vqec_cli_print(cli, "packets received:           "
                                                      VQEC_NA       VQEC_CNT1, 
                         channel_stats.tr135_packets_received);
          vqec_cli_print(cli, "packets lost:               "
                                                      VQEC_CNT1     VQEC_CNT1,
                         channel_stats.tr135_packets_lost_before_ec,
                         channel_stats.tr135_packets_lost);
          vqec_cli_print(cli, "loss events:                "
                                                      VQEC_CNT1     VQEC_CNT1,
                         channel_stats.tr135_loss_events_before_ec,
                         channel_stats.tr135_loss_events);
          vqec_cli_print(cli, "severe loss index count:    "
                                                      VQEC_NA       VQEC_CNT1,
                         channel_stats.tr135_severe_loss_index_count);
          if (channel_stats.tr135_minimum_loss_distance != INFINITY_U64) {
              vqec_cli_print(cli, "minimum loss distance:      "
                                                        VQEC_NA       VQEC_CNT1, 
                           channel_stats.tr135_minimum_loss_distance);
          } else {
              vqec_cli_print(cli, "minimum loss distance:      "
                                                        VQEC_NA       VQEC_NA); 
          }
          vqec_cli_print(cli, "maximum loss period:        "
                                                      VQEC_NA       VQEC_CNT1,
                    channel_stats.tr135_maximum_loss_period);
          vqec_cmd_show_counters_dump(cli, NULL,
                               (vqec_ifclient_common_stats_t *) &channel_stats);
    } else {
        return VQEC_CLI_ERROR;
    }
    return VQEC_CLI_OK;
}

boolean build_chan_url_from_idx (int index, char *chan_url_string, 
                                 channel_cfg_t **ptr_chan)
{
    boolean url_build_success = FALSE;

    /* Retrieve both input and output info based on the handle */
    *ptr_chan = cfg_get_channel_cfg_from_idx(index);        
    if (*ptr_chan) {
        switch ((*ptr_chan)->source_proto) {
            case  UDP_STREAM:
                url_build_success = 
                    vqec_url_build(VQEC_PROTOCOL_UDP,
                                   (*ptr_chan)->original_source_addr.s_addr,
                                   (*ptr_chan)->original_source_port,
                                   chan_url_string,
                                   VQEC_MAX_URL_LEN);
                break;

            case RTP_STREAM:
                url_build_success = 
                        vqec_url_build(VQEC_PROTOCOL_RTP,
                                       (*ptr_chan)->original_source_addr.s_addr,
                                       (*ptr_chan)->original_source_port,
                                       chan_url_string,
                                       VQEC_MAX_URL_LEN);
                break;
            default:
                url_build_success = FALSE;
                break;
        }
    }
    return url_build_success;
} 

UT_STATIC int vqec_cmd_show_channel (struct vqec_cli_def *cli, 
                                     char *command, 
                                     char *argv[], int argc)
{
#define CHAN_STATUS_STRLEN 1024
    /* The '(' is used for grouping in this variant of the BNF*/
    char *usage_string = "Usage: show channel [ {counters {all | url "
                         "<channel_url>} [cumulative]} | {config {all | url "
                         "<channel-url>}} ]";
    uint16_t i;
    channel_cfg_t * chan = NULL;
    vqec_chan_cfg_t chan_cfg;
    vqec_chanid_t temp_chan;
    vqec_chan_err_t chan_retval;
    vqec_url_t chan_input_string;
    vqec_protocol_t chan_protocol;
    in_addr_t chan_addr;
    uint16_t chan_port;
    struct in_addr chan_ip_addr;
    uint16_t total_channels;
    char chan_string[VQEC_MAX_URL_LEN];
    boolean url_build_success;
    unsigned int options_flag;
    int retval;
    vqec_ifclient_config_status_params_t params;
    boolean cumulative =  FALSE;

    if (vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, usage_string);
        return VQEC_CLI_ERROR;
    }

    options_flag = vqec_set_sc_options_flag(argv, argc, cli);
    if ((options_flag & SC_COUNTERS_CUMULATIVE_MASK) == 
        SC_COUNTERS_CUMULATIVE_MASK) {
        cumulative = TRUE;
    }
    if ((options_flag & SC_ERROR_MASK) == SC_ERROR_MASK) {
        vqec_cli_print(cli, usage_string);
        return VQEC_CLI_ERROR;
    } else if ((options_flag & SC_LIST_MASK) == SC_LIST_MASK) {
        params.config = VQEC_IFCLIENT_CONFIG_CHANNEL;
        (void)vqec_ifclient_config_status(&params);
        vqec_cli_print(cli,
                  "Last update received:       %s\n"
                  "Channel cfg file checksum:  %s",
                  params.last_update_timestamp,
                  params.md5);

        total_channels = cfg_get_total_num_channels();
        vqec_cli_print(cli, "Total number of channels:   %d\n", 
        cfg_get_total_num_active_channels());

        for (i = 0; i < total_channels; i++) {
            url_build_success = build_chan_url_from_idx(i, chan_string, &chan);
            if (url_build_success) {
                vqec_cli_print(cli, " %s (%s)", chan_string, chan->name);
            }
        }
        return VQEC_CLI_OK;
    } else if ((options_flag & SC_CONFIG_MASK) == SC_CONFIG_MASK) {
        if ((options_flag & SC_CONFIG_URL_MASK) == SC_CONFIG_URL_MASK) {
            /* LINEUP URL requested */
            chan_input_string = argv[2];
            if (!vqec_url_parse(chan_input_string,
                                &chan_protocol,
                                &chan_addr,
                                &chan_port)) {
                vqec_cli_print(cli, "%s", usage_string);
                return VQEC_CLI_ERROR;
            } /* intentionally NOT requiring RTP as the entered protocol */
            chan_ip_addr.s_addr = chan_addr;
            chan = cfg_get_channel_cfg_from_orig_src_addr(chan_ip_addr, 
                                                          chan_port);
            if (chan) {
                vqec_channel_cfg_printf(chan);
            } else {
                vqec_cli_print(cli, "Invalid channel!");
                return VQEC_CLI_ERROR;
            }
            return VQEC_CLI_OK;
        } else if ((options_flag & SC_CONFIG_ALL_MASK) == SC_CONFIG_ALL_MASK) {
            /* All config descriptions requested */
            total_channels = cfg_get_total_num_channels();

            for (i = 0; i < total_channels; i++) {
                url_build_success = build_chan_url_from_idx (i, chan_string, 
                                                             &chan);
                if (url_build_success) {
                    chan_input_string = chan_string;
                    if (!vqec_url_parse(chan_input_string,
                                        &chan_protocol,
                                        &chan_addr,
                                        &chan_port)) {
                        vqec_cli_print(cli, "%s", usage_string);
                    }
                    chan_ip_addr.s_addr = chan_addr;
                    chan = cfg_get_channel_cfg_from_orig_src_addr(chan_ip_addr, 
                                                                  chan_port);
                    if (chan) {
                        vqec_channel_cfg_printf(chan);
                        vqec_cli_print(cli, "\n");
                    }
                } 
            }
        }
        return VQEC_CLI_OK;
    } else if ((options_flag & SC_COUNTERS_MASK) == SC_COUNTERS_MASK) {
        if ((options_flag & SC_COUNTERS_URL_MASK) == 
            SC_COUNTERS_URL_MASK) {
            /* Stats for a particular channel url requested */
            chan_input_string = argv[2];
            if (!vqec_url_parse(chan_input_string,
                                &chan_protocol,
                                &chan_addr,
                                &chan_port)) {
                vqec_cli_print(cli, "%s", usage_string);
                return VQEC_CLI_ERROR;
            }

            /*
             * API caller's supplied primary stream address and port are 
             * translated from (original dest addr, original dest port) to 
             * (re-sourced dest addr, re-sourced dest port) if such a 
             * re-sourced stream is present in the channel manager.
             */
            chan_ip_addr.s_addr = chan_addr;
            chan = cfg_get_channel_cfg_from_orig_src_addr(chan_ip_addr, 
                                                          chan_port);
            if (chan) {
                vqec_chan_convert_sdp_to_cfg(chan, 
                                             VQEC_CHAN_TYPE_LINEAR, 
                                             &chan_cfg);
                /* Determine if channel exists before displaying stats */
                chan_retval = vqec_chan_find_chanid(
                                      &temp_chan,
                                      chan_cfg.primary_dest_addr.s_addr,
                                      chan_cfg.primary_dest_port);
            } else {
                chan_retval = vqec_chan_find_chanid(&temp_chan,
                                                    chan_addr,
                                                    chan_port);
            } 
            if (chan_retval == VQEC_CHAN_ERR_OK) {
                retval = cli_display_chan_stats(argv[2], cli, cumulative);
                return retval;
            } else {
                vqec_cli_print(cli, "Invalid channel or channel not active");
                return VQEC_CLI_ERROR;
            }
        } else if ((options_flag & SC_COUNTERS_ALL_MASK) == 
                   SC_COUNTERS_ALL_MASK) {
            /* Display the counters for all channels iteratively */
            total_channels = cfg_get_total_num_channels();
            for (i = 0; i < total_channels; i++) {
                url_build_success = build_chan_url_from_idx (i, chan_string,
                                                             &chan);
                if (url_build_success) {
                    /*
                     * To support re-sourced streams, the primary 
                     * stream address and port in the 'chan' variable are
                     * translated from (original dest addr, original dest port)
                     * to (re-sourced dest addr, re-sourced dest port) 
                     * if such a re-sourced stream is present in the channel 
                     * manager 
                     */
                    if (chan) {
                        vqec_chan_convert_sdp_to_cfg(chan, 
                                                     VQEC_CHAN_TYPE_LINEAR, 
                                                     &chan_cfg);
                        chan_retval = vqec_chan_find_chanid(
                                          &temp_chan,
                                          chan_cfg.primary_dest_addr.s_addr,
                                          chan_cfg.primary_dest_port);
                    } else {
                        if (!vqec_url_parse((vqec_url_t)chan_string, 
                                            &chan_protocol, 
                                            &chan_addr, &chan_port)) {
                            vqec_cli_print(cli, "URL parse error: %s", 
                                              chan_string);
                            continue;
                        }
                        chan_retval = vqec_chan_find_chanid(&temp_chan,
                                                            chan_addr,
                                                            chan_port);
                    }
                    /* Check if any tuner is bound to this channel */
                    if (chan_retval == VQEC_CHAN_ERR_OK) {
                        vqec_cli_print(cli, "Channel : %s (%s)", chan_string, 
                                  chan->name);
                        retval = cli_display_chan_stats(chan_string, cli, 
                                                        cumulative);
                        vqec_cli_print(cli, "\n");
                    }
                }
            }
        }  
    }
    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_show_drop (struct vqec_cli_def *cli, char *command, 
                                  char *argv[], int argc)
{
    if (argc > 0) {
        vqec_cli_print(cli, "Usage: show drop");
        return VQEC_CLI_ERROR;
    }

    vqec_drop_dump();

    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_show_update(struct vqec_cli_def *cli, char *command, 
                                   char *argv[], int argc)
{
    if (argc > 0) {
        vqec_cli_print(cli, "Usage: show update");
        return VQEC_CLI_ERROR;
    }

    if (!vqec_updater_get_cdi_enable()) {
        CONSOLE_PRINTF("Cisco Delivery Infrastructure not enabled.");
        return VQEC_CLI_OK;
    }

    vqec_updater_show();

    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_show_error_repair (struct vqec_cli_def *cli, 
                                          char *command, 
                                          char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: show error-repair");
        return VQEC_CLI_ERROR;
    }

    vqec_error_repair_show(cli);

    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_show_fec (struct vqec_cli_def *cli, char *command, 
                                 char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: show fec");
        return VQEC_CLI_ERROR;
    }

    vqec_cli_fec_show();

    return VQEC_CLI_OK;
}

#if HAVE_FCC
UT_STATIC int vqec_cmd_show_rcc (struct vqec_cli_def *cli, char *command, 
                                 char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: show rcc");
        return VQEC_CLI_ERROR;
    }

    vqec_rcc_show(cli);

    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_show_fast_fill (struct vqec_cli_def *cli, 
                                       char *command, 
                                       char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: show fast-fill");
        return VQEC_CLI_ERROR;
    }

    vqec_fast_fill_show(cli);

    return VQEC_CLI_OK;
}
#endif /* HAVE_FCC */

UT_STATIC int vqec_cmd_show_pak_pool (struct vqec_cli_def *cli, 
                                      char *command, 
                                      char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: show pak-pool");
        return VQEC_CLI_ERROR;
    }

    vqec_cli_pak_pool_dump_safe();

    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_show_counters (struct vqec_cli_def *cli, 
                                      char *command,
                                      char *argv[], int argc)
{
    vqec_ifclient_stats_t global_stats;
    vqec_ifclient_stats_channel_t channel_stats;
    vqec_dp_global_debug_stats_t dp_stats;
    vqec_error_t err;
    vqec_tunerid_t id = VQEC_TUNERID_INVALID;
    int retval;
    boolean tuner_valid;

    char *PARAMS1[] = { "name", 
                        "cumulative", 
                        (char *)0 };

    if ((argc > 2) || vqec_check_args_for_help_char(argv, argc)) {
        goto done;
    }

    if (argc==1) {
        retval = vqec_str_match(argv[ 0 ], PARAMS1);
        if (retval == 0 || retval==VQEC_CLI_ERROR) {
            /* retval=0, argc=1. 'name' specified but <tuner_name> missing. */
            goto done;
        } 
        /* Get global/historical cumulative counters */
        err = vqec_ifclient_get_stats_cumulative(&global_stats);
        if (err != VQEC_OK) {
            return VQEC_CLI_ERROR;
        }
        vqec_cmd_show_counters_dump(cli, NULL, 
                                    (vqec_ifclient_common_stats_t *)
                                    &global_stats);
    } else if (argc == 2) {
        retval = vqec_str_match(argv[ 0 ], PARAMS1);
        if (retval == 1 || retval==VQEC_CLI_ERROR) {
            /*
             * retval=1, argc=2. 'cumulative' specified but followed 
             * by unknown keyword. 
             */
            goto done;
        } else {
            tuner_valid = vqec_ifclient_check_tuner_validity_by_name(argv[1]);
            if (tuner_valid) {
                id = vqec_ifclient_tuner_get_id_by_name(argv[1]);
                err = vqec_ifclient_get_stats_tuner_legacy(id, &channel_stats);
                if (err != VQEC_OK) {
                    vqec_cli_print(cli, "vqec_ifclient_get_stats_tuner_legacy "
                                   "failed (%s)!",
                              vqec_err2str(err));
                   return VQEC_CLI_ERROR;
                }
            } else {
                vqec_cli_print(cli, "Incorrect/Missing Tuner Name");
                goto done;
            }
            vqec_cmd_show_counters_dump(cli, argv[1], 
                  (vqec_ifclient_common_stats_t *) &channel_stats);
        }
    } else if (argc==0) {
        /* Get global/historical counters */
        err = vqec_ifclient_get_stats(&global_stats);
        if (err != VQEC_OK) {
            return VQEC_CLI_ERROR;
        }
        vqec_cmd_show_counters_dump(cli, NULL, 
                                    (vqec_ifclient_common_stats_t *)
                                    &global_stats);
        if (vqec_dp_get_global_counters(&dp_stats) == VQEC_DP_ERR_OK) {
            vqec_cli_print(cli, "\n-- System Event Counters --");
            vqec_cli_print(cli, "clock jumpbacks:   %d", 
                           dp_stats.system_clock_jumpbacks);
        }
    }
    return VQEC_CLI_OK;

done:
    vqec_cli_print(cli, "Usage: show counters [ {name <tuner-name>} | " 
                   "{cumulative} ]");
    return VQEC_CLI_ERROR;

}

#define VQEC_DP_IPC_GET_DEBUG_FLAG(name, flag)                          \
    ({                                                                  \
        *flag = FALSE;                                                  \
        (void)vqec_dp_debug_flag_get(name, flag);                       \
        *flag;                                                          \
    })

UT_STATIC int vqec_cmd_show_debug (struct vqec_cli_def *cli, char *command,
                                   char *argv[], int argc)
{
    boolean flag;
    if (argc > 0) {
        vqec_cli_print(cli, "Usage: show debug");
        return VQEC_CLI_ERROR;
    }

    vqec_cli_print(cli, "channel:           %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CHANNEL) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "chan_cfg:          %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CHAN_CFG) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "cpchan:            %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CPCHAN) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "error-repair:      %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_ERROR_REPAIR) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "event:             %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_EVENT) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "rcc:               %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "igmp:              %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_IGMP) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "input:             %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_INPUT) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "output:            %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_OUTPUT) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "pcm:               %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_PCM) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "recv-socket:       %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RECV_SOCKET) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "rtcp:              %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RTCP) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "nat :              %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_NAT) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "timer:             %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TIMER) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "tuner:             %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TUNER) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "upcall:            %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_UPCALL) ? 
                   "enabled" : "disabled");
    vqec_cli_print(cli, "updater:           %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_UPDATER) ? 
                   "enabled" : "disabled");
   vqec_cli_print(cli, "vod:               %s", 
                   VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_VOD) ? 
                   "enabled" : "disabled");

    vqec_cli_print(cli, "dp-tlm:            %s", 
                   VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_TLM, &flag) ?
                   "enabled" : "disabled");
    vqec_cli_print(cli, "dp-inputshim:      %s", 
                   VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_INPUTSHIM, &flag) ?
                   "enabled" : "disabled");
    vqec_cli_print(cli, "dp-outputshim:     %s", 
                   VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_OUTPUTSHIM, &flag) ?
                   "enabled" : "disabled");
    vqec_cli_print(cli, "dp-outputshim-pak: %s", 
                   VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_OUTPUTSHIM_PAK,
                                              &flag) ?
                   "enabled" : "disabled");
    vqec_cli_print(cli, "dp-nll:            %s", 
                   VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_NLL, &flag) ?
                   "enabled" : "disabled");
    vqec_cli_print(cli, "dp-nll-adjust:     %s", 
                   VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_NLL_ADJUST, &flag) ?
                   "enabled" : "disabled");
    vqec_cli_print(cli, "dp-pcm:            %s", 
                   VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_PCM, &flag) ?
                   "enabled" : "disabled");
    vqec_cli_print(cli, "dp-pcm-pak:        %s", 
                   VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_PCM_PAK, &flag) ?
                   "enabled" : "disabled");
    vqec_cli_print(cli, "dp-error-repair:   %s", 
                VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_ERROR_REPAIR, &flag) ?
                "enabled" : "disabled");
    vqec_cli_print(cli, "dp-rcc:            %s", 
                   VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_RCC, &flag) ?
                   "enabled" : "disabled");
    vqec_cli_print(cli, "dp-fec:            %s", 
                   VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_FEC, &flag) ?
                   "enabled" : "disabled");
    vqec_cli_print(cli, "dp-collect-stats:  %s", 
                VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS, &flag) ?
                "enabled" : "disabled");
    vqec_cli_print(cli, "dp-failover:       %s", 
                   VQEC_DP_IPC_GET_DEBUG_FLAG(VQEC_DP_DEBUG_FAILOVER, &flag) ?
                   "enabled" : "disabled");
    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_show_system_config (struct vqec_cli_def *cli, 
                                           char *command, 
                                           char *argv[], int argc)
{
    char *params[] = 
        { "start-up", "network", "defaults", "override", "?", (char *)0 };
    vqec_dp_op_mode_t opmode;
    int param;
    char *saveptr = NULL;
    char *usage_string = 
        "  start-up             Start-up file configuration\n"
        "  network              Per-STB network configuration\n"
        "  defaults             VQE-C parameter default values\n"
        "  override             Override configuration\n";

    /* handle "show system-config" */
    if (!argc) {
#ifdef VQEC_VAR_BUILD_TYPE
        vqec_cli_print(cli, "%s %d.%d.%d build %d (%s-build)",
                  VQEC_PRODUCT_ABREV, VQEC_PRODUCT_VERSION,
                  VQEC_MAJOR_VERSION, VQEC_MINOR_VERSION,
                  VQEC_VAR_BUILD_NUMBER, VQEC_VAR_BUILD_TYPE);
#else
        vqec_cli_print(cli, "%s %d.%d.%d build %d",
                  VQEC_PRODUCT_ABREV, VQEC_PRODUCT_VERSION,
                  VQEC_MAJOR_VERSION, VQEC_MINOR_VERSION,
                  VQEC_VAR_BUILD_NUMBER);
#endif /* VQEC_VAR_BUILD_TYPE */
        
#ifdef VQEC_VAR_BUILT_BY
        vqec_cli_print(cli, "Built by: %s", VQEC_VAR_BUILT_BY);
        vqec_cli_print(cli, "Workspace: %s", VQEC_VAR_WS_PATH);
#endif
        vqec_cli_print(cli, "Timestamp: %s %s", BUILD_DATE, BUILD_TIME);

        (void)vqec_dp_get_operating_mode(&opmode);
        if (opmode == VQEC_DP_OP_MODE_USERSPACE) {
            vqec_cli_print(cli, "Dataplane operating mode: USER");
        } else if (opmode == VQEC_DP_OP_MODE_KERNELSPACE) {
            vqec_cli_print(cli, "Dataplane operating mode: KERNEL");
        }

        vqec_syscfg_dump_system();
        return VQEC_CLI_OK;
    }

    /* handle "show system-config <arg>..." */
    switch (vqec_str_match(argv[0], params)) {
    case 0:
        if (argc > 1) {
            vqec_cli_print(cli, "Usage: show system-config %s", params[0]);
            return VQEC_CLI_ERROR;
        }
        vqec_syscfg_dump_startup();
        break;
    case 1:
        if (argc > 1) {
            vqec_cli_print(cli, "Usage: show system-config %s", params[1]);
            return VQEC_CLI_ERROR;
        }
        vqec_syscfg_dump_attributes();
        break;
    case 2:
        if (argc > 1) {
            vqec_cli_print(cli, "Usage: show system-config %s", params[2]);
            return VQEC_CLI_ERROR;
        }
        vqec_syscfg_dump_defaults();
        break;
    case 3:
        if (argc > 1) {
            vqec_cli_print(cli, "Usage: show system-config %s", params[3]);
            return VQEC_CLI_ERROR;
        }
        vqec_syscfg_dump_overrides();
        break;
    default:
        /*
         * If entered argument is prefix of command plus "?", give help
         * for just that command.  Otherwise, give help for full syntax.
         */
        param = vqec_str_match(strtok_r(argv[0], "?", &saveptr), params);
        switch (param) {
        case 0:
        case 1:
        case 2:
            vqec_cli_print(cli, "Usage: show system-config %s", params[param]);
            break;
        default:
            vqec_cli_print(cli, usage_string);
            break;
        }
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}

UT_STATIC int32_t
vqec_cmd_show_nat (struct vqec_cli_def *cli, char *command, 
                   char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: show nat");
        return VQEC_CLI_ERROR;
    }

    vqec_nat_fprint_all_bindings_safe();
    return VQEC_CLI_OK;
}

UT_STATIC int32_t
vqec_cmd_show_ipc (struct vqec_cli_def *cli, char *command, 
                   char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: show ipc");
        return VQEC_CLI_ERROR;
    }

    vqec_cli_show_ipc_status_safe();
    return VQEC_CLI_OK;
}

UT_STATIC int32_t
vqec_cmd_show_proxy_igmp (struct vqec_cli_def *cli, char *command,
                          char *argv[], int argc) 
{
    vqec_tunerid_t id = VQEC_TUNERID_INVALID;
    vqec_tuner_iter_t iter;
    char *usage_string = "Usage: show proxy-igmp [<tuner-name>], where, \n"
                         "If tuner-name is not specified, igmp proxy status"
                         " for all tuners is shown\n"
                         "ex: show proxy-igmp 0\n";
    if (vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, usage_string);
        return (VQEC_CLI_OK);
    }

    if (argc == 0) {
        FOR_ALL_TUNERS_IN_LIST(iter, id) {
            vqec_cmd_show_proxy_igmp_stats(id);
        }
    } else {               
        if ((id = vqec_ifclient_tuner_get_id_by_name(argv[0])) == 
            VQEC_TUNERID_INVALID) {
            vqec_cli_print(cli, "invalid tuner name!");
            vqec_cli_print(cli, usage_string);
            return (VQEC_CLI_ERROR);
        }
        vqec_cmd_show_proxy_igmp_stats(id);
    }
    
    return VQEC_CLI_OK;
}

UT_STATIC int32_t
vqec_cmd_show_dp (struct vqec_cli_def *cli, char *command, 
                  char *argv[], int argc)
{
    char *PARAMS[] = { "counters", "help", "?", (char *)0 };
    char *usage_string = "Usage: show dp [[counters]]";

    if ((argc < 1) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, usage_string);
        return VQEC_CLI_ERROR;
    }

    switch (vqec_str_match(argv[0], PARAMS)) {
    case 0:                     /* counters */
        vqec_cli_show_dp_global_counters_safe();
        break;
    case 1:                     /* help */
        /*FALLTHRU*/
    case 27:                    /* ? */
        /*FALLTHRU*/
    default:                
        vqec_cli_print(cli, usage_string);
        return VQEC_CLI_ERROR;        
    }

    return VQEC_CLI_OK;
}


/*
 * COMMAND FUNCTIONS FOR GLOBAL CONFIGURATION COMMANDS
 */

#if HAVE_FCC
UT_STATIC int vqec_cmd_app_delay (struct vqec_cli_def *cli, char *command,
                                  char *argv[], int argc)
{
    boolean rv = TRUE;
    int32_t app_cpy_delay;

    if (argc != 1 || strchr(command, '?') ||
        vqec_check_args_for_help_char(argv, argc)) {
        rv = FALSE;
    } else {
        app_cpy_delay = atoi(argv[0]);
        if (app_cpy_delay < 0) {
            rv = FALSE;
        } else {
            (void)vqec_dp_set_app_cpy_delay(app_cpy_delay);
        }
    }

    if (!rv) {
        vqec_cli_print(cli, "Usage: app-delay <delay>\n   where\n"
                       "     <delay> is the number of msec to delay in between "
                       "each\n             successive replicated APP "
                       "packet\n");
        return VQEC_CLI_ERROR;
    } else {
        return VQEC_CLI_OK;
    }
}
#endif /* HAVE_FCC */

UT_STATIC int vqec_cmd_drop_interval (struct vqec_cli_def *cli, 
                                      char *command, 
                                      char *argv[], int argc)
{
    boolean rv = TRUE;
    int drop_num, drop_int;

    if (argc != 2 || strchr(command, '?') || vqec_check_args_for_help_char(argv, argc)) {
        rv = FALSE;
    } else {
        drop_num = atoi(argv[0]);
        drop_int = atoi(argv[1]);
        if (drop_num < 0 || drop_int < drop_num) {
            rv = FALSE;
        } else {
            vqec_set_drop_interval(strstr(command, "repair") ? 
                                   VQEC_DP_INPUT_STREAM_TYPE_REPAIR : 
                                   VQEC_DP_INPUT_STREAM_TYPE_PRIMARY,
                                   drop_num, drop_int);
        }
    }

    if (!rv) {
        vqec_cli_print(cli, "Usage: %s <int1> <int2>\r\n   where\r\n"
                "     <int1> is an integer greater than or equal to 0, and\r\n"
                "     <int2> is an integer greater than or equal to <int1>\r\n"
                "   Note: use 'drop interval 0 0' to turn off", command);
        return VQEC_CLI_ERROR;
    } else {
        return VQEC_CLI_OK;
    }
}

UT_STATIC int vqec_cmd_drop_percentage (struct vqec_cli_def *cli, 
                                        char *command, 
                                        char *argv[], int argc)
{
    boolean rv = TRUE;
    int drop_perc;

    if (argc != 1 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        rv = FALSE;
    } else {
        drop_perc = atoi(argv[0]);
        if (drop_perc < 0 || drop_perc > 100) {
            rv = FALSE;
        } else {
            vqec_set_drop_ratio(strstr(command, "repair") ? 
                                VQEC_DP_INPUT_STREAM_TYPE_REPAIR : 
                                VQEC_DP_INPUT_STREAM_TYPE_PRIMARY,
                                drop_perc);
        }
    }

    if (!rv) {
        vqec_cli_print(cli, "Usage: %s <int>\r\n"
                       "   where <int> is an integer in the range [0,100]",
                       command);
        return VQEC_CLI_ERROR;
    } else {
        return VQEC_CLI_OK;
    }
}

UT_STATIC int vqec_cmd_drop_enable (struct vqec_cli_def *cli, char *command, 
                                    char *argv[], int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: %s", command);
        return VQEC_CLI_ERROR;
    } else {
        vqec_set_drop_enable(strstr(command, "repair") ? 
                             VQEC_DP_INPUT_STREAM_TYPE_REPAIR : 
                             VQEC_DP_INPUT_STREAM_TYPE_PRIMARY,
                             TRUE);
        return VQEC_CLI_OK;
    }
}

UT_STATIC int vqec_cmd_drop_disable (struct vqec_cli_def *cli, 
                                     char *command, 
                                     char *argv[], int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: %s", command);
        return VQEC_CLI_ERROR;
    } else {
        vqec_set_drop_enable(strstr(command, "repair") ? 
                             VQEC_DP_INPUT_STREAM_TYPE_REPAIR : 
                             VQEC_DP_INPUT_STREAM_TYPE_PRIMARY,
                             FALSE);
        return VQEC_CLI_OK;
    }
}

UT_STATIC int vqec_cmd_error_repair_enable (struct vqec_cli_def *cli, 
                                            char *command, 
                                            char *argv[], int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: error-repair enable");
        return VQEC_CLI_ERROR;
    } else {
        vqec_set_error_repair(TRUE);
        vqec_cli_print(cli, 
                "Warning:  changes to error-repair setting will take "
                "effect for each channel upon binding to it, but only after "
                "all tuners currently bound to it have been unbound");
        return VQEC_CLI_OK;
    }
}

UT_STATIC int vqec_cmd_error_repair_disable (struct vqec_cli_def *cli, 
                                             char *command, 
                                             char *argv[], int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: error-repair disable");
        return VQEC_CLI_ERROR;
    } else {
        vqec_set_error_repair(FALSE);
        vqec_cli_print(cli, 
                "Warning:  changes to error-repair setting will take "
                "effect for each channel upon binding to it, but only after "
                "all tuners currently bound to it have been unbound");
        return VQEC_CLI_OK;
    }
}

UT_STATIC int vqec_cmd_error_repair_policer_enable (
    struct vqec_cli_def *cli, char *command, char *argv[], int argc)
{
    if ((argc != 0) || strchr(command, '?')) {
        vqec_cli_print(cli, "Usage: error-repair policer enable");
        return VQEC_CLI_ERROR;
    }

    (void)vqec_error_repair_policer_parameters_set(TRUE, TRUE, 
                                                   FALSE, 0, FALSE, 0);
    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_error_repair_policer_disable (
    struct vqec_cli_def *cli, char *command, char *argv[], int argc)
{
    if ((argc != 0) || strchr(command, '?')) {
        vqec_cli_print(cli, "Usage: error-repair policer disable");
        return VQEC_CLI_ERROR;
    }

    (void)vqec_error_repair_policer_parameters_set(TRUE, FALSE, 
                                                   FALSE, 0, FALSE, 0);
    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_error_repair_policer_rate (
    struct vqec_cli_def *cli, char *command, char *argv[], int argc)
{
    int cli_retval = VQEC_CLI_OK;
    vqec_error_t vqec_retval;

    if (argc != 1 || strchr(command, '?') ||
        vqec_check_args_for_help_char(argv, argc)) {
        cli_retval = VQEC_CLI_ERROR;
    } else {
        vqec_retval = 
            vqec_error_repair_policer_parameters_set(FALSE, FALSE,
                                                     TRUE, atoi(argv[0]),
                                                     FALSE, 0);
        if (vqec_retval != VQEC_OK) {
            cli_retval = VQEC_CLI_ERROR;
        }
    }

    if (cli_retval != VQEC_CLI_OK) {
        vqec_cli_print(cli, "Usage: error-repair policer rate <rate>\n"
                "   where\n"
                "     <rate> is the token bucket rate expressed as a\n"
                "       percent of the primary stream rate\n"
                "  Supported values are:\n"
                "       %u%% <= <rate> <= %u%% [default = %u%%]",
                VQEC_SYSCFG_MIN_ERROR_REPAIR_POLICER_RATE,
                VQEC_SYSCFG_MAX_ERROR_REPAIR_POLICER_RATE,
                VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_RATE);
    }

    return (cli_retval);
}

UT_STATIC int vqec_cmd_error_repair_policer_burst (
    struct vqec_cli_def *cli, char *command, char *argv[], int argc)
{
    int cli_retval = VQEC_CLI_OK;
    vqec_error_t vqec_retval;

    if (argc != 1 || strchr(command, '?') ||
        vqec_check_args_for_help_char(argv, argc)) {
        cli_retval = VQEC_CLI_ERROR;
    } else {
        vqec_retval =
            vqec_error_repair_policer_parameters_set(FALSE, FALSE,
                                                     FALSE, 0,
                                                     TRUE, atoi(argv[0]));
        if (vqec_retval != VQEC_CLI_OK) {
            cli_retval = VQEC_CLI_ERROR;
        }
    }

    if (cli_retval != VQEC_CLI_OK) {
        vqec_cli_print(cli, "Usage: error-repair policer burst <burst>\n"
                "   where\n"
                "     <burst> is the token bucket burst, expressed as the\n"
                "       minimum duration over which the token limit would\n"
                "       be reached if the bucket were initially empty\n"
                "       and filled at rate <rate> with no tokens drained.\n"
                "  Supported values are:\n"
                "       %ums <= <burst> <= %ums [default = %ums]",
                VQEC_SYSCFG_MIN_ERROR_REPAIR_POLICER_BURST,
                VQEC_SYSCFG_MAX_ERROR_REPAIR_POLICER_BURST,
                VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_BURST);
    }

    return (cli_retval);
}

/*
 * update [file <filename> type {network|override-tags|channel}]
 */
UT_STATIC int vqec_cmd_update (struct vqec_cli_def *cli, char *command, 
                               char *argv[], int argc)
{
    int status = VQEC_CLI_OK;
    boolean print_usage = FALSE;
#define VQEC_CMD_OVERRIDE_PARAMS_MAX (50)
#define VQEC_CMD_BUFFER_LINE_MAX (2*VQEC_MAX_NAME_LEN+1)
    FILE *fp = NULL;
    struct stat file_status;
    vqec_ifclient_config_update_params_t params_update;
    vqec_ifclient_config_override_params_t params_override;
    char *buffer = NULL;
    char buffer_line[VQEC_CMD_BUFFER_LINE_MAX];
    char tag[VQEC_MAX_NAME_LEN], *tags[VQEC_CMD_OVERRIDE_PARAMS_MAX+1];
    char value[VQEC_MAX_NAME_LEN], *values[VQEC_CMD_OVERRIDE_PARAMS_MAX+1];
    int i = -1, param3;
    vqec_error_t status_api;

    char *params0[] =
        { "file", (char *)0 };
    char *params2[] =
        { "type", (char *)0 };
    char *params3[] =
        { "network", "override-tags", "channel", (char *)0 };
    
    if (((argc) && (argc != 4)) || vqec_check_args_for_help_char(argv, argc)) {
        print_usage = TRUE;
        status = VQEC_CLI_ERROR;
        goto done;
    }

    if (!argc) {

        /* "update" command */
        if (!vqec_updater_get_cdi_enable()) {
            CONSOLE_PRINTF("Cisco Delivery Infrastructure not enabled.");
            status = VQEC_CLI_ERROR;
            goto done;
        }

        vqec_ifclient_trigger_update();

    } else {

        /* "update file <filename> type {network|override-tags|channel} */
        if (vqec_str_match(argv[0], params0) != 0) {
            print_usage = TRUE;
            status = VQEC_CLI_ERROR;
            goto done;
        }
        
        /* parse "type" */
        if (vqec_str_match(argv[2], params2) != 0) {
            print_usage = TRUE;
            status = VQEC_CLI_ERROR;
            goto done;
        }

        /* parse "{network|override-tags|channel}" */
        param3 = vqec_str_match(argv[3], params3);
        switch (param3) {
        case 0: /* network */
        case 1: /* override-tags */
        case 2: /* channel */
            break;
        default:
            print_usage = TRUE;
            status = VQEC_CLI_ERROR;
            goto done;
        }

        fp = fopen(argv[1], "r");
        if (fp == NULL) {
            vqec_cli_print(cli,
                           "Error: '%s' file could not be opened "
                           "(errno = %d).\n",
                           argv[1], errno);
            status = VQEC_CLI_ERROR;
            goto done;
        }

        /* Invoke VQE-C API to update the configuration */
        if (param3 == 1) {

            /* override-tags update */
            
            /*
             * Fill the (tag, value) array pair with values from the file.
             * Terminate the array with a (NULL, NULL) pair.
             */
            while ((fgets(buffer_line, VQEC_CMD_BUFFER_LINE_MAX, fp) != NULL) 
                   && (i < VQEC_CMD_OVERRIDE_PARAMS_MAX - 1)) {
                
                /*
                 * Ignore any lines having an invalid syntax
                 * such that a tag and value can't be read.
                 */
                if (sscanf(buffer_line, "%80s %80s", tag, value) != 2) {
                    continue;
                }
                i++;
                tags[i] = (char *)malloc(strlen(tag)+1);
                values[i] = (char *)malloc(strlen(value)+1);
                if (!tags[i] || !values[i]) {
                    vqec_cli_print(cli, "Error:  malloc failure.\n");
                    status = VQEC_CLI_ERROR;
                    goto done;
                }
                snprintf(tags[i], strlen(tag)+1, "%s", tag);
                snprintf(values[i], strlen(value)+1, "%s", value);
            }
            tags[i+1] = values[i+1] = NULL;
           
            /*
             * Invoke the API to apply the overrides, and print the result.
             */
            params_override.tags = (const char **)tags;
            params_override.values = (const char **)values;
            status_api = 
                vqec_ifclient_config_override_update(&params_override);
            
            vqec_cli_print(cli, "Update result: '%s'\n", 
                           vqec_err2str(status_api));
            
        } else {
            
            /* network or channel update */
            
            /*
             * Allocate a buffer and load it with the configuration file's 
             * contents.
             */
            if (fstat(fileno(fp), &file_status)) {
                vqec_cli_print(cli, 
                               "Error: size of '%s' could not be determined.\n",
                               argv[1]);
                status = VQEC_CLI_ERROR;
                goto done;
            }
            buffer = (char *)malloc(file_status.st_size+1);
            if (!buffer) {
                vqec_cli_print(cli, "Error:  malloc failure.\n");
                status = VQEC_CLI_ERROR;
                goto done;
            }
            if (fread(buffer, 1, file_status.st_size, fp) < 
                file_status.st_size) {
                vqec_cli_print(cli, 
                               "Error:  '%s' could not be loaded"
                               " (errno = %d).\n",
                               argv[1], errno);
                status = VQEC_CLI_ERROR;
                goto done;
            }
            buffer[file_status.st_size] = '\0';

            /*
             * Update the configuration.
             */
            params_update.config = (param3 ? 
                                    VQEC_IFCLIENT_CONFIG_CHANNEL : 
                                    VQEC_IFCLIENT_CONFIG_NETWORK); 
            params_update.buffer = buffer;
            status_api = vqec_ifclient_config_update(&params_update);
            
            vqec_cli_print(cli, "Update result: '%s' (persisted = %s)\n",
                           vqec_err2str(status_api), 
                           (params_update.persisted ? "TRUE" : "FALSE"));

            if ((status_api == VQEC_OK) && (vqec_updater_get_cdi_enable())) {
                vqec_cli_print(cli, "Warning:  update may be overwritten "
                                    "by CDI updates.\n");
            }
            
        }

    }

done:
    if (print_usage) {
        vqec_cli_print(cli, "Usage: \n update [file <filename> "
                       "type {network|override-tags|channel}]\n");
    }
    while (i >= 0) {
        if (tags[i]) {
            free(tags[i]);
        }
        if (values[i]) {
            free(values[i]);
        }
        i--;
    }
    if (buffer) {
        free(buffer);
    }
    if (fp) {
        fclose(fp);
    }
    return (status);
}


/*
 * COMMAND FUNCTIONS FOR 'TEST' COMMANDS
 */

static inline void vqec_cmd_tuner_bind_print_usage (struct vqec_cli_def *cli)
{
    vqec_cli_print(cli, "Usage: tuner bind <tuner-name> {<url> | "
            "chan-params {file <filename> | list <param_list>}} "
            "[no_rcc] [fastfill] [max-fastfill <max_fastfill>] "
            "[rcc-bw <max_recv_bw_rcc>] [er-bw <max_recv_bw_er>] "
            "[tr-135 gmin <gmin> slmd <slmd>]" 
            "[event-context <event_context>]");
    vqec_cli_print(cli, "See 'parse' command usage for 'chan-params' help");
    vqec_cli_print(cli, "ex. tuner bind tuner1 rtp://224.1.1.1:50000");
    vqec_cli_print(cli, "    tuner bind tuner1 rtp://224.1.1.1:50000 no_rcc");
    vqec_cli_print(cli, "    tuner bind tuner1 rtp://224.1.1.1:50000 "
                           "rcc-bw 512000 er-bw 256000");
    vqec_cli_print(cli, "    tuner bind tuner1 rtp://224.1.1.1:50000 "
                           "no_rcc er-bw 256000");
    vqec_cli_print(cli, "    tuner bind tuner1 rtp://224.1.1.1:50000 "
                           "fastfill");
    vqec_cli_print(cli, 
        "    tuner bind tuner1 rtp://224.1.1.1:50000 fastfill rcc-bw 512000 "
        "er-bw 256000");
    vqec_cli_print(cli, 
        "    tuner bind tuner1 rtp://224.1.1.1:50000 tr-135 gmin 2 slmd 3");
    vqec_cli_print(cli,
        "    tuner bind tuner1 rtp://5.3.19.10:50000 sdp myvod.cfg "
        "src-addr 5.3.19.2");
}

#define ARG_STRLEN 64
static inline boolean vqec_cmd_is_str_uint32 (char *str, uint32_t len)
{
    uint64_t num;
    char tmpstr[ARG_STRLEN];

    num = atoll(str);
    snprintf(tmpstr, ARG_STRLEN, "%u", (uint32_t)num);

    if (!strncmp(str, tmpstr, ARG_STRLEN)) {
        return (TRUE);
    } else {
        return (FALSE);
    }
}

/**
 * The following three functions are used to test VQE-C fast fill feature 
 * from vqec CLI "tuner bind", without the STB.
 * 
 * Please note, these functions are only for testing purpose. 
 * If you are in a STB which has fast fill feature integrated,
 * doing "tuner bind" from CLI does not provide you the full
 * functionalities of fast fill. It only streams out packets from 
 * VQE-C in a faster rate.
 * 
 */

static void
fastFillStartHandler (int32_t context_id,
                      vqec_ifclient_fastfill_params_t *params)
{
    if (VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC)) {
        CONSOLE_PRINTF("[FASTFILL]: started @ abs_time = %llu ms\n", 
                       TIME_GET_A(msec,get_sys_time()));
    }
}

static void
fastFillAbortHandler (int32_t context_id,
                      vqec_ifclient_fastfill_status_t *status)
{

    if (VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC)) {
        CONSOLE_PRINTF("[FASTFILL]: aborted @ abs_time = %llu ms\n", 
                       TIME_GET_A(msec,get_sys_time()));
    }
}

static void
fastFillDoneHandler (int32_t context_id,
                     vqec_ifclient_fastfill_status_t *status)
{

    if (VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC)) {
        CONSOLE_PRINTF("[FASTFILL]: finished @ abs_time = %llu ms\n", 
                       TIME_GET_A(msec,get_sys_time()));
    }
}


static vqec_ifclient_fastfill_cb_ops_t fastFillOps =
{
    fastFillStartHandler,
    fastFillAbortHandler,
    fastFillDoneHandler
};

/* Parse a parameter and add it to the cfg struct.
 *
 * @param[out] cfg          target config struct, fields are filled with
 *                          parsed values
 * @param[in]  param_name   parameter name
 * @param[in]  param_value  parameter value
 * @param[out] cur_arg      incremented by # tokens consumed
 * @param[out] boolean      TRUE on success, FALSE on failure
 */
static inline boolean
vqec_cmd_parse_chan_param(vqec_chan_cfg_t *cfg,
                          char *param_name,
                          char *param_value,
                          int *cur_arg)
{
    struct in_addr temp_addr;
    char tmp[INET_ADDRSTRLEN];
    
    /* Define a few parsing macros */
#define VQEC_PARSE_ADDR_PARAM(_name, _field)                    \
    if (!strncmp(_name, param_name, ARG_STRLEN)) {              \
        temp_addr.s_addr = inet_addr(param_value);              \
        cfg->_field = temp_addr;                                \
        inet_ntop(AF_INET, &cfg->_field, tmp, INET_ADDRSTRLEN); \
        /* dotted-decimal format check */                       \
        if (strncmp(param_value, tmp, INET_ADDRSTRLEN)) {       \
            return FALSE;                                       \
        }                                                       \
    } else                                          
#define VQEC_PARSE_PORT_PARAM(_name, _field)                \
    if (!strncmp(_name, param_name, ARG_STRLEN)) {          \
        cfg->_field = htons((uint16_t)atoi(param_value));   \
        if (ntohs(cfg->_field) != atol(param_value)) {      \
            return FALSE;                                   \
        }                                                   \
    } else
#define VQEC_PARSE_INT_PARAM(_name, _field, _type)      \
    if (!strncmp(_name, param_name, ARG_STRLEN)) {      \
        cfg->_field = (_type)atol(param_value);         \
        /* Overflow check (cast may not succeed) */     \
        if (cfg->_field != atol(param_value)) {         \
            return FALSE;                               \
        }                                               \
    } else
#define VQEC_PARSE_ENABLE_PARAM(_name, _field)          \
    if (!strncmp(_name, param_name, ARG_STRLEN)) {      \
        cfg->_field = TRUE;                             \
    } else
#define VQEC_PARSE_XRENABLE_PARAM(_name, _field)        \
    if (!strncmp(_name, param_name, ARG_STRLEN)) {      \
        cfg->_field = VQEC_RTCP_XR_RLE_ENABLE;          \
    } else
#define VQEC_PARSE_XRFLAGS_PARAM(_name, _field)         \
    if (!strncmp(_name, param_name, ARG_STRLEN)) {      \
        if (strstr(param_value, "loss")) {              \
            cfg->_field |= VQEC_RTCP_XR_STAT_LOSS;      \
        }                                               \
        if (strstr(param_value, "dup")) {               \
            cfg->_field |= VQEC_RTCP_XR_STAT_DUP;       \
        }                                               \
        if (strstr(param_value, "jitt")) {              \
            cfg->_field |= VQEC_RTCP_XR_STAT_JITT;      \
        }                                               \
    } else
        
    /* Primary Stream Transport */
    VQEC_PARSE_ADDR_PARAM("primary-dest-addr", primary_dest_addr)
    VQEC_PARSE_PORT_PARAM("primary-dest-port", primary_dest_port)
    VQEC_PARSE_PORT_PARAM("primary-dest-rtcp-port", primary_dest_rtcp_port)
    VQEC_PARSE_ADDR_PARAM("primary-src-addr", primary_source_addr)
    VQEC_PARSE_PORT_PARAM("primary-src-port", primary_source_port)
    VQEC_PARSE_PORT_PARAM("primary-src-rtcp-port", primary_source_rtcp_port)
    /* Primary Stream Parameters */
    VQEC_PARSE_INT_PARAM("primary-payload-type", primary_payload_type, uint8_t)
    VQEC_PARSE_INT_PARAM("primary-bit-rate", primary_bit_rate, uint32_t)
    VQEC_PARSE_INT_PARAM("primary-rtcp-sndr-bw", 
                         primary_rtcp_sndr_bw, uint32_t)
    VQEC_PARSE_INT_PARAM("primary-rtcp-rcvr-bw", 
                         primary_rtcp_rcvr_bw, uint32_t)
    VQEC_PARSE_INT_PARAM("primary-rtcp-per-rcvr-bw", 
                         primary_rtcp_per_rcvr_bw, uint32_t)
    VQEC_PARSE_XRENABLE_PARAM("primary_rtcp_xr_loss_rle_enable",
                              primary_rtcp_xr_loss_rle)
    VQEC_PARSE_INT_PARAM("primary-rtcp-xr-loss-rle",
                         primary_rtcp_xr_loss_rle, uint16_t)
    VQEC_PARSE_XRENABLE_PARAM("primary_rtcp_xr_per_loss_rle_enable",
                            primary_rtcp_xr_per_loss_rle)
    VQEC_PARSE_INT_PARAM("primary-rtcp-xr-per-loss-rle",
                         primary_rtcp_xr_per_loss_rle, uint16_t)
    VQEC_PARSE_XRFLAGS_PARAM("primary-rtcp-xr-stat-flags",
                             primary_rtcp_xr_stat_flags)
    /* Feedback Target Address */
    VQEC_PARSE_ADDR_PARAM("fbt-addr", fbt_addr)
    /* Error Repair and RCC enable flags */
    VQEC_PARSE_ENABLE_PARAM("er_enable", er_enable)
    VQEC_PARSE_ENABLE_PARAM("rcc_enable", rcc_enable)
    /* Retransmission Stream Transport */
    VQEC_PARSE_ADDR_PARAM("rtx-src-addr", rtx_source_addr)
    VQEC_PARSE_PORT_PARAM("rtx-src-port", rtx_source_port)
    VQEC_PARSE_PORT_PARAM("rtx-src-rtcp-port", rtx_source_rtcp_port)
    VQEC_PARSE_ADDR_PARAM("rtx-dest-addr", rtx_dest_addr)
    VQEC_PARSE_PORT_PARAM("rtx-dest-port", rtx_dest_port)
    VQEC_PARSE_PORT_PARAM("rtx-dest-rtcp-port", rtx_dest_rtcp_port)
    /* Retransmission Stream Parameters */
    VQEC_PARSE_INT_PARAM("rtx-payload-type", rtx_payload_type, uint8_t)
    VQEC_PARSE_INT_PARAM("rtx-rtcp-sndr-bw", rtx_rtcp_sndr_bw, uint32_t)
    VQEC_PARSE_INT_PARAM("rtx-rtcp-rcvr-bw", rtx_rtcp_rcvr_bw, uint32_t)
    VQEC_PARSE_XRENABLE_PARAM("rtx_rtcp_xr_loss_rle_enable",
                              rtx_rtcp_xr_loss_rle)
    VQEC_PARSE_INT_PARAM("rtx-rtcp-xr-loss-rle", 
                         rtx_rtcp_xr_loss_rle, uint16_t)
    VQEC_PARSE_XRFLAGS_PARAM("rtx-rtcp-xr-stat-flags",
                             rtx_rtcp_xr_stat_flags)
    /* FEC Enable and Mode */
    VQEC_PARSE_ENABLE_PARAM("fec_enable", fec_enable)
    if (!strncmp("fec-mode", param_name, ARG_STRLEN)) {
        if (!strncmp("1D", param_value, ARG_STRLEN)) {
            cfg->fec_mode = VQEC_FEC_1D_MODE;
        } else if (!strncmp("2D", param_value, ARG_STRLEN)) {
            cfg->fec_mode = VQEC_FEC_2D_MODE;
        } else {
            return FALSE;
        }
    } else
    /* FEC 1 Stream */
    VQEC_PARSE_ADDR_PARAM("fec1-mcast-addr", fec1_mcast_addr)
    VQEC_PARSE_PORT_PARAM("fec1-mcast-port", fec1_mcast_port)
    VQEC_PARSE_PORT_PARAM("fec1-mcast-rtcp-port", fec1_mcast_rtcp_port)
    VQEC_PARSE_ADDR_PARAM("fec1-src-addr", fec1_source_addr)
    VQEC_PARSE_INT_PARAM("fec1-payload-type", fec1_payload_type, uint8_t)
    VQEC_PARSE_INT_PARAM("fec1-rtcp-sndr-bw", fec1_rtcp_sndr_bw, uint32_t)
    VQEC_PARSE_INT_PARAM("fec1-rtcp-rcvr-bw", fec1_rtcp_rcvr_bw, uint32_t)
    /* FEC 2 Stream */
    VQEC_PARSE_ADDR_PARAM("fec2-mcast-addr", fec2_mcast_addr)
    VQEC_PARSE_PORT_PARAM("fec2-mcast-port", fec2_mcast_port)
    VQEC_PARSE_PORT_PARAM("fec2-mcast-rtcp-port", fec2_mcast_rtcp_port)
    VQEC_PARSE_ADDR_PARAM("fec2-src-addr", fec2_source_addr)
    VQEC_PARSE_INT_PARAM("fec2-payload-type", fec2_payload_type, uint8_t)
    VQEC_PARSE_INT_PARAM("fec2-rtcp-sndr-bw", fec2_rtcp_sndr_bw, uint32_t)
    VQEC_PARSE_INT_PARAM("fec2-rtcp-rcvr-bw", fec2_rtcp_rcvr_bw, uint32_t)
    /* FEC Common Parameters */
    VQEC_PARSE_INT_PARAM("fec-l-value", fec_l_value, uint8_t)
    VQEC_PARSE_INT_PARAM("fec-d-value", fec_d_value, uint8_t)
    if (!strncmp("fec-order", param_name, ARG_STRLEN)) {
        if (!strncmp("annexa", param_value, ARG_STRLEN)) {
            cfg->fec_order = VQEC_FEC_SENDING_ORDER_ANNEXA;
        } else if (!strncmp("annexb", param_value, ARG_STRLEN)) {
            cfg->fec_order = VQEC_FEC_SENDING_ORDER_ANNEXB;
        } else if (!strncmp("other", param_value, ARG_STRLEN)) {
            cfg->fec_order = VQEC_FEC_SENDING_ORDER_OTHER;
        } else {
            cfg->fec_order = VQEC_FEC_SENDING_ORDER_NOT_DECIDED;
        }
    } else
    VQEC_PARSE_INT_PARAM("fec-ts-pkt-time", fec_ts_pkt_time, uint64_t)
    /* Unrecognized parameter - return FALSE */
    {
        return FALSE;
    }

    /* _enable params only consume one token */
    if (strstr(param_name, "_enable")) {
        *cur_arg += 1;
    } else {
        *cur_arg += 2;
    }
    return TRUE;
}


UT_STATIC int vqec_cmd_tuner_bind (struct vqec_cli_def *cli,
                                   char *command,
                                   char *argv[], int argc)
{
    vqec_error_t ret;
    vqec_tunerid_t id = VQEC_TUNERID_INVALID;
    vqec_chan_cfg_t chan_cfg;
    vqec_sdp_handle_t sdp_handle;
    boolean using_url;
    vqec_bind_params_t *bp;
    boolean fastfill = FALSE;
    boolean success = TRUE;
    vqec_ifclient_tr135_params_t tr135_params;
    int cur_arg, rv;
    FILE *param_file;
    char buffer1[ARG_STRLEN], buffer2[ARG_STRLEN];

    if (argc < 2  ||
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cmd_tuner_bind_print_usage(cli);
        return VQEC_CLI_ERROR;
    }
    
    /* Determine tuner id */
    id = vqec_ifclient_tuner_get_id_by_name(argv[0]);
    if (id == VQEC_TUNERID_INVALID) {
        vqec_cli_print(cli, "Invalid tuner name (%s)", argv[0]);
        return VQEC_CLI_ERROR;
    }

    /* Bind with individual channel parameters */
    if (strncmp("chan-params", argv[1], ARG_STRLEN) == 0) {
        if (argc < 3) {
            vqec_cmd_tuner_bind_print_usage(cli);
            return VQEC_CLI_ERROR;
        }
        using_url = FALSE;
        /* Set chan_cfg to default values */
        vqec_chan_convert_sdp_to_cfg(NULL, 0, &chan_cfg);

        /* Read parameters from a file */
        if (strncmp("file", argv[2], ARG_STRLEN) == 0) {
            if (argc < 4) {
                vqec_cmd_tuner_bind_print_usage(cli);
                return VQEC_CLI_ERROR;
            }
            param_file = fopen(argv[3], "r");
            if (!param_file) {
                vqec_cli_print(cli, "Cannot open chan params file (%s)", 
                               argv[3]);
                return VQEC_CLI_ERROR;
            }
            memset(buffer1, 0, ARG_STRLEN);
            memset(buffer2, 0, ARG_STRLEN);
            /* cur_arg indicates number of buffers to replenish */
            cur_arg = 2;
            rv = EOF;
            do {
                if (cur_arg == 2) {
                    rv = fscanf(param_file, "%63s", buffer1);
                    if (rv != EOF) {
                        rv = fscanf(param_file, "%63s", buffer2);
                    }
                } else if (cur_arg == 1) {
                    strncpy(buffer1, buffer2, ARG_STRLEN);
                    rv = fscanf(param_file, "%63s", buffer2);
                }
                cur_arg = 0;
                if (!vqec_cmd_parse_chan_param(&chan_cfg, buffer1,
                                               buffer2, &cur_arg)) {
                    vqec_cli_print(cli, "Bad chan param (%s %s)", 
                                   buffer1, buffer2);
                    fclose(param_file);
                    return VQEC_CLI_ERROR;
                }
            } while (rv != EOF);
            fclose(param_file);
            cur_arg = 4;

        /* Read parameters from the command line */
        } else if (strncmp("list", argv[2], ARG_STRLEN) == 0) {
            for (cur_arg = 3; cur_arg < argc;) {
                if (!vqec_cmd_parse_chan_param(
                                       &chan_cfg, argv[cur_arg], 
                                       cur_arg+1 < argc ? argv[cur_arg+1] : "", 
                                       &cur_arg)) {
                    vqec_cli_print(cli, "Bad chan param (%s %s)", 
                                   argv[cur_arg],
                              cur_arg+1 < argc ? argv[cur_arg+1] : "");
                    return VQEC_CLI_ERROR;
                }
            }
        } else {
            vqec_cmd_tuner_bind_print_usage(cli);
            return VQEC_CLI_ERROR;
        }

    /* Bind with a channel URL */
    } else {
        using_url = TRUE;
        cur_arg = 2;
    }

    /* parse and set bind params */
    bp = vqec_ifclient_bind_params_create();
    if (!bp) {
        vqec_cli_print(cli, "failed to allocate bind parameters!\n");
        return (VQEC_CLI_ERROR);
    }
    for (; cur_arg < argc; cur_arg++)
    {
        if (strncmp("no_rcc", argv[cur_arg], ARG_STRLEN) == 0) {
            vqec_ifclient_bind_params_disable_rcc(bp);
        } else if (strncmp("fastfill", argv[cur_arg], ARG_STRLEN) == 0) {
            vqec_ifclient_bind_params_enable_fastfill(bp);
            fastfill = TRUE;
        } else if (strncmp("rcc-bw", argv[cur_arg], ARG_STRLEN) == 0) {
            if (cur_arg >= (argc - 1) ||
                !vqec_cmd_is_str_uint32(argv[cur_arg + 1], ARG_STRLEN)) {
                success = FALSE;
                break;
            }
            vqec_ifclient_bind_params_set_max_recv_bw_rcc(
                bp, atoi(argv[cur_arg + 1]));
            cur_arg++;
        } else if (strncmp("er-bw", argv[cur_arg], ARG_STRLEN) == 0) {
            if (cur_arg >= (argc - 1) ||
                !vqec_cmd_is_str_uint32(argv[cur_arg + 1], ARG_STRLEN)) {
                success = FALSE;
                break;
            }
            vqec_ifclient_bind_params_set_max_recv_bw_er(
                bp, atoi(argv[cur_arg + 1]));
            cur_arg++;
        } else if (strncmp("max-fastfill", argv[cur_arg], ARG_STRLEN) == 0) {
            if (cur_arg >= (argc - 1) ||
                !vqec_cmd_is_str_uint32(argv[cur_arg + 1], ARG_STRLEN)) {
                success = FALSE;
                break;
            }
            vqec_ifclient_bind_params_set_max_fastfill(
                bp, atoi(argv[cur_arg + 1]));
            cur_arg++;
        } else if ((strncmp("tr-135", argv[cur_arg], ARG_STRLEN) == 0)) {
            if ((cur_arg >= (argc - 4)) ||
                (strncmp("gmin", argv[cur_arg+1], ARG_STRLEN) != 0) ||
                (!vqec_cmd_is_str_uint32(argv[cur_arg + 2], ARG_STRLEN)) ||
                (strncmp("slmd", argv[cur_arg+3], ARG_STRLEN) != 0) ||
                (!vqec_cmd_is_str_uint32(argv[cur_arg + 4], ARG_STRLEN))) {
                success = FALSE;
                break;
            }
            tr135_params.gmin =  atoi(argv[cur_arg + 2]);
            tr135_params.severe_loss_min_distance =  atoi(argv[cur_arg + 4]);
            vqec_ifclient_bind_params_set_tr135_params(bp, &tr135_params);
            cur_arg += 4;
        } else {
            success = FALSE;
            break;
        }
    }
    if (!success) {
        vqec_ifclient_bind_params_destroy(bp);
        vqec_cmd_tuner_bind_print_usage(cli);
        return (VQEC_CLI_ERROR);
    }

    if (fastfill) {
        vqec_ifclient_bind_params_set_fastfill_ops(bp, &fastFillOps);
    }

    /* Perform tuner bind with channel URL */
    if (using_url) {
        sdp_handle = vqec_ifclient_alloc_sdp_handle_from_url(argv[1]);
        if (sdp_handle == NULL) {
            vqec_ifclient_bind_params_destroy(bp);
            vqec_cli_print(cli, "failed to acquire cfg handle from url: %s", 
                           argv[1]);
            return VQEC_CLI_ERROR;
        }
        ret = vqec_ifclient_tuner_bind_chan(id, sdp_handle, bp);
        vqec_ifclient_free_sdp_handle(sdp_handle);
    /* Perform tuner bind with channel configuration */
    } else {
        ret = vqec_ifclient_tuner_bind_chan_cfg(id, &chan_cfg, bp);
    }

    vqec_ifclient_bind_params_destroy(bp);

    if (ret != VQEC_OK) {
        vqec_cli_print(cli, "FAILURE: tuner-name = %s (%s)", argv[0], 
                       vqec_err2str(ret));
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_channel_tr135 (struct vqec_cli_def *cli, 
                                      char *command, 
                                      char *argv[], int argc)
{

    vqec_error_t err;
    vqec_ifclient_tr135_params_t tr135_params;
    char *PARAMS1 [] = { "gmin", "slmd", (char *) 0};
    int val1, val2;

    if (argc != 5 || vqec_check_args_for_help_char(argv, argc)) {
        goto done;
    }
    val1 = vqec_str_match(argv[ 1 ], PARAMS1);
    val2 = vqec_str_match(argv[ 3 ], PARAMS1);
     
    if ((val1 != 0) ||
        (!vqec_cmd_is_str_uint32(argv[ 2 ], ARG_STRLEN)) ||
        (val2 != 1) ||
        (!vqec_cmd_is_str_uint32(argv[ 4 ], ARG_STRLEN))) {
        goto done;         
    }
    
    tr135_params.gmin =  atoi(argv[2]);
    tr135_params.severe_loss_min_distance =  atoi(argv[4]);
    err = vqec_ifclient_set_tr135_params_channel(argv[0], &tr135_params);

    if (err != VQEC_OK) {
        vqec_cli_print(cli, "FAILURE: channel tr-135 = %s (%s)", argv[0], 
                       vqec_err2str(err));
        return VQEC_CLI_ERROR;
    }
    
    return VQEC_CLI_OK;

done: 
    vqec_cli_print(cli, "Usage: channel tr-135 <channel-url> gmin <gmin> "
                   "slmd <slmd> \nex. channel tr-135 rtp://224.1.1.1:50000 "
                   "gmin 1 slmd 2");
    return VQEC_CLI_ERROR;
}

UT_STATIC int vqec_cmd_tuner_unbind (struct vqec_cli_def *cli, char *command, 
                                     char *argv[], int argc)
{
    vqec_error_t ret;
    vqec_tunerid_t id = VQEC_TUNERID_INVALID;

    if (argc != 1 || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: tuner unbind <tuner-name> - "
                            "ex. tuner unbind tuner1");
        return VQEC_CLI_ERROR;
    }

    id = vqec_ifclient_tuner_get_id_by_name(argv[0]);
    if (id == VQEC_TUNERID_INVALID) {
        vqec_cli_print(cli, "Invalid tuner name (%s)", argv[0]);
        return VQEC_CLI_ERROR;
    }

    ret = vqec_ifclient_tuner_unbind_chan(id);

    if (ret != VQEC_OK) {
        vqec_cli_print(cli, "FAILURE: tuner-name = %s (%s)", argv[0], 
                       vqec_err2str(ret));
        return VQEC_CLI_ERROR;
    }
    
    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_tuner_create (struct vqec_cli_def *cli, 
                                     char *command, 
                                     char *argv[], int argc)
{
    vqec_tunerid_t id = VQEC_TUNERID_INVALID;
    vqec_error_t err;

    if ((argc != 1) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: tuner create <tuner-name> \n");
        return VQEC_CLI_ERROR;
    }

    err = vqec_ifclient_tuner_create(&id, argv[0]);
    if (err != VQEC_OK) {
        vqec_cli_print(cli, "failed to create tuner (%s)!", vqec_err2str(err));
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_tuner_destroy (struct vqec_cli_def *cli, 
                                      char *command, 
                                      char *argv[], int argc)
{
    vqec_error_t ret;
    vqec_tunerid_t id = VQEC_TUNERID_INVALID;

    if ((argc != 1) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: tuner destroy <tuner-name>");
        return VQEC_CLI_ERROR;
    }

    id = vqec_ifclient_tuner_get_id_by_name(argv[0]);
    if (id == VQEC_TUNERID_INVALID) {
        vqec_cli_print(cli, "Invalid tuner name (%s)", argv[0]);
        return VQEC_CLI_ERROR;
    }
    
    /*
     * Destroy the output stream for the tuner first. Have to do it here
     * since cli is the only place they know each other.
     */
    ret = vqec_stream_output(argv[0], "lo", "udp://0.0.0.0:1");
    if (ret != VQEC_OK) {
        vqec_cli_print(cli, "FAILURE: tuner-name = %s (%s)", argv[0], 
                       vqec_err2str(ret));
    }

    ret = vqec_ifclient_tuner_destroy(id);

    if (ret != VQEC_OK) {
        vqec_cli_print(cli, "FAILURE: tuner-name = %s (%s)", argv[0], 
                       vqec_err2str(ret));
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}


/**
 * vod session id and CLI name map 
 * Use max tuner 32. 
 */

char id_name_pair[32][VQEC_MAX_NAME_LEN];

#if HAVE_CISCO_RTSP_LIBRARY
/**
 * Looks up a vod session id given a name.
 * NOTE:  The given name need not be NULL-terminated--only the first
 *        RTSP_MAX_PARAM_LEN characters are used in the lookup.
 *
 * @param[in] name name of tuner to find, MUST be valid.
 */
vqec_vod_session_id_t
vod_session_get_id_by_name (const char *name)
{
    int32_t i;

    for (i = 0; i < 32; i++) {
        if ((strncmp(name,id_name_pair[i],VQEC_MAX_NAME_LEN)) == 0) {
        return i;
    }
}

return VQEC_VOD_INVALID_ID;
}


/**
 * Create a vod session
 */
UT_STATIC int 
vqec_cmd_vod_session_create (struct vqec_cli_def *cli, 
                             char *command, 
                             char *argv[], int argc)
{

    vqec_vod_session_id_t id = VQEC_VOD_INVALID_ID;
    vqec_vod_session_params_t params;
    vqec_vod_error_t err;
    char vqec_url[VQEC_MAX_URL_LEN];
    char rtsp_url[RTSP_MAX_URL_LEN];
    int cur_arg = 0;

    if ((argc < 2) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(
            cli, 
      "Usage:vod-session create <session-name> <rtsp-url> [vod-area-id <id>]");
        return VQEC_CLI_ERROR;
    }

    /* check the length of the name */
    if (strlen(argv[0]) >= VQEC_MAX_NAME_LEN) {
        vqec_cli_print(cli, 
                       "Name is too long, only support up to %d", 
                       VQEC_MAX_NAME_LEN-1);
        return VQEC_CLI_ERROR;
    }

    if(vod_session_get_id_by_name(argv[0]) != VQEC_VOD_INVALID_ID) {
        vqec_cli_print(cli, 
                       "Session with name %s exist", argv[0]);
        return VQEC_CLI_ERROR;
    }

    memset(&params, 0, sizeof(vqec_vod_session_params_t));
    memset(rtsp_url, 0, RTSP_MAX_URL_LEN);
    strncpy(rtsp_url, argv[1], RTSP_MAX_URL_LEN);

    cur_arg = 2;
    if (argc == 4) {
        if (strncmp("vod-area-id", argv[cur_arg], ARG_STRLEN) == 0) {
            params.vod_area_id = (uint16_t)(atoi(argv[cur_arg+1]));
        }
    }

    err = vqec_vod_session_create(rtsp_url, &params, &id);

    if (err != VQEC_VOD_OK) {
        vqec_cli_print(cli, "failed to create vod session (%s)!", 
                       vod_error_to_string(err));
        return VQEC_CLI_ERROR;
    }
  
    memset(vqec_url, 0, VQEC_MAX_URL_LEN);
    err = vqec_vod_session_id_to_url(id, vqec_url,
                                     VQEC_MAX_URL_LEN);
    if (err != VQEC_VOD_OK) {
        vqec_cli_print(cli, "failed to create vqec url (%s)!", 
                       vod_error_to_string(err));
        return VQEC_CLI_ERROR;
    } else {
        vqec_cli_print(cli, "VQE-C channel url: %s", vqec_url);
    }

    /* save the name */
    strncpy(id_name_pair[id], argv[0], RTSP_MAX_PARAM_LEN);

    return VQEC_CLI_OK;
}

/**
 * Destroy a vod session 
 */
UT_STATIC int 
vqec_cmd_vod_session_destroy (struct vqec_cli_def *cli, 
                              char *command, 
                              char *argv[], int argc)
{
    vqec_vod_error_t ret;
    vqec_vod_session_id_t id = VQEC_VOD_INVALID_ID;

    if ((argc != 1) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: vod-session destroy <session-name>");
        return VQEC_CLI_ERROR;
    }

    id = vod_session_get_id_by_name(argv[0]);
    if (id == VQEC_VOD_INVALID_ID) {
        vqec_cli_print(cli, "Invalid vod session name (%s)", argv[0]);
        return VQEC_CLI_ERROR;
    }
    /* remove the session from id and name pair*/
    memset(id_name_pair[id], 0, VQEC_MAX_NAME_LEN);

    ret = vqec_vod_session_destroy(id);

    if (ret != VQEC_VOD_OK) {
        vqec_cli_print(cli, "Destroy failure: vod session name = %s (%s)", 
                       argv[0], vod_error_to_string(ret));
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}

/**
 * Play a vod session.
 * This function supports the following commands:
 * Normal play, 
 * Fastforward
 * Rewind
 */
UT_STATIC int 
vqec_cmd_vod_session_play (struct vqec_cli_def *cli,
                           char *command,
                           char *argv[], int argc)
{
    vqec_vod_error_t ret;
    vqec_vod_session_id_t id = VQEC_VOD_INVALID_ID;
    float scale = 1.0;  
    uint32_t start_time = 0;
    uint32_t end_time = 0;
    vqec_vod_ctl_params_t param;
    int cur_arg = 0;

    if (argc < 1  ||
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: vod-session play "
                       "<session-name> [scale <value>] "
                       "[range <start> [<end>]]");
        vqec_cli_print(cli, "Note: scale is in percentage");
        return VQEC_CLI_ERROR;
    }
    
    id = vod_session_get_id_by_name(argv[0]);
    if (id == VQEC_VOD_INVALID_ID) {
        vqec_cli_print(cli, "Invalid vod session name (%s)", argv[0]);
        return VQEC_CLI_ERROR;
    }

    cur_arg = 1;

    if ((argc > cur_arg) && 
        (strncmp("scale", argv[cur_arg], ARG_STRLEN) == 0)) {
        cur_arg++;
        if (argc < (cur_arg+1)) {
            vqec_cli_print(cli, "Usage: vod-session play"
                           "<session-name> [scale <value>] "
                           "[range <start> [<end>]]");
            return VQEC_CLI_ERROR;
        }
        scale = atof(argv[cur_arg]);
        cur_arg++;
    } else {
        /* by default, it is a normal play */
        scale = 1.0;
    }

    if ((argc > cur_arg) &&
        (strncmp("range", argv[cur_arg], ARG_STRLEN) == 0)) {
        cur_arg++;
        if (argc < (cur_arg+1)) {
            vqec_cli_print(cli, "Usage: vod-session play"
                           "<session-name> scale <value> "
                           "[range <start> [<end>]]");
            return VQEC_CLI_ERROR;
        }

        if ((strncmp("now", argv[cur_arg], ARG_STRLEN) == 0) ||
            (strncmp("NOW", argv[cur_arg], ARG_STRLEN) == 0)) {
            start_time = VQEC_VOD_NPT_TIME_NOW;
        } else {
            start_time = atoi(argv[cur_arg]);
        }

        if (argc == 6) {
            end_time = atoi(argv[5]);
        }
    }

    memset(&param, 0, sizeof(vqec_vod_ctl_params_t));
    if (abs(scale) > 999.9) {
        vqec_cli_print(cli, "scale is too big");
        return VQEC_CLI_ERROR;
    }
    param.method = VQEC_VOD_CTL_METHOD_PLAY;
    param.start_time = start_time;
    param.end_time = end_time;
    param.scale = scale;

    ret = vqec_vod_session_control(id, &param);

    if (ret != VQEC_VOD_OK) {
        vqec_cli_print(cli, "Play failure: vod session name = %s (%s)", 
                       argv[0], vod_error_to_string(ret));
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}

/**
 * Pause a vod session
 */

UT_STATIC int 
vqec_cmd_vod_session_pause (struct vqec_cli_def *cli,
                            char *command,
                            char *argv[], int argc)
{
    vqec_vod_error_t ret;
    vqec_vod_ctl_params_t param;
    vqec_vod_session_id_t id = VQEC_VOD_INVALID_ID;
 
   if ((argc != 1) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: vod-session pause <session-name>");
        return VQEC_CLI_ERROR;
    }

    id = vod_session_get_id_by_name(argv[0]);
    if (id == VQEC_VOD_INVALID_ID) {
        vqec_cli_print(cli, "Invalid vod session name (%s)", argv[0]);
        return VQEC_CLI_ERROR;
    }
    
    param.method = VQEC_VOD_CTL_METHOD_PAUSE;
    param.start_time = 0;
    param.end_time = 0;
   
    ret = vqec_vod_session_control (id, &param);

    if (ret != VQEC_VOD_OK) {
        vqec_cli_print(cli, "Pause failure: vod session name = %s (%s)", 
                       argv[0], vod_error_to_string(ret));
        return VQEC_CLI_ERROR;
    }
    return VQEC_CLI_OK;
}

/**
 * Display information of a vod session
 */
UT_STATIC int32_t
vqec_cmd_show_vod_session (struct vqec_cli_def *cli, char *command,
                           char *argv[], int argc) 
{
    vqec_vod_error_t ret;
    vqec_vod_session_id_t id = VQEC_VOD_INVALID_ID;

    if (argc < 1  ||
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, 
                   "Usage: show vod-session {all | name <session-name>}");
        return VQEC_CLI_ERROR;
    }

    if (strncmp("all", argv[0], ARG_STRLEN) == 0) {
        vqec_vod_session_dump_info_all();
        return VQEC_CLI_ERROR;
    } else if (strncmp("name", argv[0], ARG_STRLEN) == 0) {
        if (argc < 2  ||
            vqec_check_args_for_help_char(argv, argc)) {
            vqec_cli_print(cli, 
                      "Usage: show vod-session {all | name <session-name>}");
            return VQEC_CLI_ERROR;
        }
        id = vod_session_get_id_by_name(argv[1]);
        if (id == VQEC_VOD_INVALID_ID) {
            vqec_cli_print(cli, "Invalid vod session name (%s)", argv[0]);
            return VQEC_CLI_ERROR;
        }
        ret = vqec_vod_session_dump_info(id);
        if (ret == VQEC_VOD_OK) {
            return VQEC_CLI_OK;
        } else {
            return VQEC_CLI_ERROR;
        }
    }
    return VQEC_CLI_ERROR;
}
#endif  /* HAVE_CISCO_RTSP_LIBRARY */

/**
 * Print all parameters in the chan cfg structure.
 * Retransmission stream parameters are only printed if
 * error repair or RCC is enabled. FEC stream parameters
 * are only printed if FEC is enabled. Cached FEC parameters
 * (L value, D value, etc.) are printed only if their values
 * are non-default (non-zero).
 *
 * @param[in]  f            target stream for printing
 * @param[in]  cfg          chan configuration struct
 * @param[in]  newline      TRUE adds a trailing newline,
 *                          FALSE omits a trailing newline
 */
void vqec_cmd_parse_print_params(FILE *f,
                                 vqec_chan_cfg_t *cfg,
                                 boolean newline)
{
    char tmp[INET_ADDRSTRLEN];

    /* define a few printing macros */
#define VQEC_PRINT_ADDR_PARAM(_name, _field) \
    fprintf(f, "%s %s ", _name,              \
            inet_ntop(AF_INET, &cfg->_field, \
                      tmp, INET_ADDRSTRLEN))  
#define VQEC_PRINT_PORT_PARAM(_name, _field) \
    fprintf(f, "%s %hu ", _name, ntohs(cfg->_field))
#define VQEC_PRINT_INT_PARAM(_name, _field, _format) \
    fprintf(f, "%s " _format " ", _name, cfg->_field)
#define VQEC_PRINT_XRFLAGS_PARAM(_name, _field) \
    if (cfg->_field) {                          \
        fprintf(f, "%s ", _name);               \
    }                                           \
    if (cfg->_field & VQEC_RTCP_XR_STAT_LOSS) { \
        fprintf(f, "loss");                     \
    }                                           \
    if (cfg->_field & VQEC_RTCP_XR_STAT_DUP) {  \
        fprintf(f, ",dup");                     \
    }                                           \
    if (cfg->_field & VQEC_RTCP_XR_STAT_JITT) { \
        fprintf(f, ",jitt");                    \
    }                                           \
    fprintf(f, " ")
#define VQEC_PRINT_ENABLE_PARAM(_name, _field)  \
    if (cfg->_field) {                          \
        fprintf(f, "%s", _name);                \
    }                                           \
    fprintf(f, " ")
#define VQEC_PRINT_XRENABLE_PARAM(_name, _field)        \
    if (cfg->_field != VQEC_RTCP_XR_RLE_UNSPECIFIED) {  \
        fprintf(f, "%s", _name);                        \
    }                                                   \
    fprintf(f, " ")
    
    /* Primary Stream Transport */
    VQEC_PRINT_ADDR_PARAM("primary-dest-addr", primary_dest_addr);
    VQEC_PRINT_PORT_PARAM("primary-dest-port", primary_dest_port);
    VQEC_PRINT_PORT_PARAM("primary-dest-rtcp-port", primary_dest_rtcp_port);
    VQEC_PRINT_ADDR_PARAM("primary-src-addr", primary_source_addr);
    VQEC_PRINT_PORT_PARAM("primary-src-port", primary_source_port);
    VQEC_PRINT_PORT_PARAM("primary-src-rtcp-port", primary_source_rtcp_port);
    /* Primary Stream Parameters */
    VQEC_PRINT_INT_PARAM("primary-payload-type", primary_payload_type, "%hhu");
    VQEC_PRINT_INT_PARAM("primary-bit-rate", primary_bit_rate, "%u");
    VQEC_PRINT_INT_PARAM("primary-rtcp-sndr-bw", primary_rtcp_sndr_bw, "%u");
    VQEC_PRINT_INT_PARAM("primary-rtcp-rcvr-bw", primary_rtcp_rcvr_bw, "%u");
    if (cfg->primary_rtcp_per_rcvr_bw != VQEC_RTCP_BW_UNSPECIFIED) {
        VQEC_PRINT_INT_PARAM("primary-rtcp-per-rcvr-bw", 
                             primary_rtcp_per_rcvr_bw, "%u");
    }
    VQEC_PRINT_XRENABLE_PARAM("primary_rtcp_xr_loss_rle_enable",
                              primary_rtcp_xr_loss_rle);
    VQEC_PRINT_XRENABLE_PARAM("primary_rtcp_xr_per_loss_rle_enable",
                              primary_rtcp_xr_per_loss_rle);
    VQEC_PRINT_XRFLAGS_PARAM("primary-rtcp-xr-stat-flags",
                             primary_rtcp_xr_stat_flags);
    /* Feedback Target Address */
    VQEC_PRINT_ADDR_PARAM("fbt-addr", fbt_addr);
    /* Error Repair and RCC enable */
    VQEC_PRINT_ENABLE_PARAM("er_enable", er_enable);
    VQEC_PRINT_ENABLE_PARAM("rcc_enable", rcc_enable);
    if (cfg->er_enable || cfg->rcc_enable) {
        /* Retransmission Stream Transport */
        VQEC_PRINT_ADDR_PARAM("rtx-src-addr", rtx_source_addr);
        VQEC_PRINT_PORT_PARAM("rtx-src-port", rtx_source_port);
        VQEC_PRINT_PORT_PARAM("rtx-src-rtcp-port", rtx_source_rtcp_port);
        VQEC_PRINT_ADDR_PARAM("rtx-dest-addr", rtx_dest_addr);
        VQEC_PRINT_PORT_PARAM("rtx-dest-port", rtx_dest_port);
        VQEC_PRINT_PORT_PARAM("rtx-dest-rtcp-port", rtx_dest_rtcp_port);
        /* Retransmission Stream Parameters */
        VQEC_PRINT_INT_PARAM("rtx-payload-type", rtx_payload_type, "%hhu");
        VQEC_PRINT_INT_PARAM("rtx-rtcp-sndr-bw", rtx_rtcp_sndr_bw, "%u");
        VQEC_PRINT_INT_PARAM("rtx-rtcp-rcvr-bw", rtx_rtcp_rcvr_bw, "%u");
        VQEC_PRINT_XRENABLE_PARAM("rtx_rtcp_xr_loss_rle_enable",
                                  rtx_rtcp_xr_loss_rle);
        VQEC_PRINT_XRFLAGS_PARAM("rtx-rtcp-xr-stat-flags",
                                 rtx_rtcp_xr_stat_flags);
    }
    /* FEC enable */
    VQEC_PRINT_ENABLE_PARAM("fec_enable", fec_enable);
    if (cfg->fec_enable) {
        switch (cfg->fec_mode) {
        case VQEC_FEC_2D_MODE:
            fprintf(f, "fec-mode 2D ");
            break;
        case VQEC_FEC_1D_MODE:
        default:
            fprintf(f, "fec-mode 1D ");
            break;
        }
        /* FEC 1 Transport */
        VQEC_PRINT_ADDR_PARAM("fec1-mcast-addr", fec1_mcast_addr);
        VQEC_PRINT_PORT_PARAM("fec1-mcast-port", fec1_mcast_port);
        VQEC_PRINT_PORT_PARAM("fec1-mcast-rtcp-port", fec1_mcast_rtcp_port);
        VQEC_PRINT_ADDR_PARAM("fec1-src-addr", fec1_source_addr);
        VQEC_PRINT_INT_PARAM("fec1-payload-type", fec1_payload_type, "%hhu");
        VQEC_PRINT_INT_PARAM("fec1-rtcp-sndr-bw", fec1_rtcp_sndr_bw, "%u");
        VQEC_PRINT_INT_PARAM("fec1-rtcp-rcvr-bw", fec1_rtcp_rcvr_bw, "%u");
        /* FEC 2 Transport */
        VQEC_PRINT_ADDR_PARAM("fec2-mcast-addr", fec2_mcast_addr);
        VQEC_PRINT_PORT_PARAM("fec2-mcast-port", fec2_mcast_port);
        VQEC_PRINT_PORT_PARAM("fec2-mcast-rtcp-port", fec2_mcast_rtcp_port);
        VQEC_PRINT_ADDR_PARAM("fec2-src-addr", fec2_source_addr);
        VQEC_PRINT_INT_PARAM("fec2-payload-type", fec2_payload_type, "%hhu");
        VQEC_PRINT_INT_PARAM("fec2-rtcp-sndr-bw", fec2_rtcp_sndr_bw, "%u");
        VQEC_PRINT_INT_PARAM("fec2-rtcp-rcvr-bw", fec2_rtcp_rcvr_bw, "%u");
        /* FEC Common Parameters - print conditionally */
        if (cfg->fec_l_value) {
            VQEC_PRINT_INT_PARAM("fec-l-value", fec_l_value, "%hhu");
        }
        if (cfg->fec_d_value) {
            VQEC_PRINT_INT_PARAM("fec-d-value", fec_d_value, "%hhu");
        }
        if (cfg->fec_order) {
            switch (cfg->fec_order) {
            case VQEC_FEC_SENDING_ORDER_ANNEXA:
                fprintf(f, "fec-order annexa ");
                break;
            case VQEC_FEC_SENDING_ORDER_ANNEXB:
                fprintf(f, "fec-order annexb ");
                break;
            case VQEC_FEC_SENDING_ORDER_OTHER:
                fprintf(f, "fec-order other ");
                break;
            case VQEC_FEC_SENDING_ORDER_NOT_DECIDED:
            default:
                fprintf(f, "fec-order none ");
                break;
            }
        }
        if (cfg->fec_ts_pkt_time) {
            VQEC_PRINT_INT_PARAM("fec-ts-pkt-time", fec_ts_pkt_time, "%llu");
        }
    }
    if (newline) {
        fprintf(f, "\r\n");
    }
}

UT_STATIC int vqec_cmd_channel_update (struct vqec_cli_def *cli, 
                                       char *command, 
                                       char *argv[], int argc)
{
    vqec_chan_cfg_t cfg;
    char *param_name, *param_value;
    int cur_arg;
    boolean parse_success = FALSE;
    vqec_error_t ret = VQEC_OK;

    if (vqec_check_args_for_help_char(argv, argc)) {
        goto done;
    }

    /* Load supplied parameters into 'cfg' */
    memset(&cfg, 0, sizeof(cfg));
    for (cur_arg = 0; cur_arg < argc;) {
        param_name = argv[cur_arg];
        param_value = (cur_arg+1 < argc ? argv[cur_arg+1] : "");
        if (!vqec_cmd_parse_chan_param(&cfg, param_name, param_value,
                                       &cur_arg)) {
            goto done;
        }
    }
    parse_success = TRUE;

    ret = vqec_ifclient_chan_update(&cfg, NULL);

done:
    if (!parse_success) {
        vqec_cli_print(cli, 
                       "Usage: channel update primary-dest-addr <addr> "
                       "primary-dest-port <port>\n"
                       "primary-src-addr <addr> primary-src-port <port>\n"
                       "fbt-addr <addr> primary-src-rtcp-port <port>\n"
                       "rtx-src-addr <addr>  rtx-src-port <port>\n"
                       "rtx-src-rtcp-port <port>\n");
        return (VQEC_CLI_ERROR);
    } else if (ret != VQEC_OK) {
        vqec_cli_print(cli, "Error:  could not update channel (%s)",
                       vqec_err2str(ret));
        return (VQEC_CLI_ERROR);
    }
    return (VQEC_CLI_OK);
}

UT_STATIC int vqec_cmd_parse_sdp (struct vqec_cli_def *cli, char *command, 
                                  char *argv[], int argc)
{
    vqec_chan_type_t chan_type;
    vqec_chan_cfg_t cfg;
    FILE *f, *output_file;
    long file_size;
    char *buffer, *arg1, *arg2;
    int cur_arg = 0;
    boolean success = TRUE;

    if (argc < 2 || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: parse sdp linear <sdp_filename> "
            "[output <output_filename>]");
        vqec_cli_print(cli, "       parse sdp vod <sdp_filename> "
            "[output <output_filename>] primary-dest-addr <addr> "
            "primary-dest-port <port> [primary-dest-rtcp-port <port>] "
            "[primary-src-addr <addr>] [primary-src-port <port>] "
            "[primary-src-rtcp-port <port>] [fbt-addr <addr>] "
            "[rtx-dest-addr <addr>] [rtx-dest-port <port>] "
            "[rtx-dest-rtcp-port <port>] [rtx-src-addr <addr>] "
            "[rtx-src-port <port>] [rtx-src-rtcp-port <port>]");
        return VQEC_CLI_ERROR;
    }

    if (strncmp("linear", argv[0], ARG_STRLEN) == 0) {
        chan_type = VQEC_CHAN_TYPE_LINEAR;
    } else if (strncmp("vod", argv[0], ARG_STRLEN)== 0) {
        chan_type = VQEC_CHAN_TYPE_VOD;
    } else {
        vqec_cli_print(cli, "Usage: parse sdp linear <sdp_filename> "
            "[output <output_filename>]");
        vqec_cli_print(cli, "       parse sdp vod <sdp_filename> "
            "[output <output_filename>] primary-dest-addr <addr> "
            "primary-dest-port <port> [primary-dest-rtcp-port <port>] "
            "[primary-src-addr <addr>] [primary-src-port <port>] "
            "[primary-src-rtcp-port <port>] [fbt-addr <addr>] "
            "[rtx-dest-addr <addr>] [rtx-dest-port <port>] "
            "[rtx-dest-rtcp-port <port>] [rtx-src-addr <addr>] "
            "[rtx-src-port <port>] [rtx-src-rtcp-port <port>]");
        return VQEC_CLI_ERROR;
    }

    /* Open file indicated on cmd line */
    f = fopen(argv[1], "r");
    if (!f) {
        vqec_cli_print(cli, "Cannot open file %s", argv[1]);
        return VQEC_CLI_ERROR;
    }
    
    /* Check file size */
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    rewind(f);

    /* Allocate buffer for file contents*/
    buffer = (char *)malloc(sizeof(char)*(file_size+1));
    if (!buffer) {
        fclose(f);
        vqec_cli_print(cli, "Failed to allocate file buffer\n");
        return VQEC_CLI_ERROR;
    }

    /* Read file contents into buffer */
    if (fread(buffer, 1, file_size, f) != file_size) {
        fclose(f);
        free(buffer);
        vqec_cli_print(cli, "Failed to read sdp file (%s)\n", argv[1]);
        return VQEC_CLI_ERROR;
    }
    buffer[file_size] = '\0';

    /* Close file handle */
    fclose(f);
    f = NULL;
    
    /* Parse SDP buffer into channel configuration */
    if (!vqec_ifclient_chan_cfg_parse_sdp(&cfg, buffer, chan_type)) {
        free(buffer);
        vqec_cli_print(cli, "Failed to parse sdp\n");
        return VQEC_CLI_ERROR;
    }
    free(buffer);
    cur_arg = 2;

    /* Check for output argument */
    if (argc > 3 && !strncmp("output", argv[cur_arg], ARG_STRLEN)) {
        output_file = fopen(argv[cur_arg+1], "w");
        if (!output_file) {
            vqec_cli_print(cli, "Cannot open output file (%s)", 
                           argv[cur_arg+1]);
            return VQEC_CLI_ERROR;
        }
        cur_arg += 2;
    } else {
        output_file = NULL;
    }

    /* Parse additional non-SDP parameters in VoD case */
    if (chan_type == VQEC_CHAN_TYPE_VOD) {
        while (cur_arg < argc) {
            arg1 = argv[cur_arg];
            arg2 = cur_arg+1 < argc ? argv[cur_arg+1] : "";
            if (!vqec_cmd_parse_chan_param(&cfg, arg1, arg2, &cur_arg)) {
                vqec_cli_print(cli, "Bad chan parameters (%s %s)", 
                               arg1, arg2);
                success = FALSE;
                break;
            }
        }
    }

    /* Print out all parameters on success */
    if (success) {
        if (output_file) {
            vqec_cmd_parse_print_params(output_file, &cfg, TRUE);
        } else {
            vqec_cmd_parse_print_params(cli->client, &cfg, TRUE);
        }
    }

    /* Close output file if one was opened */
    if (output_file) {
        fclose(output_file);
        output_file = NULL;
    }

    return success ? VQEC_CLI_OK : VQEC_CLI_ERROR;
}

/* added for use  by FEC */

UT_STATIC int vqec_cmd_fec_enable (struct vqec_cli_def *cli, char *command, 
                                   char *argv[], int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: fec enable");
        return VQEC_CLI_ERROR;
    } else {
        (void)vqec_dp_set_fec(TRUE);
        return VQEC_CLI_OK;
    }
}

UT_STATIC int vqec_cmd_fec_disable (struct vqec_cli_def *cli, char *command, 
                                    char *argv[], int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: fec disable");
        return VQEC_CLI_ERROR;
    } else {
        (void)vqec_dp_set_fec(FALSE);
        return VQEC_CLI_OK;
    }
}

/* added for use  by RCC */

#if HAVE_FCC

UT_STATIC int vqec_cmd_rcc_enable (struct vqec_cli_def *cli, char *command, 
                                   char *argv[], int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: rcc enable");
        return VQEC_CLI_ERROR;
    } else {
        vqec_set_rcc(TRUE);
        return VQEC_CLI_OK;
    }
}

UT_STATIC int vqec_cmd_rcc_disable (struct vqec_cli_def *cli, char *command, 
                                    char *argv[], int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: rcc disable");
        return VQEC_CLI_ERROR;
    } else {
        vqec_set_rcc(FALSE);
        return VQEC_CLI_OK;
    }
}

UT_STATIC int vqec_cmd_fast_fill_enable (struct vqec_cli_def *cli, 
                                         char *command, 
                                         char *argv[], int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: fast-fill enable");
        return VQEC_CLI_ERROR;
    } else {
        vqec_set_fast_fill(TRUE);
        return VQEC_CLI_OK;
    }
}

UT_STATIC int vqec_cmd_fast_fill_disable (struct vqec_cli_def *cli, 
                                          char *command, 
                                          char *argv[], int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: fast-fill disable");
        return VQEC_CLI_ERROR;
    } else {
        vqec_set_fast_fill(FALSE);
        return VQEC_CLI_OK;
    }
}
#endif /* if HAVE_FCC */

/*
 * COMMAND FUNCTIONS FOR 'CLEAR' COMMANDS
 */
UT_STATIC int vqec_cmd_clear_counters (struct vqec_cli_def *cli, 
                                       char *command, 
                                       char *argv[], int argc)
{
    vqec_tuner_iter_t iter;
    vqec_tunerid_t id = VQEC_TUNERID_INVALID;
    
    if ((argc != 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: clear counters");
        return VQEC_CLI_ERROR;
    }
    
    /* clear global counters */
    g_vqec_error_repair_policed_requests = 0;
    if (vqec_ifclient_histogram_clear(VQEC_HIST_JOIN_DELAY) != VQEC_OK) {
        vqec_cli_print(cli, "Error in clearing join-delay histogram");
    }
    
    /* clear global/historical tuner counters */
    vqec_ifclient_clear_stats();

    /* clear all stream output counters */
    FOR_ALL_TUNERS_IN_LIST(iter, id) {
        vqec_stream_output_mgr_clear_stats(id);
    }
    
    return VQEC_CLI_OK;
}   

/*
 * COMMAND FUNCTIONS FOR 'DEBUG' COMMANDS
 */

/*
 * Module specific actions for debugging.
 */
static void
vqec_debug_en_module_flags (vqec_cli_debug_type_t type, boolean enabled)
{
    switch (type.cp) {
        
    case VQEC_DEBUG_CHANNEL:
        if (enabled) {
            VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_MGR);
            VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_DB);
            VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_CHANNEL);
        } else {
            VQE_CFG_RESET_DEBUG_FLAG(CFG_DEBUG_MGR);
            VQE_CFG_RESET_DEBUG_FLAG(CFG_DEBUG_DB);
            VQE_CFG_RESET_DEBUG_FLAG(CFG_DEBUG_CHANNEL);
        }
        break;
        
    case VQEC_DEBUG_NAT:
        if (enabled) {
            vqec_nat_enable_debug(TRUE);
        } else {
            vqec_nat_disable_debug();
        }
        break;
        
    default:
        break;
    }
}     

/*
 * debug <type> <enable | disable>
 *   A function that implements old- and new-style debug functions.
 *   fixme: This should be modified when we remove the old debugging api.
 */
int vqec_debug_func (struct vqec_cli_def *cli, char *command, 
                     char *argv[], int argc)
{
    vqec_cli_debug_type_t type = {VQEC_DEBUG_FLAG_INVALID,
                                  VQEC_DEBUG_FLAG_INVALID};
                                      /* used to hold the debug flag */
    boolean config_all = FALSE;
    boolean enabled = FALSE;
    char *token;
    char *last = NULL;

    char *usage_string = "Usage: debug <type> {enable | disable}";
    
    char *PARAMS[] = { "channel",
                       "pcm",
                       "input",
                       "output",
                       "event",
                       "igmp",
                       "tuner",
                       "recv-socket",
                       "timer",
                       "rtcp",
                       "error-repair",
                       "rcc",
                       "nat",
                       "chan_cfg",
                       "stream_output", 
                       "updater",
                       "upcall",
                       "cpchan",
                       "vod",
                       "dp-tlm",
                       "dp-inputshim",
                       "dp-outputshim",
                       "dp-outputshim-pak",
                       "dp-nll",
                       "dp-nll-adjust",
                       "dp-pcm",
                       "dp-pcm-pak",
                       "dp-error-repair",
                       "dp-rcc",
                       "dp-fec",
                       "dp-collect-stats",
                       "dp-failover",
                       "all",
                       "help",
                       "?",
                       (char *)0 };

    char *PARAMS1[] = { "enable", "disable", "help", "?", (char *)0 };
    
    if (argc > 0) {
        vqec_cli_print(cli, usage_string);
        return VQEC_CLI_ERROR;
    }

    /*
     * Extract 'debug' key word from command.
     */
    token = strtok_r(command, " ", &last);
    if (!token) {
        return VQEC_CLI_ERROR;
    }
    VQEC_ASSERT(strncmp(token, "debug", strlen(command)) == 0);

    /*
     * Extract <type> from command
     */
    token = strtok_r(NULL, " ", &last);
    if (!token) {
        return VQEC_CLI_ERROR;
    }
    VQEC_ASSERT(token);

    switch (vqec_str_match(token, PARAMS)) {
        case 0:
            type.cp = VQEC_DEBUG_CHANNEL;
            break;
        case 1:
            type.cp = VQEC_DEBUG_PCM;
            break;
        case 2:
            type.cp = VQEC_DEBUG_INPUT;
            break;
        case 3:
            type.cp = VQEC_DEBUG_OUTPUT;
            break;
        case 4:
            type.cp = VQEC_DEBUG_EVENT;
            break;
        case 5:
            type.cp = VQEC_DEBUG_IGMP;
            break;
        case 6:
            type.cp = VQEC_DEBUG_TUNER;
            break;
        case 7:
            type.cp = VQEC_DEBUG_RECV_SOCKET;
            break;
        case 8:
            type.cp = VQEC_DEBUG_TIMER;
            break;
        case 9:
            type.cp = VQEC_DEBUG_RTCP;
            break;
        case 10:
            type.cp = VQEC_DEBUG_ERROR_REPAIR;
            break;
        case 11:
            type.cp = VQEC_DEBUG_RCC;
            break;
        case 12:
            type.cp = VQEC_DEBUG_NAT;
            break;
        case 13:
            type.cp = VQEC_DEBUG_CHAN_CFG;
            break;
        case 14:
            type.cp = VQEC_DEBUG_STREAM_OUTPUT;
            break;
        case 15:
            type.cp = VQEC_DEBUG_UPDATER;
            break;
        case 16:
            type.cp = VQEC_DEBUG_UPCALL;
            break;
        case 17:
            type.cp = VQEC_DEBUG_CPCHAN;
            break;
        case 18:
            type.cp = VQEC_DEBUG_VOD;
            break;
        case 19:
            type.dp = VQEC_DP_DEBUG_TLM;
            break;
        case 20:
            type.dp = VQEC_DP_DEBUG_INPUTSHIM;
            break;
        case 21:
            type.dp = VQEC_DP_DEBUG_OUTPUTSHIM;
            break;
        case 22:
            type.dp = VQEC_DP_DEBUG_OUTPUTSHIM_PAK;
            break;
        case 23:
            type.dp = VQEC_DP_DEBUG_NLL;
            break;
        case 24:
            type.dp = VQEC_DP_DEBUG_NLL_ADJUST;
            break;
        case 25:
            type.dp = VQEC_DP_DEBUG_PCM;
            break;
        case 26:
            type.dp = VQEC_DP_DEBUG_PCM_PAK;
            break;
        case 27:
            type.dp = VQEC_DP_DEBUG_ERROR_REPAIR;
            break;
        case 28:
            type.dp = VQEC_DP_DEBUG_RCC;
            break;
        case 29:
            type.dp = VQEC_DP_DEBUG_FEC;
            break;
        case 30:
            type.dp = VQEC_DP_DEBUG_COLLECT_STATS;
            break;
        case 31:
            type.dp = VQEC_DP_DEBUG_FAILOVER;
            break;
        case 32:
            config_all = TRUE;
            break;
        case 33:  /* help */
            /*FALLTHRU*/
        case 34:  /* ? */
            /*FALLTHRU*/
        default:
            vqec_cli_print(cli, usage_string);
            return VQEC_CLI_ERROR;
    }

    /*
     * Extract <3rd param>
     */
    token = strtok_r(NULL, " ", &last);
    if (!token) {
        return VQEC_CLI_ERROR;
    }
    VQEC_ASSERT(token);

    switch (vqec_str_match(token,PARAMS1)) {
      case 0: /* enable */
        enabled = TRUE;
        break;
      case 1: /* disable */
        enabled = FALSE;
        break;
      case 2: /* help */
          /*FALLTHRU*/
      case 3: /* ? */
          /*FALLTHRU*/
      default:
        vqec_cli_print(cli, usage_string);
        return VQEC_CLI_ERROR;
    }

    if (config_all) {
        /* CP flags */
        for (type.cp = VQEC_DEBUG_FIRST; 
             type.cp < VQEC_DEBUG_MAX;
             type.cp++) {
            if (enabled) {
                VQEC_SET_DEBUG_FLAG(type.cp);
            } else {
                VQEC_RESET_DEBUG_FLAG(type.cp);
            }
            vqec_debug_en_module_flags(type, enabled);
        }
        /* DP flags */
        for (type.dp = VQEC_DP_DEBUG_FIRST;
             type.dp < VQEC_DP_DEBUG_MAX;
             type.dp++) {
            if (enabled) {
                vqec_dp_debug_flag_enable(type.dp);
            } else {
                vqec_dp_debug_flag_disable(type.dp);
            }
        }

    } else {

        if (enabled) {
            if (type.cp != VQEC_DEBUG_FLAG_INVALID) {
                VQEC_SET_DEBUG_FLAG(type.cp);
            }
            if (type.dp != VQEC_DEBUG_FLAG_INVALID) {
                vqec_dp_debug_flag_enable(type.dp);
            }
        } else {
            if (type.cp != VQEC_DEBUG_FLAG_INVALID) {
                VQEC_RESET_DEBUG_FLAG(type.cp);
            }
            if (type.dp != VQEC_DEBUG_FLAG_INVALID) {
                vqec_dp_debug_flag_disable(type.dp);
            }
        }

        vqec_debug_en_module_flags(type, enabled);
    }

    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_send_debugs_to_cli_enable (struct vqec_cli_def *cli,
                                                  char *command, 
                                                  char *argv[],
                                                  int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: send-debugs-to-cli enable");
        return VQEC_CLI_ERROR;
    } else {
        vqec_enable_debugs_goto_cli(TRUE);
        return VQEC_CLI_OK;
    }
}

UT_STATIC int vqec_cmd_send_debugs_to_cli_disable (struct vqec_cli_def *cli,
                                                   char *command, 
                                                   char *argv[],
                                                   int argc)
{
    if (argc != 0 || strchr(command, '?') || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: send-debugs-to-cli disable");
        return VQEC_CLI_ERROR;
    } else {
        vqec_enable_debugs_goto_cli(FALSE);
        return VQEC_CLI_OK;
    }
}

/*
 * COMMAND FUNCTIONS FOR 'HAG' COMMANDS
 */

#ifdef HAVE_HAG_MODE

/*
 * proxy-igmp-join <tuner-name> <stb-if> <stb-addr>
 *    Use IGMP joins from <stb-addr> to select the desired input stream,
 *      and re-source the stream back out to address
 *    <stb-addr> = 0.0.0.0 disables use of proxy-igmp-join
 *    by default, this is disabled.
 */
UT_STATIC int vqec_cmd_proxy_igmp_join (struct vqec_cli_def *cli, 
                                        char *command, 
                                        char *argv[], int argc)
{
    in_addr_t stbaddr;

    if ((argc != 3) || 
         vqec_check_args_for_help_char(argv, argc)) { 
         vqec_cli_print(cli, "Usage: proxy-igmp-join <tuner-name> <stb-if> "
                        "<stb-addr>\n"
                        "e.g. proxy-igmp-join 0 eth1 192.168.1.128\n"
                        "Note: A stb-addr of 0.0.0.0 can be used to disable "
                        "proxy-igmp-join once it has been enabled.");
        return VQEC_CLI_ERROR;
    }
    
    if (!vqec_url_ip_parse(argv[2], &stbaddr)) {
        vqec_cli_print(cli,"Invalid stb-addr %s", argv[2]);
        return VQEC_CLI_ERROR;
    }

    if (vqec_proxy_igmp_join(argv[0], argv[1], 
                             stbaddr,
                             vqec_stream_output_channel_join_call_back_func,
                             NULL,
                             vqec_stream_output_channel_leave_call_back_func,
                             NULL)
        != VQEC_OK) {
        vqec_cli_print(cli, "Failed to start igmp join proxy");
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}

#endif /* HAVE_HAG_MODE */

UT_STATIC int vqec_cmd_stream_output (struct vqec_cli_def *cli, 
                                      char *command, 
                                      char *argv[], int argc)
{
    vqec_error_t err = VQEC_OK;

    if ((argc != 2 && argc != 3) || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: stream-output <tuner-name> { null | "
                       "{ <if-name> <output-url> }} - ex. "
                       "stream-output 0 eth1 udp://192.168.1.128:50000\n"
                       "If output-url is udp://0.0.0.0:port will turn stream "
                       "off\n"
                       "If 'null' option present, packets read but not sent");
        return VQEC_CLI_ERROR;
    }

    err = vqec_stream_output(argv[0], argv[1], argc == 3 ? argv[2] : NULL);
    if (err != VQEC_OK) {
        vqec_cli_print(cli, "stream-output failed (%s)!", vqec_err2str(err));
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}

UT_STATIC int vqec_cmd_show_stream_output (struct vqec_cli_def *cli, 
                                           char *command, 
                                           char *argv[],  int argc)
{
    vqec_tuner_iter_t iter;
    vqec_tunerid_t id = VQEC_TUNERID_INVALID;

    if (vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: show stream-output [<tuner-name>]");
        return (VQEC_CLI_ERROR);
    }

    if (argc == 0) {
        FOR_ALL_TUNERS_IN_LIST(iter, id) {
            vqec_stream_output_mgr_show_stats(id);
        }
    } else {               
        if ((id = vqec_ifclient_tuner_get_id_by_name(argv[0])) == 
            VQEC_TUNERID_INVALID) {
            vqec_cli_print(cli, "invalid tuner name!");
            return (VQEC_CLI_ERROR);
        }

        vqec_stream_output_mgr_show_stats(id);
    }
    
    return (VQEC_CLI_OK);
}

static void vqec_cmd_test_dp_reader_print_usage (struct vqec_cli_def *cli)
{
    vqec_cli_print(cli, "Usage: test-dp-reader <tuner-name> { {<src-address> "
                   "<dest-address> <dest-port> <num-iobufs> <iobuf-size> "
                   "<timeout>} | disable } - ex. "
                   "test-dp-reader 0 5.8.48.2 192.168.1.128 50000 16 2048 -1\n"
                   "  if the disable option is present, the reader "
                   "task will be stopped");
}


UT_STATIC int vqec_cmd_test_dp_reader (struct vqec_cli_def *cli, 
                                       char *command,
                                       char *argv[], int argc)
{
    test_vqec_reader_params_t p;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    memset(&p, 0, sizeof(test_vqec_reader_params_t));

    if ((argc != 2 && argc != 7) || 
        vqec_check_args_for_help_char(argv, argc)) {
        vqec_cmd_test_dp_reader_print_usage(cli);
        return (VQEC_CLI_ERROR);
    }

    /* if disable option present, stop the reader task */
    if (!strncmp(argv[2], "dis", 3)) {
        p.disable = 1;
        vqec_dp_test_reader(p);
        return (VQEC_CLI_OK);
    }

    p.tid = vqec_ifclient_tuner_get_id_by_name(argv[0]);
    if (p.tid < 0) {
        vqec_cmd_test_dp_reader_print_usage(cli);
        vqec_cli_print(cli, "test-dp-reader failed (%s)!",
                       vqec_err2str(VQEC_ERR_NOSUCHTUNER));
        return (VQEC_CLI_ERROR);
    }

    /* use the values from the module params */
    if (!strncmp(argv[1], "0", 1) && !strncmp(argv[2], "0", 1)) {
        p.srcaddr = 0;
        p.dstaddr = 0;
    } else if (!vqec_url_ip_parse(argv[1], &p.srcaddr) ||
               !vqec_url_ip_parse(argv[2], &p.dstaddr)) {
        vqec_cli_print(cli, "test-dp-reader failed "
                       "(failed to parse src/dest addresses)\n");
        return (VQEC_CLI_ERROR);
    }
    p.dstport = atoi(argv[3]);
    p.iobufs = atoi(argv[4]);
    p.iobuf_size = atoi(argv[5]);
    p.timeout = atoi(argv[6]);

    err = vqec_dp_test_reader(p);
    if (err != VQEC_DP_ERR_OK) {
        vqec_cli_print(cli, "test-dp-reader failed (%s)!", 
                       vqec_dp_err2str(err));
        return (VQEC_CLI_ERROR);
    }

    return (VQEC_CLI_OK);
}

/*
 * COMMAND FUNCTIONS FOR 'MONITOR' COMMANDS
 */
#ifdef HAVE_ELOG
UT_STATIC int vqec_cmd_monitor_elog (struct vqec_cli_def *cli, 
                                     char *command, 
                                     char *argv[], int argc)
{
    char *token;
    char *last;

    char *PARAMS[] = { "on", "off", "reset", "dump", "help", "?", (char *)0 };

    /*
     * Extract 'monitor' key word from command.
     */
    token = strtok_r(command, " ", &last);
    if (!token) {
        return VQEC_CLI_ERROR;
    }
    VQEC_ASSERT(strncmp(token, "monitor", sizeof(token)) == 0);

    /*
     * Extract 'elog' key word from command
     */
    token = strtok_r(NULL, " ", &last);
    if (!token) {
        return VQEC_CLI_ERROR;
    }
    VQEC_ASSERT(strncmp(token, "elog", sizeof(token)) == 0);

    /*
     * Extract elog option from command
     */
    token = strtok_r(NULL, " ", &last);
    if (!token) {
        return VQEC_CLI_ERROR;
    }
    VQEC_ASSERT(token);

    if (vqec_check_args_for_help_char(argv, argc))
        token = "?";
    switch (vqec_str_match(token, PARAMS)) {
        case 0:
            vqec_monitor_elog_set(TRUE);
            return VQEC_CLI_OK;
        case 1:
            vqec_monitor_elog_set(FALSE);
            return VQEC_CLI_OK;
        case 2:
            vqec_monitor_elog_reset();
            return VQEC_CLI_OK;
        case 3:
            if (argc != 1) {
                vqec_cli_print(cli, "Usage: monitor elog dump <filename>");
                return VQEC_CLI_ERROR;
            }
            vqec_monitor_elog_dump(argv[0]);
            return VQEC_CLI_OK;
        case 4:
            /*FALLTHRU*/
        default:
            vqec_cli_print(cli, 
                    "Usage: monitor elog {[on|off]|reset|dump <filename>}");
            return VQEC_CLI_ERROR;
    }
}
#endif /* HAVE_ELOG */

UT_STATIC int
vqec_cmd_monitor_outputsched_show (struct vqec_cli_def *cli, char *command, 
                                   char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: monitor output-sched show");
        return VQEC_CLI_ERROR;
    }

    if (vqec_ifclient_histogram_display(VQEC_HIST_OUTPUTSCHED) != VQEC_OK) {
        vqec_cli_print(cli, "Error in displaying histogram");
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}

UT_STATIC int
vqec_cmd_monitor_outputsched_off (struct vqec_cli_def *cli, char *command, 
                                  char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: monitor output-sched off");
        return VQEC_CLI_ERROR;
    }

    if (vqec_dp_chan_hist_outputsched_enable(FALSE) != VQEC_DP_ERR_OK) {
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}

UT_STATIC int
vqec_cmd_monitor_outputsched_on (struct vqec_cli_def *cli, char *command, 
                                 char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: monitor output-sched on");
        return VQEC_CLI_ERROR;
    }

    if (vqec_dp_chan_hist_outputsched_enable(TRUE) != VQEC_DP_ERR_OK) {
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}

UT_STATIC int
vqec_cmd_monitor_outputsched_reset (struct vqec_cli_def *cli, char *command, 
                                    char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: monitor output-sched reset");
        return VQEC_CLI_ERROR;
    }

    if (vqec_ifclient_histogram_clear(VQEC_HIST_OUTPUTSCHED)
        != VQEC_DP_ERR_OK) {
        return VQEC_CLI_ERROR;
    }

    return VQEC_CLI_OK;
}

UT_STATIC int
vqec_cmd_monitor_benchmark_show (struct vqec_cli_def *cli, char *command, 
                                 char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: monitor benchmark show");
        return VQEC_CLI_ERROR;
    }

    vqec_cli_benchmark_show();

    return VQEC_CLI_OK;
}

UT_STATIC int
vqec_cmd_monitor_benchmark_off (struct vqec_cli_def *cli, char *command, 
                                char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: monitor benchmark off");
        return VQEC_CLI_ERROR;
    }

    vqec_event_enable_benchmark(FALSE);

    return VQEC_CLI_OK;
}

UT_STATIC int
vqec_cmd_monitor_benchmark_on (struct vqec_cli_def *cli, char *command, 
                               char *argv[], int argc)
{
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: monitor benchmark on");
        return VQEC_CLI_ERROR;
    }

    vqec_event_enable_benchmark(TRUE);

    return VQEC_CLI_OK;
}

/**
 * Callback function to display show tech-support information.
 *
 * The information dumped is equivalent to what would be seen if a user were
 * to log into the VQE-C CLI and issue the following commands:
 *   show system-config
 *   show tuner all detail
 *   show channel
 *   show counters
 *   show dp counters
 *   show update
 *   show drop
 *   show error-repair
 *   show fec
 *   show rcc
 *   show fast-fill
 *   show nat
 *   show pak-pool
 *   show stream-output
 *   show ipc
 *   show proxy-igmp
 *
 * param[in] cli struct cli_def * - the handle of the cli structure. 
 * param[in] command char * - the entire command which was entered. 
 *                            This is after command expansion. 
 * param[in]argv char ** - the list of arguments entered
 * param[in]argc int - the number of arguments entered 
 * @return vqec_error_t Returns VQEC_OK if the API was successfully executed. 
 */
UT_STATIC int
vqec_cmd_show_tech_support (struct vqec_cli_def *cli, char *command,
                            char *argv[], int argc)
{
    vqec_cli_error_t retval;
    char *temp_argv[VQEC_CLI_MAX_ARGS];
    char *temp_command;
    int temp_argc;
    vqec_cli_error_t temp_retval = VQEC_CLI_OK;
    if ((argc > 0) || vqec_check_args_for_help_char(argv, argc)) {
        vqec_cli_print(cli, "Usage: show tech-support");
        return VQEC_CLI_ERROR;
    } 
    /* That we have reached here indicates that there is no help char */

    vqec_cli_print(cli, "\n# Command \"show system-config\" :");
    temp_command = strdup("show system-config");
    retval = vqec_cmd_show_system_config(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    /* "show tuner all detail" command */
    if (VQEC_CLI_MAX_ARGS >= 2) {
        vqec_cli_print(cli, "\n# Command \"show tuner all detail\" :");
        temp_argv[0] = strdup("all");
        temp_argv[1] = strdup("detail");
        temp_argc = 2;
        temp_command = strdup("show tuner");
        retval = vqec_cmd_show_tuner(cli, temp_command, temp_argv, temp_argc);
        if (retval == VQEC_CLI_ERROR) {
            temp_retval = VQEC_CLI_ERROR;
        }
        free(temp_argv[0]);
        free(temp_argv[1]);
        free(temp_command);
    }

    vqec_cli_print(cli, "\n# Command \"show channel\" :");
    temp_command = strdup("show channel");
    retval = vqec_cmd_show_channel(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    vqec_cli_print(cli, "\n# Command \"show counters\" :");
    temp_command = strdup("show counters");
    retval = vqec_cmd_show_counters(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);


    if (VQEC_CLI_MAX_ARGS > 0) {
        vqec_cli_print(cli, "\n# Command \"show dp counters\" :");
        temp_argv[0] = strdup("counters");
        temp_argc = 1;
        temp_command = strdup("show dp");
        retval = vqec_cmd_show_dp(cli, temp_command, temp_argv, temp_argc);
        if (retval == VQEC_CLI_ERROR) {
            temp_retval = VQEC_CLI_ERROR;
        }
        free(temp_argv[0]);
        free(temp_command);
    }

    vqec_cli_print(cli, "\n# Command \"show update\" :");
    temp_command = strdup("show update");
    retval = vqec_cmd_show_update(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    vqec_cli_print(cli, "\n# Command \"show drop\" :");
    temp_command = strdup("show drop");
    retval = vqec_cmd_show_drop(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    vqec_cli_print(cli, "\n# Command \"show error-repair\" :");
    temp_command = strdup("show drop");
    retval = vqec_cmd_show_error_repair(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    vqec_cli_print(cli, "\n# Command \"show fec\" :");
    temp_command = strdup("show fec");
    retval = vqec_cmd_show_fec(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    vqec_cli_print(cli, "\n# Command \"show rcc\" :");
    temp_command = strdup("show rcc");
    retval = vqec_cmd_show_rcc(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    vqec_cli_print(cli, "\n# Command \"show fast-fill\" :");
    temp_command = strdup("show fast-fill");
    retval = vqec_cmd_show_fast_fill(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    vqec_cli_print(cli, "\n# Command \"show nat\" :");
    temp_command = strdup("show nat");
    retval = vqec_cmd_show_nat(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    vqec_cli_print(cli, "\n# Command \"show pak-pool\" :");
    temp_command = strdup("show pak-pool");
    retval = vqec_cmd_show_pak_pool(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    vqec_cli_print(cli, "\n# Command \"show stream-output\" :");
    temp_command = strdup("show stream-output");
    retval = vqec_cmd_show_stream_output(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    vqec_cli_print(cli, "\n# Command \"show ipc\" :");
    temp_command = strdup("show ipc");
    retval = vqec_cmd_show_ipc(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    vqec_cli_print(cli, "\n# Command \"show proxy-igmp\" :");
    temp_command = strdup("show proxy-igmp");
    retval = vqec_cmd_show_proxy_igmp(cli, temp_command, argv, argc);
    if (retval == VQEC_CLI_ERROR) {
        temp_retval = VQEC_CLI_ERROR;
    }
    free(temp_command);

    return temp_retval;
}

/*
 *
 * BEGIN COMMAND REGISTRATION HERE
 *
 */

/*
 * Register VQE-C 'show' commands
 */
void vqec_register_show_cmds (struct vqec_cli_def *cli)
{
    struct vqec_cli_command *cmd_show_c=0;

    /*
     * show
     * show cmd link point
     */
    cmd_show_c = vqec_cli_register_command(cli, NULL, "show", NULL,
                                           PRIVILEGE_UNPRIVILEGED, 
                                           VQEC_CLI_MODE_EXEC, 
                                           "Commands to display VQE-C"
                                           " state information");
    VQEC_ASSERT(cmd_show_c);

    /*
     * show channel
     * Displays a list of the channels configured from SDP
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (2) */
    vqec_cli_register_command(cli, cmd_show_c, "channel", 
                              vqec_cmd_show_channel,
                              PRIVILEGE_UNPRIVILEGED, 
                              VQEC_CLI_MODE_EXEC, "Show channels");


    /* 
     * show counters
     * Displays all counters
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (2) */
    vqec_cli_register_command(cli, cmd_show_c, "counters", 
                              vqec_cmd_show_counters,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC, 
                              "Show all counters");

    /* 
     * show debug
     * Displays all debug flags
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_show_c, "debug", vqec_cmd_show_debug,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "Show all debug flags");

    /* 
     * show dp   
     * sub-commands for dataplane state.
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (2) */
    vqec_cli_register_command(cli, cmd_show_c, "dp", vqec_cmd_show_dp,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC, 
                              "Dataplane commands");

    /* 
     * show update
     * Displays updater status
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_show_c, "update", 
                              vqec_cmd_show_update,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "Show updater status");

    /* 
     * show drop
     * Displays drop-simulation status and current drop-interval
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_show_c, "drop", vqec_cmd_show_drop,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "Show drop-simulation status and "
                              "drop-interval");

    /* 
     * show error-repair
     * Displays current error-repair status
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_show_c, "error-repair",
                              vqec_cmd_show_error_repair,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "Show error-repair status");

    /*
     * show fec 
     * Display current fec status
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_show_c, "fec",
                              vqec_cmd_show_fec,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "Show fec status");

   /*
     * show rcc 
     * Display current rcc status
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (5) */
#if HAVE_FCC
    vqec_cli_register_command(cli, cmd_show_c, "rcc",
                              vqec_cmd_show_rcc,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "Show rcc status");
    /*
     * show fast-fill
     * show fast fill status
     */
    vqec_cli_register_command(cli, cmd_show_c, "fast-fill",
                              vqec_cmd_show_fast_fill,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "Show fast-fill status");
#endif

    /* 
     * show nat
     * Displays nat information.
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (2) */
    vqec_cli_register_command(cli, cmd_show_c, "nat", vqec_cmd_show_nat,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC, 
                              "Show NAT status");

    /* 
     * show pak-pool
     * Displays current pak-pool status
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_show_c, "pak-pool",
                              vqec_cmd_show_pak_pool,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "Show pak-pool status");

    /* 
     * show stream-output
     * Displays stream-output  information corresponding to all tuners.
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_show_c, "stream-output", 
                              vqec_cmd_show_stream_output,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "Show stream-output");

    /*
     * show system-config
     * Displays information about the system configuration
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_show_c, "system-config",
                              vqec_cmd_show_system_config,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "Show version, build, and system information");

    /* 
     * show tuner [<tuner-name>]
     * Displays tuner information.
     *   If no <tuner-name> is specified names of all tuners are shown.
     *   If a <tuner-name> is specified then tuner diagnostics information is displayed.
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (2) */
    vqec_cli_register_command(cli, cmd_show_c, "tuner", vqec_cmd_show_tuner,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC, 
                              "Show tuners");

    /* 
     * show ipc
     * Displays global IPC related information.
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (2) */
    vqec_cli_register_command(cli, cmd_show_c, "ipc", vqec_cmd_show_ipc,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC, 
                              "Show IPC status");

    /*
     * show proxy-igmp <tuner-name>
     * Displays the IGMP proxy information for the specified tuner
     */
    vqec_cli_register_command(cli, cmd_show_c, "proxy-igmp",
                              vqec_cmd_show_proxy_igmp,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC, 
                              "Show IGMP Proxy ");
#if HAVE_CISCO_RTSP_LIBRARY
    /*
     * show vod session related information
     */
    vqec_cli_register_command(cli, cmd_show_c, "vod-session",
                              vqec_cmd_show_vod_session,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC, 
                              "Show VoD Session ");
#endif  /* HAVE_CISCO_RTSP_LIBRARY */

    /*
     * show tech-support
     * Displays an aggregate output of several CLI commands. 
     * Envisioned to be useful for on-field debugging via a single command
     */
    vqec_cli_register_command(cli, cmd_show_c, "tech-support",
                              vqec_cmd_show_tech_support,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_EXEC, 
                              "Show tech-support");

#ifdef HAVE_IPLM
    iplm_register_show_cmds(cli, cmd_show_c);
#endif

}

/*
 * Register VQE-C global configuration commands
 */

/*
 * Registers the drop simulator commands under a parse node.
 *
 * @param[in] cli        - CLI context
 * @param[in] cmd_node   - CLI node under which commands should be registered
 * @param[in] deprecated - TRUE if deprecated version of commands are being 
 *                         registered
 */
void
vqec_register_drop_commands(struct vqec_cli_def *cli,
                            struct vqec_cli_command *cmd_node,
                            boolean deprecated)
{
    char help_string[VQEC_MAXHELPLEN];

    /* 
     * interval <int1> <int2>
     * drop <int1> packets for every <int2> packets received
     */
    snprintf(help_string, VQEC_MAXHELPLEN,
             "Set the drop interval to periodically drop consecutive"
             "\r\n                       packets%s",
             (deprecated ? 
              " for primary sessions"
              "\r\n                       (deprecated, use \"drop session "
              "primary interval\" "
              "\r\n                        command instead)" : ""));
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (6) */
    vqec_cli_register_command(cli, cmd_node, "interval",
                              vqec_cmd_drop_interval,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              help_string);

    /* 
     * drop percentage <int>
     * randomly drop <int>% of packets received
     */
    snprintf(help_string, VQEC_MAXHELPLEN,
             "Set the random drop percentage%s",
             (deprecated ? 
              " for primary sessions"
              "\r\n                       (deprecated, use \"drop session "
              "primary percentage\""
              "\r\n                        command instead)" : "")); 
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_node, "percentage", 
                              vqec_cmd_drop_percentage, 
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              help_string);
    
    /*
     * drop enable
     * enable drop simulation
     */
    snprintf(help_string, VQEC_MAXHELPLEN,
             "Enable drop simulation%s",
             (deprecated ? 
              " for primary sessions"
              "\r\n                       (deprecated, use \"drop session "
              "primary enable\""
              "\r\n                        command instead)" : ""));
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_node, "enable", vqec_cmd_drop_enable,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              help_string);
    
    /*
     * drop disable
     * disable drop simulation
     */
    snprintf(help_string, VQEC_MAXHELPLEN,
             "Disable drop simulation%s",
             (deprecated ? 
              " for primary sessions"
              "\r\n                       (deprecated, use \"drop session "
              "primary disable\""
              "\r\n                        command instead)" : ""));
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_node, "disable", vqec_cmd_drop_disable,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              help_string);
}


void vqec_register_global_cmds (struct vqec_cli_def *cli)
{
    struct vqec_cli_command *cmd_set_d;
    struct vqec_cli_command *cmd_set_ds;
    struct vqec_cli_command *cmd_set_dsp;
    struct vqec_cli_command *cmd_set_dsr;
    struct vqec_cli_command *cmd_set_e;
    struct vqec_cli_command *cmd_set_ep;

#if HAVE_FCC
    /* 
     * app-delay
     * increment timestamp of app copies by <delay> ms for each <group_size>
     */
    (void)vqec_cli_register_command(cli, NULL, "app-delay", 
                                    vqec_cmd_app_delay, 
                                    PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                                    "Configure app-delay settings");
#endif

    /* 
     * drop
     * drop cmd link point
     */
    cmd_set_d = vqec_cli_register_command(cli, NULL, "drop", NULL, 
                                          PRIVILEGE_PRIVILEGED, 
                                          VQEC_CLI_MODE_CONFIG,
                                          "Configure drop simulation");
    VQEC_ASSERT(cmd_set_d);
    
    /*
     * Register (deprecated) drop commands which operate on the primary 
     * session right under the "drop" parse node.
     */
    vqec_register_drop_commands(cli, cmd_set_d, TRUE);

    /*
     * drop session
     * Configure drop simulation for primary or repair sessions
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    cmd_set_ds = 
        vqec_cli_register_command(cli, cmd_set_d, "session", NULL,
                                  PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                                  "Configure drop simulation parameters "
                                  "for a session type");

    /*
     * drop session primary ...
     * Configure drop simulation for primary sessions
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    cmd_set_dsp = 
        vqec_cli_register_command(cli, cmd_set_ds, "primary", NULL,
                                  PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                                  "Configure drop simulation parameters for "
                                  "primary sessions");
    vqec_register_drop_commands(cli, cmd_set_dsp, FALSE);

    /*
     * drop session repair ...
     * Configure drop simulation for repair sessions
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    cmd_set_dsr = 
        vqec_cli_register_command(cli, cmd_set_ds, "repair", NULL,
                                  PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                                  "Configure drop simulation parameters for "
                                  "repair sessions");
    vqec_register_drop_commands(cli, cmd_set_dsr, FALSE);

    /* 
     * error-repair
     * error-repair cmd link point
     */
    cmd_set_e = vqec_cli_register_command(cli, NULL, "error-repair",
                                          NULL,
                                          PRIVILEGE_PRIVILEGED,
                                          VQEC_CLI_MODE_CONFIG, 
                                          "Configure error repair");
    
    /* 
     * error-repair enable
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_e, "enable",
                              vqec_cmd_error_repair_enable,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG, "Enable error repair");

    /* 
     * error-repair disable
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_e, "disable",
                              vqec_cmd_error_repair_disable,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG, "Disable error repair");

    /* 
     * error-repair policer
     * error-repair policer cmd link point
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    cmd_set_ep = vqec_cli_register_command(cli, cmd_set_e, "policer", NULL,
                                           PRIVILEGE_PRIVILEGED, 
                                           VQEC_CLI_MODE_CONFIG,
                                           "Configure the error repair" 
                                           " policer");

    /* 
     * error-repair policer enable
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_ep, "enable",
                              vqec_cmd_error_repair_policer_enable,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              "Enable error repair request policing");

    /* 
     * error-repair policer disable
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_ep, "disable",
                              vqec_cmd_error_repair_policer_disable,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              "Disable error repair request policing");

    /* 
     * error-repair policer rate
     * Configures the token bucket policer rate for error repair requests
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_ep, "rate",
                              vqec_cmd_error_repair_policer_rate,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              "Set the error repair policing rate");

    /* 
     * error-repair policer burst
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_ep, "burst",
                              vqec_cmd_error_repair_policer_burst,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              "Set the error repair policing burst size");

   /* added for use by FEC */

    /* 
     * FEC
     * FEC cmd link point
     */
    cmd_set_e = vqec_cli_register_command(cli, NULL, "fec",
                                          NULL,
                                          PRIVILEGE_PRIVILEGED,
                                          VQEC_CLI_MODE_CONFIG, 
                                          "Configure FEC");

    /* 
     * fec enable
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_e, "enable",
                              vqec_cmd_fec_enable,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG, "Enable FEC");

    /* 
     * fec disable
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_e, "disable",
                              vqec_cmd_fec_disable,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG, "Disable FEC");

   /* added for use by RCC */

    /* 
     * RCC
     * RCC cmd link point
     */
#if HAVE_FCC
    cmd_set_e = vqec_cli_register_command(cli, NULL, "rcc",
                                          NULL,
                                          PRIVILEGE_PRIVILEGED,
                                          VQEC_CLI_MODE_CONFIG, 
                                          "Configure RCC");

    /* 
     * rcc enable
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_e, "enable",
                              vqec_cmd_rcc_enable,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG, "Enable RCC");

    /* 
     * rcc disable
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_e, "disable",
                              vqec_cmd_rcc_disable,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG, "Disable RCC");


    /*
     * fast fill link point 
     */
    cmd_set_e = vqec_cli_register_command(cli, NULL, "fast-fill",
                                          NULL,
                                          PRIVILEGE_PRIVILEGED,
                                          VQEC_CLI_MODE_CONFIG, 
                                          "Configure FAST-FILL");

    /* 
     * fast fill enable
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_e, "enable",
                              vqec_cmd_fast_fill_enable,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG, "Enable FAST-FILL");

    /* 
     * fast fill disable
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_set_e, "disable",
                              vqec_cmd_fast_fill_disable,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG, "Disable FAST-FILL");

#endif /* HAVE_FCC */

    /*
     * update VQE-C configuration
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, NULL, "update",
                              vqec_cmd_update,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG, "Update VQE-C configuration");

#ifdef HAVE_IPLM
    iplm_register_global_cmds(cli);
#endif
}

#if HAVE_CISCO_RTSP_LIBRARY
/**
 * Register VOD session commands 
 */
void vqec_register_vod_cmds (struct vqec_cli_def *cli)
{
    struct vqec_cli_command *cmd_vod = 0;

    /* 
     * link point
     */
    cmd_vod = vqec_cli_register_command(cli, NULL, "vod-session", NULL, 
                                        PRIVILEGE_PRIVILEGED, 
                                        VQEC_CLI_MODE_CONFIG,
                                        "vod session commands");
    VQEC_ASSERT(cmd_vod);

    vqec_cli_register_command(cli, cmd_vod, "create",
                              vqec_cmd_vod_session_create, PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG,
                              "vod-session create <session-name> :"
                              " create a vod session with name <session-name>");

    vqec_cli_register_command(cli, cmd_vod, "destroy",
                              vqec_cmd_vod_session_destroy, 
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG,
                              "vod-session destroy <session-name> :"
                              " destroy a vod session with name <session-name>");

    vqec_cli_register_command(cli, cmd_vod, "play",
                              vqec_cmd_vod_session_play, PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG,
                              "vod-session play <session-name> :"
                              " play a vod session with name <session-name>");

    vqec_cli_register_command(cli, cmd_vod, "pause",
                              vqec_cmd_vod_session_pause, PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG,
                              "vod-session pause <session-name> :"
                              " pause a vod session with name <session-name>");
}
#endif  /* HAVE_CISCO_RTSP_LIBRARY */

/*
 * Register VQE-C 'test' commands
 */
void vqec_register_test_cmds (struct vqec_cli_def *cli)
{
    struct vqec_cli_command *cmd_test_c=0;
    struct vqec_cli_command *cmd_test_ch=0;

    /* 
     * test tuner link point
     */
    cmd_test_c = vqec_cli_register_command(cli, NULL, "tuner", NULL, 
                                           PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                                           "test tuner commands");
    VQEC_ASSERT(cmd_test_c);

    /* 
     * bind <tuner-name> <src-url>
     * Bind the channel identified by <src-url> to the tuner <tuner-name>
     *   <tuner-name> - name of the tuner
     *   <src-url> - the url is of the form:
     *   protocol://address:port
     *     protocol values: "rtp", "udp", "rtsp"
     *   address - a dotted demical ip-address - only IPv4 is supported in this
     *   release
     *   port - is the IP port number range 0-64,535
     * tuner bind does an implicit unbind if the called on a tuner that is 
     * already bound.
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_test_c, "bind", vqec_cmd_tuner_bind,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              "bind a tuner to the given url");

    /* 
     * create <tuner-name> 
     * Creates a tuner with the name tuner-name.  <tuner-name> must be unique.
     * No support for dynamically creating and deleting tuners.
     * Note tuner creation parameters are cached at intitialization time and
     * can not be unique per-tuner.
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (5) */
    vqec_cli_register_command(cli, cmd_test_c, "create",
                              vqec_cmd_tuner_create, PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_CONFIG,
                              "tuner-create <tuner-name> :"
                              " create a tuner with name <tuner-name>");

    /* 
     * destroy <tuner-name> 
     * Destroy a tuner with the name <tuner-name>.  <tuner-name> must be unique.
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_test_c, "destroy", 
                              vqec_cmd_tuner_destroy,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              "tuner-destroy <tuner-name> : "
                              "Destroy a tuner with name <tuner-name>");

    /* 
     * unbind  <tuner-name> 
     * Close the channel <tuner-name>
     *   <tuner-name> - name of the tuner 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_test_c, "unbind", 
                              vqec_cmd_tuner_unbind,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              "unbind a tuner from a channel");

    /* 
     * test channel link point
     */
    cmd_test_ch = vqec_cli_register_command(cli, NULL, "channel", NULL, 
                                            PRIVILEGE_PRIVILEGED, 
                                            VQEC_CLI_MODE_CONFIG,
                                            "test channel commands");

    /* 
     * tr-135  <channel-url> gmin <gmin> slmd <slmd>
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_test_ch, "tr-135", 
                              vqec_cmd_channel_tr135,
                              PRIVILEGE_UNPRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              "set tr135 parameters for specified channel");

    /* 
     * channel update primary-dest-addr <addr> primary-dest-port <port>
     *   primary-src-addr <addr> primary-src-port <port>
     *   fbt-addr <addr> primary-src-rtcp-port <port>
     *   rtx-src-addr <addr> rtx-src-port <port> rtx-src-rtcp-port <port>
     *
     * This command allows an active channel's source configuration
     * to change, where (primary-dest-addr, primary-dest-port) identify 
     * the channel, and the remaining fields identify the new primary 
     * and retransmission source.
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (5) */
    vqec_cli_register_command(cli, cmd_test_ch, "update",
                              vqec_cmd_channel_update,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              "update config for an active (currently-tuned) "
                              "channel");
}

/*
 * Register VQE-C 'clear' commands
 */
void vqec_register_clear_cmds (struct vqec_cli_def *cli)
{
    struct vqec_cli_command *cmd_clear_c;

    /* 
     * clear 
     * clear commands link point
     */
    cmd_clear_c = vqec_cli_register_command(cli, NULL, "clear", NULL, 
                                            PRIVILEGE_PRIVILEGED, 
                                            VQEC_CLI_MODE_EXEC, "clear cmds");

    VQEC_ASSERT(cmd_clear_c);

    /*
     * clear counters
     * clear all counters for all active tuners
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_clear_c, "counters",
                              vqec_cmd_clear_counters, PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_EXEC, "clear all counters");

#ifdef HAVE_IPLM
   /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_clear_c, "iplm-counters",
                              vqec_cmd_clear_counters, PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_EXEC, "clear all iplm counters");
#endif
}

/*
 * Register VQE-C 'debug' commands
 */

struct vqec_cli_command *
vqec_cli_register_debug_values (struct vqec_cli_def *cli, 
                                struct vqec_cli_command *parent,
                                char *command, 
             int (*callback)(struct vqec_cli_def *cli, char *, char **, int),
                                int privilege, int mode, char *help)
{
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (2) */
    vqec_cli_register_command(cli, parent,  "enable",
                              callback, privilege, mode, "enable debug");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (2) */
    vqec_cli_register_command(cli, parent,  "disable",
                              callback, privilege, mode, "disable debug");
    return parent;
}

struct vqec_cli_command *
vqec_cli_register_debug_cmd(struct vqec_cli_def *cli, 
                            struct vqec_cli_command *parent,
                            char *command, 
             int (*callback)(struct vqec_cli_def *cli, char *, char **, int),
                            int privilege, int mode, char *help)
{
    struct vqec_cli_command *cmd_debug_c2;
    cmd_debug_c2 = vqec_cli_register_command(cli, parent,  command, NULL, 
                                             privilege, mode, help);
    VQEC_ASSERT(cmd_debug_c2);
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (1) */
    vqec_cli_register_debug_values(cli, cmd_debug_c2,  command, callback, 
                                   privilege, mode, help);
    return parent;
}


/*
 * vqec_register_debug_cmds
 * Top-level debug registration function
 */
void vqec_register_debug_cmds (struct vqec_cli_def *cli)
{
    struct vqec_cli_command *cmd_debug_c, *cmd_debug_goto_cli;

    cmd_debug_goto_cli =
        vqec_cli_register_command(cli, NULL, "send-debugs-to-cli", NULL,
                                  PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
       "Set whether or not debug messages are to be printed on the CLI output");
    /* 
     * send-debugs-to-cli enable
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_debug_goto_cli, "enable",
                              vqec_cmd_send_debugs_to_cli_enable,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_EXEC, "Enable debug messages on the CLI");
    /* 
     * send-debugs-to-cli disable
     * 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_debug_goto_cli, "disable",
                              vqec_cmd_send_debugs_to_cli_disable,
                              PRIVILEGE_PRIVILEGED,
                              VQEC_CLI_MODE_EXEC, 
                              "Disable debug messages on the CLI");

    cmd_debug_c = vqec_cli_register_command(cli, NULL, "debug", NULL,
                                            PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                                            "Set debugging flags");
    VQEC_ASSERT(cmd_debug_c);

    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "all",         
                                vqec_debug_func, PRIVILEGE_PRIVILEGED,
                                VQEC_CLI_MODE_EXEC, "Debug all");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "channel",  
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug channel");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "cpchan",  
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug cpchan");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "error-repair",
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug error repair");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "event",       
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug event");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "rcc",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug rcc");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "igmp",   
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug igmp");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "input",   
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug input");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "output",  
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug output");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "pcm",  
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug pcm");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "recv-socket", 
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug recv_socket");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "rtcp",        
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug rtcp");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "timer",       
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug timer");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "tuner",       
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug tuner");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "nat",       
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug nat");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "chan_cfg",       
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug chan_cfg");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "stream_output",       
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug stream_output");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "upcall",       
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug upcall");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "updater",       
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug updater");

    /* DP flags from here down */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-error-repair",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-error-repair");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-nll",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-nll");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-nll-adjust",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-nll adjustments");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-pcm",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-pcm");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-pcm-pak",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-pcm-pak");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-inputshim",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-inputshim");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-outputshim",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-outputshim");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-outputshim-pak",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-outputshim-pak");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-tlm",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-tlm");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-rcc",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-rcc");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-fec",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-fec");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-collect-stats",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-collect-stats");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "dp-failover",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug dp-failover");
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_debug_cmd(cli, cmd_debug_c,  "vod",     
                                vqec_debug_func, PRIVILEGE_PRIVILEGED, 
                                VQEC_CLI_MODE_EXEC, "Debug vod");

}

/*
 * Register VQE-C 'hag' commands
 */
void vqec_register_hag_cmds (struct vqec_cli_def *cli)
{

#ifdef HAVE_HAG_MODE

    /*
     * proxy-igmp-join <stb-addr> <addr-mask>
     *    Use IGMP joins from <stb-addr> to select the desired input stream,
     *      and re-source the stream back out to address translated by 
     *      <addr-mask>.
     *    <stb-addr> = 0.0.0.0 disables use of proxy-igmp-join
     *    default join's disabled. proxy-igmp-join 0.0.0.0 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, NULL, "proxy-igmp-join", 
                              vqec_cmd_proxy_igmp_join, 
                              PRIVILEGE_PRIVILEGED, 
                              VQEC_CLI_MODE_CONFIG,
                              "Use IGMP joins to select"
                              " the desired input stream.");

#endif /* HAVE_HAG_MODE */

    /*
     * stream-output <tuner-name> <if-name> <output-url>
     *   ex. stream-output 0 eth1 udp://192.168.1.128:50000
     *    <tuner-name> name of the tuner
     *    <if-name> interface name to stream on
     *    <output-url> has the form
     *    output-type://<address>:port
     *    allowed output-type values = "udp", 
     *       udp://<address:port> - send repair stream out as a udp 
     *       stream to the designated address and port.
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, NULL, "stream-output", 
                              vqec_cmd_stream_output, 
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              "Set the tuner output type.");
    /*
     * test-dp-reader <tuner-name> <src-address> <dest-address> <dest-port>
     * ex. test-dp-reader t 5.8.48.2 192.168.1.128 50000
     *    <tuner-name> name of the tuner
     *    <src-address> source address for streamed out packets
     *    <dest-address> destination address for streamed out packets
     *    <dest-port> destination port for streamed out packets
     *
     *    if <dest-address> and <dest-port> are both 0, the reader task
     *    will be stopped
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, NULL, "test-dp-reader", 
                              vqec_cmd_test_dp_reader,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_CONFIG,
                              "Launch the kernel DP test reader, "
                              "if available.");
}

/*
 * Register VQE-C 'monitor' commands
 */
void vqec_register_monitor_cmds (struct vqec_cli_def *cli)
{
    struct vqec_cli_command *cmd_monitor_c;
#ifdef HAVE_ELOG
    struct vqec_cli_command *cmd_monitor_elog_c;
#endif
    struct vqec_cli_command *cmd_monitor_outputsched_c;
    struct vqec_cli_command *cmd_monitor_benchmark_c;

    /*
     * monitor
     * monitor cmd link point
     */
    cmd_monitor_c = vqec_cli_register_command(cli, NULL, "monitor", NULL,
                                              PRIVILEGE_PRIVILEGED, 
                                              VQEC_CLI_MODE_EXEC,
                                              "Performance monitoring tools");
    VQEC_ASSERT(cmd_monitor_c);

#ifdef HAVE_ELOG
    /*
     * monitor elog
     * ELOG performance monitoring tool
     */
    cmd_monitor_elog_c = vqec_cli_register_command(cli, cmd_monitor_c, 
                                                   "elog", NULL,
                                                   PRIVILEGE_PRIVILEGED, 
                                                   VQEC_CLI_MODE_EXEC,
                                                   "ELOG performance "
                                                   "monitoring tool");
    VQEC_ASSERT(cmd_monitor_elog_c);

    /*
     * monitor elog dump <filename>
     * Write collected data to file <filename>
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
     vqec_cli_register_command(cli, cmd_monitor_elog_c, "dump", 
                               vqec_cmd_monitor_elog,
                               PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                               "write collected data to file <filename>");
    /*
     * monitor elog off
     * Disable ELOG
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_monitor_elog_c, 
                              "off", vqec_cmd_monitor_elog,
                              PRIVILEGE_PRIVILEGED, 
                              VQEC_CLI_MODE_EXEC, "disable ELOG");
    /*
     * monitor elog on
     * Enable ELOG
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_monitor_elog_c, "on", 
                              vqec_cmd_monitor_elog,
                              PRIVILEGE_PRIVILEGED, 
                              VQEC_CLI_MODE_EXEC, "enable ELOG");

    /*
     * monitor elog reset
     * Reset current ELOG data
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
     vqec_cli_register_command(cli, cmd_monitor_elog_c, "reset", 
                               vqec_cmd_monitor_elog,
                               PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                               "reset current ELOG data");
#endif /* HAVE_ELOG */

     /*
      * monitor output
      * Commands to monitor output scheduling
      */
     cmd_monitor_outputsched_c = 
         vqec_cli_register_command(cli, cmd_monitor_c, "output-sched", NULL,
                                   PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                                   "output scheduling monitoring commands");
     
     VQEC_ASSERT(cmd_monitor_outputsched_c);

    /*
     * monitor output-sched show
     * Prints a histogram of output scheduling intervals
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_monitor_outputsched_c, "show",
                              vqec_cmd_monitor_outputsched_show,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "print a histogram of output scheduling "
                              "intervals");

    /*
     * monitor output-sched off
     * Disable monitoring of output scheduling 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_monitor_outputsched_c, "off",
                              vqec_cmd_monitor_outputsched_off,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "disable output scheduling monitoring");

    /*
     * monitor output-sched on
     * Enable monitoring of output scheduling
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_monitor_outputsched_c, "on",
                              vqec_cmd_monitor_outputsched_on,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "enable output scheduling monitoring");

    /*
     * monitor output-sched reset
     * Reset current output scheduling data
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
     vqec_cli_register_command(cli, cmd_monitor_outputsched_c, "reset",
                               vqec_cmd_monitor_outputsched_reset,
                               PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                               "reset current output scheduling data");

     /*
      * monitor benchmark
      * Commands to monitor benchmark
      */
     cmd_monitor_benchmark_c = 
         vqec_cli_register_command(cli, cmd_monitor_c, "benchmark", NULL,
                                   PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                                   "benchmark monitoring commands");
     
     VQEC_ASSERT(cmd_monitor_benchmark_c);

    /*
     * monitor benchmark show
     * Prints all avaiable VQE-C benchmarks (i.e. cpu usage)
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_monitor_benchmark_c, "show",
                              vqec_cmd_monitor_benchmark_show,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "print a list of benchmarks");

    /*
     * monitor benchmark off
     * Disable benchmark sampling 
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_monitor_benchmark_c, "off",
                              vqec_cmd_monitor_benchmark_off,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "disable benchmark sampling");

    /*
     * monitor benchmark on
     * Enable benchmark sampling
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (4) */
    vqec_cli_register_command(cli, cmd_monitor_benchmark_c, "on",
                              vqec_cmd_monitor_benchmark_on,
                              PRIVILEGE_PRIVILEGED, VQEC_CLI_MODE_EXEC,
                              "enable benchmark sampling");
}

/*
 * Register VQE-C 'parse' commands
 */
void vqec_register_parse_cmds (struct vqec_cli_def *cli)
{
    struct vqec_cli_command *cmd_test_c=0;

    /* 
     * test parse link point
     */
    cmd_test_c = vqec_cli_register_command(cli, NULL, "parse", NULL, 
                                           PRIVILEGE_PRIVILEGED, 
                                           VQEC_CLI_MODE_CONFIG,
                                           "test parse commands");

    VQEC_ASSERT(cmd_test_c);

    /* 
     * parse sdp {linear | vod} <sdp_filename> [output <output_filename>]
     * Parse the SDP in <sdp_filename> and validate as a linear or vod channel
     *   <sdp_filename> - name of a file containing SDP
     *   <output_filename> - name of output file
     */
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (3) */
    vqec_cli_register_command(cli, cmd_test_c, "sdp", vqec_cmd_parse_sdp,
                              PRIVILEGE_UNPRIVILEGED, 
                              VQEC_CLI_MODE_CONFIG,
                              "parse an linear or VoD sdp file");

}
/*
 *
 * END COMMAND REGISTRATION
 *
 */


/*
 *
 * BEGIN VQE-C CLI CONTROL FUNCTIONS HERE
 *
 */
#define VQEC_CLI_CLIENT_MAX 1
static uint32_t g_cli_client_count = 0;

static void *vqec_cli_loop_thread (void *arg)
{
    int32_t sock = (int32_t)arg;
    struct vqec_cli_def *cli = NULL;

    if (sock < 0) {
        return NULL;
    }
    
    cli = vqec_cli_startup();

    if (!cli) {
        close(sock);
        return NULL;
    }
  
    /* Temporary fix when VQEC_CLI_CLIENT_MAX = 1 */
    g_cli_handle = (void *) cli;

    (void) signal(SIGPIPE, SIG_IGN);

    /*sa_ignore {no recourse on failure} IGNORE_RETURN (1) */
    vqec_cli_loop(cli, sock);
    
    close(sock);
    /*sa_ignore {no recourse on failure} IGNORE_RETURN (1) */
    vqec_lock_lock(vqec_g_cli_client_lock);
    g_cli_client_count--;
    if (!g_cli_client_count) {
        vqec_cli_deinit(cli);
        g_cli_handle = NULL;
        vqec_set_cli_def(g_cli_handle);
    }
    vqec_lock_unlock(vqec_g_cli_client_lock);
    return NULL;
}

/*!
  Creates a telnet socket listening to the <cli_port> and then
  loops waiting to receive cli commands on that port.
  Only VQEC_CLI_CLIENT_MAX clients are accepted.
  @param[in] cli_port - specifies telnet port for the cli.
                        note specifying a cli_port of zero 
                        does not start a telnet server.
  @param[in] cli_if_addr - specifies the interface address to
                           bind the telnet server to.
                           note specifying a cli_if_addr of zero 
                           binds the server to listen on all
                           interfaces.
  @return 0 if the loop exits normally, 
   1 if the loop exited because the telnet server failed to start.
         
  CLI telnet session.  See vqec_cli_loop.
 */

int vqec_cli_loop_wrapper (uint16_t cli_port, in_addr_t cli_if_addr)
{
    struct sockaddr_in servaddr;
    int32_t s_vqec_cli_tcpsock = -1;
    int32_t temp_sock;
    pthread_t temp_tid;

    if (cli_port != 0) {

        printf("VQE-C CLI Attempting to Listen %d\n", cli_port);

        if ((s_vqec_cli_tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("CLI socket creation failure (%s) - deactivating CLI\n",
                   strerror(errno));
            return 1;
        }
       
        g_vqec_cli_tcpsock = s_vqec_cli_tcpsock;
 
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(cli_port);
        servaddr.sin_addr.s_addr = cli_if_addr;
        
        while (1) {
            if (bind(s_vqec_cli_tcpsock, 
                     (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
                if (errno == EADDRINUSE) {
                    printf("CLI bind failure (%s) - will retry in 10 secs\n",
                           strerror(errno));
                    /* sa_ignore {} IGNORE_RETURN(1) */
                    sleep(10);
                    continue;   /* retry bind */
                } else {
                    printf("bind failure (%s) - deactivating CLI\n",
                           strerror(errno));
                        return (1);
                }
            }
            
            break;              /* exit from while */
        }

        if (listen(s_vqec_cli_tcpsock, 50) < 0) {
            printf("listen error (%s) - deactivating CLI\n",
                   strerror(errno));
            return (1);       
        }

        printf("VQE-C CLI Listening on port %d\n", cli_port);
        while (1) {
            if (s_vqec_cli_tcpsock < 0) {
                printf("CLI socket has been closed\n");
                return (1);
            }
            
            temp_sock = accept(s_vqec_cli_tcpsock, NULL, 0);
            
            if (temp_sock < 0) {
                switch (errno) {
                case EINTR:
                case ECONNABORTED:
                    printf("accept error (%s) ", strerror(errno));
                    printf(" - will retry connecting -\n");
                    continue;
                default:
                    printf("VQE-C CLI ... disabling \n");
                    return (1);
                }
            }
            
            vqec_lock_lock(vqec_g_cli_client_lock);
            if (g_cli_client_count < VQEC_CLI_CLIENT_MAX) {
                if (vqec_pthread_create(&temp_tid, 
                                        vqec_cli_loop_thread, 
                                        (void *)temp_sock)) {
                    printf("Unable to create cli thread \n");
                } else if (pthread_detach(temp_tid)) {
                    printf("Unable to detach cli thread \n");
                }
                g_cli_client_count++;
            } else {
                close(temp_sock);
            }
            vqec_lock_unlock(vqec_g_cli_client_lock);
        }
    }

    if (s_vqec_cli_tcpsock != -1) {
        close(s_vqec_cli_tcpsock);
    }

    return 0;
}

/*
 *  vqec_get_cli_fd
 *  return the FD of the telnet session for the cli
 */
int vqec_get_cli_fd (void *cli_handle)
{
    int cli_fd = 0;
    struct vqec_cli_def *cli = (struct vqec_cli_def *)cli_handle;

    if (!s_vqec_debugs_goto_cli_enabled) {
        return (-1);
    }

    if (cli && cli->client) {
        cli_fd = fileno(cli->client);
        if (cli_fd < 0) {
            return fileno(stderr);
        }
        return cli_fd;
    }
    return fileno(stderr);
}


/*!
  Initialize VQEC's CLI.
  @return A cli_handle, for use in starting an optional 
  CLI telnet session.  See vqec_cli_loop.
 */
void * vqec_cli_startup(void)
{
    struct vqec_cli_def *cli;

    cli = vqec_cli_init();
    vqec_set_cli_def(cli);
    if (!cli) {
        printf("vqec_cli_init() failed!\n");
        return (void *)NULL;
    }
    vqec_cli_set_banner(cli, "Welcome to the VQE-C CLI!");
    vqec_cli_set_hostname(cli, "vqec");
    vqec_cli_print_callback(cli, vqec_print_callback);

    vqec_register_clear_cmds(cli);

    vqec_register_debug_cmds(cli);

    vqec_register_monitor_cmds(cli);

    vqec_register_show_cmds(cli);

    vqec_register_global_cmds(cli);

    vqec_register_hag_cmds(cli);

    vqec_register_parse_cmds(cli);

    vqec_register_test_cmds(cli);

#if HAVE_CISCO_RTSP_LIBRARY
    vqec_register_vod_cmds(cli);
#endif  /* HAVE_CISCO_RTSP_LIBRARY */

    return cli;
}

void vqec_destroy_cli(void)
{
    int cli_fd;
    int ret_val;
    if (g_vqec_cli_tcpsock != -1) {
        ret_val=shutdown(g_vqec_cli_tcpsock, SHUT_RDWR); 
        if (ret_val < 0) { 
            printf("Socket shutdown failure");
        }
        close(g_vqec_cli_tcpsock);
        g_vqec_cli_tcpsock=-1;
    }
    if (g_cli_handle != NULL) {
        cli_fd = vqec_get_cli_fd(g_cli_handle);
        close(cli_fd);
    }
    g_cli_client_count = 0;
}
