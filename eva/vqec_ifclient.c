/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2010, 2012 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_ifclient.c
 *
 * Description: VQEC Public API implementation.
 *
 * Documents:
 *
 *****************************************************************************/
#define _GNU_SOURCE 1

#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>
#include <utils/vmd5.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vqec_error.h"
#include "vqec_lock_defs.h"
#include "vqec_assert_macros.h"
#include "vqec_url.h"
#include "vqec_tuner.h"
#include "vqec_syscfg.h"
#include <cfg/cfgapi.h>
#include "vqec_igmp.h"
#include "vqec_updater.h"
#include "vqec_ifclient.h"
#include "vqec_ifclient_private.h"
#include "vqec_debug.h"
#include "vqec_event.h"
#include "vqec_cli_register.h"
#include "vqec_gap_reporter.h"
#include "vqec_nat_interface.h"
#include "vqec_channel_api.h"
#include "vqec_dp_api_types.h"
#include "vqec_upcall_event.h"
#include <utils/vqe_hash.h>
#include "vqec_drop.h"
#include <utils/vam_util.h>
#include <utils/vam_hist.h>
#include "vqec_pthread.h"
#include <poll.h>

#ifndef HAVE_STRLFUNCS
#include <utils/strl.h>
#endif

#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif // _VQEC_UTEST_INTERPOSERS

/*Include CFG Library syslog header files */
#include <log/vqe_cfg_syslog_def.h>
#include <log/vqe_cfg_debug.h>
#include <log/vqe_rtp_syslog_def.h>
#include <log/vqe_utils_syslog_def.h>

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

typedef enum vqec_ifclient_state_t_
{
    VQEC_IFCLIENT_UNINITED = 0,
    VQEC_IFCLIENT_INITED = 1,
    VQEC_IFCLIENT_STARTED,
    VQEC_IFCLIENT_STOPPED,
} vqec_ifclient_state_t;

#define VQEC_IFCLIENT_SDP_HANDLE_TMP "o=tmp_chan"
#define VQEC_IFCLIENT_SDP_HANDLE_LEN 20
typedef struct vqec_ifclient_tmp_chan_ {
    /* channel config details */
    vqec_chan_cfg_t chan_cfg;
    vqec_ifclient_chan_event_cb_t chan_event_cb;
    char sdp_handle[VQEC_IFCLIENT_SDP_HANDLE_LEN];
} vqec_ifclient_tmp_chan_t;

static vqec_event_t *s_vqec_keepalive_ev;
UT_STATIC vqec_ifclient_state_t s_vqec_ifclient_state;
UT_STATIC vqec_ifclient_tmp_chan_t *s_vqec_ifclient_tmp_chans;
UT_STATIC boolean s_vqec_ifclient_deliver_paks_to_user;

UT_STATIC vqec_error_t 
vqec_ifclient_tuner_create_ul(vqec_tunerid_t *id, const char *name);
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_destroy_ul(const vqec_tunerid_t id);
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_bind_chan_ul(const vqec_tunerid_t id,
                                 const vqec_sdp_handle_t sdp_handle,
                                 const vqec_bind_params_t *bp);
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_bind_chan_cfg_ul(const vqec_tunerid_t id,
                                     vqec_chan_cfg_t *cfg,
                                     const vqec_bind_params_t *bp);
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_unbind_chan_ul(const vqec_tunerid_t id);
UT_STATIC vqec_sdp_handle_t
vqec_ifclient_alloc_sdp_handle_from_url_ul(char *url);
UT_STATIC void
vqec_ifclient_free_sdp_handle_ul(vqec_sdp_handle_t sdp_handle);
UT_STATIC uint8_t 
vqec_ifclient_chan_cfg_parse_sdp_ul(vqec_chan_cfg_t *cfg,
                                    char *buffer,
                                    vqec_chan_type_t chan_type);
UT_STATIC vqec_tunerid_t 
vqec_ifclient_tuner_get_id_by_name_ul(const char *namestr);
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_get_name_by_id_ul(vqec_tunerid_t id, char *name, 
                                      int32_t len);
boolean vqec_ifclient_check_tuner_validity_by_name_ul(const char *namestr);
UT_STATIC vqec_tunerid_t 
vqec_ifclient_tuner_iter_init_first_ul(vqec_tuner_iter_t *iter);
UT_STATIC vqec_tunerid_t
vqec_ifclient_tuner_iter_getnext_ul(vqec_tuner_iter_t *iter);
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_recvmsg_ul(const vqec_tunerid_t id,
                               vqec_iobuf_t *iobuf, 
                               int32_t iobuf_num,
                               int32_t *bytes_read,
                               int32_t timeout);
UT_STATIC vqec_error_t
vqec_ifclient_init_ul(const char *filename);
UT_STATIC void
vqec_ifclient_deinit_ul(void);
UT_STATIC vqec_error_t
vqec_ifclient_start_ul(void);
UT_STATIC void
vqec_ifclient_stop_ul(void);
UT_STATIC void
vqec_ifclient_clear_stats_ul(void);
UT_STATIC vqec_error_t
vqec_ifclient_tuner_dump_state_ul(const vqec_tunerid_t id,
                                  unsigned int options_flag);
UT_STATIC vqec_error_t 
vqec_ifclient_get_stats_ul(vqec_ifclient_stats_t *stats, 
                           boolean cumulative);
UT_STATIC vqec_error_t 
vqec_ifclient_get_stats_channel_ul(const char *url,
                                   vqec_ifclient_stats_channel_t *stats,
                                   boolean cumulative);
UT_STATIC vqec_error_t 
vqec_ifclient_get_stats_channel_tr135_sample_ul (
                    const char *url,
                    vqec_ifclient_stats_channel_tr135_sample_t *stats);
UT_STATIC vqec_error_t 
vqec_ifclient_set_tr135_params_channel_ul(const char *url,
                                          vqec_ifclient_tr135_params_t *params);
UT_STATIC vqec_error_t
vqec_ifclient_get_stats_tuner_legacy_ul(vqec_tunerid_t id,
                                        vqec_ifclient_stats_channel_t *stats);
UT_STATIC vqec_error_t
vqec_ifclient_histogram_display_ul(const vqec_hist_t hist);
UT_STATIC vqec_error_t
vqec_ifclient_histogram_clear_ul(const vqec_hist_t hist);
UT_STATIC vqec_error_t 
vqec_ifclient_updater_get_stats_ul(vqec_ifclient_updater_stats_t *stats);
UT_STATIC uint8_t 
vqec_ifclient_tuner_is_fastfill_enabled_ul(vqec_tunerid_t id);
UT_STATIC uint8_t 
vqec_ifclient_tuner_is_rcc_enabled_ul(vqec_tunerid_t id);
UT_STATIC uint8_t 
vqec_ifclient_socket_open_ul(vqec_in_addr_t *addr,
                             vqec_in_port_t *rtp_port,
                             vqec_in_port_t *rtcp_port);
UT_STATIC void 
vqec_ifclient_socket_close_ul(vqec_in_addr_t addr,
                              vqec_in_port_t rtp_port,
                              vqec_in_port_t rtcp_port);
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_bind_chan_cfg_internal (
    const vqec_tunerid_t id,
    vqec_chan_cfg_t *cfg,
    const vqec_bind_params_t *bp,
    const vqec_ifclient_chan_event_cb_t *chan_event_cb);


int32_t g_vqec_client_mode;

/**
 * Add a channel to the temp channel database
 *
 * @param[in] *chan: the channel cfg that need to be added to temp chan db.
 * @param[in] *chan_params: chan parameter
 * @return TRUE if success, FALSE otherwise
 */

uint8_t
vqec_ifclient_add_new_tmp_chan_ul(vqec_chan_cfg_t *chan,
                                  vqec_ifclient_tmp_chan_params_t *chan_params)
{
    const vqec_syscfg_t *v_cfg = vqec_syscfg_get_ptr();
    uint32_t i = 0;
    vqec_ifclient_tmp_chan_t *tmp_chan = NULL;

    if (!chan || !v_cfg) {
        return FALSE;
    }
    for (i = 0; i < v_cfg->max_tuners; i++) {
        if (s_vqec_ifclient_tmp_chans[i].sdp_handle[0] == '\0') {
            break;
        }
    }
    if (i == v_cfg->max_tuners) {
        syslog_print(VQEC_ERROR,
                     "too many dynamic channels allocated");
        return FALSE;
    }

    tmp_chan = &(s_vqec_ifclient_tmp_chans[i]);
    /* copy chan cfg detail */
    memcpy(&(tmp_chan->chan_cfg), chan, sizeof(vqec_chan_cfg_t));

    if (chan_params && chan_params->event_cb.chan_event_cb) {
        memcpy(&(tmp_chan->chan_event_cb), &(chan_params->event_cb), 
               sizeof(vqec_ifclient_chan_event_cb_t));
    }

    /* looks like:  "o=tmpchan01" */
    snprintf(tmp_chan->sdp_handle,
             VQEC_IFCLIENT_SDP_HANDLE_LEN,
             VQEC_IFCLIENT_SDP_HANDLE_TMP "%02u",
             i);

    return TRUE;
}

/**
 * API for adding a channel to the temp channel database
 *
 * @param[in] *chan: the channel cfg that need to be added to temp chan db.
 * @param[in] chan_event_cb: chan event cb function
 * @return TRUE if success, FALSE otherwise
 */
uint8_t
vqec_ifclient_add_new_tmp_chan(vqec_chan_cfg_t *chan,
                               vqec_ifclient_tmp_chan_params_t *chan_params)
{
    uint8_t ret = FALSE;

    vqec_lock_lock(vqec_g_lock);
    ret = vqec_ifclient_add_new_tmp_chan_ul(chan, chan_params);
    vqec_lock_unlock(vqec_g_lock);

    return ret;
}

/**
 * Remove a channel from temp channel database
 * based on the destination IP address
 *
 * @param[in] dest_ip: dest ip address (network byte order)
 * @param[in] port: dest port (network byte order)
 * @return: TRUE if success, FALSE otherwise
 */
void
vqec_ifclient_remove_tmp_chan_ul (vqec_in_addr_t dest_ip, vqec_in_port_t port)
{
    const vqec_syscfg_t *v_cfg = vqec_syscfg_get_ptr();
    uint32_t i = 0;
    vqec_ifclient_tmp_chan_t *chan = NULL;
    
    if (!v_cfg) {
        syslog_print(VQEC_ERROR, "cfg NULL");
        return;
    }

    for (i = 0; i < v_cfg->max_tuners; i++) {
        if ((s_vqec_ifclient_tmp_chans[i].chan_cfg.primary_dest_addr.s_addr
             == dest_ip.s_addr) &&
            (s_vqec_ifclient_tmp_chans[i].chan_cfg.primary_dest_port == port)){
            break;
        }
    }
    if (i == v_cfg->max_tuners) {
        VQEC_DEBUG(VQEC_DEBUG_CHAN_CFG, "no channel found\n");
        return;
    }

    chan = &(s_vqec_ifclient_tmp_chans[i]);
    memset(chan, 0, sizeof(vqec_ifclient_tmp_chan_t));
}


/**
 * API for removing a channel from temp channel database
 * based on the destination IP address
 *
 * @param[in] dest_ip: dest ip address (network byte order)
 * @param[in] port: dest port (network byte order)
 * @return: TRUE if success, FALSE otherwise
 */
void
vqec_ifclient_remove_tmp_chan (vqec_in_addr_t dest_ip, vqec_in_port_t port)
{
    vqec_lock_lock(vqec_g_lock);
    vqec_ifclient_remove_tmp_chan_ul(dest_ip, port);
    vqec_lock_unlock(vqec_g_lock);
}

/**
 * check if channel is already in tmp chan 
 *
 * @param[in] dest_ip: dest ip address
 * @param[in] port: dest port
 * @return: TRUE if success, FALSE otherwise.
 */

vqec_sdp_handle_t
vqec_ifclient_get_tmp_chan_sdp_handle (in_addr_t dest_ip, in_port_t port)
{
    const vqec_syscfg_t *v_cfg = vqec_syscfg_get_ptr();
    uint32_t i = 0;

    if (!v_cfg) {
        syslog_print(VQEC_ERROR, "cfg NULL");
        return NULL;
    }
    for (i = 0; i < v_cfg->max_tuners; i++) {
        if ((s_vqec_ifclient_tmp_chans[i].chan_cfg.primary_dest_addr.s_addr 
             == dest_ip) &&
            (s_vqec_ifclient_tmp_chans[i].chan_cfg.primary_dest_port
             == port) &&
            (s_vqec_ifclient_tmp_chans[i].sdp_handle[0] != '\0')) {
            return (s_vqec_ifclient_tmp_chans[i].sdp_handle);
        }
    }
    return NULL;
}

void vqec_ifclient_log_err (vqec_ifclient_errtype_t type, const char *format, ...)
{
    char buf[VQEC_IFCLIENT_ERR_STRLEN];
    va_list ap;

    va_start(ap, format);    
    vsnprintf(buf, VQEC_IFCLIENT_ERR_STRLEN, format, ap);
    va_end(ap);

    switch (type) {
    case VQEC_IFCLIENT_ERR_GENERAL:
        syslog_print(VQEC_ERROR, buf);
        break;

    case VQEC_IFCLIENT_ERR_MALLOC:
        syslog_print(VQEC_MALLOC_FAILURE, buf);
        break;

    default:
        VQEC_ASSERT(FALSE);
    }
}

const char *vqec_err2str (vqec_error_t err)
{
    return (vqec_err2str_internal(err));
}

const char *
vqec_ifclient_tuner_rccabort2str (char *str,
                                  uint32_t len,
                                  vqec_ifclient_rcc_abort_t abort_code)
{
    if (!str || !len) {
        return (NULL);
    }

    /* Map the abort code to a string */
    switch (abort_code) {
    case VQEC_IFCLIENT_RCC_ABORT_SERVER_REJECT:
        (void)snprintf(str, len, "%s", "rcc server reject");
        break;
    case VQEC_IFCLIENT_RCC_ABORT_RESPONSE_INVALID:
        (void)snprintf(str, len, "%s",  "response parse error");
        break;
    case VQEC_IFCLIENT_RCC_ABORT_STUN_TIMEOUT:
        (void)snprintf(str, len, "%s",  "stun timeout");
        break;
    case VQEC_IFCLIENT_RCC_ABORT_RESPONSE_TIMEOUT:
        (void)snprintf(str, len, "%s",  "rcc response timeout");
        break;
    case VQEC_IFCLIENT_RCC_ABORT_BURST_START:
        (void)snprintf(str, len, "%s",  "burst start timeout");
        break;
    case VQEC_IFCLIENT_RCC_ABORT_BURST_ACTIVITY:
        (void)snprintf(str, len, "%s",  "burst activity timeout");
        break;
    case VQEC_IFCLIENT_RCC_ABORT_OTHER:
    default:
        (void)snprintf(str, len, "%s",  "internal error");
        break;
    }
    return (str);
}

/*
 * vqec_ifclient_chan_cfg_to_str()
 *
 * Generates a printable string for displaying the contents of a
 * vqec_chan_cfg_t structure.
 *
 * Params:
 *    @param[in]  cfg     structure whose contents are to be described
 *    @param[out] char *  pointer to string containing description of
 *                        cfg contents.  This string should be used
 *                        or copied immediately; subsequent calls to this
 *                        function will overwrite its contents.
 */
