/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: DP API main header file. This defines the main API for control-plane
 * users, and sub-includes other API files distributed in the directory hierarchy.
 *
 * Documents: 
 *
 *****************************************************************************/

#ifndef __VQEC_DP_API_H__
#define __VQEC_DP_API_H__

#include <vqec_dp_api_types.h>
#include <vqec_dp_io_stream.h>
#include <vqec_dp_debug.h>

/**
 ***********************************************************
 * Toplevel DP APIs.
 ***********************************************************
 */

/**
 * Set the default DP polling interval.
 *
 * @param[in] interval  Default polling interval (in ms).
 */
void vqec_dp_tlm_set_default_polling_interval(uint16_t interval);

/**
 * Set the DP scheduling interval for fast-scheduled tasks.
 *
 * @param[in] interval  Fast-schedule interval (in ms).
 */
void vqec_dp_tlm_set_fast_sched_interval(uint16_t interval);

/**
 * Set the DP scheduling interval for slow-scheduled tasks.
 *
 * @param[in] interval  Slow-schedule interval (in ms).
 */
void vqec_dp_tlm_set_slow_sched_interval(uint16_t interval);

/**
 * Set drop parameters for a session type. If en is true, drop
 * simulation is enabled, otherwise drop simulation is disabled. The
 * new parameter set is always accepted.
 * @param[in]  session  Type of session for which parameters are being set
 * @param[in]  state State structure that defines all drop parameters.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if successful.
 */
RPC vqec_dp_error_t 
vqec_dp_set_drop_params(INV vqec_dp_input_stream_type_t session,
                        INR vqec_dp_drop_sim_state_t *state);

/**
 * Get the current state of drop parameters for a session type.
 * @param[in]  session  Type of session for which parameters are being retrieved.
 * @param[out] state State structure that defines all drop parameters.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if successful.
 */
RPC vqec_dp_error_t
vqec_dp_get_drop_params(INV vqec_dp_input_stream_type_t session,
                        OUT vqec_dp_drop_sim_state_t *state);

/**
 * Specifies the output timing of replicated APP packets.
 *
 * @param[in]  delay_ms  Amount (in ms) to delay each APP packet.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
RPC vqec_dp_error_t
vqec_dp_set_app_cpy_delay(INV int32_t delay_ms);

/**
 * Gets the amount to delay each APP packet.
 *
 * @param[out] rel_time_t  Amount (in ms) to delay each APP packet.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success, or
 *                              VQEC_DP_ERR_INVALIDARGS on failure.
 */
RPC vqec_dp_error_t
vqec_dp_get_app_cpy_delay(OUT rel_time_t *delay_ms);

/**
 * Enables/disables FEC globally (for all channels), for the purposes
 * of comparing picture quality during product demonstrations.
 *
 * NOTE:  While FEC is disabled, received FEC packets are not processed.
 *        However, some channel state and debugs visible on the CLI may
 *        still give the appearance that FEC is enabled.
 *        
 *
 * @param[in] enabled - TRUE to enable, FALSE to disable
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
RPC vqec_dp_error_t
vqec_dp_set_fec(INV boolean enabled);

/**
 * Gets the global setting of the FEC demo CLI knob.
 * If the demo CLI knob is set, its value is returned in "fec".
 * If the demo CLI knob is unset, the value returned in "fec" is undefined.
 *
 * param[out] fec     - TRUE:  FEC is enabled for demo purposes
 *                      FALSE: FEC is disabled for demo purposes
 * param[out] fec_set = TRUE:  FEC value is set for demo purposes
 *                      FALSE: FEC value is unset for demo purposes
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
RPC vqec_dp_error_t
vqec_dp_get_fec(OUT boolean *fec, OUT boolean *fec_set);

/**
 * Initialize the dataplane: a method which should be called once at
 * dataplane initialization time (may need to add other parameters).
 */
RPC vqec_dp_error_t
vqec_dp_init_module(INR vqec_dp_module_init_params_t *params);

/**
 * De-initializes the data plane.
 */
RPC vqec_dp_error_t
vqec_dp_deinit_module(void);

/**
 * [No auto-generation for these calls]
 */

/**
 * Open the dataplane module.
 */
vqec_dp_error_t
vqec_dp_open_module(void);

/**
 * Close the dataplane module.
 */
vqec_dp_error_t
vqec_dp_close_module(void);

/**
 * Create an upcall socket .
 */
RPC vqec_dp_error_t
vqec_dp_create_upcall_socket(INV vqec_dp_upcall_socktype_t type,
                             INR vqec_dp_upcall_sockaddr_t  *addr, 
                             INV uint16_t len);

/**
 * Close all upcall sockets.
 */
RPC vqec_dp_error_t
vqec_dp_close_upcall_sockets(void);

/**
 * Benchmarking functions
 *  Note : no enable/disable functions (kernel dp code not shared with VQE-S)
 */
RPC vqec_dp_error_t
vqec_dp_get_benchmark(INV uint32_t interval,
                      OUT uint32_t *cpu_usage);

/**
 * Get operating mode of the dataplane.
 */
RPC vqec_dp_error_t
vqec_dp_get_operating_mode(OUT vqec_dp_op_mode_t *mode);

/**
 ***********************************************************
 * TLM APIs.
 ***********************************************************
 */

