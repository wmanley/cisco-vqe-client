/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtcp.h - RTCP specific defines. 
 * 
 * Dec 1996, Manoj Leelanivas
 *
 * Copyright (c) 1996-2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */

/*
 * Prevent multiple inclusion.
 */
#ifndef __RTCP_H__
#define __RTCP_H__

#include "rtp.h"
/*
 * -------------------------------------------------------------------
 * RTCP specific definitions.
 * -------------------------------------------------------------------
 */

typedef enum rtcp_parse_status_t_ {
    RTCP_PARSE_OK,
    RTCP_PARSE_BADVER,  /* version number is incorrect */
    RTCP_PARSE_RUNT,    /* applies to a msg at the end of a compound pkt:
                           either the msg was shorter than the minimum
                           length of a msg (4 bytes), or the msg was 
                           shorter than the length indicated in the hdr */
    RTCP_PARSE_BADLEN,  /* length is not valid for the given msg type,
                           e.g., too short or long, or inconsistent with
                           count information in the hdr */
    RTCP_PARSE_UNEXP    /* msg type is valid, but either unexpected
                           in its position (only certain types are
                           allowed as the first message in a packet),
                           or unexpected by the application-level
                           derived class */
} rtcp_parse_status_t;

/* min time between RTCP reports, in usecs */
#define RTCP_MIN_TIME 5000000

/*
 * RTCP common header is given below. Look at rfc1889 for details of how
 * an RTCP compound packet is generated. It should contain at least one 
 * report, and could contain a combination of sender reports, reception
 * reports, sdes items and bye packets. The udp length is the only 
 * indication of the RTCP length.
 */
struct rtcptype_ {
    
    /*
     * Keep the fields in a single ushort and do an ntohs to avoid
     * little/big endian checks.
     */
    uint16_t params;        /* version:2 padding:1 count:5 packet type:8 */
    uint16_t len;                /* length of message in long words */

    /*
     * Synchronization src id. Could be the sender's for source report,
     * or the receiver's for reception report.
     */
    uint32_t  ssrc;       
};

/*
 * anticipated maximum compound packet size (not including IP/UDP)
 *
 * The value is derived from Ethernet MTU size (1500) - IP/UDP header (28)
 */
#define RTCP_PAK_SIZE          1472

/*
 * RTCP functions for getting packet's bitfields. 
 * -input arguments ("params", etc.) must be in host order.
 * -output (of "set" functions) must be translated to network order,
 *  before being put in the message buffer.
 */

static inline uint16_t rtcp_get_type (uint16_t params) 
{
    return((params) & 0xff);
}

static inline uint16_t rtcp_set_type (uint16_t params, uint16_t type) 
{
    return(params | (type & 0xff));
}

static inline uint16_t rtcp_get_version (uint16_t params) 
{
    return(params >> 14);
}

static inline uint16_t rtcp_set_version (uint16_t params) 
{
    return(params | (RTPVERSION << 14));
}

static inline uint16_t rtcp_get_count (uint16_t params)
{
    return((params) >> 8 & 0x1f);
}

static inline uint16_t rtcp_set_count (uint16_t params, uint16_t count)
{
    return(params | ((count & 0x1f) << 8));
}

/*
 * Types of RTCP packets (messages)
 */
typedef enum {
    RTCP_SR   =  200,
    RTCP_MIN_TYPE = RTCP_SR,        /* must be lowest known type */
    RTCP_RR   =  201,
    RTCP_SDES =  202,
    RTCP_BYE  =  203,
    RTCP_APP  =  204,
    RTCP_RTPFB = 205,
    RTCP_PSFB =  206,
    RTCP_XR   =  207,
    RTCP_RSI  =  208,
    RTCP_PUBPORTS  =  209,
    RTCP_MAX_TYPE = RTCP_PUBPORTS,  /* must be highest known type */
} rtcp_type_t;

/*
 * additional defines related to RTCP message types
 */

static inline boolean range (uint32_t min, uint32_t max, uint32_t num)
{
    return ((num >= min) && (num <= max));
}

#define RTCP_NUM_MSGTYPES           (RTCP_MAX_TYPE - RTCP_MIN_TYPE + 1)
#define RTCP_MSGTYPE_OK(type)       (range(RTCP_MIN_TYPE, RTCP_MAX_TYPE, type))
#define NOT_AN_RTCP_MSGTYPE         0

