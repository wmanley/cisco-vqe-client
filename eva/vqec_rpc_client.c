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
 * Description: RPC client support.
 *
 * Documents: 
 *
 *****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <vqec_assert_macros.h>
#include <vqec_dp_api.h>
#include "vqec_rpc_common.h_rpcgen"
#include "vqec_debug.h"
#include "vqec_lock_defs.h"
#include "vqec_ifclient.h"

static void *s_vqec_shm_reqbuf_ptr = (void *)-1, 
    *s_vqec_shm_rspbuf_ptr = (void *)-1;
static int s_vqec_dp_kmod_devid = -1;

#ifndef VQEC_KERNEL_DEVNODE_PATH
  #define VQEC_KERNEL_DEVNODE_PATH "/dev/"
#endif

/**---------------------------------------------------------------------------
 *  Close the kernel module device and unmap the shared memory pages
 *  for requests and responses.
 *---------------------------------------------------------------------------*/ 
static void
vqec_rpc_client_close_dev (void)
{
    if (s_vqec_dp_kmod_devid != -1) {
        if (s_vqec_shm_reqbuf_ptr != (void *)-1) {
            (void)munmap(s_vqec_shm_reqbuf_ptr, 4096);
            s_vqec_shm_reqbuf_ptr = (void *)-1;
        }
        if (s_vqec_shm_rspbuf_ptr != (void *)-1) {
            (void)munmap(s_vqec_shm_rspbuf_ptr, 4096);
            s_vqec_shm_rspbuf_ptr = (void *)-1;
        }
        close(s_vqec_dp_kmod_devid);
        s_vqec_dp_kmod_devid = -1;
    }   
}


/**---------------------------------------------------------------------------
 *  Open the kernel module device and map the shared memory pages
 *  for requests and responses.
 * 
 * @param[out] vqec_error_t Returns VQEC_OK on success, VQEC_ERR_INTERANL
 * on failure.
 *---------------------------------------------------------------------------*/ 
static vqec_error_t
vqec_rpc_client_open_dev (void)
{
#define VQEC_RPCLIENT_LOGBUF_LEN 80
    char mbuf[VQEC_RPCLIENT_LOGBUF_LEN];
    vqec_error_t err = VQEC_ERR_INTERNAL;

    s_vqec_dp_kmod_devid = 
        open(VQEC_KERNEL_DEVNODE_PATH "vqec0", O_RDWR);
    if (s_vqec_dp_kmod_devid == -1) {
        snprintf(mbuf, VQEC_RPCLIENT_LOGBUF_LEN, 
                 "device open %s failed (%d)", 
                 VQEC_KERNEL_DEVNODE_PATH "vqec0", errno);
        syslog_print(VQEC_ERROR, mbuf);
        goto done;
    }
    
    s_vqec_shm_reqbuf_ptr = mmap(0, 
                                 VQEC_DEV_IPC_BUF_LEN, 
                                 PROT_WRITE, 
                                 MAP_SHARED, 
                                 s_vqec_dp_kmod_devid, 
                                 VQEC_DEV_IPC_BUF_REQ_OFFSET);
    if (s_vqec_shm_reqbuf_ptr == (void *)-1) {
        snprintf(mbuf, VQEC_RPCLIENT_LOGBUF_LEN, 
                 "mmap failed for request offset (%d)", errno);
        syslog_print(VQEC_ERROR, mbuf);
        goto done;
    }

    /*
     * SH4 cores if an attempt is made to cache-invalidate a page which is
     * mapped as read-only. Hence the response buffer is also mapped as R/W.
     */
    s_vqec_shm_rspbuf_ptr = mmap(0, 
                                 VQEC_DEV_IPC_BUF_LEN, 
                                 PROT_WRITE, 
                                 MAP_SHARED, 
                                 s_vqec_dp_kmod_devid, 
                                 VQEC_DEV_IPC_BUF_RSP_OFFSET);
    if (s_vqec_shm_rspbuf_ptr == (void *)-1) {
        snprintf(mbuf, VQEC_RPCLIENT_LOGBUF_LEN, 
                 "mmap failed for response offset (%d)", errno);
        syslog_print(VQEC_ERROR, mbuf);
        goto done;
    }

    /*
     * Write to all pages which will cause them to be mapped via the
     * nopage method; not having a write prior to an initial read causes 
     * SH4 to core.
     */
    memset(s_vqec_shm_reqbuf_ptr, 0, VQEC_DEV_IPC_BUF_LEN);
    memset(s_vqec_shm_rspbuf_ptr, 0, VQEC_DEV_IPC_BUF_LEN);
    err = VQEC_OK;
    
  done:    
    if (err != VQEC_OK) {
        if (s_vqec_dp_kmod_devid != -1) {
            close(s_vqec_dp_kmod_devid);
        }
    }
    return (err);
}


