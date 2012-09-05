/********************************************************************
 * vqe_rtp_debug_flags.h
 *
 * Define the debug flags used in VQE RTP/RTCP Library
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#include <log/libdebug_macros.h>

ARR_BEGIN(vqe_rtp_debug_arr)
ARR_ELEM(rtp_debug_verbose, RTP_DEBUG_VERBOSE,
         "VQE RTP verbose debugging", 0)
ARR_ELEM(rtp_debug_errors, RTP_DEBUG_ERRORS,
         "VQE RTP error debugging", 0)
ARR_ELEM(rtp_debug_events, RTP_DEBUG_EVENTS,
         "VQE RTP event debugging", 0)
ARR_ELEM(rtp_debug_packets, RTP_DEBUG_PACKETS,
         "VQE RTP packet debugging", 0)
ARR_ELEM(rtp_debug_exp_err,  RTP_DEBUG_EXP_ERR, 
	 "VQE Exporter Error Debugging",         0)
ARR_ELEM(rtp_debug_exp_info, RTP_DEBUG_EXP_INFO,
	 "VQE Exporter Informational Debugging", 0)
/* Add other flags here */
ARR_DONE
