/*
 * Copyright (c) 2006-2007 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "../include/utils/vam_util.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include "../include/utils/vam_types.h"
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/ether.h>

#define MAC_LEN 18
int main ( int argc, char ** argv ) {

    char mac[MAC_LEN];
    char          buf[1024];
    struct ifconf ifc;
    struct ifreq *ifr;
    int           nInterfaces;
    int           i;
        
    /* Query available interfaces. */
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if(!get_all_interfaces( &ifc )) {
        return -1;
    }
    
    /* Iterate through the list of interfaces. */
    ifr         = ifc.ifc_req;
    nInterfaces = ifc.ifc_len / sizeof(struct ifreq);
    for(i = 0; i < nInterfaces; i++) {
        struct ifreq *item = &ifr[i];
        
	/* Show the device name and IP address */
        printf("%s: IP %s",
               item->ifr_name,
               inet_ntoa(((struct sockaddr_in *)&item->ifr_addr)->sin_addr));
        
        if (get_mac_address_by_if( item->ifr_name, mac, MAC_LEN)) {
            printf(", MAC %s\n", mac);        
        }
    }
    
    return 0;    
}

