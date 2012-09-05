/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: A null NAT protocol implementation. This protocol simply maps the 2-tuple
 * <external IP address, external IP port> to <internal IP address, internal IP port>, and
 * simplifies internal implementation for STD and NAT modes.
 *
 * Documents: 
 *
 *****************************************************************************/

#include <stunclient/vqec_nat_api.h>
#include <utils/vam_util.h>
#include <utils/queue_plus.h>
#include <utils/id_manager.h>
#include <utils/zone_mgr.h>
#include <string.h>

typedef struct vqe_zone vqec_nat_natproto_pool_t;

typedef
struct vqec_nat_natproto_bind_
{
    /**
     * Identifier of the binding.
     */
    idmgr_id_t id;
    /**
     * NAT parameters descriptor.
     */
    vqec_nat_bind_desc_t desc;
    /**
     * External mapped IP addr.
     */
    in_addr_t ext_addr;
    /**
     * External mapped IP port (Network byte order).
     */
    in_port_t ext_port;    
    /**
     * Bind state.
     */
    vqec_nat_bind_state_t state;    
    /**
     * Is current external map valid?
     */
    boolean is_map_valid;    
    /**
     * Thread onto a list.
     */
    VQE_TAILQ_ENTRY(vqec_nat_natproto_bind_) le;
                                         
} vqec_nat_natproto_bind_t;

#define NATPROTO_HASH_BUCKETS 8
#define NATPROTO_HASH_MASK (NATPROTO_HASH_BUCKETS - 1)

typedef VQE_TAILQ_HEAD(nat_list_, vqec_nat_natproto_bind_) nat_list_t;

typedef
struct vqec_nat_natproto_t
{
    /**
     * If properly initialized or not.
     */
    boolean init_done;
    /**
     * Is debugging turned on?
     */
    boolean debug_en;
    /**
     * Is debugging verbose?
     */
    boolean verbose;
    /**
     * Id table.
     */
    id_table_key_t id_table;
    /**
     * Maximum limit.
     */
    uint32_t max_bindings;
    /**
     * Refresh interval.
     */
    uint32_t refresh_interval;
    /**
     * Presently open bindings.
     */
    uint32_t open_bindings;
    /**
     * Element pool
     */
    vqec_nat_natproto_pool_t *pool;
    /**
     * A hash-table for bindings.
     */
    nat_list_t lh[NATPROTO_HASH_BUCKETS];

} vqec_nat_natproto_t;

static vqec_nat_natproto_t s_natproto;


/**---------------------------------------------------------------------------
 * Simple hash function on ports.
 *---------------------------------------------------------------------------*/  
static inline uint16_t
vqec_nat_proto_hash (uint16_t port) 
{
    return (port & NATPROTO_HASH_MASK);
}


/**---------------------------------------------------------------------------
 * Destroy the protocol, and release all allocated objects.
 *---------------------------------------------------------------------------*/  
static void 
vqec_nat_natproto_destroy (void)
{
    int32_t i;
    vqec_nat_natproto_bind_t *b_cur, *b_nxt;

    for (i = 0; i < NATPROTO_HASH_BUCKETS; i++) {
        VQE_TAILQ_FOREACH_SAFE(b_cur,
                           &s_natproto.lh[i], 
                           le,                        
                           b_nxt) {
            
            VQE_TAILQ_REMOVE(&s_natproto.lh[i], b_cur, le);
            id_delete(b_cur->id, s_natproto.id_table);
            zone_release(s_natproto.pool, b_cur); 
            s_natproto.open_bindings--;
        }    
    }

    if (s_natproto.id_table != ID_MGR_TABLE_KEY_ILLEGAL) { 
        id_destroy_table(s_natproto.id_table);
    }
    if (s_natproto.pool) {
        /*sa_ignore {no recourse on failure} IGNORE_RETURN (1) */
        zone_instance_put(s_natproto.pool);
    }
    memset(&s_natproto, 0, sizeof(s_natproto));
}


