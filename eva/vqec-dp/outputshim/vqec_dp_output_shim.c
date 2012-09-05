/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_output_shim.c
 *
 * Description: Output shim implementation.
 *
 * Documents:
 *
 *****************************************************************************/

#ifdef _VQEC_DP_UTEST
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

#include <utils/queue_plus.h>
#include <utils/strl.h>
#include <vqec_dp_output_shim_api.h>
#include <vqec_dp_output_shim_private.h>
#include <vqec_dp_oshim_read_api.h>
#include <vqec_dp_io_stream.h>
#include <vqec_sink.h>
#include <utils/zone_mgr.h>
#ifdef __KERNEL__
#include <linux/skbuff.h>
#endif  /* __KERNEL__ */

#define VQEC_DP_OUTPUT_SHIM_STREAMS_PER_TUNER 1

/* global output_shim instance */
vqec_output_shim_t g_output_shim = {TRUE, 0, 0, 0};

/* allocation zones */
static struct vqe_zone *s_vqec_output_shim_tuner_pool = NULL;
static struct vqe_zone *s_vqec_output_shim_is_pool = NULL;
static struct vqe_zone *s_output_shim_tuners_array_pool = NULL;
static struct vqe_zone *s_output_shim_is_array_pool = NULL;

/**
 * Implementations of the functions from the input stream interface below.
 */

/**
 * Get the capability flags for the input stream. 
 */
UT_STATIC int32_t vqec_dp_output_shim_is_get_capa (vqec_dp_isid_t is)
{
    int is_idx = is - 1;

    if (is > g_output_shim.max_streams || is < 1) {
        return VQEC_DP_STREAM_CAPA_NULL;
    }

    if (g_output_shim.is[is_idx]) {
        return g_output_shim.is[is_idx]->capa;
    } else {
        return VQEC_DP_STREAM_CAPA_NULL;
    }
}

/**
 * Get the encapsulation type for the input stream. 
 */
UT_STATIC
vqec_dp_encap_type_t vqec_dp_output_shim_is_get_encap (vqec_dp_isid_t is)
{
    int is_idx = is - 1;

    if (is > g_output_shim.max_streams || is < 1) {
        return VQEC_DP_ENCAP_UNKNOWN;
    }

    if (g_output_shim.is[is_idx]) {
        return g_output_shim.is[is_idx]->encap;
    } else {
        return VQEC_DP_ENCAP_UNKNOWN;
    }
}

/**
 * Get stream status for an input stream. This includes the id of
 * any connected output stream, and statistics.
 */
vqec_dp_stream_err_t
vqec_dp_output_shim_is_get_status (vqec_dp_isid_t is,
                                   vqec_dp_is_status_t *stat, 
                                   boolean cumulative)
{
    int is_idx = is - 1;

    if ((is > g_output_shim.max_streams) || (is < 1) || !stat) {
        return VQEC_DP_STREAM_ERR_INVALIDARGS;
    }

    if (g_output_shim.is[is_idx]) {
        memcpy(stat, &g_output_shim.is[is_idx]->status,
               sizeof(vqec_dp_is_status_t));
        return VQEC_DP_STREAM_ERR_OK;
    } else {
        return VQEC_DP_STREAM_ERR_NOSUCHSTREAM;
    }
}

/**
 * Print into a buffer the list of tuner IDs connected to this sream.
 */
vqec_dp_error_t
vqec_dp_output_shim_mappedtidstr (vqec_dp_isid_t id, char *buf, 
                                  uint32_t buf_len, uint32_t *used_len)
{
    int is_idx = id - 1;
    vqec_dp_output_shim_tuner_t *tid_entry;

    char tmp[VQEC_DP_OUTPUT_SHIM_PRINT_BUF_SIZE];
    int len = VQEC_DP_OUTPUT_SHIM_PRINT_BUF_SIZE;
    memset(buf, 0, buf_len);
    memset(tmp, 0, len);

    if (!buf || !used_len) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    if (id > g_output_shim.max_streams || id < 1 || !g_output_shim.is[is_idx]) {
        snprintf(buf, buf_len, "(invalid stream id)");
        return VQEC_DP_ERR_NOSUCHSTREAM;
    }

    *used_len = 0;
    VQE_TAILQ_FOREACH(tid_entry,
                  &g_output_shim.is[is_idx]->mapped_tunerq,
                  list_obj) {
        snprintf(tmp, len, "%d ", tid_entry->tid);
        *used_len = strlcat(buf, tmp, buf_len);
    }
    if (*used_len != buf_len) {
        (*used_len)++;
    }

    return VQEC_DP_ERR_OK;
}

/**
 * Initiate a connection from the input stream to the given
 * output stream. The input stream shall first get the
 * capabilities of the output stream, compare them to it's
 * own capabilities, and request a preferred subset of their
 * intersection for the connection by invoking the 
 * accept_connect method on the output stream. If there
 * are no capabilities in the intersection, or the set is 
 * insufficient to build a service, the method must fail. 
 * It must not have any visible side-effects when it fails.
 *
 * Otherwise, the method will logically connect the input
 * and output streams together, and cache the output stream
 * handle, and operations interface. The output stream will
 * then deliver datagrams to the input stream through it's
 * own packet receive interface.
 */
