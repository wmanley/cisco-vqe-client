/*------------------------------------------------------------------
 * sample_rtp_utils.c -- Utilities for the RTP/RTCP sample application.
 *
 * December 2006, Mike Lague
 *
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "../include/utils/vam_util.h"
#include "../rtp.h"
#include "../rtcp.h"
#include "../rtp_util.h"
#include "rtp_exp_api.h"
#include "sample_rtp_utils.h"

#define RTP_BUGINF printf
#define TIMEBUF_LEN 80
#define BINARY_SIZE 16

static char *convert2binary(uint16_t data)
{
    static char binary[BINARY_SIZE];
    int i = 0;

    memset(binary, 0, BINARY_SIZE);
    data = data << 1;
    while(i != BINARY_SIZE-1) {
        if ((data & 0x8000) == 0) {
            binary[i] = '0';
        } else {
            binary[i] = '1';
        }

        i++;
        data = data << 1;
    }

    return binary;
}

/*
 * sample_socket_setup
 *
 * Five-step socket setup, for unicast connections:
 * 1. create a datagram socket via socket()
 * 2. set socket option SO_REUSEADDR via setsockopt()
 * 3. bind to local addr/port via bind()
 * 4. set SO_TIMESTAMP option via setsockopt()
 * 5. set O_NONBLOCK via fcntl
 *
 * Parameters: (these should all be in NETWORK order):
 * local_addr
 * local_port
 */

int sample_socket_setup (ipaddrtype local_addr,
                         uint16_t local_port)
{
    int fd;
    int on = 1;
    struct in_addr addr;
    struct sockaddr_in saddr;

    if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        return -1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, 
                   &on, sizeof(on)) == -1) {
        perror("setsockopt");
        goto bail;
    }

    saddr.sin_family = AF_INET;
    addr.s_addr = local_addr;
    saddr.sin_addr = addr;
    saddr.sin_port = local_port;    
    if (bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
        perror("bind");
        goto bail;
    }

    if (setsockopt(fd, SOL_SOCKET, 
                   SO_TIMESTAMP, &on, sizeof(on)) == -1) {
        perror("setsockopt:SO_TIMESTAMP");
        goto bail;
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        goto bail;
    }

    return(fd);

bail:
    close(fd);
    return(-1);
}

/*
 * sample_mcast_input_setup
 *
 * Setup a socket from input from a multicast stream: join a multicast group.
 *
 * Parameters:
 * fd          -- socket file descriptor: the fd should be "bound" to the
 *               multicast address.
 * listen_addr -- multicast address (in network order)
 * if_addr     -- address of local interface, on which to join the mcast group
 */
boolean sample_mcast_input_setup (int fd,
                                  ipaddrtype listen_addr,
                                  ipaddrtype if_addr)
{
    struct ip_mreq mreq;

    if (IN_MULTICAST(ntohl(listen_addr))) {
        mreq.imr_multiaddr.s_addr = listen_addr;
        mreq.imr_interface.s_addr = if_addr;

        if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                       &mreq, sizeof(mreq)) < 0) {
            perror("mcast setsockopt");
            return(FALSE);
        }
    } 
    return (TRUE);
}

/*
 * sample_mcast_output_setup
 *
 * Setup a socket for output to a multicast group.
 *
 * Parameters:
 * fd          -- socket file descriptor: the fd should be "bound" to the
 *               multicast address.
 * send_addr   -- multicast address (in network order)
 * if_addr     -- address of local interface, on which to send.
 */
boolean sample_mcast_output_setup (int fd,
                                   ipaddrtype send_addr,
                                   ipaddrtype if_addr)
{
    struct in_addr out_addr;
    int ttl = 5;

    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, 
		   &ttl, sizeof(ttl)) == -1) {
        perror("setsockopt IP_MULTICAST_TTL");
        return (FALSE);
    }

    out_addr.s_addr = if_addr;
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, 
		   &out_addr, sizeof(out_addr)) == -1) {
        perror("setsockopt IP_MULTICAST_IF");
        return (FALSE);
    }

    return (TRUE);
}

/*
 * sample_socket_read
 *
 * Read from a socket via recvmsg: 
 * get timestamp, as well as src_addr and src_port
 */
