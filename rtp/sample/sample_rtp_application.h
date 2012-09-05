/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * sample_rtp_application.h
 * 
 * Copyright (c) 2007-2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

#ifndef __sample_rtp_application_h__
#define __sample_rtp_application_h__
#include <sys/types.h>
#include <stdio.h>

typedef struct sample_config_t_ {
    boolean   sender;
    int64_t   send_interval;
    int       num_sessions;
    boolean   extra_messages;
    boolean   export;
    uint32_t  per_rcvr_bw;
    boolean   rtcp_xr_enabled;
    boolean   rtcp_rsize;

    in_addr_t if_addr;

    in_addr_t rtp_addr;
    in_port_t rtp_port;
    in_addr_t rtcp_addr;
    in_port_t rtcp_port;

    in_addr_t fbt_addr;
    in_port_t fbt_port;
    in_port_t rpr_port;

    in_addr_t alt_rtp_addr;
    in_port_t alt_rtp_port;
    in_addr_t alt_rtcp_addr;
    in_port_t alt_rtcp_port;

    in_addr_t alt_fbt_addr;
    in_port_t alt_fbt_port;
    in_port_t alt_rpr_port;

    in_addr_t out_addr;
    in_port_t out_port;

    int drop_count;
    int drop_intvl;
    boolean drop_jitter;
    int min_chg_secs;
    int max_chg_secs;
    boolean do_fastfill;
    uint32_t maximum_fastfill_time;
    uint32_t min_rcc_fill;
    uint32_t max_rcc_fill;
    uint32_t maximum_recv_bw;

    FILE     *rtcp_script;
    FILE     *rtp_script;
    boolean   verbose;
} sample_config_t ;

typedef struct sample_receiver_t_ sample_receiver_t;
typedef struct rtp_sample_t_      rtp_sample_t;
typedef struct sample_channel_t_  sample_channel_t;

#endif /* to prevent multiple inclusion */
