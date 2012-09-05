/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Upcall API.
 *
 * Documents: 
 *
 *****************************************************************************/

#ifndef __VQEC_UPCALL_EVENT_H__
#define __VQEC_UPCALL_EVENT_H__

#include "vam_types.h"
#include "vqec_dp_api.h"
#include "vqec_error.h"

typedef
struct vqec_upcall_ev_stats_
{
    /**
     * Upcall events received.
     */
    uint32_t upcall_events;
    /**
     * Upcall events with generation number mismatch.
     */
    uint32_t upcall_lost_events;
    /**
     * Error events, i.e., events that cannot be parsed.
     */
    uint32_t upcall_error_events;
    /**
     * Error while acknowledging / polling events.
     */
    uint32_t upcall_ack_errors;

} vqec_upcall_ev_stats_t;


/**---------------------------------------------------------------------------
 * Setup the upcall sockets, and register them with dataplane / libevent.
 * REQUIREMENT: Dataplane must be initialized prior to invocation of this call.
 *---------------------------------------------------------------------------*/ 
vqec_error_t
vqec_upcall_setup_unsocks(uint32_t max_paksize);

/**---------------------------------------------------------------------------
 * Close the upcall sockets.
 *---------------------------------------------------------------------------*/ 
void
vqec_upcall_close_unsocks(void);

/**---------------------------------------------------------------------------
 * Display global state.
 *
 * locks: < {vqec_g_lock}, = { }, > {vqec_g_lock}
 *---------------------------------------------------------------------------*/ 
void
vqec_upcall_display_state(void);

/**---------------------------------------------------------------------------
 * Get IRQ event statistics - used for unit-tests.
 *
 * @param[in] stats Pointer to the stats structure.
 *---------------------------------------------------------------------------*/ 
void
vqec_upcall_get_irq_stats(vqec_upcall_ev_stats_t *stats);

#endif /* __VQEC_UPCALL_EVENT_H__ */