boolean sample_socket_read (int sock,
                            abs_time_t * rcv_ts,
                            void * buf,
                            uint32_t * buf_len,
                            struct in_addr *src_addr,
                            uint16_t * src_port)
{
#define MAX_CMSG_BUF_SIZE 1024 
   /*$$$ */

    struct msghdr   msg;
    struct sockaddr_in saddr;
    struct iovec vec;
    char ctl_buf[MAX_CMSG_BUF_SIZE];
    struct cmsghdr *cmsg;
    void * cmsg_data;
    int bytes_received;
    struct timeval rcv_time;

    vec.iov_base = buf;
    vec.iov_len = *buf_len;

    memset(&msg, 0, sizeof(msg));
 
    msg.msg_name = &saddr;
    msg.msg_namelen = sizeof(saddr);
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1;

    msg.msg_control = ctl_buf;
    msg.msg_controllen = sizeof(ctl_buf);
 
    if ((bytes_received = recvmsg(sock, &msg, 0)) == -1) {
        if (errno != EAGAIN) {
            perror("recvmsg");
            return FALSE;
        } else {
            return FALSE;
        }
    }

    *buf_len = bytes_received;

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {

        /*$$$ Simplify and make more efficient */

       if (cmsg->cmsg_level == SOL_SOCKET 
            && cmsg->cmsg_type == SO_TIMESTAMP) {
            
           cmsg_data = CMSG_DATA(cmsg);
            if (cmsg_data) {
                /*$$$ need an extra length check in here somehow? */
                memcpy(&rcv_time, cmsg_data, sizeof(rcv_time));
                *rcv_ts = TIME_MK_A(timeval,rcv_time);
            } else {
                ASSERT(0,"no data for SO_TIMESTAMP option\n");
            }

        } else {
            ASSERT(0,"Extra message: level=%d msg=%d\n", 
                   cmsg->cmsg_level,
                   cmsg->cmsg_type);
        }
    }
                   

    ASSERT(msg.msg_namelen == sizeof(saddr),
           "wrong size for source address\n");

//    ASSERT(msg.msg_flags == 0, "non zero msg flags %d\n", msg.msg_flags);

    *src_port = saddr.sin_port;
    *src_addr = saddr.sin_addr;

    return TRUE;

}    

/*
 *------------------------------------------------------------------
 * Debugging routines for RTP/RTCP 
 * (adapted from voip_rtp_ui.c)
 *------------------------------------------------------------------
 */

/*
 * print_envelope
 *
 * Prints information about a received RTP/RTCP packet
 */
static void 
print_envelope (char *protocol, uint32_t len, 
                struct in_addr addr, uint16_t port,
                abs_time_t rcv_ts)
{
    char timebuf[TIMEBUF_LEN];
    char str[INET_ADDRSTRLEN];

    RTP_BUGINF("RCVD %s pkt (%u bytes) from %s:%u\n",
               protocol, len, inaddr_ntoa_r(addr, str, sizeof(str)), ntohs(port));
    RTP_BUGINF("     at %s:\n",
               abs_time_to_str(rcv_ts, timebuf, sizeof(timebuf)));
}

/* 
 * rtp_print_envelope
 *
 * Prints information about a received RTP packet
 */
void 
rtp_print_envelope (uint32_t len,
                    struct in_addr addr, uint16_t port,
                    abs_time_t rcv_ts)
{
    print_envelope("RTP", len, addr, port, rcv_ts);
}

/*
 * rtp_print_header
 *
 * Prints RTP header
 */
void rtp_print_header (rtptype *rtp, uint32_t len)
{
    rtpfasttype_t *hdr = (rtpfasttype_t *)rtp;
    if (len < sizeof(rtptype)) {
        return;
    }
    RTP_BUGINF("RTP: v=%u p=%u x=%u cc=%u m=%u pt=%u seq=%u (0x%04x)\n"
               "ts=0x%08x ssrc=%u (0x%08x)\n",
               RTP_VERSION(hdr), RTP_PADDING(hdr), RTP_EXT(hdr),
               RTP_CSRCC(hdr), RTP_MARKER(hdr), RTP_PAYLOAD(hdr),
               ntohs(rtp->sequence), ntohs(rtp->sequence), 
               ntohl(rtp->timestamp), ntohl(rtp->ssrc), ntohl(rtp->ssrc));
}

/*
 * rtp_print_fec
 *
 * Prints FEC header
 */
