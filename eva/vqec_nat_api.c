/*
 * Copyright (c) 2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file contains the VQE-C NAT solution implementation.
 */

#include "vqec_nat_interface.h"
#include <stdarg.h>
#include "vqec_error.h"
#include "vqec_debug.h"
#include "vqec_event.h"
#include "vqec_channel_private.h"
#include "vqec_lock_defs.h"

typedef 
struct vqec_nat_
{
    /**
     * Is the module initialized.
     */
    boolean init_done;
    /**
     * Is NAT mode enabled?
     */
    boolean nat_mode;
    /**
     * NAT protocol in use.
     */
    vqec_nat_proto_t proto;
    /**
     * Max. packet limit for injection.     
     */
    uint32_t max_paksize;
    /**
     * Proto interface. 
     */
    const vqec_nat_proto_if_t *proto_if;

} vqec_nat_t;

static vqec_nat_t m_natmodule;


/**---------------------------------------------------------------------------
 * Log an error through the client.
 * 
 * @param[in] str Format string; variable arguments.
 *---------------------------------------------------------------------------*/  
void 
vqec_nat_log_error (const char *fmt, ...)
{
    va_list ap;
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    
    va_start(ap, fmt);
    (void)vsnprintf(msg_buf, VQEC_LOGMSG_BUFSIZE, fmt, ap);
    va_end(ap);

    syslog_print(VQEC_ERROR, msg_buf);
}


/**---------------------------------------------------------------------------
 * Log a debug message through the client.
 * 
 * @param[in] str Format string; variable arguments.
 *---------------------------------------------------------------------------*/  
void vqec_nat_log_debug (const char *fmt, ...)
{
    va_list ap;
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    
    va_start(ap, fmt);
    (void)vsnprintf(msg_buf, VQEC_LOGMSG_BUFSIZE, fmt, ap);
    va_end(ap);

    VQEC_DEBUG(VQEC_DEBUG_NAT, "%s", msg_buf);
}


/**---------------------------------------------------------------------------
 * Print a console message via the client.
 * 
 * @param[in] str Format string; variable arguments.
 *---------------------------------------------------------------------------*/  
void vqec_nat_print (const char *fmt, ...)
{
    va_list ap;
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    
    va_start(ap, fmt);
    (void)vsnprintf(msg_buf, VQEC_LOGMSG_BUFSIZE, fmt, ap);
    va_end(ap);

    CONSOLE_PRINTF("%s", msg_buf);
}


/**---------------------------------------------------------------------------
 * Inject a NAT packet via the channel that owns the socket.
 * 
 * @param[in] id Id of the binding sending the NAT packet.
 * @param[in] desc Attributes of the binding.
 * @param[in] buf Pointer to the buffer.
 * @param[in] len Length of the buffer.
 * @param[out] boolean Returns true if the successfully enqueued for 
 * transmit to the socket or to the dataplane.
 *---------------------------------------------------------------------------*/  
boolean 
vqec_nat_inject_tx (vqec_nat_bindid_t id, 
                    vqec_nat_bind_desc_t *desc, char *buf, uint16_t len)
{
    if (!m_natmodule.init_done ||
        id_mgr_is_invalid_handle(id) ||
        !desc || 
        !buf || 
        !len || 
        len > m_natmodule.max_paksize) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return (FALSE);
    }
    
    return 
        vqec_chan_nat_inject_pak(desc->caller_id, id, desc, buf, len);
}


/**---------------------------------------------------------------------------
 * Callback by the protocol when a binding is updated.
 * 
 * @param[in] id Id of the binding updated.
 * @param[in] data Data associated with the binding.
 *---------------------------------------------------------------------------*/  
void
vqec_nat_bind_update (vqec_nat_bindid_t id, 
                      vqec_nat_bind_data_t *data)
{

    if (!m_natmodule.init_done ||
        id_mgr_is_invalid_handle(id) || !data) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return;
    }
    
    vqec_chan_nat_bind_update(data->desc.caller_id, id, data);
}


