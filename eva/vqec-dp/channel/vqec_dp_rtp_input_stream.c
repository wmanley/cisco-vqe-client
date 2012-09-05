/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2009-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: RTP input stream classes.
 *
 * Documents:
 *
 *****************************************************************************/

#include "vqec_dp_rtp_input_stream.h"
#include "vqec_pak.h"
#include "vqec_dpchan.h"
#include "vqec_dp_tlm.h"
#include "vqec_dp_utils.h"
#include "vqec_dp_debug_utils.h"
#include <utils/zone_mgr.h>

#include "utils/mp_mpeg.h"
#include <utils/mp_tlv_decode.h>

#define VQEC_DP_RTP_IS_IDTABLE_ID_BASE (0xA0000000)
#define VQEC_DP_RTP_IS_IDTABLE_NUM_IDS (s_id_table.total_ids)
#define VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK \
    (sizeof(rtp_id_table_block_t) * 8)  /* 8 bits per byte */

/**
 * Id table structure for id allocation.
 */ 
typedef uint64_t rtp_id_table_block_t;

typedef
struct vqec_dp_rtp_is_id_table_
{
    /**
     * Total number of id's available.
     */
    uint32_t total_ids;
    /**
     * Bitmask of allocated id's: 0 implies that the bit is allocated.
     */
    rtp_id_table_block_t *bitpos;
    /**
     * Pointers to the input stream objects (linear).
     */    
    vqec_dp_chan_rtp_input_stream_t **is_ptr;

} vqec_dp_rtp_is_id_table_t;


#define BITPOS_UNUSED(pos)                                              \
    ({                                                                  \
        int32_t _j;                                                     \
        for (_j = 0; _j < VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK; _j++) { \
            if ((pos) & (1ULL << _j)) {                                 \
                break;                                                  \
            }                                                           \
        }                                                               \
        _j;                                                             \
    })

#define BITPOS_USED(pos, j)                                             \
    (((pos) & (1ULL << (j))) == 0)

#define BITPOS_ALLOC(pos, i)                    \
    ({                                          \
        (pos) &= ~(1ULL << (i));                \
    })


#define BITPOS_DEALLOC(pos, i)                  \
    ({                                          \
        (pos) |= (1ULL << (i));                 \
    })


#define BITPOS_RESET(pos)                       \
    ({                                          \
        (pos) = 0;                              \
        (pos) = ~(pos);                         \
    })

static vqec_dp_rtp_is_id_table_t s_id_table;
static vqec_dp_chan_rtp_primary_input_stream_fcns_t  the_prim_is_func_table;
static vqec_dp_chan_rtp_repair_input_stream_fcns_t  the_repair_is_func_table;
static vqec_dp_chan_rtp_input_stream_fcns_t  the_fec_is_func_table;

typedef struct vqe_zone vqec_dp_chan_rtp_input_stream_pool_t;
static vqec_dp_chan_rtp_input_stream_pool_t *s_primary_is_pool = NULL;
static vqec_dp_chan_rtp_input_stream_pool_t *s_repair_is_pool = NULL;
static vqec_dp_chan_rtp_input_stream_pool_t *s_fec_is_pool = NULL;

static struct vqe_zone *s_bitpos_pool = NULL;
static struct vqe_zone *s_is_ptr_pool = NULL;

static void
vqec_dp_chan_rtp_repair_input_stream_process_holdq (
    vqec_dp_chan_rtp_repair_input_stream_t *in);

/**---------------------------------------------------------------------------
 * Allocate an input-stream id from the input-stream id pool.
 * 
 * @param[in] table Pointer to a id table instance.
 * @param[in] obj Pointer to the object which is to be recorded against
 * the allocated id.
 * @param[out] vqec_dp_streamid_t Identifier of the stream object.
 *---------------------------------------------------------------------------*/ 
static vqec_dp_streamid_t 
vqec_dp_chan_input_stream_id_get (vqec_dp_rtp_is_id_table_t *table,
                                  vqec_dp_chan_rtp_input_stream_t *obj)
{
    vqec_dp_streamid_t id = VQEC_DP_STREAMID_INVALID;
    int32_t i;

    if (table) {
        for (i = 0; 
             i < VQEC_DP_RTP_IS_IDTABLE_NUM_IDS /
                 VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK;
             i++) {
            
            if (table->bitpos[i] != 0) {
                id = BITPOS_UNUSED(table->bitpos[i]);
                VQEC_DP_ASSERT_FATAL(id < VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK,
                                     "implementation error - "
                                     "no unused bit position");
                
                BITPOS_ALLOC(table->bitpos[i], id);
                id += i * VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK;
                table->is_ptr[id] = obj;
                id += VQEC_DP_RTP_IS_IDTABLE_ID_BASE;
                break;
            }
        } 
    }

    return (id);
}


/**---------------------------------------------------------------------------
 * Deallocate an input-stream id.
 * 
 * @param[in] table Pointer to a id table instance.
 * @param[in] id Identifier of the allocated object. 
 *---------------------------------------------------------------------------*/ 
static void 
vqec_dp_chan_input_stream_id_put (vqec_dp_rtp_is_id_table_t *table,
                                  vqec_dp_streamid_t id)
{
    int32_t i;

    if (table && id != VQEC_DP_STREAMID_INVALID) {        
        id -= VQEC_DP_RTP_IS_IDTABLE_ID_BASE;
        if (id >= 0 && id < VQEC_DP_RTP_IS_IDTABLE_NUM_IDS) { 
            i = id / VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK;
            VQEC_DP_ASSERT_FATAL(
                i < VQEC_DP_RTP_IS_IDTABLE_NUM_IDS /
                VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK,
                "idtable index %d out of bounds.", i);

            BITPOS_DEALLOC(table->bitpos[i], id);
            table->is_ptr[id] = NULL;
        }
    }
}


/**---------------------------------------------------------------------------
 * Lookup an input-stream id and return the object for the id.
 * 
 * @param[in] table Pointer to a id table instance.
 * @param[in] id Identifier of the allocated object. 
 * @param[out] The input stream object registered against the id.
 *---------------------------------------------------------------------------*/ 
static vqec_dp_chan_rtp_input_stream_t *
vqec_dp_chan_input_stream_id_lookup (vqec_dp_rtp_is_id_table_t *table,
                                     vqec_dp_streamid_t id)
{
    if (table && table->is_ptr) {
        id -= VQEC_DP_RTP_IS_IDTABLE_ID_BASE;
        if (id >= 0 && id < VQEC_DP_RTP_IS_IDTABLE_NUM_IDS) {
            return (table->is_ptr[id]);
        }
    }

    return (NULL);
}


/**---------------------------------------------------------------------------
 * Reset the stream id table.
 * 
 * @param[in] table Pointer to a id table instance.
 *---------------------------------------------------------------------------*/ 
static void
vqec_dp_chan_input_id_table_reset (vqec_dp_rtp_is_id_table_t *table)
{
    int32_t i;

    for (i = 0; 
         i < (VQEC_DP_RTP_IS_IDTABLE_NUM_IDS / 
              VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK);
         i++) {        
        BITPOS_RESET(table->bitpos[i]);
    }
}


/**---------------------------------------------------------------------------
 * Flush an id table, i.e., destroy all input stream instances.
 * 
 * @param[in] table Pointer to a id table instance.
 *---------------------------------------------------------------------------*/ 
static void 
vqec_dp_chan_input_id_table_flush (vqec_dp_rtp_is_id_table_t *table)
{
    int32_t i, j, idx;

    if (table && table->bitpos) {
        for (i = 0; 
             i < VQEC_DP_RTP_IS_IDTABLE_NUM_IDS /
                 VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK;
             i++) {
            
            for (j = 0; j < VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK; j++) {
                if (BITPOS_USED(table->bitpos[i], j)) {
                    idx = i*VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK + j;
                    VQEC_DP_ASSERT_FATAL(s_id_table.is_ptr[idx] != NULL, 
                                         "RTP ID table");
                    MCALL(
                        (vqec_dp_chan_input_stream_t *)s_id_table.is_ptr[idx], 
                        destroy);
                }
            } 
        }
    }
}


/**---------------------------------------------------------------------------
 * Pure virtual: (*process_one) method.
 *---------------------------------------------------------------------------*/
static uint32_t 
vqec_dp_chan_input_stream_process_one_virtual (
    vqec_dp_chan_input_stream_t *in_stream,
    vqec_pak_t *pak,
    abs_time_t current_time)
{
    VQEC_DP_ASSERT_FATAL(FALSE, "no implementation");   
    return (0);
}


/**---------------------------------------------------------------------------
 * Pure virtual: (*process_vector) method.
 *---------------------------------------------------------------------------*/
static uint32_t 
vqec_dp_chan_input_stream_process_vec_virtual (
    vqec_dp_chan_input_stream_t *in_stream,    
    vqec_pak_t *pak_array[],
    uint32_t array_len,
    abs_time_t current_time)
{
    VQEC_DP_ASSERT_FATAL(FALSE, "no implementation");   
    return (0);
}


/**---------------------------------------------------------------------------
 * Pure virtual: (*destroy) method.
 *---------------------------------------------------------------------------*/
static void
vqec_dp_chan_input_stream_destroy_virtual (
    vqec_dp_chan_input_stream_t *in_stream)
{
    VQEC_DP_ASSERT_FATAL(FALSE, "no implementation");   
}


/**---------------------------------------------------------------------------
 * Get the input stream instance.
 *
 * @param[in] in Pointer to the input stream.
 * @param[out] instance Pointer to the IS instance.
 * @param[out] vqec_dp_stream_err_t Returns STREAM_ERR_OK on
 * success.
 *---------------------------------------------------------------------------*/
vqec_dp_stream_err_t 
vqec_dp_chan_input_stream_get_instance (vqec_dp_chan_input_stream_t *in,
                                        vqec_dp_is_instance_t *instance)
{
    vqec_dp_stream_err_t err = VQEC_DP_STREAM_ERR_OK;

    if (!in || !instance) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }

    memset(instance, 0, sizeof(*instance));
    instance->id = in->id;
    instance->ops = in->ops;
    
    return (err);
}


/**---------------------------------------------------------------------------
 * Get the input stream status.
 *
 * @param[in] in Pointer to the input stream.
 * @param[out] status Pointer to the IS status output.
 * @param[in] cumulative Flag to retrieve cumulative status
 * @param[out] vqec_dp_stream_err_t Returns STREAM_ERR_OK on
 * success.
 *---------------------------------------------------------------------------*/
vqec_dp_stream_err_t 
vqec_dp_chan_input_stream_get_status (vqec_dp_chan_input_stream_t *in,
                                      vqec_dp_is_status_t *status, 
                                      boolean cumulative)
{
    vqec_dp_stream_err_t err = VQEC_DP_STREAM_ERR_OK;

    if (!in || !status) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }
    
    memset(status, 0, sizeof(*status));
    status->os = in->os_instance.id;
    memcpy(&status->stats, &in->in_stats, sizeof(status->stats));
    if (!cumulative) {
        status->stats.packets -= in->in_stats_snapshot.packets;
        status->stats.bytes -= in->in_stats_snapshot.bytes;
        status->stats.drops -= in->in_stats_snapshot.drops;
    }
    return (err);
}


/**---------------------------------------------------------------------------
 * Get the capability of the input stream.
 *
 * @param[in] in Pointer to the input stream.
 * @param[out] int32_t Capabilities of the stream.
 *---------------------------------------------------------------------------*/
static int32_t
vqec_dp_chan_input_stream_get_capa (vqec_dp_chan_input_stream_t *in)
{
    if (in) {
        return (in->capa);
    } 

    return (VQEC_DP_STREAM_CAPA_NULL);
}


/**---------------------------------------------------------------------------
 * Connect the input stream to an output stream.
 *
 * @param[in] in Pointer to the input stream.
 * @param[out] instance Pointer to the IS status output.
 * @param[out] vqec_dp_stream_err_t Returns STREAM_ERR_OK on
 * success.
 *---------------------------------------------------------------------------*/
