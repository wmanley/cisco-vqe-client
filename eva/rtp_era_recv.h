/*
 * rtp_era_recv.h - defines an Error Repair Aware.
 * Note this session is sometimes called the primary session
 * This is a derived "class", derived from
 * the RTP ssm rsi session "class".
 * 
 * Copyright (c) 2006-2010 by cisco Systems, Inc.
 * All rights reserved.
 *
 */

#ifndef __rtp_era_recv_h__
#define __rtp_era_recv_h__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <netinet/in.h>
#include <utils/vam_types.h>
#include <rtp/rtp_ssm_rsi_rcvr.h>
#include "vqec_event.h"
#include "vqec_recv_socket.h"
#include "vqec_rtp.h"
#include "vqec_dp_api.h"
#include "vqec_channel_api.h"

struct vqec_chan_;
struct rtp_era_recv_t_;

/*
 * The ERA-RECV member class is derived from the SSM_RSI RTP member class
 */
#define  ERA_RECV_MEMBER_INFO                       \
    SSM_RSI_RCVR_MEMBER_INFO                        \
    /* RTCP XR stats storage */                     \
    rtcp_xr_stats_t xr_stats_data;                  \
    /* RTCP Post ER XR stats storage */             \
    rtcp_xr_post_rpr_stats_t post_er_stats_data;    

/* ERA-RECV member is a derived member */
typedef struct era_recv_member_ {
    ERA_RECV_MEMBER_INFO
} era_recv_member_t;

/*
 * The SSM-RSI session is derived from the base RTP session.
 *
 * The methods table will be overridden by any methods that are unique to
 * this type of session, and may also be augmented to include more
 * SSM-RSI specific methods.
 *
 * The member cache contains all the rtp_member_t elements for           
 * all the members(sources and receivers) of a session. The number and the  
 * access method may be different for the different types of sessions.     
 * While the cache/data struct of members may be different, currently the
 * member structure itself is the same for all types of sessions.
 */                                                                         

#define RTP_ERA_RECV_METHODS                                    \
    RTP_SSM_RSI_RCVR_METHODS                                    \
                                                                \
    /**                                                         \
     * Process an upcall event.                                 \
     */                                                         \
    void (*process_upcall_event)(struct rtp_era_recv_t_ *this,  \
                                 vqec_dp_rtp_src_table_t *table); \
                                                                \
    /**                                                         \
     * Shutdown the primary session, but allow sending byes.    \
     */                                                         \
    void (*shutdown_allow_byes)(struct rtp_era_recv_t_ *this);  \



#define RTP_ERA_RECV_INFO                                       \
    RTP_SSM_RSI_RCVR_INFO                                       \
    /**                                                         \
     * Member timeout event.                                    \
     */                                                         \
    vqec_event_t *member_timeout_event;                         \
    /**                                                         \
     * Report interval timeout event.                           \
     */                                                         \
    vqec_event_t *send_report_timeout_event;                    \
    /**                                                         \
     * RTCP receive socket.                                     \
     */                                                         \
    vqec_recv_sock_t *era_rtcp_sock;                            \
    /**                                                         \
     * RTCP transmit socket.                                    \
     */                                                         \
    int32_t era_rtcp_xmit_sock;                                 \
    /**                                                         \
     * Dataplane input stream identifier.                       \
     */                                                         \
    vqec_dp_streamid_t dp_is_id;                                \
    /**                                                         \
     * Packetflow source.                                       \
     */                                                         \
    vqec_rtp_source_id_t pktflow_src;                           \
    /**                                                         \
     * Existing dataplane session state.                        \
     */                                                         \
    vqec_rtp_session_state_t state;                             \
    /**                                                         \
     * RTP session to RTP source sequence number offset.        \
     *                                                          \
     * VQE-C only supports a single RTP source at a time for    \
     * accepting primary/repair stream packets (called the      \
     * packetflow source).  If a channel's packetflow source    \
     * fails over to a new packetflow source, the new source    \
     * is assumed to use a different RTP sequence number space  \
     * than the old, and the new source's packets are inserted  \
     * into the PCM using a sequence number offset so as to be  \
     * sequential w.r.t. the last packet from the old source.   \
     * As a consequence, an inverse mapping of this offset      \
     * needs to be applied when translating from the PCM        \
     * sequence number to the RTP sequence number space used    \
     * by the current packetflow source.                        \
     */                                                         \
    int16_t session_rtp_seq_num_offset;                         \
    /**                                                         \
     * Sent RTCP XR statistics in the last receiver report.     \
     */                                                         \
    boolean last_xr_sent;                                       \
    /**                                                         \
     * Single Source Multicast session                          \
     */                                                         \
    boolean is_ssm;                                             \
    /**                                                         \
     * Parent channel object.                                   \
     */                                                         \
    struct vqec_chan_ *chan;                                    \