/* convert an RTCP  message type to a zero-based index */
#define RTCP_MSGTYPE_IDX(msgtype)   ((msgtype) - RTCP_MIN_TYPE)
/* convert a zero-based index to an RTCP message type */
#define RTCP_IDX_MSGTYPE(idx)       (idx + RTCP_MIN_TYPE)


/*
 * rtcp_msg_mask_t 
 *
 * A set of RTCP msg types.
 * Functions to manipulate the set include:
 *
 * rtcp_set_msg_mask
 * rtcp_clear_msg_mask
 *
 * rtcp_get_first_msg
 * rtcp_get_next_msg
 *
 * (rtcp_get_ge_msg is for internal use)
 */

/*
 * Bit 0 represents msg type RTCP_MIN_TYPE
 * Bit 1 represents msg type RTCP_MIN_TYPE + 1
 * ...
 */
typedef uint32_t rtcp_msg_mask_t;

#define RTCP_MSG_MASK_SIZE (sizeof(rtcp_msg_mask_t) * 8)

#define RTCP_MSG_MASK_0 ((rtcp_msg_mask_t) { 0 })

static inline 
void rtcp_set_msg_mask (rtcp_msg_mask_t *mask, 
                        rtcp_type_t type) 
{
    *mask |= (1 << RTCP_MSGTYPE_IDX(type));
}

static inline
boolean rtcp_is_msg_set (rtcp_msg_mask_t mask,
                         rtcp_type_t type)
{
    return ((mask & (1 << RTCP_MSGTYPE_IDX(type))) ? TRUE : FALSE);
}

static inline 
void rtcp_clear_msg_mask (rtcp_msg_mask_t *mask,
                          rtcp_type_t type) 
{
    *mask &= ~(1 << RTCP_MSGTYPE_IDX(type));
}

static inline
rtcp_type_t rtcp_get_ge_msg (rtcp_msg_mask_t mask,
                             rtcp_type_t type)
{
    int idx;

    for (idx = RTCP_MSGTYPE_IDX(type) ; idx < RTCP_MSG_MASK_SIZE ; idx++) {
        if ((1 << idx) & mask) {
            return (RTCP_MIN_TYPE + idx);
        }
    }
    return (NOT_AN_RTCP_MSGTYPE);
}

static inline
rtcp_type_t rtcp_get_first_msg (rtcp_msg_mask_t mask)
{
    return (rtcp_get_ge_msg(mask, RTCP_MIN_TYPE));
}

static inline
rtcp_type_t rtcp_get_next_msg (rtcp_msg_mask_t mask,
                               rtcp_type_t type)
{
    return (rtcp_get_ge_msg(mask, type + 1));
}

/*
 * Definitions related to individual packet (message) types
 * They appear in packet type order (SR, RR, SDES, etc.)
 */

/*
 * RTCP Sender Report (SR)
 */
typedef struct rtcp_sr_t_ {
    uint32_t  sr_ntp_h;      /* ntp timestamp high 32 bits*/
    uint32_t  sr_ntp_l;      /* ntp timestamp low 32 bits*/
    uint32_t  sr_timestamp;  /* media timestamp */
    uint32_t  sr_npackets;          /* number of packets sent */
    uint32_t  sr_nbytes;          /* no of bytes sent */
} rtcp_sr_t;

/*
 * RTCP Receiver Report (RR)
 */
typedef struct rtcp_rr_t_ {
    uint32_t rr_ssrc;        /* sender for which this rcvr is generating report */
    uint32_t rr_loss;        /* loss stats (8:fraction, 24:cumulative)*/
    uint32_t rr_ehsr;        /* ext. highest seqno received */
    uint32_t rr_jitter;        /* jitter */
    uint32_t rr_lsr;        /* timestamp from last rr from this src  */
    uint32_t rr_dlsr;        /* time from recpt of last rr to xmit time */
} rtcp_rr_t;

/*
 * RTCP Source Description (SDES)
 */

/*
 * SDES item and related defines.
 */