static vqec_dp_stream_err_t 
vqec_dp_chan_input_stream_connect (vqec_dp_chan_input_stream_t *in,
                                   vqec_dp_osid_t os, 
                                   const vqec_dp_osops_t *os_ops)
{
    int32_t os_capa, inter_capa;
    vqec_dp_stream_err_t err = VQEC_DP_STREAM_ERR_OK;
    
    if (!in || !os_ops || (os == VQEC_DP_INVALID_OSID)) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }

    os_capa = (*os_ops->capa)(os);
    inter_capa = os_capa & in->capa;

#ifdef HAVE_FCC
    if (!(inter_capa & VQEC_DP_STREAM_CAPA_PUSH_POLL)) {
        err = VQEC_DP_STREAM_ERR_NACKCAPA;
        return (err);
    }
#else 
    if (!(inter_capa & VQEC_DP_STREAM_CAPA_PUSH)) {
        err = VQEC_DP_STREAM_ERR_NACKCAPA;
        return (err);
    }
#endif

    err = (*os_ops->accept_connect)(os, in->id, in->ops,
                                    VQEC_DP_ENCAP_RTP, inter_capa);
    if (err == VQEC_DP_STREAM_ERR_OK) {
        in->os_instance.id = os;
        in->os_instance.ops = os_ops;
    } else {
        VQEC_DP_TLM_CNT(os_connect_failure, vqec_dp_tlm_get());
    }

    return (err);
}


/**---------------------------------------------------------------------------
 * Get RTP packet statistics.
 *
 * @param[in] in Pointer to the RTP input stream.
 * @param[out] stats Pointer to the stats output structure.
 * @param[in] cumulative Boolean flag to retrieve cumulative stats
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 *---------------------------------------------------------------------------*/
static vqec_dp_error_t
vqec_dp_chan_rtp_input_stream_get_rtp_pak_stats (
    vqec_dp_chan_rtp_input_stream_t *in, vqec_dp_chan_rtp_is_stats_t *stats, 
    boolean cumulative) 
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    
    if (!in || !stats) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }
    memcpy(stats, &in->rtp_in_stats, sizeof(*stats));
    if (!cumulative) {
        stats->udp_paks -= in->rtp_in_stats_snapshot.udp_paks;
        stats->rtp_paks -= in->rtp_in_stats_snapshot.rtp_paks;
        stats->pakseq_inserts -= in->rtp_in_stats_snapshot.pakseq_inserts;
        stats->pakseq_drops -= in->rtp_in_stats_snapshot.pakseq_drops;

        /*vqec_drop_stats_t */
        stats->ses_stats.late_packet_counter -=
            in->rtp_in_stats_snapshot.ses_stats.late_packet_counter;
        stats->ses_stats.duplicate_packet_counter -=
            in->rtp_in_stats_snapshot.ses_stats.duplicate_packet_counter;

        /* drop stats */ 
        stats->sm_drops -= in->rtp_in_stats_snapshot.sm_drops;
        stats->sim_drops -= in->rtp_in_stats_snapshot.sim_drops;
        stats->udp_mpeg_sync_drops -=
            in->rtp_in_stats_snapshot.udp_mpeg_sync_drops;
        stats->rtp_parse_drops -= in->rtp_in_stats_snapshot.rtp_parse_drops;
        stats->fil_drops -= in->rtp_in_stats_snapshot.fil_drops;
    }
    return (err);
}

/**---------------------------------------------------------------------------
 * Clear RTP packet statistics.
 *
 * @param[in] in Pointer to the RTP input stream.
 *---------------------------------------------------------------------------*/
static void
vqec_dp_chan_rtp_input_stream_clear_rtp_pak_stats (
    vqec_dp_chan_rtp_input_stream_t *in)  
{
    if (in) {
        memcpy(&in->rtp_in_stats_snapshot, &in->rtp_in_stats, 
               sizeof(in->rtp_in_stats));
        memcpy(&in->in_stats_snapshot, &in->in_stats, sizeof(in->in_stats));
    }
}


/**---------------------------------------------------------------------------
 * Commit a resreved bind on the input stream.
 *
 * @param[in] in Pointer to the RTP input stream.
 *---------------------------------------------------------------------------*/
static boolean
vqec_dp_chan_input_stream_os_bind_commit (vqec_dp_chan_input_stream_t *in)
{
    uint16_t port = 0;
    int32_t sock_fd = -1;
    vqec_dp_stream_err_t status;

    if (in->os_instance.id == VQEC_DP_INVALID_OSID ||
        !in->os_instance.ops->bind_commit) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                             vqec_dpchan_print_name(in->chan), 
                             "Bind commit", " ");
        return (FALSE);
    }
    
    status = (*in->os_instance.ops->bind_commit)(in->os_instance.id, 
                                                 &port, &sock_fd);
    if (status != VQEC_DP_STREAM_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                             vqec_dpchan_print_name(in->chan), 
                             "Bind commit", 
                             vqec_dp_stream_err2str_complain_only(status));
        return (FALSE);
    }
                       
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Update a binding on the input stream.
 *
 * @param[in] in Pointer to the RTP input stream.
 * @param[in] fil New filter for the input stream.  Only the source address
 * and port fields are used; all other fields of the filter are ignored.
 * @param[out] vqec_dp_stream_err_t  Returns VQEC_DP_STREAM_ERR_OK on success.
 *---------------------------------------------------------------------------*/
static vqec_dp_stream_err_t
vqec_dp_chan_input_stream_os_bind_update (vqec_dp_chan_input_stream_t *in,
                                          vqec_dp_input_filter_t *fil)
{
    vqec_dp_stream_err_t status = VQEC_DP_STREAM_ERR_OK;

    if (in->os_instance.id == VQEC_DP_INVALID_OSID) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                             vqec_dpchan_print_name(in->chan), 
                             "Bind update", "invalid args");
        return (VQEC_DP_STREAM_ERR_INVALIDARGS);
    } else if (!in->os_instance.ops->bind_update) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                             vqec_dpchan_print_name(in->chan), 
                             "Bind update", "invalid method");
        return (VQEC_DP_STREAM_ERR_INTERNAL);
    }
    
    status = (*in->os_instance.ops->bind_update)(in->os_instance.id, fil);
    if (status != VQEC_DP_STREAM_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                             vqec_dpchan_print_name(in->chan), 
                             "Bind update", 
                             vqec_dp_stream_err2str_complain_only(status));
    }                       
    return (status);
}


/**---------------------------------------------------------------------------
 * Poll for data that may be queued up in the inputshim.
 *
 * @param[in] in Pointer to the RTP input stream.
 *---------------------------------------------------------------------------*/
static boolean
vqec_dp_chan_input_stream_os_poll (vqec_dp_chan_input_stream_t *in)
{
    vqec_dp_stream_err_t status;

    if (in->os_instance.id == VQEC_DP_INVALID_OSID ||
        !in->os_instance.ops->poll_push) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                             vqec_dpchan_print_name(in->chan), 
                             "poll_push", " ");
        return (FALSE);
    }
    
    status = (*in->os_instance.ops->poll_push)(in->os_instance.id);
    if (status != VQEC_DP_STREAM_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(CHAN_ERROR,
                             vqec_dpchan_print_name(in->chan), 
                             "poll push", 
                             vqec_dp_stream_err2str_complain_only(status));
        return (FALSE);
    }
                       
    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Setup the function table for the input stream class.
 *
 * @param[in] table Pointer to the input stream class function table.
 *---------------------------------------------------------------------------*/
void 
vqec_dp_chan_input_stream_set_fcns (
    vqec_dp_chan_input_stream_fcns_t *table) 
{
    table->process_one = 
        vqec_dp_chan_input_stream_process_one_virtual;
    table->process_vector = 
        vqec_dp_chan_input_stream_process_vec_virtual;
    table->destroy = 
        vqec_dp_chan_input_stream_destroy_virtual;
    table->connect_os =
        vqec_dp_chan_input_stream_connect;
    table->bind_commit = 
        vqec_dp_chan_input_stream_os_bind_commit;
    table->bind_update = 
        vqec_dp_chan_input_stream_os_bind_update;
    table->poll_data = 
        vqec_dp_chan_input_stream_os_poll;
}


/**---------------------------------------------------------------------------
 * Setup the function table for the rtp input stream class.
 *
 * @param[in] table Pointer to the rtp class function table.
 *---------------------------------------------------------------------------*/
void 
vqec_dp_chan_rtp_input_stream_set_fcns (
    vqec_dp_chan_rtp_input_stream_fcns_t *table) 
{
    vqec_dp_chan_input_stream_set_fcns(
        (vqec_dp_chan_input_stream_fcns_t *)table);

    table->get_rtp_stats = 
        vqec_dp_chan_rtp_input_stream_get_rtp_pak_stats;
    table->clear_rtp_stats = 
        vqec_dp_chan_rtp_input_stream_clear_rtp_pak_stats;
}


/**---------------------------------------------------------------------------
 * Deconstruct a base channel input stream. 
 *
 * @param[in] in Pointer to the RTP input stream.
 *---------------------------------------------------------------------------*/
static void
vqec_dp_chan_input_stream_deinit (vqec_dp_chan_input_stream_t *in)
{
    if (in) {
        vqec_dp_chan_input_stream_id_put(&s_id_table, in->id);
        
        if (in->chan) {
            /* release reference on the channel. */
            VQEC_DP_REFCNT_UNREF(in->chan);
        }
    }
}


/**---------------------------------------------------------------------------
 * Initialize a base channel input stream: allocates an Id for the stream. 
 *
 * @param[in] in Pointer to the RTP input stream.
 * @param[in] chan Parent dataplane channel.
 * @param[out] boolean True if the initialization succeeds.
 *---------------------------------------------------------------------------*/
static boolean 
vqec_dp_chan_input_stream_init (vqec_dp_chan_input_stream_t *in,
                                vqec_dpchan_t *chan)
{
    if (!in || !chan) {
        return (FALSE);
    }
        
    in->id = 
        vqec_dp_chan_input_stream_id_get(&s_id_table, 
                                         (vqec_dp_chan_rtp_input_stream_t *)in);
    if (in->id == VQEC_DP_STREAMID_INVALID) {
        VQEC_DP_SYSLOG_PRINT(CHAN_IS_CREATE_FAILED, 
                           vqec_dpchan_print_name(chan), 
                           "Allocation of identifier for input stream failed");
        
        /* increment debug counter. */
        VQEC_DP_TLM_CNT(input_stream_limit_exceeded, vqec_dp_tlm_get());
        return (FALSE);
    }

    in->chan = chan;
    VQEC_DP_REFCNT_REF(chan);   /* take reference on channel */
    
    return (TRUE); 
}


/**---------------------------------------------------------------------------
 * Deconstruct a RTP input stream. 
 *
 * @param[in] in Pointer to the RTP input stream.
 *---------------------------------------------------------------------------*/
static void
vqec_dp_chan_rtp_input_stream_deinit (
    vqec_dp_chan_rtp_input_stream_t *in)
{
    if (in) {
        /* sa_ignore {Nothing else to do if the call fails.} IGNORE_RETURN */
        vqec_dp_chan_rtp_receiver_deinit(&in->rtp_recv);
        
        /* invoke super-class destructor. */
        vqec_dp_chan_input_stream_deinit((vqec_dp_chan_input_stream_t *)in); 
    }
}


/**---------------------------------------------------------------------------
 * Initialize a RTP input stream, including initialization of the receiver.
 * If the initialization succeeds the method returns true, otherwise it 
 * returns false.
 *
 * @param[in] in Pointer to the RTP input stream.
 * @param[in] chan Parent dataplane channel.
 * @param[in] s_desc Input stream description.
 * @param[out] boolean True if the initialization succeeds.
 *---------------------------------------------------------------------------*/
