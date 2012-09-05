/*------------------------------------------------------------------
 *
 * March 2006, 
 *
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

/*
 * vqec_pcm.h - defines packet cache manager for vqec module.
 * 
 * Packets are inserted in by seqence number, pcm have an event to send out
 * the packet with the smallest seqence number.
 */
#ifndef __VQEC_PCM_H__
#define __VQEC_PCM_H__

#ifdef __cplusplus
extern "C" {
#if 0
}  /* here just to fix indent issues in XEmacs */
#endif
#endif /* __cplusplus */

#include "vqec_dp_api_types.h"
#include "vqec_pak_seq.h"
#include <utils/vam_time.h>
#include <utils/queue_plus.h>
#include "vqec_event.h"
#include <utils/zone_mgr.h>
#include "vqec_oscheduler.h"
#include "vqe_bitmap.h"
#include <vqec_log.h>
#include <vqec_dp_api_types.h>

#define STB_CACHELINE_BYTES 16

/**@brief
 * Size (in seq_nums) of the gapmap to allocate */
#define VQEC_PCM_GAPMAP_SIZE 8192

#define VQEC_PCM_OUTPUT_SCHED_INTERVAL (20*1000)
#define VQEC_PCM_RING_BUFFER_BUCKET_BITS 13
#define VQEC_PCM_RING_BUFFER_SIZE (1 << VQEC_PCM_RING_BUFFER_BUCKET_BITS)
#define VQEC_PCM_MAX_GAP_SIZE 6000
#define VQEC_PCM_MAX_HEAD_TAIL_SPREAD VQEC_PCM_GAPMAP_SIZE

#define FEC_TOUCHED     1   /* the repair packet is from FEC */

/**@brief
 * Maximum number of packet candidates for next_seq_num */
#define VQEC_PCM_MAX_CANDIDATES    10

/**
 * vqec_pcm_candidate_t
 * @brief
 * Struct to track packet candidates for next_seq_num updates */
typedef struct vqec_pcm_candidate_
{
    /**@brief
     * Candidate packet sequence number */
    vqec_seq_num_t seq_num;

    /**@brief
     * Candidate packet receive timestamp */
    abs_time_t rcv_ts;

} vqec_pcm_candidate_t;

/*
 * Note about definition of gaps:
 * A gap range of (start_seq, extent), as defined by vqec_gap_t, refers to
 * the sequence numbers beginning with start_seq, and running through
 * extent more sequence numbers.  Some examples of this are below:
 *   (start_seq, extent) ==> sequence numbers referenced by gap
 *   (234,0)             ==> 234
 *   (123,1)             ==> 123, 124
 *   (567,3)             ==> 567, 568, 569, 570
 * start_seq must be a valid sequence number in a 32-bit sequence number space
 * extent must be greater than or equal to 0 and less than 2^15
 */
typedef struct vqec_gap_ {
    vqec_seq_num_t start_seq;  /* must be a valid sequence number */
    vqec_seq_num_t extent;     /* must be >= 0 and < 2^16 */
} vqec_gap_t;

/**
 * vqec_pcm_stats_t
 * PCM statistics structure
 */
typedef struct vqec_pcm_stats_ {
    uint64_t head_ge_last_seq; /*!<
                                * number of times when pcm get_gaps
                                * happened while head >= last reqstd seq
                                */
    uint64_t repair_packet_counter; 
                                /*!< total repair packet count */
    uint64_t primary_packet_counter; 
                                /*!< total primary packet count */

    uint64_t pcm_insert_drops;  /*!< packets that failed pcm_insert() */
    uint64_t late_packet_counter; 
                                /*!< packets dropped from head */
    uint64_t duplicate_packet_counter; 
                                /*!< total duplicate packet count */
    uint64_t seq_bad_range_packet_counter; 
                                /*!< packets count in bad seq range */
    uint64_t pak_seq_insert_fail_counter; 
                                /*!< packets that failed the pak_seq_insert() */

    uint64_t input_loss_pak_counter; 
                                /*!< counter for single input seq drop */
    uint64_t input_loss_hole_counter; 
                                /*!< counter for contiguous input seq gap */
    uint64_t output_loss_pak_counter;
                                /*!< counter for single output seq drop */
    uint64_t output_loss_hole_counter;
                                /*!< counter for contiguous output seq gap */
    uint64_t under_run_counter; /*!< counter for under run */
        
    uint64_t total_tx_paks;     /*!< total transmitted packets */
    uint64_t bad_rcv_ts;        /*!< packets with invalid receive ts  */
    uint64_t duplicate_repairs; /*!<
                                 * # dup packets repaired by both
                                 * FEC and retransmission 
                                 */
    uint32_t output_loss_paks_first_prim;
                                /*!< output loss paks at first primary */
    uint32_t output_loss_holes_first_prim;
                                /*!< output loss holes at first primary */
    uint32_t dup_paks_first_prim; 
                                /*!< duplicate paks at first primary */
    vqec_tr135_stats_t tr135;
#if HAVE_FCC
    uint64_t first_prim_to_last_rcc_seq_diff; 
                                /*!< seq # diff from first pri to last repair*/
#endif
} vqec_pcm_stats_t;

