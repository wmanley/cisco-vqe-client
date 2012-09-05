/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: DP debug flags.
 *
 * Documents: Debug flags for use throughout the dataplane. It should be noted
 * that turning on any per-packet debug in the dataplane (especially in the 
 * kernel) may significantly impact performance. Hence keep the granualrity 
 * of a debug, i.e.,  the number of things it may turn on small, and also 
 * remember that in many cases they may be a last resort debug tool, rather 
 * than a preferred one.
 *
 *****************************************************************************/

#include <libdebug_macros.h>

ARR_BEGIN(vqec_dp_debug_arr)

    /* UPDATE VQEC_DP_DEBUG_FIRST BELOW IF CHANGING THIS */
    ARR_ELEM(vqec_dp_debug_tlm, VQEC_DP_DEBUG_TLM,
             "Top Level Manager", 0)

    ARR_ELEM(vqec_dp_debug_inputshim, VQEC_DP_DEBUG_INPUTSHIM,
             "Inputshim", 0)

    ARR_ELEM(vqec_dp_debug_outputshim, VQEC_DP_DEBUG_OUTPUTSHIM,
             "Outputshim", 0)

    ARR_ELEM(vqec_dp_debug_outputshim_pak, VQEC_DP_DEBUG_OUTPUTSHIM_PAK,
             "Outputshim per packet info", 0)

    ARR_ELEM(vqec_dp_debug_nll, VQEC_DP_DEBUG_NLL,
             "Numeric locked loop", 0)

    ARR_ELEM(vqec_dp_debug_nll_adjust, VQEC_DP_DEBUG_NLL_ADJUST,
             "Numeric locked loop per packet adjustments", 0)

    ARR_ELEM(vqec_dp_debug_pcm, VQEC_DP_DEBUG_PCM,
             "Pcm debug controls", 0)

    ARR_ELEM(vqec_dp_debug_pcm_pak, VQEC_DP_DEBUG_PCM_PAK,
             "Pcm per-packet debug controls", 0)

    ARR_ELEM(vqec_dp_debug_er, VQEC_DP_DEBUG_ERROR_REPAIR, 
             "Error repair debug controls", 0)

    ARR_ELEM(vqec_dp_debug_rcc, VQEC_DP_DEBUG_RCC, 
             "RCC debug controls", 0)

    ARR_ELEM(vqec_dp_debug_fec, VQEC_DP_DEBUG_FEC, 
             "FEC debug controls", 0)

    ARR_ELEM(vqec_dp_debug_collect_stats, VQEC_DP_DEBUG_COLLECT_STATS, 
             "Enable/disable collection of per-packet stats", 0)

    ARR_ELEM(vqec_dp_debug_failover, VQEC_DP_DEBUG_FAILOVER,
             "Channel source failover", 0)

    /* MUST REMAIN AS THE LAST DEBUG */
    ARR_ELEM(vqec_dp_max_debug, VQEC_DP_DEBUG_MAX,
             "LAST DEBUG -- UNUSED AS A FLAG", 0)

ARR_DONE

#define VQEC_DP_DEBUG_FIRST VQEC_DP_DEBUG_TLM
