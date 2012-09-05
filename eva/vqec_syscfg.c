/*
 * vqec_syscfg.c - Implements VQE-C system configuration, i.e. the ability 
 * to process a libconf formatted vqec system configuration file at
 * startup time, and provide access to the system config information to
 * other components.
 *
 * Copyright (c) 2007-2012 by Cisco Systems, Inc.
 * All rights reserved.
 */
#define _GNU_SOURCE  /* defining this in order to get strnlen */
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "vqec_assert_macros.h"
#include "vqec_syscfg.h"
#include "vqec_config_parser.h"
#define __DECLARE_CONFIG_ARR__  /* declare the vqec_cfg_arr */
#include "vqec_cfg_settings.h"
#define __DECLARE_CONFIG_NUMS__ /* declare the vqec_cfg enums */
#include "vqec_cfg_settings.h"
#define __DECLARE_CONFIG_ATTRIBUTES__ /* declare the vqec_cfg attributes */
#include "vqec_cfg_settings.h"
#define __DECLARE_CONFIG_OVERRIDES__ /* declare the vqec_cfg overrides */
#include "vqec_cfg_settings.h"
#define __DECLARE_CONFIG_UPDATE_TYPE__ /* declare the vqec_cfg update type */
#include "vqec_cfg_settings.h"
#define __DECLARE_CONFIG_STATUS__ /* declare the vqec_cfg status array */
#include "vqec_cfg_settings.h"
#define __DECLARE_CONFIG_IS_STATUS_CURRENT__ /*
                                              * declare the vqec_cfg
                                              * is_status_current array
                                              */
#include "vqec_cfg_settings.h"
#include "utils/vam_util.h"
#include "cfgapi.h"
#include <arpa/inet.h>
#include "vqec_debug.h"
#include "vqec_syslog_def.h"
#include <log/vqe_cfg_syslog_def.h>
#include <log/vqe_cfg_debug.h>
#include <log/vqe_rtp_syslog_def.h>
#include <log/vqe_utils_syslog_def.h>
#include <utils/strl.h>
#include <utils/vmd5.h>
#include "vqec_ifclient.h"
#include "vqec_stream_output_thread_mgr.h"
#include "vqec_gap_reporter.h"
#include "vqec_ifclient_private.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif


/*
 * Names of VQE-C configuration files which may be updated.
 *
 * NOTE:  Ordering of this list is based on "vqec_ifclient_config_t". 
 */
const char *vqec_syscfg_index_names[] = { "Network", "Override", "Channel" };

/*
 * Callback functions for VQE-C Configuration File events.
 *
 * NOTE:  Ordering of this list is based on "vqec_ifclient_config_t". 
 */
static vqec_ifclient_config_event_cb_t vqec_syscfg_callback[] = 
            { NULL, NULL, NULL };

static vqec_config_t vqec_cfg;

/* Running configuration for VQE-C */
static vqec_syscfg_t s_cfg;
static int s_cname_registered = 0;
static char s_cname[VQEC_MAX_CNAME_LEN];

/* Name of the system configuration file. */ 
static char s_vqec_syscfg_filename[VQEC_MAX_NAME_LEN];


/*
 * Returns the integrator-supplied filename at which an updatable system
 * configuration file may be stored, or NULL if no such filename is configured.
 */
char *
vqec_syscfg_get_filename (vqec_ifclient_config_t config)
{
    char *filename;

    switch (config) {
    case VQEC_IFCLIENT_CONFIG_NETWORK:
        filename = s_cfg.network_cfg_pathname;
        break;
    case VQEC_IFCLIENT_CONFIG_OVERRIDE:
        filename = s_cfg.override_cfg_pathname;
        break;
    case VQEC_IFCLIENT_CONFIG_CHANNEL:
        filename = s_cfg.channel_lineup;
        break;
    default:
        filename = NULL;
        break;
    }
    return (filename);
}

#if HAVE_FCC
void vqec_syscfg_set_rcc_pakpool_max_pct (uint32_t rcc_scaler)
{
    vqec_syscfg_t *g_cfg_p = &s_cfg;   

    if (rcc_scaler > 100) {
        return;
    }

    g_cfg_p->rcc_pakpool_max_pct = rcc_scaler;
}
#endif

/*
 * Returns the parameter ID corresponding to a given parameter name.
 *
 * @param[in]  param_name       Name of parameter to look up.
 * @param[out] vqec_cfg_enum_t  parameter ID, or
 *                              VQEC_CFG_MUST_BE_LAST if no matching
 *                               parameter is found
 */
vqec_cfg_enum_t
vqec_syscfg_param_lookup (const char *param_name)
{
    vqec_cfg_enum_t i;

    for(i=VQEC_CFG_MAX_TUNERS; i < VQEC_CFG_MUST_BE_LAST; i++) {
        if (!strncmp(param_name, vqec_cfg_arr[i].param_name,
                     strlen(vqec_cfg_arr[i].param_name)+1)) {
            break;
        }
    }

    return (i);
}

static char box_cname[VQEC_MAX_NAME_LEN];   /* RTP cname see RFC-3550 */    

void vqec_set_cname (char *input_ifname) 
{
    char temp[VQEC_MAX_CNAME_LEN];
    int i;
    boolean mac_addr_found;

    if (s_cname_registered) {
        memcpy(box_cname, s_cname, VQEC_MAX_CNAME_LEN);
    } else {
        /**
         * no cname registered, need to use NIC MAC address 
         * as cname. 
         * 
         * First, we will try to find the MAC address of 
         * the input intercace defined at sysconfig. If we
         * can not get the MAC of this input NIC, we will 
         * search all interfaces of this STB.  
         * 
         * If there is no mac address found, we use a cname
         * "no_mac_found-currenttime", where "currenttime" is
         * the current abs time in usec. Using this time, with
         * a small probability that two STBs have the same cname. 
         */

        mac_addr_found = TRUE;
        if (!get_mac_address_by_if(input_ifname, temp, 
                                   VQEC_MAX_CNAME_LEN)) {
            syslog_print(VQEC_ERROR, 
                        "CNAME: no mac addr found using defined input_ifname");
            if (!get_first_mac_address(temp, VQEC_MAX_CNAME_LEN)) {
                syslog_print(VQEC_ERROR, 
                             "CNAME: no mac address found in all interfaces");
                snprintf(temp, VQEC_MAX_CNAME_LEN, "no_mac_found-%llx", 
                         get_sys_time().usec);
                (void)strlcpy(box_cname, temp, VQEC_MAX_CNAME_LEN);
                mac_addr_found = FALSE;
            }
        }
        if (mac_addr_found) {
            /* If the CNAME is now the MAC address, it would contain the ':'
             * character, and this character is not a valid character for a
             * CNAME. Consequently, we will replace this character from the
             * MAC, with a hypen '-' character*/
            for (i = 0 ; i < strlen(temp) + 1; i++) {
                if (temp[i]!=':') { 
                    box_cname[i] = temp[i]; 
                } else {
                    box_cname[i] = '-';
                }
            }
        }
    }
}

/**
 * get the cname pointer
 */
const char * vqec_get_cname(void)
{
    return box_cname;
}


/* Note that if the cname length (inclusive of the terminating NULL character)
 * is larger than the VQEC_MAX_NAME_LEN it will be rejected
 */
boolean
vqec_syscfg_register_cname (char *cname)
{
    int i, maxlen;
    boolean valid_cname = FALSE;

    if (cname) {
        /* Caller wants to override the cname, let's validate it */
        maxlen = 0;
        for (i=0 ; i<VQEC_MAX_CNAME_LEN ; i++) {
            if (cname[i]!='\0' && isalpha(cname[i])==0 && isdigit(cname[i])==0
                && ((strchr("$-_.+!*'(),", cname[i])==NULL))) {
                /* invalid char found */
                goto done;
            }
            maxlen++;
            if (cname[ i ] == '\0') break;
        }
        if (maxlen == 1) {
            goto done;
        }
        if (maxlen == (VQEC_MAX_CNAME_LEN)) {
            if (cname[ VQEC_MAX_CNAME_LEN - 1 ] != '\0') {
                goto done; /* Reject a CNAME whose length is equal to 
                            * VQEC_MAX_CNAME_LEN, but which does not include
                            * the NULL character. This is in violation of 
                            * a CNAME being a NULL terminated string with length
                            * less than or equal to VQEC_MAX_CNAME_LEN
                            */
            }
        }
        /* cname is valid, store it */
        memcpy(s_cname, cname, maxlen);
        s_cname_registered = 1;
        valid_cname = TRUE;
        return(valid_cname);
    } 
done:
    s_cname_registered = 0;
    return (valid_cname);
}


/*
 * Sets the parameter values in the supplied cfg structure to default values.
 * By default, all VQE-C parameters have their default value and no
 * parameters are explicitly set (cfg_present is cleared).
 *
 * @param[in]  cfg         - config structure to load with default values
 * @param[out] cfg_present - [optional] initialized to indicate that 
 *                                      no parameters are explicitly set
 */ 
UT_STATIC void
vqec_syscfg_set_defaults (vqec_syscfg_t *cfg,
                          boolean        cfg_present[VQEC_CFG_MUST_BE_LAST])
{
    vqec_cfg_enum_t cfg_id;

    /* Null out the configuration */
    memset(cfg, 0, sizeof(vqec_syscfg_t));
    if (cfg_present) {
        memset(cfg_present, 0, sizeof(boolean) * VQEC_CFG_MUST_BE_LAST);
    }

    /* 
     * Make enum trick / case statement to assure we have added
     * a dump case for each configuration parameter.
     */
    for (cfg_id=VQEC_CFG_MAX_TUNERS; cfg_id < VQEC_CFG_MUST_BE_LAST; 
	 cfg_id++) {
        switch(cfg_id) {
        case VQEC_CFG_MAX_TUNERS:
            cfg->max_tuners = VQEC_SYSCFG_DEFAULT_MAX_TUNERS;
            break;
        case VQEC_CFG_CHANNEL_LINEUP:
            (void)strlcpy(cfg->channel_lineup,
                    VQEC_SYSCFG_DEFAULT_CHANNEL_LINEUP,
                    VQEC_MAX_NAME_LEN);
            break;
        case VQEC_CFG_NETWORK_CFG_PATHNAME:
            (void)strlcpy(cfg->network_cfg_pathname,
                    VQEC_SYSCFG_DEFAULT_NETWORK_CFG_PATHNAME,
                    VQEC_MAX_NAME_LEN);
            break;
        case VQEC_CFG_OVERRIDE_CFG_PATHNAME:
            (void)strlcpy(cfg->override_cfg_pathname,
                    VQEC_SYSCFG_DEFAULT_OVERRIDE_CFG_PATHNAME,
                    VQEC_MAX_NAME_LEN);
            break;
        case VQEC_CFG_INDEX_CFG_PATHNAME:
            (void)strlcpy(cfg->index_cfg_pathname,
                    VQEC_SYSCFG_DEFAULT_INDEX_CFG_PATHNAME,
                    VQEC_MAX_NAME_LEN);
            break;
        case VQEC_CFG_VQEC_ENABLE:
            /*
             * This parameter has been deprecated.
             * If the user has not supplied a value, the configured or default
             * value for VQEC_CFG_QOE_ENABLE will apply.
             */
            break;
        case VQEC_CFG_JITTER_BUFF_SIZE:
            cfg->jitter_buff_size = VQEC_SYSCFG_DEFAULT_JITTER_BUFF_SIZE;
            break;
        case VQEC_CFG_REPAIR_TRIGGER_POINT_ABS:
            cfg->repair_trigger_point_abs =
                VQEC_SYSCFG_DEFAULT_REPAIR_TRIGGER_POINT_ABS; 
            break;
        case VQEC_CFG_REPAIR_TRIGGER_POINT:
            /*
             * This parameter has been deprecated.
             * If the user has not supplied a value, the configured or default
             * value for VQEC_CFG_REPAIR_TRIGGER_POINT_ABS will apply.
             */
            break;
        case VQEC_CFG_PAKPOOL_SIZE:
            cfg->pakpool_size = VQEC_SYSCFG_DEFAULT_PAKPOOL_SIZE;
            break;
        case VQEC_CFG_SO_RCVBUF:
            cfg->so_rcvbuf = VQEC_SYSCFG_DEFAULT_SO_RCVBUF;
            break;
        case VQEC_CFG_STRIP_RTP:
            cfg->strip_rtp = VQEC_SYSCFG_DEFAULT_STRIP_RTP;
            break;
        case VQEC_CFG_INPUT_IFNAME:
            (void)strlcpy(cfg->input_ifname,
                    VQEC_SYSCFG_DEFAULT_INPUT_IFNAME,VQEC_MAX_NAME_LEN); 
            break;
        case VQEC_CFG_SIG_MODE:
            if (!strcmp(VQEC_SYSCFG_DEFAULT_SIG_MODE, 
                        VQEC_SYSCFG_SIG_MODE_NAT)) {
                cfg->sig_mode = VQEC_SM_NAT;
            } else {
                cfg->sig_mode = VQEC_SM_STD;
            }
            break;
        case VQEC_CFG_NAT_BINDING_REFRESH_INTERVAL:
            cfg->nat_binding_refresh_interval =
                VQEC_SYSCFG_DEFAULT_NAT_BINDING_REFRESH_INTERVAL;
            break;
        case VQEC_CFG_NAT_FILTER_REFRESH_INTERVAL:
            break;
        case VQEC_CFG_MAX_PAKSIZE:
            cfg->max_paksize = VQEC_SYSCFG_DEFAULT_MAX_PAKSIZE;
            break;
        case VQEC_CFG_STUN_SERVER_IP:
            break;
        case VQEC_CFG_STUN_SERVER_PORT:
            break;
        case VQEC_CFG_CDI_ENABLE:
            cfg->cdi_enable = VQEC_SYSCFG_DEFAULT_CDI_ENABLE;
            break;
        case VQEC_CFG_DOMAIN_NAME_OVERRIDE:
            (void)strlcpy(cfg->domain_name_override,
                          VQEC_SYSCFG_DEFAULT_DOMAIN_NAME_OVERRIDE,
                          VQEC_MAX_NAME_LEN);
            break;
        case VQEC_CFG_CLI_TELNET_PORT:
            cfg->cli_telnet_port = 0;
            break;
        case VQEC_CFG_OUTPUT_PAKQ_LIMIT:
            cfg->output_pakq_limit = VQEC_SYSCFG_DEFAULT_OUTPUT_PAKQ_LIMIT;
            break;
        case VQEC_CFG_UPDATE_WINDOW:
            cfg->update_window = VQEC_SYSCFG_DEFAULT_UPDATE_WINDOW;
            break;
        case VQEC_CFG_UPDATE_INTERVAL_MAX:
            cfg->update_interval_max = VQEC_SYSCFG_DEFAULT_UPDATE_INTERVAL_MAX;
            break;
        case VQEC_CFG_APP_PAKS_PER_RCC:
            cfg->app_paks_per_rcc = VQEC_SYSCFG_DEFAULT_APP_PAKS_PER_RCC;
            break;
        case VQEC_CFG_ERROR_REPAIR_ENABLE:
            cfg->error_repair_enable =
                VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_ENABLE;
            break;
        case VQEC_CFG_ERROR_REPAIR_POLICER_ENABLE:
            cfg->error_repair_policer_enable =
                VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_ENABLE;
            break;
        case VQEC_CFG_ERROR_REPAIR_POLICER_RATE:
            cfg->error_repair_policer_rate =
                VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_RATE;
            break;
        case VQEC_CFG_ERROR_REPAIR_POLICER_BURST:
            cfg->error_repair_policer_burst =
                VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_BURST;
            break;
        case VQEC_CFG_LOG_LEVEL:
            cfg->log_level =
                VQEC_SYSCFG_DEFAULT_LOG_LEVEL;
            break;
        /* added for use by FEC */
        case VQEC_CFG_FEC_ENABLE:
            cfg->fec_enable = VQEC_SYSCFG_DEFAULT_FEC_ENABLE;
            break;
        case VQEC_CFG_CLI_IFNAME:
            (void)strlcpy(cfg->cli_ifname,
                    VQEC_SYSCFG_DEFAULT_CLI_IFNAME,VQEC_MAX_NAME_LEN);
            break;
        /* added for use by RCC */
#if HAVE_FCC
        case VQEC_CFG_RCC_ENABLE:
            cfg->rcc_enable = VQEC_SYSCFG_DEFAULT_RCC_ENABLE;
            break;
        case VQEC_CFG_RCC_PAKPOOL_MAX_PCT:
            cfg->rcc_pakpool_max_pct = 
                VQEC_SYSCFG_DEFAULT_RCC_PAKPOOL_MAX_PCT;
            break;
        case VQEC_CFG_RCC_START_TIMEOUT:
            cfg->rcc_start_timeout = 
                VQEC_SYSCFG_DEFAULT_RCC_START_TIMEOUT;
            break;
        case VQEC_CFG_FASTFILL_ENABLE:
            cfg->fastfill_enable = VQEC_SYSCFG_DEFAULT_FASTFILL_ENABLE;
            break;
        case VQEC_CFG_RCC_EXTRA_IGMP_IP:
            cfg->rcc_extra_igmp_ip =
                VQEC_SYSCFG_DEFAULT_RCC_EXTRA_IGMP_IP;
            break;
#endif
        case VQEC_CFG_NUM_BYES:
            cfg->num_byes = VQEC_SYSCFG_DEFAULT_NUM_BYES;
            break;
        case VQEC_CFG_BYE_DELAY:
            cfg->bye_delay = VQEC_SYSCFG_DEFAULT_BYE_DELAY;
            break;
        case VQEC_CFG_REORDER_DELAY_ABS:
            cfg->reorder_delay_abs = VQEC_SYSCFG_DEFAULT_REORDER_DELAY_ABS;
            break;
        case VQEC_CFG_REORDER_DELAY:
            /*
             * This parameter has been deprecated.
             * If the user has not supplied a value, the configured or default
             * value for VQEC_CFG_REORDER_DELAY_ABS will apply.
             */
            break;
        case VQEC_CFG_VCDS_SERVER_IP:
            cfg->vcds_server_ip = VQEC_SYSCFG_DEFAULT_VCDS_SERVER_IP;
            break;
        case VQEC_CFG_VCDS_SERVER_PORT:
            cfg->vcds_server_port = 
                htons(VQEC_SYSCFG_DEFAULT_VCDS_SERVER_PORT);
            break;
        case VQEC_CFG_TUNER_LIST:
            break;
        case VQEC_CFG_RTCP_DSCP_VALUE:
            cfg->rtcp_dscp_value = VQEC_SYSCFG_DEFAULT_RTCP_DSCP_VALUE;
            break;
        case VQEC_CFG_DELIVER_PAKS_TO_USER:
            cfg->deliver_paks_to_user = 
                VQEC_SYSCFG_DEFAULT_DELIVER_PAKS_TO_USER;
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_SD:
            cfg->max_receive_bandwidth_sd =
                VQEC_SYSCFG_DEFAULT_MAX_RECEIVE_BANDWIDTH_SD;
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_SD_RCC:
            cfg->max_receive_bandwidth_sd_rcc =
                VQEC_SYSCFG_DEFAULT_MAX_RECEIVE_BANDWIDTH_SD_RCC;
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_HD:
            cfg->max_receive_bandwidth_hd =
                VQEC_SYSCFG_DEFAULT_MAX_RECEIVE_BANDWIDTH_HD;
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_HD_RCC:
            cfg->max_receive_bandwidth_hd_rcc =
                VQEC_SYSCFG_DEFAULT_MAX_RECEIVE_BANDWIDTH_HD_RCC;
            break;
        case VQEC_CFG_MIN_HD_STREAM_BITRATE:
            cfg->min_hd_stream_bitrate =
                VQEC_SYSCFG_DEFAULT_MIN_HD_STREAM_BITRATE;
            break;
        case VQEC_CFG_MAX_FASTFILL:
            cfg->max_fastfill =
                VQEC_SYSCFG_DEFAULT_MAX_FASTFILL;
            break;
        case VQEC_CFG_APP_DELAY:
            cfg->app_delay = VQEC_SYSCFG_DEFAULT_APP_DELAY;
            break;
        case VQEC_CFG_SRC_IP_FILTER_ENABLE:
            cfg->src_ip_filter_enable = 
                VQEC_SYSCFG_DEFAULT_SRC_IP_FILTER_ENABLE;
            break;
        case VQEC_CFG_QOE_ENABLE:
            cfg->qoe_enable = VQEC_SYSCFG_DEFAULT_QOE_ENABLE;
            break;
        case VQEC_CFG_RCC_MAX_CONCURRENT:
            cfg->rcc_max_concurrent =
                VQEC_SYSCFG_DEFAULT_RCC_MAX_CONCURRENT;
            break;
        case VQEC_CFG_INTEGRATED_RTP_FALLBACK:
            cfg->integrated_rtp_fallback =
                VQEC_SYSCFG_DEFAULT_INTEGRATED_RTP_FALLBACK;
            break;
        case VQEC_CFG_UDP_PASSTHRU_SUPPORT:
            cfg->udp_passthru_support = VQEC_SYSCFG_DEFAULT_UDP_PASSTHRU_SUPPORT;
            break;
       case VQEC_CFG_VOD_CMD_TIMEOUT:
            cfg->vod_cmd_timeout =
                VQEC_SYSCFG_DEFAULT_VOD_CMD_TIMEOUT;
            break;
       case VQEC_CFG_VOD_MAX_SESSIONS:
            cfg->vod_max_sessions =
                VQEC_SYSCFG_DEFAULT_VOD_MAX_SESSIONS;
            break;
       case VQEC_CFG_VOD_MODE:
            if (!strcmp(VQEC_SYSCFG_DEFAULT_VOD_MODE, 
                        VQEC_SYSCFG_VOD_MODE_IPTV)) {
                cfg->vod_mode = VQEC_VOD_MODE_IPTV;
            } else {
                cfg->vod_mode = VQEC_VOD_MODE_CABLE;
            }
            break;
        case VQEC_CFG_STUN_OPTIMIZATION:
            cfg->stun_optimization = VQEC_SYSCFG_DEFAULT_STUN_OPTIMIZATION;
            break;

        case VQEC_CFG_MUST_BE_LAST:
            break;
        }
    }
}