vqec_dp_stream_err_t
vqec_dp_output_shim_is_initiate_connect (vqec_dp_isid_t is,
                                         vqec_dp_osid_t os,
                                         const struct vqec_dp_osops_ *ops)
{
    int32_t os_capa, is_capa;
    int is_idx = is - 1;
    const vqec_dp_isops_t *is_ops;
    vqec_dp_encap_type_t encap;
    vqec_dp_stream_err_t ret;

    if (g_output_shim.shutdown) {
        return VQEC_DP_STREAM_ERR_SERVICESHUT;
    } else if ((is > g_output_shim.max_streams) || (is < 1) || !ops) {
        return VQEC_DP_STREAM_ERR_INVALIDARGS;
    } else if (!g_output_shim.is[is_idx]) {
        return VQEC_DP_STREAM_ERR_NOSUCHSTREAM;
    }

    os_capa = ops->capa(os);
    is_capa = g_output_shim.is[is_idx]->capa;
    is_ops = g_output_shim.is[is_idx]->ops;
    encap = g_output_shim.is[is_idx]->encap;

    ret = ops->accept_connect(os, is, is_ops, encap, (os_capa & is_capa));
    if (ret == VQEC_DP_STREAM_ERR_OK) {
        /* connection succeeded; cache OS handle and ops structure */
        g_output_shim.is[is_idx]->status.os = os;
        g_output_shim.is[is_idx]->os.ops = ops;
        g_output_shim.is[is_idx]->os.id = os;
    }

    return ret;
}

/* 
 * Put the packet into the queue (sink) of each mapped tuner.  The packet's
 * ref count will be incremented when being enqueued into each sink.
 */
UT_STATIC inline void 
vqec_dp_output_shim_is_receive_internal(int is_idx,
                                        vqec_pak_t *pak)
{
    vqec_dp_output_shim_tuner_t *tid_entry;
    vqec_dp_chan_desc_t *chan;

    VQE_TAILQ_FOREACH(tid_entry,
                  &g_output_shim.is[is_idx]->mapped_tunerq,
                  list_obj) {
        if (tid_entry->sink) {
            chan = &tid_entry->chan;
#ifdef __KERNEL__
            {
                struct sk_buff *skb;
                struct iphdr *ip;
                struct udphdr *udp;

                skb = pak->skb;
                /* 
                 * Copy over the network header information from the channel
                 * description structure into the skb headers.  The primary
                 * multicast packets should already have the correct
                 * information in their headers.
                 */
                if (pak->type != VQEC_PAK_TYPE_PRIMARY && skb) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
                    ip = skb->nh.iph;
                    udp = skb->h.uh;
#else
                    ip = (struct iphdr *)skb->network_header;
                    udp = (struct udphdr *)skb->transport_header;
#endif  /* LINUX_VERSION_CODE */

                    /* set source and dest addresses and ports for the skb */
                    ip->saddr = chan->primary.filter.u.ipv4.src_ip;
                    ip->daddr = chan->primary.filter.u.ipv4.dst_ip;
                    udp->source = chan->primary.filter.u.ipv4.src_port;
                    udp->dest = chan->primary.filter.u.ipv4.dst_port;
                }
            }
#endif  /* __KERNEL__ */
            /* copy over dest address and port */
            pak->dst_addr.s_addr = chan->primary.filter.u.ipv4.dst_ip;
            pak->dst_port = chan->primary.filter.u.ipv4.dst_port;

            MCALL(tid_entry->sink, vqec_sink_enqueue, pak);
        } else {
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_OUTPUTSHIM,
                          "tried to put pak into invalid "
                          "tuner (tid = %d)\n", tid_entry->tid);
        }
    }
}

/**
 * Receive a packet from an output stream and put it into the associated packet
 * queue (sink).  If the input handle is a valid input stream, and pak is a
 * valid pointer, the method returns ERR_OK.
 */
vqec_dp_stream_err_t vqec_dp_output_shim_is_receive (vqec_dp_isid_t is,
                                                     vqec_pak_t *pak)
{
    int is_idx = is - 1;

    if (g_output_shim.shutdown) {
        return VQEC_DP_STREAM_ERR_SERVICESHUT;
    } else if ((is > g_output_shim.max_streams) || (is < 1) || (!pak)) {
        return VQEC_DP_STREAM_ERR_INVALIDARGS;
    } else if (!g_output_shim.is[is_idx]) {
        return VQEC_DP_STREAM_ERR_NOSUCHSTREAM;
    }

    vqec_dp_output_shim_is_receive_internal(is_idx, pak);

    return VQEC_DP_STREAM_ERR_OK;
}

/**
 * Receive a vector of packets from an output stream and put them into the
 * associated packet queue (sink).  If the input handle is a valid input
 * stream, and pakvec is a valid pointer array, the method returns ERR_OK.
 */
vqec_dp_stream_err_t vqec_dp_output_shim_is_receive_vec (vqec_dp_isid_t is, 
                                                         vqec_pak_t **pakvec,
                                                         uint32_t vecnum)
{
    int pak_idx, is_idx = is - 1;
    vqec_pak_t *pak;
    vqec_dp_stream_err_t err;

    if (g_output_shim.shutdown) {
        err = VQEC_DP_STREAM_ERR_SERVICESHUT;
        goto done;
    } else if ((is > g_output_shim.max_streams) || (is < 1) || (!pakvec)) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        goto done;
    } else if (!g_output_shim.is[is_idx]) {
        err = VQEC_DP_STREAM_ERR_NOSUCHSTREAM;
        goto done;
    }

    /*
     * this can be implemented natively in the future if the extra performance
     * increase is desired, instead of just being a wrapper
     */

    /* loop through all the packets */
    for (pak_idx = 0; pak_idx < vecnum; pak_idx++) {
        pak = pakvec[pak_idx];

        vqec_dp_output_shim_is_receive_internal(is_idx, pak);
    }

    err = VQEC_DP_STREAM_ERR_OK;

done:
    return err;
}

