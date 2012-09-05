/********************************************************************
 * vqe_rtsp_debug_flags.h
 *
 * Define the debug flags used in VQE RTSP Server
 *
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#include <log/libdebug_macros.h>

ARR_BEGIN(vqe_rtsp_debug_arr)
ARR_ELEM(rtsp_debug_main, RTSP_DEBUG_MAIN,
         "VCDS Process Debugging", 0)
ARR_ELEM(rtsp_debug_cfg, RTSP_DEBUG_CFG,
         "VCDS CFG Debugging", 0)
ARR_ELEM(rtsp_debug_xmlrpc, RTSP_DEBUG_XMLRPC,
         "VCDS XMLRPC Debugging", 0)
ARR_ELEM(rtsp_debug_content, RTSP_DEBUG_CONTENT,
         "VCDS Content Debugging", 0)
ARR_ELEM(rtsp_debug_rtsp, RTSP_DEBUG_RTSP,
         "VCDS RTSP Debugging", 0)
/* Add other flags here */
ARR_DONE
