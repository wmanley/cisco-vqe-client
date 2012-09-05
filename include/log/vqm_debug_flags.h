/************************************************
 * vqm_debug_flags.h
 *
 * Define the debug flags used in VQM.
 *
 * November 2007
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ***********************************************/

#include <log/libdebug_macros.h>

ARR_BEGIN(vqm_debug_arr)
ARR_ELEM(vqm_debug_verbose, VQM_DEBUG_VERBOSE,
         "VQM verbose debugging", 0)
ARR_ELEM(vqm_debug_info, VQM_DEBUG_DETAIL,
         "VQM debugging", 0)
ARR_DONE
