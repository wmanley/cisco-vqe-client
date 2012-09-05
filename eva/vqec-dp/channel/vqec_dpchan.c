/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: VQE-C Dataplane Channel implementation.
 *
 * Documents: A dataplane channel has upto 4 RTP input-streams (IS), and 1 output 
 * stream (OS) which connects to the outputshim.  
 *
 *****************************************************************************/

#include "vqec_dpchan.h"
#include <vqec_nll.h>
#include <vqec_dp_io_stream.h>
#include <vqec_dp_utils.h>
#include "vqec_dp_rtp_input_stream.h"
#include "vqec_oscheduler.h"
#include <utils/vam_util.h>
#include <utils/mp_mpeg.h>
#include <utils/mp_tlv_decode.h>
#include <utils/zone_mgr.h>

#ifdef _VQEC_DP_UTEST
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

typedef struct vqe_zone vqec_hist_pool_t;

typedef
struct app_tlv_buf_
{
    char *buf;
    uint16_t len;
} app_tlv_buf_t;

/** 
 * Global dp channel module instance.
 */
typedef 
struct vqec_dpchan_module_
{
    /**
     * Id table for channel identifiers.
     */
    id_table_key_t id_table;
    /**
     * Generation identifier count for channel.
     */
    uint32_t generation_id;
    /**
     * Count of channels presently active.
     */
    uint32_t chan_cnt;
    /**
     * Has the module been initialized.
     */
    boolean init_done;
    /*
     * Histograms of measurements made within the dpchan module.
     */
    vam_hist_type_t *hist[VQEC_DP_HIST_MAX];
    vqec_hist_pool_t *hist_pool[VQEC_DP_HIST_MAX];
    /*
     * Is the output scheduling histogram enabled?
     */
    boolean hist_outputsched_enabled;
    /*
     * Last time output scheduling was performed
     * (when output scheduling monitoring is enabled).
     */
    abs_time_t last_outputsched_time;
    /**
     * List of all the channels.
     */
    VQE_TAILQ_HEAD(, vqec_dpchan_) chan_list;
    /**
     * Pool for channel output streams.
     */
    struct vqe_zone *os_pool;
    /**
     * Pool for channel objects.
     */
    struct vqe_zone *chan_pool;
    /**
     * Pool for the decoded TSPAKs from the APP TLV.
     */
    struct vqe_zone *tspkt_pool;
    /**
     * Buffer for TLVs.
     */
    app_tlv_buf_t tlvbuf;
    /**
     * Parameter for maximum number of (replicated) APP paks per RCC.
     */
    uint32_t app_paks_per_rcc;
    /**
     * APP control knobs for MP.
     */
    mp_tlv_mpegknobs_t knobs;
} vqec_dpchan_module_t;

static vqec_dpchan_module_t s_dpchan_module;


/**---------------------------------------------------------------------------
 * From channel identifier to the instance of the channel.
 * 
 * @param[in] chanid Id of the channel.
 * @param[out] vqec_dpchan_t* Pointer to the channel instance.
 *---------------------------------------------------------------------------*/ 
UT_STATIC inline vqec_dpchan_t *
vqec_dp_chanid_to_ptr (vqec_dp_chanid_t chanid)
{
    id_mgr_ret ret;
    vqec_dpchan_t *chan;

    if (!s_dpchan_module.init_done ||
        chanid == VQEC_DP_CHANID_INVALID ||
        s_dpchan_module.id_table == ID_MGR_TABLE_KEY_ILLEGAL) {
        return (NULL);
    }

    chan = (vqec_dpchan_t *)id_to_ptr(chanid, &ret, 
                                      s_dpchan_module.id_table);
    if (!chan || (ret != ID_MGR_RET_OK)) {
        return (NULL);
    } 

    return (chan);
}


/**---------------------------------------------------------------------------
 * Print the name of the channel in a static buffer: to-be-used only for
 * debug purposes: do-not-cache.
 * 
 * @param[in] chan Pointer to the channel.
 * @param[out] char* Pointer to a formatted string for the channel name. 
 *---------------------------------------------------------------------------*/ 
char *
vqec_dpchan_print_name (vqec_dpchan_t *chan)
{
    static char name_buf[MAX_DPCHAN_IPV4_PRINT_NAME_SIZE+1];

    (void)strlcpy(name_buf, chan->url, sizeof(name_buf));
    return (name_buf);
}


/****************************************************************************
 * Output-stream (OS).
 *
 * HELPFUL DEFINITIONS: 
 * -- The output-stream (OS) is connected to at most 1 input-stream on the 
 *     outputshim in this implementation.
 * -- The OS supports either RTP or UDP encapsulation. It is possible to
 *     support both encapsulations at the same time using 2 differnet OS'es.
 *     However, this approach requires the cloning of packet headers (2 
 *     different headers) which is absent in this implementation.
 * -- For RTP encapsulation, the repair packets need to be reformulated
 *     (OSN needs to be removed) for a player to display the repaired
 *     stream, which is not done here.
 */


/*
 * The Output Stream data object
 *
 * This represents a stream of data that is passed from the input shim
 * to a particular connected input stream.
 */
typedef struct vqec_dpchan_os  {
    vqec_dp_osid_t          os_id;    /* handle ID for this OS */
    vqec_dp_encap_type_t    encaps;   /* stream encapsulation type (udp/rtp) */
    int32_t                 capa;     /* capabilities of this OS */
    vqec_dp_isid_t          is_id;    /* connected IS (if connected) */
    const vqec_dp_isops_t   *is_ops;  /* connected IS APIs (if connected) */
    int32_t                 is_capa;  /* connected IS capabilities */
    uint32_t                packets;  /* number of packets */
    uint32_t                bytes;    /* number of bytes */
    uint32_t                drops;    /* number of dropped packets */
    vqec_dpchan_t           *dpchan;  /* pointer to associated dpchan */
} vqec_dpchan_os_t;

static vqec_dpchan_os_t
*s_vqec_dpchan_oses[VQEC_DPCHAN_MAX_DPCHAN_OSES] = {NULL};

UT_STATIC vqec_dpchan_os_t *vqec_dpchan_osid_to_os (vqec_dp_osid_t osid)
{
    if (osid > 0 && osid <= VQEC_DPCHAN_MAX_DPCHAN_OSES) {
        return s_vqec_dpchan_oses[osid - 1];
    } else {
        return NULL;
    }
}

UT_STATIC vqec_dp_osid_t vqec_dpchan_create_osid (vqec_dpchan_os_t *os)
{
    int i;

    for (i = 0; i < VQEC_DPCHAN_MAX_DPCHAN_OSES; i++) {
        if (!s_vqec_dpchan_oses[i]) {
            /* free slot found; cache os ptr and return id */
            s_vqec_dpchan_oses[i] = os;
            return (i + 1);
        }
    }

    return VQEC_DP_INVALID_OSID;
}

void vqec_dpchan_destroy_osid (vqec_dp_osid_t osid)
{
    s_vqec_dpchan_oses[osid - 1] = NULL;
}

/**
 * Get the operations structure for an IS connected to the output stream.
 *
 * @param[in] osid A valid output stream handle.
 * @param[out] vqec_dp_isops_t* Operations structure for a connected IS.
 */
const vqec_dp_isops_t 
*vqec_dpchan_os_get_connected_is_ops (vqec_dp_osid_t osid)
{
    vqec_dpchan_os_t *os;

    os = vqec_dpchan_osid_to_os(osid);
    if (!os) {
        return NULL;
    }

    return (os->is_ops); 
}

/**
 * Get the capability flags for the output stream. 
 *
 * @param[in] os A valid output stream handle.
 * @param[out] int32_t Stream's capability flags.
 */
int32_t
vqec_dpchan_os_capa (vqec_dp_osid_t osid)
{
    vqec_dpchan_os_t *os;

    os = vqec_dpchan_osid_to_os(osid);
    if (!os) {
        return VQEC_DP_STREAM_CAPA_NULL;
    }

    return (os->capa);
}

/**
 * Connect the given input and output streams together. If the 
 * capabilities requested in the parameter req_capa are either 
 * ambiguous, e.g., both data push and data pull or cannot be
 * supported, the method must fail. It must not have any
 * visible side-effects when it fails - a subsequent connect with
 * modified & supported capabilities must be allowed to succeed.
 *
 * Otherwise, the method must logically connect the output
 * and input streams together, cache the input stream operations
 * interface reference, and deliver datagrams to the
 * input stream through that interface.
 *
 * An implementation may accept multiple connections on
 * one output stream, i.e., allow multiple input streams to 
 * connect to it. For this particular case, the packets
 * delivered to input streams may be cloned (implementation specific).
 *
 * @param[in] os A valid output stream handle.
 * @param[in] is A valid input stream handle.
 * @param[in] ops* Input stream operations interface.
 * @param[in] encap Requested encapsulation type.
 * @param[in] req_capa Requested capabilities.
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK on
 * success.
 */
vqec_dp_stream_err_t
vqec_dpchan_os_accept_connect (vqec_dp_osid_t osid,
                               vqec_dp_isid_t isid,
                               const vqec_dp_isops_t *ops,
                               vqec_dp_encap_type_t encap,
                               int32_t req_capa)
{
    vqec_dp_stream_err_t status = VQEC_DP_STREAM_ERR_OK;
    vqec_dpchan_os_t *os;
    vqec_dp_oscheduler_streams_t dpchan_streams;

    memset(&dpchan_streams, 0, sizeof(vqec_dp_oscheduler_streams_t));

    /* Validate the parameters */
    os = vqec_dpchan_osid_to_os(osid);
    if (!os || !ops || (req_capa > VQEC_DP_STREAM_CAPA_MAX) ||
        ((req_capa & VQEC_DP_STREAM_CAPA_PUSH_VECTORED) &&
         !ops->receive_vec) ||
        ((req_capa & VQEC_DP_STREAM_CAPA_PUSH) && !ops->receive)) {
        status = VQEC_DP_STREAM_ERR_INVALIDARGS;
        goto done;
    }
    if (encap != os->encaps) {
        status = VQEC_DP_STREAM_ERR_ENCAPSMISMATCH;
        goto done;
    }
    if (!(req_capa & os->capa)) {
        status = VQEC_DP_STREAM_ERR_NACKCAPA;
        goto done;
    }
    if (os->is_id != VQEC_DP_INVALID_ISID) {
        status = VQEC_DP_STREAM_ERR_OSALREADYCONNECTED;
        goto done;
    }

    /* Init the output scheduler */
    if (os->dpchan) {
        dpchan_streams.osencap[0] = os->dpchan->pcm.strip_rtp ?
            VQEC_DP_ENCAP_UDP : VQEC_DP_ENCAP_RTP;
        dpchan_streams.isids[0] = isid;
        dpchan_streams.isops[0] = ops;
        if (vqec_dp_oscheduler_add_streams(&os->dpchan->pcm.osched,
                                           dpchan_streams) != VQEC_DP_ERR_OK) {
            status = VQEC_DP_STREAM_ERR_INTERNAL;
            goto done;
        }

        /* Connection can succeed, store associated IS information */
        os->is_id = isid;
        os->is_ops = ops;
        os->is_capa = req_capa;

    } else {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "dpchan_accept_connect: no dpchan; "
                             "can't init osched");
    }

done:
    return (status);
}

/**
 * Disconnect the output stream from any input stream that
 * it may have been connected to via an earlier connect call.
 *
 * @param[in] os A valid output stream handle.
 */
void
vqec_dpchan_os_disconnect (vqec_dp_osid_t osid)
{
    vqec_dpchan_os_t *os;

    /* Validate the parameter */
    os = vqec_dpchan_osid_to_os(osid);
    if (!os) {
        goto done;
    }

    /* Disconnect the OS from its IS, if any */
    os->is_id = VQEC_DP_INVALID_ISID;
    os->is_ops = NULL;
    os->is_capa = VQEC_DP_STREAM_CAPA_NULL;

done:
    return;
}


/**
 * Get stream status for an output stream. This includes the number of
 * connected input streams, the filter binding (if any), and
 * statistics. An implementation may choose not to keep per output
 * statistics. The statistics should be reset to 0 for that case.
 *
 * @param[in] os A valid output stream handle.
 * @param[out] status Status structure for the result.
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK on
 * success.
 */
vqec_dp_stream_err_t
vqec_dpchan_os_get_status (vqec_dp_osid_t osid,
                           vqec_dp_os_status_t *stat)
{
    vqec_dpchan_os_t *os;
    vqec_dp_stream_err_t status = VQEC_DP_STREAM_ERR_OK;

    /* Validate parameters */
    os = vqec_dpchan_osid_to_os(osid);
    if (!os || !stat) {
        status = VQEC_DP_STREAM_ERR_INVALIDARGS;
        goto done;
    }

    memset(stat, 0, sizeof(vqec_dp_os_status_t));
    stat->is_cnt = 1;
    stat->stats.packets = os->packets;
    stat->stats.bytes = os->bytes;
    stat->stats.drops = os->drops;

done:
    return status;
}


/**
 * Retrieves the set of input streams associated with the given
 * output stream.
 *
 * Callers supply an isid_array (of size isid_array_len) into which
 * IS IDs for OS are copied.  The number of IS IDs copied into the
 * array is bound by the array size (specified by isid_array_len),
 * and returned via the num_isids parameter.
 *
 * If there are more IS IDs for the OS that were not copied into 
 * the isid_array (due to space limitations), then the more parameter 
 * will be set to TRUE upon return of the function.  The caller may 
 * then call the function again, with is_id_last assigned the ID value
 * of the last ID returned in the previous call.  (If this is the first
 * call, is_id_last should be VQEC_DP_INVALID_ISID to start from the
 * beginning of the list.)  Should the OS to IS stream mapping change
 * between individual calls to this function, callers may not retrieve
 * an atomic snapshot of the OS to IS mapping.
 *
 * @param[in] os_id       The OS whose ISes are to be retrieved
 * @param[in] is_id_last  The last IS retrieved from the previous call
 *                         of a sequence (which returned more==TRUE), or 
 *                         VQEC_DP_INVALID_ISID for the initial call
 *                         (to retrieve IS IDs starting at the beginning)
 * @param[out] isid_array      Array into which the ISes of os_id are 
 *                              copied
 * @param[in]  isid_array_len  Size of isid_array (in elements)
 * @param[out] num_isids       Number of IS IDs copied into isid_array
 * @param[out] more            TRUE if there are more IS IDs to be
 *                              retrieved for OS, or FALSE otherwise.
 * @param[out] vqec_dp_stream_err_t  Success/failure status of request
 */ 
vqec_dp_stream_err_t
vqec_dpchan_os_get_connected_is (vqec_dp_osid_t osid,
                                 vqec_dp_isid_t isid_last,
                                 vqec_dp_isid_t *isid_array,
                                 uint32_t isid_array_len,
                                 uint32_t *num_isids,
                                 boolean *more)
{
    vqec_dpchan_os_t *os;
    vqec_dp_stream_err_t status;

    /* Validate parameters */
    os = vqec_dpchan_osid_to_os(osid);
    if (!os || !isid_array || !isid_array_len || !num_isids || !more) {
        status = VQEC_DP_STREAM_ERR_INVALIDARGS;
        goto done;
    }

    /*
     * Output Streams can only have 0 or 1 connected input stream right now.
     * Copy its IS ID into the first position in the array.
     */
    if ((isid_last == VQEC_DP_INVALID_ISID) &&
        (os->is_id != VQEC_DP_INVALID_ISID)) {
        isid_array[0] = os->is_id;
        *num_isids = 1;
    } else {
        *num_isids = 0;
    }
    *more = FALSE;
    status = VQEC_DP_STREAM_ERR_OK;

done:
    return (status);
}


/**
 * Enumerate the encapsulation types that the stream supports.
 *
 * @param[in] os A valid output stream handle.
 * @param[out] encaps The array in which supported encapsulation
 * types are returned. The length of this array is given by *num 
 * on input; *num is updated to to the number of entries actually
 * written in encaps by this method.
 * @param[in / out] num When the method is invoked, contains 
 * the size of the encaps array. Before this method returns it 
 * will update *num with the number of types that are being
 * returned in the encaps array. 
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK if
 * no errors were encountered during the execution of this method.
 */
vqec_dp_stream_err_t
vqec_dpchan_os_supported_encaps (vqec_dp_osid_t osid, 
                                 vqec_dp_encap_type_t *encaps,
                                 uint32_t *num)
{
    vqec_dpchan_os_t *os;

    /* Validate parameters */
    os = vqec_dpchan_osid_to_os(osid);
    if (!os || !encaps || !num || !*num) {
        return VQEC_DP_STREAM_ERR_INVALIDARGS;
    }

    /* Return the encapsulation defined for the OS */
    encaps[0] = os->encaps;
    *num = 1;

    return VQEC_DP_STREAM_ERR_OK;
}



/*
 * vqec_dpchan_os_ops
 *
 * The dpchan implements an API for using its Output Stream objects.
 * This API (used by Input Stream objects) is defined by the function
 * pointers below.
 */
static const vqec_dp_osops_t vqec_dpchan_os_ops = {
    vqec_dpchan_os_capa,
    vqec_dpchan_os_accept_connect,
    vqec_dpchan_os_disconnect,
    NULL,  /* bind */
    NULL,  /* unbind */
    vqec_dpchan_os_get_status,
    vqec_dpchan_os_get_connected_is,
    NULL,  /* backpressure */
    NULL,  /* read */
    NULL,  /* read_raw */
    vqec_dpchan_os_supported_encaps,
    NULL, /* bind_reserve */
    NULL, /* bind_commit */
    NULL, /* bind_update */
    NULL, /* push_poll */
};


/****************************************************************************
 * Graph builder API helpers.
 */

