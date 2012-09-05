/*
 * Copyright (c) 2008-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

/**---------------------------------------------------------------------------
 vqec_upnp_mgr.c

 Uses 3rd-party libupnp library to provided NAT traversal functionality. The
 basis of the NAT traversal method is UPnP, a standard for ad hoc interaction
 of arbitrary devices over a network.

 More information on the API can be found in vqec_nat_api.h

 The UPnP manager is best understood through its main entry points:

 - create:      Initializes state, 3rd-party UPnP library, and initiates 
                search for a UPnP capable Internet Gateway Device (IGD).

 - destroy:     Deinitializes state and 3rd-party UPnP library.

 - open:        Creates and stores a new binding object, and sends an initial
                request to open a port mapping on the discovered IGD.

 - close:       Removes and destroys an existing binding, and sends a request
                to delete any matching port mapping from the IGD.

 - query:       Fetches state of an existing binding.

 - refresh:     Called periodically every so many seconds specified during
                initialization. Requests the external IP of the gateway and
                sends refresh port mapping requests to to the IGD for all
                existing mappings. If a previous attempt has failed, a new
                external port is chosen for that mapping and it is retried.

 - callback:    The upnp_event_handler deals with UPnP library asynchronous
                responses. The event types include the discovery of an IGD
                (which is then validated and stored if viable), a response
                to some previous actions (such as getting the external ip
                or adding a port mapping), or an IGD leaving the network.

 Most simply, the UPnP manager is responsible for discovering and maintaining
 information on the IGD with which it will interact and then for communicating
 with the chosen IGD (in the form of AddPortMapping, DeletePortMapping, and
 GetExternalIPAddress requests).

----------------------------------------------------------------------------*/

#include "vqec_nat_api.h"

#if HAVE_UPNP

#include <utils/vam_time.h>
#include <utils/vam_util.h>
#include <utils/zone_mgr.h>
#include <utils/queue_plus.h>
#include <utils/id_manager.h>
#include <string.h>
#include "upnp.h"
#include "vqec_hybrid_nat_mgr.h"
#include <semaphore.h>

#define UPNP_HASH_MAP_SIZE 8
#define UPNP_HASH_MASK 7
#define DISCOVERY_TIMEOUT 5
#define LEASE_MULTIPLIER 5
#define REFRESH_WAIT_CNT_MAX 2
#define SILENCE_CNT_MAX 4
#define UPNP_UPDATE_DELAY 50000
#define COOKIE_MULTIPLIER 3

/* Port Binding Cookie */
typedef struct vqec_nat_upnp_cookie_ {
    /* Related binding's id */
    vqec_nat_bindid_t id;
    /* Number of outstanding callbacks */
    uint32_t refcnt;
    /* Original binding destroyed flag */
    boolean destroyed;

    VQE_TAILQ_ENTRY(vqec_nat_upnp_cookie_) le;
} vqec_nat_upnp_cookie_t;

/* Port Binding Object */
typedef struct vqec_nat_upnp_bind_ {
    /* Unique ID */
    vqec_nat_bindid_t id;
    /* ext_addr & port are valid */
    boolean is_map_valid;
    /* awaiting last request ack */
    boolean awaiting_ack;
    /* Refreshes since awaiting ack set */
    uint32_t refresh_wait_cnt;
    /* Mapping's WAN address */
    in_addr_t ext_addr;
    /* Mapping's WAN port */
    in_port_t ext_port;
    /* Descriptor provided by channel bind */
    vqec_nat_bind_desc_t desc;
    /* Behind NAT Status */
    vqec_nat_bind_state_t state;
    /* Bind update timer event */
    struct vqec_event_ *ev_update;
    /* Update pending */
    boolean update_pending;
    /* Binding cookie for callbacks */
    vqec_nat_upnp_cookie_t *cookie;

    VQE_TAILQ_ENTRY(vqec_nat_upnp_bind_) le;
} vqec_nat_upnp_bind_t;

/* UPnP IGD Object */
typedef struct vqec_nat_upnp_igd_ {
    /* IGD available and valid */
    boolean ready;
    /* Refreshes since last hear from IGD */
    uint32_t silence_cnt;
    /* Unique UPnP device id */
    char device_id[LINE_SIZE];
    /* UPnP service type (WANIPConnection) */
    char service_type[NAME_SIZE];
    /* First part of URL for UPnP comm */
    char base_url[NAME_SIZE];
    /* URL for sending commands */
    char control_url[NAME_SIZE];
    /* IGD WAN address */
    in_addr_t ext_addr;
} vqec_nat_upnp_igd_t;

typedef VQE_TAILQ_HEAD(nat_list_, vqec_nat_upnp_bind_) nat_list_t;
typedef VQE_TAILQ_HEAD(nat_cookie_list_, vqec_nat_upnp_cookie_) 
            nat_cookie_list_t;

typedef struct vqe_zone vqec_nat_upnp_pool_t;

/* AddPortMapping request document */
typedef struct add_port_xml_ {
    IXML_Document *doc;
    IXML_Element *remote_host;
    IXML_Element *external_port;
    IXML_Element *protocol;
    IXML_Element *internal_port;
    IXML_Element *internal_client;
    IXML_Element *enabled;
    IXML_Element *description;
    IXML_Element *lease_duration;
} add_port_xml_t;
    
/* DeletePortMapping request document */
typedef struct del_port_xml_ {
    IXML_Document *doc;
    IXML_Element *remote_host;
    IXML_Element *external_port;
    IXML_Element *protocol;
} del_port_xml_t;

/* GetExternalIPAddress request document */
typedef struct get_ext_ip_xml_ {
    IXML_Document *doc;
} get_ext_ip_xml_t;

/* Main static data structure */
typedef struct vqec_nat_upnp_ {
    /* Initialization complete */
    boolean init_done;
    /* Print debug statements */
    boolean debug_en;
    boolean verbose;

    /* Maximum allowed simultaneous bindings */
    uint32_t max_bindings;
    /* Period for refreshing bindings */
    uint32_t refresh_interval;
    /* Number of existing bindings */
    uint32_t open_bindings;

    /* Binding Refresh event handle */
    struct vqec_event_ *ev_refresh;

    /* Id table for mapping bindids to bindings */
    id_table_key_t bindid_table;
    /* Memory pool for creating bindings */
    vqec_nat_upnp_pool_t *bind_pool;
    /* Port bindings hash table */
    nat_list_t lh[UPNP_HASH_MAP_SIZE];

    /* Memory pool for creating cookies */
    vqec_nat_upnp_pool_t *cookie_pool;
    /* Number of allocated cookies */
    uint32_t cookie_cnt;
    /* Orphan cookies list */
    nat_cookie_list_t orphan_cookies;

    /* default UPnP IGD */
    vqec_nat_upnp_igd_t igd;
    /* LAN addr of default IGD */
    in_addr_t default_igd_ip;
    /* UPnP control point handle */
    UpnpClient_Handle ctrlpt_hnd;

    /* Condition for IGD ready  */
    sem_t igd_ready_cond;

    /* UPnP XML Request documents */
    add_port_xml_t add_port_xml;
    del_port_xml_t del_port_xml;
    get_ext_ip_xml_t get_ext_ip_xml;

} vqec_nat_upnp_t;
static vqec_nat_upnp_t s_upnp;


/* Function Prototypes */
static int32_t upnp_event_handler(Upnp_EventType event_type,
                                  void *event,
                                  void *cookie);
static void vqec_nat_upnp_close(vqec_nat_bindid_t id);

/**
 * vqec_nat_upnp_bind_update
 *   inform the client layer of an update to a binding.
 *
 * @param[in] p_bind Pointer to the binding
 */
