/**-----------------------------------------------------------------
 * @brief
 * BSD Packet Filter
 *
 * @file
 * socket_filter.c
 *
 * October 2007, Donghai Ma
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/filter.h>

/*           
 *             About the STUN binding request packet filter
 *
 * As suggested in STUN rfc draft-ietf-behave-rfc3489bis, we use the 
 * following BSD Packet Filter to sniff the possible STUN binding requests:
 *    "(udp[8]=0) && (udp[10:2]&0x3=0) && (udp[12:4]=0x2112a442)"
 *
 * Translated in English, the filter is checking for:
 *   1. The most significant two bits of every STUN message are zeros. For
 *      STUN binding request packets, the first octet is always 0x0.
 *   2. Last two bits of the Message Length field are always zero since
 *      the length of a STUN message is always multiples of four (4).
 *   3. The magic cookie field is 0x2112A442 in the fixed length STUN header.
 *
 * When attaching a filter to a socket, the filter needs to compiled into
 * BPF codes. tcpdump has options '-d' and '-dd' that do both compilation 
 * and optimization for filters working for layter 2 data (e.g. data received
 * by PF_PACKET socket). Since we are using a UDP raw socket (i.e. filter needs
 * to work on layer 3 data), we will need to subtract the 14-byte ethernet 
 * header from the tcpdump-generated filter codes.
 */

/* Function: attach_stun_request_filter
 * Description: Attach the STUN request BPF to a socket 
 * Parameters: sock_fd - descriptor for the socket to attach the STUN Request
 *                       Packet Filter
 * Returns:    setsockopt() return code
 * Side Effects: none
 */
int attach_stun_request_filter (int sock_fd)
{
    struct sock_fprog filter;

#if 0
# tcpdump -s 1514 -d "(not ip multicast) && (udp[8]=0) && (udp[10:2]&0x3=0) && (udp[12:4]=0x2112a442)"
    (000) ldh      [12]
    (001) jeq      #0x800           jt 2    jf 16
    (002) ldb      [30]
    (003) jge      #0xe0            jt 16   jf 4
    (004) ldb      [23]
    (005) jeq      #0x11            jt 6    jf 16
    (006) ldh      [20]
    (007) jset     #0x1fff          jt 16   jf 8
    (008) ldxb     4*([14]&0xf)
    (009) ldb      [x + 22]
    (010) jeq      #0x0             jt 11   jf 16
    (011) ldh      [x + 24]
    (012) jset     #0x3             jt 16   jf 13
    (013) ld       [x + 26]
    (014) jeq      #0x2112a442      jt 15   jf 16
    (015) ret      #1514
    (016) ret      #0

# tcpdump -s 1514 -dd "(not ip multicast) && (udp[8]=0) && (udp[10:2]&0x3=0) && (udp[12:4]=0x2112a442)"
    { 0x28, 0, 0, 0x0000000c },
    { 0x15, 0, 14, 0x00000800 },
    { 0x30, 0, 0, 0x0000001e },
    { 0x35, 12, 0, 0x000000e0 },
    { 0x30, 0, 0, 0x00000017 },
    { 0x15, 0, 10, 0x00000011 },
    { 0x28, 0, 0, 0x00000014 },
    { 0x45, 8, 0, 0x00001fff },
    { 0xb1, 0, 0, 0x0000000e },
    { 0x50, 0, 0, 0x00000016 },
    { 0x15, 0, 5, 0x00000000 },
    { 0x48, 0, 0, 0x00000018 },
    { 0x45, 3, 0, 0x00000003 },
    { 0x40, 0, 0, 0x0000001a },
    { 0x15, 0, 1, 0x2112a442 },
    { 0x6, 0, 0, 0x000005ea },
    { 0x6, 0, 0, 0x00000000 },

    /* Modify the filter: no need to check if it is IP or UDP here since we
     * are receiving via a raw UDP socket.  Also need to subtract the
     * 14-byte ethernet header from the tcpdump-generated filter codes.
     */
    (000) ldb      [16]
    (001) jge      #0xe0            jt 12   jf 2
    (002) ldh      [6]
    (003) jset     #0x1fff          jt 12   jf 4
    (004) ldxb     4*([0]&0xf)
    (005) ldb      [x + 8]
    (006) jeq      #0x0             jt 7   jf 12
    (007) ldh      [x + 10]
    (008) jset     #0x3             jt 12   jf 9
    (009) ld       [x + 12]
    (010) jeq      #0x2112a442      jt 11   jf 12
    (011) ret      #1514
    (012) ret      #0
#endif

    struct sock_filter BPF_code[]= { 
        { 0x30, 0, 0, 0x00000010 },
        { 0x35, 10, 0, 0x000000e0 },
        { 0x28, 0, 0, 0x00000006 },
        { 0x45, 8, 0, 0x00001fff },
        { 0xb1, 0, 0, 0x00000000 },
        { 0x50, 0, 0, 0x00000008 },
        { 0x15, 0, 5, 0x00000000 },
        { 0x48, 0, 0, 0x0000000a },
        { 0x45, 3, 0, 0x00000003 },
        { 0x40, 0, 0, 0x0000000c },
        { 0x15, 0, 1, 0x2112a442 },
        { 0x6, 0, 0, 0x000005ea },
        { 0x6, 0, 0, 0x00000000 },
    };

    filter.len = sizeof(BPF_code)/sizeof(BPF_code[0]);
    filter.filter = BPF_code;

    return setsockopt(sock_fd, SOL_SOCKET, SO_ATTACH_FILTER, 
                      &filter, sizeof(filter));
}

