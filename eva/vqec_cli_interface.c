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
 * Description: CP/DP CLI interface implementation.
 *
 * Documents:
 *
 *****************************************************************************/

#include "vam_util.h"
#include "vqe_port_macros.h"
#include "vqec_lock_defs.h"
#include "vqec_lock.h"
#include "vqec_pak.h"
#include "vqec_dp_api.h"
#include "vqec_upcall_event.h"
#include "vqec_recv_socket.h"
#include "vqec_syslog_def.h"
#include "vqec_ifclient_defs.h"
#include "vqec_tuner.h"
#include "vqec_channel_api.h"
#include "vqec_ifclient_private.h"
#include "vqec_event.h"
#ifndef HAVE_STRLFUNCS
#include <utils/strl.h>
#endif

/*
 * vqec_dp_input_shim_show_status()
 *
 * Print status information for the input shim to the file stream.
 */
void vqec_cli_input_shim_show_status (void)
{
    vqec_dp_input_shim_status_t s;

    bzero(&s, sizeof(vqec_dp_input_shim_status_t));

    if (vqec_dp_input_shim_get_status(&s) != VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, 
                     "DP input shim get status failed");
        return;
    }
    CONSOLE_PRINTF("Input Shim status:\n");
    CONSOLE_PRINTF("  State:                     %s\n",
                   s.is_shutdown ? "SHUTDOWN" : "OPERATIONAL");
    CONSOLE_PRINTF("  Output Streams created:    %u\n", s.os_creates);
    CONSOLE_PRINTF("  Output Streams destroyed:  %u\n", s.os_destroys);
    CONSOLE_PRINTF("  Filters:                   %u\n", s.num_filters);
    CONSOLE_PRINTF("  Internal Packet errors:    %llu\n", s.num_pkt_errors);
}

void vqec_cli_input_shim_show_one_filter (vqec_dp_display_ifilter_t *filt)
{
#define PORT_STR_LENGTH 6
    char port_str[PORT_STR_LENGTH];

    if (!filt) {
        return;
    }

    CONSOLE_PRINTF(" Filter (scheduling class %u):\n",
                   filt->scheduling_class);
    CONSOLE_PRINTF("  protocol:                   %s\n", 
                   (filt->filter.proto == INPUT_FILTER_PROTO_UDP
                    ? "UDP" : "UNKNOWN"));
    CONSOLE_PRINTF("  source IP:                  %s\n",
                   filt->filter.u.ipv4.src_ip_filter ?
                   inet_ntop_static_buf(filt->filter.u.ipv4.src_ip,
                                        INET_NTOP_STATIC_BUF0) :
                   "<any>");
    if (filt->filter.u.ipv4.src_port_filter) {
        snprintf(port_str, PORT_STR_LENGTH, "%u",
                 ntohs(filt->filter.u.ipv4.src_port));
    } else {
        snprintf(port_str, PORT_STR_LENGTH, "<any>");
    }
    CONSOLE_PRINTF("  source port:                %s\n", port_str);
    CONSOLE_PRINTF("  dest IP:                    %s\n",
                   inet_ntop_static_buf(filt->filter.u.ipv4.dst_ip,
                                        INET_NTOP_STATIC_BUF0));
    CONSOLE_PRINTF("  dest port:                  %u\n",
                   ntohs(filt->filter.u.ipv4.dst_port));
}

/*
 * vqec_dp_input_shim_show_filters()
 *
 * Print the description of all filters that are currently bound to
 * some OS to the file stream.
 *
 */
void vqec_cli_input_shim_show_filters (void)
{
    vqec_dp_display_ifilter_t filters[VQEC_DP_INPUTSHIM_MAX_FILTERS];
    uint32_t num_filters = VQEC_DP_INPUTSHIM_MAX_FILTERS;
    int i;

    bzero(filters,
          VQEC_DP_INPUTSHIM_MAX_FILTERS * sizeof(vqec_dp_display_ifilter_t));

    if (vqec_dp_input_shim_get_filters(filters, 
                                       VQEC_DP_INPUTSHIM_MAX_FILTERS, &num_filters) !=
        VQEC_DP_ERR_OK){
        syslog_print(VQEC_ERROR, 
                     "DP input shim get filters failed");
        return;
    }
    CONSOLE_PRINTF("\nInformation about %u filters:\n", num_filters);
    for (i = 0; i < num_filters; i++) {
        vqec_cli_input_shim_show_one_filter(&filters[i]);
        CONSOLE_PRINTF("\n");
    }
}

/**
 * Displays information about one input shim stream
 *
 * @param[in] name  - (Optional) short description of stream (e.g. "Primary")
 * @param[in] os    - stream information to be displayed
 */
