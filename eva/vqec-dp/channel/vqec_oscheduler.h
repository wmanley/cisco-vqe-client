/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: VQEC dp output scheduler declarations.
 *
 * Documents:
 *
 *****************************************************************************/

#ifndef __VQEC_OSCHEDULER_H__
#define __VQEC_OSCHEDULER_H__

#include <vqec_event.h>
#include <vqec_dp_io_stream.h>
#include <vqec_nll.h>
#include <vqec_dp_api_types.h>

/* forward delcaration */
struct vqec_pcm_;

/* capability for multiple types of streams for multiple encaps */
#define VQEC_DP_OSCHED_STREAMS_MAX 1

typedef struct vqec_dp_oscheduler_streams_ {
    /* local output streams */
    vqec_dp_encap_type_t osencap[VQEC_DP_OSCHED_STREAMS_MAX];

    /* connected input streams */
    vqec_dp_isid_t isids[VQEC_DP_OSCHED_STREAMS_MAX];
    const struct vqec_dp_isops_ *isops[VQEC_DP_OSCHED_STREAMS_MAX];
} vqec_dp_oscheduler_streams_t;

typedef struct vqec_dp_oscheduler_ {
    struct vqec_pcm_ *pcm;                  /* pcm for this channel */
    vqec_nll_t nll;                         /* nll for this channel */
    vqec_dp_oscheduler_streams_t streams;   /* in/out streams for this chan */
    boolean run_osched;                     /*
                                             * specifies whether the output
                                             * scheduler is running
                                             */
    vqec_pak_t *pak_pend;                   /* packet waiting to be sent  */
    abs_time_t last_pak_ts;                 /* last sent packet's timestamp  */
    abs_time_t last_pak_pred_ts;            /* last sent packet's pred ts */
    vqec_seq_num_t last_pak_seq;            /* last sent packet's sequence */
    boolean         last_pak_seq_valid;     /* last tx seq valid */
    abs_time_t last_inorder_pak_ts;         /* last sent in-order pak's ts */
    vqec_seq_num_t last_inorder_pak_seq;    /* last sent in-order pak's seq */
    boolean last_inorder_pak_seq_valid;     /* is the above value valid? */
    boolean rcc_burst_done;                 /* is the RCC repair burst done? */
    uint32_t state;                         /* current output state */ 

    uint32_t max_post_er_rle_size;          /* Maximum size of XR stats */


    /*
     * Log of certain events in output - this log is cached for RCC and must
     * not be reset even though the rest of the output block is reset.
     */
    struct {
        boolean first_pak_sent;         /* Has first pak been sent?  */        
        boolean first_prim_pak_sent;    /* Has first primary pak been sent */
        abs_time_t first_pak_ts;        /* First pak sent timestamp  */
        abs_time_t first_pak_rcv_ts;    /* First pak rcv timestamp  */
        abs_time_t first_prim_pak_ts;   /* First primary pak timestamp */
    } outp_log;

} vqec_dp_oscheduler_t;

static inline abs_time_t
vqec_dp_oscheduler_last_pak_ts_get (vqec_dp_oscheduler_t *osched)
{
    return osched->last_pak_ts;
}

static inline vqec_seq_num_t
vqec_dp_oscheduler_last_pak_seq_get (vqec_dp_oscheduler_t *osched)
{
    return osched->last_pak_seq;
}

static inline boolean
vqec_dp_oscheduler_last_pak_seq_valid (vqec_dp_oscheduler_t *osched)
{
    return osched->last_pak_seq_valid;
}

static inline abs_time_t
vqec_dp_oscheduler_outp_log_first_pak_ts (vqec_dp_oscheduler_t *osched)
{
    return osched->outp_log.first_pak_ts;
}

static inline abs_time_t
vqec_dp_oscheduler_outp_log_first_prim_pak_ts (vqec_dp_oscheduler_t *osched)
{
    return osched->outp_log.first_prim_pak_ts;
}


/**
 * Initialize the output scheduler, then start it.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[in] pcm  Pointer to the associated PCM module.
 * @param[in] max_post_er_rle_size  Maximum RLE size for RTCP post-ER XR.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_init(vqec_dp_oscheduler_t *osched,
                        struct vqec_pcm_ *pcm,
                        uint32_t max_post_er_rle_size);

/**
 * Stop the output scheduler, then deinitialize it.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_deinit(vqec_dp_oscheduler_t *osched);

/**
 * Add the relevant stream information to the output scheduler.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[in] streams  Input and output streams info for the DPChan module.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_add_streams(vqec_dp_oscheduler_t *osched,
                               vqec_dp_oscheduler_streams_t streams);

/**
 * Notify the output scheduler that the RCC repair burst is finished.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_rcc_burst_done_notify(vqec_dp_oscheduler_t *osched);

/**
 * Run the output scheduler.  If the output scheduler is stopped, this
 * function will not do anything.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[in] cur_time  The current system time.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_run(vqec_dp_oscheduler_t *osched,
                       abs_time_t cur_time, boolean *done_with_fastfill);

/**
 * Start the output scheduler.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_start(vqec_dp_oscheduler_t *osched);


/**
 * Stop the output scheduler.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_stop(vqec_dp_oscheduler_t *osched);


/**
 * Reset the output scheduler.  This will simply reset the NLL module.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_oscheduler_reset(vqec_dp_oscheduler_t *osched);

/*
 * Init state of output data block.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 * @param[in] max_post_er_rle_size  Size of the post-ER XR RLE block.
 */
void vqec_dp_oscheduler_outp_init(vqec_dp_oscheduler_t *osched);

/*
 * Reset state of output data block inclusive of the NLL.
 *
 * @param[in] osched  Pointer to the output scheduler instance.
 */
void vqec_dp_oscheduler_outp_reset(vqec_dp_oscheduler_t *osched,
                                   boolean free_pend);

/**---------------------------------------------------------------------------
 * Retrieves post-repair XR statistics.
 * 
 * @param[in] osched Pointer to oscheduler instance.
 * @param[in] reset_xr_stats Specifies whether to reset the post-ER XR stats.
 * @param[out] post_er_stats Pointer to copy the stats info into.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_oscheduler_get_post_er_stats(vqec_dp_oscheduler_t *osched,
                                  boolean reset_xr_stats,
                                  rtcp_xr_post_rpr_stats_t *post_er_stats);

#endif /* __VQEC_OSCHEDULER_H__ */