/**---------------------------------------------------------------------------
 * Add a new OS to the dpchan - this OS will be used to transmit
 * the repaired-stream from the dpchan to an outputshim IS.
 * The dpchan will allow a maximum of one OS to be created, which
 * may have either RTP or UDP encapsulation.
 *
 * The OS instance is returned as an output argument. This instance
 * may then be used to connect to output shim input stream.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[in] enc Encapsulation for the OS.
 * @param[out] instance Instance of the OS returned to the caller.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_error_t
vqec_dpchan_add_os (vqec_dp_chanid_t chanid,
                    vqec_dp_encap_type_t enc,
                    vqec_dp_os_instance_t *instance)
{
    vqec_dpchan_os_t *os = NULL;
    vqec_dpchan_t *dpchan;

    dpchan = vqec_dp_chanid_to_ptr(chanid);

    if (!dpchan || !instance) {
        return VQEC_DP_ERR_INVALIDARGS;
    } else if (dpchan->os.id != VQEC_DP_INVALID_OSID && dpchan->os.ops) {
        /* OS already exists for this dpchan; no need to create a new one */
        instance->id = dpchan->os.id;
        instance->ops = dpchan->os.ops;
        return VQEC_DP_ERR_OK;
    }

    /* malloc the os and give it an ID */
    os = (vqec_dpchan_os_t *) zone_acquire(s_dpchan_module.os_pool);
    if (!os) {
        goto bail;
    }
    memset(os, 0, sizeof(vqec_dpchan_os_t));

    os->os_id = vqec_dpchan_create_osid(os);
    if (os->os_id == VQEC_DP_INVALID_OSID) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "out-of identifiers in OS pool");
        goto bail;
    }

    /* set up os parameters */
    os->capa = VQEC_DP_STREAM_CAPA_PUSH | VQEC_DP_STREAM_CAPA_PUSH_VECTORED;
    os->encaps = enc;
    os->dpchan = dpchan;

    /* cache the os_instance in the dpchan structure */
    dpchan->os.id = os->os_id;
    dpchan->os.ops = &vqec_dpchan_os_ops;

    /* set up the os_instance for return */
    instance->id = os->os_id;
    instance->ops = &vqec_dpchan_os_ops;

    return VQEC_DP_ERR_OK;

bail:
    if (os) {
        zone_release (s_dpchan_module.os_pool, os);
    }
    return VQEC_DP_ERR_NOMEM;
}


/**---------------------------------------------------------------------------
 * Delete and destroy the OS associated with a dpchan.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_error_t
vqec_dpchan_del_os (vqec_dp_chanid_t chanid)
{
    vqec_dpchan_os_t *os = NULL;
    vqec_dpchan_t *dpchan;

    dpchan = vqec_dp_chanid_to_ptr(chanid);

    if (!dpchan) {
        return VQEC_DP_ERR_INVALIDARGS;
    } else if (dpchan->os.id == VQEC_DP_INVALID_OSID && !dpchan->os.ops) {
        /* no OS exists for this dpchan; nothing to delete */
        return VQEC_DP_ERR_OK;
    }

    os = vqec_dpchan_osid_to_os(dpchan->os.id);
    if (!os) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                     "can't find dpchan os");
        goto bail;
    }

    if (os->os_id != dpchan->os.id) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "os ID and cached os ID mismatch");
        goto bail;
    }
    vqec_dpchan_destroy_osid(os->os_id);

    zone_release (s_dpchan_module.os_pool, os);

    /* remove the cached os_instance from the dpchan structure */
    dpchan->os.id = VQEC_DP_INVALID_OSID;
    dpchan->os.ops = NULL;

    return VQEC_DP_ERR_OK;

bail:
    return VQEC_DP_ERR_INTERNAL;
}


/**---------------------------------------------------------------------------
 * Start the flow of data on a dp channel OS: external API.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_run_os (vqec_dpchan_t *chan)
{
    /* start the oscheduler */
    /* sa_ignore {} IGNORE_RETURN(1) */
    vqec_dp_oscheduler_start(&chan->pcm.osched);
}


/**---------------------------------------------------------------------------
 * Stop the flow of data on a dp channel OS: external API.
 *
 * @param[in] chanid Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_stop_os (vqec_dpchan_t *chan)
{
    /* stop the oscheduler */
    (void)vqec_dp_oscheduler_stop(&chan->pcm.osched);
}

/**---------------------------------------------------------------------------
 * Add a new IS to the dp channel.
 *
 * @param[in] chanid Pointer to the channel.
 * @param[in] type Type of the input stream generating the event. At present
 * only primary and repair IS can generate events.
 * @param[out] instance Instance of the IS to be returned back to the caller.
 * @param[in] stream_desc Description of the stream being added.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_error_t
vqec_dpchan_add_is (vqec_dpchan_t *chan, 
                    vqec_dp_input_stream_type_t type,
                    vqec_dp_is_instance_t *instance,
                    vqec_dp_input_stream_t *stream_desc)
{
#define DPCHAN_ADDIS_ERRSTR_LEN 24
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_stream_err_t s_err;
    vqec_dp_chan_input_stream_t *iptr;
    vqec_dp_isid_t *p_isid;
    uint32_t handle;
    char tmp[DPCHAN_ADDIS_ERRSTR_LEN];

    memset(tmp, 0, sizeof(tmp));

    switch (type) {
    case VQEC_DP_INPUT_STREAM_TYPE_PRIMARY:   /* Primary IS */
        snprintf(tmp, sizeof(tmp), "Add primary IS ");
        if (chan->prim_is != VQEC_DP_INVALID_ISID) {
            err = VQEC_DP_ERR_EXISTS;
            break;
        }
        chan->prim_is = 
            vqec_dp_chan_rtp_primary_input_stream_create(chan, stream_desc);
        if (chan->prim_is == VQEC_DP_INVALID_ISID) {
            err = VQEC_DP_ERR_NO_RESOURCE;
            break;
        }
        
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->prim_is);
        if (!iptr) {
            err = VQEC_DP_ERR_INTERNAL; 
            break;
        }

        VQEC_DP_ASSERT_FATAL(vqec_dp_chan_input_stream_type(iptr) ==
                             VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, "Add IS");

        s_err = vqec_dp_chan_input_stream_get_instance(iptr, instance);
        if (s_err != VQEC_DP_STREAM_ERR_OK) {
            err = VQEC_DP_ERR_INTERNAL;            
        }
        break;


    case VQEC_DP_INPUT_STREAM_TYPE_REPAIR:    /* Repair IS */
        snprintf(tmp, sizeof(tmp), "Add Repair IS ");
        if (chan->repair_is != VQEC_DP_INVALID_ISID) {
            err = VQEC_DP_ERR_EXISTS;
            break;
        }

        chan->repair_is = 
            vqec_dp_chan_rtp_repair_input_stream_create(chan, stream_desc);
        if (chan->repair_is == VQEC_DP_INVALID_ISID) {
            err = VQEC_DP_ERR_NO_RESOURCE;
            break;
        }
        
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->repair_is);
        if (!iptr) {
            err = VQEC_DP_ERR_INTERNAL; 
            break;
        }

        VQEC_DP_ASSERT_FATAL(vqec_dp_chan_input_stream_type(iptr) ==
                             VQEC_DP_INPUT_STREAM_TYPE_REPAIR, "Add IS");

        s_err = vqec_dp_chan_input_stream_get_instance(iptr, instance);
        if (s_err != VQEC_DP_STREAM_ERR_OK) {
            err = VQEC_DP_ERR_INTERNAL;            
        }
        break;


    case VQEC_DP_INPUT_STREAM_TYPE_FEC:       /* FEC IS */
        snprintf(tmp, sizeof(tmp), "Add FEC IS ");
        if ((chan->fec0_is != VQEC_DP_INVALID_ISID) &&
            (chan->fec1_is != VQEC_DP_INVALID_ISID)) {
            err = VQEC_DP_ERR_EXISTS;
            break;
        }
        if (chan->fec0_is == VQEC_DP_INVALID_ISID) {
            p_isid = &chan->fec0_is;
            handle = VQEC_DP_FEC0_IS_HANDLE;
        } else {
            p_isid = &chan->fec1_is;
            handle = VQEC_DP_FEC1_IS_HANDLE;
        }
        
        
        *p_isid = vqec_dp_chan_rtp_fec_input_stream_create(chan,
                                                           handle,
                                                           stream_desc);
        if (*p_isid == VQEC_DP_INVALID_ISID) {
            err = VQEC_DP_ERR_NO_RESOURCE;
            break;
        }
        
        iptr = vqec_dp_chan_input_stream_id_to_ptr(*p_isid);
        if (!iptr) {
            err = VQEC_DP_ERR_INTERNAL; 
            break;
        }

        VQEC_DP_ASSERT_FATAL(vqec_dp_chan_input_stream_type(iptr) ==
                             VQEC_DP_INPUT_STREAM_TYPE_FEC, "Add IS");

        s_err = vqec_dp_chan_input_stream_get_instance(iptr, instance);
        if (s_err != VQEC_DP_STREAM_ERR_OK) {
            err = VQEC_DP_ERR_INTERNAL;            
        }
        break;

    default:
        snprintf(tmp, sizeof(tmp), "Add invalid IS ");
        err = VQEC_DP_ERR_INVALIDARGS;
        break;
    }

    if (err != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             tmp, 
                             vqec_dp_err2str(err));
    }

    return (err);
}

/**---------------------------------------------------------------------------
 * Change the state of all RTP receivers to either "run"-ning or stopped: 
 * Internal implementation.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] act Boolean flag specifying run: true or stop: false.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_act_all_rtp_receivers_internal (vqec_dpchan_t *chan, boolean act)
{    
    vqec_dp_chan_input_stream_t *iptr = NULL;

    if ((chan->prim_is != VQEC_DP_INVALID_ISID) &&
        (iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->prim_is))) {

        VQEC_DP_ASSERT_FATAL(vqec_dp_chan_input_stream_type(iptr) ==
                             VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, "Run IS");
    
        if (act) {
            vqec_dp_chan_input_stream_run(iptr);
        } else {
            vqec_dp_chan_input_stream_stop(iptr);
        }
    }

    if ((chan->repair_is != VQEC_DP_INVALID_ISID) &&
        (iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->repair_is))) {

        VQEC_DP_ASSERT_FATAL(vqec_dp_chan_input_stream_type(iptr) ==
                             VQEC_DP_INPUT_STREAM_TYPE_REPAIR, "Run IS");

        if (act) {
            vqec_dp_chan_input_stream_run(iptr);
        } else {
            vqec_dp_chan_input_stream_stop(iptr);
        }
    }

    if ((chan->fec0_is != VQEC_DP_INVALID_ISID) &&
        (iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->fec0_is))) {

        VQEC_DP_ASSERT_FATAL(vqec_dp_chan_input_stream_type(iptr) ==
                             VQEC_DP_INPUT_STREAM_TYPE_FEC, "Run IS");

        if (act) {
            vqec_dp_chan_input_stream_run(iptr);
        } else {
            vqec_dp_chan_input_stream_stop(iptr);
        }
    }

    if ((chan->fec1_is != VQEC_DP_INVALID_ISID) &&
        (iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->fec1_is))) {

        VQEC_DP_ASSERT_FATAL(vqec_dp_chan_input_stream_type(iptr) ==
                             VQEC_DP_INPUT_STREAM_TYPE_FEC, "Run IS");

        if (act) {
            vqec_dp_chan_input_stream_run(iptr);
        } else {
            vqec_dp_chan_input_stream_stop(iptr);
        }
    }
}

/**---------------------------------------------------------------------------
 * Change the state of all RTP receivers to "run"-ning: external API.
 *
 * @param[in] chanid Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_run_all_rtp_receivers (vqec_dpchan_t *chan)
{
    vqec_dpchan_act_all_rtp_receivers_internal(chan, TRUE);
}

/**
 * Add the RTP receivers to the DP chan.  Return a DP chan input object with
 * the created input stream information.
 */
UT_STATIC vqec_dp_error_t
vqec_dpchan_add_rtp_receivers (vqec_dp_chan_desc_t *desc,
                               vqec_dpchan_t *chan,
                               vqec_dp_input_stream_obj_t *obj)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    /* add the RTP receivers for the created streams */
    if ((err = vqec_dpchan_add_is(chan,
                                  VQEC_DP_INPUT_STREAM_TYPE_PRIMARY,
                                  &obj->is[VQEC_DP_IO_STREAM_TYPE_PRIMARY],
                                  &desc->primary))
        != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                             vqec_dpchan_print_name(chan),
                             __FUNCTION__,
                             "cannot add rtp receiver for primary");
        goto bail;
    }
    if (desc->en_repair || desc->en_rcc) {
        /* The repair session needs to added for both ER and RCC. */
        if ((err = vqec_dpchan_add_is(chan,
                                      VQEC_DP_INPUT_STREAM_TYPE_REPAIR,
                                      &obj->is[VQEC_DP_IO_STREAM_TYPE_REPAIR],
                                      &desc->repair))
            != VQEC_DP_ERR_OK) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                                 vqec_dpchan_print_name(chan),
                                 __FUNCTION__,
                                 "cannot add rtp receiver for repair");
            goto bail;
        }
    }
    if (desc->en_fec0) {
        if ((err = vqec_dpchan_add_is(chan,
                                      VQEC_DP_INPUT_STREAM_TYPE_FEC,
                                      &obj->is[VQEC_DP_IO_STREAM_TYPE_FEC_0],
                                      &desc->fec_0))
            != VQEC_DP_ERR_OK) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                                 vqec_dpchan_print_name(chan),
                                 __FUNCTION__,
                                 "cannot add rtp receiver for fec 0");
            goto bail;
        }
    }
    if (desc->en_fec1) {
        if ((err = vqec_dpchan_add_is(chan,
                                      VQEC_DP_INPUT_STREAM_TYPE_FEC,
                                      &obj->is[VQEC_DP_IO_STREAM_TYPE_FEC_1],
                                      &desc->fec_1))
            != VQEC_DP_ERR_OK) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                                 vqec_dpchan_print_name(chan),
                                 __FUNCTION__,
                                 "cannot add rtp receiver for fec 1");
            goto bail;
        }
    }

    /* run all of the RTP receivers */
    vqec_dpchan_run_all_rtp_receivers(chan);

bail:
    return (err);
}

/**
 * Add the output OS to the DP chan.  Return a DP chan input object with
 * the created output stream information.
 */
UT_STATIC vqec_dp_error_t
vqec_dpchan_create_output (vqec_dp_chan_desc_t *desc,
                           vqec_dpchan_t *chan,
                           vqec_dp_os_instance_t *os)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    /* add (create) the dpchan scheduler OS */
    if ((err = vqec_dpchan_add_os(chan->id,
                                  desc->strip_rtp ?
                                      VQEC_DP_ENCAP_UDP : VQEC_DP_ENCAP_RTP,
                                  os))
        != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                             vqec_dpchan_print_name(chan),
                             __FUNCTION__,
                             "cannot add dpchan scheduler OS");
    }

    /* run OS */
    vqec_dpchan_run_os(chan);

    return (err);
}

/**---------------------------------------------------------------------------
 * Delete any ISes on the dp channel.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dpchan_del_all_is (vqec_dp_chanid_t chanid)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_chan_input_stream_t *iptr;
    vqec_dpchan_t *chan;

    chan = vqec_dp_chanid_to_ptr(chanid);
    if (!chan) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             __FUNCTION__, 
                             "Invalid arguments");
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    if (chan->prim_is != VQEC_DP_INVALID_ISID) {
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->prim_is);
        MCALL(iptr, destroy);
    }

    if (chan->repair_is != VQEC_DP_INVALID_ISID) {
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->repair_is);
        MCALL(iptr, destroy);
    }

    if (chan->fec0_is != VQEC_DP_INVALID_ISID) {
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->fec0_is);
        MCALL(iptr, destroy);
    }

    if (chan->fec1_is != VQEC_DP_INVALID_ISID) {
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->fec1_is);
        MCALL(iptr, destroy);
    }

    return (err);
}

/**---------------------------------------------------------------------------
 * Change the state of all RTP receivers to "stop"-ped: external API.
 *
 * @param[in] chanid Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_stop_all_rtp_receivers (vqec_dpchan_t *chan)
{    
    vqec_dpchan_act_all_rtp_receivers_internal(chan, FALSE);
}


/****************************************************************************
 * Upcall IPC events.
 *
 * HELPFUL DEFINITIONS: 
 *  -- Upcall: An unsolicited event from the data- to the control-plane.
 *  -- IRQ: An interrupt request (IRQ) is a message from the dataplane to
 *     the control-plane, sent via socket. The request tells the control-plane
 *     that an event is waiting in the dataplane which the control-plane 
 *     should read via IPC. 
 *  -- IRQ lines: Similar to multiple HW interrupt lines, in the implementation
 *     below each dataplane channel has 3 interrupt lines. Each of these lines 
 *      is associated with a "device":
 *       --- Primary RTP session device generates interrupts for events on
 *           the primary RTP input session.
 *       --- Repair RTP session device generates interrupts for events on
 *           the repair RTP input session.
 *       --- Dpchan device generates interrupts for events in the RCC FSM.
 *  -- If a device for a channel has an interrupt pending then no further
 *     "interrupts" are sent. Thus the maximum number of interrupts pending
 *     is bounded by (active_channels * IRQ_lines).
 *  -- The control-plane must "acknowledge" an interrupt via an IPC call,
 *  and read the data associated with the event that generated the interrupt.
 *  -- IRQ Polling: It is possible to request all pending events for all 
 *     devices on a channel by "polling". This is only used if the control-
 *     plane detects message loss (unlikely in this implementation).
 */


/**---------------------------------------------------------------------------
 * Collect RCC NCSI information - this method is invoked in response to.
 * an upcall IRQ.
 * 
 * @param[in] chan Pointer to the channel.
 * @param[in] ncsi_data Pointer to the ncsi data structure.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
#ifdef HAVE_FCC 
UT_STATIC vqec_dp_error_t
vqec_dp_chan_get_rcc_ncsi_info (vqec_dpchan_t *chan, 
                                vqec_dp_upc_ncsi_data_t *ncsi_data)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    memset(ncsi_data, 0, sizeof(*ncsi_data));

    if (chan->rcc_enabled) {
        ncsi_data->first_primary_seq = chan->first_primary_seq;
        ncsi_data->first_primary_rx_ts = chan->first_primary_ts;
        if (TIME_CMP_A(le, chan->first_primary_ts, chan->join_issue_time)){
            ncsi_data->join_latency = REL_TIME_0;
        } else {
            ncsi_data->join_latency = TIME_SUB_A_A(chan->first_primary_ts,
                                                   chan->join_issue_time);
        }
        ncsi_data->er_enable_ts = TIME_ADD_A_R(chan->first_repair_ts, 
                                               chan->dt_earliest_join);
        ncsi_data->er_enable_ts = TIME_ADD_A_R(ncsi_data->er_enable_ts,
                                               chan->er_holdoff_time);
    } else {

        /* NCSI info acquisition but RCC is disabled. */
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             " ", 
                             "NCSI info acquisition with rcc disabled");
        err = VQEC_DP_ERR_NOT_FOUND;       
    }

    return (err);
}


