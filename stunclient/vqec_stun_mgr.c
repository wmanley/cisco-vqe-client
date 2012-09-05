/*
 * Copyright (c) 2006-2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "vqec_nat_api.h"
#include <utils/vam_time.h>
#include <utils/vam_util.h>
#include <utils/queue_plus.h>
#include <utils/id_manager.h>
#include <utils/zone_mgr.h>
#include <string.h>
#include <stun/stun_includes.h>
#include <stun/stun.h>
#include "vqec_hybrid_nat_mgr.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif

/**
 * Maximum retries specified in RFC3489bis.
 */
#define STUNPROTO_TRIALS_MAX 9
/**
 * Timeouts specified in RFC3489bis.
 */
#define STUNPROTO_TRIALS_TIMEOUT_MS \
    {0, 100, 200, 400, 800, 1600, 1600, 1600, 1600, 1600}
static const uint16_t stunproto_trial_timeouts[] = 
    STUNPROTO_TRIALS_TIMEOUT_MS;


typedef struct vqe_zone vqec_nat_stunproto_pool_t;

typedef
struct vqec_nat_stunproto_bind_
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
     * Has a refresh been initiated on this binding?
     */
    boolean refresh_act;
    /**
     * Is the entry is ack-wait-response mode?
     */
    boolean wait_ack;
    /**
     * Last stun request timestamp.
     */
    abs_time_t last_request_time;
    /**
     * Last stun response timestamp.
     */
    abs_time_t last_response_time;
    /**
     * TID of a transaction that has been started.
     */
    transaction_id_t tid;
    /**
     * The number of trials that have occured for one transaction.
     */
    uint32_t trials;
    /**
     * Event to do retries on the message.
     */
    struct vqec_event_ *ev_retry;
    /**
     * Thread onto a list.
     */
    VQE_TAILQ_ENTRY(vqec_nat_stunproto_bind_) le;
                                         
} vqec_nat_stunproto_bind_t;

#define STUNPROTO_HASH_BUCKETS 8
#define STUNPROTO_HASH_MASK (STUNPROTO_HASH_BUCKETS - 1)

typedef VQE_TAILQ_HEAD(nat_list_, vqec_nat_stunproto_bind_) nat_list_t;

typedef
struct vqec_nat_stunproto_t
{
    /**
     * If properly initialized or not.
     */
    boolean init_done;
    /**
     * Is this device not behind a NAT?
     */
    boolean is_not_behind_nat;
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
    vqec_nat_stunproto_pool_t *pool;
    /**
     * Global refresh event.
     */
    struct vqec_event_ *ev_refresh;
    /**
     * A hash-table for bindings.
     */
    nat_list_t lh[STUNPROTO_HASH_BUCKETS];

} vqec_nat_stunproto_t;

static vqec_nat_stunproto_t s_stunproto;


/**---------------------------------------------------------------------------
 * Simple hash function on ports.
 *---------------------------------------------------------------------------*/  
static inline uint16_t
vqec_nat_proto_hash (uint16_t port) 
{
    return (port & STUNPROTO_HASH_MASK);
}


/**---------------------------------------------------------------------------
 * Destroy the protocol, and release all allocated objects.
 *---------------------------------------------------------------------------*/  
static void 
vqec_nat_stunproto_destroy (void)
{
    int32_t i;
    vqec_nat_stunproto_bind_t *b_cur, *b_nxt;

    /* delete refresh timer. */
    if (s_stunproto.ev_refresh) {
        vqec_nat_timer_destroy(s_stunproto.ev_refresh);        
    }

    for (i = 0; i < STUNPROTO_HASH_BUCKETS; i++) {
        VQE_TAILQ_FOREACH_SAFE(b_cur,
                           &s_stunproto.lh[i], 
                           le,                        
                           b_nxt) {
            
            /* delete retry timer. */
            if (b_cur->ev_retry) {
                vqec_nat_timer_destroy(b_cur->ev_retry);
            }

            VQE_TAILQ_REMOVE(&s_stunproto.lh[i], b_cur, le);
            id_delete(b_cur->id, s_stunproto.id_table);
            zone_release(s_stunproto.pool, b_cur); 
            s_stunproto.open_bindings--;
        } 
    }
    
    if (s_stunproto.id_table != ID_MGR_TABLE_KEY_ILLEGAL) { 
        id_destroy_table(s_stunproto.id_table);
    }
    if (s_stunproto.pool) {
        /*sa_ignore {no recourse on failure} IGNORE_RETURN (1) */
        zone_instance_put(s_stunproto.pool);
    }

    memset(&s_stunproto, 0, sizeof(s_stunproto));
}


/**---------------------------------------------------------------------------
 * Find an existing binding based on 4-tuple lookup.
 *
 * @param[in] Attributes of the binding to be searched.
 * @param[out] vqec_natbindid_t A valid id if the binding exists, invalid
 * handle otherwise.
 *---------------------------------------------------------------------------*/  
