/*------------------------------------------------------------------
 *
 * July 2007, 
 *
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

/**
 * vqec_fec.c - defines fec functions for VQE-C module.
 * 
 * VQE-C receives both fec streams and primary video stream in a channel. 
 * This fec module only process fec packets. The received fec packets 
 * are first input to fec module (two packet sequences, column and row). 
 * If there are video packet losses detected, fec module tries to recover them.
 * In this version, we only implement simple XOR fec based on the 
 * Pro-MPEG COP#3 and RFC 2733.
 */

#include "vqec_fec.h"
#include <vam_time.h>
#include <rtp.h>
#include "vqe_port_macros.h"
#include "vqec_pak.h"
#include "vqec_pcm.h"
#include "vqec_dpchan.h"
#include <rtp_header.h>
#include <utils/vam_util.h>
#include <vqec_dp_common.h>
#include <vqec_dp_tlm.h>
#include <utils/zone_mgr.h>

#ifdef _VQEC_DP_UTEST
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

/*
 * XOR function for BOOLEAN types
 *
 * XOR(FALSE, FALSE) = FALSE
 * XOR(FALSE, TRUE) = TRUE
 * XOR(TRUE, FALSE) = TRUE
 * XOR(TRUE, TRUE) = FALSE
 */
#define XOR_BOOLEAN(a, b) ((a && b) || (!a && !b)) ? FALSE : TRUE

/**
 * define fec header check errors based on Pro-MPEG CoP#3r2.
 * These are return values in fec header sanity check,
 */
typedef enum vqec_fec_hdr_error_ {
    VQEC_FEC_HDR_OK = 0,
    VQEC_FEC_HDR_WRONG_VERSION,
    VQEC_FEC_HDR_WRONG_FLOW,
    VQEC_FEC_HDR_UNSUPPORTED_TYPE,
    VQEC_FEC_HDR_INVALID_L,
    VQEC_FEC_HDR_INVALID_D,
    VQEC_FEC_HDR_INVALID_LD,
    VQEC_FEC_HDR_INVALID_L_2D,
    VQEC_FEC_HDR_WRONG_COLUMN_OFFSET,
    VQEC_FEC_HDR_INVALID_FLOW_ID,
    VQEC_FEC_HDR_ERROR_OTHER,
} vqec_fec_hdr_error_t;

/**
 * define the type of fec decoding errors while decoding a row or column.
 * these are return values after calling decode_basis fuction. 
 *
 * OK:          everything is fine, decode success
 * NOT_NEEDED:  no packet loss, no decoding trial
 * LATE_PAK:    one of the video paks protected by this FEC packet 
 *              is out of fec buffer
 * FUTURE_PAK:  one of the video paks protected by this FEC packet
 *              is not arrived yet.
 * UNRECOVERABLE: the number of video pak losses are more than 1 
 *              in our XOR fec.
 * FAILURE:     decode failure
 * MEM_ALLOC_WRONG: malloc wrong
 * RTP_VALIDATE_WRONG: RTP validation wrong.
 * PAK_TOO_EARLY: FEC stream is out of sync with primary, too earily
 *                usually 2 blocks earlier.
 */

typedef enum vqec_fec_dec_error_ {
    VQEC_FEC_DEC_OK = 0,
    VQEC_FEC_DEC_NOT_NEEDED,
    VQEC_FEC_DEC_LATE_PAK,
    VQEC_FEC_DEC_FUTURE_PAK,
    VQEC_FEC_DEC_UNRECOVERABLE,
    VQEC_FEC_DEC_FAILURE,
    VQEC_FEC_DEC_MEM_ALLOC_WRONG,
    VQEC_FEC_DEC_RTP_VALIDATE_WRONG,
    VQEC_FEC_DEC_PAK_TOO_EARLY,
} vqec_fec_dec_error_t;

/**
 * define the error message, used while
 * inserting an fec packet to fec_pak_seq
 */
typedef enum vqec_fec_insert_error_ {
    VQEC_FEC_INSERT_ERROR_INVALID_ARG = 0,
    VQEC_FEC_INSERT_ERROR_DUPLICATE_PAK,
    VQEC_FEC_INSERT_ERROR_BAD_RANGE_PAK,
    VQEC_FEC_INSERT_ERROR_LATE_PAK,
    VQEC_FEC_INSERT_OK,
} vqec_fec_insert_error_t;

/**
 * FEC may be enabled if the channel is configured for it (typical mode)
 * or disabled for the purposes of a demo of picture quality w/o FEC.
 */
boolean g_fec_enabled = TRUE;
boolean g_fec_set = FALSE;

typedef struct vqe_zone vqec_fec_pool_t;
static vqec_fec_pool_t *s_vqec_fec_pool = NULL;
static vqec_pak_seq_pool_t *s_vqec_pak_seq_pool = NULL;

#define PAK_SEQ_PER_FEC 2

/**
 * Initialize the output scheduler module using the the
 * data plane initialization parameters. Responsible for
 * setting aside the appropriate module resources.
 *
 * @param[in] max_fecs Maximum number of simultaneous FEC objects
 * @param[out] vqec_dp_error_t Return VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_fec_module_init (uint32_t max_fecs)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (s_vqec_fec_pool) {
        VQEC_DP_SYSLOG_PRINT(INIT_FAILURE,
                            "FEC module already initialized");
        err = VQEC_DP_ERR_ALREADY_INITIALIZED;
        goto done;
    }

    s_vqec_fec_pool = zone_instance_get_loc (
                                "vqec_fec_pool",
                                 O_CREAT,
                                 sizeof(vqec_fec_t),
                                 max_fecs,
                                 NULL, NULL);
    if (!s_vqec_fec_pool) {
        VQEC_DP_SYSLOG_PRINT(INIT_FAILURE,
                             "FEC pool creation failed");
        err = VQEC_DP_ERR_NOMEM;
        goto done;
    }

    s_vqec_pak_seq_pool = vqec_pak_seq_pool_create(
                                 "pak_seq_fec",
                                 PAK_SEQ_PER_FEC * max_fecs,
                                 VQEC_FEC_STREAM_RING_BUFFER_BUCKET_BITS);
    if (!s_vqec_pak_seq_pool) {
        VQEC_DP_SYSLOG_PRINT(INIT_FAILURE,
                             "FEC pak seq pool creation failed");
        err = VQEC_DP_ERR_NOMEM;
        goto done;
    }

 done:
    if (err != VQEC_DP_ERR_OK &&
        err != VQEC_DP_ERR_ALREADY_INITIALIZED) {
        (void)vqec_fec_module_deinit();
    }
    return err;
}

/**
 * Deinitialize the fec module. Relinquish any
 * allocations made by the vqec_fec_module_init function.
 *
 * @params[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_fec_module_deinit ()
{
    if (s_vqec_fec_pool) {
        (void)zone_instance_put(s_vqec_fec_pool);
        s_vqec_fec_pool = NULL;
    }
    if (s_vqec_pak_seq_pool) {
        vqec_pak_seq_pool_destroy(s_vqec_pak_seq_pool);
        s_vqec_pak_seq_pool = NULL;
    }
    return VQEC_DP_ERR_OK;
}

/**
 * Enables/disables FEC globally (for all channels), for the purposes
 * of comparing picture quality during product demonstrations.
 *
 * Called upon "fec {enable|disable}" CLI commands being entered.
 *
 * NOTE:  Disabling FEC for demo purposes causes received FEC packets 
 *        to be discarded, and "disabled" appears in the "show fec" display.
 *        However, some channel state and debugs visible on the CLI may
 *        still give the appearance that FEC is enabled.
 *
 * @param[in] enabled - TRUE to enable (FEC will be performed for a channel if
 *                       specified in its channel description (e.g. SDP file))
 *                      FALSE to disable (FEC will not be performed regardless
 *                       of channel description configuration (e.g. SDP file))
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK.
 */
vqec_dp_error_t
vqec_dp_set_fec (boolean enabled)
{
    g_fec_enabled = enabled;
    g_fec_set = TRUE;
    return (VQEC_DP_ERR_OK);
}

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
vqec_dp_error_t
vqec_dp_get_fec (boolean *fec, boolean *fec_set)
{
    if (fec && g_fec_set) {
        *fec = vqec_dp_get_fec_inline(); 
    }
    if (fec_set) {
        *fec_set = g_fec_set;
    }    
    return (VQEC_DP_ERR_OK);
}

/**
 * get packet sequence #
 */
UT_STATIC inline vqec_seq_num_t 
vqec_fec_get_seq_num (const vqec_pak_t * pak)
{    
    VQEC_DP_ASSERT_FATAL(pak, "fec");
    return pak->seq_num;
}

/**
 * Validate FEC RTP packet header.
 * 
 * @param[in] buff  for FEC packet, include RTP header
 * @param[in] len   length of buffer
 *
 * @return the error check code.
 */
UT_STATIC vqec_dp_error_t vqec_fec_rtp_hdr_validation (char *buf,
                                                       uint16_t len)
{
    rtp_hdr_session_t rtp_session;
    rtp_hdr_status_t rtp_status;
    vqec_dp_error_t ret = VQEC_DP_ERR_BADRTPHDR;

    if (!buf || len == 0) {
        return ret;
    }

    memset(&rtp_session, 0, sizeof(rtp_hdr_session_t));

    rtp_status = rtp_validate_hdr(&rtp_session, buf, len);

    if (rtp_hdr_ok(rtp_status)) {
        ret = VQEC_DP_ERR_OK;
    }

    return ret;
}


/**
 * get the header of fec packet
 *
 * @param[in] pak pointer to fec packet
 *
 * @return the FEC header pointer
 */

