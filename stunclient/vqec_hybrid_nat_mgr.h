/*
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */

#include "vqec_nat_api.h"

/**
 * vqec_nat_hybrid_bind_update
 * 
 * Inform NAT client of an update via hybrid manager.
 *
 * @param[in] proto     protocol calling the update
 * @param[in] id        sub-protocol bindid
 * @param[in] data      update data
 */
void vqec_nat_hybrid_bind_update (vqec_nat_proto_t proto,
                                  vqec_nat_bindid_t id,
                                  vqec_nat_bind_data_t *data);

/**
 * vqec_nat_hybrid_inject_tx
 * 
 * Inject packets to socket through hybrid layer.
 *
 * @param[in] proto     protocol calling for the inject
 * @param[in] id        sub-protocol bindid
 * @param[in] desc      bind description from open call
 * @param[in] buf       packet buffer to inject
 * @param[in] len       buffer content length
 * @param[out] boolean  true for success, false otherwise
 */
boolean vqec_nat_hybrid_inject_tx (vqec_nat_proto_t proto,
                                   vqec_nat_bindid_t id,
                                   vqec_nat_bind_desc_t *desc, 
                                   char *buf,
                                   uint16_t len);




