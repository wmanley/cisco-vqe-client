/*
 * vqec_syscfg.h - defines the VQE-C system configuration settings, which 
 * are read from the libconf formatted vqec system configuration file at
 * startup time.
 *
 * Copyright (c) 2007-2010, 2012 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifndef __VQEC_SYSCFG_H__
#define __VQEC_SYSCFG_H__

#include <stdint.h>
#include "vqec_error.h"
#include "vam_types.h"
#include "vqec_cfg_limits.h"
#include "vqec_ifclient.h"
#include "vqec_ifclient_defs.h"
#include "utils/vmd5.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum vqec_syscfg_sig_mode_{
    VQEC_SM_NAT,
    VQEC_SM_STD
} vqec_syscfg_sig_mode_t;

typedef enum vqec_syscfg_vod_mode_{
    VQEC_VOD_MODE_IPTV,
    VQEC_VOD_MODE_CABLE
} vqec_syscfg_vod_mode_t;

/* structure to cache configuration for tuners to be created from config */
typedef struct vqec_tunercfg_ {
    char tuner_name[VQEC_MAX_NAME_LEN];  /* name of the tuner */
    char tuner_url[VQEC_MAX_NAME_LEN];   /*
                                          * channel that the tuner should be
                                          * bound to upon startup
                                          */
    uint32_t max_receive_bandwidth_rcc;  /* per-tuner e-factor for RCC */
    uint32_t max_receive_bandwidth_er;   /* per-tuner e-factor for ER */
} vqec_tunercfg_t;

/*
 * These constants for min/max must be defined here as they cannot be part of
 * the enumeration in vqec_cfg_settings.h.  These values are not part of the
 * regular flat configuration list, since they are in the tuner_list
 * subsection, and therefore, they cannot be treated equally among the rest of
 * the main configuration parameters.  For example, the big switch() statement
 * which cycles through all the normal config parameters should exclude these.
 */
#define VQEC_SYSCFG_MIN_TUNER_MAX_RECEIVE_BANDWIDTH_RCC 0
#define VQEC_SYSCFG_MAX_TUNER_MAX_RECEIVE_BANDWIDTH_RCC 1000000000 /* 1 Gb */

#define VQEC_SYSCFG_MIN_TUNER_MAX_RECEIVE_BANDWIDTH_ER 0
#define VQEC_SYSCFG_MAX_TUNER_MAX_RECEIVE_BANDWIDTH_ER 1000000000 /* 1 Gb */

/*
 * When modifying this structure to add a new configuration setting
 * please add a corresponding setting to the configuration parse settings 
 * table in vqec_cfg_settings.h.
 */
