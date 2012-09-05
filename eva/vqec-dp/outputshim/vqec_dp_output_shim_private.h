/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
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

#ifndef __VQEC_DP_OUTPUT_SHIM_PRIVATE_H__
#define __VQEC_DP_OUTPUT_SHIM_PRIVATE_H__

#include <utils/queue_plus.h>
#include <vqec_dp_io_stream.h>
#include <vqec_sink.h>
#include <vqec_dp_api_types.h>

typedef struct vqec_dp_output_shim_tuner_ {
    vqec_sink_t *sink;          /*!< Pointer to the associated sink */
    int32_t cp_tid;             /*!< Control-plane tuner id */
    vqec_dp_tunerid_t tid;      /*!< Local dataplane tuner id  */
    char name[VQEC_DP_TUNER_MAX_NAME_LEN];
    int32_t qid;                /*!< Output queue id */
    vqec_dp_isid_t is;          /*!< IS to which this tuner is connected */
    vqec_dp_chan_desc_t chan;   /*!< Cached copy of the assocaited chan info;
                                     this structure is defined in
                                     vqec_dp_api_types.h */
    VQE_TAILQ_ENTRY(vqec_dp_output_shim_tuner_) list_obj;
                                /*!< List object for adding tuner to an IS */
} vqec_dp_output_shim_tuner_t;

typedef struct vqec_dp_output_shim_is_ {
    vqec_dp_isid_t id;          /*!< Input stream id */
    uint32_t capa;              /*!< Input stream capabilities */
    vqec_dp_encap_type_t encap; /*!< Input stream encapsulation type */
    VQE_TAILQ_HEAD(, vqec_dp_output_shim_tuner_) mapped_tunerq;
                                /*!< IDs of tuners connected to this IS */
    vqec_dp_is_status_t status; /*!< Input stream status */
    const vqec_dp_isops_t *ops; /*!< Input stream operations */
    vqec_dp_os_instance_t os;   /*!< OS instance of connected OS */
} vqec_dp_output_shim_is_t;

typedef struct vqec_output_shim_ {
    boolean shutdown;           /*!< TRUE if the output shim is shutdown */
    uint32_t is_creates;        /*!< Input streams created since activation */
    uint32_t is_destroys;       /*!< Input streams destroyed since activation */
    uint32_t num_tuners;        /*!< Number of active tuners  */
    /* arrays for keeping state of tuners and ISs */
    vqec_dp_output_shim_tuner_t **tuners;
    vqec_dp_output_shim_is_t **is;
    uint32_t max_tuners;            /*!< Maximum number of tuners */
    uint32_t max_streams;           /*!< Maximum number of streams */
    uint32_t max_paksize;           /*!< Maximum packet size */
    uint32_t max_iobuf_cnt;         /*!< Maximum length of iobuf array */
    uint32_t iobuf_recv_timeout;    /*!< Maximum timeout for tuner_read */
    uint32_t output_q_limit;        /*!< Maximum size of output queue */
} vqec_output_shim_t;

/* global output_shim instance */
extern vqec_output_shim_t g_output_shim;

/**
 * Looks up a DP tuner's name by its ID.
 *
 * @param[in] tunerid  ID of the DP tuner.
 * @param[out] name    Name string returned of the tuner.
 * @param[in] len      Length of the provided string buffer.
 * @param[out]         Returns VQEC_DP_ERR_OK on success.
 */
vqec_error_t
vqec_dp_outputshim_get_name_by_tunerid(vqec_tunerid_t tunerid,
                                       char *name,
                                       int32_t len);

/**
 * Looks up a DP tuner ID given a valid DP tuner name.
 * NOTE:  The given name need not be NULL-terminated--only the first
 *        VQEC_DP_TUNER_MAX_NAME_LEN characters are used in the lookup.
 *
 * @param[in] name name of DP tuner to find, MUST be valid.
 * @param[out] returns DP tuner ID of found tuner, or the invalid ID.
 */
vqec_tunerid_t
vqec_dp_outputshim_get_tunerid_by_name(const char *name);

/*
 * Map a given CP tuner ID to a DP tuner ID.
 *
 * @param[in]  cp_tid  ID of the associated CP tuner.
 * @param[out]  Returns the DP tuner ID of the associated DP tuner.
 */
static inline vqec_dp_tunerid_t
vqec_dp_output_shim_cp_to_dp_tunerid (vqec_tunerid_t cp_tid)
{
    int i;

    if (g_output_shim.tuners) {
        for (i = 0; i < g_output_shim.max_tuners; i++) {
            if (g_output_shim.tuners[i] &&
                (cp_tid == g_output_shim.tuners[i]->cp_tid)) {
                return (i + 1);
            }
        }
    }

    return (VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID);
}

/*
 * Get a DP tuner instance by its ID.
 *
 * @param[in]  dp_tid  ID of the DP tuner.
 * @param[out]  Returns a pointer to the tuner, or NULL otherwise.
 */
static inline vqec_dp_output_shim_tuner_t
*vqec_dp_output_shim_get_tuner_by_id (vqec_tunerid_t dp_tid)
{
    if ((g_output_shim.tuners) 
        && (dp_tid > 0) 
        && (dp_tid <= g_output_shim.max_tuners)) {
        return (g_output_shim.tuners[dp_tid - 1]);
    }

    return (NULL);
}

/*
 * Read pending datagrams from a tuner.
 * NOTE: There is a 1:1 correspondence between tuner's, sink's and data
 * reader threads. Only a single thread should read datagrams from one
 * tuner; otherwise incorrect behavior will be observed when reading datagrams.
 *
 * @param[in]    iobuf    array of iobufs into which datagrams will be copied
 * @param[in]    cur_buf  ptr to value to use as a starting array index for
 *                          writing into the iobuf[] array
 * @param[out]   cur_buf  ptr to value one greater than the ending array index
 *                          value for data that is successfully or 
 *                          unsucessfully written into the iobuf[] array.
 *                          (An attempt to write data into an array buffer 
 *                          may not succeed if the buffer is too small for 
 *                          the packet's data, but cur_buf is still
 *                          incremented in this case.)
 *                          
 */
static inline void
vqec_dp_oshim_read_tuner_read_one_copy (vqec_dp_tunerid_t id,
                                        vqec_sink_t *sink, 
                                        int32_t *cur_buf,
                                        vqec_iobuf_t *iobuf, 
                                        uint32_t iobuf_num,
                                        uint32_t *bytes_read) 
{
    int32_t len, flags;

    /* internal API only; no need to check id validity */

    while (*cur_buf < iobuf_num) {
        len = MCALL(sink, vqec_sink_read, &iobuf[*cur_buf]);
        if (len <= 0) {
            break;
        }
        flags = iobuf[*cur_buf].buf_flags;
        (*cur_buf)++;
        *bytes_read += len;
   }    
}

#define OS_MIN(a, b) ((a) < (b) ? (a) : (b))

#endif  /* __VQEC_DP_OUTPUT_SHIM_PRIVATE_H__ */