UT_STATIC vqec_fec_hdr_t *vqec_fec_get_fec_hdr (vqec_pak_t *pak)
{
    char *ptr = NULL;

    if ( !pak || pak->type != VQEC_PAK_TYPE_FEC) {
        return (vqec_fec_hdr_t *)ptr;
    }

    ptr = vqec_pak_get_head_ptr(pak) + sizeof(rtpfasttype_t);

    return (vqec_fec_hdr_t *)ptr;
}

/**
 * detect the sending order of head end
 * we use three successive streams to detect HE sending
 * order. 
 * Here, we only consider two sending orders according 
 * to Pro-MPEG#3r2, Annex A and Annex B. 
 *
 * The method we used is as below:
 * We first collect three successive column FEC packets
 * then test some special pattern of the three packets.
 * Given the FEC packets with sn_base in the headers are
 * sn1, sn2, sn3, the special patterns are:
 *
 * For Annex B
 * If sn2-sn1 or sn3-sn2 == L*D-(L-1)
 * or sn2-sn1 == sn3-sn2 ==1
 *
 * For Annex A
 * either sn2-sn1 or sn3-sn2 == L+1
 * Since D >=4, these two pattens will never overlap.
 *
 * @param[in] fec         pointer to fec struct
 * @param[in] fec_seq32   RTP seq # of the FEC packet
 * @param[in] sn_base32   SN_BASE in the FEC header
 * @param[in] l_value     L value in FEC header
 * @param[in] d_value     D value in FEC header
 * 
 * @return the enum VQEC_DP_FEC_SENDING_ORDER

 */

UT_STATIC vqec_fec_sending_order_t 
vqec_fec_detect_sending_order (vqec_fec_t *fec,
                               vqec_seq_num_t fec_seq32,
                               vqec_seq_num_t sn_base32,
                               uint8_t l_value,
                               uint8_t d_value)
{
    boolean start_over = FALSE;
    int32_t sn_base_diff1, sn_base_diff2;

    VQEC_DP_ASSERT_FATAL(fec != NULL, "fec");

    /* first, collect 3 successive fec packets */
    switch (fec->num_paks_saved) {

        case 0:
            start_over = TRUE;
            break;

        case 1:
            if (l_value != fec->saved_L || d_value != fec->saved_D 
                || fec_seq32 != vqec_next_seq_num(fec->fst_fec_seq32)) {
                start_over = TRUE;
            } else {
                fec->snd_fec_seq32 = fec_seq32;
                fec->snd_sn_base32 = sn_base32;
                fec->num_paks_saved++;
            }
            break;

        case 2:
            if (l_value != fec->saved_L || d_value != fec->saved_D 
                || fec_seq32 != vqec_next_seq_num(fec->snd_fec_seq32)) {
                start_over = TRUE;
            } else {
                fec->num_paks_saved++;
            }
            break;
    }

    /* need to start over */
    if (start_over) {
        fec->saved_L = l_value;
        fec->saved_D = d_value;
        fec->fst_fec_seq32 = fec_seq32;
        fec->fst_sn_base32 = sn_base32;
        fec->num_paks_saved = 1;
    }

    /* have enough 3 packets to detect */
    if (fec->num_paks_saved == 3) {
        /* sending order detected */
        fec->sending_order_detected = TRUE;
        sn_base_diff1 = vqec_seq_num_sub(fec->snd_sn_base32,
                                         fec->fst_sn_base32);
        sn_base_diff2 = vqec_seq_num_sub(sn_base32, fec->snd_sn_base32);

        if (sn_base_diff1 == l_value * d_value - (l_value -1)||
            sn_base_diff2 == l_value * d_value - (l_value -1)||
            (sn_base_diff1 == 1 && sn_base_diff2 == 1)) {
            /* Annex B, clean and return */
            fec->num_paks_saved = 0;
            return VQEC_DP_FEC_SENDING_ORDER_ANNEXB;
        }

        if (sn_base_diff1 == l_value +1 || 
            sn_base_diff2 == l_value +1 ) {
            /* Annex A, clean and return */
            fec->num_paks_saved = 0;
            return VQEC_DP_FEC_SENDING_ORDER_ANNEXA;
        }

        fec->num_paks_saved = 0;
        return VQEC_DP_FEC_SENDING_ORDER_OTHER;
    }

    return VQEC_DP_FEC_SENDING_ORDER_NOT_DECIDED;
}
                

/**
 * Performs basic checks on the fec packet header.
 * The header check is based on CoP#3r2, we do not
 * check ssrc = 0 as described at SMPTE document
 *
 * @param[in] fec               pointer to fec module
 * @param[in] buff              fec packet buffer, include rtp header
 * @param[in] is_column_fec     if column TRUE, if row FALSE
 *
 * @return the validation errors, this error will be used to 
 * test what causes the header validation wrong.
 */
UT_STATIC vqec_fec_hdr_error_t vqec_fec_hdr_validation (vqec_fec_t *fec, 
                                                        char *buff,
                                                        boolean is_column_fec)
{
    fecfasttype_t *fec_hdr = NULL;
    vqec_pcm_t *pcm = NULL;
    int factor = MAX_LD;   /* set to 256 */
    /* used to determine sending order */
    vqec_seq_num_t fec_seq32, sn_base32;
    rtpfasttype_t *rtp_hdr = NULL;
    uint8_t l_value, d_value;
    vqec_fec_sending_order_t res = VQEC_DP_FEC_SENDING_ORDER_ANNEXB;
    rel_time_t fec_delay, pkt_time;
    vqec_fec_info_t *info = NULL;

    if (!fec || !buff) {
        return VQEC_FEC_HDR_ERROR_OTHER;
    }

    fec_hdr = (fecfasttype_t *)(buff + sizeof(rtpfasttype_t));

    pcm = fec->pcm;
    if (!pcm) {
        return VQEC_FEC_HDR_ERROR_OTHER;
    }
    
    if (FEC_X_BIT(fec_hdr)) {
         return VQEC_FEC_HDR_WRONG_VERSION;
    }

    if (!FEC_E_BIT(fec_hdr)) {
        return VQEC_FEC_HDR_WRONG_VERSION;
    }
    /**
     * check if the type of FEC is supported, COP#3r2 only XOR
     */
    if (FEC_TYPE(fec_hdr) != VQEC_FEC_TYPE_XOR) {
        return VQEC_FEC_HDR_UNSUPPORTED_TYPE;
    }

    info = &fec->fec_info;
    if (is_column_fec) {
        if (FEC_D_BIT(fec_hdr)) {
            return VQEC_FEC_HDR_WRONG_FLOW;
        }

        /* check L and D only at column FEC, enough */
        l_value = FEC_OFFSET(fec_hdr);
        d_value = FEC_NA(fec_hdr);

        if (l_value > MAX_L || l_value < MIN_L) {
            return VQEC_FEC_HDR_INVALID_L;
        }
        if (d_value > MAX_D || d_value < MIN_D) {
            return VQEC_FEC_HDR_INVALID_D;
        }

        /**
         * L and D values are set to 0 at initilization, 
         * if fec streams are availble, record the  
         * right L and D value from the fec packet header.
         * The fec_delay is also updated according to the decoded
         * L and D values.
         * 
         * Use the sending_order_detected flag to detect the sending
         * order of a new channel at the beginning of a channel change.
         *
         * While watching a channel, if only the channel's FEC sending 
         * order is changed, the sending order can not be detected while 
         * in this channel. The sending order will be detected next time
         * tune to this channel. The detected sending order will take effect 
         * at the next channel change. 
         * 
         * The reason why we do not detect sending order for each FEC packet
         * is that we assume, most of the time, the sending order's change
         * is along with the L and D changes.  
         */
        if (l_value != info->fec_l_value || d_value != info->fec_d_value
            || !fec->sending_order_detected) {
            /* 2-D FEC check min L value */
            if (fec->fec_params.avail_streams == VQEC_FEC_STREAM_AVAIL_2D) {
                if (l_value < MIN_L_IN_2D) {
                    return VQEC_FEC_HDR_INVALID_L_2D;
                }
            }
            if (l_value * d_value > MAX_LD) {
                return VQEC_FEC_HDR_INVALID_LD;
            }

            /**
             * Detect sending order from HE, and then set the appropriate 
             * jitter buffer size. 
             *
             * Each time the L and/or D value changes, there is also a 
             * possibility that the sending order is also changed. Therefore,
             * We need to detect the sending order, accordingly. 
             *
             * Here, we assume only two types of sending orders, Annex A 
             * and Annex B from CoP#3r2. If in the future, more sending
             * orders are supported, need to revisit this part. 
             *
             */
            rtp_hdr = (rtpfasttype_t *)buff;
            fec_seq32 = vqec_seq_num_nearest_to_rtp_seq_num(
                fec->last_rx_seq_num_column,
                ntohs(rtp_hdr->sequence));
            sn_base32 = vqec_seq_num_nearest_to_rtp_seq_num(
                pcm->last_rx_seq_num,
                ntohs(fec_hdr->snbase_low_bits));

            res = vqec_fec_detect_sending_order(fec, fec_seq32, sn_base32,
                                                l_value,d_value);
            switch (res) {

                case VQEC_DP_FEC_SENDING_ORDER_ANNEXA:
                    factor = l_value*d_value + l_value;
                    break;

                case VQEC_DP_FEC_SENDING_ORDER_ANNEXB:
                case VQEC_DP_FEC_SENDING_ORDER_OTHER:
                    factor = 2*l_value*d_value;
                    break;

                case VQEC_DP_FEC_SENDING_ORDER_NOT_DECIDED:
                    return VQEC_FEC_HDR_OK;

            }
            /* update L and D value, no need to come back in same L and D */
            info->fec_l_value = l_value;
            info->fec_d_value = d_value;
            info->fec_order = res;
            vqec_dpchan_fec_update_notify(fec->chanid);

            /**
             * Update the fec delay.
             *
             * The rate estimation based on RTP timestam happens
             * in the beginning of a channel change. Usually, VQE-C 
             * should have done the ts_calculation when FEC need to
             * update the buffer delay. If in some special cases, the
             * ts_calculation is not done, we use the avg_pkt_time
             * from SDP.   
             */
            if (pcm->ts_calculation_done && 
                TIME_CMP_R(lt, pcm->avg_pkt_time, 
                           pcm->new_rtp_ts_pkt_time)) {
                pkt_time = pcm->new_rtp_ts_pkt_time;
            } else {
                pkt_time = pcm->avg_pkt_time;
            }
            fec_delay = TIME_MULT_R_I(pkt_time, factor);            
            vqec_pcm_update_fec_delay(pcm, fec_delay);
        }
    } else { /* row FEC */
        if (!FEC_D_BIT(fec_hdr)) {
            return VQEC_FEC_HDR_WRONG_FLOW;
        }
        if (FEC_OFFSET(fec_hdr) != 1) {
            return VQEC_FEC_HDR_WRONG_COLUMN_OFFSET;
        }
    }

    return VQEC_FEC_HDR_OK;
}   

