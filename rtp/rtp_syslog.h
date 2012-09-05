/**-----------------------------------------------------------------
 * @brief
 * RTP/RTCP Library syslog include file
 *
 * @file
 * rtp_syslog.h
 *
 * February 2007, Donghai Ma
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __RTP_SYSLOG_H__
#define __RTP_SYSLOG_H__

#include <log/vqe_rtp_syslog_def.h>
#include <log/vqe_rtp_debug.h>

#define RTP_DBG_PFX_SIZE  64

/* 
 * RTP supports two debug filter types: 
 *  1.channel filter: a channel is identified by the channel IP address and
 *    the channel port number;
 *  2.STB IP address filter: filtered by a STB's IP address
 *
 * In current libdebug implemenration, a debug flag can be associated with
 * only one filter, either a STB IP filter or a channel filter in ERA's case.
 * When a debug flag is ON, and (1) either filter matches or (2) no filter is 
 * configured, we will do the logging.
 *
 * Do the logging when we are in one of these situations:
 *  1. the debug_flag is ON, no filter is configured for this flag, or
 *  2. the debug_flag is ON, STB filter is configured AND matched, or
 *  3. the debug_flag is ON, Chan filter is configured AND matched
 */

#define rtp_filtered_debug(debug_flag, session, member, string...)      \
    do {                                                                \
        char msg_buf[RTP_DBG_PFX_SIZE];                                 \
        if (rtp_debug_check(debug_flag, session, member)) {             \
            buginf_func(TRUE, VQENAME_RTP,                              \
                        rtp_debug_prefix(session, member,               \
                                         msg_buf, RTP_DBG_PFX_SIZE),    \
                        FALSE, ##string);                               \
        }                                                               \
    } while (0)

/* Unfiltered debug logging */
#define RTP_DEBUG(flag, arg...)                                         \
    rtp_filtered_debug(flag, NULL, NULL, ##arg)

/* Unfiltered verbose debug logging */
#define RTP_DEBUG_VERBOSE(flag, arg...)                                 \
    do {                                                                \
        if (IS_VQE_RTP_DEBUG_FLAG_ON(RTP_DEBUG_VERBOSE)) {              \
            RTP_DEBUG(flag, ##arg);                                     \
        }                                                               \
    } while (0)


/* Filtered debug logging */
#define RTP_FLTD_DEBUG(flag, session, member, arg...)    \
    rtp_filtered_debug(flag, session, member, ##arg)

/* Filtered verbose debug logging */
#define RTP_FLTD_DEBUG_VERBOSE(flag, session, member, arg...)           \
    do {                                                                \
        if (IS_VQE_RTP_DEBUG_FLAG_ON(RTP_DEBUG_VERBOSE)) {              \
            RTP_FLTD_DEBUG(flag, session, member, ##arg);               \
        }                                                               \
    } while (0)

/* RTP syslog macros */
#define RTP_SYSLOG syslog_print

/*
 * temporary redefine "old" logging macros:
 * RTP_LOG_DEBUG
 * RTP_LOG_ERROR
 * temporarily add:
 * RTP_LOG_PACKET
 * $$$ remove when full conversion to filtered debugs is done
 */

#define RTP_LOG_DEBUG(fmt, args...) \
    RTP_DEBUG(RTP_DEBUG_EVENTS, fmt, ##args)

#define RTP_LOG_DEBUG_F(session, member, fmt, args...)   \
    RTP_FLTD_DEBUG(RTP_DEBUG_EVENTS, session, member, fmt, ##args)

#define RTP_LOG_DEBUG_FV(session, member, fmt, args...)   \
    RTP_FLTD_DEBUG_VERBOSE(RTP_DEBUG_EVENTS, session, member, fmt, ##args)

#define RTP_LOG_ERROR(fmt, args...) \
    RTP_DEBUG(RTP_DEBUG_ERRORS, fmt, ##args)

#define RTP_LOG_ERROR_F(session, member, fmt, args...)   \
    RTP_FLTD_DEBUG(RTP_DEBUG_ERRORS, session, member, fmt, ##args)

#define RTP_LOG_ERROR_FV(session, member, fmt, args...)   \
    RTP_FLTD_DEBUG_VERBOSE(RTP_DEBUG_ERRORS, session, member, fmt, ##args)

#define RTP_LOG_PACKET(fmt, args...) \
    RTP_DEBUG(RTP_DEBUG_PACKETS, fmt, ##args)

#define RTP_LOG_PACKET_F(session, member, fmt, args...)   \
    RTP_FLTD_DEBUG(RTP_DEBUG_PACKETS, session, member, fmt, ##args)

#define RTP_LOG_PACKET_FV(session, member, fmt, args...)   \
    RTP_FLTD_DEBUG_VERBOSE(RTP_DEBUG_PACKETS, session, member, fmt, ##args)

#endif /* __RTP_SYSLOG_H__ */
