/*
 *------------------------------------------------------------------
 * rtp_util.h  -- 
 *
 * June 2006
 *
 * Copyright (c) 2006-2007 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __RTP_UTIL_H__
#define __RTP_UTIL_H__

#include <stdint.h>
#include <arpa/inet.h>

#include "rtp.h"
#include "rtp_session.h"

extern boolean rtp_get_local_addr(ipsocktype socket,
                                  ipaddrtype *local_addr,
                                  uint16_t   *local_port);

extern boolean same_rtp_source_id (rtp_source_id_t *, rtp_source_id_t *);

extern boolean is_null_rtp_source_id (rtp_source_id_t *);

extern char *rtcp_msgtype_str(rtcp_type_t msgtype);

#endif /* __RTP_UTIL_H__ */