/**---------------------------------------------------------------------------
 * Create a libevent timer.
 * 
 * @param[in] periodic If the timer is periodic, set to true.
 * @param[in] handler Handler associated with the timer.
 * @param[in] arg Argument to be echoed in the callback.
 * @param[out]  vqec_event_* A pointer to an opaque event structure.
 *---------------------------------------------------------------------------*/  
struct vqec_event_ *
vqec_nat_timer_create (boolean periodic, 
                       vqec_nat_proto_evt_handler handler, void *arg)
{
    vqec_event_t *p_ev;

    if (!m_natmodule.init_done || !handler) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return (NULL);
    }
    
    if (!vqec_event_create(&p_ev,
                           VQEC_EVTYPE_TIMER,
                           periodic ? VQEC_EV_RECURRING : VQEC_EV_ONESHOT,
                           handler,
                           VQEC_EVDESC_TIMER,
                           arg)) {
        syslog_print(VQEC_ERROR, 
                     "Time creation failed in nat timer create");
        return (NULL);
    }

    return (p_ev);
}


/**---------------------------------------------------------------------------
 * Destroy a libevent timer. A destroy also stops a timer that is running.
 * 
 * @param[in] p_timer Pointer to the timer to be destroyed.
 *---------------------------------------------------------------------------*/ 
void 
vqec_nat_timer_destroy (struct vqec_event_ *p_timer)
{
    if (p_timer) {
        vqec_event_destroy(&p_timer);
    }
}


/**---------------------------------------------------------------------------
 * Start a libevent timer.
 * 
 * @param[in] p_timer Pointer to the timer to be start.
 * @param[in] timeout_msec Timeout in msecs.
 * @param[out] boolean Returns true if the timer was successfully started.
 *---------------------------------------------------------------------------*/ 
boolean 
vqec_nat_timer_start (struct vqec_event_ *p_timer, 
                      uint32_t timeout_msec)
{
    struct timeval tv;

    if (!m_natmodule.init_done || !p_timer || !timeout_msec) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return (FALSE);
    }

    tv.tv_sec = timeout_msec / 1000;
    tv.tv_usec = (timeout_msec - (tv.tv_sec * 1000)) * 1000;

    return 
        vqec_event_start(p_timer, &tv);
}


/**---------------------------------------------------------------------------
 * Stop a libevent timer.
 * 
 * @param[in] p_timer Pointer to the timer to be stopped.
 * @param[out] boolean Returns true if the timer was successfully stopped.
 *---------------------------------------------------------------------------*/ 
boolean 
vqec_nat_timer_stop (struct vqec_event_ *p_timer)
{
    if (!m_natmodule.init_done || !p_timer) {
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        return (FALSE);
    }
 
    return 
        vqec_event_stop(p_timer);
}

/**---------------------------------------------------------------------------
 * Acquire vqec global lock.
 *---------------------------------------------------------------------------*/ 
void
vqec_nat_acquire_lock(void)
{
    vqec_lock_lock(vqec_g_lock);
}

/**---------------------------------------------------------------------------
 * Release vqec global lock.
 *---------------------------------------------------------------------------*/ 
void
vqec_nat_release_lock(void)
{
    vqec_lock_unlock(vqec_g_lock);
}

/**---------------------------------------------------------------------------
 * A textual representation of the NAT binding state.
 * 
 * @param[in] state NAT binding state.
 * @param[out] char* Textual representation of the state.
 *---------------------------------------------------------------------------*/ 
const char *
vqec_nat_state_to_str (vqec_nat_bind_state_t state)
{
    switch (state) {
    case VQEC_NAT_BIND_STATE_UNKNOWN:
        return "Unknown";
    case VQEC_NAT_BIND_STATE_NOTBEHINDNAT:
        return "Not Behind NAT";
    case VQEC_NAT_BIND_STATE_BEHINDNAT:
        return "Behind NAT";
    default:
        return "Invalid!";
    }
}


/**---------------------------------------------------------------------------
 * Deinitialize the module.
 *---------------------------------------------------------------------------*/  
void 
vqec_nat_module_deinit (void)
{
    if (m_natmodule.init_done) {
        if (m_natmodule.proto_if) {
            (*m_natmodule.proto_if->destroy)();
        }
    }

    memset(&m_natmodule, 0, sizeof(m_natmodule));
}


