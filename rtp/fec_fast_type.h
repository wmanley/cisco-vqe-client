/*------------------------------------------------------------------
 *
 * July 2008
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *------------------------------------------------------------------
 */

/**
 * fec_fast_type.h - defines the fast type of FEC module
 */

#ifndef __FEC_FAST_TYPE_H__
#define __FEC_FAST_TYPE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <utils/vam_types.h>

/**
 * define a fec fast type header for efficient processing
 * of MPEG COP#3 FEC header. 
 */
typedef struct fecfasttype_ {
    uint16_t snbase_low_bits;    /* min seq number with this FEC pkt */
    uint16_t length_recovery;    /* number of protected media packets */
    uint32_t combined1;          /* E, PT recovery, Mask */
    uint32_t ts_recovery;        /* timestamp recovery */
    u_int16_t combined2;         /* X, D, type, index, offset */
    u_int16_t combined3;         /* NA, SNBase ext bits */
} fecfasttype_t;

/**
 * fast path to process fec header bits
 */
#define FEC_E_BIT(a)        ((GETLONG(&(a->combined1)) & 0x80000000) \
                         >> 31)        /* E-bit  */
#define FEC_PT_RECOVERY(a)  ((GETLONG(&(a->combined1)) & 0x7f000000) \
                         >> 24)        /* payload recovery  */
#define FEC_MASK(a)         ((GETLONG(&(a->combined1)) & 0x00ffffff) \
                         >> 0)         /* mask */
#define FEC_X_BIT(a)        ((GETSHORT(&(a->combined2)) & 0x8000) \
                         >> 15)        /* X-bit*/
#define FEC_D_BIT(a)        ((GETSHORT(&(a->combined2)) & 0x4000) \
                         >> 14)        /* D-Bit */
#define FEC_TYPE(a)         ((GETSHORT(&(a->combined2)) & 0x3800) \
                         >> 11)        /* fec type */
#define FEC_INDEX(a)        ((GETSHORT(&(a->combined2)) & 0x0700)   \
                         >> 8)        /* fec index */
#define FEC_OFFSET(a)       ((GETSHORT(&(a->combined2)) & 0x00ff) \
                         >> 0)        /* offset */
#define FEC_NA(a)           ((GETSHORT(&(a->combined3)) & 0xff00) \
                         >> 8)        /* NA bits */
#define FEC_SNBASE_EXT(a)   ((GETSHORT(&(a->combined3)) & 0x00ff) \
                         >> 0)        /* fec type */


#define SET_FEC_E_BIT(ptr, value) (PUTLONG(&((ptr)->combined1), \
                                  (GETLONG(&((ptr)->combined1)) \
                                           & 0x7FFFFFFF) |          \
                                  (((value) ? 1 : 0) << 31)))
#define SET_FEC_PT_RECOVERY(ptr, value) (PUTLONG(&((ptr)->combined1), \
                                        (GETLONG(&((ptr)->combined1)) \
                                           & 0x80FFFFFF) |            \
                                        (((value) & 0x7f) << 24 )))
#define SET_FEC_X_BIT(ptr, value) (PUTSHORT(&((ptr)->combined2), \
                                     (GETSHORT(&((ptr)->combined2)) \
                                               & 0x7fff) |        \
                                     (((value) ? 1 : 0) << 15)))
#define SET_FEC_D_BIT(ptr, value) (PUTSHORT(&((ptr)->combined2), \
                                     (GETSHORT(&((ptr)->combined2)) \
                                               & 0xbfff) |        \
                                     (((value) ? 1 : 0) << 14)))
#define SET_FEC_TYPE(ptr, value)  (PUTSHORT(&((ptr)->combined2), \
                                     (GETSHORT(&((ptr)->combined2)) \
                                               & 0xc7FF) |        \
                                     (((value) & 0x07) << 11)))
#define SET_FEC_OFFSET(ptr, value)  (PUTSHORT(&((ptr)->combined2), \
                                     (GETSHORT(&((ptr)->combined2)) \
                                               & 0xff00) |        \
                                     (((value) & 0xff) << 0)))
#define SET_FEC_NA(ptr, value)    (PUTSHORT(&((ptr)->combined3), \
                                     (GETSHORT(&((ptr)->combined3)) \
                                               & 0x00ff) |        \
                                     (((value) & 0xff) << 8)))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FEC_FAST_TYPE_H */


