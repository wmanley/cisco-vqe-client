/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_channel_cli.c
 *
 * Description: VQEC channel module CLI implementation.
 *
 * Documents:
 *
 *****************************************************************************/

#include "vqe_port_macros.h"
#include "vqec_channel_api.h"
#include "vqec_channel_private.h"
#include "vqec_cli_interface.h"

#define RTCP_RLE_UNSPECIFIED 0xffff

/**
 * Display the channel change stats of STB. 
 * These stats are displayed if the CC instrument
 * is available at STB. 
 * 
 * @param[in] chan   channel
 */

static void vqec_chan_display_cc_stb_stats(vqec_chan_t *chan)
{
    rel_time_t fstdec, pat_time, pmt_time, vqec_bind, cc_time;

    if (!chan) {
        return;
    }

    if (chan->dcr_ops.get_dcr_stats &&
        TIME_CMP_A(eq, chan->display_time, ABS_TIME_0)) {
        /* get the stats first, if display_time is > 0, 
         * polling event has already saved the stats into chan */
        vqec_chan_update_dcr_stats(chan);
    }

    if (!TIME_CMP_A(eq, chan->ir_time, ABS_TIME_0)) {

        fstdec = TIME_SUB_A_A(chan->fst_decode_time,
                              chan->ir_time);
        pat_time = TIME_SUB_A_A(chan->pat_time,
                                chan->ir_time);
        pmt_time = TIME_SUB_A_A(chan->pmt_time,
                                chan->ir_time);
        vqec_bind = TIME_SUB_A_A(chan->bind_time,
                                 chan->ir_time);
        cc_time = TIME_SUB_A_A(chan->display_time,
                               chan->ir_time);
        if (TIME_CMP_R(ge, REL_TIME_0, fstdec)) {
            fstdec = REL_TIME_0;
        }
        if (TIME_CMP_R(ge, REL_TIME_0, pat_time)) {
            pat_time = REL_TIME_0;
        }
        if (TIME_CMP_R(ge, REL_TIME_0, pmt_time)) {
            pmt_time = REL_TIME_0;
        }
        if (TIME_CMP_R(ge, REL_TIME_0, vqec_bind)) {
            vqec_bind = REL_TIME_0;
        }
        if (TIME_CMP_R(ge, REL_TIME_0, cc_time)) {
            cc_time = REL_TIME_0;
        }
        CONSOLE_PRINTF("STB channel change information (ms):\n");
        CONSOLE_PRINTF(" remote control CC:         %llu\n",
                       TIME_GET_A(msec, chan->ir_time));
        CONSOLE_PRINTF(" VQE-C tuner bind:          %llu\n",
                       TIME_GET_R(msec, vqec_bind));
        CONSOLE_PRINTF(" PAT found time:            %llu\n",
                       TIME_GET_R(msec, pat_time));
        CONSOLE_PRINTF(" PMT found time:            %llu\n",
                       TIME_GET_R(msec, pmt_time));
        CONSOLE_PRINTF(" first packet decode time:  %llu\n",
                       TIME_GET_R(msec, fstdec));
        CONSOLE_PRINTF(" total channel change time: %llu\n",
                       TIME_GET_R(msec, cc_time));
        CONSOLE_PRINTF("Other STB CC information:\n");
        CONSOLE_PRINTF(" decoded picture snapshot:  %u\n",
                       chan->dec_picture_cnt);
        CONSOLE_PRINTF(" last decoded PTS:          %llu\n",
                       chan->last_decoded_pts);
        CONSOLE_PRINTF(" first display PTS:         %llu\n",
                       chan->display_pts);
    }
}

/* Function:    vqec_channel_cfg_printf()
 * Description: Print out the channel configuration data
 * Parameters:  channel cfg to print
 * Returns:     void
 */