void vqec_cli_input_shim_show_one_stream (const char *name,
                                          vqec_dp_display_os_t *os)
{
    char tmpbuf[VQEC_DP_STREAM_PRINT_BUF_SIZE];
    int tmpbuf_len = VQEC_DP_STREAM_PRINT_BUF_SIZE;
    boolean collect_stats = FALSE;

    (void)vqec_dp_debug_flag_get(VQEC_DP_DEBUG_COLLECT_STATS, &collect_stats);

    CONSOLE_PRINTF("%s Output Stream [ID '0x%08x']\n",
                   (name ? name : ""), os->os_id);
    CONSOLE_PRINTF(" Encaps type:                 %s\n",
                   vqec_dp_stream_encap2str(os->encaps, tmpbuf, tmpbuf_len));
    CONSOLE_PRINTF(" Capabilities:                %s\n",
                   vqec_dp_stream_capa2str(os->capa, tmpbuf, tmpbuf_len));
    if (os->has_filter) {
        vqec_cli_input_shim_show_one_filter(&os->filter);
    }
    CONSOLE_PRINTF(" Connected Input Stream ID:   %u\n", os->is_id);
    CONSOLE_PRINTF(" Stats:");
    if (collect_stats) {
        CONSOLE_PRINTF("  packets transmitted:        %llu\n",
                       os->packets);
        CONSOLE_PRINTF("  bytes transmitted           %llu\n",
                       os->bytes);
    }
    CONSOLE_PRINTF("  packets dropped:            %llu\n",
                   os->drops);
}

/*
 * vqec_dp_input_shim_show_streams()
 *
 * Print a description of all OS's created on the input shim.
 */
void vqec_cli_input_shim_show_streams (void)
{
    vqec_dp_display_os_t streams[VQEC_DP_INPUTSHIM_MAX_STREAMS];
    uint32_t num_streams = VQEC_DP_INPUTSHIM_MAX_STREAMS;
    int i;

    bzero(streams,
          VQEC_DP_INPUTSHIM_MAX_STREAMS * sizeof(vqec_dp_display_os_t));

    if (vqec_dp_input_shim_get_streams(NULL, 0,
                                       streams, VQEC_DP_INPUTSHIM_MAX_STREAMS,
                                       &num_streams) !=
        VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, 
                     "DP input shim get streams failed");
        return;
    }
    CONSOLE_PRINTF("Information about %u output streams:\n", num_streams);
    for (i = 0; i < num_streams; i++) {
        vqec_cli_input_shim_show_one_stream(NULL, &streams[i]);
        CONSOLE_PRINTF("\n");
    }
}

/**
 * Print a description of all OS's created on the input shim for a given tuner.
 *
 * @param[in] id   tuner ID whose output shim ISes are to be displayed
 */
void vqec_cli_input_shim_show_streams_tuner (vqec_tunerid_t id)
{
    vqec_chanid_t chanid;
    vqec_dp_streamid_t os_id[VQEC_MAX_STREAMS_PER_CHANNEL];
    vqec_dp_display_os_t streams[VQEC_MAX_STREAMS_PER_CHANNEL];
    uint32_t num_streams;

    if (vqec_tuner_get_chan(id, &chanid) != VQEC_OK) {
        return;
    }
    memset(os_id, 0,
           sizeof(vqec_dp_streamid_t) * VQEC_MAX_STREAMS_PER_CHANNEL);
    if (vqec_chan_get_inputshim_streams(chanid,
                                        &os_id[0],
                                        &os_id[1],
                                        &os_id[2],
                                        &os_id[3]) != VQEC_OK) {
        return;
    }
    if (vqec_dp_input_shim_get_streams(os_id, VQEC_MAX_STREAMS_PER_CHANNEL,
                                       streams,
                                       VQEC_MAX_STREAMS_PER_CHANNEL,
                                       &num_streams) !=
        VQEC_DP_ERR_OK) {
        return;
    }
    
    CONSOLE_PRINTF("Input Shim information for channel ID %0x08:\n", chanid);
    vqec_cli_input_shim_show_one_stream("Primary", &streams[0]);
    vqec_cli_input_shim_show_one_stream("Repair", &streams[1]);
    if (os_id[2] != VQEC_DP_INVALID_OSID) {
        vqec_cli_input_shim_show_one_stream("FEC0", &streams[2]);
    }
    if (os_id[3] != VQEC_DP_INVALID_OSID) {
        vqec_cli_input_shim_show_one_stream("FEC1", &streams[3]);
    }
}

/**
 * Print out PCM statistics.
 *
 * @param[in] stats     pcm dp stats pointer
 * @param[in] brief_flag Boolean brief flag specifying optional brief output
 */
void vqec_cli_pcm_print_stats (vqec_dp_pcm_status_t *stats, 
                               boolean brief_flag) 
{
    boolean collect_stats = FALSE;

    (void)vqec_dp_debug_flag_get(VQEC_DP_DEBUG_COLLECT_STATS, &collect_stats);

    if (!stats) {
        return;
    }
    
    if (brief_flag) {
        CONSOLE_PRINTF("PCM counters:\n");
        if (collect_stats) {
            CONSOLE_PRINTF(" input primary packets:     %lld\n",
                           stats->primary_packet_counter);
        }
        CONSOLE_PRINTF(" input loss packets:        %lld\n"
                       " input loss holes:          %lld\n",
                       stats->input_loss_pak_counter,
                       stats->input_loss_hole_counter);
        if (collect_stats) {
            CONSOLE_PRINTF(" repair received:           %lld\n",
                           stats->repair_packet_counter);
        }
        CONSOLE_PRINTF(" output loss packets:       %lld\n"
                       " output loss holes:         %lld\n"
                       " under runs:                %lld\n"
                       " late packets:              %lld\n",
                       stats->output_loss_pak_counter,
                       stats->output_loss_hole_counter,
                       stats->under_run_counter,
                       stats->late_packet_counter);
    } else {
        CONSOLE_PRINTF("PCM counters:");
        CONSOLE_PRINTF(" late packets:              %lld\n"
                       " head ge last seq reqstd:   %lld\n",
                       stats->late_packet_counter,
                       stats->head_ge_last_seq);
        if (collect_stats) {
            CONSOLE_PRINTF(" primary packets:           %lld\n"
                           " repair packets:            %lld\n",
                           stats->primary_packet_counter,
                           stats->repair_packet_counter);
        }
        CONSOLE_PRINTF(" input loss packets:        %lld\n"
                       " input loss holes:          %lld\n"
                       " output loss packets:       %lld\n"
                       " output loss holes:         %lld\n"
                       " pcm_insert drops:          %lld\n"
                       " duplicate packets:         %lld\n"
                       " pak_seq_insert fail paks:  %lld\n"
                       " bad seq range:             %lld\n"
                       " under runs:                %lld\n"
                       " bad receive timestamp:     %lld\n",
                       stats->input_loss_pak_counter,
                       stats->input_loss_hole_counter,
                       stats->output_loss_pak_counter,
                       stats->output_loss_hole_counter,
                       stats->pcm_insert_drops,
                       stats->duplicate_packet_counter,
                       stats->pak_seq_insert_fail_counter,
                       stats->seq_bad_range_packet_counter,
                       stats->under_run_counter,
                       stats->bad_rcv_ts);
        
        if (collect_stats) {
            CONSOLE_PRINTF(" duplicate repair packets:  %lld\n",
                           stats->duplicate_repairs);
        }
    }
}

