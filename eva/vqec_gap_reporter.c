/*
 * Copyright (c) 2006-2010, 2012 by Cisco Systems, Inc.
 * All rights reserved.
 */

/*
 * vqec_gap_reporter.c - defines gap reporting module for vqec.
 *
 * Gaps are added into the gap list, and then stuffed into RTCP
 * NACK packets and reported back to the VQE-S.
 */

#include "vam_util.h"
#include "vqe_token_bucket.h"
#include "vqec_gap_reporter.h"
#include "vqec_debug.h"
#include "vqec_assert_macros.h"
#include <rtp/rtcp_stats_api.h>
#include "vqec_channel_private.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#include "test_vqec_utest_interposers.h"
#else
#define UT_STATIC static
#endif

#define MAX_SENDERS_SSM 1   /* for SSM */

/*
 * Controls for error repair functionality
 */
static boolean g_vqec_error_repair;
static boolean g_vqec_error_repair_set = FALSE;

static boolean s_vqec_error_repair_policer_enabled =
    VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_ENABLE;
static boolean s_vqec_error_repair_policer_enabled_set = FALSE;

static uint8_t s_vqec_error_repair_policer_rate =
    VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_RATE;
static uint8_t s_vqec_error_repair_policer_rate_set = FALSE;

static uint16_t s_vqec_error_repair_policer_burst =
    VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_BURST;
static uint16_t s_vqec_error_repair_policer_burst_set = FALSE;

uint64_t g_vqec_error_repair_policed_requests = 0;

/**
 * Retrieves the global settting of RCC, which is based on system
 * configuration but may be overriden by the demo CLI knob (if set).
 *
 * param[out] demo_set        - TRUE:  the demo CLI knob is set
 *                              FALSE: the demo CLI knob is unset
 * param[out] boolean         - TRUE:  RCC is globally enabled
 *                              FALSE: RCC is globally disabled
 */
boolean 
vqec_get_error_repair (boolean *demo_set)
{
    boolean value;

    if (demo_set) {
        *demo_set = g_vqec_error_repair_set;
    }
    if (g_vqec_error_repair_set) {
        value = g_vqec_error_repair;
    } else {
        value = vqec_syscfg_get_ptr()->error_repair_enable;
    }
    return (value);
}

/**
 * Enables/disables error repair globally (for all channels), for the
 * purposes of demonstrating the effects of ER on picture quality.
 *
 * Called upon "error-repair {enable|disable}" CLI commands being entered.
 *
 * NOTE:  Disabling error-repair for demo purposes causes error repair
 *        requests to be surpressed, and "disabled" appears in the 
 *        "show error-repair" display.  However, some channel state and 
 *        debugs visible on the CLI may still give the appearance that
 *        error repair is enabled.
 *
 * @param[in] enabled - TRUE to enable (ER will be performed for a channel if
 *                       specified in its channel description (e.g. SDP file))
 *                      FALSE to disable (ER will not be performed regardless
 *                       of channel description configuration (e.g. SDP file))
 */
void
vqec_set_error_repair (boolean enabled) {
    /*
     * Set the global ER flag.  It setting will determine whether or not
     * to send ER requests.
     */
    g_vqec_error_repair = enabled;
    g_vqec_error_repair_set = TRUE;
}

/*
 * vqec_error_repair_policer_parameters_set()
 *
 * Sets the error repair token bucket policing paramters (rate and burst).
 *   - caller may specify rate, burst, or both
 *   - new values will be applied upon the NEXT binding of a tuner
 *     to a source, if error repair with policing is enabled.
 *   - if either rate or burst are invalid, no change (to either) is made
 *
 * Parameters:
 * @param[in] set_enabled - set to TRUE if new enabled value is being supplied
 * @param[in] enabled     - new enabled value
 * @param[in] set_rate    - set to TRUE if new rate value is being supplied
 * @param[in] rate        - new rate value
 * @param[in] set_burst   - set to TRUE if new burst value is being supplied
 * @param[in] burst       - new burst value
 *
 *   @return     VQEC_OK                 - new value(s) were set
 *               VQEC_PARAMRANGE_INVALID - rate or burst is outside of the
 *                                         legal range, set attempt rejected
 */
