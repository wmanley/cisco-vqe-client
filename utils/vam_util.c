/*
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include "utils/vam_types.h"
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/sockios.h>
#include <ctype.h>
#include "utils/vam_util.h"
#include "utils/strl.h"

#define SYSLOG_DEFINITION
#include <log/vqe_utils_syslog_def.h>
#undef SYSLOG_DEFINITION

#define IPADDR(b0,b1,b2,b3) (htonl((b0 << 24) | (b1 << 16)| (b2 << 8) | b3))

typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned char u8;

/**
   Get ip address from interface name.
   @param[in] ifname interface name like "eth0" or "eth1"
   @param[in] name buffer to hold the if name
   @param[in] size size of the name buffer, has to be greater than INET_ADDRSTRLEN
   @return TRUE if success, FALSE if failed.
*/
boolean get_ip_address_by_if ( char * ifname, char * name, int size ) {
    struct ifreq ifr;
    int sock;
    
    if ( size < INET_ADDRSTRLEN ) {
        return FALSE;
    }

    memset( name, 0, size );

    sock = socket( AF_INET, SOCK_DGRAM, 0);
    
    /* Get the interface IP address */ 
    
    (void)strlcpy( ifr.ifr_name, ifname , IF_NAMESIZE);
    ifr.ifr_addr.sa_family = AF_INET;
    
    if (ioctl( sock, SIOCGIFADDR, &ifr ) < 0) {
        close( sock );
        return FALSE;
    }

    close( sock );
    
    if ( inet_ntop( 
        AF_INET, 
        &((((struct sockaddr_in *)(&ifr.ifr_addr))->sin_addr).s_addr),
        name, size ) == NULL ) {
        return FALSE;
    } else {
        return TRUE;
    }
}

/**
   Get mac address from interface name.
   @param[in] ifname interface name like "eth0" or "eth1"
   @param[out] mac pointer of char to hold return mac address.
   @param[in] max_len maximum size of mac storage
   @return TRUE or FALSE for success or not.
*/
#define VAM_MAC_FORMAT_LEN 18
boolean get_mac_address_by_if (char * ifname,
                               char * mac,
                               uint32_t max_len)
{

    int sock;
    struct ifreq ifr;

    if (max_len < VAM_MAC_FORMAT_LEN) {
        return FALSE;
    }

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        return FALSE;
    }

    ifr.ifr_addr.sa_family = AF_INET;
    (void)strlcpy(ifr.ifr_name, ifname, IF_NAMESIZE);
    
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        close(sock);
        return FALSE;
    }
    
    close(sock);
    
    snprintf(mac, max_len-1,
             "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
             (unsigned char)ifr.ifr_hwaddr.sa_data[0],
             (unsigned char)ifr.ifr_hwaddr.sa_data[1],
             (unsigned char)ifr.ifr_hwaddr.sa_data[2],
             (unsigned char)ifr.ifr_hwaddr.sa_data[3],
             (unsigned char)ifr.ifr_hwaddr.sa_data[4],
             (unsigned char)ifr.ifr_hwaddr.sa_data[5]);
    mac[max_len-1] = '\0'; 

    return TRUE;
}

/**
   Get all interfaces in a box
   @param[in] ifc pointer of struct ifconf. Caller must set up 
   ifc.ifc_len = sizeof(buf);
   ifc.ifc_buf = buf;
   @return TRUE or FALSE for success or not.
*/
boolean get_all_interfaces ( struct ifconf * ifc ) {
    int           sock;

    /* Get a socket handle. */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) {
        return FALSE;
    }

    if(ioctl(sock, SIOCGIFCONF, ifc) < 0) {
        close( sock );
        return FALSE;
    }

    close( sock );
    return TRUE;
}

/**
   Get the mac address of the first non loopback (lo)
   interface on the box.
   Note the loopback interface lo has a mac
   address of 0.0.0.0.0.0.
   @param[out] mac pointer of char to hold return mac address.
   @param[in] max_len  maximum size of mac to return.
   @return TRUE or FALSE for success or not.
*/
#define VAM_UTIL_BUF_LEN 1024
boolean get_first_mac_address (char * mac, uint32_t max_len)
{
    char          buf[VAM_UTIL_BUF_LEN];
    struct ifconf ifc;
    struct ifreq *ifr;
    int           nInterfaces;
    int i;
    struct ifreq *item = NULL;

    if (!mac) {
        return FALSE;
    }
        
    /* Query available interfaces. */
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if(!get_all_interfaces( &ifc )) {
        return FALSE;
    }
    
    /* Iterate through the list of interfaces. */
    ifr         = ifc.ifc_req;
    nInterfaces = ifc.ifc_len / sizeof(struct ifreq);

    for (i = 0; i < nInterfaces; i++) {
        if (strncmp((&ifr[i])->ifr_name, "lo", 2) != 0) {
            item = &ifr[i];
            break;
        }
    }
                
    if (item && get_mac_address_by_if(item->ifr_name,
                                      mac,
                                      max_len)) {
        return TRUE;
    } else {
        return FALSE;
    }    
}


/**
   Determine whether string contains Domain Name (vs. IP Address)
   @param[in] name_str pointer to string containing Domain Name or IP Addr
   @return TRUE if name_str contains a Domain Name (as opposed to an
       IP Address in dotted decimal notation.  We use a very simple
       algorithm:  if the string contains a letter (upper or lower
       case), we assume it is a Domain Name - else, we assume it is
       an IP Address.  This is the simplest thing we could do, and
       probably sufficient for most (if not all) cases.
*/
boolean is_domain_name (char *name_str)
{
    int     i;
    int     slen;
    boolean found_alpha = FALSE;

    if (name_str == NULL) {
	return (FALSE);
    }

    slen = strlen(name_str);

    /*
     * If contains a letter, assume it's a Domain Name (rather than dotted 
     * decimal IP address)
     */
    for (i = 0; i < slen; i++) {

	if (isalpha(name_str[i])) {
	    found_alpha = TRUE;
	    break;
	}
    }

    return (found_alpha);
}