UT_STATIC vqec_dp_error_t
vqec_dp_chan_get_fast_fill_info (vqec_dpchan_t *chan, 
                                vqec_dp_upc_fast_fill_data_t *fast_fill_data)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    memset(fast_fill_data, 0, sizeof(*fast_fill_data));

    if (chan->rcc_enabled) {
        fast_fill_data->total_fast_fill_bytes = 
            chan->pcm.total_fast_fill_bytes;
        fast_fill_data->fast_fill_start_time = 
            chan->pcm.osched.outp_log.first_pak_ts;
        fast_fill_data->fast_fill_end_time  = chan->pcm.fast_fill_end_time;
    } else {
        /* fast fill info acquisition but RCC or FF is disabled. */
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             " ", 
                             "fast fill info acquisition disabled");
        err = VQEC_DP_ERR_NOT_FOUND;       
    }

    return (err);
}
#endif /* HAVE_FCC */

UT_STATIC vqec_dp_error_t
vqec_dp_chan_get_fec_update_info (vqec_dpchan_t *chan,
                                  vqec_dp_upc_fec_update_data_t *data)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    memset(data, 0, sizeof(*data));

    if (chan->fec) {
        data->fec_l_value = chan->fec->fec_info.fec_l_value;
        data->fec_d_value = chan->fec->fec_info.fec_d_value;
    } else {
        /* FEC disabled for FEC info not found */
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             " ", 
                             "FEC info not found");
        err = VQEC_DP_ERR_NOT_FOUND;
    }

    return (err);
}

/**---------------------------------------------------------------------------
 * Assert an IRQ for an upcall event.
 *
 * @param[in] Pointer to the channel.
 * @param[in] dev Type of the device generating the event. At present
 * primary, repair IS and dpchan natively can generate events.
 * @param[in] id Id of the input stream which generates the event.
 * @param[in] reason The local event which is asserting the IRQ. 
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if either a
 * previous interrupt for the same device is pending, or if a new IRQ is
 * successfully sent to the control-plane. Even if sending the IRQ to the
 * control-plane fails, the event is still cached in the reason register 
 * associated with the IRQ which can be polled by the control-plane at
 * a subsequent stage. Therefore, callers may ignore the return code.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dpchan_tx_upcall_ev (vqec_dpchan_t *chan, 
                          vqec_dp_upcall_device_t dev, 
                          vqec_dp_isid_t id,
                          vqec_dp_upcall_irq_reason_code_t reason)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_upcall_irq_event_t event;
    vqec_dp_irq_desc_t *irq;

    if (!chan || 
        (dev < VQEC_DP_UPCALL_DEV_MIN) ||
        (dev > VQEC_DP_UPCALL_DEV_MAX) ||
        (reason >= VQEC_DP_UPCALL_REASON_MAX)) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    /* sa_ignore {the dev param is validated above} ARRAY_BOUNDS(1) */
    irq = &chan->irq_desc[dev - 1];
    irq->in_events++;
    
    if (!irq->pending) {
        memset(&event, 0, sizeof(event));
        event.chid = chan->id;
        event.cp_handle = chan->cp_handle;
        event.chan_generation_num = chan->generation_id;
        event.device = dev;
        event.device_id = id;
        event.upcall_generation_num = ++chan->msg_generation_num;

        if (vqec_dp_ipcserver_tx_upcall_irq(&event)) {
            
            /* successfully enQ'd the IRQ to the control-plane. */
            irq->pending = TRUE;
            irq->sent_irq++;

        } else {

            irq->dropped_irq++;
            err = VQEC_DP_ERR_INTERNAL;
        }
    }

    /* cache event in the local cause register. */
    irq->cause |= VQEC_DP_UPCALL_MAKE_REASON(reason);

    return (err);
}


/**---------------------------------------------------------------------------
 * Guts of the implementation for acknowledging an IRQ that was asserted by
 * the datplane for a device.
 *
 * ASSUMPTION: "device" parameter is in valid range.
 * @param[in] device Device type associated with the IRQ line.
 * @param[in] is_id Device identifier associated with the IRQ line.
 * @param[out] resp IRQ response data.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
UT_STATIC vqec_dp_error_t
vqec_dp_chan_ack_device_irq (vqec_dpchan_t *chan, 
                             vqec_dp_upcall_device_t device,
                             vqec_dp_streamid_t is_id, 
                             vqec_dp_irq_ack_resp_t *resp)
{
    vqec_dp_irq_desc_t *irq;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (!chan || !resp) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    if ((device != VQEC_DP_UPCALL_DEV_DPCHAN) &&
        (is_id == VQEC_DP_STREAMID_INVALID)) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    /* 
     * sa_ignore {the method is invoked *only* from ack_upcall_irq
     * below and thus the device parameter is bounded} ARRAY_BOUNDS(1). 
     */
    irq = &chan->irq_desc[device - 1];
    resp->irq_reason_code = irq->cause;
    irq->cause = 0;
    irq->pending = FALSE;
    irq->ack++;
    if (!resp->irq_reason_code) {
        /* There are no pending IRQs - spurious acknowledgement */
        irq->spurious_ack++;
        return (VQEC_DP_ERR_NOPENDINGIRQ);
    }

    switch (device) {
    case VQEC_DP_UPCALL_DEV_DPCHAN:       /* NCSI upcall */
#if HAVE_FCC
        if (VQEC_DP_UPCALL_REASON_ISSET(resp->irq_reason_code,
                                        VQEC_DP_UPCALL_REASON_CHAN_RCC_NCSI)) {
            err = vqec_dp_chan_get_rcc_ncsi_info(chan, 
                                                 &resp->response.chan_dev.ncsi_data);
        } else {
            err = VQEC_DP_ERR_OK;  /* rcc abort or gen sync upcall */
        }

        if (VQEC_DP_UPCALL_REASON_ISSET(resp->irq_reason_code,
                            VQEC_DP_UPCALL_REASON_CHAN_FAST_FILL_DONE)) {
            err = vqec_dp_chan_get_fast_fill_info(chan, 
                            &resp->response.chan_dev.fast_fill_data);
        } else {
            err = VQEC_DP_ERR_OK;  /* rcc abort or gen sync upcall */
        }
#endif
        if (VQEC_DP_UPCALL_REASON_ISSET(resp->irq_reason_code,
                            VQEC_DP_UPCALL_REASON_CHAN_FEC_UPDATE)) {
            err = vqec_dp_chan_get_fec_update_info(chan, 
                            &resp->response.chan_dev.fec_update_data);
        } else {
            err = VQEC_DP_ERR_OK;  /* rcc abort or gen sync upcall */
        }
        break;

    case VQEC_DP_UPCALL_DEV_RTP_PRIMARY:  /* RTP source event upcall */
    case VQEC_DP_UPCALL_DEV_RTP_REPAIR:
        err = vqec_dp_chan_rtp_input_stream_src_get_table(
            is_id,
            &resp->response.rtp_dev.src_table);
        break;

    default:
        err = VQEC_DP_ERR_INTERNAL;
        VQEC_DP_ASSERT_FATAL(FALSE, "Impossible code path");
        break;
    }

    return (err);
}


/**---------------------------------------------------------------------------
 * Acknowledge a pending IRQ upcall event (IPC) for a particular device.
 *
 * @param[in] chanid Channel identifier of the channel which generated IRQ.
 * @param[in] device Device type associated with the IRQ line.
 * @param[in] device_id Device identifier associated with the IRQ line.
 * @param[out] resp IRQ response data.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_ack_upcall_irq (vqec_dp_chanid_t chanid,
                             vqec_dp_upcall_device_t device,
                             vqec_dp_streamid_t device_id, 
                             vqec_dp_irq_ack_resp_t *resp)
{
    vqec_dpchan_t *chan;
    vqec_dp_error_t err = VQEC_DP_ERR_INVALIDARGS;

    if ((chanid == VQEC_DP_CHANID_INVALID) || !resp) {
        return (err);
    }
    
    chan = vqec_dp_chanid_to_ptr(chanid);
    if (!chan) {
        return (err);
    }

    memset(resp, 0, sizeof(*resp));
    resp->device_id = device_id;
 
    switch (device) {
    case VQEC_DP_UPCALL_DEV_RTP_PRIMARY:
        if (device_id == chan->prim_is) {
            err = vqec_dp_chan_ack_device_irq(chan, 
                                              device, chan->prim_is, resp);
        }
        break;
        
    case VQEC_DP_UPCALL_DEV_RTP_REPAIR:
        if (device_id == chan->repair_is) {
            err = vqec_dp_chan_ack_device_irq(chan, 
                                              device, chan->repair_is, resp);
        }
        break;

    case VQEC_DP_UPCALL_DEV_DPCHAN:
        err = vqec_dp_chan_ack_device_irq(chan, 
                                          device, chan->id, resp); 
        break;

    default:
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             " ", 
                             "Invalid device in IRQ acknowledgment");
        break;
    }
    
    return (err);
}


/**---------------------------------------------------------------------------
 * Poll all channel devices for pending IRQ events. This method is used *only*
 * used when message loss is detected on a channel. It is envisaged that  
 * message loss will be detected under extraordinary circumstances, where
 * the kernel will run out of memory.
 *
 * @param[in] chanid Channel identifier of the channel which generated IRQ.
 * @param[out] poll Events that may have been pending for all
 * devices on the channel.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if the input
 * arguments / channel object are valid. If there is any error in polling a
 * particular device, it is returned in the ack_error of the response for
 * that particular device.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_poll_upcall_irq (vqec_dp_chanid_t chanid,
                              vqec_dp_irq_poll_t *poll)
{
    vqec_dpchan_t *chan;
    vqec_dp_error_t err;

    if ((chanid == VQEC_DP_CHANID_INVALID) || !poll) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }
    
    chan = vqec_dp_chanid_to_ptr(chanid);
    if (!chan) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    /* 
     * Poll all devices in succession - If there is an error on one of the
     * devices, the next device is polled.
     */
    memset(poll, 0, sizeof(*poll));
    if (chan->prim_is != VQEC_DP_INVALID_ISID) {
        err = vqec_dp_chan_ack_device_irq(
            chan, 
            VQEC_DP_UPCALL_DEV_RTP_PRIMARY, 
            chan->prim_is, 
            &poll->response[VQEC_DP_UPCALL_DEV_RTP_PRIMARY - 1]);

        if (err != VQEC_DP_ERR_OK &&
            err != VQEC_DP_ERR_NOPENDINGIRQ) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                 vqec_dpchan_print_name(chan),
                                 " ", 
                                 "Error in poll of primary IS device");
        }
    } else {
        err = VQEC_DP_ERR_OK;
    }
    poll->response[VQEC_DP_UPCALL_DEV_RTP_PRIMARY - 1].ack_error = err;
    poll->response[VQEC_DP_UPCALL_DEV_RTP_PRIMARY - 1].device_id = 
        chan->prim_is;

    if (chan->repair_is != VQEC_DP_INVALID_ISID) {
        err = vqec_dp_chan_ack_device_irq(
            chan, 
            VQEC_DP_UPCALL_DEV_RTP_REPAIR, 
            chan->repair_is,
            &poll->response[VQEC_DP_UPCALL_DEV_RTP_REPAIR - 1]);

        if (err != VQEC_DP_ERR_OK &&
            err != VQEC_DP_ERR_NOPENDINGIRQ) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                 vqec_dpchan_print_name(chan),
                                 " ", 
                                 "Error in poll of repair IS device");
        }
    } else {
        err = VQEC_DP_ERR_OK;
    }
    poll->response[VQEC_DP_UPCALL_DEV_RTP_REPAIR - 1].ack_error = err; 
    poll->response[VQEC_DP_UPCALL_DEV_RTP_REPAIR - 1].device_id = 
        chan->repair_is; 

    err = vqec_dp_chan_ack_device_irq(
        chan, 
        VQEC_DP_UPCALL_DEV_DPCHAN, 
        chan->id,
        &poll->response[VQEC_DP_UPCALL_DEV_DPCHAN - 1]);

    if (err != VQEC_DP_ERR_OK &&
        err != VQEC_DP_ERR_NOPENDINGIRQ) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             " ", 
                             "Error in poll of native channel device");
    }
    poll->response[VQEC_DP_UPCALL_DEV_DPCHAN - 1].ack_error = err;

    poll->chid = chanid;
    poll->cp_handle = chan->cp_handle;
    poll->chan_generation_num = chan->generation_id;
    poll->upcall_generation_num = chan->msg_generation_num;

    return (VQEC_DP_ERR_OK);
}


/**---------------------------------------------------------------------------
 * Eject a (nat) packet on the repair session to the control-plane via a upcall
 * socket. A packet header is built which will be appended to the packet.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] pak Pointer to the packet.
 * @param[in] id Input stream id on which the packet is received.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if the packet
 * is successfully enqueued to the socket.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dpchan_eject_pak (vqec_dpchan_t *chan, vqec_dp_isid_t id, 
                       vqec_pak_t *pak)
{
    vqec_dp_pak_hdr_t pak_hdr;

    if (!chan || !pak) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }
    
    memset(&pak_hdr, 0, sizeof(pak_hdr));

    pak_hdr.dp_chanid = chan->id;
    pak_hdr.cp_handle = chan->cp_handle;
    pak_hdr.is_id = id;
    pak_hdr.rx_timestamp = pak->rcv_ts;
    pak_hdr.src_addr = pak->src_addr.s_addr;
    pak_hdr.src_port = pak->src_port;
    pak_hdr.length = vqec_pak_get_content_len(pak);

    chan->pak_ejects++;
    if (!vqec_dp_ipcserver_eject_pak(&pak_hdr,
                                     pak)) { 
        chan->pak_eject_errors++;
        return (VQEC_DP_ERR_INTERNAL);
    }
    
    return (VQEC_DP_ERR_OK);
}


/**************************************************************************
 * Histogram support
 */

/**
 * Create a histogram within the DP channel module.
 *
 * @param[in]  hist         - specifies histogram to create
 * @param[in]  ranges       - array of bucket ranges, sorted lowest to highest
 *             num_ranges   - number of bucket ranges desired
 *             title        - histogram title to display
 */
vqec_dp_error_t
vqec_dp_chan_hist_create (vqec_dp_hist_t hist,
                          int32_t *ranges, uint32_t num_ranges, char *title)
{
    char pool_name[ZONE_MAX_NAME_LEN + 1];

    if ((hist < 0) || (hist >= VQEC_DP_HIST_MAX) || !ranges || !num_ranges) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }
 
    snprintf(pool_name, ZONE_MAX_NAME_LEN + 1, "chan_hist_%i", hist);
    s_dpchan_module.hist_pool[hist] =
        zone_instance_get_loc(pool_name,
                              O_CREAT,
                              vam_hist_calc_size(num_ranges+1, FALSE),
                              1, NULL, NULL);
    if (!s_dpchan_module.hist_pool[hist]) {
        return (VQEC_DP_ERR_NO_RESOURCE);
    }
    s_dpchan_module.hist[hist] =
        (vam_hist_type_t *) zone_acquire(s_dpchan_module.hist_pool[hist]);
    if (!s_dpchan_module.hist[hist]) {
        (void) zone_instance_put(s_dpchan_module.hist_pool[hist]);
        s_dpchan_module.hist_pool[hist] = NULL;
        return (VQEC_DP_ERR_NO_RESOURCE);
    }
    if (vam_hist_create_ranges(
            s_dpchan_module.hist[hist],
            ranges,
            num_ranges,
            title)) {
        zone_release(s_dpchan_module.hist_pool[hist], 
                     s_dpchan_module.hist[hist]);
        s_dpchan_module.hist[hist] = NULL;
        (void) zone_instance_put(s_dpchan_module.hist_pool[hist]);
        s_dpchan_module.hist_pool[hist] = NULL;
        return (VQEC_DP_ERR_INTERNAL);
    }
    return (VQEC_DP_ERR_OK);
}

/**
 * Logs a data point for the join-delay histogram.
 *
 * @param[in] msec_delay Value observed for a channel change delay,
 *                       starting with the request of a join and concluding
 *                       upon the receipt of the first primary stream packet
 */
void
vqec_dp_chan_hist_join_delay_add_data (uint32_t msec_delay)
{
    if (!s_dpchan_module.init_done ||
        !s_dpchan_module.hist[VQEC_DP_HIST_JOIN_DELAY] ||
        vam_hist_add(s_dpchan_module.hist[VQEC_DP_HIST_JOIN_DELAY],
                     msec_delay)) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "Failed to log join delay data to histogram");
    }
}

/**
 * Logs a data point to the output scheduling histogram.
 *
 * Typical usage is that the caller passes the current time when output
 * schduling happens.  The delta between the passed time and the time of last
 * invocation (stored internally) is computed, and the interval is recorded
 * as a data point in the histogram.
 *
 * @param[in] now   current time
 */
void
vqec_dp_chan_hist_outputsched_add_data (abs_time_t now)
{
    rel_time_t delta;

    /* Abort if no histogram is available */
    if (!s_dpchan_module.init_done ||
        !s_dpchan_module.hist[VQEC_DP_HIST_OUTPUTSCHED]) {
        goto done;
    }

    /* If first time logging data, just record the current time */
    if (TIME_CMP_A(eq, s_dpchan_module.last_outputsched_time, ABS_TIME_0)) {
        goto done;
    }

    delta = TIME_SUB_A_A(now, s_dpchan_module.last_outputsched_time);
    (void)vam_hist_add(s_dpchan_module.hist[VQEC_DP_HIST_OUTPUTSCHED],
                       TIME_GET_R(msec, delta));

done:
    s_dpchan_module.last_outputsched_time = now;
}

