/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: RTP Receiver.
 *
 * Documents:
 *
 *****************************************************************************/

#ifndef __VQEC_DP_RTP_RECEIVER_H__
#define __VQEC_DP_RTP_RECEIVER_H__

#include "vqec_dp_api_types.h"
#include <utils/queue_plus.h>
#include <utils/zone_mgr.h>
#include <utils/vam_time.h>

struct vqec_dp_chan_rtp_primary_input_stream_;
struct vqec_dp_chan_rtp_input_stream_;
struct vqec_pak_;

/**
 * RTP source data structure which is maintained per RTP receiver.
 */
typedef
struct vqec_dp_rtp_src_
{
    /**
     * Key.
     */
    vqec_dp_rtp_src_key_t key;
    /**
     * Dynamic Info.
     */    
    vqec_dp_rtp_src_entry_info_t info;
    /**
     * RTCP XR statistics.
     */
    rtcp_xr_stats_t *xr_stats;
    /**
     * RTCP post ER XR statistics.
     */
    rtcp_xr_post_rpr_stats_t *post_er_stats;
    /**
     * A non-sticky flag which is set to true whenever a new packet
     * is received on the source. It is used for inactivity detction to
     * declare the source active or inactive, and is cleared after each
     * throughput check. 
     */
    boolean rcv_flag;
    /**
     * Thread the source into a per receiver source list.
     */
    VQE_TAILQ_ENTRY(vqec_dp_rtp_src_) le_known_sources;
                                 
} vqec_dp_rtp_src_t;


/**
 * List of all sources known / accepted for a particular RTP receiver.
 */
typedef
struct vqec_dp_rtp_src_list_
{
    /**
     * Number of entries in the known or present list.
     */
    uint32_t present;
    /**
     * Number of entries created.
     */
    uint32_t created;
    /**
     * Number of entries destroyed.
     */
    uint32_t destroyed;
    /**
     * Head of the known source list.
     */
    VQE_TAILQ_HEAD(, vqec_dp_rtp_src_) lh_known_sources;
    /**
     * For cases where there is only one source on which packeflow
     * is allowed, that source can be "cached" in this entry (since the source
     * is always in the known sources list this is only done for
     * optimization and not a necessity).
     */         
    vqec_dp_rtp_src_t *pktflow_src;

} vqec_dp_rtp_src_list_t;

/**
 * A dataplane SSRC filter.
 */
typedef
struct vqec_dp_rtp_ssrc_filter_
{
    /**
     * SSRC to be filtered. 
     */
    uint32_t ssrc;
    /**
     * Enable filter.
     */
    boolean enable;
    /**
     * Filter drops.
     */
    uint64_t drops;

} vqec_dp_rtp_ssrc_filter_t;

/**
 * A dataplane RTP receiver.
 */
typedef
struct vqec_dp_rtp_recv_
{
    /**
     * Pointer back to parent RTP input stream.
     */
    struct vqec_dp_chan_rtp_input_stream_ *input_stream;
    /**
     * List of sources
     */
    vqec_dp_rtp_src_list_t src_list;
    /**
     * RTP session info.
     */
    vqec_dp_rtp_sess_info_t session;
    /**
     * SSRC filter info.
     */
    vqec_dp_rtp_ssrc_filter_t filter;
    /**
     * Maximum RTCP XR RLE size.
     */
    uint16_t max_xr_rle_size;
    /**
     * Maximum RTCP Post ER RLE size.
     */
    uint16_t max_post_er_rle_size;

} vqec_dp_rtp_recv_t;


/**
 * Initialize a RTP receiver instance.
 *
 * @param[in] Pointer to the receiver instance to be initialzed.
 * @param[in] par Parent RTP IS pointer.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERROR_OK on success.
 * @param[in] max_post_er_rle_size Maximum size of the post ER stats block to
 * use.
 */
vqec_dp_error_t
vqec_dp_chan_rtp_receiver_init(
    vqec_dp_rtp_recv_t *rtp_recv,
    struct vqec_dp_chan_rtp_input_stream_ *par,
    uint32_t max_rle_size,
    uint32_t max_post_er_rle_size);

/**
 * Deinitialize a RTP receiver instance.
 *
 * @param[in] Pointer to the receiver instance to be deinitialzed.
 */
void
vqec_dp_chan_rtp_receiver_deinit(vqec_dp_rtp_recv_t *rtp_recv);


/**---------------------------------------------------------------------------
 * Process packets that are in the failover queue.
 * Packets will either be flushed (removed from the queue and discarded)
 * or an attempt will be made to insert them into the PCM, based on the
 * caller's request.
 *
 * @param[in] in Pointer to a RTP input stream. 
 * @param[in] flush TRUE to discard packets, FALSE to attempt PCM insert
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_process_failoverq(
    struct vqec_dp_chan_rtp_primary_input_stream_ *in,
    boolean flush);

/**
 * RTP-specific processing for a single packet of the primary input stream.
 * If the packet should be accepted / delivered to pcm the method returns 
 * a true, otherwise it returns false.  If a packet represents an alternate
 * source that may be used upon failover, VQE will enqueue it internally
 * and returns false.
 * NOTE: It is assumed that both the primary stream and packet input
 * pointers are null-checked prior to invoking this function.
 *
 * @param[in] in Pointer to the rtp input stream.
 * @param[in] pak Pointer to a RTP packet.
 * @param[out] pak_rtp_src_entry (optional) If supplied and the function 
 * returns TRUE, this points to the packet's RTP source entry
 * @param[out] boolean Returns true if the packet should be delivered
 * to the next cache element.
 */