/**
 * Dump PCM information.
 *
 * @param[in] chanid Channel ID of the channel assocaited with the PCM module
 * @param[in] brief_flag Boolean brief flag specifying optional brief output
 */
void vqec_cli_pcm_dump (vqec_dp_chanid_t chanid, boolean brief_flag)
{
    vqec_dp_pcm_status_t stats;

    bzero(&stats, sizeof(vqec_dp_pcm_status_t));

    if (vqec_dp_chan_get_status(chanid, 
                                &stats, 
                                NULL, 
                                NULL,
                                NULL, 
                                NULL, 
                                FALSE) != VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, "DP chan gets status failed");
        return;
    }

    if (brief_flag) {
        vqec_cli_pcm_print_stats(&stats, brief_flag);
    } else {
        CONSOLE_PRINTF("PCM status:\n");
        CONSOLE_PRINTF(" head:                      %u\n"
                       " tail:                      %u\n"
                       " highest_er_seq_num:        %u\n"
                       " last_reqstd_er_seq_num:    %u\n"
                       " last_rx_seq_num:           %u\n",
                       stats.head,
                       stats.tail,
                       stats.highest_er_seq_num,
                       stats.last_requested_er_seq_num,
                       stats.last_rx_seq_num);
        CONSOLE_PRINTF(" num_paks_in_pak_seq:       %d\n",
                       stats.num_paks_in_pak_seq);
        CONSOLE_PRINTF(" primary_received:          %s\n"
                       " repair_received:           %s\n"
                       " repair_trigger_time:       %lld\n"
                       " reorder_delay:             %lld\n"
                       " fec_delay:                 %lld\n"
                       " original jitter buff size: %lld\n"
                       " total delay including fec: %lld\n"
                       " gap_hold_time:             %lld\n",
                       stats.primary_received ? "true" : "false",
                       stats.repair_received ? "true" : "false",
                       TIME_GET_R(msec, stats.repair_trigger_time),
                       TIME_GET_R(msec, stats.reorder_delay),
                       TIME_GET_R(msec, stats.fec_delay),
                       TIME_GET_R(msec, stats.cfg_delay),
                       TIME_GET_R(msec, stats.default_delay),
                       TIME_GET_R(msec, stats.gap_hold_time));

        vqec_cli_pcm_print_stats(&stats, brief_flag);

        CONSOLE_PRINTF(" last tx seq num:           %u\n"
                       " last tx time:              %llu\n"
                       " total tx packets:          %lld\n",
                       (uint32_t)stats.outp_last_pak_seq,
                       TIME_GET_A(usec, stats.outp_last_pak_ts),
                       stats.total_tx_paks);
   
        CONSOLE_PRINTF("\n");
    }
}

/**
 * Print out statistics for the NLL.
 *
 * @param[in] info statistics structure for the NLL
 */
void vqec_cli_nll_print_stats (vqec_dp_nll_status_t *info)
{
    boolean collect_stats = FALSE;

    (void)vqec_dp_debug_flag_get(VQEC_DP_DEBUG_COLLECT_STATS, &collect_stats);

    CONSOLE_PRINTF("NLL state:\n");
    CONSOLE_PRINTF(" mode:                      %d\n"
                   " pred base:                 %llu\n"
                   " last act time:             %llu\n"
                   " primary offset:            %lld\n"
                   " pcr32 base:                %u\n"
                   " exp disc:                  %u\n"
                   " imp disc:                  %u\n",
                   info->mode,
                   TIME_GET_A(msec, info->pred_base),
                   TIME_GET_A(msec, info->last_actual_time),
                   TIME_GET_R(usec, info->primary_offset),
                   info->pcr32_base,
                   info->num_exp_disc,
                   info->num_imp_disc);
    if (collect_stats) {
        CONSOLE_PRINTF(" observations:              %u\n",
                       info->num_obs);
    }
    CONSOLE_PRINTF(" resets:                    %u\n"
                   " past predicts:             %u\n"
                   " reset_base w/o time:       %u\n",                   
                   info->resets,
                   info->predict_in_past,
                   info->reset_base_no_act_time);
}