vqec_error_t
vqec_error_repair_policer_parameters_set (boolean set_enabled,
                                          boolean enabled,
                                          boolean set_rate,
                                          uint32_t rate,
                                          boolean set_burst,
                                          uint32_t burst)
{
    /* Validate the new rate and burst, if supplied */
    if ((set_rate && !is_vqec_cfg_error_repair_policer_rate_valid(rate)) ||
	(set_burst && !is_vqec_cfg_error_repair_policer_burst_valid(burst))) {
	return VQEC_ERR_PARAMRANGEINVALID;
    }

    /*
     * Assignments to these values are NOT protected against interleaving
     * across writer threads:
     *    o VQE-C configuration initialization
     *    o CLI thread(s)
     * and reader threads:
     *    o CLI show command
     *    o tuner bind call
     * that want to treat these (r,b) values atomically.
     *
     * May need to resolve these issues:
     *   How to protect against one or more writes in the middle of a read?
     *   How to protect against a read in the middle of a write?
     * if interleaving proves to be an issue in deployments.
     */
    if (set_enabled) {
        s_vqec_error_repair_policer_enabled = enabled;
        s_vqec_error_repair_policer_enabled_set = TRUE;
    }
    if (set_rate) {
        s_vqec_error_repair_policer_rate = rate;
        s_vqec_error_repair_policer_rate_set = TRUE;
    }
    if (set_burst) {
        s_vqec_error_repair_policer_burst = burst;
        s_vqec_error_repair_policer_burst_set = TRUE;
    }
    return VQEC_OK;
}


/*
 * vqec_error_repair_policer_parameters_get()
 *
 * Gets CLI demo knob settings for error repair token bucket policing.
 * Caller may request info on any of {enabled, rate, burst}.
 *
 * Parameters:
 * @param[out]  enabled     error repair policer enabled status (T/F)
 * @param[out]  enabled_set TRUE if CLI demo enabled knob is set
 *                          FALSE if CLI demo enabled knob is unset
 * @param[out]  rate        token bucket rate (if non-NULL pointer supplied)
 * @param[out]  rate_set    TRUE if CLI demo rate knob is set
 *                          FALSE if CLI demo rate knob is unset
 * @param[out]  burst       token bucket burst (if non-NULL pointer supplied)
 * @param[out]  burst_set   TRUE if CLI demo burst knob is set
 *                          FALSE if CLI demo burst knob is unset
 *
 *   @return     None.
 */
void
vqec_error_repair_policer_parameters_get (boolean *enabled,
                                          boolean *enabled_set,
                                          uint32_t *rate,
                                          boolean *rate_set, 
                                          uint32_t *burst,
                                          boolean *burst_set)
{
    if (enabled_set) {
        *enabled_set = s_vqec_error_repair_policer_enabled_set;
    }
    if (enabled) {
        if (s_vqec_error_repair_policer_enabled_set) {
            *enabled = s_vqec_error_repair_policer_enabled;
        } else {
            *enabled = vqec_syscfg_get_ptr()->error_repair_policer_enable;
        }
    }

    if (rate_set) {
        *rate_set = s_vqec_error_repair_policer_rate_set;
    }
    if (rate) {
        if (s_vqec_error_repair_policer_rate_set) {
            *rate = s_vqec_error_repair_policer_rate;
        } else {
            *rate = vqec_syscfg_get_ptr()->error_repair_policer_rate;
        }
    }

    if (burst_set) {
        *burst_set = s_vqec_error_repair_policer_burst_set;
    }
    if (burst) {
        if (s_vqec_error_repair_policer_burst_set) {
            *burst = s_vqec_error_repair_policer_burst;
        } else {
            *burst = vqec_syscfg_get_ptr()->error_repair_policer_burst;
        }
    }
}


/*
 * vqec_error_repair_show()
 *
 * Displays information about error repair.
 *
 * Params:
 *   @param[in]  cli  CLI context on which output should be displayed
 */
