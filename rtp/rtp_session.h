/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtp_session.h - defines the different rtp sessions in an object-
 *                 oriented way.
 * 
 * Copyright (c) 2006-2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

/*
 * Prevent multiple inclusion.
 */
#ifndef __rtp_session_h__
#define __rtp_session_h__

#include "rtcp.h"
#include "rtcp_pkt_api.h"
#include "rtp_header.h"
#include "rtcp_memory.h"
#include "rtcp_bandwidth.h"
#include "rtcp_xr.h"
#include "rtcp_statistics.h"
#include "rtp_syslog.h"
#include "../include/utils/queue_plus.h"
#include "../include/utils/tree_plus.h"
#include <log/libdebug.h>

/*
 *-----------------------------------------------------------------
 *   The following are data structures for maintaining various 
 * information for an RTP/RTCP session. Each session would have 
 * an rtp_session_t data structure which carries 
 * all relevant info.
 *
 * All elements are stored in net order unless otherwise mentioned.
 *-----------------------------------------------------------------
 */

typedef struct rtp_session_t_  rtp_session_t;
typedef struct rtcp_handlers_  rtcp_handlers_t;
#define RTP_MEMBER_TIMEOUT_MULTIPLIER   5

/*
 * RTP error values. The RTP error base should be defined as an enum in a global
 * file with all the other modules' error bases.
 */
#define RTP_ERROR_BASE  0x002f0000
typedef enum {
    RTP_SUCCESS                     = RTP_ERROR_BASE,
    RTP_MEMBER_DOES_NOT_EXIST,
    RTP_MEMBER_IS_DUP,
    RTP_SSRC_EXISTS,
    RTP_MEMBER_RESOURCE_FAIL,
    RTP_SENDER_INFO_RESOURCE_FAIL,
    RTP_SDES_RESOURCE_FAIL,
    RTP_SESSION_RESOURCE_FAIL,

} rtp_error_t;


/*
 * RTP_MAX_SENDERS
 *
 * The maximum no. of senders (including the local sender, if any)
 * which we track in the rtp_source_bitmask, and allow on the 
 * senders list. I.e., if there are more senders than this, they
 * will not be treated as senders, i.e., there will be no reception
 * blocks for them in a Receiver Report. If this #define is changed 
 * to a higher value, it should be changed to 64, etc., and the
 * type of the variable rtp_source_bitmask in rtp_session_t has to be 
 * changed accordingly. 
 */
#define RTP_MAX_SENDERS       32

/*
 * RTP_MAX_SENDERS_CACHED
 * MAX value for the senders we keep track of (in detail) at any given 
 * time for an RTP/RTCP session. This includes the local sender, if any.
 * If the no of sending sources increases above this number, their
 * reception statistics are discarded, instead of being cached in the
 * member record of each member receiving from the sender.
 */
#define RTP_MAX_SENDERS_CACHED  2  /* old was 32 */

/*
 * rtp_member_flags_t 
 *
 * Special member characteristics
 */

typedef uint32_t rtp_member_flags_t;

#define RTP_MEMBER_FLAG_RCVR_ONLY  1  /* if set, this member cannot be 
                                         a sender; if not set, this
                                         member may or may not be a sender */

/*
 * Each entry of the member cache maintained for an RTP/RTCP session has the 
 * following RTP/RTCP information fields.
 */