/**
 * Gets a snapshot of a histogram which may be used by the control plane 
 * (e.g. for display purposes).
 *
 * @param[in]  hist               - specifies histogram to retrieve
 * @param[out] target             - buffer into which histogram is copied
 * @param[in]  target_max_buckets - max size of histogram to be retrieved
 * @param[out] vqec_dp_error_t    - VQEC_DP_ERR_OK or failure code
 */
vqec_dp_error_t
vqec_dp_chan_hist_get(vqec_dp_hist_t hist,
                      vqec_dp_histogram_data_t *target)
{
    if ((hist < 0) || (hist >= VQEC_DP_HIST_MAX)
        || !target) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    if (!s_dpchan_module.init_done) {
        return VQEC_DP_ERR_SHUTDOWN;
    } 

    if (!s_dpchan_module.hist[hist]) {
        return VQEC_DP_ERR_INTERNAL;
    }
        
    vam_hist_copy((vam_hist_type_t *)target, 
                  VQEC_DP_HIST_MAX_BUCKETS,
                  s_dpchan_module.hist[hist]);

    return VQEC_DP_ERR_OK;
}

/**
 * Clear a histogram's data
 *
 * @param[in]  hist             - specifies histogram to be cleared
 * @parampout] vqec_dp_error_t  - VQEC_DP_ERR_OK or failure code
 */
vqec_dp_error_t
vqec_dp_chan_hist_clear (vqec_dp_hist_t hist)
{
    if ((hist < 0) || (hist >= VQEC_DP_HIST_MAX)) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    if (s_dpchan_module.init_done && s_dpchan_module.hist[hist]) {
        vam_hist_clear(s_dpchan_module.hist[hist]);
    }
    return VQEC_DP_ERR_OK;
}

/**
 * Enable/Disable measurement logging for the output scheduling histogram
 *
 * @param[in]  enable            - TRUE to enable logging, FALSE to disable
 * @param[out] vqec_dp_error_t   - VQEC_DP_ERR_OK or failure code
 */
RPC vqec_dp_error_t
vqec_dp_chan_hist_outputsched_enable (boolean enable)
{
    if (!s_dpchan_module.init_done) {
        return (VQEC_DP_ERR_SHUTDOWN);
    }

    s_dpchan_module.hist_outputsched_enabled = enable;
    s_dpchan_module.last_outputsched_time = ABS_TIME_0;
    return (VQEC_DP_ERR_OK);
}

static inline boolean
vqec_dp_chan_hist_outputsched_is_enabled_internal (void)
{
    if (s_dpchan_module.init_done) {
        return (s_dpchan_module.hist_outputsched_enabled);
    } else {
        return (FALSE);
    }  
}

/**
 * Returns whether measurement logging is enabled for the output
 * scheduling histogram.
 *
 * @param[out] boolean           - TRUE if monitoring is enabled,
 *                                 FALSE if it is disabled 
 */
vqec_dp_error_t
vqec_dp_chan_hist_outputsched_is_enabled (boolean *enabled)
{
    if (enabled) {
        *enabled = vqec_dp_chan_hist_outputsched_is_enabled_internal();
        return (VQEC_DP_ERR_OK);
    }

    return (VQEC_DP_ERR_INVALIDARGS);
}


/****************************************************************************
 * General methods exported to the CP.
 */


/**---------------------------------------------------------------------------
 * Get a channel's failover status.
 *
 * @param[in] chan Pointer to the channel.
 * @param[out] stats Pointer to the cp-dp API stats structure.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_get_failover_status (vqec_dpchan_t *chan, 
                                 vqec_dp_chan_failover_status_t *status)
{
    vqec_dp_chan_rtp_primary_input_stream_t *iptr;
    vqec_pak_t *pak;

    memset(status, 0, sizeof(*status));

    if (chan->prim_is == VQEC_DP_INVALID_ISID) {
        goto done;
    }

    iptr = (vqec_dp_chan_rtp_primary_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(chan->prim_is);
    if (!iptr) {
        goto done;
    }

    if (iptr->failover_rtp_src_entry) {
        status->failover_paks_queued = iptr->failover_paks_queued;
        status->failover_src = iptr->failover_rtp_src_entry->key;
        pak = VQE_TAILQ_FIRST(&iptr->failoverq_lh);
        if (pak) {
            status->failover_queue_head_seqnum = 
                ((rtptype *)pak->rtp)->sequence;
        }
        pak = VQE_TAILQ_LAST(&iptr->failoverq_lh, vqe_failoverq_head_);
        if (pak) {
            status->failover_queue_tail_seqnum = 
                ((rtptype *)pak->rtp)->sequence;
        }
    }

    status->prev_src_last_rcv_ts = chan->prev_src_last_rcv_ts;
    if (iptr->rtp_recv.src_list.pktflow_src) {
        status->curr_src_first_rcv_ts = 
            iptr->rtp_recv.src_list.pktflow_src->info.first_rx_time;
    } else {
        status->curr_src_first_rcv_ts = ABS_TIME_0;
    }

done:
    return;
}

/**---------------------------------------------------------------------------
 * Get channel input-stream statistics on-demand from the cp.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] cumulative Boolean flag to retrieve cumulative stats
 * @param[out] stats Pointer to the cp-dp API stats structure.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_get_input_stats (vqec_dpchan_t *chan, vqec_dp_chan_stats_t *stats,
                             boolean cumulative)
{
    vqec_dp_chan_rtp_is_stats_t rtp_stats;
    vqec_dp_chan_rtp_input_stream_t *iptr;
    vqec_dp_is_status_t is_status;

    memset(stats, 0, sizeof(*stats));
    memset(&rtp_stats, 0, sizeof(rtp_stats));
    memset(&is_status, 0, sizeof(is_status));

    if (chan->prim_is != VQEC_DP_INVALID_ISID) {
        iptr = (vqec_dp_chan_rtp_input_stream_t *)
            vqec_dp_chan_input_stream_id_to_ptr(chan->prim_is);

        if (iptr) {
            if (MCALL(iptr, get_rtp_stats, &rtp_stats, cumulative) 
                == VQEC_DP_ERR_OK) {
                stats->udp_rx_paks = rtp_stats.udp_paks;
                stats->rtp_rx_paks = rtp_stats.rtp_paks;
                stats->primary_rx_paks = rtp_stats.rtp_paks;
                stats->primary_sim_drops = rtp_stats.sim_drops;
                stats->primary_sm_early_drops = rtp_stats.sm_drops;
                stats->primary_rtp_rx_drops = rtp_stats.rtp_parse_drops;
                stats->primary_udp_rx_drops = rtp_stats.udp_mpeg_sync_drops;
                stats->primary_pcm_drops_total = rtp_stats.pakseq_drops;
                stats->primary_pcm_drops_late =
                    rtp_stats.ses_stats.late_packet_counter;
                stats->primary_pcm_drops_duplicate =
                    rtp_stats.ses_stats.duplicate_packet_counter;
            }
            if (vqec_dp_chan_input_stream_get_status(
                    (vqec_dp_chan_input_stream_t *)iptr, &is_status, 
                    cumulative) == 
                VQEC_DP_STREAM_ERR_OK) {
                stats->rx_paks = is_status.stats.packets;
            }
        }
    }
    if (chan->repair_is != VQEC_DP_INVALID_ISID) {
        iptr = (vqec_dp_chan_rtp_input_stream_t *)
            vqec_dp_chan_input_stream_id_to_ptr(chan->repair_is); 

        memset(&rtp_stats, 0, sizeof(rtp_stats));
        memset(&is_status, 0, sizeof(is_status));
        
        if (iptr) {
            if (MCALL(iptr, get_rtp_stats, &rtp_stats, cumulative) 
                == VQEC_DP_ERR_OK) {
                stats->rtp_rx_paks += rtp_stats.rtp_paks;
                stats->repair_rx_paks = rtp_stats.rtp_paks;
                stats->repair_sim_drops = rtp_stats.sim_drops;
                stats->repair_sm_early_drops = rtp_stats.sm_drops;
                stats->repair_rtp_rx_drops =
                    rtp_stats.rtp_parse_drops + rtp_stats.fil_drops;
                stats->repair_pcm_drops_total = rtp_stats.pakseq_drops;
                stats->repair_pcm_drops_late =
                    rtp_stats.ses_stats.late_packet_counter;
                stats->repair_pcm_drops_duplicate =
                    rtp_stats.ses_stats.duplicate_packet_counter;
            }
            if (vqec_dp_chan_input_stream_get_status(
                    (vqec_dp_chan_input_stream_t *)iptr, &is_status, 
                    cumulative)
                    == VQEC_DP_STREAM_ERR_OK) {
                stats->rx_paks += is_status.stats.packets;
            }
        }
    }
}

/**---------------------------------------------------------------------------
 * Clear input-stream statistics on-demand from the cp.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_clear_input_stats (vqec_dpchan_t *chan)
{
    vqec_dp_chan_rtp_input_stream_t *iptr;

    if (chan->prim_is != VQEC_DP_INVALID_ISID) {
        iptr = (vqec_dp_chan_rtp_input_stream_t *)
            vqec_dp_chan_input_stream_id_to_ptr(chan->prim_is);
        
        if (iptr) {
            MCALL(iptr, clear_rtp_stats);
        }
    }

    if (chan->repair_is != VQEC_DP_INVALID_ISID) {
        iptr = (vqec_dp_chan_rtp_input_stream_t *)
            vqec_dp_chan_input_stream_id_to_ptr(chan->repair_is); 
        
        if (iptr) {
            MCALL(iptr, clear_rtp_stats);
        }
    }    
}


/**---------------------------------------------------------------------------
 * Get the sequence and gap logs for a dataplane channels. These are 
 * maintained by the pcm. (IPC). All the output pointer are optional, i.e., a 
 * caller may specify them as null.
 *
 * @param[in] chanid Identifier of a dp channel.
 * @param[out] in_log Pointer to input pcm gap log for output.
 * @param[out] out_log Pointer to output pcm gap log for output.
 * @param[out] seq_log Pointer to sequence log for output.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_get_seqlogs (vqec_dp_chanid_t chanid,
                          vqec_dp_gaplog_t *in_log,
                          vqec_dp_gaplog_t *out_log,
                          vqec_dp_seqlog_t *seq_log)
{
    vqec_dpchan_t *chan;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_log_t *log;

    chan = vqec_dp_chanid_to_ptr(chanid);
    if (!chan) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    log = vqec_pcm_gap_log_ptr(&chan->pcm);
    if (in_log) {
        vqec_log_to_dp_gaplog(&log->input_gaps, in_log);
    }
    if (out_log) {
        vqec_log_to_dp_gaplog(&log->output_gaps, out_log);
    }
    if (seq_log) {
        vqec_log_to_dp_seqlog(log, seq_log);
    }
    
    return (err);
}


/**---------------------------------------------------------------------------
 * Get status and statistics for a dataplane channel, and it's sub-components.
 * (IPC). All pointer arguments are optional, i.e., it is not an error for them
 * to be null.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[out] pcm_s Pointer to pcm status data for output.
 * @param[out] nll_s Pointer to nll status data for output.
 * @param[out] fec_s Pointer to fec status data for output.
 * @param[out] chan_s Pointer to chan stats for output.
 * @param[out] failover_s Pointer to failover status for output
 * @param[in] cumulative Boolean flag to retrieve cumulative stats
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_get_status (vqec_dp_chanid_t chanid,
                         vqec_dp_pcm_status_t *pcm_s,
                         vqec_dp_nll_status_t *nll_s,
                         vqec_dp_fec_status_t *fec_s,
                         vqec_dp_chan_stats_t *chan_s, 
                         vqec_dp_chan_failover_status_t *failover_s,
                         boolean cumulative)
{
    vqec_dpchan_t *this;

    this = vqec_dp_chanid_to_ptr(chanid);
    if (!this) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    if (pcm_s) {
        vqec_pcm_get_status(&this->pcm, pcm_s, cumulative);
    }
    if (nll_s) {
        vqec_nll_get_info(&this->pcm.osched.nll, nll_s);
    }
    if (fec_s) {
        vqec_fec_get_status(this->fec, fec_s, cumulative);
    }
    if (chan_s) {
        vqec_dpchan_get_input_stats(this, chan_s, cumulative);
    }
    if (failover_s) {
        vqec_dpchan_get_failover_status(this, failover_s);
    }

    return (VQEC_DP_ERR_OK);
}


#define INFINITY_U64 (-1)
vqec_dp_error_t
vqec_dp_chan_get_stats_tr135_sample (vqec_dp_chanid_t chanid,
                                     vqec_ifclient_stats_channel_tr135_sample_t 
                                     *stats)
{
    vqec_dpchan_t *this;

    this = vqec_dp_chanid_to_ptr(chanid);
    if (!this) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    if (stats) {
        stats->maximum_loss_period = 
            this->pcm.stats.tr135.mainstream_stats.sample_stats_after_ec.maximum_loss_period;
        stats->minimum_loss_distance = 
            this->pcm.stats.tr135.mainstream_stats.sample_stats_after_ec.minimum_loss_distance;

    
    /* At this point we also reset the sample statistics */
        this->pcm.stats.tr135.mainstream_stats.sample_stats_before_ec.maximum_loss_period =  0;
        this->pcm.stats.tr135.mainstream_stats.sample_stats_before_ec.minimum_loss_distance = INFINITY_U64;
        this->pcm.stats.tr135.mainstream_stats.sample_stats_after_ec.maximum_loss_period =  0;
        this->pcm.stats.tr135.mainstream_stats.sample_stats_after_ec.minimum_loss_distance = INFINITY_U64;
    } else {
        return (VQEC_DP_ERR_INVALIDARGS);
    }
    return (VQEC_DP_ERR_OK);
}


/**---------------------------------------------------------------------------
 * Set TR-135 configuration
 *
 * @param[in] chanid Identifier of the channel.
 * @param[in] params Pointer to tr-135 status data
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_set_tr135_params (vqec_dp_chanid_t chanid, 
                               vqec_ifclient_tr135_params_t *params)
{
    vqec_dpchan_t *this;

    this = vqec_dp_chanid_to_ptr(chanid);
    if (!this || !params) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    vqec_pcm_set_tr135_params(&this->pcm, params);

    return (VQEC_DP_ERR_OK);
}

/* 
 * Function to log the ER enable timestamp in the PCM 
 * 
 * @param[in] chanid Identifier of the channel
 */
vqec_dp_error_t
vqec_dp_set_pcm_er_en_flag (vqec_dp_chanid_t chanid)
{
    vqec_dpchan_t *this;

    this = vqec_dp_chanid_to_ptr(chanid);
    if (!this) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }
    vqec_pcm_set_er_en_ts(&this->pcm);
    return (VQEC_DP_ERR_OK);
}


/**---------------------------------------------------------------------------
 * Clear statistics for a channel. (IPC)
 *
 * @param[in] chanid Identifier of the channel.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_clear_stats (vqec_dp_chanid_t chanid)
{
    vqec_dpchan_t *this;

    this = vqec_dp_chanid_to_ptr(chanid);
    if (!this) {
        return (VQEC_DP_ERR_INVALIDARGS);
    }

    vqec_pcm_counter_clear(&this->pcm);
    vqec_fec_counter_clear(this->fec);
    vqec_dpchan_clear_input_stats(this);

    return (VQEC_DP_ERR_OK);
}


/**---------------------------------------------------------------------------
 * Collect gaps from pcm: "more" will be set to true if there are additional
 * gaps to report, and the output gap buffer wasn't large enough to retrieve
 * all the gaps (IPC).
 *
 * @param[in] chanid Channel identifier of the channel which generated IRQ.
 * @param[out] gapbuf Gap buffer.
 * @param[out] more True when there are additional gaps to report.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_get_gap_report (vqec_dp_chanid_t chanid,
                             vqec_dp_gap_buffer_t *gapbuf, 
                             boolean *more)
{
    vqec_dpchan_t *chan;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (!gapbuf || !more) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    chan = vqec_dp_chanid_to_ptr(chanid);
    if (!chan) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    if (!vqec_pcm_gap_get_gaps(&chan->pcm, gapbuf, more)) {
        err = VQEC_DP_ERR_INTERNAL;
        return (err);
    }

    return (err);
}


/****************************************************************************
 * RCC related methods.
 *
 * HELPFUL DEFINITIONS: 
 *  -- OSN: Repair stream original sequence number.
 *  -- APP first repair sequence filter: A filter on the repair IS which will force 
 *     packets to be queued in the IS, until the OSN specified by this filter
 *     is seen; the sequence number is the first repair sequence specified by
 *     the APP packet.
 *  -- OSN delta filter: A filter which *only* allows packets on the repair IS
 *     that have the same delta (difference) between their OSNs and the 
 *     repair session sequence number. This filter is present during a RCC burst.
 *  -- IS HoldQ: A queue maintained by repair IS, whereby all packets are
 *     held in that Q until the packet specified by the APP first repair 
 *     sequence filter is seen.
 */

#if HAVE_FCC
static inline boolean
vqec_dpchan_is_rcc_aborted (vqec_dpchan_t *chan)
{
    return (chan->rcc_in_abort);
}

static inline void
vqec_dpchan_set_rcc_aborted (vqec_dpchan_t *chan)
{
    chan->rcc_in_abort = TRUE;
    /**
     * set the fast fill time to zero
     * to stop the fast fill
     */
    chan->pcm.fast_fill_time = REL_TIME_0;

    /**
     * enable rcc post abort process
     * to drop remaining received RCC
     * burst packets 
     */
    chan->pcm.rcc_post_abort_process = TRUE;
}
#endif 

/**---------------------------------------------------------------------------
 * If the channel RCC state machine is in a post-ER enabled state.
 * 
 * @param[in] chan Pointer to the channel - must be null-checked prior to call.
 * @param[out] boolean True if the state-machine is in a state in which ER 
 * should be enabled (wait_end_burst / abort / success states). It always
 * returns true for the non-RCC case including if RCC is not enabled.
 *---------------------------------------------------------------------------*/ 