void
vqec_error_repair_show (struct vqec_cli_def *cli)
{
    boolean demo_set_er, demo_set_erp, demo_set_erpr, demo_set_erpb;
    boolean er_enabled, er_policer_enabled;
    uint32_t er_policer_rate, er_policer_burst;

    VQEC_ASSERT(cli);

    er_enabled = vqec_get_error_repair(&demo_set_er);
    vqec_cli_print(cli, "error-repair:                  %s%s",
                   er_enabled ? "enabled" : "disabled",
                   demo_set_er ? " (set by CLI)" : "");
    vqec_error_repair_policer_parameters_get(&er_policer_enabled,
                                             &demo_set_erp,
                                             &er_policer_rate, 
                                             &demo_set_erpr,
                                             &er_policer_burst,
                                             &demo_set_erpb);
    vqec_cli_print(cli, " repair policer:               %s%s",
                   er_policer_enabled ? "enabled" : "disabled",
                   demo_set_erp ? " (set by CLI)" : "");
    vqec_cli_print(cli, "  rate:                        %u%%%s",
                   er_policer_rate, demo_set_erpr ? " (set by CLI)" : "");
    vqec_cli_print(cli, "  burst:                       %ums%s",
                   er_policer_burst, demo_set_erpb ? " (set by CLI)" : "");
    vqec_cli_print(cli, "  packet requests policed:     %llu",
                   g_vqec_error_repair_policed_requests);
}

/*
 * vqec_pcm_construct_generic_nack()
 *
 * Constructs the variable-length Feedback Control Information (FCI)
 * generic NACK data contents for use in an RTCP feedback message 
 * (see RFC 4585, section 6.1, 6.2).
 *
 * Parameters:
 *  @param[in]:  chan           Channel whose gaps are to be included in
 *                               the FCI field
 *  @param[in]:  nd             buffer into which Generic NACKs are to be built
 *  @param[in]:  max_fci_count  max number of Generic NACK fields to insert
 *                               into the buffer (limit is 
 *                               VQEC_GAP_REPORTER_FCI_MAX)
 *  @param[out]: num_repairs    number of repairs requested within all Generic
 *                               NACK fields written to the buffer [optional]
 *  @return:
 *    how many Generic NACK fields actually written to the buffer
 *    (up to max_fci_count)
 */