#define  RTP_MEMBER_INFO                                                           \
    VQE_RB_ENTRY(rtp_member_t_)      member_entry;     /* node in member rb tree */    \
    VQE_TAILQ_ENTRY(rtp_member_t_)   p_member_chain;   /* Garbage or sender chain */   \
    uint32_t                ssrc; /* RTP/RTCP synchronization source id (SSRC) */  \
    ipaddrtype              rtp_src_addr;  /* src IP addr of RTP packets */        \
    uint16_t                rtp_src_port;  /* src UPD port of RTP packets */       \
    ipaddrtype              rtcp_src_addr; /* src IP addr of RTP packets */        \
    uint16_t                rtcp_src_port; /* src UPD port of RTP packets */       \
    rtp_member_flags_t      flags;                                                 \
    rtcp_memory_subtype_t subtype;       /* is a channel or client member */       \
    /*                                                                             \
     * SDES items stored from RTCP SDES packets.                                   \
     */                                                                            \
    char                    *sdes[RTCP_SDES_MAX + 1]; /* SDES items */               \
                                                                                   \
    /*                                                                             \
     * Gleaned info from sender report.                                            \
     */                                                                            \
    uint32_t                 sr_ntp_h;      /* ntp timestamp high 32 bits*/           \
    uint32_t                 sr_ntp_l;      /* ntp timestamp low 32 bits*/            \
    uint32_t                 sr_timestamp;  /* media timestamp */                     \
    uint32_t                 sr_npackets;         /* number of packets sent */            \
    uint32_t                 sr_nbytes;         /* no of bytes sent */                  \
                                                                                   \
    /*                                                                             \
     * Timestamps kept locally for ctrl and data packets                           \
     */                                                                            \
    abs_time_t         rcv_ctrl_ts; /* sender's ts from last ctrl packet */     \
    abs_time_t         rcv_sr_ts;   /* sender's ts from last sender report*/    \
                                                                                   \
    /*                                                                             \
     * These are the RTP stats for this remote member received locally.            \
     * The RTP recv handler should update these variables, either                  \
     * synchronously or asynchronously.                                            \
     */                                                                            \
     rtp_hdr_source_t  rcv_stats;                                                  \
                                                                                   \
    /*                                                                             \
     * These could be receiving or sending stats. If this entry is for             \
     * the local sender, these are sending stats. If this member is a              \
     * remote member, these are receiving stats measured locally.                  \
     * The RTP recv handler should update these variables, either                  \
     * either in the IOS or in the DSP, depending on where the RTP                 \
     * sending/receiving application resides.                                      \
     */                                                                            \
    uint32_t                 packets;       /* sent/rcv packet count */               \
    uint32_t                 bytes;         /* sent/rcv byte count */                 \
    uint32_t                 packets_prior; /* paks sent/rcv at last int */           \
    uint32_t                 bytes_prior;   /* bytes sent/rcv at last int */          \
    uint32_t                     rtp_timestamp; /*rtp timestamp of last sent rtp pak*/    \
    ntp64_t                     ntp_timestamp; /*ntp timestamp of last sent rtp pak*/    \
                                                                                    \
    /*                                                                              \
     * from the Group and Average Packet Size subreport                             \
     * of an RSI message (if any) received from this member                         \
     */                                                                             \
    uint32_t                rtcp_nmembers_reported;                                 \
    int32_t                 rtcp_avg_size_reported;                                 \
    /*                                                                             \
     * The following is info from RR blocks.                                       \
     *                                                                             \
     *  Each member can keep track of the statistics                               \
     *  of a maximum of max_senders_cached sending sources per session.            \
     * "pos" implies the relative bit position of this source(Sender) in the       \
     *  variable "rtp_source_bitmask", which keeps track of total senders          \
     *  per session. For receivers the value of pos is set to -1.                  \
     *                                                                             \
     * If we are interested in only in the reception stats of the local            \
     * sender at various receivers, then that can be accessed from                 \
     * sender_info[local_source->pos] or sender_info[0]. By default,               \
     * pos 0 is reserved for the local sender.                                     \
     */                                                                            \
    rtcp_rr_t             *sender_info;                                            \
    int32_t                   pos;                                                 \
    /* The following is data structure used for RTCP XR stats */                   \
    rtcp_xr_stats_t          *xr_stats;      /* Stats for Pre ER RTCP XR */        \
    rtcp_xr_post_rpr_stats_t *post_er_stats; /* RTCP stats for Post ER including   \
                                                Post ER Loss RLE */                \

/* RTP member base class */
typedef struct rtp_member_t_ {
    RTP_MEMBER_INFO
} rtp_member_t;
/*
 * RTP conflict table entry (for resolving collisions and loops)
 */
typedef struct rtp_conflict {
    ipaddrtype     addr;
    abs_time_t  ts;
} rtp_conflict_t;

#define RTP_MAX_CONFLICT_ITEMS  5

