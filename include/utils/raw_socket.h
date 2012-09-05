/**-----------------------------------------------------------------
 * @brief
 * Raw socket.
 *
 * @file
 * raw_socket.h
 *
 * October 2007, Donghai Ma
 *
 * Copyright (c) 2007 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _RAW_SOCKET_H_
#define _RAW_SOCKET_H_

typedef enum raw_socket_error_ {
    RAW_SOCKET_OK,
    RAW_SOCKET_ERR_INVALIDARGS,
    RAW_SOCKET_ERR_SOCKET,
    RAW_SOCKET_ERR_NONBLOCK,
    RAW_SOCKET_ERR_RCVBUF,
    RAW_SOCKET_ERR_SNDBUF,
    RAW_SOCKET_ERR_HDRINCL,
} raw_socket_error_t;

/* Function: create_raw_socket
 * Description: Setup a non-blocking raw socket. Change receive or send buffer 
 *              size if the passed in rcvbuflen or sndbuflen values is 
 *              non zero.
 * Parameters:  protocol - specify a particular protocol to be used with 
 *                         the socket.
 *              rcvbuflen - socket receive buffer size
 *              sndbuflen - socket send buffer size
 * Returns:     error if failed; otherwise return the created socket fd
 * Side Effects: none 
 */
extern raw_socket_error_t
create_raw_socket(int protocol,
                  int rcvbuflen,
                  int sndbuflen,
                  int *fd,
                  int *sock_err);

#endif /* _RAW_SOCKET_H_ */
