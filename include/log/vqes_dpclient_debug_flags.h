/********************************************************************
 * vqes_debug_flags.h
 *
 * Define the debug flags used in VQE Server.
 *
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#include <log/libdebug_macros.h>

ARR_BEGIN(vqes_dpclient_debug_arr)
ARR_ELEM(dpclient_debug_cm1,  DPCLIENT_DEBUG_CM1, "VQE-S Cache Manager", 0)
ARR_ELEM(dpclient_debug_cm2,  DPCLIENT_DEBUG_CM2, "VQE-S Cache Manager", 0)
ARR_ELEM(dpclient_debug_mp1,  DPCLIENT_DEBUG_MP1, "VQE-S DPCLIENT MPEG Parser", 0)
ARR_ELEM(dpclient_debug_mp2,  DPCLIENT_DEBUG_MP2, "VQE-S DPCLIENT MPEG Parser", 0)
ARR_DONE