static inline boolean
vqec_nat_stunproto_is_same_binding (vqec_nat_bind_desc_t *desc1, 
                                    vqec_nat_bind_desc_t *desc2)
{
    return 
        ((desc1->internal_port == desc2->internal_port) &&
         (desc1->internal_addr == desc2->internal_addr) &&
         (desc1->remote_addr == desc2->remote_addr) &&
         (desc1->remote_port == desc2->remote_port));
}

static vqec_nat_bindid_t
vqec_nat_stunproto_find_internal (vqec_nat_bind_desc_t *desc)
{
    vqec_nat_bindid_t id = ID_MGR_INVALID_HANDLE;
    uint16_t hash;
    vqec_nat_stunproto_bind_t *p_bind;
    
    if (!desc) {
        goto done;
    }
    
    hash = vqec_nat_proto_hash((uint16_t)desc->internal_port);    
    VQE_TAILQ_FOREACH(p_bind, &s_stunproto.lh[hash], le) {
        if (vqec_nat_stunproto_is_same_binding(&p_bind->desc, desc)) {
            id = p_bind->id;
            break;
        }
    }

  done:
    return (id);
}


/**---------------------------------------------------------------------------
 * Form a STUN message, inject to the server for the binding.
 *
 * @param[in] p_bind Pointer to the binding: must be null-checked prior to
 * invocation of this code.
 * @param[out] boolean Returns true if the request was successfully 
 * composed and no errors were reported by the inject API.
 *---------------------------------------------------------------------------*/  
#define STUNPROTO_MAX_NAMESTR_LEN 257
static boolean
vqec_nat_stunproto_send_request (vqec_nat_stunproto_bind_t *p_bind)
{
    uint8_t username[STUNPROTO_MAX_NAMESTR_LEN];
    transaction_id_t *tid = NULL;
    uint8_t *stun_req_msg;
    uint32_t stun_req_len;
    char str1[INET_ADDRSTRLEN], str2[INET_ADDRSTRLEN];
    boolean ret = TRUE;
    abs_time_t last_req_time;
    int len = 0;

    strncpy(((char*)username), p_bind->desc.name, STUNPROTO_MAX_NAMESTR_LEN);
    len = strlen((char *)username);

    stun_req_msg = stun_generate_binding_request(username, 
                                                 len,
                                                 NULL, 
                                                 0, 
                                                 0, 
                                                 &tid, 
                                                 &stun_req_len);
    if (!stun_req_msg) {
        ret = FALSE;
        vqec_nat_log_error("Unable to generate STUN request %s",
                           __FUNCTION__);
        goto done;
    }
    
#if defined HAVE_UPNP && defined HAVE_STUN
    /* 
     * Send injected packets through hybrid manager
     * so the bind id may be translated.
     */
    if (!vqec_nat_hybrid_inject_tx(VQEC_NAT_PROTO_STUN,
                                   p_bind->id,
                                   &p_bind->desc,
                                   (char *)stun_req_msg,
                                   (uint16_t)stun_req_len)) {
#else
    if (!vqec_nat_inject_tx(p_bind->id, 
                            &p_bind->desc,
                            (char *)stun_req_msg,
                            (uint16_t)stun_req_len)) {
#endif
        ret = FALSE;
        vqec_nat_log_error("Unable to inject STUN request into socket %s",                           
                           __FUNCTION__);
        goto done;
    }

    last_req_time = p_bind->last_request_time;
    p_bind->last_request_time = get_sys_time();
    memcpy(&p_bind->tid, tid, sizeof(p_bind->tid));

    if (s_stunproto.debug_en) {
        vqec_nat_log_debug(
            "Proto: STUN request for %s:%d to %s:%d, (len: %d) "
            "(time: %llu) (delta time: %llu)\n",
            uint32_ntoa_r(p_bind->desc.internal_addr,
                          str1, sizeof(str1)),
            ntohs(p_bind->desc.internal_port),
            uint32_ntoa_r(p_bind->desc.remote_addr,
                          str2, sizeof(str2)),
            ntohs(p_bind->desc.remote_port), stun_req_len, 
            TIME_GET_A(msec, p_bind->last_request_time),
            TIME_GET_R(msec,
                       TIME_SUB_A_A(p_bind->last_request_time,
                                    last_req_time)));
    }

    
  done:
    CHECK_AND_FREE(tid);
    CHECK_AND_FREE(stun_req_msg);

    return (ret);
}    


/**---------------------------------------------------------------------------
 * (Re)send a STUN message to a server for a binding: this includes retries.
 *
 * @param[in] p_bind Pointer to the binding.
 *---------------------------------------------------------------------------*/  
static void
vqec_nat_stunproto_send_msg_to_server (vqec_nat_stunproto_bind_t *p_bind)
{
    char str1[INET_ADDRSTRLEN], str2[INET_ADDRSTRLEN];
    boolean ret;

    if (!p_bind) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        return;
    }
    
    if (p_bind->trials >= STUNPROTO_TRIALS_MAX) {
        /* Exceeded trial limit - reset state. */
        p_bind->trials = 0;
        p_bind->refresh_act = FALSE;
        p_bind->wait_ack = FALSE;
        memset(&p_bind->tid, 0, sizeof(p_bind->tid));

        if (s_stunproto.debug_en) {
            vqec_nat_log_debug(
                "Proto: STUN retrial limit exceeded for %s:%d to %s:%d\n",
                uint32_ntoa_r(p_bind->desc.internal_addr,
                              str1, sizeof(str1)),
                ntohs(p_bind->desc.internal_port),
                uint32_ntoa_r(p_bind->desc.remote_addr,
                              str2, sizeof(str2)),
                ntohs(p_bind->desc.remote_port));
        }
        return;        
    }

    ret = vqec_nat_stunproto_send_request(p_bind);
    if (ret) {
        p_bind->wait_ack = TRUE;
    } else {
        memset(&p_bind->tid, 0, sizeof(p_bind->tid));
        p_bind->wait_ack = FALSE; 
        if (s_stunproto.debug_en) {
            vqec_nat_log_debug(
                "Proto: STUN send request fail: id =  %u\n",
                p_bind->tid);
        }
    }

    p_bind->trials++;          /* "trials" between 1 ... TRIALS_MAX */
    if (!vqec_nat_timer_start(p_bind->ev_retry, 
                              stunproto_trial_timeouts[p_bind->trials])) {
        vqec_nat_log_error("Unable to start retry timer event %s",
                           __FUNCTION__);
    } else {
        /* A refresh is now effective. */
        p_bind->refresh_act = TRUE;
    }

}