void rtp_print_fec (fecfasttype_t *fec, uint32_t len)
{
    if (len < sizeof(fecfasttype_t)) {
        return;
    }
    RTP_BUGINF("FEC: snb=0x%04x lenr=%u e=%u ptr=%u mask=%u tsr=0x%08x\n"
               "x=%u d=%u type=%u idx=%u off=%u na=%u snbe=%u\n",
               ntohs(fec->snbase_low_bits),
               ntohs(fec->length_recovery),
               FEC_E_BIT(fec),
               FEC_PT_RECOVERY(fec),
               FEC_MASK(fec),
               ntohl(fec->ts_recovery),
               FEC_X_BIT(fec),
               FEC_D_BIT(fec),
               FEC_TYPE(fec),
               FEC_INDEX(fec),
               FEC_OFFSET(fec),
               FEC_NA(fec),
               FEC_SNBASE_EXT(fec));
}

/*
 * rtcp_print_rb
 *
 * Print RTCP reception report blocks
 */
static void
rtcp_print_rb (rtcp_rr_t *rr, uint8_t *end_of_frame)
{
    uint32_t loss;
    int32_t cum_loss;
    struct {
        int i:24;
    } loss24;

    while ((uint8_t *)rr < end_of_frame) {
        loss = ntohl(rr->rr_loss);
        /* cumulative loss is a signed 24-bit value */
        loss24.i = (loss & 0x00ffffff);
        cum_loss = loss24.i;
        RTP_BUGINF("    sndr_ssrc=0x%08x loss_frac=%u/256 cum_loss=%d\n"
                   "    ehsr=%d (0x%08x) jitter=%u\n"
                   "    lsr=0x%08x dlsr=0x%08x\n", 
                   ntohl(rr->rr_ssrc), 
                   (loss & 0xFF000000) >> 24, cum_loss,
                   ntohl(rr->rr_ehsr), ntohl(rr->rr_ehsr),
                   ntohl(rr->rr_jitter), 
                   ntohl(rr->rr_lsr), ntohl(rr->rr_dlsr));
        rr++;
    }   
}
/*
 * rtcp_print_rr
 *
 * Prints RTCP Receiver Report
 */
static void
rtcp_print_rr (rtcptype *rtcp, uint8_t *end_of_frame)
{
    rtcp_rr_t *rr = (rtcp_rr_t *)(rtcp + 1);

    RTP_BUGINF("RR: rcvr_ssrc=0x%08x\n", ntohl(rtcp->ssrc));
    rtcp_print_rb(rr, end_of_frame);
}

/*
 * rtcp_print_sr
 *
 * Prints RTCP Sender Report
 */
static void
rtcp_print_sr (rtcptype *rtcp, uint8_t *end_of_frame)
{
    rtcp_sr_t *sr;
    ntp64_t ntp;
    char timebuf[TIMEBUF_LEN];

    sr = (rtcp_sr_t *) (rtcp + 1);
    ntp.upper = ntohl(sr->sr_ntp_h);
    ntp.lower = ntohl(sr->sr_ntp_l);

    RTP_BUGINF("SR: sndr_ssrc=0x%x\n"
               "    ntp=%s (h/l=0x%08x/0x%08x)\n"
               "    rtp=0x%08x npackets=%d nbytes=%d\n", 
               ntohl(rtcp->ssrc), 
               abs_time_to_str(ntp_to_abs_time(ntp), timebuf, sizeof(timebuf)),
               ntohl(sr->sr_ntp_h), ntohl(sr->sr_ntp_l), 
               ntohl(sr->sr_timestamp), ntohl(sr->sr_npackets),
               ntohl(sr->sr_nbytes));

    rtcp_print_rb((rtcp_rr_t *) (sr + 1), end_of_frame);
}

/*
 * rtcp_print_sdes
 *
 * Prints Sender Description 
 */
static void
rtcp_print_sdes (rtcptype *rtcp, uint8_t *end_of_frame, int count)
{
    uint32_t ssrc;
    uint8_t *ptr;
    struct sdes {
        uint8_t name;
        uint8_t len;
        uint8_t data;
    } *sdes;

    ptr =  (uint8_t *)rtcp;
    ptr += 4;        /* Move past header information */
    while (count-- && ptr < end_of_frame) {
        ssrc = ntohl(*((uint32_t *)ptr));
        sdes = (struct sdes *)(ptr + sizeof(uint32_t));

        RTP_BUGINF("SDES: ssrc=0x%08x name=%u len=%u data=%*s\n", ssrc,
                   sdes->name, sdes->len, sdes->len, &sdes->data);

        ptr += ntohl(sdes->len) + 2 + sizeof(uint32_t);
    }
}

