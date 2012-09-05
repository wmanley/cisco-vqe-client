/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_dp_oshim_read_api.c
 *
 * Description: Output shim reader implementation.
 *
 * Documents:
 *
 *****************************************************************************/

#ifdef _VQEC_DP_UTEST
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

#include <vqec_dp_output_shim_api.h>
#include <vqec_dp_output_shim_private.h>
#include <vqec_dp_io_stream.h>
#include <vqec_lock_defs.h>
#include <vqec_sink.h>
#include <vqec_dp_syslog_def.h>
#include <sys/time.h>
#include <utils/zone_mgr.h>

/* local variables for tuner_read() */
static pthread_key_t s_vqec_clientkey;
static pthread_once_t s_once_ctl = PTHREAD_ONCE_INIT;
static boolean s_key_init_done;
static struct vqe_zone *s_waiter_pool = NULL;

#define TIMEVAL_TO_TIMESPEC(tv, ts) {                                   \
        (ts)->tv_sec = (tv)->tv_sec;                                    \
        (ts)->tv_nsec = (tv)->tv_usec * 1000;                           \
}

#define VQEC_DP_ERR_STRLEN 256
UT_STATIC inline void
vqec_dp_oshim_read_log_err (const char *format, ...)
{
    char buf[VQEC_DP_ERR_STRLEN];
    va_list ap;

    va_start(ap, format);    
    vsnprintf(buf, VQEC_DP_ERR_STRLEN, format, ap);
    va_end(ap);

    VQEC_DP_SYSLOG_PRINT(OUTPUTSHIM_ERROR, buf);
}

