/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_sink.c
 *
 * Description: VQEC "sink" abstraction implementation.
 *
 * Documents:
 *
 *****************************************************************************/
#include "vqec_sink.h"

/************************************************************************
 * Packet header pools support
 */

typedef struct vqe_zone  vqec_pak_hdr_pool_t;
vqec_pak_hdr_pool_t *s_pak_hdr_pool = NULL;

typedef struct vqe_zone vqec_sink_pool_t;
vqec_sink_pool_t *s_vqec_sink_pool = NULL;

/* The global pak hdr pool */
#define VQEC_PAK_HDR_POOLID 0

/**
 * pak header pool: used by the sink's in same-user-process implementation.
 */
vqec_pak_hdr_poolid_t s_pak_hdr_poolid;

/**
 * vqec_pak_hdr_pool_create
 *
 * @param[in] name           Descriptive name of pool (for use in displays)
 * @param[in] max_buffs      Number of packet headers to create in pool
 * @param[out] vqec_pak_hdr_poolid_t ID of the pak hdr pool created,
 *                                   or VQEC_PAK_HDR_POOLID_INVALID upon error 
 */
vqec_pak_hdr_poolid_t
vqec_pak_hdr_pool_create (char *name,
                          uint32_t max_buffs)
{
    vqec_pak_hdr_poolid_t id = VQEC_PAK_HDR_POOLID_INVALID;

    if (s_pak_hdr_pool) {
        /*
         * If a pool exists, we have hit the current limit of one pool.
         */
        goto done;
    }    

    s_pak_hdr_pool = zone_instance_get_loc(name,
                                           ZONE_FLAGS_STATIC,
                                           sizeof(vqec_pak_hdr_t),
                                           max_buffs,
                                           zone_ctor_no_zero, NULL);
    if (!s_pak_hdr_pool) {
        goto done;
    }

    /* Assign the newly-created pool its predefined ID */
    id = VQEC_PAK_HDR_POOLID;
done:
    return (id);
}

/**
 * vqec_pak_hdr_pool_destroy()
 *
 * Frees the specified packet pool.
 *
 * @param[in]  id - ID of packet pool to free
 */
void
vqec_pak_hdr_pool_destroy (vqec_pak_hdr_poolid_t id)
{
    /* Nothing to do if pool is not allocated */
    if ((id != VQEC_PAK_HDR_POOLID) || (!s_pak_hdr_pool)) {
        goto done;
    }

    (void)zone_instance_put(s_pak_hdr_pool);
    s_pak_hdr_pool = NULL;

done:
    return;
}

/************************************************************************
 * Packet header support
 *
 * Packet headers allow packets to be inserted on multiple lists.
 */

/**
 * vqec_pak_hdr_alloc()
 *
 * Allocates a packet header for insertion of a packet onto a list.
 *
 * @param[id]  id                ID of hdr pool from which the packet hdr
 *                                should be allocated.
 * @param[out] vqec_pak_hdr_t *  Pointer of allocated hdr,
 *                                or NULL if unsuccessful.
 */
vqec_pak_hdr_t *
vqec_pak_hdr_alloc (vqec_pak_hdr_poolid_t id)
{
    vqec_pak_hdr_t *hdr = NULL;

    if ((id != VQEC_PAK_HDR_POOLID) || (!s_pak_hdr_pool)) {
        goto done;
    }
    
    hdr = zone_acquire(s_pak_hdr_pool);
    if (!hdr) {
        goto done;
    }

    /* Clear the pak header and store the pool from which it came */
    memset(hdr, 0, sizeof(*hdr));
    hdr->pool = VQEC_PAK_HDR_POOLID;
done:
    return (hdr);
}

/**
 * vqec_pak_hdr_free()
 *
 * Releases a packet header structure back to its pool of origin
 *
 * @param[in] hdr Packet header to free
 */
void 
vqec_pak_hdr_free (vqec_pak_hdr_t *hdr)
{
    if (!hdr || (hdr->pool != VQEC_PAK_HDR_POOLID)) {
        return;
    }

    /* Release the hdr */
    zone_release(s_pak_hdr_pool, hdr);
}


/************************************************************************
 * Sink member functions
 */

static vqec_sink_fcns_t vqec_sink_fcn_table;