/* global output_shim input stream operations */
static const vqec_dp_isops_t vqec_dp_output_shim_isops =
    {vqec_dp_output_shim_is_get_capa,
     vqec_dp_output_shim_is_initiate_connect,
     vqec_dp_output_shim_is_receive,
     vqec_dp_output_shim_is_receive_vec,
     vqec_dp_output_shim_is_get_status,
     vqec_dp_output_shim_is_get_encap};


/**
 * Implementations of the internal output shim functions below.
 */

/**
 * Get status information from the output shim.
 */
vqec_dp_error_t
vqec_dp_output_shim_get_status (vqec_dp_output_shim_status_t *status)
{
    if (!status) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    status->shutdown = g_output_shim.shutdown;
    status->is_creates = g_output_shim.is_creates;
    status->is_destroys = g_output_shim.is_destroys;
    status->num_tuners = g_output_shim.num_tuners;

    return VQEC_DP_ERR_OK;
}

/**
 * Looks up a CP tuner ID given a valid DP tuner name.
 * NOTE:  The given name need not be NULL-terminated--only the first
 *        VQEC_DP_TUNER_MAX_NAME_LEN characters are used in the lookup.
 *
 * @param[in] name name of CP tuner to find, MUST be valid.
 * @param[out] returns CP tuner ID of CP tuner associated with the found DP
 *             tuner, or the invalid ID.
 */
vqec_tunerid_t
vqec_dp_outputshim_get_tunerid_by_name (const char *name)
{
    int i;

    if (g_output_shim.tuners) {
        for (i = 0; i < g_output_shim.max_tuners; i++) {
            if (g_output_shim.tuners[i] &&
                (strncmp(name, g_output_shim.tuners[i]->name, 
                         VQEC_DP_TUNER_MAX_NAME_LEN) == 0)) {
                return (g_output_shim.tuners[i]->cp_tid);
            }
        }
    }

    return (VQEC_TUNERID_INVALID);
}

/**
 * Looks up a DP tuner's name by it's ID.
 *
 * @param[in] tunerid  ID of the CP tuner associated with the DP tuner.
 * @param[out] name    Name string returned of the tuner.
 * @param[in] len      Length of the provided string buffer.
 * @param[out]         Returns VQEC_ERR_OK on success.
 */
vqec_error_t
vqec_dp_outputshim_get_name_by_tunerid (vqec_tunerid_t id,
                                        char *name,
                                        int32_t len)
{
    int dp_tid;
    vqec_dp_output_shim_tuner_t *t;
    
    if (id == VQEC_TUNERID_INVALID) {
        return (VQEC_ERR_INVALIDARGS);
    }

    dp_tid = vqec_dp_output_shim_cp_to_dp_tunerid(id);

    t = vqec_dp_output_shim_get_tuner_by_id(dp_tid);

    if (t) {
        strncpy(name, t->name, len);
    } else {
        return (VQEC_ERR_NOSUCHTUNER);
    }

    return (VQEC_OK);
}

/**
 * Create a new dataplane tuner abstraction, and return an 
 * identifier to it. The creation operation is invoked by the
 * control-plane.
 *
 * A dataplane tuner simply provides a mechanism to identify an input
 * stream on the output shim that is receiving repaired data from 
 * a particular dataplane source.
 */
vqec_dp_error_t
vqec_dp_output_shim_create_dptuner (int32_t cp_tid,
                                    vqec_dp_tunerid_t *tid,
                                    char *name,
                                    uint32_t namelen)
{
    int i;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if ((!tid) || (namelen > VQEC_DP_TUNER_MAX_NAME_LEN)) {
        return VQEC_DP_ERR_INVALIDARGS;
    } else if (g_output_shim.shutdown || (!g_output_shim.tuners)) {
        return VQEC_DP_ERR_SHUTDOWN;
    }

    *tid = VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID;
    for (i = 0; i < g_output_shim.max_tuners; i++) {
        if (!g_output_shim.tuners[i]) {

            /* free slot found; create new tuner */
            g_output_shim.tuners[i] = (vqec_dp_output_shim_tuner_t *)
                zone_acquire(s_vqec_output_shim_tuner_pool);
            if (!g_output_shim.tuners[i]) {
                goto done;
            }
            memset(g_output_shim.tuners[i], 0,
                   sizeof(vqec_dp_output_shim_tuner_t));

            /* cache control-plane and data-plane tuner IDs, and name */
            g_output_shim.tuners[i]->cp_tid = cp_tid;
            g_output_shim.tuners[i]->tid = i + 1;
            if (vqec_dp_outputshim_get_tunerid_by_name(name)
                != VQEC_TUNERID_INVALID) {
                err = VQEC_DP_ERR_EXISTS;
                goto done;
            }
            strncpy(g_output_shim.tuners[i]->name,
                    name,
                    namelen);

            /* create the associated sink */
            g_output_shim.tuners[i]->sink =
                vqec_sink_create(g_output_shim.output_q_limit,
                                 g_output_shim.max_paksize);
            if (!g_output_shim.tuners[i]->sink) {
                /* failed to create sink; break out and fail */
                goto done;
            }

            *tid = i + 1;
            goto done;

        }
    }

done:
    if (*tid == VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID) {
        if ((i < g_output_shim.max_tuners) && g_output_shim.tuners[i]) {
            zone_release(s_vqec_output_shim_tuner_pool, 
                         g_output_shim.tuners[i]);
            g_output_shim.tuners[i] = NULL;
        }
        if (err == VQEC_DP_ERR_EXISTS) {
            return (err);
        }
        return VQEC_DP_ERR_NOMEM;
    } else {
        g_output_shim.num_tuners++;
        return VQEC_DP_ERR_OK;
    }
}