/**---------------------------------------------------------------------------
 * STUN refresh event handler.
 *
 * @param[in] p_ev Event ponter: ignored.
 * @param[in] fd A file descriptor - not used.
 * @param[in] ev Event type - not used.
 * @param[in] data Is NULL for this handler and is ignored.
 *---------------------------------------------------------------------------*/  
static void
vqec_nat_stunproto_refresh_evt_handler (const struct vqec_event_ *const p_ev, 
                                        int32_t fd, int16_t ev, void *data) 
{
    uint32_t i;
    vqec_nat_stunproto_bind_t *p_bind;

    if (s_stunproto.is_not_behind_nat) {
        return;
    }

    for (i = 0; i < STUNPROTO_HASH_BUCKETS; i++) {
        VQE_TAILQ_FOREACH(p_bind,
                      &s_stunproto.lh[i], 
                      le) {

            if (!p_bind->refresh_act) {
                /*
                 * If an  per-element retry timer is active, then that
                 * timer will pop and retry - this case deals with the case
                 * when no such timer is running.
                 */
                vqec_nat_stunproto_send_msg_to_server(p_bind);
            } 
        }
    }
}


/**---------------------------------------------------------------------------
 * STUN retry event handler.
 *
 * @param[in] p_ev Event ponter: ignored.
 * @param[in] fd A file descriptor - not used.
 * @param[in] ev Event type - not used.
 * @param[in] data Pointer to the binding for which the event is generated.
 *---------------------------------------------------------------------------*/  
static void
vqec_nat_stunproto_retry_evt_handler (const struct vqec_event_ *const p_ev, 
                                      int32_t fd, int16_t ev, void *data) 
{
    vqec_nat_stunproto_bind_t *p_bind = 
        (vqec_nat_stunproto_bind_t *)data;
   
    if (!p_bind) {
        vqec_nat_log_error("Invalid binding pointer %s",
                           __FUNCTION__);
        return;
    }

    /* Retry sending the message. */ 
    vqec_nat_stunproto_send_msg_to_server(p_bind);
}


/**---------------------------------------------------------------------------
 * Initialize the NAT protocol. 
 * 
 * @param[in] params Protocol configuration parameters.
 * @param[out] Returns true on success.
 *---------------------------------------------------------------------------*/  
#define NAT_IDTABLE_BUCKETS 1
#define STUN_PERIODIC_TIMER TRUE
#define STUN_ONESHOT_TIMER FALSE