UT_STATIC void vqec_syscfg_dump_tuner_list (const vqec_syscfg_t *cfg)
{
    int cur_idx = 0;

    for (cur_idx = 0; cur_idx < VQEC_SYSCFG_MAX_MAX_TUNERS; cur_idx++) {
        if (strncmp(cfg->tuner_list[cur_idx].tuner_name, "", 1)) {
            if (cur_idx == 0) {
                CONSOLE_PRINTF("tuner_list = (\n");
            }
            CONSOLE_PRINTF("  {\n"
                           "    name = \"%s\";\n",
                           cfg->tuner_list[cur_idx].tuner_name);
            if (strncmp(cfg->tuner_list[cur_idx].tuner_url, "", 1)) {
                CONSOLE_PRINTF("    url = \"%s\";\n",
                               cfg->tuner_list[cur_idx].tuner_url);
            }
            if (cfg->tuner_list[cur_idx].max_receive_bandwidth_rcc) {
                CONSOLE_PRINTF("    max_receive_bandwidth_rcc = %u;\n",
                               cfg->tuner_list[cur_idx].
                               max_receive_bandwidth_rcc);
            }
            if (cfg->tuner_list[cur_idx].max_receive_bandwidth_er) {
                CONSOLE_PRINTF("    max_receive_bandwidth_er = %u;\n",
                               cfg->tuner_list[cur_idx].
                               max_receive_bandwidth_er);
            }
            if (strncmp(cfg->tuner_list[cur_idx + 1].tuner_name, "", 1)) {
                CONSOLE_PRINTF("  },\n");
            } else {
                CONSOLE_PRINTF("  }\n"
                               ");\n");
            }
        }
    }
}

/*
 * Dumps the contents of a loaded configuration structure to the console.
 *
 * If 'cfg_display' is supplied, then only parameters which are marked in
 * this array as TRUE will be displayed.
 *
 * @param[in] v_cfg       - configuration to display
 * @param[in] cfg_display - [optional] if supplied, only display parameters
 *                           whose corresponding entry in this array is TRUE
 */
UT_STATIC void 
vqec_syscfg_dump (const vqec_syscfg_t *v_cfg,
                  boolean cfg_display[]) 
{
    vqec_cfg_enum_t cfg_id;
    struct in_addr addr;
    char temp[ INET_ADDRSTRLEN ];
    const char *t_ptr = 0;

    if (!v_cfg) {
        return;
    }

    /* 
     * Make enum trick / case statement to assure we have added
     * a dump case for each configuration parameter.
     *
     * Note also there is an implicit convention that the 
     * output of this configuration dump utility should be
     * readable on input as a configuration file.  This is
     * designed to make figuring out desired input format
     * a easier.
     */
    for (cfg_id=VQEC_CFG_MAX_TUNERS;
         cfg_id < VQEC_CFG_MUST_BE_LAST; cfg_id++)
    {
        /* skip this parameter if not supplied in the configuration */
        if (cfg_display && !cfg_display[cfg_id]) {
            continue;
        }
        switch(cfg_id) {
            case VQEC_CFG_MAX_TUNERS:
                CONSOLE_PRINTF("max_tuners = %d;\n", v_cfg->max_tuners);
                break;
            case VQEC_CFG_CHANNEL_LINEUP:
                CONSOLE_PRINTF("channel_lineup = \"%s\";\n", 
                               v_cfg->channel_lineup);
                break;
            case VQEC_CFG_NETWORK_CFG_PATHNAME:
                CONSOLE_PRINTF("network_cfg_pathname = \"%s\";\n",
                               v_cfg->network_cfg_pathname);
                break;
            case VQEC_CFG_OVERRIDE_CFG_PATHNAME:
                CONSOLE_PRINTF("override_cfg_pathname = \"%s\";\n",
                               v_cfg->override_cfg_pathname);
                break;
            case VQEC_CFG_INDEX_CFG_PATHNAME:
                CONSOLE_PRINTF("index_cfg_pathname = \"%s\";\n",
                               v_cfg->index_cfg_pathname);
                break;
            case VQEC_CFG_VQEC_ENABLE:
                CONSOLE_PRINTF("vqec_enable = %s;\n",
                               v_cfg->d_vqec_enable ? "true" : "false");
                break;
            case VQEC_CFG_JITTER_BUFF_SIZE:
                CONSOLE_PRINTF("jitter_buff_size = %d;\n", 
                               v_cfg->jitter_buff_size);
                break;
            case VQEC_CFG_REPAIR_TRIGGER_POINT_ABS:
                CONSOLE_PRINTF("repair_trigger_point_abs = %d;\n",
                               v_cfg->repair_trigger_point_abs);
                break;
            case VQEC_CFG_REPAIR_TRIGGER_POINT:
                CONSOLE_PRINTF("repair_trigger_point = %d;\n",
                               v_cfg->d_repair_trigger_point);
                break;
            case VQEC_CFG_PAKPOOL_SIZE:
                CONSOLE_PRINTF("pakpool_size = %d;\n", v_cfg->pakpool_size);
                break;
            case VQEC_CFG_SO_RCVBUF:
                CONSOLE_PRINTF("so_rcvbuf = %d;\n", v_cfg->so_rcvbuf);
                break;
            case VQEC_CFG_STRIP_RTP:
                CONSOLE_PRINTF("strip_rtp = %s;\n",
                               v_cfg->strip_rtp ? "true" : "false");
                break;
            case VQEC_CFG_INPUT_IFNAME:
                CONSOLE_PRINTF("input_ifname = \"%s\";\n", 
                               v_cfg->input_ifname);
                break;
            case VQEC_CFG_SIG_MODE:
                CONSOLE_PRINTF("sig_mode = \"%s\";\n", 
                           (v_cfg->sig_mode == VQEC_SM_STD) ? "std" : "nat");
                break;
            case VQEC_CFG_NAT_BINDING_REFRESH_INTERVAL:
                CONSOLE_PRINTF("nat_binding_refresh_interval = %d;\n",
                               v_cfg->nat_binding_refresh_interval);
                break;
            case VQEC_CFG_NAT_FILTER_REFRESH_INTERVAL:
                break;
            case VQEC_CFG_MAX_PAKSIZE:
                CONSOLE_PRINTF("max_paksize = %d;\n", v_cfg->max_paksize);
                break;
            case VQEC_CFG_STUN_SERVER_IP:
                break;
            case VQEC_CFG_STUN_SERVER_PORT:
                break;
            case VQEC_CFG_CDI_ENABLE:
                CONSOLE_PRINTF("cdi_enable = %s;\n",
                               v_cfg->cdi_enable ? "true" : "false");
                break;
            case VQEC_CFG_DOMAIN_NAME_OVERRIDE:
                CONSOLE_PRINTF("domain_name_override = \"%s\";\n",
                               v_cfg->domain_name_override);
                break;
            case VQEC_CFG_CLI_TELNET_PORT:
                CONSOLE_PRINTF("libcli_telnet_port = %d;\n",
                               v_cfg->cli_telnet_port);
                break;
            case VQEC_CFG_OUTPUT_PAKQ_LIMIT:
                CONSOLE_PRINTF("output_pakq_limit = %d;\n", 
                               v_cfg->output_pakq_limit);
                break;
            case VQEC_CFG_UPDATE_WINDOW:
                CONSOLE_PRINTF("update_window = %d;\n",
                               v_cfg->update_window);
                break;
            case VQEC_CFG_UPDATE_INTERVAL_MAX:
                CONSOLE_PRINTF("update_interval_max = %d;\n",
                               v_cfg->update_interval_max);
                break;
            case VQEC_CFG_APP_PAKS_PER_RCC:
#if HAVE_FCC
                CONSOLE_PRINTF("app_paks_per_rcc = %d;\n",
                               v_cfg->app_paks_per_rcc);
#endif
                break;
            case VQEC_CFG_NUM_BYES:
                CONSOLE_PRINTF("num_byes = %d;\n",
                               v_cfg->num_byes);
                break;
            case VQEC_CFG_BYE_DELAY:
                CONSOLE_PRINTF("bye_delay = %d;\n",
                               v_cfg->bye_delay);
                break;
            case VQEC_CFG_ERROR_REPAIR_ENABLE:
                CONSOLE_PRINTF("error_repair_enable = %s;\n",
                               v_cfg->error_repair_enable ? "true" : "false");
                break;
            case VQEC_CFG_ERROR_REPAIR_POLICER_ENABLE:
                CONSOLE_PRINTF("error_repair_policer.enable = %s\n",
                               v_cfg->error_repair_policer_enable ? 
                               "true" : "false");
                break;
            case VQEC_CFG_ERROR_REPAIR_POLICER_RATE:
                CONSOLE_PRINTF("error_repair_policer.rate = %d;\n",
                               v_cfg->error_repair_policer_rate);
                break;
            case VQEC_CFG_ERROR_REPAIR_POLICER_BURST:
                CONSOLE_PRINTF("error_repair_policer.burst = %d;\n",
                               v_cfg->error_repair_policer_burst);
                break;
            case VQEC_CFG_LOG_LEVEL:
                CONSOLE_PRINTF("log_level = %d;\n",
                               v_cfg->log_level);
                break;
            /* added for use by FEC */
            case VQEC_CFG_FEC_ENABLE:
                CONSOLE_PRINTF("fec_enable = %s;\n",
                               v_cfg->fec_enable ? "true" : "false");
                break;
            case VQEC_CFG_CLI_IFNAME:
                CONSOLE_PRINTF("cli_ifname = \"%s\";\n", 
                               v_cfg->cli_ifname);
                break;

            /* added for use by RCC */
#if HAVE_FCC
            case VQEC_CFG_RCC_ENABLE:
                CONSOLE_PRINTF("rcc_enable = %s;\n",
                               v_cfg->rcc_enable ? "true" : "false");
                break;
            case VQEC_CFG_RCC_PAKPOOL_MAX_PCT:
                /* hide rcc_pakpool_max_pct from show sys */
                break;
            case VQEC_CFG_RCC_START_TIMEOUT:
                CONSOLE_PRINTF("rcc_start_timeout = %d;\n",
                               v_cfg->rcc_start_timeout);
                break;
            case VQEC_CFG_FASTFILL_ENABLE:
                CONSOLE_PRINTF("fastfill_enable = %s;\n",
                               v_cfg->fastfill_enable ? "true" : "false");
                break;
            case VQEC_CFG_RCC_EXTRA_IGMP_IP:
                addr.s_addr = v_cfg->rcc_extra_igmp_ip;            
                t_ptr = inet_ntop(AF_INET, &addr.s_addr, temp, INET_ADDRSTRLEN);
                CONSOLE_PRINTF("rcc_extra_igmp_ip = \"%s\";\n", t_ptr);
                break;
#endif
            case VQEC_CFG_REORDER_DELAY_ABS:
                CONSOLE_PRINTF("reorder_delay_abs = %d;\n",
                               v_cfg->reorder_delay_abs);
                break;
            case VQEC_CFG_REORDER_DELAY:
                CONSOLE_PRINTF("reorder_delay = %d;\n",
                               v_cfg->d_reorder_delay);
                break;
            case VQEC_CFG_VCDS_SERVER_IP:
                addr.s_addr = v_cfg->vcds_server_ip;            
                t_ptr = inet_ntop(AF_INET, &addr.s_addr, temp, INET_ADDRSTRLEN);
                CONSOLE_PRINTF("vcds_server_ip = \"%s\";\n", t_ptr);
                break;
            case VQEC_CFG_VCDS_SERVER_PORT:
                CONSOLE_PRINTF("vcds_server_port = %d;\n",
                               ntohs(v_cfg->vcds_server_port));
                break;
            case VQEC_CFG_TUNER_LIST:
                vqec_syscfg_dump_tuner_list(v_cfg);
                break;
            case VQEC_CFG_RTCP_DSCP_VALUE:
                CONSOLE_PRINTF("rtcp_dscp_value = %d;\n", 
                               v_cfg->rtcp_dscp_value);
                break;
            case VQEC_CFG_DELIVER_PAKS_TO_USER:
                CONSOLE_PRINTF("deliver_paks_to_user = %s;\n",
                               v_cfg->deliver_paks_to_user ? "true" : "false");
                break;
            case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_SD:
                CONSOLE_PRINTF("max_receive_bandwidth_sd = %u;\n", 
                               v_cfg->max_receive_bandwidth_sd);
                break;
            case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_SD_RCC:
                CONSOLE_PRINTF("max_receive_bandwidth_sd_rcc = %u;\n", 
                               v_cfg->max_receive_bandwidth_sd_rcc);
                break;
            case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_HD:
                CONSOLE_PRINTF("max_receive_bandwidth_hd = %u;\n", 
                               v_cfg->max_receive_bandwidth_hd);
                break;
            case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_HD_RCC:
                CONSOLE_PRINTF("max_receive_bandwidth_hd_rcc = %u;\n", 
                               v_cfg->max_receive_bandwidth_hd_rcc);
                break;
            case VQEC_CFG_MIN_HD_STREAM_BITRATE:
                CONSOLE_PRINTF("min_hd_stream_bitrate = %u;\n", 
                               v_cfg->min_hd_stream_bitrate);
                break;
            case VQEC_CFG_MAX_FASTFILL:
                CONSOLE_PRINTF("max_fastfill = %d;\n", 
                               v_cfg->max_fastfill);
                break;
            case VQEC_CFG_APP_DELAY:
                CONSOLE_PRINTF("app_delay = %d;\n", 
                               v_cfg->app_delay);
                break;
            case VQEC_CFG_SRC_IP_FILTER_ENABLE:
                CONSOLE_PRINTF("src_ip_filter_enable = %s;\n", 
                               v_cfg->src_ip_filter_enable? "true" : "false");
                break;
            case VQEC_CFG_QOE_ENABLE:
                CONSOLE_PRINTF("qoe_enable = %s;\n",
                               v_cfg->qoe_enable ? "true" : "false");
                break;
            case VQEC_CFG_RCC_MAX_CONCURRENT:
                CONSOLE_PRINTF("rcc_max_concurrent = %d;\n",
                               v_cfg->rcc_max_concurrent);
                break;
            case VQEC_CFG_INTEGRATED_RTP_FALLBACK:
                CONSOLE_PRINTF("integrated_rtp_fallback = %s;\n",
                               v_cfg->integrated_rtp_fallback ?
                               "true" : "false");
                break;
            case VQEC_CFG_UDP_PASSTHRU_SUPPORT:
                CONSOLE_PRINTF("udp_passthru_support = %s;\n",
                               v_cfg->udp_passthru_support ? "true" : "false");
                break;
           case VQEC_CFG_VOD_CMD_TIMEOUT:
                CONSOLE_PRINTF("vod_cmd_timeout = %d;\n", 
                               v_cfg->vod_cmd_timeout);
                break;
           case VQEC_CFG_VOD_MAX_SESSIONS:
                CONSOLE_PRINTF("vod_max_sessions = %d;\n", 
                               v_cfg->vod_max_sessions);
                break;
            case VQEC_CFG_VOD_MODE:
                CONSOLE_PRINTF("vod_mode = \"%s\";\n", 
                  (v_cfg->vod_mode == VQEC_VOD_MODE_IPTV) ? 
                   VQEC_SYSCFG_VOD_MODE_IPTV : VQEC_SYSCFG_VOD_MODE_CABLE);
                break;
            case VQEC_CFG_STUN_OPTIMIZATION:
                CONSOLE_PRINTF("stun_optimization = %s;\n",
                               v_cfg->stun_optimization ? "true" : "false");
                break;

            case VQEC_CFG_MUST_BE_LAST:
                break;
        }
    }
}

