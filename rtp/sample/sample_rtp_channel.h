/**-----------------------------------------------------------------
 * @brief
 * Declarations/definitions for sample per-channel data/functions.
 *
 * @file
 * sample_rtp_channel.h
 *
 * June 2008, Mike Lague.
 *
 * Copyright (c) 2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __SAMPLE_RTP_CHANNEL_H__
#define __SAMPLE_RTP_CHANNEL_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#include "../rtp_ptp.h"
#include "../../include/sys/event.h"
#include "../../include/utils/zone_mgr.h"

#include "sample_rtp_application.h"

typedef enum chanevt_type_t_ {
    CHANEVT_PRIMARY = 1,
    CHANEVT_REPAIR,
    CHANEVT_MORERPR,
    CHANEVT_REPORT
} chanevt_type_t;

typedef struct sample_chanevt_t_ {
    struct event      ev;
    chanevt_type_t    type;
    sample_channel_t *channel;
    boolean           from_recv_only_members;
} sample_chanevt_t;

struct sample_channel_t_ {
#define MAIN_CHANNEL 0
#define ALT_CHANNEL  1
    int       channel_number;
    boolean   channel_valid;
    sample_config_t *config;

    in_addr_t pri_rtp_addr;
    in_port_t pri_rtp_port;

    in_addr_t fbt_addr;
    in_port_t fbt_port;
    in_port_t rpr_port;

    in_addr_t pri_rtcp_addr;
    in_port_t pri_rtcp_port;
    int       pri_recv_socket;
    int       pri_send_socket;
    sample_chanevt_t   pri_rtcp_ev;
    sample_receiver_t *pri_rcvr;

    in_addr_t rpr_rtcp_addr;
    in_port_t rpr_rtcp_port;
    int       rpr_recv_socket;
    int       rpr_send_socket;
    sample_chanevt_t   rpr_rtcp_ev;
    sample_receiver_t *rpr_rcvr;

    in_addr_t morerpr_rtcp_addr;
    in_port_t morerpr_rtcp_port;
    int       morerpr_recv_socket;
    int       morerpr_send_socket;
    sample_chanevt_t   morerpr_rtcp_ev;
    sample_receiver_t *morerpr_rcvr;

    sample_chanevt_t   report_ev;

    uint32_t           psfbs_sent;

    VQE_LIST_HEAD(, rtp_sample_t_) pri_sessions;
    VQE_LIST_HEAD(, rtp_sample_t_) rpr_sessions;
} ;

/* prototypes */

int rtp_create_channel_sample(sample_config_t *config);
int get_num_channels(void);
sample_channel_t *get_sample_channel(int channel_number);
sample_channel_t *get_other_channel(int channel_number);

#endif /* to prevent multiple inclusion */