static void
vqec_nat_upnp_bind_update (vqec_nat_upnp_bind_t *p_bind)
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
     * Inform combined manager of bind update
     * so it may choose choose between UPNP and STUN. 
     */
    vqec_nat_hybrid_bind_update(VQEC_NAT_PROTO_UPNP, 
                                data.id, &data);
#else
    vqec_nat_bind_update(data.id, &data);
#endif

    if (s_upnp.debug_en) {
        vqec_nat_log_debug("Proto: UPnP, updated bind id %u, local port %u\n",
                           p_bind->id,
                           ntohs(p_bind->desc.internal_port));
    }

}

static void update_timer_callback (const struct vqec_event_ *const p_ev,
                                   int32_t fd, int16_t ev, void *arg) 
{
    vqec_nat_upnp_bind_t *p_bind = arg;
    vqec_nat_upnp_bind_update(p_bind);    
    p_bind->update_pending = FALSE;
}

static void vqec_nat_upnp_schedule_bind_update (vqec_nat_upnp_bind_t *p_bind)
{
    if (!p_bind->update_pending) {
        (void)vqec_nat_timer_start(p_bind->ev_update, UPNP_UPDATE_DELAY);
        p_bind->update_pending = TRUE;
    }
}

/**
 * get_default_route
 * 
 * Parses the /proc/net/route file to determine the default gateway.
 *
 * @param[out] in_addr_t the default gateway
 */
static in_addr_t get_default_route (void)
{
    FILE *route_fp;
    uint32_t dest, gateway, flag, mask;
    gateway = 0;

    /* Parse the route file */
    route_fp = fopen("/proc/net/route", "r");
    if (!route_fp) {
        return (in_addr_t)0;
    }
    fscanf(route_fp, "%*[^\n]\n");

    /* Look for 0.0.0.0 dest and mask, gateway flag set to true */
    while(fscanf(route_fp, "%*s\t%x\t%x\t%x\t%*s\t%*s\t%*s\t%x%*[^\n]\n", 
                 &dest, &gateway, &flag, &mask) != EOF) {
        if (dest == 0 && (flag & 0x2) != 0 && mask == 0) {
            break;
        }
    }

    fclose(route_fp);

    return (in_addr_t)gateway;    
}

/** 
 * vqec_nat_proto_hash
 *   hash function based on port number
 */
static inline uint16_t
vqec_nat_proto_hash (uint16_t port)
{
    return (port & UPNP_HASH_MASK);
}

/**
 * vqec_nat_upnp_is_same_binding
 *   compares bind_descs for equality
 */
static inline boolean
vqec_nat_upnp_is_same_binding (vqec_nat_bind_desc_t *desc1,
                                    vqec_nat_bind_desc_t *desc2)
{
    return
        ((desc1->internal_port == desc2->internal_port) &&
         (desc1->internal_addr == desc2->internal_addr) &&
         (desc1->remote_addr == desc2->remote_addr) &&
         (desc1->remote_port == desc2->remote_port));
}

/**
 * vqec_nat_upnp_find_by_desc
 *   finds an existing binding matching the desc
 *
 * @param[in] desc bind descriptor to match
 * @param[out] vqec_nat_bindid_t matching bindid, invalid if none
 */
static vqec_nat_bindid_t 
vqec_nat_upnp_find_by_desc (vqec_nat_bind_desc_t * desc) 
{
    uint16_t hash;
    vqec_nat_upnp_bind_t *p_bind;

    if (!desc) {
        return FALSE;
    }

    hash = vqec_nat_proto_hash((uint16_t)desc->internal_port);
    VQE_TAILQ_FOREACH(p_bind, &s_upnp.lh[hash], le) {
        if (vqec_nat_upnp_is_same_binding(&p_bind->desc, desc)) {
            return p_bind->id;
        }
    }

    return VQEC_NAT_BINDID_INVALID;
}

/**
 * append_xml_element
 *   adds a new element with the given tag name to the parent element
 *
 * @param[in] doc parent xml document
 * @param[in] parent element to which the new element is added
 * @param[in] name new element's tag name
 * @param[out] IXML_Element* pointer to the new element
 */
static IXML_Element *append_xml_element (IXML_Document *doc, 
                                         IXML_Element *parent, 
                                         const char *name)
{
    IXML_Element *elem;

    elem = ixmlDocument_createElement(doc, name);
    /* No NULL check required because appendChild ignores */
    if (parent) {
        (void)ixmlNode_appendChild((IXML_Node *)parent, (IXML_Node *)elem);
    } else {
        (void)ixmlNode_appendChild((IXML_Node *)doc, (IXML_Node *)elem);
    }

    return elem;
}

/**
 * append_xml_text
 *   adds a new text node to the parent element
 *
 * @param[in] doc the parent xml document handle
 * @param[in] parent element to which the new text node will be added
 * @param[in] text value for the new text node
 * @param[out] IXML_Node* pointer to the new text node
 */
static IXML_Node *append_xml_text (IXML_Document *doc,
                                   IXML_Element *parent,
                                   const char *text)
{
    IXML_Node *node;
 
    node = ixmlDocument_createTextNode(doc, text);
    /* No NULL check required because appendChild ignores */
    (void)ixmlNode_appendChild((IXML_Node *)parent, (IXML_Node *)node);

    return node;
}

/**
 * remove_xml_child
 *   removes the first child of the parent node
 *
 * @param[in] parent parent of child to be destroyed and removed
 */
static inline void remove_xml_child (IXML_Node *parent) 
{
    IXML_Node *child;
    IXML_Node *ret_node;

    child = ixmlNode_getFirstChild(parent);
    if (child) {
        (void)ixmlNode_removeChild(parent, child, &ret_node);
        ixmlNode_free(child);
    }
}

/**
 * vqec_nat_upnp_add_port_mapping
 *   forms and sends an AddPortMapping request
 *
 * @param[in] p_bind port mapping object
 * @param[out] boolean true if successful, false if not
 */
static boolean vqec_nat_upnp_add_port_mapping (vqec_nat_upnp_bind_t *p_bind)
{
    IXML_Node *text_node;
    char buffer[INET_ADDRSTRLEN];
    char action_url[2*NAME_SIZE + 1];
    int32_t ret_upnp;
    boolean ret = FALSE;
    vqec_nat_bind_desc_t *desc = &p_bind->desc;

    /* Create and append relevant text nodes */
    /* Internal port number */
    (void)snprintf(buffer, INET_ADDRSTRLEN, "%u", ntohs(desc->internal_port));
    text_node = append_xml_text(s_upnp.add_port_xml.doc,
                                s_upnp.add_port_xml.internal_port, buffer);
    if (!text_node) {
        vqec_nat_log_error("Proto: UPnP xml allocation failure %s",
                           __FUNCTION__);
        goto done;
    }

    /* Internal client IP address */
    (void)uint32_ntoa_r(desc->internal_addr, buffer, sizeof(buffer));
    text_node = append_xml_text(s_upnp.add_port_xml.doc,
                                s_upnp.add_port_xml.internal_client, buffer);
    if (!text_node) {
        vqec_nat_log_error("Proto: UPnP xml allocation failure %s",
                           __FUNCTION__);
        goto done;
    }
    
    /* External port number */
    (void)snprintf(buffer, INET_ADDRSTRLEN, "%u", ntohs(p_bind->ext_port));
    text_node = append_xml_text(s_upnp.add_port_xml.doc,
                                s_upnp.add_port_xml.external_port, buffer); 
    if (!text_node) {
        vqec_nat_log_error("Proto: UPnP xml allocation failure %s",
                           __FUNCTION__);
        goto done;
    }

    /* Form action URL and send out request */
    (void)snprintf(action_url, sizeof(action_url),
                   "%s%s", s_upnp.igd.base_url, s_upnp.igd.control_url);
    ret_upnp = UpnpSendActionAsync(s_upnp.ctrlpt_hnd, action_url, 
                                   s_upnp.igd.service_type, 
                                   NULL, s_upnp.add_port_xml.doc, 
                                   upnp_event_handler, 
                                   (void *)p_bind->cookie);

    if (ret_upnp != UPNP_E_SUCCESS) {
        if (s_upnp.debug_en) {
            vqec_nat_log_debug("Proto: UPnP AddPortMapping send error #%i\n",
                               ret_upnp);
        }
        goto done;
    }
    
    /* Increment outstanding references counter */
    p_bind->cookie->refcnt++;

    /* Set waiting flag until callback rec'd */
    p_bind->awaiting_ack = TRUE;
    p_bind->refresh_wait_cnt = 0;

    if (s_upnp.debug_en) {
        vqec_nat_log_debug("Proto: UPnP AddPortMapping request for %s:%u\n",
                           uint32_ntoa_r(desc->internal_addr,
                                         buffer, sizeof(buffer)),
                           ntohs(desc->internal_port));
    }

    /* AddPortMapping request send successful */
    ret = TRUE;

  done:
    /* Remove and destroy temporary text nodes */
    remove_xml_child((IXML_Node *)s_upnp.add_port_xml.internal_port);
    remove_xml_child((IXML_Node *)s_upnp.add_port_xml.internal_client);
    remove_xml_child((IXML_Node *)s_upnp.add_port_xml.external_port);

    return ret;
}

