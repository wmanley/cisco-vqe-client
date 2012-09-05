/**------------------------------------------------------------------------
 * @brief
 * Per-channel functions for the sample_session application.
 *
 * @file
 * sample_rtp_channel.c
 *
 * June 2008, Mike Lague.
 *
 * Copyright (c) 2008-2009 by cisco Systems, Inc.
 * All rights reserved.
 *-------------------------------------------------------------------------
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#include "../../include/sys/event.h"
#include "../../include/utils/zone_mgr.h"
#include "../rtp_util.h"
#include "rtp_exp_api.h"
#include "../rtcp_stats_api.h"
#include "../rtp_database.h"
#include "../rcc_tlv.h"
#include "sample_rtp_application.h"
#include "sample_rtp_channel.h"
#include "sample_rtp_session.h"
#include "sample_rtp_dataplane.h"
#include "sample_rtp_utils.h"
#include "sample_rtcp_script.h"

#define ADDRSTRLEN 16

#define SAMPLE_NUM_CHANNELS 2
static sample_channel_t *rtp_sample_channels[SAMPLE_NUM_CHANNELS] = 
    { NULL, NULL };


/*
 * rtcp_receive_cb
 *
 * Callback function invoked by libevent when
 * an RTCP receive socket is ready to be read.
 *
 * Parameters:
 * fd          -- recv socket file description
 * event       -- event code: should be EV_READ
 * arg         -- context ptr: should be to a sample_chanevt_t struct
 */

static void rtcp_receive_cb (int fd, short event, void *arg)
{
    uint32_t len;
#define RCVBUFLEN RTCP_PAK_SIZE
    char  buf[RCVBUFLEN];
    sample_chanevt_t *ev = arg;
    sample_channel_t *channel = ev->channel;
    rtp_sample_t *sample = NULL;
    abs_time_t rcv_ts;
    struct in_addr src_addr;
    uint16_t src_port;
    rtp_envelope_t addrs;

    len = sizeof(buf);
    if (sample_socket_read(fd, &rcv_ts, 
                           buf, &len,
                           &src_addr, &src_port)) {

        if (channel->config->num_sessions <= 1) {
            rtcp_print_envelope(len, src_addr, src_port, rcv_ts);
            rtcp_print_packet((rtcptype *)buf, len);
        }

        addrs.src_addr = src_addr.s_addr;
        addrs.src_port = src_port;

        switch (ev->type) {
        case CHANEVT_PRIMARY:
        default:
            addrs.dst_addr = channel->pri_rtcp_addr;
            addrs.dst_port = channel->pri_rtcp_port;
            VQE_LIST_FOREACH(sample, &channel->pri_sessions, link) {
                MCALL((rtp_session_t *)sample, 
                      rtcp_recv_packet,
                      ABS_TIME_0,
                      ev->from_recv_only_members,
                      &addrs,
                      buf, len);
            }
            break;
        case CHANEVT_REPAIR:
            addrs.dst_addr = channel->rpr_rtcp_addr;
            addrs.dst_port = channel->rpr_rtcp_port;
            VQE_LIST_FOREACH(sample, &channel->rpr_sessions, link) {
                MCALL((rtp_session_t *)sample, 
                      rtcp_recv_packet, 
                      ABS_TIME_0,
                      ev->from_recv_only_members,
                      &addrs,
                      buf, len);
            }
            break;
        case CHANEVT_MORERPR:
            /* 
             * don't bother processing all the sender reports
             * sent to the "more repairs" transport address
             */
            return;
        }
    }
    (void)event_add(&ev->ev, NULL);
}

/*
 * sender_timeout_cb
 *
 * Callback invoked by libevent when the "sender timeout" expires,
 * This used to check for idle senders, but now it's just used to
 * display information on packets received, for this application.
 *
 * Parameters:
 * fd          -- recv socket file description
 * event       -- event code: should be EV_TIMEOUT
 * arg         -- context ptr: should be to a sample_chanevt_t struct
 */
 
