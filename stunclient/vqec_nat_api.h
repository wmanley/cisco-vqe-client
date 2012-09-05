/*
 * Copyright (c) 2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file contains APIs for VQE-C's NAT solution.  Basic functions
 * are init, deinit, add, remove, and get binding information.
 */

/**---------------------------------------------------------------------------

UDP NAT

------------         -----------         -----------
| NAT      | <-----> | VQEC    | <-----> | VQEC    |
| PROTOCOL | NAT API | NAT API | NAT IF  | CHANNEL |
------------         -----------         -----------

NAT protocol
------------

A NAT protocol is an abstraction created to hide the details of the actual
mechanism that allows a device to operate behind a NAT. VQE-C currently
supports STUN, but extensions to support UPnP may be possible wit the
same abstraction.

The architectural goals of the abstraction are:
-- No "root" privilege support or explicit kernel support.
-- Keep NAT indepedent of core VQE-C, with a possible reuse of the 
abstraction as a more general purpose utility in a system.
-- The protocol may need to send packets to / receive packets from UDP
ports that have been opened by other entities in the system.  It is therefore
a goal to establish an "inject" (packet transmit) path and an "eject" 
path (packet receive) between the protocol and the entities that own
these sockets.
-- Keep the abstraction independent of packet transport, i.e., the details of
where packets are received from / sent to is transparent to the protocol.

A NAT protocol must implement the NAT protocol interface, vqec_nat_proto_if,
which is defined by the following methods:

--  boolean (*create)(uint32_t max_bindings, uint32_t refresh_interval)

This method must be called on the protocol before the protocol can
be used. The parameter max_bindings defines the maximum number of
NAT bindings the protocol may be asked to support. The refresh
interval is specified for protocols that may wish to periodically
refresh their bindings, and is specified in seconds. If the protocol
can support the requested parameters it should return a true in
response.

--- void (*destroy)(void)

The method will be called by a client of the protocol to shutdown it's
services. The protocol must release all resources / bindings during
this call.

-- vqec_nat_bindid_t (*open)(vqec_nat_bind_desc_t *desc)

This method will be invoked by a client of the protocol to open a new
NAT binding. The NAT binding descriptor defines the parameters of
the binding that needs to be opened. It consists of a 4-tuple, 
<internal IP address, internal IP port, remote IP address, remote IP port>
and an opaque identifier of the caller (a vqec channel) which
is opening up the binding. This method *MUST* not call back into
the client to inject packets or request other services. If the
binding can be successfully opened, the method will return an
identifier of the binding, otherwise an invalid identifier should
be returned.

-- void (*close)(vqec_nat_bindid_t id);

Invoked when a previously opended NAT binding with the specified id
is to be closed.

-- boolean (*query)(vqec_nat_bindid_t id, 
                    vqec_nat_bind_data_t *data, boolean refresh);

Clients may query an open NAT binding to see if the binding
has been resolved or not. The behavior of this method as follows:
  * If the binding is incomplete, and refresh is set to true,
    the protocol *MUST* schedule an immediate refresh for the
    binding. The protocol *MAY* invoke VQE-C APIs to inject 
    packets within the context of this call.
  * If the binding is incomplete, the method *MUST* reflect
    the <internal IP address, internal port> in the external
    mapping returned in the "data" output argument.


-- void (*eject_rx)(vqec_nat_bindid_t id, char *buf, uint16_t len)

This method will be invoked by clients of the protocol when a 
NAT packet is received on a particular binding, defined by the
NAT binding id. The packet is ejected to the protocol using this
method. This handler is for the "inbound" direction, i.e., packets 
that are received on a socket. In particular, for STUN, this method
is used to hand stun responses to the STUN protocol.

-- void (*fprint)(vqec_nat_bindid_t id)

The protocol will print the status of a particular binding using
a print API which is provided by the client of the protocol.

-- void (*fprint_all)(void)

Print the status of all bindings using a print API which is
provided by the client of the protocol.

-- void (*debug_set)(boolean verbose)

Enable debugging on the protocol. The debug messages are logged
using a debug logging API which is provided by the client of
the protocol.

-- void (*debug_clr)(void)

Disable protocol debug.

-- boolean (*is_behind_nat)(void)

This method will return true if the protocol has conclusively
established whether the device on which the protocol is running
is behind a NAT.



APIs Provided to the NAT protocol by the VQEC
---------------------------------------------

-- void vqec_nat_log_error(const char *str, ...)

The protocol should call this method when it wants to log a
syslog error message.

-- void vqec_nat_log_debug(const char *str, ...)

The protocol should call this method when it wants to log
a debug message.

-- void vqec_nat_print(const char *str, ...)

The protocol should call this method when it wants to print
status to the console.

-- boolean vqec_nat_inject_tx(vqec_nat_bindid_t id, 
                           vqec_nat_bind_desc_t *desc, 
                           char *buf, 
                           uint16_t len)

The protocol should call this method when it wants to inject a packet
on a particular binding through the VQEC. The client is responsible
for actually transmitting the packet. In particular, for STUN, this method
is used to transmit stun requests to a server.

-- void vqec_nat_bind_update(vqec_nat_bindid_t id, 
                             vqec_nat_bind_data_t *data)

The protocol will call this method whenever a binding update
is seen for a particular NAT binding. This transition will normally
be from incomplete-to-complete. The function *MAY* be called 
within the context of an "eject", or it may be called asynchronously
by the protocol.

-- struct vqec_event_ *
   vqec_nat_timer_create(boolean periodic, 
                         vqec_nat_proto_evt_handler handler, void *arg)

The protocol can create timers (periodic or one-shot) through this 
VQEC provided API. Through this mechanism the protocol can use a 
completely transparent timer interface.

-- void vqec_nat_timer_destroy(struct vqec_event_ *timer);

Destroy a previously created timer.

-- boolean vqec_nat_timer_start(struct vqec_event_ *timer, 
                                uint32_t timeout_msec);

Start a timer for a particular timeout period.

-- boolean vqec_nat_timer_stop(struct vqec_event_ *timer);

Stop a previously started timer.



How to Register the Protocol Interface?
---------------------------------------
Each protocol must have it's protocol interface extern'ed in vqec_nat_api.h,
using The macro

NAT_GET_PROTO_INTF(p) vqec_nat_get_if_ ##p()

Currently the following 2 protocols are registered:

const vqec_nat_proto_if_t *NAT_GET_PROTO_INTF(natproto); 
const vqec_nat_proto_if_t *NAT_GET_PROTO_INTF(stunproto);

"natproto" is a "dummy" NAT protocol, that provides "identical" internal to
external port mapping, and is used when STUN is not present.

"stunproto" is the STUN protocol.


vqec_nat_interface.h
---------------------
This file "wraps" all of the functions implemented by the NAT protocol for
use by VQEC channel, so that the actual use of a specific protocol is 
hidden from the VQEC channel. This file *MUST* not be used by the protocol.


stunclient/vqec_stun_mgr.c
--------------------------
This is the only file in the stunclient directory. No other header files 
are needed, since the entire protocol is defined the NAT protocol interface.

 *---------------------------------------------------------------------------*/ 