/*
 * rtcp_print_bye
 *
 * Prints BYE
 */
static void
rtcp_print_bye (rtcptype *rtcp, uint8_t *end_of_frame, int count)
{
    uint32_t *ssrc = &rtcp->ssrc;
    uint8_t *ptr;

    RTP_BUGINF("BYE: ");
    while (count-- && (uint8_t *)ssrc < end_of_frame) {
        RTP_BUGINF("ssrc=0x%08x ", ntohl(*ssrc++));
    }
    ptr = (uint8_t *)ssrc;

    if (ptr < end_of_frame) {
        RTP_BUGINF("length=%d reason=%*s", *ptr, *ptr, ptr + 1);
    }
    RTP_BUGINF("\n");
}

/*
 * rtcp_print_app
 *
 * Prints information on some of the common elements of an APP message,
 * using RTP_BUGINF. Application-specific elements (which are typically
 * the bulk of the message) are not displayed.
 *
 * Parameters:
 * rtcp          -- ptr to the APP message header
 * end_of_frame  -- ptr to the byte just past the end of the message
 * count         -- value of the "count" field in the APP message header;
 *                  note that the "count" field is used as a "subtype",
 *                  for APP messages.
 */
static void
rtcp_print_app (rtcptype *rtcp, uint8_t *end_of_frame, int count)
{
    rtcp_app_t *p_app = (rtcp_app_t *)(rtcp + 1);


    RTP_BUGINF("APP: ssrc=0x%08x name=%4s (%d bytes)\n",
               ntohl(rtcp->ssrc),
               (char *)(&p_app->name),
               (int)(end_of_frame - (uint8_t *)rtcp));
}

/*
 * rtpfb_fmt_str
 *
 * Return a string describing an RTPFB format type.
 *
 * Parameters:
 * type         -- format type value (e.g., RTCP_RTPFB_GENERIC_NACK).
 * Returns:     -- ptr to a string describing the value; if the value
 *                 is unrecognized, a pointer to a default string is returned.
 */
static char *rtpfb_fmt_str (int type)
{
    switch (type) {
    case RTCP_RTPFB_GENERIC_NACK:
        return ("generic NACK");
    default:
        return ("Unassigned or reserved");
    }
}

/*
 * rtcp_print_rtpfb
 *
 * Prints an RTPFB message, using RTP_BUGINF.
 *
 * Parameters:
 * rtcp          -- ptr to the RTPFB message header
 * end_of_frame  -- ptr to the byte just past the end of the message
 * count         -- value of the "count" field in the RTPFB message header;
 *                  note that the "count" field is used as a format type,
 *                  for RTPFB messages.
 */
static void
rtcp_print_rtpfb (rtcptype *rtcp, uint8_t *end_of_frame, int count)
{
    rtcpfbtype_t *rtcpfb = (rtcpfbtype_t *)rtcp;
    char *fci = &rtcpfb->fci[0];
    rtcp_rtpfb_generic_nack_t *nack = NULL;

    RTP_BUGINF("RTPFB (%s): ssrc(pkt_sndr)=0x%08x ssrc(media_src)=0x%08x\n",
               rtpfb_fmt_str(count), 
               ntohl(rtcp->ssrc), 
               ntohl(rtcpfb->ssrc_media_sender));
    switch (count) {
    case RTCP_RTPFB_GENERIC_NACK:
        nack = (rtcp_rtpfb_generic_nack_t *)fci;
        RTP_BUGINF("      pid=0x%04x mask=0x%04x\n",
                   ntohs(nack->pid), ntohs(nack->bitmask));
        break;
    default:
        break;
    }
}

/*
 * psfb_fmt_str
 *
 * Return a string describing a PSFB format type.
 *
 * Parameters:
 * type         -- format type value (e.g., RTCP_PSFB_PLI).
 * Returns:     -- ptr to a string describing the value; if the value
 *                 is unrecognized, a pointer to a default string is returned.
 */
static char *psfb_fmt_str (int type)
{
    switch (type) {
    case RTCP_PSFB_PLI:
        return ("PLI");
    case RTCP_PSFB_SLI:
        return ("SLI");
    case RTCP_PSFB_RPSI:
        return ("RPSI");
    case RTCP_PSFB_ALFB:
        return ("AFB");
    default:
        return ("Unassigned or reserved");
    }
}

