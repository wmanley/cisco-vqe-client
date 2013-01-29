/*
 * vqec_rtp.h - API definition for RTP helper routines.
 *
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef VQEC_RTP_H
#define VQEC_RTP_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <netinet/in.h>
#include <cfg/cfgapi.h>
#include <rtp/rtp_ssm_rsi_rcvr.h>
#include <rtp/rtcp_bandwidth.h>
#include "vqec_dp_api.h"
#include "vqec_recv_socket.h"
#include "vqec_event.h"
#include "vqec_channel_api.h"

/*
 * Constant used for sizing RTP memory allocation zones.
 * This defines the max number of entities per session.  
 * Note we allow a variable number of sessions at startup.
 * The number 3 comes from:
 *    1 for the sender, 1 for the receiver and an
 *    one for an extra sender during transition/failover.
 */
#define VQEC_RTP_MEMBERS_MAX      3

typedef enum vqec_rtcp_timer_type_ {
    VQEC_RTCP_MEMBER_TIMEOUT,
    VQEC_RTCP_SEND_REPORT_TIMEOUT,
    VQEC_RTCP_LAST_TIMEOUT
} vqec_rtcp_timer_type;


/**
 * Structure to maintain RTP pktflow sources.
 */
typedef
struct vqec_rtp_source_id_
{
    /**
     * RTP identifier for the source.
     */
    rtp_source_id_t id;
    /**
     * Last reported threshold crossing count.
     */
    uint32_t thresh_cnt;

} vqec_rtp_source_id_t;

/**
 * Control-plane rtp session state which is modified by dataplane events. 
 */
typedef
enum vqec_rtp_session_state_
{
    /**
     * Inactive after creation, in-wait for first rtp source event from dp.
     */
    VQEC_RTP_SESSION_STATE_INACTIVE_WAITFIRST,
    /**
     * An active pktflow rtp source is present.
     */
    VQEC_RTP_SESSION_STATE_ACTIVE,
    /**
     * No active pktflow rtp source is present.
     */
    VQEC_RTP_SESSION_STATE_INACTIVE,
    /**
     * Error state.
     */
    VQEC_RTP_SESSION_STATE_ERROR,
    /**
     * CP shutdown state - used for multiple byes.
     */
    VQEC_RTP_SESSION_STATE_SHUTDOWN,

} vqec_rtp_session_state_t;

/**
 * Get the ssrc of the local source.
 *
 * @param[in] rtp_session Pointer to an rtp_session
 * @param[out] ssrc SSRC of the local source.
 * @param[out] boolean TRUE if we were able to get a local_source else FALSE.
 */
boolean rtp_get_local_source_ssrc(const rtp_session_t *rtp_session,
                                  uint32_t *ssrc);

/**
 * Construct a report including the PUBPORTS message.
 * This is an overriding implementation of the rtcp_construct_report method,
 * for the "repair-recv" and "era-recv" application-level classes.
 *
 * @param[in] p_sess Pointer to session object
 * @param[in] p_source Pointer to local source member
 * @param[in] p_rtcp Pointer to start of RTCP packet in the buffer
 * @param[in] bufflen Length (in bytes) of RTCP packet buffer
 * @param[in] p_pkt_info Pointer to additional packet info 
 * (if any; may be NULL)
 * @param[in[] primary Indicates whether this is the primary or repair
 * rtp session
 * @param[in] reset_xr_stats Flag for whether to reset the RTCP XR stats
 * @param[in] rtp_port External rtp port for pubports.
 * @param[in] rtcp_port External rtcp port for pubports.
 * @param[out] uint32_t Returns length of the packet including RTCP header;
 * zero indicates a construction failure.
 */
uint32_t rtcp_construct_report_pubports(rtp_session_t   *p_sess,
                                        rtp_member_t    *p_source, 
                                        rtcptype        *p_rtcp,
                                        uint32_t         bufflen,
                                        rtcp_pkt_info_t *p_pkt_info,
                                        boolean          primary,
                                        boolean          reset_xr_stats,
                                        in_port_t        rtp_port,
                                        in_port_t        rtcp_port);

/**
 * Show RTP drop statistics.
 * 
 * @param[in] p_sess Pointer to a RTP session.
 */
void 
rtp_show_drop_stats(rtp_session_t *p_sess);

/**
 * Internal implementation of RTCP packet processing.
 *
 * @param[in] rtp_session Pointer to a repair or primary session.
 * @param[in] pak_src_addr IP source address of the RTCP packet.
 * @param[in] pak_src_port UDP source port of the RTCP packet.
 * @param[in] pak_buff Contents of the RTCP packet.
 * @param[in] pak_buff_len Length of the RTCP packet.
 * @param[in] recv_time Time at which packet is received.
 */ 
void 
rtcp_event_handler_internal_process_pak (rtp_session_t *rtp_session,
                                         struct in_addr pak_src_addr,
                                         uint16_t pak_src_port,
                                         char *pak_buff,
                                         int32_t pak_buff_len,
                                         struct timeval *recv_time);

/**
 * Front-end RTCP event handler.
 *
 * @param[in] rtp_session Pointer to a repair or primary session.
 * @param[in] rtcp_sock Input socket to be read.
 * @param[in] iobuf An IO buffer to receive RTCP packet data.
 * @param[in] iobuf_len Length of the IO buffer.
 * @param[in] is_primary_session True if the session is the primary session.
 */
void rtcp_event_handler_internal(rtp_session_t *rtp_session,
                                 vqec_recv_sock_t *rtp_sock,
                                 char *iobuf,
                                 uint32_t iobuf_len,
                                 boolean is_primary_session);