/*
 * rtp_conflict_type_t
 *
 * Describes the type of conflict with an existing member,
 * and the characteristics of that member:
 *
 * a PARTIAL conflict mean that the SSRCs are the same,
 * but the CNAMES are different.
 * a FULL conflict means both SSRC and CNAME are the same,
 * but one member (or potential member) is "receiver only",
 * and the other is not.
 * 
 * The other terms describe the characteristics of the
 * existing member with which there is a conflict:
 *
 * LOCAL means the member is the local source;
 * REMOTE means the member is not the local source.
 * SNDR means the member is allowed to send;
 * RCVR means the member is "receiver only".
 *
 * Since this value is used an index, the lowest
 * value should be always be zero, and RTP_CONFLICT_MAX
 * must be set to the highest value.
 */

typedef enum rtp_conflict_type_t_ {
    RTP_CONFLICT_MIN = 0,
    RTP_CONFLICT_PARTIAL_LOCAL_SNDR = RTP_CONFLICT_MIN,
    RTP_CONFLICT_FULL_LOCAL_SNDR,
    RTP_CONFLICT_PARTIAL_LOCAL_RCVR,
    RTP_CONFLICT_FULL_LOCAL_RCVR,
    RTP_CONFLICT_PARTIAL_REMOTE_SNDR,
    RTP_CONFLICT_FULL_REMOTE_SNDR,
    RTP_CONFLICT_PARTIAL_REMOTE_RCVR,
    RTP_CONFLICT_FULL_REMOTE_RCVR,
    RTP_CONFLICT_MAX = RTP_CONFLICT_FULL_REMOTE_RCVR
} rtp_conflict_type_t;

/*
 * rtp_conflict_info_t
 *
 * Information about a particular conflict
 */

typedef struct rtp_conflict_info_t_ {
    int num_conflicts;
    /*
     * array of ptrs to conflicting members, 
     * indexed by conflict type: 
     * a NULL ptr indicates no conflict of that type.
     */
    rtp_member_t *cmember[RTP_CONFLICT_MAX+1];
} rtp_conflict_info_t;

/*
 * rtp_source_id_t
 *
 * This 3-tuple identifies a sending data source
 * (within an RTP session)
 */
typedef struct rtp_source_id_t_ {
    uint32_t   ssrc;
    ipaddrtype src_addr;  /* from RTP packets: in network order */
    uint16_t   src_port;  /* from RTP packets: in network order */
} rtp_source_id_t;

/*
 * rtp_member_id_type_t
 *
 * Defines the type of member identification in use (for member lookup, etc.)
 */

typedef enum rtp_member_id_type_t_ {
    RTP_SMEMBER_ID_RTP_DATA = 1,   /* id info is from an RTP packet,
                                      from an (implicitly) sending member */
    RTP_SMEMBER_ID_RTCP_DATA,      /* id info is from an RTCP packet,
                                      from a (potentially) sending member */
    RTP_RMEMBER_ID_RTCP_DATA       /* id info is from an RTCP packet,
                                      from a "receive only" participant */
} rtp_member_id_type_t;

/*
 * rtp_member_id_t
 *
 * Member identification data
 */

typedef struct rtp_member_id_t_ {
    rtp_member_id_type_t type;  /* source of id data: RTP or RTCP packet,
                                   from a "receive only" or regular member */
    rtcp_memory_subtype_t subtype; /* indicates association of member to
                                    * either a provisioned channel or a 
                                    * remote client */
    uint32_t   ssrc;      /* from an RTP or RTCP packet: in host order */
    ipaddrtype src_addr;  /* from an RTP or RTCP packet: in network order */
    uint16_t   src_port;  /* from an RTP or RTCP packet: in network order */
    char      *cname;     /* CNAME: null-terminated string, taken from an
                             RTCP packet; or NULL, if id info is taken from
                             an RTP packet */
} rtp_member_id_t;
    
/* rtp_ssrc_selection_t
 *
 * Method used to select an SSRC (for a local member)
 */

typedef enum rtp_ssrc_selection_t_ {
    RTP_SELECT_RANDOM_SSRC = 1, /* select a strongly random SSRC */
    RTP_USE_SPECIFIED_SSRC,     /* use SSRC given in rtp_member_id_t */
} rtp_ssrc_selection_t;
    
