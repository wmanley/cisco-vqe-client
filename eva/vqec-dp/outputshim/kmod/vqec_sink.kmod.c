/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: sink
 *
 * Documents:
 *
 *****************************************************************************/

#include "vqec_sink.h"
#include <net/sock.h>
#include <linux/file.h>
#include <linux/fs.h>

/**
 * Set the output socket to use.
 */
int32_t vqec_sink_set_sock (struct vqec_sink_ *sink,
                            int fd)
{
    int err;
    struct socket *sock;

    if (fd == VQEC_DP_OUTPUT_SHIM_INVALID_FD) {
        sink->output_sock = NULL;
        return (0);
    }

    /* lookup the socket from the fd */
    sock = sockfd_lookup(fd, &err);  /* calls fget() internally */
    if (!sock) {
        VQEC_DP_SYSLOG_PRINT(OUTPUTSHIM_ERROR,
                             "ERROR: was not able to lookup the sock for fd\n");
        return (-1);
    }

    sink->output_sock = sock;

    fput(sock->file);

    return (0);
}


/**---------------------------------------------------------------------------
 * Add a new waiter or reader thread to the sink. 
 * 
 * @param[in] sink Pointer to the sink object.
 * @param[in] waiter Pointer to the waiter object.
 *---------------------------------------------------------------------------*/ 
static void 
vqec_sink_add_waiter (vqec_sink_t * sink, 
                      vqec_sink_waiter_t *waiter)
{
    if (sink->waiter) {
        VQEC_DP_SYSLOG_PRINT(OUTPUTSHIM_ERROR,
                             "sink:: a waiter already on sink!");  
    }
    sink->waiter = waiter;
}


/**---------------------------------------------------------------------------
 * Delete a previously enqueued waiter from the sink. 
 * 
 * @param[in] sink Pointer to the sink object.
 *---------------------------------------------------------------------------*/ 
static void 
vqec_sink_del_waiter (vqec_sink_t * sink)
{
    if (!sink->waiter) {
        VQEC_DP_SYSLOG_PRINT(OUTPUTSHIM_ERROR,
                             "sink:: waiter is already deleted!");
        return;
    }
    sink->waiter = NULL;
}


/**---------------------------------------------------------------------------
 * Drop the earliest packet on the sink queue, and free up it's "header". 
 * The header is then returned to the caller for subsequent use.
 * 
 * @param[in] sink Pointer to the sink object.
 * @param[out] vqec_pak_hdr_t* Pointer to a packet header structure.
 *---------------------------------------------------------------------------*/ 
static vqec_pak_hdr_t *
vqec_sink_drop_earliest_pak (vqec_sink_t *sink)
{
    vqec_pak_hdr_t *pak_hdr;

    pak_hdr = VQE_TAILQ_FIRST(&sink->pak_queue);
    VQE_TAILQ_REMOVE(&sink->pak_queue, pak_hdr, list_obj);
    sink->rcv_len_ready_for_read -= 
        vqec_pak_get_content_len(pak_hdr->pak);
    vqec_pak_free(pak_hdr->pak);
    sink->queue_drops++;
    sink->pak_queue_depth--;

    return (pak_hdr);
}


/**---------------------------------------------------------------------------
 * Enqueue a packet for kernel-receive message processing.
 * 
 * @param[in] sink Pointer to the sink object.
 * @param[in] pak Input packet.
 * @param[out] vqec_dp_error_t Always returns OK.
 *---------------------------------------------------------------------------*/ 
static vqec_dp_error_t 
vqec_sink_kernel_enq (vqec_sink_t *sink, 
                      vqec_pak_t *pak)
{
    vqec_pak_hdr_t *pak_hdr = NULL;
    vqec_sink_waiter_t *waiter;
    int32_t readlen;
#ifdef HAVE_FCC
    boolean app_pkt_written = FALSE;
#endif /* HAVE_FCC */

    /*
     * If the number of paks on a sink's outbound Q exceeds a 
     * maximum limit, then we drop the earliest pak in the Q. 
     */    
    if (unlikely(sink->pak_queue_depth == 
                 sink->queue_size)) {
        pak_hdr = vqec_sink_drop_earliest_pak(sink);
    }

    /*
     * If there is a waiter for the sink, see if we can copy the data
     * to this waiter's buffers.
     */
    waiter = sink->waiter;

#ifdef HAVE_FCC
    /*
     * APP packets must be in their own buffer. If we are processing an APP
     * packet and a waiter is present, advance to the next buffer if the current
     * one is not empty and a buffer is available.
     */
    if (unlikely((pak->type == VQEC_PAK_TYPE_APP) && 
                 waiter && 
                 (waiter->i_cur != waiter->buf_cnt) &&
                 waiter->iobuf[waiter->i_cur].buf_wrlen)) {
        waiter->i_cur++;
    }
#endif /* HAVE_FCC */

    /*
     * If a waiter is present and there is room in its buffers, then copy the 
     * data to the waiter's buffers, otherwise, enqueue the data to the 
     * sink's pending data Q. 
     */
    if (waiter && 
        (waiter->i_cur != waiter->buf_cnt) &&
        ((waiter->iobuf[waiter->i_cur].buf_len - 
          waiter->iobuf[waiter->i_cur].buf_wrlen) >=
         vqec_pak_get_content_len(pak))) {
        readlen = vqec_sink_read_internal(sink, pak, 
                                          &waiter->iobuf[waiter->i_cur]);
        waiter->len += readlen;

        /*
         * Terminate processing of packets in this iobuf if:
         *   1. its remaining length is likely too small for another pkt, or
         *   2. we just wrote an APP packet
         */
#ifdef HAVE_FCC
        if (pak->type == VQEC_PAK_TYPE_APP) {
            app_pkt_written = TRUE;
        }
#endif /* HAVE_FCC */
        if (((waiter->iobuf[waiter->i_cur].buf_len -
              waiter->iobuf[waiter->i_cur].buf_wrlen)
             < sink->max_paksize)
#ifdef HAVE_FCC
            || app_pkt_written
#endif /* HAVE_FCC */
            ) {
            waiter->i_cur++;    /* done with current buffer - it is full now */

            /*
             * Wakeup the reader thread if:
             *   1. all of the reader's buffers are filled, or
             *   2. we just wrote an APP packet
             */
            if (waiter->i_cur == waiter->buf_cnt
#ifdef HAVE_FCC
                || app_pkt_written
#endif
                ) {
                wake_up_interruptible(waiter->waitq);
            }
        }
    } else {
        if (likely(
                pak_hdr ||
                ((pak_hdr = vqec_pak_hdr_alloc(sink->hdr_pool))))) {
                
            pak_hdr->pak = pak;            
            vqec_pak_ref(pak);
            VQE_TAILQ_INSERT_TAIL(&sink->pak_queue, 
                                  pak_hdr, list_obj);
            sink->rcv_len_ready_for_read += 
                vqec_pak_get_content_len(pak);
            sink->pak_queue_depth++;
        } else {
            sink->queue_drops++;
        }
    }

    sink->inputs++;
    return (VQEC_DP_ERR_OK);
}