/**
   Get stats from sink
   Currently sink doesn't have any stats.
   @param[in] sink Pointer of sink
   @param[in] stats Pointer of stats
   @param[in] cumulative Boolean flag to retrieve cumulative stats
   @return success or error
*/
int32_t vqec_sink_get_stats (struct vqec_sink_ *sink, 
                             vqec_dp_sink_stats_t *stats, 
                             boolean cumulative)
{
    if (!stats) {
        VQEC_DP_SYSLOG_PRINT(INVALIDARGS, __FUNCTION__);
        return (VQEC_DP_ERR_INVALIDARGS);
    }
    stats->inputs = sink->inputs;
    stats->queue_drops = sink->queue_drops;
    stats->queue_depth = sink->pak_queue_depth;
    stats->outputs = sink->inputs - sink->queue_drops - sink->pak_queue_depth;
    if (!cumulative) {
        stats->inputs -= sink->inputs_snapshot;
        stats->queue_drops -= sink->queue_drops_snapshot;
        stats->outputs -= sink->outputs_snapshot;
    }
    return (VQEC_DP_ERR_OK);
}

/**
   Clear stats from sink
   @param[in] sink Pointer of sink
   @return success or error
*/
int32_t vqec_sink_clear_stats (struct vqec_sink_ *sink) 
{
    sink->inputs_snapshot = sink->inputs;
    sink->queue_drops_snapshot = sink->queue_drops;
    sink->outputs_snapshot = sink->inputs - sink->queue_drops - sink->pak_queue_depth;
    return (VQEC_DP_ERR_OK);
}

/**
   Read bytes from a sink into an iobuf.

   Only whole packet payloads are written into an iobuf.  Packet payloads will
   be written into iobufs until no more packet payloads fit, and then the
   function returns (with iobuf containing 0 or more whole packet payloads).

   If an APP packet is queued on the sink, it is assumed to be FIRST in 
   the queue, and will be returned as the first and only packet within
   the iobuf (with the VQEC_DP_BUF_FLAGS_APP flag set within iobuf->buf_flags)
   assuming it fits.

   @param[in] sink Pointer of sink
   @param[in] buf pointer of buf to hold content.
                  Input fields are iobuf->buf_ptr, iobuf->buf_len
                  Output fields are iobuf->buf_wrlen, iobuf->buf_flags
                   (must be zero'd prior to calling this function).
   @return length read (also in iobuf->buf_wrlen); -1 if error
*/
int32_t vqec_sink_read (struct vqec_sink_ *sink, 
                        vqec_iobuf_t *iobuf)
{
    vqec_pak_t *pak;
    vqec_pak_hdr_t *pak_hdr;
    char *paktype;

    if (!iobuf) {
        return (-1);
    } else if (sink->rcv_len_ready_for_read == 0) { 
        return (0);
    }

    while (1) {
        pak_hdr = VQE_TAILQ_FIRST(&sink->pak_queue);
        if (!pak_hdr) {
            /* no more packets! */
            break;
        }

        /* only deQ when enough room to copy packet */
        if ((iobuf->buf_len - iobuf->buf_wrlen) >= 
            vqec_pak_get_content_len(pak_hdr->pak)) {
        
            VQE_TAILQ_REMOVE(&sink->pak_queue, pak_hdr, list_obj);
            sink->pak_queue_depth--;
            pak = pak_hdr->pak; 
            (void)vqec_sink_read_internal(sink, pak, iobuf);

            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_OUTPUTSHIM_PAK)) {
                switch (pak->type) {
                case (VQEC_PAK_TYPE_APP):
                    paktype = "APP";
                    break;
                case (VQEC_PAK_TYPE_PRIMARY):
                    paktype = "PRIMARY";
                    break;
                case (VQEC_PAK_TYPE_REPAIR):
                    paktype = "REPAIR";
                    break;
                case (VQEC_PAK_TYPE_FEC):
                    paktype = "FEC";
                    break;
                case (VQEC_PAK_TYPE_UDP):
                    paktype = "UDP";
                    break;
                default:
                    paktype = "<unknown>";
                }
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_OUTPUTSHIM_PAK,
                              "Read packet from sink; type %s len %u "
                              "seq %u rcv_ts %u\n",
                              paktype,
                              vqec_pak_get_content_len(pak),
                              pak->seq_num,
                              pak->rcv_ts.usec);
            }
            sink->rcv_len_ready_for_read -= vqec_pak_get_content_len(pak);
            VQEC_DP_ASSERT_FATAL(sink->rcv_len_ready_for_read >= 0, 
                                 "length ready to be read is < 0");
            
            vqec_pak_free(pak);
            vqec_pak_hdr_free(pak_hdr);