/**
 * Destroy an existing tuner. For correct operation, any sockets that
 * are mapped to the tuner (for the split user-kernel implementation)
 * must be closed, and any input streams that are mapped to the tuner
 * must be unmapped, prior to the destroy operation. If these 
 * conditions are not met, destroy shall fail. 
 *
 * The alternative, i.e., shutdown the socket, and simply ignore the
 * input stream's packet data, is not implemented, but is open for
 * later adaptation.
 */
vqec_dp_error_t
vqec_dp_output_shim_destroy_dptuner (vqec_dp_tunerid_t tid)
{
    int i = tid - 1;

    if (tid > g_output_shim.max_tuners || tid < 1) {
        return VQEC_DP_ERR_INVALIDARGS;
    }  else if (!g_output_shim.tuners || !g_output_shim.tuners[i]) {
        return VQEC_DP_ERR_NOSUCHTUNER;
    } else if (g_output_shim.tuners[i]->is) {
        return VQEC_DP_ERR_ISMAPPEDTOTUNER;
    }

    /* destroy the associated sink */
    if (g_output_shim.tuners[i]->sink) {
        vqec_sink_destroy(g_output_shim.tuners[i]->sink);
    }

    /* free the tuner */
    zone_release(s_vqec_output_shim_tuner_pool, g_output_shim.tuners[i]);
    g_output_shim.tuners[i] = NULL;

    g_output_shim.num_tuners--;
    return VQEC_DP_ERR_OK;
}

/**
 * Associate a user-space socket with a dataplane tuner so that it may be used
 * for output.
 *
 * @param[in] tid A valid dataplane tuner handle.
 * @param[in] sockfd Socket file descriptor.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
vqec_dp_error_t
vqec_dp_output_shim_add_tuner_output_socket (vqec_dp_tunerid_t tid,
                                             int sockfd)
{
    vqec_dp_output_shim_tuner_t *t;

    if (tid > g_output_shim.max_tuners || tid < 1) {
        return VQEC_DP_ERR_INVALIDARGS;
    } else if (!vqec_dp_output_shim_get_tuner_by_id(tid)) {
        return VQEC_DP_ERR_NOSUCHTUNER;
    }

    /* add the socket to the associated sink */
    t = vqec_dp_output_shim_get_tuner_by_id(tid);
    if (t && t->sink) {
        MCALL(t->sink, vqec_sink_set_sock, sockfd);
    }

    return VQEC_DP_ERR_OK;
}

/**
 * Get status information for a dataplane tuner.
 */
vqec_dp_error_t
vqec_dp_output_shim_get_tuner_status (vqec_dp_tunerid_t tid,
                                      vqec_dp_output_shim_tuner_status_t *s,
                                      boolean cumulative)
{
    vqec_dp_output_shim_tuner_t *t;
    vqec_dp_sink_stats_t sink_stats;

    if (((tid > g_output_shim.max_tuners) || (tid < 1)) || !s) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    memset(&sink_stats, 0, sizeof(vqec_dp_sink_stats_t));

    t = vqec_dp_output_shim_get_tuner_by_id(tid);
    if (!t) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    MCALL(t->sink, vqec_sink_get_stats, &sink_stats, cumulative);

    s->cp_tid = t->cp_tid;
    s->tid = tid;
    s->qid = t->qid;  /* not currently used */
    s->is = t->is;
    s->qinputs = sink_stats.inputs;
    s->qdrops = sink_stats.queue_drops;
    s->qdepth = sink_stats.queue_depth;
    s->qoutputs = sink_stats.outputs;

    return VQEC_DP_ERR_OK;
}

/**
 * Clear the counters for a dataplane tuner.
 */
vqec_dp_error_t
vqec_dp_output_shim_clear_tuner_counters (vqec_dp_tunerid_t tid)
{
    vqec_dp_output_shim_tuner_t *t;

    if ((tid > g_output_shim.max_tuners) || (tid < 1)) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    t = vqec_dp_output_shim_get_tuner_by_id(tid);
    if (!t) {
        return VQEC_DP_ERR_INVALIDARGS;
    }
    MCALL(t->sink, vqec_sink_clear_stats);

    return VQEC_DP_ERR_OK;
}

/**
 * Wrapper for the vqec_dp_oshim_tuner_read() function.
 */
vqec_dp_error_t
vqec_dp_output_shim_tuner_read (vqec_dp_tunerid_t id,
                                vqec_iobuf_t *iobuf,
                                uint32_t iobuf_num,
                                uint32_t *len,
                                int32_t timeout_msec)
{
    return vqec_dp_oshim_read_tuner_read(id,
                                         iobuf,
                                         iobuf_num,
                                         len,
                                         timeout_msec);
}

/**
 * Start the output shim services. User's of the shim must call this 
 * method prior to using it's services for the 1st time, or restarting it
 * after a shutdown. The behavior may be assumed unpredictable
 * if these guidelines are not followed. The shim may allocate, and 
 * initialize internal resources upon this method's invocation.
 */