/**
 * vqec_pcm_t
 *
 * VQEC Packet cache manager
 *
 */
typedef struct vqec_pcm_ {
    struct vqec_dpchan_ *dpchan;/*!< back pointer to PCM's channel */
    vqe_bitmap_t *gapmap;       /*!< input seq_num gap bitmap */

    vqec_pak_seq_t *pak_seq;    /*!< real buffer to store packet */
    vqec_seq_num_t head;        /*!< the smallest seq num */
    vqec_seq_num_t tail;        /*!< the largest seq num */

    boolean er_enable;          /*!< error repair enable/disable: this is the
                                  current ER state local to pcm. */
    boolean cfg_er_enable;
                                /*!< Configured ER state. */
    boolean rcc_enable;         /*!< Is RCC enabled */
    boolean fec_enable;         /*!< Is FEC enabled */
    boolean underrun_reset_dis; 
                                /*!< If true, disables resets due to 
                                  under-runs */
    boolean dyn_jitter_buf_act; 
                                /*!< If true, late-instantiation buffer is in
                                 *   use, which simply states that under-run
                                 *   count = 0. */
    boolean strip_rtp;          /*!< If RTP headers are to be stripped  */

    /* track primary / repair packets received or not */
    boolean primary_received;   /*!< boolean used for internal state
                                  machine */
    boolean repair_received;    /*!< boolean used for internal state
                                  machine */

    /** stats counters */
    vqec_pcm_stats_t stats;
    vqec_pcm_stats_t stats_snapshot;
    vqec_log_t log;             /*!< ER Log */ 
    vqec_log_gaps_t first_prim_log;  
                                /*!< Output log at first primary use for RCC */ 
    
    rel_time_t delay_from_apps; /*!< delay in usec caused by replicated and
                                  delayed APP paks - must delay the rest of
                                  paks */
    
    rel_time_t repair_trigger_time; 
                                /*!< jitter_buff_size repair_trigger_point */
    rel_time_t reorder_delay;   /*!< time to wait for reordered packet gaps to
                                  be fixed before doing ER */

    /*
     * Since we are now binding error repair in output_ev, we 
     * need a counter to signal the error repair. When this decrease to
     * 0, it's time to signal a repair.
     */
    uint32_t gap_report_schedule_count;
    rel_time_t gap_hold_time;   /*!< time to hold pak before er */
    vqec_seq_num_t highest_er_seq_num; 
                                /*!< highest seq_num for gap report */
    vqec_seq_num_t last_requested_er_seq_num;
                                /*!< seq_num last requested for
                                  error repair */
    boolean pktflow_src_seq_num_start_set;  /*!<
                                             *!< tracks whether
                                             *!< pktflow_src_seq_num_start
                                             *!< is recorded.
                                             */
    vqec_seq_num_t pktflow_src_seq_num_start;
                     /*
                      * Set for UNICAST channels only for failover support.
                      * 
                      * Records the lowest sequence number inserted 
                      * into the PCM by the current packetflow source.
                      * If set, gaps reported by the PCM will not
                      * include packets with sequence numbers earlier
                      * than this, as such packets came from a different
                      * source (prior to failover) and are thus not
                      * eligible for repair with the current packetflow
                      * source.
                      */

    VQE_TAILQ_HEAD(, vqec_pak_) inorder_q;
                                /*!< tail queue of inorder packets */
    rel_time_t  avg_pkt_time;   /*!< per packet time estimate */
    rel_time_t default_delay;   /*!< buffer delay per packet */
    rel_time_t fec_delay;       /*!< delay induced by FEC */
    rel_time_t cfg_delay;       /*!< added for FEC */
                                         
    
    vqec_seq_num_t last_rx_seq_num; 
                                /*!< use to change 16 bits
                                 * rtp seq number into 32
                                 * bits vqec seq number.
                                 * We can put this in
                                 * rtp_era_session, but
                                 * will introduce
                                 * dependence of
                                 * rtp_repair_session to 
                                 * rtp_era_session
                                 */

    vqec_dp_oscheduler_t osched;
                                /*!< output scheduler instance */



    /* Following members track candidate packets used for updating  */
    /* highest_er_seq_num after holding for fec                     */
    vqec_pcm_candidate_t
        candidates[VQEC_PCM_MAX_CANDIDATES] __attribute__
                                            ((aligned (STB_CACHELINE_BYTES)));
    uint16_t first_candidate;
    uint16_t last_candidate;
    uint16_t next_candidate;
    rel_time_t candidate_bucket_width;
    boolean candidates_recently_checked;
    boolean first_er_poll_done;

    /* Params for PCM to create FEC buffer */
    vqec_fec_info_t fec_info;
    uint16_t fec_default_block_size; 

    /**
     * The following four parameters are used for calculating the 
     * per packet time in MPEG PCR. 
     */
    vqec_seq_num_t prev_seq_num;
    uint32_t prev_rtp_timestamp;
    rel_time_t new_rtp_ts_pkt_time;
    boolean ts_calculation_done;
#if HAVE_FCC
    boolean rcc_post_abort_process; /* need a post process */
    vqec_seq_num_t first_primary_seq;   /* first primary seq num */

    /**
     * fast fill time in ms 
     * This value is also used to flag the fast fill.
     * If the value is zero, no fast fill is perform.
     * After finish fast fill, this one will be set to zero
     */
    rel_time_t fast_fill_time;
    /**
     * Maximum amount of data that can be fast filled during a memory
     * optimized RCC burst.
     */
    rel_time_t max_fastfill;
    /**
     * Minimum amount of the jitter buffer that can be filled during and after
     * an RCC burst, in order to allow other services (FEC, ER) to work.
     */
    rel_time_t min_backfill;
    /**
     * fast fill end time
     */
    abs_time_t fast_fill_end_time;
    /**
     * RCC burst end time
     */
    rel_time_t dt_repair_end;
    uint32_t total_fast_fill_bytes;

    void (*pre_primary_repairs_done_cb)(uint32_t data);
                                  /*!< Callback to signify that last repair pak
                                   *   before first prim pak was received */
    uint32_t pre_primary_repairs_done_data;
                                  /*!< Data to be passed to above callback */
    boolean pre_primary_repairs_done_notified;
                                  /*!< Indicates whether or not the callback
                                   *   has already been called once */
    /**
     * Indicates whether or not the pre-first-primary output stats are done
     * being tabulated
     */
    boolean pre_prim_outp_stats_done;
    abs_time_t er_en_ts;
    /* Maintain tr-135 writable parameters configuration at the pcm level */
    vqec_ifclient_tr135_params_t tr135_params;

#endif  /* HAVE_FCC */
} vqec_pcm_t;

