/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: DP top level manager.
 *
 * Documents: Controls the task of running the dataplane services.
 *
 *****************************************************************************/

#ifndef __VQEC_DP_TLM_H__
#define __VQEC_DP_TLM_H__


#include "vqec_dp_common.h"
#include <id_manager.h>
#include <queue_plus.h>
#include <vqec_pak.h>
#include <vqec_event.h>
#include <vqec_dp_api_types.h>
#include <vqec_dp_event_counter.h>

/**
 * Global lock acquistion, and release macros.
 * They are empty for a same-user-process implementation.
 */
#define VQEC_DP_TLM_ACQUIRE_LOCK(tlm)
#define VQEC_DP_TLM_RELEASE_LOCK(tlm)

/**
 * Debug counters that are maintained within the tlm.
 * Generate definitions for these counters.
 */
typedef 
struct vqec_dp_tlm_debug_cnt_ 
{
#define VQEC_DP_TLM_CNT_DECL(name,desc)         \
    vqec_dp_ev_cnt_t name;

    #include "vqec_dp_tlm_cnt_decl.h"
    #undef VQEC_DP_TLM_CNT_DECL

} vqec_dp_tlm_debug_cnt_t;

/**
 * Dataplane top-level manager object structure.
 */
typedef
struct vqec_dp_tlm_
{
    /**
     * Interval with which tlm invokes the input shim to process packets 
     */
    uint16_t polling_interval;
    /**
     * Event used to awake the TLM for invoking the input shim
     */
    vqec_event_t *polling_event;
    /**
     * Various debug counters and statistics.
     */
    vqec_dp_tlm_debug_cnt_t counters;

} vqec_dp_tlm_t;

/* 
 * Generate a bunch of inlines of the form vqec_dp_tlm_count_FOO to bump
 * the FOO counter.
 */ 
#define VQEC_DP_TLM_CNT_DECL(name,desc)                                 \
    static inline void vqec_dp_tlm_count_ ## name(vqec_dp_tlm_t * tlm)  \
    {                                                                   \
        vqec_dp_ev_cnt_cnt(&tlm->counters.name);                        \
    }                                                                   \

#include "vqec_dp_tlm_cnt_decl.h"
#undef VQEC_DP_TLM_CNT_DECL

/* 
 * Macro to bump one of the top level counters. Use UL version if you
 * already hold the top lock, otherwise non-ul version. 
 */
#define VQEC_DP_TLM_CNT(name, tlm) vqec_dp_tlm_count_ ## name(tlm)

/**
 * Gets a pointer to the global instance of the top level manager (TLM) object.
 * Callers should treat the object as opaque, and use the provided APIs
 * for any operations on the TLM.
 *
 * @param[out] vqec_dp_tlm_t * Pointer to the global TLM object
 */
vqec_dp_tlm_t *
vqec_dp_tlm_get(void);

#endif /* __VQEC_DP_TLM_H__ */