typedef struct rtp_era_recv_methods_t_ {
    RTP_ERA_RECV_METHODS 
} rtp_era_recv_methods_t;


/*
 * Function to populate member function pointer table
 */
void rtp_era_recv_set_methods(rtp_era_recv_methods_t * ssm_table,
                              rtp_era_recv_methods_t * ptp_table);

/**
 * rtp_era_recv_t
 */
typedef struct rtp_era_recv_t_ {
    rtp_era_recv_methods_t * __func_table;
    RTP_ERA_RECV_INFO
} rtp_era_recv_t;

boolean rtp_era_recv_init_memory(rtcp_memory_t *memory, int32_t max_sessions);
boolean rtp_era_recv_init_module(uint32_t max_sessions);

/* Prototypes...  create, plus all the overloading functions... */

rtp_era_recv_t *rtp_create_session_era_recv(vqec_dp_streamid_t dp_id,
                                            in_addr_t src_addr,
                                            char      *p_cname,
                                            in_addr_t era_dest_addr,
                                            uint16_t  recv_rtcp_port,
                                            uint16_t  send_rtcp_port,
                                            in_addr_t fbt_ip_addr,
                                            rtcp_bw_cfg_t *rtcp_bw_cfg,
                                            rtcp_xr_cfg_t *rtcp_xr_cfg,
                                            in_addr_t orig_source_addr,
                                            uint16_t orig_source_port, 
                                            uint32_t sock_buf_depth,
                                            uint8_t rtcp_dscp_value,
                                            uint8_t rtcp_rsize,
                                            struct vqec_chan_ *parent_chan);

boolean
rtp_update_session_era_recv(rtp_era_recv_t *p_era_recv,
                            in_addr_t fbt_ip_addr,
                            uint16_t send_rtcp_port);

void rtp_delete_session_era_recv(rtp_era_recv_t **p_era_recv,
                                 boolean deallocate);

void rtp_show_session_era_recv(rtp_era_recv_t *p_era_sess);

uint32_t rtcp_construct_report_era_recv(rtp_session_t   *p_sess,
                                        rtp_member_t    *p_source, 
                                        rtcptype        *p_rtcp,
                                        uint32_t         bufflen,
                                        rtcp_pkt_info_t *p_pkt_info,
                                        boolean         reset_xr_stats);

void rtp_session_era_recv_send_bye(rtp_era_recv_t *pp_era_recv);

#if HAVE_FCC
boolean
rtp_era_send_ncsi(rtp_era_recv_t *p_session,
                   uint32_t seq_num,
                   rel_time_t join_latency);

boolean
rtp_era_send_nakpli (rtp_era_recv_t *p_session,
                     rel_time_t rcc_min_fill,
                     rel_time_t rcc_max_fill,
                     uint32_t do_fast_fill,
                     uint32_t maximum_recv_bw,
                     rel_time_t max_fastfill);
#endif


boolean
rtp_era_send_to_rtcp_socket(rtp_era_recv_t *p_session,
                            in_addr_t remote_addr, 
                            in_port_t remote_port,
                            char *buf, 
                            uint16_t len);

vqec_chanid_t
rtp_era_session_chanid(rtp_era_recv_t *p_primary);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /*__rtp_era_recv_h__ */
