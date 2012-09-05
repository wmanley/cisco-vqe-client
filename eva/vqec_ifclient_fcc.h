//------------------------------------------------------------------
// Vqec. Client Interface API for Rapid Channel Change.
//
//
// Copyright (c) 2007-2008  by cisco Systems, Inc.
// All rights reserved.
//------------------------------------------------------------------

#ifndef __VQEC_IFCLIENT_FCC_H__
#define __VQEC_IFCLIENT_FCC_H__

#include "vqec_ifclient_defs.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*----------------------------------------------------------------------------
 * Get the PAT stored in the source associated with a given tuner.
 *
 *  @param[in]  id      ID of the tuner whose PAT to get.
 *  @param[out]  pat_buf  Pointer to buffer to copy PAT to.
 *  @param[out]  pat_len  Pointer to value to copy PAT length to.
 *  @param[in]  buf_len Size of pat_buf.
 *--------------------------------------------------------------------------*/
vqec_error_t 
vqec_ifclient_tuner_get_pat(const vqec_tunerid_t id,
                            uint8_t *pat_buf,
                            uint32_t *pat_len,
                            uint32_t buf_len);

/*----------------------------------------------------------------------------
 * Get the PMT stored in the source associated with a given tuner.
 *
 *  @param[in]  id      ID of the tuner whose PMT to get.
 *  @param[out]  pmt_buf  Pointer to buffer to copy PMT to.
 *  @param[out]  pmt_len  Pointer to value to copy PMT length to.
 *  @param[in]  buf_len Size of pmt_buf.
 *--------------------------------------------------------------------------*/
vqec_error_t 
vqec_ifclient_tuner_get_pmt(const vqec_tunerid_t id,
                            uint8_t *pmt_buf,
                            uint32_t *pmt_len,
                            uint32_t buf_len);

/*----------------------------------------------------------------------------
 * Get the PMT's PID stored in the source associated with a given tuner.
 *
 *  @param[in]  id      ID of the tuner whose PMT to get.
 *  @param[out]  pmt_pid  Pointer to value to copy PMT PID to.
 *--------------------------------------------------------------------------*/
vqec_error_t 
vqec_ifclient_tuner_get_pmt_pid(const vqec_tunerid_t id,
                            uint16_t *pmt_pid);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __VQEC_IFCLIENT_FCC_H__ */