/**
 * Gets the status of the dataplane's global packet pool
 *
 * @param[out] vqec_pak_poolid_t Global packet pool's ID, or 
 *                                VQEC_PAK_POOLID_INVALID if not allocated
 */
RPC vqec_dp_error_t
vqec_dp_get_default_pakpool_status(OUT vqec_pak_pool_status_t *status);

/**
 * Retrieve all of the global debug counters kept within the TLM.
 *
 * @param[out] counters Pointer to the data structure than holds
 * the debug counters. 
 */
RPC vqec_dp_error_t
vqec_dp_get_global_counters(OUT vqec_dp_global_debug_stats_t *stats);

/**
 ***********************************************************
 * Input Shim APIs.
 ***********************************************************
 */

/**
 * Get status information from the input shim.
 *
 * @param[in] status The global status of the input shim.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success. 
 */
RPC vqec_dp_error_t
vqec_dp_input_shim_get_status(OUT vqec_dp_input_shim_status_t *status);

/**
 * Get filter table from input shim module.
 *
 * @param[out] filters array of filters to be populated by this function
 * @param[in/out] num_filters as input, number of filters the passed-in
 *                            filter array can hold; as output, number of
 *                            filters that were actually copied into the array
 */
RPC vqec_dp_error_t
vqec_dp_input_shim_get_filters(OUTARRAY(vqec_dp_display_ifilter_t,
                                        VQEC_DP_INPUTSHIM_MAX_FILTERS));


/**
 * Get stream information from the input shim module.
 * Can be used to get a set of streams the caller cares about
 * (via the "requested_streams" and "requested_streams_array_size"
 * parameters) or an arbitrary set not specified by ID (e.g. all streams).
 *
 * @param[in]  requested_streams - (optional) an array of input shim os_id
 *                                  values corresponding to the streams for
 *                                  which information is requested.
 *                                 NULL must be passed if the caller does not
 *                                  want to request particular streams by ID.
 *                                 If a request includes an os_id with
 *                                  value VQEC_DP_INVALID_OSID, then the
 *                                  corresponding entry in the "returned
 *                                  streams" array will be zero'd out.
 *                                 If a request is made for an os_id which
 *                                  cannot be found, processing aborts and
 *                                  the error code VQEC_DP_ERR_NOSUCHSTREAM
 *                                  is returned.
 * @param[in]  requested_streams_array_size -
 *                                 Size of "requested_streams" array, with 
 *                                  a value of 0 implying that the caller 
 *                                  does not want to request particular 
 *                                  streams by ID.
 * @param[out] returned_streams  - Array into which requested stream info
 *                                  is copied.  If a "requested_streams"
 *                                  array was supplied, then the returned
 *                                  streams in this array will correspond
 *                                  (based on index position) to IDs supplied
 *                                  in the "requested_streams" array.
 * @param[in]  returned_streams_array_size -
 *                                 Size of "returned_streams" array
 * @param[out] returned_streams_count -
 *                                 Number of streams whose information have
 *                                  been copied into the "returned_streams" 
 *                                  array.  Includes streams which have
 *                                  zero'd out based on a request for a
 *                                  stream with ID of VQEC_DP_INVALID_OSID.
 * @param[out] vqec_dp_error_t   - VQEC_DP_ERR_OK upon success, or
 *                                  an error code indicating reason for failure
 */
RPC vqec_dp_error_t
vqec_dp_input_shim_get_streams(
    INARRAY(vqec_dp_isid_t, VQEC_DP_INPUTSHIM_MAX_STREAMS),
    OUTARRAY(vqec_dp_display_os_t, VQEC_DP_INPUTSHIM_MAX_STREAMS));

/**
 * Open an RTP socket in the dataplane. 
 *
 * @param[out]  addr  IP address of newly opened socket
 * @param[out]  port  port number of newly opened socket
 * @param[out]  vqec_dp_error_t  VQEC_DP_ERR_OK upon success
 */
RPC vqec_dp_error_t
vqec_dp_input_shim_open_sock(
    INOUT in_addr_t *addr,
    INOUT in_port_t *port);

/**
 * Close an RTP socket in the dataplane.
 * 
 * @param[in]  addr  IP address of open socket
 * @param[in]  port  port number of open socket
 * @param[out] vqec_dp_error_t  VQEC_DP_ERR_OK upon success
 */
RPC vqec_dp_error_t
vqec_dp_input_shim_close_sock(
    INV in_addr_t addr,
    INV in_port_t port);

/**
 ***********************************************************
 * RTP Receiver APIs.
 ***********************************************************
 */

/** 
 * Enable packetflow for a particular source. The method will be meaningful
 * only for primary sessions, and will return a failure code for all other 
 * session types. For primary sessions, only one source at any
 * given time will have packetflow enabled. This method will displace any
 * previous source that had packeflow enabled, and disable packetflow on it.
 *
 * @param[in] id Input stream (IS) identifier of the stream. Has global
 * scope amongst all input streams.
 * @param[in] key Key of the source on which packetflow should be enabled.
 * @param[out] session_rtp_seq_num_offset Sequence offset between the
 *                1) session's rtp sequence number space and 
 *                2) source's rtp sequence number space (in use by the
 *                   newly chosen packetflow source)
 * @param[out] vqec_dp_error_t OK if successful.
 */