boolean
vqec_dp_chan_rtp_process_primary_pak(
    struct vqec_dp_chan_rtp_input_stream_ *in, 
    struct vqec_pak_ *pak,
    boolean *drop,
    vqec_dp_rtp_src_t **pak_rtp_src_entry);

/**---------------------------------------------------------------------------
 * RTP-specific processing for packets of the primary input stream
 * from a single RTP source (SSRC, source IP, source port).
 * Returns the number of packets accepted / delivered to the PCM.
 * NOTE: It is assumed that both the primary stream and packet input
 * pointers are null-checked prior to invoking this function.
 *
 * @param[in] in Pointer to the rtp input stream.
 * @param[in] pak_array Pointer to the packet vector which is to be processed.
 * @param[in] array_len Length of the array.
 * @param[in] current_time The current system time. The value can
 * be left unspecified, i.e., 0, and is essentially used for optimization.
 * @param[in] rtp_src_entry Pointer to the RTP source corresponding to
 * the packets in the pak_array
 * @param[out] uint32_t The method returns the number of packets
 * that were dropped due to failing RTP receive processing.  It will thus 
 * return a value less than or equal to array_len.  The return value
 * is for informational purpose, and may be ignored by the caller.
 *---------------------------------------------------------------------------*/
uint32_t
vqec_dp_chan_rtp_process_primary_paks_and_insert_to_pcm(
    struct vqec_dp_chan_rtp_input_stream_ *in, 
    struct vqec_pak_ *pak_array[],
    uint32_t array_len,
    abs_time_t current_time,
    vqec_dp_rtp_src_t *rtp_src_entry);

/**
 * RTP-specific processing for a single packet of the repair input stream.
 * If the packet should be accepted / delivered to pcm the method returns 
 * a true, otherwise it returns false. 
 * NOTE: It is assumed that both the repair stream and packet input
 * pointers are null-checked prior to invoking this function.
 *
 * @param[in] in Pointer a rtp input stream object.
 * @param[in] pak Pointer to a RTP packet.
 * @param[in] validate_hdr If true, validate the header.
 * @param[out] session_rtp_seq_num_offset RTP sequence space offset for
 * mapping this receiver's packets into the session's RTP sequence space.
 * @param[out] boolean Returns true if the packet should be delivered
 * to the next cache element.
 */
boolean
vqec_dp_chan_rtp_process_repair_pak(
    struct vqec_dp_chan_rtp_input_stream_ *in, 
    struct vqec_pak_ *pak, boolean validate_hdr,
    int16_t *session_rtp_seq_num_offset);

/**
 * RTP-specific processing for a single packet of the fec input stream.
 * If the packet should be accepted / delivered to fec cache the method 
 *  returns a true, otherwise it returns false. 
 * NOTE: It is assumed that both the fec stream and packet input
 * pointers are null-checked prior to invoking this function.
 *
 * @param[in] in Pointer a rtp input stream object.
 * @param[in] pak Pointer to a RTP packet.
 * @param[in] validate_hdr If true, validate the header.
 * @param[out] boolean Returns true if the packet should be delivered
 * to the next cache element.
 */
boolean
vqec_dp_chan_rtp_process_fec_pak(
    struct vqec_dp_chan_rtp_input_stream_ *in, 
    struct vqec_pak_ *pak, boolean validate_hdr);

/**---------------------------------------------------------------------------
 * RTP-specific processing for a single packet of the post-repair stream
 * for a channel.  This performs only RTP-related statistic updates.
 *
 * @param[in] in Pointer the rtp input stream object corresponding to the
 * PRIMARY stream of the packet's channel.
 * @param[in] pak Pointer to a RTP packet.
 * @param[in] now Current time.
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_process_post_repair_pak(
    struct vqec_dp_chan_rtp_input_stream_ *in, 
    struct vqec_pak_ *pak, abs_time_t now);

/**
 * Initialize the RTP receiver module.
 *
 * @param[in] params Pointer to the initialization parameters structure. 
 */
vqec_dp_error_t
vqec_dp_rtp_receiver_module_init(vqec_dp_module_init_params_t *params);

/**
 * Deinitialize the RTP receiver module.
 */
void
vqec_dp_rtp_receiver_module_deinit (void);

/**
 * Iterator for source entries: if the current key's 3-tuple 
 * (ssrc, address, port) is specified as 0, the key of the first source entry
 *  in the source list is returned in *next. If the key is non-0, then the 
 * successor of the list entry corresponding to the key is returned.
 *
 * @param[in] in Pointer to an RTP input stream.
 * @param[in] cur Key of the source whose successor is to be returned.
 * @param[out] next Key of the successor element.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise.
 */
vqec_dp_error_t 
vqec_dp_rtp_src_entry_get_next(struct vqec_dp_chan_rtp_input_stream_ *in, 
                               vqec_dp_rtp_src_key_t *cur, 
                               vqec_dp_rtp_src_key_t *next_key);


/**
 * Method which will scan a single RTP input stream's RTP receivers, and 
 * check all sources for age and activity detection.
 * 
 * @param[in] in Pointer to an RTP input stream.
 * @param[in] cur_time Current system time.
 */
void 
vqec_dp_chan_rtp_scan_one_input_stream(
    struct vqec_dp_chan_rtp_input_stream_ *in, 
    abs_time_t cur_time);


#endif /* __VQEC_DP_RTP_RECEIVER_H__ */