/**---------------------------------------------------------------------------
 * Initialize the NAT module. 
 * 
 * @param[in] mode Current NAT mode. If NAT mode is standard, the
 * null NAT protocol will be used.
 * @param[in] params Protocol configuration parameters.
 * @param[out] Returns VQEC_OK on success, error code on failure.
 *---------------------------------------------------------------------------*/  
vqec_error_t 
vqec_nat_module_init (vqec_syscfg_sig_mode_t mode,
                      vqec_nat_proto_t proto, 
                      vqec_nat_init_params_t *params)
{
    vqec_error_t err = VQEC_OK;

    if (m_natmodule.init_done) {
        syslog_print(VQEC_ERROR, "NAT module already initialized");
        return (err);
    }

    if (!params ||
        !params->max_bindings || 
        !params->max_paksize ||
        (proto < VQEC_NAT_MIN_PROTO) ||        
        (proto > VQEC_NAT_MAX_PROTO) ||        
        ((mode != VQEC_SM_STD) && (mode != VQEC_SM_NAT))) {
        
        err = VQEC_ERR_INVALIDARGS;
        syslog_print(VQEC_INVALIDARGS, __FUNCTION__);
        goto done;
    }

    m_natmodule.proto = proto;    
    if (mode == VQEC_SM_STD) {
        m_natmodule.proto_if = NAT_GET_PROTO_INTF(natproto);
    } else {
        m_natmodule.nat_mode = TRUE;
#if defined HAVE_UPNP || defined HAVE_STUN
        if (params->refresh_interval) {
            /* Only use STUN or UPnP if configured 
             * and refresh interval is non-zero */
#if defined HAVE_UPNP && defined HAVE_STUN
            m_natmodule.proto_if = NAT_GET_PROTO_INTF(hybridproto);
#elif defined HAVE_UPNP
            m_natmodule.proto_if = NAT_GET_PROTO_INTF(upnpproto);
#elif defined HAVE_STUN
            m_natmodule.proto_if = NAT_GET_PROTO_INTF(stunproto);
#else
            m_natmodule.proto_if = NAT_GET_PROTO_INTF(natproto);
#endif
        } else {
            m_natmodule.proto_if = NAT_GET_PROTO_INTF(natproto);
        }
#else
        m_natmodule.proto_if = NAT_GET_PROTO_INTF(natproto);
        syslog_print(VQEC_ERROR, 
                     "No NAT protocol is available! using a null protocol");
#endif 
    }
    
    /* Allow protocol handlers to call back. */
    m_natmodule.init_done = TRUE;    
    m_natmodule.max_paksize = params->max_paksize;

    if (!((*m_natmodule.proto_if->create)(params))) {
        err = VQEC_ERR_INTERNAL;
        syslog_print(VQEC_ERROR, "Initialization of NAT protocol failed");
        goto done;    
    }

  done:
    if (err != VQEC_OK) {
        vqec_nat_module_deinit();
    }
    
    return (err);
}


/**---------------------------------------------------------------------------
 * Request creation / opening of a binding for a 4-tuple:
 *    (inside IP addr, inside IP port, remote IP addr, remote port).
 * A new binding is created and an identifier is returned for the binding.
 * 
 * @param[in] desc The attributes of the binding.
 * @param[out] vqec_nat_bindid_t  Identifier of the created binding.
 *---------------------------------------------------------------------------*/  
vqec_nat_bindid_t 
vqec_nat_open_binding (vqec_nat_bind_desc_t *desc)
{
    if (!m_natmodule.init_done) {
        return (VQEC_NAT_BINDID_INVALID);
    }

    return
        ((*m_natmodule.proto_if->open)(desc));
}


/**---------------------------------------------------------------------------
 * Close an open binding.
 *
 * @param[in] vqec_nat_bindid_t  Identifier of the binding.
 *---------------------------------------------------------------------------*/  
void
vqec_nat_close_binding (vqec_nat_bindid_t id)
{
    if (m_natmodule.init_done) {        
        (*m_natmodule.proto_if->close)(id);
    }
}


