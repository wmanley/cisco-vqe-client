/* $Id: rtp.h,v 1.1 1996/05/14 00:05:03 mleelani Exp $
 * $Source: /trunk/california/cvs/Xsys/rtp/rtp.h,v $
 *------------------------------------------------------------------
 * rtp.h - Real-time Transport Protocol related  definitions.
 *
 * May, 1996
 *
 * Copyright (c) 1996-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log: rtp.h,v $
 * Revision 1.1  1996/05/14  00:05:03  mleelani
 * Placeholders for Real-time Transport Protocol related development.
 *
 *------------------------------------------------------------------
 * $Endlog$
 */
/*
 * Prevent multiple inclusion.
 */
#ifndef __RTP_H__
#define __RTP_H__

#include "../include/utils/vam_types.h"
#include "rtp_types.h"

#ifndef VQE_SQRT
#include <math.h>
#define VQE_SQRT(x) sqrt((float)(x))
#endif /* VQE_SQRT */

/*
typedef unsigned int uint32_t
typedef unsigned short uint16_t
typedef unsigned char uint8_t
typedef int int32_t
typedef short int16_t
*/
/*
 *-----------------------------------------------------------------
 * Fix these later!!  KK !!
 *-----------------------------------------------------------------
 */

#ifndef GETSHORT
#define GETSHORT(p) (ntohs(*(p)))
#define PUTSHORT(p,val) ((*p) = htons(val))
#endif


#define TRUE 1
#define FALSE 0
#define SUCCESS 0
#define FAILURE -1

#ifndef NULL
#define NULL 0
#endif

typedef uint32_t ipaddrtype;
typedef uint32_t ipsocktype;
typedef struct rtcptype_ rtcptype;
typedef struct rtptype_ rtptype;

#define UDPHEADERBYTES 8
#define IPHEADERSIZE  20

/*
 * IP header structure definition.
 */

#  define IP_TOS_BYTE  1        /* number of bytes into TOS field      */
#  define IP_TL_INFO   1        /* number of shorts into this structure */
#if defined (__BIG_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)
#  define FOMASK 0x1FFF
#  define MOREFRAGMASK 0x2000
#  define IP_FRAG_DFMASK 0x4000
#  define IP_FRAG_INFO 3        /* number of shorts into this structure */
#  define IP_FRAG_INFO_BYTE  (IP_FRAG_INFO * sizeof(uint16_t))
#  define IHLMASK        0x0f00
#  define IHLSHIFT        8        /* number bits to shift to get to LSB */
#  define IP_IHL        0        /* number of shorts into this structure */
#  define IP_IHL_BYTE        (IP_IHL * sizeof(uint16_t))
#  define IPMACRO \
    uint8_t version: 4;        /* IP version number */ \
    uint8_t ihl: 4;                /* IP header length */ \
    uint8_t tos;                      /* type of service */ \
    uint16_t tl;                        /* total length */ \
    uint16_t id;                        /* identifier */ \
    unsigned ipreserved: 1; \
    unsigned dontfragment: 1; \
    unsigned morefragments: 1; \
    unsigned fo: 13;                /* fragment offset */ \
    uint8_t ttl;                        /* time to live */ \
    uint8_t prot;                        /* protocol type */ \
    uint16_t checksum;                /* checksum */ \
    ipaddrtype srcadr;                /* IP source address */ \
    ipaddrtype dstadr;                /* IP destination address */ 
#else /* LITTLEENDIAN */
#  define morefragments F1.F2.morefrags
#  define dontfragment F1.F2.dontfrag
#  define ipreserved F1.F2.Reserv
#  define IP_FRAG_DFMASK 0x0040
#  define MOREFRAGMASK 0x0020
#  define FOMASK 0xFF1F
#  define getfo(p) ((p)->F1.fragoff & FOMASK)
#  define fo F1.fragoff
#  define IPMACRO \
    uint8_t ihl: 4;        /* IP header length */ \
    uint8_t version: 4;        /* IP version number */ \
    uint8_t tos;                /* type of service */ \
    uint16_t tl;                        /* total length */ \
    uint16_t id;                        /* identifier */ \
    /* Stupid C compiler assigns bit orders wrong... */ \
    union f1 {                         /* first get the bits right. */ \
        struct f2 { \
            unsigned char dmy1:5; \
            unsigned char morefrags:1; \
            unsigned char dontfrag:1; \
            unsigned char Reserv:1; \
            unsigned char dmy2;   \
        } F2; \
            unsigned short fragoff; \
    } F1; \
    uint8_t ttl;                        /* time to live */ \
    uint8_t prot;                        /* protocol type */ \
    uint16_t checksum;                /* checksum */ \
    ipaddrtype srcadr;                /* IP source address */ \
    ipaddrtype dstadr;                /* IP destination address */ 
#endif

