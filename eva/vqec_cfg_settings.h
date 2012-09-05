/********************************************************************
 *
 * Copyright (c) 2007-2012 by cisco Systems, Inc.
 * All rights reserved.
 *
 *
 *********************************************************************/

#include "vqec_cfg_macros.h"

/*++++++++++++
 * Attributes are supported only when CDI is enabled.
 *++++++++++++*/

/*++++++++++++
 * IMPORTANT
 *++++++++++++
 *
 * This file is parsed by an automated perl-script, which relies on using "," 
 * as the field separators. Please do not add any "," to the Help strings.
 */

/*++++++++++++
 * SCHEMA VERSIONING RULES
 *++++++++++++
 *
 * The attributes version is defined as the combination of a major number,
 * also called the "attributes namespace identifier", and a minor number. The
 * following rules apply:
 *
 * (a) For the 1st release that contains the per-client attributes provisioning
 *      feature, the namespace id is 1, and the minor version is 0 (1.0).
 * (b) After the 1st release, whenever a developer is to add a new attribute
 *      he / she must first define a new namespace id, by adding 1 to the 
 *      previous id. The new attribute's namespaced id must then be set to
 *      this new namespace id. In addition the minor version number must
 *      be reset to 0.
 * (c) For changes that either impact documentation, i.e., help strings, or 
 *      change the update type of an attribute, i.e, when an attribute's 
 *      update value is utilized by the client, increase the minor version by 1.
 *
 * Example:
 *    #define VQEC_V1_ATTRIBUTES_NAMESPACE_ID 2
 *    #define VQEC_ATTRIBUTES_VERSION VQEC_V1_ATTRIBUTES_NAMESPACE_ID.0
 * would create a new attribute version which can then be included in the
 * schema by the perl script.
 */
#define VQEC_V0_ATTRIBUTES_NAMESPACE_ID 1
#define VQEC_V1_ATTRIBUTES_NAMESPACE_ID 2
#define VQEC_V2_ATTRIBUTES_NAMESPACE_ID 3
#define VQEC_V3_ATTRIBUTES_NAMESPACE_ID 4
#define VQEC_V4_ATTRIBUTES_NAMESPACE_ID 5
#define VQEC_ATTRIBUTES_VERSION VQEC_V4_ATTRIBUTES_NAMESPACE_ID.0

/*++++++++++++
 * RULES TO MODIFY AND / OR ADD ATTRIBUTES
 *++++++++++++
 *
 * (a) The relative-order of attribute definitions in this file cannot be changed.
 *       For example, vqec_enable must be immediately before jitter_buff_size
 *       for attribute parameters. This also means that if there are 2 params
 *       e.g., "network_cfg_pathname" which is a non-attribute, and 
 *       "vqec_enable" which is an attribute, and one wishes to make 
 *       "network_cfg_pathname" an attribute, "network_cfg_pathname" must 
 *       be moved to the end of this file immediately prior "must_be_last".
 *       The necessity of this is because of the forward compatibility 
 *       requirement imposed on the xml schema.
 * 
 * (b) Do not use pre-processor macros or definitions to describe the 
 *       constructor values, i.e., defaults, minimas and maximas. Again, this 
 *       is necessary for automatic generation of the schema from a single
 *       file, and not doing a recursive parse.
 *
 * (c) The use of update type is simply provided as a hint to the schema users
 *       to indicate when the attribute will be updated. The vcds has no actual
 *       means to control when a particular updated value is utilized, and any
 *       behavior described has to be correctly enforced by the client. 
 *
 * (d) It is not possible to modify the constructor arguments of an attribute.
 *       If you need to modify these defaults, the only way is to define a new
 *       attribute, and deprecate the old one.
 * 
 * (e) A new attribute must always be added to the end of this file.
 *
 * (f) If the description of any attribute is changed, or a new attribute is
 *       added, the script vqec_cfggen.pl must be used to regenerate the
 *       pre-processor definitions for the constructor constants, and also
 *       to regenerate the attribute schema.
 */


/*
 * Empty macros to satisfy the C pre-processor. These macros are primarily
 * utilized as tags to present information in a parser-friendly manner to a 
 * perl script which is used to generate pre-processor definitions and schemas.
 */