static char *
vqec_ifclient_chan_cfg_to_str (const vqec_chan_cfg_t *cfg)
{
    
    /* TBD:  this dependency needs to be included properly */
    extern void vqec_cmd_parse_print_params(FILE *f,
                                            vqec_chan_cfg_t *cfg,
                                            boolean newline);


#define VQEC_IFCLIENT_CHAN_CFG_TMP_STR_LEN 1024
    static char s_chan_cfg_tmp[VQEC_IFCLIENT_CHAN_CFG_TMP_STR_LEN];
    FILE *fp = NULL;
    int len;

    /* Initialize the argument string to a user-friendly, empty value */
    snprintf(s_chan_cfg_tmp, VQEC_IFCLIENT_CHAN_CFG_TMP_STR_LEN,
                 "<arguments unavailable>");

    /* Generate the argument string */
    fp = fmemopen(NULL, VQEC_IFCLIENT_CHAN_CFG_TMP_STR_LEN, "r+");
    if (!fp) {
        goto done;
    }
    vqec_cmd_parse_print_params(fp, (vqec_chan_cfg_t *)cfg, FALSE);
    if (fseek(fp, 0, SEEK_END)) {
        goto done;
    }
    len = ftell(fp);
    if (fseek(fp, 0, SEEK_SET)) {
        goto done;
    }
    memset(s_chan_cfg_tmp, 0, VQEC_IFCLIENT_CHAN_CFG_TMP_STR_LEN);
    (void)fread(s_chan_cfg_tmp, len, 1, fp);

 done:
    if (fp) {
        fclose(fp);
    }
    return (s_chan_cfg_tmp);
}


//----------------------------------------------------------------------------
// Bind Parameters methods.
//----------------------------------------------------------------------------
struct vqec_bind_params_ {
    int32_t context_id;           /* tuner id*/
    uint8_t do_rcc;           /* do rcc if enabled in chan change*/
    uint8_t do_fastfill;      /* do fastfill if requested */
    uint32_t max_fastfill;    /* max allowed fastfill (in bytes) for RCC */
    uint32_t max_recv_bw_rcc;  /* per-tuner e-factor for RCC */
    uint32_t max_recv_bw_er;  /* per-tuner e-factor for ER */
    vqec_ifclient_get_dcr_info_ops_t *dcr_info_ops;
                              /**
                               * This optional function pointer is passed
                               * to vqec from STB at channel change.
                               * vqec will use this pointer to get some
                               * stats information from decoder.
                               */
    vqec_ifclient_fastfill_cb_ops_t *fastfill_ops;
    void (*mrb_transition_notify)(int32_t context_id);

    boolean ir_time_valid;    /* TRUE if user sets ir_time via bp */
    uint64_t ir_time;         /* time of the IR keypress */
    vqec_ifclient_tr135_params_t tr135_params; /* tr-135 GMIN & SLMD settings */
};

                
/*
 * vqec_ifclient_bp_to_str()
 *
 * Generates a printable string for displaying the contents of a
 * vqec_bind_params_t structure.
 *
 * Params:
 *    @param[in]  bp      structure whose contents are to be described
 *    @param[out] char *  pointer to string containing description of
 *                        bp contents.  This string should be used
 *                        or copied immediately; subsequent calls to this
 *                        function will overwrite its contents.
 */
char *
vqec_ifclient_bp_to_str (const vqec_bind_params_t *bp)
{
#define VQEC_IFCLIENT_BP_STR_LEN   1024
    static char s_bp[VQEC_IFCLIENT_BP_STR_LEN];
#define VQEC_IFCLIENT_BP_TMP_STR_LEN   32
    static char s_bp_tmp[VQEC_IFCLIENT_BP_TMP_STR_LEN];
    boolean comma_needed;
    int wr_len;
        
    s_bp[0] = '\0';

    if (!bp) {
        goto done;
    }

    wr_len = strlcat(s_bp, "{", VQEC_IFCLIENT_BP_STR_LEN);

    /* context_id */
    snprintf(s_bp_tmp, VQEC_IFCLIENT_BP_TMP_STR_LEN, "%d", bp->context_id);
    wr_len = strlcat(s_bp, "context_id=", VQEC_IFCLIENT_BP_STR_LEN);
    wr_len = strlcat(s_bp, s_bp_tmp, VQEC_IFCLIENT_BP_STR_LEN);

    /* do_rcc */
    wr_len = strlcat(s_bp, ", do_rcc=", VQEC_IFCLIENT_BP_STR_LEN);
    wr_len = strlcat(s_bp, bp->do_rcc ? "TRUE" : "FALSE", 
                     VQEC_IFCLIENT_BP_STR_LEN);
    
    /* do_fastfill */
    wr_len = strlcat(s_bp, ", do_fastfill=", VQEC_IFCLIENT_BP_STR_LEN);
    wr_len = strlcat(s_bp, bp->do_fastfill ? "TRUE" : "FALSE", 
                     VQEC_IFCLIENT_BP_STR_LEN);

    /* max_fastfill */
    snprintf(s_bp_tmp, VQEC_IFCLIENT_BP_TMP_STR_LEN, "%d", bp->max_fastfill);
    wr_len = strlcat(s_bp, ", max_fastfill=", VQEC_IFCLIENT_BP_STR_LEN);
    wr_len = strlcat(s_bp, s_bp_tmp, VQEC_IFCLIENT_BP_STR_LEN);

    /* max_recv_bw_rcc */
    snprintf(s_bp_tmp, VQEC_IFCLIENT_BP_TMP_STR_LEN, "%d", 
             bp->max_recv_bw_rcc);
    wr_len = strlcat(s_bp, ", max_recv_bw_rcc=", VQEC_IFCLIENT_BP_STR_LEN);
    wr_len = strlcat(s_bp, s_bp_tmp, VQEC_IFCLIENT_BP_STR_LEN);
 
    /* max_recv_bw_er */
    snprintf(s_bp_tmp, VQEC_IFCLIENT_BP_TMP_STR_LEN, "%d", 
             bp->max_recv_bw_er);
    wr_len = strlcat(s_bp, ", max_recv_bw_er=", VQEC_IFCLIENT_BP_STR_LEN);
    wr_len = strlcat(s_bp, s_bp_tmp, VQEC_IFCLIENT_BP_STR_LEN);

    /* dcr_info_ops */
    if (bp->dcr_info_ops) {
        snprintf(s_bp_tmp, VQEC_IFCLIENT_BP_TMP_STR_LEN, "%p", 
                 bp->dcr_info_ops->get_dcr_stats);
        wr_len = strlcat(s_bp, ", dcr_info_ops={get_dcr_stats=", 
                         VQEC_IFCLIENT_BP_STR_LEN);
        wr_len = strlcat(s_bp, s_bp_tmp, VQEC_IFCLIENT_BP_STR_LEN);
        wr_len = strlcat(s_bp, "}", VQEC_IFCLIENT_BP_STR_LEN);
    }

    /* fastfill_ops */
    if (bp->fastfill_ops) {
        wr_len = strlcat(s_bp, ", fastfill_ops=", VQEC_IFCLIENT_BP_STR_LEN);

        wr_len = strlcat(s_bp, "{", VQEC_IFCLIENT_BP_STR_LEN);        
        comma_needed = FALSE;

        /* fastfill_start */
        if (bp->fastfill_ops->fastfill_start) {
            snprintf(s_bp_tmp, VQEC_IFCLIENT_BP_TMP_STR_LEN, "%p", 
                     bp->fastfill_ops->fastfill_start);
            wr_len = strlcat(s_bp, "fastfill_start=", 
                             VQEC_IFCLIENT_BP_STR_LEN);
            wr_len = strlcat(s_bp, s_bp_tmp, VQEC_IFCLIENT_BP_STR_LEN);
            comma_needed = TRUE;
        }

        /* fastfill_abort */
        if (bp->fastfill_ops->fastfill_abort) {
            if (comma_needed) {
                wr_len = strlcat(s_bp, ", ", VQEC_IFCLIENT_BP_STR_LEN);
            }
            snprintf(s_bp_tmp, VQEC_IFCLIENT_BP_TMP_STR_LEN, "%p", 
                     bp->fastfill_ops->fastfill_abort);
            wr_len = strlcat(s_bp, "fastfill_abort=", 
                             VQEC_IFCLIENT_BP_STR_LEN);
            wr_len = strlcat(s_bp, s_bp_tmp, VQEC_IFCLIENT_BP_STR_LEN);
            comma_needed = TRUE;
        }

        /* fastfill_done */
        if (bp->fastfill_ops->fastfill_done) {
            if (comma_needed) {
                wr_len = strlcat(s_bp, ", ", VQEC_IFCLIENT_BP_STR_LEN);
            }
            snprintf(s_bp_tmp, VQEC_IFCLIENT_BP_TMP_STR_LEN, "%p", 
                     bp->fastfill_ops->fastfill_done);
            wr_len = strlcat(s_bp, "fastfill_done=", VQEC_IFCLIENT_BP_STR_LEN);
            wr_len = strlcat(s_bp, s_bp_tmp, VQEC_IFCLIENT_BP_STR_LEN);
        }

        wr_len = strlcat(s_bp, "}", VQEC_IFCLIENT_BP_STR_LEN);
    }

    /* mrb_transition_notify */
    if (bp->mrb_transition_notify) {
        snprintf(s_bp_tmp, VQEC_IFCLIENT_BP_TMP_STR_LEN, "%p", 
                 bp->fastfill_ops->fastfill_done);
        wr_len = strlcat(s_bp, ", mrb_transition_notify=", 
                         VQEC_IFCLIENT_BP_STR_LEN);
        wr_len = strlcat(s_bp, s_bp_tmp, VQEC_IFCLIENT_BP_STR_LEN);
    }

    /* ir_time_valid */
    wr_len = strlcat(s_bp, ", ir_time_valid=", VQEC_IFCLIENT_BP_STR_LEN);
    wr_len = strlcat(s_bp, bp->ir_time_valid ? "TRUE" : "FALSE", 
                     VQEC_IFCLIENT_BP_STR_LEN);
    
    /* tr135_params.minimum_loss_distance */
    snprintf(s_bp_tmp, VQEC_IFCLIENT_BP_TMP_STR_LEN, "%u", 
             bp->tr135_params.gmin);
    wr_len = strlcat(s_bp, ", tr135_params.gmin=", VQEC_IFCLIENT_BP_STR_LEN);
    wr_len = strlcat(s_bp, s_bp_tmp, VQEC_IFCLIENT_BP_STR_LEN);

    /* tr135_params.severe_loss_min_distance */
    snprintf(s_bp_tmp, VQEC_IFCLIENT_BP_TMP_STR_LEN, "%u", 
             bp->tr135_params.severe_loss_min_distance);
    wr_len = strlcat(s_bp, ", tr135_params.severe_loss_min_distance=", 
                     VQEC_IFCLIENT_BP_STR_LEN);
    wr_len = strlcat(s_bp, s_bp_tmp, VQEC_IFCLIENT_BP_STR_LEN);

    wr_len = strlcat(s_bp, "}", VQEC_IFCLIENT_BP_STR_LEN);
    
 done:
    return (s_bp);
}

vqec_bind_params_t *
vqec_ifclient_bind_params_create (void)
{
    vqec_bind_params_t *bp;
    vqec_syscfg_t v_cfg;

    vqec_syscfg_get(&v_cfg);

    bp = malloc(sizeof(vqec_bind_params_t)); 
    if (bp) {   
        memset(bp, 0, sizeof(vqec_bind_params_t));
        bp->do_rcc = TRUE;
        bp->do_fastfill = TRUE;
        bp->max_fastfill = v_cfg.max_fastfill;
        bp->max_recv_bw_rcc = 0;
        bp->max_recv_bw_er = 0;
        bp->tr135_params.gmin = 0;  /* Default */
        bp->tr135_params.severe_loss_min_distance = 0;  /* Default */
        bp->ir_time_valid = FALSE;
        bp->ir_time = 0;
    }

    return bp;
}

uint8_t
vqec_ifclient_bind_params_destroy (vqec_bind_params_t *bp)
{
    if (bp) {
        free(bp);
        return TRUE;
    } else {
        return FALSE;
    }
}

uint8_t
vqec_ifclient_bind_params_set_context_id (vqec_bind_params_t *bp,
                                        const int32_t context_id)
{
    if (bp) {
        bp->context_id = context_id;
        return TRUE;
    } else {
        return FALSE;
    }
}

int32_t
vqec_ifclient_bind_params_get_context_id (const vqec_bind_params_t *bp)
{
    if (bp) {
        return bp->context_id;
    } else {
        return 0;
    }
}

uint8_t
vqec_ifclient_bind_params_enable_rcc (vqec_bind_params_t *bp)
{
    if (bp) {
        bp->do_rcc = TRUE;
        return TRUE;
    } else {
        return FALSE;
    }
}

uint8_t
vqec_ifclient_bind_params_disable_rcc (vqec_bind_params_t *bp)
{
    if (bp) {
        bp->do_rcc = FALSE;
        return TRUE;
    } else {
        return FALSE;
    }
}

uint8_t
vqec_ifclient_bind_params_is_rcc_enabled (const vqec_bind_params_t *bp)
{
    if (bp) {
        return bp->do_rcc;
    } else {
        return FALSE;
    }
}

uint8_t
vqec_ifclient_bind_params_set_dcr_info_ops (
                vqec_bind_params_t *bp,
                vqec_ifclient_get_dcr_info_ops_t *dcr_info_ops)
{
    if (bp) {
        bp->dcr_info_ops = dcr_info_ops;
        return TRUE;
    } else {
        return FALSE;
    }
}

vqec_ifclient_get_dcr_info_ops_t *
vqec_ifclient_bind_params_get_dcr_info_ops (
                           const vqec_bind_params_t *bp) 
{
    if (bp) {
        return bp->dcr_info_ops;
    } else {
        return NULL;
    }
}

uint8_t
vqec_ifclient_bind_params_set_fastfill_ops (
    vqec_bind_params_t *bp,
    vqec_ifclient_fastfill_cb_ops_t *fastfill_ops)
{
    if (bp) {
        bp->fastfill_ops = fastfill_ops;
        return TRUE;
    } else {
        return FALSE;
    }
}

vqec_ifclient_fastfill_cb_ops_t *
vqec_ifclient_bind_params_get_fastfill_ops (const vqec_bind_params_t *bp)
{
    if (bp) {
        return bp->fastfill_ops;
    } else {
        return NULL;
    }
}

uint8_t
vqec_ifclient_bind_params_enable_fastfill (vqec_bind_params_t *bp)
{
    if (bp) {
        bp->do_fastfill = TRUE;
        return TRUE;
    } else {
        return FALSE;
    }
}

uint8_t
vqec_ifclient_bind_params_disable_fastfill (vqec_bind_params_t *bp)
{
    if (bp) {
        bp->do_fastfill = FALSE;
        return TRUE;
    } else {
        return FALSE;
    }
}

uint8_t
vqec_ifclient_bind_params_is_fastfill_enabled (const vqec_bind_params_t *bp)
{
    if (bp) {
        return bp->do_fastfill;
    } else {
        return FALSE;
    }
}

uint8_t
vqec_ifclient_bind_params_set_max_fastfill (vqec_bind_params_t *bp,
                                            const uint32_t max_fastfill)
{
    if (bp) {
        bp->max_fastfill = max_fastfill;
        return TRUE;
    } else {
        return FALSE;
    }
}

uint32_t
vqec_ifclient_bind_params_get_max_fastfill (const vqec_bind_params_t *bp)
{
    if (bp) {
        return bp->max_fastfill;
    } else {
        return 0;
    }
}

uint8_t
vqec_ifclient_bind_params_set_max_recv_bw_rcc (vqec_bind_params_t *bp,
                                               const uint32_t max_recv_bw_rcc)
{
    if (bp) {
        bp->max_recv_bw_rcc = max_recv_bw_rcc;
        return TRUE;
    } else {
        return FALSE;
    }
}