boolean
vqec_dpchan_sm_state_post_er_enable (vqec_dpchan_t *chan)
{
    if (!chan) {
        return (FALSE);
    }

#ifdef HAVE_FCC
    if (chan->state >= VQEC_DP_SM_STATE_WAIT_END_BURST) {
        return (TRUE);
    } else {
        return (FALSE);
    }
#else /* HAVE_FCC */

    return (TRUE);
#endif /* HAVE_FCC */
}

/**
 * Records information about the arrival of a channel's first primary packet.
 *
 * @param[in] chan - channel which has not (previously) received a primary pkt
 * @param[in] pak  - first primary packet to be accepted for chan.
 *                   It is the caller's responsibility to ensure the packet
 *                    is the first and is acceptable (e.g. it's RTP header
 *                    has been validated, the channel is ready to accept it
 *                    if doing RCC, etc.)
 * @param[in] cur_time - current time 
 */
void
vqec_dpchan_pak_event_record_first_primary (vqec_dpchan_t *chan,
                                            vqec_pak_t *pak,
                                            abs_time_t cur_time)
{
    rel_time_t join_latency;

    chan->first_primary_ts = pak->rcv_ts;
    chan->first_primary_seq = pak->seq_num;
    chan->first_primary_ev_ts = cur_time;
    chan->rx_primary = TRUE;
    /*
     * Log the interval between the receipt of first primary pkt
     * just received and the time it was requested via join.
     * In the event the first primary pkt's timestamp precedes
     * the join (e.g. because another application on the host
     * was already joined to that group), the join delay is
     * considered to be zero.
     */
    if (TIME_CMP_A(le, chan->first_primary_ts, chan->join_issue_time)){
        join_latency = REL_TIME_0;
    } else {
        join_latency = TIME_SUB_A_A(chan->first_primary_ts,
                                    chan->join_issue_time);
    }
    vqec_dp_chan_hist_join_delay_add_data(TIME_GET_R(msec, join_latency));
}

/**---------------------------------------------------------------------------
 * This method is used to process / deliver a packet event to the RCC 
 * state machine during the time when a RCC burst is active. It allows the
 * state machine to filter incoming packets and transition it's state. The
 * method is invoked from the RTP input stream(s) receive method.
 *
 * @param[in] chan Pointer to the channel.
 * @param[in] ev Packet event type, i.e., primary / repair.
 * @param[in] pak Pointer to the packet.
 * @param[in] cur_time Current absolute time.
 * @param[out] boolean True if the packet if the should be processed by
 * the RTP IS, false if it should be dropped. In thus current implementation
 * true is the only returned value.
 *---------------------------------------------------------------------------*/ 
boolean
vqec_dpchan_pak_event_update_state (vqec_dpchan_t *chan, 
                                    vqec_dpchan_rx_pak_ev_t ev, 
                                    vqec_pak_t *pak, abs_time_t cur_time)
{
#ifdef HAVE_FCC
    if (IS_ABS_TIME_ZERO(cur_time)) {
        cur_time = get_sys_time();
    }
    
    switch (ev) {
    case VQEC_DP_CHAN_RX_PRIMARY_PAK:
        if (!chan->rx_primary) {
            vqec_dpchan_pak_event_record_first_primary(chan, pak, cur_time);

            VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                          "Channel %s: RCC first primary sequence %lu, "
                          "rxd' at time %llu\n",
                          vqec_dpchan_print_name(chan),
                          chan->first_primary_seq, chan->first_primary_ts); 

            /* sa_ignore {failure handled in state-machine} IGNORE_RETURN(2) */
            (void)vqec_dp_sm_deliver_event(chan, 
                                           VQEC_DP_SM_EVENT_PRIMARY, NULL); 
        }
        chan->last_primary_pak_ts = cur_time;
        break;

    case VQEC_DP_CHAN_RX_REPAIR_PAK:
        if (!chan->rx_repair && chan->process_first_repair) {
            /**
             * Only process the first repair packet indicated at
             * APP. If out of order, wait for first repair 
             */ 
            chan->first_repair_seq = pak->seq_num;
            chan->first_repair_ts = pak->rcv_ts;
            chan->first_repair_ev_ts = cur_time;
            chan->rx_repair = TRUE;
            chan->process_first_repair = FALSE;
            
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                          "Channel %s, RCC first repair sequence %lu, "
                          "rxd' at time %llu\n",
                          vqec_dpchan_print_name(chan),
                          chan->first_repair_seq, chan->first_repair_ts);
            
            /* sa_ignore {failure handled in state-machine} IGNORE_RETURN(2) */
            (void)vqec_dp_sm_deliver_event(chan, 
                                           VQEC_DP_SM_EVENT_REPAIR, NULL);
        }
        chan->last_repair_pak_ts = cur_time;
        break;
        
    default:
        break;
    }
#endif /* HAVE_FCC */

    return (TRUE);
}

/**---------------------------------------------------------------------------
 * Get RCC status and counters (IPC).
 * 
 * @param[in] chanid Channel identifier of the channel
 * @param[in] cumulative Boolean flag to retrieve cumulative status
 * @param[out] rcc_data Pointer to the RCC status data block.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
#ifdef HAVE_FCC
vqec_dp_error_t
vqec_dp_chan_get_rcc_status (vqec_dp_chanid_t chanid,
                             vqec_dp_rcc_data_t *rcc_data)
{
    vqec_dpchan_t *chan;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (!rcc_data) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    
    chan = vqec_dp_chanid_to_ptr(chanid);
    if (!chan) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    memset(rcc_data, 0, sizeof(*rcc_data));

    rcc_data->join_snap = chan->pcm_join_snap;
    rcc_data->prim_snap = chan->pcm_firstprim_snap;
    rcc_data->er_en_snap = chan->pcm_er_en_snap;
    rcc_data->end_burst_snap = chan->pcm_endburst_snap;
    rcc_data->rcc_enabled = chan->rcc_enabled;
    rcc_data->rcc_in_abort = chan->rcc_in_abort;
    if (chan->rcc_enabled) {
        rcc_data->rcc_success = (chan->state == VQEC_DP_SM_STATE_FIN_SUCCESS);
    } else {
        rcc_data->rcc_success = FALSE;
    }
    rcc_data->event_mask = chan->event_mask;
    rcc_data->first_repair_ev_ts = chan->first_repair_ev_ts;
    rcc_data->join_issue_ev_ts = chan->join_issue_time;
    rcc_data->burst_end_time = chan->burst_end_time;
    rcc_data->first_primary_ev_ts = chan->first_primary_ev_ts;
    rcc_data->first_primary_seq = chan->first_primary_seq;
    rcc_data->er_en_ev_ts = chan->er_enable_time;

    rcc_data->first_repair_ts = chan->first_repair_ts;
    rcc_data->first_primary_ts = chan->first_primary_ts;

    if (TIME_CMP_A(le, chan->first_primary_ts, chan->join_issue_time)){
        rcc_data->join_latency = REL_TIME_0;
    } else {
        rcc_data->join_latency = TIME_SUB_A_A(chan->first_primary_ts,
                                              chan->join_issue_time);
    }

    vqec_pcm_cap_outp_rcc_data(&chan->pcm, &rcc_data->outp_data);
    vqec_dp_sm_copy_log(chan, &rcc_data->sm_log);
    (void)strlcpy(rcc_data->fail_reason, vqec_dp_sm_fail_reason(chan), 
                  sizeof(rcc_data->fail_reason));
    
    return (err);
}
#endif /* HAVE_FCC */

/**---------------------------------------------------------------------------
 * Flush the repair input stream at RCC abort or when the RCC burst finishes.
 * The APP first sequence number filter, the OSN sequence delta filter and
 * the hold Q are all cleared.
 * 
 * @param[in] chan Pointer to channel.
 *---------------------------------------------------------------------------*/ 
#ifdef HAVE_FCC
UT_STATIC void
vqec_dpchan_reset_repair_stream_rcc_state (vqec_dpchan_t *chan)
{
    vqec_dp_chan_rtp_repair_input_stream_t *iptr;

    iptr = (vqec_dp_chan_rtp_repair_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(chan->repair_is);
    if (!iptr) {
        VQEC_DP_SYSLOG_PRINT(ERROR, 
                             "repair input stream not found");
        return;
    }
        
    vqec_dp_chan_rtp_repair_input_stream_clear_firstseq_fil(iptr);
    vqec_dp_chan_rtp_repair_input_stream_flush_holdq(iptr);
}

#endif /* HAVE_FCC */


/**---------------------------------------------------------------------------
 * Issue a join on the FEC input stream(s) when RCC + FEC are enabled during 
 * a channel change. It is assumed that the OS that the FEC IS's are 
 * connected to is in a bind-reserve state.
 *
 * * If the commit fails we have no recourse except to log an error: this will
 *   only happen because of a programming error or state-corruption. *
 * 
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
#ifdef HAVE_FCC
static inline void
vqec_dpchan_join_fec (vqec_dpchan_t *chan)
{
    vqec_dp_chan_input_stream_t *iptr;
    
    if (chan->fec_enabled && !chan->joined_fec) {

        if (chan->fec0_is != VQEC_DP_INVALID_ISID) {
            iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->fec0_is);
            if (!iptr) {
                VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                     vqec_dpchan_print_name(chan),
                                     " ",
                                     "Join failed, fec0 input stream not found");
                return;
            }

            if (!MCALL(iptr, bind_commit)) {
                VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                     vqec_dpchan_print_name(chan),
                                     " ",
                                     "Join failed, fec0 commit on IS failed");
            } 
        }

        if (chan->fec1_is != VQEC_DP_INVALID_ISID) {
            iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->fec1_is);
            if (!iptr) {
                VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                     vqec_dpchan_print_name(chan),
                                     " ",
                                     "Join failed, fec1 input stream not found");
                return;
            }

            if (!MCALL(iptr, bind_commit)) {
                VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                     vqec_dpchan_print_name(chan),
                                     " ",
                                     "Join failed, fec1 commit on IS failed");
            }    
        }

        chan->joined_fec = TRUE;
    }
}
#endif /* HAVE_FCC */


/**---------------------------------------------------------------------------
 * Issue a join on the primary input stream when RCC is enabled during a 
 * channel change. It is assumed that the OS that the IS is connected to is
 * in a bind-reserve state. 
 *
 * * If the commit fails we have no recourse except to log an error: this will
 *   only happen because of a programming error or state-corruption. *
 * 
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
#ifdef HAVE_FCC
static inline void
vqec_dpchan_join_primary (vqec_dpchan_t *chan)
{
    vqec_dp_chan_input_stream_t *iptr;

    if (!chan->joined_primary) {

        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->prim_is);
        if (!iptr) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                 vqec_dpchan_print_name(chan),
                                 " ",
                                 "Join failed, primary input stream not found");
            return;
        }
        
        if (!MCALL(iptr, bind_commit)) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                 vqec_dpchan_print_name(chan),
                                 " ",
                                 "Join failed, primary commit on IS failed");
        }

        chan->joined_primary = TRUE;
        chan->join_issue_time = get_sys_time();
    }
}
#endif /* HAVE_FCC */


/**---------------------------------------------------------------------------
 * Notification from the RCC state-machine. The RCC burst has completed, 
 * and it is time to remove the OSN sequence number delta filter that exists
 * on the repair IS; Also join the FEC streams, if FEC is enabled, and capture
 * a pcm state snapshot.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void 
vqec_dpchan_rcc_success_notify (vqec_dpchan_t *chan)
{
#ifdef HAVE_FCC
    vqec_dpchan_join_fec(chan);
    vqec_dpchan_reset_repair_stream_rcc_state(chan);
    vqec_pcm_cap_state_snapshot(&chan->pcm, &chan->pcm_endburst_snap);

#endif /* HAVE_FCC */
}


/**---------------------------------------------------------------------------
 * Notification from the RCC state-machine. The time-to-join epoch is now:
 * issue a join on the primary stream; A state snapshot of pcm is also
 * captured.
 * 
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_dpchan_rcc_join_notify (vqec_dpchan_t *chan)
{
#ifdef HAVE_FCC
    vqec_dpchan_join_primary(chan);
    vqec_pcm_cap_state_snapshot(&chan->pcm, &chan->pcm_join_snap);

#endif /* HAVE_FCC */
}


/**---------------------------------------------------------------------------
 * Notification from the RCC state-machine. First primary pak seen; send
 * NCSI event to the control-plane. A state snapshot of pcm is also captured.
 * 
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_dpchan_rcc_ncsi_notify (vqec_dpchan_t *chan)
{
#ifdef HAVE_FCC
    if (vqec_dpchan_tx_upcall_ev(chan, 
                                 VQEC_DP_UPCALL_DEV_DPCHAN,
                                 0, 
                                 VQEC_DP_UPCALL_REASON_CHAN_RCC_NCSI) !=
        VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             " ",
                             "Upcall ncsi event post failure");
    }
    vqec_pcm_cap_state_snapshot(&chan->pcm, &chan->pcm_firstprim_snap);

#endif /* HAVE_FCC */
}


/**---------------------------------------------------------------------------
 * Called by the RCC state-machine time to enable ER has arrived, or when
 * RCC has been aborted.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_dpchan_rcc_er_en_notify (vqec_dpchan_t *chan)
{
#ifdef HAVE_FCC
    vqec_dp_chan_input_stream_t *iptr;

    /* 
     * "Poll" the repair IS at this point to flush all packets that may have
     * accumulated in the socket or input Q's since the last poll interval.
     */
    if (chan->repair_is != VQEC_DP_INVALID_ISID) {
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->repair_is);
        if (iptr) {
            (void)MCALL(iptr, poll_data);            
        }
    }
    
    /* 
     * "Poll" the primary IS at this point to flush all packets that may have
     * accumulated in the socket or input Q's since the last poll interval.
     */
    if (chan->prim_is != VQEC_DP_INVALID_ISID) {
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->prim_is);
        if (iptr) {
            (void)MCALL(iptr, poll_data);
        }
    }

    /* Inform pcm that ER is now enabled */
    vqec_pcm_notify_rcc_en_er(&chan->pcm);

    /* reset repair stream's rcc state */
    vqec_dpchan_reset_repair_stream_rcc_state(chan);
    
    /* capture pcm state snapshot at ER enable time. */
    vqec_pcm_cap_state_snapshot(&chan->pcm, &chan->pcm_er_en_snap);
    chan->er_enable_time = get_sys_time();

#endif /* HAVE_FCC */
}


/**---------------------------------------------------------------------------
 * RCC abort actions that are targeted for multiple channel sub-components.
 *  -- Notify pcm that RCC has been aborted.
 *  -- Join the primary and FEC streams.
 *  -- Enable error-repair.
 *  -- Reset the repair session so that it no longer filters for the first APP
 *  sequence number.
 *
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
#ifdef HAVE_FCC
UT_STATIC void
vqec_dpchan_abort_rcc_doactions (vqec_dpchan_t *chan)
{
    /* Inform pcm that RCC has been aborted */
    vqec_pcm_notify_rcc_abort(&chan->pcm);
    vqec_dpchan_join_primary(chan);
    vqec_dpchan_join_fec(chan);
    vqec_dpchan_rcc_er_en_notify(chan);
    vqec_dpchan_reset_repair_stream_rcc_state(chan);
}
#endif /* HAVE_FCC */


/**---------------------------------------------------------------------------
 * RCC abort notification from the dp state machine. A upcall notification is
 * sent to the control-plane, and then dataplane local actions corresponding
 * to the abort are performed.
 * 
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_dpchan_rcc_abort_notify (vqec_dpchan_t *chan)
{
#ifdef HAVE_FCC
    if (!vqec_dpchan_is_rcc_aborted(chan)) {
        if (vqec_dpchan_tx_upcall_ev(chan, 
                                     VQEC_DP_UPCALL_DEV_DPCHAN,
                                     0, 
                                     VQEC_DP_UPCALL_REASON_CHAN_RCC_ABORT) !=
            VQEC_DP_ERR_OK) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                 vqec_dpchan_print_name(chan),
                                 " ",
                                 "Upcall abort event post failure");
        }
        
        vqec_dpchan_abort_rcc_doactions(chan);
        vqec_dpchan_set_rcc_aborted(chan);        
    }
#endif /* HAVE_FCC */
}

/**---------------------------------------------------------------------------
 * Fastfill done notification from the dpchan.  A upcall notification is
 * sent to the control-plane.
 * 
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_fast_fill_done_notify (vqec_dpchan_t *chan)
{
#ifdef HAVE_FCC

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                  "[FASTFILL] Channel %s, Done, notify CP @ time = %llu\n",
                  vqec_dpchan_print_name(chan), get_sys_time());

    if (vqec_dpchan_tx_upcall_ev(chan, 
                                 VQEC_DP_UPCALL_DEV_DPCHAN,
                                 0, 
                                 VQEC_DP_UPCALL_REASON_CHAN_FAST_FILL_DONE) !=
        VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             " ",
                             "Upcall fast fill event post failure");
    }

#endif /* HAVE_FCC */
}

/**---------------------------------------------------------------------------
 * RCC burst done notification from the dpchan.  An upcall notification is
 * sent to the control-plane to signify that the RCC burst and all associated
 * repair packets have been received.
 * 
 * @param[in] chanid ID of the channel.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_rcc_burst_done_notify (uint32_t chanid)
{
#ifdef HAVE_FCC
    vqec_dpchan_t *dpchan;

    if (chanid == VQEC_DP_CHANID_INVALID) {
        return;
    }
    
    dpchan = vqec_dp_chanid_to_ptr(chanid);
    if (!dpchan) {
        return;
    }

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                  "Channel %s, burst (and assoc. repairs) done; "
                  "notify CP @ time = %llu\n",
                  vqec_dpchan_print_name(dpchan), get_sys_time());

    if (vqec_dpchan_tx_upcall_ev(dpchan, 
                                 VQEC_DP_UPCALL_DEV_DPCHAN,
                                 0, 
                                 VQEC_DP_UPCALL_REASON_CHAN_BURST_DONE) !=
        VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(dpchan),
                             " ",
                             "Upcall burst done event post failure");
    }

#endif /* HAVE_FCC */
}