RPC vqec_dp_error_t vqec_dp_chan_rtp_input_stream_src_permit_pktflow(
    INV vqec_dp_streamid_t id,
    INR vqec_dp_rtp_src_key_t *key,
    OUT int16_t *session_rtp_seq_num_offset);

/** 
 * Disable packetflow for a particular source. The method will be meaningful
 * only for primary sessions, and will return a failure code for all other 
 * session types. The primary session will have no packetflow sources after
 * this call completes.
 *
 * @param[in] id Input stream (IS) identifier of the stream.
 * @param[in] key Key of the source on which packetflow should be disabled.
 * @param[out] vqec_dp_error_t OK if successful.
 */
RPC vqec_dp_error_t vqec_dp_chan_rtp_input_stream_src_disable_pktflow(
    INV vqec_dp_streamid_t id,
    INR vqec_dp_rtp_src_key_t *key);

/** 
 * Delete a source for a RTP session, and optionally retrieve
 * the entire source entry table. 
 *
 * @param[in] id Input stream (IS) identifier of the stream.
 * @param[in] key Key of the source which should be deleted.
 * @param[out] table Instance of the table in which the dp's copy  
 * will be returned. The instance may be specified as NULL. 
 * @param[out] vqec_dp_error_t OK if successful.
 */
RPC vqec_dp_error_t vqec_dp_chan_rtp_input_stream_src_delete(
    INV vqec_dp_streamid_t id,
    INR vqec_dp_rtp_src_key_t *key,
    OUT_OPT vqec_dp_rtp_src_table_t *table);

/**
 * Retrieve the entire source entry table for a RTP session.
 *
 * @param[in] id Input stream (IS) identifier of the stream.
 * @param[out] table Instance of the table in which the dp's copy  
 * will be returned.
 * @param[out] vqec_dp_error_t OK if successful.
 */
RPC vqec_dp_error_t vqec_dp_chan_rtp_input_stream_src_get_table(
    INV vqec_dp_streamid_t id,
    OUT vqec_dp_rtp_src_table_t *table);

/**
 * Retrieve info for the RTCP XR Media Acquisition report
 * 
 * @param[in] id Identifier of a RTP input stream.
 * @param[out] xr_ma_stats Pointer to the XR RMA stats for the RTP source.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 */
RPC vqec_dp_error_t vqec_dp_chan_rtp_input_stream_xr_ma_get_info (
    INV vqec_dp_streamid_t id,
    OUT rtcp_xr_ma_stats_t *xr_ma_stats);

/**
 * Multi-op call. It can be used to update the prior statistics, and
 * retrieve the information for a particular source entry, or optionally 
 * the rtp session statistics.
 *
 * @param[in] id Input stream (IS) identifier of the stream.
 * @param[in] key Key of the source which is to be retrieved.
 * @param[in] update_prior Indicates whether prior source counters should  
 * be updated as a result of this call.
 * @param[in] reset_xr_stats Indicates whether to reset the XR stats counters.
 * @param[out] info RTP source information output pointer.
 * @param[out] xr_stats XR statistics for the RTP source. 
 * @param[out] post_er_stats XR Post Repair stats.
 * @param[out] xr_ma_stats XR MA stats for the RTP source.
 * @param[out] sess_info RTP session information output pointer. 
 * @param[out] vqec_dp_error_t OK if successful.
 */
RPC vqec_dp_error_t vqec_dp_chan_rtp_input_stream_src_get_info(
    INV vqec_dp_streamid_t id,
    INR vqec_dp_rtp_src_key_t *key,
    INV boolean update_prior,
    INV boolean reset_xr_stats,
    OUT_OPT vqec_dp_rtp_src_entry_info_t *info, 
    OUT_OPT rtcp_xr_stats_t *xr_stats,
    OUT_OPT rtcp_xr_post_rpr_stats_t *post_er_stats,
    OUT_OPT rtcp_xr_ma_stats_t *xr_ma_stats,
    OUT_OPT vqec_dp_rtp_sess_info_t *sess_info);

/**
 * Retrieve the session information (statistics) for a RTP session.
 * Optionally the caller may also retrieve the entire source table.
 *
 * @param[in] id Input stream (IS) identifier of the stream.
 * @param[out] info Instance of the session information in which the
 * dataplane's copy will be returned.
 * @param[out] table Instance of the table in which the dataplane's copy  
 * will be returned. The instance may be specified as NULL. 
 * @param[out] vqec_dp_error_t OK if successful.
 */
RPC vqec_dp_error_t vqec_dp_chan_input_stream_rtp_session_get_info(
    INV vqec_dp_streamid_t id,
    OUT vqec_dp_rtp_sess_info_t *info,
    OUT_OPT vqec_dp_rtp_src_table_t *table);


/**
 * Setup an SSRC filter on a input stream. Optionally the source table
 * table for the input stream may be returned as well.
 *
 * @param[in] id Input stream (IS) identifier of the stream.
 * @param[in] ssrc SSRC filter value.
 * @param[out] table Instance of the table in which the dataplane's copy  
 * will be returned. The instance may be specified as NULL. 
 * @param[out] vqec_dp_error_t OK if successful.
 */
RPC vqec_dp_error_t vqec_dp_chan_input_stream_rtp_session_add_ssrc_filter(
    INV vqec_dp_streamid_t id,		
    INV uint32_t ssrc,
    OUT_OPT vqec_dp_rtp_src_table_t *table);