/**---------------------------------------------------------------------------
 *  External interface to open the kernel module device.
 * 
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success, 
 * VQEC_DP_ERR_INTERNAL on failure.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_open_module (void)
{
    g_vqec_client_mode = VQEC_CLIENT_MODE_KERN;
    if (vqec_rpc_client_open_dev() != VQEC_OK) {
        return (VQEC_DP_ERR_INTERNAL);
    }

    return (VQEC_DP_ERR_OK);
}


/**---------------------------------------------------------------------------
 *  External interface to close the kernel module device.
 * 
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_close_module (void)
{
    vqec_rpc_client_close_dev();
    return (VQEC_DP_ERR_OK);
}


/**---------------------------------------------------------------------------
 * Read packets from dataplane.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t 
vqec_dp_output_shim_tuner_read(vqec_dp_tunerid_t id,
                               vqec_iobuf_t *iobuf,
                               uint32_t iobuf_num,
                               uint32_t *len,
                               int32_t timeout_msec)
{
    return (VQEC_DP_ERR_NO_RPC_SUPPORT);
}


/**---------------------------------------------------------------------------
 *  Invoke a IPC system call [most parameters are unused for now].
 * 
 * @param[in] req [char*] Pointer to the request object (in shared memory).
 * @param[in] req_size [int] Request object size in bytes. 
 * @param[out] rsp [char *] Pointer to the response object (in shared memory).
 * @param[out] rsp_size [int *] Response object size (pointer) in bytes.
 * @param[in] rd_wr [int] Bitmask specifying if the request is read-only, 
 * write-only or both read-write.
 * The macro returns the return-value of the ioctl system call.
 *---------------------------------------------------------------------------*/ 
#define RPC_SND_RCV(req, req_size, rsp, rsp_size, rd_wr)        \
    ({                                                                  \
        int32_t retval = 0, dir = 0;                                    \
        if (rd_wr & VQEC_DEV_IPC_IOCTL_WRITE) {                         \
            dir |= _IOC_WRITE;                                          \
        }                                                               \
        if (rd_wr & VQEC_DEV_IPC_IOCTL_READ) {                          \
            dir |= _IOC_READ;                                           \
        }                                                               \
        retval =                                                        \
            ioctl(s_vqec_dp_kmod_devid,                                 \
                  _IOC(dir, VQEC_DEV_IPC_IOCTL_TYPE,                    \
                       req->__rpc_fcn_num, req_size), NULL);            \
        retval;                                                         \
    })

        
/**---------------------------------------------------------------------------
 *  Announcement for a malformed response.
 * 
 * @param[in] result [int] Ioctl return value.
 * @param[in] rsp_len [int] Actual length of the response in bytes.
 * @param[in] exp_rsp_len [int] Expected length of the response in bytes.
 * @param[in] rsp_fcn IPC function number received in response.
 * @param[in] req_fcn IPC function number sent in request.
 * @param[in] rsp_ver API version in the response.
 *---------------------------------------------------------------------------*/ 
#define RPC_BAD_RSP(result, rsp_len, exp_rsp_len, rsp_fcn, req_fcn, rsp_ver) \
    syslog_print(VQEC_IPC_ERROR, result, rsp_len,                       \
                 exp_rsp_len, rsp_fcn, req_fcn, rsp_ver);

#define RPC_TIMEOUT syslog_print(VQEC_IPC_TIMEOUT_ERROR);

#define VQEC_RPC_CLIENT_SHM_REQBUF_PTR (volatile void *)s_vqec_shm_reqbuf_ptr
#define VQEC_RPC_CLIENT_SHM_RSPBUF_PTR (volatile void *)s_vqec_shm_rspbuf_ptr

#include "vqec_rpc_client.c_rpcgen"
#include "vqec-dp/vqec_dp_io_stream.c"

