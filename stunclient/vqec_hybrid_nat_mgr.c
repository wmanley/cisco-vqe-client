/*
 * Copyright (c) 2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "vqec_nat_api.h"
#include "vqec_hybrid_nat_mgr.h"
#include <utils/vam_time.h>
#include <utils/vam_util.h>
#include <utils/zone_mgr.h>
#include <utils/queue_plus.h>
#include <utils/id_manager.h>
#include <string.h>

#define COMBINED_HASH_MAP_SIZE 8
#define COMBINED_HASH_MASK 7

/* Port binding object */
typedef struct vqec_nat_hybrid_bind_ {
    /* Unique IDs */
    vqec_nat_bindid_t id, stun_id, upnp_id;
    /* Descriptor provided by channel bind */
    vqec_nat_bind_desc_t desc;
    /* Responsible Protocol */
    vqec_nat_proto_t proto;
    /* External IP addrs */
    in_addr_t stun_ext_addr, upnp_ext_addr;

    VQE_TAILQ_ENTRY(vqec_nat_hybrid_bind_) port_le;
} vqec_nat_hybrid_bind_t;

typedef struct vqe_zone vqec_nat_hybrid_pool_t;
typedef VQE_TAILQ_HEAD(nat_list_, vqec_nat_hybrid_bind_) nat_list_t;

/* Main static data structure */
typedef struct vqec_nat_hybrid_ {
    /* Init done */
    boolean init_done;
    /* Maximum number of bindings */
    uint32_t max_bindings;
    /* Memory pool for creating bindings */
    vqec_nat_hybrid_pool_t *bind_pool;
    /* Id table for mapping bindids to bindings */
    id_table_key_t bindid_table;
    /* Hash table of all bindings by port */
    nat_list_t port_lh[COMBINED_HASH_MAP_SIZE];
    /* Open bindings */
    uint32_t stun_bindings, upnp_bindings;

    /* Available nat protocols */
    const vqec_nat_proto_if_t *stun;
    const vqec_nat_proto_if_t *upnp;

} vqec_nat_hybrid_t;
static vqec_nat_hybrid_t s_hybrid;

static void vqec_nat_hybrid_close(vqec_nat_bindid_t id);

/**
 * vqec_nat_hybrid_port_hash
 *   hash function based on internal port number
 */
static inline uint16_t
vqec_nat_hybrid_port_hash (in_port_t port)
{
    return (((uint16_t)port) & COMBINED_HASH_MASK);
}

/**
 * vqec_nat_hybrid_bindid_hash
 *   hash function based on bind id
 */
static inline uint16_t
vqec_nat_hybrid_bindid_hash (vqec_nat_bindid_t id)
{
    return (uint16_t)(id & COMBINED_HASH_MASK);
}

/**
 * vqec_nat_hybrid_is_same_binding
 *   compares bind_descs for equality
 */
static inline boolean
vqec_nat_hybrid_is_same_binding (vqec_nat_bind_desc_t *desc1,
                                 vqec_nat_bind_desc_t *desc2)
{
    return
        ((desc1->internal_port == desc2->internal_port) &&
         (desc1->internal_addr == desc2->internal_addr) &&
         (desc1->remote_addr == desc2->remote_addr) &&
         (desc1->remote_port == desc2->remote_port));
}

/**
 * vqec_nat_hybrid_find_by_desc
 *   finds an existing binding matching the desc
 *
 * @param[in] desc bind descriptor to match
 * @param[out] vqec_nat_hybrid_bind_t* matching binding, NULL if none
 */
static vqec_nat_hybrid_bind_t*
vqec_nat_hybrid_find_by_desc (vqec_nat_bind_desc_t * desc)
{
    uint16_t hash;
    vqec_nat_hybrid_bind_t *p_bind;

    if (!desc) {
        return NULL;
    }

    hash = vqec_nat_hybrid_port_hash(desc->internal_port);
    VQE_TAILQ_FOREACH(p_bind, &s_hybrid.port_lh[hash], port_le) {
        if (vqec_nat_hybrid_is_same_binding(&p_bind->desc, desc)) {
            return p_bind;
        }
    }

    return NULL;
}

/**
 * vqec_nat_hybrid_find_by_upnp_id
 *   finds an existing hybrid binding matching the upnp bindid
 *
 * @param[in] id upnp bindid to match
 * @param[out] vqec_nat_hybrid_bind_t* hybrid bind obj matching upnp_id
 */