vqec_dp_error_t
vqec_dp_output_shim_startup (vqec_dp_module_init_params_t *params)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    
    err = init_vqec_sink_module(params);
    if (err != VQEC_DP_ERR_OK) {
        goto bail;
    }

    if (!vqec_dp_oshim_read_init(params->max_tuners)) {
        err = VQEC_DP_ERR_INTERNAL;
        goto bail;
    }

    /* allocate resource zones */
    s_vqec_output_shim_tuner_pool = zone_instance_get_loc(
                                        "oshim_tuner",
                                        O_CREAT,
                                        sizeof(vqec_dp_output_shim_tuner_t),
                                        params->max_tuners,
                                        NULL, NULL);
    if (!s_vqec_output_shim_tuner_pool) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }

    s_vqec_output_shim_is_pool = zone_instance_get_loc(
                    "oshim_is_pool",
                    O_CREAT,
                    sizeof(vqec_dp_output_shim_is_t),
                    params->max_tuners * VQEC_DP_OUTPUT_SHIM_STREAMS_PER_TUNER,
                    NULL, NULL);
    if (!s_vqec_output_shim_is_pool) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }

    memset(&g_output_shim, 0, sizeof(vqec_output_shim_t));

    s_output_shim_tuners_array_pool = zone_instance_get_loc(
                                "oshim_t_array",
                                O_CREAT,
                                params->max_tuners * 
                                    sizeof(vqec_dp_output_shim_tuner_t *),
                                1, NULL, NULL);
    if (!s_output_shim_tuners_array_pool) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }
    g_output_shim.tuners = (vqec_dp_output_shim_tuner_t **)
                            zone_acquire(s_output_shim_tuners_array_pool);
    if (!g_output_shim.tuners) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }
    memset(g_output_shim.tuners, 0, 
           params->max_tuners * sizeof(vqec_dp_output_shim_tuner_t *));
    g_output_shim.max_tuners = params->max_tuners;
    g_output_shim.max_paksize = params->max_paksize;
    g_output_shim.max_iobuf_cnt = params->max_iobuf_cnt;
    g_output_shim.iobuf_recv_timeout = params->iobuf_recv_timeout;
    g_output_shim.output_q_limit = params->output_q_limit;

    s_output_shim_is_array_pool = zone_instance_get_loc(
                            "oshim_is_array",
                            O_CREAT,
                            params->max_tuners *
                                VQEC_DP_OUTPUT_SHIM_STREAMS_PER_TUNER *
                                sizeof(vqec_dp_output_shim_is_t *),
                            1, NULL, NULL);
    if (!s_output_shim_is_array_pool) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }
    g_output_shim.is = (vqec_dp_output_shim_is_t **)
                        zone_acquire(s_output_shim_is_array_pool);
    if (!g_output_shim.is) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }
    memset(g_output_shim.is, 0, 
           params->max_tuners * VQEC_DP_OUTPUT_SHIM_STREAMS_PER_TUNER * 
           sizeof(vqec_dp_output_shim_tuner_t *));
    g_output_shim.max_streams = params->max_tuners * 
        VQEC_DP_OUTPUT_SHIM_STREAMS_PER_TUNER;

    g_output_shim.shutdown = FALSE;
    return VQEC_DP_ERR_OK;

bail:
    if (s_vqec_output_shim_tuner_pool) {
        (void)zone_instance_put(s_vqec_output_shim_tuner_pool);
        s_vqec_output_shim_tuner_pool = NULL;
    }
    if (s_vqec_output_shim_is_pool) {
        (void)zone_instance_put(s_vqec_output_shim_is_pool);
        s_vqec_output_shim_is_pool = NULL;
    }
    if (g_output_shim.tuners) {
        zone_release(s_output_shim_tuners_array_pool, g_output_shim.tuners);
        g_output_shim.tuners = NULL;
    }
    if (s_output_shim_tuners_array_pool) {
        (void) zone_instance_put(s_output_shim_tuners_array_pool);
        s_output_shim_tuners_array_pool = NULL;
    }
    if (g_output_shim.is) {
        zone_release(s_output_shim_is_array_pool, g_output_shim.is);
        g_output_shim.is = NULL;
    }
    if (s_output_shim_is_array_pool) {
        (void) zone_instance_put(s_output_shim_is_array_pool);
        s_output_shim_is_array_pool = NULL;
    }
    deinit_vqec_sink_module();
    return (err);
}

/**
 * Shutdown output shim. If the shim is already shutdown
 * no action should be taken. The following behavior is expected:
 *
 * (a) Allocation of new resources should no longer be
 * allowed.
 *
 * (b) If there are existing tuners or input streams, the creator of
 * these streams, which has an implicit reference on them, must 
 * destroy them explicitly, and they must not be destroyed 
 * at this time. The expected behavior is that all packets delivered
 * to the shim shall be dropped, and the shim will flush
 * it's internal queues. It must also disallow new connections, 
 * and creation of tuners in the post-shutdown state.
 *
 * (c) All other resources must be released.
 */
void vqec_dp_output_shim_shutdown (void)
{
    int i;

    if (g_output_shim.shutdown == TRUE) {
        return;
    }

    /* first, unmap and destroy all input streams */
    for (i = 0; i < g_output_shim.max_streams; i++) {
        if (g_output_shim.is[i]) {
            vqec_dp_output_shim_destroy_is(i + 1);
        }
    }
    if (g_output_shim.is) {
        zone_release(s_output_shim_is_array_pool, g_output_shim.is);
        g_output_shim.is = NULL;
    }
    if (s_output_shim_is_array_pool) {
        (void) zone_instance_put(s_output_shim_is_array_pool);
        s_output_shim_is_array_pool = NULL;
    }

    if (g_output_shim.tuners) {

        /* destroy all tuners and flush all internal queues */
        for (i = 0; i < g_output_shim.max_tuners; i++) {
            if (g_output_shim.tuners[i]) {
                if (g_output_shim.tuners[i]->sink) {
                    MCALL(g_output_shim.tuners[i]->sink, vqec_sink_flush);
                }
                vqec_dp_output_shim_destroy_dptuner(i + 1);
            }
        }
    }
    if (g_output_shim.tuners) {
        /* free the tuner pointer array */
        zone_release(s_output_shim_tuners_array_pool, g_output_shim.tuners);
        g_output_shim.tuners = NULL;
    }
    if (s_output_shim_tuners_array_pool) {
        (void) zone_instance_put(s_output_shim_tuners_array_pool);
        s_output_shim_tuners_array_pool = NULL;
    }

    /* dealloc resource zones */
    if (s_vqec_output_shim_tuner_pool) {
        (void)zone_instance_put(s_vqec_output_shim_tuner_pool);
        s_vqec_output_shim_tuner_pool = NULL;
    }
    if (s_vqec_output_shim_is_pool) {
        (void)zone_instance_put(s_vqec_output_shim_is_pool);
        s_vqec_output_shim_is_pool = NULL;
    }    

    vqec_dp_oshim_read_deinit();

    deinit_vqec_sink_module();

    g_output_shim.shutdown = TRUE;
    return;
}

