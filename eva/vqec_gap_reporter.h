
/*------------------------------------------------------------------
 *
 * March 2006, 
 *
 * VQE-C Gap Reporter.  Periodically retrieves gap information from
 * the data plane and requests repairs from the VQE-S.
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __VQEC_GAP_REPORTER_H__
#define __VQEC_GAP_REPORTER_H__

#include "vqec_event.h"
#include "vqec_error.h"
#include "vqe_port_macros.h"
#include "vqec_cli.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define VQEC_GAP_REPORTER_OUTPUT_SCHED_INTERVAL (20*1000)
#define VQEC_GAP_REPORTER_FCI_MAX 256 /* One packet is about 1500 bytes, 
                              * 1 FCI(4 bytes) can report 16 packet loss.
                              * So 256 FCI can report around 4k losses.
                              * And 256 * 4 = 1k bytes, less that ethernet
                              * packet length.
                              */
#define VQEC_GAP_REPORTER_MAX_GAP_SIZE 1000
#define VQEC_GAP_REPORTER_GAP_BUF_SIZE 32

/* Methods for en/disabling error repair */
boolean vqec_get_error_repair(boolean *demo_set);
void vqec_set_error_repair (boolean enabled);

/* Number of policed error repair requests */
extern uint64_t g_vqec_error_repair_policed_requests;

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
vqec_error_repair_policer_parameters_set(boolean set_enabled,
                                         boolean enabled,
                                         boolean set_rate,
                                         uint32_t rate,
                                         boolean set_burst,
                                         uint32_t burst);

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
vqec_error_repair_policer_parameters_get(boolean *enabled,
                                         boolean *enabled_set,
                                         uint32_t *rate,
                                         boolean *rate_set, 
                                         uint32_t *burst,
                                         boolean *burst_set);

/*
 * vqec_error_repair_show()
 *
 * Displays information about error repair.
 *
 * Params:
 *   @param[in]  cli  CLI context on which output should be displayed
 */
void
vqec_error_repair_show(struct vqec_cli_def *cli);

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
void vqec_gap_reporter_report(const vqec_event_t *const evptr,
                              int unused_fd,
                              short event, 
                              void *arg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VQEC_GAP_REPORTER_H */