void vqec_channel_cfg_printf (channel_cfg_t *channel_p)
{
    char tmp[INET_ADDRSTRLEN];
    channel_cfg_sdp_handle_t sdp_handle;
    if (channel_p) {
        CONSOLE_PRINTF("Channel name: %s\n", channel_p->name);
        sdp_handle =
            cfg_alloc_SDP_handle_from_orig_src_addr(
                channel_p->source_proto,
                channel_p->original_source_addr,
                channel_p->original_source_port);
        if (sdp_handle) {
            CONSOLE_PRINTF("Channel sdp_handle: %s\n", sdp_handle);
            cfg_free_SDP_handle(sdp_handle);
        }
        CONSOLE_PRINTF("Channel handle: 0x%lx\n", channel_p->handle);
        CONSOLE_PRINTF("Channel session identifier: %s\n",
                       channel_p->session_key);
        CONSOLE_PRINTF("Channel version: %llu\n",
                       channel_p->version);
        CONSOLE_PRINTF("Configuration data: %s\n",
                       (channel_p->complete) ?
                       "complete" : "incomplete");

        if (channel_p->mode == SOURCE_MODE) {
            CONSOLE_PRINTF("Channel mode: source\n");
        }
        else if (channel_p->mode == LOOKASIDE_MODE) {
            CONSOLE_PRINTF("Channel mode: lookaside\n");
        }
        else if (channel_p->mode == RECV_ONLY_MODE) {
            CONSOLE_PRINTF("Channel mode: recvonly\n");
        }
        else {
            CONSOLE_PRINTF("Channel mode: unknown\n");
        }

        CONSOLE_PRINTF("Original source multicast address: %s\n",
                       inet_ntop(AF_INET, &channel_p->original_source_addr,
                                 tmp, INET_ADDRSTRLEN));
        CONSOLE_PRINTF("Source address for original source stream: %s\n",
                       inet_ntop(AF_INET,
                                 &channel_p->src_addr_for_original_source,
                                 tmp, INET_ADDRSTRLEN));
        CONSOLE_PRINTF("Original source port: %d\n",
                       ntohs(channel_p->original_source_port));

        if (channel_p->source_proto == RTP_STREAM) {
            CONSOLE_PRINTF("Original source RTCP port: %d\n",
                           ntohs(channel_p->original_source_rtcp_port));
            CONSOLE_PRINTF("Original source RTP payload type: %d\n",
                           channel_p->original_source_payload_type);
            CONSOLE_PRINTF("Original source RTCP sender bandwidth: %d\n",
                           channel_p->original_rtcp_sndr_bw);
            CONSOLE_PRINTF("Original source RTCP receiver bandwidth: %d\n",
                           channel_p->original_rtcp_rcvr_bw);
            CONSOLE_PRINTF("Original source RTCP per receiver bandwidth: %d\n",
                           channel_p->original_rtcp_per_rcvr_bw);

            CONSOLE_PRINTF("Original source RTCP XR Loss RLE Report: %s\n",
                           (channel_p->original_rtcp_xr_loss_rle !=
                            RTCP_RLE_UNSPECIFIED) ?
                           "On" : "Off");

            CONSOLE_PRINTF("Original source RTCP XR Stat Summary Report: %s\n",
                           (channel_p->original_rtcp_xr_stat_flags != 0) ?
                           "On" : "Off");

            CONSOLE_PRINTF("RTCP XR Post Repair Loss RLE Report: %s\n",
                           (channel_p->original_rtcp_xr_per_loss_rle !=
                            RTCP_RLE_UNSPECIFIED) ?
                           "On" : "Off");

            CONSOLE_PRINTF("RTCP Reduced Size RTCP Support: %s\n",
                           (channel_p->original_rtcp_rsize != 0) ?
                           "On" : "Off");

            CONSOLE_PRINTF("RTCP XR Multicast Acquisition Report: %s\n",
                           (channel_p->original_rtcp_xr_multicast_acq != 0) ?
                           "On" : "Off");

            CONSOLE_PRINTF("RTCP XR Diagnostic Counters Report: %s\n",
                           (channel_p->original_rtcp_xr_diagnostic_counters 
                            != 0) ?
                           "On" : "Off");
        }

        CONSOLE_PRINTF("Maximum bit rate: %d\n",
                       channel_p->bit_rate);

        if (channel_p->mode == SOURCE_MODE) {
            CONSOLE_PRINTF("Re-sourced stream multicast address: %s\n",
                           inet_ntop(AF_INET, &channel_p->re_sourced_addr,
                                     tmp, INET_ADDRSTRLEN));
            CONSOLE_PRINTF("Re-sourced stream RTP port: %d\n",
                           ntohs(channel_p->re_sourced_rtp_port));
            CONSOLE_PRINTF("Re-sourced stream RTCP port: %d\n",
                           ntohs(channel_p->re_sourced_rtcp_port));
            CONSOLE_PRINTF("Re-sourced stream RTP payload type: %d\n",
                           channel_p->re_sourced_payload_type);
        }

       if (channel_p->er_enable) {
            CONSOLE_PRINTF("Retransmission/FBT address: %s\n",
                       inet_ntop(AF_INET, &channel_p->fbt_address,
                                 tmp, INET_ADDRSTRLEN));
            CONSOLE_PRINTF("Retransmission RTP port: %d\n",
                           ntohs(channel_p->rtx_rtp_port));
            CONSOLE_PRINTF("Retransmission RTCP port: %d\n",
                           ntohs(channel_p->rtx_rtcp_port));
            CONSOLE_PRINTF("Retransmission associated payload type: %d\n",
                           channel_p->rtx_apt);

            CONSOLE_PRINTF("Repair stream RTCP sender bandwidth: %d\n",
                           channel_p->repair_rtcp_sndr_bw);
            CONSOLE_PRINTF("Repair stream RTCP receiver bandwidth: %d\n",
                           channel_p->repair_rtcp_rcvr_bw);

            CONSOLE_PRINTF("Repair stream RTCP XR Loss RLE Report: %s\n",
                           (channel_p->repair_rtcp_xr_loss_rle !=
                            RTCP_RLE_UNSPECIFIED) ?
                           "On" : "Off");
            CONSOLE_PRINTF("Repair stream RTCP XR Stat Summary Report: %s\n",
                           (channel_p->repair_rtcp_xr_stat_flags != 0) ?
                           "On" : "Off");
        }

        CONSOLE_PRINTF("Error repair: %s\n",
                       (channel_p->er_enable) ?
                       "enabled" : "disabled");
#if HAVE_FCC
        CONSOLE_PRINTF("Rapid channel change: %s\n",
                       (channel_p->fcc_enable) ?
                       "enabled" : "disabled");
        if (channel_p->fcc_enable && !channel_p->er_enable) {
            CONSOLE_PRINTF("Retransmission/FBT address: %s\n",
                       inet_ntop(AF_INET, &channel_p->fbt_address,
                                 tmp, INET_ADDRSTRLEN));
        } 
#endif /* HAVE_FCC */

        if (channel_p->fec_enable) {
            CONSOLE_PRINTF("FEC stream1 address: %s\n",
                           inet_ntop(AF_INET,
                                     &channel_p->fec_stream1.multicast_addr,
                                     tmp, INET_ADDRSTRLEN));
            CONSOLE_PRINTF("Source address for FEC stream1: %s\n",
                           inet_ntop(AF_INET, &channel_p->fec_stream1.src_addr,
                                     tmp, INET_ADDRSTRLEN));
            CONSOLE_PRINTF("FEC stream1 RTP port: %d\n",
                           ntohs(channel_p->fec_stream1.rtp_port));
            CONSOLE_PRINTF("FEC stream1 RTP payload type: %d\n",
                           channel_p->fec_stream1.payload_type);
            CONSOLE_PRINTF("FEC stream1 RTCP sender bandwidth: %d\n",
                           channel_p->fec_stream1.rtcp_sndr_bw);
            CONSOLE_PRINTF("FEC stream1 RTCP receiver bandwidth: %d\n",
                           channel_p->fec_stream1.rtcp_rcvr_bw);

            if (channel_p->fec_mode == FEC_2D_MODE) {
                CONSOLE_PRINTF("FEC stream2 address: %s\n",
                               inet_ntop(AF_INET,
                                        &channel_p->fec_stream2.multicast_addr,
                                         tmp, INET_ADDRSTRLEN));
                CONSOLE_PRINTF("Source address for FEC stream2: %s\n",
                               inet_ntop(AF_INET,
                                         &channel_p->fec_stream2.src_addr,
                                         tmp, INET_ADDRSTRLEN));
                CONSOLE_PRINTF("FEC stream2 RTP port: %d\n",
                               ntohs(channel_p->fec_stream2.rtp_port));
                CONSOLE_PRINTF("FEC stream2 RTP payload type: %d\n",
                               channel_p->fec_stream2.payload_type);
                CONSOLE_PRINTF("FEC stream2 RTCP sender bandwidth: %d\n",
                               channel_p->fec_stream2.rtcp_sndr_bw);
                CONSOLE_PRINTF("FEC stream2 RTCP receiver bandwidth: %d\n",
                               channel_p->fec_stream2.rtcp_rcvr_bw);
            }
        }
    }
}