/**
 * This function create fec module.
 * The caller first fills the fec_param stucture with
 * parameters from SDP and sysconfig. During creation, fec 
 * structure is dynamic allocated, initialized and then returned
 * 
 * @param[in] fec_param  pointer to fec parameter structure
 * @param[in] pcm Pointer to the associated pcm structure.
 *
 * @return    initialized fec module, otherwise NULL
 */
vqec_fec_t *vqec_fec_create (vqec_fec_params_t *fec_param,
                             vqec_pcm_t *pcm,
                             uint32_t chanid)
{
    vqec_fec_t *fec = NULL;
 
    if (!fec_param || !fec_param->is_fec_enabled || !pcm) {
        return NULL;
    }

    fec = (vqec_fec_t *)zone_acquire(s_vqec_fec_pool);
    if (!fec) {
        return NULL;
    }
    memset(fec, 0, sizeof(vqec_fec_t));
    fec->pcm = pcm;
    fec->chanid = chanid;

    switch (fec_param->avail_streams) {

        case VQEC_FEC_STREAM_AVAIL_NONE:
            break;

        case VQEC_FEC_STREAM_AVAIL_1D:    /* 1-D FEC */
            fec->fec_pak_seq_column =  vqec_pak_seq_create_in_pool(
                                    s_vqec_pak_seq_pool,
                                    VQEC_FEC_STREAM_RING_BUFFER_BUCKET_BITS);
            if (!fec->fec_pak_seq_column) {
                break;
            }
            VQEC_DP_ASSERT_FATAL(
                vqec_pak_seq_get_num_paks(fec->fec_pak_seq_column) == 0,
                "fec");

            memcpy(&(fec->fec_params), fec_param, sizeof(vqec_fec_params_t));

            return fec;

        case VQEC_FEC_STREAM_AVAIL_2D:
            fec->fec_pak_seq_column =  vqec_pak_seq_create_in_pool(
                                   s_vqec_pak_seq_pool,
                                   VQEC_FEC_STREAM_RING_BUFFER_BUCKET_BITS);
            if (!fec->fec_pak_seq_column) {
                break;
            }
            fec->fec_pak_seq_row =  vqec_pak_seq_create_in_pool(
                                   s_vqec_pak_seq_pool, 
                                   VQEC_FEC_STREAM_RING_BUFFER_BUCKET_BITS);
            if (!fec->fec_pak_seq_row) {
                break;
            }
            VQEC_DP_ASSERT_FATAL(
                vqec_pak_seq_get_num_paks(fec->fec_pak_seq_column) == 0,
                "fec");
            VQEC_DP_ASSERT_FATAL(
                vqec_pak_seq_get_num_paks(fec->fec_pak_seq_row) == 0,
                "fec");

            memcpy(&(fec->fec_params), fec_param, sizeof(vqec_fec_params_t));

            return fec;

        default:
           VQEC_DP_SYSLOG_PRINT(ERROR,"SDP FEC parameters setup wrong\n");
           break;
    }  /* switch */
   
    vqec_fec_destroy(fec);
 
    return NULL;
}


/**
 * destroy fec module, clean the whole module, and free the fec buffer
 *
 * @param[in] fec   pointer to fec module
 */

void vqec_fec_destroy (vqec_fec_t *fec)
{
    if (!fec) {
        return;
    }
    if (fec->fec_pak_seq_column) {
        vqec_pak_seq_destroy_in_pool(s_vqec_pak_seq_pool,
                                     fec->fec_pak_seq_column);
    }
    if (fec->fec_pak_seq_row) {
        vqec_pak_seq_destroy_in_pool(s_vqec_pak_seq_pool,
                                     fec->fec_pak_seq_row);
    }
    zone_release(s_vqec_fec_pool, fec);
}

/**
 * flush fec module
 * flush FEC pak_sequence for column or row FEC
 * need to seperate row and column, since sometimes, we only need to flush 
 * one row or column.

 * @param[in] fec               pointer to fec module
 * @param[in] is_column_fec     if column TRUE, if row FALSE
 *
 * @return                      TRUE if success, otherwise FALSE
 */

boolean vqec_fec_flush (vqec_fec_t *fec, 
                        boolean is_column_fec)
{
    if (!fec) {
        return FALSE;
    }

    /* flush the sequencer */
    if (is_column_fec) {   /*column fec */
        if (fec->fec_pak_seq_column) {
            vqec_pak_seq_destroy_in_pool(s_vqec_pak_seq_pool,
                                         fec->fec_pak_seq_column);
            fec->fec_pak_seq_column = NULL;
        }
        fec->fec_pak_seq_column = vqec_pak_seq_create_in_pool(
            s_vqec_pak_seq_pool,
            VQEC_FEC_STREAM_RING_BUFFER_BUCKET_BITS);
        if (!fec->fec_pak_seq_column) {
            return FALSE;
        }

        if (vqec_pak_seq_get_num_paks(fec->fec_pak_seq_column) != 0) {
            return FALSE;
        }
        fec->last_rx_seq_num_column = 
            vqec_seq_num_mark_rtp_seq_num_discont(fec->last_rx_seq_num_column);
    } else {  /* row fec */
        if (fec->fec_pak_seq_row) {
            vqec_pak_seq_destroy_in_pool(s_vqec_pak_seq_pool,
                                         fec->fec_pak_seq_row);
            fec->fec_pak_seq_row = NULL;
        }
        fec->fec_pak_seq_row = vqec_pak_seq_create_in_pool(
            s_vqec_pak_seq_pool,
            VQEC_FEC_STREAM_RING_BUFFER_BUCKET_BITS);
        if (!fec->fec_pak_seq_row) {
            return FALSE;
        }

        if (vqec_pak_seq_get_num_paks(fec->fec_pak_seq_row) != 0) {
            return FALSE;
        }
        fec->last_rx_seq_num_row =
            vqec_seq_num_mark_rtp_seq_num_discont(fec->last_rx_seq_num_row);
    }
  
    return TRUE;
}


/**
 * This function decodes received packets using XOR fec.
 * The decoding is word by word. 
 * All the related video packets as well as the FEC packet are 
 * input to **received_pkts, must allocate a buffer for recovered
 * packet and pass to the function. 
 *
 * @param[in] received_pkts pointer to received packets pointers
 * @param[in] num_pkts      total number of packets received
 * @param[in] pkt_size      size of each packet in bytes
 * @param[in] recovered_pkt pointer to the decoded FEC packet
 *
 * @return TRUE is decoding success, otherwise FALSE
 */

UT_STATIC boolean vqec_fec_decode_xor_word (char **received_pkts,
                                            uint16_t num_pkts,
                                            uint16_t pkt_size,
                                            char *recovered_pkt)
{
  uint16_t i,j;
  uint16_t num_words = 0;
  const uint16_t WORD_LEN = 4;
  uint16_t remain_bytes = 0;
  char *ptr1 = NULL;
  char *ptr2 = NULL;

    
  if (!received_pkts || !recovered_pkt) {
      return FALSE;
  }

  /**
   * number of receivered packets should be at least two 
   */
  if (num_pkts < 2 || pkt_size == 0) {
      return FALSE;
  }

  /**
   * all received packets in fec block should be present 
   */
  for (i = 0; i < num_pkts; i++) {
      if (received_pkts[i] == NULL) {
          return FALSE;
      }
  }

  remain_bytes = pkt_size % WORD_LEN;
  num_words = (pkt_size - remain_bytes)/WORD_LEN;

  if (num_words == 0) {
     /* byte based decode*/
      memcpy(recovered_pkt, received_pkts[0], pkt_size);

      for (i = 1; i < num_pkts; i++) {
          for (j = 0; j < pkt_size; j++) {
              *(recovered_pkt+j) = *(recovered_pkt+j) ^ *(received_pkts[i]+j);
          }
      }
  } else {
     /* word based decoding */
      memcpy(recovered_pkt, received_pkts[0], pkt_size);

      for (i = 1; i < num_pkts; i++) {
          /* decode the received packets bitwise*/   
          for (j = 0; j < num_words; j++) {
              *((uint32_t *)recovered_pkt+j) = 
                  *((uint32_t *)recovered_pkt+j) ^
                  *((uint32_t *)received_pkts[i]+j);
          }
          /* decode the remaining bytes */
          if (remain_bytes != 0) {
              ptr1 = recovered_pkt + num_words * WORD_LEN;
              ptr2 = received_pkts[i] + num_words *WORD_LEN;
              for (j = 0; j < remain_bytes; j++) {
                  *(ptr1+j) = *(ptr1+j) ^ *(ptr2+j);
              }
          }
      }
  }

  return TRUE;
}

