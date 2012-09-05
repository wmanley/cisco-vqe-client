/*------------------------------------------------------------------
 * VQEC.  Numeric Lock Loop for clock recovery
 *
 * April 2006, Josh Gahm
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include "vqec_nll.h"

/**
 * NLL Adjustment algorithm
 *
 * E[n] -- cumulative error at the n'th sample
 * e[n] -- individual error of the n'th sample
 * r -- decay rate of the error average.
 * f[n] -- estimated clock mismatch after the n'th sample
 * c[n] -- correction after the n'th sample
 * s -- correction slew rate
 * f'[n] -- new estimated clock mismatch after the correction
 * E'[n] -- new estimated cumulative error after the correction
 * 
 *
 * E[n] = e[n] + r*e[n-1] + r**2*e[n-2]......
 *
 * Let f[n] be defined by the value such that 
 *
 * E[n] = f[n](1 + r + r**2 + ...)
 *      = f[n](1/(1-r))
 *
 *
 * f[n] = E[n]*(1 - r) 
 * c[n] = -f[n]*s
 * f'[n] = f[n] + c[n]
 *
 * E'[n] = f'[n]*(1/(1-r))
 *       = f[n]/(1-r) + c[n]*(1/(1-r))
 *       = E[n] + c[n]*(1/(1-r))
 *
 * r is set to about 0.95.
 *
 * s is set to 1/1024
 *
 */

#define MAX_ARRIVAL_ERROR MSECS(100) 
#define MAX_NLL_DISCONTINUITY_THRESHOLD MSECS(100)

#define SHIFT_1024X1024 (20)

/* r is MV_AVG_MULT/(1<<MV_AVG_SHIFT) */
#define MV_AVG_SHIFT SHIFT_1024X1024
/* about 5% decay rate for moving average */
#define MV_AVG_MULT ((1<<MV_AVG_SHIFT) - (1<<MV_AVG_SHIFT)/20) 

/* s is 1/(1<<SLEW_SHIFT) */
/* correct about 1/1024 of the moving average error each time */
#define SLEW_SHIFT 10 

/* MV_AVG_ERROR_CORR is (1/(1-r)) */
#define MV_AVG_ERROR_CORR (1<<MV_AVG_SHIFT)/((1<<MV_AVG_SHIFT) - MV_AVG_MULT)

/* NLL modes */
enum
{
    VQEC_NLL_MODE_NONTRACKING = 0,
    VQEC_NLL_MODE_TRACKING
} ;

/******************************************************************************
 * Initalize nll.
 ******************************************************************************/
void vqec_nll_init (vqec_nll_t * nll)
{
    if (!nll) {
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, "init() null nll instance\n");
        return;
    }
    memset(nll, 0, sizeof(*nll));
}

/******************************************************************************
 * Force-reset nll: default state is set to non-tracking.
 ******************************************************************************/
void vqec_nll_reset (vqec_nll_t * nll)
{
    uint32_t resets;

    if (!nll) {
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, "reset() null nll instance\n");
        return;
    }

    resets = nll->resets;

    memset(nll, 0, sizeof(*nll));
    nll->resets = ++resets;

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, "nll(%p) is reset\n", nll);
}

/******************************************************************************
 * Change nll's mode to tracking. We don't envisage change from tracking
 * to non-tracking without a reset. This is a "prefix" operation, i.e., the 
 * actual switch will happen at the next sample given to the NLL.
 ******************************************************************************/
void vqec_nll_set_tracking_mode (vqec_nll_t * nll)
{
    if (!nll) {
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, "set_mode() null nll instance\n");
        return;
    }

    nll->switch_to_tracking = TRUE;
}

/******************************************************************************
 * Helper inline: returns closest difference of 2 RTP timestamps.
 ******************************************************************************/
static inline rel_time_t vqec_nll_rtp_delta (uint32_t pcr32_base, uint32_t pcr32)
{
    uint32_t pcr32_diff = pcr32 - pcr32_base;
    uint32_t pcr32_diff_neg = pcr32_base - pcr32;
    int64_t pcr64_diff = pcr32_diff < pcr32_diff_neg ? pcr32_diff : 
        -((int64_t)pcr32_diff_neg);
    return (TIME_MK_R(pcr, pcr64_diff));
}

/******************************************************************************
 * Helper inline: update the cumulative error, and return the correction
 * that should be applied to the current sample. 
 ******************************************************************************/