/**
 * Remove an SSRC filter on a input stream. 
 *
 * @param[in] id Input stream (IS) identifier of the stream.
 * @param[out] vqec_dp_error_t OK if successful.
 */
RPC vqec_dp_error_t vqec_dp_chan_input_stream_rtp_session_del_ssrc_filter(
    INV vqec_dp_streamid_t id);


/**
 * Acknowledge IRQ on a channel with the response.
 */
RPC vqec_dp_error_t
vqec_dp_chan_ack_upcall_irq(INV vqec_dp_chanid_t chanid,
                            INV vqec_dp_upcall_device_t device,
                            INV vqec_dp_streamid_t device_id, 
                            OUT vqec_dp_irq_ack_resp_t *resp);

/**
 * Poll a channel for pending IRQs.
 */
RPC vqec_dp_error_t
vqec_dp_chan_poll_upcall_irq(INV vqec_dp_chanid_t chanid,
                             OUT vqec_dp_irq_poll_t *poll);


/**
 * Get gaps from a channel observed within the given source's transmission.
 */
RPC vqec_dp_error_t
vqec_dp_chan_get_gap_report(INV vqec_dp_chanid_t chanid,
                            OUT vqec_dp_gap_buffer_t *gapbuf, 
                            OUT boolean *more);

#ifdef HAVE_FCC
/**
 * Gather the PAT for a DP channel.
 */ 
RPC vqec_dp_error_t
vqec_dp_chan_get_pat(INV vqec_dp_chanid_t chanid,
                     OUT vqec_dp_psi_section_t *pat);

/**
 * Gather the PMT for a channel.
 */ 
RPC vqec_dp_error_t
vqec_dp_chan_get_pmt(INV vqec_dp_chanid_t chanid,
                     OUT uint16_t *pmt_pid,
                     OUT vqec_dp_psi_section_t *pmt);

/**
 * Gather the PCR value for a channel.
 */ 
RPC vqec_dp_error_t
vqec_dp_chan_get_pcr(INV vqec_dp_chanid_t chanid,
                     OUT uint64_t *pcr);

/**
 * Gather the PTS value for a channel.
 */ 
RPC vqec_dp_error_t
vqec_dp_chan_get_pts(INV vqec_dp_chanid_t chanid,
                     OUT uint64_t *pts);

/**
 * Process the APP packet from the control-plane. This will setup the
 * RCC state machine, and wait for the first sequence number on the
 * repair stream.
 */ 
RPC vqec_dp_error_t
vqec_dp_chan_process_app(INV vqec_dp_chanid_t chanid,
                         INR vqec_dp_rcc_app_t *app,
                         INARRAY(uint8_t, VQEC_DP_IPC_MAX_PAKSIZE));

/**
 * Control-plane invoked RCC abort.
 */
RPC vqec_dp_error_t
vqec_dp_chan_abort_rcc(INV vqec_dp_chanid_t chanid);
#endif   /* HAVE_FCC */

/**
 * Get status and stats for PCM / NLL / FEC / DPchan modules of a channel.
 */
RPC vqec_dp_error_t
vqec_dp_chan_get_status(INV vqec_dp_chanid_t chanid,
                        OUT_OPT vqec_dp_pcm_status_t *pcm_s,
                        OUT_OPT vqec_dp_nll_status_t *nll_s,
                        OUT_OPT vqec_dp_fec_status_t *fec_s,
                        OUT_OPT vqec_dp_chan_stats_t *chan_s, 
                        OUT_OPT vqec_dp_chan_failover_status_t *failover_s,
                        INV boolean cumulative);

/**
 * Get TR-135 sample stats from a channel's PCM
 */
RPC vqec_dp_error_t
vqec_dp_chan_get_stats_tr135_sample(INV vqec_dp_chanid_t chanid,
                                    OUT 
                                    vqec_ifclient_stats_channel_tr135_sample_t
                                    *stats);

/**
 * Get gap and sequence logs for inputs and outputs.
 */
RPC vqec_dp_error_t
vqec_dp_chan_get_seqlogs(INV vqec_dp_chanid_t chanid,
                         OUT_OPT vqec_dp_gaplog_t *in_log,
                         OUT_OPT vqec_dp_gaplog_t *out_log,
                         OUT_OPT vqec_dp_seqlog_t *seq_log);

/**
 * Clear stats for PCM, NLL, and FEC modules on a channel.
 */
RPC vqec_dp_error_t
vqec_dp_chan_clear_stats(INV vqec_dp_chanid_t chanid);

/**
 * Get RCC status / counters.
 */
#if HAVE_FCC
RPC vqec_dp_error_t
vqec_dp_chan_get_rcc_status(INV vqec_dp_chanid_t chanid,
                            OUT vqec_dp_rcc_data_t *rcc_data);
#endif   /* HAVE_FCC */

/**
 * Gets a snapshot of a histogram which may be used by the control plane 
 * (e.g. for display purposes).
 *
 * @param[in]  hist               - specifies histogram to retrieve
 * @param[out] target             - buffer into which histogram is copied
 * @param[in]  target_max_buckets - max size of histogram to be retrieved
 * @param[out] vqec_dp_error_t    - VQEC_DP_ERR_OK or failure code
 */
