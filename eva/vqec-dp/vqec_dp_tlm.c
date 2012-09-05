/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: DP top level manager implementation.
 *
 * Documents: Controls the task of running the dataplane services. 
 *
 *****************************************************************************/

#include "vqec_dp_tlm.h"
#include "vqec_dp_graph.h"
#include "vqec_dp_utils.h"
#include <vqec_dp_input_shim_api.h>
#include <vqec_dp_output_shim_api.h>
#include <vqec_dp_rtp_input_stream.h>
#include <vqec_dpchan_api.h>
#include "vqec_dp_common.h"

#ifdef _VQEC_DP_UTEST
#define UT_STATIC
#else
#define UT_STATIC static
#endif

/*
 * Temporary storage for VQEC_DP_ASSERT_FATAL macro (in vam_types.kmod.h).
 * This is only necessary in kernel mode.
 */
#if __KERNEL__
char g_vqec_assertbuff[ASSERT_MAX_BUFF];
#endif  /* __KERNEL__ */

/*
 * Temporary storage used for building debug strings
 */
#define DEBUG_STR_LEN 120
static char s_debug_str[DEBUG_STR_LEN];

static vqec_dp_tlm_t s_vqec_dp_tlm_info_mem;
UT_STATIC vqec_dp_tlm_t *s_vqec_dp_tlm_info = NULL;
static rel_time_t vqec_dp_default_polling_interval =
    VQEC_DP_DEFAULT_POLL_INTERVAL;
static uint16_t vqec_dp_fast_sched_interval =
    VQEC_DP_FAST_SCHEDULE_INTERVAL_MSEC;
static uint16_t vqec_dp_slow_sched_interval =
    VQEC_DP_SLOW_SCHEDULE_INTERVAL_MSEC;

/**
 * Set the default DP polling interval.
 *
 * @param[in] interval  Default polling interval (in ms).
 */
void vqec_dp_tlm_set_default_polling_interval (uint16_t interval) {
    vqec_dp_default_polling_interval = TIME_MK_R(msec, interval);
}

/**
 * Set the DP scheduling interval for fast-scheduled tasks.
 *
 * @param[in] interval  Fast-schedule interval (in ms).
 */
void vqec_dp_tlm_set_fast_sched_interval (uint16_t interval) {
    vqec_dp_fast_sched_interval = interval;
}

/**
 * Set the DP scheduling interval for slow-scheduled tasks.
 *
 * @param[in] interval  Slow-schedule interval (in ms).
 */
void vqec_dp_tlm_set_slow_sched_interval (uint16_t interval) {
    vqec_dp_slow_sched_interval = interval;
}

/**
 * Deinitializes the top level manager module.
 *
 * NOTE:  This function does NOT assume that the TLM is in a fully
 *        initialized state (e.g. it may be partially initialized if
 *        initialiation failed).  The uninitialization code below must
 *        ensure that that either:
 *          o each piece of TLM state exists before uninitializing it, or
 *          o the function called to do uninitialization will be harmless
 *            if no initialized state exists.
 */
void
vqec_dp_tlm_deinit_module_internal (void)
{
    if (!s_vqec_dp_tlm_info) {
        return;
    }

    /* Delete polling event and shut down the data plane modules */
    vqec_dp_graph_deinit_module();
    vqec_dp_chan_rtp_module_deinit();
    vqec_dpchan_module_deinit();
    vqec_dp_input_shim_shutdown();
    vqec_dp_output_shim_shutdown();
    (void)vqec_event_stop(s_vqec_dp_tlm_info->polling_event);
    vqec_event_destroy(&s_vqec_dp_tlm_info->polling_event);
    (void)vqec_dp_close_upcall_sockets();
    vqec_pak_pool_destroy();

    /* 
     * These two *MUST* be last in deinit order; first stop the event
     * loop and then deinit the event library. 
     */
    (void)vqec_dp_event_loop_exit();
    vqec_dp_event_deinit();
    
    /* Delete global tlm object */ 
    s_vqec_dp_tlm_info = NULL;
}