/**
 * vqec_pcm_params_t 
 * Used to init pcm
 */
typedef struct vqec_pcm_params_ {
    struct vqec_dpchan_ *dpchan;  /*!< back pointer to PCM's channel */

    boolean er_enable;            /*!< error repair enable/disable */
    boolean fec_enable;           /*!< tracks if FEC is enabled/disabled */
    boolean rcc_enable;           /*!< Is RCC enabled */
    rel_time_t avg_pkt_time;
    rel_time_t default_delay;
    rel_time_t repair_trigger_time; 
                                  /*!< Jitter buffer's repair trigger point */
    rel_time_t reorder_delay;     /*!< Time to wait for reordered packet gaps
                                   *   to be fixed before doing ER */
    boolean strip_rtp;            /*!< If RTP headers are to be stripped  */

                                  /*!< Params for PCM to create FEC buffer */
    vqec_fec_info_t fec_info;
    uint16_t fec_default_block_size; 

    /**
     * TR-135 control parameters.
     */
    vqec_ifclient_tr135_params_t tr135_params;

#if HAVE_FCC
    void (*pre_primary_repairs_done_cb)(uint32_t data);
                                  /*!< Callback to signify that last repair pak
                                   *   before first prim pak was received */
    uint32_t pre_primary_repairs_done_data;
                                  /*!< Data to be passed to above callback */
#endif  /* HAVE_FCC */
} vqec_pcm_params_t;

