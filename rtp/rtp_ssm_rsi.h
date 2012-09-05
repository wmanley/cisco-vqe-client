/**-----------------------------------------------------------------------
 * @brief
 * Declarations for constructing/processing RSI messages.
 * (RSI is the RTCP Receiver Summary Information message)
 *
 * @file
 * rtp_ssm_rsi.h
 *
 * February 2007, Mike Lague -- consolidate existing RSI functions here.
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------------
 */

#include "rtp_session.h"

/*
 * Public function declarations
 */

extern uint32_t rtcp_construct_report_ssm_rsi(rtp_session_t   *p_sess,
                                              rtp_member_t    *p_source, 
                                              rtcptype        *p_rtcp,
                                              uint32_t         bufflen,
                                              rtcp_pkt_info_t *p_pkt_info,
                                              rtcp_rsi_info_t *p_rsi_info,
                                              boolean          reset_xr_stats);
extern rtcp_parse_status_t rtcp_process_rsi(rtp_session_t *p_sess,
                                            rtp_member_t *p_pak_source,
                                            ipaddrtype src_addr,
                                            uint16_t src_port,
                                            rtcptype *p_rtcp,
                                            uint16_t count,
                                            uint8_t *p_end_of_frame);
