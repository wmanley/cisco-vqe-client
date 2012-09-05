/**-----------------------------------------------------------------
 * @brief
 * VQE-S STUN Server.  Packet building utility
 *
 * @file
 * ss_packet.c
 *
 * October 2007, Donghai Ma
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include <string.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include "utils/vam_types.h"
#include "utils/vam_util.h"

#include "raw_packet.h"


/*
 * Copyright Notice
 * The two checksum related routines in_cksum() and udp_cksum() are copied
 * from tcpdump with minor changes. tcpdump uses BSD license and has the 
 * following copyright notice:
 *
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *      The Regents of the University of California.  All rights reserved.
 */

/*
 * compute an IP header checksum.
 * don't modifiy the packet.
 */
static uint16_t in_cksum (const uint16_t *addr, uint16_t len, int csum)
{
    int nleft = len;
    const uint16_t *w = addr;
    uint16_t answer;
    int sum = csum;

    /*
     *  Our algorithm is simple, using a 32 bit accumulator (sum),
     *  we add sequential 16 bit words to it, and at the end, fold
     *  back all the carry bits from the top 16 bits into the lower
     *  16 bits.
     */
    while (nleft > 1)  {
        sum += *w++;
        nleft -= 2;
    }
    if (nleft == 1)
        sum += htons(*(u_char *)w<<8);

    /*
     * add back carry outs from top 16 bits to low 16 bits
     */
    sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
    sum += (sum >> 16);			/* add carry */
    answer = ~sum;			/* truncate to 16 bits */
    return (answer);
}

/* Function:    udp_cksum
 * Description: To calculate UDP (same for TCP) checksum, a 12-byte 
 *              pseudo-header (phdr) is used in addition to the UDP datagram.
 * Parameters:  ip - ptr to an ip header
 *              up - ptr to an udp header plus the udp payload
 *              udp_len - udp length: header plus payload
 * Returns:     udp checksum
 * Side Effects: none
 */
uint16_t udp_cksum (const struct iphdr *ip,
                    const struct udphdr *up, 
                    uint16_t udp_len)
{
    union phu {
        struct phdr {
            uint32_t src;
            uint32_t dst;
            uint8_t mbz;
            uint8_t proto;
            uint16_t len;
        } ph;
#define PSEUDO_HDR_LENGTH_IN_WORD 6
        uint16_t pa[PSEUDO_HDR_LENGTH_IN_WORD];
    } phu;
    const uint16_t *sp;

    /* pseudo-header.. */
    phu.ph.len = htons(udp_len);
    phu.ph.mbz = 0;
    phu.ph.proto = IPPROTO_UDP;
    memcpy(&phu.ph.src, &ip->saddr, sizeof(uint32_t));

    /* Original tcpdump code checks for possible ip options (source routing
     * for example) before setting phu.ph.dst. When an IP header does not
     * contain any IP option, i.e. if (if->ihl == 5), we can simply copy the 
     * destination address over from ip->daddr.
     * 
     * Here we choose only compute udp chksum for packets without IP options 
     * in the IP headers for reasons: 1. we expect all STUN requests from 
     * clients will not have IP options; and 2. the code to handle IP options
     * will be hard to test if ported. 
     *
     * Note we return 0 when we see IP options in the IP header. 
     * When this funtion is called to validate a UDP chksum for a
     * incoming packet, returning 0 suggests the checking has passed.  When
     * this is called to generate a UDP chksum for an outgoing packet, 
     * returning 0 suggests we as the sender don't calculate UDP cksum and ask
     * the receiver not to perform UDP checksum check.
     * 
     * For our STUN server, we don't set any IP option in the IP headers of 
     * the STUN response packets. So we always generate UDP chksums for the 
     * STUN response packets. As for the incoming STUN request packets, we
     * don't expect to see ip options in most of the request packets. To 
     * validate this assumption we have a counter to count the number of 
     * the requests that have IP options in their IP headers, i.e. 
     * when ip->ihl != 5.
     */
    if (ip->ihl == 5) {
        memcpy(&phu.ph.dst, &ip->daddr, sizeof(uint32_t));
    } else {
        return 0; 
    }
    
    sp = &phu.pa[0];
    return in_cksum((uint16_t *)up, udp_len,
                    sp[0]+sp[1]+sp[2]+sp[3]+sp[4]+sp[5]);
}


/* Function: packet_frame_check
 * Description: Perform the following sanity check on the potential STUN 
 *              request frame:
 *                 1. The frame has a complete IP header;
 *                 2. The frame has a UDP header;
 *                 3. UDP checksum is correct;
 *
 * Parameters:  frame - message frame
 *              length - message length
 *              no_udpcsum_checking - do not check UDP checksum if TRUE
 * Returns:     RAW_PACKET_OK if passed the check; error code otherwise
 */
raw_packet_error_t
packet_frame_check (const uint8_t *frame, const uint16_t length,
                    boolean no_udpcsum_checking)
{
    struct iphdr *ip_header = NULL;
    struct udphdr *udp_header = NULL;
    uint8_t iphdr_len = 0;  /* variable length depending on ->ihl */
    uint8_t udphdr_len = sizeof(struct udphdr);
    uint16_t udp_csum;
    
    uint8_t *ptr = (uint8_t*)frame;
    int len = length;

    if (!frame || !length) {
        return RAW_PACKET_ERR_INVALIDARGS;
    }

    /* Make sure that the frame is large enough to at least hold the minimum
     * fixed ip header strut
     */
    if (len < sizeof(struct iphdr)) {
        return RAW_PACKET_ERR_REQTOOSHORTIP;
    }

    ip_header = (struct iphdr*)ptr;

    if(ip_header->protocol != IPPROTO_UDP) {
        return RAW_PACKET_ERR_NOTUDP;
    }
    
    /* Make sure it contains the whole ip header, which may be longer than
     * 20 bytes if it contains ip options.
     */
    iphdr_len = ip_header->ihl*4;
    if (len < iphdr_len) {
        return RAW_PACKET_ERR_REQTOOSHORTIP;
    }
    ptr += iphdr_len;
    len -= iphdr_len;
    
    /* Check if we are large enough to contains a udp header */
    if (len < udphdr_len) {
        return RAW_PACKET_ERR_REQTOOSHORTUDP;
    }
    
    udp_header = (struct udphdr*)ptr;
    
    /* Make sure the udp_header->len is not exceeding the frame */
    if (ntohs(udp_header->len) > len) {
        return RAW_PACKET_ERR_UDPHEADERTOOLARGE;
    }
    
    ptr += udphdr_len;
    len -= udphdr_len;
    
    /* Check the udp checksum */
    if (!no_udpcsum_checking) {
        if (udp_header->check) {
            udp_csum = udp_cksum(ip_header, udp_header, 
                                 ntohs(udp_header->len));
            if (udp_csum) {
                return RAW_PACKET_ERR_INVALIDUDPCHKSUM;
            }
        }
    }

    return RAW_PACKET_OK;
}