typedef enum vqec_pcm_seq_num_cmp_ {
    VQEC_PCM_SEQ_NUM_EQ = 0,
    VQEC_PCM_SEQ_NUM_LT,
    VQEC_PCM_SEQ_NUM_GT,
} vqec_pcm_seq_num_cmp_t;

typedef enum vqec_pcm_insert_error_ {
    VQEC_PCM_INSERT_ERROR_DUPLICATE_PAK = 0,
    VQEC_PCM_INSERT_ERROR_BAD_RANGE_PAK,
    VQEC_PCM_INSERT_ERROR_BAD_RANGE_SOME,
    VQEC_PCM_INSERT_ERROR_LATE_PAK,
    VQEC_PCM_INSERT_ERROR_LATE_SOME,
    VQEC_PCM_INSERT_PAK_SEQ_INSERT_FAIL,
    VQEC_PCM_INSERT_OK,
} vqec_pcm_insert_error_t;

const char * vqec_pcm_insert_error_to_str(vqec_pcm_insert_error_t err);


vqec_dp_error_t vqec_pcm_module_init(uint32_t max_pcms);
vqec_dp_error_t vqec_pcm_module_deinit(void);

boolean vqec_pcm_init(vqec_pcm_t *pcm, vqec_pcm_params_t *pcm_params);

/**
 * vqec_pcm_insert_packet
 * Insert a pak into pcm
 * @param[in] pcm ptr of pcm
 * @param[in] ptr to the vector of packets to insert
 * @param[in] number of packets in the vector being inserted
 * @param[in] indicated whether or not this vector contains contiguous packets
 * @param[in,out] ses_stats per-session stats to be updated w.r.t.
 *                          results of pcm insert attempt (optional) 
 * @return number of packets successfully inserted into the PCM
 */
uint32_t
vqec_pcm_insert_packets(vqec_pcm_t *pcm, vqec_pak_t *paks[], 
                        int num_paks, boolean contig,
                        vqec_drop_stats_t *ses_stats);

/**
 * vqec_pcm_remove_packet.
 * Remove a pak from pcm, this pak is just removed from pcm, memory maybe
 * not freeed if ref-count is not 0.
 * @param[in] pcm ptr of pcm
 * @param[in] pak ptr of packet removed from pcm
 * @param[in] remove_time is used to log the remove time of the pak
 * @return TRUE of FALSE.
 */
boolean vqec_pcm_remove_packet(vqec_pcm_t *pcm, vqec_pak_t *pak,
                               abs_time_t remove_time);

/**
 * vqec_pcm_gap_get_gaps
 * Get latest gaps from gapmap and put them into an array of vqec_gap_t's.
 *
 * @param[in]    pcm     Pcm from which gaps are to be retrieved.
 * @param[in/out] gapbuff   Buffer (array) of vqec_dp_gap_t's in which to store
 *                          collected gaps.
 * @param[out]   more   This will be set to TRUE if gapbuff ran out of room
 *                      before gap collection was finished; in this case,
 *                      there MAY or MAY NOT be more uncollected gaps in the
 *                      specified range.  If this is set to FALSE, then there
 *                      are no more gaps in the range.
 * @return              TRUE on success; FALSE on failure
 */
boolean vqec_pcm_gap_get_gaps(vqec_pcm_t *pcm,
                              vqec_dp_gap_buffer_t *gapbuf,
                              boolean *more);

/**
 * vqec_pcm_gap_search
 * Walks the gapmap with a stride looking for set bits (gaps) and returns the
 * number and values of them.
 *
 * @param[in]    pcm       Pointer to the PCM.
 * @param[in]    start     Seq num at which to start the search.
 * @param[in]    stride    Stride (increment) of each iteration of the search.
 *                         Must be greater than 0 and less than 256.
 * @param[in]    num_paks  Number of sequence numbers to search through.  This
 *                         is NOT a search range, but the number of search
 *                         steps to iterate through.
 * @param[in/out] gap_seq_buf   Buffer (array) of vqec_seq_num_t's in which to
 *                              store the results of the search, i.e. the seq
 *                              nums that correspond to gaps in the gapmap.
 *                              This buffer must have no fewer than num_paks
 *                              elements allocated for it.
 */