/**
 * vqec_nat_upnp_delete_port_mapping
 *   form and send a DeletePortMapping request
 *
 * @param[in] desc port mapping descriptor
 * @param[out] boolean true if successful, false if not
 */
static boolean vqec_nat_upnp_delete_port_mapping (vqec_nat_upnp_bind_t *p_bind)
{
    IXML_Node *text_node;
    char buffer[INET_ADDRSTRLEN];
    char action_url[2*NAME_SIZE + 1];
    int32_t ret_upnp;
    boolean ret = FALSE;

    /* Create and append relevant text nodes */
    /* External port number */
    (void)snprintf(buffer, INET_ADDRSTRLEN, "%u", ntohs(p_bind->ext_port));
    text_node = append_xml_text(s_upnp.del_port_xml.doc,
                                s_upnp.del_port_xml.external_port, buffer);
    if (!text_node) {
        vqec_nat_log_error("UPnP xml allocation failure %s",
                           __FUNCTION__);
        goto done;
    }

    /* Form action URL and send delete request */
    (void)snprintf(action_url, sizeof(action_url), 
                   "%s%s", s_upnp.igd.base_url, s_upnp.igd.control_url);    
    ret_upnp = UpnpSendActionAsync(s_upnp.ctrlpt_hnd, action_url, 
                                   s_upnp.igd.service_type, 
                                   NULL, s_upnp.del_port_xml.doc, 
                                   upnp_event_handler, 
                                   NULL);

    if (ret_upnp != UPNP_E_SUCCESS) {
        if (s_upnp.debug_en) {
            vqec_nat_log_debug("Proto: UPnP DelPortMapping send error #%i\n",
                               ret_upnp);
        }
        goto done;
    }

    if (s_upnp.debug_en) {
        vqec_nat_log_debug("Proto: UPnP DeletePortMapping request for %s:%u\n",
                           uint32_ntoa_r(p_bind->desc.internal_addr,
                                         buffer, sizeof(buffer)),
                           ntohs(p_bind->desc.internal_port));
    }

    /* DeletePortMapping request send successful */
    ret = TRUE;

  done:
    /* Remove and destroy temporary text nodes */
    remove_xml_child((IXML_Node *)s_upnp.del_port_xml.external_port);

    return ret;
}

/**
 * vqec_nat_upnp_get_external_ip
 *   forms and sends GetExternalIPAddress request
 *
 * @param[out] boolean true if successful, false if not
 */
static boolean vqec_nat_upnp_get_external_ip (void)
{
    char action_url[2*NAME_SIZE + 1];
    int32_t ret_upnp;
    boolean ret = TRUE;

    /* form action URL and send get external ip request */
    (void)snprintf(action_url, sizeof(action_url),
                   "%s%s", s_upnp.igd.base_url, s_upnp.igd.control_url);
    ret_upnp = UpnpSendActionAsync(s_upnp.ctrlpt_hnd, action_url, 
                                   s_upnp.igd.service_type, 
                                   NULL, s_upnp.get_ext_ip_xml.doc, 
                                   upnp_event_handler, 
                                   &s_upnp.ctrlpt_hnd);

    if (ret_upnp != UPNP_E_SUCCESS) {
        if (s_upnp.debug_en) {
            vqec_nat_log_debug("Proto: UPnP GetExternalIP send error #%i\n",
                               ret_upnp);
        }
        ret = FALSE;
    }

    if (s_upnp.debug_en) {
        vqec_nat_log_debug("Proto: UPnP GetExternalIPAddress request\n");
    }

    return ret;
}

/**
 * get_first_element
 *   finds and returns the first element matching the item tag string
 *
 * @param[in] element starting point for searching (pre-order traversal)
 * @param[in] item string to compare against tag names
 * @param[out] IXML_Node* the matching element, NULL if none found
 */
static inline 
IXML_Node *get_first_element (IXML_Element *element, const char *item)
{
    IXML_NodeList *node_list = NULL;
    IXML_Node *ret_node = NULL;

    node_list = ixmlElement_getElementsByTagName(element, (char *)item);
    if (!node_list) {
        return NULL;
    }
    if (ixmlNodeList_length(node_list) == 0) {
        ixmlNodeList_free(node_list);
        return NULL;
    }
    ret_node = ixmlNodeList_item(node_list, 0);
    ixmlNodeList_free(node_list);

    return ret_node;
}

/**
 * get_first_element_item
 *   finds and returns the first element matching the item tag string
 *
 * @param[in] element starting point for searching (pre-order traversal)
 * @param[in] item string to compare against tag names
 * @param[out] char* the value of the matching element, NULL if none found
 */
static inline 
char *get_first_element_item (IXML_Element *element, const char *item)
{
    IXML_Node *text_node = NULL;
    IXML_Node *tmp_node = NULL;
    char *ret = NULL;

    tmp_node = get_first_element(element, item);
    if (!tmp_node) {
        return NULL;
    }

    text_node = ixmlNode_getFirstChild(tmp_node);
    if (!text_node) {
        return NULL;
    }

    ret = (char *)ixmlNode_getNodeValue( text_node );

    return ret;
}

/** 
 * vqec_nat_upnp_acquire_lock
 *   Acquires global lock. Must only be invoked by
 *   the UPnP callback thread. All other threads
 *   already hold global lock. Note: the global
 *   lock is only acquired when init_done is TRUE.
 *
 * @param[out] lock_acquired TRUE if global lock actually acquired
 */
static inline
void vqec_nat_upnp_acquire_lock(boolean *lock_acquired)
{
    *lock_acquired = FALSE;
    if (s_upnp.init_done) {
        vqec_nat_acquire_lock();
        *lock_acquired = TRUE;
    }

}
/**
 *  vqec_nat_upnp_release_lock
 *    Release global lock if lock_acquire is TRUE.
 *
 * @param[in] lock_acquired TRUE if global lock is held
 */
static inline
void vqec_nat_upnp_release_lock(boolean lock_acquired)
{
    if (lock_acquired) {
        vqec_nat_release_lock();
    }
}

/**
 * upnp_handle_igd_found
 *   Handles event types:
 *   - UPNP_DISCOVERY_ADVERTISEMENT_ALIVE
 *   - UPNP_DISCOVERY_SEARCH_RESULT
 *   Called whenever a new IGD is discovered.
 * 
 * @param[in] disc_event the new event to be processed
 */
