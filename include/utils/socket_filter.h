/**-----------------------------------------------------------------
 * @brief
 * BSD Packet Filter
 *
 * @file
 * socket_filter.h
 *
 * October 2007, Donghai Ma
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _SOCKET_FILTER_H_
#define _SOCKET_FILTER_H_


/* Function: attach_stun_request_filter
 * Description: Attach the STUN request BPF to a socket 
 * Parameters: sock_fd - descriptor for the socket to attach the STUN Request
 *                       Packet Filter
 * Returns:    setsockopt() return code
 * Side Effects: none
 */
extern int attach_stun_request_filter(int sock);

/* Function: attach_stun_response_filter
 * Description: Attach the STUN response BPF to a socket 
 * Parameters: sock_fd - descriptor for the socket to attach the STUN Response
 *                       Packet Filter
 * Returns:    setsockopt() return code
 * Side Effects: none
 */
extern int attach_stun_response_filter(int sock);

int attach_igmp_filter (int sock_fd, unsigned int stb_ip_addr);

#endif /* _SOCKET_FILTER_H_ */
