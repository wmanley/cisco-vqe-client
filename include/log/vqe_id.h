/********************************************************************
 * vqe_id.h
 *
 * This file corresponds to DC-OS file: include/isan/uuid.h
 * 
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQE_ID_H
#define _VQE_ID_H

/* IMPORTANT NOTES:
 *   The following VQE logging facility names are duplicated in 
 * vam/syslogd/syslogd.c, in order to minimize the risk of GPL'ed codes 
 * pulling in VQE codes.
 *
 *   ALWAYS KEEP THEM IN SYNC!!
 */ 
#define VQEDESC_EX_DEBUG   "A logging/debugging test facility"
#define VQENAME_EX_DEBUG   "ex_debug"

#define VQEDESC_VQES_CP    "VQE Server Control Plane"
#define VQENAME_VQES_CP    "vqes_cp"

#define VQEDESC_VQES_DP    "VQE Server Data Plane"
#define VQENAME_VQES_DP    "vqes_dp"

#define VQEDESC_VQES_DPCLIENT    "VQE Server Data Plane Client API"
#define VQENAME_VQES_DPCLIENT    "vqes_dp_client"

#define VQEDESC_VQES_MLB    "VQE Server Multicast Load Balancer"
#define VQENAME_VQES_MLB    "vqes_mlb"

#define VQEDESC_VQES_MLBCLIENT    "VQES Multicast Load Balancer Client"
#define VQENAME_VQES_MLBCLIENT    "vqes_mlb_client"

#define VQEDESC_VQES_PM    "VQE Server Process Monitor"
#define VQENAME_VQES_PM    "vqes_pm"

#define VQEDESC_RTSP       "VQE RTSP Server"
#define VQENAME_RTSP       "vqe_rtsp"

#define VQEDESC_RTP        "VQE RTP/RTCP"
#define VQENAME_RTP        "vqe_rtp"

#define VQEDESC_VQEC       "VQE Client"
#define VQENAME_VQEC       "vqec"

#define VQEDESC_LIBCFG     "VQE CFG Library"
#define VQENAME_LIBCFG     "vqe_cfg"

#define VQEDESC_VQE_UTILS  "VQE UTILS"
#define VQENAME_VQE_UTILS  "vqe_utils"

#define VQEDESC_STUN_SERVER   "STUN Server"
#define VQENAME_STUN_SERVER   "stun_server"

#define VQEDESC_VQEC_DP    "VQE Client Dataplane"
#define VQENAME_VQEC_DP    "vqec_dp"

#define VQEDESC_VQE_CFGTOOL    "VQE Configuration Management System"
#define VQENAME_VQE_CFGTOOL    "vqe_cfgtool"

#define VQEDESC_VQM        "Video Quality Monitoring"
#define VQENAME_VQM        "VQM"

#define VQEDESC_RPC        "VQE RPC library"
#define VQENAME_RPC        "vqe_rpc"

#endif /* _VQE_ID_H */
