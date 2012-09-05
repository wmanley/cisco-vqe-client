/********************************************************************
 * syslog_all_const.h
 *
 * Derived from DC-OS include/isan/syslog_all_const.h
 * 
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _SYSLOG_ALL_CONST_H
#define _SYSLOG_ALL_CONST_H

#include <log/vqe_id.h>

#define    INTERNAL_NOPRI    0x10    /* the "no priority" priority */
/* mark "facility" */
#define    INTERNAL_MARK    LOG_MAKEPRI(LOG_NFACILITIES, 0)

typedef struct _code {
    char    *c_name;
    int      c_val;
} CODE;

#define    LOG_FACMASK    0x03f8    /* mask to extract facility part */
                /* facility of pri */
#define    LOG_FAC(p)    (((p) & LOG_FACMASK) >> 3)

#ifdef SYSLOG_NAMES
/* In this array, the order of the entries doesn't matter */
CODE facilitynames[] =
{
    { "auth",       LOG_AUTH },
    { "authpriv",   LOG_AUTHPRIV },
    { "cron",       LOG_CRON },
    { "daemon",     LOG_DAEMON },
    { "ftp",        LOG_FTP },
    { "kern",       LOG_KERN },
    { "lpr",        LOG_LPR },
    { "mail",       LOG_MAIL },
    { "news",       LOG_NEWS },
    { "security",   LOG_AUTH },        /* DEPRECATED */
    { "syslog",     LOG_SYSLOG },
    { "user",       LOG_USER },
    { "uucp",       LOG_UUCP },
    { "local0",     LOG_LOCAL0 },
    { "local1",     LOG_LOCAL1 },
    { "local2",     LOG_LOCAL2 },
    { "local3",     LOG_LOCAL3 },
    { "local4",     LOG_LOCAL4 },
    { "local5",     LOG_LOCAL5 },
    { "local6",     LOG_LOCAL6 },
    { "local7",     LOG_LOCAL7 },
/* Change this define if the first define in the following list changes */
#define LOG_FIRST_VQE_MOD  LOG_EX_SYSLOG
    { VQENAME_EX_DEBUG,   LOG_EX_SYSLOG },
    { VQENAME_VQES_CP,    LOG_VQES_CP },
    { VQENAME_VQES_DP,    LOG_VQES_DP },
    { VQENAME_VQES_DPCLIENT,    LOG_VQES_DPCLIENT },
    { VQENAME_RTSP,       LOG_RTSP },
    { VQENAME_RTP,        LOG_RTP },
    { VQENAME_VQEC,       LOG_VQEC },
    { VQENAME_VQES_MLB,   LOG_VQES_MLB },
    { VQENAME_VQES_MLBCLIENT,    LOG_VQES_MLBCLIENT },
    { VQENAME_VQES_PM,    LOG_VQES_PM },
    { VQENAME_LIBCFG,     LOG_LIBCFG },
    { VQENAME_VQE_UTILS,   LOG_VQE_UTILS },
    { VQENAME_STUN_SERVER, LOG_STUN_SERVER },
    { VQENAME_VQEC_DP,    LOG_VQEC_DP },
    { VQENAME_VQE_CFGTOOL, LOG_VQE_CFGTOOL },
    { VQENAME_VQM, LOG_VQM },
    { VQENAME_RPC, LOG_RPC },
    { "mark",    INTERNAL_MARK },      /* INTERNAL */
    { NULL, -1 }
};

CODE prioritynames[] =
{
    { "alert",      LOG_ALERT },
    { "crit",       LOG_CRIT },
    { "debug",      LOG_DEBUG },
    { "emerg",      LOG_EMERG },
    { "err",        LOG_ERR },
    { "error",      LOG_ERR },        /* DEPRECATED */
    { "info",       LOG_INFO },
    { "none",       INTERNAL_NOPRI }, /* INTERNAL */
    { "notice",     LOG_NOTICE },
    { "panic",      LOG_EMERG },      /* DEPRECATED */
    { "warn",       LOG_WARNING },    /* DEPRECATED */
    { "warning",    LOG_WARNING },
    { NULL, -1 }
};
#endif /* SYSLOG_NAMES */

#endif  /* _SYSLOG_ALL_CONST_H */
