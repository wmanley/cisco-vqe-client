/*
 *------------------------------------------------------------------
 * Database access for configuration module
 *
 * May 2006, Dong Hsu
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _CFG_DATABASE_H_
#define _CFG_DATABASE_H_

/*! \file cfg_database.h
    \brief Database access component for Configuration Module.

    Internal database access functions including open and close a database.
*/

#include <stdio.h>
#include <unistd.h>
#include <utils/vam_types.h>
#include "cfg_channel.h"

#define MAX_DB_NAME_LENGTH 1024

#define MAX_LINE_LENGTH 512
#define MAX_SESSION_SIZE 4096

#define BACKUP_DB_NAME "vam-config-backup.cfg"

#define SDP_TEMPLATE "v=0\n\
o=%s\n\
s=%s\n\
i=Channel configuration for %s\n\
t=0 0\n\
a=rtcp-unicast:rsi\n\
%s\
%s\
m=video %d %s %d\n\
i=Original source stream\n\
c=IN IP4 %s/255\n\
b=AS:%d\n\
%s\
%s\
%s\
a=%s\n\
a=source-filter: incl IN IP4 %s %s\n\
a=rtpmap:%d MP2T/90000\n\
%s\
a=mid:1\n\
m=video %d RTP/AVPF %d\n\
i=Re-sourced stream\n\
c=IN IP4 %s/255\n\
%s\
%s\
a=%s\n\
a=source-filter: incl IN IP4 %s %s\n\
a=rtpmap:%d MP2T/90000\n\
%s\
a=mid:2\n\
m=video %d RTP/AVPF %d\n\
i=Unicast retransmission stream\n\
c=IN IP4 %s\n\
%s\
%s\
a=%s\n\
a=rtpmap:%d rtx/90000\n\
a=rtcp:%d\n\
a=fmtp:%d apt=%d\n\
a=fmtp:%d rtx-time=%d\n\
a=mid:3\n\
%s"

#define SESSION_SEPARATOR "\n\n--50UBfW7LSCVLtggUPe5z\n\n\n"

#define GROUP_SIZE 20
#define GROUP "a=group:FID %s\n"
#define FEC_GROUP "a=group:FEC %s\n"

#define RTCP_FB_CTL_SIZE 80
#define RTCP_FB_CTL "a=rtcp:%d IN IP4 %s\n%s%s"
#define RTCP_NACK_MSG_SIZE 30
#define RTCP_NACK_MSG "a=rtcp-fb:%d nack\n"
#define RTCP_NACK_PLI_MSG "a=rtcp-fb:%d nack pli\n"

#define FEC_SESSION_SIZE 1024
#define FEC_SESSION "m=video %d RTP/AVP %d\n\
i=FEC column stream\n\
c=IN IP4 %s/255\n\
b=RS:0\n\
b=RR:0\n\
a=%s\n\
a=source-filter: incl IN IP4 %s %s\n\
a=rtpmap:%d %s/90000\n\
a=rtcp:%d\n\
a=mid:4\n\
m=video %d RTP/AVP %d\n\
i=FEC row stream\n\
c=IN IP4 %s/255\n\
b=RS:0\n\
b=RR:0\n\
a=%s\n\
a=source-filter: incl IN IP4 %s %s\n\
a=rtpmap:%d %s/90000\n\
a=rtcp:%d\n\
a=mid:5\n"

#define BW_SIZE 80
#define RS_BW "b=RS:%d\n"
#define RR_BW "b=RR:%d\n"
#define PER_RCVR_BW "a=fmtp:%d rtcp-per-rcvr-bw=%d\n"

/*! \enum cfg_db_ret_e
    \brief Return value from cfg_db function calls.
*/
typedef enum {
    CFG_DB_SUCCESS,
    CFG_DB_FAILURE,
    CFG_DB_NO_NAME_SPECIFIED,
    CFG_DB_OPEN_FAILED,
    CFG_DB_NOT_OPEN,
    CFG_DB_CORRUPTED,
    CFG_DB_MALLOC_ERR,
    CFG_DB_NOT_SUPPORTED
} cfg_db_ret_e;

/*! \enum db_type_e
    \brief Types of database supported by this module
*/
typedef enum {
    DB_FILE
} db_type_e;

/*! \struct cfg_database_t
    \brief Configuration database structure
*/
typedef struct cfg_database_ {
    char        db_name[MAX_DB_NAME_LENGTH];
    db_type_e   type;
    void        *db_file_p;
} cfg_database_t;

/**
 * Open configuration database
 */
extern cfg_db_ret_e cfg_db_open(cfg_database_t *db,
                                const char *cfg_db_name,
                                db_type_e type);

/**
 * Close configuration database
 */
extern cfg_db_ret_e cfg_db_close(cfg_database_t *db);

/**
 * Read channel configuration from database and store it in channel_mgr_t
 */
extern cfg_db_ret_e cfg_db_read_channels(cfg_database_t *db,
                                         channel_mgr_t *channel_mgr_p);

/**
 * Write channel configuration from channel_mgr_t to database
 */
extern cfg_db_ret_e cfg_db_write_channels(cfg_database_t *db,
                                          channel_mgr_t *channel_mgr_p);

/**
 * Create a SDP description based on the channel configuration data
 */
extern cfg_db_ret_e cfg_db_create_SDP(channel_cfg_t *channel_p,
                                      char *data_buffer,
                                      int length);


/**
 * Parse SDP syntax for the channel configuration data
 */
extern boolean cfg_db_parse_SDP(void *sdp_in__p, char* data_p,
                                boolean *syntax_error);

/**
 * Compute MD5 checksum for channel SDP content
 */
extern uint32_t cfg_db_checksum(char *sdp_buffer, int length);


#endif /* _CFG_DATABASE_H_ */
