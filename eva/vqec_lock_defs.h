//------------------------------------------------------------------
// VQEC. Global Locks.
//
//
// Copyright (c) 2006-2008 by Cisco Systems, Inc.
// All rights reserved.
//------------------------------------------------------------------

#ifndef __VQEC_LOCK_DEFS_H__
#define __VQEC_LOCK_DEFS_H__

#include "vqec_lock.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

VQEC_LOCK_DEF_NATIVE(vqec_g_lock);
VQEC_LOCK_DEF_NATIVE(vqec_g_cli_client_lock);
VQEC_LOCK_DEF_NATIVE(vqec_stream_output_lock);
VQEC_LOCK_DEF_NATIVE(vqec_ipc_lock);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __VQEC_LOCK_DEFS_H__
