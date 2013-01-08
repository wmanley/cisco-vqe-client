/******************************************************************************
  *
  * Cisco Systems, Inc.
  *
  * Copyright (c) 2008-2012 by Cisco Systems, Inc.
  * All rights reserved.
  *
  ******************************************************************************/

/* THIS IS GENERATED CODE.  DO NOT EDIT. */


#define VQEC_SYSCFG_SIG_MODE_STD "std"
#define VQEC_SYSCFG_SIG_MODE_NAT "nat"
// YouView2
#define VQEC_SYSCFG_SIG_MODE_MUX "mux"
#define VQEC_SYSCFG_VOD_MODE_IPTV "iptv"
#define VQEC_SYSCFG_VOD_MODE_CABLE "cable"
#define VQEC_MAX_NAME_LEN        (255)

/*****
 * max_tuners
 ******/
#define VQEC_SYSCFG_DEFAULT_MAX_TUNERS                      (1)
#define VQEC_SYSCFG_MIN_MAX_TUNERS                          (1)
#define VQEC_SYSCFG_MAX_MAX_TUNERS                          (32)
static inline boolean is_vqec_cfg_max_tuners_valid (uint32_t val) {
    if ((val >= (1)) && (val <= (32))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * channel_lineup
 ******/
#define VQEC_SYSCFG_DEFAULT_CHANNEL_LINEUP                  ""

/*****
 * network_cfg_pathname
 ******/
#define VQEC_SYSCFG_DEFAULT_NETWORK_CFG_PATHNAME            ""

/*****
 * override_cfg_pathname
 ******/
#define VQEC_SYSCFG_DEFAULT_OVERRIDE_CFG_PATHNAME           ""

/*****
 * index_cfg_pathname
 ******/
#define VQEC_SYSCFG_DEFAULT_INDEX_CFG_PATHNAME              ""

/*****
 * vqec_enable
 ******/
#define VQEC_SYSCFG_DEFAULT_VQEC_ENABLE                     (TRUE)

/*****
 * jitter_buff_size
 ******/
#define VQEC_SYSCFG_DEFAULT_JITTER_BUFF_SIZE                (200)
#define VQEC_SYSCFG_MIN_JITTER_BUFF_SIZE                    (0)
#define VQEC_SYSCFG_MAX_JITTER_BUFF_SIZE                    (20000)
static inline boolean is_vqec_cfg_jitter_buff_size_valid (uint32_t val) {
    if (val <= (20000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * repair_trigger_point_abs
 ******/
#define VQEC_SYSCFG_DEFAULT_REPAIR_TRIGGER_POINT_ABS        (20)
#define VQEC_SYSCFG_MIN_REPAIR_TRIGGER_POINT_ABS            (0)
#define VQEC_SYSCFG_MAX_REPAIR_TRIGGER_POINT_ABS            (200)
static inline boolean is_vqec_cfg_repair_trigger_point_abs_valid (uint32_t val) {
    if (val <= (200)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * repair_trigger_point
 ******/
#define VQEC_SYSCFG_DEFAULT_REPAIR_TRIGGER_POINT            (20)
#define VQEC_SYSCFG_MIN_REPAIR_TRIGGER_POINT                (0)
#define VQEC_SYSCFG_MAX_REPAIR_TRIGGER_POINT                (100)
static inline boolean is_vqec_cfg_repair_trigger_point_valid (uint32_t val) {
    if (val <= (100)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * pakpool_size
 ******/
#define VQEC_SYSCFG_DEFAULT_PAKPOOL_SIZE                    (1000)
#define VQEC_SYSCFG_MIN_PAKPOOL_SIZE                        (100)
#define VQEC_SYSCFG_MAX_PAKPOOL_SIZE                        (200000)
static inline boolean is_vqec_cfg_pakpool_size_valid (uint32_t val) {
    if ((val >= (100)) && (val <= (200000))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * so_rcvbuf
 ******/
#define VQEC_SYSCFG_DEFAULT_SO_RCVBUF                       (128000)
#define VQEC_SYSCFG_MIN_SO_RCVBUF                           (0)
#define VQEC_SYSCFG_MAX_SO_RCVBUF                           (1000000)
static inline boolean is_vqec_cfg_so_rcvbuf_valid (uint32_t val) {
    if (val <= (1000000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * strip_rtp
 ******/
#define VQEC_SYSCFG_DEFAULT_STRIP_RTP                       (TRUE)

/*****
 * input_ifname
 ******/
#define VQEC_SYSCFG_DEFAULT_INPUT_IFNAME                    ""

/*****
 * sig_mode
 ******/
#define VQEC_SYSCFG_DEFAULT_SIG_MODE                        "nat"
static inline boolean is_vqec_cfg_sig_mode_valid (char *str) {
    if ((strcmp("nat", str) != 0) ||
// YouView2 BEGIN
       (strcmp("std", str) != 0) ||
       (strcmp("mux", str) != 0)) {
// YouView2 END
        return (FALSE);
    }
    return (TRUE);
}

/*****
 * nat_binding_refresh_interval
 ******/
#define VQEC_SYSCFG_DEFAULT_NAT_BINDING_REFRESH_INTERVAL    (30)
#define VQEC_SYSCFG_MIN_NAT_BINDING_REFRESH_INTERVAL        (0)
#define VQEC_SYSCFG_MAX_NAT_BINDING_REFRESH_INTERVAL        (100000)
static inline boolean is_vqec_cfg_nat_binding_refresh_interval_valid (uint32_t val) {
    if (val <= (100000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * nat_filter_refresh_interval
 ******/
#define VQEC_SYSCFG_DEFAULT_NAT_FILTER_REFRESH_INTERVAL     (30)
#define VQEC_SYSCFG_MIN_NAT_FILTER_REFRESH_INTERVAL         (0)
#define VQEC_SYSCFG_MAX_NAT_FILTER_REFRESH_INTERVAL         (100000)
static inline boolean is_vqec_cfg_nat_filter_refresh_interval_valid (uint32_t val) {
    if (val <= (100000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * max_paksize
 ******/
#define VQEC_SYSCFG_DEFAULT_MAX_PAKSIZE                     (1508)
#define VQEC_SYSCFG_MIN_MAX_PAKSIZE                         (1330)
#define VQEC_SYSCFG_MAX_MAX_PAKSIZE                         (10000)
static inline boolean is_vqec_cfg_max_paksize_valid (uint32_t val) {
    if ((val >= (1330)) && (val <= (10000))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * stun_server_ip
 ******/
#define VQEC_SYSCFG_DEFAULT_STUN_SERVER_IP                  (0)
#define VQEC_SYSCFG_MIN_STUN_SERVER_IP                      (0)
#define VQEC_SYSCFG_MAX_STUN_SERVER_IP                      ((1<<32) - 1)
static inline boolean is_vqec_cfg_stun_server_ip_valid (uint32_t val) {
    if (val == (0)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * stun_server_port
 ******/
#define VQEC_SYSCFG_DEFAULT_STUN_SERVER_PORT                (0)
#define VQEC_SYSCFG_MIN_STUN_SERVER_PORT                    (1000)
#define VQEC_SYSCFG_MAX_STUN_SERVER_PORT                    ((1<<16) - 1)
static inline boolean is_vqec_cfg_stun_server_port_valid (uint32_t val) {
    if ((val >= (1000)) && (val <= ((1<<16) - 1))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * cdi_enable
 ******/
#define VQEC_SYSCFG_DEFAULT_CDI_ENABLE                      (FALSE)

/*****
 * domain_name_override
 ******/
#define VQEC_SYSCFG_DEFAULT_DOMAIN_NAME_OVERRIDE            ""

/*****
 * libcli_telnet_port
 ******/
#define VQEC_SYSCFG_DEFAULT_CLI_TELNET_PORT                 (0)
#define VQEC_SYSCFG_MIN_R0_CLI_TELNET_PORT                  (0)
#define VQEC_SYSCFG_MAX_R0_CLI_TELNET_PORT                  (0)
#define VQEC_SYSCFG_MIN_R1_CLI_TELNET_PORT                  (8000)
#define VQEC_SYSCFG_MAX_R1_CLI_TELNET_PORT                  ((1<<16)- 1)
static inline boolean is_vqec_cfg_cli_telnet_port_valid (uint32_t val) {
    if (val == 0) {
        return (TRUE);
    }
    if ((val >= (8000)) && (val <= ((1<<16)- 1))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * output_pakq_limit
 ******/
#define VQEC_SYSCFG_DEFAULT_OUTPUT_PAKQ_LIMIT               (200)
#define VQEC_SYSCFG_MIN_OUTPUT_PAKQ_LIMIT                   (100)
#define VQEC_SYSCFG_MAX_OUTPUT_PAKQ_LIMIT                   (20000)
static inline boolean is_vqec_cfg_output_pakq_limit_valid (uint32_t val) {
    if ((val >= (100)) && (val <= (20000))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * update_window
 ******/
#define VQEC_SYSCFG_DEFAULT_UPDATE_WINDOW                   (60)
#define VQEC_SYSCFG_MIN_UPDATE_WINDOW                       (1)
#define VQEC_SYSCFG_MAX_UPDATE_WINDOW                       (100000)
static inline boolean is_vqec_cfg_update_window_valid (uint32_t val) {
    if ((val >= (1)) && (val <= (100000))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * update_interval_max
 ******/
#define VQEC_SYSCFG_DEFAULT_UPDATE_INTERVAL_MAX             (3600)
#define VQEC_SYSCFG_MIN_R0_UPDATE_INTERVAL_MAX              (0)
#define VQEC_SYSCFG_MAX_R0_UPDATE_INTERVAL_MAX              (0)
#define VQEC_SYSCFG_MIN_R1_UPDATE_INTERVAL_MAX              (30)
#define VQEC_SYSCFG_MAX_R1_UPDATE_INTERVAL_MAX              (604800)
static inline boolean is_vqec_cfg_update_interval_max_valid (uint32_t val) {
    if (val == 0) {
        return (TRUE);
    }
    if ((val >= (30)) && (val <= (604800))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * app_paks_per_rcc
 ******/
#define VQEC_SYSCFG_DEFAULT_APP_PAKS_PER_RCC                (1)
#define VQEC_SYSCFG_MIN_APP_PAKS_PER_RCC                    (1)
#define VQEC_SYSCFG_MAX_APP_PAKS_PER_RCC                    (20)
static inline boolean is_vqec_cfg_app_paks_per_rcc_valid (uint32_t val) {
    if ((val >= (1)) && (val <= (20))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * error_repair_enable
 ******/
#define VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_ENABLE             (TRUE)

/*****
 * error_repair_policer.enable
 ******/
#define VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_ENABLE     (FALSE)

/*****
 * error_repair_policer.rate
 ******/
#define VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_RATE       (5)
#define VQEC_SYSCFG_MIN_ERROR_REPAIR_POLICER_RATE           (1)
#define VQEC_SYSCFG_MAX_ERROR_REPAIR_POLICER_RATE           (100)
static inline boolean is_vqec_cfg_error_repair_policer_rate_valid (uint32_t val) {
    if ((val >= (1)) && (val <= (100))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * error_repair_policer.burst
 ******/
#define VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_BURST      (10000)
#define VQEC_SYSCFG_MIN_ERROR_REPAIR_POLICER_BURST          (1)
#define VQEC_SYSCFG_MAX_ERROR_REPAIR_POLICER_BURST          (60000)
static inline boolean is_vqec_cfg_error_repair_policer_burst_valid (uint32_t val) {
    if ((val >= (1)) && (val <= (60000))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * fec_enable
 ******/
#define VQEC_SYSCFG_DEFAULT_FEC_ENABLE                      (TRUE)

/*****
 * rcc_enable
 ******/
#define VQEC_SYSCFG_DEFAULT_RCC_ENABLE                      (TRUE)

/*****
 * rcc_pakpool_max_pct
 ******/
#define VQEC_SYSCFG_DEFAULT_RCC_PAKPOOL_MAX_PCT             (80)
#define VQEC_SYSCFG_MIN_RCC_PAKPOOL_MAX_PCT                 (0)
#define VQEC_SYSCFG_MAX_RCC_PAKPOOL_MAX_PCT                 (100)
static inline boolean is_vqec_cfg_rcc_pakpool_max_pct_valid (uint32_t val) {
    if (val <= (100)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * rcc_start_timeout
 ******/
#define VQEC_SYSCFG_DEFAULT_RCC_START_TIMEOUT               (120)
#define VQEC_SYSCFG_MIN_RCC_START_TIMEOUT                   (0)
#define VQEC_SYSCFG_MAX_RCC_START_TIMEOUT                   (1000)
static inline boolean is_vqec_cfg_rcc_start_timeout_valid (uint32_t val) {
    if (val <= (1000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * num_byes
 ******/
#define VQEC_SYSCFG_DEFAULT_NUM_BYES                        (2)
#define VQEC_SYSCFG_MIN_NUM_BYES                            (2)
#define VQEC_SYSCFG_MAX_NUM_BYES                            (5)
static inline boolean is_vqec_cfg_num_byes_valid (uint32_t val) {
    if ((val >= (2)) && (val <= (5))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * bye_delay
 ******/
#define VQEC_SYSCFG_DEFAULT_BYE_DELAY                       (40)
#define VQEC_SYSCFG_MIN_BYE_DELAY                           (10)
#define VQEC_SYSCFG_MAX_BYE_DELAY                           (100)
static inline boolean is_vqec_cfg_bye_delay_valid (uint32_t val) {
    if ((val >= (10)) && (val <= (100))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * reorder_delay_abs
 ******/
#define VQEC_SYSCFG_DEFAULT_REORDER_DELAY_ABS               (20)
#define VQEC_SYSCFG_MIN_REORDER_DELAY_ABS                   (0)
#define VQEC_SYSCFG_MAX_REORDER_DELAY_ABS                   (200)
static inline boolean is_vqec_cfg_reorder_delay_abs_valid (uint32_t val) {
    if (val <= (200)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * reorder_delay
 ******/
#define VQEC_SYSCFG_DEFAULT_REORDER_DELAY                   (20)
#define VQEC_SYSCFG_MIN_REORDER_DELAY                       (0)
#define VQEC_SYSCFG_MAX_REORDER_DELAY                       (100)
static inline boolean is_vqec_cfg_reorder_delay_valid (uint32_t val) {
    if (val <= (100)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * vcds_server_ip
 ******/
#define VQEC_SYSCFG_DEFAULT_VCDS_SERVER_IP                  (0)
#define VQEC_SYSCFG_MIN_VCDS_SERVER_IP                      (0)
#define VQEC_SYSCFG_MAX_VCDS_SERVER_IP                      ((1<<32) - 1)
static inline boolean is_vqec_cfg_vcds_server_ip_valid (uint32_t val) {
    if (val == (0)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * vcds_server_port
 ******/
#define VQEC_SYSCFG_DEFAULT_VCDS_SERVER_PORT                (8554)
#define VQEC_SYSCFG_MIN_VCDS_SERVER_PORT                    (1024)
#define VQEC_SYSCFG_MAX_VCDS_SERVER_PORT                    ((1<<16) - 1)
static inline boolean is_vqec_cfg_vcds_server_port_valid (uint32_t val) {
    if ((val >= (1024)) && (val <= ((1<<16) - 1))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * cli_ifname
 ******/
#define VQEC_SYSCFG_DEFAULT_CLI_IFNAME                      "lo"

/*****
 * tuner_list
 ******/
#define VQEC_SYSCFG_DEFAULT_TUNER_LIST                      ""

/*****
 * rtcp_dscp_value
 ******/
#define VQEC_SYSCFG_DEFAULT_RTCP_DSCP_VALUE                 (24)
#define VQEC_SYSCFG_MIN_RTCP_DSCP_VALUE                     (0)
#define VQEC_SYSCFG_MAX_RTCP_DSCP_VALUE                     (63)
static inline boolean is_vqec_cfg_rtcp_dscp_value_valid (uint32_t val) {
    if (val <= (63)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * deliver_paks_to_user
 ******/
#define VQEC_SYSCFG_DEFAULT_DELIVER_PAKS_TO_USER            (TRUE)

/*****
 * fastfill_enable
 ******/
#define VQEC_SYSCFG_DEFAULT_FASTFILL_ENABLE                 (TRUE)

/*****
 * max_receive_bandwidth_sd
 ******/
#define VQEC_SYSCFG_DEFAULT_MAX_RECEIVE_BANDWIDTH_SD        (0)
#define VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_SD            (0)
#define VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_SD            (1000000000)
static inline boolean is_vqec_cfg_max_receive_bandwidth_sd_valid (uint32_t val) {
    if (val <= (1000000000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * max_receive_bandwidth_sd_rcc
 ******/
#define VQEC_SYSCFG_DEFAULT_MAX_RECEIVE_BANDWIDTH_SD_RCC    (0)
#define VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_SD_RCC        (0)
#define VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_SD_RCC        (1000000000)
static inline boolean is_vqec_cfg_max_receive_bandwidth_sd_rcc_valid (uint32_t val) {
    if (val <= (1000000000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * max_receive_bandwidth_hd
 ******/
#define VQEC_SYSCFG_DEFAULT_MAX_RECEIVE_BANDWIDTH_HD        (0)
#define VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_HD            (0)
#define VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_HD            (1000000000)
static inline boolean is_vqec_cfg_max_receive_bandwidth_hd_valid (uint32_t val) {
    if (val <= (1000000000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * max_receive_bandwidth_hd_rcc
 ******/
#define VQEC_SYSCFG_DEFAULT_MAX_RECEIVE_BANDWIDTH_HD_RCC    (0)
#define VQEC_SYSCFG_MIN_MAX_RECEIVE_BANDWIDTH_HD_RCC        (0)
#define VQEC_SYSCFG_MAX_MAX_RECEIVE_BANDWIDTH_HD_RCC        (1000000000)
static inline boolean is_vqec_cfg_max_receive_bandwidth_hd_rcc_valid (uint32_t val) {
    if (val <= (1000000000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * min_hd_stream_bitrate
 ******/
#define VQEC_SYSCFG_DEFAULT_MIN_HD_STREAM_BITRATE           (0)
#define VQEC_SYSCFG_MIN_MIN_HD_STREAM_BITRATE               (0)
#define VQEC_SYSCFG_MAX_MIN_HD_STREAM_BITRATE               (1000000000)
static inline boolean is_vqec_cfg_min_hd_stream_bitrate_valid (uint32_t val) {
    if (val <= (1000000000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * max_fastfill
 ******/
#define VQEC_SYSCFG_DEFAULT_MAX_FASTFILL                    (0)
#define VQEC_SYSCFG_MIN_MAX_FASTFILL                        (0)
#define VQEC_SYSCFG_MAX_MAX_FASTFILL                        (1000000000)
static inline boolean is_vqec_cfg_max_fastfill_valid (uint32_t val) {
    if (val <= (1000000000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * app_delay
 ******/
#define VQEC_SYSCFG_DEFAULT_APP_DELAY                       (0)
#define VQEC_SYSCFG_MIN_APP_DELAY                           (0)
#define VQEC_SYSCFG_MAX_APP_DELAY                           (60000)
static inline boolean is_vqec_cfg_app_delay_valid (uint32_t val) {
    if (val <= (60000)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * src_ip_filter_enable
 ******/
#define VQEC_SYSCFG_DEFAULT_SRC_IP_FILTER_ENABLE            (FALSE)

/*****
 * qoe_enable
 ******/
#define VQEC_SYSCFG_DEFAULT_QOE_ENABLE                      (TRUE)

/*****
 * rcc_extra_igmp_ip
 ******/
#define VQEC_SYSCFG_DEFAULT_RCC_EXTRA_IGMP_IP               (0)

/*****
 * rcc_max_concurrent
 ******/
#define VQEC_SYSCFG_DEFAULT_RCC_MAX_CONCURRENT              (1)
#define VQEC_SYSCFG_MIN_RCC_MAX_CONCURRENT                  (0)
#define VQEC_SYSCFG_MAX_RCC_MAX_CONCURRENT                  (32)
static inline boolean is_vqec_cfg_rcc_max_concurrent_valid (uint32_t val) {
    if (val <= (32)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * integrated_rtp_fallback
 ******/
#define VQEC_SYSCFG_DEFAULT_INTEGRATED_RTP_FALLBACK         (TRUE)

/*****
 * udp_passthru_support
 ******/
#define VQEC_SYSCFG_DEFAULT_UDP_PASSTHRU_SUPPORT            (FALSE)

/*****
 * vod_cmd_timeout
 ******/
#define VQEC_SYSCFG_DEFAULT_VOD_CMD_TIMEOUT                 (2000)
#define VQEC_SYSCFG_MIN_VOD_CMD_TIMEOUT                     (100)
#define VQEC_SYSCFG_MAX_VOD_CMD_TIMEOUT                     (10000)
static inline boolean is_vqec_cfg_vod_cmd_timeout_valid (uint32_t val) {
    if ((val >= (100)) && (val <= (10000))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * vod_max_sessions
 ******/
#define VQEC_SYSCFG_DEFAULT_VOD_MAX_SESSIONS                (1)
#define VQEC_SYSCFG_MIN_VOD_MAX_SESSIONS                    (1)
#define VQEC_SYSCFG_MAX_VOD_MAX_SESSIONS                    (32)
static inline boolean is_vqec_cfg_vod_max_sessions_valid (uint32_t val) {
    if ((val >= (1)) && (val <= (32))) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * vod_mode
 ******/
#define VQEC_SYSCFG_DEFAULT_VOD_MODE                        "iptv"
static inline boolean is_vqec_cfg_vod_mode_valid (char *str) {
    if ((strcmp("iptv", str) != 0) ||
       (strcmp("cable", str) != 0)) {
        return (FALSE);
    }
    return (TRUE);
}

/*****
 * log_level
 ******/
#define VQEC_SYSCFG_DEFAULT_LOG_LEVEL                       (4)
#define VQEC_SYSCFG_MIN_LOG_LEVEL                           (0)
#define VQEC_SYSCFG_MAX_LOG_LEVEL                           (7)
static inline boolean is_vqec_cfg_log_level_valid (uint32_t val) {
    if (val <= (7)) {
        return (TRUE);
    }
    return (FALSE);
}

/*****
 * stun optimization
 ******/
#define VQEC_SYSCFG_DEFAULT_STUN_OPTIMIZATION            (TRUE)