typedef struct iphdrtype_ {
    IPMACRO                        /* IP header */
} iphdrtype;

/*
 * UDP definitions
 */
typedef struct udptype_ {
        uint16_t sport;                        /* source port */
        uint16_t dport;                        /* destination port */
        uint16_t length;                        /* bytes in data area */
        uint16_t checksum;                /* checksum */
        uint32_t udpdata[0];                /* start of UDP data bytes */
} udptype;


#define ipheadstart(pak) pak

/*
 *-----------------------------------------------------------------
 * Fix these later!!  KK !!
 *-----------------------------------------------------------------
 */



#define RTP_TYPE_ANNEX_B    123     /* RTP annex B payload type */
/*
 *------------------------------------------------------------------
 *  RTP specific defintions and declarations.
 *------------------------------------------------------------------
 */
#define MINRTPHEADERBYTES 12
#define RTPVERSION         2
#define RTP_SEQ_MOD       (1 << 16)
#define RTP_MAX_DROPOUT   3000
#define RTP_MAX_MISORDER  100

/*
 * Fixed portion of RTP header.
 */
typedef struct rtptype_ {
#if defined (__BIG_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)
    uint16_t  version:    2;
    tinybool padding:    1;
    tinybool extension:  1;
    uint16_t  csrc_count: 4;
    tinybool marker:     1;
    uint16_t  payload:    7;
#else
    uint16_t  csrc_count: 4;
    tinybool extension:  1;
    tinybool padding:    1;
    uint16_t  version:    2;
    uint16_t  payload:    7;
    tinybool marker:     1;
#endif
    uint16_t  sequence;          /* RTP sequence */
    uint32_t   timestamp;         /* RTP timestamp */
    uint32_t   ssrc;              /* Syncronization source identifier */
    /* list of CSRCs */
} rtptype_t;

/*
 * Version for fast path with combined bitmasks
 */
typedef struct rtpfasttype_ {
    uint16_t  combined_bits;     /* RTP V,P,X,CC,M & PT */
    uint16_t  sequence;          /* RTP sequence */
    uint32_t   timestamp;         /* RTP timestamp */
    uint32_t   ssrc;              /* Syncronization source identifier */
    /* list of CSRCs */
} rtpfasttype_t;

/*
 * Fast path masks for combined_bits field
 */
#define VERSION_SHIFT        14
#define PADDING_SHIFT        13
#define EXT_SHIFT        12
#define CSRCC_SHIFT        8
#define MARKER_SHIFT        7
#define PAYLOAD_SHIFT        0

#define RTP_VERSION(a)        ((GETSHORT(&(a->combined_bits)) & 0xc000) \
                         >> VERSION_SHIFT)        /* Version  */
#define RTP_PADDING(a)        ((GETSHORT(&(a->combined_bits)) & 0x2000) \
                         >> PADDING_SHIFT)        /* Padding  */
#define RTP_EXT(a)        ((GETSHORT(&(a->combined_bits)) & 0x1000) \
                         >> EXT_SHIFT)                /* Extension */
#define RTP_CSRCC(a)        (((GETSHORT(&(a->combined_bits)) & 0x0f00) \
                         >> CSRCC_SHIFT) & 0x000f) /* CSRC count*/
#define RTP_MARKER(a)        ((GETSHORT(&(a->combined_bits)) & 0x0080) \
                         >> MARKER_SHIFT)        /* Marker */
#define RTP_PAYLOAD(a)        ((GETSHORT(&(a->combined_bits)) & 0x007f) \
                         >> PAYLOAD_SHIFT)        /* Payload */

#define SET_RTP_PAYLOAD(ptr, value) (PUTSHORT(&((ptr)->combined_bits), \
                                     (GETSHORT(&((ptr)->combined_bits))\
                                      & 0xFF80) | ((value) & 0x7F)))
#define SET_RTP_CSRCC(ptr, value) (PUTSHORT(&((ptr)->combined_bits), \
                                     (GETSHORT(&((ptr)->combined_bits)) \
                                               & 0xF0FF) |        \
                                     (((value) & 0x0F) << CSRCC_SHIFT)))
#define SET_RTP_MARKER(ptr, value) (PUTSHORT(&((ptr)->combined_bits), \
                                     (GETSHORT(&((ptr)->combined_bits)) \
                                               & 0xFF7F) |        \
                                     (((value) ? 1 : 0) << MARKER_SHIFT)))
#define SET_RTP_EXT(ptr, value)      (PUTSHORT(&((ptr)->combined_bits), \
                                     (GETSHORT(&((ptr)->combined_bits)) \
                                               & 0xEFFF) |        \
                                     (((value) ? 1 : 0) << EXT_SHIFT)))