RPC vqec_dp_error_t
vqec_dp_chan_hist_get(INV vqec_dp_hist_t hist,
                      OUT vqec_dp_histogram_data_t *hist_ptr);

/**
 * Clear a histogram's data
 *
 * @param[in]  hist             - specifies histogram to be cleared
 * @parampout] vqec_dp_error_t  - VQEC_DP_ERR_OK or failure code
 */
RPC vqec_dp_error_t
vqec_dp_chan_hist_clear(INV vqec_dp_hist_t hist);

/**
 * Enable/Disable measurement logging for the output scheduling histogram
 *
 * @param[in]  enable            - TRUE to enable logging, FALSE to disable
 * @param[out] vqec_dp_error_t   - VQEC_DP_ERR_OK or failure code
 */
RPC vqec_dp_error_t
vqec_dp_chan_hist_outputsched_enable(INV boolean enable);

/**
 * Returns whether measuringment logging is enabled for the output
 * scheduling histogram.
 *
 * @param[out] enabled           - TRUE if monitoring is enabled,
 *                                 FALSE if it is disabled 
 * @param[out] vqec_dp_error_t   - VQEC_DP_ERR_OK or failure code
 */
RPC vqec_dp_error_t
vqec_dp_chan_hist_outputsched_is_enabled(OUT boolean *enabled);

/**
 * If the FEC information of the channel (L, D, Annex) need to be updated,
 * return the value to CP and tell CP to do this. 
 *
 * @param[in] chanid Identifier of the dpchan instance.
 * @param[out] fec_info  fec information returned here
 * @param[out] true if needed, otherwise false
 */
RPC boolean 
vqec_dpchan_fec_need_update(INV vqec_dp_chanid_t chanid, 
                            OUT vqec_fec_info_t *fec_info);

/**
 ***********************************************************
 * Graph Builder APIs.
 ***********************************************************
 */

/**
 * Build the flowgraph for the given channel descriptor. A new
 * graph is always built.
 * [The case with SSRC multiplexing is not handled in this 
 * implementation].
 *
 * The flow of data through the graph is started, once the graph
 * has been built within the context of this call. The identifier of
 * the flowgraph which contains the channel is returned in *id.
 * 
 * @param[in] chan Poiner to the channel descriptor.
 * @param[in] tid Tuner identifier for the tuner that should be
 * added to this flowgraph. The tuner identifier corresponds to
 * a resource in the outputshim. 
 * @param[in] tuner_encap The encapsulation type that the
 * tuner wishes at  it's output interface (RTP or UDP). 
 * @param[out] graphinfo returns pertinent information about the graph that
 * was just created.
 * @param[out] dp_err_t Return DP_ERR_OK on success, 
 * error code on failure.
 */

RPC vqec_dp_error_t
vqec_dp_graph_create(INR vqec_dp_chan_desc_t *chan,
                     INV vqec_dp_tunerid_t tid,
                     INV vqec_dp_encap_type_t tuner_encap,
                     OUT vqec_dp_graphinfo_t *graphinfo);

/**
 * Delete the flowgraph for a channel. In the current implementation
 * each flowgraph maps to exactly one channel.
 * 
 * @param[in] id Flowgraph id to which the channel is associated.
 * @param[out] dp_error_t Return DP_ERR_OK on success, 
 * error code on failure.
 */
RPC vqec_dp_error_t
vqec_dp_graph_destroy(INV vqec_dp_graphid_t id);

/**
 * Add a new tuner to a channel's flowgraph. 
 *
 * @param[in] id Flowgraph id to which the channel is associated.
 * @param[in] tid Tuner identifier for the tuner which corresponds to
 * an output shim resource.
 * @param[in] tuner_encap The encapsulation type that the
 * tuner wishes at  it's output interface (RTP or UDP). 
 * @param[out] dp_error_t Return DP_ERR_OK on success, 
 * error code on failure.
 */
RPC vqec_dp_error_t
vqec_dp_graph_add_tuner(INV vqec_dp_graphid_t id, 
                        INV vqec_dp_tunerid_t tid);

/**
 * Remove a tuner from a channel's flowgraph. 
 *
 * @param[in] tid Tuner identifier for the tuner which corresponds to
 * an output shim resource.
 * @param[out] dp_error_t Return DP_ERR_OK on success, 
 * error code on failure.
 */
RPC vqec_dp_error_t
vqec_dp_graph_del_tuner(INV vqec_dp_tunerid_t tid);


/**
 * Inject a packet corresponding to the transmit direction for the 
 * repair stream of the specified graph. A non-logical hack used for NAT.
 * This is the method where the "buf" input argument's length is not
 * fixed. Thus, this API may not lend itself to automatic IPC wrapper 
 * generation scripts.
 */
RPC vqec_dp_error_t
vqec_dp_graph_repair_inject(INV vqec_dp_graphid_t id,
                            INV in_addr_t remote_addr,
                            INV in_port_t remote_port,
                            INARRAY(char, VQEC_DP_IPC_MAX_PAKSIZE)); 

/**
 * Return the socket file descriptor of the filter that is bound
 * to the inputshim output stream for the repair session
 * of the specified dataplane graph.
 *
 * @param[in] id Dataplane tuner identifier.
 * @param[out] int Returns a file descriptor, or -1 if not found.
 */
RPC int
vqec_dp_graph_filter_fd(INV vqec_dp_graphid_t id);

