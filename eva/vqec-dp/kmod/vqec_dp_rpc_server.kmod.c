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
 * Description: RPC server support.
 *
 * Documents: 
 *
 *****************************************************************************/
#include "utils/vam_types.h"
#include "vqecutils/vqec_lock.h"
#include "vqec_dp_api.h"
#include "vqec_rpc_common.h_rpcgen"

#define VQEC_DP_IPC_LOG(err, format,...)                        \
    printk(err "<vqec-dev>" format "\n", ##__VA_ARGS__);

/**---------------------------------------------------------------------------
 *  RPC request error announcement.
 * 
 * @param[in] id [int] Identifier (number) of the call.
 * @param[in] ioctl_size [int] Request object size in bytes from the ioctl.
 * @param[in] req_size [int] Request object size from shared memory. 
 * @param[in] req_id [int] Identifier (number) of the call from shared memory.
 * @param[in] req_ver [int] API version.
 *---------------------------------------------------------------------------*/ 
#define RPC_REQ_ERROR(id, ioctl_size, req_size, req_id, req_ver)        \
    VQEC_DP_IPC_LOG(KERN_ERR,                                           \
                    "IPC Error: request id (%u/%u), request size (%u/%u), " \
                    "API version %u", \
                    id, req_id, ioctl_size, req_size, req_ver);    

/**---------------------------------------------------------------------------
 *  RPC function trace.
 * 
 * @param[in] name [const char *] Name of the function invoked.
 *---------------------------------------------------------------------------*/ 
#define RPC_TRACE(name)                         \

#if 0
#define RPC_TRACE(name)                         \
    VQEC_DP_IPC_LOG(KERN_INFO, "%s", name);
#endif

#include "vqec_dp_rpc_server.c_rpcgen"
