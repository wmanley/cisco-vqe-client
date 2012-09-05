/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Kernel module network utility methods.
 *
 * Documents: 
 *
 *****************************************************************************/
#ifndef VAM_UTIL_H
#define VAM_UTIL_H

#include "vam_types.h"

/**
   A DSCP value is specified as a 6-bit value, from 0 to 63. It needs to be
   left shifted when passing to setsockopt() to set the IP_TOS option for a 
   socket.
 */
#define DSCP_SHIFT  2

#define VQE_IPV4_ADDR_STRLEN sizeof("255.255.255.255")
#define VQE_IPV4_OCTET_STRLEN sizeof("255")

/**
   Given an uint32_t IP address (in network order), return a dot separated 
   IP address string.  Note both ipaddrtype and in_addr_t are uint32_t type.
   @param[in] h_addr   an uint32_t IP address in network order
   @param[in/out] bug  caller allocated buffer
   @param[in] buf_len  the allocated buffer length
   @return ptr to the buf
*/
char *uint32_ntoa_r(uint32_t addr, char *buf, int buf_len);

/**
   Given an uint32_t IP address in host order, return a dot separated 
   IP address string.
   @param[in] h_addr   an uint32_t IP address in host order
   @param[in/out] bug  caller allocated buffer
   @param[in] buf_len  the allocated buffer length
   @return ptr to the buf
*/
char *uint32_htoa_r(uint32_t h_addr, char *buf, int buf_len);

#endif /* VAM_UTIL_H */