/**---------------------------------------------------------------------------
 * Initialize the NAT protocol. 
 * 
 * @param[in] params Protocol configuration parameters.
 * @param[out] Returns true on success.
 *---------------------------------------------------------------------------*/  
#define NAT_IDTABLE_BUCKETS 1
static boolean 
vqec_nat_natproto_create (vqec_nat_init_params_t *params) 
{
    boolean ret = TRUE;
    int32_t i;

    if (s_natproto.init_done) {
        vqec_nat_log_error("Already initialized %s",
                           __FUNCTION__);
        goto done;
    }

    if (!params ||
        !params->max_bindings ||
        (params->max_bindings > VQEC_NAT_MAX_BINDINGS)) {
        ret = FALSE;
        vqec_nat_log_error("Max bindings input is 0 %s",
                           __FUNCTION__);
        goto done;
    }

    /* Allocate Id table. */
    s_natproto.id_table = id_create_new_table(NAT_IDTABLE_BUCKETS, 
                                              params->max_bindings);
   
    if (s_natproto.id_table == ID_MGR_TABLE_KEY_ILLEGAL) { 
        ret = FALSE;
        vqec_nat_log_error("Failed to allocate id table %s",
                           __FUNCTION__);
        goto done;
    }

    s_natproto.pool = zone_instance_get_loc("natproto_pool",
                                            O_CREAT,
                                            sizeof(vqec_nat_natproto_bind_t),
                                            params->max_bindings,
                                            NULL,
                                            NULL);
    if (!s_natproto.pool) {
        vqec_nat_log_error("Failed to allocate nat element pool %s",
                           __FUNCTION__);        
        goto done;
    }

    s_natproto.max_bindings = params->max_bindings;
    s_natproto.refresh_interval = 0;
    s_natproto.init_done = TRUE;    
    for (i = 0; i < NATPROTO_HASH_BUCKETS; i++) {
        VQE_TAILQ_INIT(&s_natproto.lh[i]);
    }

  done:
    if (!ret) {
        vqec_nat_natproto_destroy();
    }
    return (ret);
}


/**---------------------------------------------------------------------------
 * Find an existing binding based on 4-tuple lookup.
 *
 * @param[in] desc Attributes of the binding to be searched.
 * @param[out] vqec_nat_bindid_t A valid id if the binding exists, invalid
 * handle otherwise.
 *---------------------------------------------------------------------------*/  
static inline boolean
vqec_nat_natproto_is_same_binding (vqec_nat_bind_desc_t *desc1, 
                                   vqec_nat_bind_desc_t *desc2)
{
    return 
        ((desc1->internal_port == desc2->internal_port) &&
         (desc1->internal_addr == desc2->internal_addr) &&
         (desc1->remote_addr == desc2->remote_addr) &&
         (desc1->remote_port == desc2->remote_port));
}

static vqec_nat_bindid_t
vqec_nat_natproto_find_internal (vqec_nat_bind_desc_t *desc)
{
    vqec_nat_bindid_t id = ID_MGR_INVALID_HANDLE;
    uint16_t hash;
    vqec_nat_natproto_bind_t *p_bind;

    if (!desc) {
        goto done;
    }

    hash = vqec_nat_proto_hash((uint16_t)desc->internal_port);    
    VQE_TAILQ_FOREACH(p_bind, &s_natproto.lh[hash], le) {
        if (vqec_nat_natproto_is_same_binding(&p_bind->desc, desc)) {
            id = p_bind->id;
            break;
        }
    }

  done:
    return (id);
}


/**---------------------------------------------------------------------------
 * Open a new protocol binding. 
 * 
 * @param[in] desc Descriptor which specifies the binding.
 * @param[out]  vqec_nat_bindid_t An identifier which can then be used to
 * refer to the binding. If no identifier can be allocated, an Invalid id
 * is returned.
 *---------------------------------------------------------------------------*/  