/**
 * Create a new input stream, and return an opaque handle to it.
 * If resources cannot be allocated for the stream, an invalid handle must be
 * returned. 
 */
vqec_dp_error_t
vqec_dp_output_shim_create_is (vqec_dp_is_instance_t *is,
                               vqec_dp_encap_type_t encap)
{
    int i;
    vqec_dp_error_t ret = VQEC_DP_ERR_NOMEM;

    if (!is ||
        ((encap != VQEC_DP_ENCAP_RTP) && (encap != VQEC_DP_ENCAP_UDP))) {
        ret = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    } else if (g_output_shim.shutdown) {
        ret = VQEC_DP_ERR_SHUTDOWN;
        goto done;
    }

    for (i = 0; i < g_output_shim.max_streams; i++) {
        if (!g_output_shim.is[i]) {
            g_output_shim.is[i] = (vqec_dp_output_shim_is_t *)
                zone_acquire(s_vqec_output_shim_is_pool);
            if (!g_output_shim.is[i]) {
                ret = VQEC_DP_ERR_NOMEM;
                break;
            }
            memset(g_output_shim.is[i], 0, sizeof(vqec_dp_output_shim_is_t));
            g_output_shim.is[i]->encap = encap;
            VQE_TAILQ_INIT(&g_output_shim.is[i]->mapped_tunerq);
            g_output_shim.is[i]->id = i + 1;
            g_output_shim.is[i]->ops = &vqec_dp_output_shim_isops;
            g_output_shim.is[i]->capa = VQEC_DP_STREAM_CAPA_PUSH
                | VQEC_DP_STREAM_CAPA_PUSH_VECTORED;
            /* all ISs in output shim have only these two capabilities */
            is->id = i + 1;
            is->ops = &vqec_dp_output_shim_isops;
            ret = VQEC_DP_ERR_OK;
            break;
        }
    }

done:
    if (ret == VQEC_DP_ERR_OK) {
        g_output_shim.is_creates++;
    } else if (is) {
        is->id = VQEC_DP_INVALID_ISID;
    }
        
    return ret;
}

/**
 * Destroy the specified input stream. If any tuners are attached to this
 * input stream, that binding shall be removed.
 * All resources allocated for the stream should be released.
 */
vqec_dp_error_t vqec_dp_output_shim_destroy_is (vqec_dp_isid_t id)
{
    int is_idx = id - 1;
    vqec_dp_output_shim_tuner_t *tid_entry, *tid_entry_tmp;

    if (id > g_output_shim.max_streams || id < 1) {
        return VQEC_DP_ERR_INVALIDARGS;
    } else if (!g_output_shim.is[is_idx]) {
        return VQEC_DP_ERR_NOSUCHSTREAM;
    }

    /* if any tuners are attached to the stream, unbind them */
    VQE_TAILQ_FOREACH_SAFE(tid_entry,
                       &g_output_shim.is[is_idx]->mapped_tunerq,
                       list_obj,
                       tid_entry_tmp) {
        vqec_dp_output_shim_unmap_dptuner(tid_entry->tid);
    }

    zone_release(s_vqec_output_shim_is_pool, g_output_shim.is[is_idx]);
    g_output_shim.is[is_idx] = NULL;

    g_output_shim.is_destroys++;
    return VQEC_DP_ERR_OK;
}

/**
 * Part of the iterator to scan all input streams that have been created.
 * This method returns the opaque handle of the "first" input stream. The 
 * ordering between "first" and "next" is implementation-specific. If there 
 * are no input streams, an invalid handle is returned.
 */
vqec_dp_error_t
vqec_dp_output_shim_get_first_is (vqec_dp_isid_t *id)
{
    int i;

    if (!id) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    *id = VQEC_DP_INVALID_ISID;
    for (i = 0; i < g_output_shim.max_streams; i++) {
        if (g_output_shim.is[i]) {	
            *id = i + 1;
            break;
        }
    }

    return VQEC_DP_ERR_OK;
}

/**
 * Part of the iterator to scan all input streams that have been created.
 * This method returns the opaque handle of the "next" input stream  
 * given the "prev" handle. The ordering between "next" and "prev" is
 * implementation specific. If there are no more input streams, an 
 * invalid handle is returned. The following behavior is sought:
 *
 * (a)  Atomic semantics for the iterator may be enforced by the caller, if
 * needed, since a race exists between the addition of new input streams, 
 * and termination of the iteration. 
 * (b) If no input streams are created or destroyed, an implementation 
 * of the iterator must return each input stream's handle exactly once
 * before terminating.
 * (c) The behavior, when the caller does not impose atomic semantics,
 * and the collection of input streams is modified, is left unspecified.
 */
vqec_dp_error_t
vqec_dp_output_shim_get_next_is (vqec_dp_isid_t prev_is, 
                                 vqec_dp_isid_t *next_is)
{
    int i;
    int prev_idx = prev_is - 1;

    if (!next_is) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    *next_is = VQEC_DP_INVALID_ISID;
    if (prev_is <= g_output_shim.max_streams && prev_is >= 1) {
        for (i = prev_idx + 1; i < g_output_shim.max_streams; i++) {
            if (g_output_shim.is[i]) {
                *next_is = i + 1;
                break;
            }
        }
    }
    
    return (VQEC_DP_ERR_OK);
}

