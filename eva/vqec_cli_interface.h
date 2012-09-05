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
 * Description: CP/DP CLI interface implementation.
 *
 * Documents:
 *
 *****************************************************************************/

#ifndef __VQEC_CLI_INTERFACE_H__
#define __VQEC_CLI_INTERFACE_H__

#include "vqec_tuner.h"
#include "vqec_dp_api.h"

/**
 * Print status information for the input shim to the file stream.
 */
void vqec_cli_input_shim_show_status(void);

/**
 * Print the description of all filters that are currently bound to
 * some OS  to the file stream.
 */
void vqec_cli_input_shim_show_filters(void);

/**
 * Print a description of all OS's created on the input shim.
 */
void vqec_cli_input_shim_show_streams(void);

/**
 * Print a description of all OS's created on the input shim for a given tuner.
 *
 * @param[in] id   tuner ID whose output shim ISes are to be displayed
 */
void vqec_cli_input_shim_show_streams_tuner(vqec_tunerid_t id);

/**
 * Print out PCM statistics.
 *
 * @param[in] stats     pcm dp stats pointer
 * @param[in] brief_flag Boolean brief flag specifying optional brief output
 */
void vqec_cli_pcm_print_stats(vqec_dp_pcm_status_t *stats, boolean brief_flag); 

/**
 * Dump all status info on the PCM module.
 *
 * @param[in] chanid Channel ID of the channel whose PCM module to get status
 * @param[in] brief_flag Boolean brief flag specifying optional brief output
 */
void vqec_cli_pcm_dump(vqec_dp_chanid_t chanid, boolean brief_flag);

/**
 * Print state of the NLL to console.
 * 
 * @param[in] info - pointer of info.
 */
void vqec_cli_nll_print_stats(vqec_dp_nll_status_t *info);

/**
 * Dump NLL information.
 *
 * @param[in] chanid Channel ID of the channel assocaited with the NLL module
 * @param[in] brief_flag Boolean brief flag specifying optional brief output
 */
void vqec_cli_nll_dump(vqec_dp_chanid_t chanid, boolean brief_flag);

/**
 * Displays information about fec.
 */
void vqec_cli_fec_show(void);

/**
 * Print out FEC statistics.
 *
 * @param[in] stats     fec dp stats pointer
 * @param[in] brief_flag Boolean brief flag specifying optional brief output
 */
void vqec_cli_fec_print_stats(vqec_dp_fec_status_t *stats, boolean brief_flag);

/**
 * Dump all status info on the FEC module.
 *
 * @param[in] chanid Channel ID of the channel whose FEC module to get status
 * @param[in] brief_flag Boolean brief flag specifying optional brief output
 */
void vqec_cli_fec_dump(vqec_dp_chanid_t chanid, boolean brief_flag);

/**
 * Clear all stats counters in the PCM, NLL, and FEC modules.
 *
 * @param[in] chanid Channel ID of the channel for which to clear stats
 */
void vqec_cli_chan_clear_stats(vqec_dp_chanid_t chanid);

/**
 * Print status information for the output shim to the file stream.
 */
void vqec_cli_output_shim_show_status(void);

/**
 * Print a description of all IS's created on the output shim.
 */
void vqec_cli_output_shim_show_streams(void);

/**
 * Print a description of the IS created on the output shim for a given 
 * tuner.
 *
 * @param[in] id   Tuner ID whose output shim IS is to be displayed
 */
void vqec_cli_output_shim_show_stream_tuner(vqec_tunerid_t id);

/**
 * Print a description of one tuner created on the output shim.
 */
void vqec_cli_output_shim_show_tuner(vqec_dp_tunerid_t tid);

/**
 * Print a description of all tuners created on the output shim.
 */
void vqec_cli_output_shim_show_tuners(void);

/**
 * Print status information for the dataplane pak pool.
 */
void vqec_cli_pak_pool_dump(void);

/**
 * Print status information for the dataplane pak pool, and
 * internally acquire / release the global lock.
 */
void vqec_cli_pak_pool_dump_safe(void);

/**
 * Print counters / status for IPC.
 */
void vqec_cli_show_ipc_status_safe(void);

/**
 * Display dataplane global debug counters.
 * Acquires and releases the global lock.
 */
void
vqec_cli_show_dp_global_counters_safe(void);

/**
 * Display log sequence data from dataplane to the CLI.
 */ 
void 
vqec_cli_log_seq_dump(vqec_dp_seqlog_t *log_seq);

/**
 * Display gap lop data from dataplane to the CLI.
 */ 
void 
vqec_cli_log_gap_dump(vqec_dp_gaplog_t *gaplog, 
                      uint32_t options_mask,
                      const char *output_name);

/**
 * Dump PCM packet sequence / gap logs.
 */
void
vqec_cli_log_dump(vqec_dp_chanid_t chanid, uint32_t options_mask);

/**
 * Dump dataplane channel statistics.
 */
void
vqec_cli_dpchan_stats_dump(vqec_dp_chanid_t chanid);

/**
 * Print performance benchmark.
 */
void
vqec_cli_benchmark_show(void);

#endif /* __VQEC_CLI_INTERFACE_H__ */    