/* Function:    vqec_channel_cfg_printf_brief()
 * Description: Print out the brief output for the channel configuration data
 * Parameters:  N/A
 * Returns:     void
 */
void vqec_channel_cfg_printf_brief (channel_cfg_t *channel_p)
{
      char tmp[INET_ADDRSTRLEN];

      if (channel_p) {
          CONSOLE_PRINTF("Channel name: %s\n", channel_p->name);
          CONSOLE_PRINTF("Source multicast address: %s\n",
                         inet_ntop(AF_INET, &channel_p->original_source_addr,
                                   tmp, INET_ADDRSTRLEN));
          CONSOLE_PRINTF("Source port: %d\n",
                         ntohs(channel_p->original_source_port));

          if (channel_p->er_enable) {
              CONSOLE_PRINTF("Retransmission/FBT address: %s\n",
                             inet_ntop(AF_INET, &channel_p->fbt_address, tmp, 
                                       INET_ADDRSTRLEN));
          }

#if HAVE_FCC
          if (channel_p->fcc_enable && !channel_p->er_enable) {
              CONSOLE_PRINTF("Retransmission/FBT address: %s\n",
                             inet_ntop(AF_INET, &channel_p->fbt_address, tmp,
                                       INET_ADDRSTRLEN));
          }
#endif /* HAVE_FCC */

          CONSOLE_PRINTF("Maximum bit rate: %d\n",
                         channel_p->bit_rate);
      }
}


/* Function:    vqec_chan_cfg_printf()
 * Description: Print out the channel configuration data
 * Parameters:  channel cfg to print
 * Returns:     void
 */
