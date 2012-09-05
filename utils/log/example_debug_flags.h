/********************************************************************
 *
 *      File:   example_debug_flags.h
 *      DEG:   Venkat Krishnamurthy
 *
 *      Description:
 *       This file contaiex_debug source code that describes how to use
 *       Vegas Debug Library API.
 *
 *
 * Copyright (c) 1985-2002, 2003, 2004 by cisco Systems, Inc.
 * All rights reserved.
 *
 *
 * $Id: example_debug_flags.h,v 1.3 2002/02/20 22:45:26 venkat Exp $
 * $Source: /auto/vwsvua/kirsten/sanos6/VegasSW__isan__utils/debug/example_debug_flags.h,v $
 * $Author: venkat $
 *
 *********************************************************************/

#include <log/libdebug_macros.h>

ARR_BEGIN(ex_debug_arr)
ARR_ELEM(ex_debug_events, EX_DEBUG_EVENTS, "Ex_debug Server Events", 0)
ARR_ELEM(ex_debug_errors, EX_DEBUG_ERRORS, "Ex_debug Server Errors", 0)
ARR_ELEM(ex_debug_requests, EX_DEBUG_REQUESTS, "Ex_debug Server Requests", 0)
ARR_ELEM(ex_debug_responses, EX_DEBUG_RESPONSES, "Ex_debug Server Responses", 0)
ARR_DONE
