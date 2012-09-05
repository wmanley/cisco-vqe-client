/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */
/*
 * vqec_igmp.h - simulate a igmp proxy for vqec module.
 *
 * igmp proxy is trying to catch igmp join/leave packet, and send
 * corresponding request to vqes, at the same time, tell the call back
 * function that a signal is captured. If it's a join, the call back
 * function will spawn a output thread to send output stream to setup
 * box; if it's a leave, the call back function will kill the output
 * thread.
 * 
 * Notice that since in order to capture igmp join/leave in
 * application level, proxy needs to join all the channels vqec wants
 * to monitor. This brings the scability problem that proxy can not 
 * serve for a lot of channels.
 */
#ifndef VQEC_IGMP_H
#define VQEC_IGMP_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MAX_ETHERNET_INTERFACE_NAME_LEN (32)

#include "vqec_event.h"
#include "vqec_ifclient_defs.h"
#include "vqec_error.h"
#include "vqec_recv_socket.h"

typedef struct vqec_igmp_t {

    vqec_tunerid_t tuner_id;

    vqec_event_t *igmp_ev;

    vqec_recv_sock_t * igmp_sock;

    boolean proxy;
    /* 
     * We want to just listen to the igmp join we want, so we need the
     * stb ip address here. 
     */ 
    struct in_addr stb_addr;
    in_addr_t output_ifaddr;             /* output inteface address */
    char output_if_name[MAX_ETHERNET_INTERFACE_NAME_LEN];
    
    void (*channel_join_call_back_func)(const char *url, 
                                        vqec_tunerid_t tuner_id,
                                        in_addr_t output_ifaddr,
                                        in_addr_t output_dest,
                                        in_port_t output_port,
                                        void * call_back_data);

    void * channel_join_call_back_data;
    void (*channel_leave_call_back_func)(const char *url, 
                                         vqec_tunerid_t tuner_id,
                                         in_addr_t output_ifaddr,
                                         in_addr_t output_dest,
                                         in_port_t output_port,
                                         void * call_back_data);
    void * channel_leave_call_back_data;

} vqec_igmp_t;

boolean vqec_igmp_create(vqec_tunerid_t tuner_id, char * if_name,
                         in_addr_t output_if);
void vqec_igmp_destroy(vqec_tunerid_t tuner_id);

vqec_error_t
vqec_proxy_igmp_join(const char *name,
                     char *if_name,
                     in_addr_t stb_addr,
                     void (*channel_join_call_back_func),
                     void *channel_join_call_back_data,
                     void (*channel_leave_call_back_func),
                     void *channel_leave_call_back_data);

void vqec_cmd_show_proxy_igmp_stats(vqec_tunerid_t id);
boolean vqec_igmp_module_init(void);
void vqec_igmp_module_deinit(void);
void vqec_igmp_proxy_init(char *filename);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VQEC_IGMP_H */
