/*------------------------------------------------------------------
 * VQE-C Updater Internal API.
 *
 * NOTE:  These declarations are for use within the updater only.
 *
 * Copyright (c) 2007-2009 by cisco Systems, Inc.
 * All rights reserved.
 *----------------------------------------------------------------*/

#ifndef VQEC_UPDATER_PRIVATE_H
#define VQEC_UPDATER_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "utils/vmd5.h"

#define MAX_LINE_LENGTH 512
#define MAX_DOMAIN_NAME_LENGTH 80
#define MAX_DB_NAME_LENGTH 80

/*
 * Version string used for VQE-C attribute or channel files
 * which are not present.  This string is intended to be user-friendly
 * for use in displays, and does not match any valid MD5 checksums.
 */
#define VQEC_VERSION_FILE_NOT_AVAILABLE VQEC_SYSCFG_CHECKSUM_UNAVAILABLE

/*
 * Version string used for VQE-C network or channel files
 * which have been corrupted.  This string does not match any valid
 * MD5 checksums.
 */
#define VQEC_VERSION_FILE_CORRUPTED "<file corrupted>"

/*
 * The service name below is used by the VQE-C to locate a VCDS
 * when querying the DNS server.  Although the service name is
 * "vqe_channel_cfg", the VCDS specified in the DNS response will
 * be contacted for channel or attribute updates.
 */
#define SERVICE_NAME "vqe_channel_cfg"

/*
 * The resource names below are used by the VQE-C when asking the
 * VCDS for a particular resource, i.e. attribute or channel
 * config file.
 */
#define VQEC_UPDATER_RESOURCE_NAME_INDEX   "index"
#define VQEC_UPDATER_RESOURCE_NAME_NETCFG  "vqec-network-cfg"
#define VQEC_UPDATER_RESOURCE_NAME_CHANCFG "vqe-channels"
#define VQEC_UPDATER_RESOURCE_NAME_MAX_LEN (                     \
        max( max( sizeof(VQEC_UPDATER_RESOURCE_NAME_INDEX),      \
                  sizeof(VQEC_UPDATER_RESOURCE_NAME_NETCFG)),    \
             sizeof(VQEC_UPDATER_RESOURCE_NAME_CHANCFG) )        \
        )

/*
 * Maximum size of VCDS "Server" field in RTSP response
 */
#define VQEC_UPDATER_SERVER_VERSION_LEN      40

/* Result codes */
typedef enum vqec_updater_result_err_ {
    VQEC_UPDATER_RESULT_ERR_OK = 0,            /* update succeeded */
    VQEC_UPDATER_RESULT_ERR_MEM,               /* memory allocation failure */
    VQEC_UPDATER_RESULT_ERR_NA,                /* update not attempted */
    VQEC_UPDATER_RESULT_ERR_UNNECESSARY,       /* 
                                                * update not needed--
                                                * resource is up to date
                                                */
    VQEC_UPDATER_RESULT_ERR_VCDS_LIST,         /*
                                                * could not get VCDS list
                                                * from DNS
                                                */   
    VQEC_UPDATER_RESULT_ERR_VCDS_COMM,         /* 
                                                * could not connect to/
                                                * communicate with/
                                                * get response from
                                                * any VCDS's
                                                */
    VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_OLDVERSION,
                                               /*
                                                * index not supported by VCDS
                                                */
    VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_UNEXPECTED,
                                               /*
                                                * unexpected response from VCDS
                                                */
    VQEC_UPDATER_RESULT_ERR_COMMIT,            /*
                                                * error in processing
                                                * retrieved resource
                                                */
} vqec_updater_result_err_t;

/*
 * Information about a config resource being updated
 */
