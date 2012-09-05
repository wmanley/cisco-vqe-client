/**-----------------------------------------------------------------
 * @brief
 * Packet building utility
 *
 * @file
 * raw_packet.h
 *
 * October 2007, Donghai Ma
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _RAW_PACKET_H_
#define _RAW_PACKET_H_

#include <netinet/ip.h>
#include <netinet/udp.h>

typedef enum raw_packet_error_ {
    RAW_PACKET_OK,
    RAW_PACKET_ERR_INVALIDARGS,
    RAW_PACKET_ERR_REQTOOSHORTIP,
    RAW_PACKET_ERR_REQTOOSHORTUDP,
    RAW_PACKET_ERR_NOTUDP,
    RAW_PACKET_ERR_UDPHEADERTOOLARGE,
    RAW_PACKET_ERR_INVALIDUDPCHKSUM,
} raw_packet_error_t;

/* Function:    udp_cksum
 * Description: To calculate UDP (same for TCP) checksum, a 12-byte 
 *              pseudo-header (phdr) is used in addition to the UDP datagram.
 * Parameters:  ip - ptr to an ip header
 *              up - ptr to an udp header plus the udp payload
 *              udp_len - udp length: header plus payload
 * Returns:     udp checksum
 * Side Effects: none
 */
extern uint16_t udp_cksum(const struct iphdr *ip,
                          const struct udphdr *up,
                          uint16_t udp_len);

/* Function: packet_frame_check
 * Description: Perform the following sanity check on the frame:
 *                 1. The frame has a complete IP header;
 *                 2. The frame has a UDP header;
 *                 3. UDP checksum is correct;
 *
 * Parameters:  frame - message frame
 *              length - message length
 *              no_udpcsum_checking - do not check UDP checksum if TRUE
 * Returns:     SS_OK if passed the check; otherwise an error code is returned
 */
extern raw_packet_error_t
packet_frame_check(const uint8_t *frame, const uint16_t length, 
                   boolean no_udpcsum_checking);

#endif /* _RAW_PACKET_H_ */