/**
 * Dump NLL information.
 *
 * @param[in] chanid Channel ID of the channel assocaited with the NLL module
 * @param[in] brief_flag Boolean brief flag specifying optional brief output
 */
void vqec_cli_nll_dump (vqec_dp_chanid_t chanid, boolean brief_flag)
{
    vqec_dp_nll_status_t stats;

    bzero(&stats, sizeof(vqec_dp_nll_status_t));

    if (vqec_dp_chan_get_status(chanid, 
                                NULL, 
                                &stats, 
                                NULL, 
                                NULL, 
                                NULL, 
                                FALSE) != VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, "DP chan get status failed");
        return;
    }

    if (!brief_flag) {
        vqec_cli_nll_print_stats(&stats);
    }
}

/*
 * vqec_cli_fec_show()
 *
 * Displays information about fec.
 *
 * Params:
 *   @param[in]  cli  CLI context on which output should be displayed
 */
void
vqec_cli_fec_show (void)
{
    boolean fec = FALSE, fec_set = FALSE;
    uint16_t max_fec_block_observed = 0;

    (void)vqec_dp_get_fec(&fec, &fec_set);
    if (!fec_set) {
        fec = vqec_syscfg_get_ptr()->fec_enable;
    }
    CONSOLE_PRINTF("fec:                  %s%s",
                   fec ? "enabled" : "disabled",
                   fec_set ? " (set by CLI)" : "");

    if (fec) {
        max_fec_block_observed = 
            vqec_chan_get_max_fec_block_size_observed();
        CONSOLE_PRINTF("max block observed:   %u",
                       max_fec_block_observed);
    }
}

/**
 * print out the statistics information
 *
 * @param[in] stats     fec stats pointer
 * @param[in] brief_flag Boolean brief flag specifying optional brief output
 */
void vqec_cli_fec_print_stats (vqec_dp_fec_status_t *stats, boolean brief_flag)
{
    boolean collect_stats = FALSE;

    (void)vqec_dp_debug_flag_get(VQEC_DP_DEBUG_COLLECT_STATS, &collect_stats);

    if (!stats) {
        return;
    }

    if (brief_flag) {
        CONSOLE_PRINTF("FEC counters:");
        CONSOLE_PRINTF(" input fec packets:         %lld\n"
                       " fec recovered packets:     %lld\n"
                       " fec paks, unrecoverable:   %lld\n"
                       " late fec packets:          %lld\n"
                       " fec paks, other:           %lld\n",
                       stats->fec_total_paks,
                       stats->fec_recovered_paks,
                       stats->fec_unrecoverable_paks,
                       stats->fec_late_paks,                       
                       stats->fec_drops_other);
    } else {
        CONSOLE_PRINTF("FEC counters:");
        CONSOLE_PRINTF(" late fec packets:          %lld\n"
                       " fec recovered packets:     %lld\n",
                       stats->fec_late_paks,
                       stats->fec_recovered_paks);
        if (collect_stats) {
            CONSOLE_PRINTF(" no need to decode paks:    %lld\n",
                           stats->fec_dec_not_needed);
        }
        CONSOLE_PRINTF(" total fec packets:         %lld\n",
                       stats->fec_total_paks);
        CONSOLE_PRINTF(" duplicate fec packets:     %lld\n",
                       stats->fec_duplicate_paks);
        CONSOLE_PRINTF(" rtp hdr invalid paks:      %lld\n"
                       " fec hdr invalid paks:      %lld\n",
                       stats->fec_rtp_hdr_invalid, 
                       stats->fec_hdr_invalid);
        if (collect_stats) {
            CONSOLE_PRINTF(" fec paks, unrecoverable:   %lld\n",
                           stats->fec_unrecoverable_paks);
        }
        CONSOLE_PRINTF(" fec paks, other:           %lld\n"
                       " fec gaps detected:         %lld\n", 
                       stats->fec_drops_other,
                       stats->fec_gap_detected);
    }
}


/**
 * Dump FEC information.
 *
 * @param[in] chanid Channel ID of the channel associated with the FEC module
 * @param[in] brief_flag Boolean brief flag specifying optional brief output
 */
void vqec_cli_fec_dump (vqec_dp_chanid_t chanid, boolean brief_flag)
{
    vqec_dp_fec_status_t stats;

    bzero(&stats, sizeof(vqec_dp_fec_status_t));

    /* To be backward compatible, display "relative" stats */
    if (vqec_dp_chan_get_status(chanid, 
                                NULL, 
                                NULL, 
                                &stats, 
                                NULL, 
                                NULL, 
                                FALSE) != VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, "DP chan get status failed");
        return;
    }

    if (brief_flag) {
        CONSOLE_PRINTF("FEC status: \n");
        CONSOLE_PRINTF(" fec enabled in channel:    %s\n",
                       stats.is_fec_enabled ? "true" : "false");
        vqec_cli_fec_print_stats(&stats, brief_flag);
    } else {
        CONSOLE_PRINTF("\n");
        CONSOLE_PRINTF("FEC status: \n");
        CONSOLE_PRINTF(" fec enabled in channel:    %s\n"
                       " fec streams:               %d_D\n"
                       " fec_column_stream_avail:   %s\n"
                       " fec_row_stream_avail:      %s\n", 
                       stats.is_fec_enabled ? "true" : "false",
                       stats.avail_streams,
                       stats.column_avail ? "true" : "false",
                       stats.row_avail ? "true" : "false");
        CONSOLE_PRINTF(" L value:                   %d\n"
                       " D value:                   %d\n",
                       stats.L, 
                       stats.D);
        CONSOLE_PRINTF(" column head:               %u\n"
                       " column tail:               %u\n",
                       stats.fec_column_head, 
                       stats.fec_column_tail);

        CONSOLE_PRINTF(" row head:                  %u\n"
                       " row tail:                  %u\n",
                       stats.fec_row_head, 
                       stats.fec_row_tail);

        CONSOLE_PRINTF("\n");
        vqec_cli_fec_print_stats(&stats, brief_flag);
        CONSOLE_PRINTF("\n");
    }
}