/*
 * check to see if we can insert this packet into our ring buffer(fec)
 *
 * @param[in] fec             pointer to fec module
 * @param[in] pak             buff for FEC packet
 * @param[in] is_column_fec   if column TRUE, if row FALSE
 *
 * @return the check stutus
 *
 */
UT_STATIC vqec_fec_insert_error_t 
vqec_fec_insert_packet_check (vqec_fec_t *fec, 
                              const vqec_pak_t *pak,
                              boolean is_column_fec)
{
    vqec_seq_num_t seq_num;
    const int seq_num_gap_max = VQEC_FEC_MAX_GAP_SIZE;
    vqec_pak_t *old_pak = NULL;
    vqec_pak_seq_t *fec_pak_seq = NULL;
    vqec_seq_num_t head, tail;
    vqec_fec_stats_t *fec_stats = NULL;

    if( !fec || !pak) {
        return VQEC_FEC_INSERT_ERROR_INVALID_ARG;
    }

    fec_stats = &fec->fec_stats;

    if (is_column_fec) {  /* column fec */
        if (!fec->fec_pak_seq_column) {
            return VQEC_FEC_INSERT_ERROR_INVALID_ARG;
        }
        fec_pak_seq = fec->fec_pak_seq_column;
        head = fec->fec_column_head;
        tail = fec->fec_column_tail;
        
    } else {    /* row fec */
        if (!fec->fec_pak_seq_row) {
            return VQEC_FEC_INSERT_ERROR_INVALID_ARG;
        } 
        fec_pak_seq = fec->fec_pak_seq_row;
        head = fec->fec_row_head;
        tail = fec->fec_row_tail;
       
    }

    seq_num = vqec_fec_get_seq_num(pak);

    old_pak = vqec_pak_seq_find(fec_pak_seq, seq_num);

    if (old_pak) {
        fec_stats->fec_duplicate_paks++;
        return VQEC_FEC_INSERT_ERROR_DUPLICATE_PAK;  
    }

    if (vqec_pak_seq_get_num_paks(fec_pak_seq) > 0) {
        /* In the same sequence number space ? */
        if (vqec_seq_num_ge(pak->seq_num, 
                            vqec_seq_num_add(tail, seq_num_gap_max))) {
            return VQEC_FEC_INSERT_ERROR_BAD_RANGE_PAK;
        }
  
        if (vqec_seq_num_le(pak->seq_num, 
                            vqec_seq_num_sub(head, seq_num_gap_max))) {
            return VQEC_FEC_INSERT_ERROR_BAD_RANGE_PAK;
        }
    }

    return VQEC_FEC_INSERT_OK;
}


/* 
 * insert fec packet into fec module pak_seq
 *
 * @param[in] fec               pointer to fec module
 * @param[in] pak               pak buff for FEC packet
 * @param[in] is_column_fec     if column TRUE, if row FALSE
 *
 * @return: TRUE if we successfully put the packet in fec
 * otherwise FALSE.
 */
UT_STATIC 
vqec_fec_insert_error_t vqec_fec_insert_packet (vqec_fec_t *fec, 
                                                vqec_pak_t *pak,
                                                boolean is_column_fec)
{
    vqec_seq_num_t seq_num, largest_seq_num_recved;
    vqec_fec_insert_error_t ret = VQEC_FEC_INSERT_OK;
    vqec_pak_seq_t *fec_pak_seq = NULL;
    vqec_seq_num_t head, tail;

    if (!fec || !pak) {
        return VQEC_FEC_INSERT_ERROR_INVALID_ARG;
    }

    if (is_column_fec) {  /* column fec */
        if (!fec->fec_pak_seq_column) {
            return VQEC_FEC_INSERT_ERROR_INVALID_ARG;
        } 
        /* change pak seq# to 32 bits */
        pak->seq_num = vqec_seq_num_nearest_to_rtp_seq_num(
            fec->last_rx_seq_num_column, pak->seq_num);

        fec_pak_seq = fec->fec_pak_seq_column;
        head = fec->fec_column_head;
        tail = fec->fec_column_tail;
        largest_seq_num_recved = fec->largest_seq_num_recved_column;
    } else {    /* row fec */
        if (!fec->fec_pak_seq_row) {
            return VQEC_FEC_INSERT_ERROR_INVALID_ARG;
        } 
        /* change pak seq# to 32 bits */
        pak->seq_num = vqec_seq_num_nearest_to_rtp_seq_num(
            fec->last_rx_seq_num_row, pak->seq_num);

        fec_pak_seq = fec->fec_pak_seq_row;           
        head = fec->fec_row_head;
        tail = fec->fec_row_tail;
        largest_seq_num_recved = fec->largest_seq_num_recved_row;
    }

    /**
     * Handle overflow in fec - this is done at the beginning of this
     * function to minimize side-effects of the fec flush operation on cached
     * state in automatic variables. If the fec is full, simply flush it.
     */
    if (vqec_pak_seq_get_num_paks(fec_pak_seq) == 
        (1 << VQEC_FEC_STREAM_RING_BUFFER_BUCKET_BITS)) {
        (void)vqec_fec_flush(fec, is_column_fec);
    }

    seq_num = vqec_fec_get_seq_num(pak);

    /**
     * Check if we can insert the packet
     */
    ret = vqec_fec_insert_packet_check(fec, pak, is_column_fec);

    if (ret != VQEC_FEC_INSERT_OK) {
        return ret;
    }

    if (vqec_pak_seq_insert(fec_pak_seq, pak)) {

        /* Need to update the head and tail */
        if (vqec_pak_seq_get_num_paks(fec_pak_seq) == 1) {
            head = seq_num;
            tail = seq_num;
        } else {            
            if (vqec_seq_num_lt(seq_num, head)) {
                if (head != vqec_next_seq_num(seq_num)) {
                    fec->fec_stats.fec_gap_detected += 
                        vqec_seq_num_sub(vqec_pre_seq_num(head),
                                         vqec_next_seq_num(seq_num)) + 1;
                }
                head = seq_num;
                /**
                 * This packet is inserted ahead of the head in the past.
                 * Hence, it was reordered. The reordered flag is set.
                 */
                VQEC_PAK_FLAGS_SET(&pak->flags, 
                                   VQEC_PAK_FLAGS_RX_REORDERED);
            } else if (vqec_seq_num_gt(seq_num, largest_seq_num_recved)) {
                if (seq_num != vqec_next_seq_num(largest_seq_num_recved)) {
                    fec->fec_stats.fec_gap_detected +=
                        vqec_seq_num_sub(vqec_pre_seq_num(seq_num),
                            vqec_next_seq_num(largest_seq_num_recved)) + 1;
                }

                tail = seq_num;
            } else {
                if (vqec_seq_num_gt(seq_num, tail)) {
                    tail = seq_num;
                }
                VQEC_PAK_FLAGS_SET(&pak->flags, 
                                   VQEC_PAK_FLAGS_RX_REORDERED);
            }            
        }
        if (vqec_seq_num_lt(largest_seq_num_recved, tail)) {
            largest_seq_num_recved = tail;
        }
    }

    /* update the fec structure */
   if (is_column_fec) {
        fec->fec_column_head = head;
        fec->fec_column_tail = tail;
        fec->last_rx_seq_num_column = pak->seq_num;
        fec->largest_seq_num_recved_column = largest_seq_num_recved;
    } else {
        fec->fec_row_head = head;
        fec->fec_row_tail = tail;
        fec->last_rx_seq_num_row = pak->seq_num;
        fec->largest_seq_num_recved_row = largest_seq_num_recved;
    }
    
    return ret;
}

/**
 * find the next packet vs current one at pak_seq
 * If pak is NULL, return the first one.
 * Be careful of the deadlock, the caller should hold the lock
 *
 * @param[in] fec               pointer to fec module
 * @param[in] pak               buff for FEC packet
 * @param[in] is_column_fec     if column TRUE, if row FALSE
 *
 * @return the next packet pointer
 */
UT_STATIC 
vqec_pak_t *vqec_fec_find_next_packet_internal (vqec_fec_t *fec, 
                                                const vqec_pak_t *pak,
                                                boolean is_column_fec)
{    
    vqec_seq_num_t seq_num;
    vqec_seq_num_t temp_seq_num;
    vqec_pak_t *next_pak = NULL;
    vqec_pak_seq_t *fec_pak_seq = NULL;
    vqec_seq_num_t head, tail;

    if (!fec) {
        return NULL;
    }

    if (is_column_fec) {  /* column fec */
        if (!fec->fec_pak_seq_column) {
            return NULL;
        }
        fec_pak_seq = fec->fec_pak_seq_column;
        head = fec->fec_column_head;
        tail = fec->fec_column_tail;
       
    } else {    /* row fec */
        if (!fec->fec_pak_seq_row) {
            return NULL;
        } 
        fec_pak_seq = fec->fec_pak_seq_row;
        head = fec->fec_row_head;
        tail = fec->fec_row_tail;
    }

    if (!vqec_pak_seq_get_num_paks(fec_pak_seq)) {
        return NULL;
    }

    if (!pak) {
        next_pak = vqec_pak_seq_find(fec_pak_seq, head);
    } else if (!vqec_pak_seq_find(fec_pak_seq, 
                                  vqec_fec_get_seq_num(pak))) {
        /* This packet is not in our cache manager, don't know what
           caller want */
        next_pak = NULL;
    } else {

        seq_num = vqec_fec_get_seq_num(pak);
        temp_seq_num = vqec_next_seq_num(seq_num);
        
        /*
         * If this is the last packet, don't need to go through the whole 
         */
        if (seq_num == tail) {            
            return NULL;
        }

        while (TRUE) {
            next_pak = vqec_pak_seq_find(fec_pak_seq, 
                                                   temp_seq_num);
            
            if (next_pak) {
                break;
            } else {
                temp_seq_num = vqec_next_seq_num(temp_seq_num);
                if (vqec_seq_num_gt(temp_seq_num,tail)) {
                    break;
                }
                if (vqec_pak_seq_find_bucket(fec_pak_seq, 
                           temp_seq_num) == vqec_pak_seq_find_bucket(
                               fec_pak_seq, seq_num)) {
                    break;
                }
            }
        }
    }

    return next_pak;
}

