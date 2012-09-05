/********************************************************************
 * vqes_cp_debug_flags.h
 *
 * Define the debug flags used in VQE Server Control Plane.
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#include <log/libdebug_macros.h>

ARR_BEGIN(vqes_cp_debug_arr)
/* ERA Component */
ARR_ELEM(era_debug_verbose,  ERA_DEBUG_VERBOSE, 
         "VQE-S ERA Verbose Debugging", 0)
ARR_ELEM(era_debug_tb_verbose,  ERA_DEBUG_TB_VERBOSE,
         "VQE-S ERA Verbose TB Policing", 0)
ARR_ELEM(era_debug_tb,  ERA_DEBUG_TB,
         "VQE-S ERA Token Bucket Policing", 0)
ARR_ELEM(era_debug_er,  ERA_DEBUG_ER,
         "VQE-S ERA Error Repair Debugging", 0)
ARR_ELEM(era_debug_fcc, ERA_DEBUG_FCC, 
         "VQE-S ERA Fast Channel Change Debugging", 0)
ARR_ELEM(era_debug_general, ERA_DEBUG_GENERAL,
         "VQE-S ERA General Debugging", 0)

/* Other CP components */

ARR_DONE
