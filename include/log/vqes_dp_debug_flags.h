/********************************************************************
 * vqes_debug_flags.h
 *
 * Define the debug flags used in VQE Server.
 *
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#include <log/libdebug_macros.h>

ARR_BEGIN(vqes_dp_debug_arr)
ARR_ELEM(dp_debug_cm1,  DP_DEBUG_CM1, "VQE-S Cache Manager", 0)
ARR_ELEM(dp_debug_cm2,  DP_DEBUG_CM2, "VQE-S Cache Manager", 0)
ARR_ELEM(mp_debug_general, MP_DEBUG_GEN, "General MP Debugs", 0)
ARR_ELEM(mp_debug_parse, MP_DEBUG_PARSE, "MP Parsing Debugs", 0)
ARR_ELEM(mp_debug_psi, MP_DEBUG_PSI, "MP PSI Parsing Debugs", 0)
ARR_ELEM(mp_debug_pes, MP_DEBUG_PES, "MP PES Parsing Debugs", 0)
ARR_DONE
