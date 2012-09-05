/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_sink.c
 *
 * Description: VQEC "sink" abstraction implementation.
 *
 * Documents:
 *
 *****************************************************************************/
#include "vqec_sink.h"
#include "vqec_pak.h"
#include "vam_time.h"
#include "zone_mgr.h"

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
    if (sink->pak_queue_depth == sink->queue_size) {
        pak_hdr = VQE_TAILQ_FIRST(&sink->pak_queue);
        VQEC_DP_ASSERT_FATAL(pak_hdr, "Pak header is null");
        VQE_TAILQ_REMOVE(&sink->pak_queue, pak_hdr, list_obj);
        sink->rcv_len_ready_for_read -= 
            vqec_pak_get_content_len(pak_hdr->pak);
        vqec_pak_free(pak_hdr->pak);
        sink->pak_queue_depth--;
        sink->queue_drops++;
    }

    /*
     * If there is a waiter for the sink, see if we can copy the data
     * to this waiter's buffers.
     */
    waiter = sink->waiter;

#ifdef HAVE_FCC
    /*
     * APP packets must be in their own buffer.
     * If we are processing an APP packet and a waiter is present,
     * advance to the next buffer if the current one is not empty
     * and a buffer is available.
     */
    if ((pak->type == VQEC_PAK_TYPE_APP) && waiter && 
        (waiter->i_cur != waiter->buf_cnt) &&
        waiter->iobuf[waiter->i_cur].buf_wrlen) {
        waiter->i_cur++;
    }
#endif /* HAVE_FCC */

    /*
     * If a waiter is present and there is room in its buffers,
     * then copy the data to the waiter's buffers, otherwise, 
     * enqueue the data to the sink's pending data Q. 
     */
    if (waiter && (waiter->i_cur != waiter->buf_cnt) &&
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
        if (waiter->iobuf[waiter->i_cur].buf_flags & VQEC_DP_BUF_FLAGS_APP) {
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
            /* sa_ignore {Ignore "uninit'ed" cond variable} IGNORE_RETURN(1) */
            waiter->i_cur++;    /* done with current buffer - it is full now */
            /*
             * Notify the reader thread if:
             *   1. all of the reader's buffers are filled, or
             *   2. we just wrote an APP packet
             */
            if (waiter->i_cur == waiter->buf_cnt
#ifdef HAVE_FCC
                || app_pkt_written
#endif
                ) {
                /* sa_ignore {Ignore "uninit'ed" cond var} IGNORE_RETURN(1) */
               pthread_cond_signal(&waiter->cond);
            }
        }
    } else {
        if (pak_hdr ||
            ((pak_hdr = vqec_pak_hdr_alloc(sink->hdr_pool)))) {
            pak_hdr->pak = pak;            
            vqec_pak_ref(pak);
            VQE_TAILQ_INSERT_TAIL(&sink->pak_queue, pak_hdr, list_obj);
            sink->rcv_len_ready_for_read += vqec_pak_get_content_len(pak);
            sink->pak_queue_depth++;
        } else {
            sink->queue_drops++;
        }
    }

    sink->inputs++;
    return (VQEC_DP_ERR_OK);
}

static void 
vqec_sink_add_waiter (vqec_sink_t * sink, 
                      vqec_sink_waiter_t *waiter)
{
    if (sink->waiter) {
        VQEC_DP_SYSLOG_PRINT(OUTPUTSHIM_ERROR,
                             "sink:: a waiter already on sink!");  
        VQEC_DP_ASSERT_FATAL(sink->waiter, "Waiter is null");
    }
    sink->waiter = waiter;
}

static void 
vqec_sink_del_waiter (vqec_sink_t * sink, 
                      vqec_sink_waiter_t *waiter)
{
    if (!sink->waiter) {
        VQEC_DP_SYSLOG_PRINT(OUTPUTSHIM_ERROR,
                             "sink:: waiter is already deleted!");
        return;
    }
    sink->waiter = NULL;
}

static void 
vqec_sink_flush (vqec_sink_t * sink)
{
    vqec_pak_hdr_t *pak_hdr, *pak_hdr_n;

    sink->rcv_len_ready_for_read = 0;
    VQE_TAILQ_FOREACH_SAFE(pak_hdr, &sink->pak_queue, list_obj, pak_hdr_n) {
        VQE_TAILQ_REMOVE(&sink->pak_queue, pak_hdr, list_obj);
        vqec_pak_free(pak_hdr->pak);
        vqec_pak_hdr_free(pak_hdr);
        sink->queue_drops++;
        sink->pak_queue_depth--;
    }

    if (sink->waiter) {
        /* sa_ignore {Ignore "uninit'ed" cond variable} IGNORE_RETURN(1) */
        pthread_cond_signal(&sink->waiter->cond);
    }

    if (sink->pak_queue_depth != 0) {
        VQEC_DP_SYSLOG_PRINT(OUTPUTSHIM_ERROR,
                             "sink:: sink is not empty after flush!");
    }
}


/**
 * STUB.  Set the output socket to use.
 */
int32_t vqec_sink_set_sock (struct vqec_sink_ *sink,
                            int fd)
{
    return (0);
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
    table->vqec_sink_add_waiter = vqec_sink_add_waiter;
    table->vqec_sink_del_waiter = vqec_sink_del_waiter;
    table->vqec_sink_set_sock = vqec_sink_set_sock;
}