uint16_t vqec_pcm_gap_search(vqec_pcm_t *pcm,
                             vqec_seq_num_t start,
                             uint8_t stride,
                             uint8_t num_paks,
                             vqec_seq_num_t *gap_seq_buf);

/*
 * vqec_pcm_gap_flush
 * Remove all pak/gap data from the pcm's internal gapmap.
 *
 * @param[in]    pcm    Pointer to the PCM.
 */
void vqec_pcm_gap_flush(vqec_pcm_t *pcm);

/**
 * vqec_pcm_deinit
 * @param[in] pcm ptr of pcm to deinit
 */
void vqec_pcm_deinit(vqec_pcm_t *pcm);

/**
 * vqec_pcm_flush
 * Flush all the packets in the cache and gapmap, also
 * reset all the internal variables.
 * @param[in] pcm ptr of pcm
 * @return TRUE or FALSE
 */
boolean vqec_pcm_flush(vqec_pcm_t *pcm);

/**
 * vqec_pcm_get_status
 * Get all status and counters for PCM.
 * @param[in] pcm pcm module to retrieve info from
 * @param[in] cumulative Boolean flag to retrieve cumulative status
 * @param[out] s structure to stuff info into
 */
void vqec_pcm_get_status(vqec_pcm_t *pcm, vqec_dp_pcm_status_t *s, 
                         boolean cumulative);

/**
 * vqec_pcm_counter_clear
 * Clear pcm counters
 * @param[in] pcm ptr of pcm
 */
void vqec_pcm_counter_clear(vqec_pcm_t *pcm);

/*
 * Update last received sequence number.
 *
 * @param[in] pcm Pointer to the pcm instance.
 * @param[in] seq_num Received sequence number.
 */
static inline void 
vqec_pcm_set_last_rx_seq_num(vqec_pcm_t *pcm, vqec_seq_num_t seq_num)
{
    pcm->last_rx_seq_num = seq_num;
}

/*
 * Get last received sequence number.
 *
 * @param[in] pcm Pointer to the pcm instance.
 * @param[out] vqec_seq_num_t Last received sequence number.
 */
static inline vqec_seq_num_t 
vqec_pcm_get_last_rx_seq_num(vqec_pcm_t *pcm)
{
    return pcm->last_rx_seq_num;
}

/*
 * Get highest received sequence number (aka "tail").
 *
 * @param[in] pcm Pointer to the pcm instance.
 * @param[out] vqec_seq_num_t Highest received sequence number.
 */
static inline vqec_seq_num_t 
vqec_pcm_get_highest_rx_seq_num (vqec_pcm_t *pcm)
{
    return pcm->tail;
}

/*
 * Records the PCM sequence number to be used by a failover packetflow source
 * whose packets will be inserted the PCM next.  Only gaps with sequence
 * numbers higher than this will be reported for a new packetflow source.
 *
 * This should ONLY be called when a failover source is assigned for a
 * unicast channel.  For multicast channels, gap reporting is independent
 * of source.
 * 
 * @param[in] pcm Pointer to the pcm instance.
 */
static inline void 
vqec_pcm_set_pktflow_src_seq_num_start (vqec_pcm_t *pcm)
{
    pcm->pktflow_src_seq_num_start_set = TRUE;
    pcm->pktflow_src_seq_num_start =
        vqec_next_seq_num(vqec_pcm_get_highest_rx_seq_num(pcm));
}

/*
 * Clears the record of the PCM lowest sequence number in use by the
 * current paktflow source.
 *
 * @param[in] pcm Pointer to the pcm instance.
 */
static inline void 
vqec_pcm_clear_pktflow_src_seq_num_start (vqec_pcm_t *pcm)
{
    pcm->pktflow_src_seq_num_start_set = FALSE;
}

/*
 * get value of candidates_recently_checked
 * pcm must not be NULL
 */
static inline boolean vqec_pcm_candidates_recently_checked_get(vqec_pcm_t *pcm)
{
    return pcm->candidates_recently_checked;
}

/*
 * reset value of candidates_recently_checked
 * pcm must not be NULL
 */
static inline void vqec_pcm_candidates_recently_checked_reset(vqec_pcm_t *pcm)
{
    pcm->candidates_recently_checked = FALSE;
}


/*
 * get the average packet time
 * pcm must not be NULL
 */
