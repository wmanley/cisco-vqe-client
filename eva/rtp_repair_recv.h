/* 
 * rtp_repair_recv.h - defines an RTP repair session.
 * 
 * Copyright (c) 2006-2010 by cisco Systems, Inc.
 * All rights reserved.
 *
 */

#ifndef __rtp_repair_recv_h__
#define __rtp_repair_recv_h__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <utils/vam_types.h>
#include <rtp/rtp_ptp.h>
#include "vqec_recv_socket.h"  
#include "vqec_rtp.h"    
#include "vqec_channel_api.h"
#if HAVE_FCC
#include <utils/mp_tlv_decode.h>
#endif /* HAVE_FCC */ 

struct vqec_chan_;
struct rtp_repair_recv_t_;

/*
 * The repair-recv session is derived from the rtp_ptp session.
 *
 * The methods table will be overridden by any methods that are unique to
 * this type of session, and may also be augmented to include more
 * repair-recv specific methods.
 *
 */                                                                         

#define RTP_REPAIR_RECV_METHODS     \
    RTP_PTP_METHODS                 \
                                    \
    /**                                                         \
     * Process an upcall event.                                 \
     */                                                         \
    void (*process_upcall_event)(struct rtp_repair_recv_t_ *this, \
                                 vqec_dp_rtp_src_table_t *table); \
    /**                                                           \
     * Process an change to the primary packet flow source.       \
     */                                                           \
    void (*primary_pktflow_src_update)(struct rtp_repair_recv_t_ *this, \
                                       rtp_source_id_t *id);            \
                                                                        \
    /**                                                                 \
     * Inject a NAT packet on the RTCP socket.                          \
     */                                                                 \
    boolean (*send_to_rtcp_socket)(struct rtp_repair_recv_t_ *this,     \
                                   in_addr_t remote_addr,               \
                                   in_port_t remote_port,               \
                                   char *buf,                           \
                                   uint16_t len);                       \



#define RTP_REPAIR_RECV_INFO                                            \
    RTP_PTP_INFO                                                        \
    /**                                                                 \
     * Member timeout event.                                            \
     */                                                                 \
    vqec_event_t *member_timeout_event;                                 \
    /**                                                                 \
     * Report interval timeout event.                                   \
     */                                                                 \
    vqec_event_t *send_report_timeout_event;                            \
    /**                                                                 \
     * RTCP transmit socket.                                            \
     */                                                                 \
    int repair_rtcp_xmit_sock;                                          \
    /**                                                                 \
     * Dataplane input stream identifier.                               \
     */                                                                 \
    vqec_dp_streamid_t dp_is_id;                                        \
    /**                                                                 \
     * Cached view of dataplane rtp sources.                            \
     */                                                                 \
    vqec_rtp_source_id_t src_ids[VQEC_DP_RTP_MAX_KNOWN_SOURCES];        \
    /**                                                                 \
     * Number of cached sources.                                        \
     */                                                                 \
    int32_t num_src_ids;                                                \
    /**                                                                 \
     * Any SSRC filter that may be operational.                         \
     */                                                                 \
    uint32_t src_ssrc_filter;                                           \
    /**                                                                 \
     * Is SSRC filter set.                                              \
     */                                                                 \
    boolean ssrc_filter_en;                                             \
    /**                                                                 \
     * Existing dataplane session state.                                \
     */                                                                 \
    vqec_rtp_session_state_t state;                                     \
    /*                                                                  \
     * Repair rtcp socket.                                              \
     */                                                                 \
    vqec_recv_sock_t *repair_rtcp_sock;                                 \
    /**                                                                 \
     * Parent channel object.                                           \
     */                                                                 \
    struct vqec_chan_ *chan;                                            \
    /**                                                                 \
     * RTCP XR statistics.                                              \
     */                                                                 \
    rtcp_xr_stats_t xr_stats;                                           \
    /**                                                                 \
     * Sent RTCP XR statistics in the last receiver report.             \
     */                                                                 \
    boolean last_xr_sent;                                       


typedef struct rtp_repair_recv_methods_t_ {
    RTP_REPAIR_RECV_METHODS 
} rtp_repair_recv_methods_t;


/*
 * Function to init the rtp repair recv module
 */
boolean rtp_repair_recv_init_module(uint32_t max_sessions);

/**
 * rtp_repair_recv_t
 */
typedef struct rtp_repair_recv_t_ {
    rtp_repair_recv_methods_t * __func_table;
    RTP_REPAIR_RECV_INFO
} rtp_repair_recv_t;


/* Prototypes...  create, plus all the overloading functions... */

rtp_repair_recv_t *rtp_create_session_repair_recv(vqec_dp_streamid_t dp_id,
                                                  in_addr_t src_addr,
                                                  char *cname,
                                                  in_addr_t fbt_ip_addr,
                                                  uint16_t  *recv_rtcp_port,
                                                  uint16_t  send_rtcp_port,
                                                  uint32_t  local_ssrc,
                                                  rtcp_bw_cfg_t *rtcp_bw_cfg,
                                                  rtcp_xr_cfg_t *rtcp_xr_cfg,
                                                  in_addr_t orig_source_addr,
                                                  uint16_t orig_source_port, 
                                                  uint32_t sock_buf_depth,
                                                  uint8_t rtcp_dscp_value,
                                                  uint8_t rtcp_rsize,
                                                  struct vqec_chan_ *parent_chan);

boolean
rtp_update_session_repair_recv(rtp_repair_recv_t *p_repair_recv,
                               in_addr_t fbt_ip_addr,
                               uint16_t  send_rtcp_port);

void rtp_delete_session_repair_recv(rtp_repair_recv_t **p_repair_recv_sess, 
                                   boolean deallocate);

void rtp_show_session_repair_recv(rtp_repair_recv_t *p_sess);

uint32_t rtcp_construct_report_repair_recv(rtp_session_t   *p_sess,
                                           rtp_member_t    *p_source, 
                                           rtcptype        *p_rtcp,
                                           uint32_t         bufflen,
                                           rtcp_pkt_info_t *p_pkt_info,
                                           boolean         reset_xr_stats);

void rtp_session_repair_recv_send_bye(rtp_repair_recv_t *pp_repair_recv);

vqec_chanid_t
rtp_repair_session_chanid(rtp_repair_recv_t *p_repair);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __rtp_repair_recv_h__ */
