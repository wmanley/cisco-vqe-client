
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
 * Description: DP output API header file.  This file defines the APIs that
 *              shall be used to read repaired stream packets out of VQE-C.
 *
 * Documents: 
 *
 *****************************************************************************/

#ifndef __VQEC_IFCLIENT_TUNER_READ_H__
#define __VQEC_IFCLIENT_TUNER_READ_H__

#include "vqec_error.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifndef VQEC_PUBLIC
#define VQEC_PUBLIC extern
#endif

#ifndef VQEC_SYNCHRONIZED
#define VQEC_SYNCHRONIZED
#endif

#define VQEC_TUNERID_INVALID (-1)
typedef int32_t vqec_tunerid_t;

/**
 * IO buffer abstraction for receiving UDP datagrams.
 */
typedef struct vqec_iobuf_
{
    /**
     * Pointer to the memory area.
     */
    void       *buf_ptr;
    /**
     * Length of memory area.
     */
    uint32_t    buf_len;
    /**
     * Length of data written.
     */
    uint32_t    buf_wrlen;
    /**
     * Status flags.
     */
    uint32_t    buf_flags;
} vqec_iobuf_t;

/**---------------------------------------------------------------------------
 * This function is used to receive a VQEC repaired RTP datagram stream,
 * or a raw UDP stream, corresponding to a channel. The function
 * can be used as an "on-demand" reader in either user- or kernel- space,
 * although it's envisioned that a "push" mechanism will be used in kernel.
 * RTP headers may be stripped, based on the global configuration settings.
 *
 * The routine returns the total number of bytes that have been copied
 * into the input iobuf array in <I>*bytes_read</I>. Received datagrams
 * are copied into the caller's first buffer until no more whole datagrams
 * fit (or a packet supporting fast-channel-change is received--more on this
 * later). If the received datagram is too big to fit in the space remaining
 * in the current buffer, an attempt will be made to write the datagram to
 * the beginning of the next buffer, until all buffers are exhausted.
 *
 * VQEC only supports receipt of datagrams upto length of
 * VQEC_MSG_MAX_DATAGRAM_LEN. A maximum of VQEC_MSG_MAX_IOBUF_CNT
 * buffers can be returned in one single call.
 *
 * Callers are responsible for passing in an array of iobufs, and assigning
 * the "buf_ptr" and "buf_len" fields of each iobuf in the array.  Upon
 * return, the "buf_wrlen" and "buf_flags" fields will be assigned for
 * each buffer, indicating the amount of data written to the buffer and
 * any status flags, respectively.
 *
 * The "buf_flags" field may contain the VQEC_MSG_FLAGS_APP upon return.
 * This flag indicates MPEG management information that may be useful
 * for fast-channel-change has been received inline with RTP or UDP PDUs.
 * This only occurs at the onset of channel-change, and before the
 * reception of RTP or UDP payloads.  Upon receipt of a fast-channel-change
 * APP packet, the APP packet will be copied into the next available iobuf,
 * and the VQEC_MSG_FLAGS_APP flag will be set.  The API call will return
 * immediately after copying the APP packet into the buffer and before filling
 * any remaining buffers with UDP or RTP datagrams, if present.
 *
 * If no receive datagrams are available, and timeout set to 0, the call
 * returns immediately with the return value VQEC_OK, and
 * <I>*bytes_read</I> set to 0. Otherwise any available datagrams
 * are copied into the iobuf array, and <I>*bytes_read</I> is the
 * total number of bytes copied. This is non-blocking mode of operation.
 *
 * If timeout > 0, then the call will block until either of the
 * following two conditions is met:
 *
 * (a) iobuf_num datagram buffers are received before timeout
 *     is reached. <BR>
 * (b) The timeout occurs before iobuf_num buffers are received.
 *    In this case the call will return with the number of buffers
 *    that have been received till that instant. If no data is
 *     available, the return value will be VQEC_OK, with
 *     <I>*bytes_read</I> set to 0.
 *
 * If timeout is -1, then the call will block indefinitely until
 * iobuf_num datagram buffers are received.
 *
 * If the invocation of the call fails  <I>*bytes_read</I> will be
 * set to 0, with the return value indicative of the failure condition.
 *
 * @param[in] id Existing tuner object's identifier.
 * @param[in] iobuf Array of buffers into which received data is copied.
 * @param[in] iobuf_num Number of buffers in the input array.
 * @param[out] bytes_read The total number of bytes copied. The value
 * shall be 0 if no data became availalbe within the specified timeout, which
 * itself maybe 0. The return code will be VQEC_OK for these cases.
 * If the call fails, or if the tuner is deleted, channel
 * association changed, etc. while the call was blocked bytes_read will still
 * be 0, however, the return code will signify an error.
 * @param[in] timeout Timeout specified in milliseconds. A timeout
 * of -1 corresponds to an infinite timeout. In that case the call
 * will return only after iobuf_num datagrams have been received. A timeout
 * of 0 corresponds to a non-blocking mode of operation.
 * Timeout is always truncated to VQEC_MSG_MAX_RECV_TIMEOUT.
 * @param[out]        vqec_err_t Returns VQEC_OK on success with the total
 * number of bytes copied in <I>*bytes_read</I>. Upon failure,
 * <I>*bytes_read</I> is 0, with following return codes:
 *
 *     <I>VQEC_ERR_NOSUCHTUNER</I><BR>
 *     <I>VQEC_ERR_NOBOUNDCHAN</I><BR>
 *     <I>VQEC_ERR_INVCLIENTSTATE</I><BR>
 *     <I>VQEC_ERR_INVALIDARGS</I><BR>
 *     <I>VQEC_ERR_SYSCALL</I><BR>
 *     <I>VQEC_ERR_MALLOC</I><BR>
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t vqec_ifclient_tuner_recvmsg(const vqec_tunerid_t id,
                                         vqec_iobuf_t *iobuf,
                                         int32_t iobuf_num,
                                         int32_t *bytes_read,
                                         int32_t timeout);

/**---------------------------------------------------------------------------
 * Map a tuner's given name to its id.
 *
 * @param[in]  namestr Unique name of the tuner; *namestr cannot be an
 * empty string.
 * @param[out]  id The identifier for the tuner object identified
 * by *namestr. If there are errors, VQEC_TUNERID_INVALID is returned.
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_tunerid_t
vqec_ifclient_tuner_get_id_by_name(const char *namestr);

/**---------------------------------------------------------------------------
 * Map a tuner's id to its name.
 *
 * @param[in]  id The opaque integral identifier for a tuner object
 * @param[in]  name Array to which the tuner's name shall be copied bounded
 * by VQEC_MAX_TUNER_NAMESTR_LEN.
 * @param[in]  len Length of the "name" array in bytes. The length must be
 * greater than or equal to VQEC_MAX_TUNER_NAMESTR_LEN.
 * @param[out] vqec_err_t Returns VQEC_OK on success. Otherwise,
 * the following failures may occur:
 *
 *     <I>VQEC_ERR_NOSUCHTUNER</I><BR>
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t
vqec_ifclient_tuner_get_name_by_id(vqec_tunerid_t id,
                                   char *name,
                                   int32_t len);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __VQEC_IFCLIENT_TUNER_READ_H__ */
