/**-----------------------------------------------------------------
 * @brief
 * Raw socket related routines.
 *
 * @file
 * raw_socket.c
 *
 * October 2007, Donghai Ma
 *
 * Copyright (c) 2007 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "raw_socket.h"

/* Function: create_raw_socket
 * Description: Setup a non-blocking raw socket. Change receive or send buffer 
 *              size if the passed in rcvbuflen or sndbuflen values is 
 *              non zero.
 * Parameters:  protocol - specify a particular protocol to be used with 
 *                         the socket.
 *              rcvbuflen - socket receive buffer size
 *              sndbuflen - socket send buffer size
 *              fd - returned fd of the socket created
 *              sock_err - returned errno if one of the socket operations failed
 * Returns:     -1 if failed; otherwise return the created socket fd
 * Side Effects: none 
 */
raw_socket_error_t
create_raw_socket (int protocol,
                   int rcvbuflen,
                   int sndbuflen,
                   int *fd,
                   int *sock_err)
{
    raw_socket_error_t err;
    int raw_sock_fd = -1;
    int one = 1;
    const int *val = &one;

    if (!fd || !sock_err) {
        err = RAW_SOCKET_ERR_INVALIDARGS;
        goto bail;
    }

    raw_sock_fd = socket(PF_INET, SOCK_RAW, protocol);
    if (raw_sock_fd == -1) {
        err = RAW_SOCKET_ERR_SOCKET;
        *sock_err = errno;
        goto bail;
    }

    /* Set it as non-blocking */
    if (fcntl(raw_sock_fd, F_SETFL, O_NONBLOCK) == -1) {
        err = RAW_SOCKET_ERR_NONBLOCK;
        *sock_err = errno;
        goto bail;
    }    

    /* Set recv buffer size */
    if (rcvbuflen) {
        if (setsockopt(raw_sock_fd, SOL_SOCKET, SO_RCVBUF, 
                       (char *)&rcvbuflen, sizeof(rcvbuflen)) == -1) {
            err = RAW_SOCKET_ERR_RCVBUF;
            *sock_err = errno;
            goto bail;
        }
    }

    if (sndbuflen) {
        if (setsockopt(raw_sock_fd, SOL_SOCKET, SO_SNDBUF, 
                       (char *) &sndbuflen, sizeof(rcvbuflen)) == -1) {
            err = RAW_SOCKET_ERR_SNDBUF;
            *sock_err = errno;
            goto bail;
        }
    }

    /* Inform the kernel do not fill up the IP header structure and we will 
     * build our own.
     */
    if (setsockopt(raw_sock_fd, IPPROTO_IP, IP_HDRINCL, 
                   val, sizeof(one)) == -1) {
        err = RAW_SOCKET_ERR_HDRINCL;
        *sock_err = errno;
        goto bail;
    }

    *fd = raw_sock_fd;
    return RAW_SOCKET_OK;
    
bail:
    if (raw_sock_fd != -1) {
        close(raw_sock_fd);
    }

    *fd = -1;
    return err;
}
