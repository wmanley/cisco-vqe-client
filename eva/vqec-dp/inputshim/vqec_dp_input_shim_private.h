/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Input shim private/internal definitions.
 *              See vqec_dp_input_shim_api.h for public definitions. 
 *
 * Documents:
 *
 *****************************************************************************/
#ifndef __VQEC_DP_INPUT_SHIM_PRIVATE_H__
#define __VQEC_DP_INPUT_SHIM_PRIVATE_H__

#include "vqec_recv_socket.h"
#include "vqec_dp_io_stream.h"
#include "vqec_dp_input_shim_api.h"
#include "queue_plus.h"
#include "vqec_dp_api.h"

extern vqec_dp_input_shim_status_t vqec_dp_input_shim_status;

/*
 * The Filter Entry object
 *
 * This represents an entry point to the network layer for which a packet
 * filter is applied, essentially encapsulating the socket abstraction which
 * implements it.
 */
struct vqec_dp_input_shim_os;
typedef struct vqec_filter_entry_ {
    VQE_LIST_ENTRY(vqec_filter_entry_) list_obj;
    vqec_dp_input_filter_t         filter;      /* filter values */
    vqec_recv_sock_t               *socket;     /* filter implementation */
    vqec_recv_sock_t               *extra_igmp_socket;
                                                /* for rcc extra igmp ip */
    uint32_t                       scheduling_class;
                                                /* when to process filter */ 
    struct vqec_dp_input_shim_os   *os;         /* associated OS */
    uint32_t                       so_rcvbuf;   /* limit of the receive buffer */
    uint8_t                        xmit_dscp_value;  /*
                                                      * DSCP value to be
                                                      * used when sending
                                                      * pkts to the stream's
                                                      * source.
                                                      */
    boolean                        committed;   /* Has bind been committed ? */
                                   
} vqec_filter_entry_t;

/*
 * The Filter Entry table
 *
 * Filter table entries are processed by reading packets from their 
 * associated socket, and passing them to the associated output stream's 
 * connected input stream.
 * 
 * Since different filters need to be processed at different frequencies, 
 * filters are associated with "scheduling classes".  All filters associated
 * with a given scheduling class are maintained on a list whose list head is
 * accessible via an array indexed by scheduling class:
 *
 *  scheduling class:      0       1     ....     max
 *                     +-------+-------+-------+-------+
 *           interval: |  20   |  40   |       |   x   |
 *          remaining: |   0   |  20   |       | x-20  |
 *                     |  LH   |  LH   |       |  LH   |
 *                     +---|---+-------+-------+---|---+
 *                         v                       v
 *                       +---+                   +---+
 *                    /  |   |                   |   |
 *           filter /    +---+                   +---+
 *           entries       |
 *                 \       v
 *                   \   +---+
 *                     \ |   |
 *                       +---+
 *
 * Also stored for each scheduling class are its scheduling interval and
 * amount of time remaining until next service.
 */
extern uint32_t vqec_dp_input_shim_num_scheduling_classes;
typedef struct vqec_dp_scheduling_class_ {
    uint16_t interval;   /* requested service interval (in milliseconds) */
    int32_t remaining;   /*
                          * time remaining until service is needed
                          *  (in milliseconds, can go negative temporarily)
                          */
    VQE_LIST_HEAD(,vqec_filter_entry_) filters;
} vqec_dp_scheduling_class_t;
extern vqec_dp_scheduling_class_t *vqec_dp_input_shim_filter_table;

/*
 * The Output Stream data object
 *
 * This represents a stream of data that is passed from the input shim
 * to a particular connected input stream.
 */
typedef struct vqec_dp_input_shim_os  {
    VQE_LIST_ENTRY(vqec_dp_input_shim_os) list_obj;
    vqec_dp_osid_t         os_id;    /* handle ID for this OS */
    vqec_dp_encap_type_t   encaps;   /* stream encapsulation type (udp/rtp) */
    int32_t                capa;     /* capabilities of this OS */
    vqec_dp_isid_t         is_id;    /* connected IS (if connected) */
    const vqec_dp_isops_t  *is_ops;  /* connected IS APIs (if connected) */
    int32_t                is_capa;  /* connected IS capabilities */ 
    vqec_filter_entry_t    *filter_entry;  /* associated filter (or NULL) */
    vqec_dp_stream_stats_t stats;    /* OS statistics */
} vqec_dp_input_shim_os_t;

/*
 * The Output Stream list
 *
 * Simple list of all output streams created within the input shim.
 */
VQE_LIST_HEAD(,vqec_dp_input_shim_os) vqec_dp_input_shim_os_list;


#endif  /* __VQEC_DP_INPUT_SHIM_PRIVATE_H__ */