#define SET_RTP_PADDING(ptr, value)  (PUTSHORT(&((ptr)->combined_bits), \
                                     (GETSHORT(&((ptr)->combined_bits)) \
                                               & 0xDFFF) |        \
                                     (((value) ? 1 : 0) << PADDING_SHIFT)))
#define SET_RTP_VERSION(ptr, value)  (PUTSHORT(&((ptr)->combined_bits), \
                                     (GETSHORT(&((ptr)->combined_bits)) \
                                               & 0x3FFF) |        \
                                     (((value) & 0x03) << VERSION_SHIFT)))

#define MAX_CSRCC 15
#define RTP_CSRC_LEN(a)  (RTP_CSRCC(a) * 4)  /* len in bytes of CSRC values */

/*
 * RTP extension header
 */
typedef struct rtpexttype_ {
    uint16_t profile_specific_code;
    uint16_t length; /* in 32-bit words, excluding the code and length
                        (i.e., zero is a legal value) */
    uint32_t lwords[0] ; /* 'length' 32-bit words */
} rtpexttype_t;

#define MINRTPEXTHDRBYTES    4
#define RTPEXTHDRBYTES(ext)  (((GETSHORT(&ext->length)) + 1) * 4)

/*
 * RTP payload types
 */
typedef enum rtp_ptype_ {
    RTP_PCMU         = 0,
    RTP_CELP         = 1,
    RTP_G726         = 2,
    RTP_GSM          = 3,
    RTP_G723         = 4,
    RTP_DVI4         = 5,
    RTP_DVI4_II      = 6,
    RTP_LPC          = 7,
    RTP_PCMA         = 8,
    RTP_G722         = 9,
    RTP_COMFORT_NOISE_13 = 13,
    RTP_G728         = 15,
    RTP_G729         = 18,
    RTP_COMFORT_NOISE_19 = 19,
    RTP_GSM_EFR      = 20, /* not in IETF */
    RTP_G726_40      = 21, /* not in IETF */
    RTP_G726_24      = 22, /* not in IETF */
    RTP_G726_16      = 23, /* not in IETF */
    RTP_JPEG         = 26,
    RTP_NV           = 28,
    RTP_H261         = 31,
    RTP_MPV          = 32, /* RFC2250 */
    RTP_MP2T         = 33, /* RFC2250 */
    RTP_H263         = 34,

    RTP_FIRST_DYNAMIC_PAYLOAD_TYPE = 96,

    RTP_G726_16_DYNAMIC = 118, /* Conexant default */
    RTP_G726_24_DYNAMIC = 119, /* Conexant default */
    RTP_G726_40_DYNAMIC = 120, /* Conexant default */

    RTP_DIGIT_RELAY  = 121,
    RTP_FAX          = 122,
    RTP_CAS_SIGNAL   = 123,
    RTP_G729A_OLD    = 124,
    RTP_CLEAR_CHANNEL = 125,
    RTP_G711ULAW_SWITCH_OVER = 126,
    RTP_G711ALAW_SWITCH_OVER = 127,

    RTP_LAST_DYNAMIC_PAYLOAD_TYPE = 127,

    RTP_UNKNOWN_TYPE = 255, /* not in IETF */

    RTP_CCM_DYNAMIC_PAYLOAD_PASS_THRU = 256
} rtp_ptype;


/*
 * Dynamic payload type checking macro.
 */
#define RTP_VALID_DYNAMIC_PAYLOAD_TYPE(payload_type) \
                  (payload_type >= RTP_FIRST_DYNAMIC_PAYLOAD_TYPE && \
                   payload_type <= RTP_LAST_DYNAMIC_PAYLOAD_TYPE)

/*
 * RTPHEADERBYTES 
 * 
 * Returns the number of RTP header bytes,
 * including the RTP extension header, if present
 * 
 * This assumes the header has been validated, 
 * so it SHOULD NOT be applied to an unvalidated header.
 */
static inline uint32_t RTPHEADERBYTES (rtpfasttype_t *rtp) 
{
    uint32_t bytes = MINRTPHEADERBYTES + RTP_CSRC_LEN(rtp);

    if (RTP_EXT(rtp)) {
        rtpexttype_t *ext = (rtpexttype_t *)((uint8_t *)rtp + bytes);
        bytes += RTPEXTHDRBYTES(ext);
    }

    return (bytes);
}

enum {
        RTCP_SOURCE_SIMPLE,
        RTCP_SOURCE_RSI,
        RTCP_RECEIVER_SIMPLE,
        RTCP_RECEIVER_RSI,
        RTCP_CLEANUP,
        RTCP_SEND_REPORT,
        RTCP_MEMBER,
        RTCP_UPDATE_SEQ,
        RTCP_UNICAST,
        RTCP_VIF,
        RTCP_SHOW,
        RTCP_SEND_APP,
        RTCP_SEND_BYE,
        RTCP_SEND_SR
};

#endif /* to prevent multiple inclusion */