/**
 * Inject a packet corresponding to the transmit direction for the 
 * primary stream of the specified graph. A non-logical hack used for NAT.
 * This is the method where the "buf" input argument's length is not
 * fixed. Thus, this API may not lend itself to automatic IPC wrapper 
 * generation scripts.
 */
RPC vqec_dp_error_t
vqec_dp_graph_primary_inject(INV vqec_dp_graphid_t id,
                            INV in_addr_t remote_addr,
                            INV in_port_t remote_port,
                            INARRAY(char, VQEC_DP_IPC_MAX_PAKSIZE)); 

/**
 ***********************************************************
 * Channel APIs.
 ***********************************************************
 */

/**
 * Set TR-135 configuration
 *
 * @param[in] chanid Identifier of the channel.
 * @param[in] config Pointer to tr-135 status data
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success
 */
RPC vqec_dp_error_t
vqec_dp_chan_set_tr135_params(INV vqec_dp_chanid_t chanid, 
                              INR vqec_ifclient_tr135_params_t *params);

/**
 * Update a channel's source information
 *
 * @param[in] chanid       Identifier of the channel
 * @param[in] primary_fil  new primary source filtering information
 * @param[in] repair_fil   new repair source filtering information
 * @param[out] dp_error_t Return VQEC_DP_ERR_OK on success, 
 * error code on failure.
 */
RPC vqec_dp_error_t
vqec_dp_chan_update_source(INV vqec_dp_chanid_t chanid,
                           INV vqec_dp_input_filter_t *primary_fil,
                           INV vqec_dp_input_filter_t *repair_fil);


/**
 ***********************************************************
 * Output Shim APIs.
 ***********************************************************
 */

/**
 * Get status information from the output shim.
 *
 * @param[in] status Pointer to the status data structure.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success. 
 */
RPC vqec_dp_error_t vqec_dp_output_shim_get_status(
    OUT vqec_dp_output_shim_status_t *status);

/**
 * Create a new dataplane tuner abstraction, and return an 
 * identifier to it. The creation operation is invoked by the
 * control-plane.  
 *
 * A dataplane tuner simply provides a mechanism to identify an input
 * stream on the output shim that is receiving repaired data from 
 * a particular dataplane source.
 *
 * In our split user-kernel reference model, a special VQE client 
 * socket will be attached to a tuner. For that case vqec_ifclient_recvmsg
 * will simply become a wrapper for a socket read call. The socket
 * fd-to-tuner mapping shall be maintained inside of the client in
 * this initial implementation. The chief reason for that is there isn't
 * an easy, explicit mechanism to flush sockets without closing them.
 * Hence, equivalence between tuner identifiers which are persistent
 * across the lifetime of the application, and sockets that need to
 * be closed and re-opened at every channel change is not possible.
 * 
 * @param[in] cp_tid A tuner-identification index that is maintained by
 * the control-plane. This is stored as part of the tuner's state when
 * it is created, and is not used otherwise in the dataplane.
 * @param[out] dp_tuner_id_t Id of the created tuner. If tuner creation
 * fails, returns INVALID_TUNER_ID.
 * @param[in] char[] NULL-terminated name of the tuner to be created. Must
 * be unique.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success. 
 */
RPC vqec_dp_error_t vqec_dp_output_shim_create_dptuner(
    INV int32_t cp_tid,
    OUT vqec_dp_tunerid_t *tid,
    INARRAY(char, VQEC_DP_TUNER_MAX_NAME_LEN));

