/*
 *------------------------------------------------------------------
 * SRV lookup module
 *
 * July 2006, Dong Hsu
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _SRV_LOOKUP_H_
#define _SRV_LOOKUP_H_

#include <netinet/in.h>
#include <pthread.h>

#define NAME_LEN 80

typedef enum {
    TCP,
    UDP,
    UNKNOWN
} IpProtocolSocketType;

/**
 * Structure to describe a server found for a RTSP domain.
 */
typedef struct remote_server_ {
    char host[NAME_LEN];        /**< Host name. (Owned by this object.) */
    IpProtocolSocketType type;  /**< TCP,UDP */
    char addr[NAME_LEN];        /**< IP address and port */
    int port;
    unsigned int priority;      /**< SRV priority value */
    unsigned int weight;        /**< SRV weight */
    float score;                /**< Calculated sorting score */
    struct remote_server_ *next;  /**< Next server record */
} remote_server_t;



/* Get the list of server entries for RTSP domain name 'domain'. */
extern remote_server_t* lookup_SRV(const char *domain, const char *service, const char *proto_string);

/* Free the list of server entries */
extern void free_SRV(void);

#endif  // _SRV_LOOKUP_H_