/**---------------------------------------------------------------------------
 * Query an open binding.
 *
 * @param[in] vqec_nat_bindid_t  Identifier of the binding.
 * @param[out] data All data associated with the binding / mapping.
 * @param[in] refresh Refresh the binding if it is incomplete. 
 * @param[out] boolean Returns true if the query succeeds.
 *---------------------------------------------------------------------------*/  
boolean
vqec_nat_query_binding (vqec_nat_bindid_t id, 
                        vqec_nat_bind_data_t *data, boolean refresh)
{
    if (!m_natmodule.init_done) {
        return (FALSE);
    }

    return 
        ((*m_natmodule.proto_if->query)(id, data, refresh));
}


/**---------------------------------------------------------------------------
 * Print the status of a binding.
 *
 * @param[in] vqec_nat_bindid_t  Identifier of the binding.
 *---------------------------------------------------------------------------*/  
void
vqec_nat_fprint_binding (vqec_nat_bindid_t id)
{
    if (m_natmodule.init_done) {
        (*m_natmodule.proto_if->fprint)(id); 
    }
}


/**---------------------------------------------------------------------------
 * Print the status of all bindings.
 *---------------------------------------------------------------------------*/  
void
vqec_nat_fprint_all_bindings (void)
{
    if (m_natmodule.init_done) {
        (*m_natmodule.proto_if->fprint_all)();
    }
}


/**---------------------------------------------------------------------------
 * Print the status of all bindings, with acquisition of the global lock.
 *---------------------------------------------------------------------------*/  
void
vqec_nat_fprint_all_bindings_safe (void)
{
    if (m_natmodule.init_done) {
        vqec_lock_lock(vqec_g_lock);
        (*m_natmodule.proto_if->fprint_all)();
        vqec_lock_unlock(vqec_g_lock);
    }
}


/**---------------------------------------------------------------------------
 * Forward an eject packet to the protocol for a binding.
 * 
 * @param[in] vqec_nat_bindid_t  Identifier of the binding.
 * @param[out] buf Contents of the packet.
 * @param[out] len Length of the packet.
 * @param[in] source_ip: source ip of the packet (ip of the pak sender) 
 * @param[in] source_port: source port of the packet
 *---------------------------------------------------------------------------*/  
void 
vqec_nat_eject_to_binding (vqec_nat_bindid_t id, char *buf, uint16_t len,
                           in_addr_t source_ip, in_port_t source_port)
{
    if (m_natmodule.init_done) {
        (*m_natmodule.proto_if->eject_rx)(id, buf, len, source_ip, source_port);
    }    
}


/**---------------------------------------------------------------------------
 * Enable debugging on protocol.
 *
 * @param[in] verbose Enable a more verbose mode.
 *---------------------------------------------------------------------------*/  
void
vqec_nat_enable_debug (boolean verbose)
{
    if (m_natmodule.init_done) {
        (*m_natmodule.proto_if->debug_set)(verbose); 
    }
}


/**---------------------------------------------------------------------------
 * Disable debugging on protocol.
 *---------------------------------------------------------------------------*/  
void
vqec_nat_disable_debug (void)
{
    if (m_natmodule.init_done) {
        (*m_natmodule.proto_if->debug_clr)(); 
    }
}


/**---------------------------------------------------------------------------
 * Is the device behind a NAT?
 *
 * @param[out] Returns false if it the protocol has determined that the
 * device is not behind a NAT, true otherwise.
 *---------------------------------------------------------------------------*/  
boolean
vqec_nat_is_behind_nat (void)
{
    if (!m_natmodule.init_done) {
        return (TRUE);          /* conservative - cannot be established */
    }

    return 
        ((*m_natmodule.proto_if->is_behind_nat)());
}


/**---------------------------------------------------------------------------
 * Is the system in NAT mode?
 *
 * @param[out] Returns true if it the system is configured for NAT mode.
 *---------------------------------------------------------------------------*/  
boolean
vqec_nat_is_natmode_en (void)
{
    if (!m_natmodule.init_done) {
        return (FALSE);
    }

    return (m_natmodule.nat_mode);
}
