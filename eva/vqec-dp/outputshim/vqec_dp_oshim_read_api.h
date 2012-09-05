/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_dp_oshim_read_api.h
 *
 * Description: Output shim reader implementation.
 *
 * Documents:
 *
 *****************************************************************************/

#ifndef __VQEC_DP_OSHIM_READ_API_H__
#define __VQEC_DP_OSHIM_READ_API_H__

/**
 * This function is used to receive a VQEC repaired RTP datagram stream,  
 * or a raw UDP stream, corresponding to a channel. The function
 * can be used as an "on-demand" reader in either user- or kernel- space,
 * although it's envisioned that a "push" mechanism will be used in kernel.
 * RTP headers may be stripped, based on the global configuration settings.
 *  
 * The routine returns the total number of bytes that have been copied
 * into the input iobuf array in <I>*len</I>. Received datagrams 
 * are copied into the caller's first buffer until no more whole datagrams
 * fit (or a packet supporting fast-channel-change is received--more on this
 * later). If the received datagram is too big to fit in the space remaining 
 * in the current buffer, an attempt will be made to write the datagram to
 * the beginning of the next buffer, until all buffers are exhausted.
 *
 * VQEC only supports receipt of datagrams upto length of 
 * max_paksize (from init params). A maximum of max_iobuf_cnt (from init
 * params) buffers can be returned in one single call. 
 *
 * Callers are responsible for passing in an array of iobufs, and assigning
 * the "buf_ptr" and "buf_len" fields of each iobuf in the array.  Upon 
 * return, the "buf_wrlen" and "buf_flags" fields will be assigned for 
 * each buffer, indicating the amount of data written to the buffer and 
 * any status flags, respectively.
 *
 * The "buf_flags" field may contain the VQEC_DP_BUF_FLAGS_APP upon return.
 * This flag indicates MPEG management information that may be useful
 * for fast-channel-change has been received inline with RTP or UDP PDUs.
 * This only occurs at the onset of channel-change, and before the
 * reception of RTP or UDP payloads.  Upon receipt of a fast-channel-change
 * APP packet, the APP packet will be copied into the next available iobuf,
 * and the VQEC_DP_BUF_FLAGS_APP flag will be set.  The API call will return
 * immediately after copying the APP packet into the buffer and before filling
 * any remaining buffers with UDP or RTP datagrams, if present.
 *
 * If no receive datagrams are available, and timeout set to 0, the call
 * returns immediately with the return value VQEC_DP_ERR_OK, and
 * <I>*len</I> set to 0. Otherwise any available datagrams
 * are copied into the iobuf array, and <I>*len</I> is the
 * total number of bytes copied. This is non-blocking mode of operation.
 *
 * If timeout > 0, then the call will block until either of the 
 * following two conditions is met:
 *
 * (a) iobuf_num datagram buffers are received before timeout 
 *     is reached. <BR>                
 * (b) The timeout occurs before iobuf_num buffers are received. 
 *	In this case the call will return with the number of buffers 
 *	that have been received till that instant. If no data is
 *     available, the return value will be VQEC_DP_ERR_OK, with
 *     <I>*len</I> set to 0.
 *  
 * If timeout is -1, then the call will block indefinitely until 
 * iobuf_num datagram buffers are received. 
 * 
 * If the invocation of the call fails  <I>*len</I> will be
 * set to 0, with the return value indicative of the failure condition.
 *
 * @param[in]	id Existing tuner object's identifier.
 * @param[in]   clientkey Client pthread key for the invoking thread.
 * @param[in]	iobuf Array of buffers into which received data is copied.
 * @param[in]	iobuf_num Number of buffers in the input array.
 * @param[out]  len The total number of bytes copied. The value 
 * shall be 0 if no data became available within the specified timeout, which
 * itself maybe 0. The return code will be VQEC_OK for these cases.
 * If the call fails, or if the tuner is deleted, channel
 * association changed, etc. while the call was blocked bytes_read will still
 * be 0, however, the return code will signify an error.
 * @param[in]	timeout Timeout specified in milliseconds. A timeout
 * of -1 corresponds to an infinite timeout. In that case the call  
 * will return only after iobuf_num datagrams have been received. A timeout
 * of 0 corresponds to a non-blocking mode of operation.
 * Timeout is always truncated to VQEC_DP_MSG_MAX_RECV_TIMEOUT.
 * @param[out]	vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success with the total number of bytes copied in <I>*len</I>. Upon failure, 
 * <I>*len</I> is 0, with following return codes:
 *
 *     <I>VQEC_DP_ERR_NOSUCHTUNER</I><BR>
 *     <I>VQEC_DP_ERR_NOSUCHSTREAM</I><BR>
 *     <I>VQEC_DP_ERR_INVALIDARGS</I><BR>
 *     <I>VQEC_DP_ERR_INTERNAL</I><BR>
 *     <I>VQEC_DP_ERR_NOMEM</I><BR>
 */
vqec_dp_error_t
vqec_dp_oshim_read_tuner_read(vqec_dp_tunerid_t id,
                              vqec_iobuf_t *iobuf,
                              uint32_t iobuf_num,
                              uint32_t *len,
                              int32_t timeout_msec);

/**
 * Initialize the oshim_read module.
 *
 * @param[in] max_tuners Maximum number of simultaneous tuners
 * @param[out] boolean Returns TRUE on success.
 */
boolean vqec_dp_oshim_read_init(uint32_t max_tuners);

/**
 * Deinitialize the oshim_read module.
 */
void vqec_dp_oshim_read_deinit(void);

struct __wait_queue_head;
struct __wait_queue_head *
vqec_dp_outputshim_get_waitq_by_id(vqec_dp_tunerid_t tid);

#endif  /* __VQEC_DP_OSHIM_READ_API_H__ */