/**
 * Clear statistics and counters in PCM, NLL, and FEC modules.
 */
void vqec_cli_chan_clear_stats (vqec_dp_chanid_t chanid)
{
    if (vqec_dp_chan_clear_stats(chanid) != VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, "DP chan clear stats failed");
    }
}

/**
 * Print status information for the output shim to the file stream.
 */
void vqec_cli_output_shim_show_status (void)
{
    vqec_dp_output_shim_status_t status;
    vqec_dp_error_t ret;

    bzero(&status, sizeof(vqec_dp_output_shim_status_t));

    ret = vqec_dp_output_shim_get_status(&status);
    if (ret == VQEC_DP_ERR_OK) {
        CONSOLE_PRINTF("Output shim status:\n");
        CONSOLE_PRINTF(" state:                     %s\n",
                       status.shutdown ? "SHUTDOWN" : "OPERATIONAL");
        CONSOLE_PRINTF(" is_creates:                %d\n", status.is_creates);
        CONSOLE_PRINTF(" is_destroys:               %d\n", status.is_destroys);
        CONSOLE_PRINTF(" num_tuners:                %d\n", status.num_tuners);
    }
}

/**
 * Print a description of a single IS on the output shim.
 */
void vqec_cli_output_shim_show_stream (vqec_dp_isid_t is)
{
    vqec_dp_display_is_t info;
    vqec_dp_error_t err;
    char tmpbuf[VQEC_DP_STREAM_PRINT_BUF_SIZE];
    int tmpbuf_len = VQEC_DP_STREAM_PRINT_BUF_SIZE;
    uint32_t usedlen = 0;
    
    bzero(&info, sizeof(vqec_dp_display_is_t));

    err = vqec_dp_output_shim_get_is_info(is, &info);
    if (err != VQEC_DP_ERR_OK) {
        return;
    }

    CONSOLE_PRINTF(" Post-repair stream id:  %d\n", is);
    CONSOLE_PRINTF("  capabilities:          %s\n",
                   vqec_dp_stream_capa2str(info.capa, tmpbuf, tmpbuf_len));
    CONSOLE_PRINTF("  encapsulation:         %s\n",
                   vqec_dp_stream_encap2str(info.encap, tmpbuf, tmpbuf_len));
    if (vqec_dp_output_shim_mappedtidstr(is, tmpbuf, 
                                         sizeof(tmpbuf), &usedlen) !=
        VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, 
                     "DP output shim mappedtidstr failed");
        return;
    }
    CONSOLE_PRINTF("  mapped TunIDs:         %s\n", tmpbuf);
    CONSOLE_PRINTF("  connected os:          %d\n", info.os_id);
    CONSOLE_PRINTF("  packets:               %llu\n", info.packets);
    CONSOLE_PRINTF("  bytes:                 %llu\n", info.bytes);
    CONSOLE_PRINTF("  drops:                 %llu\n", info.drops);

}

/**
 * Print a description of all IS's created on the output shim.
 */
void vqec_cli_output_shim_show_streams (void)
{
    vqec_dp_isid_t is, next_is;

    CONSOLE_PRINTF("Output shim input streams:\n");
    if ((vqec_dp_output_shim_get_first_is(&next_is) == VQEC_DP_ERR_OK) &&
        (next_is != VQEC_DP_INVALID_ISID)) {

        do {
            vqec_cli_output_shim_show_stream(next_is);
            is = next_is;
        } while (
            (vqec_dp_output_shim_get_next_is(is, &next_is) == VQEC_DP_ERR_OK) &&
            (next_is != VQEC_DP_INVALID_ISID));
    }
}

/**
 * Print a description of all IS's created on the output shim for a given 
 * tuner.
 *
 * @param[in] id   Tuner ID whose output shim ISes are to be displayed
 */
void vqec_cli_output_shim_show_stream_tuner (vqec_tunerid_t id)
{
    vqec_chanid_t chanid;
    vqec_dp_streamid_t postrepair_id;

    if (vqec_tuner_get_chan(id, &chanid) != VQEC_OK) {
        return;
    }
    if (vqec_chan_get_outputshim_stream(chanid, &postrepair_id) != VQEC_OK) {
        return;
    }
    CONSOLE_PRINTF("Output Shim information for channel ID %0x08:\n", chanid);
    vqec_cli_output_shim_show_stream(postrepair_id);
}

/**
 * Print a description of one tuner created on the output shim.
 */