/**
 * find the previous packet vs. current one at fec pak_seq
 * If pak is NULL, return the last one.
 * Be careful of the deadlock, the caller should hold the lock
 *
 * @param[in] fec            pointer to fec module
 * @param[in] pak            buff for FEC packet
 * @param[in] is_column_fec  if column TRUE, if row FALSE
 *
 * @return the previous packet pointer
 */
UT_STATIC
vqec_pak_t * vqec_fec_find_pre_packet_internal (vqec_fec_t *fec, 
                                                const vqec_pak_t *pak,
                                                boolean is_column_fec)
{
    vqec_seq_num_t seq_num;
    vqec_seq_num_t temp_seq_num;
    vqec_pak_t *pre_pak = NULL;
    vqec_pak_seq_t *fec_pak_seq = NULL;
    vqec_seq_num_t head, tail;

    if (!fec) {
        return NULL;
    }

    if (is_column_fec) {  /* column fec */
        if (!fec->fec_pak_seq_column) {
            return NULL;
        } 
        fec_pak_seq = fec->fec_pak_seq_column;
        head = fec->fec_column_head;
        tail = fec->fec_column_tail;
       
    } else {    /* row fec */
        if (!fec->fec_pak_seq_row) {
            return NULL;
        } 
        fec_pak_seq = fec->fec_pak_seq_row;
        head = fec->fec_row_head;
        tail = fec->fec_row_tail;
    }

    if (!vqec_pak_seq_get_num_paks(fec_pak_seq)) {
        return NULL;
    }

    if (!pak) { /* pak is NULL, return last packet */
        pre_pak = vqec_pak_seq_find(fec_pak_seq, tail);
    } else if (!vqec_pak_seq_find(fec_pak_seq, vqec_fec_get_seq_num(pak))) {
        /**
         * This packet is not in our cache manager, don't know what
         * caller want
         */
        pre_pak = NULL;
    } else {
        seq_num = vqec_fec_get_seq_num(pak);
        temp_seq_num = vqec_pre_seq_num(seq_num);

        if (seq_num == head) {
            return NULL;
        }

        while (TRUE) {
            pre_pak = vqec_pak_seq_find(fec_pak_seq, temp_seq_num);
            
            if (pre_pak) {
                break;
            } else {
                temp_seq_num = vqec_pre_seq_num(temp_seq_num);
                if (vqec_seq_num_lt(temp_seq_num, head)) {
                    break;
                }
                if (vqec_pak_seq_find_bucket(fec_pak_seq, 
                            temp_seq_num) == vqec_pak_seq_find_bucket(
                                fec_pak_seq, seq_num)) {
                    break;
                }
            }
        }
    }

    return pre_pak;
}


/**
 * remove a packet from fec pak_seq
 *
 * @param[in] fec           pointer to fec module
 * @param[in] pak           buff for FEC packet that will be removed
 * @param[in] is_column_fec if column TRUE, if row FALSE
 *
 * @return TRUE if successful removed, otherwise FALSE
 */

UT_STATIC boolean vqec_fec_remove_packet (vqec_fec_t *fec, 
                                          vqec_pak_t *pak,
                                          boolean is_column_fec)
{
    vqec_seq_num_t seq_num;
    boolean ret = TRUE;    
    const vqec_pak_t *next_pak;
    const vqec_pak_t *pre_pak;
    vqec_pak_seq_t *fec_pak_seq = NULL;
    vqec_seq_num_t head, tail;

    if (!fec || !pak) {
        return FALSE;
    }

    if (is_column_fec) {  /* column fec */
        if (!fec->fec_pak_seq_column) {
            return FALSE;
        } 
        fec_pak_seq = fec->fec_pak_seq_column;
        head = fec->fec_column_head;
        tail = fec->fec_column_tail;
        
    } else {    /* row fec */
        if (!fec->fec_pak_seq_row) {
            return FALSE;
        }
        fec_pak_seq = fec->fec_pak_seq_row;
        head = fec->fec_row_head;
        tail = fec->fec_row_tail;
    }
  
    seq_num = vqec_fec_get_seq_num(pak);

    if (pak == vqec_pak_seq_find(fec_pak_seq, seq_num)) {

        if (head == seq_num &&tail == seq_num) {
            VQEC_DP_ASSERT_FATAL(vqec_pak_seq_get_num_paks(fec_pak_seq) == 1,
                                 "fec");
        } else if (head == seq_num){
            next_pak = vqec_fec_find_next_packet_internal(
                fec, pak, is_column_fec);
            if (next_pak) {
                head = vqec_fec_get_seq_num(next_pak);
            } else {
                return FALSE;
            }
        } else if (tail == seq_num) {
            
            pre_pak = vqec_fec_find_pre_packet_internal(
                fec, pak, is_column_fec);
            if (pre_pak) {
                tail = vqec_fec_get_seq_num(pre_pak);
            } else {
                return FALSE;
            }
        }

        if (vqec_pak_seq_delete(fec_pak_seq, seq_num)) { 
            /* update the fec structure */
            if(is_column_fec){
                fec->fec_column_head = head;
                fec->fec_column_tail = tail;
            } else {
                fec->fec_row_head = head;
                fec->fec_row_tail = tail;
            }
            ret = TRUE;
        } else {
            ret = FALSE;
        }
    } else {
        ret = FALSE;
    }

    return ret;
}

/**
 * This function parses decode error after vqec_fec_decode_basic()
 * and properly processes the FEC packet just used.
 *
 * @param[in] dec_res       decode error results 
 * @param[in] is_column_fec column decoding (TRUE) or row decoding(FALSE)
 * @param[in] fec           fec module pointer
 * @param[in] fec_pak       fec packet pointer
 */

UT_STATIC void vqec_fec_parse_dec_error (const vqec_fec_dec_error_t dec_res,
                                         const boolean is_column_fec,
                                         vqec_fec_t *fec,
                                         vqec_pak_t *fec_pak)
{
    vqec_fec_stats_t *fec_stats = NULL;

    if (!fec || !fec_pak) {
        return;
    }
    fec_stats = &fec->fec_stats;

    /* process 1-D FEC return */
    if (fec->fec_params.avail_streams == VQEC_FEC_STREAM_AVAIL_1D 
       && is_column_fec == TRUE) {

        switch(dec_res) {
            case VQEC_FEC_DEC_OK:
                 if (vqec_fec_remove_packet(fec,fec_pak,is_column_fec)){
                    fec_stats->fec_recovered_paks++;
                }
                break;
            case VQEC_FEC_DEC_LATE_PAK:
                /**
                 * If the pak is looked only once by FEC and return late_pak,
                 * we remove this pak immediatelly.
                 * If the pak is looked more than 1 time by FEC module and
                 * return late_pak, this must be an unrecoverable pak.
                 * We hold a pak for one more timer interval (40 ms) if it 
                 * is an unrecoverable pak for out of order paks.
                 */ 
                if (fec_pak->fec_touched > 1) {
                    if (vqec_fec_remove_packet(fec,fec_pak,is_column_fec) &&
                        VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                        fec_stats->fec_unrecoverable_paks++;
                    }
                } else {
                    if (vqec_fec_remove_packet(fec,fec_pak,is_column_fec)) {
                        fec_stats->fec_late_paks++;
                    }
                }
                break;
            case VQEC_FEC_DEC_NOT_NEEDED:
                if (vqec_fec_remove_packet(fec,fec_pak,is_column_fec) &&
                    VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                    fec_stats->fec_dec_not_needed++;
                }
                break;
            case VQEC_FEC_DEC_UNRECOVERABLE:
                /**
                 * need to keep the FEC packet for two timer intervals, 
                 * in case of out of order primary packet
                 */
                if (fec_pak->fec_touched > 1) {
                    if (vqec_fec_remove_packet(fec,fec_pak,is_column_fec) &&
                        VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                        fec_stats->fec_unrecoverable_paks++;
                    }
                }
                break;
            case VQEC_FEC_DEC_MEM_ALLOC_WRONG:
            case VQEC_FEC_DEC_FAILURE:
            case VQEC_FEC_DEC_RTP_VALIDATE_WRONG:
            case VQEC_FEC_DEC_PAK_TOO_EARLY:
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC, 
                              "fec_drops_other, code: %d\n",
                              dec_res);
                if (vqec_fec_remove_packet(fec,fec_pak,is_column_fec)) {
                    fec_stats->fec_drops_other++;
                }
                break;
            default:
                break;
        }
    }

    /* process 2-D FEC */
    if (fec->fec_params.avail_streams == VQEC_FEC_STREAM_AVAIL_2D ) {
        switch (dec_res) {
            case VQEC_FEC_DEC_OK:
                if (vqec_fec_remove_packet(fec,fec_pak,is_column_fec)) {
                    fec_stats->fec_recovered_paks++;
                }
                break;
            case VQEC_FEC_DEC_LATE_PAK:
                if (fec_pak->fec_touched > 1) {
                    if (vqec_fec_remove_packet(fec,fec_pak,is_column_fec) &&
                        VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                        fec_stats->fec_unrecoverable_paks++;
                    }
                } else {
                    if (vqec_fec_remove_packet(fec,fec_pak,is_column_fec)) {
                        fec_stats->fec_late_paks++;
                    }
                }
                break;
            case VQEC_FEC_DEC_NOT_NEEDED:
                if (vqec_fec_remove_packet(fec,fec_pak,is_column_fec) &&
                    VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                    fec_stats->fec_dec_not_needed++;
                }
                break;
            case VQEC_FEC_DEC_MEM_ALLOC_WRONG:
            case VQEC_FEC_DEC_FAILURE:
            case VQEC_FEC_DEC_RTP_VALIDATE_WRONG:
            case VQEC_FEC_DEC_PAK_TOO_EARLY:
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC, 
                              "fec_drops_other, code: %d\n",
                              dec_res);       
                if (vqec_fec_remove_packet(fec,fec_pak,is_column_fec)) {
                    fec_stats->fec_drops_other++;
                }
                break;
            default:
                break;
        }
    }
}