UT_STATIC uint32_t 
vqec_gap_reporter_construct_generic_nack (vqec_chan_t *chan,
                                          rtcp_rtpfb_generic_nack_t *nd,
                                          int max_fci_count,
                                          uint32_t *num_repairs)
{
    boolean more = TRUE;
    int i;
    vqec_seq_num_t seq;

    int fci_count = 0;          /*
                                 * How many Generic NACK fields are encoded
                                 * into the nd buffer?
                                 */
    uint32_t repair_count = 0;  /*
                                 * How many repair requests are encoded across
                                 * all generic NACKs (excludes policed repairs)
                                 */
    uint32_t policed_count = 0; /*
                                 * How many repair requests were policed?
                                 */
    uint32_t first_pid;
    boolean first_pid_set = FALSE;
    uint32_t current_pid = 0, current_blp = 0; 
    boolean current_pid_set = FALSE;
    const int blp_max = 16;
    uint16_t gap_diff = 0;    
    uint32_t avail_tokens;
    tb_retval_t tb_retval;
    char str[VQEC_LOGMSG_BUFSIZE];
    vqec_dp_error_t err;
    vqec_dp_gap_buffer_t gapbuff;

    if (!chan || !nd) {
        return 0;
    }

    if (max_fci_count > VQEC_GAP_REPORTER_FCI_MAX) {
        VQEC_ASSERT(0);
        return 0;
    }
    if (!chan->er_enabled) {
        return 0;
    }

    bzero(nd, sizeof(rtcp_rtpfb_generic_nack_t) * max_fci_count);
    
    if (chan->er_policer_enabled) {
        tb_retval = tb_credit_tokens(&chan->er_policer_tb, 
                                     ABS_TIME_0, &avail_tokens);
        if (tb_retval != TB_RETVAL_OK) {
            snprintf(str, VQEC_LOGMSG_BUFSIZE,
                     "Failed to credit repair policer: %s",
                     tb_retval_to_str(tb_retval));
            syslog_print(VQEC_ERROR, str);
            /*
             * Force the error-repair policer to be disabled for this stream.
             */
            chan->er_policer_enabled = FALSE;
        }
    }

    /*
     * First we need to go through the gap list to figure out how many 
     * packets we need to send.
     */
    while (more) {
        err = vqec_dp_chan_get_gap_report(chan->dp_chanid, &gapbuff, &more);
        if (err != VQEC_DP_ERR_OK) {
            snprintf(str, VQEC_LOGMSG_BUFSIZE,
                     "channel %s, failure in gap collection from dataplane, "
                     "reason-code (%d)", 
                     vqec_chan_print_name(chan), err);
            syslog_print(VQEC_ERROR, str);
            return (0);
        }

        for (i = 0; i < gapbuff.num_gaps; i++) {
            for (seq = gapbuff.gap_list[i].start_seq;
                 vqec_seq_num_le(seq, 
                                 vqec_seq_num_add(gapbuff.gap_list[i].start_seq,
                                                  gapbuff.gap_list[i].extent));
                 seq = vqec_next_seq_num(seq)) {

                /*
                 * If error-repair policing is enabled for this stream,
                 * police the request using the local copy of number of tokens
                 * available.
                 */
                if (chan->er_policer_enabled) {
                    if (avail_tokens) {
                        avail_tokens--;
                    } else {
                        policed_count++;
                        continue;
                    }
                }

                /*
                 * Keep track of how many repairs are added to the FCI field 
                 * (across all generic NACK fields)
                 */
                repair_count++;

                /* 
                 * Check if we have enough space to store the fci
                 */
                if (fci_count >= max_fci_count) {
                    syslog_print(VQEC_ERROR, 
                                 "Too many gaps, can't be report by one NACK");
                    break;
                }

                if (!first_pid_set) {
                    first_pid_set = TRUE;
                    first_pid = seq;
                    current_pid = first_pid;
                    current_pid_set = TRUE;
                } else {
                    if (vqec_seq_num_lt(seq, current_pid) ||
                        vqec_seq_num_lt(vqec_seq_num_add(current_pid,
                                                         blp_max), seq)) {
                        nd[fci_count].pid = 
                            vqec_seq_num_to_rtp_seq_num(current_pid) -
                            chan->prim_session->session_rtp_seq_num_offset;
                        nd[fci_count].bitmask = current_blp;
                        gap_diff = 0;
                        fci_count++;
                        current_pid = seq;
                        current_blp = 0;
                    } else {
                        gap_diff = 
                            vqec_seq_num_sub(seq, current_pid);
                        current_blp |= 1 << (gap_diff - 1);
                    }            
                }
            } /* end-of-for(seq) */
        } /* end-of-for(i) */
    } /* end-of-while */

    /*
     * Update the token bucket.  This shouldn't fail--if it does, 
     * log an error, since it could mean some other thread is using
     * the token bucket that is not anticipated.
     *
     * Note that once the token bucket is updated, we do not re-credit 
     * the bucket if these encoded repair requests cannot be requested 
     * for some reason (e.g. not all repairs can be encoded in the 
     * caller's buffer, or the RTCP packet containing the request cannot 
     * be sent).
     */
    if (chan->er_policer_enabled) {
        tb_retval = tb_drain_tokens(&chan->er_policer_tb, repair_count);
        if (tb_retval != TB_RETVAL_OK) {
            /*
             * Force the error-repair policer to be disabled for this stream.
             */
            chan->er_policer_enabled = FALSE;
            snprintf(str, VQEC_LOGMSG_BUFSIZE,
                     "Failed to update repair policer:  %s",
                     tb_retval_to_str(tb_retval));
            syslog_print(VQEC_ERROR, str);
        }
        if (policed_count) {
            chan->stats.total_repairs_policed += policed_count;
            g_vqec_error_repair_policed_requests +=policed_count;
            VQEC_DEBUG(VQEC_DEBUG_ERROR_REPAIR,
                       "policed %u of %u repair requests for channel: %s:%d\n",
                       policed_count, policed_count + repair_count,
                       inaddr_ntoa_r(
                           chan->cfg.primary_dest_addr,
                           str, sizeof(str)),
                       ntohs(chan->cfg.primary_dest_port));
        }
    }

    /* Remember the last fci if we have space */
    if ((repair_count) && (fci_count < max_fci_count)) {
        nd[fci_count].pid =
            vqec_seq_num_to_rtp_seq_num(current_pid) -
            chan->prim_session->session_rtp_seq_num_offset;
        nd[fci_count].bitmask = current_blp;
        fci_count++;
    } else if (repair_count) {
        VQEC_DEBUG(VQEC_DEBUG_ERROR_REPAIR, "suppressed jumbo gaps");
        chan->stats.suppressed_jumbo_gap_counter++;
        chan->stats.total_repairs_unrequested += repair_count;
        fci_count = 0;
        repair_count = 0;
    }

    if (num_repairs) {
        *num_repairs = repair_count;
    }
    return fci_count;
}