static vqec_nat_bindid_t 
vqec_nat_natproto_open (vqec_nat_bind_desc_t *desc)
{
    boolean ret = TRUE;
    vqec_nat_natproto_bind_t *p_bind = NULL;
    char str1[INET_ADDRSTRLEN], str2[INET_ADDRSTRLEN] ;
    vqec_nat_bindid_t other_id;
    uint16_t hash;

    if (!s_natproto.init_done || !desc) {
        ret = FALSE;
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        goto done;
    }

    other_id = vqec_nat_natproto_find_internal(desc);
    if (!id_mgr_is_invalid_handle(other_id)) {
        ret = FALSE;
        vqec_nat_log_error("Binding for %s:%d:%s:%d is already open %s",                           
                           uint32_ntoa_r(desc->internal_addr,
                                         str1, sizeof(str1)),                           
                           ntohs(desc->internal_port),
                           uint32_ntoa_r(desc->remote_addr,
                                         str2, sizeof(str2)),
                           ntohs(desc->remote_port),
                           __FUNCTION__);
        goto done;        
    }

    p_bind = (vqec_nat_natproto_bind_t *)zone_acquire(s_natproto.pool);
    if (!p_bind) {
        ret = FALSE;
        vqec_nat_log_error("Memory allocation failed %s", 
                           __FUNCTION__);
        goto done;
    }
    memset(p_bind, 0, sizeof(*p_bind));

    p_bind->id = id_get(p_bind, s_natproto.id_table);
    if (id_mgr_is_invalid_handle(p_bind->id)) { 
        ret = FALSE;
        vqec_nat_log_error("Id allocation failed %s",
                           __FUNCTION__);
        goto done;
    }

    p_bind->desc = *desc;
    p_bind->ext_addr = p_bind->desc.internal_addr;
    p_bind->ext_port = p_bind->desc.internal_port;
    p_bind->state = VQEC_NAT_BIND_STATE_NOTBEHINDNAT;
    p_bind->is_map_valid = TRUE;
    hash = vqec_nat_proto_hash((uint16_t)p_bind->desc.internal_port); 
    VQE_TAILQ_INSERT_TAIL(&s_natproto.lh[hash], p_bind, le);
    s_natproto.open_bindings++;

    if (s_natproto.debug_en) {
        vqec_nat_log_debug("Proto: NAT opened binding %s:%d:%s:%d\n",
                           uint32_ntoa_r(p_bind->desc.internal_addr,
                                         str1, sizeof(str1)),
                           ntohs(p_bind->desc.internal_port),
                           uint32_ntoa_r(p_bind->desc.remote_addr,
                                         str2, sizeof(str2)),
                           ntohs(p_bind->desc.remote_port));
    }

  done:
    if (!ret) {        
        if (p_bind) {
            zone_release(s_natproto.pool, p_bind);
        }

        return (VQEC_NAT_BINDID_INVALID);
    }

    return (p_bind->id);
}


/**---------------------------------------------------------------------------
 * Close an existing protocol binding. 
 * 
 * @param[in] id Identifier of the binding to be closed.
 *---------------------------------------------------------------------------*/  
static void 
vqec_nat_natproto_close (vqec_nat_bindid_t id)
{
    vqec_nat_natproto_bind_t *p_bind;
    char str1[INET_ADDRSTRLEN], str2[INET_ADDRSTRLEN] ;
    id_mgr_ret ret;
    uint16_t hash;

    if (!s_natproto.init_done || id_mgr_is_invalid_handle(id)) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        return;
    }

    p_bind = (vqec_nat_natproto_bind_t *)id_to_ptr(id, &ret, 
                                                   s_natproto.id_table);
    if (!p_bind || (ret != ID_MGR_RET_OK)) {
        vqec_nat_log_error("Id lookup failure  %s",
                           __FUNCTION__); 
        return;
    }

    hash = vqec_nat_proto_hash((uint16_t)p_bind->desc.internal_port); 
    VQE_TAILQ_REMOVE(&s_natproto.lh[hash], p_bind, le);   

    if (s_natproto.debug_en) {
        vqec_nat_log_debug("Proto: NAT closed binding %s:%d:%s:%d\n",
                           uint32_ntoa_r(p_bind->desc.internal_addr,
                                         str1, sizeof(str1)),
                           ntohs(p_bind->desc.internal_port),
                           uint32_ntoa_r(p_bind->desc.remote_addr,
                                         str2, sizeof(str2)),
                           ntohs(p_bind->desc.remote_port));
    }

    id_delete(id, s_natproto.id_table);
    zone_release(s_natproto.pool, p_bind);
    s_natproto.open_bindings--;
}