static void sender_timeout_cb (int fd, short event, void *arg) {
    struct timeval tv;
    sample_chanevt_t *ev = arg;
    sample_channel_t *channel = ev->channel;
    rtp_sample_t *pri_sess = NULL;
    rtp_sample_t *rpr_sess = NULL;
#define TIMEBUFLEN 80
#define TIMEIDX    11
    char timebuf[TIMEBUFLEN];
    const char *p_timebuf = NULL;
    int timeidx = 0;
    char addr_str[ADDRSTRLEN];
    boolean channel_change = channel->config->min_chg_secs ? TRUE : FALSE;

    timerclear(&tv);
    tv = TIME_GET_R(timeval,TIME_MK_R(usec, 2*RTCP_MIN_TIME)) ;
    (void)event_add(&ev->ev, &tv);

    if (channel->config->sender) {
        return;
    }
    p_timebuf = abs_time_to_str(get_sys_time(), 
                                timebuf, sizeof(timebuf));
    if (p_timebuf && (strlen(p_timebuf) > TIMEIDX)) {
        timeidx = TIMEIDX;
    } 

    if (channel_change) {
        fprintf(stderr, "%s", &timebuf[timeidx]);
        fprintf(stderr, " RCCs to %s:%u: %u", 
                uint32_ntoa_r(channel->pri_rtp_addr, addr_str, sizeof(addr_str)),
                ntohs(channel->pri_rtp_port),
                channel->psfbs_sent);
        if (channel->rpr_rcvr) {
            fprintf(stderr, " rpr: %llu", channel->rpr_rcvr->received);
        }
        if (channel->morerpr_rcvr &&
            channel->morerpr_rcvr->received) {
            fprintf(stderr, " more rpr: %llu",
                    channel->morerpr_rcvr->received);
        } 
        fprintf(stderr, "\n");
        return;
    }
        
    pri_sess = VQE_LIST_FIRST(&channel->pri_sessions);
    if (pri_sess) {
        rtcp_report_notify((rtp_session_t *)(pri_sess));
        if (channel->config->rpr_port) {
            rpr_sess = VQE_LIST_FIRST(&channel->rpr_sessions);
            rtcp_report_notify((rtp_session_t *)(rpr_sess));
            fprintf(stderr, "%s", &timebuf[timeidx]);
            if (channel->config->drop_count) {
                fprintf(stderr, " dropped: %llu", channel->pri_rcvr->dropped);
            } 
            fprintf(stderr, " rpr: %llu", channel->rpr_rcvr->received);
            if (channel->config->num_sessions > 1) {
                fprintf(stderr, " more rpr: %llu",
                        channel->morerpr_rcvr->received);
            } 
            fprintf(stderr, "\n");
        }
    }
}

/*
 * create_channel
 *
 * Initialize per-channel information.
 * 
 * Parameters:
 *   config          -- ptr to sample_session config information.       
 *   channel_number  -- Unique number for channel, beginning at zero.
 *   main            -- if TRUE, use the "main" channel info in the config;
 *                      if FALSE, use the "alt" channel info.
 *   channel         -- ptr to channel struct to initialize.
 *
 * Returns:          TRUE on success, FALSE otherwise.
 */