static boolean 
vqec_nat_stunproto_create (vqec_nat_init_params_t *params)
{
    boolean ret = TRUE;
    int32_t i;

    if (s_stunproto.init_done) {
        vqec_nat_log_error("Already initialized %s",
                           __FUNCTION__);
        goto done;
    }

    if (!params ||
        !params->max_bindings || 
        !params->refresh_interval ||
        (params->max_bindings > VQEC_NAT_MAX_BINDINGS)) {
        ret = FALSE;
        vqec_nat_log_error("Invalid input argument range %s",
                           __FUNCTION__);
        goto done;
    }

    /* Allocate Id table. */
    s_stunproto.id_table = id_create_new_table(NAT_IDTABLE_BUCKETS, 
                                               params->max_bindings);
   
    if (s_stunproto.id_table == ID_MGR_TABLE_KEY_ILLEGAL) { 
        ret = FALSE;
        vqec_nat_log_error("Failed to allocate id table %s",
                           __FUNCTION__);
        goto done;
    }

    s_stunproto.pool = zone_instance_get_loc("stunproto_pool",
                                             O_CREAT,
                                             sizeof(vqec_nat_stunproto_bind_t),
                                             params->max_bindings,
                                             NULL,
                                             NULL);
    if (!s_stunproto.pool) {
        vqec_nat_log_error("Failed to allocate nat element pool %s",
                           __FUNCTION__);        
        goto done;
    }

    s_stunproto.max_bindings = params->max_bindings;
    s_stunproto.refresh_interval = params->refresh_interval;
    for (i = 0; i < STUNPROTO_HASH_BUCKETS; i++) {
        VQE_TAILQ_INIT(&s_stunproto.lh[i]);
    }

    s_stunproto.ev_refresh = 
        vqec_nat_timer_create(STUN_PERIODIC_TIMER,
                              vqec_nat_stunproto_refresh_evt_handler, NULL);
    if (!s_stunproto.ev_refresh) {
        ret = FALSE;
        vqec_nat_log_error("Failed to allocate event refresh timer %s",
                           __FUNCTION__);
        goto done;
    }
    if (!vqec_nat_timer_start(s_stunproto.ev_refresh, 
                              s_stunproto.refresh_interval * 1000)) {
        vqec_nat_log_error("Failed to startup event refresh timer %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }

    s_stunproto.init_done = TRUE; 

  done:
    if (!ret) {
        vqec_nat_stunproto_destroy();
    }
    return (ret);
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
vqec_nat_stunproto_open (vqec_nat_bind_desc_t *desc)
{
    boolean ret = TRUE;
    vqec_nat_stunproto_bind_t *p_bind = NULL;
    char str1[INET_ADDRSTRLEN], str2[INET_ADDRSTRLEN] ;
    vqec_nat_bindid_t other_id;
    uint16_t hash;
    boolean allow_update = FALSE;
    
    if (!s_stunproto.init_done || 
        !desc ||
        (!desc->internal_addr && !desc->internal_port)) {
        ret = FALSE;
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        goto done;
    }
    
    other_id = vqec_nat_stunproto_find_internal(desc);
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

    p_bind = (vqec_nat_stunproto_bind_t *)zone_acquire(s_stunproto.pool);
    if (!p_bind) {
        ret = FALSE;
        vqec_nat_log_error("Memory allocation failed %s", 
                           __FUNCTION__);
        goto done;
    }
    memset(p_bind, 0, sizeof(*p_bind));

    p_bind->id = id_get(p_bind, s_stunproto.id_table);
    if (id_mgr_is_invalid_handle(p_bind->id)) { 
        ret = FALSE;
        vqec_nat_log_error("Id allocation failed %s",
                           __FUNCTION__);        
        goto done;
    }

    allow_update = desc->allow_update;
    if (!s_stunproto.is_not_behind_nat || allow_update) {        
        p_bind->ev_retry = 
            vqec_nat_timer_create(STUN_ONESHOT_TIMER,
                                  vqec_nat_stunproto_retry_evt_handler, p_bind);
        if (!p_bind->ev_retry) {
            ret = FALSE;
            vqec_nat_log_error("Event timer creation for element failed %s",
                               __FUNCTION__);
            goto done;        
        }
    }

    p_bind->desc = *desc;    
    hash = vqec_nat_proto_hash((uint16_t)p_bind->desc.internal_port); 
    VQE_TAILQ_INSERT_TAIL(&s_stunproto.lh[hash], p_bind, le);
    s_stunproto.open_bindings++;
    
    if (s_stunproto.is_not_behind_nat && !allow_update) { 
        /* Not behind a NAT. */
        p_bind->state = VQEC_NAT_BIND_STATE_NOTBEHINDNAT;
        p_bind->ext_addr = p_bind->desc.internal_addr;
        p_bind->ext_port = p_bind->desc.internal_port;
        p_bind->is_map_valid = TRUE; 
    } 

    if (s_stunproto.debug_en) {
        vqec_nat_log_debug("Proto: STUN opened binding %s:%d:%s:%d\n",
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
            if (!id_mgr_is_invalid_handle(p_bind->id)) { 
                id_delete(p_bind->id, s_stunproto.id_table);
            }
            zone_release(s_stunproto.pool, p_bind);            
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
vqec_nat_stunproto_close (vqec_nat_bindid_t id)
{
    vqec_nat_stunproto_bind_t *p_bind;
    char str1[INET_ADDRSTRLEN], str2[INET_ADDRSTRLEN] ;
    id_mgr_ret ret;
    uint16_t hash;

    if (!s_stunproto.init_done || id_mgr_is_invalid_handle(id)) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        return;
    }

    p_bind = (vqec_nat_stunproto_bind_t *)id_to_ptr(id, &ret, 
                                                    s_stunproto.id_table);
    if (!p_bind || (ret != ID_MGR_RET_OK)) {
        vqec_nat_log_error("Id lookup failure  %s",
                           __FUNCTION__); 
        return;
    }

    if (p_bind->ev_retry) {
        vqec_nat_timer_destroy(p_bind->ev_retry);
    }

    hash = vqec_nat_proto_hash((uint16_t)p_bind->desc.internal_port); 
    VQE_TAILQ_REMOVE(&s_stunproto.lh[hash], p_bind, le);   

    if (s_stunproto.debug_en) {
        vqec_nat_log_debug("Proto: STUN closed binding %s:%d:%s:%d\n",
                           uint32_ntoa_r(p_bind->desc.internal_addr,
                                         str1, sizeof(str1)),
                           ntohs(p_bind->desc.internal_port),
                           uint32_ntoa_r(p_bind->desc.remote_addr,
                                         str2, sizeof(str2)),
                           ntohs(p_bind->desc.remote_port));
    }

    id_delete(id, s_stunproto.id_table);    
    zone_release(s_stunproto.pool, p_bind);
    s_stunproto.open_bindings--;
}


/**---------------------------------------------------------------------------
 * Query the latest mapping for an existing binding. 
 * 
 * @param[in] id Identifier of the binding queried.
 * @param[out] data Pointer to the data associated with the mapping.
 * @param[in] refresh If the binding should be refreshed: this is done if
 * if no active refresh is in progress, the device is presumably behind NAT.
 * @param[out] boolean Returns true ifthe id is for a valid binding.
 *---------------------------------------------------------------------------*/  
static boolean 
vqec_nat_stunproto_query (vqec_nat_bindid_t id, vqec_nat_bind_data_t *data,
                          boolean refresh)
{
    vqec_nat_stunproto_bind_t *p_bind;
    id_mgr_ret id_ret;
    boolean ret = TRUE;
    char str[INET_ADDRSTRLEN];
    boolean allow_update = FALSE;
    
    if (!s_stunproto.init_done || 
        id_mgr_is_invalid_handle(id) || !data) {
        ret = FALSE;
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);        
        goto done;
    }

    memset(data, 0, sizeof(*data));

    p_bind = (vqec_nat_stunproto_bind_t *)id_to_ptr(id, &id_ret, 
                                                    s_stunproto.id_table);
    if (!p_bind || (id_ret != ID_MGR_RET_OK)) {
        ret = FALSE;
        vqec_nat_log_error("Id lookup failure %s",
                           __FUNCTION__); 
        goto done;
    }

    allow_update = p_bind->desc.allow_update;
    if (refresh &&
        (!s_stunproto.is_not_behind_nat || allow_update) &&
        !p_bind->refresh_act) {
     
        /* Enable immediate refresh. */
        vqec_nat_stunproto_send_msg_to_server(p_bind);
    }

    data->id = p_bind->id;
    data->desc = p_bind->desc;
    data->state = p_bind->state;
    data->is_map_valid = p_bind->is_map_valid;
    /****
     * NOTE: This is not a standards-based behavior, but internal to our
     * implementation. If the binding is not valid, return the ext_add rand
     * ext_port to be the same as the internal address and port. User must
     * check the is_map_valid flag to establish if a binding is complete.
     ****/ 
    if (data->is_map_valid) {
        data->ext_addr = p_bind->ext_addr;
        data->ext_port = p_bind->ext_port;
    } else {
        data->ext_addr = p_bind->desc.internal_addr;
        data->ext_port = p_bind->desc.internal_port;
    }

    if (s_stunproto.debug_en && !data->is_map_valid) {
        vqec_nat_log_debug("Proto: STUN Query binding id: %lu %s:%d\n",
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
 * Print information about one binding. 
 * 
 * @param[in] id Identifier of the binding.
 *---------------------------------------------------------------------------*/  
static void
vqec_nat_stunproto_fprint (vqec_nat_bindid_t id)
{
    vqec_nat_stunproto_bind_t *p_bind;
    id_mgr_ret id_ret;
    char str[INET_ADDRSTRLEN];
    
    if (!s_stunproto.init_done || 
        id_mgr_is_invalid_handle(id)) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);        
        return;
    }
    
    p_bind = (vqec_nat_stunproto_bind_t *)id_to_ptr(id, &id_ret, 
                                                    s_stunproto.id_table);
    if (!p_bind || (id_ret != ID_MGR_RET_OK)) {
        vqec_nat_log_error("Id lookup failure %s",
                           __FUNCTION__); 
        return;
    }
    
    vqec_nat_print(" Binding name:              %s\n",
                   p_bind->desc.name);
    vqec_nat_print(" NAT protocol:              STUN\n");
    vqec_nat_print(" NAT status:                %s\n",
                   vqec_nat_state_to_str(p_bind->state));
    vqec_nat_print(" Internal address:          %s:%u\n",
                   uint32_ntoa_r(p_bind->desc.internal_addr,
                                 str, sizeof(str)),
                   ntohs(p_bind->desc.internal_port));
    vqec_nat_print(" Public address:            %s:%u\n",
                   uint32_ntoa_r(p_bind->ext_addr,
                                 str, sizeof(str)),
                   ntohs(p_bind->ext_port));    
    vqec_nat_print(" Last request time:         %llu\n", 
                   TIME_GET_A(msec, p_bind->last_request_time));
    vqec_nat_print(" Last response time:        %llu\n", 
                   TIME_GET_A(msec, p_bind->last_response_time));
} 


/**---------------------------------------------------------------------------
 * Print information about all bindings. 
 *---------------------------------------------------------------------------*/  
static void
vqec_nat_stunproto_fprint_all (void)
{
    int32_t i;
    vqec_nat_stunproto_bind_t *p_bind;

    if (!s_stunproto.init_done) {
        vqec_nat_log_error("Bad input arguments %s", 
                           __FUNCTION__);        
        return;
    }
    
    
    vqec_nat_print("NAT protocol:               STUN\n");
    vqec_nat_print("NAT bindings open:          %d\n",
                   s_stunproto.open_bindings);

    for (i = 0; i < STUNPROTO_HASH_BUCKETS; i++) {
        VQE_TAILQ_FOREACH(p_bind,
                      &s_stunproto.lh[i], 
                      le) {

            vqec_nat_print("NAT id:                     %lu\n",
                           p_bind->id);
            vqec_nat_stunproto_fprint(p_bind->id);
        }    
    }
}


/**---------------------------------------------------------------------------
 * Inform the client layer of an update to a binding.
 *
 * @param[in] p_bind Pointer to the binding: must be null-checked prior to
 * invoking the method.
 *---------------------------------------------------------------------------*/  
static void
vqec_nat_stunproto_bind_update (vqec_nat_stunproto_bind_t *p_bind)
{
    vqec_nat_bind_data_t data;

    memset(&data, 0, sizeof(data));

    data.id = p_bind->id;
    data.desc = p_bind->desc;
    data.state = p_bind->state;
    data.is_map_valid = p_bind->is_map_valid;
     /****
     * NOTE: This is not a standards-based behavior, but internal to our
     * implementation. If the binding is not valid, return the ext_addr and
     * ext_port to be the same as the internal address and port. User must
     * check the is_map_valid flag to establish if a binding is complete.
     ****/ 
    if (data.is_map_valid) {
        data.ext_addr = p_bind->ext_addr;
        data.ext_port = p_bind->ext_port;
    } else {
        data.ext_addr = p_bind->desc.internal_addr;
        data.ext_port = p_bind->desc.internal_port;
    }

#if defined HAVE_UPNP && defined HAVE_STUN
    /* 
     * Inform combined manager of binding changes
     * so it may choose between STUN and UPNP.
     */
    vqec_nat_hybrid_bind_update(VQEC_NAT_PROTO_STUN,
                                data.id, &data);
#else
    vqec_nat_bind_update(data.id, &data);
#endif

    if (s_stunproto.debug_en) {
        vqec_nat_log_debug("Proto: STUN, updated bind id %lu, local port %d\n",
                           p_bind->id,
                           ntohs(p_bind->desc.internal_port));
    }

}


/**---------------------------------------------------------------------------
 * The device is not behind NAT. Update all bindings that are invalid, and
 * inform the client layer of updates.
 *---------------------------------------------------------------------------*/  
static void
vqec_nat_stunproto_not_behind_nat (void)
{
    uint32_t i;
    vqec_nat_stunproto_bind_t *p_bind;
    

    if (s_stunproto.ev_refresh) {
        /*sa_ignore {no recourse on failure} IGNORE_RETURN (1) */
        vqec_nat_timer_stop(s_stunproto.ev_refresh);
    }
    s_stunproto.is_not_behind_nat = TRUE;

    for (i = 0; i < STUNPROTO_HASH_BUCKETS; i++) {
        VQE_TAILQ_FOREACH(p_bind,
                      &s_stunproto.lh[i], 
                      le) {
            
            if (!p_bind->is_map_valid && !p_bind->desc.allow_update) {
                p_bind->trials = 0;
                p_bind->refresh_act = FALSE;
                p_bind->wait_ack = FALSE; 
                p_bind->is_map_valid = TRUE;
                p_bind->ext_addr = p_bind->desc.internal_addr;
                p_bind->ext_port = p_bind->desc.internal_port;
                memset(&p_bind->tid, 0, sizeof(p_bind->tid));
                p_bind->state = VQEC_NAT_BIND_STATE_NOTBEHINDNAT;
                if (p_bind->ev_retry) {
                    /*sa_ignore {no recourse on failure} IGNORE_RETURN (1) */
                    vqec_nat_timer_stop(p_bind->ev_retry);
                } 
                vqec_nat_stunproto_bind_update(p_bind); 
            }
        }
    }
}


/**
 * This function responds a STUN request, it is mainly used ICE connectivity
 * check. 
 * 
 * @param[in] p_bind: bind instance
 * @param[in] *stun_msg: the stun msg after parse request
 * @param[in] source_ip: the ip the request comes from.
 * @param[in] source_port: the port the request comes from. 
 */
static void 
vqec_nat_stunproto_send_stun_response (vqec_nat_stunproto_bind_t *p_bind,
                                       stun_message_t *stun_msg,
                                       in_addr_t source_ip, 
                                       in_port_t source_port)
{
    uint8_t *resp_msg = NULL;
    uint32_t resp_len = 0; 
    struct sockaddr_storage sa_storage;
    struct sockaddr_in *saddr = (struct sockaddr_in *)(&sa_storage);

    memset(&sa_storage, 0, sizeof (struct sockaddr_storage));
    saddr->sin_family = AF_INET;
    saddr->sin_addr.s_addr = source_ip;
    saddr->sin_port = source_port;

    sock_to_stun_addr(&sa_storage, &(stun_msg->mapped_address.content));
    stun_msg->mapped_address.is_valid = true;

    resp_msg = stun_generate_binding_response(stun_msg, &resp_len);

    if (!vqec_nat_inject_tx(p_bind->id, 
                            &p_bind->desc,
                            (char *)resp_msg,
                            (uint16_t)resp_len)) {
        vqec_nat_log_error("Unable to inject STUN response into socket %s",                           
                           __FUNCTION__);
    }

    if (resp_msg) {
        free(resp_msg);
    }
}


/**---------------------------------------------------------------------------
 * The internals of processing a STUN response from the server.
 * 
 * @param[in] p_bind Pointer to the NAT binding.
 * @param[in] msg_buffer Input stun message.
 * @param[in] len Length of the inbound stun message.
 *---------------------------------------------------------------------------*/  
static void
vqec_nat_stunproto_process_stun_msg (vqec_nat_stunproto_bind_t *p_bind, 
                                     uint8_t *msg_buffer, uint16_t len,
                                     in_addr_t source_ip, in_port_t source_port) 
{
    stun_message_t stun_msg;
    uint32_t ret_code;
    char str[INET_ADDRSTRLEN];
    in_addr_t ext_addr;
    in_port_t ext_port;

    if (s_stunproto.debug_en) {
        vqec_nat_log_debug("Recv new stun msg, refresh=%d, wait_ack=%d, len=%d "
                           " bind id  = %lu\n",
                           p_bind->refresh_act,
                           p_bind->wait_ack,
                           len,
                           p_bind->id);
    }

    ret_code = stun_message_parse(msg_buffer, len, &stun_msg, false);
    if (ret_code) {
        if (s_stunproto.debug_en) {
            vqec_nat_log_debug("Proto: STUN, stun response parse failed  "
                               "bind id %lu, ret_code %d\n",
                               p_bind->id,
                               ret_code);
        }
        return;
    }

    /* this is a stun request from server, need to send response now */
    if (stun_msg.stun_header.message_type == STUN_BINDING_REQ) {
       if (s_stunproto.debug_en) {
            vqec_nat_log_debug("Proto: STUN %llu msec, receiv one stun "
                               "request for bind id %lu, local port %d\n",
                               TIME_GET_A(msec, get_sys_time()),
                               p_bind->id,
                               ntohs(p_bind->desc.internal_port));
        }
        vqec_nat_stunproto_send_stun_response(p_bind, &stun_msg,
                                              source_ip, source_port);
        return;
    }


    /* process stun response */
    if (!p_bind->refresh_act || 
        !p_bind->wait_ack) {

        if (s_stunproto.debug_en) {
            vqec_nat_log_debug("Proto: STUN %llu msec, stale or spurious "
                               "response for bind id %lu, local port %d\n",
                               TIME_GET_A(msec, get_sys_time()),
                               p_bind->id,
                               ntohs(p_bind->desc.internal_port));
        }
        return;
    }

    p_bind->wait_ack = FALSE;   /* Acknowledged one response */
    if (!stun_message_match_transaction_id(&p_bind->tid,
                                           msg_buffer)) { 
        if (s_stunproto.debug_en) {
            vqec_nat_log_debug("Proto: STUN, transaction id does not match "
                               "bind id %lu, local port %d\n",
                               p_bind->id,
                               ntohs(p_bind->desc.internal_port));
        }
        return;
    }

    /* Update last response time,. reset tid.  */
    p_bind->last_response_time = get_sys_time();
    memset(&p_bind->tid, 0, sizeof(p_bind->tid));

    /* Ensure that the IP address is valid. */
    if (stun_msg.mapped_address.is_valid) {
        ext_addr = 
            stun_msg.mapped_address.content.addr.ipv4_addr.s_addr;
        /* 
         * stun message parser converts the network byte order port
         * to host order, therefore, the reverse conversion.
         */
        ext_port = htons(stun_msg.mapped_address.content.port);

        if (s_stunproto.debug_en) {
            vqec_nat_log_debug("Proto: STUN %llu msec, Bind id %lu, "
                               "internal port %d has Public address: %s:%u\n",
                               TIME_GET_A(msec, get_sys_time()),
                               p_bind->id, ntohs(p_bind->desc.internal_port),
                               uint32_ntoa_r(ext_addr, str, sizeof(str)),
                               ntohs(ext_port));   
        } 

    } else {

        if (s_stunproto.debug_en) {
            vqec_nat_log_debug("Proto: STUN, stun IP address parse failed  "
                               "bind id %lu, local port %d\n",
                               p_bind->id,
                               ntohs(p_bind->desc.internal_port));
        }
        return;                 /* continue with refresh */
    }
        
    /* Trial successful - update state */
    p_bind->trials = 0;
    p_bind->refresh_act = FALSE;
    p_bind->is_map_valid = TRUE;    
    if (p_bind->ev_retry) {
        /*sa_ignore {no recourse on failure} IGNORE_RETURN (1) */
        vqec_nat_timer_stop(p_bind->ev_retry);
    }

    /* Has the mapping-changed? */
    if ((p_bind->ext_addr != ext_addr) || 
        (p_bind->ext_port != ext_port)) {
        
        p_bind->ext_addr = ext_addr;
        p_bind->ext_port = ext_port; 
        
        if ((p_bind->ext_addr == p_bind->desc.internal_addr) &&
            (p_bind->ext_port == p_bind->desc.internal_port)) {
            /* The device is not behind a NAT. */
            p_bind->state = VQEC_NAT_BIND_STATE_NOTBEHINDNAT;  
            vqec_nat_stunproto_bind_update(p_bind);
            vqec_nat_stunproto_not_behind_nat();

        } else {

            p_bind->state = VQEC_NAT_BIND_STATE_BEHINDNAT;
            /* Inform the client layer of an update on this binding. */
            vqec_nat_stunproto_bind_update(p_bind);            

        }

        if (s_stunproto.debug_en) {
            vqec_nat_log_debug("Bind id %lu, internal port %d changed "
                               "Public address: %s:%u\n",
                               p_bind->id, p_bind->desc.internal_port,
                               uint32_ntoa_r(ext_addr, str, sizeof(str)),
                               ntohs(ext_port));
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
 * @param[in] source_ip: source ip of the packet (ip of the pak sender) 
 * @param[in] source_port: source port of the packet
 *---------------------------------------------------------------------------*/  
#define NAT_STUNPROTO_MAX_BUFSIZE 1500  /* common ethernet MTU */
static void 
vqec_nat_stunproto_eject (vqec_nat_bindid_t id, char *buf, uint16_t len,
                          in_addr_t source_ip, in_port_t source_port)
{
    id_mgr_ret ret;
    vqec_nat_stunproto_bind_t *p_bind;

    if (!s_stunproto.init_done || 
        id_mgr_is_invalid_handle(id) || 
        !buf || 
        !len || 
        len > NAT_STUNPROTO_MAX_BUFSIZE) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);        
        return;
    }

    p_bind = (vqec_nat_stunproto_bind_t *)id_to_ptr(id, &ret, 
                                                    s_stunproto.id_table);
    if (!p_bind || (ret != ID_MGR_RET_OK)) {
        vqec_nat_log_error("Id lookup failure %s",
                           __FUNCTION__); 
        return;
    }
    
    vqec_nat_stunproto_process_stun_msg(p_bind, (uint8_t *)buf, len,
                                        source_ip, source_port);
}


/**---------------------------------------------------------------------------
 * Enable debug for NAT. 
 * 
 * @param[in] verbose Controls verbose mode for debugging.
 *---------------------------------------------------------------------------*/  
static void 
vqec_nat_stunproto_debug_set (boolean verbose)
{
    s_stunproto.debug_en = TRUE;
    s_stunproto.verbose = verbose;
}


/**---------------------------------------------------------------------------
 * Disable debug for NAT. 
 *---------------------------------------------------------------------------*/  
static void 
vqec_nat_stunproto_debug_clr (void)
{
    s_stunproto.debug_en = FALSE;    
}


/**---------------------------------------------------------------------------
 * Is the device behind a NAT. The method is "permissive", in that it will
 * return "behind NAT" until it is established that it is not.
 *
 * @param[out] boolean Returns true if the device is behind a NAT. 
 *---------------------------------------------------------------------------*/  
static boolean 
vqec_nat_stunproto_is_behind_nat (void) 
{
    return (!s_stunproto.is_not_behind_nat);
}


/**---------------------------------------------------------------------------
 * Interface definition.
 *---------------------------------------------------------------------------*/  
static vqec_nat_proto_if_t s_stunproto_if = 
{

    .create = vqec_nat_stunproto_create,
    .destroy = vqec_nat_stunproto_destroy,
    .open = vqec_nat_stunproto_open,
    .close = vqec_nat_stunproto_close,
    .query = vqec_nat_stunproto_query,
    .fprint = vqec_nat_stunproto_fprint,
    .fprint_all = vqec_nat_stunproto_fprint_all,
    .eject_rx = vqec_nat_stunproto_eject,
    .debug_set = vqec_nat_stunproto_debug_set,
    .debug_clr = vqec_nat_stunproto_debug_clr,
    .is_behind_nat = vqec_nat_stunproto_is_behind_nat
   
};

const vqec_nat_proto_if_t *
vqec_nat_get_if_stunproto (void)
{
    return (&s_stunproto_if);
}