uint32_t
vqec_ifclient_bind_params_get_max_recv_bw_rcc (const vqec_bind_params_t *bp)
{
    if (bp) {
        return bp->max_recv_bw_rcc;
    } else {
        return 0;
    }
}

uint8_t
vqec_ifclient_bind_params_set_max_recv_bw_er (vqec_bind_params_t *bp,
                                              const uint32_t max_recv_bw_er)
{
    if (bp) {
        bp->max_recv_bw_er = max_recv_bw_er;
        return TRUE;
    } else {
        return FALSE;
    }
}

uint32_t
vqec_ifclient_bind_params_get_max_recv_bw_er (const vqec_bind_params_t *bp)
{
    if (bp) {
        return bp->max_recv_bw_er;
    } else {
        return 0;
    }
}

uint8_t
vqec_ifclient_bind_params_set_mrb_transition_cb (vqec_bind_params_t *bp,
                                                 void *transition_cb)
{
    if (bp) {
        bp->mrb_transition_notify = transition_cb;
        return TRUE;
    } else {
        return FALSE;
    }
}

void *
vqec_ifclient_bind_params_get_mrb_transition_cb (const vqec_bind_params_t *bp)
{
    if (bp) {
        return bp->mrb_transition_notify;
    } else {
        return (NULL);
    }
}

uint8_t
vqec_ifclient_bind_params_set_ir_time (vqec_bind_params_t *bp,
                                       uint64_t ir_time) 
{
    if (bp) {
        bp->ir_time_valid = TRUE;
        bp->ir_time = ir_time;
        return TRUE;
    } else {
        return FALSE;
    }
}

uint64_t
vqec_ifclient_bind_params_get_ir_time(const vqec_bind_params_t *bp, 
                                      boolean *ir_time_valid) 
{
    if (bp) {
        if (ir_time_valid) {
            *ir_time_valid = bp->ir_time_valid;
        }
        return bp->ir_time;
    } else {
        return 0;
    }
}

uint8_t
vqec_ifclient_bind_params_set_tr135_params (
                vqec_bind_params_t *bp,
                vqec_ifclient_tr135_params_t *tr135_params)
{
    if (bp && tr135_params) {
        memcpy(&bp->tr135_params, 
               tr135_params, 
               sizeof(vqec_ifclient_tr135_params_t));
        return TRUE;
    } else {
        return FALSE;
    }
}

vqec_ifclient_tr135_params_t *
vqec_ifclient_bind_params_get_tr135_params (const vqec_bind_params_t *bp) 
{
    if (bp) {
        return (vqec_ifclient_tr135_params_t *)&bp->tr135_params;
    } else {
        return NULL;
    }
}

//----------------------------------------------------------------------------
// Thread-safe wrappers for synchronized methods. 
// [These contain no implementation]
//----------------------------------------------------------------------------

vqec_error_t
vqec_ifclient_tuner_create (vqec_tunerid_t *id, const char *name) 
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_create_ul(id, name);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);
}

vqec_error_t 
vqec_ifclient_tuner_destroy (const vqec_tunerid_t id) 
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_destroy_ul(id);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);    
}

vqec_error_t 
vqec_ifclient_tuner_bind_chan (const vqec_tunerid_t id,
                               const vqec_sdp_handle_t sdp_handle,
                               const vqec_bind_params_t *bp)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_bind_chan_ul(id, sdp_handle, bp);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);    
}

vqec_error_t 
vqec_ifclient_tuner_bind_chan_cfg (const vqec_tunerid_t id,
                                   vqec_chan_cfg_t *cfg,
                                   const vqec_bind_params_t *bp) 
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_bind_chan_cfg_ul(id, cfg, bp);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);    
}

vqec_error_t 
vqec_ifclient_tuner_unbind_chan (const vqec_tunerid_t id)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_unbind_chan_ul(id);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);
}

vqec_sdp_handle_t
vqec_ifclient_alloc_sdp_handle_from_url (char *url) 
{
    vqec_sdp_handle_t sdp_handle;

    vqec_lock_lock(vqec_g_lock);
    sdp_handle = vqec_ifclient_alloc_sdp_handle_from_url_ul(url);
    vqec_lock_unlock(vqec_g_lock);        
    return sdp_handle;
}

//----------------------------------------------------------------------------
/// Free an sdp_handle 
//----------------------------------------------------------------------------
void
vqec_ifclient_free_sdp_handle (vqec_sdp_handle_t sdp_handle)
{
    vqec_lock_lock(vqec_g_lock);        
    vqec_ifclient_free_sdp_handle_ul(sdp_handle);
    vqec_lock_unlock(vqec_g_lock);        
}

uint8_t 
vqec_ifclient_chan_cfg_parse_sdp(vqec_chan_cfg_t *cfg,
                                 char *buffer,
                                 vqec_chan_type_t chan_type)
{
    uint8_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_chan_cfg_parse_sdp_ul(cfg, buffer, chan_type);
    vqec_lock_unlock(vqec_g_lock);

    return retval;
}

vqec_tunerid_t
vqec_ifclient_tuner_get_id_by_name (const char *namestr)
{
    vqec_tunerid_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_get_id_by_name_ul(namestr);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);    
}

uint8_t
vqec_ifclient_check_tuner_validity_by_name (const char *namestr)
{
    boolean retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_check_tuner_validity_by_name_ul(namestr);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);    
}

vqec_error_t
vqec_ifclient_tuner_get_name_by_id (vqec_tunerid_t id, char *name, 
                                    int32_t len)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_get_name_by_id_ul(id, name, len);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);
}

vqec_tunerid_t 
vqec_ifclient_tuner_iter_init_first (vqec_tuner_iter_t *iter)
{
    vqec_tunerid_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_iter_init_first_ul(iter);
    vqec_lock_unlock(vqec_g_lock);

    return (retval);
}

UT_STATIC vqec_tunerid_t
vqec_ifclient_tuner_iter_getnext (vqec_tuner_iter_t *iter)
{
    vqec_tunerid_t id;

    vqec_lock_lock(vqec_g_lock);
    id = vqec_ifclient_tuner_iter_getnext_ul(iter);
    vqec_lock_unlock(vqec_g_lock);
    
    return (id);
}

vqec_error_t vqec_ifclient_tuner_recvmsg (const vqec_tunerid_t id,
                                          vqec_iobuf_t *iobuf, 
                                          int32_t iobuf_num,
                                          int32_t *bytes_read,
                                          int32_t timeout)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_recvmsg_ul(id, iobuf, 
                                            iobuf_num, bytes_read, timeout);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);    
}

vqec_error_t vqec_ifclient_init (const char *filename)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_init_ul(filename);
    vqec_lock_unlock(vqec_g_lock);

    return (retval);
}

void vqec_ifclient_deinit (void)
{
    vqec_lock_lock(vqec_g_lock);
    vqec_ifclient_deinit_ul();
    vqec_lock_unlock(vqec_g_lock);
}

vqec_error_t vqec_ifclient_start (void)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_start_ul();
    vqec_lock_unlock(vqec_g_lock);

    return (retval);
}

void vqec_ifclient_stop (void)
{
    vqec_lock_lock(vqec_g_lock);
    vqec_ifclient_stop_ul();
    vqec_lock_unlock(vqec_g_lock);
}

boolean
vqec_ifclient_register_cname_ul(char *cname) 
{
    return vqec_syscfg_register_cname(cname);
}

boolean
vqec_ifclient_register_cname(char *cname)
{
    boolean retval;
    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_register_cname_ul(cname);
    vqec_lock_unlock(vqec_g_lock);
    return (retval);
}

void
vqec_ifclient_clear_stats (void) 
{
    vqec_lock_lock(vqec_g_lock);
    vqec_ifclient_clear_stats_ul();
    vqec_lock_unlock(vqec_g_lock);    
}

vqec_error_t
vqec_ifclient_tuner_dump_state (const vqec_tunerid_t id,
                                unsigned int options_flag)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_dump_state_ul(id, options_flag);
    vqec_lock_unlock(vqec_g_lock);    

    return (retval);
}

vqec_error_t 
vqec_ifclient_get_stats (vqec_ifclient_stats_t *stats)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_get_stats_ul(stats, FALSE);
    vqec_lock_unlock(vqec_g_lock);        

    return (retval);
}

vqec_error_t 
vqec_ifclient_get_stats_cumulative (vqec_ifclient_stats_t *stats)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_get_stats_ul(stats, TRUE);
    vqec_lock_unlock(vqec_g_lock);        

    return (retval);
}

vqec_error_t
vqec_ifclient_get_stats_tuner_legacy (vqec_tunerid_t id,
                                      vqec_ifclient_stats_channel_t *stats)
{
    vqec_error_t retval;
    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_get_stats_tuner_legacy_ul(id, stats);
    vqec_lock_unlock(vqec_g_lock);

    return (retval);
}

vqec_error_t 
vqec_ifclient_get_stats_channel (const char *url,
                                 vqec_ifclient_stats_channel_t *stats)
{
    vqec_error_t retval;
    
    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_get_stats_channel_ul(url, stats, FALSE);
    vqec_lock_unlock(vqec_g_lock);        

    return (retval);
}

vqec_error_t 
vqec_ifclient_get_stats_channel_cumulative (const char *url,
                                              vqec_ifclient_stats_channel_t 
                                              *stats)
{
    vqec_error_t retval;
    
    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_get_stats_channel_ul(url, stats, TRUE);
    vqec_lock_unlock(vqec_g_lock);        

    return (retval);
}

vqec_error_t
vqec_ifclient_get_stats_channel_tr135_sample (const char *url, 
                              vqec_ifclient_stats_channel_tr135_sample_t *stats)
{
    vqec_error_t retval;
    
    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_get_stats_channel_tr135_sample_ul(url, stats);
    vqec_lock_unlock(vqec_g_lock);        
    return (retval);    
}

vqec_error_t 
vqec_ifclient_set_tr135_params_channel (const char *url,
                                        vqec_ifclient_tr135_params_t *params)
{
    vqec_error_t retval;
    
    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_set_tr135_params_channel_ul(url, params);
    vqec_lock_unlock(vqec_g_lock);        

    return (retval);
}

vqec_error_t
vqec_ifclient_histogram_display (const vqec_hist_t hist)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_histogram_display_ul(hist);
    vqec_lock_unlock(vqec_g_lock);    

    return (retval);
}

vqec_error_t
vqec_ifclient_histogram_clear (const vqec_hist_t hist)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_histogram_clear_ul(hist);
    vqec_lock_unlock(vqec_g_lock);    

    return (retval);
}

vqec_error_t 
vqec_ifclient_updater_get_stats (vqec_ifclient_updater_stats_t *stats)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_updater_get_stats_ul(stats);
    vqec_lock_unlock(vqec_g_lock);

    return (retval);
}

/*
 * Returns whether or not VQE-C Core Services are enabled.
 * "enabled" means that VQE-C Core Services initialization
 * was attempted and completed successfully.
 *
 * @param[out] boolean     - TRUE:  VQE-C is enabled/initialized
 *                           FALSE: otherwise
 */
uint8_t
vqec_ifclient_is_vqec_enabled_ul (void)
{
    return (s_vqec_ifclient_state >= VQEC_IFCLIENT_INITED ? TRUE : FALSE);
}

uint8_t
vqec_ifclient_is_vqec_enabled (void)
{
    uint8_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_is_vqec_enabled_ul();
    vqec_lock_unlock(vqec_g_lock);

    return (retval);
}

uint8_t 
vqec_ifclient_tuner_is_fastfill_enabled (vqec_tunerid_t id)
{
    uint8_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_is_fastfill_enabled_ul(id);
    vqec_lock_unlock(vqec_g_lock);

    return (retval);
}

uint8_t 
vqec_ifclient_tuner_is_rcc_enabled (vqec_tunerid_t id)
{
    uint8_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_is_rcc_enabled_ul(id);
    vqec_lock_unlock(vqec_g_lock);

    return (retval);
}

uint8_t 
vqec_ifclient_socket_open (vqec_in_addr_t *addr,
                           vqec_in_port_t *rtp_port,
                           vqec_in_port_t *rtcp_port)
{
    uint8_t retval;
    
    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_socket_open_ul(addr, rtp_port, rtcp_port);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);
}

void 
vqec_ifclient_socket_close (vqec_in_addr_t addr,
                            vqec_in_port_t rtp_port,
                            vqec_in_port_t rtcp_port)
{
    vqec_lock_lock(vqec_g_lock);
    vqec_ifclient_socket_close_ul(addr, rtp_port, rtcp_port);
    vqec_lock_unlock(vqec_g_lock);
}

//----------------------------------------------------------------------------
// Create a new tuner.
//----------------------------------------------------------------------------
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_create_ul (vqec_tunerid_t *id, const char *name) 
{
    vqec_error_t err;
    char cbuf[VQEC_MAX_TUNER_NAMESTR_LEN];

    err = vqec_tuner_create(id, name);
    switch (err) {
    case VQEC_OK:
        if (vqec_info_logging()) {
            snprintf(s_log_str_large, VQEC_LOGMSG_BUFSIZE_LARGE,
                     "%s(name=\"%s\"): Created tuner %d name=\"%s\"",
                     __FUNCTION__,
                     name ? name : "",
                     *id,
                     vqec_tuner_get_name(*id));
            syslog_print(VQEC_INFO, s_log_str_large);
        }
        break;
    case VQEC_ERR_EXISTTUNER:
        /* Log this error case separately to include the duplicate name */
        snprintf(cbuf, VQEC_MAX_TUNER_NAMESTR_LEN, "%s", name);
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s %s (name %s exists)", 
                              __FUNCTION__, vqec_err2str(err), cbuf);
        break;
    default:
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                              __FUNCTION__, vqec_err2str(err));
        break;
    }

    return (err);
}

//----------------------------------------------------------------------------
// Destroy an existing tuner.
//----------------------------------------------------------------------------
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_destroy_ul (const vqec_tunerid_t id)
{
    vqec_error_t err;

    err = vqec_tuner_destroy(id);
    if (err == VQEC_OK) {
        if (vqec_info_logging()) {
            snprintf(s_log_str_large, VQEC_LOGMSG_BUFSIZE_LARGE,
                     "%s(id=%d)",
                     __FUNCTION__, id);
            syslog_print(VQEC_INFO, s_log_str_large);
        }
    } else {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }

    return (err);
}