static boolean create_channel (sample_config_t *config,
                               int channel_number,
                               boolean main,
                               sample_channel_t *channel)
{
    in_addr_t recv_addr = INADDR_NONE;
    in_port_t recv_port = 0;
    in_addr_t send_addr = INADDR_NONE;
    in_addr_t send_port = 0;
    in_addr_t rpr_send_addr = INADDR_NONE;
    in_addr_t rpr_send_port = 0;
    struct timeval tv;
    char addr_str[ADDRSTRLEN];

    if (!config || !channel) {
        return (FALSE);
    }

    memset(channel, 0, sizeof(sample_channel_t));

    channel->channel_number = channel_number;
    channel->channel_valid = TRUE;
    channel->config = config;

    if (main) {
        channel->pri_rtp_addr = config->rtp_addr;
        channel->pri_rtp_port = config->rtp_port;
        channel->pri_rtcp_addr = config->rtcp_addr;
        channel->pri_rtcp_port = config->rtcp_port;
        channel->fbt_addr = config->fbt_addr;
        channel->fbt_port = config->fbt_port;
        channel->rpr_port = config->rpr_port;
    } else {
        channel->pri_rtp_addr = config->alt_rtp_addr;
        channel->pri_rtp_port = config->alt_rtp_port;
        channel->pri_rtcp_addr = config->alt_rtcp_addr;
        channel->pri_rtcp_port = config->alt_rtcp_port;
        channel->fbt_addr = config->alt_fbt_addr;
        channel->fbt_port = config->alt_fbt_port;
        channel->rpr_port = config->alt_rpr_port;
    }

    /* set up RTCP sockets for primary stream */

    if (config->sender) {
        recv_addr = channel->fbt_addr;
        recv_port = channel->fbt_port;
        send_addr = channel->pri_rtcp_addr;
        send_port = channel->pri_rtcp_port;
    } else { /* we're a receiver */
        recv_addr = channel->pri_rtcp_addr;
        recv_port = channel->pri_rtcp_port;
        send_addr = channel->fbt_addr;
        send_port = channel->fbt_port;
        rpr_send_addr = channel->fbt_addr;
        rpr_send_port = channel->rpr_port;
    }

    channel->pri_send_socket = sample_socket_setup(config->if_addr, 0);
    if (IN_MULTICAST(ntohl(send_addr))) {
        (void)sample_mcast_output_setup(channel->pri_send_socket,
                                        send_addr,
                                        config->if_addr);
    }
    channel->pri_recv_socket = sample_socket_setup(recv_addr, recv_port);
    if (IN_MULTICAST(ntohl(recv_addr))) {
        (void)sample_mcast_input_setup(channel->pri_recv_socket,
                                       recv_addr,
                                       config->if_addr);
    }

    /* set up "read" event for primary RTCP */
    channel->pri_rtcp_ev.type = CHANEVT_PRIMARY;
    channel->pri_rtcp_ev.channel = channel;
    /* 
     * if we're a sender, received data is from "recv only" members;
     * if we're not a sender, rcvd data is NOT from "recv only" members 
     */
    channel->pri_rtcp_ev.from_recv_only_members = config->sender;
    event_set(&(channel->pri_rtcp_ev.ev), 
              channel->pri_recv_socket,
              EV_READ, 
              rtcp_receive_cb, 
              &(channel->pri_rtcp_ev));
    (void)event_add(&(channel->pri_rtcp_ev.ev), 0);

    printf("Sending RTCP to %s:%u ...\n", 
           uint32_ntoa_r(send_addr, addr_str, sizeof(addr_str)),
           ntohs(send_port));
    printf("Receiving RTCP from %s:%u ...\n", 
           uint32_ntoa_r(recv_addr, addr_str, sizeof(addr_str)),
           ntohs(recv_port));

    /* set up RTCP sockets for the repair stream, if any */

    if (!config->sender && config->rpr_port) {
        channel->rpr_send_socket = sample_socket_setup(config->if_addr, 0);
        channel->rpr_recv_socket = channel->rpr_send_socket;
        (void)rtp_get_local_addr(channel->rpr_recv_socket,
                                 &channel->rpr_rtcp_addr,
                                 &channel->rpr_rtcp_port);

        /* set up "read" event for repair RTCP */
        channel->rpr_rtcp_ev.type = CHANEVT_REPAIR;
        channel->rpr_rtcp_ev.channel = channel;
        channel->rpr_rtcp_ev.from_recv_only_members = config->sender;
        event_set(&(channel->rpr_rtcp_ev.ev), 
                  channel->rpr_recv_socket,
                  EV_READ, 
                  rtcp_receive_cb, 
                  &(channel->rpr_rtcp_ev));
        (void)event_add(&(channel->rpr_rtcp_ev.ev), 0);

        printf("Sending repair RTCP to %s:%u ...\n", 
               uint32_ntoa_r(rpr_send_addr, addr_str, sizeof(addr_str)),
               ntohs(rpr_send_port));
        printf("Receiving %s RTCP at %s:%u ...\n", 
               "repair", 
               uint32_ntoa_r(channel->rpr_rtcp_addr, 
                             addr_str, sizeof(addr_str)),
               ntohs(channel->rpr_rtcp_port));
    }

    /* set up additional sockets for RTCP repair, if required */

    if (!config->sender && config->rpr_port && config->num_sessions > 1) {
        channel->morerpr_send_socket = sample_socket_setup(config->if_addr, 0);
        channel->morerpr_recv_socket = channel->rpr_send_socket;
        (void)rtp_get_local_addr(channel->morerpr_recv_socket,
                                 &channel->morerpr_rtcp_addr,
                                 &channel->morerpr_rtcp_port);

        /* set up "read" event for additional repair RTCP */
        channel->morerpr_rtcp_ev.type = CHANEVT_REPAIR;
        channel->morerpr_rtcp_ev.channel = channel;
        channel->morerpr_rtcp_ev.from_recv_only_members = config->sender;
        event_set(&(channel->morerpr_rtcp_ev.ev), 
                  channel->morerpr_recv_socket,
                  EV_READ, 
                  rtcp_receive_cb, 
                  &(channel->morerpr_rtcp_ev));
        (void)event_add(&(channel->morerpr_rtcp_ev.ev), 0);

        printf("Receiving %s RTCP at %s:%u ...\n", 
               "more rpr", 
               uint32_ntoa_r(channel->morerpr_rtcp_addr, 
                             addr_str, sizeof(addr_str)),
               ntohs(channel->morerpr_rtcp_port));
    }

    channel->report_ev.type = CHANEVT_REPORT;
    channel->report_ev.channel = channel;
    channel->report_ev.from_recv_only_members = FALSE;
    evtimer_set(&(channel->report_ev.ev), 
                sender_timeout_cb, 
                &(channel->report_ev));
    timerclear(&tv);
    tv = TIME_GET_R(timeval,TIME_MK_R(usec, 2*RTCP_MIN_TIME)) ;
    (void)event_add(&(channel->report_ev.ev), &tv);

    return (TRUE);
}