/*
 * rtcp_print_psfb
 *
 * Prints a PSFB message, using RTP_BUGINF.
 * 
 * Parameters:
 * rtcp          -- ptr to the PSFB message header
 * end_of_frame  -- ptr to the byte just past the end of the message
 * count         -- value of the "count" field in the PSFB message header;
 *                  note that the "count" field is used as a format type,
 *                  for PSFB messages.
 */
static void
rtcp_print_psfb (rtcptype *rtcp, uint8_t *end_of_frame, int count)
{
    rtcpfbtype_t *rtcpfb = (rtcpfbtype_t *)rtcp;

    RTP_BUGINF("PSFB (%s): ssrc(pkt_sndr)=0x%08x ssrc(media_src)=0x%08x\n",
               psfb_fmt_str(count), 
               ntohl(rtcp->ssrc), 
               ntohl(rtcpfb->ssrc_media_sender));
    /* we only care about PLI, which has no FCI */
}

/*
 * rtcp_print_xr
 *
 * Prints an XR message, using RTP_BUGINF.
 * 
 * Parameters:
 * rtcp          -- ptr to the XR message header
 * end_of_frame  -- ptr to the byte just past the end of the message
 * count         -- value of the "count" field in the XR message header;
 *                  note that the "count" field is marked as "reserved",
 *                  for RSI messages.
 */
static void
rtcp_print_xr (rtcptype *rtcp, uint8_t *end_of_frame, int count)
{
    rtcp_xr_gen_t *p_xr;
    rtcp_xr_loss_rle_t *p_loss;
    rtcp_xr_stat_summary_t *p_stats;
    uint8_t *p_end_of_xr;
    int pkt_len;
    rtcp_xr_report_type_t type;
    uint32_t i;
    uint32_t num_chunks;
    uint16_t *p_chunk;
    uint16_t chunk;

    p_xr = (rtcp_xr_gen_t *)(rtcp + 1);
    
    /*
     * The rtcp length is in 32-bit words minus one
     * including the header and any padding.
     */
    pkt_len = (ntohs(rtcp->len) + 1) * 4;
    p_end_of_xr = (uint8_t *)rtcp + pkt_len;

    RTP_BUGINF("XR: (len %d) ssrc=0x%08x\n",
               pkt_len, ntohl(rtcp->ssrc));
    
    if (p_end_of_xr > end_of_frame) {
        RTP_BUGINF("XR: pkt too long\n");
        return;
    }

    while ((uint8_t *)p_xr < p_end_of_xr) {
        type = p_xr->bt;
        switch (type) {
            case RTCP_XR_LOSS_RLE:
            case RTCP_XR_POST_RPR_LOSS_RLE:
                p_loss = (rtcp_xr_loss_rle_t *)p_xr;
                num_chunks = (ntohs(p_loss->length) - 2) << 1;
                RTP_BUGINF("     XR %s Loss RLE info (length=%u):\n"
                           "     frame: begin=%u, end=%u\n",
                           (type == RTCP_XR_POST_RPR_LOSS_RLE) ?
                           "Post Repair" : "",
                           num_chunks,
                           ntohs(p_loss->begin_seq), 
                           ntohs(p_loss->end_seq));

                p_chunk = (uint16_t *)p_loss->chunk;
                for (i = 0; i < num_chunks; i++) {
                    chunk = ntohs(*p_chunk);
                    if (chunk & 0x8000) {
                        RTP_BUGINF("     chunk[%d]: bit vector %s\n",
                                   i+1, convert2binary(chunk & 0x7FFF));
                    } else {
                        if (chunk) {
                            RTP_BUGINF("     chunk[%d]: type %d, length %u\n",
                                       i+1, 
                                       (chunk&0x4000) ? 1 : 0, 
                                       chunk & 0x3FFF);
                        } else {
                            RTP_BUGINF("     chunk[%d]: terminating null\n",
                                       i+1);
                        }
                    }
                    p_chunk++;
                }
                
                break;

            case RTCP_XR_STAT_SUMMARY:
                p_stats = (rtcp_xr_stat_summary_t *)p_xr;
                RTP_BUGINF("     XR stat-summary info (flags %x):\n"
                           "     frame: begin=%u, end=%u\n"
                           "     lost_packets=%u, dup_packets=%u\n"
                           "     jitter: min=%u, max=%u, mean=%u, dev=%u\n",
                           p_stats->type_specific,
                           ntohs(p_stats->begin_seq),
                           ntohs(p_stats->end_seq),
                           ntohl(p_stats->lost_packets),
                           ntohl(p_stats->dup_packets),
                           ntohl(p_stats->min_jitter),
                           ntohl(p_stats->max_jitter),
                           ntohl(p_stats->mean_jitter),
                           ntohl(p_stats->dev_jitter));

                break;

            default:
                RTP_BUGINF("    Unknown/unsupported report 0x%08x (len %u)\n",
                           type, ntohs(p_xr->length));
                break;
        }
        
        p_xr = (rtcp_xr_gen_t *)((uint8_t *)p_xr + 
                                 ((ntohs(p_xr->length) + 1) * 4));
    }

    if ((uint8_t *)p_xr != p_end_of_xr) {
        RTP_BUGINF("XR: Inconsistent packet length %d\n", pkt_len);
    }
}