#ifndef __VQEC_NAT_API_H__
#define __VQEC_NAT_API_H__

#ifdef __cplusplus
extern "C" 
{
#endif /* __cplusplus */

#include <utils/vam_types.h>
#include <netinet/in.h>

struct vqec_event_;

typedef uint32_t vqec_nat_bindid_t;
#define VQEC_NAT_BINDID_INVALID 0

/**
 * NAT protocols.
 */
typedef
enum vqec_nat_proto_
{
    /**
     * A "null" protocol.
     */
    VQEC_NAT_PROTO_NULL,
    /**
     * STUN proto.
     */
    VQEC_NAT_PROTO_STUN,
    /**
     * UPNP proto.
     */
    VQEC_NAT_PROTO_UPNP,
    /**
     * Must be last.
     */
    VQEC_NAT_PROTO_NUM_VAL,

} vqec_nat_proto_t;

#define VQEC_NAT_MIN_PROTO VQEC_NAT_PROTO_NULL
#define VQEC_NAT_MAX_PROTO (VQEC_NAT_PROTO_NUM_VAL - 1)

/**
 * Maxiumum limit on the number of bindings that may be supported.
 */
#define VQEC_NAT_MAX_BINDINGS 256

/**
 * NAT binding state.
 */
typedef
enum vqec_nat_bind_state_
{
    /**
     * Not known at present.
     */
    VQEC_NAT_BIND_STATE_UNKNOWN,
    /**
     * Not behind a NAT.
     */
    VQEC_NAT_BIND_STATE_NOTBEHINDNAT,
    /**
     * Behind a NAT.
     */
    VQEC_NAT_BIND_STATE_BEHINDNAT,
    /**
     * Error state.
     */
    VQEC_NAT_BIND_STATE_ERROR,
    /**
     * Must be last.
     */
    VQEC_NAT_BIND_STATE_NUM_VAL,

} vqec_nat_bind_state_t;

#define VQEC_NAT_MIN_BIND_STATE VQEC_NAT_BIND_STATE_UNKNOWN,
#define VQEC_NAT_MAX_BIND_STATE (VQEC_NAT_BIND_STATE_NUM_VAL - 1)
#define VQEC_NAT_API_NAME_MAX_LEN 257


/**
 * NAT binding descriptor, which specifies the "request" for a binding.
 */  
typedef
struct vqec_nat_bind_desc_
{
    /**
     * Canonical name for the binding.
     */
    char name[VQEC_NAT_API_NAME_MAX_LEN];
    /**
     * Opaque caller identifier.
     */
    uint32_t caller_id;
    /**
     * allow update, used for vod ICE session.
     * If allow_update is true, the NAT query
     * will be allowed even the is_not_behind_nat
     * is true.
     */
    boolean allow_update;
    /**
     * Internal IPv4 addr.
     */
    in_addr_t internal_addr;
    /**
     * Internal IPv4 port (Network byte order).
     */
    in_port_t internal_port;
    /**
     * Remote IPv4 addr.
     */
    in_addr_t remote_addr;
    /**
     * Remote IPv4 port (Network byte order).
     */
    in_port_t remote_port;

} vqec_nat_bind_desc_t;


/**
 * NAT binding data, which forms the "request-response" metadata for a binding.
 */
typedef
struct vqec_nat_bind_data_
{    
    /**
     * Identifier of the NAT binding (opaque)
     */
    vqec_nat_bindid_t id;
    /**
     * Bind descriptor provided at open.
     */
    vqec_nat_bind_desc_t desc;    
    /**
     * Bind response: external mapped IP addr.
     */
    in_addr_t ext_addr;
    /**
     * Bind response: external mapped IP port.
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

} vqec_nat_bind_data_t;

/**
 * NAT initializion parameters.
 */
typedef 
struct vqec_nat_init_params_
{
    /**
     * Maximum bindings to be supported.
     */
    uint32_t max_bindings;
    /**
     * Refresh interval in secs.
     */
    uint32_t refresh_interval;
    /**
     * Maximum packet size.
     */
    uint32_t max_paksize;
    /**
     * Interface for NAT transactions
     */
    char *input_ifname;

} vqec_nat_init_params_t;


/**---------------------------------------------------------------------------
 * NAT protocol interface: implemented by the nat protocol.
 *---------------------------------------------------------------------------*/  
typedef 
struct vqec_nat_proto_if_
{
    /**
     * Instantiate the NAT protocol state.
     */
    boolean (*create)(vqec_nat_init_params_t *params);
    /**
     * Destroy NAT protocol state.
     */
    void (*destroy)(void);
    /**
     * Open a new NAT binding.
     */
    vqec_nat_bindid_t (*open)(vqec_nat_bind_desc_t *desc);
    /**
     * Close an existing NAT binding.
     */
    void (*close)(vqec_nat_bindid_t id);
    /**
     * Query a NAT binding to find it's most recent mapping.
     */
    boolean (*query)(vqec_nat_bindid_t id, 
                     vqec_nat_bind_data_t *data, boolean refresh);
    /**
     * Print state of a NAT binding.
     */
    void (*fprint)(vqec_nat_bindid_t id);
    /**
     * Print all protocol / binding status.
     */
    void (*fprint_all)(void);
    /**
     * Insert an ejected packet for the given binding.
     */
    void (*eject_rx)(vqec_nat_bindid_t id, char *buf, uint16_t len,
                     in_addr_t source_ip, in_port_t source_port);
    /**
     * Switch debugging on.
     */
    void (*debug_set)(boolean verbose);
    /**
     * Switch debugging off.
     */
    void (*debug_clr)(void);
    /**
     * Is the device behind a NAT?
     */
    boolean (*is_behind_nat)(void);

} vqec_nat_proto_if_t;


/**---------------------------------------------------------------------------
 * NAT client interface for use by the protocol: implemented by vqec.
 *---------------------------------------------------------------------------*/  

/**
 * Log an error through the client.
 */
void vqec_nat_log_error(const char *str, ...);

/**
 * Log a debug message through the client.
 */
void vqec_nat_log_debug(const char *str, ...);

/**
 * Console prints through the client.
 */
void vqec_nat_print(const char *str, ...);

/**
 * Inject a NAT packet into the data path.
 */   
boolean vqec_nat_inject_tx(vqec_nat_bindid_t id, 
                                vqec_nat_bind_desc_t *desc, char *buf, 
                                uint16_t len);

/**
 * Inform the control-plane of an update in a NAT binding.
 */   
void vqec_nat_bind_update(vqec_nat_bindid_t id, 
                          vqec_nat_bind_data_t *data);

/**
 * From NAT state to a character representation.
 */
const char *vqec_nat_state_to_str(vqec_nat_bind_state_t state);

/**---------------------------------------------------------------------------
 * Export timer interface (use of this interface is optional).
 *---------------------------------------------------------------------------*/  

typedef void (*vqec_nat_proto_evt_handler)(
    const struct vqec_event_ *const ptr, int32_t fd, int16_t ev, void *arg);

/**
 * Use the nat api to create a timer.
 */   
struct vqec_event_ *
vqec_nat_timer_create(boolean periodic, 
                      vqec_nat_proto_evt_handler handler, void *arg);

/**
 * Use the nat api to destroy a timer.
 */   
void vqec_nat_timer_destroy(struct vqec_event_ *timer);

/**
 * Use the nat api to start a timer.
 */   
boolean vqec_nat_timer_start(struct vqec_event_ *timer, 
                             uint32_t timeout_msec);

/**
 * Use the nat api to stop a timer.
 */   
boolean vqec_nat_timer_stop(struct vqec_event_ *timer);

/**---------------------------------------------------------------------------
 * Export locking interface (use of this interface is optional).
 *---------------------------------------------------------------------------*/  

/**
 * Use the nat api to acquire the global lock
 */
void vqec_nat_acquire_lock(void);

/**
 * Use the nat api to release the global lock
 */
void vqec_nat_release_lock(void);

/**---------------------------------------------------------------------------
 * Define all available NAT protocol prototypes here - done in
 * place of a formal protocol registration mechanism. Two protocols,
 * "null: nat" and "stun" are defined.
 *---------------------------------------------------------------------------*/  
#define NAT_GET_PROTO_INTF(p) vqec_nat_get_if_ ##p()
const vqec_nat_proto_if_t *vqec_nat_get_if_natproto(void);
const vqec_nat_proto_if_t *vqec_nat_get_if_stunproto(void);
const vqec_nat_proto_if_t *vqec_nat_get_if_upnpproto(void);
const vqec_nat_proto_if_t *vqec_nat_get_if_hybridproto(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VQEC_NAT_API_H__ */