/******************************************
 * Local helper functions and definitions *
 ******************************************/

#define vqec_inv_int_range_fmt \
              "Invalid %s value (%d); Valid range: %d - %d\n"

#define vqec_tuner_inv_int_range_fmt \
              "Invalid tuner[%d].%s value (%d); Valid range: %d - %d\n"

#define vqec_inv_string_value_fmt \
              "Invalid %s value; Value must be a string\n"

#define vqec_param_deprecated_conflict_fmt \
              "WARNING: config parameter \"%s\" ignored, " \
              "conflicts with parameter \"%s\"\n"

#define vqec_param_deprecated_accepted_fmt \
              "WARNING: config parameter \"%s\" accepted but is being " \
              "deprecated, use \"%s\" instead\n"

#define vqec_param_obsoleted_fmt \
              "WARNING: config parameter \"%s\" is obsolete\n"

static void strtolower (char *out_str, int out_len, const char *in_str)
{
    int i,slen;

    /* 
     * Assure that we pick a length that is less than the
     * out_len - 1, so that we have room for a NULL.
     */
    slen = strnlen(in_str, out_len-1);

    for (i = 0; i < slen; i++) {
	if (isupper(in_str[i])) {
	    out_str[i] = tolower(in_str[i]);
        } else {
	    out_str[i] = in_str[i];
        }
    }
    out_str[i] = '\0';
}


UT_STATIC vqec_error_t
vqec_syscfg_read_tuner_list (vqec_syscfg_t *cfg,
                             vqec_config_setting_t *r_setting,
                             boolean log_nonfatal_messages)
{
    int num_tuners = 0;
    int cur_idx = 0;
    vqec_config_setting_t *cur_tuner;
    vqec_config_setting_t *temp_setting;
    const char *temp_str = 0;
    int temp_int;
    int max_tuners_in_list;
    char debug_str[DEBUG_STR_LEN];

    num_tuners = vqec_config_setting_length(r_setting);
    max_tuners_in_list = num_tuners;
    if (num_tuners > cfg->max_tuners) {
        max_tuners_in_list = cfg->max_tuners; 
    }

    for (cur_idx = 0; cur_idx < max_tuners_in_list; cur_idx++) {
        /* get the cur_idxth tuner in the list of tuners */
        cur_tuner = vqec_config_setting_get_elem(r_setting, cur_idx);

        /* get the params from this tuner */
        temp_setting = vqec_config_setting_get_member(cur_tuner, "name");
        if (temp_setting) {
            temp_str = vqec_config_setting_get_string(temp_setting);
            if (!temp_str) {
                return (VQEC_ERR_PARAMRANGEINVALID);
            }
            (void)strlcpy(cfg->tuner_list[cur_idx].tuner_name,
                          temp_str, VQEC_MAX_NAME_LEN);
        }

        temp_setting = vqec_config_setting_get_member(cur_tuner, "url");
        if (temp_setting) {
            temp_str = vqec_config_setting_get_string(temp_setting);
            if (!temp_str) {
                return (VQEC_ERR_PARAMRANGEINVALID);
            }
            (void)strlcpy(cfg->tuner_list[cur_idx].tuner_url,
                          temp_str, VQEC_MAX_NAME_LEN);
        }

        temp_setting =
            vqec_config_setting_get_member(cur_tuner,
                                           "max_receive_bandwidth_rcc");
        if (temp_setting) {
            temp_int = vqec_config_setting_get_int(temp_setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_TUNER_MAX_RECEIVE_BANDWIDTH_RCC) &&
                (temp_int <= VQEC_SYSCFG_MAX_TUNER_MAX_RECEIVE_BANDWIDTH_RCC)) {
                cfg->tuner_list[cur_idx].max_receive_bandwidth_rcc = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_tuner_inv_int_range_fmt,
                             cur_idx,
                             "max_receive_bandwidth_rcc",
                             temp_int,
                             VQEC_SYSCFG_MIN_TUNER_MAX_RECEIVE_BANDWIDTH_RCC,
                             VQEC_SYSCFG_MAX_TUNER_MAX_RECEIVE_BANDWIDTH_RCC);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                return (VQEC_ERR_PARAMRANGEINVALID);
            }
        }

        temp_setting =
            vqec_config_setting_get_member(cur_tuner,
                                           "max_receive_bandwidth_er");
        if (temp_setting) {
            temp_int = vqec_config_setting_get_int(temp_setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_TUNER_MAX_RECEIVE_BANDWIDTH_ER) &&
                (temp_int <= VQEC_SYSCFG_MAX_TUNER_MAX_RECEIVE_BANDWIDTH_ER)) {
                cfg->tuner_list[cur_idx].max_receive_bandwidth_er = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_tuner_inv_int_range_fmt,
                             cur_idx,
                             "max_receive_bandwidth_er",
                             temp_int,
                             VQEC_SYSCFG_MIN_TUNER_MAX_RECEIVE_BANDWIDTH_ER,
                             VQEC_SYSCFG_MAX_TUNER_MAX_RECEIVE_BANDWIDTH_ER);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                return (VQEC_ERR_PARAMRANGEINVALID);
            }
        }
    }

    if (num_tuners > cfg->max_tuners) {
        return (VQEC_ERR_MAXLIMITTUNER);
    }

    return VQEC_OK;
}

/*
 * Validates the contents of a configuration file or memory buffer.
 *
 * If an (optional) 'cfg' structure is passed in, then valid
 * configuration will be loaded into the caller's cfg structure.
 * The 'cfg' structure is NOT initialized--if a parameter is not
 * assigned a valid value in the configuration file, its field within
 * the 'cfg' structure will be unmodified.  Callers should thus 
 * initialize this structure prior to input.
 *
 * If an (optional) 'cfg_supported' array is passed in, then only the
 * parameters which are marked TRUE in the array will be eligible for
 * processing.  if 'cfg_support' is not passed in, all defined
 * parameters are eligible for processing.
 * 
 * If an (optional) 'cfg_present" array is passed in, then a parameter's
 * corresponding entry in the array will be marked TRUE only if the
 * parameter is eligible for processing, present in the configuration file,
 * and assigned a valid value. 
 *
 * NOTE:  The output arguments will only be assigned the return code
 *        indicates that a configuraiton has been successfully processed
 *        (VQEC_OK or VQEC_ERR_PARAMRANGEINVALID).
 *
 * @param[in/out]  cfg           - [optional] structure which is loaded with 
 *                                  the valid parameters values assigned in
 *                                  the config file.  Caller must initialize.
 * @param[in]      cfg_supported - [optional] array (of size 
 *                                  VQEC_CFG_MUST_BE_LAST) which limits
 *                                  configuration parameters loaded by
 *                                  this API to the ones marked TRUE
 * @param[in/out]  cfg_present   - [optional] array (of size
 *                                  VQEC_CFG_MUST_BE_LAST) which records
 *                                  (as TRUE) parameters which were present
 *                                  and assigned valid values in the
 *                                  configuration file. Caller must initialize.
 * @param[in]      filename      - configuration file to load
 * @param[in]      buffer        - buffer from which to read configuration
 *                                  (Used ONLY when 'filename' is NULL)
 * @param[in]      log_nonfatal_messages
 *                               - TRUE:  non-fatal messages are logged
 *                               - FALSE: non-fatal messages are not logged
 * @param[out]     vqec_error_t  - Return code
 *                                    VQEC_OK:
 *                                       all recognized parameter settings
 *                                       in the file were valid
 *                                    VQEC_ERR_PARAMRANGEINVALID
 *                                       if at least one invalid parameter
 *                                       setting was detected
 *                                    VQEC_ERR_INVALIDARGS
 *                                       no configuration specified
 *                                    VQEC_ERR_CONFIG_NOT_FOUND
 *                                       the supplied config file not be read
 *                                    VQEC_ERR_CONFIG_INVALID
 *                                       the supplied config could not be
 *                                       parsed (syntax error)
 */
