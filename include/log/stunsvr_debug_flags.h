/********************************************************************
 * stunsvr_debug_flags.h
 *
 * Define the debug flags used in STUN Server.
 *
 * October 2007, Donghai Ma
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#include <log/libdebug_macros.h>

ARR_BEGIN(stunsvr_debug_arr)
ARR_ELEM(stunsvr_debug_verbose, SS_DEBUG_VERBOSE,
         "STUN Server verbose debugging", 0)
ARR_ELEM(stunsvr_debug_info, SS_DEBUG,
         "STUN Server debugging", 0)
ARR_DONE