//----------------------------------------------------------------------------
// Bind a tuner to a channel.
//----------------------------------------------------------------------------
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_bind_chan_ul (const vqec_tunerid_t id,
                                  const vqec_sdp_handle_t sdp_handle,
                                  const vqec_bind_params_t *bp)
{
    vqec_error_t err = VQEC_OK;
    channel_cfg_t *sdp_cfg;
    vqec_chan_cfg_t chan_cfg;
    uint32_t i;
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();
    vqec_ifclient_chan_event_cb_t *chan_event_cb = NULL;

    if (!vqec_ifclient_is_vqec_enabled_ul()) {
        return (VQEC_ERR_DISABLED);
    }

    memset(&chan_cfg, 0, sizeof(vqec_chan_cfg_t));

    /* compare first part of string to determine if dynamic or SDP channel */
    if (sdp_handle &&
        !strncmp(sdp_handle,
                 VQEC_IFCLIENT_SDP_HANDLE_TMP,
                 strlen(VQEC_IFCLIENT_SDP_HANDLE_TMP)) &&
        (strlen(sdp_handle) == 
         strlen(VQEC_IFCLIENT_SDP_HANDLE_TMP) + 2)) {
        /* dynamic channel - find the correct one */
        for (i = 0; i < syscfg->max_tuners; i++) {
            if (!strncmp(sdp_handle,
                         s_vqec_ifclient_tmp_chans[i].sdp_handle,
                         VQEC_IFCLIENT_SDP_HANDLE_LEN)) {
                break;
            }
        }
        if (i == syscfg->max_tuners) {
            /* no dynamic channel matches sdp_handle provided */
            return (VQEC_ERR_CHANNELLOOKUP);
        }

        /* copy the channel cfg from tmp chan db*/
        memcpy(&chan_cfg, &(s_vqec_ifclient_tmp_chans[i].chan_cfg),
               sizeof(vqec_chan_cfg_t));

        /* chan event cb */
        if (s_vqec_ifclient_tmp_chans[i].chan_event_cb.chan_event_cb) {
            chan_event_cb = &(s_vqec_ifclient_tmp_chans[i].chan_event_cb);
        }

        snprintf(chan_cfg.name,
                 VQEC_MAX_CHANNEL_NAME_LENGTH,
                 "<non-VQE %s channel>",
                 (chan_cfg.protocol == VQEC_PROTOCOL_RTP ?
                  "RTP" : "UDP"));
        chan_cfg.passthru = TRUE;

        if ((chan_cfg.protocol == VQEC_PROTOCOL_RTP) && chan_cfg.er_enable) {
            chan_cfg.passthru = FALSE;
        }
    } else {
        /* SDP channel */
        sdp_cfg = cfg_get_channel_cfg_from_SDP_hdl(sdp_handle);
        if (!sdp_cfg) {
            syslog_print(VQEC_ERROR, "channel change to invalid channel");
            return VQEC_ERR_CHANNELLOOKUP;
        }
        vqec_chan_convert_sdp_to_cfg(sdp_cfg, VQEC_CHAN_TYPE_LINEAR, &chan_cfg);
    }

    err = vqec_ifclient_tuner_bind_chan_cfg_internal(id, &chan_cfg, bp, 
                                                     chan_event_cb);
    if (err == VQEC_OK) {
        if (vqec_info_logging()) {
            snprintf(s_log_str_large, VQEC_LOGMSG_BUFSIZE_LARGE,
                     "%s(id=%d, sdp_handle=\"%s\", bp=%s)",
                     __FUNCTION__,
                     id,
                     sdp_handle ? sdp_handle : "",
                     vqec_ifclient_bp_to_str(bp));
            syslog_print(VQEC_INFO, s_log_str_large);
        }
    }

    return (err);
}


/*----------------------------------------------------------------------------
 * Bind a tuner to channel (specified by channel parameters).
 *--------------------------------------------------------------------------*/
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_bind_chan_cfg_internal (
    const vqec_tunerid_t id,
    vqec_chan_cfg_t *cfg,
    const vqec_bind_params_t *bp,
    const vqec_ifclient_chan_event_cb_t *chan_event_cb)
{
    vqec_error_t err = VQEC_OK;
    vqec_chanid_t chanid_cur, chanid_new;
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();

    if (!vqec_ifclient_is_vqec_enabled_ul()) {
        return (VQEC_ERR_DISABLED);
    }

    if (!cfg) {
        syslog_print(VQEC_ERROR, "channel change with null chan cfg");
        return VQEC_ERR_INVALIDARGS;
    }
    if (!syscfg->udp_passthru_support && cfg->protocol == VQEC_PROTOCOL_UDP) {
        syslog_print(VQEC_ERROR, "UDP channels not supported "
                     "(check setting of \"udp_passthru_support\")");
        return VQEC_ERR_INVALIDARGS;
    }

    /* assume RTP for now; will autodetect the correct protocol in dpchan */
    cfg->protocol = VQEC_PROTOCOL_RTP;

    /* stun optimization flag */
    cfg->stun_optimization = syscfg->stun_optimization;

    if (vqec_chan_get_chanid(&chanid_new, cfg) != VQEC_CHAN_ERR_OK) {
        syslog_print(VQEC_ERROR, "channel change ID retrieval failure");
        return VQEC_ERR_INTERNAL;
    }
    if ((vqec_tuner_get_chan(id, &chanid_cur) == VQEC_OK) &&
        (chanid_cur == chanid_new)) {
        VQEC_DEBUG(VQEC_DEBUG_TUNER, "Trying to change to the same channel\n");
        return VQEC_ERR_DUPLICATECHANNELTUNEREQ;
    }

    err = vqec_tuner_bind_chan(id, chanid_new, bp, chan_event_cb);
    switch (err) {
    case VQEC_ERR_NOSUCHTUNER:
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                              __FUNCTION__, vqec_err2str(err));
        break;
    default:
        if (err != VQEC_OK) {
            vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                                  "%s %s (tuner set chan)",
                                  __FUNCTION__, vqec_err2str(err));
        }
        break;
    }
    if (err != VQEC_OK) {
        (void)vqec_chan_put_chanid(chanid_new);
    }
    return (err);
}

/*----------------------------------------------------------------------------
 * Bind a tuner to channel (specified by channel parameters).
 *--------------------------------------------------------------------------*/
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_bind_chan_cfg_ul (const vqec_tunerid_t id,
                                      vqec_chan_cfg_t *cfg,
                                      const vqec_bind_params_t *bp)
{
    vqec_error_t err;

    err = vqec_ifclient_tuner_bind_chan_cfg_internal(id, cfg, bp, NULL);

    if (err == VQEC_OK) {
        if (vqec_info_logging()) {
            snprintf(s_log_str_large, VQEC_LOGMSG_BUFSIZE_LARGE,
                     "%s(id=%d, cfg={%s}, bp=%s)",
                     __FUNCTION__,
                     id,
                     vqec_ifclient_chan_cfg_to_str(cfg),
                     vqec_ifclient_bp_to_str(bp));
            syslog_print(VQEC_INFO, s_log_str_large);
        }
    }
    
    return (err);
}




/*----------------------------------------------------------------------------
 * Update the configuration for an active channel.
 *--------------------------------------------------------------------------*/
UT_STATIC vqec_error_t 
vqec_ifclient_chan_update_ul (const vqec_chan_cfg_t *cfg)
{
    vqec_chanid_t chanid;
    vqec_chan_err_t err_chan;
    vqec_error_t err = VQEC_OK;

    if (!cfg) {
        err = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    /* Verify the channel is unicast */
    if (IN_MULTICAST(ntohl(cfg->primary_dest_addr.s_addr))) {
        err = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    /* Look up channel in active channel database */
    err_chan = vqec_chan_find_chanid(&chanid, 
                                     cfg->primary_dest_addr.s_addr,
                                     cfg->primary_dest_port);
    switch (err_chan) {
    case VQEC_CHAN_ERR_NOTFOUND:
        err = VQEC_ERR_CHANNOTACTIVE;
        goto done;
    case VQEC_CHAN_ERR_OK:
        break;
    default:
        err = VQEC_ERR_INTERNAL;
        goto done;
    }

    /* Pass the caller's parameters for updating the channel source cfg */
    err_chan = vqec_chan_update_source(chanid, cfg);
    switch (err_chan) {
    case VQEC_CHAN_ERR_NOTFOUND:
        /* Non-existent channel should previously be detected */
        err = VQEC_ERR_INTERNAL;
        break;
    case VQEC_CHAN_ERR_INVALIDARGS:
        err = VQEC_ERR_INVALIDARGS;
        break;
    case VQEC_CHAN_ERR_OK:
        break;
    default:
        err = VQEC_ERR_INTERNAL;
        break;
    }
    
done:
    if (err == VQEC_OK) {
        if (vqec_info_logging()) {
            snprintf(s_log_str_large, VQEC_LOGMSG_BUFSIZE_LARGE,
                     "%s(cfg={%s})",
                     __FUNCTION__,
                     vqec_ifclient_chan_cfg_to_str(cfg));
            syslog_print(VQEC_INFO, s_log_str_large);
        }
    } else {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }
    return (err);
}

vqec_error_t 
vqec_ifclient_chan_update (const vqec_chan_cfg_t *cfg,
                           const vqec_update_params_t *up)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_chan_update_ul(cfg);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);    
}

/*----------------------------------------------------------------------------
 * Update the nat binding for an active channel.
 *--------------------------------------------------------------------------*/
UT_STATIC vqec_error_t 
vqec_ifclient_nat_binding_update_ul (in_addr_t prim_dest_ip, 
                                     in_port_t prim_dest_port)
{
    vqec_chanid_t chanid;
    vqec_chan_err_t err_chan;
    vqec_error_t err = VQEC_OK;

    if (!prim_dest_ip || !prim_dest_port) {
        err = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    /* Look up channel in active channel database */
    err_chan = vqec_chan_find_chanid(&chanid, 
                                     prim_dest_ip,
                                     prim_dest_port);
    switch (err_chan) {
    case VQEC_CHAN_ERR_NOTFOUND:
        err = VQEC_ERR_CHANNOTACTIVE;
        goto done;
    case VQEC_CHAN_ERR_OK:
        break;
    default:
        err = VQEC_ERR_INTERNAL;
        goto done;
    }

    /* Pass the caller's parameters for updating  */
    err_chan = vqec_chan_update_nat_binding(chanid);
    switch (err_chan) {
    case VQEC_CHAN_ERR_NOTFOUND:
        /* Non-existent channel should previously be detected */
        err = VQEC_ERR_INTERNAL;
        break;
    case VQEC_CHAN_ERR_INVALIDARGS:
        err = VQEC_ERR_INVALIDARGS;
        break;
    case VQEC_CHAN_ERR_OK:
        break;
    default:
        err = VQEC_ERR_INTERNAL;
        break;
    }
    
done:
    if (err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }
    return (err);
}

vqec_error_t 
vqec_ifclient_nat_binding_update (in_addr_t prim_dest_ip, 
                                  in_port_t prim_dest_port)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_nat_binding_update_ul(prim_dest_ip, 
                                                 prim_dest_port);
    vqec_lock_unlock(vqec_g_lock);

    return (retval);
}


//----------------------------------------------------------------------------
// Unbind a tuner from a channel.
//----------------------------------------------------------------------------
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_unbind_chan_ul (const vqec_tunerid_t id)
{
    vqec_error_t err;

    err = vqec_tuner_unbind_chan(id);

    if (err == VQEC_OK) {
        if (vqec_info_logging()) {
            snprintf(s_log_str_large, VQEC_LOGMSG_BUFSIZE_LARGE,
                     "%s(id=%d)",
                     __FUNCTION__,
                     id);
            syslog_print(VQEC_INFO, s_log_str_large);        
        }
    } else {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                              __FUNCTION__, vqec_err2str(err));
    }

    return (err);
}

vqec_sdp_handle_t
vqec_ifclient_alloc_sdp_handle_from_url_ul (char *url) 
{
    vqec_protocol_t protocol;
    in_addr_t ip;
    uint16_t port;
    struct in_addr ip_addr;
    vqec_sdp_handle_t sdp_handle;
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();
    uint32_t i;

    if (!vqec_ifclient_is_vqec_enabled_ul()) {
        return (NULL);
    }

    if (vqec_url_parse(url, &protocol, &ip, &port)) {
        ip_addr.s_addr = ip;
        switch (protocol) {
        case VQEC_PROTOCOL_RTP:
            sdp_handle = cfg_alloc_SDP_handle_from_orig_src_addr(RTP_STREAM,
                                                                 ip_addr,
                                                                 port);
            break;
        case VQEC_PROTOCOL_UDP:
            sdp_handle = cfg_alloc_SDP_handle_from_orig_src_addr(UDP_STREAM, 
                                                                 ip_addr,
                                                                 port);
            break;
        case VQEC_PROTOCOL_UNKNOWN:
        default:
            sdp_handle = NULL;
            break;
        }
        
        if (sdp_handle) {
            return (sdp_handle);
        } else if ((syscfg->integrated_rtp_fallback &&
                    protocol == VQEC_PROTOCOL_RTP) ||
                   (syscfg->udp_passthru_support &&
                    protocol == VQEC_PROTOCOL_UDP)) {
            /* trigger an update only in RTP case, to avoid excess updates */
            if (protocol == VQEC_PROTOCOL_RTP) {
                VQEC_DEBUG(VQEC_DEBUG_CHAN_CFG,
                           "triggering background channel config update\n");
                vqec_updater_update(FALSE);
            }

            /**
             * first check if the channel is in temp chan DB
             * port already in network order 
             */
            sdp_handle = vqec_ifclient_get_tmp_chan_sdp_handle(ip, port);
            if (sdp_handle) {
                return sdp_handle;
            }

            /* construct temporary channel structure from URL */
            for (i = 0; i < syscfg->max_tuners; i++) {
                if (s_vqec_ifclient_tmp_chans[i].sdp_handle[0] == '\0') {
                    break;
                }
            }
            if (i == syscfg->max_tuners) {
                syslog_print(VQEC_ERROR,
                             "too many dynamic channels allocated");
                return (NULL);
            }
            s_vqec_ifclient_tmp_chans[i].chan_cfg.protocol = protocol;
            s_vqec_ifclient_tmp_chans[i].chan_cfg.primary_dest_addr.s_addr = ip;
            s_vqec_ifclient_tmp_chans[i].chan_cfg.primary_dest_port = port;
            /* looks like:  "o=tmpchan01" */
            snprintf(s_vqec_ifclient_tmp_chans[i].sdp_handle,
                     VQEC_IFCLIENT_SDP_HANDLE_LEN,
                     VQEC_IFCLIENT_SDP_HANDLE_TMP "%02u",
                     i);
            return (s_vqec_ifclient_tmp_chans[i].sdp_handle);
        }
    }
    return (NULL);
}

//----------------------------------------------------------------------------
/// Free an sdp_handle 
//----------------------------------------------------------------------------
void
vqec_ifclient_free_sdp_handle_ul (vqec_sdp_handle_t sdp_handle)
{
    uint32_t i;
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();

    if (!sdp_handle) {
        return;
    }

    if (!strncmp(sdp_handle,
                 VQEC_IFCLIENT_SDP_HANDLE_TMP,
                 strlen(VQEC_IFCLIENT_SDP_HANDLE_TMP)) &&
        (strlen(sdp_handle) == 
         strlen(VQEC_IFCLIENT_SDP_HANDLE_TMP) + 2)) {
        /* dynamic channel - find the correct one */
        for (i = 0; i < syscfg->max_tuners; i++) {
            if (!strncmp(sdp_handle,
                         s_vqec_ifclient_tmp_chans[i].sdp_handle,
                         VQEC_IFCLIENT_SDP_HANDLE_LEN)) {
                break;
            }
        }
        if (i == syscfg->max_tuners) {
            /* no dynamic channel matches sdp_handle provided */
            return;
        }
        memset(&s_vqec_ifclient_tmp_chans[i], 0,
               sizeof(vqec_ifclient_tmp_chan_t));
    } else if (sdp_handle[0] == '\0') {
        /* dynamic channel that's already been freed */
        return;
    } else {
        cfg_free_SDP_handle(sdp_handle);
    }
}

/*----------------------------------------------------------------------------
 * Parse a single SDP into a vqec_chan_cfg_t structure
 *--------------------------------------------------------------------------*/
uint8_t 
vqec_ifclient_chan_cfg_parse_sdp_ul(vqec_chan_cfg_t *cfg,
                                    char *buffer,
                                    vqec_chan_type_t chan_type) 
{
    channel_cfg_t sdp_cfg;
    cfg_ret_e ret;

    if (!cfg || !buffer) {
        return FALSE;
    }

    ret = cfg_parse_single_channel_data(buffer, 
                                        &sdp_cfg, 
                                        (cfg_chan_type_e) chan_type);
    if (ret != CFG_SUCCESS) {
        return FALSE;
    }

    vqec_chan_convert_sdp_to_cfg(&sdp_cfg, chan_type, cfg);

    return TRUE;
}

