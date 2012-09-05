/*
 * Copyright (c) 2007=2008 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef VQEC_STREAM_OUTPUT_THREAD_MGR_H
#define VQEC_STREAM_OUTPUT_THREAD_MGR_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include "vqec_ifclient_defs.h"
#include "vqec_error.h"
#include "vqec_url.h"

/**
 * socket structure 
 */
typedef struct vqec_stream_output_sock_ {
    struct in_addr output_if_address;
    in_port_t port;
    int blocking;              /*!< blocking or not */
    int caller_provided_mem;   /*!< caller allocated memory or not */
    struct in_addr dest_addr;  /*!< dest addr to send to */
    uint32_t send_buff_bytes;  /*!< receive buff size for socket */
    int fd;                    /*!< fd */
    uint64_t outputs;          /*!< output packets */
    uint64_t output_drops;     /*!< output drops on socket */
} vqec_stream_output_sock_t;

typedef struct vqec_stream_output_thread_ {
    vqec_tunerid_t tuner_id;
    pthread_t thread_id;
    in_addr_t output_ifaddr;             /* output inteface address */
    short output_prot;
    in_addr_t output_dest;
    in_port_t output_port;
    vqec_stream_output_sock_t *output_sock;           /* output socket */
    int changed;                         /* parameter changed, need to 
                                            update socket */
    int exit;

    /*
     * FIXME 
     * Since igmp event handler is part of libevent, and our mutxt lock
     * is not recursive. So when igmp captured channel change, we can't
     * do chan bind/unbind in igmp module. We have to do it in seperate 
     * thread, i.e., output thread for now.
     */
    int bind;
    int unbind;
    char url[VQEC_MAX_URL_LEN];
} vqec_stream_output_thread_t;

vqec_error_t
vqec_stream_output (char *name,
                    char * if_name,
                    char *url);

void 
vqec_stream_output_channel_join_call_back_func(char *url, 
                                               vqec_tunerid_t tuner_id,
                                               in_addr_t output_ifaddr,
                                               in_addr_t output_dest,
                                               in_port_t output_port,
                                               void *call_back_data);

void 
vqec_stream_output_channel_leave_call_back_func(char *url, 
                                                vqec_tunerid_t tuner_id,
                                                in_addr_t output_ifaddr,
                                                in_addr_t output_dest,
                                                in_port_t output_port,
                                                void *call_back_data);

void vqec_stream_output_thread_mgr_module_init(void);
void vqec_stream_output_thread_mgr_module_deinit(void);
void vqec_stream_output_thread_mgr_init_startup_streams(char *filename);
void vqec_stream_output_mgr_show_stats(vqec_tunerid_t id);
void vqec_stream_output_mgr_clear_stats(vqec_tunerid_t id);
vqec_stream_output_thread_t *
vqec_stream_output_thread_get_by_tuner_id(vqec_tunerid_t tuner_id);

#endif /* VQEC_STREAM_OUTPUT_THREAD_MGR_H */
