/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_ifclient_fcc.c
 *
 * Description: VQEC Public API implementation for Rapid Channel Change.
 *
 * Documents:
 *
 *****************************************************************************/
#if HAVE_FCC

#define _GNU_SOURCE 1

#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>

#include "rtp_repair_recv.h"
#include <utils/mp_tlv_decode.h>
#include "vqec_tuner.h"
#include "vqec_lock_defs.h"

#include "vqec_ifclient_fcc.h"
#include "vqec_ifclient.h"
#include "vqec_ifclient_private.h"

#include "vqec_error.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

UT_STATIC vqec_error_t 
vqec_ifclient_tuner_get_pat_ul(const vqec_tunerid_t id,
                               uint8_t *pat_buf,
                               uint32_t *pat_len,
                               uint32_t buf_len);
UT_STATIC vqec_error_t
vqec_ifclient_tuner_get_pmt_ul(const vqec_tunerid_t id,
                               uint8_t *pmt_buf,
                               uint32_t *pmt_len,
                               uint32_t buf_len);
UT_STATIC vqec_error_t
vqec_ifclient_tuner_get_pmt_pid_ul(const vqec_tunerid_t id,
                                   uint16_t *pmt_pid);

/*----------------------------------------------------------------------------
 * Thread-safe wrappers for synchronized methods. 
 * [These contain no implementation]
 *--------------------------------------------------------------------------*/

vqec_error_t
vqec_ifclient_tuner_get_pat (const vqec_tunerid_t id,
                             uint8_t *pat_buf,
                             uint32_t *pat_len,
                             uint32_t buf_len)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_get_pat_ul(id, pat_buf, pat_len, buf_len);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);
}

vqec_error_t 
vqec_ifclient_tuner_get_pmt (const vqec_tunerid_t id,
                             uint8_t *pmt_buf,
                             uint32_t *pmt_len,
                             uint32_t buf_len)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_get_pmt_ul(id, pmt_buf, pmt_len, buf_len);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);
}

vqec_error_t
vqec_ifclient_tuner_get_pmt_pid (const vqec_tunerid_t id,
                                 uint16_t *pmt_pid)
{
    vqec_error_t retval;

    vqec_lock_lock(vqec_g_lock);
    retval = vqec_ifclient_tuner_get_pmt_pid_ul(id, pmt_pid);
    vqec_lock_unlock(vqec_g_lock);
    
    return (retval);
}




/*----------------------------------------------------------------------------
 * Get the PAT stored in the source of a given tuner.
 *--------------------------------------------------------------------------*/
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_get_pat_ul (const vqec_tunerid_t id,
                                uint8_t *pat_buf,
                                uint32_t *pat_len,
                                uint32_t buf_len)
{
    vqec_error_t err;

    err = vqec_tuner_get_pat(id, 
                             pat_buf,
                             pat_len,
                             buf_len);
    if (err != VQEC_OK) {        
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }

    return (err);
}

/*----------------------------------------------------------------------------
 * Get the PMT stored in the source of a given tuner.
 *--------------------------------------------------------------------------*/
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_get_pmt_ul (const vqec_tunerid_t id,
                                uint8_t *pmt_buf,
                                uint32_t *pmt_len,
                                uint32_t buf_len)
{
    vqec_error_t err = VQEC_OK;
    uint16_t pmt_pid = 0;

    err = vqec_tuner_get_pmt(id, 
                             &pmt_pid,
                             pmt_buf,
                             pmt_len,
                             buf_len);

    if (err != VQEC_OK) {        
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }
    
    return (err);
}

/*----------------------------------------------------------------------------
 * Get the PMT PID stored in the source of a given tuner.
 *--------------------------------------------------------------------------*/
UT_STATIC vqec_error_t 
vqec_ifclient_tuner_get_pmt_pid_ul (const vqec_tunerid_t id,
                                    uint16_t *pmt_pid)
{
    vqec_error_t err = VQEC_OK;
    uint32_t pmt_len = 0;
    uint8_t *pmt_buf;

    pmt_buf = malloc(VQEC_MSG_MAX_DATAGRAM_LEN);
    if (!pmt_buf) {
        err = VQEC_ERR_MALLOC;
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));        
        return (err);
    }

    err = vqec_tuner_get_pmt(id, 
                             pmt_pid,
                             pmt_buf,
                             &pmt_len,
                             VQEC_MSG_MAX_DATAGRAM_LEN);
    
    if (err != VQEC_OK) {        
        vqec_ifclient_log_err(VQEC_IFCLIENT_ERR_GENERAL, "%s %s", 
                              __FUNCTION__, vqec_err2str(err));
    }
    if (pmt_buf) {
        free(pmt_buf);
    }

    return (err);
}
#endif /* HAVE_FCC */
