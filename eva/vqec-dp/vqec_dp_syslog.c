/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Dataplane syslog and debug array declaraton.
 *
 * Documents: 
 *
 *****************************************************************************/

/* VQEC-DP */
/*------------------------------------------------------------------------*/
/* Among all the C files including it, only ONE C file (say the one
 * where main() is defined) should instantiate the message definitions
 * described in XXX_syslog_def.h.  To do so, the designated C file
 * should define the symbol SYSLOG_DEFINITION before including the
 * file.
 */
#define SYSLOG_DEFINITION
#include "vqec_dp_syslog_def.h"
#undef SYSLOG_DEFINITION

/* Include base definitions: boolean flags, enum codes */
#include "vqec_dp_debug_flags.h"
#define __DECLARE_DEBUG_NUMS__
#include "vqec_dp_debug_flags.h"
/* Declare the debug array */
#define __DECLARE_DEBUG_ARR__
#include "vqec_dp_debug_flags.h"
/*------------------------------------------------------------------------*/