vqec_error_t
vqec_syscfg_load (vqec_syscfg_t *cfg,
                  const boolean cfg_supported[VQEC_CFG_MUST_BE_LAST],
                  boolean cfg_present[VQEC_CFG_MUST_BE_LAST],
                  const char *filename,
                  const char *buffer,
                  const boolean log_nonfatal_messages)
{
    vqec_cfg_enum_t i;
    vqec_error_t param_err, ret = VQEC_OK;
    const char *temp_str=0;
    int temp_int, config_result;
    vqec_config_setting_t *setting;
    char t_str[VQEC_MAX_NAME_LEN];
    static vqec_syscfg_t tmp_cfg;
    struct in_addr temp_addr;
    char debug_str[DEBUG_STR_LEN];
  
    /* Initialize the vqec_config configuration */
    vqec_config_init(&vqec_cfg);

    /* Load the configuration */
    if (filename) {
        if (access(filename, F_OK)) {
            ret = VQEC_ERR_CONFIG_NOT_FOUND;
            goto done;
        }
        config_result = vqec_config_read_file(&vqec_cfg, filename);
    } else if (buffer) {
        config_result = vqec_config_read_buffer(&vqec_cfg, buffer);
    } else {
        ret = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    /* Log any fatal/syntax errrors and abort */
    if (!config_result) {
        if (filename) {
            snprintf(t_str, VQEC_MAX_NAME_LEN, "file [%s]", filename);
        } else {
            snprintf(t_str, VQEC_MAX_NAME_LEN, "buffer");
        }
        snprintf(debug_str, DEBUG_STR_LEN, 
                 "failed to load configuration %s - (%s error line = %d)\n",
                 t_str, vqec_cfg.error_text, vqec_cfg.error_line);
        syslog_print(VQEC_SYSCFG_ERROR, debug_str);
        ret = VQEC_ERR_CONFIG_INVALID;
        goto done;
    }

    /*
     * If caller just wants to validate the config, assign a temporary
     * area into which parameters may be loaded during the validation.
     */
    if (!cfg) {
        cfg = &tmp_cfg;
    }

    /*
     * Loop through all of the supported params in the config array,
     * looking for each within the config file.
     * Note no warning or error is given for unsupported parameters
     * that are in a valid syntax, nor are all parameters
     * required to be set from the configuration file.
     */
    for(i=VQEC_CFG_MAX_TUNERS; i < VQEC_CFG_MUST_BE_LAST; i++) {
        /* Avoid loading this parameter if caller requests */
        if (cfg_supported && !cfg_supported[i]) {
            continue;
        }
        /* Lookup the next parameter file setting in the vqec_cfg. */
        setting = vqec_config_lookup(&vqec_cfg, vqec_cfg_arr[i].param_name);
        if (!setting) {
            continue;
        }
        /*
         * The parameter is present in the config file.
         * Validate the value and (if valid) load it.
         * Warnings may be printed in the event of an invalid value.
         * Also record in "param_err" whether its value is valid 
         * (we assume "yes" until proven otherwise...)
         */
        param_err = VQEC_OK;
        switch (i) {
        case VQEC_CFG_MAX_TUNERS:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_MAX_TUNERS) &&
                (temp_int <= VQEC_SYSCFG_MAX_MAX_TUNERS)) {
                cfg->max_tuners = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "max_tuners",
                             temp_int,
                             VQEC_SYSCFG_MIN_MAX_TUNERS,
                             VQEC_SYSCFG_MAX_MAX_TUNERS);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_CHANNEL_LINEUP:
            temp_str = vqec_config_setting_get_string(setting);
            if (temp_str) {
                (void)strlcpy(cfg->channel_lineup,
                              temp_str, VQEC_MAX_NAME_LEN);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_string_value_fmt,
                             vqec_cfg_arr[i].param_name);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_NETWORK_CFG_PATHNAME:
            temp_str = vqec_config_setting_get_string(setting);
            if (temp_str) {
                (void)strlcpy(cfg->network_cfg_pathname,
                              temp_str, VQEC_MAX_NAME_LEN);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_string_value_fmt,
                             vqec_cfg_arr[i].param_name);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_OVERRIDE_CFG_PATHNAME:
            temp_str = vqec_config_setting_get_string(setting);
            if (temp_str) {
                (void)strlcpy(cfg->override_cfg_pathname,
                              temp_str, VQEC_MAX_NAME_LEN);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_string_value_fmt,
                             vqec_cfg_arr[i].param_name);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_INDEX_CFG_PATHNAME:
            temp_str = vqec_config_setting_get_string(setting);
            if (temp_str) {
                (void)strlcpy(cfg->index_cfg_pathname,
                              temp_str, VQEC_MAX_NAME_LEN);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_string_value_fmt,
                             vqec_cfg_arr[i].param_name);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_VQEC_ENABLE:
            if (vqec_config_setting_type(setting) ==
                VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->d_vqec_enable = vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "vqec_enable");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_JITTER_BUFF_SIZE:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_JITTER_BUFF_SIZE) &&
                (temp_int <= VQEC_SYSCFG_MAX_JITTER_BUFF_SIZE)) {
                cfg->jitter_buff_size = temp_int;
            } else {
                 if (log_nonfatal_messages) {
                     snprintf(debug_str, DEBUG_STR_LEN,
                              vqec_inv_int_range_fmt,
                              "jitter_buff_size",
                              temp_int,
                              VQEC_SYSCFG_MIN_JITTER_BUFF_SIZE,
                              VQEC_SYSCFG_MAX_JITTER_BUFF_SIZE);
                     syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                 }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_REPAIR_TRIGGER_POINT_ABS:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >=
                 VQEC_SYSCFG_MIN_REPAIR_TRIGGER_POINT_ABS) &&
                (temp_int <=
                 VQEC_SYSCFG_MAX_REPAIR_TRIGGER_POINT_ABS)) {
                cfg->repair_trigger_point_abs = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "repair_trigger_point_abs",
                             temp_int,
                             VQEC_SYSCFG_MIN_REPAIR_TRIGGER_POINT_ABS,
                             VQEC_SYSCFG_MAX_REPAIR_TRIGGER_POINT_ABS);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_REPAIR_TRIGGER_POINT:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >=
                 VQEC_SYSCFG_MIN_REPAIR_TRIGGER_POINT) &&
                (temp_int <=
                 VQEC_SYSCFG_MAX_REPAIR_TRIGGER_POINT)) {
                cfg->d_repair_trigger_point = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "repair_trigger_point",
                             temp_int,
                             VQEC_SYSCFG_MIN_REPAIR_TRIGGER_POINT,
                             VQEC_SYSCFG_MAX_REPAIR_TRIGGER_POINT);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_PAKPOOL_SIZE:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_PAKPOOL_SIZE) &&
                (temp_int <= VQEC_SYSCFG_MAX_PAKPOOL_SIZE)) {
                cfg->pakpool_size = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "pakpool_size",
                             temp_int,
                             VQEC_SYSCFG_MIN_PAKPOOL_SIZE,
                             VQEC_SYSCFG_MAX_PAKPOOL_SIZE);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_SO_RCVBUF:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_SO_RCVBUF) &&
                (temp_int <= VQEC_SYSCFG_MAX_SO_RCVBUF)) {
                cfg->so_rcvbuf = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "so_rcvbuf",
                             temp_int,
                             VQEC_SYSCFG_MIN_SO_RCVBUF,
                             VQEC_SYSCFG_MAX_SO_RCVBUF);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_STRIP_RTP:
            if (vqec_config_setting_type(setting) ==
                VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->strip_rtp = vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "strip_rtp");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_INPUT_IFNAME:
            temp_str = vqec_config_setting_get_string(setting);
            if (temp_str) {
                (void)strlcpy(cfg->input_ifname,
                              temp_str, VQEC_MAX_NAME_LEN);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_string_value_fmt,
                             vqec_cfg_arr[i].param_name);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_SIG_MODE:
            temp_str = vqec_config_setting_get_string(setting);
            if (temp_str) {
                strtolower(t_str, VQEC_MAX_NAME_LEN, temp_str);
            }
            if (temp_str && !strcmp(t_str, VQEC_SYSCFG_SIG_MODE_STD)) {
                cfg->sig_mode = VQEC_SM_STD;
            } else if (temp_str && !strcmp(t_str,VQEC_SYSCFG_SIG_MODE_NAT)) {
                cfg->sig_mode = VQEC_SM_NAT;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "Invalid sig_mode value (%s) "
                             "Valid values [%s,%s]\n",
                             temp_str ? temp_str : "non-string",
                             VQEC_SYSCFG_SIG_MODE_NAT,
                             VQEC_SYSCFG_SIG_MODE_STD);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_NAT_BINDING_REFRESH_INTERVAL:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >=
                 VQEC_SYSCFG_MIN_NAT_BINDING_REFRESH_INTERVAL) &&
                (temp_int <=
                 VQEC_SYSCFG_MAX_NAT_BINDING_REFRESH_INTERVAL)) {
                cfg->nat_binding_refresh_interval = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "nat_binding_refresh_interval",
                             temp_int,
                             VQEC_SYSCFG_MIN_NAT_BINDING_REFRESH_INTERVAL,
                             VQEC_SYSCFG_MAX_NAT_BINDING_REFRESH_INTERVAL);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_NAT_FILTER_REFRESH_INTERVAL:
            break;
        case VQEC_CFG_MAX_PAKSIZE:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_MAX_PAKSIZE) &&
                (temp_int <= VQEC_SYSCFG_MAX_MAX_PAKSIZE)) {
                cfg->max_paksize = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "max_paksize",
                             temp_int,
                             VQEC_SYSCFG_MIN_MAX_PAKSIZE,
                             VQEC_SYSCFG_MAX_MAX_PAKSIZE);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_STUN_SERVER_IP:
            break;
        case VQEC_CFG_STUN_SERVER_PORT:
            break;
        case VQEC_CFG_CDI_ENABLE:
            if (vqec_config_setting_type(setting) ==
                VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->cdi_enable = vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "cdi_enable");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_DOMAIN_NAME_OVERRIDE:
            temp_str = vqec_config_setting_get_string(setting);
            if (temp_str) {
                (void)strlcpy(cfg->domain_name_override, temp_str,
                              VQEC_MAX_NAME_LEN);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_string_value_fmt,
                             vqec_cfg_arr[i].param_name);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_CLI_TELNET_PORT:
            temp_int = vqec_config_setting_get_int(setting);
            if (is_vqec_cfg_cli_telnet_port_valid(temp_int)) {
                cfg->cli_telnet_port = (int16_t)temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "Invalid %s value (%d)\n "
                             " Valid range: %d - %d or 0\n",
                             "libcli_telnet_port",
                             temp_int,
                             VQEC_SYSCFG_MIN_R1_CLI_TELNET_PORT,
                             VQEC_SYSCFG_MAX_R1_CLI_TELNET_PORT);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_OUTPUT_PAKQ_LIMIT:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_OUTPUT_PAKQ_LIMIT) &&
                (temp_int <= VQEC_SYSCFG_MAX_OUTPUT_PAKQ_LIMIT)) {
                cfg->output_pakq_limit = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "output_pakq_limit",
                             temp_int,
                             VQEC_SYSCFG_MIN_OUTPUT_PAKQ_LIMIT,
                             VQEC_SYSCFG_MAX_OUTPUT_PAKQ_LIMIT);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_UPDATE_WINDOW:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_UPDATE_WINDOW) &&
                (temp_int <= VQEC_SYSCFG_MAX_UPDATE_WINDOW)) {
                cfg->update_window = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             vqec_cfg_arr[i].param_name,
                             temp_int,
                             VQEC_SYSCFG_MIN_UPDATE_WINDOW,
                             VQEC_SYSCFG_MAX_UPDATE_WINDOW);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_UPDATE_INTERVAL_MAX:
            temp_int = vqec_config_setting_get_int(setting);
            if (is_vqec_cfg_update_interval_max_valid(temp_int)) {
                cfg->update_interval_max = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             vqec_cfg_arr[i].param_name,
                             temp_int,
                             VQEC_SYSCFG_MIN_R0_UPDATE_INTERVAL_MAX,
                             VQEC_SYSCFG_MAX_R1_UPDATE_INTERVAL_MAX);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;            
        case VQEC_CFG_APP_PAKS_PER_RCC:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_APP_PAKS_PER_RCC) &&
                (temp_int <= VQEC_SYSCFG_MAX_APP_PAKS_PER_RCC)) {
                cfg->app_paks_per_rcc = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             vqec_cfg_arr[i].param_name,
                             temp_int,
                             VQEC_SYSCFG_MIN_APP_PAKS_PER_RCC,
                             VQEC_SYSCFG_MAX_APP_PAKS_PER_RCC);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_ERROR_REPAIR_ENABLE:
            if (vqec_config_setting_type(setting) ==
                VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->error_repair_enable =
                    vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "error_repair_enable");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_ERROR_REPAIR_POLICER_ENABLE:
            if (vqec_config_setting_type(setting) ==
                VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->error_repair_policer_enable =
                    vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "error_repair_policer_enable");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_ERROR_REPAIR_POLICER_RATE:
            temp_int = vqec_config_setting_get_int(setting);
            if (is_vqec_cfg_error_repair_policer_rate_valid(temp_int)) {
                cfg->error_repair_policer_rate = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             vqec_cfg_arr[i].param_name,
                             temp_int,
                             VQEC_SYSCFG_MIN_ERROR_REPAIR_POLICER_RATE,
                             VQEC_SYSCFG_MAX_ERROR_REPAIR_POLICER_RATE);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_ERROR_REPAIR_POLICER_BURST:
            temp_int = vqec_config_setting_get_int(setting);
            if (is_vqec_cfg_error_repair_policer_burst_valid(temp_int)) {
                cfg->error_repair_policer_burst = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             vqec_cfg_arr[i].param_name,
                             temp_int,
                             VQEC_SYSCFG_MIN_ERROR_REPAIR_POLICER_BURST,
                             VQEC_SYSCFG_MAX_ERROR_REPAIR_POLICER_BURST);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_LOG_LEVEL:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= LOG_EMERG) &&
                (temp_int <= LOG_DEBUG)) {
                cfg->log_level = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             vqec_cfg_arr[i].param_name,
                             temp_int,
                             LOG_EMERG,
                             LOG_DEBUG);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
            /* added for use by FEC */
        case VQEC_CFG_FEC_ENABLE:
            if (vqec_config_setting_type(setting) ==
                VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->fec_enable = vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "fec_enable");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
            /* added for use by RCC */
#if HAVE_FCC
        case VQEC_CFG_RCC_ENABLE:
            if (vqec_config_setting_type(setting) ==
                VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->rcc_enable = vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "rcc_enable");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_RCC_PAKPOOL_MAX_PCT:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= 
                 VQEC_SYSCFG_MIN_RCC_PAKPOOL_MAX_PCT) &&
                (temp_int <= 
                 VQEC_SYSCFG_MAX_RCC_PAKPOOL_MAX_PCT)) {
                cfg->rcc_pakpool_max_pct = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             vqec_cfg_arr[i].param_name,
                             temp_int,
                             VQEC_SYSCFG_MIN_RCC_PAKPOOL_MAX_PCT,
                             VQEC_SYSCFG_MAX_RCC_PAKPOOL_MAX_PCT);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_RCC_START_TIMEOUT:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= 
                 VQEC_SYSCFG_MIN_RCC_START_TIMEOUT) &&
                (temp_int <= 
                 VQEC_SYSCFG_MAX_RCC_START_TIMEOUT)) {
                cfg->rcc_start_timeout = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "rcc_start_timeout",
                             temp_int,
                             VQEC_SYSCFG_MIN_RCC_START_TIMEOUT,
                             VQEC_SYSCFG_MAX_RCC_START_TIMEOUT);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_FASTFILL_ENABLE:
            if (vqec_config_setting_type(setting) ==
                VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->fastfill_enable = vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "fastfill_enable");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_RCC_EXTRA_IGMP_IP:
            temp_str = vqec_config_setting_get_string(setting);
            /*
             * conditions checked here:
             * 1.  setting string exists
             * 2.  setting string successfully parsed into integer address
             * 3.  parsed address is either a valid multicast address or
             *     0.0.0.0
             */
            if (temp_str && 
                inet_aton(temp_str, &temp_addr) &&
                (IN_MULTICAST(ntohl(temp_addr.s_addr)) ||
                 temp_addr.s_addr == 0)) {
                     cfg->rcc_extra_igmp_ip = temp_addr.s_addr;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid rcc_extra_igmp_ip = %s\n",
                             temp_str ? temp_str : "non-string");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