/*
 * rtcp_print_rsi
 *
 * Prints an RSI message, using RTP_BUGINF.
 * 
 * Parameters:
 * rtcp          -- ptr to the RSI message header
 * end_of_frame  -- ptr to the byte just past the end of the message
 * count         -- value of the "count" field in the RSI message header;
 *                  note that the "count" field is marked as "reserved",
 *                  for RSI messages.
 */
static void
rtcp_print_rsi (rtcptype *rtcp, uint8_t *end_of_frame, int count)
{
    rtcp_rsi_t *p_rsi;
    rtcp_rsi_gen_subrpt_t *p_gensb;
    rtcp_rsi_gaps_subrpt_t *p_gapsb;
    rtcp_rsi_rtcpbi_subrpt_t *p_bisb;
    uint16_t role;
    uint8_t *p_end_of_rsi;
    uint8_t *p_end_of_subrpt;
    int pkt_len;
    ntp64_t ntp;
    char timebuf[TIMEBUF_LEN];

    p_rsi = (rtcp_rsi_t *)(rtcp + 1);
    p_gensb = (rtcp_rsi_gen_subrpt_t *)&(p_rsi->data[0]);
    
    /*
     * The rtcp length is in 32-bit words minus one
     * including the header and any padding.
     */
    pkt_len = (ntohs(rtcp->len) + 1) * 4;
    p_end_of_rsi = (uint8_t *)rtcp + pkt_len;

    ntp.upper = ntohl(p_rsi->ntp_h);
    ntp.lower = ntohl(p_rsi->ntp_l);

    RTP_BUGINF("RSI: (len %d) ssrc=0x%08x summ_ssrc=0x%08x\n"
               "     ntp=%s (h/l=0x%08x/0x%08x)\n",
               pkt_len, ntohl(rtcp->ssrc), ntohl(p_rsi->summ_ssrc),
               abs_time_to_str(ntp_to_abs_time(ntp), 
                               timebuf, sizeof(timebuf)),
               ntohl(p_rsi->ntp_h), ntohl(p_rsi->ntp_l));
    
    if (p_end_of_rsi > end_of_frame) {
        RTP_BUGINF("RSI: pkt too long\n");
        return;
    }
    while ((uint8_t *)p_gensb < p_end_of_rsi) {
        p_end_of_subrpt = (uint8_t *)p_gensb + (p_gensb->length * 4);
        if (p_gensb->length == 0 || p_end_of_subrpt > p_end_of_rsi) {
            /* subreport too long */
            return;
        }
        switch (p_gensb->srbt) {
        case RTCP_RSI_GAPSB:
            p_gapsb = (rtcp_rsi_gaps_subrpt_t *)p_gensb;
            RTP_BUGINF("     Group info (len %u): "
                       "group_size=%u avg_pkt_size=%u\n",
                       p_gensb->length,
                       ntohl(p_gapsb->group_size), 
                       ntohs(p_gapsb->average_packet_size));
            break;
        case RTCP_RSI_BISB:
            p_bisb = (rtcp_rsi_rtcpbi_subrpt_t *)p_gensb;
            role = ntohs(p_bisb->role);
            RTP_BUGINF("     RTCP bw ind (len %u): "
                       "role=0x%04x (%s%s) bw=0x%08x\n",
                       p_gensb->length,
                       role,
                       role & RTCP_RSI_BI_RECEIVERS ? "R" : "",
                       role & RTCP_RSI_BI_SENDER ? "S" : "",
                       ntohl(p_bisb->rtcp_bandwidth));
            break;
        default:
            RTP_BUGINF("    Unknown/unsupported subreport %u (len %u)\n",
                       p_gensb->srbt, p_gensb->length);
            break;
        }
        p_gensb = (rtcp_rsi_gen_subrpt_t *)((uint8_t *)p_gensb + 
                                            (p_gensb->length * 4));
    }
    if ((uint8_t *)p_gensb != p_end_of_rsi) {
        RTP_BUGINF("RSI: Inconsistent packet length %d\n", pkt_len);
    }

}