/**---------------------------------------------------------------------------
 * Primary input inactive notification from the dpchan.  A upcall notification is
 * sent to the control-plane.
 * 
 * @param[in] chan Pointer to the channel.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_primary_inactive_notify (vqec_dpchan_t *chan)
{

    if (vqec_dpchan_tx_upcall_ev(chan, 
                                 VQEC_DP_UPCALL_DEV_DPCHAN,
                                 0, 
                                 VQEC_DP_UPCALL_REASON_CHAN_PRIM_INACTIVE) !=
        VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             " ",
                             "Upcall primary inactive post failure");
    }

}

/**---------------------------------------------------------------------------
 * Notification from the dpchan that the FEC L and D values have been updated.
 * An upcall notification is sent to the control-plane.
 * 
 * @param[in] chanid ID of the channel.
 *---------------------------------------------------------------------------*/ 
void
vqec_dpchan_fec_update_notify (uint32_t chanid)
{
    vqec_dpchan_t *dpchan;

    if (chanid == VQEC_DP_CHANID_INVALID) {
        return;
    }
    
    dpchan = vqec_dp_chanid_to_ptr(chanid);
    if (!dpchan) {
        return;
    }

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_FEC,
                  "FEC L and D values updated, Channel %s, "
                  "notify CP @ time = %llu\n",
                  vqec_dpchan_print_name(dpchan), get_sys_time());

    if (vqec_dpchan_tx_upcall_ev(dpchan, 
                                 VQEC_DP_UPCALL_DEV_DPCHAN,
                                 0, 
                                 VQEC_DP_UPCALL_REASON_CHAN_FEC_UPDATE) !=
        VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(dpchan),
                             " ",
                             "Upcall FEC update event post failure");
    }
}


/**---------------------------------------------------------------------------
 * Abort an RCC operation upon command from the control-plane. (IPC)
 * 
 * @param[in] chanid Identifier of the channel.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if the abort
 * is successfully issued to the channel.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_abort_rcc (vqec_dp_chanid_t chanid)
{
#ifdef HAVE_FCC
    vqec_dpchan_t *chan;
    vqec_dp_error_t err = VQEC_DP_ERR_INVALIDARGS;

    if (chanid == VQEC_DP_CHANID_INVALID) {
        return (err);
    }
    
    chan = vqec_dp_chanid_to_ptr(chanid);
    if (!chan) {
        return (err);
    }

    if (!vqec_dpchan_is_rcc_aborted(chan)) {
        vqec_dpchan_set_rcc_aborted(chan);
        /* sa_ignore {state-machine to abort} IGNORE_RETURN(2) */
        (void)vqec_dp_sm_deliver_event(chan, 
                                       VQEC_DP_SM_EVENT_ABORT, NULL);
        vqec_dpchan_abort_rcc_doactions(chan);
    }

#endif /* HAVE_FCC */

    return (VQEC_DP_ERR_OK);
}

/**---------------------------------------------------------------------------
 * Formal implementation of APP processing in the dataplane.
 * 
 * @param[in] chan Pointer to the dp channel.
 * @param[in] app Pointer to the APP contents and it's associated data.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if the dataplane
 * is able to proceed with the rapid channel change request.
 *---------------------------------------------------------------------------*/ 
#ifdef HAVE_FCC
UT_STATIC vqec_dp_error_t
vqec_dp_chan_process_app_internal (vqec_dpchan_t *chan,
                                   vqec_dp_rcc_app_t *app, char *tspkt, 
                                   uint32_t len)
{
    vqec_pak_t *pak;
    vqec_dp_chan_rtp_repair_input_stream_t *iptr;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    uint32_t num_paks, total_udp_paks, num_udp_paks_per_app;
    uint8_t* copy_from;
    uint16_t copy_len;
    int32_t pkt_mpegts_buflen;

    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                  "Channel %s, received APP data from CP\n",
                  vqec_dpchan_print_name(chan));
    
    if (chan->repair_is == VQEC_DP_INVALID_ISID) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan), 
                             " ",
                             "repair IS identifier is invalid");
        vqec_dpchan_set_rcc_aborted(chan);
        vqec_dpchan_abort_rcc_doactions(chan);
        /* sa_ignore {state-machine to abort} IGNORE_RETURN(2) */
        (void)vqec_dp_sm_deliver_event(chan, 
                                       VQEC_DP_SM_EVENT_INTERNAL_ERR, NULL);        
        err = VQEC_DP_ERR_NOT_FOUND;
        return (err);
    }

    iptr = (vqec_dp_chan_rtp_repair_input_stream_t *)
        vqec_dp_chan_input_stream_id_to_ptr(chan->repair_is);

    if ((!iptr) ||
        (!VQE_TAILQ_EMPTY(&chan->app_paks)) ||
        ((app->tspkt_len % MP_MPEG_TSPKT_LEN) != 0)) {

        if (!iptr) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                 vqec_dpchan_print_name(chan),
                                 " ",
                                 "repair IS pointer is not valid");
        } else if (!VQE_TAILQ_EMPTY(&chan->app_paks)) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                 vqec_dpchan_print_name(chan),
                                 " ",
                                 "app_paks TAILQ is not empty");
        } else if ((app->tspkt_len % MP_MPEG_TSPKT_LEN) != 0) {
            VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                 vqec_dpchan_print_name(chan),
                                 " ",
                                 "TS Packet buffer len not a multiple of "
                                 "MP_MPEG_TSPKT_LEN bytes");
        }
        vqec_dpchan_set_rcc_aborted(chan);
        vqec_dpchan_abort_rcc_doactions(chan);
        /* sa_ignore {state-machine to abort} IGNORE_RETURN(2) */
        (void)vqec_dp_sm_deliver_event(chan, 
                                       VQEC_DP_SM_EVENT_INTERNAL_ERR, NULL);
        err = VQEC_DP_ERR_NOT_FOUND;
        return (err);
    }

    /* total number of RTP (or UDP) packets we'll produce from the APP */
    num_paks = (app->tspkt_len / MP_MPEG_TSPKT_LEN) / MP_NUM_TSPAKS_PER_DP_PAK;
    total_udp_paks = num_paks;

    /*
     * Number of RTP (or UDP) packets per APP
     *
     * This computation assumes that app->tspkt_len is already a multiple
     * of (7*188) bytes, and therefore fits perfectly into an integer number
     * of UDP packets.
     */
    num_udp_paks_per_app = num_paks / s_dpchan_module.app_paks_per_rcc;

    chan->act_min_backfill = app->act_min_backfill;
    chan->dt_earliest_join = app->dt_earliest_join;
    chan->dt_repair_end = app->dt_repair_end;
    chan->er_holdoff_time = app->er_holdoff_time;
    chan->start_seq_num = app->start_seq_num;
    chan->first_repair_deadline = app->first_repair_deadline;
    chan->act_backfill_at_join = app->act_backfill_at_join;

    /*
     * The following equation gives us how much data we actually need to fast
     * fill for memory optimized bursts:
     *
     * fast_fill_time = fast_fill_stream_amount + (fast_fill_stream_amount / e)
     *   where
     *     fast_fill_stream_amount = actual_backfill - minimum_backfill
     *     e = actual_fill_at_join / dt_earliest_join
     *
     * Plugging in these expressions for fast_fill_stream_amount and e gives:
     *
     * fast_fill_time =
     *   (actual_backfill - minimum_backfill) + 
     *   (((actual_backfill - minimum_backfill) * dt_earliest_join) /
     *    actual_fill_at_join)
     *
     * Notes:
     *    1.  This equation can only be used of the following conditions are
     *        true:
     *        a.  memory optimized bursts are enabled (max_fastfill > 0)
     *        b.  conservative mode RCC is in use (er_holdoff_time > 0)
     *    2.  If actual_fill_at_join is 0, then fast_fill_time cannot be
     *        calculated, and is instead assumed to be 0.
     *    3.  The product of ((actual_backfill - minimum_backfill) *
     *        dt_earliest_join is assumed to fit in a uint32_t.
     *    4.  If the above formula is changed, the max TS age computation 
     *        on the VQE-S (vam/era/era_rcc.c) must also be changed.
     */
    if (TIME_CMP_R(gt, chan->max_fastfill, REL_TIME_0) &&
        TIME_CMP_R(gt, chan->er_holdoff_time, REL_TIME_0)) {
        if (TIME_CMP_R(eq, REL_TIME_0, chan->act_backfill_at_join)) {
            chan->pcm.fast_fill_time = REL_TIME_0;
        } else {
            uint64_t ff_stream_amt;

            ff_stream_amt = TIME_GET_R(msec,
                                   TIME_SUB_R_R(app->act_min_backfill,
                                                chan->min_backfill));
            chan->pcm.fast_fill_time =
                TIME_MK_R(msec,
                          ff_stream_amt +
                          (((uint32_t)(ff_stream_amt *
                                       TIME_GET_R(msec,
                                                  chan->dt_earliest_join))) /
                           (uint32_t)TIME_GET_R(msec,
                                                chan->act_backfill_at_join)));
        }

        VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                      "app->fast_fill_time = %lld\n"
                      "chan->max_fastfill = %lld\n"
                      "chan->dt_earliest_join = %lld\n"
                      "chan->dt_repair_end = %lld\n"
                      "app->act_min_backfill = %lld\n"
                      "app->act_backfill_at_join = %lld\n"
                      "chan->min_backfill = %lld\n"
                      "chan->max_backfill = %lld\n"
                      "fastfilltime (%lld) (will be limited to range [0, "
                      "max_fastfill]) =\n"
                      " (act_backfill (%lld) - min_backfill (%lld)) +\n"
                      " (((act_backfill (%lld) - min_backfill (%lld)) * "
                      "dt_earliest_join (%lld)) /\n"
                      "  act_backfill_at_join (%lld))\n",
                      TIME_GET_R(msec, app->fast_fill_time),
                      TIME_GET_R(msec, chan->max_fastfill),
                      TIME_GET_R(msec, chan->dt_earliest_join),
                      TIME_GET_R(msec, chan->dt_repair_end),
                      TIME_GET_R(msec, app->act_min_backfill),
                      TIME_GET_R(msec, app->act_backfill_at_join),
                      TIME_GET_R(msec, chan->min_backfill),
                      TIME_GET_R(msec, chan->max_backfill),
                      TIME_GET_R(msec, chan->pcm.fast_fill_time),
                      TIME_GET_R(msec, chan->act_min_backfill),
                      TIME_GET_R(msec, chan->min_backfill),
                      TIME_GET_R(msec, chan->act_min_backfill),
                      TIME_GET_R(msec, chan->min_backfill),
                      TIME_GET_R(msec, chan->dt_earliest_join),
                      TIME_GET_R(msec, chan->act_backfill_at_join));

        /* enforce the limits of fast_fill_time */
        if (TIME_CMP_R(lt, chan->pcm.fast_fill_time, REL_TIME_0)) {
            chan->pcm.fast_fill_time = REL_TIME_0;
        } else if (TIME_CMP_R(gt, chan->pcm.fast_fill_time,
                              chan->max_fastfill)) {
            chan->pcm.fast_fill_time = chan->max_fastfill;
        }
    } else {
        chan->pcm.fast_fill_time = app->fast_fill_time;

        VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                      "fastfilltime (%lld) = appfastfill (%lld)\n",
                      TIME_GET_R(msec, chan->pcm.fast_fill_time),
                      TIME_GET_R(msec, app->fast_fill_time));
    }
    chan->pcm.max_fastfill = chan->max_fastfill;
    chan->pcm.min_backfill = chan->min_backfill;
    chan->pcm.dt_repair_end = chan->dt_repair_end;

    /* Allocate dp packet buffers for sending APP TLV packets to decoder. */
    copy_from = (uint8_t*)tspkt;
    copy_len  = app->tspkt_len;
    pkt_mpegts_buflen = MP_NUM_TSPAKS_PER_DP_PAK * MP_MPEG_TSPKT_LEN;  
    while (copy_len > 0) {

        pak = vqec_pak_alloc_with_particle();
        if ((!pak) || 
            (pak->alloc_len < pkt_mpegts_buflen)) {

            /* free pak buffers allocated so far, if any */
            vqec_pak_t * pak_p, *pak_p_next;
            VQE_TAILQ_FOREACH_SAFE(pak_p, &chan->app_paks, ts_pkts,
                                   pak_p_next) {
                VQE_TAILQ_REMOVE(&chan->app_paks, pak_p, ts_pkts);
                vqec_pak_free(pak_p);
            }

            if (!pak) {
                VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                     vqec_dpchan_print_name(chan),
                                     " ",
                                     "unable to allocate packet for APP");
            } else {
                VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                                     vqec_dpchan_print_name(chan),
                                     " ",
                                     "dp pak buffers too small for APP");
            }
            vqec_dpchan_set_rcc_aborted(chan);
            vqec_dpchan_abort_rcc_doactions(chan);
            /* sa_ignore {state-machine to abort} IGNORE_RETURN(2) */
            (void)vqec_dp_sm_deliver_event(chan, 
                                           VQEC_DP_SM_EVENT_INTERNAL_ERR, NULL);
            err = VQEC_DP_ERR_NO_RESOURCE;
            /* disable fast fill here, since something is wrong */
            chan->pcm.fast_fill_time = REL_TIME_0;
            vqec_pcm_log_tr135_overrun(&chan->pcm, 1);
            return (err);
        }

        /* 
         * NOTE: This packet has been created as: APP-TLV-->TS-APP, and is a
         * Transport stream packet, and it has no RTP header. If strip_rtp is
         * FALSE, we need to construct an RTP header on this packet. However,
         * we do not construct the RTP header here, because we do not have 
         * the correct SSRC and Timestamp information with us. We therefore 
         * postpone this header construction till when we receive the first 
         * repair packet.
         */
        pak->type = VQEC_PAK_TYPE_APP;
        pak->seq_num = (chan->start_seq_num - num_paks) & 0xFFFF;
        
        /*
         * No mapping from APP's sequence number to the session's
         * sequence number is required
         */
        pak->seq_num = vqec_seq_num_nearest_to_rtp_seq_num(
            vqec_pcm_get_last_rx_seq_num(&chan->pcm), pak->seq_num);
        pak->rcv_ts = get_sys_time();

        /* set the packet delay from the app_cpy_delay value */
        if (!(num_paks % num_udp_paks_per_app) && num_paks != total_udp_paks) {
            (void)vqec_dp_get_app_cpy_delay(&pak->app_cpy_delay);
        }

        pak->mpeg_payload_offset = 0;
        vqec_pak_set_content_len(pak, pkt_mpegts_buflen);

        memcpy(vqec_pak_get_head_ptr(pak), copy_from, pkt_mpegts_buflen);
        copy_from += pkt_mpegts_buflen;
        copy_len  -= pkt_mpegts_buflen;

        /* add this packet to channel's app_paks queue */
        VQE_TAILQ_INSERT_TAIL(&chan->app_paks, pak, ts_pkts);
        /* 
         * We will insert the APP packet without filling in the timestamp
         * and the SSRC, and revisit this packet when the first repair
         * packet arrives.
         */

        /* decrement the number of paks remaining counter */
        num_paks--;
    }

    /* Now deliver the RCC start event to the state-machine. */
    if (!vqec_dp_sm_deliver_event(chan, 
                                  VQEC_DP_SM_EVENT_START_RCC, NULL)) { 
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(chan),
                             " ",
                             "finite-state-machine startup failed");
        /* the state-machine posts an abort if there is an internal error. */
        err = VQEC_DP_ERR_INTERNAL;
        /* disable fast fill here, since something is wrong */
        chan->pcm.fast_fill_time = REL_TIME_0;
        return (err); 
    }

    /* 
     * Install a sequence number filter on the repair IS: if the holdQ has
     * packets those will be immediately processed.
     */
    if (!vqec_dpchan_is_rcc_aborted(chan)) {
        vqec_dp_chan_rtp_repair_input_stream_set_firstseq_fil(
            iptr,
            chan->start_seq_num & 0xFFFF);
    }
    return (err);
}
#endif /* HAVE_FCC */

/**---------------------------------------------------------------------------
 * Gather the PAT for a DP channel.
 *
 * param[in]  chanid    DP channel identifier.
 * param[out] pat       Pointer to the PAT buffer
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_get_pat (vqec_dp_chanid_t chanid,
                      vqec_dp_psi_section_t *pat)
{
    vqec_dp_error_t status = VQEC_DP_ERR_OK;
    vqec_dpchan_t *dpchan;

    if (!pat) {
        status = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    dpchan = vqec_dp_chanid_to_ptr(chanid);
    if (!dpchan) {
        status = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    VQEC_DP_ASSERT_FATAL(dpchan->pmt_len <= MP_PSISECTION_LEN,
                         "PAT len too large for buffer!");

    memcpy(&pat->data, dpchan->pat_buf, dpchan->pat_len);
    pat->len = dpchan->pat_len;

done:
    return (status);
}

/**---------------------------------------------------------------------------
 * Gather the PMT for a channel.
 *
 * param[in]  chanid    Channel identifier.
 * param[out] pmt_pid   PID of the PMT.
 * param[out] pmt       Pointer to the PMT buffer
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_get_pmt (vqec_dp_chanid_t chanid,
                      uint16_t *pmt_pid,
                      vqec_dp_psi_section_t *pmt)
{
    vqec_dp_error_t status = VQEC_DP_ERR_OK;
    vqec_dpchan_t *dpchan;

    if (!pmt) {
        status = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    dpchan = vqec_dp_chanid_to_ptr(chanid);
    if (!dpchan) {
        status = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    VQEC_DP_ASSERT_FATAL(dpchan->pmt_len <= MP_PSISECTION_LEN,
                         "PMT len too large for buffer!");

    memcpy(&pmt->data, dpchan->pmt_buf, dpchan->pmt_len);
    pmt->len = dpchan->pmt_len;

    if (pmt_pid) {
        *pmt_pid = dpchan->pmt_pid;
    }
    
done:
    return (status);
}

/**---------------------------------------------------------------------------
 * Gather the PCR value for a channel.
 *
 * param[in]  chanid    Channel identifier.
 * param[out] pcr       PCR value.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_get_pcr (vqec_dp_chanid_t chanid,
                      uint64_t *pcr)
{
    vqec_dpchan_t *dpchan;
    vqec_dp_error_t status = VQEC_DP_ERR_OK;

    dpchan = vqec_dp_chanid_to_ptr(chanid);
    if (!dpchan || !pcr) {
        status = VQEC_DP_ERR_INVALIDARGS;
        return (status);
    }

    *pcr = dpchan->pcr_val;

    return (status);
}

/**---------------------------------------------------------------------------
 * Gather the PTS value for a channel.
 *
 * param[in]  chanid    Channel identifier.
 * param[out] pts       PTS value.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_chan_get_pts (vqec_dp_chanid_t chanid,
                      uint64_t *pts)
{
    vqec_dpchan_t *dpchan;
    vqec_dp_error_t status = VQEC_DP_ERR_OK;

    dpchan = vqec_dp_chanid_to_ptr(chanid);
    if (!dpchan || !pts) {
        status = VQEC_DP_ERR_INVALIDARGS;
        return (status);
    }

    *pts = dpchan->pts_val;

    return (status);
}


/**---------------------------------------------------------------------------
 * Copy the PAT to a cached buffer in the DP channel.
 *
 * param[in] sect_ptr   Pointer to the PAT buffer
 * param[in] sect_len   Length of the PAT
 * param[in] data       Callback data
 *---------------------------------------------------------------------------*/ 
