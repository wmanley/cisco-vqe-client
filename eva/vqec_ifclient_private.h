//------------------------------------------------------------------
// Vqec. Client Interface Private Header File.
//
//
// Copyright (c) 2007-2009 by cisco Systems, Inc.
// All rights reserved.
//------------------------------------------------------------------

#ifndef __VQEC_IFCLIENT_PRIVATE_H__
#define __VQEC_IFCLIENT_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define VQEC_MAX_STREAMS_PER_CHANNEL 5  /* including rcc_extra_igmp socket */
#define VQEC_IFCLIENT_ERR_STRLEN 256

typedef
enum vqec_ifclient_errtype_t_
{
    VQEC_IFCLIENT_ERR_GENERAL,
    VQEC_IFCLIENT_ERR_MALLOC,
} vqec_ifclient_errtype_t;

/*
 * This is not a publicly exposed structure. The purpose of this
 * structure is for usage in internal functions, such as those for
 * printing the counters
 */
typedef
struct _common_stats_t
{
    VQE_STAT_COUNTERS
} vqec_ifclient_common_stats_t;

void vqec_ifclient_log_err (vqec_ifclient_errtype_t type, const char *format, ...);

void
vqec_ifclient_add_stats(vqec_ifclient_stats_t *total,
                        vqec_ifclient_stats_channel_t *stats);

vqec_error_t
vqec_ifclient_get_stats_tuner_legacy(vqec_tunerid_t id,
                                     vqec_ifclient_stats_channel_t *stats);

/*
 * Returns whether or not VQE-C Core Services are enabled.
 * "enabled" means that VQE-C Core Services initialization
 * was attempted and completed successfully.
 *
 * @param[out] boolean     - TRUE:  VQE-C is enabled/initialized
 *                           FALSE: otherwise
 */
uint8_t
vqec_ifclient_is_vqec_enabled_ul(void);

vqec_error_t
vqec_ifclient_config_status_ul(vqec_ifclient_config_status_params_t *params);

vqec_error_t
vqec_ifclient_config_register_ul(
    const vqec_ifclient_config_register_params_t *params);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __VQEC_IFCLIENT_PRIVATE_H__ */