static vqec_nat_hybrid_bind_t*
vqec_nat_hybrid_find_by_upnp_id (vqec_nat_bindid_t upnp_id)
{
    vqec_nat_bind_data_t data;

    if (upnp_id == VQEC_NAT_BINDID_INVALID) {
        return NULL;
    }

    if ((*s_hybrid.upnp->query)(upnp_id, &data, FALSE)) {
        return vqec_nat_hybrid_find_by_desc(&data.desc);
    }

    return NULL;
}

/**
 * vqec_nat_hybrid_find_by_stun_id
 *   finds an existing hybrid binding matching the stun bindid
 *
 * @param[in] id stun bindid to match
 * @param[out] vqec_nat_hybrid_bind_t* hybrid bind obj matching stun_id
 */
static vqec_nat_hybrid_bind_t*
vqec_nat_hybrid_find_by_stun_id (vqec_nat_bindid_t stun_id)
{
    vqec_nat_bind_data_t data;

    if (stun_id == VQEC_NAT_BINDID_INVALID) {
        return NULL;
    }

    if ((*s_hybrid.stun->query)(stun_id, &data, FALSE)) {
        return vqec_nat_hybrid_find_by_desc(&data.desc);
    }

    return NULL;
}

/**
 * vqec_nat_hybrid_find_by_id
 *   finds a port binding object based on id number
 */
static inline vqec_nat_hybrid_bind_t *
vqec_nat_hybrid_find_by_id (vqec_nat_bindid_t id) 
{
    id_mgr_ret ret;
    vqec_nat_hybrid_bind_t *p_bind;

    p_bind = (vqec_nat_hybrid_bind_t *)id_to_ptr(id, &ret,
                                                 s_hybrid.bindid_table);
    if (ret != ID_MGR_RET_OK) {
        p_bind = NULL;
    }
    return p_bind;
}

/**
 * Destroy NAT protocol state.
 */
void vqec_nat_hybrid_destroy (void) 
{
    vqec_nat_hybrid_bind_t *p_bind, *p_bind_nxt;
    int32_t i;

    /* Close remaining bindings */
    for (i = 0; i < COMBINED_HASH_MAP_SIZE; i++) {
        VQE_TAILQ_FOREACH_SAFE(p_bind, &s_hybrid.port_lh[i], 
                               port_le, p_bind_nxt) {
            vqec_nat_hybrid_close(p_bind->id);
        }
    }

    /* Call destroy on sub-protocols */
    if (s_hybrid.stun) {
        (*s_hybrid.stun->destroy)();
        s_hybrid.stun = NULL;
    }
    if (s_hybrid.upnp) {
        (*s_hybrid.upnp->destroy)();
        s_hybrid.upnp = NULL;
    }

    /* Free memory resources and clear variables */
    if (s_hybrid.bindid_table != ID_MGR_TABLE_KEY_ILLEGAL) {
        id_destroy_table(s_hybrid.bindid_table);
        s_hybrid.bindid_table = ID_MGR_TABLE_KEY_ILLEGAL;
    }
    if (s_hybrid.bind_pool) {
        zone_instance_put(s_hybrid.bind_pool);
        s_hybrid.bind_pool = NULL;
    }
    memset(&s_hybrid, 0, sizeof(vqec_nat_hybrid_t));
}

/**
 * Instantiate the NAT protocol state.
 */