void vqec_chan_cfg_printf (vqec_chan_cfg_t *channel_p)
{
    char tmp[INET_ADDRSTRLEN];
    char *prefix;
    channel_cfg_sdp_handle_t sdp_handle;
    if (channel_p) {
        CONSOLE_PRINTF("Channel name: %s\n", channel_p->name);
        /* 'Original source' retained for backward compatibility */
        if (IN_MULTICAST(ntohl(channel_p->primary_dest_addr.s_addr))) {
            sdp_handle =
                cfg_alloc_SDP_handle_from_orig_src_addr(
                    RTP_STREAM,
                    channel_p->primary_dest_addr,
                    channel_p->primary_dest_port);
            if (sdp_handle) {
                CONSOLE_PRINTF("Channel sdp_handle: %s\n", sdp_handle);
                cfg_free_SDP_handle(sdp_handle);
            }
            CONSOLE_PRINTF("Original source multicast address: %s\n",
                           inet_ntop(AF_INET, &channel_p->primary_dest_addr,
                                     tmp, INET_ADDRSTRLEN));
            CONSOLE_PRINTF("Source address for original source stream: %s\n",
                           inet_ntop(AF_INET,
                                     &channel_p->primary_source_addr,
                                     tmp, INET_ADDRSTRLEN));
            CONSOLE_PRINTF("Original source port: %d\n",
                           ntohs(channel_p->primary_dest_port));
            CONSOLE_PRINTF("Original source RTCP port: %d\n",
                           ntohs(channel_p->primary_dest_rtcp_port));
            prefix = "Original source";
        } else {
            CONSOLE_PRINTF("Primary destination address: %s\n",
                           inet_ntop(AF_INET, &channel_p->primary_dest_addr,
                                     tmp, INET_ADDRSTRLEN));
            CONSOLE_PRINTF("Primary source address: %s\n",
                           inet_ntop(AF_INET,
                                     &channel_p->primary_source_addr,
                                     tmp, INET_ADDRSTRLEN));
            CONSOLE_PRINTF("Primary destination port: %d\n",
                           ntohs(channel_p->primary_dest_port));
            CONSOLE_PRINTF("Primary destination RTCP port: %d\n",
                           ntohs(channel_p->primary_dest_rtcp_port));
            CONSOLE_PRINTF("Primary source port: %d\n",
                           ntohs(channel_p->primary_source_port));
            CONSOLE_PRINTF("Primary source RTCP port: %d\n",
                           ntohs(channel_p->primary_source_rtcp_port));
            prefix = "Primary";
        }
        CONSOLE_PRINTF("%s RTP payload type: %d\n", prefix,
                       channel_p->primary_payload_type);
        CONSOLE_PRINTF("%s RTCP sender bandwidth: %d\n", prefix,
                       channel_p->primary_rtcp_sndr_bw);
        CONSOLE_PRINTF("%s RTCP receiver bandwidth: %d\n", prefix,
                       channel_p->primary_rtcp_rcvr_bw);
        CONSOLE_PRINTF("%s RTCP per receiver bandwidth: %d\n", prefix,
                       channel_p->primary_rtcp_per_rcvr_bw);

        CONSOLE_PRINTF("%s RTCP XR Loss RLE Report: %s\n", prefix,
                       (channel_p->primary_rtcp_xr_loss_rle !=
                        RTCP_RLE_UNSPECIFIED) ?
                       "On" : "Off");

        CONSOLE_PRINTF("%s RTCP XR Stat Summary Report: %s\n", prefix,
                       (channel_p->primary_rtcp_xr_stat_flags != 0) ?
                       "On" : "Off");

        CONSOLE_PRINTF("RTCP XR Post Repair Loss RLE Report: %s\n",
                       (channel_p->primary_rtcp_xr_per_loss_rle !=
                        RTCP_RLE_UNSPECIFIED) ?
                       "On" : "Off");

        CONSOLE_PRINTF("Maximum bit rate: %d\n",
                       channel_p->primary_bit_rate);

       if (channel_p->er_enable) {
            CONSOLE_PRINTF("Retransmission/FBT address: %s\n",
                       inet_ntop(AF_INET, &channel_p->fbt_addr,
                                 tmp, INET_ADDRSTRLEN));
            CONSOLE_PRINTF("Retransmission RTP port: %d\n",
                           ntohs(channel_p->rtx_source_port));
            CONSOLE_PRINTF("Retransmission RTCP port: %d\n",
                           ntohs(channel_p->rtx_source_rtcp_port));
            CONSOLE_PRINTF("Retransmission associated payload type: %d\n",
                           channel_p->rtx_payload_type);

            CONSOLE_PRINTF("Repair stream RTCP sender bandwidth: %d\n",
                           channel_p->rtx_rtcp_sndr_bw);
            CONSOLE_PRINTF("Repair stream RTCP receiver bandwidth: %d\n",
                           channel_p->rtx_rtcp_rcvr_bw);

            CONSOLE_PRINTF("Repair stream RTCP XR Loss RLE Report: %s\n",
                           (channel_p->rtx_rtcp_xr_loss_rle !=
                            RTCP_RLE_UNSPECIFIED) ?
                           "On" : "Off");
            CONSOLE_PRINTF("Repair stream RTCP XR Stat Summary Report: %s\n",
                           (channel_p->rtx_rtcp_xr_stat_flags != 0) ?
                           "On" : "Off");
        }

        CONSOLE_PRINTF("Error repair: %s\n",
                       (channel_p->er_enable) ?
                       "enabled" : "disabled");
#if HAVE_FCC
        CONSOLE_PRINTF("Rapid channel change: %s\n",
                       (channel_p->rcc_enable) ?
                       "enabled" : "disabled");
        if (channel_p->rcc_enable && !channel_p->er_enable) {
            CONSOLE_PRINTF("Retransmission/FBT address: %s\n",
                       inet_ntop(AF_INET, &channel_p->fbt_addr,
                                 tmp, INET_ADDRSTRLEN));
        } 
#endif /* HAVE_FCC */

        if (channel_p->fec_enable) {
            CONSOLE_PRINTF("FEC stream1 address: %s\n",
                           inet_ntop(AF_INET,
                                     &channel_p->fec1_mcast_addr,
                                     tmp, INET_ADDRSTRLEN));
            CONSOLE_PRINTF("Source address for FEC stream1: %s\n",
                           inet_ntop(AF_INET, &channel_p->fec1_source_addr,
                                     tmp, INET_ADDRSTRLEN));
            CONSOLE_PRINTF("FEC stream1 RTP port: %d\n",
                           ntohs(channel_p->fec1_mcast_port));
            CONSOLE_PRINTF("FEC stream1 RTP payload type: %d\n",
                           channel_p->fec1_payload_type);
            CONSOLE_PRINTF("FEC stream1 RTCP sender bandwidth: %d\n",
                           channel_p->fec1_rtcp_sndr_bw);
            CONSOLE_PRINTF("FEC stream1 RTCP receiver bandwidth: %d\n",
                           channel_p->fec1_rtcp_rcvr_bw);

            if (channel_p->fec_mode == FEC_2D_MODE) {
                CONSOLE_PRINTF("FEC stream2 address: %s\n",
                               inet_ntop(AF_INET,
                                        &channel_p->fec2_mcast_addr,
                                         tmp, INET_ADDRSTRLEN));
                CONSOLE_PRINTF("Source address for FEC stream2: %s\n",
                               inet_ntop(AF_INET,
                                         &channel_p->fec2_source_addr,
                                         tmp, INET_ADDRSTRLEN));
                CONSOLE_PRINTF("FEC stream2 RTP port: %d\n",
                               ntohs(channel_p->fec2_mcast_port));
                CONSOLE_PRINTF("FEC stream2 RTP payload type: %d\n",
                               channel_p->fec2_payload_type);
                CONSOLE_PRINTF("FEC stream2 RTCP sender bandwidth: %d\n",
                               channel_p->fec2_rtcp_sndr_bw);
                CONSOLE_PRINTF("FEC stream2 RTCP receiver bandwidth: %d\n",
                               channel_p->fec2_rtcp_rcvr_bw);
            }
        }
    }
}