/*
 * rtp_create_channel_sample
 *
 * Create all needed channels
 */

int rtp_create_channel_sample (sample_config_t *config)
{
    int num_channels = 0;

    rtp_sample_channels[0] = malloc(sizeof(sample_channel_t));
    if (rtp_sample_channels[0]) {
        if (create_channel(config, 0, TRUE, rtp_sample_channels[0])) {
            num_channels++;
        }
    }
    if (config->alt_rtcp_port) {
        rtp_sample_channels[1] = malloc(sizeof(sample_channel_t));
        if (rtp_sample_channels[1]) {
            if (create_channel(config, 1, FALSE, rtp_sample_channels[1])) {
                num_channels++;
            }
        }
    } else {
        rtp_sample_channels[1] = NULL;
    }

    return (num_channels);
}

/*
 * get_num_channels
 */
int get_num_channels (void)
{
    return (SAMPLE_NUM_CHANNELS);
}

/*
 * get_sample_channel
 *
 * Return a pointer to per-channel info for the specified channel number
 */
sample_channel_t *get_sample_channel (int channel_number) 
{
    if (channel_number >= 0 && channel_number < SAMPLE_NUM_CHANNELS) {
        return (rtp_sample_channels[channel_number]);
    } else {
        return (NULL);
    }
}

/*
 * get_other_channel
 *
 * Return a pointer to a channel different than specified channel number
 */
sample_channel_t *get_other_channel (int channel_number)
{
    int idx = 0;

    if (channel_number >= 0 && channel_number < SAMPLE_NUM_CHANNELS) {
        idx = (channel_number + 1) % SAMPLE_NUM_CHANNELS;
        return (rtp_sample_channels[idx]);
    } else {
        return (NULL);
    }
}

