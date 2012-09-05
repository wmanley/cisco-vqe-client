/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File:
 *
 * Description: VQEC TR-135 data structures.
 *
 * Documents:
 *
 *****************************************************************************/
#ifndef __VQEC_TR135_DEFS_H__
#define __VQEC_TR135_DEFS_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Public TR-135 writable parameters data Structure
 *
 * Upon assigning parameter values, values must be supplied for *all* fields."
 */
typedef struct vqec_ifclient_tr135_params_t_
{
   uint32_t gmin;                  /*<
                                    *< Minimum number of consecutive received
                                    *< packets after the end of an RTP Loss
                                    *< Event (See TR-135 for info on RTP
                                    *< Loss Event algorithm)
                                    *< Default value : 0/Disabled
                                    */
   uint32_t severe_loss_min_distance; /*<
                                       *< The minimum distance required between
                                       *< error events before an RTP Loss Event
                                       *< is considered severe.
                                       *< Default value : 0/Disabled
                                       */
} vqec_ifclient_tr135_params_t;

typedef struct vqec_ifclient_stats_channel_tr135_sample_t_ {
    uint64_t minimum_loss_distance;
    uint64_t maximum_loss_period;
} vqec_ifclient_stats_channel_tr135_sample_t;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