/**
 * Destroy an existing tuner. For correct operation, any sockets that
 * are mapped to the tuner (for the split user-kernel implementation)
 * must be closed, and any input streams that are mapped to the tuner
 * must be unmapped, prior to the destroy operation. If these 
 * conditions are not met, destroy shall fail. 

 * The alternative, i.e., shutdown the socket, and simply ignore the
 * input stream's packet data, is not implemented, but is open for
 * later adaptation.
 *
 * @param[in] id Id of an existing tuner.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t vqec_dp_output_shim_destroy_dptuner(
    INV vqec_dp_tunerid_t tid);

/**
 * Associate a user-space socket with a dataplane tuner so that it may be used
 * for output.
 *
 * @param[in] tid A valid dataplane tuner handle.
 * @param[in] sockfd Socket file descriptor.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t
vqec_dp_output_shim_add_tuner_output_socket(
    INV vqec_dp_tunerid_t tid,
    INV int sockfd);

/**
 * Get status information for a dataplane tuner.
 *
 * @param[in] id A valid dataplane tuner handle.
 * @param[in] cumulative Boolean flag to retrieve cumulative status
 * @param[out] status Status structure for the result.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t
vqec_dp_output_shim_get_tuner_status(
    INV vqec_dp_tunerid_t tid,
    OUT vqec_dp_output_shim_tuner_status_t *status,
    INV boolean cumulative);

/**
 * Print into a buffer the list of tuner IDs connected to a sream.
 *
 * @param[in] id ISID of the particular stream
 * @param[out] buf buffer to print the output string into
 * @param[in] buf_len size of the above output buffer; should be equal
 *                    to VQEC_DP_OUTPUT_SHIM_PRINT_BUF_SIZE
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t
vqec_dp_output_shim_mappedtidstr(INV vqec_dp_isid_t id,
                                 OUTARRAY(char, 
                                          VQEC_DP_OUTPUT_SHIM_PRINT_BUF_SIZE));

/**
 * Clear counters associated with a dataplane tuner.
 *
 * @param[in] id A valid dataplane tuner handle.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t
vqec_dp_output_shim_clear_tuner_counters(INV vqec_dp_tunerid_t tid);


#ifndef __KERNEL__
/**
 * This function is used to receive a VQEC repaired RTP datagram stream,  
 * or a raw UDP stream, corresponding to a channel. The function
 * can be used as an "on-demand" reader in either user- or kernel- space,
 * although it's envisioned that a "push" mechanism will be used in kernel.
 * RTP headers may be stripped, based on the global configuration settings.
 *  
 * The routine returns the total number of bytes that have been copied
 * into the input iobuf array in <I>*len</I>. Received datagrams 
 * are copied into the caller's first buffer until no more whole datagrams
 * fit (or a packet supporting fast-channel-change is received--more on this
 * later). If the received datagram is too big to fit in the space remaining 
 * in the current buffer, an attempt will be made to write the datagram to
 * the beginning of the next buffer, until all buffers are exhausted.
 *
 * VQEC only supports receipt of datagrams upto length of 
 * max_paksize (from init params). A maximum of max_iobuf_cnt (from init
 * params) buffers can be returned in one single call. 
 *
 * Callers are responsible for passing in an array of iobufs, and assigning
 * the "buf_ptr" and "buf_len" fields of each iobuf in the array.  Upon 
 * return, the "buf_wrlen" and "buf_flags" fields will be assigned for 
 * each buffer, indicating the amount of data written to the buffer and 
 * any status flags, respectively.
 *
 * The "buf_flags" field may contain the VQEC_MSG_FLAGS_APP upon return.
 * This flag indicates MPEG management information that may be useful
 * for fast-channel-change has been received inline with RTP or UDP PDUs.
 * This only occurs at the onset of channel-change, and before the
 * reception of RTP or UDP payloads.  Upon receipt of a fast-channel-change
 * APP packet, the APP packet will be copied into the next available iobuf,
 * and the VQEC_MSG_FLAGS_APP flag will be set.  The API call will return
 * immediately after copying the APP packet into the buffer and before filling
 * any remaining buffers with UDP or RTP datagrams, if present.
 *
 * If no receive datagrams are available, and timeout set to 0, the call
 * returns immediately with the return value VQEC_DP_ERR_OK, and
 * <I>*len</I> set to 0. Otherwise any available datagrams
 * are copied into the iobuf array, and <I>*len</I> is the
 * total number of bytes copied. This is non-blocking mode of operation.
 *
 * If timeout > 0, then the call will block until either of the 
 * following two conditions is met:
 *
 * (a) iobuf_num datagram buffers are received before timeout 
 *     is reached. <BR>                
 * (b) The timeout occurs before iobuf_num buffers are received. 
 *	In this case the call will return with the number of buffers 
 *	that have been received till that instant. If no data is
 *     available, the return value will be VQEC_DP_ERR_OK, with
 *     <I>*len</I> set to 0.
 *  
 * If timeout is -1, then the call will block indefinitely until 
 * iobuf_num datagram buffers are received. 
 * 
 * If the invocation of the call fails  <I>*len</I> will be
 * set to 0, with the return value indicative of the failure condition.
 *
 * @param[in]	id Existing tuner object's identifier.
 * @param[in]   clientkey Client pthread key for the invoking thread.
 * @param[in]	iobuf Array of buffers into which received data is copied.
 * @param[in]	iobuf_num Number of buffers in the input array.
 * @param[out]  len The total number of bytes copied. The value 
 * shall be 0 if no data became available within the specified timeout, which
 * itself maybe 0. The return code will be VQEC_OK for these cases.
 * If the call fails, or if the tuner is deleted, channel
 * association changed, etc. while the call was blocked bytes_read will still
 * be 0, however, the return code will signify an error.
 * @param[in]	timeout Timeout specified in milliseconds. A timeout
 * of -1 corresponds to an infinite timeout. In that case the call  
 * will return only after iobuf_num datagrams have been received. A timeout
 * of 0 corresponds to a non-blocking mode of operation.
 * Timeout is always truncated to VQEC_DP_MSG_MAX_RECV_TIMEOUT.
 * @param[out]	vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success with the total number of bytes copied in <I>*len</I>. Upon failure, 
 * <I>*len</I> is 0, with following return codes:
 *
 *     <I>VQEC_DP_ERR_NOSUCHTUNER</I><BR>
 *     <I>VQEC_DP_ERR_NOSUCHSTREAM</I><BR>
 *     <I>VQEC_DP_ERR_INVALIDARGS</I><BR>
 *     <I>VQEC_DP_ERR_INTERNAL</I><BR>
 *     <I>VQEC_DP_ERR_NOMEM</I><BR>
 */
vqec_dp_error_t vqec_dp_output_shim_tuner_read(
    vqec_dp_tunerid_t id,
    vqec_iobuf_t *iobuf,
    uint32_t iobuf_num,
    uint32_t *len,
    int32_t timeout_msec);

#endif /* !__KERNEL__ */