typedef enum {
    RTCP_SDES_END   = 0,
    RTCP_SDES_CNAME = 1,
    RTCP_SDES_MIN   = RTCP_SDES_CNAME, /* must be set to lowest value */
    RTCP_SDES_NAME  = 2,
    RTCP_SDES_EMAIL = 3,
    RTCP_SDES_PHONE = 4,
    RTCP_SDES_LOC   = 5,
    RTCP_SDES_TOOL  = 6,
    RTCP_SDES_NOTE  = 7,
    RTCP_SDES_PRIV  = 8,
    RTCP_SDES_MAX   = RTCP_SDES_PRIV   /* must be set to highest value */

} rtcp_sdes_type_t;

#define RTCP_MAX_SDES_ITEMS  (RTCP_SDES_MAX - RTCP_SDES_MIN + 1)

#define RTP_MAX_SDES_LEN     255      /* maximum text length for SDES */
#define RTCP_MAX_CNAME       RTP_MAX_SDES_LEN


/*
 * SDES packet
 */
typedef struct rtcp_sdes_chunk_t_ {
    uint32_t            ssrc;           /* first SSRC/CSRC */
    uint8_t             items[4];       /* SDES items ... */
} rtcp_sdes_chunk_t;

/*
 * index of item elements, from start of an item:
 */
#define RTCP_SDES_ITEM_TYPE 0  /* takes on rtcp_sdes_type_t values */
#define RTCP_SDES_ITEM_LEN  1  /* length in bytes of the data to follow */
#define RTCP_SDES_ITEM_DATA 2  /* "len" bytes of item data */

/*
 * RTCP BYE packet
 */
typedef struct rtcp_bye_t_ {
    uint32_t ssrc[0]; /* list of sources */
} rtcp_bye_t;

/*
 * RTCP APP packet
 */
typedef struct rtcp_app_t_ {
    uint32_t name;
    uint8_t data[0];
} rtcp_app_t;

/*
 * RTCP Feedback Packets (RTPFB/PSFB)
 */

/*
 * common feedback packet organization
 * (shared by RTPFB/PSPB)
 */

#define RTCP_GENERIC_NACK_OVERHEAD 12  /* NACK overhead in bytes */
typedef struct rtcpfbtype_ {
    struct rtcptype_ rtcp;
    uint32_t  ssrc_media_sender;
    char fci[0];  /* feedback control information */
} rtcpfbtype_t;

/*
 * The RTCP feedback RFC overrides the count field
 * with the fmt field.
 */
static inline uint16_t rtcp_get_fmt (uint16_t params)
{
    return((params) >> 8 & 0x1f);
}

static inline uint16_t rtcp_set_fmt (uint16_t params, uint16_t fmt)
{
    return(params | ((fmt & 0x1f) << 8));
}

/*
 * RTCP RTPFB packet (transport level feedback)
 */

/* RTPFB-specific format values */

typedef enum {
  RTCP_RTPFB_GENERIC_NACK = 1
} rtcp_rtpfb_fmt_t;

/*
 * Feedback Control Information (FCI) 
 * for the "generic NACK" format of an RTPFB message
 */
#define GNACK_PID_LENGTH  2    /* two octets packet ID field */
#define GNACK_BLP_LENGTH  2    /* two octets bitmap field */
typedef struct rtcp_rtpfb_generic_nack_ {
  uint16_t     pid;
  uint16_t     bitmask;
} rtcp_rtpfb_generic_nack_t;

/* 
 * RTCP PSFB packet (protocol-specific feedback)
 */

/* PSFB-specific format values */

typedef enum {
    RTCP_PSFB_PLI  = 1,  /* Picture Loss Indication */
    RTCP_PSFB_SLI  = 2,
    RTCP_PSFB_RPSI = 3,
    RTCP_PSFB_ALFB = 15
} rtcp_psfb_fmt_t;

/*
 * RTCP Extended Report (XR)
 */

/*
 * RTCP XR report block type
 */