/**
 * This is a general function for decoding both column and a row. 
 * According to CoP#3r2,only one packet can be corrected in a column 
 * or row. Therefore, we do decoding if there is only one packet loss. 
 * This can minimize the number of decoding trials, to avoid unecessary 
 * computation. The recovered packet is directly inserted into pcm as repair. 
 * This function is called at event handler. 
 * The decoding process is described at RFC 2733.
 *
 * @param[in] fec       pointer to fec module
 * @param[in] fec_pak   pointer to received packet
 * 
 * @return error code defined at vqec_fec_dec_error_t
 *
 */

UT_STATIC vqec_fec_dec_error_t vqec_fec_decode_basic (vqec_fec_t *fec, 
                                                      vqec_pak_t *fec_pak)
{
    uint16_t sn_base, offset, na_bits, temp_seq; 
    vqec_seq_num_t sn_base32, temp_seq32;
    uint16_t num_loss, pt_recovery;
    uint16_t lost_seq_num = 0;
    uint16_t i,j, *length[MAX_D], length_recovery, pkt_size;
    uint16_t payload_len[MAX_D];
    rtpfasttype_t fec_rtp_header, *header_blks[MAX_D];
    rtpfasttype_t *rtp_header = NULL;
    vqec_pcm_t *pcm = NULL;
    vqec_pak_seq_t *pcm_pak_seq = NULL;
    fecfasttype_t *fec_hdr = NULL;
    vqec_pak_t *decode_blks[MAX_D];
    vqec_pak_t *tmp_pak = NULL;
    vqec_pak_t *recovered = NULL;
    char *payload_blks[MAX_D];
    vqec_dp_error_t status = VQEC_DP_ERR_OK;
    vqec_seq_num_t lost_seq_nums[MAX_D];
    int num_paks = 0;

    if (!fec || !fec_pak) {
        return VQEC_FEC_DEC_MEM_ALLOC_WRONG;
    }

    /* initilize all pointers */    
    memset(decode_blks, 0, sizeof(decode_blks[0])*MAX_D);
    memset(header_blks, 0, sizeof(header_blks[0])*MAX_D);
    memset(payload_blks, 0, sizeof(payload_blks[0])*MAX_D);
    memset(length, 0, sizeof(length[0])*MAX_D);

    memset(&fec_rtp_header,0,sizeof(rtpfasttype_t));
    memset(lost_seq_nums, 0, sizeof(vqec_seq_num_t) * MAX_D);

    /**
     * first need to check the fec header to figure out which
     * packets are associated with this FEC packets 
     */

    fec_hdr = (fecfasttype_t *) fec_pak->fec_hdr; 
    VQEC_DP_ASSERT_FATAL(fec_hdr, "fec");
    sn_base = ntohs(fec_hdr->snbase_low_bits);
    offset = FEC_OFFSET(fec_hdr);
    na_bits = FEC_NA(fec_hdr);   /* # of packets in a column or row */
    pt_recovery = (uint16_t)FEC_PT_RECOVERY(fec_hdr);

    pcm = fec->pcm;
    if (pcm == NULL) {
        return VQEC_FEC_DEC_MEM_ALLOC_WRONG;
    } else {
        pcm_pak_seq = pcm->pak_seq;
    }

    /**
     * convert the 16 bit seq numb to 32 bit seq numb
     * to find packets in pcm pak_seq.
     */
    sn_base32 = 
        vqec_seq_num_nearest_to_rtp_seq_num(pcm->last_rx_seq_num,
                                            sn_base);
     /** 
     * some data paks are already gone, late fec pak
     */
    fec_pak->fec_touched++;
    if(vqec_seq_num_lt(sn_base32, pcm->head)){
        return VQEC_FEC_DEC_LATE_PAK;
    }

    /*the last seq # in a column or row*/
    temp_seq = sn_base + (na_bits - 1) * offset;

    /* convert the 16 bit seq numb to 32 bit seq numb*/
    temp_seq32 = 
        vqec_seq_num_nearest_to_rtp_seq_num(pcm->last_rx_seq_num,
                                            temp_seq);

    if (vqec_seq_num_lt(pcm->tail,temp_seq32)) {
        /*
         * still need to wait for several data packs, 
         * fec pak too early.
         * if the packet is 2 FEC blocks earlier, 
         * remove this packet 
         */
        if (vqec_seq_num_sub(temp_seq32, pcm->tail) > 
            2 * fec->fec_info.fec_l_value * fec->fec_info.fec_d_value) {
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC, 
                          "FEC pak too early: "
                          " seq32  = %u, seq = %d, "
                          " pcm: tail = %u, last_rx_seq_num  = %u ",
                          temp_seq32, 
                          temp_seq,
                          pcm->tail, 
                          pcm->last_rx_seq_num);
            return VQEC_FEC_DEC_PAK_TOO_EARLY;
        } else {
            return VQEC_FEC_DEC_FUTURE_PAK;
        }
    }

    /* check the number of lost data packs in pcm */
    num_loss = vqec_pcm_gap_search (fec->pcm, sn_base32, offset, 
                                    na_bits, lost_seq_nums);
 
    /*If there are no packet loss, no need for decoding*/
    if (num_loss == 0) {
        return VQEC_FEC_DEC_NOT_NEEDED;
    }
    if (num_loss > MAX_XOR_CORRECTION) {  
        if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_FEC)) {
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC, 
                          "FEC can not correct losses: "
                          " Num_loss from Bitmap:  %d \n",num_loss);
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC, 
                          " The lost pak seq_nums (32 bits):\n");
            for (i = 0; i < num_loss; i++) {
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC, 
                              " %u \n", lost_seq_nums[i]);
            }
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC, 
                          " The FEC pak information: "
                          " sn_base32   = %u, sn_base = %d "
                          " offset(L)   = %d, na_bits(D)  = %d, "
                          " fec_touched = %d, pcm->last_rx_seq_num  = %u\n",
                          sn_base32,
                          sn_base,
                          offset,
                          na_bits,
                          fec_pak->fec_touched,
                          pcm->last_rx_seq_num);
        }
        return VQEC_FEC_DEC_UNRECOVERABLE;
    }

    j = 0;
    for (i = 0; i < na_bits; i++) {
        temp_seq32 = sn_base32 + i*offset;
        tmp_pak = vqec_pak_seq_find(pcm_pak_seq, 
                                    temp_seq32);
        if (tmp_pak == NULL) {
            lost_seq_num = sn_base +i*offset; 
         }else{
            decode_blks[j] = tmp_pak;  /*put packet in to decode blocks */
            j++;
        }
    }

    /**
     * there is only one packet missing, decode it 
     * the decode is based on procedure recommended at 
     * RFC 2733
     */
    recovered = vqec_pak_alloc_with_particle();
    if (recovered == NULL) {
        return VQEC_FEC_DEC_MEM_ALLOC_WRONG;
    }
    recovered->head_offset = 0;

    recovered->rtp = (rtpfasttype_t *)vqec_pak_get_head_ptr(recovered);
    /* the parity packet is put to the last one in block*/
    decode_blks[na_bits-1] = fec_pak;

    /**
     * recover the header
     * first construct an fec rtp header for computation, based on RFC 2733
     * no need to consider network byte order or host order 
     */
    rtp_header = fec_pak->rtp;
    VQEC_DP_ASSERT_FATAL(rtp_header, "fec");   
    fec_rtp_header.combined_bits = rtp_header->combined_bits;
    SET_RTP_PAYLOAD(&fec_rtp_header, pt_recovery);
    fec_rtp_header.timestamp = fec_hdr->ts_recovery;

    /*decode header */
    for (i=0;i<na_bits-1;i++) {
        header_blks[i] = decode_blks[i]->rtp;
    }
    header_blks[na_bits-1] = &fec_rtp_header;

    /*decode the first 8 bytes in the FEC header */
    if (!vqec_fec_decode_xor_word((char **)header_blks, na_bits,8, 
                                  (char *)recovered->rtp)) {
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC,
                      "FEC header decode failed \n");
        vqec_pak_free(recovered);
        return VQEC_FEC_DEC_FAILURE;
    }

    /*set RTP version after hdr decode success*/   
    SET_RTP_VERSION(recovered->rtp, RTPVERSION);

    /*header decode success,recover the seq # */
    recovered->rtp->sequence = htons(lost_seq_num);

    /* get the length of each media packet without rtp header*/ 
    for (i=0;i<na_bits-1; i++) {
        payload_len[i] = vqec_pak_get_content_len(decode_blks[i])
            - sizeof(rtpfasttype_t);
        length[i] = &(payload_len[i]);
    }
    /*get the recover length from fec header */
    length_recovery = ntohs(fec_hdr->length_recovery);
    length[na_bits-1]= &length_recovery;

    /*recover the packet length, only 2 bytes */
    if (!vqec_fec_decode_xor_word((char **)length, na_bits, 2, 
                            (char *)&pkt_size)) {
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC,
                      " FEC length decode failed\n");
        vqec_pak_free(recovered);
        return VQEC_FEC_DEC_FAILURE;
    }
    /* decode the payload, let payload block point to payload*/
    for (i = 0; i < na_bits - 1; i++) {
        payload_blks[i] = vqec_pak_get_head_ptr(decode_blks[i])
            + decode_blks[i]->mpeg_payload_offset;
    }
    payload_blks[na_bits - 1]= 
        vqec_pak_get_head_ptr(decode_blks[na_bits-1]) +
        sizeof(rtpfasttype_t) + sizeof(vqec_fec_hdr_t);
  
    if (!vqec_fec_decode_xor_word((char **)payload_blks, na_bits, 
                                  pkt_size, 
                                  (char *) (recovered->rtp + 1))) {
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC,
                      "FEC payload decode failed\n");
        vqec_pak_free(recovered);
        return VQEC_FEC_DEC_FAILURE;
    }
 
    /* update pak_information */
    vqec_pak_set_content_len(recovered, pkt_size + sizeof(rtpfasttype_t));
    recovered->rtp_ts = ntohl(recovered->rtp->timestamp);
    recovered->seq_num = 
        vqec_seq_num_nearest_to_rtp_seq_num(pcm->last_rx_seq_num,
                                            lost_seq_num);
    recovered->mpeg_payload_offset = RTPHEADERBYTES(recovered->rtp);
    recovered->type = VQEC_PAK_TYPE_REPAIR;
    VQEC_PAK_FLAGS_SET(&recovered->flags, VQEC_PAK_FLAGS_AFTER_EC);

    memcpy(&(recovered->src_addr), &(decode_blks[0]->src_addr),
           sizeof(struct in_addr));
    recovered->src_port = decode_blks[0]->src_port;
    recovered->rtp->ssrc = decode_blks[0]->rtp->ssrc;

    /* do a RTP sanity check */
    status = vqec_fec_rtp_hdr_validation(vqec_pak_get_head_ptr(recovered), 
                                         vqec_pak_get_content_len(recovered));
    if (status != VQEC_DP_ERR_OK) {
        vqec_pak_free(recovered);
        return VQEC_FEC_DEC_RTP_VALIDATE_WRONG;
    }

    /* insert to pcm */
    recovered->fec_touched = FEC_TOUCHED;
    num_paks = 1;
    /* Note:  Drops of FEC recovered packets are currently not reported. */
    if (!vqec_pcm_insert_packets(pcm, &recovered, num_paks, FALSE, NULL)) {
        vqec_pcm_log_tr135_overrun(pcm, num_paks);
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "Error upon inserting recovered FEC pkt"); 
    }
    vqec_pak_free(recovered);
    return VQEC_FEC_DEC_OK;
}