static inline vqec_sink_waiter_t *
vqec_dp_outputshim_get_pthread_waiter (void)
{
    vqec_sink_waiter_t *waiter;
    vqec_dp_error_t err;
    int32_t ret;

    if (!(waiter = 
          (vqec_sink_waiter_t *)pthread_getspecific(s_vqec_clientkey))) {

        waiter = (vqec_sink_waiter_t *) zone_acquire(s_waiter_pool);

        if (!waiter) {
            err = VQEC_DP_ERR_NOMEM;
            vqec_dp_oshim_read_log_err("%s (waiter)", __FUNCTION__);
            goto done;
        } 
        memset(waiter, 0, sizeof(vqec_sink_waiter_t));

        if ((ret = pthread_setspecific(s_vqec_clientkey, waiter))) {
            err = VQEC_DP_ERR_INTERNAL;
            vqec_dp_oshim_read_log_err("%s (pthread_setspecific %s)", 
                            __FUNCTION__, strerror(ret));
            goto done;

        } else if ((ret = pthread_cond_init(&waiter->cond, NULL))) {
            err = VQEC_DP_ERR_INTERNAL;
            vqec_dp_oshim_read_log_err("%s (pthread_condinit %s)",
                            __FUNCTION__, strerror(ret));
            goto done;

        }
    }

    return (waiter);

  done:

    if (waiter) {
        /* sa_ignore {ignore recursive failure} IGNORE_RETURN (1) */
        pthread_setspecific(s_vqec_clientkey, NULL);
        zone_release(s_waiter_pool, waiter);
    }

    return (NULL);
}



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
 *-----------------------------------------------------------------------------
 *
 * (a) pthread_... calls: This function relies on the use several pthread_... 
 * library calls. The meaning, and use of these calls is described below:
 *
 * pthread_once(pthread_once_t once_ctl, void (*init_routine)(void): 
 * The first call to pthread_once() by any thread with a given "once_ctl" 
 * calls init_routine; subsequent calls to pthread_once() do not call the
 * init routine. The special once_ctl constant PTHREAD_ONCE_INIT is used 
 * as the first argument.
 *
 * pthread_key_create(pthread_key_t *key, (void *)(*destructor)(void)):
 * Creates a thread-specific data key which is visible to all threads in
 * the process. Although, the same data key is visible to all threads, the
 * data values bound to the key by pthread_setspecific() are maintained
 * on a per-thread basis, and exist for the lifetime of the thread. Upon
 * creation the value NULL is associated with the key for all active
 * threads, and in a newly created thread all existing keys will have NULL
 * associated values. A destructor can be associated with each key, and
 * upon exit of each thread, if a key has a non-null destructor, and the
 * thread has a non-NULL value associated with the key, the destructor is
 * called is called with the key's associated value.
 *
 * pthread_getspecific(pthread_key_t *key):
 * This function returns the current value bound to the specified key on
 * behalf of the calling thread.
 *
 * pthread_setspecific(pthread_key_t *key, const void *value):
 * This function associates a thread-specific value with the specified key.
 * The key must have been created with pthread_key_create. If the key has
 * been deleted prior to calling this function, it's behavior is undefined. 
 *
 * (b) The flow of the code below is as follows: If there are packets
 * on the sink queue, first copy them into the iobuf's. Else, if a timeout
 * is specified, enQ the user-specified list of iobuf's as a "waiter"
 * data structure on the sink. Packets will be copied into these iobuf's
 * as the sink receives them from the pcm. The thread will be woken up
 * once iobuf_num packets have been received, or the timeout expires.
 */
vqec_dp_error_t
vqec_dp_oshim_read_tuner_read (vqec_dp_tunerid_t id,
                                vqec_iobuf_t *iobuf,
                                uint32_t iobuf_num,
                                uint32_t *len,
                                int32_t timeout_msec)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    int32_t cur_buf, ret, i;
    vqec_dp_output_shim_tuner_t *tuner, *tuner_n;
    vqec_sink_t *sink;
    vqec_sink_waiter_t *waiter;
    struct timeval tv_beg, tv_end;
    struct timespec ts_end;
    boolean waiter_on_q = FALSE;

    cur_buf = 0;
    if (len) {
        *len = 0; 
    }

    if ((id > g_output_shim.max_tuners || id < 1)
        || !len
        || !iobuf
        || !iobuf_num) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    /* Clear the output fields of all of the caller's iobufs */
    for (i=0; i < iobuf_num; i++) {
        iobuf[i].buf_wrlen = 0;
        iobuf[i].buf_flags = 0;
    }

    timeout_msec = 
        OS_MIN(timeout_msec, g_output_shim.iobuf_recv_timeout);
    iobuf_num = 
        OS_MIN(iobuf_num, g_output_shim.max_iobuf_cnt);

    if (timeout_msec > 0) {
        /* Form absolute expiration time for blocking. */
        tv_beg = abs_time_to_timeval(get_sys_time());
        tv_end.tv_sec = 0;
        tv_end.tv_usec = timeout_msec * 1000;
        timeradd(&tv_beg, &tv_end, &tv_end);
            
        TIMEVAL_TO_TIMESPEC(&tv_end, &ts_end);
    }

    /* Per-thread cached wait object. */
    waiter = vqec_dp_outputshim_get_pthread_waiter();
    if (!waiter) {
        err = VQEC_DP_ERR_INTERNAL;
        return (err);
    }

    tuner = vqec_dp_output_shim_get_tuner_by_id(id);
    if (!tuner) {
        err = VQEC_DP_ERR_NOSUCHTUNER;
        vqec_dp_oshim_read_log_err("%s: no such tuner", __FUNCTION__);
        return (err);
    } else if (tuner->is == VQEC_DP_INVALID_ISID) {
        err = VQEC_DP_ERR_NOSUCHSTREAM;
        return (err);
    }

    sink = tuner->sink;    
    vqec_dp_oshim_read_tuner_read_one_copy(id, 
                                            sink, 
                                            &cur_buf, 
                                            iobuf,
                                            iobuf_num, 
                                            len);

    /*
     * Return immediately after copying available data if either:
     *  1. caller requested non-blocking behavior,
     *  2. all of caller's buffers have been used/attempted, or
     *  3. an APP packet was copied into an iobuf
     */
    if (!timeout_msec || 
        (cur_buf == iobuf_num) 
#ifdef HAVE_FCC
        || ((cur_buf > 0) &&
            (iobuf[cur_buf-1].buf_flags & VQEC_DP_BUF_FLAGS_APP))
#endif /* HAVE_FCC */
        ) {
        return (err);
    }


    while (TRUE) {

        if (timeout_msec == -1) {
            waiter->iobuf   = &iobuf[cur_buf];
            waiter->buf_cnt = iobuf_num - cur_buf; 
            waiter->i_cur   = 0; 
            waiter->len     = 0;
            
            if (!waiter_on_q) {
                MCALL(sink, vqec_sink_add_waiter, waiter);
                waiter_on_q = TRUE;
            }
            if ((ret = 
                 pthread_cond_wait(&waiter->cond, 
                                   &vqec_lockdef_vqec_g_lock.mutex))) {
                
                err = VQEC_DP_ERR_INTERNAL;
                vqec_dp_oshim_read_log_err("%s (pthread_cond_wait %s)", 
                                __FUNCTION__, strerror(ret));
                break;
            }
            cur_buf += waiter->i_cur;
            *len += waiter->len;
            if (cur_buf == iobuf_num) {
                break;
            }
        } else {
            waiter->iobuf   = &iobuf[cur_buf];
            waiter->buf_cnt = iobuf_num - cur_buf;            
            waiter->i_cur   = 0; 
            waiter->len     = 0;
            
            if (!waiter_on_q) {
                MCALL(sink, vqec_sink_add_waiter, waiter);
                waiter_on_q = TRUE;
            }
            if ((ret = 
                 pthread_cond_timedwait(&waiter->cond, 
                                        &vqec_lockdef_vqec_g_lock.mutex, 
                                        &ts_end))) {
                if (ret == ETIMEDOUT) {
                    cur_buf += waiter->i_cur;
                    *len += waiter->len;
                    break;
                } else {
                    err = VQEC_DP_ERR_INTERNAL;
                    vqec_dp_oshim_read_log_err("%s (pthread_cond_wait %s)", 
                                    __FUNCTION__, strerror(ret));
                    break;
                }
            }

            cur_buf += waiter->i_cur;
            *len += waiter->len;
            if (cur_buf == iobuf_num) {
                break;
            }
        }
            
        /*
         * Check if the tuner was deleted/re-created, or an unbind was
         * invoked while the thread was suspended. If so, return error
         * to the caller.
         */
        tuner_n = vqec_dp_output_shim_get_tuner_by_id(id);
        if (!tuner_n || (tuner_n != tuner)) {
            tuner = NULL;
            err = VQEC_DP_ERR_NOSUCHTUNER;
            vqec_dp_oshim_read_log_err("%s (nosuchtuner) (%p/%p)",
                            __FUNCTION__, tuner, tuner_n);
            break;
        } else if (!tuner->is) {
            err = VQEC_DP_ERR_NOSUCHSTREAM;
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_OUTPUTSHIM, 
                          "%s NOSUCHSTREAM (source removed)", __FUNCTION__);
            break;
        }
    }

    /* remove the waiter from the sink */
    if (waiter_on_q && tuner && tuner->sink) {
        MCALL(tuner->sink, vqec_sink_del_waiter, waiter);
    }

    return (err);
}

