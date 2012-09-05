/********************************************************************
 * syslog_facility_num.h
 *
 * Derived from DC-OS include/isan/syslog_macros.h
 * 
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

/* facility codes */
#define    LOG_KERN      (0<<3)    /* kernel messages */
#define    LOG_USER      (1<<3)    /* random user-level messages */
#define    LOG_MAIL      (2<<3)    /* mail system */
#define    LOG_DAEMON    (3<<3)    /* system daemons */
#define    LOG_AUTH      (4<<3)    /* security/authorization messages */
#define    LOG_SYSLOG    (5<<3)    /* messages generated internally by syslogd*/
#define    LOG_LPR       (6<<3)    /* line printer subsystem */
#define    LOG_NEWS      (7<<3)    /* network news subsystem */
#define    LOG_UUCP      (8<<3)    /* UUCP subsystem */
#define    LOG_CRON      (9<<3)    /* clock daemon */
#define    LOG_AUTHPRIV (10<<3)    /* security/authorization msgs (private)*/
#define    LOG_FTP      (11<<3)    /* ftp daemon */

/* other codes through 15 reserved for system use */
#define    LOG_LOCAL0   (16<<3)    /* reserved for local use */
#define    LOG_LOCAL1   (17<<3)    /* reserved for local use */
#define    LOG_LOCAL2   (18<<3)    /* reserved for local use */
#define    LOG_LOCAL3   (19<<3)    /* reserved for local use */
#define    LOG_LOCAL4   (20<<3)    /* reserved for local use */
#define    LOG_LOCAL5   (21<<3)    /* reserved for local use */
#define    LOG_LOCAL6   (22<<3)    /* reserved for local use */
#define    LOG_LOCAL7   (23<<3)    /* reserved for local use */

/*
 *  facilty numbers for VQE use
 *  A new VQE log facility number must be added between
 *  LOG_EX_SYSLOG and LOG_NFACILITIES
 */ 
#define    LOG_EX_SYSLOG      (24<<3)    /* Example */
#define    LOG_VQES_CP        (25<<3)    /* VQE-S Control Plane */
#define    LOG_VQES_DP        (26<<3)    /* VQE-S Data Plane */
#define    LOG_VQES_DPCLIENT  (27<<3)    /* VQE-S Data Plane Client API */
#define    LOG_RTSP           (28<<3)    /* RTSP Server */
#define    LOG_RTP            (29<<3)    /* RTP/RTCP  */
#define    LOG_VQEC           (30<<3)    /* VQE-C */
#define    LOG_VQES_MLB       (31<<3)    /* VQE-S Multicast Load Balancer */
#define    LOG_VQES_MLBCLIENT (32<<3)    /* MLB Client */
#define    LOG_VQES_PM        (33<<3)    /* VQE-S Process Monitor */
#define    LOG_LIBCFG         (34<<3)    /* CFG library */
#define    LOG_VQE_UTILS      (35<<3)    /* VQE Utils */
#define    LOG_STUN_SERVER    (36<<3)    /* STUN Server */
#define    LOG_VQEC_DP        (37<<3)    /* VQE-C Dataplane */
#define    LOG_VQE_CFGTOOL    (38<<3)    /* CMS */
#define    LOG_VQM            (39<<3)    /* VQM         */
#define    LOG_RPC            (40<<3)    /* VQE RPC library */
#define    LOG_NFACILITIES    41         /* current number of facilities */