/**
   Enqueue a packet to sink
   If a "waiter buffer", i.e, a user-provided buffer is present the packet
   is copied directly into that buffer, otherwise, the packet is enQ'ed to
   the sink's packet Q. If the packet Q's max depth is exceeded, the 
   earlies packet in the Q is dropped.
   @param[in] sink Pointer of sink
   @param[in] pak Pointer of packet to insert.
   @return VQEC_DP_ERR_OK on success, error code on failure.
*/
int32_t vqec_sink_enqueue (struct vqec_sink_ *sink, 
                           vqec_pak_t *pak) 
{
    int err;
    struct socket *sock;
    struct sk_buff *skb, *skb1;

    sock = sink->output_sock;

    /* insert the pak into the skb_queue and return */
    if (sock && pak && pak->skb) {
        skb = (struct sk_buff *)pak->skb;

        sink->inputs++;

        /*
         * Disabling UDP checksumming increases performance since we don't want
         * to be dropping any packets in the output socket anyway.  This option
         * must be set here as there is no way to disable *incoming* UDP packet
         * checksumming from user-space.
         */
        skb->ip_summed = CHECKSUM_UNNECESSARY;

        /*
         * The clone of this skb is necessary since the original skb is tied to
         * the socket that it was received by (on the input of VQEC).  After it
         * is cloned, the clone will be tied to this output socket instead, so
         * as to avoid socket packet memory accounting interference.
         */
        skb1 = skb_clone(skb, GFP_ATOMIC);
        if (!skb1) {
            sink->queue_drops++;
            return (VQEC_DP_ERR_OK);
        }

        if (sock->sk) {
            err = sock_queue_rcv_skb(sock->sk, skb1);
            if (err < 0) {
                kfree_skb(skb1);
                sink->queue_drops++;
            }
        } else {
            kfree_skb(skb1);
            sink->queue_drops++;
        }

        return (VQEC_DP_ERR_OK);
    }
    
    return (vqec_sink_kernel_enq(sink, pak));
}


/**---------------------------------------------------------------------------
 * Flush a sink queue, and wakeup any thread that may be in-wait.
 * 
 * @param[in] sink Pointer to the sink object.
 *---------------------------------------------------------------------------*/ 
static void 
vqec_sink_flush (vqec_sink_t * sink)
{
    vqec_pak_hdr_t *pak_hdr, *pak_hdr_n;

    sink->rcv_len_ready_for_read = 0;

    VQE_TAILQ_FOREACH_SAFE(pak_hdr, 
                           &sink->pak_queue, list_obj, pak_hdr_n) {
        VQE_TAILQ_REMOVE(&sink->pak_queue, pak_hdr, list_obj);
        vqec_pak_free(pak_hdr->pak);
        vqec_pak_hdr_free(pak_hdr);
        sink->queue_drops++;
        sink->pak_queue_depth--;
    }

    /* wakeup any sleeping tasks. */
    if (sink->waiter) {
        wake_up_interruptible(sink->waiter->waitq);
    }
    
    if (sink->pak_queue_depth != 0) {
        VQEC_DP_SYSLOG_PRINT(OUTPUTSHIM_ERROR,
                             "sink:: sink is not empty after flush!");
    }
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    sink->exit_ts = ABS_TIME_0;
#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
}

/**
   Set sink module function table
   @param[in] table Function table for sink module.
 */
void vqec_sink_set_fcns (vqec_sink_fcns_t *table) 
{
    vqec_sink_set_fcns_common(table);
    table->vqec_sink_enqueue = vqec_sink_enqueue;
    table->vqec_sink_flush = vqec_sink_flush;
    table->vqec_sink_set_sock = vqec_sink_set_sock;
    table->vqec_sink_add_waiter = vqec_sink_add_waiter;
    table->vqec_sink_del_waiter = vqec_sink_del_waiter;
}