#define IP_ADDR_PARTS  4  /* IP addr has 4 'parts' (#s between the dots) */

/**
   Validate an IP host address
   @param[in] addr_str pointer to string containing address to be validated
   @param[in] addr pointer to 32-bit memory for returning valid IP address,
   in network byte-order, if this function returns TRUE
   @return TRUE if the address in addr_str is valid, FALSE otherwise
*/
boolean is_valid_ip_host_addr (char *addr_str, uint32_t *addr)
{
    uint32_t d[IP_ADDR_PARTS];
    uint32_t temp_addr; 
    int      i;
    /*
     * Validate input args
     */
    if ((addr == NULL) || (addr_str == NULL)) {
	return (FALSE);
    }

    if (is_valid_ip_address(addr_str)) {
        i = sscanf(addr_str, "%u.%u.%u.%u", &d[0], &d[1], &d[2], &d[3]);
        if ( i < 4) {
            /* Something is really wrong because we have already check */
            /* this condition */
            return (FALSE);
        }

        /*
         * Cannot be in multicast range (Class D), experimental or bcast
         * (Class E).  These ranges are 
         *    multicast:    [224.0.0.0 - 239.255.255.255] Class D
         *    experimental: [240.0.0.0 - 254.255.255.255] Class E
         *    broadcast:    [255.0.0.0 - 255.255.255.255] Class E
         */
        if (d[0] > 223) {
            return (FALSE);            
        }

        temp_addr = d[0] << 24 | d[1] << 16 | d[2] << 8 | d[3];
        
        if (temp_addr == INADDR_NONE) {
            return (FALSE);            
        }

        if (temp_addr == INADDR_ANY) {
            return (FALSE);            
        }

        /* 
         * Return IP Address, in network byte-order
         */ 
        *addr = htonl(temp_addr);

        return (TRUE);
    }
    else {
        return (FALSE);
    }
}

/**
   Validate a TCP (or UDP) port number
   @param[in] port_str pointer to string containing port to be validated
   @param[in] addr pointer to 16-bit memory for returning valid port,
   in network byte-order, if this function returns TRUE
   @return TRUE if the port in port_str is valid, FALSE otherwise
*/
boolean is_valid_port (char *port_str, uint16_t *port)
{
    uint32_t temp_long;
    uint16_t temp_port;
    int      slen;

    /*
     * Validate input args
     */
    if ((!port_str) || (!port)) {
	return (FALSE);
    }

    /*
     * Longest valid Port string is 5 chars ("65535")
     */
    slen = strlen(port_str);

    if (slen > MAX_PORT_LEN) {
	goto bad_port;
    }

    temp_long = atol(port_str);

    if (temp_long > 0xffff) {
	goto bad_port;
    }

    temp_port = (uint16_t)temp_long;

    temp_port = htons(temp_port);

    if (temp_port == 0) {
	goto bad_port;
    }

    /* 
     * Return port number, in network byte-order
     */ 
    *port = temp_port;

    return (TRUE);

 bad_port:

    return (FALSE);

}


/**
   Validate an IP host address
   @param[in] addr_str pointer to string containing address to be validated
   @return TRUE if the address in addr_str is valid, FALSE otherwise
*/
boolean is_valid_ip_address (const char *addr_str)
{
    uint32_t d[IP_ADDR_PARTS];
    int      i;
    int      slen;
    int      num_dots = 0;
    char     temp_str[MAX_IP_ADDR_STR];
    char    *ptr;
    char    *saveptr;
    /*
     * Validate input args
     */
    if (addr_str == NULL) {
	return (FALSE);
    }

    /*
     * Longest IP Addr string is 16 chars (xxx.xxx.xxx.xxx\0)
     */
    slen = strlen(addr_str);

    if (slen > MAX_IP_ADDR_LEN) {
	goto bad_ip_addr;
    }

    /*
     * Must contain only digits or dots "."
     */
    for (i = 0; i < slen; i++) {

	if (!isdigit(addr_str[i]) && addr_str[i] != '.') {
	    goto bad_ip_addr;
	}
	if (addr_str[i] == '.') {
	    num_dots++;
	}
    }

    /*
     * Must contain exactly 3 dots
     */
    if (num_dots != 3) {
	goto bad_ip_addr;
    }
    

    /*
     * No more than 3 digits between dots
     */
    strncpy(temp_str, addr_str, MAX_IP_ADDR_STR);
    i = 0;
    ptr = strtok_r(temp_str, ".", &saveptr);

    while (ptr) {
	ptr = strtok_r(NULL, ".", &saveptr);
	i++;
	if (i > 4) {
	    goto bad_ip_addr;
	}
    }

    /*
     * ??
     */
    i = sscanf(addr_str, "%u.%u.%u.%u", &d[0], &d[1], &d[2], &d[3]);
    if ( i < 4) {
	goto bad_ip_addr;
    }


    /*
     * digits between dots cannot be greater than 255
     */
    for ( i = 0; i < 4; i++) {
        if (d[i] > 255) {
            goto bad_ip_addr;
	}
    }

    /*
     * 1st digit cannot be zero
     */
    if (d[0] == 0) {
	goto bad_ip_addr;
    }

    return (TRUE);

 bad_ip_addr:

    return (FALSE);

}