static void upnp_handle_igd_found (struct Upnp_Discovery *disc_event)
{
    IXML_Document *device_desc = NULL;
    IXML_NodeList *node_list = NULL;
    IXML_Node *node = NULL;
    char *service_type = NULL;
    char *control_url = NULL;
    char base_url[NAME_SIZE];
    int32_t i = 0;
    boolean lock_acquired;

    if (disc_event->ErrCode != UPNP_E_SUCCESS) {
        return;
    }
    
    /* Check that Discovery event is for a WANIPConnection service */
    if (!strstr(disc_event->ServiceType, "WANIPConnection")) {
        return;
    }

    /* Check that this discovery event matches default gateway IP addr */
    if (disc_event->DestAddr.sin_addr.s_addr 
                        != s_upnp.default_igd_ip) {
        vqec_nat_log_debug("Proto: UPnP non-default IGD found");
        return;
    }

    /* Download XML UPnP service descriptor */
    UpnpDownloadXmlDoc(disc_event->Location, &device_desc);
    if (!device_desc) {
        vqec_nat_log_error("Device description download failure %s",
                           __FUNCTION__);
        return;
    }
    
    /* Extract base URL */
    if (sscanf(disc_event->Location, "http://%[^/]/%*s", base_url) < 1) {
        vqec_nat_log_error("Failed to determine IGD base URL %s",
                           __FUNCTION__);
        goto done;
    }

    /* Extract WANIPConnection service details */
    node_list = ixmlDocument_getElementsByTagName(device_desc, 
                                                  "service");
    if (!node_list) {
        vqec_nat_log_error("UPnP xml allocation failure %s",
                           __FUNCTION__);
        goto done;
    }
    for (i = 0; i < ixmlNodeList_length(node_list); i++) {
        node = ixmlNodeList_item(node_list, i);
        service_type = get_first_element_item((IXML_Element *)node, 
                                              "serviceType");
        if (service_type && strstr(service_type, "WANIPConnection")) {
            break;
        }
    }
    ixmlNodeList_free(node_list);
    if (!service_type) {
        goto done;
    }

    /* Copy UPnP URLs to IGD structure */
    control_url = get_first_element_item((IXML_Element *)node, 
                                          "controlURL");
    if (!control_url) {
        vqec_nat_log_error("Failed to determine IGD control url %s",
                           __FUNCTION__);
        goto done;
    }

    /* Acquire global lock before accessing s_upnp */
    vqec_nat_upnp_acquire_lock(&lock_acquired);

    /* Copy discovered info into IGD structure */
    (void)snprintf(s_upnp.igd.base_url, NAME_SIZE, "http://%s", base_url);
    strncpy(s_upnp.igd.service_type, service_type, NAME_SIZE);
    strncpy(s_upnp.igd.control_url, control_url, NAME_SIZE);
    strncpy(s_upnp.igd.device_id, disc_event->DeviceId, LINE_SIZE);        

    /* Change and notify ready flag */
    s_upnp.igd.ready = TRUE;
    s_upnp.igd.silence_cnt = 0;
    (void)sem_post(&s_upnp.igd_ready_cond);

    if (s_upnp.debug_en) {
        vqec_nat_log_debug("Proto: UPnP IGD found %s\n",
                           s_upnp.igd.base_url);
    }

    vqec_nat_upnp_release_lock(lock_acquired);

  done:
    /* Free the downloaded descriptor */
    ixmlDocument_free(device_desc);
}

/**
 * upnp_handle_discovery_lost
 *   Handles event types:
 *   - UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE
 *
 * @param[in] disc_event the new event to be processed
 */
static void upnp_handle_igd_lost (struct Upnp_Discovery *disc_event)
{
    boolean lock_acquired;

    if (disc_event->ErrCode != UPNP_E_SUCCESS) {
        return;
    }

    vqec_nat_upnp_acquire_lock(&lock_acquired);

    /* Clear ready flag if IGD is lost */    
    if (s_upnp.igd.ready
        && strncmp(disc_event->DeviceId, 
                   s_upnp.igd.device_id, 
                   LINE_SIZE) == 0) {
        s_upnp.igd.ready = FALSE;
    }

    vqec_nat_upnp_release_lock(lock_acquired);
}

/**
 * upnp_handle_action_complete
 *   Handle event type:
 *   - UPNP_CONTROL_ACTION_COMPLETE
 *
 * @param[in] action_event the new event to processed
 * @param[in] action_arg argument passed in when async func called
 */
static void 
upnp_handle_action_complete (struct Upnp_Action_Complete *action_event,
                             void *action_arg)
{
    IXML_Element *action_elem; 
    boolean lock_acquired;

    /* Check for UPnP library error code (negative values) */
    if (action_event->ErrCode < UPNP_E_SUCCESS) {
        if (s_upnp.debug_en) {
            vqec_nat_log_debug("Proto: UPnP action completion error #%i\n",
                               action_event->ErrCode);
        }
        return;
    }

    vqec_nat_upnp_acquire_lock(&lock_acquired);

    /* Handle AddPortMapping response */
    if (action_arg != &s_upnp.ctrlpt_hnd && s_upnp.init_done) {
        vqec_nat_upnp_bind_t *p_bind;
        vqec_nat_upnp_cookie_t *cookie = (vqec_nat_upnp_cookie_t *)action_arg;
        id_mgr_ret ret;

        /* Convert the cookie to a binding */
        cookie->refcnt--;
        if (cookie->destroyed) {
            if (cookie->refcnt == 0) {
                VQE_TAILQ_REMOVE(&s_upnp.orphan_cookies, cookie, le);
                zone_release(s_upnp.cookie_pool, cookie);
                s_upnp.cookie_cnt--;
            }
            goto done;
        }
        p_bind = (vqec_nat_upnp_bind_t *)id_to_ptr(cookie->id, &ret,
                                                   s_upnp.bindid_table);
        if (!p_bind || (ret != ID_MGR_RET_OK)) {
            vqec_nat_log_error("Id lookup failure  %s",
                               __FUNCTION__);
            goto done;
        }

        /* Ack rec'd: reset flag */
        p_bind->awaiting_ack = FALSE;
        p_bind->refresh_wait_cnt = 0;

        /* Check for UPnP IGD error code (positive values) */
        if (action_event->ErrCode > UPNP_E_SUCCESS) {
            p_bind->is_map_valid = FALSE;
            p_bind->state = VQEC_NAT_BIND_STATE_ERROR;
            if (s_upnp.debug_en) {
                vqec_nat_log_debug("Proto: UPnP add port mapping error #%i\n",
                               action_event->ErrCode);
            }

        /* Successful binding established */
        } else {
            if (!p_bind->is_map_valid) {
                p_bind->is_map_valid = TRUE;
                p_bind->state = VQEC_NAT_BIND_STATE_BEHINDNAT;
            }
            if (p_bind->ext_addr && lock_acquired) {
                vqec_nat_upnp_schedule_bind_update(p_bind);
            }
            if (s_upnp.debug_en) {
                vqec_nat_log_debug("Proto: UPnP bind id %lu success\n",
                                   p_bind->id);
            }
        }

    /* Handle GetExternalIPAddress response */
    } else if (action_arg == &s_upnp.ctrlpt_hnd) {
        char *ext_ip_str;
        in_addr_t ext_addr;
        int32_t i = 0;
        vqec_nat_upnp_bind_t *p_bind;

        /* Silence count reset */
        s_upnp.igd.silence_cnt = 0;

        if (action_event->ErrCode > UPNP_E_SUCCESS) {
            if (s_upnp.debug_en) {
                vqec_nat_log_debug("Proto: UPnP get external ip error #%i\n",
                               action_event->ErrCode);
            }
            goto done;
        }

        /* Extract external IP value */
        action_elem = (IXML_Element *)ixmlNode_getFirstChild(
                               (IXML_Node *)action_event->ActionResult);
        if (!action_elem) {
            goto done;
        }
        ext_ip_str = get_first_element_item(action_elem,
                                                  "NewExternalIPAddress");
        if (!ext_ip_str) {
            goto done;
        }
        if (!inet_pton(AF_INET, ext_ip_str, (void *)&ext_addr)) {
            goto done;
        }

        /* Update all bindings with ext addr */
        if (s_upnp.igd.ext_addr != ext_addr && s_upnp.init_done) {
            for (i = 0; i < UPNP_HASH_MAP_SIZE; i++) {
                VQE_TAILQ_FOREACH(p_bind, &s_upnp.lh[i], le) {
                    p_bind->ext_addr = ext_addr;
                    if (p_bind->is_map_valid && lock_acquired) {
                        vqec_nat_upnp_schedule_bind_update(p_bind);
                    }
                }
            }
        }

        /* Update IGD structure */
        s_upnp.igd.ext_addr = ext_addr;

        if (s_upnp.debug_en) {
            vqec_nat_log_debug("Proto: UPnP external IP update %s\n",
                               ext_ip_str);
        }

    }
  done:
    vqec_nat_upnp_release_lock(lock_acquired);
}