/* Function:    vqec_chan_cfg_printf_brief()
 * Description: Print out the brief output for the channel configuration data
 * Parameters:  N/A
 * Returns:     void
 */
void vqec_chan_cfg_printf_brief (vqec_chan_cfg_t *channel_p)
{
      char tmp[INET_ADDRSTRLEN];

      if (channel_p) {
          CONSOLE_PRINTF("Channel name: %s\n", channel_p->name);
          if (IN_MULTICAST(ntohl(channel_p->primary_dest_addr.s_addr))) {
              CONSOLE_PRINTF("Source multicast address: %s\n",
                             inet_ntop(AF_INET, &channel_p->primary_dest_addr,
                                       tmp, INET_ADDRSTRLEN));
              CONSOLE_PRINTF("Source port: %d\n",
                             ntohs(channel_p->primary_dest_port));
          } else {
              CONSOLE_PRINTF("Primary destination address: %s\n",
                             inet_ntop(AF_INET, &channel_p->primary_dest_addr,
                                       tmp, INET_ADDRSTRLEN));
              CONSOLE_PRINTF("Primary destination port: %d\n",
                             ntohs(channel_p->primary_dest_port));
          }

          if (channel_p->er_enable) {
              CONSOLE_PRINTF("Retransmission/FBT address: %s\n",
                             inet_ntop(AF_INET, &channel_p->fbt_addr, tmp, 
                                       INET_ADDRSTRLEN));
          }

#if HAVE_FCC
          if (channel_p->rcc_enable && !channel_p->er_enable) {
              CONSOLE_PRINTF("Retransmission/FBT address: %s\n",
                             inet_ntop(AF_INET, &channel_p->fbt_addr, tmp,
                                       INET_ADDRSTRLEN));
          }
#endif /* HAVE_FCC */

          CONSOLE_PRINTF("Maximum bit rate: %d\n",
                         channel_p->primary_bit_rate);
      }
}

/**
 * Display the state of a channel (equivalent to src_dump sans
 * the display of channel configuration from the configuration
 * manager) to the given file stream.
 *
 * @param[in] chanid Channel whose state is to be written
 * @param[in] options_flag Conditional flags under which to display.
 */