void vqec_cli_output_shim_show_tuner (vqec_dp_tunerid_t tid)
{
    vqec_dp_output_shim_tuner_status_t ts;

    bzero(&ts, sizeof(vqec_dp_output_shim_tuner_status_t));
    if (tid == VQEC_TUNERID_INVALID) {
        return;
    }

    /* 
     * show tuner will show the non-cumulative version to keep this 
     * consistent with previous versions of show tuner 
     */
    if (vqec_dp_output_shim_get_tuner_status(tid, &ts, FALSE) !=
        VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, 
                     "DP output shim get tuner status failed");
        return;
    }
    CONSOLE_PRINTF("Tuner status (tunerid = %d):\n", ts.tid);
    CONSOLE_PRINTF(" cp_tid:         %d\n", ts.cp_tid);
    CONSOLE_PRINTF(" qid:            %d\n", ts.qid);
    CONSOLE_PRINTF(" isid:           %d\n", ts.is);
    CONSOLE_PRINTF(" qinputs:        %d\n", ts.qinputs);
    CONSOLE_PRINTF(" qdrops:         %d\n", ts.qdrops);
    CONSOLE_PRINTF(" qdepth:         %d\n", ts.qdepth);
    CONSOLE_PRINTF(" qoutputs:       %d\n", ts.qoutputs);
}

/**
 * Print a description of all tuners created on the output shim.
 */
void vqec_cli_output_shim_show_tuners (void)
{
    vqec_dp_tunerid_t tid, next_tid;

    CONSOLE_PRINTF("Output shim tuners:\n");

    if ((vqec_dp_output_shim_get_first_dp_tuner(&next_tid) == VQEC_DP_ERR_OK) &&
        (next_tid != VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID)) {
        
        do {
            vqec_cli_output_shim_show_tuner(next_tid);
            tid = next_tid;
        } while (
            (vqec_dp_output_shim_get_next_dp_tuner(tid, &next_tid) == VQEC_DP_ERR_OK) &&
            (next_tid != VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID));
    }
}

/**
 * Display a packet pool's status to the CLI.
 */
static void 
vqec_cli_pak_pool_status_print (vqec_pak_pool_status_t *status)
{
    CONSOLE_PRINTF("global input pak pool stats:\n");
    CONSOLE_PRINTF(" max entries:               %d\n", status->max);
    CONSOLE_PRINTF(" used entries:              %d\n", status->used);
    CONSOLE_PRINTF(" high water entries:        %d\n", status->hiwat);
    CONSOLE_PRINTF(" fail pak alloc drops:      %d\n", status->alloc_fail);
}


/**
 * Print status information for the dataplane pak pool.
 * It is assumed that the global lock is already held.
 */
void vqec_cli_pak_pool_dump (void)
{
    vqec_pak_pool_status_t status;
    vqec_dp_error_t ret;

    ret = vqec_dp_get_default_pakpool_status(&status);
    if (ret != VQEC_DP_ERR_OK) {
        goto done;
    }

    vqec_cli_pak_pool_status_print(&status);
done:
    return;
}

/**
 * Print status information for the dataplane pak pool.
 * It is assumed that the global lock is not held, and is acquired.
 */
void
vqec_cli_pak_pool_dump_safe (void)
{
    vqec_pak_pool_status_t status;
    vqec_dp_error_t ret;

    vqec_lock_lock(vqec_g_lock);
    ret = vqec_dp_get_default_pakpool_status(&status);
    vqec_lock_unlock(vqec_g_lock);

    if (ret != VQEC_DP_ERR_OK) {
        goto done;
    }

    vqec_cli_pak_pool_status_print(&status);
done:
    return;    
}

/**
 * Display global dataplane IPC statistics.
 */
enum
{
    DP_GLOBAL_COUNTERS_IPC = (1 << 0),
    DP_GLOBAL_COUNTERS_RTP = (1 << 1),
    DP_GLOBAL_COUNTERS_CHAN = (1 << 2),
    DP_GLOBAL_COUNTERS_ALL = (DP_GLOBAL_COUNTERS_CHAN << 1) - 1
};
static void
vqec_cli_dp_global_counters_print (vqec_dp_global_debug_stats_t *stats, 
                                   uint32_t mask)
{
    if (mask & DP_GLOBAL_COUNTERS_IPC) {
        CONSOLE_PRINTF("---Dataplane IPC---\n");
        CONSOLE_PRINTF(" IRQ sent                   %u\n", stats->irqs_sent);
        CONSOLE_PRINTF(" IRQ dropped                %u\n", stats->irqs_dropped);
        CONSOLE_PRINTF(" Ejected packets sent       %u\n", stats->ejects_sent);
        CONSOLE_PRINTF(" Ejected packets dropped    %u\n", stats->ejects_dropped);
    }
    if (mask & DP_GLOBAL_COUNTERS_CHAN) {
        CONSOLE_PRINTF("---Dataplane Channel---\n");
        CONSOLE_PRINTF(" Creates                    %u\n", stats->channel_create);
        CONSOLE_PRINTF(" Destroys                   %u\n", stats->channel_destroy);
        CONSOLE_PRINTF(" Creation failures          %u\n", stats->channel_create_failed);
    }
    if (mask & DP_GLOBAL_COUNTERS_RTP) {
        CONSOLE_PRINTF("---Dataplane RTP---\n");
        CONSOLE_PRINTF(" Source creates             %u\n", 
                       stats->rtp_src_creates);
        CONSOLE_PRINTF(" Source destroys            %u\n", 
                       stats->rtp_src_destroys);
        CONSOLE_PRINTF(" Source table full          %u\n", 
                       stats->rtp_src_table_full);
        CONSOLE_PRINTF(" Source limit exceeded      %u\n", 
                       stats->rtp_src_limit_exceeded);
        CONSOLE_PRINTF(" Source aged out            %u\n", 
                       stats->rtp_src_aged_out);
        CONSOLE_PRINTF(" RTP IS creates             %u\n", 
                       stats->input_stream_creates);
        CONSOLE_PRINTF(" RTP IS deletes             %u\n", 
                       stats->input_stream_deletes);
        CONSOLE_PRINTF(" RTP IS limit exceeded      %u\n", 
                       stats->input_stream_limit_exceeded);
        CONSOLE_PRINTF(" RTP IS ejected paks        %u\n", 
                       stats->input_stream_eject_paks);
        CONSOLE_PRINTF(" XR stats malloc failures   %u\n", 
                       stats->xr_stats_malloc_fails);
        CONSOLE_PRINTF(" SSRC filter drops          %u\n", 
                       stats->ssrc_filter_drops);
        CONSOLE_PRINTF(" Repair stream filter drops %u\n", 
                       stats->repair_filter_drops);
    }
}