void vqec_dp_oshim_read_key_deallocate (void *arg)
{
    if (arg && s_waiter_pool) {
        zone_release(s_waiter_pool, arg);
    }
}

void vqec_dp_oshim_read_key_init (void) 
{
    int32_t err;
    char buf[VQEC_DP_ERR_STRLEN];

    if (!(err = pthread_key_create(&s_vqec_clientkey, 
                                   vqec_dp_oshim_read_key_deallocate))) {
        s_key_init_done = TRUE;
    } else {
        s_key_init_done = FALSE;
        if (!strerror_r(err, buf, VQEC_DP_ERR_STRLEN)) {
            buf[VQEC_DP_ERR_STRLEN - 1] = '\0';
        } else {
            buf[0] = '\0';
        }
        vqec_dp_oshim_read_log_err("%s (pthread_key_create %s)",
                                   __FUNCTION__, buf);
    }
}

boolean vqec_dp_oshim_read_init (uint32_t max_tuners)
{
    int32_t ret;

    if ((ret = pthread_once(&s_once_ctl, vqec_dp_oshim_read_key_init))) {
        vqec_dp_oshim_read_log_err("%s (pthread_once %s)",
                                    __FUNCTION__, strerror(ret)); 
        return FALSE;
    } else if (!s_key_init_done) {
        vqec_dp_oshim_read_log_err("%s (pthread_key_create)",
                                    __FUNCTION__);
        return FALSE;
    }

    if (s_waiter_pool) {
        return FALSE;
    }
    s_waiter_pool = zone_instance_get_loc("sink_waiter",
                                          O_CREAT,
                                          sizeof(vqec_sink_waiter_t),
                                          max_tuners,
                                          NULL, NULL);
    if (!s_waiter_pool) {
        return FALSE;
    }

    return TRUE;
}

void vqec_dp_oshim_read_deinit (void)
{
    if (s_waiter_pool) {
        (void) zone_instance_put(s_waiter_pool);
        s_waiter_pool = NULL;
    }

    /* destroy the ifclient pthread key if it exists */
    if (pthread_getspecific(s_vqec_clientkey)) {
        pthread_key_delete(s_vqec_clientkey);
    }

    bzero(&s_vqec_clientkey, sizeof(pthread_key_t));
    s_key_init_done = FALSE;
    s_once_ctl = PTHREAD_ONCE_INIT;
}
