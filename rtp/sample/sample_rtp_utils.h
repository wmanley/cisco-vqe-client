/*------------------------------------------------------------------------ 
 * sample_rtp_utils.h -- Utility definitions for the RTP sample application.
 *
 * Copyright (c) 2001, 2003-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *-----------------------------------------------------------------------
 * $Log:$
 *-----------------------------------------------------------------------
 * $Endlog$
 */

#ifndef __rtp_sample_utils_h__

#include <sys/socket.h>
#include <netinet/in.h>

#include "utils/vam_util.h"

#include "../rtp.h"
#include "../rtcp.h"
#include "../rtp_util.h"
#include "../fec_fast_type.h"

int sample_socket_setup(ipaddrtype local_addr,
                        uint16_t local_port);
boolean sample_mcast_input_setup(int fd,
                                 ipaddrtype listen_addr,
                                 ipaddrtype if_addr);
boolean sample_mcast_output_setup(int fd,
                                  ipaddrtype send_addr,
                                  ipaddrtype if_addr);
boolean sample_socket_read(int sock,
                           abs_time_t * rcv_ts,
                           void * buf,
                           uint32_t * buf_len,
                           struct in_addr *src_addr,
                           uint16_t * src_port);
void rtp_print_envelope(uint32_t len,
                        struct in_addr addr, uint16_t port,
                        abs_time_t rcv_ts);
void rtp_print_header(rtptype *rtp, uint32_t len);
void rtp_print_fec(fecfasttype_t *fec, uint32_t len);

void rtcp_print_envelope(uint32_t len,
                         struct in_addr addr, uint16_t port,
                         abs_time_t rcv_ts);
void rtcp_print_packet(rtcptype *rtcp, uint32_t len);

#endif  /* ifndef __rtp_sample_utils_h__ */