/*
 * About the STUN Binding Response Filter
 *
 * The only difference between the request and response filter is that the
 * first octet of a response message is 0x01, whereas for the request it
 * is 0x00.  The filter in the following function reflects this variation, and
 * thus should be used to filter STUN Binding Response messages.
 */

/* Function: attach_stun_response_filter
 * Description: Attach the STUN response BPF to a socket 
 * Parameters: sock_fd - descriptor for the socket to attach the STUN Response
 *                       Packet Filter
 * Returns:    setsockopt() return code
 * Side Effects: none
 */
int attach_stun_response_filter (int sock_fd)
{
    struct sock_fprog filter;

    struct sock_filter BPF_code[]= { 
        { 0x30, 0, 0, 0x00000010 },
        { 0x35, 10, 0, 0x000000e0 },
        { 0x28, 0, 0, 0x00000006 },
        { 0x45, 8, 0, 0x00001fff },
        { 0xb1, 0, 0, 0x00000000 },
        { 0x50, 0, 0, 0x00000008 },
        { 0x15, 0, 5, 0x00000001 },
        { 0x48, 0, 0, 0x0000000a },
        { 0x45, 3, 0, 0x00000003 },
        { 0x40, 0, 0, 0x0000000c },
        { 0x15, 0, 1, 0x2112a442 },
        { 0x6, 0, 0, 0x000005ea },
        { 0x6, 0, 0, 0x00000000 },
    };

    filter.len = sizeof(BPF_code)/sizeof(BPF_code[0]);
    filter.filter = BPF_code;

    return setsockopt(sock_fd, SOL_SOCKET, SO_ATTACH_FILTER, 
                      &filter, sizeof(filter));
}


/* 
 * Create a BPF for IGMPv1, IGMPv2 and IGMPv3 snooping
 * Proxy-igmp does not support IGMPv1 but we will enable sniffing for
 * IGPMv1 so that we can at least log that IGMPv1 was received. 
 *
 *
 *
 *
 * kanjoshi-3.cisco.com:4> tcpdump -i eth1 -d '(ip[9:1]=0x02) && 
 *                                             ((ip[((ip[0:1] & 0x0F) << 2):1]
 *                                             =0x17) ||
 *                                             (ip[((ip[0:1] & 0x0F)<< 2):1]
 *                                             =0x16) ||
 *                                             (ip[((ip[0:1] & 0x0F)<< 2):1]
 *                                             =0x22) ||(ip[((ip[0:1] & 0x0F)
 *                                             << 2):1]=0x12)) && ip[12:4]
 *                                             =0xc0a8030b (stb_ip_address)'
 *                                             
 *                                             
 * (000) ldh      [12]
 * (001) jeq      #0x800           jt 2    jf 16
 * (002) ldb      [23]
 * (003) jeq      #0x2             jt 4    jf 16
 * (004) ldb      [14]
 * (005) and      #0xf
 * (006) lsh      #2
 * (007) tax
 * (008) ldb      [x + 14]
 * (009) jeq      #0x17            jt 13   jf 10
 * (010) jeq      #0x16            jt 13   jf 11
 * (011) jeq      #0x22            jt 13   jf 12
 * (012) jeq      #0x12            jt 13   jf 16
 * (013) ld       [26]
 * (014) jeq      #0xc0a8030b      jt 15   jf 16
 * (015) ret      #96
 * (016) ret      #0
 *
 *
 * { 0x28, 0, 0, 0x0000000c },
 * { 0x15, 0, 14, 0x00000800 },
 * { 0x30, 0, 0, 0x00000017 },
 * { 0x15, 0, 12, 0x00000002 },
 * { 0x30, 0, 0, 0x0000000e },
 * { 0x54, 0, 0, 0x0000000f },
 * { 0x64, 0, 0, 0x00000002 },
 * { 0x7, 0, 0, 0x00000005 },
 * { 0x50, 0, 0, 0x0000000e },
 * { 0x15, 3, 0, 0x00000017 },
 * { 0x15, 2, 0, 0x00000016 },
 * { 0x15, 1, 0, 0x00000022 },
 * { 0x15, 0, 3, 0x00000012 },
 * { 0x20, 0, 0, 0x0000001a },
 * { 0x15, 0, 1, 0xc0a8030b },
 * { 0x6, 0, 0, 0x00000060 },
 * { 0x6, 0, 0, 0x00000000 },
 */
int attach_igmp_filter (int sock_fd, unsigned int stb_ip_addr) 
{
    struct sock_fprog filter;
    struct sock_filter BPF_code[] = {
        { 0x28, 0, 0, 0x0000000c },
        { 0x15, 0, 14, 0x00000800 },
        { 0x30, 0, 0, 0x00000017 },
        { 0x15, 0, 12, 0x00000002 },
        { 0x30, 0, 0, 0x0000000e },
        { 0x54, 0, 0, 0x0000000f },
        { 0x64, 0, 0, 0x00000002 },
        { 0x7, 0, 0, 0x00000005 },
        { 0x50, 0, 0, 0x0000000e },
        { 0x15, 3, 0, 0x00000017 },
        { 0x15, 2, 0, 0x00000016 },
        { 0x15, 1, 0, 0x00000022 },
        { 0x15, 0, 3, 0x00000012 },
        { 0x20, 0, 0, 0x0000001a },
        { 0x15, 0, 1, stb_ip_addr },
        { 0x6, 0, 0, 0x00000060 },
        { 0x6, 0, 0, 0x00000000 },
    };
 
    filter.len = sizeof(BPF_code)/sizeof(BPF_code[0]);
    filter.filter = BPF_code;

    return setsockopt(sock_fd, SOL_SOCKET, SO_ATTACH_FILTER, 
                      &filter, sizeof(filter));
}