static inline rel_time_t vqec_pcm_avg_pkt_time_get(vqec_pcm_t *pcm)
{
    if (pcm->ts_calculation_done &&
        TIME_CMP_R(lt, pcm->avg_pkt_time, pcm->new_rtp_ts_pkt_time)) {
        return pcm->new_rtp_ts_pkt_time;
    } else {
        return pcm->avg_pkt_time;
    }
}

/*
 * get the default delay
 * pcm must not be NULL
 */
static inline rel_time_t vqec_pcm_default_delay_get(vqec_pcm_t *pcm)
{
    return pcm->default_delay;
}

/*
 * cache pcm state to first_prim values
 * pcm must not be NULL
 */
static inline void vqec_pcm_cache_state_first_prim(vqec_pcm_t *pcm)
{
    pcm->pre_prim_outp_stats_done = TRUE;
}

/*
 * bump the total tx packets counter
 * pcm must not be NULL
 */
static inline void vqec_pcm_total_tx_paks_bump(vqec_pcm_t *pcm)
{
    pcm->stats.total_tx_paks++;
}

/*
 * bump the bad rcv ts counter
 * pcm must not be NULL
 */
static inline void vqec_pcm_bad_rcv_ts_bump(vqec_pcm_t *pcm)
{
    pcm->stats.bad_rcv_ts++;
}

/*
 * bump the output gap counter
 * pcm must not be NULL
 */
static inline void
vqec_pcm_output_gap_counter_bump(vqec_pcm_t *pcm,
                                 uint32_t num_lost)
{
    if (pcm->pre_prim_outp_stats_done) {
        pcm->stats.output_loss_pak_counter += num_lost;
        pcm->stats.output_loss_hole_counter++;
    } else {
        pcm->stats.output_loss_paks_first_prim += num_lost;
        pcm->stats.output_loss_holes_first_prim++;
    }
}

/**
 * Log output gap.  The choice of which log to use (pre- or post-primary)
 * is made internally.
 */
static inline void
vqec_pcm_output_gap_log(vqec_pcm_t *pcm,
                        vqec_seq_num_t start,
                        vqec_seq_num_t end,
                        abs_time_t time)
{
    if (pcm->pre_prim_outp_stats_done) {
        vqec_log_insert_output_gap(&pcm->log,
                                   vqec_next_seq_num(start),
                                   vqec_pre_seq_num(end),
                                   time);
    } else {
        vqec_log_insert_output_gap_direct(&pcm->first_prim_log,
                                          vqec_next_seq_num(start),
                                          vqec_pre_seq_num(end),
                                          time);
    }
}

/**
 * vqec_pcm_timeout_old_candidates
 * @brief
 * This function times out (removes) all candidates that are older than the
 * gap_hold_time minus the cur_time.
 *
 * Every time the output scheduler wakes up, it'll check to see if the
 * candidate array has been recently updated (i.e. a packet has been inserted
 * since the last time the output scheduler checked this), and if not, then
 * time out all packets that should have timed out already.  Then, the output
 * scheduler will reset the candidates_recently_checked flag.
 */
void
vqec_pcm_timeout_old_candidates(vqec_pcm_t *pcm,
                                abs_time_t cur_time);

/**
 * vqec_pcm_get_next_packet 
 * Retrieve a handle to the next packet from pcm.
 * @param[in] pcm ptr of pcm
 * @param[in] pak ptr of packet to start 
 * @return ptr of next packet, return NULL if not found.
 */
vqec_pak_t * 
vqec_pcm_get_next_packet(vqec_pcm_t *pcm, const vqec_pak_t *pak);

/**
 * vqec_pcm_inorder_pak_list_get_next
 * Get the next inorder packet from the pcm.
 * @param[in] pcm ptr of pcm
 * @return ptr of the next inorder packet, NULL otherwise.
 */
vqec_pak_t *vqec_pcm_inorder_pak_list_get_next(vqec_pcm_t *pcm);

/**
 * Enable ER / under-run detection post RCC unicast burst, or after an
 * RCC abort has taken place.
 *
 * @param[in] pcm Pointer to the pcm instance.
 */
void
vqec_pcm_notify_rcc_en_er(vqec_pcm_t *pcm);

/**
 * Update FEC delay as a change in default parameters has bee detected
 * by the FEC module. 
 *
 * @param[in] pcm Pointer to the pcm instance.
 * @param[in] fec_delay The FEC delay to be used.
 */