/*
 * rtp_envelope_t
 *
 * Represents addressing information for an RTP or RTCP packet
 * All information is in network order.
 */

typedef struct {
    ipaddrtype src_addr;
    uint16_t   src_port;
    ipaddrtype dst_addr;
    uint16_t   dst_port;
} rtp_envelope_t;

/*
 * This enum is used to identify the caller with a unique ID for inform
 * registries.
 * KK !! Decide whether to get rid of IOS-type apps
 */
typedef enum rtpapp_type_ {
    RTPAPP_DEFAULT = 0,
    RTPAPP_NOR,
    RTPAPP_MRM,
    RTP_APP_VOIPRTP,
    RTPAPP_VAMRTP = 32, 
    RTPAPP_STBRTP,
    RTPAPP_DVAMRTP,
    RTPAPP_MAX
} rtpapp_type;

/*
 * rtp_session_id_t 
 *
 * Session id for RTP sessions, used for debug filtering.
 * It is NOT guaranteed to uniquely identify a session.
 */
typedef uint64_t rtp_session_id_t;

typedef struct rtp_config_t_ {
    rtp_session_id_t session_id;
    ipsocktype rtp_sock;
    ipsocktype rtcp_sock;
    ipaddrtype rtcp_dst_addr;
    uint16_t   rtcp_dst_port;
    rtcp_bw_cfg_t rtcp_bw_cfg;
    rtcp_xr_cfg_t rtcp_xr_cfg;
    uint32_t senders_cached;
    rtpapp_type app_type;
    boolean rtcp_rsize;
    void *app_ref;
} rtp_config_t;

/* Hash List for members. Note the hash size (number of buckets) is dynmaically
 * allocated
 */
typedef struct rtp_member_hash_t_ {
    VQE_LIST_HEAD(,rtp_member_t_) collision_head;
} rtp_member_hash_t;

/* 
 * New hash algorithm (old hash algorithm was nethash() 
 * see rtcp_memory.h for possible "size" values 
 */
#define RTP_HASH(ssrc,size)         ((ssrc) % size)

#define RTP_HASH_BUCKET_HEAD(p_sess, hash)  \
    (rtp_member_hash_t *)(p_sess->member_hash + hash);

typedef VQE_TAILQ_HEAD(garbage_list_t_,rtp_member_t_) garbage_list_t;
typedef VQE_TAILQ_HEAD(senders_list_t_,rtp_member_t_) senders_list_t;
typedef VQE_RB_HEAD(rtp_member_tree_t_, rtp_member_t_) rtp_member_tree_t;

/* 
 * Each RTP/RTCP session has the following structure to maintain 
 * all RTP/RTCP information. This is the basic information. The different types
 * of RTP session objects will have additional, different pieces of information
 */
/*
 * Since comments don't work inside a macro, gather the interesting comments
 * here.
 * The app_type and app_ref are the application, which can receive/send app
 * packets.
 * rtp_source_bitmask keeps track of indices for
 * a max of 32 active senders in its bit fields.
 * 
 * max_senders_cached is the maximum no of senders,
 * the rr stats of which we want to cache in every element
 * in this session. This variable decides the size of the
 * the sender_info block in each member.
 *
 * (1 is by default, implying all members' reception reports for the
 * local source is cached.   Ie, bit position 0 in the bitmask is
 * reserved for local sender.
 * If the value is 10, each member's reception reports
 * for a maximum of 10 senders will be cached. The maximum is 32 to save
 * memory.
 * The rtp_local_source is a pointer to the local member's info
 * if we are a member of the session.
 * The rtp_timestamp is the sent rtp timestamp.
 * Conflict resolution table. This is a list of
 * conflict addresses and timestamps of conflicts.
 * The RTCP average size estimate is updated each time we
 * we receive an RTCP report or send one.
 * rtcp_nmembers is the total session members during this interval
 * rtcp_nsources is the total active sources during this interval
 * rtcp_interval is the timeout for scheduling the RTCP packet
 */

