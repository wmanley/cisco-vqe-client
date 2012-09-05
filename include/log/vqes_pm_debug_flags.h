/********************************************************************
 * vqes_pm_debug_flags.h
 *
 * Define the debug flags used in VQE-S Process Monitor
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#include <log/libdebug_macros.h>

/*
 * For each component there are 2 debug levels:
 * ERR- Unexpected error occured. This may also result in a syslog, 
 *         but this debug message will contain more detailed information.
 * DETL- Debug traces for normal operation. 
 */

ARR_BEGIN(vqes_pm_debug_arr)
ARR_ELEM(pm_debug_cfg_err, PM_DEBUG_CFG_ERR,
		 "VQE-S Process Monitor Configuration File Parsing Error Messages", 0)
ARR_ELEM(pm_debug_cfg_detl, PM_DEBUG_CFG_DETL,
		 "VQE-S Process Monitor Configuration File Parsing Detail Messages", 0)

ARR_ELEM(pm_debug_ctl_err, PM_DEBUG_CTL_ERR,
		 "VQE-S Process Monitor Controller Error Messages", 0)
ARR_ELEM(pm_debug_ctl_detl, PM_DEBUG_CTL_DETL,
		 "VQE-S Process Monitor Controller Detail Messages", 0)

ARR_ELEM(pm_debug_process_err, PM_DEBUG_PROC_ERR,
		 "VQE-S Process Monitor Process Manager Error Messages", 0)
ARR_ELEM(pm_debug_process_detl, PM_DEBUG_PROC_DETL,
		 "VQE-S Process Monitor Process Manager Detail Messages", 0)

ARR_ELEM(pm_debug_dep_err, PM_DEBUG_DEP_ERR, 
		 "VQE-S Process Monitor Dependency Generator Error Messages", 0)
ARR_ELEM(pm_debug_dep_detl, PM_DEBUG_DEP_DETL, 
		 "VQE-S Process Monitor Dependency Generator Detail Messages", 0)

ARR_ELEM(pm_debug_xmlrpc_err, PM_DEBUG_XMLRPC_ERR,
		 "VQE-S Process Monitor XML-RPC Module Error Messages", 0)
ARR_ELEM(pm_debug_xmlrpc_detl, PM_DEBUG_XMLRPC_DETL,
		 "VQE-S Process Monitor XML-RPC Module Detail Messages", 0)

ARR_ELEM(pm_debug_main_err, PM_DEBUG_MAIN_ERR,
		  "VQE-S Process Monitor Main Error Messages", 0)
ARR_ELEM(pm_debug_main_detl, PM_DEBUG_MAIN_DETL,
		  "VQE-S Process Monitor Main Detail Messages", 0)


ARR_ELEM(pm_debug_lock_err, PM_DEBUG_LOCK_ERR,
		  "VQE-S Process Monitor Locking Error Messages", 0)
ARR_ELEM(pm_debug_lock_detl, PM_DEBUG_LOCK_DETL,
		  "VQE-S Process Monitor Locking Detail Messages", 0)
/* Add other flags here */
ARR_DONE