/**
 * upnp_event_handler
 *   handles all asynchronous libupnp event
 *   called from the thread created by UpnpInit
 *
 * @param[in] event_type enumeration specifying event type
 * @param[in] event pointer to event structure (depends on event type)
 * @param[in] cookie 
 */
static int32_t upnp_event_handler (Upnp_EventType event_type,
                                   void *event,
                                   void *cookie)
{

    /* Switch on event type and call appropriate handler function */
    switch (event_type) {
        case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
        case UPNP_DISCOVERY_SEARCH_RESULT: 
            upnp_handle_igd_found((struct Upnp_Discovery *)event);
            break;
        case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE: 
            upnp_handle_igd_lost((struct Upnp_Discovery *)event);
            break;
        case UPNP_DISCOVERY_SEARCH_TIMEOUT: 
            (void)sem_post(&s_upnp.igd_ready_cond);
            break;
        case UPNP_CONTROL_ACTION_COMPLETE: 
            if (!cookie) {
                break;
            }
            upnp_handle_action_complete((struct Upnp_Action_Complete *)event,
                                        cookie);
            break;
       default: 
            break;
    }

    return 0;
}

/**
 * upnp_refresh_handler
 *   callback for refresh_interval timer
 *   responsible for renewing all existing port mappings
 *   responsible for updating external ip address
 */
static void upnp_refresh_handler (const struct vqec_event_ *const p_ev,
                                  int32_t fd, int16_t ev, void *arg) 
{
    uint32_t i;
    vqec_nat_upnp_bind_t *p_bind;

    if (!s_upnp.init_done || !s_upnp.igd.ready) {
        return;
    }

    /* Update external IP address (asynchronous) */
    (void) vqec_nat_upnp_get_external_ip();

    /* Renew all existing port mapings */
    for (i = 0; i < UPNP_HASH_MAP_SIZE; i++) {
        VQE_TAILQ_FOREACH(p_bind, &s_upnp.lh[i], le) {

            /* Ready to send new refresh */
            if (!p_bind->awaiting_ack) {
                if (!p_bind->is_map_valid) {
                    /* 
                     * Choose a random external port number 
                     * between 16383 and 65535 for this mapping 
                     */
                    p_bind->ext_port = (in_port_t)(0x3FFF + 
                        (0xFFFF-0x3FFF)*(rand()/(float)RAND_MAX));
                }
                (void) vqec_nat_upnp_add_port_mapping(p_bind);

            /* Waiting on a previous refresh */
            } else {
                if (p_bind->refresh_wait_cnt > REFRESH_WAIT_CNT_MAX) {
                    p_bind->refresh_wait_cnt = 0;
                    p_bind->awaiting_ack = FALSE;
                } else {
                    p_bind->refresh_wait_cnt++;
                }
            }              
        }
    }

    /* Check and update silence count */
    if (s_upnp.igd.silence_cnt > SILENCE_CNT_MAX) {
        s_upnp.igd.ready = FALSE;
    }
    if ((s_upnp.igd.silence_cnt - 1 == SILENCE_CNT_MAX) && s_upnp.debug_en) {
        vqec_nat_log_debug("UPnP IGD connectivity lost\n");
    }
    s_upnp.igd.silence_cnt++;
}

/**
 * Destroy NAT protocol state.
 */
void vqec_nat_upnp_destroy (void) 
{
    vqec_nat_upnp_bind_t *p_bind, *p_bind_nxt;
    int32_t i;
    vqec_nat_upnp_cookie_t *cookie, *cookie_nxt;
    
    /* Close existing bindings */
    for (i = 0; i < UPNP_HASH_MAP_SIZE; i++) {
        VQE_TAILQ_FOREACH_SAFE(p_bind, &s_upnp.lh[i], le, p_bind_nxt) {
            if (p_bind->cookie) {
               zone_release(s_upnp.cookie_pool, p_bind->cookie);
               s_upnp.cookie_cnt--;
               p_bind->cookie = NULL;
            }
            vqec_nat_upnp_close(p_bind->id);
        }
    }

    /* Destroy orphan cookies */
    VQE_TAILQ_FOREACH_SAFE(cookie, &s_upnp.orphan_cookies, le, cookie_nxt) {
        VQE_TAILQ_REMOVE(&s_upnp.orphan_cookies, cookie, le);
        zone_release(s_upnp.cookie_pool, cookie);
        s_upnp.cookie_cnt--;
    }

    /* Stop and destroy refresh timer */
    if (s_upnp.ev_refresh) {
        (void)vqec_nat_timer_stop(s_upnp.ev_refresh);
        vqec_nat_timer_destroy(s_upnp.ev_refresh);
        s_upnp.ev_refresh = NULL;
    }
    /* Unregister UPnP control point */
    if (s_upnp.ctrlpt_hnd) {
        (void)UpnpUnRegisterClient(s_upnp.ctrlpt_hnd);
        s_upnp.ctrlpt_hnd = (UpnpClient_Handle)NULL;
    }
    /* Deinit UPnP library */
    /* (void)UpnpFinish(); */ 
    /* Causes Deadlock, Leave out */

    /* Free common xml documents */
    if (s_upnp.add_port_xml.doc) {
        ixmlDocument_free(s_upnp.add_port_xml.doc);
        memset(&s_upnp.add_port_xml, 0, sizeof(add_port_xml_t));
    }
    if (s_upnp.del_port_xml.doc) {
        ixmlDocument_free(s_upnp.del_port_xml.doc);
        memset(&s_upnp.del_port_xml, 0, sizeof(del_port_xml_t));
    }

    /* Destroy bind id table and bind pool */
    if (s_upnp.bindid_table != ID_MGR_TABLE_KEY_ILLEGAL) {
        id_destroy_table(s_upnp.bindid_table);
        s_upnp.bindid_table = ID_MGR_TABLE_KEY_ILLEGAL;
    }
    if (s_upnp.bind_pool) {
        zone_instance_put(s_upnp.bind_pool);
        s_upnp.bind_pool = NULL;
    }
    if (s_upnp.cookie_pool) {
        zone_instance_put(s_upnp.cookie_pool);
        s_upnp.cookie_pool = NULL;
    }

    (void)sem_destroy(&s_upnp.igd_ready_cond);
    s_upnp.init_done = FALSE;
}


