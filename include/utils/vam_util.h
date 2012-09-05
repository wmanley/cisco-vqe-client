/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef VAM_UTIL_H
#define VAM_UTIL_H

#ifndef __USE_MISC
#define __USE_MISC
#endif

#include "vam_types.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>

/**
   Macros to specify the event priorities for libevent
 */
#define VQE_EV_NUM_PRIORITIES   3  /* total number of the priorities */

#define VQE_EV_HIGH_PRIORITY    0
#define VQE_EV_MEDIUM_PRIORITY  1  /* unless explicitly set, events will use 
                                      the medium priority */
#define VQE_EV_LOW_PRIORITY     2

/**
   A DSCP value is specified as a 6-bit value, from 0 to 63. It needs to be
   left shifted when passing to setsockopt() to set the IP_TOS option for a 
   socket.
 */
#define DSCP_SHIFT  2

/**
   Get ip address from interface name.
   @param[in] ifname interface name like "eth0" or "eth1"
   @param[in] name buffer to hold the if name
   @param[in] size size of the name buffer, has to be greater than INET_ADDRSTRLEN
   @return TRUE if success, FALSE if failed.
*/
boolean get_ip_address_by_if( char * ifname, char * name, int size );

/**
   Get mac address from interface name.
   @param[in] ifname interface name like "eth0" or "eth1"
   @param[out] mac pointer of char to hold return mac address.
   @param[in] max lenth of returned mac string.
   @return TRUE or FALSE for success or not.
*/
boolean get_mac_address_by_if(char * ifname, char * mac, uint32_t max_len);

/**
   Get all interfaces in a box
   @param[in] ifc pointer of struct ifconf. Caller must set up 
   ifc.ifc_len = sizeof(buf);
   ifc.ifc_buf = buf;
   @return TRUE or FALSE for success or not.
*/
boolean get_all_interfaces(struct ifconf *);

/**
   Get first mac address from this box, start from eth0.
   Because interface lo has mac of 0.0.0.0.0.0
   @param[out] mac pointer of char to hold return mac address.
   @param[in] max_len - maximum length of mac string to return.
   @return TRUE or FALSE for success or not.
*/
boolean get_first_mac_address(char * mac, uint32_t max_len);

/**
   Given an in_addr struct, return a dot separated IP address string. 
   Caller MUST alocate memory for the buf[].
   @saddr[in] IP address in a in_addr struct.
   @return ptr to a dot separated IP address string.
*/
static inline
char *inaddr_ntoa_r(struct in_addr saddr, char *buf, int buf_len)
{
    return ((char *)inet_ntop(AF_INET, &saddr, buf, buf_len));
}

#define MAX_HOST_STR 255 /* the maximum length of an internet Domain Name */

#define MAX_IP_ADDR_LEN 16  /* longest addr: xxx.xxx.xxx.xxx = 15, plus \0 */

#define MAX_IP_ADDR_STR 25  /* longer  than MAX_IP_ADDR_LEN to avoid
                             * truncating what the user entered, and validate
                             * he entered no more than MAX_IP_ADDR_LEN chars
			     */
#define MAX_PORT_LEN 5  /* largest port: 65535, = 5 characters */

#define MAX_PORT_STR 10 /* larger than MAX_PORT_LEN to avoid
		         * truncating what the user entered, and validate
		         * he entered no more than MAX_IP_ADDR_LEN chars
		         */

/**
   Distinguish a string containing a Domain Name from one containing
   a IP address in dotted decimal notation.
   @param[in] name_str pointer to string containing either Domain Name or
   IP address in dotted decimal format
   @return TRUE if the string contains a Domain Name (rather than IP Addr)
*/
extern boolean is_domain_name(char *name_str);

/**
   Validate an IP host address
   @param[in] addr_str pointer to string containing address to be validated
   @param[in] addr pointer to 32-bit memory for returning valid IP address,
   in network byte-order, if this function returns TRUE
   @return TRUE if the address in addr_str is valid, FALSE otherwise
*/
extern boolean is_valid_ip_host_addr(char *addr_str, uint32_t *addr);

/**
   Validate an IP address including multicast address range
   @param[in] addr_str pointer to string containing address to be validated
   @return TRUE if the address in addr_str is valid, FALSE otherwise
*/
extern boolean is_valid_ip_address(const char *addr_str);

/**
   Validate a TCP (or UDP) port number
   @param[in] port_str pointer to string containing port to be validated
   @param[in] addr pointer to 16-bit memory for returning valid port,
   in network byte-order, if this function returns TRUE
   @return TRUE if the port in port_str is valid, FALSE otherwise
*/
extern boolean is_valid_port(char *port_str, uint16_t *port);


/**
   Given an uint32_t IP address (in network order), return a dot separated 
   IP address string.  Note both ipaddrtype and in_addr_t are uint32_t type.
   @param[in] h_addr   an uint32_t IP address in network order
   @param[in/out] bug  caller allocated buffer
   @param[in] buf_len  the allocated buffer length
   @return ptr to the buf
*/
static inline
char *uint32_ntoa_r (uint32_t addr, char *buf, int buf_len)
{
    struct in_addr saddr;

    saddr.s_addr = addr;
    return ((char *)inet_ntop(AF_INET, &saddr, buf, buf_len));
}

/**
   Given an uint32_t IP address in host order, return a dot separated 
   IP address string.
   @param[in] h_addr   an uint32_t IP address in host order
   @param[in/out] bug  caller allocated buffer
   @param[in] buf_len  the allocated buffer length
   @return ptr to the buf
*/
static inline
char *uint32_htoa_r (uint32_t h_addr, char *buf, int buf_len)
{
    return (uint32_ntoa_r(htonl(h_addr), buf, buf_len));
}

#endif /* VAM_UTIL_H */