static boolean 
vqec_dp_chan_rtp_input_stream_init (vqec_dp_chan_rtp_input_stream_t *in,
                                    vqec_dpchan_t *chan,
                                    vqec_dp_input_stream_t *s_desc)
{
    if (!in || !chan || !s_desc) {
        VQEC_DP_SYSLOG_PRINT(INVALIDARGS, __FUNCTION__);
        return (FALSE);
    }

    /* invoke super-class constructor. */
    if (!vqec_dp_chan_input_stream_init(
            (vqec_dp_chan_input_stream_t *)in, chan)) {
        VQEC_DP_SYSLOG_PRINT(
            CHAN_IS_CREATE_FAILED, 
            vqec_dpchan_print_name(chan), 
            "Failure in initialization of input stream base class");
        return (FALSE);
    }

    /* local construction - initialize RTP receiver. */
    if (vqec_dp_chan_rtp_receiver_init(&in->rtp_recv,
                                       in,
                                       s_desc->rtcp_xr_max_rle_size,
                                       s_desc->rtcp_xr_post_er_rle_size)
        != VQEC_DP_ERR_OK) { 
        VQEC_DP_SYSLOG_PRINT(
            CHAN_IS_CREATE_FAILED, 
            vqec_dpchan_print_name(chan), 
            "Failure in initialization of RTP receiver instance");

        /* deinitialize the super-class instance. */
        vqec_dp_chan_input_stream_deinit((vqec_dp_chan_input_stream_t *)in);
        return (FALSE);
    } 

    return (TRUE);
}


/**---------------------------------------------------------------------------
 * Method that receives and processes a packet vector for the primary
 * stream as delivered by the input-shim. The inputs and outputs for this
 * method are specified as follows.
 *
 * @param[in] in Pointer to a dp channel input stream: in this case this
 * must be an instance of a primary input stream.
 * @param[in] pak_array Pointer to the packet vector which is to be processed.
 * @param[in] array_len Length of the array.
 * @param[in] current_time The current system time. The value can
 * be left unspecified, i.e., 0, and is essentially used for optimization.
 * @param[out] uint32_t The method returns the number of packets
 * that were dropped due to failing RTP receive processing.  It will thus 
 * return a value less than or equal to array_len.
 *---------------------------------------------------------------------------*/
static uint32_t
vqec_dp_chan_rtp_primary_input_stream_rcv_vec (
    struct vqec_dp_chan_input_stream_ *in, 
    vqec_pak_t *pak_array[],
    uint32_t array_len,
    abs_time_t current_time)
{
    uint32_t rtp_drops = 0, wr = 0, i;
    vqec_pak_t *pak;
    vqec_dp_chan_rtp_input_stream_t *rtp_is;
    vqec_pak_t *l_pak_array[VQEC_DP_STREAM_PUSH_VECTOR_PAKS_MAX];
    vqec_dp_rtp_src_t *rtp_src_entry, *rtp_src_entry_pktflow_permitted = NULL;
    boolean drop;

    if (!in || !pak_array) {
        /* no packets processed. */
        return (array_len);
    }

    rtp_is = (vqec_dp_chan_rtp_input_stream_t *)in;
  
    for (i = 0; i < array_len; i++) {

        pak = pak_array[i];
        pak->rtp = (rtpfasttype_t *)vqec_pak_get_head_ptr(pak);

        /*
         * drop-simulation
         *
         * Note this is only dependent on order of packet arrival
         * for packets on this primary stream.
         *
         * For backward compatibility, we assume simulator-dropped
         * packets are RTP paks and increment this counter accordingly.
         */
        if (vqec_dp_drop_sim_drop(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY)) {
            rtp_is->rtp_in_stats.rtp_paks++;
            rtp_is->rtp_in_stats.sim_drops++;
            rtp_drops++;
            continue;
        }
        
        /*
         * Invoke rtp processing on the packet, which determines whether
         * the packet is acceptable for insertion into the PCM.
         * If the packet should be dropped, the appropriate RTP drop counter
         * is updated internally, and the "dropped" parameter is set to TRUE.
         */
        if (vqec_dp_chan_rtp_process_primary_pak(rtp_is, pak, &drop,
                                                 &rtp_src_entry)) {
            /*
             * Packets which passed RTP processing (and were not 
             * dropped/queued) must be from a single RTP source.
             * Record this common source if we have not already.
             */
            if (!rtp_src_entry_pktflow_permitted) {
                rtp_src_entry_pktflow_permitted = rtp_src_entry;
            } else {
                VQEC_DP_ASSERT_FATAL(
                    rtp_src_entry_pktflow_permitted == rtp_src_entry, 
                    __FUNCTION__);
            }
            /* Add the packet to the array for PCM insertion */
            l_pak_array[wr++] = pak;
        } else {
            /* 
             * Packet is not destined for PCM.
             * Increment input stream's drop counter if applicable.
             */ 
            if (drop) {
                rtp_drops++;
            }
        }
    }

    if (wr) {
        /* 
         * For those packets which passed main RTP processing, insert them
         * into the PCM.  Keep track of the number of packets dropped due
         * to late-stage RTP processing within the function call below.
         */
        rtp_drops += 
            vqec_dp_chan_rtp_process_primary_paks_and_insert_to_pcm(
                rtp_is, l_pak_array, wr, current_time, 
                rtp_src_entry_pktflow_permitted);
    }

    return (rtp_drops);
}


/*-----------------------------------------------------------------------------
 * Method that constructs an RTP header on top of the APP packets in the
 * channel app_paks queue. The RTP header information such as SSRC and 
 * timestamp is picked up from the first repair packet.
 *
 * @param[in] parent_chan Pointer to the data plane channel instance which 
 *            carries the app_paks queue.
 * @param[in] first_repair_pak The packet whose RTP header will be copied on 
 *            top of the TS-APP packets lying in the app_paks queue of the dp 
 *            channel.
 * @param[out] vqec_dp_error_t returns VQEC_DP_ERR_NO_RESOURCE_FOR_RTP_HEADER
 *             if RTP header cannot be constructed on the APP packet, and 
 *             returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/
static vqec_dp_error_t
construct_rtp_hdr_over_ts_app(vqec_dpchan_t *parent_chan, 
                              vqec_pak_t *first_repair_pak)
{
    boolean strip_rtp;
    vqec_pak_t * pak_p, *pak_p_next;
    uint32_t pkt_mpegts_buflen;  
    uint32_t rtpfasttype_t_len = sizeof(rtpfasttype_t);
    vqec_dp_error_t err;
  
    if (!parent_chan || !first_repair_pak) {
        return VQEC_DP_ERR_NO_RESOURCE_FOR_RTP_HEADER;
    }
    
    strip_rtp = parent_chan->pcm.strip_rtp;
    if (!strip_rtp) {
        pkt_mpegts_buflen = MP_NUM_TSPAKS_PER_DP_PAK * 
                            MP_MPEG_TSPKT_LEN;
        /* pick up the APP packet from the channel APP queue */
        VQE_TAILQ_FOREACH_SAFE(pak_p, &parent_chan->app_paks, ts_pkts, 
                               pak_p_next) {
            /* 
             * Check if the app packet has enough space to be able to
             * accomodate a RTP header without extensions. 
             * If not, delete all APP packets, report error, and 
             * abort RCC
             */
            if (pak_p->alloc_len < pkt_mpegts_buflen + rtpfasttype_t_len) {
                /* 
                 * Abort RCC takes care of free-ing the holdq and the app_paks
                 * queue
                 */
                (void)vqec_dp_sm_deliver_event(parent_chan, 
                                               VQEC_DP_SM_EVENT_INTERNAL_ERR, 
                                               NULL);
                err = VQEC_DP_ERR_NO_RESOURCE_FOR_RTP_HEADER;
                return (err);
            }
            /* 
             * We have reached here implies that there is enough 
             * space to allocate the RTP header in the pak_p->buff
             */
            pak_p->rtp = (rtpfasttype_t *)pak_p->buff;
            /* 
             * Move the existing payload sizeof(rtpfasttype_t) bytes 
             * to the right
             */
            memmove(&pak_p->buff[rtpfasttype_t_len], pak_p->buff, 
                    pkt_mpegts_buflen);
            pak_p->mpeg_payload_offset = rtpfasttype_t_len;
            vqec_pak_set_content_len(pak_p,
                                     vqec_pak_get_content_len(pak_p) +
                                     rtpfasttype_t_len);
            /* 
             * Now we will copy the RTP header from the first repair
             * packet as it is, but without RTP extensions. RTP ext.
             * will not be copied from the RTP packet, because doing so
             * may cause some issues for the STB decoder. 
             * After this copying, we have the 'almost' correct
             * header, and we will further make more changes to the 
             * header. Note that pak_p->type and pak_p->seq_num has
             * been already set in vqec_dp_chan_process_app_internal
             */
            memcpy(pak_p->rtp, first_repair_pak->rtp, rtpfasttype_t_len);
            pak_p->rtp->sequence = htons(pak_p->seq_num); /* 16 bit */
            pak_p->rtp->combined_bits = 0;
            SET_RTP_VERSION(pak_p->rtp, RTPVERSION);
            SET_RTP_PAYLOAD(pak_p->rtp, RTP_MP2T);
            SET_RTP_MARKER(pak_p->rtp, 1);
        } 
    }
    return (VQEC_DP_ERR_OK);
}

/**---------------------------------------------------------------------------
 * Method that receives and processes one packet from the repair
 * stream as delivered by the input-shim. The inputs and outputs for this
 * method are specified as follows.
 *
 * @param[in] in Pointer to a dp channel input stream: in this case this
 * must be an instance of a repair input stream.
 * @param[in] pak Pointer to the packet which is to be processed.
 * @param[in] current_time The current system time. 
 * @param[out] uint32_t The method returns the number of packets
 * that passed RTP receive processing. For this particular variant, it
 * will thus return either zero or one.  The return value is for informational
 * purpose, and may be ignored by the caller.
 *---------------------------------------------------------------------------*/
#define VQEC_DP_RTP_REPAIR_IS_OSN_LENGTH 2

