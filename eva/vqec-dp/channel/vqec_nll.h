/*------------------------------------------------------------------
 * VQEC.  Numeric Lock Loop for clock recovery
 *
 * April 2006, Josh Gahm
 *
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#ifndef __VQEC_NLL_H__
#define  __VQEC_NLL_H__

#include "vqec_dp_common.h"
#include <vqec_dp_api_types.h>

/**
   Numeric Lock loop for eva module.
 */
typedef struct vqec_nll_ 
{
    uint32_t mode;                      /*!< tracking / non-tracking mode */
    abs_time_t pred_base;               /*!< prediction time base */
    uint32_t pcr32_base;                /*!< The PCR value, without the MSB. 
                                        use 32 bit rather than 33 bit PCR values 
                                        in here so it will work with RTP 
                                        timestamps too.*/
    rel_time_t  error_avg;              /*!< average error time */
    abs_time_t last_actual_time;        /*!< last prediction's actual rx time */
    uint32_t num_obs;                   /*!< samples predicted */
    rel_time_t total_adj;               /*!< total adjustment applied (debug) */
    boolean got_first;                  /*!< flags start of prediction  */
    rel_time_t primary_offset;          /*!< repair-burst-to-primary offset */
    uint32_t resets;                    /*!< forced resets  */
    uint32_t predict_in_past;           /*!< samples predicted in the past */
    boolean switch_to_tracking;         /*!< go-to-tracking on next sample  */
    uint32_t num_exp_disc;              /*!< explicit discontinuity count */
    uint32_t num_imp_disc;              /*!< implicit discontinuity count */    
    uint32_t reset_base_no_act_time;
                                        /*!<  reset_base without actual time  */
} vqec_nll_t;

/**
 * vqec_nll_init
 *
 * Must be called once with a pointer to the vqec_nll_t structure to be
 * initialized.  Calling it again will reset the NLL to its starting
 * state.
 *
 * @param[in] nll - pointer of nll to init.
 */
void vqec_nll_init(vqec_nll_t * nll);

/**
 * vqec_nll_reset
 *
 * Reset the NLL. This is similar in function to vqec_nll_init, i.e., the
 * the NLL is reinitialized, except that a persistent reset count is  
 * incremented to log the reset. This function should be called for
 * overflow / underflow, other situations in which NLL should be fully
 * reinitialized.
 *
 * @param[in] nll - pointer of nll to reset. 
 */
void vqec_nll_reset(vqec_nll_t * nll);

/**
 * vqec_nll_set_tracking_mode
 *
 * Change the NLL's mode to tracking from non-tracking. We do not
 * foresee any reason for going from tracking to non-tracking without
 * a reset. The NLL change it's state from non-tracking to tracking at
 * the next sample. Therefore, the function should be called prior to the
 * sample at which the transition will happen. This function should be
 * called only once after a reset or init. 
 *
 * @param[in] nll - pointer of nll. 
 */
void vqec_nll_set_tracking_mode(vqec_nll_t * nll);

/**
 * vqec_nll_adjust
 *
 * Adjust the NLL based on a sample point and compute the NLL
 * predicted time for a given receive timestamp. The NLL operates in
 * two modes: non-tracking and tracking. In non-tracking mode, the original
 * receive times of the samples are unknown, and therefore, it just uses
 * the sender RTP timestamps to determine the send time for packets.
 * In tracking mode, the receive times of samples are known, except
 * e.g., when  packets are received of order.  
 *
 * @param[in] nll -- The nll tracking the receive clock for the stream.
 *
 * @param[in] actual_time -- The actual time at which the sample data passed
 * through the reference point in the data pipeline.  In non-tracking mode
 * this parameter is ignored, since packets have no actual receive time.
 * In the tracking mode this parameter should be non 0, either a correct
 * receive time, or interpolated using adjacent samples (done by the caller).
 * If no actual time is provided, the actual time is estimated from the 
 * last predicted time, and the rtp timestamp delta, i.e., an arrival error
 * of 0 is assumed (in the absence of any other information).
 *
 * @param[in] est_rtp_delta - This is an estimate of the rtp timestamp
 * difference between two packets provided by the caller in non-tracking
 * mode to assist in cases explicit discontinuity in the timebase, i.e., the
 * a sudden shift in the rtp time base. It is also used when the implementation
 * detects a shift in the rtp time base greater than an implementation
 * specific value. In non-tracking mode, receive timestamps are not available
 * to help estimate the rtp time difference, therefore, we allow the application
 * to use external parameters to provide an estimate, wherever possible.
 * 
 * @param[in] pcr32 -- The 32 bit PCR value from an mpeg-ts packet, 
 * ignoring the MSb, or,
 * equivalently, the RTP timestamp under the RFC-2250 MPEG-TS payload
 * format.  NB, the 90KHz PCR value is actually 33 bits and the MSb of
 * the value should be ignored to get the 32 bit value.
 *
 * @param[in/out] disc -- In/Out value.  If true on input, the PCR value is
 * associated with a discontinuity marker in the MPEG-TS packet or,
 * equivalently, the M bit is set in the RTP header under the RFC 2250
 * MPEG-TS encapsulation.  *disc may be set to TRUE on output, even if
 * it was not set on input, in the case where the NLL attempts to
 * adjust the clock and discovers that the predicted and actual time
 * values are too far apart for this sample to do this without a
 * discontinuity. 
 *
 * @param[out] pred_time -- Out.  Returns the predicted time for the sample
 * point.  This value is always returned. Predicted time cannot move
 * backwards without resetting the NLL, i.e., it is only predicted forward 
 * in time.
 */
void vqec_nll_adjust(vqec_nll_t * nll, 
                   abs_time_t actual_time,
                   uint32_t pcr32,
                   rel_time_t est_rtp_delta,
                   boolean * disc, /* value/result */
                   abs_time_t * pred_time);

/**
 * vqec_nll_get_info  
 *
 * Retrieve current state of the NLL.
 * 
 * @param[in] nll - pointer of nll.
 * @param[out] info - pointer of info.
 */
void vqec_nll_get_info(vqec_nll_t * nll, vqec_dp_nll_status_t * info);

#endif /*  __VQEC_NLL_H__ */
