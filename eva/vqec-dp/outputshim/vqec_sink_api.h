/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: sink API.
 *
 * Documents:
 *
 *****************************************************************************/

#ifndef __VQEC_SINK_API_H__
#define __VQEC_SINK_API_H__

#include <vqec_dp_api_types.h>
#include <utils/queue_plus.h>
#include <vqec_pak.h>

/************************************************************************
 * Packet header pools support
 */

#define VQEC_PAK_HDR_POOLID_INVALID -1
typedef uint32_t vqec_pak_hdr_poolid_t;


/**
 * vqec_pak_hdr_pool_create
 *
 * @param[in] name           Descriptive name of pool (for use in displays)
 * @param[in] max_buffs      Number of packet headers to create in pool
 * @param[out] vqec_pak_hdr_poolid_t ID of the pak hdr pool created,
 *                                   or VQEC_PAK_HDR_POOLID_INVALID upon error 
 */
vqec_pak_hdr_poolid_t
vqec_pak_hdr_pool_create(char *name,
                         uint32_t max_buffs);

/**
 * vqec_pak_hdr_pool_destroy()
 *
 * Frees the specified packet pool.
 *
 * @param[in]  id - ID of packet pool to free
 */
void
vqec_pak_hdr_pool_destroy(vqec_pak_hdr_poolid_t id);


/************************************************************************
 * Packet header support
 *
 * Packet headers allow packets to be inserted on multiple lists.
 */

typedef struct vqec_pak_hdr_ {
    VQE_TAILQ_ENTRY(vqec_pak_hdr_) list_obj;
    struct vqec_pak_ *pak;
    vqec_pak_hdr_poolid_t pool;  /* pool from which this hdr was allocated */
} vqec_pak_hdr_t;

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
vqec_pak_hdr_alloc(vqec_pak_hdr_poolid_t id);

/**
 * vqec_pak_hdr_free()
 *
 * Releases a packet header structure back to its pool of origin
 *
 * @param[in] hdr Packet header to free
 */
void
vqec_pak_hdr_free (vqec_pak_hdr_t *hdr);


/************************************************************************
 * Sink declarations
 */

#define VQEC_SINK_INSTANCE struct vqec_sink_ *sk

/**
 * Sink statistics.
 */
typedef 
struct vqec_dp_sink_stats_
{
    /**
     * Total packets enqueued.
     */
    uint64_t        inputs;
    /** 
     * Queue drops.
     */
    uint64_t        queue_drops;
    /**
     * Current queue depth.
     */
    uint64_t        queue_depth;
    /**
     * Total packets output.
     */
    uint64_t        outputs;

} vqec_dp_sink_stats_t;

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
#define VQEC_SINK_METHODS_JITTER_HISTOGRAM                              \
    /**                                                                 \
     *  	Update the exit-to-enter jitter for a reader thread.        \
     */                                                                 \
    void (*vqec_sink_upd_reader_jitter)(VQEC_SINK_INSTANCE,             \
                                        abs_time_t enter_ts);
#else
#define VQEC_SINK_METHODS_JITTER_HISTOGRAM
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */

/**
 *  Methods for the sink.
 */
#define VQEC_SINK_METHODS_COMMON                                    \
    /**                                                             \
     *  Read sink statistics.                                       \
     */                                                             \
    int32_t (*vqec_sink_get_stats)(VQEC_SINK_INSTANCE,              \
                                   vqec_dp_sink_stats_t *stats,     \
                                   boolean cumulative);           \
    /**                                                             \
     * Clear sink statistics.                                       \
     */                                                             \
    int32_t (*vqec_sink_clear_stats)(VQEC_SINK_INSTANCE);           \
                                                                    \
    /**                                                             \
     * Read packet data from the output packet queues.              \
     */                                                             \
    int32_t (*vqec_sink_read)(VQEC_SINK_INSTANCE,                   \
                              vqec_iobuf_t *buf);                   \
    /**                                                             \
     * Put packet data into output the queues.                      \
     */                                                             \
    int32_t (*vqec_sink_enqueue)(VQEC_SINK_INSTANCE,                \
                                 vqec_pak_t * pak);                 \
    /**                                                             \
     *  Flush the sink.                                             \
     */                                                             \
    void (*vqec_sink_flush)(VQEC_SINK_INSTANCE);                    \
    /**                                                             \
     * Set the output socket to use.  Set fd as -1 to disable sock. \
     */                                                             \
    int32_t (*vqec_sink_set_sock)(VQEC_SINK_INSTANCE,               \
                                  int fd);                          \
    VQEC_SINK_METHODS_JITTER_HISTOGRAM                              \