#endif
        case VQEC_CFG_NUM_BYES:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_NUM_BYES) &&
                (temp_int <= VQEC_SYSCFG_MAX_NUM_BYES)) {
                cfg->num_byes = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "num_byes",
                             temp_int,
                             VQEC_SYSCFG_MIN_NUM_BYES,
                             VQEC_SYSCFG_MAX_NUM_BYES);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_BYE_DELAY:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_BYE_DELAY) &&
                (temp_int <= VQEC_SYSCFG_MAX_BYE_DELAY)) {
                cfg->bye_delay = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "bye_delay",
                             temp_int,
                             VQEC_SYSCFG_MIN_BYE_DELAY,
                             VQEC_SYSCFG_MAX_BYE_DELAY);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;            
        case VQEC_CFG_REORDER_DELAY_ABS:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_REORDER_DELAY_ABS) &&
                (temp_int <= VQEC_SYSCFG_MAX_REORDER_DELAY_ABS)) {
                cfg->reorder_delay_abs = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "reorder_delay_abs",
                             temp_int,
                             VQEC_SYSCFG_MIN_REORDER_DELAY_ABS,
                             VQEC_SYSCFG_MAX_REORDER_DELAY_ABS);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_REORDER_DELAY:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_REORDER_DELAY) &&
                (temp_int <= VQEC_SYSCFG_MAX_REORDER_DELAY)) {
                cfg->d_reorder_delay = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "reorder_delay",
                             temp_int,
                             VQEC_SYSCFG_MIN_REORDER_DELAY,
                             VQEC_SYSCFG_MAX_REORDER_DELAY);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_VCDS_SERVER_IP:
            temp_str = vqec_config_setting_get_string(setting);
            if (temp_str && 
                inet_aton(temp_str, &temp_addr) &&
                (temp_addr.s_addr != -1)) {
                     cfg->vcds_server_ip = temp_addr.s_addr;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid vcds_server_ip = %s\n",
                             temp_str ? temp_str : "non-string");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_VCDS_SERVER_PORT:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_VCDS_SERVER_PORT) &&
                (temp_int <= VQEC_SYSCFG_MAX_VCDS_SERVER_PORT)) {
                cfg->vcds_server_port = htons(temp_int);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "vcds_server_port",
                             temp_int,
                             VQEC_SYSCFG_MIN_VCDS_SERVER_PORT,
                             VQEC_SYSCFG_MAX_VCDS_SERVER_PORT);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_TUNER_LIST:
            param_err = vqec_syscfg_read_tuner_list(cfg, setting,
                                                    log_nonfatal_messages);
            switch (param_err) {
                case VQEC_OK:
                    /* silent success */
                    break;
                case VQEC_ERR_MAXLIMITTUNER:
                    if (log_nonfatal_messages) {
                        snprintf(debug_str, DEBUG_STR_LEN,
                                 "Too many tuners in tuner_list! "
                                 "Ignoring tuners beyond max_tuners\n");
                        syslog_print(VQEC_SYSCFG_PARAM_IGNORED, debug_str);
                    }
                    break;
                case VQEC_ERR_PARAMRANGEINVALID:
                    /* error printed in read_tuner_list() */
                    break;
                default:
                    if (log_nonfatal_messages) {
                        snprintf(debug_str, DEBUG_STR_LEN,
                                 "Unknown error reading tuner_list!\n");
                        syslog_print(VQEC_SYSCFG_PARAM_IGNORED, debug_str);
                       break;
                    }
            }
            break;
        case VQEC_CFG_RTCP_DSCP_VALUE:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_RTCP_DSCP_VALUE) &&
                (temp_int <= VQEC_SYSCFG_MAX_RTCP_DSCP_VALUE)) {
                cfg->rtcp_dscp_value = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "rtcp_dscp_value",
                             temp_int,
                             VQEC_SYSCFG_MIN_RTCP_DSCP_VALUE,
                             VQEC_SYSCFG_MAX_RTCP_DSCP_VALUE);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_CLI_IFNAME:
            temp_str = vqec_config_setting_get_string(setting);
            if (temp_str) {
                (void)strlcpy(cfg->cli_ifname,
                              temp_str, VQEC_MAX_NAME_LEN);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_string_value_fmt,
                             vqec_cfg_arr[i].param_name);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_DELIVER_PAKS_TO_USER:
            if (vqec_config_setting_type(setting) ==
                VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->deliver_paks_to_user =
                    vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "deliver_paks_to_user");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_SD:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_SD) &&
                (temp_int <= VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_SD)) {
                cfg->max_receive_bandwidth_sd = (uint32_t)temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "max_receive_bandwidth_sd",
                             temp_int,
                             VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_SD,
                             VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_SD);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_SD_RCC:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_SD_RCC) &&
                (temp_int <= VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_SD_RCC)) {
                cfg->max_receive_bandwidth_sd_rcc = (uint32_t)temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "max_receive_bandwidth_sd_rcc",
                             temp_int,
                             VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_SD_RCC,
                             VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_SD_RCC);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_HD:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_HD) &&
                (temp_int <= VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_HD)) {
                cfg->max_receive_bandwidth_hd = (uint32_t)temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "max_receive_bandwidth_hd",
                             temp_int,
                             VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_HD,
                             VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_HD);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_HD_RCC:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_HD_RCC) &&
                (temp_int <= VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_HD_RCC)) {
                cfg->max_receive_bandwidth_hd_rcc = (uint32_t)temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "max_receive_bandwidth_hd_rcc",
                             temp_int,
                             VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_HD_RCC,
                             VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_HD_RCC);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_MIN_HD_STREAM_BITRATE:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_MIN_HD_STREAM_BITRATE) &&
                (temp_int <= VQEC_SYSCFG_MAX_MIN_HD_STREAM_BITRATE)) {
                cfg->min_hd_stream_bitrate = (uint32_t)temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "min_hd_stream_bitrate",
                             temp_int,
                             VQEC_SYSCFG_MIN_MIN_HD_STREAM_BITRATE,
                             VQEC_SYSCFG_MAX_MIN_HD_STREAM_BITRATE);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_APP_DELAY:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_APP_DELAY) &&
                (temp_int <= VQEC_SYSCFG_MAX_APP_DELAY)) {
                cfg->app_delay = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "app_delay",
                             temp_int,
                             VQEC_SYSCFG_MIN_APP_DELAY,
                             VQEC_SYSCFG_MAX_APP_DELAY);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_MAX_FASTFILL:
            temp_int = vqec_config_setting_get_int(setting);
            if (is_vqec_cfg_max_fastfill_valid(temp_int)) {
                cfg->max_fastfill = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "max_fastfill",
                             temp_int,
                             VQEC_SYSCFG_MIN_MAX_FASTFILL,
                             VQEC_SYSCFG_MAX_MAX_FASTFILL);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_SRC_IP_FILTER_ENABLE:
            if (vqec_config_setting_type(setting) ==
                VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->src_ip_filter_enable =
                    vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "src_ip_filter_enable");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_QOE_ENABLE:
            if (vqec_config_setting_type(setting) ==
                VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->qoe_enable = vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "qoe_enable");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_RCC_MAX_CONCURRENT:
            temp_int = vqec_config_setting_get_int(setting);
            if ((temp_int >= VQEC_SYSCFG_MIN_RCC_MAX_CONCURRENT) &&
                (temp_int <= VQEC_SYSCFG_MAX_RCC_MAX_CONCURRENT)) {
                cfg->rcc_max_concurrent = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "rcc_max_concurrent",
                             temp_int,
                             VQEC_SYSCFG_MIN_RCC_MAX_CONCURRENT,
                             VQEC_SYSCFG_MAX_RCC_MAX_CONCURRENT);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_INTEGRATED_RTP_FALLBACK:
            if (vqec_config_setting_type(setting) == VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->integrated_rtp_fallback = vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "integrated_rtp_fallback");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_UDP_PASSTHRU_SUPPORT:
            if (vqec_config_setting_type(setting) == VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->udp_passthru_support = vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "udp_passthru_support");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;
        case VQEC_CFG_VOD_CMD_TIMEOUT:
            temp_int = vqec_config_setting_get_int(setting);
            if (is_vqec_cfg_vod_cmd_timeout_valid(temp_int)) {
                cfg->vod_cmd_timeout = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "vod_cmd_timeout",
                             temp_int,
                             VQEC_SYSCFG_MIN_VOD_CMD_TIMEOUT,
                             VQEC_SYSCFG_MAX_VOD_CMD_TIMEOUT);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;

        case VQEC_CFG_VOD_MAX_SESSIONS:
            temp_int = vqec_config_setting_get_int(setting);
            if (is_vqec_cfg_vod_max_sessions_valid(temp_int)) {
                cfg->vod_max_sessions = temp_int;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             vqec_inv_int_range_fmt,
                             "vod_max_sessions",
                             temp_int,
                             VQEC_SYSCFG_MIN_VOD_MAX_SESSIONS,
                             VQEC_SYSCFG_MAX_VOD_MAX_SESSIONS);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;

        case VQEC_CFG_VOD_MODE:
            temp_str = vqec_config_setting_get_string(setting);
            if (temp_str) {
                strtolower(t_str, VQEC_MAX_NAME_LEN, temp_str);
            }
            if (temp_str && !strcmp(t_str, VQEC_SYSCFG_VOD_MODE_IPTV)) {
                cfg->vod_mode = VQEC_VOD_MODE_IPTV;
            } else if (temp_str && !strcmp(t_str,VQEC_SYSCFG_VOD_MODE_CABLE)) {
                cfg->vod_mode = VQEC_VOD_MODE_CABLE;
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "Invalid vod_mode value (%s) "
                             "Valid values [%s,%s]\n",
                             temp_str ? temp_str : "non-string",
                             VQEC_SYSCFG_VOD_MODE_IPTV,
                             VQEC_SYSCFG_VOD_MODE_CABLE);
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;

        case VQEC_CFG_STUN_OPTIMIZATION:
            if (vqec_config_setting_type(setting) == VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
                cfg->stun_optimization = vqec_config_setting_get_bool(setting);
            } else {
                if (log_nonfatal_messages) {
                    snprintf(debug_str, DEBUG_STR_LEN,
                             "invalid boolean value for \"%s\"",
                             "stun_optimization");
                    syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
                }
                param_err = VQEC_ERR_PARAMRANGEINVALID;
            }
            break;

        case VQEC_CFG_MUST_BE_LAST:
            param_err = VQEC_ERR_PARAMRANGEINVALID;
            break;
        }
        /*
         * Record parameters which were assigned valid values,
         * if the caller so requests.
         */
        if (cfg_present && (param_err == VQEC_OK)) {
            cfg_present[i] = TRUE;
        }
        /*
         * If a problem was encountered in this parameter,
         * reflect it in the return value.
         */
        if ((ret == VQEC_OK) && (param_err != VQEC_OK)) {
            ret = VQEC_ERR_PARAMRANGEINVALID;
        }
    }
    
done:
    /* Free the configuration */
    vqec_config_destroy(&vqec_cfg);
    if ((ret == VQEC_ERR_PARAMRANGEINVALID) 
        && log_nonfatal_messages && filename) {
        snprintf(debug_str, DEBUG_STR_LEN, 
                 "One or more invalid config parameter assignments in file "
                 "[%s] were ignored.", filename);
        syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
    }
    return ret;
}


/*
 * Reads the checksum of the specified config from the index file.
 * If no checksum is available for the specified config within the index,
 * an invalid checksum string of VQEC_SYSCFG_CHECKSUM_UNAVAILABLE will 
 * be returned.
 *
 * @param[in] config        - configuration file whose checksum is to be read
 * @param[out] checksum     - retrieved checksum
 */
void
vqec_syscfg_index_read (vqec_ifclient_config_t config,
                        char checksum[MD5_CHECKSUM_STRLEN])
{
    FILE *fp = NULL;
#define MAX_LINE_LENGTH 80
    char buffer_line[MAX_LINE_LENGTH];
    char *curr_name, *curr_checksum, *saveptr = NULL;

    VQEC_ASSERT(checksum);
    strncpy(checksum, VQEC_SYSCFG_CHECKSUM_UNAVAILABLE, MD5_CHECKSUM_STRLEN);

    fp = fopen(s_cfg.index_cfg_pathname, "r");
    if (fp == NULL) {
        goto done;
    }

    while (fgets(buffer_line, MAX_LINE_LENGTH, fp) != NULL) {        
        
        /* Parse the line's config name and checksum */
        curr_name = strtok_r(buffer_line, " ", &saveptr);
        curr_checksum = strtok_r(NULL, " \r\n", &saveptr);
         
        /* Ignore the line if it has unrecognized syntax */
        if (!curr_name || !curr_checksum ||
            (strlen(curr_checksum) != (MD5_CHECKSUM_STRLEN - 1))) {
            continue;
        }

        /* Terminate if the desired checksum is found */
        if (!strcmp(curr_name, vqec_syscfg_index_names[config])) {
            strncpy(checksum, curr_checksum, MD5_CHECKSUM_STRLEN);
            break;
        }
    }

    fclose(fp);

done:
    return;
}


/*
 * Updates the index file with the given (config, checksum) values
 * supplied.
 *
 * A checksum value of VQEC_SYSCFG_CHECKSUM_UNAVAILABLE
 * may be supplied to indicate that a config is not present,
 * in which case its entry will be removed from the index file
 * (and the index file deleted, if empty).
 *
 * @param[in]  config       - configuration to be updated
 * @param[in]  checksum     - configuration's checksum
 * @param[out] vqec_error_t - VQEC_OK, or 
 *                            error code indicating failure reason
 */