#define RTP_SESSION_INFO                                                      \
    ipsocktype rtp_socket;                                                   \
    ipsocktype rtcp_socket;                                                  \
    rtp_envelope_t send_addrs;                                               \
    rtp_session_id_t session_id;                                             \
    rtpapp_type app_type;                                                     \
    void *app_ref;                                                            \
    rtcp_memory_t *rtcp_mem;                                                  \
    rtp_member_tree_t member_tree;                                            \
    boolean rtcp_rsize;                                                       \
    uint32_t           rtp_source_bitmask;                                       \
    uint8_t           max_senders_cached;                                       \
    rtp_member_t    *rtp_local_source;                                        \
    uint32_t                rtp_timestamp;                                            \
    rtp_conflict_t rtp_conflict_table[RTP_MAX_CONFLICT_ITEMS];                \
    rtcp_bw_info_t rtcp_bw;                                                       \
    rtcp_xr_cfg_t rtcp_xr_cfg;                                                    \
    rtcp_jitter_data_t intvl_jitter;                                             \
    uint32_t rtcp_nmembers;                                                      \
    uint32_t rtcp_nmembers_learned;                                              \
    uint32_t rtcp_pmembers;                                                      \
    uint32_t rtcp_nsenders;                                                      \
    uint32_t rtcp_interval;                                                      \
    boolean  we_sent;                                                            \
    rtcp_session_stats_t  rtcp_stats;                                            \
    rtcp_session_stats_t *rtcp_child_stats;                                         \
    rtp_hdr_session_t     rtp_stats;                                             \
    uint32_t           badcreate;                                                 \
    uint32_t           loop_own;                                                  \
    uint32_t           collision_own;                                             \
    uint32_t           collision_third_party;                                     \
    uint32_t           loop_third_party;                                          \
    rtcp_handlers_t     *rtcp_handler;                                            \
    abs_time_t         next_send_ts; /* time to send next RTCP report   */     \
    abs_time_t         prev_send_ts; /* Time when prev RTCP report was sent*/  \
    boolean                initial; /* Initial is set if no RTCP is sent yet */   \
    garbage_list_t  garbage_list;                                                 \
    senders_list_t  senders_list;                                               \
    rtcp_xr_ma_stats_t xr_ma_stats;                                               \
    boolean xr_ma_sent;             /* Set to true once the XR MA is sent*/       \
    rtcp_xr_dc_stats_t xr_dc_stats;                                               \
    abs_time_t last_rtcp_pkt_ts;    /* Updated by rtcp_recv_timestamped_packet*/ \




     /****************************************
     * Note that the data struct of members is NOT part of the base session.
     * This is because the members list data struct will be handled differently
     * for the different types of sessions.
     ****************************************/