/*--------------------------------------------------
 * Check if the tuner with this name exists
 *--------------------------------------------------
 */
boolean vqec_ifclient_check_tuner_validity_by_name_ul(const char *namestr)
{
    if (vqec_tuner_get_id_by_name(namestr) == VQEC_TUNERID_INVALID) {
        return (FALSE);
    }

    return (TRUE);
}

//----------------------------------------------------------------------------
// Retrieve a tuner's id by it's name.
//----------------------------------------------------------------------------
UT_STATIC vqec_tunerid_t 
vqec_ifclient_tuner_get_id_by_name_ul (const char *namestr)
{
    vqec_tunerid_t id;

    id = vqec_tuner_get_id_by_name(namestr);
    if (id == VQEC_TUNERID_INVALID) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                              __FUNCTION__,
                              vqec_err2str(VQEC_ERR_NOSUCHTUNER));        
    }

    return (id);
}

//----------------------------------------------------------------------------
// Retrieve a tuner's name by it's id.
//----------------------------------------------------------------------------
vqec_error_t 
vqec_ifclient_tuner_get_name_by_id_ul (vqec_tunerid_t id, char *name, 
                                       int32_t len)
{
    vqec_error_t err = VQEC_OK;
    const char *tuner_name;

    if (!name || (len <= 0)) {
        err = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    tuner_name = vqec_tuner_get_name(id);
    if (!tuner_name) {
        err = VQEC_ERR_NOSUCHTUNER;
        goto done;
    }

    /*
     * Copy the tuner's name into the provided buffer, NULL-terminating
     * the result.
     */
    (void)strlcpy(name, tuner_name, len);

done:
    if (err != VQEC_OK) {
        if (len > 0) {
            name[0] = '\0';
        }
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                              __FUNCTION__, vqec_err2str(err));  
    }
    return (err);
}

//----------------------------------------------------------------------------
// Initialize iterator to walk all valid tuner-ids.
// The iterator macro (FOR_ALL_TUNERS_..) is based on the two underlying
// functions: tuner_iter_init_first(..) returns the first valid tuner index
// in the tuner array, and sets the getnext pointer in the iterator to 
// the tuner_iter_getnext(..) function. Therefore, calling iter->getnext() 
// will return the next valid tuner index in the tuner array. The global
// lock is acquired before the tuner array is accessed, to ensure thread
// safety. The tuner id's returned were valid as of the time either 
// init_first() or getnext() were executed. Subsequent operations on these
// id's may succeed if the id is still valid at the time the operation
// is invoked.
//----------------------------------------------------------------------------
UT_STATIC vqec_tunerid_t 
vqec_ifclient_tuner_iter_init_first_ul (vqec_tuner_iter_t *iter)
{
    if (!iter) {
        return (VQEC_TUNERID_INVALID);
    }

    memset(iter, 0, sizeof(*iter));
    iter->i_cur = vqec_tuner_get_first_tuner_id();
    iter->getnext = vqec_ifclient_tuner_iter_getnext;

    return (iter->i_cur);
}

UT_STATIC vqec_tunerid_t
vqec_ifclient_tuner_iter_getnext_ul (vqec_tuner_iter_t *iter)
{
    if (!iter) {
        return (VQEC_TUNERID_INVALID);
    }

    iter->i_cur = vqec_tuner_get_next_tuner_id(iter->i_cur);
    return (iter->i_cur);
}

/*----------------------------------------------------------------------------
 * Validate and decapsulate an RTP packet.
 *--------------------------------------------------------------------------*/
vqec_error_t vqec_ifclient_rtp_hdr_parse (char *buf,
                                          uint16_t len,
                                          vqec_rtphdr_t *rtp_hdr)
{
    rtp_hdr_session_t rtp_session;
    rtp_hdr_status_t rtp_status;
    rtpfasttype_t *rtpbuf;
    vqec_error_t ret = VQEC_ERR_BADRTPHDR;

    if (!buf || !len || !rtp_hdr) {
        return ret;
    }

    memset(&rtp_session, 0, sizeof(rtp_hdr_session_t));
    memset(rtp_hdr, 0, sizeof(*rtp_hdr));

    rtp_status = rtp_validate_hdr(&rtp_session, buf, len);

    if (rtp_hdr_ok(rtp_status)) {
        rtpbuf = (rtpfasttype_t *)buf;

        rtp_hdr->combined_bits = ntohs(rtpbuf->combined_bits);
        rtp_hdr->sequence = ntohs(rtpbuf->sequence); /* network byte-order */
        rtp_hdr->timestamp = ntohl(rtpbuf->timestamp);
        rtp_hdr->ssrc = ntohl(rtpbuf->ssrc);
        rtp_hdr->header_bytes = RTPHEADERBYTES(rtpbuf);
        rtp_hdr->rtp_payload = (uint8_t *)buf + RTPHEADERBYTES(rtpbuf);

        ret = VQEC_OK;
    }

    return ret;
}

/*----------------------------------------------------------------------------
 * Explicitly trigger a background VQEC update.
 *--------------------------------------------------------------------------*/
void vqec_ifclient_trigger_update (void)
{
    vqec_updater_update(TRUE);
}

//----------------------------------------------------------------------------
// Receive datagrams.
// (a) pthread_... calls: This function relies on the use several pthread_... 
// library calls. The meaning, and use of these calls is described below:
//
// pthread_once(pthread_once_t once_ctl, void (*init_routine)(void): 
// The first call to pthread_once() by any thread with a given "once_ctl" 
// calls init_routine; subsequent calls to pthread_once() do not call the
// init routine. The special once_ctl constant PTHREAD_ONCE_INIT is used 
// as the first argument.
//
// pthread_key_create(pthread_key_t *key, (void *)(*destructor)(void)):
// Creates a thread-specific data key which is visible to all threads in
// the process. Although, the same data key is visible to all threads, the
// data values bound to the key by pthread_setspecific() are maintained
// on a per-thread basis, and exist for the lifetime of the thread. Upon
// creation the value NULL is associated with the key for all active
// threads, and in a newly created thread all existing keys will have NULL
// associated values. A destructor can be associated with each key, and
// upon exit of each thread, if a key has a non-null destructor, and the
// thread has a non-NULL value associated with the key, the destructor is
// called is called with the key's associated value.
//
// pthread_getspecific(pthread_key_t *key):
// This function returns the current value bound to the specified key on
// behalf of the calling thread.
//
// pthread_setspecific(pthread_key_t *key, const void *value):
// This function associates a thread-specific value with the specified key.
// The key must have been created with pthread_key_create. If the key has
// been deleted prior to calling this function, it's behavior is undefined. 
//
// (b) The flow of the code below is as follows: If there are packets
// on the sink queue, first copy them into the iobuf's. Else, if a timeout
// is specified, enQ the user-specified list of iobuf's as a "waiter"
// data structure on the sink. Packets will be copied into these iobuf's
// as the sink receives them from the pcm. The thread will be woken up
// once iobuf_num packets have been received, or the timeout expires.
//----------------------------------------------------------------------------
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_recvmsg_ul (const vqec_tunerid_t id,
                                vqec_iobuf_t *iobuf, 
                                int32_t iobuf_num,
                                int32_t *bytes_read,
                                int32_t timeout)
{
    vqec_error_t err = VQEC_OK;
    vqec_dp_error_t ret;

#define MAX_CMSG_BUF_SIZE 1024

    int sockfd;
    struct msghdr   msg;
    struct sockaddr_in saddr;
    struct iovec vec;
    char ctl_buf[MAX_CMSG_BUF_SIZE];
    int cur_buf = 0;
    int len = 0, i;
    struct pollfd fds;
    int pollret;
    abs_time_t timeout_end = ABS_TIME_0;
    uint64_t timeout_64b = timeout;

    if (s_vqec_ifclient_state == VQEC_IFCLIENT_UNINITED) {
        err = VQEC_ERR_INVCLIENTSTATE;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                              __FUNCTION__, vqec_err2str(err));
        return (err);
    }

    if (g_vqec_client_mode == VQEC_CLIENT_MODE_KERN) {
        /* delivery of paks to user-mode is disabled. */
        if (!s_vqec_ifclient_deliver_paks_to_user) {
            return (VQEC_ERR_NO_RPC_SUPPORT);
        }

        /* initialize everything before reading any packets */
        *bytes_read = 0;
        for (i=0; i < iobuf_num; i++) {
            iobuf[i].buf_wrlen = 0;
            iobuf[i].buf_flags = 0;
        }

        /* get cached output socket fd */
        sockfd = vqec_tuner_get_output_sock_fd(id);
        if (sockfd == VQEC_DP_OUTPUT_SHIM_INVALID_FD) {
            return (VQEC_ERR_NOBOUNDCHAN);
        }

        vec.iov_base = (char *)iobuf[cur_buf].buf_ptr
            + iobuf[cur_buf].buf_wrlen;
        vec.iov_len = iobuf[cur_buf].buf_len
            - iobuf[cur_buf].buf_wrlen;

        memset(&msg, 0, sizeof(msg));
 
        msg.msg_name = &saddr;
        msg.msg_namelen = sizeof(saddr);
        msg.msg_iov = &vec;
        msg.msg_iovlen = 1;

        msg.msg_control = ctl_buf;
        msg.msg_controllen = sizeof(ctl_buf);    

        /* compute absolute timeout for read */
        if (timeout > 0) {
            timeout_end = TIME_ADD_A_R(get_sys_time(), 
                                       TIME_MK_R(msec, timeout_64b));
        }

#define GET_TIMEOUT_LEFT    \
        TIME_GET_R(msec, (TIME_SUB_A_A(timeout_end, get_sys_time())))

        /* call recvmsg() and copy datagram into iobuf */
        while ((cur_buf < iobuf_num) || !len) {

            len = recvmsg(sockfd, &msg, 0);

            if (len <= 0) {
                if (timeout <= 0) {
                    break;
                }

                /* nothing ready to be read; release lock and poll sock */
                vqec_lock_unlock(vqec_g_lock);
                fds.fd = sockfd;
                fds.events = POLLIN;
                pollret = poll(&fds, 1, GET_TIMEOUT_LEFT);

                /* re-acquire the lock and process the results of poll */
                vqec_lock_lock(vqec_g_lock);

                if (pollret == -1) {
                    /* error with socket */
                    syslog_print(VQEC_ERROR, "error in polling socket");
                    err = VQEC_ERR_INTERNAL;
                    break;
                } else if ((pollret > 0) &&
                           (fds.revents & POLLIN) &&
                           !(fds.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                    /* data ready to be read; go back and recvmsg() */
                    /* check to make sure the tuner's sockfd is still the same */
                    if (sockfd != vqec_tuner_get_output_sock_fd(id)) {
                        /* tuner may have been destroyed/changed */
                        err = VQEC_ERR_NOSUCHTUNER;
                        break;
                    }
                    continue;
                }

                /* poll() timed out */
                break;

            } else {
                iobuf[cur_buf].buf_wrlen = len;
            }

            cur_buf++;
            *bytes_read += len;
            vec.iov_base = (char *)iobuf[cur_buf].buf_ptr
                + iobuf[cur_buf].buf_wrlen;
            vec.iov_len = iobuf[cur_buf].buf_len
                - iobuf[cur_buf].buf_wrlen;
        }

        return (err);
    }

    /* 
     * In user-space, this function is simply a wrapper for the output_shim's
     * tuner_read_one function.
     */
    ret = vqec_dp_output_shim_tuner_read(
        vqec_tuner_get_dptuner(id),
        iobuf,
        (uint32_t)iobuf_num,
        (uint32_t *)bytes_read,
        timeout);

    /* convert dp_error to vqec_error */
    switch (ret) {
        case VQEC_DP_ERR_OK:
            err = VQEC_OK;
            break;
        case VQEC_DP_ERR_NOSUCHTUNER:
            err = VQEC_ERR_NOSUCHTUNER;
            break;
        case VQEC_DP_ERR_NOSUCHSTREAM:
            err = VQEC_ERR_NOBOUNDCHAN;
            break;
        case VQEC_DP_ERR_INVALIDARGS:
            err = VQEC_ERR_INVALIDARGS;
            break;
        case  VQEC_DP_ERR_INTERNAL:
            err = VQEC_ERR_SYSCALL;
            break;
        case VQEC_DP_ERR_NOMEM:
            err = VQEC_ERR_MALLOC;
            break;
        case VQEC_DP_ERR_NO_RPC_SUPPORT:
            err = VQEC_ERR_NO_RPC_SUPPORT;
            break;
        default:
            err = VQEC_ERR_UNKNOWN;
            syslog_print(VQEC_ERROR, "invalid return value from "
                         "vqec_dp_output_shim_tuner_read()");
            break;
    }

    return (err);
}

//----------------------------------------------------------------------------
// Initialize the client library.
// s_vqec_keepalive_ev is used to keep the event loop from exiting, since
// if there are no events enQ'ed the loop it exits; keepalive_handler(..)
// is an empty handler for this event.
//----------------------------------------------------------------------------

UT_STATIC void *vqec_cli_thread (void* arg)
{
#define VQEC_CLI_IFNAME_LEN 5
    int16_t port;
    in_addr_t if_addr;
    vqec_syscfg_t v_cfg;
    char temp[INET_ADDRSTRLEN];

    vqec_syscfg_get(&v_cfg);

    port = v_cfg.cli_telnet_port;

    /* get CLI interface address */
    if (!strncmp(v_cfg.cli_ifname, "*", VQEC_CLI_IFNAME_LEN)) {
        if_addr = INADDR_ANY;
    } else {
        /* use the specified interface name */
        if (get_ip_address_by_if(v_cfg.cli_ifname, 
                                 temp,
                                 INET_ADDRSTRLEN)) {
            if_addr = inet_addr(temp);
        } else {
            /* fallback to loopback only */
            if (get_ip_address_by_if("lo", temp, INET_ADDRSTRLEN)) {
                if_addr = inet_addr(temp);
            } else {
                /* fallback to disabling the CLI completely */
                if_addr = INADDR_ANY;
                port = 0;
            }
        }
    }

    /* sa_ignore {Ignoring return value} IGNORE_RETURN(1) */
    vqec_cli_loop_wrapper(port, if_addr);
    return (NULL);
}

UT_STATIC void
vqec_ifclient_keepalive_handler (const vqec_event_t *const evptr, 
                                 int fd, short event, void *dptr)
{
    return;
}

//----------------------------------------------------------------------------
// Why the timeout for keepalive_handler is 20 msec?
// Channel changes occur from a thread different than the libevent thread, 
// i.e., tuners are bound, unbound and then bound again from a control thread.
// However, if the libevent thread is sleeping on a epoll/poll/select,etc.
// for some socket descriptors with a particular timeout, then the new 
// timer event additions to libevent from the control thread wouldn't 
// become "active", unless the libevent thread returns from that system 
// call [for epoll socket descriptors are immediately added to the kernel
// queue, but for poll & select the socket descriptors will be added/polled  
// for once the system-call that the libevent thread is blocked on returns.
// We use a timer-driven mechanism to poll data-sockets, [rtcp socket
// descriptors are polled, but they have sparse traffic]. When there are no
// tuners bound to a channel [particularly in STD mode], the only "timer"  
// event that's enQ'ed onto libevent is the keepalive_handler above, and there. 
// are no socket-based events polled. If libevent sleeps at the poll/epoll/...
// system call with a timeout of e.g., 1 sec, "notification" of new timer
// events [i.e., events that were added after the system call was made] 
// may be delayed by a maximum of 1 sec, since the system call has to return.
// For this reason, the timeout below is set to 20-msec. Now expirations
// due to timers that were added post the poll/epoll/.. system-call will
// be checked within 20-msecs of their addition.
//----------------------------------------------------------------------------
#define VQEC_CLIENIF_KEEPALIVE_HANDLER_TIMEOUT (20 * 1000)