typedef struct vqec_syscfg_ {
    uint32_t max_tuners;                  /*
                                           * Maximum number of tuners 
                                           * permitted to create
                                           */
    uint8_t src_ip_filter_enable;          /* filter for channel streams */
    char channel_lineup[VQEC_MAX_NAME_LEN];/* VQEC channel lineup filename */
    char network_cfg_pathname[VQEC_MAX_NAME_LEN];
                                          /* Per-client network config */
    char override_cfg_pathname[VQEC_MAX_NAME_LEN];
                                          /* Override config */
    char index_cfg_pathname[VQEC_MAX_NAME_LEN];
                                          /* Configuration Index file */
    boolean d_vqec_enable;                /* Is VQE-C enabled? (deprecated) */
    boolean qoe_enable;                   /* Is VQE-C enabled? */
    uint32_t jitter_buff_size;            /* RTP jitter buffer size in ms */
    uint32_t repair_trigger_point_abs;    /*
                                           * RTP repair trigger point (in ms)
                                           */
    uint32_t d_repair_trigger_point;      /*
                                           * RTP repair trigger point pct of 
                                           * jitter-buff-size [0..100]
                                           * (deprecated)
                                           */
    uint32_t pakpool_size;                /*
                                           * Number of pak pool elems to 
                                           * create
                                           */
    uint32_t so_rcvbuf;                   /*
                                           * socket receive size [0..1000000]
                                           */
    uint32_t strip_rtp;                   /*
                                           * strip the rtp header 
					   *  [true==1/false==0]
                                           */
    char input_ifname[VQEC_MAX_NAME_LEN]; /* Input interface name */
    vqec_syscfg_sig_mode_t sig_mode;    /* signalling-mode(NAT/STD) */
    uint32_t nat_binding_refresh_interval;/*
                                           * NAT binding refresh interval
                                           * in ms [0..100000]
                                           */
    uint32_t max_paksize;                 /*
                                           * sized of fixed allocated pak pool
                                           * buffers
                                           */
    uint32_t cdi_enable;                  /*
                                           * enable config delivery 
                                           * infrastructure 
                                           * [true==1/false==0]
                                           */
    char domain_name_override[VQEC_MAX_NAME_LEN]; /* override domain name */
    uint16_t cli_telnet_port;             /* cli port, network order */
    uint32_t output_pakq_limit;           /* Limit on output packet Q size */
    uint32_t update_window;               /* 
                                           * the time-window (in seconds) 
                                           * over which to jitter triggered/
                                           * polled updates after the 
                                           * initial update
                                           */
    uint32_t update_interval_max;         /*
                                           * Max time between config update
                                           * requests from VQE-C to VCDS.
                                           * 0 implies infinite time
                                           *   (update polling is not enabled)
                                           */
    uint16_t app_paks_per_rcc;            /*
                                           * the number of APP paks to send
                                           * in output just after an RCC
                                           */
    uint32_t error_repair_enable;         /*
                                           * enable the error repair feature
                                           * in vqec
                                           */
    boolean error_repair_policer_enable;  /* error repair policing enabled? */
    uint8_t error_repair_policer_rate;    /*
                                           * Token bucket rate for policing
                                           * of client repair requests,
                                           * expressed as a percentage of 
                                           * the stream rate.
                                           */
    uint16_t error_repair_policer_burst;  /* Token bucket burst for policing
                                           * of client repair requests,
                                           * expressed as a duration of 
                                           * time (in ms).
                                           */
    uint8_t log_level;                    /*
                                           * log level
                                           */
    uint32_t reorder_delay_abs;           /*
                                           * time (ms) to hold a gap that may
                                           * be due to reordered packets before
                                           * making available for ER
                                           */
    uint32_t d_reorder_delay;             /*
                                           * time (pct of jitter_buff_size
                                           * [0..100]) to hold a gap that may
                                           * be due to reordered packets before
                                           * making available for ER
                                           * (deprecated)
                                           */
    uint32_t vcds_server_ip;              /* vcds server ip, network order */
    uint16_t vcds_server_port;            /* vcds server port, network order */
    vqec_tunercfg_t
    tuner_list[VQEC_SYSCFG_MAX_MAX_TUNERS+1]; /* 
                                               * Note the last element
                                               * will always be NULL.
                                               */
    uint32_t fec_enable;                  /*
                                           * added for use by FEC 
                                           * enable FEC in vqec_c
                                           */
#if HAVE_FCC
    uint32_t rcc_enable;                  /*
                                           * added for use by RCC 
                                           * enable RCC in vqec_c
                                           */
    uint32_t rcc_pakpool_max_pct;         /*
                                           * added for use by RCC
                                           * used to calculate
                                           * max fill requirement
                                           * late instantiation
                                           */
    uint32_t rcc_start_timeout;           /*
                                           * timeout value of 
                                           * RCC start
                                           */
    uint32_t fastfill_enable;             /*
                                           * added for use by fastfill
                                           * enable or disable fastfill
                                           * in vqec
                                           */
    uint32_t rcc_extra_igmp_ip;           /*
                                           * multicast address to be joined
                                           * during the RCC burst
                                           */
    uint32_t rcc_max_concurrent;          /*
                                           * max concurrent RCCs for the pakpool
                                           * to be allocated for
                                           */
#endif  /* HAVE_FCC */
    uint32_t num_byes;                    /* num of byes sent to terminate
                                           * repair burst 
                                           */
    uint32_t bye_delay;                   /* min delay between two byes */
    uint8_t rtcp_dscp_value;              /* dscp bits in the RTCP packet */
    uint32_t deliver_paks_to_user;        /*
                                           * specifies whether paks are able to
                                           * be received in user space from a
                                           * kernel DP
                                           */
    char cli_ifname[VQEC_MAX_NAME_LEN];   /* interface name for CLI */
    uint32_t max_fastfill;                /* 
                                           * max amount of video (in bytes) to
                                           * fastfill
                                           */
    uint32_t max_receive_bandwidth_sd;    /* global e-factor for SD */
    uint32_t max_receive_bandwidth_sd_rcc;/* global e-factor for SD RCC */
    uint32_t max_receive_bandwidth_hd;    /* global e-factor for HD */
    uint32_t max_receive_bandwidth_hd_rcc;/* global e-factor for HD RCC */
    uint32_t min_hd_stream_bitrate;       /* min bitrate of an HD stream */
    uint32_t app_delay;                   /*
                                           * interpacket delay (in msec) between
                                           * replicated APP packets
                                           */
    boolean integrated_rtp_fallback;      /*
                                           * make sdp_handle_alloc API
                                           * allocate a dynamic channel for
                                           * non-VQE RTP channels
                                           */
    boolean udp_passthru_support;         /*
                                           * make sdp_handle_alloc API
                                           * allocate a dynamic channel for
                                           * UDP channels
                                           */
    uint32_t vod_cmd_timeout;             /*
                                           * vod rtsp response timeout in ms
                                           */
    uint32_t vod_max_sessions;            /*
                                           * max vod sessions supported
                                           */
    vqec_syscfg_vod_mode_t vod_mode;      /* vod-mode(IPTV/CABLE) */
    boolean stun_optimization;            /*
                                           * TRUE to disable stun signaling after
                                           * detecting not behind NAT;
                                           * FALSE to force stun signaling for each
                                           * channel change, event not behind nat. 
                                           */

} vqec_syscfg_t;