/**---------------------------------------------------------------------------
 * Query the latest mapping for an existing binding. 
 * 
 * @param[in] id Identifier of the binding queried.
 * @param[out] data Pointer to the data associated with the mapping.
 * @param[in] refresh If the binding should be refreshed: no action.
 * @param[out] boolean Returns true ifthe id is for a valid binding.
 *---------------------------------------------------------------------------*/  
static boolean 
vqec_nat_natproto_query (vqec_nat_bindid_t id, vqec_nat_bind_data_t *data,
                         boolean refresh)
{
    vqec_nat_natproto_bind_t *p_bind;
    id_mgr_ret id_ret;
    boolean ret = TRUE;
    char str[INET_ADDRSTRLEN];
    
    if (!s_natproto.init_done || 
        id_mgr_is_invalid_handle(id) || !data) {
        ret = FALSE;
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);        
        goto done;
    }

    p_bind = (vqec_nat_natproto_bind_t *)id_to_ptr(id, &id_ret, 
                                                   s_natproto.id_table);
    if (!p_bind || (id_ret != ID_MGR_RET_OK)) {
        ret = FALSE;
        vqec_nat_log_error("Id lookup failure %s",
                           __FUNCTION__); 
        goto done;
    }

    data->id = p_bind->id;
    data->desc = p_bind->desc;
    data->ext_addr = p_bind->ext_addr;
    data->ext_port = p_bind->ext_port;
    data->state = p_bind->state;
    data->is_map_valid = p_bind->is_map_valid;

    if (s_natproto.debug_en && !data->is_map_valid) {
        vqec_nat_log_debug("Proto: Query binding id: %d %s:%d\n",
                           data->id,
                           uint32_ntoa_r(data->ext_addr,
                                         str, sizeof(str)),
                           ntohs(data->ext_port));
    }

  done:
    if (!ret && data) {
        memset(data, 0, sizeof(*data));
    }
    return (ret);
}


/**---------------------------------------------------------------------------
 * A character string representing the current discovered nat state.
 * 
 * @param[in] p_bind Pointer to the NAT binding.
 * @param[out] char* Pointer to a constant string representing the state
 * of the binding.
 *---------------------------------------------------------------------------*/  
static const char *
vqec_nat_natproto_state (vqec_nat_natproto_bind_t *p_bind)
{
    if (!p_bind) {
        return "Error!";
    }
    
    return "No active NAT protocol";
}


/**---------------------------------------------------------------------------
 * Print information about one binding. 
 * 
 * @param[in] id Identifier of the binding.
 *---------------------------------------------------------------------------*/  
static void
vqec_nat_natproto_fprint (vqec_nat_bindid_t id)
{
    vqec_nat_natproto_bind_t *p_bind;
    id_mgr_ret id_ret;
    char str[INET_ADDRSTRLEN];

    if (!s_natproto.init_done || 
        id_mgr_is_invalid_handle(id)) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);        
        return;
    }
    
    p_bind = (vqec_nat_natproto_bind_t *)id_to_ptr(id, &id_ret, 
                                                   s_natproto.id_table);
    if (!p_bind || (id_ret != ID_MGR_RET_OK)) {
        vqec_nat_log_error("Id lookup failure %s",
                           __FUNCTION__); 
        return;
    }

    vqec_nat_print(" Binding name:              %s\n",
                   p_bind->desc.name);
    vqec_nat_print(" NAT status:                %s\n",
                   vqec_nat_natproto_state(p_bind));
    vqec_nat_print(" Internal address:          %s:%u\n",
                   uint32_ntoa_r(p_bind->desc.internal_addr,
                                 str, sizeof(str)),
                   ntohs(p_bind->desc.internal_port));
    vqec_nat_print(" Public address:            %s:%u\n",
                   uint32_ntoa_r(p_bind->ext_addr,
                                 str, sizeof(str)),
                   ntohs(p_bind->ext_port));    
} 