/**
 * Get statistics and information on a particular stream for display.
 */
vqec_dp_error_t vqec_dp_output_shim_get_is_info (vqec_dp_isid_t isid,
                                                 vqec_dp_display_is_t *is_info)
{
    vqec_dp_output_shim_is_t *is;

    if ((isid > g_output_shim.max_streams) || (isid < 1) || !is_info) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    is = g_output_shim.is[isid - 1];
    if (!is) {
        return VQEC_DP_ERR_NOSUCHSTREAM;
    }

    is_info->id = is->id;
    is_info->capa = is->capa;
    is_info->encap = is->encap;
    is_info->os_id = is->status.os;
    is_info->packets = is->status.stats.packets;
    is_info->bytes = is->status.stats.bytes;
    is_info->drops = is->status.stats.drops;

    return VQEC_DP_ERR_OK;
}

/**
 * Establish a binding between the given IS and the tuner
 * identifier. The tuner's queue will be added to the list of queues
 * to which the IS enqueues incoming packets.
 */
vqec_dp_error_t
vqec_dp_output_shim_map_is_to_dptuner (vqec_dp_isid_t is, 
                                       vqec_dp_tunerid_t tid)
{
    int is_idx = is - 1;
    vqec_dp_output_shim_tuner_t *tid_entry;

    if ((is > g_output_shim.max_streams) ||
        (is < 1) || (tid > g_output_shim.max_tuners || tid < 1)) {
        return VQEC_DP_ERR_INVALIDARGS;
    } else if (!g_output_shim.is[is_idx]) {
        return VQEC_DP_ERR_NOSUCHSTREAM;
    } else if (!vqec_dp_output_shim_get_tuner_by_id(tid)) {
        return VQEC_DP_ERR_NOSUCHTUNER;
    } else if (g_output_shim.shutdown) {
        return VQEC_DP_ERR_SHUTDOWN;
    }

    /* check to see if this tid is in the list already */
    VQE_TAILQ_FOREACH(tid_entry,
                  &g_output_shim.is[is_idx]->mapped_tunerq,
                  list_obj) {
        if (tid_entry->tid == tid) {
            return VQEC_DP_ERR_OK;
        }
    }

    /* it must not be; add it */
    tid_entry = vqec_dp_output_shim_get_tuner_by_id(tid);
    if (tid_entry) {
        VQE_TAILQ_INSERT_TAIL(&g_output_shim.is[is_idx]->mapped_tunerq,
                              tid_entry,
                              list_obj);
        tid_entry->is = is;
    }

    return VQEC_DP_ERR_OK;
}

/**
 * Destroy the binding between the given tuner identifier and its mapped
 * IS. There can be only one IS associated with a single tuner.
 */
vqec_dp_error_t
vqec_dp_output_shim_unmap_dptuner (vqec_dp_tunerid_t tid)
{
    int is_idx;
    vqec_dp_output_shim_tuner_t *tid_entry, *tid_entry_tmp;

    if (tid > g_output_shim.max_tuners || tid < 1) {
        return VQEC_DP_ERR_INVALIDARGS;
    } else if (!vqec_dp_output_shim_get_tuner_by_id(tid)) {
        return VQEC_DP_ERR_NOSUCHTUNER;
    }

    tid_entry = vqec_dp_output_shim_get_tuner_by_id(tid);
    if (tid_entry) {
        is_idx = tid_entry->is - 1;
    } else {
        is_idx = -1;
    }

    if (is_idx == -1) {
        return VQEC_DP_ERR_OK;
    } else if ((is_idx < 0) || (is_idx > g_output_shim.max_streams - 1)
               || (!g_output_shim.is[is_idx])) {
        return VQEC_DP_ERR_NOSUCHSTREAM;
    }

    /* clear tuner's mapped-is field */
    tid_entry->is = VQEC_DP_INVALID_ISID;

    /* find and remove the tid from the IS's mapped_tid list */
    VQE_TAILQ_FOREACH_SAFE(tid_entry,
                       &g_output_shim.is[is_idx]->mapped_tunerq,
                       list_obj,
                       tid_entry_tmp) {
        if (tid_entry->tid == tid) {
            VQE_TAILQ_REMOVE(&g_output_shim.is[is_idx]->mapped_tunerq,
                         tid_entry,
                         list_obj);
        }
    }

    /* flush the associated sink */
    tid_entry = vqec_dp_output_shim_get_tuner_by_id(tid);
    if (tid_entry && tid_entry->sink) {
        MCALL(tid_entry->sink, vqec_sink_flush);
    }

    return VQEC_DP_ERR_OK;
}

/**
 * Cache channel info inside the tuner.
 */
vqec_dp_error_t
vqec_dp_output_shim_tuner_cache_channel (vqec_dp_tunerid_t tid,
                                         vqec_dp_chan_desc_t *chan)
{
    vqec_dp_output_shim_tuner_t *tuner;

    if ((tid > g_output_shim.max_tuners || tid < 1) || (!chan)) {
        return VQEC_DP_ERR_INVALIDARGS;
    } else if (!vqec_dp_output_shim_get_tuner_by_id(tid)) {
        return VQEC_DP_ERR_NOSUCHTUNER;
    } else if (g_output_shim.shutdown) {
        return VQEC_DP_ERR_SHUTDOWN;
    }

    tuner = vqec_dp_output_shim_get_tuner_by_id(tid);
    if (tuner) {
        memcpy(&tuner->chan, chan, sizeof(vqec_dp_chan_desc_t));
    }

    return (VQEC_DP_ERR_OK);
}