/**
 * Gets a pointer to the global instance of the top level manager (TLM) object.
 * Callers should treat the object as opaque, and use the provided APIs
 * for any operations on the TLM.
 *
 * @param[out] vqec_dp_tlm_t * Pointer to the global TLM object
 */
vqec_dp_tlm_t *
vqec_dp_tlm_get (void)
{
    return (s_vqec_dp_tlm_info);
}

/**
 * Event handler callback for the TLM polling event.
 *
 * @param[in] evptr  - pointer to libevent structure.
 * @param[in] fd     - not used
 * @param[in] event  - not used
 * @param[in] arg    - ptr to polling interval (in ms)
 */
void
vqec_dp_tlm_event_handler (const vqec_event_t *const evptr,
                           int32_t fd,
                           int16_t event,
                           void *arg)
{
    abs_time_t cur_time;

    vqec_dp_input_shim_run_service(*(uint16_t *)arg);
    cur_time = get_sys_time();
    vqec_dpchan_poll_ev_handler(cur_time);
}

/* wait 20 msec * 100 times = 2 seconds total */
#define VQEC_DP_EVENT_WAIT_EXIT_TIMEOUT_MS 20
#define VQEC_DP_EVENT_WAIT_EXIT_MAX_COUNT 100

/**
 * Initialize the data plane's top level manager (tlm) module. 
 * The TLM must be initialized prior to use.  It may be re-initialized, but
 * the implementation is the same as deinitializing and then successively
 * initializing again.  The state prior to re-initialization will be completely
 * erased.
 * The tlm module has the following responsibilities:
 *    - initialize the input shim and periodically invoke it
 *      to process arriving data packets
 *    - supplies APIs for creating and deleting IDs for DP object state,
 *       converting IDs to pointers, and inserting channel objects into
 *       a database 
 *
 * @params[in/out] params  Supplies initialization parameters to the dataplane.
 *                         Some fields in this structure will be updated by
 *                         the data-plane, as the control-plane is not aware
 *                         of all of these fields.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK upon success or an
 *                               error code identifying the error otherwise
 */
vqec_dp_error_t
vqec_dp_init_module (vqec_dp_module_init_params_t *params)
{
    vqec_dp_error_t status = VQEC_DP_ERR_OK;
    vqec_dp_error_t status_input_shim, status_output_shim,
        status_rtp_module, status_chan_module, status_graph_module;
    struct timeval tv;
    uint32_t polling_interval;
    uint32_t event_wait_cnt = 0;

    if (s_vqec_dp_tlm_info) {
        /* module has already been initialized, so deinitialize it first */
        vqec_dp_tlm_deinit_module_internal();
    }

    /* Validate parameters */
    if (!params) {
        status = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    polling_interval = TIME_GET_R(msec, vqec_dp_default_polling_interval);
    VQEC_DP_ASSERT_FATAL(polling_interval < USHRT_MAX,
                         "poll interval must be less than 65535 msecs");

    params->polling_interval = vqec_dp_default_polling_interval;

    /* Make sure that the event library is completely stopped first */
    while (vqec_dp_event_get_state() != VQEC_EVENT_STATE_STOPPED) {   
        if (event_wait_cnt > VQEC_DP_EVENT_WAIT_EXIT_MAX_COUNT) {
            status = VQEC_DP_ERR_ALREADY_INITIALIZED;
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_TLM, "dp_init: already initted\n");
            goto done;
        }
        vqec_dp_event_sleep(VQEC_DP_EVENT_WAIT_EXIT_TIMEOUT_MS);
        event_wait_cnt++;
    }

    /* Validation succeeded, try to init the tlm module */
    s_vqec_dp_tlm_info = &s_vqec_dp_tlm_info_mem;
    memset(s_vqec_dp_tlm_info, 0, sizeof(vqec_dp_tlm_t));    
    s_vqec_dp_tlm_info->polling_interval = 
        TIME_GET_R(msec, params->polling_interval);

    /* Initialize the app-delay parameter */
    vqec_dp_set_app_cpy_delay(params->app_cpy_delay);

    /* Initialize the event library. */
#define VQEC_DP_TIMERS_PER_CHANNEL 4
    if (!vqec_dp_event_init(params->max_channels * 
                            VQEC_DP_TIMERS_PER_CHANNEL)) {
        status = VQEC_DP_ERR_NOMEM;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_TLM, "dp_init: dp_event_init fail\n");
        goto done;
    }
    /* Start the event poll loop */
    if (!vqec_dp_event_loop_enter() ||
        !vqec_dp_event_loop_dispatch()) {
        status = VQEC_DP_ERR_INTERNAL;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_TLM,
                      "dp_init: loop_enter or loop_dispatch fail\n");
        goto done;
    }

    /* Init paks and pools */
    vqec_pak_pool_create("VQE-C_pak_pool",
                         params->max_paksize,
                         params->pakpool_size);

    /* Initialize counters */