#define RTP_SESSION_METHODS                                                       \
    rtp_member_t * (*rtp_remove_member_by_id) (rtp_session_t *p_sess,             \
                                               rtp_member_id_t *p_member_id,      \
                                               boolean destroy_flag);             \
    void           (*rtp_delete_member) (rtp_session_t *p_sess,                   \
                                         rtp_member_t **pp_source,                \
                                         boolean      destroy_flag);              \
    uint32_t        (*rtp_create_member) (rtp_session_t *p_sess,                  \
                                          rtp_member_id_t *p_member_id,           \
                                          rtp_member_t **pp_member);              \
    rtp_error_t     (*rtp_lookup_member) (rtp_session_t *p_sess,                  \
                                          rtp_member_id_t *p_member_id,           \
                                          rtp_member_t **pp_member,               \
                                          rtp_conflict_info_t *p_conflict);       \
    boolean (*rtp_resolve_collision_or_loop) (rtp_session_t *p_sess,              \
                                              rtp_member_id_t *p_member_id,       \
                                              rtp_conflict_info_t *p_conflict);   \
    rtp_error_t (*rtp_lookup_or_create_member) (rtp_session_t *p_sess,            \
                                                rtp_member_id_t *p_member_id,     \
                                                rtp_member_t **pp_member);        \
    rtp_member_t * (*rtp_create_local_source) (rtp_session_t *p_sess,             \
                                               rtp_member_id_t *p_member_id,      \
                                               rtp_ssrc_selection_t ssrc_type);   \
    uint32_t      (*rtp_choose_local_ssrc) (rtp_session_t *p_sess);               \
    void       (*rtp_update_local_ssrc) (rtp_session_t *p_sess,                   \
                                         uint32_t old_ssrc, uint32_t new_ssrc);   \
    rtp_error_t  (*rtp_new_data_source) (rtp_session_t *p_sess,                   \
                                         rtp_source_id_t *p_srcid);               \
    void         (*rtp_data_source_demoted) (rtp_session_t *p_sess,               \
                                             rtp_source_id_t *p_srcid,            \
                                             boolean deleted);                    \
    void         (*rtp_cleanup_session) (rtp_session_t *p_sess);                  \
    rtp_session_t * (*rtp_get_parent_session) (rtp_session_t *p_sess);            \
    boolean          (*rtp_recv_packet) (rtp_session_t *p_sess,                   \
                                         abs_time_t rcv_ts,                       \
                                         ipaddrtype src_addr,                     \
                                         uint16_t src_port,                       \
                                         char *p_buf,                             \
                                         uint16_t len);                           \
    void            (*rtcp_recv_packet) (rtp_session_t *p_sess,                   \
                                         abs_time_t rcv_ts,                       \
                                         boolean from_recv_only_member,           \
                                         rtp_envelope_t *addrs,                   \
                                     char *       p_buf,                          \
                                     uint16_t     len);                           \
    void            (*rtcp_packet_rcvd) (rtp_session_t *p_sess,                   \
                                         boolean from_recv_only_member,           \
                                         rtp_envelope_t *p_env,                   \
                                         ntp64_t orig_send_time,                  \
                                         uint8_t *p_buf,                          \
                                         uint16_t len);                           \
    boolean  (*rtcp_process_packet) (rtp_session_t *p_sess,                       \
                                     boolean       from_recv_only_member,         \
                                     ipaddrtype    src_addr,                      \
                                     uint16_t     src_port,                       \
                                     char *       p_buf,                          \
                                     uint16_t     len);                           \
    uint32_t (*rtcp_construct_report) (rtp_session_t   *p_sess,                   \
                                       rtp_member_t    *p_source,                 \
                                       rtcptype        *p_rtcp,                   \
                                       uint32_t         bufflen,                  \
                                       rtcp_pkt_info_t *p_pkt_info,               \
                                       boolean         reset_xr_stats);           \
    uint32_t     (*rtcp_send_report)  (rtp_session_t   *p_sess,                   \
                                       rtp_member_t    *p_source,                 \
                                       rtcp_pkt_info_t *p_pkt_info,               \
                                       boolean         re_schedule,               \
                                       boolean         reset_xr_stats);           \
    void            (*rtcp_packet_sent) (rtp_session_t *p_sess,                   \
                                         rtp_envelope_t *p_env,                   \
                                         ntp64_t orig_send_time,                  \
                                         uint8_t *p_buf,                          \
                                         uint16_t len);                           \
    uint32_t       (* rtcp_report_interval) (rtp_session_t    *p_sess,            \
                                             boolean          we_sent,            \
                                             boolean          add_jitter);        \
    void       (* rtp_update_sender_stats) (rtp_session_t *p_sess,                \
                                            rtp_member_t *p_source);              \
    void       (* rtp_update_receiver_stats) (rtp_session_t *p_sess,              \
                                              rtp_member_t *p_source,             \
                                              boolean reset_xr_stats);            \
    void     (* rtp_update_stats) (rtp_session_t *p_sess,                         \
                                   boolean reset_xr_stats);                       \
    void      (* rtp_session_timeout_slist)(rtp_session_t *p_sess);               \
    void     (* rtp_session_timeout_glist) (rtp_session_t *p_sess);               \
    void     (* rtp_session_timeout_transmit_report)(rtp_session_t *p_sess);




/*  KK !! left out rtcp_update_sr_packet() and rtcp_update_rr-packet(). 
 *  They are not useful. */

typedef struct rtp_session_methods_t_ {
    /*
     * RTP session methods are the dynamic function pointers which
     * will be filled in by each specific type of session
     */
    RTP_SESSION_METHODS
} rtp_session_methods_t;

/*
 * Write the base class methods for an rtp session
 * into *table.  Base class uses this once to construct the base class
 * method table.  Each subclass uses it once to fill in the base class
 * defined portion of the table.
 */