/**---------------------------------------------------------------------------
 * Print information about all bindings. 
 *---------------------------------------------------------------------------*/  
static void
vqec_nat_natproto_fprint_all (void)
{
    int32_t i;
    vqec_nat_natproto_bind_t *p_bind;

    if (!s_natproto.init_done) {
        vqec_nat_log_error("Bad input arguments %s", 
                           __FUNCTION__);        
        return;
    }
    
    
    vqec_nat_print("NAT protocol:               None\n");
    vqec_nat_print("NAT bindings open:          %d\n",
                   s_natproto.open_bindings);

    for (i = 0; i < NATPROTO_HASH_BUCKETS; i++) {
        VQE_TAILQ_FOREACH(p_bind,
                      &s_natproto.lh[i], 
                      le) {

            vqec_nat_print("NAT id:                     %lu\n",
                           p_bind->id);
            vqec_nat_natproto_fprint(p_bind->id);
        }    
    }
}


/**---------------------------------------------------------------------------
 * This method delivers a NAT packet ejected from the RTP or RTCP
 * streams to the NAT protocol for processing.
 *
 * @param[in] id Identifier of the binding on which the packet is received.
 * @param[in] buf Pointer to the packet's content buffer.
 * @param[in] len Length of the buffer.
 *---------------------------------------------------------------------------*/  
#define NAT_NATPROTO_MAX_BUFSIZE 1514
static void 
vqec_nat_natproto_eject (vqec_nat_bindid_t id, char *buf, uint16_t len,
                         in_addr_t source_ip, in_port_t source_port)
{

    if (!s_natproto.init_done || 
        id_mgr_is_invalid_handle(id) || 
        !buf || 
        !len || 
        len > NAT_NATPROTO_MAX_BUFSIZE) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);        
        return;
    }
    
    vqec_nat_log_error("Nat packet received with null NAT protocol %s",
                       __FUNCTION__);  
}


/**---------------------------------------------------------------------------
 * Enable debug for NAT. 
 * 
 * @param[in] verbose Controls verbose mode for debugging.
 *---------------------------------------------------------------------------*/  
static void 
vqec_nat_natproto_debug_set (boolean verbose)
{
    s_natproto.debug_en = TRUE;
    s_natproto.verbose = verbose;
}


/**---------------------------------------------------------------------------
 * Disable debug for NAT. 
 *---------------------------------------------------------------------------*/  
static void 
vqec_nat_natproto_debug_clr (void)
{
    s_natproto.debug_en = FALSE;    
}


/**---------------------------------------------------------------------------
 * Is the device behind a NAT.
 *
 * @param[out] boolean Returns true if the device is behind a NAT. 
 *---------------------------------------------------------------------------*/  
static boolean 
vqec_nat_natproto_is_behind_nat (void) 
{
    return (FALSE);
}


/**---------------------------------------------------------------------------
 * Interface definition.
 *---------------------------------------------------------------------------*/  
static vqec_nat_proto_if_t s_natproto_if = 
{

    .create = vqec_nat_natproto_create,
    .destroy = vqec_nat_natproto_destroy,
    .open = vqec_nat_natproto_open,
    .close = vqec_nat_natproto_close,
    .query = vqec_nat_natproto_query,
    .fprint = vqec_nat_natproto_fprint,
    .fprint_all = vqec_nat_natproto_fprint_all,
    .eject_rx = vqec_nat_natproto_eject,
    .debug_set = vqec_nat_natproto_debug_set,
    .debug_clr = vqec_nat_natproto_debug_clr,
    .is_behind_nat = vqec_nat_natproto_is_behind_nat

};

const vqec_nat_proto_if_t *
vqec_nat_get_if_natproto (void)
{
    return (&s_natproto_if);
}