static uint32_t
vqec_dp_chan_rtp_repair_input_stream_rcv_one (
    vqec_dp_chan_input_stream_t *in, 
    vqec_pak_t *pak,
    abs_time_t current_time)
{
    int32_t wr = 0, inserts;
    vqec_dp_chan_rtp_input_stream_t *rtp_is;
    vqec_dp_chan_rtp_repair_input_stream_t *repair_is;
    vqec_seq_num_t seqnum, osn;
    vqec_dp_error_t err;
    rtp_hdr_status_t rtp_status;
    uint32_t hdr_len;
    vqec_dpchan_t *parent_chan = NULL;
    boolean flush_holdq = FALSE;
    int16_t session_rtp_seq_num_offset;

    if (!in || !pak) {
        /* no packets processed. */
        return (wr);
    }

    if (in->chan) {
        parent_chan = in->chan;
    }

    /* Initialize time argument if set to zero. */
    if (IS_ABS_TIME_ZERO(current_time)) {
        current_time = get_sys_time();
    }

    pak->rtp = (rtpfasttype_t *)pak->buff;
    rtp_is = (vqec_dp_chan_rtp_input_stream_t *)in;
    repair_is = (vqec_dp_chan_rtp_repair_input_stream_t *)in;
    
    /* validate header. */
    rtp_status = rtp_validate_hdr(&rtp_is->rtp_recv.session.sess_stats,
                                  vqec_pak_get_head_ptr(pak),
                                  vqec_pak_get_content_len(pak));
    if (!rtp_hdr_ok(rtp_status)) {
        /* Check for stun packet: punt to control plane. */
        if ((vqec_pak_get_head_ptr(pak)[0] & 0xC0) == 0) {
            err = vqec_dpchan_eject_pak(in->chan, in->id, pak);
            if (err == VQEC_DP_ERR_OK) {
                VQEC_DP_TLM_CNT(input_stream_eject_paks, vqec_dp_tlm_get());
                wr++;
            } else {
                VQEC_DP_TLM_CNT(input_stream_eject_paks_failure, 
                                vqec_dp_tlm_get());
            }
            return (wr);
        }
        rtp_is->rtp_in_stats.rtp_parse_drops++;
        return (wr);
    }

    /* recover OSN and sequence number from repair packet */
    hdr_len = RTPHEADERBYTES(pak->rtp);
    if (vqec_pak_get_content_len(pak) <
        (hdr_len + VQEC_DP_RTP_REPAIR_IS_OSN_LENGTH)) {
        rtp_is->rtp_in_stats.rtp_parse_drops++;
        return (wr); 
    }
    osn = ntohs(*(uint16_t *)(vqec_pak_get_head_ptr(pak) + hdr_len));
    seqnum = ntohs(pak->rtp->sequence);

    /* For RCC re-order. 
     * If the first seqnum_fil_act filter is set, that means the 
     * APP already arrived, safe to PCM the holdq when the first
     * repair is received, otherwise, all paks should be queued 
     * in hold_q. 
     */
    if (repair_is->first_seqnum_fil_act) {
        if (vqec_seq_num_eq(repair_is->first_seqnum_fil, osn)) {
            if (parent_chan != NULL) {
                parent_chan->process_first_repair = TRUE;
            }
            err = construct_rtp_hdr_over_ts_app(parent_chan, pak);
            /* Incase err!=VQEC_DP_ERR_OK, RCC has been already aborted */
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                          "first repair arrived after APP, "
                          "seq # = %u, @ time = %llu ms, err = %d\n",
                          osn, TIME_GET_A(msec, get_sys_time()), err);
            repair_is->first_seqnum_fil_act = FALSE; 
            flush_holdq = TRUE;
        } else {
            /* queue this packet in holdq */
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                          "one packet queued in holdq after APP, "
                          "seq # = %u, @ time = %llu ms \n",
                          osn, TIME_GET_A(msec, get_sys_time()));
        }
    }

    /*
     * If rcc is active, and we are waiting for an app, then hold packets
     * within the input-stream (the packets do not go through rtp
     * processing, thus, the return value is 0 - this can be adjusted later
     * when the queue is flushed; this is different than the behavior
     * in the existing code where packets go through rtp processing).
     */
    if (vqec_dpchan_pak_event(in->chan, VQEC_DP_CHAN_RX_REPAIR_PAK, 
                              pak, current_time) ==
        VQEC_DP_CHAN_PAK_ACTION_QUEUE ||
        repair_is->first_seqnum_fil_act) {
        if (!repair_is->first_seqnum_fil_act) {
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                          "one packet queued in holdq before APP, "
                          "seq # = %u, @ time = %llu ms \n",
                          osn, TIME_GET_A(msec, get_sys_time()));
        }
        VQE_TAILQ_INSERT_TAIL(
            &((vqec_dp_chan_rtp_repair_input_stream_t *)in)->holdq_lh, 
            pak, 
            inorder_obj);
        /* increment reference count - internal enQ. */
        vqec_pak_ref(pak);
        wr++;
        return (wr);
    }

    /* rtp process packet, and insert into pcm */
    if (vqec_dp_chan_rtp_process_repair_pak(rtp_is, pak, FALSE,
                                            &session_rtp_seq_num_offset)) { 

        rtp_is->rtp_in_stats.rtp_paks++;

        /*
         * do drop-simulation
         *
         * Note:  Dropping occurs here primarily to maintain consistency
         *        with primary stream dropping, i.e. after the packet has
         *        been counted in the rtp_paks (received RTP paks) stat.
         */
        if (vqec_dp_drop_sim_drop(VQEC_DP_INPUT_STREAM_TYPE_REPAIR)) {
            rtp_is->rtp_in_stats.sim_drops++;
            return (wr);
        }

        do {
            /* 
             * If the rcc state is abort, the state machine may drop the repair
             * packets. This is done so that primary paks flow prior to repair.
             */
            if (vqec_dpchan_pak_event(in->chan, VQEC_DP_CHAN_RX_REPAIR_PAK, 
                                      pak, current_time) !=
                VQEC_DP_CHAN_PAK_ACTION_ACCEPT) {
                
                rtp_is->rtp_in_stats.sm_drops++;
                break;
            }
            
            pak->type = VQEC_PAK_TYPE_REPAIR;
            osn = ntohs(*(uint16_t *)(vqec_pak_get_head_ptr(pak) +
                                      pak->mpeg_payload_offset));
              /*
               * Shift the rtp-header OSN_length bytes to the right
               * overwriting the OSN bytes. Set the sequence number of
               * the RTP header as the sequence number as it appears in the
               * packet (Network byte order).
               */
            pak->rtp->sequence = * ((uint16_t *)(vqec_pak_get_head_ptr(pak) +
                                                 pak->mpeg_payload_offset));
            memmove(vqec_pak_get_head_ptr(pak) + 
                        VQEC_DP_RTP_REPAIR_IS_OSN_LENGTH,
                    vqec_pak_get_head_ptr(pak),
                    RTPHEADERBYTES(pak->rtp));
            
            /* Adjust the head pointer */
            vqec_pak_adjust_head_ptr(pak, VQEC_DP_RTP_REPAIR_IS_OSN_LENGTH);

            /* Adjust the RTP header offset*/
            pak->rtp= (rtpfasttype_t *)(pak->buff + 
                                        VQEC_DP_RTP_REPAIR_IS_OSN_LENGTH);

            SET_RTP_VERSION(pak->rtp, RTPVERSION);
            SET_RTP_PAYLOAD(pak->rtp, RTP_MP2T);
            /* project to extended sequence number space. */
            pak->seq_num = 
                vqec_seq_num_nearest_to_rtp_seq_num(
                    vqec_pcm_get_last_rx_seq_num(
                        vqec_dpchan_pcm_ptr(in->chan)), 
                    osn + session_rtp_seq_num_offset);
            vqec_pcm_set_last_rx_seq_num(vqec_dpchan_pcm_ptr(in->chan), 
                                         pak->seq_num);

            if ((!IS_ABS_TIME_ZERO(in->chan->pcm.er_en_ts)) &&
                (TIME_CMP_A(gt, current_time, in->chan->pcm.er_en_ts))) {
                    VQEC_PAK_FLAGS_SET(&pak->flags, VQEC_PAK_FLAGS_AFTER_EC);
            }

#if HAVE_FCC
            /**
             * In some very special cases, for instance, when there is a 
             * large amount of network delay, the incoming repair packets 
             * may take a long time to arrive, but when they do and RCC is 
             * aborted,they'll be wrongly inserted into the PCM and cause 
             * large output gaps.
             * This post process handles these kind of cases.
             */
            if (vqec_pcm_rcc_post_abort_process(
                    vqec_dpchan_pcm_ptr(in->chan), pak)) {
                rtp_is->rtp_in_stats.pakseq_drops++;
                break;
            }
#endif 
            /* rtp packets delivered to the next element in the chain. */
            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                rtp_is->rtp_in_stats.pakseq_inserts++;
            }
            inserts = 
                vqec_pcm_insert_packets(vqec_dpchan_pcm_ptr(in->chan), 
                                        &pak, 1, TRUE,
                                        &rtp_is->rtp_in_stats.ses_stats);
            /* some packets may be dropped on insert */
            rtp_is->rtp_in_stats.pakseq_drops += 1 - inserts;
            wr++;
        } while (0);   

    } else {
        /* packet dropped by rtp. */
        rtp_is->rtp_in_stats.rtp_parse_drops++;
    }

    if (flush_holdq) {
            flush_holdq = FALSE;
            vqec_dp_chan_rtp_repair_input_stream_process_holdq (repair_is);
    }

    return (wr);    
}        

/**
 * When this function is called, the first repair is already inserted
 * into PCM, so insert all the paks in holdq to PCM
 */
static void
vqec_dp_chan_rtp_repair_input_stream_process_holdq (
    vqec_dp_chan_rtp_repair_input_stream_t *in)
{
    vqec_pak_t *pak, *pak_next;
    uint32_t hdr_len, wr;
    rtp_hdr_status_t rtp_status;
    rtpfasttype_t *rtp;
    vqec_dpchan_t *parent_chan = NULL;    
    
    if (!in) {
        return;
    }
    
    if (in->chan) {
        parent_chan = in->chan;
    }

    VQE_TAILQ_FOREACH_SAFE(pak, &in->holdq_lh, inorder_obj, pak_next) {

        /*
         * When processing the packets on the holdq and pushing them to the next
         * element in the chain (e.g., the PCM) by passing them to 
         * vqec_dp_chan_rtp_repair_input_stream_rcv_one, it is possible for the
         * RCC state machine to get into an error state, which will then cause
         * it to proceed to the ABORT state, flushing the holdq along the way.
         * When the holdq is flushed while within this loop, we are left still
         * holding a pointer to an element of the queue which has an invalid
         * next pointer.  It should be noted, however, that the current element
         * is still valid, is it was removed from the holdq *before* the flush
         * occurred.
         */
        if (VQE_TAILQ_EMPTY(&in->holdq_lh)) {
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                          "process_holdq: holdq is empty; "
                          "breaking out of FOREACH loop\n");
            break;
        }
        
        VQE_TAILQ_REMOVE(&in->holdq_lh, pak, inorder_obj);
        
        /*
         * Must validate the rtp header first, prior to using a sequence
         * number field from the rtp header.
         */
        rtp_status = rtp_validate_hdr(
            &in->rtp_recv.session.sess_stats,
            vqec_pak_get_head_ptr(pak),
            vqec_pak_get_content_len(pak));
        if (!rtp_hdr_ok(rtp_status)) {
            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                in->in_stats.drops++;  /* postfix of counter */
            }
            in->rtp_in_stats.rtp_parse_drops++;
            goto next_pkt;
        }
        
        rtp = (rtpfasttype_t *)vqec_pak_get_head_ptr(pak);
        hdr_len = RTPHEADERBYTES(rtp);

        if (vqec_pak_get_content_len(pak) >=
            (hdr_len + VQEC_DP_RTP_REPAIR_IS_OSN_LENGTH)) {
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                          "Insert a pak to PCM from process holdq, "
                          "osn seq # = %u \n",
                          ((vqec_seq_num_t)(ntohs(*(uint16_t *)
                                                 (vqec_pak_get_head_ptr(pak)
                                                  + hdr_len)))));
            wr = vqec_dp_chan_rtp_repair_input_stream_rcv_one(
                (vqec_dp_chan_input_stream_t *)in, 
                (vqec_pak_t *)pak, 
                ABS_TIME_0);
            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                in->in_stats.drops += 1 - wr;  /* postfix of drops */
            }

        } else {
            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                in->in_stats.drops++;  /* postfix of counter */
            }
            in->rtp_in_stats.rtp_parse_drops++;
        }
        
    next_pkt:
        vqec_pak_free(pak);  /* drop packet reference */
    }
}

/**---------------------------------------------------------------------------
 * Transmit all packets that are present on a holdq for a repair stream to
 * the next element in the chain. The packets in the "linked-list" that were 
 * received after the packet with start_seq_num are delivered; all packets
 * that are "topologically" before that sequence are dropped.  If the 
 * desired sequence number is not present on the list, the IS will queue
 * all future packets to the hold queue until the start sequence number is
 * received, or the start sequence number filter is explicitly removed & 
 * the hold queue is flushed. 
 *
 * @param[in] in Pointer to a input stream.
 * @param[in] start_seq_num Filter the hold queue, and transmit packets
 * starting from start_seq_num in topological order. All prior packets
 * are dropped. If the start_seq_num is not present a sequence number
 * filter is set internally.
 *--------------------------------------------------------------------------*/