/*
 * rtcp_print_pubports
 *
 * Print a PUBPORTS message, using RTP_BUGINF.
 *
 * Parameters:
 * rtcp          -- ptr to the PUBPORTS message header
 * end_of_frame  -- ptr to the byte just past the end of the message
 * count         -- value of the "count" field in the PUBPORTS message header
 */
static void
rtcp_print_pubports (rtcptype *rtcp, uint8_t *end_of_frame, int count)
{
    rtcp_pubports_t *p_pubports = (rtcp_pubports_t *)(rtcp + 1);
    RTP_BUGINF("PUBPORTS: ssrc(sender)=0x%08x ssrc(media_src)=0x%08x\n"
               "          rtp_port=%u rtcp_port=%u\n",
               ntohl(rtcp->ssrc), ntohl(p_pubports->ssrc_media_sender),
               ntohs(p_pubports->rtp_port), ntohs(p_pubports->rtcp_port));
}


/*
 * vqr_hdr_type_str
 *
 * Return a string describing a vqr_hdr_type_e value.
 */
char *exp_hdr_type_str (exp_hdr_type_e value)
{
    switch (value) {
    case EXP_RTCP_REPORT:
        return ("RTCP REPORT");
    default:
        return ("???");
    }
}

/*
 * exp_hdr_subtype_str
 *
 * Return a string describing a exp_hdr_subtype_e value.
 */
char *exp_hdr_subtype_str (exp_hdr_subtype_e value)
{
    switch (value) {
    case EXP_ORIGINAL:
        return ("Original");
    case EXP_RESOURCED:
        return ("Re-sourced");
    case EXP_REXMIT:
        return ("Repair");
    default:
        return ("???");
    }
}

/*
 * exp_hdr_role_str
 *
 * Return a string describing a exp_hdr_role_e value.
 */
char *exp_hdr_role_str (exp_hdr_role_e value)
{
    switch (value) {
    case EXP_VQEC:
        return ("VQE-C");
    case EXP_VQES:
        return ("VQE-S");
    case EXP_SSM_DS:
        return ("VQE-SH");
    default:
        return ("???");
    }
}

/*
 * exp_print_report
 *
 * Prints a EXP Report header
 */

void 
exp_print_report (rtcptype *rtcp, uint8_t *end_of_frame, int count)
{
    exp_hdr_t *hdr = (exp_hdr_t *)rtcp;
    ntp64_t ntp;
    char timebuf[TIMEBUF_LEN];
    char chan_addr_str[INET_ADDRSTRLEN];
    char src_addr_str[INET_ADDRSTRLEN];
    char dst_addr_str[INET_ADDRSTRLEN];

    ntp.upper = ntohl(hdr->ntp_upper);
    ntp.lower = ntohl(hdr->ntp_lower);

    RTP_BUGINF("%s for channel %s:%u at %s\n"
               "  %s:%u(%s)->%s:%u(%s) (%s stream)\n",
               exp_hdr_type_str(rtcp_get_type(ntohs(rtcp->params))),
               uint32_ntoa_r(hdr->chan_addr, chan_addr_str, sizeof(chan_addr_str)),
               ntohs(hdr->chan_port),
               abs_time_to_str(ntp_to_abs_time(ntp), timebuf, sizeof(timebuf)),
               uint32_ntoa_r(hdr->src_addr, src_addr_str, sizeof(src_addr_str)),
               ntohs(hdr->src_port),
               exp_hdr_role_str(hdr->sndr_role),
               uint32_ntoa_r(hdr->dst_addr, dst_addr_str, sizeof(dst_addr_str)),
               ntohs(hdr->dst_port),
               exp_hdr_role_str(hdr->rcvr_role),
               exp_hdr_subtype_str(count));
}