UT_STATIC vqec_error_t
vqec_ifclient_start_ul (void) 
{
    struct timeval tv;
    vqec_error_t err = VQEC_OK;

    if ((s_vqec_ifclient_state == VQEC_IFCLIENT_INITED) ||
        (s_vqec_ifclient_state == VQEC_IFCLIENT_STOPPED)) {

        s_vqec_ifclient_state = VQEC_IFCLIENT_STARTED;
        
        tv.tv_sec  = 0;
        tv.tv_usec = VQEC_CLIENIF_KEEPALIVE_HANDLER_TIMEOUT;
        
        if (!vqec_event_create(&s_vqec_keepalive_ev,
                               VQEC_EVTYPE_TIMER, 
                               VQEC_EV_RECURRING,
                               vqec_ifclient_keepalive_handler,
                               VQEC_EVDESC_TIMER, 
                               NULL) ||
            !vqec_event_start(s_vqec_keepalive_ev, &tv)) {

            vqec_event_destroy(&s_vqec_keepalive_ev);
            vqec_ifclient_stop_ul();
            vqec_ifclient_deinit_ul();

            err = VQEC_ERR_MALLOC;
            vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_MALLOC, "%s", __FUNCTION__);
            return (err);
        }
        
        /*
         * Event dispatch acquires, releases the lock, so we release
         * the lock here.
         */
        vqec_lock_unlock(vqec_g_lock);        
        /* sa_ignore {Ignoring return value} IGNORE_RETURN(1) */
        vqec_event_dispatch();
        vqec_lock_lock(vqec_g_lock);        

        vqec_event_destroy(&s_vqec_keepalive_ev);
    } else {
        err = VQEC_ERR_INVCLIENTSTATE;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s (%d)",
                              __FUNCTION__, 
                              "Start in invalid state", 
                              s_vqec_ifclient_state);
    }

    return (err);
}

UT_STATIC void 
vqec_ifclient_stop_ul (void)
{
    if (s_vqec_ifclient_state == VQEC_IFCLIENT_STARTED) {
        s_vqec_ifclient_state = VQEC_IFCLIENT_STOPPED;
        /* sa_ignore {This function does not return errors;
           error message is logged by loopexit} IGNORE_RETURN(1) */
        vqec_event_loopexit(0);
    } else {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s (%d)",
                              __FUNCTION__, 
                              "Stop invalid state", s_vqec_ifclient_state);
    }
}

//----------------------------------------------------------------------------
// Function that has all the deinit opers for deinit'ing the client.
//----------------------------------------------------------------------------
UT_STATIC void
vqec_ifclient_deinit_do_oper (void)
{
    vqec_updater_deinit();
    vqec_tuner_module_deinit();
    vqec_chan_module_deinit();
    if (s_vqec_ifclient_tmp_chans) {
        free(s_vqec_ifclient_tmp_chans);
        s_vqec_ifclient_tmp_chans = NULL;
    }

    /* sa_ignore {No recourse if cfg_shutdown fails} IGNORE_RETURN (1) */
    cfg_shutdown();

    vqec_igmp_module_deinit();

    vqec_nat_module_deinit();
    vqec_destroy_cli();    
    vqec_upcall_close_unsocks();
    (void)vqec_dp_deinit_module();
    (void)vqec_dp_close_module();
    syslog_facility_close();
}

//----------------------------------------------------------------------------
// Invoke deinit_do_oper() if the client is in proper state.
//----------------------------------------------------------------------------
UT_STATIC void
vqec_ifclient_deinit_ul (void) 
{
    if (s_vqec_ifclient_state >= VQEC_IFCLIENT_INITED) {
        vqec_ifclient_deinit_do_oper();
        s_vqec_ifclient_state = VQEC_IFCLIENT_UNINITED;
        if (vqec_info_logging()) {
            snprintf(s_log_str_large, VQEC_LOGMSG_BUFSIZE_LARGE,
                     "%s()",
                     __FUNCTION__);
            syslog_print(VQEC_INFO, s_log_str_large);
        }
    } else {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s (%d)",
                              __FUNCTION__, "Deinit in invalid state",
                              s_vqec_ifclient_state);
    }
}


UT_STATIC int32_t 
vqec_ifclient_pthread_create (pthread_t *tid, 
                              void *(*start_routine)(void *), void *arg)
{
    int32_t ret = 0;

    ret = vqec_pthread_create(tid, start_routine, arg);

    return (ret);
}

/* Sockets per tuner for NAT. */
#define NAT_SOCKS_PER_TUNER 4

vqec_error_t vqec_ifclient_init_ul (const char *filename)
{
    vqec_syscfg_t v_cfg;
    vqec_error_t err = VQEC_OK;  /* used for error codes to be stringified */
    /*
     * unless code goes to "bail" label, syscfg_err is the only returnable
     * error code.
     */
    vqec_error_t syscfg_err;
    cfg_ret_e cfg_err;
    pthread_t vqec_cli_thread_id;
    int32_t cur_idx;
    vqec_tunerid_t tuner_ids[VQEC_SYSCFG_MAX_MAX_TUNERS];
    int32_t ret;
    vqec_sdp_handle_t sdp_handle;
    int loglevel = LOG_DEBUG;
    char *chan_db_name;
    uint32_t num_parsed, num_validated, total;
    vqec_nat_init_params_t nat_params;
    vqec_dp_module_init_params_t dp_init_params;
    vqec_bind_params_t *bp;
    abs_time_t init_start_time, update_abort_time;
    rel_time_t init_duration;

    /*
     * Store the starting time of VQE-C initializtion,
     * in case it is needed later for a log message.
     */
    init_start_time = get_sys_time();

    /* Load the start-up configuration from supplied file */
    syscfg_err = vqec_syscfg_init(filename);
    vqec_syscfg_get(&v_cfg);

    /* Initialize the logging facility */
    loglevel = v_cfg.log_level;
    syslog_facility_open(LOG_VQEC, LOG_CONS);
    syslog_facility_filter_set(LOG_VQEC, loglevel);
    sync_lib_facility_filter_with_process(LOG_VQEC, LOG_LIBCFG);
    sync_lib_facility_filter_with_process(LOG_VQEC, LOG_RTP);
    sync_lib_facility_filter_with_process(LOG_VQEC, LOG_VQE_UTILS);

    /* Deferred error logging for system config */
    if (syscfg_err == VQEC_ERR_PARAMRANGEINVALID) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
             "invalid sys cfg parameters are set to default values");
    } else if (syscfg_err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL,
             "error loading configuration file");
    }

    /*
     * Compute a time limit by which network config updates must complete,
     * or else they will be aborted so as to not delay initialization by
     * too much.
     */
    update_abort_time =
        TIME_ADD_A_R(get_sys_time(), VQEC_UPDATER_TIMEOUT_INIT);

    /* Update client attributes from VCDS if configured */
    if (v_cfg.cdi_enable &&
        v_cfg.network_cfg_pathname[0]) {
        /* Initialize the updater */
        err = vqec_updater_init(&v_cfg);
        if (err != VQEC_OK) {
            vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL,
                                  "Initialization of updater failed");
            goto bail;
        }
        /* Request an update */
        vqec_updater_request_update(TRUE, TRUE, FALSE, update_abort_time);
    }

    /*
     * Read the system configuration from the filesystem
     * (incorporating all contributing sources), and then update
     * the syscfg module with it.
     *
     * Note that no system configuration parameter defined as an
     * attribute has been already used by VQE-C during initialization.
     * If any parameters supported as attributes in the future are used
     * prior to this point during initialization, then the affected parts
     * of VQE-C will need to be re-initializaed with the new/updated value.
     */
    vqec_syscfg_read(&v_cfg);
    vqec_syscfg_set(&v_cfg, FALSE);

    /* Verify that VQE-C is enabled before continuing */
    if (!v_cfg.qoe_enable) {
        syslog_print(VQEC_NOTICE,
                     "VQE-C is disabled, aborting initialization");
        err = VQEC_ERR_DISABLED;
        goto bail;
    }

    /*
     * VQE-C is enabled, proceed with initialization 
     */

    /* Allocate space for dynamic channel structures to be cached */
    s_vqec_ifclient_tmp_chans = malloc(sizeof(vqec_ifclient_tmp_chan_t) *
                                       v_cfg.max_tuners);
    if (!s_vqec_ifclient_tmp_chans) {
        err = VQEC_ERR_MALLOC;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_MALLOC, 
                              "%s (dynamic channels)", __FUNCTION__);
        goto bail;
    }
    memset(s_vqec_ifclient_tmp_chans,
           0,
           sizeof(vqec_ifclient_tmp_chan_t) * v_cfg.max_tuners);

    /*
     * Initialize the channel config module with the static SDP configuration
     * file, if configured.
     */
    if (loglevel == LOG_DEBUG) {
        VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_MGR);
        VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_DB);
        VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_CHANNEL);
        VQEC_SET_DEBUG_FLAG(VQEC_DEBUG_CHANNEL);
    }
#define MAX_VQEC_CHAN_LINEUP_NAMELEN (1024)
    if (!strncmp(v_cfg.channel_lineup, VQEC_SYSCFG_DEFAULT_CHANNEL_LINEUP, 
                 MAX_VQEC_CHAN_LINEUP_NAMELEN)) {
        chan_db_name = NULL;
    } else {
        if (vqec_syscfg_checksum_is_valid(VQEC_IFCLIENT_CONFIG_CHANNEL)) {
            chan_db_name = v_cfg.channel_lineup;
        } else {
            /*
             * Avoid initializing VQE-C with a channel lineup whose
             * checksum doesn't match what's in the index file.
             */
            vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL,
                                  "Channel Configuration checksum "
                                  "verification failed, ignoring "
                                  "configuration file.");
            chan_db_name = NULL;
        }
    }
    cfg_err = cfg_init(chan_db_name);
    switch (cfg_err) {
    case CFG_SUCCESS:
        break;                
    case CFG_FAILURE:                
        err = VQEC_ERR_NOCHANNELLINEUP;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s (CFG_FAILURE))", __FUNCTION__);
        break;
    case CFG_OPEN_FAILED:
        err = VQEC_ERR_NOCHANNELLINEUP;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s (CFG_OPEN_FAILED))", __FUNCTION__);
        break;
    case CFG_INVALID_HANDLE:
        err = VQEC_ERR_NOCHANNELLINEUP;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s (CFG_INV_HANDLE))", __FUNCTION__);
        break;
    case CFG_GET_FAILED:
        err = VQEC_ERR_NOCHANNELLINEUP;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s (CFG_GET_FAILED))", __FUNCTION__);
        break;
    case CFG_ALREADY_INIT:
        err = VQEC_ERR_NOCHANNELLINEUP;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s (CFG_ALREADY_INIT))", __FUNCTION__);
        break;
    case CFG_UNINITIALIZED:
        err = VQEC_ERR_NOCHANNELLINEUP;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s (CFG_UNINITIALIZED))", __FUNCTION__);
        break;
    case CFG_MALLOC_REQ:
        err = VQEC_ERR_MALLOC;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_MALLOC, 
                              "%s %s (cfg memory allocation)",
                              __FUNCTION__, vqec_err2str(err));
        break;
    default:
        err = VQEC_ERR_UNKNOWN;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s (some unknown error)", __FUNCTION__);
        break;
    }
    
    /*
     * If the configuration delivery infrastructure is in use,
     * then attempt to retreive an updated channel lineup.
     */
    if (v_cfg.cdi_enable) {
        /* Initialize the updater */
        err = vqec_updater_init(&v_cfg);
        if ((err != VQEC_OK) && (err != VQEC_ERR_ALREADYINITIALIZED)) {
            vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL,
                                  "Initialization of updater failed");
            goto bail;
        }
        /* Request an update */
        vqec_updater_request_update(FALSE, FALSE, TRUE, update_abort_time);
    }
    /* Get the channel stats and display on the console */
    if (cfg_get_cfg_stats(&num_parsed, &num_validated, &total) == 
        CFG_SUCCESS) {
        CONSOLE_PRINTF("Channel summary: %d out of %d channels "
                       "passed the validation\n", num_validated, total);
    }

    /* Initialize the cli */
    if (v_cfg.cli_telnet_port != 0) {
        
        /* 
         * This thread is explicitly set to run @ low priority. Hence, the
         * chance of priority inversion under heavy workloads / simultaneous
         * use of CLI exists.
         */
        if ((ret = 
             vqec_ifclient_pthread_create(&vqec_cli_thread_id, 
                                          vqec_cli_thread, NULL))) {

            vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                                  __FUNCTION__, "Cannot create CLI thread");
        }
    }

    if (!vqec_event_init()) {
        err = VQEC_ERR_MALLOC;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_MALLOC, 
                              "%s (libevent)", __FUNCTION__);
        goto bail;
    }

    if (vqec_dp_open_module() != VQEC_DP_ERR_OK) {
        err = VQEC_ERR_INTERNAL;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                              __FUNCTION__, "DP module not present");
        goto bail;
    }

    memset(&dp_init_params, 0, sizeof(dp_init_params));
    dp_init_params.pakpool_size = v_cfg.pakpool_size;
    dp_init_params.max_paksize = v_cfg.max_paksize;
    dp_init_params.max_tuners = v_cfg.max_tuners;
    dp_init_params.max_channels = v_cfg.max_tuners;
    dp_init_params.max_streams_per_channel = 
        VQEC_MAX_STREAMS_PER_CHANNEL; 
    dp_init_params.output_q_limit = v_cfg.output_pakq_limit;
    dp_init_params.max_iobuf_cnt = 
        VQEC_MSG_MAX_IOBUF_CNT;
    dp_init_params.iobuf_recv_timeout = 
        VQEC_MSG_MAX_RECV_TIMEOUT;
    dp_init_params.app_paks_per_rcc = v_cfg.app_paks_per_rcc;
    dp_init_params.app_cpy_delay = v_cfg.app_delay;

    if (vqec_dp_init_module(&dp_init_params) != VQEC_DP_ERR_OK) {
        err = VQEC_ERR_INTERNAL;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                              __FUNCTION__, "TLM initialization failed");
        goto bail;
    }

    /* 
     * Upcall socket instantiation / setup - also creates peer sockets
     * in the dataplane.
     */
    err = vqec_upcall_setup_unsocks(v_cfg.max_paksize);
    if (err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s (upcall socket)", __FUNCTION__);
        goto bail;            
    }

    /* initialize the NAT module */	
    nat_params.max_bindings = NAT_SOCKS_PER_TUNER * v_cfg.max_tuners;
    nat_params.refresh_interval = v_cfg.nat_binding_refresh_interval;
    nat_params.max_paksize = v_cfg.max_paksize;
    nat_params.input_ifname = v_cfg.input_ifname;
    if ((err = vqec_nat_module_init(v_cfg.sig_mode,
                                    VQEC_NAT_PROTO_STUN,
                                    &nat_params)) != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s (nat api %s)", 
                              __FUNCTION__, vqec_err2str(err));
        goto bail;
    }

    s_vqec_ifclient_deliver_paks_to_user = v_cfg.deliver_paks_to_user;
    if ((err = init_vqec_tuner_module(v_cfg.max_tuners)) != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s (tuner module %s)", 
                              __FUNCTION__, vqec_err2str(err));
        goto bail;
    }
    if (vqec_chan_module_init() != VQEC_CHAN_ERR_OK) {
        err = VQEC_ERR_INTERNAL;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, 
                              "%s (chan module %s)", 
                              __FUNCTION__, vqec_err2str(err));
        goto bail;
    }

    init_vqe_hash_elem_module();
    init_vqe_hash_module();
    if (!vqec_igmp_module_init()) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_MALLOC, 
                              "%s (igmp module init)", 
                              __FUNCTION__);
        goto bail;
    }
 
    /* create tuners listed in config file, if any */
    for (cur_idx = 0; cur_idx < v_cfg.max_tuners; cur_idx++) {
        if (strncmp(v_cfg.tuner_list[cur_idx].tuner_name, "", 1)) {
            err = vqec_ifclient_tuner_create_ul(&tuner_ids[cur_idx],
                                      v_cfg.tuner_list[cur_idx].tuner_name);
            if ((err != VQEC_OK)) {
                printf("Error creating tuner (%s)\n", vqec_err2str(err));
                continue;
            }
            if (strncmp(v_cfg.tuner_list[cur_idx].tuner_url, "", 1)) {
                sdp_handle = vqec_ifclient_alloc_sdp_handle_from_url_ul(
                    v_cfg.tuner_list[cur_idx].tuner_url);
                if (sdp_handle == NULL) {
                    printf("Error binding tuner null sdp_handle");
                }
                bp = vqec_ifclient_bind_params_create();
                if (bp == NULL) {
                    printf("Error binding tuner null bind params handle");
                } else {
                    bp->max_recv_bw_rcc =
                        v_cfg.tuner_list[cur_idx].max_receive_bandwidth_rcc;
                    bp->max_recv_bw_er =
                        v_cfg.tuner_list[cur_idx].max_receive_bandwidth_er;
                }
                err = vqec_ifclient_tuner_bind_chan_ul(
                    tuner_ids[cur_idx], sdp_handle, bp);
                if (err != VQEC_OK) {
                    printf("Error in binding tuner \"%s\" to chan (%s)\n",
                           v_cfg.tuner_list[cur_idx].tuner_name,
                           vqec_err2str(err));
                }
                vqec_ifclient_free_sdp_handle_ul(sdp_handle);
                vqec_ifclient_bind_params_destroy(bp);
            }
        }
    }

    if (v_cfg.cdi_enable) {
        err = vqec_updater_start();
        if (err != VQEC_OK) {
            /* Log this error, treat as non-fatal */
            vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                                  __FUNCTION__,
                                  "Updater thread failed to start");
        }
    }

    s_vqec_ifclient_state = VQEC_IFCLIENT_INITED;

    if (vqec_info_logging()) {
        init_duration = TIME_SUB_A_A(get_sys_time(), init_start_time);
        snprintf(s_log_str_large, VQEC_LOGMSG_BUFSIZE_LARGE,
                 "%s(filename=\"%s\"): Initialization completed in %llu ms",
                 __FUNCTION__,
                 filename ? filename : "",
                 TIME_GET_R(msec, init_duration));
        syslog_print(VQEC_INFO, s_log_str_large);
    }

    return VQEC_OK;

  bail:
    vqec_ifclient_deinit_do_oper();    
    return (err);
}