boolean vqec_nat_hybrid_create (vqec_nat_init_params_t *params) 
{
    boolean ret = TRUE;
    int32_t i;

    if (s_hybrid.init_done) {
        vqec_nat_log_error("Already initialized %s", __FUNCTION__);
        return TRUE;
    }
    if (!params) {
        vqec_nat_log_error("Invalid input parameters %s", __FUNCTION__);
        return FALSE;
    }

    /* Initialize s_hybrid structure */
    memset(&s_hybrid, 0, sizeof(vqec_nat_hybrid_t));
    s_hybrid.max_bindings = params->max_bindings;

    /* Create bindings pool */
    s_hybrid.bind_pool = zone_instance_get_loc(
                                           "hybrid bind",
                                           O_CREAT,
                                           sizeof(vqec_nat_hybrid_bind_t),
                                           params->max_bindings,
                                           NULL, NULL);
    if (!s_hybrid.bind_pool) {
        vqec_nat_log_error("Failed to allocate bind pool %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }

    /* Initialize id table for port bindings*/
    s_hybrid.bindid_table = id_create_new_table(1, params->max_bindings);
    if (s_hybrid.bindid_table == ID_MGR_TABLE_KEY_ILLEGAL) {
        vqec_nat_log_error("Failed to allocate id table %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }

    /* Initialize bindings hash map */
    for (i = 0; i < COMBINED_HASH_MAP_SIZE; i++) {
        VQE_TAILQ_INIT(&s_hybrid.port_lh[i]);
    }

    /* Fetch sub-protocol function tables */
    s_hybrid.stun = NAT_GET_PROTO_INTF(stunproto);
    s_hybrid.upnp = NAT_GET_PROTO_INTF(upnpproto);

    /* Call create on sub-protocols */
    if (!((*s_hybrid.stun->create)(params))) {
        vqec_nat_log_error("Failed to initialize STUN %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    if (!((*s_hybrid.upnp->create)(params))) {
        vqec_nat_log_error("Failed to initialize UPnP %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    
    /* Initialization complete */
    s_hybrid.init_done = TRUE;

  done:
    if (!ret) {
        vqec_nat_hybrid_destroy();
    } 
    return ret;
}

/**
 * Open a new NAT binding.
 */
vqec_nat_bindid_t vqec_nat_hybrid_open (vqec_nat_bind_desc_t *desc)
{
    vqec_nat_hybrid_bind_t *new_bind = NULL;
    uint16_t hash;
    boolean err = FALSE;
    char str1[INET_ADDRSTRLEN], str2[INET_ADDRSTRLEN];

    if (!s_hybrid.init_done || !desc) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        return VQEC_NAT_BINDID_INVALID;
    }
    if (s_hybrid.stun_bindings + s_hybrid.upnp_bindings 
            >= s_hybrid.max_bindings) {
        vqec_nat_log_error("Too many bindings %s",
                           __FUNCTION__);
        return VQEC_NAT_BINDID_INVALID;
    }

    /* Check that new mapping doesn't exist already */
    if (vqec_nat_hybrid_find_by_desc(desc)) {
        vqec_nat_log_error("Binding %s:%u:%s:%u already exists %s",
                           uint32_ntoa_r(desc->internal_addr,
                                         str1, sizeof(str1)),
                           ntohs(desc->internal_port),
                           uint32_ntoa_r(desc->remote_addr,
                                         str2, sizeof(str2)),
                           ntohs(desc->remote_port),
                           __FUNCTION__);
        err = TRUE;
        goto done;
    }

    /* Allocate and initialize new bind object */
    new_bind = (vqec_nat_hybrid_bind_t *)zone_acquire(s_hybrid.bind_pool);
    if (!new_bind) {
        vqec_nat_log_error("Failed to allocate new binding %s",
                           __FUNCTION__);
        err = TRUE;
        goto done;
    }
    memset(new_bind, 0, sizeof(vqec_nat_hybrid_bind_t));
    new_bind->desc = *desc;

    /* Create a new bind id for the new bind object */
    new_bind->id = id_get(new_bind, s_hybrid.bindid_table);
    if (id_mgr_is_invalid_handle(new_bind->id)) {
        vqec_nat_log_error("Bind id allocation failed %s",
                           __FUNCTION__);
        err = TRUE;
        goto done;
    }

    /* Open sub-protocol bindings */
    new_bind->stun_id = (*s_hybrid.stun->open)(desc);
    new_bind->upnp_id = (*s_hybrid.upnp->open)(desc);
   
    /* Abort if either open call fails */ 
    if (new_bind->stun_id == VQEC_NAT_BINDID_INVALID 
        || new_bind->upnp_id == VQEC_NAT_BINDID_INVALID) {
        vqec_nat_log_error("Sub-protocol open failed %s",
                           __FUNCTION__);
        err = TRUE;
        goto done;
    }

    /* Default protocol is STUN */
    new_bind->proto = VQEC_NAT_PROTO_STUN;
        
    /* Add new binding to hash map */
    hash = vqec_nat_hybrid_port_hash(desc->internal_port);
    VQE_TAILQ_INSERT_TAIL(&s_hybrid.port_lh[hash], new_bind, port_le);
    
    /* Increment appropriate bindings counter */
    switch (new_bind->proto) {
        case VQEC_NAT_PROTO_STUN:
            s_hybrid.stun_bindings++;
            break;
        case VQEC_NAT_PROTO_UPNP:
            s_hybrid.upnp_bindings++;
            break;
        default:
            assert(FALSE);
            break;
    }

  done:
    if (err) {
        /* Clean up should an error have occurred */
        if (new_bind) {
            if (new_bind->stun_id != VQEC_NAT_BINDID_INVALID) {
                (*s_hybrid.stun->close)(new_bind->stun_id);
            }
            if (new_bind->upnp_id != VQEC_NAT_BINDID_INVALID) {
                (*s_hybrid.upnp->close)(new_bind->upnp_id);
            }
            if (!id_mgr_is_invalid_handle(new_bind->id)) {
                id_delete(new_bind->id, s_hybrid.bindid_table);
            }
            zone_release(s_hybrid.bind_pool, new_bind);
        }
        return VQEC_NAT_BINDID_INVALID;
    }
    return new_bind->id;
}

/**
 * Close an existing NAT binding.
 */
void vqec_nat_hybrid_close (vqec_nat_bindid_t id)
{
    vqec_nat_hybrid_bind_t *p_bind;
    uint16_t hash;

    if (!s_hybrid.init_done) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        return;
    }
   
    /* Look up bind object based on bindid */ 
    p_bind = vqec_nat_hybrid_find_by_id(id);
    if (!p_bind) {
        vqec_nat_log_error("Id lookup failure %lu %s",
                           id,
                           __FUNCTION__);
        return;
    }

    /* Remove binding from hash map and call sub-protocol close */
    hash = vqec_nat_hybrid_port_hash(p_bind->desc.internal_port);
    VQE_TAILQ_REMOVE(&s_hybrid.port_lh[hash], p_bind, port_le);
    if (p_bind->stun_id != VQEC_NAT_BINDID_INVALID) {
        (*s_hybrid.stun->close)(p_bind->stun_id);
    }
    if (p_bind->upnp_id != VQEC_NAT_BINDID_INVALID) {
        (*s_hybrid.upnp->close)(p_bind->upnp_id);
    }

    /* Decrement proper binding counter */
    switch (p_bind->proto) {
        case VQEC_NAT_PROTO_STUN:
            s_hybrid.stun_bindings--;
            break;
        case VQEC_NAT_PROTO_UPNP:
            s_hybrid.upnp_bindings--;
            break;
        default:
            assert(FALSE);
            break;
    }

    /* Free associated memory resources */
    id_delete(p_bind->id, s_hybrid.bindid_table);
    zone_release(s_hybrid.bind_pool, p_bind);
}

/**
 * Query a NAT binding to find it's most recent mapping.
 */
boolean vqec_nat_hybrid_query (vqec_nat_bindid_t id,
                                 vqec_nat_bind_data_t *data, 
                                 boolean refresh)
{
    vqec_nat_hybrid_bind_t *p_bind;

    if (!s_hybrid.init_done) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        goto done;
    }
    
    /* Find bind object based on bindid */
    p_bind = vqec_nat_hybrid_find_by_id(id);
    if (!p_bind) {
        vqec_nat_log_error("Id lookup failure %s",
                           __FUNCTION__);
        goto done;
    }

    /* Call query on appropriate sub-protocol */
    switch (p_bind->proto) {
        case VQEC_NAT_PROTO_STUN:
            return (*s_hybrid.stun->query)(p_bind->stun_id, data, refresh);
        case VQEC_NAT_PROTO_UPNP:
            return (*s_hybrid.upnp->query)(p_bind->upnp_id, data, refresh);
        default:
            assert(FALSE);
            break;
    }

  done:
    return FALSE;
}

/**
 * Print state of a NAT binding.
 */
void vqec_nat_hybrid_fprint (vqec_nat_bindid_t id)
{
    vqec_nat_hybrid_bind_t *p_bind;
    char str[INET_ADDRSTRLEN];

    if (!s_hybrid.init_done) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        return;
    }
    
    /* Look up bind object based on bindid */
    p_bind = vqec_nat_hybrid_find_by_id(id);
    if (!p_bind) {
        vqec_nat_log_error("Id lookup failure %s",
                           __FUNCTION__);
        return;
    }
    
    /* Print formatted info */
    if (p_bind->stun_id != VQEC_NAT_BINDID_INVALID) {
        vqec_nat_print(" STUN status:               %s\n",
                       p_bind->stun_ext_addr ? 
                            uint32_ntoa_r(p_bind->stun_ext_addr, 
                                          str, sizeof(str))
                            : "unknown");
    } else {
        vqec_nat_print(" STUN status:               closed\n");
    }
    if (p_bind->upnp_id != VQEC_NAT_BINDID_INVALID) {
        vqec_nat_print(" UPnP status:               %s\n",
                       p_bind->upnp_ext_addr ? 
                            uint32_ntoa_r(p_bind->upnp_ext_addr, 
                                          str, sizeof(str))
                            : "unknown");
    } else {
        vqec_nat_print(" UPnP status:               closed\n");
    }

    /* Call fprint on appropriate sub-protocol */
    switch (p_bind->proto) {
        case VQEC_NAT_PROTO_STUN:
            (*s_hybrid.stun->fprint)(p_bind->stun_id);
            break;
        case VQEC_NAT_PROTO_UPNP:
            (*s_hybrid.upnp->fprint)(p_bind->upnp_id);
            break;
        default:
            assert(FALSE);
            break;
    }
}

/**
 * Print all protocol / binding status.
 */
void vqec_nat_hybrid_fprint_all (void)
{
    vqec_nat_hybrid_bind_t *p_bind;
    int i;

    vqec_nat_print("NAT protocol:               STUN/UPnP Hybrid\n");
    vqec_nat_print("NAT bindings open:          %d\n",
                   (s_hybrid.stun_bindings + s_hybrid.upnp_bindings));

    /* Print status of each binding */
    for (i = 0; i < COMBINED_HASH_MAP_SIZE; i++) {
        VQE_TAILQ_FOREACH(p_bind, &s_hybrid.port_lh[i], port_le) {
            vqec_nat_print("NAT id:                     %lu\n",
                           p_bind->id);
            vqec_nat_hybrid_fprint(p_bind->id);
        }
    }

}

/**
 * Insert an ejected packet for the given binding.
 */
void vqec_nat_hybrid_eject (vqec_nat_bindid_t id, char *buf, uint16_t len,
                            in_addr_t source_ip, in_port_t source_port)
{
    vqec_nat_hybrid_bind_t *p_bind;

    if (!s_hybrid.init_done) {
        vqec_nat_log_error("Hybrid NAT not initialized %s",
                           __FUNCTION__);
        return;
    }
    
    /* Find bind object based on bindid */
    p_bind = vqec_nat_hybrid_find_by_id(id);
    if (!p_bind) {
        vqec_nat_log_error("Id lookup failure %s",
                           __FUNCTION__);
        return;
    }
        
    /* Ejected packets only apply to STUN protocol */
    if (p_bind->stun_id != VQEC_NAT_BINDID_INVALID) {
        (*s_hybrid.stun->eject_rx)(p_bind->stun_id, buf, len,
                                   source_ip, source_port);
    }
}

/**
 * Switch debugging on.
 */
void vqec_nat_hybrid_debug_set (boolean verbose)
{
    (*s_hybrid.stun->debug_set)(verbose);
    (*s_hybrid.upnp->debug_set)(verbose);
}

/**
 * Switch debugging off.
 */
void vqec_nat_hybrid_debug_clr (void)
{
    (*s_hybrid.stun->debug_clr)();
    (*s_hybrid.upnp->debug_clr)();
}

/**
 * Is the device behind a NAT?
 */
boolean vqec_nat_hybrid_is_behind_nat (void)
{
    /* Only STUN can determine if behind nat or not */
    return ((*s_hybrid.stun->is_behind_nat)());
}

/**---------------------------------------------------------------------------
 * Interface definition.
 *---------------------------------------------------------------------------*/  
static vqec_nat_proto_if_t s_hybrid_if = 
{

    .create = vqec_nat_hybrid_create,
    .destroy = vqec_nat_hybrid_destroy,
    .open = vqec_nat_hybrid_open,
    .close = vqec_nat_hybrid_close,
    .query = vqec_nat_hybrid_query,
    .fprint = vqec_nat_hybrid_fprint,
    .fprint_all = vqec_nat_hybrid_fprint_all,
    .eject_rx = vqec_nat_hybrid_eject,
    .debug_set = vqec_nat_hybrid_debug_set,
    .debug_clr = vqec_nat_hybrid_debug_clr,
    .is_behind_nat = vqec_nat_hybrid_is_behind_nat
   
};

const vqec_nat_proto_if_t *
vqec_nat_get_if_hybridproto (void)
{
    return (&s_hybrid_if);
}

/**---------------------------------------------------------------------------
 *  vqec_hybrid_mgr interface - for setting external ip
 *--------------------------------------------------------------------------*/

void vqec_nat_hybrid_bind_update (vqec_nat_proto_t proto, 
                                  vqec_nat_bindid_t id,
                                  vqec_nat_bind_data_t *data)
{
    vqec_nat_proto_t new_proto;
    vqec_nat_hybrid_bind_t *p_bind = NULL;
    in_addr_t ext_addr = data->ext_addr;

    /* Find relevant bind object and set ext addr based on protocol */
    switch (proto) { 
        case VQEC_NAT_PROTO_STUN:
            p_bind = vqec_nat_hybrid_find_by_stun_id(id);
            if (!p_bind) {
                return;
            }
            p_bind->stun_ext_addr = ext_addr;
            break;
        case VQEC_NAT_PROTO_UPNP:
            p_bind = vqec_nat_hybrid_find_by_upnp_id(id);
            if (!p_bind) {
                return;
            }
            p_bind->upnp_ext_addr = ext_addr;
            break;
        default:
            assert(FALSE);
            break;
    }

    /* 
     * Choose appropriate protocol based on external addrs.
     *  - stun and upnp addr both zero -> stun
     *  - stun addr zero, upnp non zero -> upnp
     *  - upnp addr zero, stun non zero -> stun
     *  - stun and upnp addr non zero, addrs equal -> upnp
     *  - stun and upnp addr non zero, addrs not equal -> stun
     */
    if (!p_bind->stun_ext_addr && !p_bind->upnp_ext_addr) {
        new_proto = VQEC_NAT_PROTO_STUN;
    } else if (!p_bind->stun_ext_addr) {
        new_proto = VQEC_NAT_PROTO_UPNP;
    } else if (!p_bind->upnp_ext_addr) {
        new_proto = VQEC_NAT_PROTO_STUN;
    } else if (p_bind->stun_ext_addr == p_bind->upnp_ext_addr) {
        new_proto = VQEC_NAT_PROTO_UPNP;
    } else {
        new_proto = VQEC_NAT_PROTO_STUN;
    }

    /* 
     * If decision is certain (both ext addrs are known),
     * close the sub-protocol no longer needed.
     */
    if (p_bind->stun_ext_addr && p_bind->upnp_ext_addr) {
        switch (new_proto) {
            case VQEC_NAT_PROTO_UPNP:
                if (p_bind->stun_id != VQEC_NAT_BINDID_INVALID) {
                    (*s_hybrid.stun->close)(p_bind->stun_id);
                    p_bind->stun_id = VQEC_NAT_BINDID_INVALID;
                }
                break;
            case VQEC_NAT_PROTO_STUN:
                if (p_bind->upnp_id != VQEC_NAT_BINDID_INVALID) {
                    (*s_hybrid.upnp->close)(p_bind->upnp_id);
                    p_bind->upnp_id = VQEC_NAT_BINDID_INVALID;
                }
                break;
            default:
                assert(FALSE); 
                break;
        }
    }

    /* Correct binding counters */
    if (p_bind->proto != new_proto) {
        switch (new_proto) {
            case VQEC_NAT_PROTO_STUN:
                s_hybrid.upnp_bindings--;
                s_hybrid.stun_bindings++;
                break;
            case VQEC_NAT_PROTO_UPNP:
                s_hybrid.stun_bindings--;
                s_hybrid.upnp_bindings++;
                break;
            default:
                assert(FALSE);
                break;
        }
    }

    /* Set the new protocol for the binding */
    p_bind->proto = new_proto;
     
    /* Pass on update if current protocol matches calling proto */
    if (proto == p_bind->proto) {
        vqec_nat_bind_update(p_bind->id, data);
    }
}


boolean vqec_nat_hybrid_inject_tx (vqec_nat_proto_t proto,
                                   vqec_nat_bindid_t id,
                                   vqec_nat_bind_desc_t *desc,
                                   char *buf,
                                   uint16_t len)
{
    vqec_nat_hybrid_bind_t *p_bind = NULL;

    switch (proto) {
        case VQEC_NAT_PROTO_STUN:
            p_bind = vqec_nat_hybrid_find_by_stun_id(id);
            break;
        case VQEC_NAT_PROTO_UPNP:
            p_bind = vqec_nat_hybrid_find_by_upnp_id(id);
            break;
        default:
            assert(FALSE);
            break;
    }
    if (!p_bind) {
        return FALSE;
    }    

    return vqec_nat_inject_tx(p_bind->id, desc, buf, len);
}
