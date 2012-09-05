/*------------------------------------------------------------------
 *
 * STUN Socket API
 *
 * Copyright (c) 2006-2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file is intended SOLELY for testing purposes within the stun
 * component.  It should not be used as a socket interface for any
 * other purpose.
 *
 *------------------------------------------------------------------
 */

#include "stun_socket.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TBUFLEN 20
stun_sock_t * 
stun_sock_create(char * name,
                 in_addr_t if_addr, 
                 uint16_t         port,
                 uint32_t rcv_buff_bytes)
{
    stun_sock_t * result = malloc(sizeof(stun_sock_t));
    struct sockaddr_in saddr;
    char buf[TBUFLEN];
    int on = 1;

    if (!inet_ntop(AF_INET, &if_addr, buf, TBUFLEN)) {
        return NULL;
    }
    
    if (! result)
        return NULL;

    memset(result, 0, sizeof(stun_sock_t));

    if ((result->fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        free(result);
        return NULL;
    }

    
    /* remember arguments */
    result->if_addr.s_addr = if_addr;
    result->port = port;
    result->rcv_buff_bytes = rcv_buff_bytes;

    memset(&saddr, 0, sizeof(saddr));

    if (setsockopt(result->fd, SOL_SOCKET, SO_REUSEADDR, 
                   &on, sizeof(on)) == -1) {
        perror("setsockopt");
        goto bail;
    }

    saddr.sin_addr.s_addr = if_addr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = port;

    if (bind(result->fd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
        perror("bind");
        goto bail;
    }

    if (setsockopt(result->fd, SOL_SOCKET, 
                   SO_TIMESTAMP, &on, sizeof(on)) == -1) {
        perror("setsockopt");
        goto bail;
    }

    if (rcv_buff_bytes) {
        if (setsockopt(result->fd, SOL_SOCKET,
                       SO_RCVBUF, &rcv_buff_bytes, sizeof(rcv_buff_bytes)) == -1) {
            perror("setsockopt");
            goto bail;
        }
    }

    if (fcntl(result->fd, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        goto bail;
    }

    return result;

 bail:
    close(result->fd);
    free(result);
    return NULL;
  
}