void rtp_session_set_methods (rtp_session_methods_t * table);


/* Function handlers for different RTCP types */
typedef 
rtcp_parse_status_t (*rtcp_type_processor)(rtp_session_t  *p_sess,
                                           rtp_member_t   *p_source,
                                           ipaddrtype     src_addr,
                                           uint16_t       src_port,
                                           rtcptype       *p_rtcp,
                                           uint16_t       count,
                                           uint8_t        *p_end_of_frame);

struct rtcp_handlers_ {
    rtcp_type_processor proc_handler[RTCP_MAX_TYPE+1];
} ;

void rtp_session_set_rtcp_handlers(rtcp_handlers_t *table);
void rtp_session_init_module(void);

/*
 * Write the base class methods for an rtp session
 * into *table.  Base class uses this once to construct the base class
 * method table.  Each subclass uses it once to fill in the base class
 * defined portion of the table.
 */

struct rtp_session_t_ {
    rtp_session_methods_t * __func_table;
    RTP_SESSION_INFO
};

/*
 * debug utility (see rtp_syslog.c)
 */
extern boolean rtp_debug_check(int debug_flag,
                               rtp_session_t *session,
                               rtp_member_t *member);
extern char *rtp_debug_prefix(rtp_session_t *session,
                              rtp_member_t *member,
                              char *buffer,
                              int buflen);
/*
 * Lookup utilities
 */
extern rtcptype *rtcp_find_pkttype(rtcptype *pkt, int plen, 
                                   rtcp_type_t pkttype, int *outlen);

extern uint8_t *rtcp_find_cname(rtcptype *p_rtcp, int len, uint32_t ssrc,
                                uint32_t *cname_len);

extern rtcptype *rtcp_find_bye(rtcptype *p_rtcp, int len, uint32_t ssrc);

/*
 * RTCP bandwidth check utility
 */
extern boolean rtcp_may_send(rtp_session_t *p_sess);

/*
 * inlines
 */

static inline void rtp_init_config(rtp_config_t *rtp_config, 
                                   rtpapp_type apptype)
{

    memset(rtp_config, 0, sizeof(rtp_config_t));
    rtp_config->app_type = apptype;
    rtcp_init_bw_cfg(&rtp_config->rtcp_bw_cfg);
    rtcp_init_xr_cfg(&rtp_config->rtcp_xr_cfg);
}

/*
 * rtp_taddr_to_session_id
 *
 * Convert transport address to RTP session id
 */
static inline 
rtp_session_id_t rtp_taddr_to_session_id (ipaddrtype addr,
                                          uint16_t   port)
{
    return (CONCAT_U32_U16_TO_U64(addr, port));
}

/*
 * rtp_get_session_id_addr
 *
 * Retrieve the IP address in an RTP session id
 */
static inline 
uint16_t rtp_get_session_id_addr (rtp_session_id_t id)
{
    return ((ipaddrtype)(id >> 16));
}

/*
 * rtp_get_session_id_port
 *
 * Retrieve the UDP port in an RTP session id
 */
static inline 
uint16_t rtp_get_session_id_port (rtp_session_id_t id)
{
    return ((uint16_t)(id & 0xffff));
}

/*
 *-----------------------------------------------------------------
 * Base RTP session function prototypes.
 *-----------------------------------------------------------------
 */
/* Constructor and Destructor */
boolean rtp_create_session_base (rtp_config_t      *config, 
                                 rtp_session_t     *p_sess);
void rtp_delete_session_base (rtp_session_t **pp_sess);


/* Member functions */
uint32_t  rtp_create_member_base(rtp_session_t  *p_sess,
                                 rtp_member_id_t *p_member_id,
                                 rtp_member_t  **p_member);

rtp_error_t rtp_lookup_member_base(rtp_session_t *p_sess,
                                   rtp_member_id_t *p_member_id,
                                   rtp_member_t **pp_member,
                                   rtp_conflict_info_t *p_conflict);

rtp_error_t rtp_lookup_or_create_member_base(rtp_session_t *p_sess,
                                             rtp_member_id_t *p_member_id,
                                             rtp_member_t  **pp_member);