typedef enum rtcp_xr_report_type_t_ {
    RTCP_XR_LOSS_RLE = 1,  /* Loss RLE report */
    RTCP_XR_DUP_RLE,       /* Duplicate RLE report */
    RTCP_XR_RTCP_TIMES,    /* Packet receipt times report */
    RTCP_XR_RCVR_RTT,      /* Receiver reference time report */
    RTCP_XR_DLRR,          /* DLRR report */
    RTCP_XR_STAT_SUMMARY,  /* Statistics summary report */
    RTCP_XR_VOIP_METRICS,  /* VoIP metrics report */
    RTCP_XR_BT_XNQ,        /* BT's eXtended Network Quality report */
    RTCP_XR_TI_XVQ,        /* TI eXtended VoIP Quality report */
    RTCP_XR_POST_RPR_LOSS_RLE,  /* Post ER Loss RLE report */
    RTCP_XR_MA = 200,           /* Media Acquisition report (avoid */
    RTCP_XR_DC,                 /* Diagnostic Counters report (TBD) */
    NOT_AN_XR_REPORT       /* this MUST always be LAST */
} rtcp_xr_report_type_t;

/*
 * generic XR report definition
 */
typedef struct rtcp_xr_gen_t_ {
    uint8_t  bt;                /* Report Block Type */
    uint8_t  type_specific;     /* Report Type Specific */
    uint16_t length;            /* Report Length */
} rtcp_xr_gen_t;

/*
 * RTCP XR Loss RLE report
 */
typedef struct rtcp_xr_loss_rle_t_ {
    uint8_t  bt;                /* Report Block Type */
    uint8_t  type_specific;     /* Report Type Specific */
    uint16_t length;            /* Report Length */
    uint32_t xr_ssrc;           /* SSRC of src. being reported upon */
    uint16_t begin_seq;         /* First seq. number of the report */
    uint16_t end_seq;           /* Last seq. number of the report */
    uint16_t chunk[0];          /* List of chunks */
} rtcp_xr_loss_rle_t;

/*
 * RTCP XR Statistics summary report
 */
typedef struct rtcp_xr_stat_summary_t_ {
    uint8_t  bt;                /* Report Block Type */
    uint8_t  type_specific;     /* Report Type Specific */
    uint16_t length;            /* Report Length */
    uint32_t xr_ssrc;           /* SSRC of src. being reported upon */
    uint16_t begin_seq;         /* First seq. number of the report */
    uint16_t end_seq;           /* Last seq. number of the report */
    uint32_t lost_packets;      /* No. of lost packets */
    uint32_t dup_packets;       /* No. of duplicate packets */
    uint32_t min_jitter;        /* Min. relative transit time */
    uint32_t max_jitter;        /* Max. relative transit time */
    uint32_t mean_jitter;       /* Mean relative transit time */
    uint32_t dev_jitter;        /* Standard dev. of relative transit time */
    uint8_t  min_ttl_or_hl;     /* Min. TTL or Hop Limit value */
    uint8_t  max_ttl_or_hl;     /* Max. TTL or Hop Limit value */
    uint8_t  mean_ttl_or_hl;    /* Mean TTL or Hop Limit value */
    uint8_t  dev_ttl_or_hl;     /* Standard dev. TTL or Hop Limit values */
} rtcp_xr_stat_summary_t;


/**
 * rcc_tlv_status_t
 * @brief
 * Status codes describing RCC failure reasons
 */
typedef enum rcc_tlv_status_t_ {
    ERA_RCC_NORCC=0,             /* No RCC Requested */              
    ERA_RCC_SUCCESS,             /* Success */
    ERA_RCC_BAD_REQ,             /* Bad Request */
    ERA_RCC_SERVER_RES,          /* No Server Resources Available */
    ERA_RCC_NETWORK_RES,         /* No Network Resources Available */
    ERA_RCC_NO_CONTENT,          /* No Buffered Content Available */
    ERA_RCC_CLIENT_FAIL,         /* RCC Fails on client */
} rcc_tlv_status_t;



/* 
 * RTCP XR Diagnostic Counters packet 
 */
typedef struct rtcp_xr_dc_t_ {
    uint8_t bt;
    uint8_t ver_flags;
    uint16_t length; /* length in 32 bit words - 1 */
    uint32_t xr_ssrc; /* SSRC of the primary multicast session */
    uint8_t tlv_data[0]; /* Start of the TLV data */
} rtcp_xr_dc_t;    

/*
 * RTCP XR Media Acquisition report (wire-format)
 */
