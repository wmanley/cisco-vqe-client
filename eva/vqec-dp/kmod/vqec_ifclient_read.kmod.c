/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_ifclient.c
 *
 * Description: VQEC Public API implementation.
 *
 * Documents:
 *
 *****************************************************************************/

#include <vqec_dp_api.h>
#include <vqec_dp_output_shim_private.h>
#include <vqec_dp_oshim_read_api.h>
#include "linux/wait.h"
#include <vqec_sink.h>
#include <vqec_lock.h>
#include <linux/jiffies.h>

#define VQEC_DP_ERR_STRLEN 256
void vqec_ifclient_read_log_err (const char *format, ...)
{
    char buf[VQEC_DP_ERR_STRLEN];
    va_list ap;

    va_start(ap, format);    
    vsnprintf(buf, VQEC_DP_ERR_STRLEN, format, ap);
    va_end(ap);

    VQEC_DP_SYSLOG_PRINT(OUTPUTSHIM_ERROR, buf);
}

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
 * @param[out]        vqec_err_t Returns VQEC_DP_ERR_OK on success with the 
 * total number of bytes copied in <I>*bytes_read</I>. Upon failure,
 * <I>*bytes_read</I> is 0, with following return codes:
 *
 *     <I>VQEC_DP_ERR_NOSUCHTUNER</I><BR>
 *     <I>VQEC_DP_ERR_INVALIDARGS</I><BR>
 *     <I>VQEC_DP_ERR_INTERNAL</I><BR>
 *     <I>VQEC_DP_ERR_NOSUCHSTREAM</I><BR>
 *----------------------------------------------------------------------------
 */
static inline vqec_error_t vqec_ifclient_tuner_recvmsg_ul (
    const vqec_tunerid_t cp_tid,
    vqec_iobuf_t *iobuf,
    int32_t iobuf_num,
    int32_t *bytes_read,
    int32_t timeout)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    int32_t cur_buf = 0, i;
    vqec_dp_output_shim_tuner_t *tuner, *cur_t;
    vqec_tunerid_t dp_tid;
    vqec_sink_waiter_t wait_buf;
    boolean rd_more_paks = TRUE;
    DEFINE_WAIT(waitq);
    wait_queue_head_t *qh;

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    abs_time_t enter_ts;

    enter_ts = get_sys_time();
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */

    dp_tid = vqec_dp_output_shim_cp_to_dp_tunerid(cp_tid);

    if (unlikely((dp_tid == VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID)
                 || !bytes_read
                 || !iobuf
                 || !iobuf_num)) {
        if (bytes_read) {
            *bytes_read = 0; 
        }
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    *bytes_read = 0; 
    if (unlikely(timeout > (int32_t)g_output_shim.iobuf_recv_timeout)) {
        timeout = g_output_shim.iobuf_recv_timeout;
    }
    if (unlikely(iobuf_num > g_output_shim.max_iobuf_cnt)) {
        iobuf_num = g_output_shim.max_iobuf_cnt;
    }
   for (i=0; i < iobuf_num; i++) {
            iobuf[i].buf_wrlen = 0;
            iobuf[i].buf_flags = 0;
   }

    if (timeout > 0) {
        timeout = msecs_to_jiffies(timeout);
    } else if (timeout < 0) {
        timeout = MAX_SCHEDULE_TIMEOUT;
    }
    tuner = vqec_dp_output_shim_get_tuner_by_id(dp_tid);
    if (unlikely(!tuner)) {
        err = VQEC_DP_ERR_NOSUCHTUNER;
        vqec_ifclient_read_log_err("%s: no such tuner", __FUNCTION__);
        return (err);
    } else if (unlikely((tuner->is == VQEC_DP_INVALID_ISID) || 
                        !tuner->sink)) {
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
        if (tuner->sink) {
            tuner->sink->exit_ts = ABS_TIME_0;
        }
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
        err = VQEC_DP_ERR_INTERNAL;
        return (err);
    }

    if (unlikely(tuner->sink->output_sock)) {
        err = VQEC_DP_ERR_INTERNAL;
        vqec_ifclient_read_log_err("%s: output socket exists; check setting "
                                   "of deliver_paks_to_user in system config",
                                   __FUNCTION__);
        return (err);
    }

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    MCALL(tuner->sink, vqec_sink_upd_reader_jitter, enter_ts);
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
    vqec_dp_oshim_read_tuner_read_one_copy(dp_tid, 
                                           tuner->sink,
                                           &cur_buf, 
                                           iobuf,
                                           iobuf_num, 
                                           bytes_read);

    /*
     * Return immediately after copying available data if either:
     *  1. caller requested non-blocking behavior,
     *  2. all of caller's buffers have been used/attempted, or
     *  3. an APP packet was copied into an iobuf
     */
    if (!timeout ||
        (cur_buf == iobuf_num)
#ifdef HAVE_FCC
        || ((cur_buf > 0) &&
            (iobuf[cur_buf-1].buf_flags & VQEC_DP_BUF_FLAGS_APP))
#endif /* HAVE_FCC */
        ) {
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
        tuner->sink->exit_ts = get_sys_time();
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
        return (err);
    }

    /*
     * Block waiting for further packets, until either the timeout period
     * expires or until the iobuf's are all utilized. The sink will send a
     * wakeup when all of the iobuf's are utilized. The iobuf data and
     * the associated wait queue head ptr are provisioned for the sink.
     */
    wait_buf.iobuf   = &iobuf[0];
    wait_buf.buf_cnt = iobuf_num;
    wait_buf.i_cur   = 0;
    wait_buf.len     = 0;
    wait_buf.waitq   = (qh = vqec_dp_outputshim_get_waitq_by_id(dp_tid));
    if (unlikely(!qh)) {
        err = VQEC_DP_ERR_INTERNAL;
        return (err);
    }
    MCALL(tuner->sink, vqec_sink_add_waiter, &wait_buf);

    while (TRUE) {
        /* Add the task onto the tuner's wait queue. */
        prepare_to_wait_exclusive(qh,
                                  &waitq,
                                  TASK_INTERRUPTIBLE);
        /* If a signal is pending let the task handle it. */
        if (unlikely(signal_pending(current))) {
            rd_more_paks = FALSE;
            goto __deq_task_from_list;
        }
        /*
         * Release, reacquire lock around the timed sleep. If timeout is
         * MAX_SCHEDULE_TIMEOUT, schedule_timeout will block indefinitely.
         * The return value is the remaining timeout, and is
         * MAX_SCHEDULE_TIMEOUT if the input is MAX_SCHEDULE_TIMEOUT.
         */
        vqec_lock_unlock(g_vqec_dp_lock);
        timeout = schedule_timeout(timeout);

    __deq_task_from_list:
        finish_wait(qh, &waitq);

        vqec_lock_lock(g_vqec_dp_lock);
        cur_buf = wait_buf.i_cur;
        *bytes_read = wait_buf.len;
        if ((cur_buf == iobuf_num) || (timeout <= 0) || !rd_more_paks
#ifdef HAVE_FCC
            || ((cur_buf > 0) &&
                (iobuf[cur_buf-1].buf_flags & VQEC_DP_BUF_FLAGS_APP))
#endif /* HAVE_FCC */
            ) {
            break;
        }

        /*
         * Check for tuner delete / re-create, or unbind / rebind while the
         * thread was suspended. If so, return error to the caller.
         */
        cur_t = vqec_dp_output_shim_get_tuner_by_id(dp_tid);
        if (unlikely(!cur_t || (cur_t != tuner))) {
            tuner = NULL;
            err = VQEC_DP_ERR_NOSUCHTUNER;
            printk("No tuner error");
            break;
        } else if (unlikely(!tuner->is) || (!tuner->sink)) {
            err = VQEC_DP_ERR_NOSUCHSTREAM;
            break;
        }
    }

    if (likely(tuner && tuner->sink)) {
        MCALL(tuner->sink, vqec_sink_del_waiter);
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
        tuner->sink->exit_ts = get_sys_time();
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
    }

    return (err);
}