/**
 * vqec_gap_reporter_report
 *
 * Retrieve the gap list associated with the given channel, convert it into
 * a generic nack, then send it.  Don't need to check the lock, caller should
 * get it.
 *
 * @param[in]    evptr Pointer to the event triggering the callback
 * @param[in]    unused_fd Not used
 * @param[in]    event Not used
 * @param[in]    arg Channel ID for the channel whose gaps are to be reported.
 */
void vqec_gap_reporter_report (const vqec_event_t *const unused_evptr,
                               int unused_fd,
                               short unused_event,
                               void *arg)
{
    vqec_chan_t *chan;
    int fci_count = 0; /* How many FCIs we need to have */
    uint32_t num_repairs;   /*
                             * How many repair requests were encoded across
                             * all generic NACKs
                             */
    rtp_session_t * prim_session;
    uint32_t i;
    uint32_t send_result;
    uint8_t retval;

    /* 
     * One packet is about 1500 bytes, 
     * 1 FCI(4 bytes) can report 17 packet loss.
     * So 256 FCI can report around 4k losses.
     * And 256 * 4 = 1k bytes, less that ethernet
     * packet length.
     */
    rtcp_rtpfb_generic_nack_t nd[VQEC_GAP_REPORTER_FCI_MAX];
    rtcp_pkt_info_t pkt_info;
    rtcp_msg_info_t nack;

    uint32_t data_len;
    rtcp_msg_info_t app;
    erri_tlv_t tlv;
    uint32_t recv_bw;
    
    bzero(nd, sizeof(rtcp_rtpfb_generic_nack_t) * VQEC_GAP_REPORTER_FCI_MAX);
    
    chan = vqec_chanid_to_chan((vqec_chanid_t)arg);
    if (!chan) {
        return;
    }

    if (!chan->er_enabled) {
        /*
         * If ER has been disabled via CLI, do not issue repair requests.
         */
        return;
    }

    if (!IN_MULTICAST(ntohl(chan->cfg.primary_dest_addr.s_addr)) &&
        ((chan->cfg.primary_source_addr.s_addr !=
          chan->prim_session->pktflow_src.id.src_addr) ||
         (chan->cfg.primary_source_port !=
          chan->prim_session->pktflow_src.id.src_port))) {

        /*
         * If a unicast channel's packetflow source does not match its
         * configured source, then repairs are suppressed.
         *
         * This situation may arise during failover, e.g.:
         *   1) the configured source has gone away and been replaced
         *      by a new source in the DP (failover), but the CP has
         *      not yet been updated by its FBT address and port, or
         *   2) the configured source is replacing a source that will be
         *      going away very soon (planned failover), but we haven't 
         *      yet heard packets from the new source in the dataplane.
         *
         * In these situations, sending repair requests to the configured
         * source could be unproductive, since the packetflow source's
         * sequence space has an uncertain relationship to that of the
         * configured source.
         */
        VQEC_DEBUG(VQEC_DEBUG_PCM,
                   "Suppressing error repair attempt due to mismatch in "
                   "configured and observed channel primary source\n");
        return;
    }

    fci_count =
        vqec_gap_reporter_construct_generic_nack(chan, nd,
                                                 VQEC_GAP_REPORTER_FCI_MAX,
                                                 &num_repairs);
    if (!fci_count) {
        return;
    }

    /* set up to include an RTPFB (generic NACK) in an RTCP report */
    rtcp_init_pkt_info(&pkt_info);
    rtcp_init_msg_info(&nack);
    nack.rtpfb_info.fmt = RTCP_RTPFB_GENERIC_NACK;
    nack.rtpfb_info.ssrc_media_sender = 
        chan->prim_session->pktflow_src.id.ssrc;
    nack.rtpfb_info.num_nacks = fci_count;
    nack.rtpfb_info.nack_data = nd;
    rtcp_set_pkt_info(&pkt_info, RTCP_RTPFB, &nack);
    if (chan->cfg.primary_rtcp_rsize) {
        /* Set the packet up for Reduced size RTCP for generic NACK */
        retval = rtcp_set_rtcp_rsize(&pkt_info, TRUE);
    }

    for (i = 0; i < nack.rtpfb_info.num_nacks; i++) {
        VQEC_DEBUG(VQEC_DEBUG_PCM,
                   "gap detected - num_nacks = %d, nack data[%d] gap "
                   "pid = %d, bitmask = %x\n",
                   nack.rtpfb_info.num_nacks, i, nd[i].pid, nd[i].bitmask);
    }

    /* compute receive bandwidth and determine if it's enough to do ER */
    if (chan->use_rcc_bw_for_er) {
        recv_bw = vqec_chan_get_max_recv_bw_rcc(chan);
        if (!recv_bw && chan->max_recv_bw_rcc) {
            /*
             * FEC bw must be greater than max_recv_bw_er; cause the ER not to
             * happen by setting recv_bw to 1 bps, which is too low for an ER.
             *
             * A bw of 0 means no room for "primary stream + VQE services", but
             * the request is sent anyway so VQE-S can track admission control
             * failures.  A value of 1 bps is singalled instead of 0, since 0
             * implies that the server should pick an e-factor.  Similar logic
             * is used for the RCC signalling as well.
             */
            recv_bw = 1;
        }
    } else {
        recv_bw = vqec_chan_get_max_recv_bw_er(chan);
        if (!recv_bw && chan->max_recv_bw_er) {
            /*
             * FEC bw must be greater than max_recv_bw_er; cause the ER not to
             * happen by setting recv_bw to 1 bps, which is too low for an ER.
             */
            recv_bw = 1;
        }
    }
    /* attach ERRI APP info if necessary */
    if (recv_bw) {  /* only attach this APP if we have a valid recv_bw value */
        rtcp_init_msg_info(&app);
        memcpy(&app.app_info.name, "ERRI", sizeof(uint32_t));
        /* Prepare TLV */
        memset(&tlv, 0, sizeof(erri_tlv_t));
        tlv.maximum_recv_bw = recv_bw;

        app.app_info.data = 
            (uint8_t *) erri_tlv_encode_allocate(tlv, &data_len);
        app.app_info.len = data_len;
        rtcp_set_pkt_info(&pkt_info, RTCP_APP, &app);
    }

    prim_session = (rtp_session_t *)chan->prim_session;
    send_result = MCALL(prim_session, 
                        rtcp_send_report,
                        prim_session->rtp_local_source,
                        &pkt_info,
                        FALSE,  /* not re-schedule next RTCP report */
                        FALSE); /* not reset RTCP XR stats */
    if (send_result > 0) {
        chan->stats.total_repairs_requested += num_repairs;
        if (!chan->stats.generic_nack_counter) {
            chan->stats.first_nack_repair_cnt = num_repairs;
        }
        chan->stats.generic_nack_counter++;
    } else {
        chan->stats.fail_to_send_rtcp_counter++;
        chan->stats.total_repairs_unrequested += num_repairs;

        VQEC_DEBUG(VQEC_DEBUG_PCM,
                   "Failed to send rtcp packet (%s)\n",
                   strerror(errno));
    }
}

