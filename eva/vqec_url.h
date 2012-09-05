/*
 * Copyright (c) 2006-2007 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef VQEC_URL_H
#define VQEC_URL_H

#include <utils/vam_types.h>
#include <arpa/inet.h>
#define VQEC_MAX_URL_LEN 50

//
// Channel change URL - protocol://ip_address:port for IPv4.
// For IPv6, the nominal format is protocol://[IPv6 address]:port
//
typedef char* vqec_url_t;

typedef enum vqec_protocol_t {
    VQEC_PROTOCOL_UNKNOWN,
    VQEC_PROTOCOL_UDP,
    VQEC_PROTOCOL_RTP,
} vqec_protocol_t;

char * vqec_url_protocol_to_string(vqec_protocol_t);
boolean vqec_url_ip_parse (char *ip_string, in_addr_t *ip);

int vqec_url_build(vqec_protocol_t, in_addr_t, uint16_t, 
                      char*, int);

boolean vqec_url_parse(vqec_url_t, vqec_protocol_t *, 
                      in_addr_t *, uint16_t *);

#endif /* VQEC_URL_H */