typedef struct rtcp_xr_ma_t_ {
    uint8_t  bt;                /* Report Block Type */
    uint8_t  type_specific;     /* Rpt Type Spec (rcc_tlv_status_t) */
    uint16_t length;            /* Report Length */
    uint32_t xr_ssrc;           /* SSRC of primary multicast session */
    uint8_t  tlv_data[0];       /* Optional TLVs, some are vendor specific */
} rtcp_xr_ma_t;

typedef enum rtcp_xr_ma_tlv_types_t_ {
    XR_MA_TLV_T_OFFSET = 200, /* Start high to avoid conflicts once stdized */
    FIRST_MCAST_EXT_SEQ,
    SFGMP_JOIN_TIME,
    APP_REQ_TO_RTCP_REQ,
    RTCP_REQ_TO_BURST,
    RTCP_REQ_TO_BURST_END,
    APP_REQ_TO_MCAST,
    APP_REQ_TO_PRES,
    NUM_DUP_PKTS,
    NUM_GAP_PKTS,
    TOTAL_CC_TIME,
    RCC_EXPECTED_PTS,
    RCC_ACTUAL_PTS,
} rtcp_xr_ma_tlv_types_t;

/*
 * Generic RTCP XR MA TLV Block 
 */
typedef struct VAM_PACKED rtcp_xr_ma_tlv_t_ {
    uint8_t type;               /* Type of TLV block */
    uint16_t length;            /* Length of value field (in bytes) */
    uint8_t value[0];           /* Contain data for TLV */
} rtcp_xr_ma_tlv_t;

/*
 * Vendor Specific XR MA TLV Block
 */
#define RTCP_XR_ENTERPRISE_NUM_CISCO 9
typedef struct VAM_PACKED rtcp_xr_ma_vs_tlv_t_ {
    uint8_t type;               /* Type of TLV block */
    uint16_t length;            /* Length of value field (in bytes) */
    uint32_t ent_num;           /* Enterprise Number as defined by IANA */
    uint8_t value[0];           /* Contain data for TLV */
} rtcp_xr_ma_vs_tlv_t;

/*
 * RTCP Receiver Summary Information (RSI) packet
 */

typedef struct rtcp_rsi_t_ {
    uint32_t summ_ssrc;     /* summarized SSRC */
    uint32_t ntp_h;         /* ntp timestamp high 32 bits */
    uint32_t ntp_l;         /* ntp timestamp low 32 bits */
    uint8_t  data[0];       /* optional sub blocks */
} rtcp_rsi_t;

/*
 * RSI packet sub-report (subblock) types
 */

typedef enum rtcp_rsi_subrpt_t_ {
    RTCP_RSI_V4FAB = 0,    /* IPv4 Feedback Address Target sub-report */
    RTCP_RSI_V6FAB,        /* IPv6 Feedback Address Target sub-report */
    RTCP_RSI_DNFAB,        /* DNS Feedback Address Target sub-report */
    RTCP_RSI_RESERVED_3,   /* -- reserved -- */
    RTCP_RSI_LSRB,         /* Loss sub-report */
    RTCP_RSI_JSRB,         /* Jitter sub-report */
    RTCP_RSI_RTSB,         /* Round Trip Time sub-report */
    RTCP_RSI_CLSB,         /* Cumulative Loss sub-report */
    RTCP_RSI_COLSB,        /* Collisions sub-report */
    RTCP_RSI_RESERVED_9,   /* -- reserved -- */    
    RTCP_RSI_GSSB,         /* General Statistics sub-report */
    RTCP_RSI_BISB,         /* RTCP Bandwidth indication sub-report */
    RTCP_RSI_GAPSB,        /* RTCP Group and Average Packet Size Sub-report */

    NOT_AN_RSI_SUBRPT      /* this MUST always be LAST */
} rtcp_rsi_subrpt_t;

/*
 * rtcp_rsi_subrpt_mask_t 
 *
 * A set of RSI subreport types.
 * Functions to manipulate the set include:
 *
 * rtcp_set_rsi_subrpt_mask
 * rtcp_clear_rsi_subrpt_mask
 *
 * rtcp_rsi_get_first_subrpt
 * rtcp_rsi_get_next_subrpt
 *
 * (rtcp_rsi_get_ge_subrpt is for internal use)
 */