#define VQEC_DP_TLM_CNT_DECL(name,descr)                        \
    vqec_dp_ev_cnt_init(&(s_vqec_dp_tlm_info->counters.name), NULL, NULL, 0);
#include "vqec_dp_tlm_cnt_decl.h"
#undef VQEC_DP_TLM_CNT_DECL
    

    /* Initialize the input shim */
    params->scheduling_policy.max_classes = 
        VQEC_DP_NUM_SCHEDULING_CLASSES;
    params->scheduling_policy.polling_interval[VQEC_DP_FAST_SCHEDULE] = 
        vqec_dp_fast_sched_interval;
    params->scheduling_policy.polling_interval[VQEC_DP_SLOW_SCHEDULE] = 
        vqec_dp_slow_sched_interval;
    status_input_shim = vqec_dp_input_shim_startup(params);
    if (status_input_shim != VQEC_DP_ERR_OK) {
        status = VQEC_DP_ERR_INTERNAL;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_TLM, "dp_init: input_shim_startup fail\n");
        goto done;
    }

    /* Initialize the output shim */
    status_output_shim = vqec_dp_output_shim_startup(params);
    if (status_output_shim != VQEC_DP_ERR_OK) {
        status = VQEC_DP_ERR_INTERNAL;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_TLM, "dp_init: output_shim_startup fail\n");
        goto done;
    }

    /* Initialize the DP channel module */
    status_chan_module = vqec_dpchan_module_init(params);
    if (status_chan_module != VQEC_DP_ERR_OK) {
        status = VQEC_DP_ERR_INTERNAL;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_TLM, "dp_init: dpchan_module_init fail\n");
        goto done;
    }

    /* Initialize the DP RTP module */
    status_rtp_module = vqec_dp_chan_rtp_module_init(params);
    if (status_rtp_module != VQEC_DP_ERR_OK) {
        status = VQEC_DP_ERR_INTERNAL;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_TLM,
                      "dp_init: dp_chan_rtp_module_init fail\n");
        goto done;
    }

    /* Initialize the graph module */
    status_graph_module = vqec_dp_graph_init_module(params);
    if (status_graph_module != VQEC_DP_ERR_OK) {
        status = VQEC_DP_ERR_INTERNAL;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_TLM,
                      "dp_init: dp_graph_init_module fail\n");
        goto done;
    }

    /*
     * Create and start an event that awakes each polling interval
     * to allow the input shim to process packets.
     */
    tv = rel_time_to_timeval(params->polling_interval);
    if (!vqec_event_create(&s_vqec_dp_tlm_info->polling_event,
                           VQEC_EVTYPE_TIMER, 
                           VQEC_EV_RECURRING, 
                           vqec_dp_tlm_event_handler, 
                           VQEC_EVDESC_TIMER, 
                           &s_vqec_dp_tlm_info->polling_interval) ||
        !vqec_event_start(s_vqec_dp_tlm_info->polling_event, &tv)) {
        status = VQEC_DP_ERR_INTERNAL;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_TLM,
                      "dp_init: event_create or event_start fail\n");
        goto done;
    }