/**
 * Part of the iterator to scan all input streams that have been created.
 * This method returns the opaque handle of the "first" input stream. The 
 * ordering between "first" and "next" is impelementation-specific. If there 
 * are no input streams, an invalid handle is returned.
 *
 * @param[out] id Returns either the handle of the first is or
 * the invalid handle if there are no input streams.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t 
vqec_dp_output_shim_get_first_is(OUT vqec_dp_isid_t *id);

/**
 * Part of the iterator to scan all input streams that have been created.
 * This method returns the opaque handle of the "next" input stream  
 * given the "prev" handle. The ordering between "next" and "prev" is
 *  impelementation specific. If there are no more input streams, an 
 * invalid handle is returned.The following behavior is sought:
 *
 * (a)  Atomic semantics for the iterator may be enforced by the caller, if
 * needed, since a race exists between the addition of new input streams, 
 * and termination of the iteration. 
 * (b) If no input streams are created or destroyed, an implementation 
 * of the iterator must return each input stream's handle exactly once
 * before terminating.
 * (c) The behavior, when the caller does not impose atomic semantics,
 * and the collection of input streams is modified, is left unspecified.
 *
 * @param[in] prev_is Handle of the "previous" input stream handed out
 * by the iterator.
 * @param[out] id Returns either the handle of the next is or
 * the invalid handle if there are no additional output streams.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t 
vqec_dp_output_shim_get_next_is(INV vqec_dp_isid_t prev_is, 
                                OUT vqec_dp_isid_t *id);

/**
 * Get statistics and information on a particular stream for display.
 *
 * @param[in] id Handle of the relevant input stream.
 * @param[out] is_info Pointer to the input stream info structure.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
RPC vqec_dp_error_t
vqec_dp_output_shim_get_is_info(INV vqec_dp_isid_t isid,
                                OUT vqec_dp_display_is_t *is_info);

/**
 * Part of the iterator to scan all tuners that have been created.
 * This method returns the opaque handle of the "first" tuner. The 
 * ordering between "first" and "next" is impelementation-specific. If there 
 * are no tuners, an invalid handle is returned.
 *
 * @param[out] tid Returns either the handle of the first
 * tuner or the invalid handle if there are no tuners.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t
vqec_dp_output_shim_get_first_dp_tuner(OUT vqec_dp_tunerid_t *tid);

/**
 * Part of the iterator to scan all dp tuners that have been created.
 * This method returns the opaque handle of the "next" tuner  
 * given the "prev" handle. The ordering between "next" and "prev" is
 *  impelementation specific. If there are no more tuners, an 
 * invalid handle is returned.The following behavior is sought:
 *
 * (a)  Atomic semantics for the iterator may be enforced by the caller, if
 * needed, since a race exists between the addition of new tuners, 
 * and termination of the iteration. 
 * (b) If no tuners are created or destroyed, an implementation 
 * of the iterator must return each dp tuner's handle exactly once
 * before terminating.
 * (c) The behavior, when the caller does not impose atomic semantics,
 * and the collection of tuners is modified, is left unspecified.
 *
 * @param[in] prev_tid Handle of the "previous" dp tuners handed out
 * by the iterator.
 * @param[out] next_tid Returns either the handle of the next tuner
 *  or the invalid handle if there are no additional tuners.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t
vqec_dp_output_shim_get_next_dp_tuner(INV vqec_dp_tunerid_t prev_tid,
                                      OUT vqec_dp_tunerid_t *next_tid);

/**
 * Gets a snapshot of the output thread's exit-to-enter jitter histogram
 * for a particular dataplane tuner.
 *
 * @param[in] dp_tid - Dataplane tuner id.
 * @param[out]  hist_ptr - Buffer into which histogram is copied.
 * @param[out] vqec_dp_error_t    - VQEC_DP_ERR_OK or failure code
 */
RPC vqec_dp_error_t
vqec_dp_output_shim_hist_get(INV vqec_dp_tunerid_t dp_tid,
                             INV vqec_dp_outputshim_hist_t type,                             
                             OUT vqec_dp_histogram_data_t *hist_ptr);

/**
 ***********************************************************
 * VQE-C DP Debug APIs.
 ***********************************************************
 */

/**
 * Enable a DP debug flag.
 *
 * @param[in] type The flag type to enable.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t
vqec_dp_debug_flag_enable(INV uint32_t type);

/**
 * Disable a DP debug flag.
 *
 * @param[in] type The flag type to disable.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t
vqec_dp_debug_flag_disable(INV uint32_t type);

/**
 * Get the status of a DP debug flag.
 *
 * @param[in] type The flag type to get the status on.
 * @param[out] flag Returns whether or not the flag is enabled.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
RPC vqec_dp_error_t
vqec_dp_debug_flag_get(INV uint32_t type, OUT boolean *flag);

/**
 ***********************************************************
 * VQE-C DP Debug APIs.
 ***********************************************************
 */

/**
 * Launch the DP test reader module, if available.  If destination address and
 * port are specified as 0, then reader module streaming task will be stopped.
 *
 * @param[in] params  Parameters structure specific to the test reader module.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
RPC vqec_dp_error_t
vqec_dp_test_reader(INV test_vqec_reader_params_t params);

/** 
 * Function to log the ER enable timestamp in the PCM 
 * 
 * @param[in] chanid Identifier of the channel
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success
 */
RPC vqec_dp_error_t
vqec_dp_set_pcm_er_en_flag(INV vqec_dp_chanid_t chanid);

#endif /* __VQEC_DP_API_H__ */