/*
 * Integer type constructor. Values are in the order:
 *    (default value, minimum limit, maximum limit)
 */
#define VQEC_UINT32_CONSTRUCTOR(defaultValue, minValue, maxValue)
/*
 * Integer range type constructor. Values are in the order:
 *    (default value, range1, range2...)
 * A range is a contiguous span of the integer-space.
 */
#define VQEC_UINT32_RANGE_CONSTRUCTOR(defaultValue, range1,...)
/*
 * String type constructor. Values are in the order:
 *    (default string, maximum length of string)
 */
#define VQEC_STRING_CONSTRUCTOR(defaultStr, maxStrLength)
/*
 * Boolean type constructor. Values are in the order:
 *    (default boolean value)
 */
#define VQEC_BOOL_CONSTUCTOR(defaultValue)
/*
 * Multicast address type constructor.  Values are in the order:
 *    (default multicast address value in UINT format)
 */
#define VQEC_MULTICAST_ADDR_CONSTRUCTOR(defaultValue)
#define __DEFAULT 
/*
 * A enumerated list-of-strings type constructor. This is a variadic macro.
 * Simply enumerate all the values, e.g., ("A", "B", ...). There must be 
 * exactly one default value in the list. Use the __DEFAULT define above to
 * tag the default value. For example, if there are 2 items, "A", "B", and "C" 
 * and "B" is the default use:
 *    ("A", __DEFAULT "B", "C")
 */
#define VQEC_STRING_LIST_CONSTRUCTOR(strA,...)

/* 
 * The following MACROS are used to define the global vqec global
 * configuration data which can be read from a configuration file.
 * Note the ARR_... behavior of the following table changes based on the
 * definition of the above macros.
 *
 * The fields of ARR_ELEM are defined as follows:
 * The first field indicates the configuration file parameter name.
 * The second field indicates is used to specify an enumeration
 * value that tracks the position of this element in the array.  A naming
 * convention of VQEC_CFG_<uppercase parameter_name> is used to make
 * tracking of values easy.
 * The third field indicates its type.
 * The fourth field provides a descriptive string used for CLI help.
 * The fifth field defines whether the parameter may be configured via
 * a per-client attribute update.
 * The sixth field defines whether the parameter may be configured via
 * a configuration override.
 * 
 * Do not add elements after "must_be_last" or before "max_tuners" which must
 * be first.
 */