void
vqec_chan_display_state (vqec_chanid_t chanid, uint32_t options_flag)
{
    uint32_t curr_tokens = 0;
    vqec_chan_t *chan;
    uint64_t primary_rtcp_inputs = 0, repair_rtcp_inputs = 0;

    /* Validate state and parameters */
    if (!g_channel_module) {
        goto done;
    }
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        goto done;
    }

    CONSOLE_PRINTF("Channel information for channel 0x%08x\n", chan->chanid);
    /* 'Original source multicast' printed for backwards compatibility */
    if (IN_MULTICAST(ntohl(chan->cfg.primary_dest_addr.s_addr))) {
        CONSOLE_PRINTF(" Original source multicast: %s\n",
                       inet_ntop_static_buf(
                           chan->cfg.primary_dest_addr.s_addr,
                           INET_NTOP_STATIC_BUF0));
        CONSOLE_PRINTF(" Original source port:      %u\n",
                       ntohs(chan->cfg.primary_dest_port));
    } else {
        CONSOLE_PRINTF(" Primary destination addr:  %s\n",
                       inet_ntop_static_buf(
                           chan->cfg.primary_dest_addr.s_addr,
                           INET_NTOP_STATIC_BUF0));
        CONSOLE_PRINTF(" Primary destination port:  %u\n",
                       ntohs(chan->cfg.primary_dest_port));
    }
    CONSOLE_PRINTF(" Dataplane channel ID:      %x\n",
                   chan->dp_chanid);
    CONSOLE_PRINTF(" Dataplane graph ID:        %x\n",
                   chan->graph_id);
    CONSOLE_PRINTF(" Repair RTP ephemeral port: %u\n",
                   ntohs(chan->rtp_rtx_port));
    CONSOLE_PRINTF(" Primary RTP ID:            %x\n",
                   chan->prim_rtp_id);
    CONSOLE_PRINTF(" Repair RTP ID:             %x\n",
                   chan->repair_rtp_id);
    CONSOLE_PRINTF(" FEC0 RTP ID:               %x\n",
                   chan->fec0_rtp_id);
    CONSOLE_PRINTF(" FEC1 RTP ID:               %x\n",
                   chan->fec1_rtp_id);

    if ((options_flag & (DETAIL_MASK|CHANNEL_MASK)) != 0) {
        CONSOLE_PRINTF("Current channel information: \n");
        vqec_chan_cfg_printf(&chan->cfg);
        CONSOLE_PRINTF("Active channel cfg updates: %llu\n",
                       chan->stats.active_cfg_updates -
                       chan->stats_snapshot.active_cfg_updates);
        CONSOLE_PRINTF("Active channel's last cfg update time: %llu",
                       TIME_GET_A(msec, chan->last_cfg_update_ts));
    } else if ((options_flag & (BRIEF_MASK)) != 0) {
        vqec_chan_cfg_printf_brief(&chan->cfg);
    }

    if (options_flag & (DETAIL_MASK | CHANNEL_MASK)) {
        /* display cc stb stats if available */
        vqec_chan_display_cc_stb_stats(chan);
    }

    /* Error repair policing information: only for detail */
    if ((options_flag & DETAIL_MASK) != 0) {
        if (chan->er_enabled) {
            CONSOLE_PRINTF("Error repair policer:            %s\n",
                           chan->er_policer_enabled ? "enabled" : "disabled");
            if (chan->er_policer_enabled) {
                (void)tb_credit_tokens(&chan->er_policer_tb, ABS_TIME_0,
                                       &curr_tokens);
                CONSOLE_PRINTF("  rate (repair pkts/s):    %u\n"
                               "  burst (repair pkts):     %u\n"
                               "  current tokens:          %u\n",
                               chan->er_policer_tb.rate,
                               chan->er_policer_tb.burst,
                               curr_tokens);
            }
        }
    }

    /* RTP session info; only for detail */
    if ((options_flag & DETAIL_MASK) != 0) {
        if (chan->prim_session) {
            rtp_show_session_era_recv(chan->prim_session);
        }
        if (chan->repair_session && chan->er_enabled) {
            rtp_show_session_repair_recv(chan->repair_session);
        }
    }


    if ((options_flag & DETAIL_MASK) != 0) {
        /* dataplane channel statistics: only for detail */
        vqec_cli_dpchan_stats_dump(chan->dp_chanid);
        if (chan->prim_session) {
            rtp_show_drop_stats((rtp_session_t *)chan->prim_session);
            primary_rtcp_inputs =
                ((rtp_session_t *)chan->prim_session)->rtcp_stats.pkts.rcvd;
        }
        if (chan->repair_session && chan->er_enabled) {
            rtp_show_drop_stats((rtp_session_t *)chan->repair_session);
            repair_rtcp_inputs =
                ((rtp_session_t *)chan->repair_session)->rtcp_stats.pkts.rcvd;
       }

        /* display local state. */
        CONSOLE_PRINTF(" total recvd rtcp paks:     %d\n", 
                       primary_rtcp_inputs + repair_rtcp_inputs);
        CONSOLE_PRINTF(" generic nack counter:      %d\n", 
                       chan->stats.generic_nack_counter);
        CONSOLE_PRINTF(" total repairs requested:   %d\n", 
                       chan->stats.total_repairs_requested);
        CONSOLE_PRINTF(" total repairs policed:     %d\n", 
                       chan->stats.total_repairs_policed);
        CONSOLE_PRINTF(" total repairs unrequested: %d\n", 
                       chan->stats.total_repairs_unrequested);
        CONSOLE_PRINTF(" failed to send rtcp pak:   %d\n", 
                       chan->stats.fail_to_send_rtcp_counter);
        CONSOLE_PRINTF(" failed to report gap:      %d\n\n", 
                       chan->stats.suppressed_jumbo_gap_counter);
    }

