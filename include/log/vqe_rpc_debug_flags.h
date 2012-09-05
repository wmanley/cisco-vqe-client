/********************************************************************
 * vqe_rpc_debug_flags.h
 *
 * Define the debug flags used in VQE RPC library
 *
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#include <log/libdebug_macros.h>

ARR_BEGIN(vqe_rpc_debug_arr)
ARR_ELEM(vqe_rpc_debug_verbose, VQE_RPC_DEBUG_VERBOSE,
         "VQE RPC verbose debugging", 0)
ARR_ELEM(vqe_rpc_debug_errors, VQE_RPC_DEBUG_ERRORS,
         "VQE RPC error debugging", 0)
ARR_ELEM(vqe_rpc_debug_events, VQE_RPC_DEBUG_EVENTS,
         "VQE RPC event debugging", 0)
ARR_ELEM(vqe_rpc_debug_packets, VQE_RPC_DEBUG_PACKETS,
         "VQE RPC packet debugging", 0)
/* Add other flags here */
ARR_DONE
