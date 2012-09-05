/**-----------------------------------------------------------------
 * @brief
 * Declarations/definitions for RTP database functions.
 *
 * @file
 * rtp_database.h
 *
 * April 2007, Mike Lague.
 *
 * Copyright (c) 2007 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __RTP_DATABASE_H__
#define __RTP_DATABASE_H__

#include "rtp_session.h"

extern int rtp_cmp_cname(char *cn1, char *cn2);

extern rtp_member_t *rtp_first_member(rtp_session_t *sess);
extern rtp_member_t *rtp_next_member(rtp_session_t *sess,
                                     rtp_member_t *member);

extern rtp_member_t *rtp_first_member_with_ssrc(rtp_session_t *sess,
                                                uint32_t ssrc);
extern rtp_member_t *rtp_next_member_with_ssrc(rtp_session_t *sess,
                                               rtp_member_t *member);
extern rtp_member_t *rtp_sending_member_with_ssrc(rtp_session_t *sess,
                                                  uint32_t ssrc);

extern rtp_member_t *rtp_find_member(rtp_session_t *sess,
                                     uint32_t ssrc,
                                     char *cname);
extern boolean rtp_add_member(rtp_session_t *sess,
                              rtp_member_t *member);
extern boolean rtp_update_member_cname(rtp_session_t *sess,
                                       rtp_member_t  *member,
                                       char *cname);
extern boolean rtp_update_member_ssrc(rtp_session_t *sess,
                                      rtp_member_t  *member,
                                      uint32_t ssrc);
extern void rtp_remove_member(rtp_session_t *sess,
                              rtp_member_t  *member);

#endif /* __RTP_DATABASE_H__ */