done:
    return;    
}

/**---------------------------------------------------------------------------
 * Gather RCC data from dataplane, and display it on the console.
 * 
 * @param[in] chanid CP channel id for which the log is collected.
 * @param[in] brief_flag If brief mode is on.
 *---------------------------------------------------------------------------*/ 
#if HAVE_FCC
void
vqec_chan_display_rcc_state (vqec_chanid_t chanid, boolean brief_flag)
{
    vqec_dp_rcc_data_t data;
    vqec_dp_error_t err;
    rel_time_t nakpli, app, fstrep, fstpri, join, eren, fstpak, lstpak, burst;
    rel_time_t socket_app, socket_fstrep, socket_fstpri;
    vqec_chan_t *chan;
    uint64_t app_pcr, app_pts;
    int32_t num_xr_ma_gap_pkts;

    if (!g_channel_module) {
        return;
    }
    chan = vqec_chanid_to_chan(chanid);
    if (!chan) {
        return;
    }

    bzero(&data, sizeof(data));
    err = vqec_dp_chan_get_rcc_status(chan->dp_chanid, &data);
    if (err != VQEC_DP_ERR_OK) {
        return;
    }

    CONSOLE_PRINTF("--- RCC status ---\n");
    CONSOLE_PRINTF(
        " rcc enabled:               %s\n"
        " rcc result:                %s\n"
        " cp failure reason:         %s\n"
        " dp failure reason:         %s\n",
        data.rcc_enabled ? "true" : "false",
        data.rcc_success ? "success" : (data.rcc_in_abort ?
                                        "failure" : "ongoing"),
        vqec_sm_fail_reason(chan),
        data.fail_reason);

    if (brief_flag) {
        return;
    }

    (void)vqec_dp_chan_get_pcr(chan->dp_chanid, &app_pcr);
    (void)vqec_dp_chan_get_pts(chan->dp_chanid, &app_pts);

    CONSOLE_PRINTF("--- Buffer Fill (ms) ---\n");
    CONSOLE_PRINTF(
        " minimum buffer fill:       %llu\n"
        " maximum buffer fill:       %llu\n"
        " buffer fill from APP:      %llu\n"
        " fast fill from APP (PCR):  %u\n"
        " PCR from APP:              %llu\n"
        " PTS from APP:              %llu\n",
        TIME_GET_R(msec, chan->rcc_min_fill),
        TIME_GET_R(msec, chan->rcc_max_fill),
        TIME_GET_R(msec, chan->act_min_backfill),
        (uint32_t)TIME_GET_R(pcr, chan->fast_fill_time),
        app_pcr,
        app_pts);

    nakpli = TIME_SUB_A_A(chan->nakpli_sent_time,
                          chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, nakpli)) {
        nakpli = REL_TIME_0;
    }
    app = TIME_SUB_A_A(chan->app_rx_ev_ts,
                       chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, app)) {
        app = REL_TIME_0;
    }
    fstrep = TIME_SUB_A_A(data.first_repair_ev_ts,
                          chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, fstrep)) {
        fstrep = REL_TIME_0;
    }
    join = TIME_SUB_A_A(data.join_issue_ev_ts,
                        chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, join)) {
        join = REL_TIME_0;
    }
    fstpri = TIME_SUB_A_A(data.first_primary_ev_ts,
                          chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, fstpri)) {
        fstpri = REL_TIME_0;
    }
    eren = TIME_SUB_A_A(data.er_en_ev_ts,
                        chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, eren)) {
        eren = REL_TIME_0;
    }

    burst = TIME_SUB_A_A(data.burst_end_time,
                        chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, burst)) {
        burst = REL_TIME_0;
    }
    
    /* Packet receive times */
    socket_app = TIME_SUB_A_A(chan->app_rx_ts,
                       chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, socket_app)) {
        socket_app = REL_TIME_0;
    }
    socket_fstrep = TIME_SUB_A_A(data.first_repair_ts,
                                 chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, socket_fstrep)) {
        socket_fstrep = REL_TIME_0;
    }
    socket_fstpri = TIME_SUB_A_A(data.first_primary_ts,
                                 chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, socket_fstpri)) {
        socket_fstpri = REL_TIME_0;
    }


    CONSOLE_PRINTF("--- APP expected relative times (ms) ---\n");
    CONSOLE_PRINTF(
        " Join      ER       End-Of-Burst\n"
        " %-5llu     %-5llu    %-5llu\n",
        TIME_GET_R(msec, chan->dt_earliest_join),
        TIME_GET_R(msec, chan->er_holdoff_time),
        TIME_GET_R(msec, chan->dt_repair_end));
    CONSOLE_PRINTF("---Actual relative times (ms)---\n");
    CONSOLE_PRINTF("  First line is VQE-C time, second line is pkt recv "
                   "time\n");
    CONSOLE_PRINTF(
        " CC       Pli      APP      Rep      Join     Prim     ER      "
        "BEND     Join-lat\n"
        " %-5llu    %-5llu    %-5llu    %-5llu    %-5llu    %-5llu    %-5llu"
        "   %-5llu    %-5llu\n"
        " %-5llu    %-5llu    %-5llu    %-5llu    %-5llu    %-5llu    %-5llu"
        "   %-5llu    %-5llu\n",
        /* VQE-C Times */
        REL_TIME_0,
        TIME_GET_R(msec, nakpli),
        TIME_GET_R(msec, app),
        TIME_GET_R(msec, fstrep),
        TIME_GET_R(msec, join),
        TIME_GET_R(msec, fstpri),
        TIME_GET_R(msec, eren),
        TIME_GET_R(msec, burst),
        TIME_GET_R(msec, data.join_latency),
        /* Socket Times (as reported in RTCP XR) */
        REL_TIME_0,
        TIME_GET_R(msec, nakpli),
        TIME_GET_R(msec, socket_app),
        TIME_GET_R(msec, socket_fstrep),
        TIME_GET_R(msec, join),
        TIME_GET_R(msec, socket_fstpri),
        TIME_GET_R(msec, eren),
        TIME_GET_R(msec, burst),
        TIME_GET_R(msec, data.join_latency));
    
    CONSOLE_PRINTF("--- Pcm snapshots ---\n");
    CONSOLE_PRINTF("       Head            Tail            Paks\n");
    CONSOLE_PRINTF(" JOIN  %-12u    %-12u    %-12u\n",
                   data.join_snap.head,
                   data.join_snap.tail,
                   data.join_snap.num_paks);
    CONSOLE_PRINTF(" PRIM  %-12u    %-12u    %-12u\n",
                   data.prim_snap.head,
                   data.prim_snap.tail,
                   data.prim_snap.num_paks);
    CONSOLE_PRINTF(" EREN  %-12u    %-12u    %-12u\n",
                   data.er_en_snap.head,
                   data.er_en_snap.tail,
                   data.er_en_snap.num_paks);

    fstpak = TIME_SUB_A_A(data.outp_data.first_rcc_pak_ts,
                          chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, fstpak)) {
        fstpak = REL_TIME_0;
    }
    lstpak = TIME_SUB_A_A(data.outp_data.last_rcc_pak_ts,
                          chan->bind_time);
    if (TIME_CMP_R(ge, REL_TIME_0, lstpak)) {
        lstpak = REL_TIME_0;
    }

    /*
     * There are 2 different duplicate packet counters; one captured at the
     * time at which the 1st primary packet reaches the head of the pcm; the
     * 2nd which is captured at the time at which the burst ends. Because of
     * duplicates in the stream, the 1st counter may be captured "too-soon",
     * e.g., if the multicast arrived immediately after the join.  The 2nd 
     * counter is displayed.
     */
    num_xr_ma_gap_pkts = data.outp_data.first_prim_to_last_rcc_seq_diff - 1;
    num_xr_ma_gap_pkts = (num_xr_ma_gap_pkts >= 0) ? num_xr_ma_gap_pkts : 0; 
    CONSOLE_PRINTF("--- Output statistics ---\n");
    CONSOLE_PRINTF(
        " first primary sequence:    %u\n"
        " first repair sequence:     %u\n"
        " first repair rtp ts:       %u\n"
        " rcc output loss packets:   %d\n"
        " rcc output loss holes:     %d\n"
        " rcc duplicate packets:     %llu\n"
        " repairs in 1st nack:       %d\n"
        " first packet output time   %llu\n"
        " last packet output time    %llu\n"
        " rtcp xr num gap packets:   %u\n",
        data.first_primary_seq,
        chan->start_seq_num,
        chan->app_rtp_ts,
        data.outp_data.output_loss_paks_in_rcc,
        data.outp_data.output_loss_holes_in_rcc,
        data.end_burst_snap.dup_paks,
        chan->stats.first_nack_repair_cnt,
        TIME_GET_R(msec, fstpak),
        TIME_GET_R(msec, lstpak),
        num_xr_ma_gap_pkts);
    CONSOLE_PRINTF(
        " concurrent rccs limited:   %llu\n",
        chan->stats.concurrent_rccs_limited);

    vqec_cli_log_gap_dump(&data.outp_data.outp_gaps,
                          0, "RCC output loss (holes):\n");

    /* display cc stb stats if available */
    vqec_chan_display_cc_stb_stats(chan);
}
#endif