/**
   Create a RTCP timer event object.
   @param[in] to_type Type of timer to create
   @param[in] p_tv Default timeout duration
   @param[in] p_sess Pointer to session object (cached)
   @return A non-null pointer to an opaque vqec_event_t struct,
   when the call succeeds. If the call fails, the ptr is null.
 */
vqec_event_t *rtcp_timer_event_create(vqec_rtcp_timer_type to_type,
                                      struct timeval *p_tv,
                                      void *p_sess);


/*
 * vqec_set_primary_rtcp_bw
 *
 * Set the RTCP bandwidth for the primary stream,
 * from channel configuration information.
 */
void vqec_set_primary_rtcp_bw(vqec_chan_cfg_t *chan_cfg,
                              rtcp_bw_cfg_t *rtcp_bw_cfg);

/*
 * vqec_set_resourced_rtcp_bw
 *
 * Set the RTCP bandwidth for the resourced stream,
 * from channel configuration information.
 */
void vqec_set_resourced_rtcp_bw(vqec_chan_cfg_t *chan_cfg,
                                rtcp_bw_cfg_t *rtcp_bw_cfg);

/*
 * vqec_set_repair_rtcp_bw
 *
 * Set the RTCP bandwidth for the repair stream,
 * from channel configuration information.
 */
void vqec_set_repair_rtcp_bw(vqec_chan_cfg_t *chan_cfg,
                             rtcp_bw_cfg_t *rtcp_bw_cfg);

/*
 * vqec_set_primary_rtcp_xr_options
 *
 * Set the RTCP XR option for the primary stream
 * from channel configuration information.
 */
void vqec_set_primary_rtcp_xr_options(vqec_chan_cfg_t *chan_cfg,
                                      rtcp_xr_cfg_t *rtcp_xr_cfg);

/*
 * vqec_set_repair_rtcp_xr_options
 *
 * Set the RTCP XR option for the repair stream
 * from channel configuration information.
 */
void vqec_set_repair_rtcp_xr_options(vqec_chan_cfg_t *chan_cfg,
                                      rtcp_xr_cfg_t *rtcp_xr_cfg);

/**
 * Display statistics from a RTP session.
 * 
 * @param[in] is_id Input stream identifier of the session in the dataplane.
 *  It can be used to fetch the most recent copy of the statistics.
 * @param[in] p_session pointer to the RTP session.
 * @param[in] namestr "Primary" or "Repair " session name.
 * @param[in] first_pak_rx True if the session has received at least one
 * packet since creation.
 */
void
vqec_rtp_session_show(vqec_dp_streamid_t id,
                      rtp_session_t *p_session,
                      const char *namestr,
                      boolean first_pak_rx);

/**
 * Convert a session state to it's textual representation.
 *
 * @param[in] state Session state.
 * @param[out] char* Textual representation of the state.
 */
const char *
vqec_rtp_session_state_tostr(vqec_rtp_session_state_t state);

/**
 * Parse a dataplane RTP source table and find / return the first pktflow 
 * source entry in this table.
 *
 * @param[in] table Pointer to the dataplane RTP source table.
 * @param[out] vqec_dp_rtp_src_table_entry_t* Pointer to the first
 * pktflow entry encountered, or NULL otherwise.
 */
vqec_dp_rtp_src_table_entry_t *
vqec_rtp_src_table_find_pktflow_entry(vqec_dp_rtp_src_table_t *table);

/**
 * Parse a dataplane RTP source table and find the most recent active 
 * source entry in this table.
 *
 * @param[in] table Pointer to the dataplane RTP source table.
 * @param[out] vqec_dp_rtp_src_table_entry_t* Pointer to the most recent
 * active source entry encountered, or NULL otherwise.
 */
vqec_dp_rtp_src_table_entry_t *
vqec_rtp_src_table_most_recent_active_src(vqec_dp_rtp_src_table_t *table);

/**
 * Find the failover source entry in the rtp source table.
 * The failover source corresponds to an non-packetflow
 * sender whose most-recent packets are being queued by the dataplane.
 * If no such failover source exists, or if the failover source is 
 * not active, NULL will be returned 
 * 
 * @param[in] table Pointer to the RTP source table.
 * @param[out] vqec_dp_rtp_src_table_entry_t* Pointer to the failover
 * entry if there is one, null otherwise.
 */
vqec_dp_rtp_src_table_entry_t *
vqec_rtp_src_table_failover_src(vqec_dp_rtp_src_table_t *table);

/**
 * Delete a dataplane source entry for a RTP session.
 * 
 * @param[in] p_session Pointer to the RTP session
 * @param[in] id Identifier of the RTP IS in the dataplane.
 * @param[in] source_id Attributes of the source.
 * @param[out] table [optional]If the caller needs, the most recent
 * source table will be returned in this parameter.
 * @param[out] boolean Returns true if the delete succeeded, or
 * if the source-entry was not present in the dataplane. All other cases,
 * such as null-pointers, non-existent stream in the dataplane, etc. are
 * considered failures.
 */
boolean
vqec_rtp_session_dp_src_delete(rtp_session_t *p_session, 
                               vqec_dp_streamid_t id,
                               rtp_source_id_t *source_id,
                               vqec_dp_rtp_src_table_t *table);

/**
 * Fetch a copy of the source table for a RTP session.
 * 
 * @param[in] p_session Pointer to the RTP session (unused).
 * @param[in] id Identifier of the RTP IS in the dataplane.
 * @param[out] table The most recent source table will be returned
 * in this parameter.
 * @param[out] boolean Returns true if a valid table was returned,
 * false otherwise.
 */
boolean
vqec_rtp_session_dp_get_src_table(rtp_session_t *p_session, 
                                  vqec_dp_streamid_t id,
                                  vqec_dp_rtp_src_table_t *table);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VQEC_RTP_H */