uint32_t rtp_choose_local_ssrc_base (rtp_session_t   *p_sess);


void rtp_delete_member_base ( rtp_session_t *p_sess,
                               rtp_member_t **pp_source,
                               boolean      destroy_flag);

rtp_member_t *rtp_remove_member_by_id_base(rtp_session_t *p_sess,
                                           rtp_member_id_t *p_member_id,
                                           boolean destroy_flag);

void rtp_cleanup_session_base (rtp_session_t *p_sess);
rtp_session_t *rtp_get_parent_session_base(rtp_session_t *p_sess);

rtp_member_t *rtp_create_local_source_base(rtp_session_t *p_sess,
                                           rtp_member_id_t *p_member_id,
                                           rtp_ssrc_selection_t ssrc_type);

boolean rtp_resolve_collision_or_loop_base (rtp_session_t *p_session,
                                            rtp_member_id_t *p_member_id,
                                            rtp_conflict_info_t *p_info);

void rtp_update_local_ssrc_base (rtp_session_t *p_sess, 
                                 uint32_t         old_ssrc,
                                 uint32_t         new_ssrc);

rtp_error_t rtp_new_data_source_base (rtp_session_t   *p_sess,
                                      rtp_source_id_t *p_srcid);

void rtp_data_source_demoted_base(rtp_session_t   *p_sess,
                                  rtp_source_id_t *p_srcid,
                                  boolean          deleted);

boolean rtp_recv_packet_base (rtp_session_t *p_sess,
                              abs_time_t    rcv_ts,
                              ipaddrtype    src_addr,
                              uint16_t      src_port,
                              char          *p_buf,
                              uint16_t        len);

void rtcp_recv_packet_base (rtp_session_t *p_sess,
                            abs_time_t    rcv_ts,
                            boolean       from_recv_only_member,
                            rtp_envelope_t *addrs,
                            char           *p_buf,
                            uint16_t        len);
void rtcp_packet_rcvd_base(rtp_session_t *p_sess,
                           boolean from_recv_only_member,
                           rtp_envelope_t *p_env,
                           ntp64_t orig_send_time,
                           uint8_t *p_buf,
                           uint16_t len);
boolean rtcp_process_packet_base(rtp_session_t *p_sess,
                                 boolean       from_recv_only_member,
                                 ipaddrtype    src_addr,
                                 uint16_t      src_port,
                                 char           *p_buf,
                                 uint16_t        len);
uint32_t rtcp_report_interval_base(rtp_session_t    *p_sess,
                                   boolean          we_sent,
                                   boolean          add_jitter);
uint32_t rtcp_construct_report_base (rtp_session_t   *p_sess,
                                     rtp_member_t    *p_source, 
                                     rtcptype        *p_rtcp,
                                     uint32_t         bufflen,
                                     rtcp_pkt_info_t *p_pkt_info,
                                     boolean          reset_xr_stats);
void rtp_update_data_statistics_base(rtp_session_t *p_sess, 
                                     rtp_member_t *p_source);
void rtp_update_sender_stats_base(rtp_session_t *p_sess, 
                                  rtp_member_t *p_source);
void rtp_update_receiver_stats_base(rtp_session_t *p_sess, 
                                    rtp_member_t *p_source,
                                    boolean reset_xr_stats);
void rtp_update_stats_base(rtp_session_t *p_sess, 
                           boolean reset_xr_stats);
uint32_t rtcp_send_report_base (rtp_session_t   *p_sess,
                                rtp_member_t    *p_source, 
                                rtcp_pkt_info_t *p_pkt_info,
                                boolean          re_schedule,
                                boolean          reset_xr_stats);
void rtcp_packet_sent_base(rtp_session_t *p_sess,
                           rtp_envelope_t *p_env,
                           ntp64_t orig_send_time,
                           uint8_t *p_buf,
                           uint16_t len);
void rtp_session_timeout_slist_base(rtp_session_t *p_sess);
void rtp_session_timeout_glist_base (rtp_session_t *p_sess);
void rtp_session_timeout_transmit_report_base (rtp_session_t *p_sess);


#endif /* to prevent multiple inclusion */
