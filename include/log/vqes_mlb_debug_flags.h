/********************************************************************
 * vqes_mlb_debug_flags.h
 *
 * Define the debug flags used in VQE-S Multicast Load Balancer
 *
 *
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#include <log/libdebug_macros.h>

ARR_BEGIN(vqes_mlb_debug_arr)
ARR_ELEM(mlb_debug_rpc, MLB_DEBUG_RPC, "VQE-S MLB RPC Debugging Messages", 0)
ARR_ELEM(mlb_debug_netlink, MLB_DEBUG_NETLINK, 
			  "VQE-S MLB Netlink Debugging Messages", 0)
ARR_ELEM(mlb_debug_hash, MLB_DEBUG_HASH, 
		 "VQE-S MLB Hashtable Debugging Messages", 0)
ARR_ELEM(mlb_debug_mlb, MLB_DEBUG, "VQE-S Generic MLB Debugging Messages", 0)
ARR_ELEM(mlb_debug_xml, MLB_DEBUG_XML, "VQE-S MLB XMLRPC Debugging Messages", 0)
ARR_ELEM(mlb_debug_lock, MLB_DEBUG_LOCK, "VQE-S MLB XMLRPC Debugging Messages", 0)
ARR_ELEM(mlb_debug_intf, MLB_DEBUG_INTF, "VQE-S MLB Interface Debugging Messages", 0)
/* Add other flags here */
ARR_DONE
