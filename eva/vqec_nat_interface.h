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
 * Description: Internal NAT interface - not used by the protocol.
 *
 * Documents: 
 *
 *****************************************************************************/

#ifndef __VQEC_NAT_INTF_H__
#define __VQEC_NAT_INTF_H__

#include <stunclient/vqec_nat_api.h>
#include "vqec_error.h"
#include "vqec_syscfg.h"

/**
 * Initialize NAT module.
 */
vqec_error_t vqec_nat_module_init(vqec_syscfg_sig_mode_t mode,
                                  vqec_nat_proto_t proto, 
                                  vqec_nat_init_params_t *params);

/**
 * Deinitialize NAT module.
 */
void vqec_nat_module_deinit(void);

/**
 * Open a new NAT binding.
 *
 * locks: < {vqec_g_lock}, = { }, > {vqec_g_lock}
 */
vqec_nat_bindid_t 
vqec_nat_open_binding(vqec_nat_bind_desc_t *desc);
    
/**
 * Close an existing NAT binding.
 *
 * locks: < {vqec_g_lock}, = { }, > {vqec_g_lock}
 */
void
vqec_nat_close_binding(vqec_nat_bindid_t id);

/**
 * Query an existing NAT binding.
 *
 * locks: < {vqec_g_lock}, = { }, > {vqec_g_lock}
 */
boolean
vqec_nat_query_binding(vqec_nat_bindid_t id, 
                       vqec_nat_bind_data_t *data, boolean refresh);

/**
 * Print the status of a NAT binding.
 *
 * locks: < {vqec_g_lock}, = { }, > {vqec_g_lock}
 */
void
vqec_nat_fprint_binding(vqec_nat_bindid_t id);

/**
 * Print the status of all NAT bindings.
 *
 * locks: < {vqec_g_lock}, = { }, > {vqec_g_lock}
 */
void
vqec_nat_fprint_all_bindings(void);

/**
 * Print the status of all NAT bindings.
 *
 * locks: < { }, = {vqec_g_lock}, > { }
 */
void
vqec_nat_fprint_all_bindings_safe(void);

/**
 * Forward an ejected packet for a binding to the protocol.
 *
 * locks: < {vqec_g_lock}, = { }, > {vqec_g_lock}
 */
void 
vqec_nat_eject_to_binding(vqec_nat_bindid_t id, char *buf, uint16_t len,
                          in_addr_t source_ip, in_port_t source_port);

/**
 * Enable NAT debugging for the protocol.
 */
void
vqec_nat_enable_debug(boolean verbose);

/**
 * Disable NAT debugging for the protocol.
 */
void
vqec_nat_disable_debug(void);

/**
 * Is the client behind a NAT?
 *
 * locks: < {vqec_g_lock}, = { }, > {vqec_g_lock}
 */
boolean
vqec_nat_is_behind_nat(void);

/**
 * Is NAT mode enabled?
 *
 * locks: < {vqec_g_lock}, = { }, > {vqec_g_lock}
 */
boolean
vqec_nat_is_natmode_en(void);


#endif /*__VQEC_NAT_INTF_H__ */