/*
 * Checksum value used for a configuration which does not exist.
 * This string is guaranteed not to match any true 32-char MD5 checksum
 * string.
 */
#define VQEC_SYSCFG_CHECKSUM_UNAVAILABLE "<file not available>"

/*
 * Dumps the default configuration to the console.
 */
void
vqec_syscfg_dump_defaults(void);

/*
 * Dumps the system configuration to the console.
 */
void
vqec_syscfg_dump_system(void);

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
vqec_syscfg_dump_startup(void);

/*
 * Dumps the attribute configuration to the console.
 * 
 * Usage considerations are very similar to vqec_syscfg_dump_system(), 
 * but with respect to the attribute configuration file.
 */
void
vqec_syscfg_dump_attributes(void);

/*
 * Dumps the override configuration to the console.
 */
void
vqec_syscfg_dump_overrides(void);

/*
 * Updates the Network Configuration file
 *
 * @param[in]  buffer        - buffer containing Network Configuration
 * @param[out] vqec_error_t  - result of update
 */
vqec_error_t
vqec_syscfg_update_network(const char *buffer);

/*
 * Updates the Override Configuration.
 *
 * (tag, value) pairs represent configuration parameters that should
 * exist within the Override Configuration.  Parameters which are not
 * present in the list will be excluded from the Override Configuration.
 *
 * @param[in]  tags         - list of parameters for which an override
 *                            is to exist
 * @param[in]  values       - values for parameters in "tags" array
 * @param[out] vqec_error_t - indicates result of update
 */
vqec_error_t
vqec_syscfg_update_override_tags(const char *tags[], const char *values[]);