void
vqec_pcm_update_fec_delay(vqec_pcm_t *pcm, rel_time_t fec_delay);

/**
 * Capture a snaphot of PCM's state.
 *
 * @param[in] pcm Pointer to the pcm instance.
 * @param[out] snap Pointer that will contain the snapshot.
 */
#if HAVE_FCC
void
vqec_pcm_cap_state_snapshot(vqec_pcm_t *pcm, vqec_dp_pcm_snapshot_t *snap);

/**
 * Capture RCC related output state from PCM.
 *
 * @param[in] pcm Pointer to the pcm instance.
 * @param[out] outp RCC related output data information.
 */
void
vqec_pcm_cap_outp_rcc_data(vqec_pcm_t *pcm, 
                           vqec_dp_pcm_outp_rcc_data_t *outp);

/**
 * RCC has been aborted: pcm must reset it's state including a flush
 * of packet buffers.
 *
 * @param[in] pcm Pointer to the pcm instance.
 */
void
vqec_pcm_notify_rcc_abort(vqec_pcm_t *pcm);

#endif   /* HAVE_FCC */


/**
 * Get the gap log instance.
 *
 * @param[in] pcm Pointer to the pcm instance.
 */
static inline vqec_log_t *
vqec_pcm_gap_log_ptr (vqec_pcm_t *pcm)
{
    if (pcm) {
        return (&pcm->log);
    }

    return (NULL);
}

#if HAVE_FCC
/**---------------------------------------------------------------------------
 * Identify if we need to post process a repair packet if RCC is aborted. 
 *
 * @param[in] pcm :  pcm pointer. 
 * @param[in] pak:   the pak that will be tested. 
 * 
 * @return: TRUE if the packet will be post processed, otherwise, FALSE.
 *---------------------------------------------------------------------------*/

boolean vqec_pcm_rcc_post_abort_process(vqec_pcm_t *pcm, vqec_pak_t *pak);


/**--------------------------------------------------------------------------
 * Update the PCM rcc_post_abort_process flag if needed.
 *
 * @param[in] pcm:  Pcm instance.
 * update the flag if the condition meet.
 *--------------------------------------------------------------------------*/

void vqec_pcm_update_rcc_post_abort_process(vqec_pcm_t *pcm);

/**--------------------------------------------------------------------------
 * Get the PCM rcc_post_abort_process flag 
 *
 * @param[in] pcm Pcm instance.
 * return the flag if PCM is valid
 *--------------------------------------------------------------------------*/

boolean vqec_pcm_get_rcc_post_abort_process (vqec_pcm_t *pcm);
#endif

/** 
 * Function to set TR-135 config parameters in the channel PCM
 *
 * @param[in] pcm Pcm instance
 * @param[in] config Pointer to tr135 configurable parameter structure
 */
void vqec_pcm_set_tr135_params(vqec_pcm_t *pcm, 
                               vqec_ifclient_tr135_params_t *config);

/**
 * Update the tr135 state machine.
 *
 * @param[in] tr135 Pointer to a tr135 total statistics data structure.
 * @param[in] tr135_s Pointer to a tr135 sample statistics data structure.
 * @param[in] seq_num The current RTP packet sequence number.
 * @param[in] gmin to be used for TR-135 stats collection.
 * @param[in] severe_loss_min_distance to be used for TR-135 stats collection.
 */
void
vqec_pcm_update_tr135_stats(vqec_tr135_stream_total_stats_t *tr135,
                            vqec_tr135_stream_sample_stats_t *tr135_s,
                            vqec_seq_num_t seq_num,
                            uint32_t gmin,
                            uint32_t severe_loss_min_distance);

/** 
 * Function to set error enable flag in the PCM
 *
 * @param[in] pcm Pcm instance
 */
void vqec_pcm_set_er_en_ts(vqec_pcm_t *pcm);

/** 
 * Function to update TR-135 overrun counter
 *
 * @param[in] pcm Pcm instance
 * @param[in] increment Integer expressing increment step
 */
void vqec_pcm_log_tr135_overrun(vqec_pcm_t *pcm, int increment);

/**
 * Function to update TR-135 underrrun counter
 *
 * @param[in] pcm Pcm instance
 * @param[in] increment Integer expressing increment step
 */
void vqec_pcm_log_tr135_underrun(vqec_pcm_t *pcm, int increment);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VQEC_PCM_H */