vqec_error_t
vqec_syscfg_index_write (vqec_ifclient_config_t config,
                         char checksum[MD5_CHECKSUM_STRLEN])
{
    FILE *fp = NULL;
#define MAX_LINE_LENGTH 80
    char buffer_line[MAX_LINE_LENGTH];
    char *curr_name, *curr_checksum, *saveptr = NULL;
    vqec_error_t status = VQEC_OK;
    char debug_str[DEBUG_STR_LEN];
    int buffer_line_len, found_line_len = 0, err;
    long new_length;
    boolean remove_config;

    /* Detect case in which config is to be removed */
    remove_config = checksum && !strncmp(checksum, 
                                         VQEC_SYSCFG_CHECKSUM_UNAVAILABLE,
                                         MD5_CHECKSUM_STRLEN);
    /*
     * Abort if validation of input fails:
     *   1. 'config' is outside of supported range
     *   2. 'checksum' is not supplied, or 
     *       doesn't match expected length
     *   3. no index pathname is configured
     */
    if ((config < 0 || config >= VQEC_IFCLIENT_CONFIG_MAX) ||
        (!checksum || (!remove_config && 
                       (strlen(checksum) != (MD5_CHECKSUM_STRLEN - 1)))) ||
        !s_cfg.index_cfg_pathname[0]) {
        status = VQEC_ERR_INTERNAL;
        goto done;
    }
    
    /*
     * Open index file, creating one if necessary.
     */
    fp = fopen(s_cfg.index_cfg_pathname, "r+");
    if (!fp && remove_config) {
        goto done;
    }
    if (!fp && (errno == ENOENT)) {
        fp = fopen(s_cfg.index_cfg_pathname, "w");
    }
    if (!fp) {
        snprintf(debug_str, DEBUG_STR_LEN, 
                 "Error opening index file %s (errno = %d)",
                 s_cfg.index_cfg_pathname, errno);
        status = VQEC_ERR_INTERNAL;
        goto done;
    }

    /*
     * Expected index file contents are zero or more lines in the form:
     * "<Config Name> <Checksum>\n"
     * with each line corresponding to a persisted configuration.
     * 
     * For example:
     *   Override b2abbf915e247322b540d1d7ee05ed03
     *   Network 9a2c8da781b03c65863b6eed2d2cdc0d
     *
     * Search for the configuration's existing entry in the file:
     *   o If found, 
     *       - the file pointer will be positioned at the byte 
     *         following the configuration entry's line, and
     *       - found_line_len will store the number of bytes in
     *         the configuration entry's line
     *   o If not found
     *       - the file pointer will be positioned at the byte
     *         following the last byte of the file, and
     *       - found_line_len will be 0
     */
    while (fgets(buffer_line, MAX_LINE_LENGTH, fp)) {
        
        /* Record length of buffer_line before strtok_r() overwrites it */
        buffer_line_len = strlen(buffer_line);

        /* Parse the line's config name and checksum */
        curr_name = strtok_r(buffer_line, " ", &saveptr);
        curr_checksum = strtok_r(NULL, " \r\n", &saveptr);
         
        /* Ignore the line if it has unrecognized syntax */
        if (!curr_name || !curr_checksum ||
            (strlen(curr_checksum) != (MD5_CHECKSUM_STRLEN - 1))) {
            continue;
        }

        /* If found, record the length of the configuration entry's line */
        if (!strcmp(curr_name, vqec_syscfg_index_names[config])) {
            found_line_len = buffer_line_len;
            break;
        }

    }
    
    /*
     * Ensure that the configuration entry is removed or updated,
     * as per the caller's request.
     */
    if (remove_config) {

        if (!found_line_len) {
            /* Configuration is already not listed in index file */
            goto done;
        }
        /* 
         * Copy the remaining portion of the file (if any) backwards
         * starting at the found entry in the file, one line at a time.
         */
        while (fgets(buffer_line, MAX_LINE_LENGTH, fp)) {
            buffer_line_len = strlen(buffer_line);
            err = fseek(fp, -(buffer_line_len + found_line_len), SEEK_CUR);
            if (err == -1) {
                snprintf(debug_str, DEBUG_STR_LEN, 
                         "Error seeking within index file %s offset %d "
                         "from %ld (errno = %d)",
                         s_cfg.index_cfg_pathname,
                         -(buffer_line_len + found_line_len),
                         ftell(fp), errno);
                status = VQEC_ERR_INTERNAL;           
                goto done;
            }
            /* Note:  newline ('\n') is already contained in buffer_line */
            (void)fprintf(fp, "%s", buffer_line);
            err = fseek(fp, found_line_len, SEEK_CUR);
            if (err == -1) {
                snprintf(debug_str, DEBUG_STR_LEN, 
                         "Error seeking within index file %s offset %d "
                         "from %ld (errno = %d)",
                         s_cfg.index_cfg_pathname,
                         found_line_len,
                         ftell(fp), errno);
                status = VQEC_ERR_INTERNAL;           
                goto done;
            }
        }
        /*
         * Back up to where the new index file contents end,
         * and determine the new file's length.
         */
        err = fseek(fp, -found_line_len, SEEK_CUR);
        if (err == -1) {
            snprintf(debug_str, DEBUG_STR_LEN, 
                     "Error seeking within index file %s offset %d "
                     "from %ld (errno = %d)",
                     s_cfg.index_cfg_pathname,
                     -found_line_len, ftell(fp), errno);
            status = VQEC_ERR_INTERNAL;           
            goto done;
        }        
        new_length = ftell(fp);
        if (new_length == -1) {
            snprintf(debug_str, DEBUG_STR_LEN, 
                     "Error determining new index file %s length (errno = %d)",
                     s_cfg.index_cfg_pathname, errno);
            status = VQEC_ERR_INTERNAL;           
            goto done;
        }
        /* 
         * Truncate the file or remove it completely,
         * based on its new length.
         */
        if (new_length) {
            err = ftruncate(fileno(fp), new_length);
            if (err == -1) {
                snprintf(debug_str, DEBUG_STR_LEN, 
                         "Error determining new index file %s length "
                         "(errno = %d)",
                         s_cfg.index_cfg_pathname, errno);
                status = VQEC_ERR_INTERNAL;           
                goto done;
            }
        } else {
            fclose(fp);
            fp = NULL;
            err = remove(s_cfg.index_cfg_pathname);
            if (err == -1) {
                snprintf(debug_str, DEBUG_STR_LEN, 
                         "Error removing unnecessary index file %s "
                         "(errno = %d)",
                         s_cfg.index_cfg_pathname, errno);
                status = VQEC_ERR_INTERNAL;           
                goto done;   
            }
        }
        
    } else {
        
        /* Back the file pointer up (if needed) */
        err = fseek(fp, -found_line_len, SEEK_CUR);            
        if (err) {
            snprintf(debug_str, DEBUG_STR_LEN, 
                     "Error seeking within index file %s offset %d from %ld"
                     "(errno = %d)",
                     s_cfg.index_cfg_pathname, -found_line_len,
                     ftell(fp), errno);
            status = VQEC_ERR_INTERNAL;           
            goto done;
        }
        
        /* Write the configuration's new entry to the file */
        (void)fprintf(fp, "%s %s\n", 
                      vqec_syscfg_index_names[config], checksum);
        
    }

done:
    if (fp) {
        fclose(fp);
    }
    if (status != VQEC_OK) {
        syslog_print(VQEC_SYSCFG_ERROR, debug_str);
    }
    return (status);
}

boolean
vqec_syscfg_checksum_is_valid (vqec_ifclient_config_t config)
{
    char index_checksum[MD5_CHECKSUM_STRLEN],
        file_checksum[MD5_CHECKSUM_STRLEN];
    boolean status = TRUE;

    if (!s_cfg.index_cfg_pathname[0]) {
        /* If an index is not used, file is always considered valid */
        goto done;
    }

    strncpy(file_checksum, VQEC_SYSCFG_CHECKSUM_UNAVAILABLE, 
            MD5_CHECKSUM_STRLEN);
    (void)vqe_MD5ComputeChecksumStr(vqec_syscfg_get_filename(config), TRUE, 
                                    file_checksum);
    vqec_syscfg_index_read(config, index_checksum);
    status = !strncmp(index_checksum, file_checksum, MD5_CHECKSUM_STRLEN) ?
        TRUE : FALSE;

done:
    return (status);
}


/*
 * Verifies the checksum of a configuration file, then (if valid) loads it
 * into the cfg structure supplied by the caller.
 *
 * @param[in]      config        - identifies configuration to load and verify
 * @param[in/out]  cfg           - [optional] structure which is loaded with 
 *                                  the valid parameters values assigned in
 *                                  the config file.  Caller must initialize.
 * @param[in/out]  cfg_present   - [optional] array (of size
 *                                  VQEC_CFG_MUST_BE_LAST) which records
 *                                  (as TRUE) parameters which were present
 *                                  and assigned valid values in the
 *                                  configuration file. Caller must initialize.
 * @param[out]     vqec_error_t  - Return code
 *                                    All return codes from vqec_syscfg_load()
 *                                    plus the following additional one:
 *                                    VQEC_ERR_CONFIG_CHECKSUM_VERIFICATION
 *                                       config file not loaded due to
 *                                       checksum verification error
 */
vqec_error_t
vqec_syscfg_verify_and_load (
    vqec_ifclient_config_t config,
    vqec_syscfg_t *cfg,
    boolean cfg_present[VQEC_CFG_MUST_BE_LAST])

{
    vqec_error_t status = VQEC_OK;
    boolean *cfg_supported;
    vqec_ifclient_config_event_params_t config_event_params;
    char debug_str[DEBUG_STR_LEN];

    switch(config) {
    case VQEC_IFCLIENT_CONFIG_NETWORK:
        cfg_supported = vqec_cfg_attribute_support;
        break;
    case VQEC_IFCLIENT_CONFIG_OVERRIDE:
        cfg_supported = vqec_cfg_override_support;
        break;
    default:
        status = VQEC_ERR_INTERNAL;
        goto done;
    }

    if (!vqec_syscfg_checksum_is_valid(config)) {

        if (vqec_syscfg_callback[config]) {
            config_event_params.event = 
                VQEC_IFCLIENT_CONFIG_EVENT_CONFIG_INVALID;
            config_event_params.config = config;
            (*vqec_syscfg_callback[config])(&config_event_params);
        }
        snprintf(debug_str, DEBUG_STR_LEN, 
                 "%s Configuration checksum verification failed, ignoring "
                 "configuration file.", vqec_syscfg_index_names[config]);
        syslog_print(VQEC_SYSCFG_ERROR, debug_str);
        status = VQEC_ERR_CONFIG_CHECKSUM_VERIFICATION;
        goto done;
    }

    status = vqec_syscfg_load(cfg, cfg_supported, cfg_present,
                              vqec_syscfg_get_filename(config), NULL, TRUE);

done:
    return (status);
}

/* defines configurations supported by vqec_syscfg_dump_from_file() */
typedef enum vqec_syscfg_dump_ {
    VQEC_SYSCFG_DUMP_STARTUP,
    VQEC_SYSCFG_DUMP_NETWORK,
    VQEC_SYSCFG_DUMP_OVERRIDE
} vqec_syscfg_dump_t;
    
/*
 * Dumps a configuration file to the console.
 *
 * @param[in]  filename      - file containing configuration to dump
 * @param[in]  cfg_supported - [optional] array (of size 
 *                                  VQEC_CFG_MUST_BE_LAST) which limits
 *                                  configuration parameters dumped by
 *                                  this API to the ones marked TRUE
 * @param[in]  descr         - string describing configuration being displayed
 * @param[in]  dump_config   - identifies configuration to be displayed
 */
void
vqec_syscfg_dump_from_file (const char *filename,
                            const boolean cfg_supported[VQEC_CFG_MUST_BE_LAST],
                            const char *descr,
                            const vqec_syscfg_dump_t dump_config)

{
    vqec_ifclient_config_status_params_t params;
    boolean cfg_present[VQEC_CFG_MUST_BE_LAST];
    vqec_syscfg_t cfg;
    vqec_error_t err;
    vqec_ifclient_config_t config = VQEC_IFCLIENT_CONFIG_MAX;

    /* Validate requested configuration type */ 
    switch (dump_config) {
    case VQEC_SYSCFG_DUMP_STARTUP:
        break;
    case VQEC_SYSCFG_DUMP_NETWORK:
        config = VQEC_IFCLIENT_CONFIG_NETWORK;
        break;
    case VQEC_SYSCFG_DUMP_OVERRIDE:
        config = VQEC_IFCLIENT_CONFIG_OVERRIDE;
        break;
    default:
        goto done;
    }

    /* Print a message if the configuration file is not specified */
    if (!filename[0]) {
        CONSOLE_PRINTF("No %s Config file specified for use by VQE-C.\n",
                       descr);
        goto done;
    }

    /* Load the configuration file's contents */
    vqec_syscfg_set_defaults(&cfg, cfg_present);
    if (dump_config == VQEC_SYSCFG_DUMP_STARTUP) {
        err = vqec_syscfg_load(&cfg, NULL, cfg_present, filename, NULL, TRUE);
    } else {
        err = vqec_syscfg_verify_and_load(config, &cfg, cfg_present);
    }
    if (!((err == VQEC_OK) || (err == VQEC_ERR_PARAMRANGEINVALID))) {
        if (err == VQEC_ERR_CONFIG_NOT_FOUND) {
            CONSOLE_PRINTF("\n%s\n", vqec_err2str(err));
        } else {
            CONSOLE_PRINTF("\nError loading file - %s\n", vqec_err2str(err));
        }
        goto done;
    }

    /* Display properties for updatable configuration files */
    if (dump_config != VQEC_SYSCFG_DUMP_STARTUP) {
        params.config = config;
        (void)vqec_ifclient_config_status(&params);
        CONSOLE_PRINTF("Last update received:       %s",
                       params.last_update_timestamp);
        CONSOLE_PRINTF("%s file checksum: %*s%s", descr,
                       12 - strlen(descr), "", params.md5);
    }

    /* Display configuration file contents */
    CONSOLE_PRINTF("\n%s configuration file:  %s\n", descr, filename);
    vqec_syscfg_dump(&cfg, cfg_present);

done:
    return;
}

/*
 * Dumps the start-up configuration to the console.
 *
 * I.e., reads the start-up configuration file located at the pathname
 * supplied to VQE-C at initialization, parses/validates its contents,
 * and prints any parameters which are set to valid values.
 * Parameters which are not set in the system configuration file
 * will not be displayed.
 *
 * Note that the values displayed represent the *current* contents of
 * the start-up configuration file configuration file (which may be, 
 * but are not necessarily, the contents of the file at the time VQE-C
 * was initialized using the file).
 *
 * If the start-up configuration file can not be displayed (e.g. parse
 * error, or the file was deleted following initialization), then an
 * error message will be printed.
 */
void
vqec_syscfg_dump_startup (void)
{
    vqec_syscfg_dump_from_file(s_vqec_syscfg_filename, NULL,
                               "Start-up", VQEC_SYSCFG_DUMP_STARTUP);
}

/*
 * Dumps the attribute configuration to the console.
 * 
 * Usage considerations are very similar to vqec_syscfg_dump_system(), 
 * but with respect to the attribute configuration file.
 */
void
vqec_syscfg_dump_attributes (void)
{
    /* Print the parameter values */
    vqec_syscfg_dump_from_file(s_cfg.network_cfg_pathname,
                               vqec_cfg_attribute_support,
                               "Network", VQEC_SYSCFG_DUMP_NETWORK);
}

/*
 * Dumps the override configuration to the console.
 */
void
vqec_syscfg_dump_overrides (void)
{
    /* Print the override values */
    vqec_syscfg_dump_from_file(s_cfg.override_cfg_pathname,
                               vqec_cfg_override_support,
                               "Override", VQEC_SYSCFG_DUMP_OVERRIDE);
}

/*
 * Dumps the system configuration to the console.
 */
void
vqec_syscfg_dump_system (void)
{
    vqec_syscfg_t cfg;

    vqec_syscfg_get(&cfg);
    vqec_syscfg_dump(&cfg, vqec_cfg_is_status_current);
}

/*
 * Dumps the default configuration to the console.
 */
void
vqec_syscfg_dump_defaults (void)
{
    vqec_syscfg_t cfg;

    vqec_syscfg_set_defaults(&cfg, NULL);
    vqec_syscfg_dump(&cfg, vqec_cfg_is_status_current);
}

/*
 * Reads the system configuration from persistent storage,
 * by merging all sources of system configuration
 * (defaults, startup, network, and override).
 *
 * @param[out] cfg  - returns the system configuration, derived from
 *                    reading and merging the persisted configuration
 *                    contributing files
 */
