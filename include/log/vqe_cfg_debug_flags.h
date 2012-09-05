/********************************************************************
 * vqe_cfg_debug_flags.h
 *
 * Define the debug flags used in VQE CFG Library
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#include <log/libdebug_macros.h>

ARR_BEGIN(vqe_cfg_debug_arr)
ARR_ELEM(cfg_debug_mgr, CFG_DEBUG_MGR,
         "VQE CFG Manager Debugging", 0)
ARR_ELEM(cfg_debug_db, CFG_DEBUG_DB,
         "VQE CFG Database Debugging", 0)
ARR_ELEM(cfg_debug_channel, CFG_DEBUG_CHANNEL,
         "VQE CFG Channel Debugging", 0)
ARR_ELEM(cfg_debug_sdp, CFG_DEBUG_SDP,
         "VQE CFG SDP Session Debugging", 0)
/* Add other flags here */
ARR_DONE