//----------------------------------------------------------------------------
// Statistics interface.
//----------------------------------------------------------------------------

/**
 * Add stats to a running total.
 *
 * @param[in,out] total        - stats structure to which "stats" are added
 * @param[in]     stats        - stats to be added to "total"
 */
void
vqec_ifclient_add_stats (vqec_ifclient_stats_t *total,
                         vqec_ifclient_stats_channel_t *stats)
{
    int i;

    VQEC_ASSERT(total && stats);
    
    /*
     * Roll up "stats" into "total".
     */
    total->primary_udp_inputs += stats->primary_udp_inputs;
    total->primary_udp_drops += stats->primary_udp_drops;

    total->primary_rtp_inputs += stats->primary_rtp_inputs;
    total->primary_rtp_drops += stats->primary_rtp_drops;
    total->primary_rtp_drops_late += stats->primary_rtp_drops_late;

    total->primary_rtcp_inputs += stats->primary_rtcp_inputs;
    total->primary_rtcp_outputs += stats->primary_rtcp_outputs;

    total->repair_rtp_inputs += stats->repair_rtp_inputs;
    total->repair_rtp_drops += stats->repair_rtp_drops;
    total->repair_rtp_drops_late += stats->repair_rtp_drops_late;

    total->repair_rtcp_inputs += stats->repair_rtcp_inputs;
    total->fec_inputs += stats->fec_inputs;
    total->fec_drops += stats->fec_drops;
    total->fec_drops_late += stats->fec_drops_late;

    total->repair_rtp_stun_inputs += stats->repair_rtp_stun_inputs;
    total->repair_rtp_stun_outputs += stats->repair_rtp_stun_outputs;
    total->repair_rtcp_stun_inputs += stats->repair_rtcp_stun_inputs;
    total->repair_rtcp_stun_outputs += stats->repair_rtcp_stun_outputs;
    
    total->post_repair_outputs += stats->post_repair_outputs;
    total->tuner_queue_drops += stats->tuner_queue_drops;
    total->underruns += stats->underruns;

    total->pre_repair_losses += stats->pre_repair_losses;
    total->post_repair_losses += stats->post_repair_losses;
    total->post_repair_losses_rcc += stats->post_repair_losses_rcc;

    total->repairs_requested += stats->repairs_requested;
    total->repairs_policed += stats->repairs_policed;

    total->fec_recovered_paks += stats->fec_recovered_paks;

    total->rcc_requests += stats->rcc_requests;
    total->concurrent_rccs_limited += stats->concurrent_rccs_limited;
    total->rcc_with_loss += stats->rcc_with_loss;
    total->rcc_aborts_total += stats->rcc_aborts_total;
    for (i=0; i<VQEC_IFCLIENT_RCC_ABORT_MAX; i++) {
        total->rcc_abort_stats[i] += stats->rcc_abort_stats[i];
    }
}

UT_STATIC void
vqec_ifclient_clear_stats_ul (void)
{
    /*
     * Clear stats kept by the channel, tuner, and stream out modules.
     */
    (void)vqec_chan_clear_counters();
    vqec_tuner_clear_stats();
    vqec_updater_clear_counters();
}

UT_STATIC vqec_error_t
vqec_ifclient_tuner_dump_state_ul (const vqec_tunerid_t id, 
                                   unsigned int options_flag)
{
    vqec_error_t err;
    err = vqec_tuner_dump(id, options_flag);
    if (err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }

    return (err);
}

