/*------------------------------------------------------------------
 *
 * STUN Socket API
 *
 * Copyright (c) 2006 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file is intended SOLELY for testing purposes within the stun
 * component.  It should not be used as a socket interface for any
 * other purpose.
 *
 *------------------------------------------------------------------
 */

#ifndef __STUN_SOCKET_H__
#define __STUN_SOCKET_H__

#include <netinet/ip.h>
#include <netinet/in.h>
#include <stun_includes.h>

#define UDP_PROT 17
#define UDPHEADERBYTES 8
#define MINIPHEADERBYTES 20 /* no options */

#define IPADDR(b0,b1,b2,b3) (htonl((b0 << 24) | (b1 << 16) | (b2 << 8) | b3))
#define IPPORT(p) (htons(p))

typedef uint8_t boolean;

struct stun_sock_;

typedef struct stun_sock_ 
{
    struct in_addr if_addr; /*!< receive interface address */
    uint16_t port;                 /*!< bind port */
    uint32_t rcv_buff_bytes;       /*!< receive buff size for socket */
    int fd;                        /*!< fd */
} stun_sock_t;

/**
   Create socket
   @param[in] name Name of this socket.
   @param[in] if_addr ip address of receive interface.
   @param[in] port the port we want to bind/receive packet.
   @param[in] rcv_buff_bytes Buffer size pass to socket.
   @return pointer of created socket.
*/
stun_sock_t *
stun_sock_create(char * name,
                     in_addr_t if_addr, 
                     in_port_t      port,
                     uint32_t rcv_buff_bytes);
          

#endif /* __STUN_SOCKET_H__ */