/**
 * Display IPC counters and status.
 * Acquires and releases the global lock.
 */
void
vqec_cli_show_ipc_status_safe (void)
{
    vqec_dp_global_debug_stats_t stats;

    vqec_lock_lock(vqec_g_lock);

    if (vqec_dp_get_global_counters(&stats) == VQEC_DP_ERR_OK) {
        vqec_cli_dp_global_counters_print(&stats, DP_GLOBAL_COUNTERS_IPC);
    }
    vqec_upcall_display_state();
    vqec_lock_unlock(vqec_g_lock);
}

/**
 * Display dataplane global debug counters.
 * Acquires and releases the global lock.
 */
void
vqec_cli_show_dp_global_counters_safe (void)
{
    vqec_dp_global_debug_stats_t stats;

    vqec_lock_lock(vqec_g_lock);

    if (vqec_dp_get_global_counters(&stats) == VQEC_DP_ERR_OK) {
        vqec_cli_dp_global_counters_print(&stats, DP_GLOBAL_COUNTERS_ALL);
    }
    vqec_lock_unlock(vqec_g_lock);
}

void
vqec_cli_benchmark_show (void)
{
    uint32_t cpu_usage;
    
    CONSOLE_PRINTF("Benchmark:        VQE-C CPU Usage\n");

    vqec_dp_get_benchmark(5, &cpu_usage);
    cpu_usage += vqec_event_get_benchmark(5);
    CONSOLE_PRINTF(" 5s average:      %u.%u%%\n", cpu_usage/10, cpu_usage%10); 

    vqec_dp_get_benchmark(60, &cpu_usage);
    cpu_usage += vqec_event_get_benchmark(60);
    CONSOLE_PRINTF(" 60s average:     %u.%u%%\n", cpu_usage/10, cpu_usage%10);

    vqec_dp_get_benchmark(300, &cpu_usage);
    cpu_usage += vqec_event_get_benchmark(300);
    CONSOLE_PRINTF(" 300s average:    %u.%u%%", cpu_usage/10, cpu_usage%10);
}

/**---------------------------------------------------------------------------
 * Print a single log sequence to the CLI.
 *---------------------------------------------------------------------------*/ 
#define VQEC_CLI_LOGSEQ_STRLEN 512
#define VQEC_CLI_LOGSEQ_ULONG_STRLEN 16
#define VQEC_CLI_LOGGAP_STRLEN 512
#define VQEC_CLI_LOGGAP_ENTRY_STRLEN 64

static void
vqec_cli_log_seq_print (vqec_seq_num_t *seq, uint32_t cnt)
{
    uint32_t i;  
    char temp[VQEC_CLI_LOGSEQ_ULONG_STRLEN];
    char log_string[VQEC_CLI_LOGSEQ_STRLEN];

    memset(log_string, 0, sizeof(log_string));
    memset(temp, 0, sizeof(temp));
    for (i = 0; i < cnt; i++) {        
        (void)snprintf(temp, 
                       VQEC_CLI_LOGSEQ_ULONG_STRLEN,
                       " %u ", 
                       seq[i]);
        (void)strlcat(log_string, temp, VQEC_CLI_LOGSEQ_STRLEN);
    }
    
    CONSOLE_PRINTF("%s\n", log_string);
}

/**---------------------------------------------------------------------------
 * Display log sequence data from dataplane to the CLI.
 *
 * @param[in] log_seq The sequence log to dump.
 *---------------------------------------------------------------------------*/ 
void 
vqec_cli_log_seq_dump (vqec_dp_seqlog_t *log_seq) 
{
    CONSOLE_PRINTF("\nlast %d repair seq:\n", 
                   VQEC_DP_SEQLOG_ARRAY_SIZE);    
    vqec_cli_log_seq_print(log_seq->repair, VQEC_DP_SEQLOG_ARRAY_SIZE);

    CONSOLE_PRINTF("first %d primary seq:\n",
                   VQEC_DP_SEQLOG_ARRAY_SIZE);
    vqec_cli_log_seq_print(log_seq->prim, VQEC_DP_SEQLOG_ARRAY_SIZE);

    CONSOLE_PRINTF("last %d fec repair seq:\n", 
                   VQEC_DP_SEQLOG_ARRAY_SIZE);
    vqec_cli_log_seq_print(log_seq->fec, VQEC_DP_SEQLOG_ARRAY_SIZE);
}


/**---------------------------------------------------------------------------
 * Display gap lop data from dataplane to the CLI.
 *
 * @param[in] gaplog The gap log to dump.
 *---------------------------------------------------------------------------*/ 