#ifdef HAVE_SCHED_JITTER_HISTOGRAM
#define VQEC_SINK_MEMBERS_JITTER_HISTOGRAM                              \
    /**                                                                 \
     * Last exit timestamp of the reader thread from recvmsg API.       \
     */                                                                 \
    abs_time_t exit_ts;                                                 \
    /**                                                                 \
     * Histogram for reader thread's exit-to-enter jitter.              \
     */                                                                 \
    vam_hist_type_t *reader_jitter_hist;                                \
    /**                                                                 \
     * Histogram for the time each input packet is delayed.             \
     */                                                                 \
    vam_hist_type_t *inp_delay_hist;
#else
#define VQEC_SINK_MEMBERS_JITTER_HISTOGRAM
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */

/**
 * Members of the class.
 */
#define VQEC_SINK_MEMBERS_COMMON                                        \
    /**                                                                 \
     * Number of bytes available to be read.                            \
     */                                                                 \
    uint32_t rcv_len_ready_for_read;                                    \
    /**                                                                 \
     * Total packets enqueued.                                          \
     */                                                                 \
    uint64_t inputs;                                                    \
    uint64_t inputs_snapshot;                                           \
    /**                                                                 \
     * Queue drops on the output queue.                                 \
     */                                                                 \
    uint64_t queue_drops;                                               \
    uint64_t queue_drops_snapshot;                                      \
    /**                                                                 \
     * Current depth of packet queue.                                   \
     */                                                                 \
    uint32_t pak_queue_depth;                                           \
    /**                                                                 \
     * Total packets output snapshot (cumulative value is computed).    \
     */                                                                 \
    uint64_t outputs_snapshot;                                          \
    /**                                                                 \
     * Packet header pool.                                              \
     */                                                                 \
    vqec_pak_hdr_poolid_t hdr_pool;                                     \
    /**                                                                 \
     * Maximum queue size limit.                                        \
     */                                                                 \
    uint32_t queue_size;                                                \
    /**                                                                 \
     * Maximum packet size.                                             \
     */                                                                 \
    uint32_t max_paksize;                                               \
    VQEC_SINK_MEMBERS_JITTER_HISTOGRAM                                  \
    /**                                                                 \
     * Packet queue chain.                                              \
     */                                                                 \
    VQE_TAILQ_HEAD(, vqec_pak_hdr_) pak_queue;  

vqec_dp_error_t init_vqec_sink_module(vqec_dp_module_init_params_t *params);
void deinit_vqec_sink_module(void);

struct vqec_sink_ *vqec_sink_create(uint32_t max_q_depth, uint32_t max_paksize);
void vqec_sink_destroy(struct vqec_sink_ *);

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
vam_hist_type_t *vqec_sink_get_inp_delay_hist(struct vqec_sink_ *);
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */

struct vqec_sink_fcns_;
void vqec_sink_set_fcns_common(struct vqec_sink_fcns_ *table);

/*
 * Copy a packet's contents into a user-provided buffer [iobuf].
 *
 * It is the caller's responsibility to ensure the packet will fit
 * in the buffer, or else an overflow may occur.
 *
 * If the packet is an APP packet and RCC support is compiled in:
 *   o the caller's iobuf should be empty (or else an assert will fail)
 *   o upon writing an APP packet to a buffer, the VQEC_DP_BUF_FLAGS_APP
 *     flag will be set within the iobuf->buf_flags field.
 */
static inline int32_t
vqec_sink_read_internal (struct vqec_sink_ *sink,
                         vqec_pak_t *pak,
                         vqec_iobuf_t *iobuf)
{
#ifdef HAVE_FCC
    if(pak->type == VQEC_PAK_TYPE_APP) {
        VQEC_DP_ASSERT_FATAL(iobuf->buf_wrlen == 0,
                             "buffer write length is 0");
        iobuf->buf_flags |= VQEC_DP_BUF_FLAGS_APP;
    }
#endif /* HAVE_FCC */

    memcpy((char *)iobuf->buf_ptr + iobuf->buf_wrlen, 
           vqec_pak_get_head_ptr(pak),
           vqec_pak_get_content_len(pak));

     /* increment written length */
    iobuf->buf_wrlen += vqec_pak_get_content_len(pak);

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    /* record input delay */
    if (pak->type == VQEC_PAK_TYPE_PRIMARY) {
        (void)vam_hist_add(
            vqec_sink_get_inp_delay_hist(sink),
            TIME_GET_R(msec, 
                       TIME_SUB_A_A(get_sys_time(), pak->rcv_ts)));
    }
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
    return (vqec_pak_get_content_len(pak));    
}

#endif /* __VQEC_SINK_API_H__ */

