/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * sample_rtp_dataplane.h
 * 
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */
/*
 * Prevent multiple inclusion.
 */
#ifndef __sample_rtp_dataplane_h__
#define __sample_rtp_dataplane_h__
#include "../../include/utils/vam_types.h"
#include "../../include/utils/vam_time.h"
#include "../rtp.h"
#include "../rtp_header.h"
#include "sample_rtp_application.h"
#include "sample_rtp_session.h"

/*
 * Sender data and functions
 */
typedef struct sample_sender_t_ {
    int        fd;      
    in_addr_t  rtp_addr;
    in_port_t  rtp_port;
    uint32_t   ssrc;
    uint16_t   seq;
    FILE      *rtp_script;
    uint32_t   packets;
    uint32_t   bytes;
    uint32_t   rtp_timestamp;
    abs_time_t send_time;
    int64_t    interval;    /* interval between data sends, in msecs */

    boolean      verbose;
    struct event timer;
} sample_sender_t;

void create_sample_sender(sample_config_t *config, uint32_t ssrc, uint16_t seq);
sample_sender_t *get_sample_sender(void);

/*
 * "Translator" data and functions
 */

typedef enum {
    SEND_OK,
    SEND_FAILED,
    SEND_SKIPPED    /* packet send skipped, due to RTP script command */
} send_status_t;

void create_sample_translator(sample_config_t *config);
/* a "translator" will maintain the same data as a "sender" */
sample_sender_t *get_sample_translator(void);

/*
 * Receiver data and functions
 */

typedef struct sample_source_t_ {
    VQE_LIST_ENTRY(sample_source_t_) source_link;
    rtp_source_id_t   id;
    rtp_error_t       status;
    rtp_hdr_source_t  stats;
    rtcp_xr_stats_t   xr_stats;
} sample_source_t;
    
typedef enum {
    SAMPLE_EXTERNAL_RECEIVER = 0,    /* "split" RTP/RTCP processing */
    SAMPLE_INTERNAL_RECEIVER,        /* "unified" RTP/RTCP processing */
    SAMPLE_REPAIR_RECEIVER,          /* "split" proc., master repair session */
    SAMPLE_DUMMY_RECEIVER,           /* "split" proc., other repair sessions */
    SAMPLE_TRANS_RECEIVER,           /* "split" proc., with "translation"
                                        (forwarding) to another destination
                                        transport address */
    SAMPLE_NUM_RECEIVERS             /* must be last: defines no. rcvr types */
} sample_receiver_type_t;
    
struct sample_receiver_t_ {
    sample_receiver_type_t type;
    sample_channel_t *channel;

    uint64_t received;
    int drop_count;
    int drop_intvl;
    uint64_t dropped;
#define DROP_BUFSIZ 1500 /* about 1 second's worth of seq. nos., at HD rates */
    uint16_t drop_seq_nos[DROP_BUFSIZ];
    int drop_next_write_idx;

    boolean verbose;
    int fd;
    in_addr_t recv_addr;
    in_port_t recv_port;
    struct event event;
    rtp_hdr_session_t stats;
    boolean rtcp_xr_enabled;
    VQE_LIST_HEAD(, sample_source_t_) sources;
} ;

void create_sample_receiver(sample_config_t *config, 
                            sample_receiver_type_t type, 
                            sample_channel_t *channel);
boolean sample_getnext_drop(sample_receiver_t *receiver,
                            int *next_read_idx,
                            uint16_t *sequence);
sample_receiver_t *get_sample_receiver(sample_receiver_type_t type);
char *sample_receiver_type(sample_receiver_type_t type);
sample_source_t *get_sample_source(sample_receiver_t *receiver,
                                   rtp_source_id_t *source_id);
void delete_sample_source(sample_receiver_t *receiver,
                          rtp_source_id_t *source_id);
#endif /* to prevent multiple inclusion */
