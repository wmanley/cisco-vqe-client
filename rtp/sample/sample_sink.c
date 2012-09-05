/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * sample_sink.c
 * 
 * A sample program to process exported RTCP reports.
 * To build: Do "make" in the sample directory
 * To run: Run on 2 separate machines by using following command line:
 *        sample_sink <udp port> 
 * 
 * Copyright (c) 2006-2007 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "../../include/sys/event.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include "../../include/utils/vam_types.h"
#include "../../include/utils/vam_util.h"
#include <netinet/ip.h>
#include "sample_rtp_session.h"
#include "sample_rtp_dataplane.h"
#include "sample_rtp_utils.h"
#include "exp.h"
#include "rtp_exp_api.h"

#define RCVBUF_LEN 1500
#define NUM_CHAN_DTAS 10

static char rcvbuf[RCVBUF_LEN];
static struct event rcv_event;

typedef struct dta_ {
    in_addr_t addr;
    in_port_t port;
} dta_t;

int filter_chan_dta = 0;
dta_t chan_dtas[NUM_CHAN_DTAS];

static void print_usage (void) 
{
    printf("Usage: sample_sink -l <addr>:<port> -i <addr>:<port>\n"
           "                   [-c <addr1:port1>[...,<addrn:portn>]]\n");
}

static boolean ok_to_print (rtcptype *rtcp)
{
    boolean ok = TRUE;
    exp_hdr_t *hdr;
    int i;

    if (rtcp_get_type(ntohs(rtcp->params)) == EXP_RTCP_REPORT) {
        if (filter_chan_dta) {
            ok = FALSE;
            hdr = (exp_hdr_t *)rtcp;
            for (i = 0; i < filter_chan_dta ; i++) {
                if (hdr->chan_addr == chan_dtas[i].addr &&
                    hdr->chan_port == chan_dtas[i].port) {
                    ok = TRUE;
                    break;
                }
            }
        }
    }
    return (ok);
}

static void rtcp_receive_cb (int fd, short event, void *arg)
{
    uint32_t len;
    struct event *eventp = arg;
    abs_time_t rcv_ts;
    struct in_addr src_addr;
    uint16_t src_port;

    len = sizeof(rcvbuf);
    if (sample_socket_read(fd, &rcv_ts, 
                           rcvbuf, &len,
                           &src_addr, &src_port)) {
        if (ok_to_print((rtcptype *)rcvbuf)) {
            rtcp_print_envelope(len, src_addr, src_port, rcv_ts);
            rtcp_print_packet((rtcptype *)rcvbuf, len);
            printf("--\n");
        }
    }
    (void)event_add(eventp, NULL);
}

int main (int argc, char **argv)
{
    int32_t fd;
    int on = 1;
    struct sockaddr_in saddr;
    in_addr_t listen_addr = 0;
    in_port_t listen_port = 0;
    in_addr_t if_addr = INADDR_NONE;
#define ADDR_STRLEN 16
    char addr_str[ADDR_STRLEN];
#define PORT_STRLEN  6
    char port_str[PORT_STRLEN];
    char *chan_dta;
    struct ip_mreq mreq;
    char str[INET_ADDRSTRLEN];

    int c = 0;
    addr_str[0] = '\0';
    port_str[0] = '\0';

    while ((c = getopt(argc, argv, "l:i:c:")) != EOF) {
        switch (c) {
        case 'l':
            if (sscanf(optarg, "%15[^:]:%5s", addr_str, port_str) != 2) {
                print_usage();
                return (-1);
            }
            listen_addr = inet_addr(addr_str);
            listen_port = htons((in_port_t)(atoi(port_str)));
            break;
        case 'i':
            if_addr = inet_addr(optarg);
            break;
        case 'c':
            chan_dta = strtok(optarg, ",");
            while(chan_dta != NULL && filter_chan_dta < NUM_CHAN_DTAS) {
                if (sscanf(chan_dta, "%15[^:]:%5s", addr_str, port_str) != 2) {
                    print_usage();
                    return (-1);
                }
                chan_dtas[filter_chan_dta].addr = inet_addr(addr_str);
                chan_dtas[filter_chan_dta].port = 
                    htons((in_port_t)(atoi(port_str)));
                printf("Scanning for channel %s:%s...\n", 
                       addr_str, port_str);
                filter_chan_dta++;
                chan_dta = strtok(NULL, ",");
            }
            
            break;

        default:
            print_usage();
            return (-1);
        }
    }

    if (!listen_addr) {
        listen_addr = INADDR_ANY;
    }
    if (!listen_port) {
        listen_port = htons((in_port_t)51234);
    }

    /* Setup RTCP Socket */
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
    saddr.sin_addr.s_addr = listen_addr;
    saddr.sin_port = listen_port;    

    if (IN_MULTICAST(ntohl(listen_addr))) {
        /* request the kernel to join a multicast group */
        mreq.imr_multiaddr.s_addr = listen_addr;
        mreq.imr_interface.s_addr = if_addr;

        if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                       &mreq, sizeof(mreq)) < 0) {
            perror("mcast setsockopt");
            return(-1);
        }
    } 

    if (bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
        perror("bind");
        goto bail;
    }
    if (setsockopt(fd, SOL_SOCKET, 
                   SO_TIMESTAMP, &on, sizeof(on)) == -1) {
        perror("setsockopt");
        goto bail;
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        goto bail;
    }

    /* Initalize the event library */ 
    (void)event_init();

    /* Now create a "receiver" */
    memset(&rcv_event, 0, sizeof(rcv_event));
    event_set(&rcv_event, fd, EV_READ, rtcp_receive_cb, &rcv_event);
    (void)event_add(&rcv_event, 0);

    struct in_addr in_listen_addr;
    in_listen_addr.s_addr = listen_addr;
    printf("Receiving on %s:%d ...\n", 
           inaddr_ntoa_r(in_listen_addr, str, sizeof(str)), ntohs(listen_port));

    /* just wait for events */
    (void)event_dispatch();
    return (0);

 bail:
    close(fd);
    return(-1);
}
