/*
 *------------------------------------------------------------------
 * Video Quality Reporter
 *
 * January 11, 2007, Carol Iturralde
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _VIDEO_QUALITY_REPORTER_H_
#define _VIDEO_QUALITY_REPORTER_H_

#include "rtcp.h"
#include "rtp_session.h"

/*
 * The VQR "export header" is prefixed to RTCP compound messages
 * that are "exported" for VQM purposes to a server, which typically
 * stores them for later analysis.
 *
 * This format of this header is intentionally similar to that of
 * other RTCP messages, in the hope that it will simplify the decoding
 * of the entire exported message, which consists of the export header
 * followed by the original compound message.
 */

typedef struct vqr_hdr_t_ {
    uint16_t params;        /* version:2 padding:1 count:5 packet type:8 */
                        /* the name params is used so we can 
                           reuse RTCP header macros */
                        /* count is reused as a 'subtype' field, 
                           as is done with the RTCP APP msg */
    uint16_t len;        /* length of this header ("message") in 32-bit words,
                           excluding this 4-byte prefix (params/len) */
    ipaddrtype chan_addr; /* dest IP addr of original (primary)
                             multicast stream for the channel */
    uint16_t   chan_port; /* dest UDP port for original multicast stream */
    uint8_t    sndr_role; /* role of the sender of the original message:
                             takes on vqr_hdr_role_e values */
    uint8_t    rcvr_role; /* role of the rcvr of the original message:
                             takes on vqr_hdr_role_e values */
    uint32_t   ntp_upper; /* NTP timestamp for when the original message
                             was sent: upper half */
    uint32_t   ntp_lower; /* NTP timestamp for when the original message
                             was sent: lower half */
    ipaddrtype src_addr;  /* src IP addr of the original RTCP compound pkt */
    ipaddrtype dst_addr;  /* dst IP addr of the original RTCP compound pkt */
    uint16_t   src_port;  /* src UDP port of the original RTCP compound pkt */
    uint16_t   dst_port;  /* dst UDP port of the original RTCP compound pkt */
} vqr_hdr_t;

/*
 * VQR export header packet type:
 *
 * There MUST NOT be more than MAX_VQR_EXPORT_TYPES values defined here.
 * The values are chosen to avoid conflict with RTCP message types, and
 * with RTP packets with the marker bit set (where the following types
 * would be seen as presumably "unassigned" static RTP payload types.)
 */

#define MAX_VQR_EXPORT_TYPES 8
typedef enum {
    VQR_RTCP_REPORT  =  RTCP_SR - MAX_VQR_EXPORT_TYPES, 
} vqr_hdr_type_e;

/*
 * RTCP export header subtype:
 *
 * Identifies the type of session (stream) for which
 * the original compound RTCP packet was sent.
 */

typedef enum {
    VQR_ORIGINAL = 1,  /* original (primary) session */
    VQR_RESOURCED,     /* resourced stream */
    VQR_REXMIT,        /* retransmission (repair) stream */
} vqr_hdr_subtype_e;

/*
 * RTCP export header role:
 *
 * Identifies the role of either the sender or receiver 
 * of the original compound packet.
 */

typedef enum {
    VQR_VQEC = 1,  /* VQE Client */
    VQR_VQES,      /* VQE Server */
    VQR_SSM_DS,    /* SSM Distribution Source (head end) */
} vqr_hdr_role_e;

/*
 * VQR Return Status
 */
typedef enum {
    VQR_SUCCESS,         /**< success */
    VQR_FAILURE,         /**< generic failure */
    VQR_MALLOC_ERR,      /**< memory allocation error */
    VQR_INVALID_INDEX,   /**< invalid channel */
    VQR_ALREADY_CREATED, /**< channel already being created */
    VQR_ALREADY_INIT,    /**< channel already initialized */
    VQR_NOT_EXIST,       /**< channel does not exist */
    VQR_IN_USE           /**< channel in use */
} vqr_status_e;

/* 
 *  External API
 */
extern vqr_status_e vqr_init (in_addr_t vqm_ip_addr,
                              uint16_t  vqm_port,
                              boolean   vqm_ip_addr_configured,
                              boolean   vqm_port_configured);

extern void vqr_report_init(vqr_hdr_t *hdr,
                            vqr_hdr_subtype_e subtype,
                            in_addr_t chan_addr,
                            uint16_t  chan_port,
                            vqr_hdr_role_e sndr_role,
                            vqr_hdr_role_e rcvr_role,
                            ntp64_t orig_send_time,
                            rtp_envelope_t *orig_addrs);

extern void vqr_export(vqr_hdr_t *hdr_p,
                       uint8_t *buf_p, 
                       uint16_t len);

/* 
 * Shutdown of Video Quality Reporter 
 */
extern vqr_status_e vqr_shutdown ();


/*
 * RTCP export header manipulation functions.
 *
 * Note that the output of these functions, 
 * and the params argument, are in host order:
 * conversion to/from network order must take
 * place when getting/setting the params field in the
 * network-order header.
 */

static inline uint16_t 
vqr_hdr_get_version (uint16_t params) 
{
    return (rtcp_get_version(params));
}

static inline uint16_t
vqr_hdr_set_version (uint16_t params) 
{
    return (rtcp_set_version(params));
}

static inline uint16_t
vqr_hdr_get_subtype (uint16_t params)
{
    return (rtcp_get_count(params));
}

static inline uint16_t
vqr_hdr_set_subtype (uint16_t params, uint16_t subtype)
{
    return (rtcp_set_count(params, subtype));
}

static inline uint16_t 
vqr_hdr_get_type (uint16_t params) 
{
    return (rtcp_get_type(params));
}

static inline uint16_t 
vqr_hdr_set_type (uint16_t params, uint16_t type) 
{
    return (rtcp_set_type(params, type));
}

#define VQR_BUF_SIZE  1400

/*
 *  VQR Globals (State, Counters, ...)
 */

#define VQR_STATE_NO_CONFIG   0
#define VQR_STATE_INIT_FAILED 1
#define VQR_STATE_UP          2

typedef struct vqr_globals_ {
    char      state;
    boolean   vqm_ip_addr_config;
    boolean   vqm_port_config;
    boolean   socket_open;
    in_addr_t vqm_ip_addr;
    uint16_t  vqm_port;
    uint32_t  socket;
    uint8_t   *send_buf_p;
    uint32_t  num_reports_sent;    /* 64-bit counter instead? */
    uint32_t  num_reports_dropped; /* 64-bit counter instead? */
    uint32_t  drops_pending_export;/* drops not yet exported */
} vqr_globals_t;


/*
 *  Records Exported by VQE-S to VQM Application:
 */

#define VQR_BASE_TYPE_RTCP_REPORT      1
#define VQR_BASE_TYPE_MISSED_REPORTS   2

typedef struct vqr_base_header_ {
    uint8_t    type;
    uint8_t    flags;
    uint16_t   length;
    uint32_t   payload[0];
} vqr_base_header_t;

typedef struct vqr_rtcp_report_header_ {
    uint8_t    version;
    uint8_t    unused[3];
    in_addr_t  src_addr;
    uint32_t   payload[0];   /* start of RTCP Report */
} vqr_rtcp_report_header_t;

typedef struct vqr_missed_reports_ {
    uint32_t   num_reports_missing;
} vqr_missed_reports_t;


#endif /* _VIDEO_QUALITY_REPORTER_H_ */