static void
vqec_dp_chan_rtp_repair_input_stream_filter_holdq (
    vqec_dp_chan_rtp_repair_input_stream_t *in,
    vqec_seq_num_t start_seq_num)
{
    vqec_pak_t *pak, *pak_next;
    uint32_t hdr_len, wr;
    vqec_seq_num_t seqnum, osn;
    boolean sc_found = FALSE;
    rtp_hdr_status_t rtp_status;
    rtpfasttype_t *rtp;
    vqec_dpchan_t *parent_chan = NULL;    
    vqec_dp_error_t err;
    boolean remove = FALSE;

    if (!in) {
        return;
    }
    
    if (in->chan) {
        parent_chan = in->chan;
    }

    /**
     * There are two rounds to process the holdq, first round is to 
     * find the first repair and insert into PCM, the second round is 
     * to add remaining packets to PCM. 
     * If first repair is not found, only one round is excuted. 
     */

    VQE_TAILQ_FOREACH_SAFE(pak, &in->holdq_lh, inorder_obj, pak_next) {

        /*
         * When processing the packets on the holdq and pushing them to the next
         * element in the chain (e.g., the PCM) by passing them to 
         * vqec_dp_chan_rtp_repair_input_stream_rcv_one, it is possible for the
         * RCC state machine to get into an error state, which will then cause
         * it to proceed to the ABORT state, flushing the holdq along the way.
         * When the holdq is flushed while within this loop, we are left still
         * holding a pointer to an element of the queue which has an invalid
         * next pointer.  It should be noted, however, that the current element
         * is still valid, is it was removed from the holdq *before* the flush
         * occurred.
         */
        if (VQE_TAILQ_EMPTY(&in->holdq_lh)) {
            VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                          "filter_holdq: holdq is empty; "
                          "breaking out of FOREACH loop\n");
            break;
        }
        
        /*
         * Must validate the rtp header first, prior to using a sequence
         * number field from the rtp header.
         */
        rtp_status = rtp_validate_hdr(
            &in->rtp_recv.session.sess_stats,
            vqec_pak_get_head_ptr(pak),
            vqec_pak_get_content_len(pak));
        if (!rtp_hdr_ok(rtp_status)) {
            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                in->in_stats.drops++;  /* postfix of counter */
            }
            in->rtp_in_stats.rtp_parse_drops++;
            /* remove and drop bad header paks */
            VQE_TAILQ_REMOVE(&in->holdq_lh, pak, inorder_obj);
            remove = TRUE;
            goto next_pak;
        }
        
        rtp = (rtpfasttype_t *)vqec_pak_get_head_ptr(pak);
        hdr_len = RTPHEADERBYTES(rtp);

        if (vqec_pak_get_content_len(pak) >=
            (hdr_len + VQEC_DP_RTP_REPAIR_IS_OSN_LENGTH)) {
            
            osn = ntohs(*(uint16_t *)(vqec_pak_get_head_ptr(pak)
                                      + hdr_len));
            seqnum = ntohs(rtp->sequence);

            if (sc_found) {
                /**
                 * the first repair is already inserted into PCM,
                 * safe to remove the paks after that. 
                 */
                VQE_TAILQ_REMOVE(&in->holdq_lh, pak, inorder_obj);
                remove = TRUE;
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                              "Insert a pak to PCM from filter holdq, "
                              "osn seq # = %u \n",
                              osn);
                wr = vqec_dp_chan_rtp_repair_input_stream_rcv_one(
                    (vqec_dp_chan_input_stream_t *)in, 
                    (vqec_pak_t *)pak, 
                    ABS_TIME_0);
                if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                    in->in_stats.drops += 1 - wr;  /* postfix of drops */
                }
            } else if (vqec_seq_num_eq(osn, start_seq_num)) {
                /* 
                 * We have found the first repair packet on the 
                 * holdq, and thus we will attempt to construct the 
                 * RTP header from this first packet
                 */
                VQE_TAILQ_REMOVE(&in->holdq_lh, pak, inorder_obj);
                remove = TRUE;
                if (parent_chan != NULL) {
                    parent_chan->process_first_repair = TRUE;
                }
                err = construct_rtp_hdr_over_ts_app(parent_chan, pak);
                VQEC_DP_DEBUG(VQEC_DP_DEBUG_RCC,
                              "first repair arrived before APP"
                              "seq # = %u, @ time = %llu ms\n",
                              osn, TIME_GET_A(msec, get_sys_time()));
                if (err != VQEC_DP_ERR_OK) {
                    /* 
                     * In case of an error, RCC would have been aborted, and
                     * holdq would be flushed. We thus do not explicitly
                     * need to flush the holdq, as abort RCC takes care of that.
                     */
                    return;
                }
                sc_found = TRUE;
                wr = vqec_dp_chan_rtp_repair_input_stream_rcv_one(
                    (vqec_dp_chan_input_stream_t *)in, 
                    pak, 
                    ABS_TIME_0);
                if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                    in->in_stats.drops += 1 - wr;  /* postfix of drops */
                }
            } else { 
                /**
                 * Could be re-ordered packets, skip for next pak
                 */
            }
            
        } else {
            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                in->in_stats.drops++;  /* postfix of counter */
            }
            in->rtp_in_stats.rtp_parse_drops++;
            VQE_TAILQ_REMOVE(&in->holdq_lh, pak, inorder_obj);
            remove = TRUE;
        }
        
    next_pak:
        if (remove) {
            vqec_pak_free(pak);  /* drop packet reference */
            remove = FALSE;
        }
    }

    if (sc_found) {
        /* flush all other packs in holdq to PCM */
        vqec_dp_chan_rtp_repair_input_stream_process_holdq(in);
    } else {
        /**
         * The first repair sequence number is not present on queue,
         * activate a sequence number filter 
         */
        in->first_seqnum_fil = start_seq_num;
        in->first_seqnum_fil_act = TRUE;
    }
}

/**---------------------------------------------------------------------------
 * Drop all packets that may be queued onto a hold queue. Hold queues
 * are used for repair streams with rcc in use. 
 *
 * @param[in] in Pointer to a input stream.
 *--------------------------------------------------------------------------*/
static void
vqec_dp_chan_rtp_repair_input_stream_flushq (
    vqec_dp_chan_rtp_repair_input_stream_t *in)
{
    vqec_pak_t *pak, *pak_next;

    if (in) {
        VQE_TAILQ_FOREACH_SAFE(pak, &in->holdq_lh, inorder_obj, pak_next) {
            VQE_TAILQ_REMOVE(&in->holdq_lh, pak, inorder_obj);
            /* drop our reference on the packet. */
            vqec_pak_free(pak);
        }
    }
}


/**---------------------------------------------------------------------------
 * Set first sequence number filter on the repair stream. The holdQ is 
 * checked first to see if it has the desired first sequence present.
 *
 * @param[in] in Pointer to the repair input stream.
 * @param[in] first_seq First sequence number.
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_repair_input_stream_set_firstseq_fil (
    vqec_dp_chan_rtp_repair_input_stream_t *in, vqec_seq_num_t first_seq)
{
    if (in) {
        vqec_dp_chan_rtp_repair_input_stream_filter_holdq(in, first_seq);
    }
}


/**---------------------------------------------------------------------------
 * Clear first sequence number filter on the repair stream.
 *
 * @param[in] in Pointer to the repair input stream.
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_repair_input_stream_clear_firstseq_fil (
    vqec_dp_chan_rtp_repair_input_stream_t *in)
{
    if (in) {
        in->first_seqnum_fil = 0;
        in->first_seqnum_fil_act = FALSE;
    }
}


/**---------------------------------------------------------------------------
 * Flush hold Q.
 *
 * @param[in] in Pointer to the repair input stream.
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_repair_input_stream_flush_holdq (
    vqec_dp_chan_rtp_repair_input_stream_t *in)
{
    if (in) {
        vqec_dp_chan_rtp_repair_input_stream_flushq(in);
    }
}


/**---------------------------------------------------------------------------
 * Method that receives and processes a packet from the fec
 * stream as delivered by the input-shim. 
 *
 * @param[in] in Pointer to a dp channel input stream: in this case this
 * must be an instance of a fec input stream.
 * @param[in] pak Pointer to the packet which is to be processed.
 * @param[in] current_time The current system time. The value can
 * be left unspecified, i.e., 0, and is essentially used for optimization.
 * @param[out] uint32_t The method returns the number of packets
 * that passed RTP receive processing. For this particular variant, it
 * will thus return either zero or one.  The return value is for informational
 * purpose, and may be ignored by the caller.
 *---------------------------------------------------------------------------*/
static uint32_t
vqec_dp_chan_rtp_fec_input_stream_rcv_one (
    vqec_dp_chan_input_stream_t *in, 
    vqec_pak_t *pak,
    abs_time_t current_time)
{
    int32_t wr = 0;
    vqec_dp_chan_rtp_input_stream_t *rtp_is;

    if (!in || !pak) {
        /* no packets processed. */
        return (wr);
    }

    /* Initialize time argument if set to zero. */
    if (IS_ABS_TIME_ZERO(current_time)) {
        current_time = get_sys_time();
    }
    
    pak->rtp = (rtpfasttype_t *)vqec_pak_get_head_ptr(pak);
    rtp_is = (vqec_dp_chan_rtp_input_stream_t *)in;
    
    /* rtp process packet (partial), and insert into fec cache */
    if (vqec_dp_chan_rtp_process_fec_pak(rtp_is, pak, TRUE)) {

        rtp_is->rtp_in_stats.rtp_paks++;

        /* Ensure that the state machine allows for flow of these packets. */
        if (vqec_dpchan_pak_event(in->chan, VQEC_DP_CHAN_RX_FEC_PAK, 
                                  pak, current_time) ==
            VQEC_DP_CHAN_PAK_ACTION_ACCEPT) {
            
            pak->type = VQEC_PAK_TYPE_FEC;

            /* rtp packets delivered to the next element in the chain. */
            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                rtp_is->rtp_in_stats.pakseq_inserts++;
            }
            if (!vqec_fec_receive_packet(vqec_dpchan_fec_ptr(in->chan),  
                                         pak, in->handle)) {
                /* some packets may be dropped on insert */
                rtp_is->rtp_in_stats.pakseq_drops++;
            }
            wr++;
        }
    } else {
        
        /* packet dropped by rtp. */
        rtp_is->rtp_in_stats.rtp_parse_drops++;
    }

    return (wr);
}


/**---------------------------------------------------------------------------
 * Deconstruct a RTP primary input stream. 
 *
 * @param[in] in Pointer to the RTP primary input stream.
 *---------------------------------------------------------------------------*/
static void
vqec_dp_chan_rtp_primary_input_stream_deinit (
    vqec_dp_chan_rtp_primary_input_stream_t *in)
{
    if (in) {
        /* flush any packets pending on the failover queue. */
        vqec_dp_chan_rtp_process_failoverq(in, FALSE);

        /* deconstruct the super-class. */
        vqec_dp_chan_rtp_input_stream_deinit(
            (vqec_dp_chan_rtp_input_stream_t *)in);
    }
}

/**---------------------------------------------------------------------------
 * Deconstruct a RTP repair input stream. 
 *
 * @param[in] in Pointer to the RTP repair input stream.
 *---------------------------------------------------------------------------*/
static void
vqec_dp_chan_rtp_repair_input_stream_deinit (
    vqec_dp_chan_rtp_repair_input_stream_t *in)
{
    if (in) {
        /* flush any packets pending on the hold queue. */
        vqec_dp_chan_rtp_repair_input_stream_flushq(in);

        /* deconstruct the super-class. */
        vqec_dp_chan_rtp_input_stream_deinit(
            (vqec_dp_chan_rtp_input_stream_t *)in);
    }
}


/**---------------------------------------------------------------------------
 * Destroy a primary input stream: defined in base class.
 * 
 * @param[in] in Pointer to a (primary) input stream object.
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_primary_input_stream_destroy (vqec_dp_chan_input_stream_t *in)
{
    if (in) {
        vqec_dp_chan_rtp_primary_input_stream_deinit(
            (vqec_dp_chan_rtp_primary_input_stream_t *)in);

        /* increment debug counter. */
        VQEC_DP_TLM_CNT(input_stream_deletes, vqec_dp_tlm_get());
        zone_release(s_primary_is_pool, in);
    }
}