done:
    if (status != VQEC_DP_ERR_OK) {
        vqec_dp_tlm_deinit_module_internal();
    }
    if (status != VQEC_DP_ERR_OK) {
        snprintf(s_debug_str, DEBUG_STR_LEN, ": %s", vqec_dp_err2str(status));
    } else {
        s_debug_str[0] = '\0';
    }
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_TLM, "%s %s\n", __FUNCTION__, s_debug_str);
    return (status);
}

/**
 * De-initializes the data plane's top level manager (tlm) module. 
 */
vqec_dp_error_t
vqec_dp_deinit_module (void)
{
    vqec_dp_tlm_deinit_module_internal();
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_TLM, "%s\n", __FUNCTION__);
    return (VQEC_DP_ERR_OK);
}

vqec_dp_error_t
vqec_dp_open_module (void)
{
    return (VQEC_DP_ERR_OK);
}

vqec_dp_error_t
vqec_dp_close_module (void)
{
    return (VQEC_DP_ERR_OK);
}


/**---------------------------------------------------------------------------
 * Get the status of the global pak pool.
 *
 * @param[out] status Output status for the pool.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_get_default_pakpool_status (vqec_pak_pool_status_t *status)
{
    vqec_pak_pool_err_t err;

    if (!s_vqec_dp_tlm_info || !status) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    err = vqec_pak_pool_get_status(status);
    if (err != VQEC_PAK_POOL_ERR_OK) {
        return (VQEC_DP_ERR_INTERNAL);
    }
    
    return (VQEC_DP_ERR_OK);
}


/**---------------------------------------------------------------------------
 * Retrieve the global event debug counters.
 *
 * @param[out] counters Output structure for the counters.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_get_global_counters (vqec_dp_global_debug_stats_t *stats)
{
    if (!s_vqec_dp_tlm_info || !stats) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    #define VQEC_DP_TLM_CNT_DECL(name,desc)                                 \
        stats->name = vqec_dp_ev_cnt_get(&s_vqec_dp_tlm_info->counters.name, \
                                         FALSE);
    #include "vqec_dp_tlm_cnt_decl.h"
    #undef VQEC_DP_TLM_CNT_DECL

    return (VQEC_DP_ERR_OK);   
}

/* Stores drop config and sequence counter for 2 streams (primary + repair) */
#define VQEC_DP_NUM_DROP_STREAMS_SUPPORTED (VQEC_DP_INPUT_STREAM_TYPE_REPAIR+1)
static vqec_dp_drop_sim_state_t 
            s_dp_drop_sim[VQEC_DP_NUM_DROP_STREAMS_SUPPORTED];
static uint32_t
            s_dp_drop_sim_seq[VQEC_DP_NUM_DROP_STREAMS_SUPPORTED];

/**---------------------------------------------------------------------------
 * Set drop parameters.
 *
 * @param[in]  session  Type of session for which parameters are being set
 * @param[in]  state State structure that defines all drop parameters.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if successful.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t 
vqec_dp_set_drop_params (vqec_dp_input_stream_type_t session,
                         vqec_dp_drop_sim_state_t *state)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (!state) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    switch (session) {
    case VQEC_DP_INPUT_STREAM_TYPE_PRIMARY:
    case VQEC_DP_INPUT_STREAM_TYPE_REPAIR:
        break;
    default:
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    s_dp_drop_sim[session] = *state;
    s_dp_drop_sim_seq[session] = 0;

    return (err);
}


/**---------------------------------------------------------------------------
 * Get drop parameters.
 *
 * @param[in]  session  Type of session for which parameters are being set
 * @param[out] state State structure that defines all drop parameters.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if successful.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t 
vqec_dp_get_drop_params (vqec_dp_input_stream_type_t session,
                         vqec_dp_drop_sim_state_t *state)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (!state) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    switch (session) {
    case VQEC_DP_INPUT_STREAM_TYPE_PRIMARY:
    case VQEC_DP_INPUT_STREAM_TYPE_REPAIR:
        *state = s_dp_drop_sim[session];
        break;
    default:
        err = VQEC_DP_ERR_INVALIDARGS;
        break;
    }

    return (err);
}


/**---------------------------------------------------------------------------
 * If drop_packet is true, drop packet
 * If drop_packet_interval is 0, drop randomly in accordance with
 * drop_packet_ratio;
 * otherwise drop drop_continuous_packets for
 * every drop_packet_interval packets.
 *---------------------------------------------------------------------------*/ 
