/*
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *****************************************************************************
 *
 * File: vqec_tuner.h
 *
 * Description: Vqec tuner implementation.
 * 
 * VQE-C "tuner" is a conceptual abstraction defined to encompass the
 * pairing of an input and output stream.
 *
 * Documents:
 *
 *****************************************************************************/

#ifndef __VQEC_TUNER_H__
#define __VQEC_TUNER_H__

#include <utils/vam_types.h>
#if 0
#include <utils/queue_plus.h>
#include "vqec_url.h"
#endif
#include "vqec_ifclient_defs.h"
#include "vqec_channel_api.h"

#ifdef __cplusplus
extern "C" 
{
#endif /* __cplusplus */

vqec_error_t
vqec_tuner_create(vqec_tunerid_t *id, 
                  const char * name);
vqec_error_t
vqec_tuner_destroy(vqec_tunerid_t id);

vqec_error_t
vqec_tuner_bind_chan(vqec_tunerid_t id,
                     vqec_chanid_t chanid,
                     const vqec_bind_params_t *bp,
                     const vqec_ifclient_chan_event_cb_t *chan_event_cb);
vqec_error_t
vqec_tuner_unbind_chan(vqec_tunerid_t id);

vqec_tunerid_t
vqec_tuner_get_id_by_name(const char *name);

int
vqec_tuner_get_output_sock_fd(vqec_tunerid_t id);

const char *
vqec_tuner_get_name(vqec_tunerid_t id);

vqec_dp_tunerid_t
vqec_tuner_get_dptuner(vqec_tunerid_t id);

vqec_error_t
vqec_tuner_get_chan(vqec_tunerid_t id,
                    vqec_chanid_t *chanid);

vqec_tunerid_t 
vqec_tuner_get_first_tuner_id(void);

vqec_tunerid_t
vqec_tuner_get_next_tuner_id(vqec_tunerid_t cur);

void
vqec_tuner_add_stats(vqec_ifclient_stats_t *total,
                     vqec_ifclient_stats_channel_t *stats);

vqec_error_t
vqec_tuner_get_stats_global(vqec_ifclient_stats_t *stats, boolean cumulative);

vqec_error_t
vqec_tuner_get_stats_legacy(vqec_tunerid_t id, vqec_ifclient_stats_channel_t 
                            *stats);

void
vqec_tuner_clear_stats(void);

vqec_error_t
vqec_tuner_dump(vqec_tunerid_t id, unsigned int options_flag);

vqec_error_t
vqec_tuner_get_pat(vqec_tunerid_t id,
                   uint8_t *buf, uint32_t *pat_len, uint32_t buf_len);
vqec_error_t
vqec_tuner_get_pmt(vqec_tunerid_t id,
                   uint16_t *pid, uint8_t *buf, 
                   uint32_t *pmt_len, uint32_t buf_len);
vqec_error_t
init_vqec_tuner_module(uint32_t max_tuners);

void
vqec_tuner_module_deinit(void);

uint8_t 
vqec_tuner_is_fastfill_enabled(vqec_tunerid_t id);

uint8_t 
vqec_tuner_is_rcc_enabled(vqec_tunerid_t id);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VQEC_TUNER_H__ */