/**---------------------------------------------------------------------------
 * Destroy a repair input stream: defined in base class.
 * 
 * @param[in] in Pointer to a (repair) input stream object.
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_repair_input_stream_destroy (vqec_dp_chan_input_stream_t *in)
{
    if (in) {
        vqec_dp_chan_rtp_repair_input_stream_deinit(
            (vqec_dp_chan_rtp_repair_input_stream_t *)in);

        /* increment debug counter. */
        VQEC_DP_TLM_CNT(input_stream_deletes, vqec_dp_tlm_get());
        zone_release(s_repair_is_pool, in);
    }
}


/**---------------------------------------------------------------------------
 * Destroy a fec input stream: defined in base class.
 * 
 * @param[in] in Pointer to a (fec) input stream object.
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_fec_input_stream_destroy (vqec_dp_chan_input_stream_t *in)
{
    if (in) {
        vqec_dp_chan_rtp_input_stream_deinit(
            (vqec_dp_chan_rtp_input_stream_t *)in);

        /* increment debug counter. */
        VQEC_DP_TLM_CNT(input_stream_deletes, vqec_dp_tlm_get());
        zone_release(s_fec_is_pool, in);
    }
}


/**---------------------------------------------------------------------------
 * Override methods in the function table for the primary input stream class.
 *
 * @param[in] table Pointer to the rtp class function table.
 *---------------------------------------------------------------------------*/
static void 
vqec_dp_chan_rtp_primary_input_stream_overload_fcns (
    vqec_dp_chan_rtp_primary_input_stream_fcns_t *table) 
{
    table->process_vector = 
        vqec_dp_chan_rtp_primary_input_stream_rcv_vec;
    table->destroy = 
        vqec_dp_chan_rtp_primary_input_stream_destroy;
    
}


/**---------------------------------------------------------------------------
 * Override methods in the function table for the repair input stream class.
 *
 * @param[in] table Pointer to the rtp class function table.
 *---------------------------------------------------------------------------*/
static void 
vqec_dp_chan_rtp_repair_input_stream_overload_fcns (
    vqec_dp_chan_rtp_repair_input_stream_fcns_t *table) 
{
    table->process_one = 
        vqec_dp_chan_rtp_repair_input_stream_rcv_one;
    table->destroy = 
        vqec_dp_chan_rtp_repair_input_stream_destroy;
}


/**---------------------------------------------------------------------------
 * Override methods in the function table for the fec input stream class.
 *
 * @param[in] table Pointer to the rtp class function table.
 *---------------------------------------------------------------------------*/
static void 
vqec_dp_chan_rtp_fec_input_stream_overload_fcns (
    vqec_dp_chan_rtp_input_stream_fcns_t *table) 
{
    table->process_one = 
        vqec_dp_chan_rtp_fec_input_stream_rcv_one;
    table->destroy = 
        vqec_dp_chan_rtp_fec_input_stream_destroy;
}

/**---------------------------------------------------------------------------
 * Setup the function table for the rtp primary input stream class.
 *
 * @param[in] table Pointer to the rtp primary class function table.
 *---------------------------------------------------------------------------*/
void 
vqec_dp_chan_rtp_primary_input_stream_set_fcns (
    vqec_dp_chan_rtp_primary_input_stream_fcns_t *table) 
{
    vqec_dp_chan_rtp_input_stream_set_fcns(
        (vqec_dp_chan_rtp_input_stream_fcns_t *)table);
}

/**---------------------------------------------------------------------------
 * Setup the function table for the rtp repair input stream class.
 *
 * @param[in] table Pointer to the rtp repair class function table.
 *---------------------------------------------------------------------------*/
void 
vqec_dp_chan_rtp_repair_input_stream_set_fcns (
    vqec_dp_chan_rtp_repair_input_stream_fcns_t *table) 
{
    vqec_dp_chan_rtp_input_stream_set_fcns(
        (vqec_dp_chan_rtp_input_stream_fcns_t *)table);
}


/**---------------------------------------------------------------------------
 * Input stream methods.
 *---------------------------------------------------------------------------*/


/**---------------------------------------------------------------------------
 * Instance vector..
 *
 * @param[in]  is Input stream id.
 * @param[out] instance Pointer to output instance structure.
 * @param[out] vqec_dp_stream_err_t Returns STREAM_ERR_OK on success.
 *---------------------------------------------------------------------------*/
vqec_dp_stream_err_t 
vqec_dp_chan_rtp_iostream_get_instance (vqec_dp_isid_t is,
                                        vqec_dp_is_instance_t *instance) 
{
    vqec_dp_chan_rtp_input_stream_t *in;
    vqec_dp_stream_err_t err = VQEC_DP_STREAM_ERR_OK;

    if (is == VQEC_DP_INVALID_ISID || !instance) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }

    memset(instance, 0, sizeof(*instance));
    in = vqec_dp_chan_input_stream_id_lookup(&s_id_table, is);
    if (!in) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }
    
    err = vqec_dp_chan_input_stream_get_instance(
        (vqec_dp_chan_input_stream_t *)in, instance);
    
    return (err);
}


/**---------------------------------------------------------------------------
 * Capability vector.
 *
 * @param[in]  is Input stream id. 
 * @param[out] int32_t Capabilities that are supported. 
 *---------------------------------------------------------------------------*/
static int32_t 
vqec_dp_chan_rtp_iostream_capa (vqec_dp_isid_t is)
{
    vqec_dp_chan_rtp_input_stream_t *in;
    int32_t capa;

    if (is == VQEC_DP_INVALID_ISID) {
        return (VQEC_DP_STREAM_CAPA_NULL);
    }
    
    in = vqec_dp_chan_input_stream_id_lookup(&s_id_table, is);
    if (!in) {
        return (VQEC_DP_STREAM_CAPA_NULL);
    }
    
    capa = 
        vqec_dp_chan_input_stream_get_capa((vqec_dp_chan_input_stream_t *)in);
    return (capa);
}


/**---------------------------------------------------------------------------
 * Receive a primary packet vector from an output stream, and deliver it to the
 * next element in the chain.
 *
 * @param[in] is Input stream id. 
 * @param[in] pakvec Pointer to the packet vector.
 * @param[in] vecum Number of packets in the vector.
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK
 * if the packet was delivered to the input stream (packet may be dropped by
 * the input stream).
 *---------------------------------------------------------------------------*/
static vqec_dp_stream_err_t
vqec_dp_chan_rtp_primary_iostream_rcv_vec_internal (vqec_dp_isid_t is,
                                                    vqec_pak_t *pakvec[],
                                                    uint32_t vecnum)
{
/*
 * This is used to add to the reorder time when detecting underruns, so that
 * packets that are only slightly more than reorder time apart (due to
 * scheduling) do not trigger encapsulation autodetection.
 */
#define UNDERRUN_DETECT_FUDGE_MS (MSECS(20))
    vqec_dp_chan_rtp_input_stream_t *in;
    vqec_dp_stream_err_t err = VQEC_DP_STREAM_ERR_OK;
    vqec_dp_error_t err_stun;
    uint32_t rtp_drops;
    vqec_dp_oscheduler_t *osched;
    vqec_dpchan_t *dpchan;
    vqec_dp_isid_t isid;
    const vqec_dp_isops_t *isops;
    int stream_idx;
    int i = 0;
    int num_paks = 0; /* none-stun paks when encap is not RTP */
    vqec_pak_t *pakvec_act[VQEC_DP_STREAM_PUSH_VECTOR_PAKS_MAX];
    
    if ((is == VQEC_DP_INVALID_ISID) || 
        !pakvec || 
        (vecnum == 0) ||
        (vecnum > VQEC_DP_STREAM_PUSH_VECTOR_PAKS_MAX)) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }

    in = vqec_dp_chan_input_stream_id_lookup(&s_id_table, is);
    if (!in || in->type != VQEC_DP_INPUT_STREAM_TYPE_PRIMARY) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }

    in->in_stats.packets += vecnum;
    /* ignore if stream is shutdown . */ 
    if (!in->act) {
        if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
            in->in_stats.drops += vecnum;
        }
        return (err);
    }

    /*
     * dpchan->passthru is set from the value of chan_cfg->passthru, which is
     * set in vqec_ifclient_tuner_bind_chan_ul(), and then this value is
     * passed all the way down to here.  The conditions for setting this value
     * are as follows:
     *
     * 1.  The channel is a dynamic channel (i.e., a RTP URL was supplied and
     *     rtp_integrated_fallback is enabled, or a UDP URL was supplied and
     *     udp_passthru_support is enabled).
     * 2.  If the supplied URL was RTP, then ER must be disabled in the
     *     channel configuration.  This should always be the case when an
     *     integration simply binds to a URL without creating a custom config.
     */

    dpchan = ((vqec_dp_chan_input_stream_t *)in)->chan;
    /* monitor for underruns by looking at recv time of sequential packets */
    if (dpchan->passthru) {
        if (TIME_CMP_A(lt,
                       in->last_pak_ts,
                       TIME_SUB_A_R(pakvec[0]->rcv_ts,
                                    TIME_ADD_R_R(dpchan->reorder_time,
                                                 UNDERRUN_DETECT_FUDGE_MS))) ||
            (in->encap == VQEC_DP_ENCAP_UNKNOWN)) {

            /*
             * Detect the encapsulation type on the first packet.  If the
             * encapsulation type is detected as RTP, then the process_vector
             * method of the input stream will call rtp_header_validate() on the
             * full header to make sure it is valid, and increment the invalid
             * RTP counter if it is not.
             * If the detected encap is UNKNOWN, it keeps on detecting until
             * find the first UDP or RTP pak. 
             */
            i = 0;
            do {
                /* need to test every pack if ENCAP_UNKNOWN */
                in->encap = 
                    vqec_dp_detect_encap(vqec_pak_get_head_ptr(pakvec[i]));
                i++;
            } while((in->encap == VQEC_DP_ENCAP_UNKNOWN) && (i < vecnum));
        }
 
        /* detect STUN packet first, in UDP and UNKNOWN cases */
        num_paks = 0;
        if (in->encap != VQEC_DP_ENCAP_RTP) {
            for (i = 0; i < vecnum; i++) {
                /* Check for stun packet: punt to control plane. */
                if ((vqec_pak_get_head_ptr(pakvec[i])[0] & 0xC0) == 0) {
                    err_stun = vqec_dpchan_eject_pak(in->chan, in->id, 
                                                     pakvec[i]);
                    if (err_stun == VQEC_DP_ERR_OK) {
                        VQEC_DP_TLM_CNT(input_stream_eject_paks, 
                                        vqec_dp_tlm_get());
                    } else {
                        VQEC_DP_TLM_CNT(input_stream_eject_paks_failure, 
                                        vqec_dp_tlm_get());
                    }
                    continue;
                } 
                pakvec_act[num_paks] = pakvec[i];
                num_paks++;
            }
        }
        /*
         * only log none STUN packets, if all are STUN paks,
         * wait for next cycle. 
         */ 
        if (num_paks) {
            in->last_pak_ts = pakvec[num_paks - 1]->rcv_ts;
            in->chan->prim_inactive = FALSE;
        }

        switch (in->encap) {
            case VQEC_DP_ENCAP_UDP:
                /* mark paks as UDP so they're handled correctly in outputq */
                for (i = 0; i < vecnum; i++) {
                    if (pakvec[i]) {
                        pakvec[i]->type = VQEC_PAK_TYPE_UDP;
                    }
                }

                /* skip the dpchan module and insert right into outputshim */
                osched = &dpchan->pcm.osched;

                /* process pkt for each input stream connected to the osched */
                for (stream_idx = 0;
                     stream_idx < VQEC_DP_OSCHED_STREAMS_MAX;
                     stream_idx++) {
                    isops = osched->streams.isops[stream_idx];
                    isid = osched->streams.isids[stream_idx];

                    /* push packet into connected input stream */
                    if (isops && (isid != VQEC_DP_INVALID_ISID)) {
                        isops->receive_vec(isid, pakvec_act, num_paks);
                    }    
                }
                in->rtp_in_stats.udp_paks += num_paks;
                break;
            case VQEC_DP_ENCAP_RTP:
                /* insert into the dpchan module via rtp receiver as normal */
                rtp_drops = MCALL((vqec_dp_chan_input_stream_t *)in, 
                                  process_vector, pakvec, vecnum, ABS_TIME_0);
                if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                    in->in_stats.drops += rtp_drops;
                }
                if (rtp_drops < vecnum) {
                    in->last_pak_ts = pakvec[vecnum - 1]->rcv_ts;
                    in->chan->prim_inactive = FALSE;
                }

                break;
            case VQEC_DP_ENCAP_UNKNOWN:
                 in->rtp_in_stats.udp_mpeg_sync_drops += num_paks;
                /* FALLTHRU */
            default:
                if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                    in->in_stats.drops += num_paks;
                }
        }  /* end of switch */
    } /* end of if (dpchan->passthru) */
    else {
        /*
         * NOT a passthru channel.
         *
         * Insert into the dpchan module via the rtp receiver as normal.
         */
        rtp_drops = MCALL((vqec_dp_chan_input_stream_t *)in, 
                          process_vector, pakvec, vecnum, ABS_TIME_0);
        if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
            in->in_stats.drops += rtp_drops;
        }
        if (rtp_drops < vecnum) {
            in->last_pak_ts = pakvec[vecnum - 1]->rcv_ts;
            in->chan->prim_inactive = FALSE;
        }
    }

    return (VQEC_DP_STREAM_ERR_OK);    
}