#ifdef HAVE_FCC
            if (iobuf->buf_flags & VQEC_DP_BUF_FLAGS_APP) {
                /* 
                 * Abort reading of any remaining packets--get the APP
                 * packet information back to the caller ASAP.
                 */
                break;
            }
#endif /* HAVE_FCC */

        } else {
            /* no more room! */
            break;
        }
    }

    return (iobuf->buf_wrlen);
}

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
static int32_t
s_reader_jitter_ranges[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100,
                            150, 200, 250, 300, 350, 400, 450, 500};
static const uint32_t
s_reader_jitter_num_ranges =
    sizeof(s_reader_jitter_ranges)/sizeof(s_reader_jitter_ranges[0]);
static struct vqe_zone *s_reader_hist_pool;
static int32_t
s_inp_delay_ranges[] = {-200, -150, -100, -50, -20, 0, 20, 40, 60, 80, 100, 120,
                        140, 160, 180, 200, 250, 300, 350, 400, 450, 500};
static const uint32_t
s_inp_delay_num_ranges =
    sizeof(s_inp_delay_ranges)/sizeof(s_inp_delay_ranges[0]);
static struct vqe_zone *s_inp_delay_hist_pool;
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */

/**
   Create sink object.
   @return pointer of sink allocated.
 */
vqec_sink_t * vqec_sink_create (uint32_t max_q_depth, uint32_t max_paksize) 
{

    vqec_sink_t * sink;
    
    sink = (vqec_sink_t *) zone_acquire(s_vqec_sink_pool);
    
    if (!sink) {
        return (NULL);
    }

    memset(sink, 0, sizeof(vqec_sink_t));
    sink->__func_table = &vqec_sink_fcn_table;

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    sink->reader_jitter_hist =
        zone_acquire(s_reader_hist_pool);
    if (!sink->reader_jitter_hist) {
        zone_release(s_vqec_sink_pool, sink);
        return (NULL);
    }

    if (vam_hist_create_ranges(sink->reader_jitter_hist,
                               s_reader_jitter_ranges,
                               s_reader_jitter_num_ranges,
                               "Reader exit-to-enter jitter")) {
        zone_release(s_reader_hist_pool,
                     sink->reader_jitter_hist);
        zone_release(s_vqec_sink_pool, sink);
        return (NULL);
    }

    sink->inp_delay_hist =
        zone_acquire(s_inp_delay_hist_pool);
    if (!sink->inp_delay_hist) {
        zone_release(s_reader_hist_pool,
                     sink->reader_jitter_hist);
        zone_release(s_vqec_sink_pool, sink);
        return (NULL);
    }

    if (vam_hist_create_ranges(sink->inp_delay_hist,
                               s_inp_delay_ranges,
                               s_inp_delay_num_ranges,
                               "Input jitter")) {
        zone_release(s_inp_delay_hist_pool,
                     sink->inp_delay_hist);
        zone_release(s_reader_hist_pool,
                     sink->reader_jitter_hist);
        zone_release(s_vqec_sink_pool, sink);
        return (NULL);
    }
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */

    VQE_TAILQ_INIT(&sink->pak_queue);
    sink->hdr_pool = s_pak_hdr_poolid;
    sink->queue_size = max_q_depth;
    sink->max_paksize = max_paksize;

    return sink;
}

/**
   Destroy sink object.
   @param[in] sink pointer of sink to destroy.
 */
void vqec_sink_destroy (vqec_sink_t * sink) 
{
    if (!sink) {
        return;
    }

    MCALL(sink, vqec_sink_flush);
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    zone_release(s_inp_delay_hist_pool,
                 sink->inp_delay_hist);
    zone_release(s_reader_hist_pool,
                 sink->reader_jitter_hist);
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
    zone_release(s_vqec_sink_pool, sink);
}

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
/**---------------------------------------------------------------------------
 * Update a reader's thread exit-to-enter jitter when reading packet data
 * using the recvmsg API.
 *
 * @param[in] sink Pointer to the sink object.
 * @param[in] enter_ts Entry timestamp for the thread.
 *---------------------------------------------------------------------------*/
static void
vqec_sink_upd_reader_jitter (vqec_sink_t * sink,
                             abs_time_t enter_ts)
{
    if (!IS_ABS_TIME_ZERO(sink->exit_ts)) {
        (void)vam_hist_add(sink->reader_jitter_hist,
                           TIME_GET_R(msec,
                                      TIME_SUB_A_A(enter_ts, sink->exit_ts)));
    }
}