typedef uint32_t rtcp_rsi_subrpt_mask_t;

#define RTCP_RSI_SUBRPT_MASK_SIZE (sizeof(rtcp_rsi_subrpt_mask_t) * 8)

#define RTCP_RSI_SUBRPT_MASK_0 ((rtcp_rsi_subrpt_mask_t) { 0 })

static inline 
void rtcp_set_rsi_subrpt_mask (rtcp_rsi_subrpt_mask_t *mask, 
                               rtcp_rsi_subrpt_t type) 
{
    *mask |= (1 << type);
}

static inline 
void rtcp_clear_rsi_subrpt_mask (rtcp_rsi_subrpt_mask_t *mask,
                                 rtcp_rsi_subrpt_t type) 
{
    *mask &= ~(1 << type);
}

static inline
rtcp_rsi_subrpt_t rtcp_rsi_get_ge_subrpt (rtcp_rsi_subrpt_mask_t mask,
                                          rtcp_rsi_subrpt_t type)
{
    
    rtcp_rsi_subrpt_t rtype;
    for (rtype = type; rtype < RTCP_RSI_SUBRPT_MASK_SIZE; rtype++) {
        if ((1 << rtype) & mask) {
            return (rtype);
        }
    }
    return (NOT_AN_RSI_SUBRPT);
}

static inline
rtcp_rsi_subrpt_t rtcp_rsi_get_first_subrpt (rtcp_rsi_subrpt_mask_t mask)
{
    return (rtcp_rsi_get_ge_subrpt(mask, 0));
}

static inline
rtcp_rsi_subrpt_t rtcp_rsi_get_next_subrpt (rtcp_rsi_subrpt_mask_t mask,
                                            rtcp_rsi_subrpt_t type)
{
    return (rtcp_rsi_get_ge_subrpt(mask, type + 1));
}

/*
 * generic RSI sub-report definition
 */
typedef struct rtcp_rsi_gen_subrpt_t_ {
    uint8_t srbt;           /* Sub-Report Block Type */
    uint8_t length;         /* Sub-Report Length, in 32-bit words */
    uint8_t data[2];        /* Sub-Report Data: (length*4 - 2) octets */
} rtcp_rsi_gen_subrpt_t;

/*
 * Group and Average Packet Size sub-report
 */
typedef struct rtcp_rsi_gapsb_surbrpt_t_ {
    uint8_t srbt;           /* Sub-Report Block Type */
    uint8_t length;         /* Sub-Report Length, in 32-bit words */
    uint16_t average_packet_size;  /* RTCP average packet size */
    uint32_t group_size;           /* RTCP group size */
} rtcp_rsi_gaps_subrpt_t;

/*
 * RTCP Bandwidth Indication sub-report
 */
typedef struct rtcp_gapsb_subrpt_t_ {
    uint8_t srbt;           /* Sub-Report Block Type */
    uint8_t length;         /* Sub-Report Length, in 32-bit words */
    uint16_t role;          /* Sender: 1 Receivers: 1 
                               all other bits are reserved */
#define RTCP_RSI_BI_SENDER    0x8000
#define RTCP_RSI_BI_RECEIVERS 0x4000
#define RTCP_RSI_BI_RESERVED  0x3FFF

    uint32_t rtcp_bandwidth;   /* per-sender/receiver bandwidth, as a
                                  16:16 fixed point value (0 <= bw < 65536),
                                  in kilobits/sec */
} rtcp_rsi_rtcpbi_subrpt_t;

/*
 * Collisions sub-report
 */
typedef struct rtcp_rsi_collisions_subrpt_t_ {
    uint8_t srbt;           /* Sub-Report Block Type */
    uint8_t length;         /* Sub-Report Length, in 32-bit words */
    uint16_t reserved;
    uint32_t collision_ssrc[1];  /* one or more collision SSRCs */
} rtcp_rsi_collisions_subrpt_t;

/*
 * RTCP PUBPORTS packet
 */
typedef struct rtcp_pubports_t_ {
    uint32_t ssrc_media_sender;
    uint16_t rtp_port;
    uint16_t rtcp_port;
} rtcp_pubports_t;

#endif /* to prevent multiple inclusion */