UT_STATIC vqec_error_t 
vqec_ifclient_get_stats_ul (vqec_ifclient_stats_t *stats, boolean 
                            cumulative)
{
    vqec_error_t err;
    vqec_chan_err_t status_channel;

    if (!stats) {
        err = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    /*
     * Roll up the global/historical stats by adding the following:
     *   1. channel-based stats (from historical & active channels), and
     *   2. tuner-based stats (from previous & current tuner bindings)
     */
    memset(stats, 0, sizeof(*stats));
    status_channel = vqec_chan_get_counters(stats, cumulative);
    if (status_channel != VQEC_CHAN_ERR_OK) {
        err = VQEC_ERR_INTERNAL;
        goto done;
    }
    err = vqec_tuner_get_stats_global(stats, cumulative);

done:
    if (err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }
    return (err);
}

UT_STATIC vqec_error_t 
vqec_ifclient_get_stats_channel_ul (const char *url,
                                    vqec_ifclient_stats_channel_t *stats,
                                    boolean cumulative)
{
    vqec_protocol_t protocol;
    in_addr_t ip;
    uint16_t port;
    struct in_addr ip_addr;
    channel_cfg_t *sdp_cfg = NULL;
    vqec_chan_cfg_t chan_cfg;
    vqec_chanid_t chanid;
    vqec_chan_err_t err_chan;
    vqec_error_t err = VQEC_OK;

    if (!url || !stats) {
        err = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    memset(stats, 0, sizeof(*stats));
    if (!vqec_url_parse((vqec_url_t)url, &protocol, &ip, &port)) {
        err = VQEC_ERR_CHANNELPARSE;
        goto done;
    }

    /* 
     * API caller's supplied primary stream address and port are translated 
     * from (original dest addr, original dest port) to (re-sourced dest addr, 
     * re-sourced dest port) if such a re-sourced stream is present in the 
     * channel manager.
     */
    ip_addr.s_addr = ip;
    sdp_cfg = cfg_get_channel_cfg_from_orig_src_addr(ip_addr, port);
    if (sdp_cfg) {
        vqec_chan_convert_sdp_to_cfg(sdp_cfg, VQEC_CHAN_TYPE_LINEAR, &chan_cfg);
  
        err_chan = vqec_chan_find_chanid(&chanid, 
                                         chan_cfg.primary_dest_addr.s_addr,
                                         chan_cfg.primary_dest_port);
    } else {
       /*
        * Signifies a channel that is not in the channel_mgr's channel map
        * e.g. a VOD channel
        */
       err_chan = vqec_chan_find_chanid(&chanid, ip, port);

    }
    switch (err_chan) {
    case VQEC_CHAN_ERR_NOTFOUND:
        err = VQEC_ERR_CHANNOTACTIVE;
        goto done;
    case VQEC_CHAN_ERR_OK:
        break;
    default:
        err = VQEC_ERR_INTERNAL;
        goto done;
    }

    err_chan = vqec_chan_get_counters_channel(chanid, stats, cumulative);
    if (err_chan != VQEC_CHAN_ERR_OK) {
        err = VQEC_ERR_INTERNAL;
        goto done;
    }
    err_chan = vqec_chan_get_tuner_q_drops(chanid, &stats->tuner_queue_drops, 
                                           cumulative);
    if (err_chan != VQEC_CHAN_ERR_OK) {
        err = VQEC_ERR_INTERNAL;
    }
done:
    if (err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }
    return (err);
}


UT_STATIC vqec_error_t 
vqec_ifclient_get_stats_channel_tr135_sample_ul (
                    const char *url,
                    vqec_ifclient_stats_channel_tr135_sample_t *stats)
{
     
    vqec_protocol_t protocol;
    in_addr_t ip;
    uint16_t port;
    struct in_addr ip_addr;
    channel_cfg_t *sdp_cfg = NULL;
    vqec_chan_cfg_t chan_cfg;
    vqec_chanid_t chanid;
    vqec_chan_err_t err_chan;
    vqec_error_t err = VQEC_OK;

    if (!url || !stats) {
        err = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    memset(stats, 0, sizeof(*stats));
    if (!vqec_url_parse((vqec_url_t)url, &protocol, &ip, &port)) {
        err = VQEC_ERR_CHANNELPARSE;
        goto done;
    }

    /* 
     * API caller's supplied primary stream address and port are translated 
     * from (original dest addr, original dest port) to (re-sourced dest addr, 
     * re-sourced dest port) if such a re-sourced stream is present in the 
     * channel manager.
     */
    ip_addr.s_addr = ip;
    sdp_cfg = cfg_get_channel_cfg_from_orig_src_addr(ip_addr, port);
    if (sdp_cfg) {
        vqec_chan_convert_sdp_to_cfg(sdp_cfg, VQEC_CHAN_TYPE_LINEAR, &chan_cfg);
  
        err_chan = vqec_chan_find_chanid(&chanid, 
                                         chan_cfg.primary_dest_addr.s_addr,
                                         chan_cfg.primary_dest_port);
    } else {
       /*
        * Signifies a channel that is not in the channel_mgr's channel map
        * e.g. a VOD channel
        */
       err_chan = vqec_chan_find_chanid(&chanid, ip, port);

    }
    switch (err_chan) {
    case VQEC_CHAN_ERR_NOTFOUND:
        err = VQEC_ERR_CHANNOTACTIVE;
        goto done;
    case VQEC_CHAN_ERR_OK:
        break;
    default:
        err = VQEC_ERR_INTERNAL;
        goto done;
    }

    err_chan = vqec_chan_get_counters_channel_tr135_sample(chanid, 
                                                           stats);
    if (err_chan != VQEC_CHAN_ERR_OK) {
        err = VQEC_ERR_INTERNAL;
        goto done;
    }
    
done:
    if (err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }
    return (err);
}


UT_STATIC vqec_error_t 
vqec_ifclient_set_tr135_params_channel_ul (const char *url,
                                           vqec_ifclient_tr135_params_t *params)
{

    vqec_protocol_t protocol;
    in_addr_t ip;
    uint16_t port;
    struct in_addr ip_addr;
    channel_cfg_t *sdp_cfg = NULL;
    vqec_chan_cfg_t chan_cfg;
    vqec_chanid_t chanid;
    vqec_chan_err_t err_chan;
    vqec_error_t err = VQEC_OK;

    if (!url || !params) {
        err = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    if (!vqec_url_parse((vqec_url_t)url, &protocol, &ip, &port)) {
        err = VQEC_ERR_CHANNELPARSE;
        goto done;
    }

    /*
     * API caller's supplied primary stream address and port are translated
     * from (original dest addr, original dest port) to 
     * (re-sourced dest addr, re-sourced dest port) 
     * if such a re-sourced stream is present in the channel manager.
     */
    ip_addr.s_addr = ip;
    sdp_cfg = cfg_get_channel_cfg_from_orig_src_addr(ip_addr, port);
    if (sdp_cfg) {
        vqec_chan_convert_sdp_to_cfg(sdp_cfg, VQEC_CHAN_TYPE_LINEAR, &chan_cfg);
        err_chan = vqec_chan_find_chanid(&chanid, 
                                         chan_cfg.primary_dest_addr.s_addr,
                                         chan_cfg.primary_dest_port);
    } else {
        /* 
         * Signifies a channel that is not in the channel_mgr's channel map
         * e.g. a VOD channel
         */
        err_chan = vqec_chan_find_chanid(&chanid, ip, port);
    }

    switch (err_chan) {
    case VQEC_CHAN_ERR_NOTFOUND:
        err = VQEC_ERR_CHANNOTACTIVE;
        goto done;
    case VQEC_CHAN_ERR_OK:
        break;
    default:
        err = VQEC_ERR_INTERNAL;
        goto done;
    }

    err_chan = vqec_chan_set_tr135_params(chanid, params);
    if (err_chan != VQEC_CHAN_ERR_OK) {
        err = VQEC_ERR_INTERNAL;
    }

done:
    if (err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }
    return (err);
}

vqec_error_t
vqec_ifclient_get_stats_tuner_legacy_ul (vqec_tunerid_t id,
                                         vqec_ifclient_stats_channel_t *stats)
{
    vqec_error_t err;
    err = vqec_tuner_get_stats_legacy(id, stats);
    return (err);
}

UT_STATIC vqec_error_t
vqec_ifclient_histogram_display_ul (const vqec_hist_t hist)
{
    vqec_error_t err = VQEC_OK;
    vqec_dp_error_t err_dp;
    vqec_dp_histogram_data_t *hist_ptr = NULL;
    vqec_dp_hist_t hist_dp;

    /*
     * Retrieve a pointer to the histogram to be displayed based on its name.
     */
    switch (hist) {
    /* Data plane histograms:  retrieve a copy for display */
    case VQEC_HIST_JOIN_DELAY:
    case VQEC_HIST_OUTPUTSCHED:
        switch (hist) {
        case VQEC_HIST_JOIN_DELAY:
            hist_dp = VQEC_DP_HIST_JOIN_DELAY;
            break;
        case VQEC_HIST_OUTPUTSCHED:
            hist_dp = VQEC_DP_HIST_OUTPUTSCHED;
            break;
        default:
            err = VQEC_ERR_INTERNAL;
            goto done;
        }
        hist_ptr = malloc(sizeof(*hist_ptr));
        if (!hist_ptr) {
            err = VQEC_ERR_MALLOC;
            goto done;
        }
        err_dp = vqec_dp_chan_hist_get(hist_dp, hist_ptr);
        switch (err_dp) {
        case VQEC_DP_ERR_OK:
            break;
        case VQEC_DP_ERR_SHUTDOWN:
            err = VQEC_ERR_NOTINITIALIZED;
            goto done;
        default:
            err = VQEC_ERR_INTERNAL;
            goto done;
        }
        break;
    /* Unrecognized histograms */
    default:
        err = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    /* Dislay the histogram */
    vam_hist_display_nonzero_hits((vam_hist_type_t *)hist_ptr);

 done:
    if (err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }
    if (hist_ptr) {
        free(hist_ptr);
    }
    return (err);
}

UT_STATIC vqec_error_t
vqec_ifclient_histogram_clear_ul (const vqec_hist_t hist)
{
    vqec_error_t err = VQEC_OK;
    vqec_dp_error_t err_dp;
    vqec_dp_hist_t hist_dp;

    switch (hist) {
    /* Data plane histograms:  send the clear request to the data plane */
    case VQEC_HIST_JOIN_DELAY:
    case VQEC_HIST_OUTPUTSCHED:
        switch (hist) {
        case VQEC_HIST_JOIN_DELAY:
            hist_dp = VQEC_DP_HIST_JOIN_DELAY;
            break;
        case VQEC_HIST_OUTPUTSCHED:
            hist_dp = VQEC_DP_HIST_OUTPUTSCHED;
            break;
        default:
            err = VQEC_ERR_INTERNAL;
            goto done;
        }
        err_dp = vqec_dp_chan_hist_clear(hist_dp);
        if (err_dp != VQEC_DP_ERR_OK) {
            err = VQEC_ERR_INTERNAL;
            goto done;
        }
        break;
    /* Unrecognized histograms */
    default:
        err = VQEC_ERR_INVALIDARGS;
        goto done;
    }

 done:
    if (err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }
    return (err);
}

UT_STATIC vqec_error_t 
vqec_ifclient_updater_get_stats_ul (vqec_ifclient_updater_stats_t *stats)
{
    vqec_error_t err = VQEC_OK;
    boolean in_progress;
    struct timeval chancfg_last_update_time;
    char *chancfg_version;

    if (!stats) {
        err = VQEC_ERR_INVALIDARGS;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                              __FUNCTION__, vqec_err2str(err));
        goto done;
    }

    memset(stats, 0, sizeof(*stats));
    err = vqec_updater_get_status(NULL, &in_progress, NULL, NULL, NULL,
                                  &chancfg_last_update_time, &chancfg_version);
    if (err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                              __FUNCTION__, vqec_err2str(err));
        goto done;
    }

    stats->updater_update_in_progress = in_progress;
    stats->updater_last_update_time = chancfg_last_update_time;
    stats->chan_cfg_total_channels = cfg_get_total_num_channels();
    if (chancfg_version) {
        strncpy(stats->chan_cfg_version, chancfg_version,
                VQEC_UPDATER_VERSION_STRLEN);
    }
done:
    return (err);
}

//----------------------------------------------------------------------------
// Channel update interface.
//----------------------------------------------------------------------------
vqec_error_t
vqec_ifclient_cfg_channel_lineup_set (sdp_block_t *sdp_block)
{
    vqec_error_t err = VQEC_OK;

    if (sdp_block){
        if (s_vqec_ifclient_state >= VQEC_IFCLIENT_INITED) {
            // Call channel configuration parsing and validation function
            if (cfg_parse_all_channel_data(sdp_block) == CFG_SUCCESS) {
                // Acquire the global lock and then commit the new channel
                // configuration data
                vqec_lock_lock(vqec_g_lock);
                if (cfg_commit_update() != CFG_SUCCESS) {
                    err = VQEC_ERR_NOCHANNELLINEUP;
                }
                vqec_lock_unlock(vqec_g_lock);
            }
            else {
                err = VQEC_ERR_SETCHANCFG;
            }
        }
        else {
            err = VQEC_ERR_INVCLIENTSTATE;
        }
    } else {
        err = VQEC_ERR_INVALIDARGS;
    }

    if (err != VQEC_OK) {
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s",
                              __FUNCTION__, vqec_err2str(err));
    }

    return (err);
}


UT_STATIC vqec_error_t
vqec_ifclient_config_update_ul (
    vqec_ifclient_config_update_params_t *params)
{
    char *config_str = "";
    vqec_error_t status = VQEC_OK;
    cfg_stats_t stats;

    if (!params ||
        (params->config != VQEC_IFCLIENT_CONFIG_NETWORK &&
         params->config != VQEC_IFCLIENT_CONFIG_CHANNEL) ||
        (!params->buffer)) {
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }
    
    params->persisted = FALSE;

    switch (params->config) {
    case VQEC_IFCLIENT_CONFIG_NETWORK:
        config_str = "Network";
        status = vqec_syscfg_update_network(params->buffer);
        if (status != VQEC_OK) {
            goto done;
        }

        params->persisted = TRUE;
        break;

    case VQEC_IFCLIENT_CONFIG_CHANNEL:
        config_str = "Channel";
        if (!vqec_ifclient_is_vqec_enabled_ul()) {
            /*
             * If VQE-C is not initialized, channel configuration updates 
             * are not accepted.
             */
            status = VQEC_ERR_INVALIDARGS;
            goto done;
        }
        
        if (cfg_parse_all_channel_data(params->buffer) != CFG_SUCCESS) {
            /* possible failure reasons:  insufficient memory */
            status = VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT;
            goto done;
        }
        cfg_get_update_stats_values(&stats);
        if (stats.num_syntax_errors) {
            /*
             * Prevent a configuration with any syntax errors from
             * being committed.
             */
            status = VQEC_ERR_CONFIG_INVALID;
            goto done;
        }
        
        if (cfg_commit_update() != CFG_SUCCESS) {
            status = VQEC_ERR_UPDATE_FAILED_EXISTING_REMOVED;
            goto done;
        }
        break;

    default:
        config_str = "Unsupported";
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }
    
done:
    if (status == VQEC_OK) {
        /* informational level debug */
        syslog_print(VQEC_CONFIG_UPDATE, config_str);
    } else {
        /* error level debug */
        syslog_print(VQEC_CONFIG_ERROR, config_str, vqec_err2str(status));
    }
    return (status);    
}

vqec_error_t
vqec_ifclient_config_update (
    vqec_ifclient_config_update_params_t *params)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_config_update_ul(params);
    vqec_lock_unlock(vqec_g_lock);

    return (retval);
}


vqec_error_t
vqec_ifclient_config_override_update (
                     vqec_ifclient_config_override_params_t *params)
{
    vqec_error_t status;

    if (!params) {
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    vqec_lock_lock(vqec_g_lock);
    status = vqec_syscfg_update_override_tags(params->tags, params->values);
    vqec_lock_unlock(vqec_g_lock);

done:
    if (status == VQEC_OK) {
        /* informational level debug */
        syslog_print(VQEC_CONFIG_UPDATE, "Override");
    } else {
        /* error level debug */
        syslog_print(VQEC_CONFIG_ERROR, "Override", vqec_err2str(status));
    }
    return (status);
}


vqec_error_t
vqec_ifclient_config_register_ul (
    const vqec_ifclient_config_register_params_t *params)
{
    return (vqec_syscfg_register(params));
}

vqec_error_t
vqec_ifclient_config_register (
    const vqec_ifclient_config_register_params_t *params)
{
    vqec_error_t status;

    vqec_lock_lock(vqec_g_lock);
    status = vqec_ifclient_config_register_ul(params);
    vqec_lock_unlock(vqec_g_lock);
    
    return (status);
}

vqec_error_t
vqec_ifclient_config_status_ul (vqec_ifclient_config_status_params_t *params)
{
    vqec_error_t status = VQEC_OK;
    const char *filename;
    const vqec_syscfg_t *syscfg = vqec_syscfg_get_ptr();
    FILE *fp = NULL;
    struct stat file_status;
    struct timeval tv;

    if (!params) {
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }
    strncpy(params->md5, VQEC_SYSCFG_CHECKSUM_UNAVAILABLE, 
            VQEC_MD5_CHECKSUM_STRLEN);
    strncpy(params->last_update_timestamp, "<unavailable>",
            VQEC_TIMESTAMP_STRLEN);

    switch (params->config) {
    case VQEC_IFCLIENT_CONFIG_NETWORK:
        filename = syscfg->network_cfg_pathname;
        break;
    case VQEC_IFCLIENT_CONFIG_CHANNEL:
        filename = syscfg->channel_lineup;
        break;
    case VQEC_IFCLIENT_CONFIG_OVERRIDE:
        filename = syscfg->override_cfg_pathname;
        break;
    default:
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    /* 
     * A configuration is persistent if both
     *   1) it is configured to be persisted, and
     *   2) a file actually exists (and can be accessed)
     * The call to access() below checks both conditions,
     * since the call fails if filename is ""
     * (meaning persistence hasn't been configured), 
     * or is not "" but the file can't be accessed.
     */
    params->persistent = !access(filename, F_OK);

    /*
     * A configuration's status is applicable if the file is
     * configured to be persisted and an index file is in use.
     */
    if (!filename[0] || !syscfg->index_cfg_pathname[0]) {
        params->status = VQEC_IFCLIENT_CONFIG_STATUS_NA;
    } else {
        params->status = 
            vqec_syscfg_checksum_is_valid(params->config) ? 
            VQEC_IFCLIENT_CONFIG_STATUS_VALID : 
            VQEC_IFCLIENT_CONFIG_STATUS_INVALID;
    }

    /*
     * A configuration's checksum can only be computed on files
     * that are configured to be persistent.
     */
    if (params->status != VQEC_IFCLIENT_CONFIG_STATUS_INVALID) {
        (void)vqe_MD5ComputeChecksumStr(filename, TRUE, params->md5);
    }

    /*
     * A configuration's timestamp of last update is derived
     * from the filesystem (for persisted configuration files)
     * or from memory (for a non-persisted channel configuration).
     */
    if (!filename[0] && params->config == VQEC_IFCLIENT_CONFIG_CHANNEL) {
        cfg_get_timestamp(params->last_update_timestamp,
                          VQEC_TIMESTAMP_STRLEN);
    } else {
        fp = fopen(filename, "r");
        if (fp && !fstat(fileno(fp), &file_status)) {
            tv.tv_sec = file_status.st_mtime;
            tv.tv_usec = 0;
            (void)abs_time_to_str_secs(timeval_to_abs_time(tv), 
                                       params->last_update_timestamp,
                                       VQEC_TIMESTAMP_STRLEN);
        };
    }

done:
    if (fp) {
        fclose(fp);
    }
    return (status);
}

vqec_error_t
vqec_ifclient_config_status (vqec_ifclient_config_status_params_t *params)
{
    vqec_error_t status;

    vqec_lock_lock(vqec_g_lock);
    status = vqec_ifclient_config_status_ul(params);
    vqec_lock_unlock(vqec_g_lock);
    
    return (status);
}

/**---------------------------------------------------------------------------
 * Is a fastfill operation enabled for the tuner?  Fastfill will be inactive
 * if the application requested no fastfill at bind time, or if the client or 
 * server had fastfill disabled via configuration.  If called when the tuner
 * is not bound the method will return FALSE.
 *
 * @param[in]  id Tuner identifier.
 * @param[out] uint8_t TRUE for fastfill enabled, FALSE otherwise.
 *----------------------------------------------------------------------------
 */
uint8_t 
vqec_ifclient_tuner_is_fastfill_enabled_ul (vqec_tunerid_t id)
{
    return (vqec_tuner_is_fastfill_enabled(id));
}

/**---------------------------------------------------------------------------
 * Is a RCC operation enabled for the tuner?  RCC will be inactive
 * if the application requested no RCC at bind time, or if the client or 
 * server had RCC disabled via configuration.  If called when the tuner
 * is not bound the method will return FALSE.
 *
 * @param[in]  id Tuner identifier.
 * @param[out] uint8_t TRUE for RCC enabled, FALSE otherwise.
 *----------------------------------------------------------------------------
 */
uint8_t 
vqec_ifclient_tuner_is_rcc_enabled_ul (vqec_tunerid_t id)
{
    return (vqec_tuner_is_rcc_enabled(id));
}

UT_STATIC uint8_t 
vqec_ifclient_socket_open_ul (vqec_in_addr_t *addr,
                              vqec_in_port_t *rtp_port,
                              vqec_in_port_t *rtcp_port)
{
    vqec_syscfg_t v_cfg;
    char temp[INET_ADDRSTRLEN], temp2[INET_ADDRSTRLEN];
    vqec_in_addr_t input_addr;
    vqec_in_port_t input_rtp_port = 0, input_rtcp_port = 0;

    /* Check input arguments */
    if (!addr || !rtp_port || !rtcp_port) {
        syslog_print(VQEC_ERROR, "Invalid args to open socket");
        return FALSE;
    }

    if (vqec_info_logging()) {
        memcpy(&input_addr, addr, sizeof(vqec_in_addr_t));
        memcpy(&input_rtp_port, rtp_port, sizeof(vqec_in_port_t));
        memcpy(&input_rtcp_port, rtcp_port, sizeof(vqec_in_port_t));
    }

    /* Assign an interface address */
    if (!addr->s_addr) {
        vqec_syscfg_get(&v_cfg); 
        if (get_ip_address_by_if(v_cfg.input_ifname, 
                                 temp, 
                                 INET_ADDRSTRLEN)) {
            addr->s_addr = inet_addr(temp);
        } else {
            addr->s_addr = INADDR_ANY;
        }
    }

    /* Open control plane RTCP port */
    if (!vqec_recv_sock_open(&addr->s_addr, rtcp_port, NULL)) {
        return FALSE;
    }

    /* Open dataplane RTP port */
    if (vqec_dp_input_shim_open_sock(&addr->s_addr, rtp_port) != 
        VQEC_DP_ERR_OK) {
        vqec_recv_sock_close(addr->s_addr, *rtcp_port);
        return FALSE;
    }
    
    if (vqec_info_logging()) {
        snprintf(s_log_str_large, VQEC_LOGMSG_BUFSIZE_LARGE,
                 "%s(addr=%s, rtp_port=%u, rtcp_port=%u): "
                 "addr=%s, rtp_port=%u, rtcp_port=%u",
                 __FUNCTION__,
                 inet_ntop(AF_INET, &input_addr, temp, INET_ADDRSTRLEN),
                 ntohs(input_rtp_port),
                 ntohs(input_rtcp_port),
                 inet_ntop(AF_INET, addr, temp2, INET_ADDRSTRLEN),
                 ntohs(*rtp_port),
                 ntohs(*rtcp_port));
        syslog_print(VQEC_INFO, s_log_str_large);
    }

    return TRUE;
}

UT_STATIC void 
vqec_ifclient_socket_close_ul (vqec_in_addr_t addr,
                               vqec_in_port_t rtp_port,
                               vqec_in_port_t rtcp_port)
{
    char tmp[INET_ADDRSTRLEN];

    vqec_recv_sock_close(addr.s_addr, rtcp_port);
    vqec_dp_input_shim_close_sock(addr.s_addr, rtp_port);
    if (vqec_info_logging()) {
        snprintf(s_log_str_large, VQEC_LOGMSG_BUFSIZE_LARGE,
                 "%s(addr=%s, rtp_port=%u, rtcp_port=%u)",
                 __FUNCTION__,
                 inet_ntop(AF_INET, &addr, tmp, INET_ADDRSTRLEN),
                 ntohs(rtp_port),
                 ntohs(rtcp_port));
        syslog_print(VQEC_INFO, s_log_str_large);
    }
}