static 
void vqec_dp_chan_pat_callback (uint8_t *sect_ptr, 
                                uint32_t sect_len,
                                void *data)
{
    vqec_dpchan_t *dpchan = (vqec_dpchan_t *)data;
    VQEC_DP_ASSERT_FATAL(dpchan, "dpchan");

    if (sect_len <= sizeof(dpchan->pat_buf)) {
        memcpy(dpchan->pat_buf, sect_ptr, sect_len);
        dpchan->pat_len = sect_len;
    }
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC, 
               "DP channel %s, pat_callback, len=%d\n", 
               vqec_dpchan_print_name(dpchan), sect_len);
}


/**---------------------------------------------------------------------------
 * Copy the PMT to a cached buffer in the DP channel.
 *
 * param[in] sect_ptr   Pointer to the PMT buffer
 * param[in] sect_len   Length of the PMT
 * param[in] sect_pid   PID of the PMT
 * param[in] data       Callback data
 *---------------------------------------------------------------------------*/ 
static 
void vqec_dp_chan_pmt_callback (uint8_t *sect_ptr, 
                                uint32_t sect_len,
                                uint16_t sect_pid,
                                void *data)
{
    vqec_dpchan_t *dpchan = (vqec_dpchan_t *)data;
    VQEC_DP_ASSERT_FATAL(dpchan, "dpchan");

    if (sect_len <= sizeof(dpchan->pmt_buf)) {
        memcpy(dpchan->pmt_buf, sect_ptr, sect_len);
        dpchan->pmt_len = sect_len;
        dpchan->pmt_pid = sect_pid;
    }
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
               "DP channel %s, pmt_callback, len=%d, pmt_pid=%d\n",
               vqec_dpchan_print_name(dpchan), sect_len, sect_pid);
}


/**---------------------------------------------------------------------------
 * Copy the PCR to a cached buffer in the DP channel.
 *
 * param[in] pcr_base   PCR base
 * param[in] pcr_ext    PCR ext
 * param[in] data       Callback data
 *---------------------------------------------------------------------------*/ 
static 
void vqec_dp_chan_pcr_callback (uint64_t pcr_base,
                                uint16_t pcr_ext,
                                void *data)
{
    vqec_dpchan_t *dpchan = (vqec_dpchan_t *)data;
    VQEC_DP_ASSERT_FATAL(dpchan, "dpchan");

    dpchan->pcr_val = pcr_base;
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC, 
               "DP channel %s, pcr_callback %llu\n",
               vqec_dpchan_print_name(dpchan), pcr_base);
}

/**---------------------------------------------------------------------------
 * Copy the PTS to a cached buffer in the DP channel.
 *
 * param[in] pts_base   PTS base
 * param[in] pts_ext    PTS ext
 * param[in] data       Callback data
 *---------------------------------------------------------------------------*/ 
static 
void vqec_dp_chan_pts_callback (uint64_t pts_base,
                                void *data)
{
    vqec_dpchan_t *dpchan = (vqec_dpchan_t *)data;
    VQEC_DP_ASSERT_FATAL(dpchan, "dpchan");

    dpchan->pts_val = pts_base;
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC, 
               "DP channel %s, pts_callback %llu\n",
               vqec_dpchan_print_name(dpchan), pts_base);
}

/**---------------------------------------------------------------------------
 * Process an APP packet from the control-plane. This will start-up the RCC
 * state machine, and will wait for the first repair sequence number from
 * the repair stream. (IPC)
 * 
 * @param[in] chanid Identifier of the channel.
 * @param[in] app Pointer to the APP contents and it's associated data.
 * @param[in] tsrap Pointer to the TSRAP TLV data.
 * @param[in] tsrap_len Length of the TSRAP TLV data.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK if the state 
 * machine is successfully started.
 *---------------------------------------------------------------------------*/ 
#ifdef HAVE_FCC
vqec_dp_error_t
vqec_dp_chan_process_app (vqec_dp_chanid_t chanid,
                          vqec_dp_rcc_app_t *app,
                          uint8_t *tsrap,
                          uint32_t tsrap_len)
{
    vqec_dpchan_t *dpchan;
    app_tlv_buf_t *tlvbuf;
    mp_tlv_buflist_t buf_list;
    mp_tlv_pidlist_t pidlist;
    mp_tlv_decodecb_t m_decode_cb;
    mp_tlv_mpegknobs_t *knobs;
    uint32_t tspak_len;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    dpchan = vqec_dp_chanid_to_ptr(chanid);
    if (!dpchan || !app || !tsrap) {
        err = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    tlvbuf = &s_dpchan_module.tlvbuf;
    knobs = &s_dpchan_module.knobs;

    /*---
     *  Translate tsrap data to mpeg cells.
     *---*/

     /* Buffer setup. */
    VQE_TAILQ_INIT(&vqe_tsrap_app_list_head);
    buf_list.tspkt_buf = tlvbuf->buf;
    buf_list.buf_len = tlvbuf->len;
    buf_list.buf_rem = tlvbuf->len;
    VQE_TAILQ_INSERT_TAIL(&vqe_tsrap_app_list_head, &buf_list, list_entry);

    /* callbacks */
    memset(&m_decode_cb, 0, sizeof(m_decode_cb));
    m_decode_cb.pat_cb = vqec_dp_chan_pat_callback;
    m_decode_cb.pmt_cb = vqec_dp_chan_pmt_callback;
    m_decode_cb.pcr_cb = vqec_dp_chan_pcr_callback;
    m_decode_cb.pts_cb = vqec_dp_chan_pts_callback;

    tspak_len = mp_tlv_to_tspkts(tsrap,
                                 tsrap_len,
                                 knobs,
                                 s_dpchan_module.app_paks_per_rcc,
                                 &m_decode_cb,
                                 &buf_list,
                                 dpchan,
                                 &pidlist);
    if (!tspak_len) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "App-TSRAP data does not fit in max packet bound");
        err = VQEC_DP_ERR_INVALID_APP;
        goto done;
    }

    /* cache resultant TS packet length in APP structure */
    app->tspkt_len = tspak_len;

    /* process APP */
    if (!vqec_dpchan_is_rcc_aborted(dpchan)) {
        err = vqec_dp_chan_process_app_internal(dpchan,
                                                app,
                                                tlvbuf->buf,
                                                tspak_len);
    }

done:
    return (err);
}

#endif /* HAVE_FCC */

/****************************************************************************
 * Channel creation and destruction.
 */

/**---------------------------------------------------------------------------
 * Destroy a dpchan instance.
 *
 * @param[in] chanid Identifier of the dpchan instance to be destroyed.
 *---------------------------------------------------------------------------*/ 
void 
vqec_dpchan_destroy (vqec_dp_chanid_t chanid)
{
    vqec_dpchan_t *chan;
    vqec_dp_chan_input_stream_t *iptr;

    if (!s_dpchan_module.init_done) {
        VQEC_DP_SYSLOG_PRINT(INVALIDARGS, __FUNCTION__);
        return;
    }
    
    chan = vqec_dp_chanid_to_ptr(chanid);
    if (!chan) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "dpchan instance not found");
        return;
    }

    /* stop dpchan ISs and OS */
    vqec_dpchan_stop_all_rtp_receivers(chan);
    vqec_dpchan_stop_os(chan);

    /* deinit the output scheduler */
    (void)vqec_dp_oscheduler_deinit(&chan->pcm.osched);

    /* delete any OS */
    (void)vqec_dpchan_del_os(chanid);

    /* deinit pcm */
    vqec_pcm_deinit(&chan->pcm);

    /* destroy fec */
    if (chan->fec) {
        vqec_fec_destroy(chan->fec);
    }

    /* deinit state machine */
#ifdef HAVE_FCC
    vqec_dpchan_set_rcc_aborted(chan);
    vqec_dp_sm_deinit(chan);
#endif /* HAVE_FCC */

    /* delete all RTP input streams */
    if (chan->prim_is != VQEC_DP_INVALID_ISID) {
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->prim_is); 
        if (iptr) {
            MCALL(iptr, destroy);
        }
    }
    if (chan->repair_is != VQEC_DP_INVALID_ISID) {
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->repair_is); 
        if (iptr) {
            MCALL(iptr, destroy);
        }
    }
    if (chan->fec0_is != VQEC_DP_INVALID_ISID) {
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->fec0_is); 
        if (iptr) {
            MCALL(iptr, destroy);
        }
    }
    if (chan->fec1_is != VQEC_DP_INVALID_ISID) {
        iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->fec1_is); 
        if (iptr) {
            MCALL(iptr, destroy);
        }
    }

    /* delete channel id. */
    id_delete(chanid, s_dpchan_module.id_table);
    s_dpchan_module.chan_cnt--;
    if (!s_dpchan_module.chan_cnt) {
        s_dpchan_module.last_outputsched_time = ABS_TIME_0;
    }
    VQE_TAILQ_REMOVE(&s_dpchan_module.chan_list, chan, le);

    zone_release (s_dpchan_module.chan_pool, chan);
}


/**---------------------------------------------------------------------------
 * If the FEC information of the channel (L, D, Annex) need to be updated,
 * return the value to CP and tell CP to do this. 
 *
 * @param[in] chanid Identifier of the dpchan instance.
 * @param[out] fec_info fec information returned here
 * @param[out] true if needed, otherwise false
 *---------------------------------------------------------------------------*/ 
boolean 
vqec_dpchan_fec_need_update (vqec_dp_chanid_t chanid, vqec_fec_info_t *fec_info)
{

    vqec_dpchan_t *chan = NULL;

    if (!s_dpchan_module.init_done || !fec_info) {
        VQEC_DP_SYSLOG_PRINT(INVALIDARGS, __FUNCTION__);
        return FALSE;
    }

    chan = vqec_dp_chanid_to_ptr(chanid);
    if (!chan) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "dpchan instance not found");
        return FALSE;
    }

    if (!chan->fec_enabled || !chan->fec ||
        !chan->fec->sending_order_detected) {
        return FALSE;
    }

    /* copy the newly calculated rtp_ts_pkt_time from PCM */
    chan->fec->fec_info.rtp_ts_pkt_time = chan->pcm.new_rtp_ts_pkt_time;

    if (memcmp(&chan->pcm.fec_info, &chan->fec->fec_info, 
               sizeof(vqec_fec_info_t))) {
        memset(fec_info, 0, sizeof(vqec_fec_info_t));
        memcpy(fec_info, &chan->fec->fec_info, 
               sizeof(vqec_fec_info_t));
        return TRUE;
    }
    return FALSE;
}



/**---------------------------------------------------------------------------
 * HELPER: Load the pcm parameters from the channel descriptor.
 *
 * @param[out] params Pcm parameters structure.
 * @param[in] desc Pointer to the dataplane channel descriptor.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_load_pcm_params (vqec_pcm_params_t *params, 
                             vqec_dp_chan_desc_t *desc,
                             vqec_dpchan_t *dpchan)
{
    memset(params, 0, sizeof(*params));
    params->dpchan = dpchan;
    params->er_enable = desc->en_repair;
    params->fec_enable = desc->en_fec0 || desc->en_fec1;
    params->rcc_enable = desc->en_rcc;
    params->avg_pkt_time = desc->avg_pkt_time;
    params->strip_rtp = desc->strip_rtp;
     memcpy(&params->fec_info, &desc->fec_info, sizeof(vqec_fec_info_t));
    params->fec_default_block_size = desc->fec_default_block_size;
    memcpy(&params->tr135_params, &desc->tr135_params, 
           sizeof(vqec_ifclient_tr135_params_t)); 

    if (desc->en_repair) {
        params->default_delay = desc->jitter_buffer;
        params->reorder_delay = desc->reorder_time;
    } else {
        /* 
         * If ER is disabled, set jitter buffer to reorder time, and
         * set reorder delay to 0. 
         */
        params->default_delay = desc->reorder_time;
        params->reorder_delay = REL_TIME_0;
    }
    params->repair_trigger_time = desc->repair_trigger_time;

#if HAVE_FCC
    /* set up pre-primary repair completion notification callback */
    params->pre_primary_repairs_done_cb = vqec_dpchan_rcc_burst_done_notify;
    params->pre_primary_repairs_done_data = dpchan->id;
#endif  /* HAVE_FCC */
}


/**---------------------------------------------------------------------------
 * HELPER: Load the fec parameters for FEC initialization.
 *
 * @param[out] params FEC parameters structure.
 * @param[in] desc Pointer to the dataplane channel descriptor.
 *---------------------------------------------------------------------------*/ 
UT_STATIC void
vqec_dpchan_load_fec_params (vqec_fec_params_t *params,
                             vqec_dp_chan_desc_t *desc)
{
    memset(params, 0, sizeof(*params));
    params->is_fec_enabled = TRUE;

    if (desc->en_fec1) {  /* assuming that if en_fec1 is TRUE, then en_fec0
                           * must also be TRUE (i.e. 2-D mode) */
        params->avail_streams = VQEC_FEC_STREAM_AVAIL_2D;
        params->fec_type_stream1 = VQEC_FEC_TYPE_XOR;
        params->fec_type_stream2 = VQEC_FEC_TYPE_XOR;
    } else {              /* 1-D mode */
        params->avail_streams = VQEC_FEC_STREAM_AVAIL_1D;
        params->fec_type_stream1 = VQEC_FEC_TYPE_XOR;
    }
}


/**---------------------------------------------------------------------------
 * Create a new channel instance. The method if successful returns the 
 * identifier of the channel. An Invalid identifier is returned in case of 
 * a failure. 
 *
 * @pararm[in] desc Pointer to the dataplane channel descriptor that is
 * filled in by the control-plane.
 * @param[out] vqec_dp_chanid_t Returns a valid channel on success, invalid
 * identifier on failure.
 *---------------------------------------------------------------------------*/ 

vqec_dp_chanid_t 
vqec_dpchan_create (vqec_dp_chan_desc_t *desc,
                    vqec_dp_input_stream_obj_t *dpchan_input_obj,
                    vqec_dp_os_instance_t *dpchan_os)
{
    vqec_dpchan_t *dpchan = NULL;
    vqec_pcm_params_t pcm_params;
    vqec_fec_params_t fec_params;
    boolean fec_enabled;
    char tmp[INET_ADDRSTRLEN];

    if (!s_dpchan_module.init_done) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "dpchan module is not initialized");
        goto bail;
    }
    if (!desc || !dpchan_input_obj || !dpchan_os) {
        VQEC_DP_SYSLOG_PRINT(INVALIDARGS, __FUNCTION__);
        goto bail;
    }

    dpchan = (vqec_dpchan_t *) zone_acquire(s_dpchan_module.chan_pool);
    if (!dpchan) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "failed to allocate dpchan instance");
        goto bail;
    }
    memset(dpchan, 0, sizeof(vqec_dpchan_t));

    dpchan->id = id_get(dpchan, s_dpchan_module.id_table);
    if (id_mgr_is_invalid_handle(dpchan->id)) { 
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "failed to allocate an identifier for dpchan");
        goto bail;
    }
    
    /*
     * NOTE: Each channel has a generation identifier, which is the 
     * pre-increment value of a global counter; the value of this counter
     * increments at each new channel creation. This generation identifier
     * helps the control-plane in discarding upcall messages that may
     * have been enQ'd by a dataplane channel prior to it's deletion. 
     */
    dpchan->generation_id = ++s_dpchan_module.generation_id;
    dpchan->cp_handle = desc->cp_handle;
    dpchan->is_multicast = 
        IN_MULTICAST(ntohl(desc->primary.filter.u.ipv4.dst_ip));
    dpchan->prev_src_last_rcv_ts = ABS_TIME_0;
    dpchan->er_enabled = desc->en_repair;
    dpchan->rcc_enabled = desc->en_rcc;
    dpchan->passthru = desc->passthru;
    dpchan->reorder_time = desc->reorder_time;

#ifdef HAVE_FCC
    dpchan->max_backfill = desc->max_backfill;
    dpchan->min_backfill = desc->min_backfill;
    dpchan->max_fastfill = desc->max_fastfill;
#endif  /* HAVE_FCC */

    fec_enabled = (desc->en_fec0 || desc->en_fec1);
    dpchan->fec_enabled = fec_enabled;

    /* setup the pcm_params structure and init the pcm */
    vqec_dpchan_load_pcm_params(&pcm_params, desc, dpchan);
    if (!vqec_pcm_init(&dpchan->pcm, &pcm_params)) {
        goto bail;
    }

    /* Initialize finite state-machine. */