/**---------------------------------------------------------------------------
 * Accessor for inp_delay_hist pointer.
 *
 * @param[in] sink Pointer to the sink object.
 * @param[out] vam_hist_type_t* Pointer to the input delay histogram.
 *---------------------------------------------------------------------------*/
vam_hist_type_t *
vqec_sink_get_inp_delay_hist (vqec_sink_t * sink)
{
    return (sink->inp_delay_hist);
}
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */


/**
   Set sink module function table
   @param[in] table Function table for sink module.
 */
void vqec_sink_set_fcns_common (struct vqec_sink_fcns_ *table)
{
    table->vqec_sink_get_stats = vqec_sink_get_stats;
    table->vqec_sink_clear_stats = vqec_sink_clear_stats;
    table->vqec_sink_read = vqec_sink_read;
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    table->vqec_sink_upd_reader_jitter = vqec_sink_upd_reader_jitter;
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
}

/**
   Static variable to tell sink module is initialized or not.
 */
static boolean vqec_sink_module_initialized = FALSE;

/**
   Init sink module
 */
vqec_dp_error_t
init_vqec_sink_module (vqec_dp_module_init_params_t *params) 
{
    vqec_dp_error_t status = VQEC_DP_ERR_OK;

    if (vqec_sink_module_initialized) {
        goto done;
    }
   
    s_vqec_sink_pool = zone_instance_get_loc(
                    "vqec_sink_pool",
                    O_CREAT,
                    sizeof(vqec_sink_t),
                    params->max_tuners,
                    NULL, NULL);
    if (!s_vqec_sink_pool) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }
 
    vqec_sink_set_fcns(&vqec_sink_fcn_table);
    s_pak_hdr_poolid =
        vqec_pak_hdr_pool_create(
            "VQEC_pakhdrpool",
            params->pakpool_size * params->max_tuners);
    if (s_pak_hdr_poolid == VQEC_PAK_HDR_POOLID_INVALID) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    s_reader_hist_pool =
        zone_instance_get_loc("reader_jitter",
                              O_CREAT,
                              vam_hist_calc_size(s_reader_jitter_num_ranges + 1,
                                                 FALSE),
                              params->max_tuners,
                              NULL,
                              NULL);
    if (!s_reader_hist_pool) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }
    s_inp_delay_hist_pool =
        zone_instance_get_loc("inp_delay_",
                              O_CREAT,
                              vam_hist_calc_size(s_inp_delay_num_ranges + 1,
                                                 FALSE),
                              params->max_tuners,
                              NULL,
                              NULL);
    if (!s_inp_delay_hist_pool) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */

    vqec_sink_module_initialized = TRUE;
done:
    if (status != VQEC_DP_ERR_OK) {
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
        if (s_pak_hdr_poolid != VQEC_PAK_HDR_POOLID_INVALID) {
            vqec_pak_hdr_pool_destroy(s_pak_hdr_poolid);
            s_pak_hdr_poolid = VQEC_PAK_HDR_POOLID_INVALID;
        }
        if (s_reader_hist_pool) {
            (void) zone_instance_put(s_reader_hist_pool);
            s_reader_hist_pool = NULL;
        }
        if (s_inp_delay_hist_pool) {
            (void) zone_instance_put(s_inp_delay_hist_pool);
            s_inp_delay_hist_pool = NULL;
        }
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
        if (s_vqec_sink_pool) {
            (void) zone_instance_put(s_vqec_sink_pool);
            s_vqec_sink_pool = NULL;
        }
    }
    return (status);
}

/**
 * Deinit the sink module.
 */
void
deinit_vqec_sink_module (void)
{
    if (!vqec_sink_module_initialized) {
        return;
    }
    vqec_pak_hdr_pool_destroy(s_pak_hdr_poolid);
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    if (s_reader_hist_pool) {
        (void) zone_instance_put(s_reader_hist_pool);
        s_reader_hist_pool = NULL;
    }
    if (s_inp_delay_hist_pool) {
        (void) zone_instance_put(s_inp_delay_hist_pool);
        s_inp_delay_hist_pool = NULL;
    }
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
    if (s_vqec_sink_pool) {
        (void) zone_instance_put(s_vqec_sink_pool);
        s_vqec_sink_pool = NULL;
    }
    vqec_sink_module_initialized = FALSE;
}
