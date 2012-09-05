/*
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <libdebug_macros.h>

ARR_BEGIN(vqec_debug_arr)

/* UPDATE VQEC_DEBUG_FIRST BELOW IF CHANGING THIS */
ARR_ELEM(vqec_nwdebug_channel, VQEC_DEBUG_CHANNEL,
         "Vqec_debug channel", 0)

ARR_ELEM(vqec_nwdebug_pcm, VQEC_DEBUG_PCM,
         "Vqec_debug pcm", 0)

ARR_ELEM(vqec_nwdebug_input, VQEC_DEBUG_INPUT,
         "Vqec_debug input", 0)

ARR_ELEM(vqec_nwdebug_output, VQEC_DEBUG_OUTPUT,
         "Vqec_debug output", 0)

ARR_ELEM(vqec_nwdebug_event, VQEC_DEBUG_EVENT,
         "Vqec_debug event", 0)

ARR_ELEM(vqec_nwdebug_igmp, VQEC_DEBUG_IGMP,
         "Vqec_debug igmp", 0)

ARR_ELEM(vqec_nwdebug_tuner, VQEC_DEBUG_TUNER,
         "Vqec_debug tuner", 0)

ARR_ELEM(vqec_nwdebug_recv_socket, VQEC_DEBUG_RECV_SOCKET,
         "Vqec_debug recv_socket", 0)

ARR_ELEM(vqec_nwdebug_timer, VQEC_DEBUG_TIMER,
         "Vqec_debug timer", 0)

ARR_ELEM(vqec_nwdebug_rtcp, VQEC_DEBUG_RTCP,
         "Vqec_debug rtcp", 0)

ARR_ELEM(vqec_nwdebug_error_repair, VQEC_DEBUG_ERROR_REPAIR,
         "Vqec_debug error_repair", 0)

ARR_ELEM(vqec_nwdebug_rcc, VQEC_DEBUG_RCC,
         "Vqec_debug rcc", 0)

ARR_ELEM(vqec_nwdebug_stun, VQEC_DEBUG_NAT,
         "Vqec_debug nat", 0)

ARR_ELEM(vqec_nwdebug_chan_cfg, VQEC_DEBUG_CHAN_CFG,
         "Vqec_debug chan_cfg", 0)

ARR_ELEM(vqec_nwdebug_updater, VQEC_DEBUG_UPDATER,
         "Vqec_debug updater", 0)

ARR_ELEM(vqec_nwdebug_upcall, VQEC_DEBUG_UPCALL,
         "Vqec_debug upcall", 0)

ARR_ELEM(vqec_nwdebug_cpchan, VQEC_DEBUG_CPCHAN,
         "Vqec_debug cpchan", 0)

ARR_ELEM(vqec_nwdebug_strout, VQEC_DEBUG_STREAM_OUTPUT, 
         "Vqec_debug stream_output", 0)

ARR_ELEM(vqec_nwdebug_vod, VQEC_DEBUG_VOD,
         "Vqec_debug vod", 0)

/* MUST REMAIN AS THE LAST DEBUG */
ARR_ELEM(vqec_nwdebug_max_debug, VQEC_DEBUG_MAX,
         "LAST DEBUG -- UNUSED AS A FLAG", 0)

ARR_DONE

#define VQEC_DEBUG_FIRST VQEC_DEBUG_CHANNEL