static inline rel_time_t vqec_nll_update_error (vqec_nll_t *nll, 
                                                rel_time_t arrival_error)
{
    rel_time_t correction;

    /* Maintain the cumulative arrival error, E[n] */
    nll->error_avg 
        = TIME_ADD_R_R(TIME_RSHIFT_R(TIME_MULT_R_I(nll->error_avg,
                                                   MV_AVG_MULT), MV_AVG_SHIFT),
                       arrival_error);

    /* 
     * Calculate the correction, c[n], without bothering to calculate f[n] 
     * separately first. The shift operation at once divides by (1<<MV_AVG_SHIFT)
     * and (1<<SLEW_SHIFT).
     */ 
    correction = TIME_NEG_R(
        TIME_RSHIFT_R(TIME_MULT_R_I(nll->error_avg,
                                    ((1<<MV_AVG_SHIFT) - MV_AVG_MULT)),
                      (MV_AVG_SHIFT+SLEW_SHIFT)));    
    if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
        nll->total_adj = TIME_ADD_R_R(nll->total_adj, correction);
    }
    
    /* Update the moving error average after the adjustment (E'[n]) */
    nll->error_avg = TIME_ADD_R_R(nll->error_avg,
                                  TIME_MULT_R_I(correction, MV_AVG_ERROR_CORR));

    return (correction);
}

/******************************************************************************
 * Adjust nll's state / predict a receive time for the given sample.
 ******************************************************************************/