#ifdef HAVE_FCC
    vqec_dp_sm_init(dpchan);
    if (!dpchan->rcc_enabled) {
        /*
         * Join is issued upon binding of the OS for the primary stream within
         * the input shim.  In the non-RCC case, we consider such setup of
         * the input shim to be happening effectively at channel creation
         * time (i.e. now), and any time difference due to the ordering in
         * which the graph builder initializes the input shim vs. this
         * channel object to be insignificant.
         */ 
        dpchan->join_issue_time = get_sys_time();
        vqec_dpchan_set_rcc_aborted(dpchan);
    }
#endif /* HAVE_FCC */

    /* Initialize FEC */
    if (fec_enabled) {
        vqec_dpchan_load_fec_params(&fec_params, desc);
        dpchan->fec = vqec_fec_create(&fec_params, &dpchan->pcm, dpchan->id);
        if (!dpchan->fec) {
            VQEC_DP_SYSLOG_PRINT(ERROR,
                                 "failed to init FEC module");
            goto bail;
        }
    }

    /* initialize the oscheduler */
    if (vqec_dp_oscheduler_init(&dpchan->pcm.osched,
                                &dpchan->pcm,
                                desc->primary.rtcp_xr_post_er_rle_size)
        != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                                 "failed to init output scheduler");
            goto bail; 
    }

#ifdef HAVE_FCC
    VQE_TAILQ_INIT(&dpchan->app_paks);
#endif

    s_dpchan_module.chan_cnt++;
    VQE_TAILQ_INSERT_TAIL(&s_dpchan_module.chan_list, dpchan, le);

    /*sa_ignore {result ignored} IGNORE_RETURN (5) */
    snprintf(dpchan->url, sizeof(dpchan->url),
             "%s:%d",
             uint32_ntoa_r(
                 desc->primary.filter.u.ipv4.dst_ip, tmp, INET_ADDRSTRLEN),
             ntohs(desc->primary.filter.u.ipv4.dst_port));


    /* add the necessary RTP receivers */
    if (vqec_dpchan_add_rtp_receivers(desc,
                                      dpchan,
                                      dpchan_input_obj)
        != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "failed to add rtp receivers to dpchan");
        goto bail;
    }

    /* add the output stream */
    if (vqec_dpchan_create_output(desc,
                                  dpchan,
                                  dpchan_os)
        != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "failed to add os to dpchan");
        goto bail;
    }

    return (dpchan->id);

bail:
    if (dpchan) {
        (void)vqec_dpchan_del_all_is(dpchan->id);
        (void)vqec_dpchan_del_os(dpchan->id);
        if (dpchan->fec) {
            vqec_fec_destroy(dpchan->fec);
        }
        if (dpchan->id != VQEC_DP_CHANID_INVALID) {
            id_delete(dpchan->id, s_dpchan_module.id_table);
        }
        (void)vqec_dp_oscheduler_deinit(&dpchan->pcm.osched);
        vqec_pcm_deinit(&dpchan->pcm);

        zone_release (s_dpchan_module.chan_pool, dpchan);
    }

    return (VQEC_DP_CHANID_INVALID);
}

/**---------------------------------------------------------------------------
 * Update the source filtering address and port information for an existing
 * channel.   Destination filters are not changed (supplied values are
 * ignored).
 *
 * @param[in] chanid       Identifier of the channel
 * @param[in] primary_fil  new primary source filtering information
 * @param[in] repair_fil   [optional] new repair source filtering information
 * @param[out] dp_error_t  Return VQEC_DP_ERR_OK on success, 
 * error code on failure.
 *
 *---------------------------------------------------------------------------*/ vqec_dp_error_t
vqec_dp_chan_update_source (vqec_dp_chanid_t chanid,
                            vqec_dp_input_filter_t *primary_fil,
                            vqec_dp_input_filter_t *repair_fil)
{
    vqec_dp_error_t error = VQEC_DP_ERR_OK;
    vqec_dp_stream_err_t stream_error;
    vqec_dpchan_t *dpchan = NULL;
    vqec_dp_chan_input_stream_t *iptr;

    if ((chanid  == VQEC_DP_CHANID_INVALID) || !primary_fil) {
        VQEC_DP_SYSLOG_PRINT(INVALIDARGS, __FUNCTION__);
        error = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }
    dpchan = vqec_dp_chanid_to_ptr(chanid);
    if (!dpchan) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "dpchan to update not found");
        error = VQEC_DP_ERR_NOT_FOUND;
        goto done;
    }
    
    /* Update the source filter for the primary stream */
    iptr = vqec_dp_chan_input_stream_id_to_ptr(dpchan->prim_is);
    stream_error = MCALL(iptr, bind_update, primary_fil);
    if (stream_error != VQEC_DP_STREAM_ERR_OK) {
        error = VQEC_DP_ERR_INTERNAL;
        goto done;
    }
    /* Update the source filter for the repair stream (if provided) */
    if (repair_fil) {
        iptr = vqec_dp_chan_input_stream_id_to_ptr(dpchan->repair_is);
        stream_error = MCALL(iptr, bind_update, repair_fil);
        if (stream_error != VQEC_DP_STREAM_ERR_OK) {
            error = VQEC_DP_ERR_INTERNAL;
            goto done;
        }
    }

 done:
    if (error != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR, 
                             vqec_dpchan_print_name(dpchan),
                             "Failure to update channel source", 
                             vqec_dp_err2str(error));
    }
    return (error);
}


/****************************************************************************
 * Module-level init, deinit, and poll handler.
 */

/**---------------------------------------------------------------------------
 * Poll handler which periodically runs the schedulers for all channels. 
 * 
 * @param[in] cur_time Absolute current time.
 *---------------------------------------------------------------------------*/ 
#define VQEC_DPCHAN_IRQ_GEN_NUM_SYNC_INTERVAL SECS(10)

void 
vqec_dpchan_poll_ev_handler (abs_time_t cur_time)
{
    vqec_dpchan_t *chan;
    static abs_time_t next_gen_sync_time = ABS_TIME_0;
    boolean gen_sync = FALSE, done_with_fastfill;
    vqec_dp_chan_input_stream_t *iptr;

/**
 * This is used for vod session to reconnect.
 * If the input inactive for this long time, 
 * the client trigger for a RTSP reconnect 
 */ 

#define PRIM_INPUT_INACTIVE_TIME_MAX  (MSECS(500))

    if (s_dpchan_module.init_done &&
        s_dpchan_module.chan_cnt) {
        
        /* If output scheduling is being monitored, log this event */
        if (vqec_dp_chan_hist_outputsched_is_enabled_internal()) {
            vqec_dp_chan_hist_outputsched_add_data(cur_time);
        }
        if (IS_ABS_TIME_ZERO(next_gen_sync_time)) {
            next_gen_sync_time = 
                TIME_ADD_A_R(cur_time,
                             VQEC_DPCHAN_IRQ_GEN_NUM_SYNC_INTERVAL);
        } else if (TIME_CMP_A(ge, cur_time, next_gen_sync_time)) {
            gen_sync = TRUE;
            next_gen_sync_time = 
                TIME_ADD_A_R(cur_time,
                             VQEC_DPCHAN_IRQ_GEN_NUM_SYNC_INTERVAL);
        }

        VQE_TAILQ_FOREACH(chan,
                      &s_dpchan_module.chan_list, 
                      le) {

            (void)vqec_dp_oscheduler_run(&chan->pcm.osched,
                                         cur_time, &done_with_fastfill);
            if (done_with_fastfill) {
                vqec_dpchan_fast_fill_done_notify(chan);
            }
            if (chan->prim_is != VQEC_DP_INVALID_ISID) {
                iptr = vqec_dp_chan_input_stream_id_to_ptr(chan->prim_is);
                if (iptr) {
                    vqec_dp_chan_rtp_scan_one_input_stream(
                        (vqec_dp_chan_rtp_input_stream_t *)iptr, cur_time);
                    /* check for primary input in-activity */
                    if ((!IS_ABS_TIME_ZERO(iptr->last_pak_ts)) && 
                        chan->rx_primary &&
                        TIME_CMP_A(ge, cur_time, 
                                   TIME_ADD_A_R(iptr->last_pak_ts, 
                                                PRIM_INPUT_INACTIVE_TIME_MAX)) &&
                        !chan->prim_inactive){
                        /* signal in-active */
                        vqec_dpchan_primary_inactive_notify(chan);
                        chan->prim_inactive = TRUE;
                    }
                }
            }
            if (gen_sync) {
                (void)vqec_dpchan_tx_upcall_ev(
                    chan, 
                    VQEC_DP_UPCALL_DEV_DPCHAN,
                    0, 
                    VQEC_DP_UPCALL_REASON_CHAN_GEN_NUM_SYNC);
            }
        }
    } 
}


/**---------------------------------------------------------------------------
 * De-initialize the channel module. All channels on the channel list must 
 * have been destroyed prior to the deinit of the module.
 *---------------------------------------------------------------------------*/ 
void
vqec_dpchan_module_deinit (void)
{
    int i;
    vqec_dpchan_t *chan, *chan_next;

    if (s_dpchan_module.chan_cnt != 0) {
        VQEC_DP_SYSLOG_PRINT(ERROR, 
                        "dpchan module deinit: non-zero channel count");
        VQE_TAILQ_FOREACH_SAFE(chan, &s_dpchan_module.chan_list, le, chan_next) {
            vqec_dpchan_destroy(chan->id);
        }
    }
    if (s_dpchan_module.os_pool) {
        (void) zone_instance_put (s_dpchan_module.os_pool);
    }
    if (s_dpchan_module.chan_pool) {
        (void) zone_instance_put (s_dpchan_module.chan_pool);
    }
    if (s_dpchan_module.tspkt_pool) {
        if (s_dpchan_module.tlvbuf.buf) {
            zone_release(s_dpchan_module.tspkt_pool,
                         s_dpchan_module.tlvbuf.buf);
        }
        (void)zone_instance_put(s_dpchan_module.tspkt_pool);
    }
    if (s_dpchan_module.id_table != ID_MGR_TABLE_KEY_ILLEGAL) { 
        id_destroy_table(s_dpchan_module.id_table);
    }
    for (i = VQEC_DP_HIST_JOIN_DELAY; i < VQEC_DP_HIST_MAX; i++) {
        if (s_dpchan_module.hist[i]) {
            zone_release(s_dpchan_module.hist_pool[i],
                         s_dpchan_module.hist[i]);
            (void) zone_instance_put(s_dpchan_module.hist_pool[i]);
        }
    }
    (void)vqec_pcm_module_deinit();
    (void)vqec_fec_module_deinit();
    memset(&s_dpchan_module, 0, sizeof(s_dpchan_module));        
}

#define DPCHAN_IDTABLE_BUCKETS 1
static int32_t
join_delay_ranges[] = {0,10,20,30,40,50,60,90,120,150,200,300,400,500};
static const uint32_t
join_delay_num_ranges = sizeof(join_delay_ranges)/sizeof(join_delay_ranges[0]);
static int32_t
outputsched_ranges[] = {0,10,20,30,40,50,60,90,120,150,200,300,500,700,1000,2000,3000};
static int32_t outputsched_num_ranges = 
sizeof(outputsched_ranges)/sizeof(outputsched_ranges[0]);

/**---------------------------------------------------------------------------
 * Initialize the channel module.
 *
 * s_dpchan_module.init_done is an implicit parameter:
 *   o if TRUE, the module is assumed to be initialized, and
 *     no reinitialization takes place.
 *   o if FALSE, the module is assumed to be uninitialized
 *
 * @param[in] params Dataplane module initialization parameters block.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success,
 * error code on failure.
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dpchan_module_init (vqec_dp_module_init_params_t *params)
{
#define VQEC_DPCHAN_MIN_CHANNELS 2  /* Id manager limitation */
    uint32_t channels;
    vqec_dp_error_t status = VQEC_DP_ERR_OK;
#if HAVE_FCC
    uint16_t tlvbuf_len = 0;     /* size (bytes) of tlvbuf.buf buffer */
    uint16_t num_ts_pkts;    /* num MPEG TS pkts tlvbuf.buf buffer must hold */
#endif /* HAVE_FCC */

    if (s_dpchan_module.init_done) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "dpchan module is already initialized");
        status = VQEC_DP_ERR_ALREADY_INITIALIZED;
        goto done;
    }

    memset(&s_dpchan_module, 0, sizeof(s_dpchan_module));
    s_dpchan_module.id_table = ID_MGR_TABLE_KEY_ILLEGAL;

    if (!params) {
        VQEC_DP_SYSLOG_PRINT(INVALIDARGS, __FUNCTION__);
        status = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

#ifdef HAVE_FCC
    /*
     * Set up the MPEG knobs for the MPEG APP TLV parsing.
     */
#define MPTLV_KNOBS_NUM_PATPMT_SECTIONS 2
#define MPTLV_KNOBS_NUM_PCRS 2
    s_dpchan_module.knobs.num_patpmt = MPTLV_KNOBS_NUM_PATPMT_SECTIONS;
    s_dpchan_module.knobs.pcr_knobs.num_pcrs = MPTLV_KNOBS_NUM_PCRS;
    s_dpchan_module.knobs.pcr_knobs.dind_flag = TRUE;
    s_dpchan_module.knobs.pcr_knobs.rap_flag = TRUE;

    /* cache the app_paks_per_rcc parameter */
    s_dpchan_module.app_paks_per_rcc = params->app_paks_per_rcc;

    /* 
     * Compute size of buffer we'll use for storing TS Packets decoded from
     * RTCP APP packet.
     */
    num_ts_pkts = MP_MAX_DECODED_TSRAP_TLV + MP_TSRAP_TLV_FUDGE;

    if (s_dpchan_module.knobs.num_patpmt > 1) {
        /* 2: one TLV for PAT, one TLV for PMT */
        num_ts_pkts += 2 * (s_dpchan_module.knobs.num_patpmt - 1);
    }
    if (s_dpchan_module.knobs.pcr_knobs.num_pcrs > 1) {
        num_ts_pkts += (s_dpchan_module.knobs.pcr_knobs.num_pcrs - 1);
    }

    /* 
     * Decoder expects TS packets in groups of MP_NUM_TSPAKS_PER_DP_PAK,
     * so ensure buffer large enough to hold a multiple of this number.
     * In worst case, could need to add MP_NUM_TSPAKS_PER_DP_PAK - 1.
     */
    num_ts_pkts += (MP_NUM_TSPAKS_PER_DP_PAK - 1);

    if (s_dpchan_module.app_paks_per_rcc > 1) {
        num_ts_pkts += num_ts_pkts * (s_dpchan_module.app_paks_per_rcc - 1);
    }

    tlvbuf_len = (num_ts_pkts * MP_MPEG_TSPKT_LEN);
#endif  /* HAVE_FCC */

    /* Initialize resource zones */
    s_dpchan_module.os_pool = zone_instance_get_loc ("dpchan_os_pool",
                                                O_CREAT,
                                                sizeof(vqec_dpchan_os_t),
                                                params->max_channels,
                                                NULL,
                                                NULL);
    if (!s_dpchan_module.os_pool) {
        VQEC_DP_SYSLOG_PRINT (ERROR, "unable to allocate OS pool for dpchan");
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }

    s_dpchan_module.chan_pool = zone_instance_get_loc ("dpchan_pool",
                                                O_CREAT,
                                                sizeof(vqec_dpchan_t),
                                                params->max_channels,
                                                NULL,
                                                NULL);
    if (!s_dpchan_module.chan_pool) {
        VQEC_DP_SYSLOG_PRINT (ERROR, "unable to allocate chan pool for dpchan");
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }

#if HAVE_FCC
    s_dpchan_module.tspkt_pool = zone_instance_get_loc("tspkt_pool",
                                                       O_CREAT,
                                                       tlvbuf_len,
                                                       1,
                                                       NULL,
                                                       NULL);
    if (!s_dpchan_module.tspkt_pool) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "unable to allocate tspkt pool for dpchan");
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }

    /* directly allocate the buffer here */
    s_dpchan_module.tlvbuf.buf =
        (char *)zone_acquire(s_dpchan_module.tspkt_pool);
    if (!s_dpchan_module.tlvbuf.buf) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }
    s_dpchan_module.tlvbuf.len = tlvbuf_len;
#endif  /* HAVE_FCC */

    /* Initialize the PCM module */
    status = vqec_pcm_module_init(params->max_channels);
    if (status != VQEC_DP_ERR_OK) {
        goto done;
    }

    /* Initialize the FEC module */
    status = vqec_fec_module_init(2*params->max_channels);
    if (status != VQEC_DP_ERR_OK) {
        goto done;
    }

    /* Allocate Id table. */
    channels = params->max_channels;
    if (channels < VQEC_DPCHAN_MIN_CHANNELS) {
        channels = VQEC_DPCHAN_MIN_CHANNELS;
    }
    s_dpchan_module.id_table = id_create_new_table(DPCHAN_IDTABLE_BUCKETS, 
                                                   channels);
    if (s_dpchan_module.id_table == ID_MGR_TABLE_KEY_ILLEGAL) { 
        VQEC_DP_SYSLOG_PRINT(ERROR,
                             "unable to allocated Id table for dpchan");
        status = VQEC_DP_ERR_NO_RESOURCE;
        goto done;
    }

    VQE_TAILQ_INIT(&s_dpchan_module.chan_list);

    /* Initialize join-delay histogram */
    status = 
        vqec_dp_chan_hist_create(VQEC_DP_HIST_JOIN_DELAY,
                                 join_delay_ranges,
                                 join_delay_num_ranges,
                                 "Join to First Primary Pkt Delay (in ms)");
    if (status != VQEC_DP_ERR_OK) {
        goto done;
    }
    
    /* Initialize output scheduling histogram */
    status = 
        vqec_dp_chan_hist_create(VQEC_DP_HIST_OUTPUTSCHED,
                                 outputsched_ranges,
                                 outputsched_num_ranges,
                                 "Output Scheduling Intervals (in ms)");
    if (status != VQEC_DP_ERR_OK) {
        goto done;
    }

    s_dpchan_module.init_done = TRUE;

 done:
    if ((status != VQEC_DP_ERR_OK) &&
        (status != VQEC_DP_ERR_ALREADY_INITIALIZED)) {
        vqec_dpchan_module_deinit();
    }
    return (status);
}