/**
 * Part of the iterator to scan all tuners that have been created.
 * This method returns the opaque handle of the "first" tuner. The 
 * ordering between "first" and "next" is implementation-specific. If there 
 * are no tuners, an invalid handle is returned.
 */
vqec_dp_error_t
vqec_dp_output_shim_get_first_dp_tuner (vqec_dp_tunerid_t *id)
{
    int i;
    
    if (!id) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    *id = VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID;
    if (g_output_shim.tuners) {
        for (i = 0; i < g_output_shim.max_tuners; i++) {
            if (g_output_shim.tuners[i]) {
                *id = i + 1;
                break;
            }
        }
        return (VQEC_DP_ERR_OK);
    }

    return (VQEC_DP_ERR_NOSUCHTUNER);
}

/**
 * Part of the iterator to scan all dp tuners that have been created.
 * This method returns the opaque handle of the "next" tuner  
 * given the "prev" handle. The ordering between "next" and "prev" is
 * implementation specific. If there are no more tuners, an 
 * invalid handle is returned.The following behavior is sought:
 *
 * (a)  Atomic semantics for the iterator may be enforced by the caller, if
 * needed, since a race exists between the addition of new tuners, 
 * and termination of the iteration. 
 * (b) If no tuners are created or destroyed, an implementation 
 * of the iterator must return each dp tuner's handle exactly once
 * before terminating.
 * (c) The behavior, when the caller does not impose atomic semantics,
 * and the collection of tuners is modified, is left unspecified.
 */
vqec_dp_error_t
vqec_dp_output_shim_get_next_dp_tuner (vqec_dp_tunerid_t prev_tid, 
                                       vqec_dp_tunerid_t *next_tid)
{
    int i;
    int prev_idx = prev_tid - 1;

    if (!next_tid) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    *next_tid = VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID;
    if ((g_output_shim.tuners)
        && (prev_tid <= g_output_shim.max_tuners)
        && (prev_tid >= 1)) {
        for (i = prev_idx + 1; i < g_output_shim.max_tuners; i++) {
            if (g_output_shim.tuners[i]) {
                *next_tid = i + 1;
                break;
            }
        }
    }
    return (VQEC_DP_ERR_OK);
}


/**
 * Gets a snapshot of the output thread's exit-to-enter jitter histogram
 * for a particular dataplane tuner.
 *
 * @param[in] dp_tid - Dataplane tuner id.
 * @param[out]  hist_ptr - Buffer into which histogram is copied.
 * @param[out] vqec_dp_error_t    - VQEC_DP_ERR_OK or failure code
 */
vqec_dp_error_t
vqec_dp_output_shim_hist_get (vqec_dp_tunerid_t dp_tid,
                              vqec_dp_outputshim_hist_t type,
                              vqec_dp_histogram_data_t *hist_ptr)
{
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    if (dp_tid > g_output_shim.max_tuners ||
        dp_tid < 1 ||
        type < VQEC_DP_HIST_OUTPUTSHIM_READER_JITTER ||
        type > VQEC_DP_HIST_OUTPUTSHIM_INP_DELAY ||
        !hist_ptr) {
        return VQEC_DP_ERR_INVALIDARGS;
    }  else if (!g_output_shim.tuners || !g_output_shim.tuners[dp_tid - 1]) {
        return VQEC_DP_ERR_NOSUCHTUNER;
    }  else if (!g_output_shim.tuners[dp_tid - 1]->sink ||
                !g_output_shim.tuners[dp_tid - 1]->sink->reader_jitter_hist) {
        return VQEC_DP_ERR_NOT_FOUND;
    }

    if (type == VQEC_DP_HIST_OUTPUTSHIM_READER_JITTER) {
        vam_hist_copy((vam_hist_type_t *)hist_ptr,
                      VQEC_DP_HIST_MAX_BUCKETS,
                      g_output_shim.tuners[dp_tid - 1]->sink->reader_jitter_hist);
    } else {
        vam_hist_copy((vam_hist_type_t *)hist_ptr,
                      VQEC_DP_HIST_MAX_BUCKETS,
                      g_output_shim.tuners[dp_tid - 1]->sink->inp_delay_hist);
    }

#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
    return (VQEC_DP_ERR_OK);
}


/**
 * Publish output jitter for all active tuners.
 *
 * @param[in] bufp buffer pointer in which to publish the data.
 * @param[in] len Length of buffer.
 * @param[out] int32_t Number of bytes written.
 */
int32_t
vqec_dp_output_shim_hist_publish (vqec_dp_outputshim_hist_t type,
                                  char *bufp,
                                  int32_t len)
{
    int32_t cum = 0;
#ifdef HAVE_SCHED_JITTER_HISTOGRAM
    vqec_dp_histogram_data_t data;
    int32_t i, wr = 0;

    if (bufp && len && g_output_shim.tuners) {
        for (i = 0; i < g_output_shim.max_tuners; i++) {
            if (g_output_shim.tuners[i]) {
                if (vqec_dp_output_shim_hist_get(i+1,
                                                 type,
                                                 &data) == VQEC_DP_ERR_OK) {
                    wr = snprintf(bufp, len, "DP TUNER %d\n", i+1);
                    if (wr >= len) {
                        cum += len;
                        break;
                    }
                    cum += wr;
                    bufp += wr;
                    len -= wr;
                    wr = vam_hist_publish_nonzero_hits((vam_hist_type_t *)&data,
                                                       bufp, len);
                    if (wr >= len) {
                        cum += len;
                        break;
                    }
                    cum += wr;
                    bufp += wr;
                    len -= wr;
                }
            }
        }
    }

#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
    return (cum);
}
