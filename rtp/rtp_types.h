/* 
 * $Id:
 * $Source:
 *------------------------------------------------------------------
 * rtp_types.h - Real-time Transport Protocol related  definitions.
 *
 * May, 1996
 *
 * Copyright (c) 1996-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log: 
 * Revision 
 *
 *------------------------------------------------------------------
 * $Endlog$
 */
/*
 * Prevent multiple inclusion.
 */
#ifndef __RTP_TYPES_H__
#define __RTP_TYPES_H__

#include "../include/utils/vam_time.h"

typedef uint8_t tinybool;

#define GET_TIMESTAMP(__ts)  __ts = get_sys_time();

/*
 * Conversions between the two fixed point types
 */
#define        MFPTOFP(x_i, x_f)        (((x_i)<<16) | (((x_f)>>16)&0xffff))
#define ONESEC   1000000
#define ONEMIN   ONESEC * 60

#define RANDOM_GENERIC_TYPE   1
#define RANDOM_SSRC_TYPE      1
#define RANDOM_TIMESTAMP_TYPE 2
#define RANDOM_SEQUENCE_TYPE  3
/*
 * Return random unsigned 32-bit quantity. Use 'type' argument if
 * you need to generate several different values in close succession.
 */
extern uint32_t rtp_random32(int type);

#endif /* to prevent multiple inclusion */