void
vqec_syscfg_read (vqec_syscfg_t *cfg)
{
    uint32_t repair_trigger_point_abs, reorder_delay_abs;
    char debug_str[DEBUG_STR_LEN];
    boolean cfg_present[VQEC_CFG_MUST_BE_LAST];
    vqec_cfg_enum_t i;

    /*
     * Merge the configurations into the caller's structure.
     * Configuration sources are read in order of lowest priority
     * to highest, with higher priority configuration settings
     * overwriting those of lower priority configuration settings.
     */
    vqec_syscfg_set_defaults(cfg, cfg_present);
    vqec_syscfg_load(cfg, NULL, cfg_present, s_vqec_syscfg_filename,
                     NULL, TRUE);
    vqec_syscfg_verify_and_load(VQEC_IFCLIENT_CONFIG_NETWORK, cfg,
                                cfg_present);
    vqec_syscfg_verify_and_load(VQEC_IFCLIENT_CONFIG_OVERRIDE, cfg,
                                cfg_present);

    /*
     * Handle the presence of obsoleted parameters by logging a
     * warning message (saying obsoleted parameter values are ignored).
     */
    for (i=VQEC_CFG_MAX_TUNERS; i < VQEC_CFG_MUST_BE_LAST; i++) {
        if (cfg_present[i] &&
            (vqec_cfg_status[i] == VQEC_PARAM_STATUS_OBSOLETE)) {
            snprintf(debug_str, DEBUG_STR_LEN,
                     vqec_param_obsoleted_fmt,
                     vqec_cfg_arr[i].param_name);
            syslog_print(VQEC_SYSCFG_PARAM_IGNORED, debug_str);
        }
    }

    /*
     * Handle presence of deprecated parameters:
     *   - if current and deprecated forms of a parameter are both present,
     *     the current is used and a syslog is issued to log the conflict
     *   - if only the deprecated form of a parameter is supplied, 
     *     the current form will be set based on it, and a syslog
     *     issued indicating that the parameter is being deprecated
     */
     
    /*
     *   - vqec_enable
     *        the old/deprecated parameter for enabling VQE-C services
     *   - qoe_enable
     *        the current parameter for enabling VQE-C services
     */
    if (cfg_present[VQEC_CFG_VQEC_ENABLE] &&
        cfg_present[VQEC_CFG_QOE_ENABLE]) {
        /* both forms supplied -- current will be used, deprecated ignored */
        snprintf(debug_str, DEBUG_STR_LEN,
                 vqec_param_deprecated_conflict_fmt,
                 vqec_cfg_arr[VQEC_CFG_VQEC_ENABLE].param_name,
                 vqec_cfg_arr[VQEC_CFG_QOE_ENABLE].param_name);
        syslog_print(VQEC_SYSCFG_PARAM_IGNORED, debug_str);
    } else if (cfg_present[VQEC_CFG_VQEC_ENABLE]) {
        /* 
         * just deprecated parameter supplied -- 
         * Issue a warning about the parameter being deprecated,
         * and assign the current parameter from it.
         */
        snprintf(debug_str, DEBUG_STR_LEN,
                 vqec_param_deprecated_accepted_fmt,
                 vqec_cfg_arr[VQEC_CFG_VQEC_ENABLE].param_name,
                 vqec_cfg_arr[VQEC_CFG_QOE_ENABLE].param_name);
        syslog_print(VQEC_SYSCFG_PARAM_DEPRECATED, debug_str);
        cfg->qoe_enable = cfg->d_vqec_enable;
    }

    /*
     *   - repair_trigger_point
     *        the old/deprecated way of configuring the repair trigger point,
     *        as a relative percentage of the jitter_buff_size
     *   - repair_trigger_point_abs
     *        the current way of configuring the repair trigger point,
     *        as an absolute value of time
     */
    if (cfg_present[VQEC_CFG_REPAIR_TRIGGER_POINT_ABS] &&
        cfg_present[VQEC_CFG_REPAIR_TRIGGER_POINT]) {
        /* both forms supplied -- current will be used, deprecated ignored */
        snprintf(debug_str, DEBUG_STR_LEN,
                 vqec_param_deprecated_conflict_fmt,
                 vqec_cfg_arr[VQEC_CFG_REPAIR_TRIGGER_POINT].param_name,
                 vqec_cfg_arr[VQEC_CFG_REPAIR_TRIGGER_POINT_ABS].param_name);
        syslog_print(VQEC_SYSCFG_PARAM_IGNORED, debug_str);
    } else if (cfg_present[VQEC_CFG_REPAIR_TRIGGER_POINT]) {
        /*
         * just relative form supplied-- 
         * need to convert it to abs and validate it
         */
        repair_trigger_point_abs = cfg->d_repair_trigger_point *
            cfg->jitter_buff_size / 100;
        if (repair_trigger_point_abs >
            VQEC_SYSCFG_MAX_REPAIR_TRIGGER_POINT_ABS) {
            /* invalid, use default _abs form */
            snprintf(debug_str, DEBUG_STR_LEN,
                     "\"repair_trigger_point\" value of %dms exceeds limit "
                     "of %dms, setting ignored\n",
                     repair_trigger_point_abs,
                     VQEC_SYSCFG_MAX_REPAIR_TRIGGER_POINT_ABS);
            syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
            cfg->repair_trigger_point_abs =
                VQEC_SYSCFG_DEFAULT_REPAIR_TRIGGER_POINT_ABS;
        } else {
            /* valid, issue warning that parameter is deprecated */
            snprintf(debug_str, DEBUG_STR_LEN,
                     vqec_param_deprecated_accepted_fmt,
                     vqec_cfg_arr[VQEC_CFG_REPAIR_TRIGGER_POINT].param_name,
                     vqec_cfg_arr[
                         VQEC_CFG_REPAIR_TRIGGER_POINT_ABS].param_name);
            syslog_print(VQEC_SYSCFG_PARAM_DEPRECATED, debug_str);
            cfg->repair_trigger_point_abs = repair_trigger_point_abs;
        }
    }

    /*
     *   - reorder_delay
     *        the old/deprecated way of configuring the reorder delay,
     *        as a relative percentage of the jitter_buff_size
     *   - reorder_delay_abs
     *        the current way of configuring the reorder delay
     *        as an absolute value of time
     */
    if (cfg_present[VQEC_CFG_REORDER_DELAY_ABS] &&
        cfg_present[VQEC_CFG_REORDER_DELAY]) {
        /* both forms supplied -- current will be used, deprecated ignored */
        snprintf(debug_str, DEBUG_STR_LEN,
                 vqec_param_deprecated_conflict_fmt,
                 vqec_cfg_arr[VQEC_CFG_REORDER_DELAY].param_name,
                 vqec_cfg_arr[VQEC_CFG_REORDER_DELAY_ABS].param_name);
        syslog_print(VQEC_SYSCFG_PARAM_IGNORED, debug_str);
    } else if (cfg_present[VQEC_CFG_REORDER_DELAY]) {
        /*
         * just relative form supplied-- 
         * need to convert it to abs and validate it
         */
        reorder_delay_abs = cfg->d_reorder_delay * cfg->jitter_buff_size / 100;
        if (reorder_delay_abs > VQEC_SYSCFG_MAX_REORDER_DELAY_ABS) {
            /* invalid, use default _abs form */
            snprintf(debug_str, DEBUG_STR_LEN,
                     "\"reorder_delay\" value of %dms exceeds limit "
                     "of %dms, setting ignored\n",
                     reorder_delay_abs,
                     VQEC_SYSCFG_MAX_REORDER_DELAY_ABS);
            syslog_print(VQEC_SYSCFG_PARAM_INVALID, debug_str);
            cfg->reorder_delay_abs = VQEC_SYSCFG_DEFAULT_REORDER_DELAY_ABS;
        } else {
            /* valid, issue warning that parameter is deprecated */
            snprintf(debug_str, DEBUG_STR_LEN,
                     vqec_param_deprecated_accepted_fmt,
                     vqec_cfg_arr[VQEC_CFG_REORDER_DELAY].param_name,
                     vqec_cfg_arr[VQEC_CFG_REORDER_DELAY_ABS].param_name);
            syslog_print(VQEC_SYSCFG_PARAM_DEPRECATED, debug_str);
            cfg->reorder_delay_abs = reorder_delay_abs;
        }
    }

}

/*
 * Returns a pointer to the system configuration.
 */
const vqec_syscfg_t *
vqec_syscfg_get_ptr (void)
{
    return (&s_cfg);
}

/*
 * Copies the system configuration into the caller's cfg structure.
 *
 * @param[out] cfg   - structure into which the system config is copied
 *                     MUST be non-NULL.
 */
void 
vqec_syscfg_get (vqec_syscfg_t *cfg)
{
    if (!cfg) {
        return;
    }
    memcpy(cfg, &s_cfg, sizeof(vqec_syscfg_t));
}

/*
 * Updates the system configuration from the caller's cfg structure.
 *
 * May be called in two contexts:
 *   during initialization:
 *     - all parameters which could be present in network or override
 *       configurations are assigned
 *   during post-initialization configuration updates:
 *     - all parameters which are dynamically updatable
 *       (without a restart) are assigned
 *
 * @param[in] cfg    - structure containing new system configuration
 *                     MUST be non-NULL.
 * @param[in] update - indicates whether new config represents an
 *                      update to the system config (TRUE)
 *                      or is being set during initialization (FALSE)
 */
void
vqec_syscfg_set (vqec_syscfg_t *cfg, boolean update)
{
    vqec_cfg_enum_t cfg_id;
    uint8_t log_level_old, log_level_new;
    char log_str[VQEC_LOGMSG_BUFSIZE];

    if (!cfg) {
        return;
    }
    
    /* 
     * Walk through all parameters, enforcing the update on a parameter
     * by parameter basis.
     */
    for (cfg_id=VQEC_CFG_MAX_TUNERS; cfg_id < VQEC_CFG_MUST_BE_LAST; 
	 cfg_id++) {

        /*
         * Avoid updating a parameter if
         * 1. it does not support updates, or
         * 2. the configuration is being updated after initialization
         *    and the parameter does not support such an update
         */
        if ((vqec_cfg_update_type[cfg_id] == VQEC_UPDATE_INVALID) ||
            (update && 
             (vqec_cfg_update_type[cfg_id] == VQEC_UPDATE_STARTUP))) {
            continue;
        }

        /*
         * Assign the configuration parameter the value supplied by the caller.
         *
         * All parameters are handled in the case below, with the
         * vqec_cfg_update_type[] array determining what parameters
         * may be updated.
         *
         * Updates of type VQEC_UPDATE_NEWCHANCHG generally require just
         * a copy of the new value into the system configuration field.
         * The new value will be applied to a channel upon it next becoming
         * active.
         *
         * Updates of type VQEC_UPDATE_IMMEDIATE generally also require
         * additional action that updates the value currently in use.
         */
        switch(cfg_id) {
        case VQEC_CFG_MAX_TUNERS:
            s_cfg.max_tuners = cfg->max_tuners;
            break;
        case VQEC_CFG_CHANNEL_LINEUP:
            (void)strlcpy(s_cfg.channel_lineup, cfg->channel_lineup,
                          VQEC_MAX_NAME_LEN);
            break;
        case VQEC_CFG_NETWORK_CFG_PATHNAME:
            (void)strlcpy(s_cfg.network_cfg_pathname,
                          cfg->network_cfg_pathname,
                          VQEC_MAX_NAME_LEN);
            break;
        case VQEC_CFG_OVERRIDE_CFG_PATHNAME:
            (void)strlcpy(s_cfg.override_cfg_pathname,
                          cfg->override_cfg_pathname,
                          VQEC_MAX_NAME_LEN);
            break;
        case VQEC_CFG_INDEX_CFG_PATHNAME:
            (void)strlcpy(s_cfg.index_cfg_pathname,
                          cfg->index_cfg_pathname,
                          VQEC_MAX_NAME_LEN);
            break;
        case VQEC_CFG_VQEC_ENABLE:
            break;
        case VQEC_CFG_JITTER_BUFF_SIZE:
            s_cfg.jitter_buff_size = cfg->jitter_buff_size;
            break;
        case VQEC_CFG_REPAIR_TRIGGER_POINT_ABS:
            s_cfg.repair_trigger_point_abs = cfg->repair_trigger_point_abs;
            break;
        case VQEC_CFG_REPAIR_TRIGGER_POINT:
            /*
             * This parameter has been deprecated.
             * If the user has not supplied a value, the configured or default
             * value for VQEC_CFG_REPAIR_TRIGGER_POINT_ABS will apply.
             */
            break;
        case VQEC_CFG_PAKPOOL_SIZE:
            s_cfg.pakpool_size = cfg->pakpool_size;
            break;
        case VQEC_CFG_SO_RCVBUF:
            s_cfg.so_rcvbuf = cfg->so_rcvbuf;
            break;
        case VQEC_CFG_STRIP_RTP:
            s_cfg.strip_rtp = cfg->strip_rtp;
            break;
        case VQEC_CFG_INPUT_IFNAME:
            (void)strlcpy(s_cfg.input_ifname,
                          cfg->input_ifname, VQEC_MAX_NAME_LEN); 
            break;
        case VQEC_CFG_SIG_MODE:
            s_cfg.sig_mode = cfg->sig_mode;
            break;
        case VQEC_CFG_NAT_BINDING_REFRESH_INTERVAL:
            s_cfg.nat_binding_refresh_interval =
                cfg->nat_binding_refresh_interval;
            break;
        case VQEC_CFG_NAT_FILTER_REFRESH_INTERVAL:
            break;
        case VQEC_CFG_MAX_PAKSIZE:
            s_cfg.max_paksize = cfg->max_paksize;
            break;
        case VQEC_CFG_STUN_SERVER_IP:
            break;
        case VQEC_CFG_STUN_SERVER_PORT:
            break;
        case VQEC_CFG_CDI_ENABLE:
            s_cfg.cdi_enable = cfg->cdi_enable;
            break;
        case VQEC_CFG_DOMAIN_NAME_OVERRIDE:
            (void)strlcpy(s_cfg.domain_name_override,
                          cfg->domain_name_override,
                          VQEC_MAX_NAME_LEN);
            break;
        case VQEC_CFG_CLI_TELNET_PORT:
            s_cfg.cli_telnet_port = cfg->cli_telnet_port;
            break;
        case VQEC_CFG_OUTPUT_PAKQ_LIMIT:
            s_cfg.output_pakq_limit = cfg->output_pakq_limit;
            break;
        case VQEC_CFG_UPDATE_WINDOW:
            s_cfg.update_window = cfg->update_window;
            break;
        case VQEC_CFG_UPDATE_INTERVAL_MAX:
            s_cfg.update_interval_max = cfg->update_interval_max;
            break;
        case VQEC_CFG_APP_PAKS_PER_RCC:
            s_cfg.app_paks_per_rcc = cfg->app_paks_per_rcc;
            break;
        case VQEC_CFG_ERROR_REPAIR_ENABLE:
            s_cfg.error_repair_enable =
                cfg->error_repair_enable;
            break;
        case VQEC_CFG_ERROR_REPAIR_POLICER_ENABLE:
            s_cfg.error_repair_policer_enable =
                cfg->error_repair_policer_enable;
            break;
        case VQEC_CFG_ERROR_REPAIR_POLICER_RATE:
            s_cfg.error_repair_policer_rate =
                cfg->error_repair_policer_rate;
            break;
        case VQEC_CFG_ERROR_REPAIR_POLICER_BURST:
            s_cfg.error_repair_policer_burst =
                cfg->error_repair_policer_burst;
            break;
        case VQEC_CFG_LOG_LEVEL:
            log_level_old = s_cfg.log_level;
            log_level_new = cfg->log_level;

            if (log_level_old != log_level_new) {

                s_cfg.log_level = cfg->log_level;

                /*
                 * Updates to the log_level configuration will be logged
                 * at LOG_NOTICE level, if the old or new filter allows it.
                 */
                snprintf(log_str, VQEC_LOGMSG_BUFSIZE,
                         "VQE-C log_level changed from %d to %d",
                         log_level_old, log_level_new);

                if (log_level_old > log_level_new) {
                    syslog_print(VQEC_NOTICE, log_str);
                }

                /* Enforce immediate update */
                syslog_facility_filter_set(LOG_VQEC, s_cfg.log_level);
                sync_lib_facility_filter_with_process(LOG_VQEC, LOG_LIBCFG);
                sync_lib_facility_filter_with_process(LOG_VQEC, LOG_RTP);
                sync_lib_facility_filter_with_process(LOG_VQEC, LOG_VQE_UTILS);

                if (log_level_new > log_level_old) {
                    syslog_print(VQEC_NOTICE, log_str);
                }

            }
            break;


        case VQEC_CFG_FEC_ENABLE:
            s_cfg.fec_enable = cfg->fec_enable;
            break;
        case VQEC_CFG_CLI_IFNAME:
            (void)strlcpy(s_cfg.cli_ifname,
                          cfg->cli_ifname, VQEC_MAX_NAME_LEN);
            break;
#if HAVE_FCC
        /* added for use by RCC */ 
        case VQEC_CFG_RCC_ENABLE:
            s_cfg.rcc_enable = cfg->rcc_enable;
            break;
        case VQEC_CFG_RCC_PAKPOOL_MAX_PCT:
            s_cfg.rcc_pakpool_max_pct = 
                cfg->rcc_pakpool_max_pct;
            break;
        case VQEC_CFG_RCC_START_TIMEOUT:
            s_cfg.rcc_start_timeout = 
                cfg->rcc_start_timeout;
            break;
        case VQEC_CFG_FASTFILL_ENABLE:
            s_cfg.fastfill_enable = cfg->fastfill_enable;
            break;
        case VQEC_CFG_RCC_EXTRA_IGMP_IP:
            s_cfg.rcc_extra_igmp_ip = cfg->rcc_extra_igmp_ip;
            break;
#endif
        case VQEC_CFG_NUM_BYES:
            s_cfg.num_byes = cfg->num_byes;
            break;
        case VQEC_CFG_BYE_DELAY:
            s_cfg.bye_delay = cfg->bye_delay;
            break;
        case VQEC_CFG_REORDER_DELAY_ABS:
            s_cfg.reorder_delay_abs = cfg->reorder_delay_abs;
            break;
        case VQEC_CFG_REORDER_DELAY:
            /*
             * This parameter has been deprecated.
             * If the user has not supplied a value, the configured or default
             * value for VQEC_CFG_REORDER_DELAY_ABS will apply.
             */
            break;
        case VQEC_CFG_VCDS_SERVER_IP:
            s_cfg.vcds_server_ip = cfg->vcds_server_ip;
            break;
        case VQEC_CFG_VCDS_SERVER_PORT:
            s_cfg.vcds_server_port = cfg->vcds_server_port;
            break;
        case VQEC_CFG_TUNER_LIST:
            break;
        case VQEC_CFG_RTCP_DSCP_VALUE:
            s_cfg.rtcp_dscp_value = cfg->rtcp_dscp_value;
            break;
        case VQEC_CFG_DELIVER_PAKS_TO_USER:
            s_cfg.deliver_paks_to_user = 
                cfg->deliver_paks_to_user;
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_SD:
            s_cfg.max_receive_bandwidth_sd =
                cfg->max_receive_bandwidth_sd;
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_SD_RCC:
            s_cfg.max_receive_bandwidth_sd_rcc =
                cfg->max_receive_bandwidth_sd_rcc;
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_HD:
            s_cfg.max_receive_bandwidth_hd =
                cfg->max_receive_bandwidth_hd;
            break;
        case VQEC_CFG_MAX_RECEIVE_BANDWIDTH_HD_RCC:
            s_cfg.max_receive_bandwidth_hd_rcc =
                cfg->max_receive_bandwidth_hd_rcc;
            break;
        case VQEC_CFG_MIN_HD_STREAM_BITRATE:
            s_cfg.min_hd_stream_bitrate =
                cfg->min_hd_stream_bitrate;
            break;
        case VQEC_CFG_MAX_FASTFILL:
            s_cfg.max_fastfill = cfg->max_fastfill;
            break;
        case VQEC_CFG_APP_DELAY:
            s_cfg.app_delay = cfg->app_delay;
            break;
        case VQEC_CFG_SRC_IP_FILTER_ENABLE:
            s_cfg.src_ip_filter_enable = 
                cfg->src_ip_filter_enable;
            break;
        case VQEC_CFG_QOE_ENABLE:
            s_cfg.qoe_enable = cfg->qoe_enable;
            break;
        case VQEC_CFG_RCC_MAX_CONCURRENT:
            s_cfg.rcc_max_concurrent = cfg->rcc_max_concurrent;
            break;
        case VQEC_CFG_INTEGRATED_RTP_FALLBACK:
            s_cfg.integrated_rtp_fallback = cfg->integrated_rtp_fallback;
            break;
        case VQEC_CFG_UDP_PASSTHRU_SUPPORT:
            s_cfg.udp_passthru_support = cfg->udp_passthru_support;
            break;
        case VQEC_CFG_VOD_CMD_TIMEOUT:
            s_cfg.vod_cmd_timeout = cfg->vod_cmd_timeout;
            break;
        case VQEC_CFG_VOD_MAX_SESSIONS:
            s_cfg.vod_max_sessions = cfg->vod_max_sessions;
            break;
        case VQEC_CFG_VOD_MODE:
            s_cfg.vod_mode = cfg->vod_mode;
            break;
        case VQEC_CFG_STUN_OPTIMIZATION:
            s_cfg.stun_optimization = cfg->stun_optimization;
            break;
        case VQEC_CFG_MUST_BE_LAST:
            break;
        }
    }

}