/**
 * Instantiate the NAT protocol state.
 */
boolean vqec_nat_upnp_create (vqec_nat_init_params_t *params) 
{
    int32_t upnp_err;
    boolean ret = TRUE;  
    IXML_Element *action_elem; 
    char host_ip[INET_ADDRSTRLEN];
    int32_t i;
 
    if (s_upnp.init_done) {
        vqec_nat_log_error("Already initialized %s", __FUNCTION__);
        return TRUE;
    }

    if (!params) {
        vqec_nat_log_error("Invalid input parameters %s", __FUNCTION__);
        return FALSE;
    }

    /* Initialize s_upnp structure with params */
    memset(&s_upnp, 0, sizeof(vqec_nat_upnp_t));
    s_upnp.max_bindings = params->max_bindings;
    s_upnp.refresh_interval = params->refresh_interval;
    if (sem_init(&s_upnp.igd_ready_cond, 0, 0)) {
        vqec_nat_log_error("Failed to initialize upnp thread condition %s",
                           __FUNCTION__);
        return FALSE;
    }

    /* Allocate port binding structure pool */
    s_upnp.bind_pool = zone_instance_get_loc("upnp bind pool",
                                             O_CREAT,
                                             sizeof(vqec_nat_upnp_bind_t),
                                             s_upnp.max_bindings,
                                             NULL, NULL);
    if (!s_upnp.bind_pool) {
        vqec_nat_log_error("Failed to allocate bind pool %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }

    /* Setup callback cookies pool, count, and orphan list */
    s_upnp.cookie_pool = zone_instance_get_loc("upnpcookiepool",
                                               O_CREAT,
                                               sizeof(vqec_nat_upnp_cookie_t),
                                               s_upnp.max_bindings * 
                                                   COOKIE_MULTIPLIER,
                                               NULL, NULL);
    if (!s_upnp.cookie_pool) {
        vqec_nat_log_error("Failed to allocate cookie pool %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    s_upnp.cookie_cnt = 0;
    VQE_TAILQ_INIT(&s_upnp.orphan_cookies);

    /* Create an id table for port bindings */
    s_upnp.bindid_table = id_create_new_table(1, s_upnp.max_bindings);
    if (s_upnp.bindid_table == ID_MGR_TABLE_KEY_ILLEGAL) {
        vqec_nat_log_error("Failed to allocate id table %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }

    /* Initialize bindings hash table */
    for (i = 0; i < UPNP_HASH_MAP_SIZE; i++) {
        VQE_TAILQ_INIT(&s_upnp.lh[i]);
    }

    /* Initialize UPnP library and register UPnP client*/
    if (!get_ip_address_by_if(params->input_ifname, host_ip, 
                              INET_ADDRSTRLEN)) {
       vqec_nat_log_error("Failed to determine ip from ifname %s",
                          __FUNCTION__); 
       ret = FALSE;
       goto done;
    }
    upnp_err = UpnpInit(host_ip, 0);
    if (upnp_err != UPNP_E_SUCCESS) {
        vqec_nat_log_error("Failed to initialize UPnP library %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    upnp_err = UpnpRegisterClient(upnp_event_handler, 
                                  &s_upnp.ctrlpt_hnd, 
                                  &s_upnp.ctrlpt_hnd);
    if (upnp_err != UPNP_E_SUCCESS) {
        vqec_nat_log_error("Failed to register UPnP Control Point %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }

    /* Determine default gateway - fail if none found */
    s_upnp.default_igd_ip = get_default_route();
    if (!s_upnp.default_igd_ip) {
        vqec_nat_log_error("Failed to determine default route %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }

    /* Initiate search for WANIPConnection service device */
    upnp_err = UpnpSearchAsync(s_upnp.ctrlpt_hnd, 
                    DISCOVERY_TIMEOUT,
                    "urn:schemas-upnp-org:service:WANIPConnection:1", 
                    &s_upnp.ctrlpt_hnd);

    /* Wait on ready condition - signaled on timeout or IGD found */
    (void)sem_wait(&s_upnp.igd_ready_cond);

    if (upnp_err != UPNP_E_SUCCESS) {
        vqec_nat_log_error("Failed to discover UPnP IGD %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    if (!s_upnp.igd.ready && s_upnp.debug_en) {
        vqec_nat_log_debug("Failed to discover UPnP IGD\n");
    }

/* Macro for appending argument elements to an xml request */
#define ADD_ARG_TO_XML(_doc, _elem, _name)                  \
    s_upnp._doc._elem = append_xml_element(                 \
                                  s_upnp._doc.doc,          \
                                  action_elem,              \
                                  _name);                   \
    if (!s_upnp._doc._elem) {                               \
        vqec_nat_log_error("UPnP xml allocation failed %s", \
                           __FUNCTION__);                   \
        ret = FALSE;                                        \
        goto done;                                          \
    }
                                            
    
    /* Initialize an AddPortMapping request document */
    s_upnp.add_port_xml.doc = ixmlDocument_createDocument();
    if (!s_upnp.add_port_xml.doc) {
        vqec_nat_log_error("UPnP xml allocation failed %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    action_elem = append_xml_element(s_upnp.add_port_xml.doc, 
                                     NULL, "u:AddPortMapping");
    if (!action_elem) {
        vqec_nat_log_error("UPnP xml allocation failed %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    (void)ixmlElement_setAttribute(action_elem, 
                                   "xmlns:u", 
                                   s_upnp.igd.service_type);
    ADD_ARG_TO_XML(add_port_xml, remote_host, "NewRemoteHost")
    ADD_ARG_TO_XML(add_port_xml, external_port, "NewExternalPort")
    ADD_ARG_TO_XML(add_port_xml, protocol, "NewProtocol")
    ADD_ARG_TO_XML(add_port_xml, internal_port, "NewInternalPort")
    ADD_ARG_TO_XML(add_port_xml, internal_client, "NewInternalClient")
    ADD_ARG_TO_XML(add_port_xml, enabled, "NewEnabled")
    ADD_ARG_TO_XML(add_port_xml, description, "NewPortMappingDescription")
    ADD_ARG_TO_XML(add_port_xml, lease_duration, "NewLeaseDuration")
    (void) append_xml_text(s_upnp.add_port_xml.doc, 
                           s_upnp.add_port_xml.remote_host, "");
    (void) append_xml_text(s_upnp.add_port_xml.doc,
                           s_upnp.add_port_xml.protocol, "UDP");
    (void) append_xml_text(s_upnp.add_port_xml.doc,
                           s_upnp.add_port_xml.enabled, "1");
    (void) append_xml_text(s_upnp.add_port_xml.doc,
                           s_upnp.add_port_xml.lease_duration, "0");


    /* Initialize DeletePortMapping request document */
    s_upnp.del_port_xml.doc = ixmlDocument_createDocument();
    if (!s_upnp.del_port_xml.doc) {
        vqec_nat_log_error("UPnP xml allocation failed %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    action_elem = append_xml_element(s_upnp.del_port_xml.doc,
                                     NULL, "u:DeletePortMapping");
    if (!action_elem) {
        vqec_nat_log_error("UPnP xml allocation failed %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    (void)ixmlElement_setAttribute(action_elem,
                                   "xmlns:u",
                                   s_upnp.igd.service_type);
    ADD_ARG_TO_XML(del_port_xml, remote_host, "NewRemoteHost")
    ADD_ARG_TO_XML(del_port_xml, external_port, "NewExternalPort")
    ADD_ARG_TO_XML(del_port_xml, protocol, "NewProtocol")
    (void) append_xml_text(s_upnp.del_port_xml.doc,
                           s_upnp.del_port_xml.protocol, "UDP");


    /* Initialize GetExternalIPAddress request document */
    s_upnp.get_ext_ip_xml.doc = ixmlDocument_createDocument();
    if (!s_upnp.get_ext_ip_xml.doc) {
        vqec_nat_log_error("UPnP xml allocation failed %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    action_elem = append_xml_element(s_upnp.get_ext_ip_xml.doc,
                                     NULL, "u:GetExternalIPAddress");
    if (!action_elem) {
        vqec_nat_log_error("UPnP xml allocation failed %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    (void)ixmlElement_setAttribute(action_elem,
                                   "xmlns:u",
                                   s_upnp.igd.service_type);

    /* Request the external IP address */
    if (s_upnp.igd.ready) {
        (void) vqec_nat_upnp_get_external_ip();
    }

    /* Create and start refresh timer, based on refresh_interval */
    s_upnp.ev_refresh = vqec_nat_timer_create(TRUE, upnp_refresh_handler, NULL);
    if (!s_upnp.ev_refresh) {
        vqec_nat_log_error("Failed to allocate refresh timer event %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }
    if (!vqec_nat_timer_start(s_upnp.ev_refresh, 
                              s_upnp.refresh_interval*1000)) {
        vqec_nat_log_error("Failed to start refresh timer %s",
                           __FUNCTION__);
        ret = FALSE;
        goto done;
    }

    /* Initialization successful */
    s_upnp.init_done = TRUE;

  done:
    if (!ret) {
        vqec_nat_upnp_destroy();
    }
    return ret;
}


/**
 * Open a new NAT binding.
 */
vqec_nat_bindid_t vqec_nat_upnp_open (vqec_nat_bind_desc_t *desc)
{
    vqec_nat_upnp_bind_t *new_bind = NULL;
    boolean err = FALSE;
    uint16_t hash;
    char str1[INET_ADDRSTRLEN], str2[INET_ADDRSTRLEN];

    if (!s_upnp.init_done || !desc) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        return VQEC_NAT_BINDID_INVALID;
    }

    /* Check that new mapping doesn't exist already */
    if (vqec_nat_upnp_find_by_desc(desc) != VQEC_NAT_BINDID_INVALID) {
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

    /* Create and zero a new bind object */
    new_bind = (vqec_nat_upnp_bind_t *)zone_acquire(s_upnp.bind_pool);
    if (!new_bind) {
        vqec_nat_log_error("Failed to allocate new binding %s",
                           __FUNCTION__);
        err = TRUE;
        goto done;
    }
    memset(new_bind, 0, sizeof(vqec_nat_upnp_bind_t));

    /* Initialize the new binding based on desc */
    new_bind->id = id_get(new_bind, s_upnp.bindid_table);
    if (id_mgr_is_invalid_handle(new_bind->id)) {
        vqec_nat_log_error("Bind id allocation failed %s",
                           __FUNCTION__);
        err = TRUE;
        goto done;
    }
    new_bind->desc = *desc;
    new_bind->state = VQEC_NAT_BIND_STATE_UNKNOWN;
    new_bind->is_map_valid = FALSE;
    new_bind->ext_port = desc->internal_port;
    new_bind->ext_addr = s_upnp.igd.ext_addr;
    new_bind->awaiting_ack = FALSE;

    /* Create bind update timer event */
    new_bind->ev_update = vqec_nat_timer_create(FALSE, 
                                                update_timer_callback,
                                                (void *)new_bind);
    if (!new_bind->ev_update) {
        err = TRUE;
        vqec_nat_log_error("Failed to create update timer %s",
                           __FUNCTION__);
        goto done;
    }

    /* Create callback cookie */
    /* Allocate from zone if available, otherwise adopt orphan */
    if (s_upnp.cookie_cnt < COOKIE_MULTIPLIER*s_upnp.max_bindings) {
        new_bind->cookie = 
                (vqec_nat_upnp_cookie_t *)zone_acquire(s_upnp.cookie_pool);
        if (!new_bind->cookie) {
            err = TRUE;
            vqec_nat_log_error("Failed to allocate cookie %s",
                               __FUNCTION__);
            goto done;
        }
        s_upnp.cookie_cnt++;
    } else {
        new_bind->cookie = VQE_TAILQ_FIRST(&s_upnp.orphan_cookies);
        VQE_TAILQ_REMOVE(&s_upnp.orphan_cookies, new_bind->cookie, le);
    }
    memset(new_bind->cookie, 0, sizeof(vqec_nat_upnp_cookie_t));
    new_bind->cookie->id = new_bind->id;
    new_bind->cookie->refcnt = 0;
    new_bind->cookie->destroyed = FALSE;

    /* Add new binding to s_upnp hashtable */
    hash = vqec_nat_proto_hash((uint16_t)new_bind->desc.internal_port);
    VQE_TAILQ_INSERT_TAIL(&s_upnp.lh[hash], new_bind, le);
    s_upnp.open_bindings++;

    /* Send an initial port mapping request */
    if (s_upnp.igd.ready && !vqec_nat_upnp_add_port_mapping(new_bind)) {
        vqec_nat_log_error("UPnP failed to add port mapping %s",
                           __FUNCTION__);
        goto done;
    }

    if (s_upnp.debug_en) {
        vqec_nat_log_debug("Proto: UPnP opened binding %s:%u:%s:%u\n",
                           uint32_ntoa_r(new_bind->desc.internal_addr,
                                         str1, sizeof(str1)),
                           ntohs(new_bind->desc.internal_port),
                           uint32_ntoa_r(new_bind->desc.remote_addr,
                                         str2, sizeof(str2)),
                           ntohs(new_bind->desc.remote_port));
    }

  done:
    if (err) {
        /* Clean up should an error have occured */
        if (new_bind) {
            if (!id_mgr_is_invalid_handle(new_bind->id)) {
                id_delete(new_bind->id, s_upnp.bindid_table);
                s_upnp.open_bindings--;
            }
            if (new_bind->ev_update) {
                vqec_nat_timer_destroy(new_bind->ev_update);
            }
            if (new_bind->cookie) {
                zone_release(s_upnp.cookie_pool, new_bind->cookie);
                s_upnp.cookie_cnt--;
            }
            zone_release(s_upnp.bind_pool, new_bind);
        }
        return (VQEC_NAT_BINDID_INVALID);
    }

    return new_bind->id;
}

/**
 * Close an existing NAT binding.
 */
void vqec_nat_upnp_close (vqec_nat_bindid_t id)
{
    vqec_nat_upnp_bind_t *p_bind;
    id_mgr_ret ret;
    uint16_t hash;
    char str1[INET_ADDRSTRLEN], str2[INET_ADDRSTRLEN];

    if (!s_upnp.init_done || id_mgr_is_invalid_handle(id)) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        return;
    }

    /* Find bind object matching bindid */
    p_bind = (vqec_nat_upnp_bind_t *)id_to_ptr(id, &ret,
                                               s_upnp.bindid_table);
    if (!p_bind || (ret != ID_MGR_RET_OK)) {
        vqec_nat_log_error("Id lookup failure  %s",
                           __FUNCTION__);
        return;
    }

    /* Send out a delete port mapping request */
    if (s_upnp.igd.ready) {
        (void)vqec_nat_upnp_delete_port_mapping(p_bind);
    }

    /* Remove binding from the s_upnp hash table */
    hash = vqec_nat_proto_hash((uint16_t)p_bind->desc.internal_port);
    VQE_TAILQ_REMOVE(&s_upnp.lh[hash], p_bind, le);

    /* Destroy update timer */
    if (p_bind->ev_update) {
        (void)vqec_nat_timer_stop(p_bind->ev_update);
        vqec_nat_timer_destroy(p_bind->ev_update);
    }

    /* Destroy callback cookie */
    if (p_bind->cookie) {
        p_bind->cookie->destroyed = TRUE;
        /* Release to pool if no outstanding callbacks */
        if (p_bind->cookie->refcnt == 0) {
            zone_release(s_upnp.cookie_pool, p_bind->cookie);
            s_upnp.cookie_cnt--;
        /* Add to orphan cookies list otherwise */
        } else {
            VQE_TAILQ_INSERT_TAIL(&s_upnp.orphan_cookies, p_bind->cookie, le);
        }
        p_bind->cookie = NULL;
    }
        
    if (s_upnp.debug_en) {
        vqec_nat_log_debug("Proto: UPnP closed binding %s:%u:%s:%u\n",
                           uint32_ntoa_r(p_bind->desc.internal_addr,
                                         str1, sizeof(str1)),
                           ntohs(p_bind->desc.internal_port),
                           uint32_ntoa_r(p_bind->desc.remote_addr,
                                         str2, sizeof(str2)),
                           ntohs(p_bind->desc.remote_port));
    }

    /* Zero the structure */
    memset(p_bind, 0, sizeof(vqec_nat_upnp_bind_t));
    
    /* Free additional resources */
    id_delete(id, s_upnp.bindid_table);
    zone_release(s_upnp.bind_pool, p_bind);
    s_upnp.open_bindings--;    
}

/**
 * Query a NAT binding to find it's most recent mapping.
 */
boolean vqec_nat_upnp_query (vqec_nat_bindid_t id,
                             vqec_nat_bind_data_t *data, 
                             boolean refresh)
{
    vqec_nat_upnp_bind_t *p_bind;
    id_mgr_ret id_ret;
    char str[INET_ADDRSTRLEN];   
 
    if (!s_upnp.init_done ||
        id_mgr_is_invalid_handle(id) || !data) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        return FALSE;
    }

    /* Find binding object based on bindid */
    p_bind = (vqec_nat_upnp_bind_t *)id_to_ptr(id, &id_ret,
                                               s_upnp.bindid_table);
    if (!p_bind || (id_ret != ID_MGR_RET_OK)) {
        vqec_nat_log_error("Id lookup failure %s",
                           __FUNCTION__);
        return FALSE;
    }

    /* Send an add port mapping request, should refresh be desired */
    if (refresh && s_upnp.igd.ready && !p_bind->awaiting_ack) {
        (void)vqec_nat_upnp_add_port_mapping(p_bind);
    }

    /* Fill data structure with bind info */
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

    if (s_upnp.debug_en && data->is_map_valid) {
        vqec_nat_log_debug("Proto: UPnP Query binding id: %lu %s:%u\n",
                           data->id,
                           uint32_ntoa_r(data->ext_addr,
                                         str, sizeof(str)),
                           ntohs(data->ext_port));
    }

    return TRUE;
}

/**
 * Print state of a NAT binding.
 */
void vqec_nat_upnp_fprint (vqec_nat_bindid_t id)
{
    vqec_nat_upnp_bind_t *p_bind;
    id_mgr_ret id_ret;
    char str[INET_ADDRSTRLEN];

    if (!s_upnp.init_done ||
        id_mgr_is_invalid_handle(id)) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        return;
    }

    /* Find binding object from bindid */
    p_bind = (vqec_nat_upnp_bind_t *)id_to_ptr(id, &id_ret,
                                               s_upnp.bindid_table);
    if (!p_bind || (id_ret != ID_MGR_RET_OK)) {
        vqec_nat_log_error("Id lookup failure %s",
                           __FUNCTION__);
        return;
    }

    /* Print formatted info */
    vqec_nat_print(" Binding name:              %s\n",
                   p_bind->desc.name);
    vqec_nat_print(" NAT protocol:              UPnP\n");
    vqec_nat_print(" NAT status:                %s\n",
                   vqec_nat_state_to_str(p_bind->state));
    vqec_nat_print(" Internal address:          %s:%u\n",
                   uint32_ntoa_r(p_bind->desc.internal_addr,
                                 str, sizeof(str)),
                   ntohs(p_bind->desc.internal_port));
    if (p_bind->is_map_valid) {
        vqec_nat_print(" Public address:            %s:%u\n",
                       uint32_ntoa_r(p_bind->ext_addr,
                                     str, sizeof(str)),
                       ntohs(p_bind->ext_port));
    } else {
        vqec_nat_print(" Public address:            0.0.0.0:0\n");
    }
}

/**
 * Print all protocol / binding status.
 */
void vqec_nat_upnp_fprint_all (void)
{
    int32_t i;
    vqec_nat_upnp_bind_t *p_bind;

    if (!s_upnp.init_done) {
        vqec_nat_log_error("Bad input arguments %s",
                           __FUNCTION__);
        return;
    }

    /* Print usual NAT info */
    vqec_nat_print("NAT protocol:               UPnP\n");
    vqec_nat_print("NAT bindings open:          %d\n",
                   s_upnp.open_bindings);

    /* Print IGD specific info */
    vqec_nat_print("IGD status:                 %s\n",
                   s_upnp.igd.ready ? "ready" : "unknown");
    if (s_upnp.igd.ready) {
        vqec_nat_print(" Service Type:              %s\n",
                       s_upnp.igd.service_type);
        vqec_nat_print(" Base URL:                  %s\n",
                       s_upnp.igd.base_url);
        vqec_nat_print(" Control URL:               %s\n",
                       s_upnp.igd.control_url);
    }

    /* Print status of each binding */
    for (i = 0; i < UPNP_HASH_MAP_SIZE; i++) {
        VQE_TAILQ_FOREACH(p_bind,
                      &s_upnp.lh[i],
                      le) {
            vqec_nat_print("NAT id:                     %lu\n",
                           p_bind->id);
            vqec_nat_upnp_fprint(p_bind->id);
        }
    }
}

/**
 * Insert an ejected packet for the given binding.
 */
void vqec_nat_upnp_eject (vqec_nat_bindid_t id, char *buf, uint16_t len,
                          in_addr_t source_ip, in_port_t source_port)
{
    /* Ignore ejected packets */
}

/**
 * Switch debugging on.
 */
void vqec_nat_upnp_debug_set (boolean verbose)
{
    s_upnp.debug_en = TRUE;
    s_upnp.verbose = verbose;
}

/**
 * Switch debugging off.
 */
void vqec_nat_upnp_debug_clr (void)
{
    s_upnp.debug_en = FALSE;
}

/**
 * Is the device behind a NAT?
 */
boolean vqec_nat_upnp_is_behind_nat (void)
{
    return TRUE;
}

/**---------------------------------------------------------------------------
 * Interface definition.
 *---------------------------------------------------------------------------*/  
static vqec_nat_proto_if_t s_upnp_if = 
{

    .create = vqec_nat_upnp_create,
    .destroy = vqec_nat_upnp_destroy,
    .open = vqec_nat_upnp_open,
    .close = vqec_nat_upnp_close,
    .query = vqec_nat_upnp_query,
    .fprint = vqec_nat_upnp_fprint,
    .fprint_all = vqec_nat_upnp_fprint_all,
    .eject_rx = vqec_nat_upnp_eject,
    .debug_set = vqec_nat_upnp_debug_set,
    .debug_clr = vqec_nat_upnp_debug_clr,
    .is_behind_nat = vqec_nat_upnp_is_behind_nat
   
};

const vqec_nat_proto_if_t *
vqec_nat_get_if_upnpproto (void)
{
    return (&s_upnp_if);
}

#else /* HAVE_UPNP */

const vqec_nat_proto_if_t *
vqec_nat_get_if_upnpproto (void)
{
    return (NULL);
}

#endif /* HAVE_UPNP */