/**
 * fec event process each fec packet,
 * This function validate RTP header and FEC header of a FEC packet
 * and then insert into corresponding FEC pak_seq 
 *
 * @param[in] is_column_fec if column TRUE, if row FALSE
 * @param[in] rtp_sock      socket for column FEC or row
 * @param[in] fec           pointer to fec module
 * @param[in] pak           pointer to fec packet will be processed
 * @param[in] recv_time     packet receive tme 
 *
 * @return      TRUE if everything ok, FALSE if any validation wrong
 */
UT_STATIC
boolean fec_event_handler_internal_process_pak (boolean is_column_fec,
                                                vqec_fec_t *fec,
                                                vqec_pak_t *pak)
{
    vqec_dp_error_t status = VQEC_DP_ERR_OK;
    vqec_fec_hdr_error_t error = VQEC_FEC_HDR_OK;
 
    if (!fec || !pak) {
        if (pak) {
        }
        return FALSE;
    }

    /**
     * perform basic checks on the RTP header, 
     * abort further processing if it's invalid
     */
    status = vqec_fec_rtp_hdr_validation(vqec_pak_get_head_ptr(pak),
                                         vqec_pak_get_content_len(pak));
    if (status != VQEC_DP_ERR_OK) {
        fec->fec_stats.fec_rtp_hdr_invalid++;
        return (FALSE);
    }

    pak->fec_hdr = vqec_fec_get_fec_hdr(pak);
    if (pak->fec_hdr == NULL) {
        fec->fec_stats.fec_hdr_invalid++;
        return (FALSE);
    }

    /**
     * perform FEC header check
     * abort if it is invalid
     */
    error = vqec_fec_hdr_validation(fec,
                                    vqec_pak_get_head_ptr(pak),
                                    is_column_fec);
    if (error != VQEC_FEC_HDR_OK){
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC, 
                      "Invalid FEC hdr, code = %d\n",
                      error);       
        fec->fec_stats.fec_hdr_invalid++;
        return FALSE;
    }

    /* pak->rcv_ts should already be set from the input shim */
    pak->rtp = (rtpfasttype_t *) vqec_pak_get_head_ptr(pak);
    pak->seq_num = ntohs(pak->rtp->sequence);
    pak->rtp_ts = ntohl(pak->rtp->timestamp);  /* save rtp timestamp in pak */

    pak->fec_touched = 0;
    /* insert packet to fec module */
    if (vqec_fec_insert_packet(fec, pak, is_column_fec) 
       != VQEC_FEC_INSERT_OK) {
            return FALSE;
    }

    return TRUE;
}

UT_STATIC void fec_trigger_decoding (boolean is_column_fec,
                                     vqec_fec_t *fec)
{
    vqec_pak_t *fec_pak = NULL;
    vqec_fec_dec_error_t dec_res;
    uint32_t i;
    vqec_seq_num_t  start, end;
    uint16_t num_iterations;
    boolean iteration;

    switch (fec->fec_params.avail_streams) {

        case VQEC_FEC_STREAM_AVAIL_1D:
    
            /* decode 1-D fec if there is even one fec packet availble */
            if (!vqec_pak_seq_get_num_paks(fec->fec_pak_seq_column)) {
                return; /* no fec paks available */
            }
            /* decode all fec paks 
             * use start and end in case the tail and head are changed
             * while removing a packet
             */
            start = fec->fec_column_head;
            end = fec->fec_column_tail;
            for (i=start; vqec_seq_num_le(i,end); i=vqec_next_seq_num(i)) {
                fec_pak = vqec_pak_seq_find(fec->fec_pak_seq_column, i);
                if (fec_pak) {
                    dec_res = vqec_fec_decode_basic(fec,fec_pak);
                    vqec_fec_parse_dec_error(dec_res, TRUE, fec,fec_pak);
                }
            } /*for()*/
            break;

            /**
             * decode 2-D FEC, row is always processed first
             * fixme: argument: decode row first can reduce delay, but may 
             * compute more packets in xor, L > D.  consider this carefully
             * for future performance optimizations.
             */
        case VQEC_FEC_STREAM_AVAIL_2D:

            /*if it is row fec, directly decode them */   
            if (!is_column_fec) {
                if (!vqec_pak_seq_get_num_paks (fec->fec_pak_seq_row)) {
                    return; /* no fec paks available */
                }
                /** 
                 * decode all fec paks 
                 */
                start = fec->fec_row_head;
                end = fec->fec_row_tail;
                for (i=start; vqec_seq_num_le(i,end);i=vqec_next_seq_num(i)) {
                    fec_pak = vqec_pak_seq_find(fec->fec_pak_seq_row, i);
                    if (fec_pak) {
                        dec_res = vqec_fec_decode_basic(fec,fec_pak);
                        vqec_fec_parse_dec_error(dec_res, FALSE, fec,fec_pak);
                    }
                } /*for()*/
            } else {
                /** 
                 * column fec decode
                 * in 2-D decoding, row is first decoded due to shorter delay,
                 * then column fec, if column fec has some corrected paks
                 * need to go back to decode row again to see if more packets
                 * can be decoded. 
                 */
                num_iterations = 0;

                do {
                    iteration = FALSE;

                    /** 
                     * decode column 
                     * since row is already decoded, we need to test if 
                     * we need to do the iteration
                     */
                    if (!vqec_pak_seq_get_num_paks (fec->fec_pak_seq_column)) {
                        return; /* no fec paks available */
                    }

                   /* decode all column fec packets */
                    start = fec->fec_column_head;
                    end = fec->fec_column_tail;
                    for (i=start;vqec_seq_num_le(i,end);
                         i=vqec_next_seq_num(i)) {
                        fec_pak = vqec_pak_seq_find(fec->fec_pak_seq_column, i);
                        if (fec_pak) {
                            dec_res = vqec_fec_decode_basic(fec,fec_pak);
                            vqec_fec_parse_dec_error(dec_res,TRUE,fec,fec_pak);
                            /* if a packet is decoded, need to do iteration */
                            if (dec_res ==  VQEC_FEC_DEC_OK) {
                                iteration = TRUE;
                                num_iterations++;
                            }
                        }
                    } /*for()*/

                    if (iteration) {
                        /* iteratively decode row fec */
                        iteration = FALSE;

                        if (!vqec_pak_seq_get_num_paks (fec->fec_pak_seq_row)) {
                            return; /* no fec paks available */
                        }

                        start = fec->fec_row_head;
                        end = fec->fec_row_tail;
                        for (i=start; vqec_seq_num_le(i,end);
                             i=vqec_next_seq_num(i)){
                            fec_pak = vqec_pak_seq_find(
                                fec->fec_pak_seq_row, i);
                            if (fec_pak) {
                                dec_res = vqec_fec_decode_basic(fec,fec_pak);
                                vqec_fec_parse_dec_error(dec_res, FALSE, 
                                                         fec,fec_pak);
                                /* if a packet is decoded, do iteration */
                                if (dec_res ==  VQEC_FEC_DEC_OK) {
                                    iteration = TRUE;
                                    num_iterations++;
                                 }
                            }
                        } /*for()*/
                    }
                } while (iteration);
            }
            break;

        default:
            return;
    }
}