void vqec_nll_adjust (vqec_nll_t * nll, 
                      abs_time_t actual_time,
                      uint32_t pcr32,
                      rel_time_t est_rtp_delta,
                      boolean * disc, 
                      abs_time_t * predicted_time)
{
    rel_time_t time_delta = REL_TIME_0, correction = REL_TIME_0, arrival_error;
    abs_time_t pred_arrival, prev_pred_base;
    boolean reset_base;

    if (!nll || !disc || !predicted_time) {        
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, "adjust() invalid inputs %p/%p/%p\n",
                   nll, disc, predicted_time);
        if (predicted_time) {
            *predicted_time = get_sys_time();
        }
        return;
    }

    prev_pred_base = nll->pred_base;
    if (*disc) {
        nll->num_exp_disc++;
        VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, 
                   "nll(%p) explicit discontinuity signaled: "
                   "actual_time %llu, pcr32 %u, est_rtp %llu\n", nll,
                   TIME_GET_A(msec, actual_time), pcr32, 
                   TIME_GET_R(usec, est_rtp_delta));
    }
    
    /* 
     * The NLL operates in two modes: non-tracking and tracking. In 
     * non-tracking mode, the original receive times of the samples are 
     * unknown, and therefore, it just uses the sender RTP timestamps to
     *  determine the send time for packets. In tracking mode, the receive
     *  times of samples are known, except, e.g., when  packets are received
     *  of order.  
     */
    switch (nll->mode) {

    case VQEC_NLL_MODE_NONTRACKING:     /* non-tracking mode */

        if (!nll->got_first) {
            /*
             * if this is the 1st sample, set pred_base = actual_time if 
             * actual_time is non-0, otherwise set it to current time.
             */
            nll->got_first = TRUE;
            *disc = TRUE;
            if (!IS_ABS_TIME_ZERO(actual_time)) {
                nll->pred_base = actual_time;
            } else {
                nll->pred_base = get_sys_time();
            }

        } else {                        /* else-of !nll->got_first */

            /*
             * the code below considers the condition delta(rtp) >100 msecs
             * an "Implicit discontinuity". Whenever there the code detects 
             * that the timing may be discontinuous, or the caller sets the 
             * discontinuity flag, "est_rtp_delta" is added to the previous 
             * value of pred_base. No limits are imposed on "est_rtp_delta".
             */

            if (!*disc) {
                time_delta = vqec_nll_rtp_delta(nll->pcr32_base, pcr32); 
            }

            if (*disc || 
                TIME_CMP_R(gt, time_delta, 
                           MAX_NLL_DISCONTINUITY_THRESHOLD) ||
                TIME_CMP_R(gt, TIME_NEG_R(time_delta), 
                           MAX_NLL_DISCONTINUITY_THRESHOLD)) {
            
                nll->pred_base = 
                    TIME_ADD_A_R(nll->pred_base, est_rtp_delta);
                if (!*disc) {
                    nll->num_imp_disc++;  /* implicitly discontinuous oper */
                    *disc = TRUE;

                    VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, "Implicit discontinuity : "
                               "nll(%p) pcr %u old_pcr %u pred_base %llu\n", 
                               nll, pcr32, nll->pcr32_base, 
                               TIME_GET_A(msec, nll->pred_base));
                }
            
            } else {                    /* else-of (*disc || TIME_CMP_R(... */

                nll->pred_base = TIME_ADD_A_R(nll->pred_base, time_delta);
            }
        }                               /* !nll->got_first */


        /* 
         * Protect backward fold in predicted time; the previous predicted
         * time is selected if this condition is true.
         */
        if (TIME_CMP_A(lt, nll->pred_base, prev_pred_base)) {
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, "Prediction in past: nll(%p)"
                       "new_base = %llu  old_base = %llu pcr32 %u\n", nll,
                       TIME_GET_A(msec, nll->pred_base), 
                       TIME_GET_A(msec, prev_pred_base), 
                       nll->pcr32_base);

            nll->pred_base = prev_pred_base;
            nll->predict_in_past++;
        }

        nll->pcr32_base = pcr32;
        *predicted_time = nll->pred_base;
           
        if (nll->switch_to_tracking) {  /* switch to tracking mode */
            /* 
             * The rcc-repair burst and primary streams are received
             * somewhat "asynchronously" of each other, i.e., the primary
             * packets may be received at any epoch within the join latency
             * window. Because of the bandwidth / join skew, in tracking
             * mode the arrival error should be computed from the first
             * primary packet's actual receive time.  However, the time
             * skew between the predicted receive time of the 1st primary, 
             * and it's actual receive time must be preserved, and added to
             * all subsequent predictions in tracking mode. This value is
             * termed "primary_offset". To switch to tracking mode it is
             * *necessary* that a packet with non-0 actual time is provided. 
             * The error case is explicitly handled by using current predicted
             * base as actual time causing primary_offset to be 0. This concept
             * of time-shift is now an integral part of the nll, as is 
             * computed / cached implicitly when switching to tracking mode.
             */ 
            abs_time_t act;
            if (IS_ABS_TIME_ZERO(actual_time)) {
                act = nll->pred_base;
            } else {
                act = actual_time;
            }

            nll->primary_offset = 
                TIME_SUB_A_A(nll->pred_base, act);
            nll->pred_base = 
                nll->last_actual_time = act;

            nll->switch_to_tracking = FALSE;
            nll->mode = VQEC_NLL_MODE_TRACKING;

            VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, 
                       "Switch to tracking : nll(%p) pred_base %llu "
                       "primary offset %lld actual time %llu \n", nll, 
                       TIME_GET_A(msec, nll->pred_base),
                       TIME_GET_R(usec, nll->primary_offset),
                       TIME_GET_A(msec, actual_time));
        }
        break;                          /* done with non-tracking processing */



    case VQEC_NLL_MODE_TRACKING:        /* tracking mode */

        if (!nll->got_first) {
            /*
             * if this is the 1st sample, set pred_base to actual_time + 
             * non-tracking-offset if actual time is non-0; otherwise, set it to 
             * current time + non-tracking-offset.
             */
            nll->got_first = TRUE;
            *disc = TRUE;
            if (!IS_ABS_TIME_ZERO(actual_time)) {
                nll->pred_base = actual_time;
            } else {
                nll->pred_base = get_sys_time();
            }

        } else {                        /* else-of !nll->got_first */

            reset_base = FALSE;

            if (!*disc) {
                time_delta = vqec_nll_rtp_delta(nll->pcr32_base, pcr32);

                /*
                 * We leave open the possibility of bounding this delta
                 * prior  computing arrival error, and instead using the 
                 * receive timestamp delta estimate (as for explicit disc)
                 * in an attempt to better predict implicit discontinuities.
                 * (this can also be done if the arrival error exceeded 
                 * threshold, but there was no explicit discontinuity).
                 */

            } else {

                /* 
                 * In case of explicit discontinuity, approximate rtp delta from 
                 * the receive timestamps of the current & previous sample, if
                 * they are both non-0. This approx is used only If the delta is
                 * < DISCONTINUITY_THRESHOLD msecs. Otherwise we'll reset
                 *  the nll's pred_base / error average.
                 */
                if (!IS_ABS_TIME_ZERO(actual_time) &&
                    (!IS_ABS_TIME_ZERO(nll->last_actual_time))) {
                    time_delta = TIME_SUB_A_A(actual_time, nll->last_actual_time); 
                    if (TIME_CMP_R(gt, time_delta, 
                                   MAX_NLL_DISCONTINUITY_THRESHOLD) ||
                        TIME_CMP_R(gt, TIME_NEG_R(time_delta), 
                                   MAX_NLL_DISCONTINUITY_THRESHOLD)) {
                        reset_base = TRUE;
                    }                       /* (TIME_CMP_R(gt...) */
                }                           /* (!IS_ABS_TIME_ZERO()... */
            }                               /* (!*disc) */
            
            
            /* 
             * If we have a bounded estimate of the sender's time delta,
             * compute arrival error, and ensure that it is bounded by
             * +/- MAX_ARRIVAL_ERROR msecs. If not, we'll reset the nll's
             *  pred_base / error average. If no actual time is provided,
             * we use, actual_time = pred_base + rtp_delta = pred_arrival.
             */
            if (!reset_base) {
                abs_time_t act;
                if (IS_ABS_TIME_ZERO(actual_time)) {
                    act =  TIME_ADD_A_R(nll->pred_base, time_delta);
                } else {
                    act = actual_time;
                }

                pred_arrival  = TIME_ADD_A_R(nll->pred_base, time_delta);
                arrival_error = TIME_SUB_A_A(pred_arrival, act);
                
                if (TIME_CMP_R(lt, arrival_error, MAX_ARRIVAL_ERROR) 
                    && TIME_CMP_R(lt, TIME_NEG_R(arrival_error), 
                                  MAX_ARRIVAL_ERROR)) {
                    correction = vqec_nll_update_error(nll, arrival_error);
                    nll->pred_base = TIME_ADD_A_R(pred_arrival, correction);
                } else {

                    reset_base = TRUE;  /* arrival error is out-of-bound */
                } 
            }
        
            if (reset_base) {           /* explicit disc or excessive error  */
                if (!*disc) {  
                    nll->num_imp_disc++;  /* implicit discontinuous oper */
                    *disc = TRUE;
                }

                /* 
                 * We must have an actual time provided for this particular
                 * case. If one is not provided, the last pred_base is used; 
                 * a counter is incremented to keep track of such rather 
                 * unlikely incidents.
                 */
                if (!IS_ABS_TIME_ZERO(actual_time)) {
                    nll->pred_base = actual_time;
                } else {
                    nll->reset_base_no_act_time++;
                    VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, "Reset base w/o act time "
                               "nll(%p)", nll);
                }
                nll->error_avg = REL_TIME_0; /* reset error average */

                VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, "Tracking mode discontinuity : "
                           "nll(%p) imp_disc %u exp_disc %u pcr %u old_pcr %u "
                           "pred_base %llu\n",
                           nll, nll->num_imp_disc, nll->num_exp_disc, pcr32, 
                           nll->pcr32_base, TIME_GET_A(msec, nll->pred_base));
            }                           /* end-of (reset_base) */
        }                               /* end-of (!nll->got_first) */

        /* 
         * Protect backward fold in predicted time; the previous predicted
         * time is selected if this condition is true.
         */
        if (TIME_CMP_A(lt, nll->pred_base, prev_pred_base)) {
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL, "Prediction in past: nll(%p)"
                       "new_base = %llu  old_base = %llu pcr32 %u\n", nll,
                       TIME_GET_A(msec, nll->pred_base), 
                       TIME_GET_A(msec, prev_pred_base), 
                       nll->pcr32_base);
            
            nll->pred_base = prev_pred_base;
            nll->predict_in_past++;
        }

        nll->pcr32_base = pcr32;
        nll->last_actual_time = actual_time;
        *predicted_time = TIME_ADD_A_R(nll->pred_base, nll->primary_offset);
        break;                          /* done with tracking processing */


    default:                            /* not reached */
        VQEC_DP_ASSERT_FATAL(0, "nll fatal error");
        break;
    }                                   /* end switch(mode) */

    if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
        nll->num_obs++;
    }

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_NLL_ADJUST, 
               "Adjustment: nll(%p), base %llu, "
               "pcr32 %u, correction %lld, cum err %lld\n", nll, 
               TIME_GET_A(msec, nll->pred_base), nll->pcr32_base, 
               TIME_GET_R(usec, correction), TIME_GET_R(usec, nll->error_avg));
}

/******************************************************************************
 * Retrieve nll statistics / info.
 ******************************************************************************/
void vqec_nll_get_info (vqec_nll_t * nll, vqec_dp_nll_status_t * info)
{
    memset(info, 0, sizeof(*info));

    info->mode                   = nll->mode;
    info->pred_base              = nll->pred_base;
    info->pcr32_base             = nll->pcr32_base;
    info->num_exp_disc           = nll->num_exp_disc;
    info->num_imp_disc           = nll->num_imp_disc;
    info->num_obs                = nll->num_obs;
    info->total_adj              = nll->total_adj;
    info->predict_in_past        = nll->predict_in_past;
    info->resets                 = nll->resets;
    info->last_actual_time       = nll->last_actual_time;
    info->primary_offset         = nll->primary_offset;
    info->reset_base_no_act_time = info->reset_base_no_act_time;
}