/**---------------------------------------------------------------------------
 * Receive one primary packet from an output stream, and deliver it to the
 * next element in the chain.
 *
 * @param[in] is Input stream id. 
 * @param[in] pak Pointer to the packet.
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK
 * if the packet was delivered to the input stream (packet may be dropped by
 * the input stream).
 *---------------------------------------------------------------------------*/
static vqec_dp_stream_err_t
vqec_dp_chan_rtp_primary_iostream_rcv_one (vqec_dp_isid_t is,
                                           vqec_pak_t *pak)
{
    vqec_pak_t *pakvec[1];

    if (!pak) {
        return (VQEC_DP_STREAM_ERR_INVALIDARGS);
    }

    pakvec[0] = pak;
    return (vqec_dp_chan_rtp_primary_iostream_rcv_vec_internal(is, pakvec, 1));
}


/**---------------------------------------------------------------------------
 * Where not implemented..
 *---------------------------------------------------------------------------*/
static vqec_dp_stream_err_t
vqec_dp_chan_rtp_iostream_rcv_vec_assert (vqec_dp_isid_t is,
                                          vqec_pak_t *pakvec[], uint32_t vecnum)
{
    VQEC_DP_ASSERT_FATAL(FALSE, "not implemented");
    return (VQEC_DP_STREAM_ERR_INVALIDARGS);
}


/**---------------------------------------------------------------------------
 * Receive a primary packet vector from an output stream, and deliver it to the
 * next element in the chain.
 *
 * @param[in] is Input stream id. 
 * @param[in] pakvec Pointer to the packet vector.
 * @param[in] vecum Number of packets in the vector.
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK
 * if the packet was delivered to the input stream (packet may be dropped by
 * the input stream).
 *---------------------------------------------------------------------------*/
static vqec_dp_stream_err_t
vqec_dp_chan_rtp_primary_iostream_rcv_vec (vqec_dp_isid_t is,
                                           vqec_pak_t *pakvec[],
                                           uint32_t vecnum)
{
    return (vqec_dp_chan_rtp_primary_iostream_rcv_vec_internal(
                is, pakvec, vecnum));
}

/**---------------------------------------------------------------------------
 * Receive one repair packet from an output stream, and deliver it to the
 * next element in the chain.
 *
 * @param[in] is Input stream id. 
 * @param[in] pak Pointer to the packet.
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK
 * if the packet was delivered to the input stream (packet may be dropped by
 * the input stream).
 *---------------------------------------------------------------------------*/
static vqec_dp_stream_err_t
vqec_dp_chan_rtp_repair_iostream_rcv_one (vqec_dp_isid_t is,
                                          vqec_pak_t *pak)
{
    vqec_dp_chan_rtp_input_stream_t *in;
    vqec_dp_stream_err_t err = VQEC_DP_STREAM_ERR_OK;
    uint32_t wr;

    if (is == VQEC_DP_INVALID_ISID || !pak) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }

    in = vqec_dp_chan_input_stream_id_lookup(&s_id_table, is);
    if (!in || in->type != VQEC_DP_INPUT_STREAM_TYPE_REPAIR) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }
 
    in->in_stats.packets++;
    /* ignore if stream is shutdown . */ 
    if (!in->act) {
        if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
            in->in_stats.drops++;
        }
        return (err);
    }
   
    wr = MCALL((vqec_dp_chan_input_stream_t *)in, 
               process_one, pak, ABS_TIME_0);
    if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
        in->in_stats.drops += 1 - wr;
    }
    
    return (err);
}


/**---------------------------------------------------------------------------
 * Receive one fec packet from an output stream, and deliver it to the
 * next element in the chain.
 *
 * @param[in] is Input stream id. 
 * @param[in] pak Pointer to the packet.
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK
 * if the packet was delivered to the input stream (packet may be dropped by
 * the input stream).
 *---------------------------------------------------------------------------*/
static vqec_dp_stream_err_t
vqec_dp_chan_rtp_fec_iostream_rcv_one (vqec_dp_isid_t is,
                                       vqec_pak_t *pak)
{
    vqec_dp_chan_rtp_input_stream_t *in;
    vqec_dp_stream_err_t err = VQEC_DP_STREAM_ERR_OK;
    uint32_t wr;

    if (is == VQEC_DP_INVALID_ISID || !pak) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }

    if (!vqec_dp_get_fec_inline()) {
        /*
         * If FEC has been disabled via CLI, abort processing any FEC packets.
         */
        return (err);
    }

    in = vqec_dp_chan_input_stream_id_lookup(&s_id_table, is);
    if (!in || (in->type != VQEC_DP_INPUT_STREAM_TYPE_FEC)) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }

    in->in_stats.packets++;
    /* ignore if stream is shutdown . */ 
    if (!in->act) {
        if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
            in->in_stats.drops++;
        }
        return (err);
    }
    
    wr = MCALL((vqec_dp_chan_input_stream_t *)in, 
               process_one, pak, ABS_TIME_0);
    if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
        in->in_stats.drops += 1 - wr;
    }
       
    return (err);
}


/**---------------------------------------------------------------------------
 * Encapsulation for the input stream.
 *
 * @param[in] is Input stream id. 
 * @param[out] vqec_dp_encap_type_t Supported encapsulation type.
 *---------------------------------------------------------------------------*/
static vqec_dp_encap_type_t
vqec_dp_chan_rtp_iostream_encap (vqec_dp_isid_t is) 
{    
    if ((is != VQEC_DP_INVALID_ISID) && 
        (vqec_dp_chan_input_stream_id_lookup(&s_id_table, is) != NULL)) {
        return (VQEC_DP_ENCAP_RTP);
    }

    return (VQEC_DP_ENCAP_UNKNOWN);
}


/**---------------------------------------------------------------------------
 * Get the status for an input stream.
 *
 * @param[in] is Input stream id. 
 * @param[out] stat Pointer to the status structure.
 * @param[in] cumulative Boolean flag to retrieve cumulative status.
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK
 * if a valid stream's status is returned.
 *---------------------------------------------------------------------------*/
static vqec_dp_stream_err_t
vqec_dp_chan_rtp_iostream_status (vqec_dp_isid_t is, 
                                  vqec_dp_is_status_t *status, 
                                  boolean cumulative)
{
    vqec_dp_chan_rtp_input_stream_t *in;
    vqec_dp_stream_err_t err = VQEC_DP_STREAM_ERR_OK;
    
    if (is == VQEC_DP_INVALID_ISID || !status) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }

    memset(status, 0, sizeof(*status));
    in = vqec_dp_chan_input_stream_id_lookup(&s_id_table, is);
    if (!in) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }

    err = 
        vqec_dp_chan_input_stream_get_status((vqec_dp_chan_input_stream_t *)in,
                                             status, cumulative); 
    return (err);
}

                             
/**---------------------------------------------------------------------------
 * Connect the input stream to an output stream.
 *
 * @param[in] is Input stream id. 
 * @param[in] os OS identifier with which to connect the IS.
 * @param[in] ops Pointer to the OS operations function vector.
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK
 * if a valid stream's status is returned.
 *---------------------------------------------------------------------------*/
static vqec_dp_stream_err_t 
vqec_dp_chan_rtp_iostream_connect (vqec_dp_isid_t is,
                                   vqec_dp_osid_t os, 
                                   const struct vqec_dp_osops_ *ops)
{
    vqec_dp_chan_rtp_input_stream_t *in;
    vqec_dp_stream_err_t err = VQEC_DP_STREAM_ERR_OK;

    if ((is == VQEC_DP_INVALID_ISID) || 
        (os == VQEC_DP_INVALID_OSID) || !ops) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }
    
    in = vqec_dp_chan_input_stream_id_lookup(&s_id_table, is);
    if (!in) {
        err = VQEC_DP_STREAM_ERR_INVALIDARGS;
        return (err);
    }
    
    err = MCALL((vqec_dp_chan_input_stream_t *)in, connect_os, os, ops); 
    return (err);
}
                             

/**---------------------------------------------------------------------------
 * Primary IS operations table..
 *---------------------------------------------------------------------------*/
static vqec_dp_isops_t vqec_dp_rtp_primary_isops =
{
    .capa = vqec_dp_chan_rtp_iostream_capa,
    .initiate_connect = vqec_dp_chan_rtp_iostream_connect,
    .receive = vqec_dp_chan_rtp_primary_iostream_rcv_one,
    .receive_vec = vqec_dp_chan_rtp_primary_iostream_rcv_vec,
    .get_status = vqec_dp_chan_rtp_iostream_status,
    .encap = vqec_dp_chan_rtp_iostream_encap
};


/**---------------------------------------------------------------------------
 * Repair IS operations table..
 *---------------------------------------------------------------------------*/
static vqec_dp_isops_t vqec_dp_rtp_repair_isops = 
{
    .capa = vqec_dp_chan_rtp_iostream_capa,
    .initiate_connect = vqec_dp_chan_rtp_iostream_connect,
    .receive = vqec_dp_chan_rtp_repair_iostream_rcv_one,
    .receive_vec = vqec_dp_chan_rtp_iostream_rcv_vec_assert,
    .get_status = vqec_dp_chan_rtp_iostream_status,
    .encap = vqec_dp_chan_rtp_iostream_encap
};


/**---------------------------------------------------------------------------
 * FEC IS operations table..
 *---------------------------------------------------------------------------*/
static vqec_dp_isops_t vqec_dp_rtp_fec_isops = 
{
    .capa = vqec_dp_chan_rtp_iostream_capa,
    .initiate_connect = vqec_dp_chan_rtp_iostream_connect,
    .receive = vqec_dp_chan_rtp_fec_iostream_rcv_one,
    .receive_vec = vqec_dp_chan_rtp_iostream_rcv_vec_assert,
    .get_status = vqec_dp_chan_rtp_iostream_status,
    .encap = vqec_dp_chan_rtp_iostream_encap
};



/**---------------------------------------------------------------------------
 * Create a new primary input stream and return a pointer to the
 * newly created object. 
 *
 * @param[in] chan Pointer to the parent channel object.
 * @param[in] s_desc Input stream description.
 * @param[out] vqec_dp_isid_t Identifier of the primary input stream object.
 *---------------------------------------------------------------------------*/