/*
 * Writes a buffer to a file.
 *
 * @param[in] config    - configuration to be updated
 * @param[in] buffer    - contains data to be written
 * @param[out] boolean  - TRUE returned if file was succesfully written
 *                        FALSE returned otherwise
 *
 * Return codes:
 *   VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT  
 *           - write failed, existing file is not modified
 *   VQEC_ERR_UPDATE_FAILED_EXISTING_REMOVED 
 *           - write failed, existing file was removed
 *   VQEC_OK - write succeeded
 */
vqec_error_t
vqec_syscfg_write_to_file (vqec_ifclient_config_t config,
                           const char *buffer)
{
    FILE *fp = NULL;
    vqec_error_t status = VQEC_OK;
    char debug_str[DEBUG_STR_LEN];
    char *filename = vqec_syscfg_get_filename(config);
    
    if (!filename || filename[0] == '\0') {
        snprintf(debug_str, DEBUG_STR_LEN,
                 "Configuration file cannot be written:  "
                 "no path specified for %s Configuration.", 
                 vqec_syscfg_index_names[config]);
        status = VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT;
        goto done;
    }
    
    /* Write the configuration data to the file */
    fp = fopen(filename, "w");
    if (fp == NULL) {
        snprintf(debug_str, DEBUG_STR_LEN,
                 "Failed to open the file %s to write.", filename);
        status = VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT;
        goto done;
    }
    
    if (fprintf(fp, "%s", buffer) < 0) {
        snprintf(debug_str, DEBUG_STR_LEN,
                 "Failed to write the file %s", filename);
        (void)remove(filename);
        status = VQEC_ERR_UPDATE_FAILED_EXISTING_REMOVED;
        goto done;
    }
    
done:
    if (fp) {
        (void)fclose(fp);
    }
    if (status != VQEC_OK) {
        syslog_print(VQEC_SYSCFG_ERROR, debug_str);
    }
    return (status);
}


/*
 * Validates and (if validation is successful) updates a file which
 * contributes to the system configuration (e.g. network or override 
 * configuration).
 *
 * @param[in] config        - type of configuration (network or override)
 * @param[in] buffer        - contains updated contributor file
 * @param[out] vqec_error_t - result of update
 */
vqec_error_t
vqec_syscfg_update (const vqec_ifclient_config_t config,
                    const char *buffer)
{
    boolean *cfg_supported;
    vqec_error_t status;
    vqec_syscfg_t cfg;
    char checksum[MD5_CHECKSUM_STRLEN];

    switch (config) {
    case VQEC_IFCLIENT_CONFIG_NETWORK:
        cfg_supported = vqec_cfg_attribute_support;
        break;
    case VQEC_IFCLIENT_CONFIG_OVERRIDE:
        cfg_supported = vqec_cfg_override_support;
        break;
    default:
        status = VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT;
        goto done;
    }

    /* Validate / Load configuration */
    if (!buffer) {
        /* Interpret no buffer as meaning an empty buffer */
        buffer = "";
    }
    status = vqec_syscfg_load(NULL, cfg_supported, NULL, NULL, buffer, FALSE);
    switch (status) {
    case VQEC_OK:
    case VQEC_ERR_PARAMRANGEINVALID:
        break;
    case VQEC_ERR_CONFIG_INVALID:
        goto done;
    default:
        status = VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT;
        goto done;
    }
    
    /* Write configuration to the file specified */
    status = vqec_syscfg_write_to_file(config, buffer);
    if (status != VQEC_OK) {
        goto done;
    }
    /*
     * Update the index file, if one is in use.
     */
    if (s_cfg.index_cfg_pathname[0]) {
        vqe_MD5ComputeChecksumStr(buffer, FALSE, checksum);
        status = vqec_syscfg_index_write(config, checksum);
        if (status != VQEC_OK) {
            remove(vqec_syscfg_get_filename(config));
            status = VQEC_ERR_UPDATE_FAILED_EXISTING_REMOVED;
            goto done;
        }
    }
    
    /* If VQE-C is not initialized, update is complete */
    if (!vqec_ifclient_is_vqec_enabled_ul()) {
        goto done;
    }

    vqec_syscfg_read(&cfg);
    vqec_syscfg_set(&cfg, TRUE);

done:
    return (status);
}

vqec_error_t
vqec_syscfg_update_override_tags (const char *tags[], const char *values[])
{
    vqec_error_t status = VQEC_OK;
    int i, length, wr_len;
    vqec_cfg_enum_t param;
    char *buffer = NULL;
    char file_checksum[MD5_CHECKSUM_STRLEN],
         buffer_checksum[MD5_CHECKSUM_STRLEN];
    char debug_str[DEBUG_STR_LEN];

    if (!tags || !values) {
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    /*
     * Pre-process the tags[] and values[] array, to
     * 1. log any unsupported tags, and
     * 2. compute the size of the buffer needed to hold the configuration
     */
    for (i = 0, length = 0; tags[i]; i++) {
        
        /* Verify a value is given, otherwise skip the tag */
        if (!values[i]) {
            continue;
        }

        /*
         * Look up the VQE-C parameter ID matching the tag.
         * Verify parameter is recognized and supported as an override.
         * If not, move on to the next parameter.
         */
        param = vqec_syscfg_param_lookup(tags[i]);
        if (param == VQEC_CFG_MUST_BE_LAST) {
            snprintf(debug_str, DEBUG_STR_LEN,
                     "Unrecognized configuration parameter ('%s') supplied "
                     "within Override Configuration", tags[i]);
            syslog_print(VQEC_SYSCFG_PARAM_IGNORED, debug_str);
            continue;
        } else if (!vqec_cfg_override_support[param]) {
            snprintf(debug_str, DEBUG_STR_LEN,
                     "Unsupported configuration parameter ('%s') supplied "
                     "within Override Configuration", tags[i]);
            syslog_print(VQEC_SYSCFG_PARAM_IGNORED, debug_str);
            continue;
        }

        /*
         * Compute amount of space required for parameter's assignment
         * in buffer format.  Assumes scalar syntax for the parameter,
         * i.e.: "<tags[i]> = <values[i]>;\n"
         */
        length += strlen(tags[i]) + strlen(" = ") + strlen(values[i]) + 
            strlen(";\n");
    }
    
    /* Allocate storage for the override configuration in buffer format */
    buffer = (char *)malloc(length+1);
    if (!buffer) {
        /* no memory */
        status = VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT;
        goto done;
    }

    /*
     * Write the (tag, value) pairs into the buffer
     * Note that this uses scalar syntax, consistent with the assumption
     * made when computing the necessary size of the buffer.
     */
    *buffer = '\0';
    for (i = 0; tags[i]; i++) {
        
        /* Verify a value is given, otherwise skip the tag */
        if (!values[i]) {
            continue;
        }

        /*
         * Look up the VQE-C parameter ID matching the tag.
         * Verify parameter is recognized and supported as an override.
         * If not, move on to the next parameter.
         */
        param = vqec_syscfg_param_lookup(tags[i]);
        if ((param == VQEC_CFG_MUST_BE_LAST) ||
            (!vqec_cfg_override_support[param])) {
            continue;
        }

        /* Add parameter to buffer */
        wr_len = strlcat(buffer, tags[i], length);
        wr_len = strlcat(buffer, " = ", length);
        wr_len = strlcat(buffer, values[i], length);
        wr_len = strlcat(buffer, ";\n", length);

        /* Abort if the parameters exceed the buffer size */
        if (wr_len > length) {
            status = VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT;
            goto done;
        }
    }

    /*
     * Skip processing the update if both are true:
     *   1. the supplied buffer's checksum matches the existing file's 
     *      checksum, and
     *   2. the existing file's checksum matches the index
     *      (should always be true, but if not, the buffer update is processed
     *       as a recovery mechanism to bring the index and file checksums
     *       in sync again)
     */
    if (!vqe_MD5ComputeChecksumStr(s_cfg.override_cfg_pathname,
                                   TRUE, file_checksum) &&
        !vqe_MD5ComputeChecksumStr(buffer, FALSE, buffer_checksum) &&
        !strncmp(buffer_checksum, file_checksum, MD5_CHECKSUM_STRLEN) &&
        vqec_syscfg_checksum_is_valid(VQEC_IFCLIENT_CONFIG_OVERRIDE)) {

        /* Skip unnecessary update */
        goto done;
    }

    status = vqec_syscfg_update(VQEC_IFCLIENT_CONFIG_OVERRIDE, buffer);
    if (status == VQEC_ERR_CONFIG_INVALID) {
        status = VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT;
    }
   
done:
    if (buffer) {
        free(buffer);
    }
    return (status);
}

vqec_error_t
vqec_syscfg_register (const vqec_ifclient_config_register_params_t *params)
{
    vqec_error_t status = VQEC_OK;

    if (!params || !params->event_cb) {
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }
    
    switch (params->config) {
    case VQEC_IFCLIENT_CONFIG_NETWORK:
    case VQEC_IFCLIENT_CONFIG_CHANNEL:
    case VQEC_IFCLIENT_CONFIG_OVERRIDE:
        if (vqec_syscfg_callback[params->config]) {
            status = VQEC_ERR_ALREADYREGISTERED;
            goto done;
        }
        vqec_syscfg_callback[params->config] = params->event_cb;       
        break;
    default:
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }

done:
    return (status);
}

vqec_error_t
vqec_syscfg_update_network (const char *buffer)
{
    vqec_error_t status;

    status = vqec_syscfg_update(VQEC_IFCLIENT_CONFIG_NETWORK, buffer);

    return (status);
}

/*
 * Initializes the VQE-C system configuration from a local file.
 *
 * Any parameters which are not assigned a valid value within the
 * file will be assigned a default value chosen by VQE-C.
 *
 * @param[in]     filename       - configuration file to load
 * @param[out]    vqec_error_t   - VQEC_OK if no invalid parameter settings
 *                                  are detected
 *                                 VQEC_ERR_CONFIG_NOT_FOUND if the supplied file
 *                                  could not be loaded or parsed
 *                                 VQEC_ERR_PARAMRANGEINVALID if at least one
 *                                  invalid parameter setting was detected
 */
vqec_error_t
vqec_syscfg_init (const char *filename)
{
    vqec_error_t err = VQEC_OK;

    /*
     * In the absensce of any overriding configuration, VQE-C picks its
     * own defaults for each parameter.
     */
    vqec_syscfg_set_defaults(&s_cfg, NULL);

    /*
     * Store the integrator-supplied filename for future use in the
     * "show system-config start-up" command.
     */
    if (filename && (strlen(filename) <= VQEC_MAX_NAME_LEN)) {
        strncpy(s_vqec_syscfg_filename, filename, VQEC_MAX_NAME_LEN);
    } else {
        s_vqec_syscfg_filename[0] = '\0';
    }

    /*
     * Initialize the system config (s_cfg) from the config file
     * (if supplied).  The configuration may be later overridden by
     * attribute updates.
     */
    err = vqec_syscfg_load(&s_cfg, NULL, NULL, filename, NULL, TRUE);
    vqec_set_cname(s_cfg.input_ifname);

    return (err);
}