/**
 * Determine the dimension (row or column) of the FEC streams from a given
 * received packet.
 * @param[in] fec pointer to the FEC instance
 * @param[in] pak pointer to the FEC packet
 * @param[in] is_stream1 indicates whether the packet was received on the first
 *                       FEC stream (TRUE) or the second FEC stream (FALSE)
 */
void vqec_fec_determine_stream_dimension(vqec_fec_t *fec,
                                         vqec_pak_t *pak,
                                         boolean is_stream1)
{
    fecfasttype_t *fec_hdr;

    if (!fec || !pak) {
        return;
    }

    fec_hdr = (fecfasttype_t *)pak->fec_hdr;

    /*
     * check the D bit in FEC header
     * if D is 0, this is a column FEC pack
     * if D is 1, this is a row FEC pak
     */
    if (!FEC_D_BIT(fec_hdr)) {
        if (is_stream1) {
            fec->is_stream1_column = TRUE;
        } else {
            fec->is_stream1_column = FALSE;
        }
    } else {
        if (is_stream1) {
            fec->is_stream1_column = FALSE;
        } else {
            fec->is_stream1_column = TRUE;
        }
    }
}

/**
 * Receive an incoming FEC packet.
 * @param[in] fec pointer to the FEC instance
 * @param[in] pak pointer to the FEC packet
 * @param[in] stream_hdl indicates whether the packet was received on the first
 *                       FEC stream (1) or the second FEC stream (2)
 * @return  TRUE on success; FALSE otherwise 
 */
boolean vqec_fec_receive_packet (vqec_fec_t *fec,
                                 vqec_pak_t *pak,
                                 uint32_t stream_hdl)
{
    boolean is_column_fec = TRUE;
    fecfasttype_t *fec_hdr = NULL;
    vqec_fec_stats_t *fec_stats = NULL;
    boolean is_stream1 = TRUE;

    if (!fec || !pak) {
        return FALSE;
    }

    if (stream_hdl == VQEC_DP_FEC1_IS_HANDLE) {
        is_stream1 = FALSE;        
    } else if (stream_hdl != VQEC_DP_FEC0_IS_HANDLE) {
        return FALSE;
    }

    fec_stats = &fec->fec_stats; 

    switch (fec->fec_params.avail_streams){
        case VQEC_FEC_STREAM_AVAIL_NONE:
            break;
        case VQEC_FEC_STREAM_AVAIL_1D: /* only column FEC */
            is_column_fec = TRUE;
            fec->column_avail = TRUE;
            break;
        case VQEC_FEC_STREAM_AVAIL_2D: /* 2-D fec,row first, then column*/
            /**
             * From SDP, in 1-D FEC, fec_stream1 is column, we do not need 
             * to anaylize the first packet. 
             * But in 2-D case, stream1 could be row or column, we need 
             * to test the first received fec pack to associate streams 
             * with the right fec_pak_seq.
             */
            if (!fec->first_pak_processed) {

                /* process the received first pak */
                pak->type = VQEC_PAK_TYPE_FEC;
                pak->rtp = (rtpfasttype_t *)vqec_pak_get_head_ptr(pak);
                fec->fec_stats.fec_total_paks++;
                if (vqec_fec_rtp_hdr_validation(
                        vqec_pak_get_head_ptr(pak),
                        vqec_pak_get_content_len(pak)) != VQEC_DP_ERR_OK) {
                    fec->fec_stats.fec_rtp_hdr_invalid++;
                    return FALSE;
                }
                pak->fec_hdr = vqec_fec_get_fec_hdr(pak);
                if (pak->fec_hdr == NULL) {
                    fec->fec_stats.fec_hdr_invalid++;
                    return FALSE;
                }

                fec_hdr = (fecfasttype_t *)pak->fec_hdr;
                if (FEC_X_BIT(fec_hdr)) {
                    return FALSE;  /* not supported in COP#3r2 */
                }

                if (!FEC_E_BIT(fec_hdr)) {
                    return FALSE; /* not supported in COP#3r2*/
                }
                if (FEC_TYPE(fec_hdr)!= VQEC_FEC_TYPE_XOR) {
                    return FALSE; /* not supported in COP#3r2 */
                }

                vqec_fec_determine_stream_dimension(fec, pak, is_stream1);
                fec->first_pak_processed = TRUE;
            }

            /* process/insert the packet */
            /*
             * truth table for is_column_fec:
             * is_stream1_col   is_stream1  ==>  is_col
             *          FALSE        FALSE  ==>  TRUE
             *          FALSE         TRUE  ==>  FALSE
             *           TRUE        FALSE  ==>  FALSE
             *           TRUE         TRUE  ==>  TRUE
             * so, is_column_fec = !(is_stream1_col XOR is_stream1)
             */
            is_column_fec = !XOR_BOOLEAN(fec->is_stream1_column, is_stream1);
            if (is_column_fec) {
                fec->column_avail = TRUE;
            } else {
                fec->row_avail = TRUE;
            }
            break;
        default:
            VQEC_DP_SYSLOG_PRINT(ERROR,
            "Illegal FEC stream count %d, Max 2 FEC streams supported \n", 
                         fec->fec_params.avail_streams);
            break;
    }

    /* process the pak header */
    pak->type = VQEC_PAK_TYPE_FEC;
    pak->rtp = (rtpfasttype_t *)vqec_pak_get_head_ptr(pak);
    fec_stats->fec_total_paks++;

    /* process the pak */
    (void)fec_event_handler_internal_process_pak(is_column_fec, fec, pak);

    /** 
     * fec decoding is triggered
     */
    fec_trigger_decoding(is_column_fec, fec);

    return TRUE;
}

/**
 * clear FEC statistics counters, called at fec module initialization
 *
 * @param[in] fec_stats     pointer to fec stats 
 */

void vqec_fec_counter_clear (vqec_fec_t *fec)
{
    if (!fec) {
        return;
    }
    memcpy(&fec->fec_stats_snapshot, &fec->fec_stats, 
           sizeof(vqec_fec_stats_t));
}

/**
 * get fec statistics counters to tuner
 *
 * @param[in] fec_stats     pointer to fec stats
 * @param[in] stats         tuner stats pointer
 */

void vqec_fec_get_status (vqec_fec_t *fec, vqec_dp_fec_status_t *s,
                          boolean cumulative)
{
    if (!fec || !s) {
        return;
    }

    s->is_fec_enabled = fec->fec_params.is_fec_enabled;
    s->avail_streams = fec->fec_params.avail_streams;
    s->column_avail = fec->column_avail;
    s->row_avail = fec->row_avail;
    s->L = fec->fec_info.fec_l_value;
    s->D = fec->fec_info.fec_d_value;
    s->fec_column_head = fec->fec_column_head;
    s->fec_column_tail = fec->fec_column_tail;
    s->fec_row_head = fec->fec_row_head;
    s->fec_row_tail = fec->fec_row_tail;
    s->fec_late_paks = fec->fec_stats.fec_late_paks;
    s->fec_recovered_paks = fec->fec_stats.fec_recovered_paks;
    s->fec_dec_not_needed = fec->fec_stats.fec_dec_not_needed;
    s->fec_duplicate_paks = fec->fec_stats.fec_duplicate_paks;
    s->fec_total_paks = fec->fec_stats.fec_total_paks;
    s->fec_rtp_hdr_invalid = fec->fec_stats.fec_rtp_hdr_invalid;
    s->fec_hdr_invalid = fec->fec_stats.fec_hdr_invalid;
    s->fec_unrecoverable_paks = fec->fec_stats.fec_unrecoverable_paks;
    s->fec_drops_other = fec->fec_stats.fec_drops_other;
    s->fec_gap_detected = fec->fec_stats.fec_gap_detected;

    if (!cumulative) {
        s->fec_late_paks -= fec->fec_stats_snapshot.fec_late_paks;
        s->fec_recovered_paks -= fec->fec_stats_snapshot.fec_recovered_paks;
        s->fec_dec_not_needed -= fec->fec_stats_snapshot.fec_dec_not_needed;
        s->fec_duplicate_paks -= fec->fec_stats_snapshot.fec_duplicate_paks;
        s->fec_total_paks -= fec->fec_stats_snapshot.fec_total_paks;
        s->fec_rtp_hdr_invalid -= fec->fec_stats_snapshot.fec_rtp_hdr_invalid;
        s->fec_hdr_invalid -= fec->fec_stats_snapshot.fec_hdr_invalid;
        s->fec_unrecoverable_paks -= 
                                 fec->fec_stats_snapshot.fec_unrecoverable_paks;
        s->fec_drops_other -= fec->fec_stats_snapshot.fec_drops_other;
        s->fec_gap_detected -= fec->fec_stats_snapshot.fec_gap_detected;
    }
    return;
}