typedef struct vqec_updater_resource_info_ {
    char datafile[MAX_DB_NAME_LENGTH]; /*
                                        * Pathname of local file to which
                                        * configurations should be written,
                                        * or empty string if configs should
                                        * not be written to a file.
                                        */
    struct timeval last_commit_time;   /*
                                        * Last time config was successfully
                                        * committed.
                                        */
    rel_time_t last_response_time;     /*
                                        * How long it took to receive a
                                        * complete response from VCDS after
                                        * the last update (retrieval) request
                                        * (from issue of RTSP DESCRIBE until
                                        *  file completely arrived)
                                        */
    vqec_updater_result_err_t last_update_result;
                                       /*
                                        * Result of last config update request
                                        */
    uint32_t total_update_attempts;    /* Number resource update attempts */
    uint32_t total_update_failures;    /* Number resource update failures */
    char vqec_version[MD5_CHECKSUM_STRLEN];
    char vcds_version[MD5_CHECKSUM_STRLEN];
                                       /*
                                        * Version identifiers of this resource
                                        * as stored on VQE-C and on the VCDS.
                                        * Version identifier is the MD5
                                        * checksum of the resource,
                                        * represented as a 32-char hex string
                                        * (1 byte = 2 hex chars)
                                        */
} vqec_updater_resource_info_t;

/*
 * The VQE-C updater manager
 *
 * Holds the configuration and state of the VQE-C updater.
 */
typedef struct vqec_updater_mgr_ {
    /*
     * updater configuration
     */
    char domain_name[MAX_DOMAIN_NAME_LENGTH];
                                         /*
                                          * Domain name to use for SRV query
                                          * of a VCDS to provide RTSP service.
                                          * If empty string, the domain name
                                          * of the box (resolv.conf records)
                                          * will be used.
                                          */
    char cname[VQEC_MAX_CNAME_LEN];      /*
                                          * CNAME to be used in RTSP DESCRIBE
                                          * requests
                                          */

    in_addr_t vcds_addr;
    in_port_t vcds_port;                 /*
                                          * VCDS IP & port configured for use
                                          * for update requests.
                                          * If unspecified, the VCDS will be
                                          * learned via SRV query.
                                          */

    /*
     * information about the updater's current state
     */
    vqec_updater_status_t status;        /* status of updater service */

    /*
     * information about the last update request and index file request
     */
    struct timeval last_update_time;     /*
                                          * Last time of an update request
                                          * (for any resource).  This is
                                          * also the time of the last index 
                                          * file request, except for when
                                          * a cached index file is used.
                                          */
    struct {
        int16_t vcds_servers_eligible;   /*
                                          * Number of VCDS servers avail 
                                          * (from config or DNS lookup)
                                          */
        int16_t vcds_servers_attempted;  /*
                                          * Number of VCDS servers VQE-C
                                          * attempted to contact
                                          */

        char vcds_addr[MAX_DOMAIN_NAME_LENGTH];
        in_port_t vcds_port;             /*
                                          * VCDS host/IP & port actually used
                                          * during last update request
                                          * (or 0.0.0.0:0 if no VCDS servers
                                          *  servers could be identified )
                                          */
        vqec_updater_result_err_t result;/*
                                          * Result of last index file request
                                          */
        char server_version[VQEC_UPDATER_SERVER_VERSION_LEN];
                                         /*
                                          * Version string of last contacted 
                                          * server
                                          */
    } last_index_request;
    uint32_t total_update_attempts;      /* Total index retrieval attempts */
    uint32_t total_update_failures;      /* Total index retrieval failures */

    /*
     * information about the next index file request to be issued
     */
    struct {
        int32_t seconds_until_update;    /* 
                                          * Number of seconds until the next
                                          * update request is to be attempted,
                                          * inclusive of update_window (below)
                                          * -1 implies no pending update.
                                          */
        int32_t update_window;           /*
                                          * Random number of seconds chosen
                                          * for the upcoming index request's
                                          * update window.
                                          */
    } next_index_request;

    /*
     * Resource information
     */
    vqec_updater_resource_info_t attrcfg;
                                         /* Attribute config update info */
    vqec_updater_resource_info_t chancfg;
                                         /* Channel config update info */
} vqec_updater_mgr_t;

/* VQE-C Updater state */
UT_STATIC vqec_updater_mgr_t vqec_updater;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VQEC_UPDATER_PRIVATE_H */