void 
vqec_cli_log_gap_dump (vqec_dp_gaplog_t *gaplog, 
                       uint32_t options_mask,
                       const char *output_name)
{
    uint32_t i;  
    char temp[VQEC_CLI_LOGGAP_ENTRY_STRLEN];
    char log_string[VQEC_CLI_LOGGAP_STRLEN];

    memset(log_string, 0, sizeof(log_string));
    memset(temp, 0, sizeof(temp));
    for (i = 0; i < gaplog->num_entries; i++) {
        (void)snprintf(temp, 
                       VQEC_CLI_LOGGAP_ENTRY_STRLEN, 
                       " %u - %u time %llu, ", 
                       gaplog->gaps[i].start_seq,
                       gaplog->gaps[i].end_seq,
                       TIME_GET_A(msec, gaplog->gaps[i].time));
        (void)strlcat(log_string, temp, VQEC_CLI_LOGGAP_STRLEN);
    }
    CONSOLE_PRINTF("%s%s\n", output_name, log_string); 
}


/**---------------------------------------------------------------------------
 * Gather log data from dataplane, and display it on the console.
 * 
 * @param[in] chanid Identifier of the channel for which the log is collected.
 * @param[in] options_mask Options mask.
 *---------------------------------------------------------------------------*/ 
void
vqec_cli_log_dump (vqec_dp_chanid_t chanid, uint32_t options_mask) 
{
    vqec_dp_gaplog_t in_log, out_log;
    vqec_dp_seqlog_t seq_log;
    vqec_dp_error_t err;

    bzero(&in_log, sizeof(in_log));
    bzero(&out_log, sizeof(out_log));
    bzero(&seq_log, sizeof(seq_log));

    err = vqec_dp_chan_get_seqlogs(chanid, 
                                   &in_log,
                                   &out_log,
                                   &seq_log);
    if (err != VQEC_DP_ERR_OK) {
        return;
    }

    vqec_cli_log_seq_dump(&seq_log);
    vqec_cli_log_gap_dump(&in_log, options_mask, "Input loss (holes):\n");
    vqec_cli_log_gap_dump(&out_log, options_mask, "Output loss (holes):\n");
    CONSOLE_PRINTF("\n");
}


/**---------------------------------------------------------------------------
 * Dump channel statistics from dataplane
 * 
 * @param[in] chanid Identifier of the channel for which the log is collected.
 *---------------------------------------------------------------------------*/ 
void
vqec_cli_dpchan_stats_dump (vqec_dp_chanid_t chanid)
{
    vqec_dp_chan_stats_t stats;
    vqec_dp_chan_failover_status_t failover_status;
    vqec_dp_error_t err;
    char str[INET_ADDRSTRLEN];

    bzero(&stats, sizeof(stats));

    /*
     * For backward compatibility reasons, we will display stats relative to 
     * snapshot
     */
    err = vqec_dp_chan_get_status(chanid, 
                                  NULL,
                                  NULL, 
                                  NULL,
                                  &stats,
                                  &failover_status,
                                  FALSE);
    if (err != VQEC_DP_ERR_OK) {
        return;
    }

    CONSOLE_PRINTF("\n--- Dataplane channel stats ---\n");
    CONSOLE_PRINTF(" total recvd paks:          %llu\n", 
                   stats.rx_paks);
    CONSOLE_PRINTF(" total recvd primary paks:  %llu\n", 
                   stats.primary_rx_paks);
    CONSOLE_PRINTF(" total recvd repair paks:   %llu\n", 
                   stats.repair_rx_paks);
    CONSOLE_PRINTF(" total recvd rtp paks:      %llu\n", 
                   stats.rtp_rx_paks);
    CONSOLE_PRINTF(" total rtp drops:           %llu\n",
                   stats.primary_rtp_rx_drops + stats.repair_rtp_rx_drops);
    CONSOLE_PRINTF(" total sim drops:           %llu\n",
                   stats.primary_sim_drops + stats.repair_sim_drops);
    CONSOLE_PRINTF(" total early drops:         %llu\n",
                   stats.primary_sm_early_drops + stats.repair_sm_early_drops);
    if (failover_status.failover_paks_queued) {
        CONSOLE_PRINTF(
                   " failover source:           ssrc=%8x, src ip:%s port:%d\n",
                   failover_status.failover_src.ssrc,
                   inaddr_ntoa_r(failover_status.failover_src.ipv4.src_addr,
                               str, sizeof(str)),
                   ntohs(failover_status.failover_src.ipv4.src_port));
        CONSOLE_PRINTF(
                   " num paks queued            %u\n",
                   failover_status.failover_paks_queued);
        CONSOLE_PRINTF(
                   " queue head RTP seq num:    %u\n",
                   ntohs(failover_status.failover_queue_head_seqnum));
        CONSOLE_PRINTF(
                   " queue tail RTP seq num:    %u\n",
                   ntohs(failover_status.failover_queue_tail_seqnum));
    }
    if (!IS_ABS_TIME_ZERO(failover_status.prev_src_last_rcv_ts)) {
        CONSOLE_PRINTF(
                   " prev src last rcv time:    %llu\n"
                   " curr src first rcv time:   %llu\n",
                   TIME_GET_A(msec, failover_status.prev_src_last_rcv_ts),
                   TIME_GET_A(msec, failover_status.curr_src_first_rcv_ts));
    }
}