ARR_BEGIN(vqec_cfg_arr)
/* max_tuners must be first */
ARR_ELEM("max_tuners",           VQEC_CFG_MAX_TUNERS,
         VQEC_TYPE_UINT32_T, "Maximum number of simultaneous streams "
         "that may be supported",
         FALSE, 
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(1, 1, 32),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("channel_lineup",       VQEC_CFG_CHANNEL_LINEUP,
         VQEC_TYPE_STRING,   "VQEC Channel lineup configuration filename",
         FALSE,
         FALSE, 
         VQEC_STRING_CONSTRUCTOR("", 255),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("network_cfg_pathname",   VQEC_CFG_NETWORK_CFG_PATHNAME,
         VQEC_TYPE_STRING,   "VQEC Per-client Network configuration filename",
         FALSE,
         FALSE, 
         VQEC_STRING_CONSTRUCTOR("", 255),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("override_cfg_pathname",   VQEC_CFG_OVERRIDE_CFG_PATHNAME,
         VQEC_TYPE_STRING,   "VQEC Override configuration filename",
         FALSE,
         FALSE, 
         VQEC_STRING_CONSTRUCTOR("", 255),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("index_cfg_pathname",   VQEC_CFG_INDEX_CFG_PATHNAME,
         VQEC_TYPE_STRING,   "VQEC Configuration Index filename",
         FALSE,
         FALSE, 
         VQEC_STRING_CONSTRUCTOR("", 255),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("vqec_enable",          VQEC_CFG_VQEC_ENABLE,
         VQEC_TYPE_BOOLEAN,   "Controls whether VQE-C is enabled. If false "
         "VQE-C will be disabled and will not use any resources. If CDI is "
         "disabled VQE-C may check for updates to the system "
         "configuration file. (deprecated:  use qoe_enable)",
         TRUE,
         TRUE,
         VQEC_BOOL_CONSTRUCTOR(TRUE),
         VQEC_UPDATE_STARTUP,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_DEPRECATED)
ARR_ELEM("jitter_buff_size",     VQEC_CFG_JITTER_BUFF_SIZE,
         VQEC_TYPE_UINT32_T, "RTP jitter buffer size in ms",
         TRUE,
         TRUE,
         VQEC_UINT32_CONSTRUCTOR(200, 0, 20000),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("repair_trigger_point_abs", VQEC_CFG_REPAIR_TRIGGER_POINT_ABS,
         VQEC_TYPE_UINT32_T,
         "RTP repair trigger point in ms",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(20, 0, 200),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
/*
 * "repair_trigger_point" must come after 
 *    "repair_trigger_point_abs" and
 *    "jitter_buff_size"
 */
ARR_ELEM("repair_trigger_point", VQEC_CFG_REPAIR_TRIGGER_POINT,
         VQEC_TYPE_UINT32_T,
         "RTP repair trigger point pct of jitter-buff-size [0..100] "
         "(deprecated: use repair_trigger_point_abs)",
         FALSE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(20, 0, 100),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_DEPRECATED)
ARR_ELEM("pakpool_size",        VQEC_CFG_PAKPOOL_SIZE,
         VQEC_TYPE_UINT32_T, "Number of pak pool packets to create",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(1000, 100, 200000),
         VQEC_UPDATE_STARTUP,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("so_rcvbuf",        VQEC_CFG_SO_RCVBUF,
         VQEC_TYPE_UINT32_T, "Socket receive buffer size in bytes",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(128000, 0, 1000000),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("strip_rtp",            VQEC_CFG_STRIP_RTP,
         VQEC_TYPE_BOOLEAN,  "Strip the rtp header [true/false]",
         FALSE,
         FALSE, 
         VQEC_BOOL_CONSTRUCTOR(TRUE),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("input_ifname",         VQEC_CFG_INPUT_IFNAME,
         VQEC_TYPE_STRING,   "Input interface name. Specifying a NULL "
         "interface name allows the OS to pick a default input interface for "
         "the stream",
         FALSE,
         FALSE, 
         VQEC_STRING_CONSTRUCTOR("", 255),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("sig_mode",             VQEC_CFG_SIG_MODE,
         VQEC_TYPE_STRING,   "Signalling mode [nat/std]. It is recommended "
         "that one uses NAT signalling even when not behind a NAT. Always "
         "using NAT mode is preferred as it removes the channel configuration "
         "restrictions imposed by STD mode signalling that require that all RTP "
         "and RTCP ports in the configuration must be unused on the STB.  Note "
         "that NAT signalling does not increase network load as keepalives "
         "are disabled when the box is not behind a NAT.",
         TRUE,
         FALSE, 
         VQEC_STRING_LIST_CONSTRUCTOR(__DEFAULT "nat", "std"),
         VQEC_UPDATE_STARTUP,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("nat_binding_refresh_interval",
         VQEC_CFG_NAT_BINDING_REFRESH_INTERVAL,VQEC_TYPE_UINT32_T,
         "NAT binding refresh interval in secs",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(30, 0, 100000),
         VQEC_UPDATE_STARTUP,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("nat_filter_refresh_interval",
         VQEC_CFG_NAT_FILTER_REFRESH_INTERVAL,VQEC_TYPE_UINT32_T,
         "OBSOLETE in 2.1: NAT filter refresh interval in s.",
         FALSE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(30, 0, 100000),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_OBSOLETE)
ARR_ELEM("max_paksize",          VQEC_CFG_MAX_PAKSIZE,
         VQEC_TYPE_UINT32_T, "Maximum pakpool buffer size in bytes. ",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(1508, 1330, 10000),
         VQEC_UPDATE_STARTUP,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("stun_server_ip",       VQEC_CFG_STUN_SERVER_IP,
         VQEC_TYPE_UINT32_T, "OBSOLETE in 2.1: STUN server ip. ",
         FALSE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(0, 0, (1<<32) - 1),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_OBSOLETE)
ARR_ELEM("stun_server_port",     VQEC_CFG_STUN_SERVER_PORT,
         VQEC_TYPE_UINT32_T, "OBSOLETE in 2.1: STUN server port. ",
         FALSE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(0, 1000, (1<<16) - 1),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_OBSOLETE)
ARR_ELEM("cdi_enable",           VQEC_CFG_CDI_ENABLE,
         VQEC_TYPE_BOOLEAN,  "Enable configuration delivery infrastructure.",
         FALSE,
         FALSE, 
         VQEC_BOOL_CONSTRUCTOR(FALSE),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("domain_name_override", VQEC_CFG_DOMAIN_NAME_OVERRIDE,
         VQEC_TYPE_STRING,
         "Domain name to search rstp server and stun server",
         FALSE,
         FALSE, 
         VQEC_STRING_CONSTRUCTOR("", 255),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("libcli_telnet_port",   VQEC_CFG_CLI_TELNET_PORT,
         VQEC_TYPE_UINT32_T, "Telnet port for cli. A telnet port of 0 has "
         "a special meaning. It means do not allow cli access to VQE-C. The "
         "CLI may be disabled in production systems. It's main use is easy "
         "access to debug information especially in lab environments and "
         "trials",
         TRUE,
         FALSE, 
         VQEC_UINT32_RANGE_CONSTRUCTOR(0, 0:0, 8000:(1<<16) - 1),
         VQEC_UPDATE_STARTUP,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("output_pakq_limit",    VQEC_CFG_OUTPUT_PAKQ_LIMIT,
         VQEC_TYPE_UINT32_T, "Output packet queue limit in UDP packets",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(200, 100, 20000),
         VQEC_UPDATE_STARTUP,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("update_window",   VQEC_CFG_UPDATE_WINDOW,
         VQEC_TYPE_UINT32_T, "Time-window (in sec) during which to schedule"
         " random updates",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(60, 1, 100000),
         VQEC_UPDATE_IMMEDIATE,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("update_interval_max",   VQEC_CFG_UPDATE_INTERVAL_MAX,
         VQEC_TYPE_UINT32_T, "Maximum time (in sec) between polled updates",
         TRUE,
         FALSE, 
         VQEC_UINT32_RANGE_CONSTRUCTOR(3600, 0:0, 30:604800),
         VQEC_UPDATE_IMMEDIATE,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("app_paks_per_rcc",   VQEC_CFG_APP_PAKS_PER_RCC,
         VQEC_TYPE_UINT32_T, "Number of APP packets to send in output just"
         " after an RCC",
         FALSE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(1, 1, 20),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("error_repair_enable",   VQEC_CFG_ERROR_REPAIR_ENABLE,
         VQEC_TYPE_BOOLEAN, "Controls whether or not error repair is enabled",
         TRUE,
         TRUE,
         VQEC_BOOL_CONSTRUCTOR(TRUE),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("error_repair_policer.enable",   VQEC_CFG_ERROR_REPAIR_POLICER_ENABLE,
         VQEC_TYPE_BOOLEAN, "Controls whether or not error repair policing"
         " is enabled",
         TRUE,
         FALSE, 
         VQEC_BOOL_CONSTRUCTOR(FALSE),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("error_repair_policer.rate",   VQEC_CFG_ERROR_REPAIR_POLICER_RATE,
         VQEC_TYPE_UINT32_T, "Policing rate for error repairs (expressed as a "
         "percentage of stream rate)",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(5, 1, 100),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("error_repair_policer.burst",   VQEC_CFG_ERROR_REPAIR_POLICER_BURST,
         VQEC_TYPE_UINT32_T, "Policing burst for error repairs (expressed as "
         "a duration of time at the policing rate)",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(10000, 1, 60000),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("fec_enable",   VQEC_CFG_FEC_ENABLE,
         VQEC_TYPE_BOOLEAN, "Controls whether or not FEC is enabled",
         TRUE,
         TRUE, 
         VQEC_BOOL_CONSTRUCTOR(TRUE),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("rcc_enable",   VQEC_CFG_RCC_ENABLE,
         VQEC_TYPE_BOOLEAN, "Controls whether or not RCC is enabled",
         TRUE,
         TRUE, 
         VQEC_BOOL_CONSTRUCTOR(TRUE),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("rcc_pakpool_max_pct",   VQEC_CFG_RCC_PAKPOOL_MAX_PCT,
         VQEC_TYPE_UINT32_T, "The max pakpool pct allocated to RCC",
         FALSE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(80, 0, 100),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("rcc_start_timeout",   VQEC_CFG_RCC_START_TIMEOUT,
         VQEC_TYPE_UINT32_T, "The RCC start time out value in msecs",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(120, 0, 1000),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("num_byes", VQEC_CFG_NUM_BYES,
         VQEC_TYPE_UINT32_T, "Number of BYEs needs to be sent",
         FALSE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(2, 2, 5),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("bye_delay", VQEC_CFG_BYE_DELAY,
         VQEC_TYPE_UINT32_T, "The minimum delay in ms between BYEs",
         FALSE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(40, 10, 100),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("reorder_delay_abs", VQEC_CFG_REORDER_DELAY_ABS,
         VQEC_TYPE_UINT32_T, "Time (in ms) to hold gaps before doing "
         "retransmission-based error-repair",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(20, 0, 200),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
/*
 * "reorder_delay" must come after
 *   o "reorder_delay_abs"
 *   o "jitter_buff_size"
 */
ARR_ELEM("reorder_delay", VQEC_CFG_REORDER_DELAY,
         VQEC_TYPE_UINT32_T, "Time (pct of jitter_buff_size [0..100]) to hold "
         "gaps before doing retransmission-based error-repair (deprecated: use "
         "reorder_delay_abs)",
         FALSE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(20, 0, 100),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_DEPRECATED)
ARR_ELEM("vcds_server_ip",       VQEC_CFG_VCDS_SERVER_IP,
         VQEC_TYPE_UINT32_T, "Manually configured VCDS server IP.",
         FALSE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(0, 0, (1<<32) - 1),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("vcds_server_port",     VQEC_CFG_VCDS_SERVER_PORT,
         VQEC_TYPE_UINT32_T, "Manually configured VCDS server port.",
         FALSE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(8554, 1024, (1<<16) - 1),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("cli_ifname",      VQEC_CFG_CLI_IFNAME,
         VQEC_TYPE_STRING, "Interface name on which CLI will listen. "
         "Specifying a NULL interface name allows the CLI to listen on all "
         "configured interfaces",
         TRUE,
         FALSE, 
         VQEC_STRING_CONSTRUCTOR("lo", 255),
         VQEC_UPDATE_STARTUP,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("tuner_list",           VQEC_CFG_TUNER_LIST,
         VQEC_TYPE_STRING,   "List of tuners to create at startup and"
         " their parameters",
         FALSE,
         FALSE, 
         VQEC_STRING_CONSTRUCTOR("", 255),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("rtcp_dscp_value",      VQEC_CFG_RTCP_DSCP_VALUE,
         VQEC_TYPE_UINT32_T, "IP DSCP Value for an RTCP Payload. The "
         "default value is set to CS3 (precedence 3). If a thread does not "
         "have sufficient privilege to change the DSCP value the default "
         "DSCP value of 0 or best-effort is used.",
         TRUE,
         FALSE, 
         VQEC_UINT32_CONSTRUCTOR(24, 0, 63),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("deliver_paks_to_user",      VQEC_CFG_DELIVER_PAKS_TO_USER,
         VQEC_TYPE_BOOLEAN, "Determines whether kernel DP packets should remain"
         " in kernel or be accessible via the userspace API",
         FALSE,
         FALSE, 
         VQEC_BOOL_CONSTRUCTOR(TRUE),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("fastfill_enable",   VQEC_CFG_FASTFILL_ENABLE,
         VQEC_TYPE_BOOLEAN, "Controls whether or not fastfill is enabled",
         TRUE,
         FALSE, 
         VQEC_BOOL_CONSTRUCTOR(TRUE),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("max_receive_bandwidth_sd",      VQEC_CFG_MAX_RECEIVE_BANDWIDTH_SD,
         VQEC_TYPE_UINT32_T, "Total max receive bandwidth value for an SD "
         "channel.  Includes primary stream and FEC stream (if applicable) and "
         "VQE services (error repair and rcc).  A value of 0 will allow the "
         "server to use its own configured value for max receive bandwidth "
         "unless otherwise specified in the bind() API.",
         TRUE,
         TRUE,
         VQEC_UINT32_CONSTRUCTOR(0, 0, 1000000000 /* 1 Gb */),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V1_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("max_receive_bandwidth_sd_rcc",  VQEC_CFG_MAX_RECEIVE_BANDWIDTH_SD_RCC,
         VQEC_TYPE_UINT32_T, "The total max receive bandwidth value "
         "for an SD channel during RCC only.  Includes primary stream and FEC "
         "stream (if applicable).  A value of 0 will fall back to using the "
         "value of max_receive_bandwidth_sd for the RCC case unless otherwise "
         "specified in the bind() API.",
         TRUE,
         TRUE,
         VQEC_UINT32_CONSTRUCTOR(0, 0, 1000000000 /* 1 Gb */),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V2_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("max_receive_bandwidth_hd",      VQEC_CFG_MAX_RECEIVE_BANDWIDTH_HD,
         VQEC_TYPE_UINT32_T, "Total max receive bandwidth value for an HD "
         "channel.  Includes primary stream and FEC stream (if applicable) and "
         "VQE services (error repair and rcc).  A value of 0 will allow the "
         "server to use its own configured value for max receive bandwidth "
         "unless otherwise specified in the bind() API.",
         TRUE,
         TRUE,
         VQEC_UINT32_CONSTRUCTOR(0, 0, 1000000000 /* 1 Gb */),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V1_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("max_receive_bandwidth_hd_rcc",  VQEC_CFG_MAX_RECEIVE_BANDWIDTH_HD_RCC,
         VQEC_TYPE_UINT32_T, "The total max receive bandwidth value "
         "for an HD channel during RCC only.  Includes primary stream and FEC "
         "stream (if applicable).  A value of 0 will fall back to using the "
         "value of max_receive_bandwidth_hd for the RCC case unless otherwise "
         "specified in the bind() API.",
         TRUE,
         TRUE,
         VQEC_UINT32_CONSTRUCTOR(0, 0, 1000000000 /* 1 Gb */),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V2_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("min_hd_stream_bitrate",      VQEC_CFG_MIN_HD_STREAM_BITRATE,
         VQEC_TYPE_UINT32_T, "Value to be used to determine whether a given "
         "stream is SD or HD.  If the stream's bitrate is less than this "
         "value it is considered SD; if it is greater than or equal it is "
         "considered HD.",
         TRUE,
         TRUE,
         VQEC_UINT32_CONSTRUCTOR(0, 0, 1000000000 /* 1 Gb */),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V1_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("max_fastfill",      VQEC_CFG_MAX_FASTFILL,
         VQEC_TYPE_UINT32_T, "Global max amount of video data (in bytes) to be "
         "fastfilled into the decoder HW buffer.",
         TRUE,
         FALSE,
         VQEC_UINT32_CONSTRUCTOR(0, 0, 1000000000 /* 1 GB */),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V1_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("app_delay",      VQEC_CFG_APP_DELAY,
         VQEC_TYPE_UINT32_T, "Specifies the interpacket output delay (in msec) "
         "between replicated APP packets.",
         TRUE,
         FALSE,
         VQEC_UINT32_CONSTRUCTOR(0, 0, 60000 /* 60 seconds */),
         VQEC_UPDATE_STARTUP,
         VQEC_V1_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("src_ip_filter_enable", VQEC_CFG_SRC_IP_FILTER_ENABLE,
         VQEC_TYPE_BOOLEAN,  "Enable Source IP Filter for Channel Streams.",
         TRUE,
         FALSE,
         VQEC_BOOL_CONSTRUCTOR(FALSE),
         VQEC_UPDATE_STARTUP,
         VQEC_V1_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("qoe_enable",          VQEC_CFG_QOE_ENABLE,
         VQEC_TYPE_BOOLEAN, "Controls whether VQE-C is enabled. If false "
         "VQE-C will be disabled and will not use any resources. If CDI is "
         "disabled VQE-C may check for updates to the system "
         "configuration file.",
         TRUE,
         TRUE,
         VQEC_BOOL_CONSTRUCTOR(TRUE),
         VQEC_UPDATE_STARTUP,
         VQEC_V2_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("rcc_extra_igmp_ip",       VQEC_CFG_RCC_EXTRA_IGMP_IP,
         VQEC_TYPE_UINT32_T, "IP address of extra multicast group to be "
         "joined during RCC burst.",
         TRUE,
         TRUE, 
         VQEC_MULTICAST_ADDR_CONSTRUCTOR(0),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V3_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("rcc_max_concurrent",       VQEC_CFG_RCC_MAX_CONCURRENT,
         VQEC_TYPE_UINT32_T, "The maximum number of channels that may be "
         "simultaneously bound with RCC.",
         FALSE,
         FALSE,
         VQEC_UINT32_CONSTRUCTOR(1, 0, 32),
         VQEC_UPDATE_INVALID,
         VQEC_V3_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("integrated_rtp_fallback",       VQEC_CFG_INTEGRATED_RTP_FALLBACK,
         VQEC_TYPE_BOOLEAN, "When TRUE, enables the creation of dynamic "
         "channels for RTP channels not in the SDP channel database.",
         FALSE,
         FALSE, 
         VQEC_BOOL_CONSTRUCTOR(TRUE),
         VQEC_UPDATE_INVALID,
         VQEC_V3_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("udp_passthru_support",       VQEC_CFG_UDP_PASSTHRU_SUPPORT,
         VQEC_TYPE_BOOLEAN, "When TRUE, enables the creation of dynamic "
         "channels for UDP channels, so that UDP video traffic will be passed "
         "through VQE-C.",
         FALSE,
         FALSE, 
         VQEC_BOOL_CONSTRUCTOR(FALSE),
         VQEC_UPDATE_INVALID,
         VQEC_V3_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("vod_cmd_timeout",       VQEC_CFG_VOD_CMD_TIMEOUT,
         VQEC_TYPE_UINT32_T, "VOD RTSP response timeout in ms. "
         "The amount of time the client will wait for an RTSP response "
         "after sending an RTSP request",
         TRUE,
         TRUE,
         VQEC_UINT32_CONSTRUCTOR(2000, 100, 10000),
         VQEC_UPDATE_NEWCHANCHG,
         VQEC_V3_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT) 
ARR_ELEM("vod_max_sessions",       VQEC_CFG_VOD_MAX_SESSIONS,
         VQEC_TYPE_UINT32_T, "Max vod sessions supported. "
         "The max number of vod session supported in this box",
         TRUE,
         FALSE,
         VQEC_UINT32_CONSTRUCTOR(1, 1, 32),
         VQEC_UPDATE_STARTUP,
         VQEC_V3_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT) 
ARR_ELEM("vod_mode",             VQEC_CFG_VOD_MODE,
         VQEC_TYPE_STRING,   "VOD working mode. "
         "Two types of modes are supported: IPTV and CABLE",
         TRUE,
         FALSE, 
         VQEC_STRING_LIST_CONSTRUCTOR(__DEFAULT "iptv", "cable"),
         VQEC_UPDATE_STARTUP,
         VQEC_V3_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("log_level",      VQEC_CFG_LOG_LEVEL,
         VQEC_TYPE_UINT32_T,   "Log level from 0 to 7",
         TRUE,
         TRUE, 
         VQEC_UINT32_CONSTRUCTOR(4, 0, 7),
         VQEC_UPDATE_IMMEDIATE,
         VQEC_V4_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("stun_optimization",       VQEC_CFG_STUN_OPTIMIZATION,
         VQEC_TYPE_BOOLEAN, "When TRUE, enables stun signaling optimization,"
         "i.e no stun signaling after detecting not behind nat; "
         "when FALSE, stun signaling for each channel change.",
         FALSE,
         FALSE, 
         VQEC_BOOL_CONSTRUCTOR(FALSE),
         VQEC_UPDATE_INVALID,
         VQEC_V4_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_ELEM("must_be_last",         VQEC_CFG_MUST_BE_LAST,
         VQEC_TYPE_STRING,   "Don't add after this",
         FALSE,      /* Must be last */
         FALSE,
         VQEC_STRING_CONSTRUCTOR("", 0),
         VQEC_UPDATE_INVALID,
         VQEC_V0_ATTRIBUTES_NAMESPACE_ID,
         VQEC_PARAM_STATUS_CURRENT)
ARR_DONE