vqec_dp_isid_t
vqec_dp_chan_rtp_primary_input_stream_create (vqec_dpchan_t *chan,
                                              vqec_dp_input_stream_t *s_desc)
{
    vqec_dp_chan_rtp_primary_input_stream_t * _instance;

    if (!chan) {
        return (VQEC_DP_INVALID_ISID);
    }

    _instance = zone_acquire(s_primary_is_pool);
    if (!_instance) {
        return (VQEC_DP_INVALID_ISID);
    }

    memset(_instance, 0, sizeof(*_instance));
    
    /* invoke super-class constructor. */
    if (!vqec_dp_chan_rtp_input_stream_init(
            (vqec_dp_chan_rtp_input_stream_t *)_instance, chan, s_desc)) {

        zone_release(s_primary_is_pool, _instance);
        return (VQEC_DP_INVALID_ISID);
    }
    
    _instance->type = VQEC_DP_INPUT_STREAM_TYPE_PRIMARY;
    _instance->capa = 
        VQEC_DP_STREAM_CAPA_PUSH | 
        VQEC_DP_STREAM_CAPA_PUSH_POLL | 
        VQEC_DP_STREAM_CAPA_PUSH_VECTORED;
    _instance->ops = &vqec_dp_rtp_primary_isops;
    VQE_TAILQ_INIT(&_instance->failoverq_lh);

    /* override base class methods in the function table. */
    _instance->__func_table = &the_prim_is_func_table;

    /* increment debug counter. */
    VQEC_DP_TLM_CNT(input_stream_creates, vqec_dp_tlm_get());

    return (_instance->id);
}


/**---------------------------------------------------------------------------
 * Create a new repair input stream and return a pointer to the
 * newly created object. 
 *
 * @param[in] chan Pointer to the parent channel object.
 * @param[in] s_desc Input stream description.
 * @param[out] vqec_dp_isid_t Identifier of the repair input stream object.
 *---------------------------------------------------------------------------*/
vqec_dp_isid_t
vqec_dp_chan_rtp_repair_input_stream_create (vqec_dpchan_t *chan,
                                             vqec_dp_input_stream_t *s_desc)
{
    vqec_dp_chan_rtp_repair_input_stream_t * _instance;

    if (!chan) {
        return (VQEC_DP_INVALID_ISID);
    }

    _instance = zone_acquire(s_repair_is_pool);
    if (!_instance) {
        return (VQEC_DP_INVALID_ISID);
    }

    memset(_instance, 0, sizeof(*_instance));

    /* invoke super-class constructor. */
    if (!vqec_dp_chan_rtp_input_stream_init(
            (vqec_dp_chan_rtp_input_stream_t *)_instance, chan, s_desc)) {

        zone_release(s_repair_is_pool, _instance);
        return (VQEC_DP_INVALID_ISID);
    }
    
    _instance->type = VQEC_DP_INPUT_STREAM_TYPE_REPAIR;
    _instance->capa = VQEC_DP_STREAM_CAPA_PUSH |
        VQEC_DP_STREAM_CAPA_PUSH_POLL;    
    _instance->ops = &vqec_dp_rtp_repair_isops;
    VQE_TAILQ_INIT(&_instance->holdq_lh);

    /* override base class methods in the function table. */
    _instance->__func_table = &the_repair_is_func_table;

    /* increment debug counter. */
    VQEC_DP_TLM_CNT(input_stream_creates, vqec_dp_tlm_get());

    return (_instance->id);
}


/**---------------------------------------------------------------------------
 * Create a new fec input stream and return a pointer to the
 * newly created object. 
 *
 * @param[in] chan Pointer to the parent channel object.
 * @param[in] s_desc Input stream description.
 * @param[out] vqec_dp_isid_t Identifier of the fec input stream object.
 *---------------------------------------------------------------------------*/
vqec_dp_isid_t
vqec_dp_chan_rtp_fec_input_stream_create (vqec_dpchan_t *chan,
                                          int32_t handle,
                                          vqec_dp_input_stream_t *s_desc)
{
    vqec_dp_chan_rtp_fec_input_stream_t * _instance;

    if (!chan) {
        return (VQEC_DP_INVALID_ISID);
    }

    _instance = zone_acquire(s_fec_is_pool);
    if (!_instance) {
        return (VQEC_DP_INVALID_ISID);
    }

    memset(_instance, 0, sizeof(*_instance));

    /* invoke super-class constructor. */
    if (!vqec_dp_chan_rtp_input_stream_init(
            (vqec_dp_chan_rtp_input_stream_t *)_instance, chan, s_desc)) {

        zone_release(s_fec_is_pool, _instance);
        return (VQEC_DP_INVALID_ISID);
    }
    
    _instance->type = VQEC_DP_INPUT_STREAM_TYPE_FEC;
    _instance->capa = VQEC_DP_STREAM_CAPA_PUSH |
        VQEC_DP_STREAM_CAPA_PUSH_POLL;
    _instance->ops = &vqec_dp_rtp_fec_isops;
    _instance->handle = handle;

    /* override base class methods in the function table. */
    _instance->__func_table = &the_fec_is_func_table;

    /* increment debug counter. */
    VQEC_DP_TLM_CNT(input_stream_creates, vqec_dp_tlm_get());

    return (_instance->id);
}


/**---------------------------------------------------------------------------
 * Initialize the RTP subsystem.
 *
 * @param[in] params Initialization parameters.
 * @param[out] vqec_dp_error_t VQEC_DP_ERR_OK on success. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_chan_rtp_module_init (vqec_dp_module_init_params_t *params)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    uint32_t id_blocks;

    if (!params ||
        !params->max_channels ||
        !params->max_streams_per_channel) {
        err = VQEC_DP_ERR_INVALIDARGS;
        goto bail;
    }

    if (s_id_table.total_ids) {
        err = VQEC_DP_ERR_ALREADY_INITIALIZED;
        goto bail;
    }

    id_blocks = ((params->max_channels * 
                 params->max_streams_per_channel) +
                 (VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK - 1)) /
        VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK; 

    s_id_table.total_ids = id_blocks * 
        VQEC_DP_RTP_IS_IDTABLE_IDS_PER_BLOCK;
    
    s_bitpos_pool = zone_instance_get_loc(
                            "bitpos_pool",
                            O_CREAT,
                            sizeof(rtp_id_table_block_t) * id_blocks,
                            1, NULL, NULL);
    if (!s_bitpos_pool) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }
    s_id_table.bitpos = (rtp_id_table_block_t *) zone_acquire(s_bitpos_pool);
    if (!s_id_table.bitpos) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }
    memset(s_id_table.bitpos, 0,
           sizeof(rtp_id_table_block_t) * id_blocks);

    s_is_ptr_pool = zone_instance_get_loc(
                        "is_ptr_pool",
                        O_CREAT,
                        sizeof(vqec_dp_chan_rtp_input_stream_t *) 
                            * s_id_table.total_ids,
                        1, NULL, NULL);
    if (!s_is_ptr_pool) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }
    s_id_table.is_ptr = 
            (vqec_dp_chan_rtp_input_stream_t **)zone_acquire(s_is_ptr_pool);
    if (!s_id_table.is_ptr) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }
    memset(s_id_table.is_ptr, 0,
           sizeof(vqec_dp_chan_rtp_input_stream_t *) * s_id_table.total_ids);

    err = vqec_dp_rtp_receiver_module_init(params);
    if (err != VQEC_DP_ERR_OK) {
        goto bail;
    }

    vqec_dp_chan_rtp_primary_input_stream_set_fcns(&the_prim_is_func_table);
    vqec_dp_chan_rtp_input_stream_set_fcns(&the_fec_is_func_table);
    vqec_dp_chan_rtp_repair_input_stream_set_fcns(
        &the_repair_is_func_table);
                
    vqec_dp_chan_rtp_primary_input_stream_overload_fcns(
        &the_prim_is_func_table);
    vqec_dp_chan_rtp_repair_input_stream_overload_fcns(
        &the_repair_is_func_table);
    vqec_dp_chan_rtp_fec_input_stream_overload_fcns(
        &the_fec_is_func_table); 
    
    vqec_dp_chan_input_id_table_reset(&s_id_table);

    s_primary_is_pool = zone_instance_get_loc(
                            "prim_is_pool",
                            O_CREAT,
                            sizeof(vqec_dp_chan_rtp_primary_input_stream_t),
                            params->max_channels,
                            NULL, NULL);
    if (!s_primary_is_pool) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }

    s_repair_is_pool = zone_instance_get_loc(
                            "er_is_pool",
                            O_CREAT,
                            sizeof(vqec_dp_chan_rtp_repair_input_stream_t),
                            params->max_channels,
                            NULL, NULL);
    if (!s_repair_is_pool) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }

    s_fec_is_pool = zone_instance_get_loc(
                            "fec_is_pool",
                            O_CREAT,
                            sizeof(vqec_dp_chan_rtp_fec_input_stream_t),
                            params->max_channels * 2,
                            NULL, NULL);
    if (!s_fec_is_pool) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }

  bail:
    if (err != VQEC_DP_ERR_OK &&
        err != VQEC_DP_ERR_ALREADY_INITIALIZED) {
        vqec_dp_chan_rtp_module_deinit();
    }

    return (err);
}


/**---------------------------------------------------------------------------
 * Deinitialize the RTP subsystem.
 *---------------------------------------------------------------------------*/
void
vqec_dp_chan_rtp_module_deinit (void)
{
    vqec_dp_chan_input_id_table_flush(&s_id_table);
    vqec_dp_rtp_receiver_module_deinit();
    if (s_id_table.bitpos) {
        zone_release(s_bitpos_pool, s_id_table.bitpos);
        s_id_table.bitpos = NULL;
    }
    if (s_bitpos_pool) {
        (void) zone_instance_put(s_bitpos_pool);
        s_bitpos_pool = NULL;
    }
    if (s_id_table.is_ptr) {
        zone_release(s_is_ptr_pool, s_id_table.is_ptr);
        s_bitpos_pool = NULL;
    }
    if (s_is_ptr_pool) {
        (void) zone_instance_put(s_is_ptr_pool);
        s_is_ptr_pool = NULL;
    }
    memset(&s_id_table, 0, sizeof(s_id_table));
    if (s_primary_is_pool) {
        (void) zone_instance_put(s_primary_is_pool);
        s_primary_is_pool = NULL;
    }
    if (s_repair_is_pool) {
        (void) zone_instance_put(s_repair_is_pool);
        s_repair_is_pool = NULL;
    }
    if (s_fec_is_pool) {
        (void) zone_instance_put(s_fec_is_pool);
        s_fec_is_pool = NULL;
    }
}


/**---------------------------------------------------------------------------
 * Id-to-ptr method.
 *---------------------------------------------------------------------------*/
vqec_dp_chan_input_stream_t *
vqec_dp_chan_input_stream_id_to_ptr (vqec_dp_isid_t id)
{
    vqec_dp_chan_rtp_input_stream_t *in;

    if (id == VQEC_DP_INVALID_ISID) {
        return (NULL);
    }

    in = vqec_dp_chan_input_stream_id_lookup(&s_id_table, id);
    return ((vqec_dp_chan_input_stream_t *)in);
}


/**---------------------------------------------------------------------------
 * Upcall event handler.
 *
 * @param[in] in Pointer to a rtp input stream object.
 * @param[in] src Source which triggered the event (this parameter is ignored
 * in the interrupt driven model).
 * @param[in] ev Type of local event generated.
 * @param[out] vqec_dp_error_t VQEC_DP_ERR_OK if the event is sucessfully
 * enQ'ed for delivery to the control-plane.
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_chan_rtp_input_stream_src_ev (
    vqec_dp_chan_rtp_input_stream_t *in,
    vqec_dp_rtp_src_t *src, vqec_dp_upcall_irq_reason_code_t ev)
{    
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_upcall_device_t dev;

    if (!in || (ev <= VQEC_DP_UPCALL_REASON_INVALID) ||
        (ev >= VQEC_DP_UPCALL_REASON_MAX)) {
        
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    /* Proxy the event to the channel for sending the IRQ. */
    switch (in->type) {
    case VQEC_DP_INPUT_STREAM_TYPE_PRIMARY:
        dev = VQEC_DP_UPCALL_DEV_RTP_PRIMARY;
        break;

    case VQEC_DP_INPUT_STREAM_TYPE_REPAIR:
        dev = VQEC_DP_UPCALL_DEV_RTP_REPAIR;
        break;

    default:
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);        
    }

    err = vqec_dpchan_tx_upcall_ev(in->chan, dev, in->id, ev);
    return (err);
}