vqec_error_t vqec_ifclient_tuner_recvmsg (const vqec_tunerid_t cp_tid,
                                          vqec_iobuf_t *iobuf,
                                          int32_t iobuf_num,
                                          int32_t *bytes_read,
                                          int32_t timeout)
{
    vqec_error_t err;

    vqec_lock_lock(g_vqec_dp_lock);
    err = vqec_ifclient_tuner_recvmsg_ul(cp_tid, iobuf, iobuf_num,
                                         bytes_read, timeout);
    vqec_lock_unlock(g_vqec_dp_lock);

    return (err);
}

EXPORT_SYMBOL(vqec_ifclient_tuner_recvmsg);

/**---------------------------------------------------------------------------
 * Map a tuner's given name to it's id.
 *
 * @param[in]  namestr Unique name of the tuner; *namestr cannot be an
 * empty string.
 * @param[in]  id The opaque integral identifier for the tuner object
 * identified by *namestr. If there are errors id is set to VQEC_TUNERID_INVALID.
 * @param[out] vqec_err_t Returns VQEC_OK on success. Otherwise,
 * the following failures may occur:
 *
 *     <I>VQEC_ERR_NOSUCHTUNER</I><BR>
 *----------------------------------------------------------------------------
 */
vqec_tunerid_t
vqec_ifclient_tuner_get_id_by_name (const char *namestr)
{
    return (vqec_dp_outputshim_get_tunerid_by_name(namestr));
}

EXPORT_SYMBOL(vqec_ifclient_tuner_get_id_by_name);

/**---------------------------------------------------------------------------
 * Map a tuner's id to it's name.
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
vqec_error_t
vqec_ifclient_tuner_get_name_by_id (vqec_tunerid_t id,
                                    char *name,
                                    int32_t len)
{
    return (vqec_dp_outputshim_get_name_by_tunerid(id,
                                                   name,
                                                   len));
}

EXPORT_SYMBOL(vqec_ifclient_tuner_get_name_by_id);