boolean 
vqec_dp_drop_sim_drop (vqec_dp_input_stream_type_t session) 
{
    boolean ret = FALSE;
    static uint32_t seed = 8654;

    vqec_dp_drop_sim_state_t *sim;
    uint32_t *sim_seq;
    
    switch (session) {
    case VQEC_DP_INPUT_STREAM_TYPE_PRIMARY:
    case VQEC_DP_INPUT_STREAM_TYPE_REPAIR:
        break;
    default:
        goto done;
    }

    sim = &s_dp_drop_sim[session];

    if (!sim->en) {
        goto done;
    }

    if (sim->desc.interval_span && sim->desc.num_cont_drops) {        
        sim_seq = &s_dp_drop_sim_seq[session];
        if (*sim_seq < sim->desc.num_cont_drops) {
            ret = TRUE;
        }
        *sim_seq = 
            ((*sim_seq >= sim->desc.interval_span - 1) ? 0 : *sim_seq+1);
    } else if (sim->desc.drop_percent) {
        /* 
         * get a pseudo-random number, and use that to generate a number 
         * between [1,100]. Please note that different random generators may
         * be in use between the kernel and user-space ports.
         */
        if (((VQE_RAND_R(&seed) % 100) + 1) <= sim->desc.drop_percent) {
            ret = TRUE;
        }
    }
done:
    return (ret);
}

static rel_time_t g_vqec_dp_app_cpy_delay;

/**
 * Specifies the output timing of replicated APP packets.
 *
 * @param[in]  delay  Amount to delay each APP packet.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_set_app_cpy_delay (int32_t delay_ms)
{
    g_vqec_dp_app_cpy_delay = TIME_MK_R(msec, delay_ms);

    return (VQEC_DP_ERR_OK);
}

/**
 * Gets the amount to delay each APP packet.
 *
 * @param[out] rel_time_t  Amount (in ms) to delay each APP packet.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success, or
 *                              VQEC_DP_ERR_INVALIDARGS on failure.
 */
vqec_dp_error_t
vqec_dp_get_app_cpy_delay (rel_time_t *delay_ms)
{
    if (!delay_ms) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    *delay_ms = g_vqec_dp_app_cpy_delay;
    return (VQEC_DP_ERR_OK);
}

/**
 * Gets dataplane benchmark: cpu usage percentage
 *
 * @param[in] interval time span in seconds for benchmark
 * @param[out] cpu_usage VQE-C's estimated CPU usage for interval
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success
 */
vqec_dp_error_t
vqec_dp_get_benchmark(uint32_t interval, uint32_t *cpu_usage) {
    *cpu_usage = vqec_dp_event_get_benchmark(interval);
    return VQEC_DP_ERR_OK;
}

/**
 * Get operating mode of the dataplane.
 *
 * @param[out] mode The current operating mode of the VQE-C dataplane.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success; otherwise
 *                             VQEC_DP_ERR_INVALIDARGS is returned.
 */
vqec_dp_error_t
vqec_dp_get_operating_mode (vqec_dp_op_mode_t *mode)
{
    if (!mode) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

#ifdef __KERNEL__
    *mode = VQEC_DP_OP_MODE_KERNELSPACE;
#else
    *mode = VQEC_DP_OP_MODE_USERSPACE;
#endif  /* __KERNEL__ */

    return (VQEC_DP_ERR_OK);
}