/*
 * Registers a function which VQE-C will call when events of interest occur 
 * related to one of its configurations (Network Configuration, Override 
 * Configuration, and Channel Configuration).
 *
 * For example, if cfg_index_pathname is configured, then VQE-C will issue 
 * a notification if it detects that a persistent configuration is 
 * missing/corrupt.
 *
 * Only one (the first) function registered with this API will be accepted 
 * per configuration type; subsequent registrations will be rejected.
 *
 * @param[in]  params       - register parameters
 * @param[out] vqec_error_t - result of registration:
 *        VQEC_ERR_INVALIDARGS
 *          Invalid parameters supplied
 *        VQEC_ERR_ALREADYREGISTERED
 *          Callback already registered for the supplied configuration type.
 *        VQEC_OK
 *          Registration succeeded
 */
vqec_error_t
vqec_syscfg_register(const vqec_ifclient_config_register_params_t *params);

/*
 * Determines if a configuration file's checksum is valid.
 * A configuration file's checksum is valid iff:
 *   - no index file is configured for use, or
 *   - an index file is configured for use, and
 *        - the config file does not exist and has no index file entry, or
 *        - the config file does exist and has an index file entry with a
 *           matching checksum
 *
 * @param[in]  config  - configuration file whose checksum is to be validated
 * @param[out] boolean - TRUE if the checksum is valid,
 *                       FALSE if the checksum is invalid
 */
boolean
vqec_syscfg_checksum_is_valid(vqec_ifclient_config_t config);

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
vqec_syscfg_index_read(vqec_ifclient_config_t config,
                       char checksum[MD5_CHECKSUM_STRLEN]);

/*
 * Updates the index file with the given (config, checksum) values
 * supplied.  A checksum value of VQEC_SYSCFG_CHECKSUM_UNAVAILABLE
 * may be supplied to indicate that a config is not present.
 *
 * @param[in]  config       - configuration to be updated
 * @param[in]  checksum     - configuration's checksum
 * @param[out] vqec_error_t - VQEC_OK, or 
 *                            error code indicating failure reason
 */
vqec_error_t
vqec_syscfg_index_write(vqec_ifclient_config_t config,
                        char checksum[MD5_CHECKSUM_STRLEN]);

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
vqec_syscfg_read (vqec_syscfg_t *cfg);

/*
 * Returns a pointer to the system configuration.
 */
const vqec_syscfg_t *
vqec_syscfg_get_ptr(void);

/*
 * Copies the system configuration into the caller's cfg structure.
 *
 * @param[out] cfg   - structure into which the system config is copied
 *                     MUST be non-NULL.
 */
void
vqec_syscfg_get(vqec_syscfg_t *cfg);

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
vqec_syscfg_set(vqec_syscfg_t *cfg, boolean update);

/*
 * Initializes the VQE-C running configuration from a local file.
 *
 * Any parameters which are not assigned a valid value within the
 * file will be assigned a default value chosen by VQE-C.
 *
 * @param[in]     filename       - configuration file to load
 * @param[out]    vqec_error_t   - VQEC_OK if no invalid parameter settings
 *                                  are detected
 *                                 VQEC_ERR_CONFIG_NOT_FOUND if the supplied
 *                                  file could not be loaded
 *                                 VQEC_ERR_CONFIG_INVALID if the supplied
 *                                  file could not be parsed
 *                                 VQEC_ERR_PARAMRANGEINVALID if at least one
 *                                  invalid parameter setting was detected
 */
vqec_error_t
vqec_syscfg_init(const char *filename);

/*!
  @param[in] Pointer to the cname character array.
*/
boolean vqec_syscfg_register_cname(char *cname);

/**
 * get cname pointer
 */
const char * vqec_get_cname(void);

/*!
 * Set the rcc-max-backfill-scaler on the fly.
 * This is used to test the max-buff-fill of rcc
 * without reboot the STB.
 */
#if HAVE_FCC
void vqec_syscfg_set_rcc_pakpool_max_pct (uint32_t rcc_scaler);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VQEC_SYSCFG_H__ */