/* 
 * rtcp_print_envelope
 *
 * Prints information about a received RTP packet
 */
void rtcp_print_envelope(uint32_t len,
                         struct in_addr addr, uint16_t port,
                         abs_time_t rcv_ts)
{
    print_envelope("RTCP", len, addr, port, rcv_ts);
}

/*
 * rtcp_print_byte_stream
 */

void rtcp_print_byte_stream (uint8_t *data, int len) 
{
    int i;

    if (!data || !len) {
        return;
    }

    for (i = 0 ; i < len - 1 ; i++) {
        RTP_BUGINF("%02x.", data[i]);
    }
    printf("%02x\n", data[len-1]);
}

/*
 * rtcp_print_packet
 *
 * Prints an RTCP compound packet
 */
void
rtcp_print_packet (rtcptype *rtcp, uint32_t len)
{
    uint8_t         *end_of_pak, *end_of_frame;
    ushort        params = 0, packet_type, count;

    if ((len < 1) || (len < sizeof(rtcptype))) {
        return;
    }

    switch (rtcp_get_type(ntohs(rtcp->params))) {
      case RTCP_SR:
      case RTCP_RR:
      case RTCP_BYE:
      case EXP_RTCP_REPORT:
        break;
 
      default:
          printf("Unknown RTCP header: params=0x%04x len=0x%04x\n",
                 ntohs(rtcp->params), ntohs(rtcp->len));
          rtcp_print_byte_stream((uint8_t *)rtcp, sizeof(rtcptype));
          return;
    }

    if (rtcp_get_version(ntohs(rtcp->params)) != RTPVERSION) {
          printf("Unknown RTCP header: params=0x%04x len=0x%04x\n",
                 ntohs(rtcp->params), ntohs(rtcp->len));
          rtcp_print_byte_stream((uint8_t *)rtcp, sizeof(rtcptype));
          return;
    }    

    end_of_pak = (uint8_t *) rtcp + len;

    while ((uint8_t *) rtcp < end_of_pak) {
    
        len = ((ntohs(rtcp->len)) << 2) + 4;
        end_of_frame = (uint8_t *) rtcp + len;
        if (end_of_frame > end_of_pak) {
            RTP_BUGINF("Truncated packet:\n");
            rtcp_print_byte_stream((uint8_t *)rtcp, 
                                   end_of_pak - (uint8_t *)rtcp);
            return;
        }
 
        params = ntohs(rtcp->params);
        if (rtcp_get_version(params) != RTPVERSION) {
            RTP_BUGINF("Bad version:\n");
            rtcp_print_byte_stream((uint8_t *)rtcp, sizeof(rtcptype));
            return;
        }
 
        packet_type = rtcp_get_type(params);
        count       = rtcp_get_count(params);
 
        /*
         * Demux the single RTCP packet stream.
         */
        switch (packet_type) {
          case EXP_RTCP_REPORT:
            exp_print_report(rtcp, end_of_frame, count);
            break;

          case RTCP_SR:
            rtcp_print_sr(rtcp, end_of_frame);
            break;
 
          case RTCP_RR:
            rtcp_print_rr(rtcp, end_of_frame);
            break;
 
          case RTCP_SDES:
            rtcp_print_sdes(rtcp, end_of_frame, count);
            break;
 
          case RTCP_BYE:
            rtcp_print_bye(rtcp, end_of_frame, count);
            break;
 
          case RTCP_APP:
            rtcp_print_app(rtcp, end_of_frame, count);
            break;

          case RTCP_RTPFB:
            rtcp_print_rtpfb(rtcp, end_of_frame, count);
            break;

          case RTCP_PSFB:
            rtcp_print_psfb(rtcp, end_of_frame, count);
            break;

          case RTCP_XR:
            rtcp_print_xr(rtcp, end_of_frame, count);
            break;

          case RTCP_RSI:
            rtcp_print_rsi(rtcp, end_of_frame, count);
            break;

          case RTCP_PUBPORTS:
            rtcp_print_pubports(rtcp, end_of_frame, count);
            break;

          default:
            RTP_BUGINF("Unknown message type %u: (%u bytes)\n",
                       packet_type, len);
            break;
        }
        rtcp_print_byte_stream((uint8_t *)rtcp, 
                               end_of_frame - (uint8_t *)rtcp);
        rtcp = (rtcptype *) end_of_frame;
    }
}



