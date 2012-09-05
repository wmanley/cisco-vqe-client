/*
 * vqec_updater.c - Functions that are used to retrieve configuration
 * information supplied by the VCDS.
 *
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif // _VQEC_UTEST_INTERPOSERS

#include <sys/time.h>
#include <pthread.h>
#include <utils/strl.h>
#include <utils/vam_util.h>

#include "vqe_token_bucket.h"
#include "vqec_gap_reporter.h"
#include "vqec_debug.h"
#include "vqec_assert_macros.h"
#include <rtp/rtcp_stats_api.h>
#include "vqec_channel_private.h"
#include <resolv.h>
#include "vqec_updater.h"
#include "vqec_lock_defs.h"
#include "vqec_debug.h"
#include "vqec_error.h"
#include "vqec_syscfg.h"
#include "vqec_ifclient.h"
#include "vqec_pthread.h"
#include "rtsp_client.h"
#include "srv_lookup.h"
#include "cfgapi.h"
#include "cfg_channel.h"
#include "utils/vmd5.h"
#include "vqec_updater_private.h"
#include "vqec_ifclient_private.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif // _VQEC_UTEST_INTERPOSERS

/* Static VQE-C Updater state */
UT_STATIC vqec_updater_mgr_t vqec_updater;
static boolean vqec_updater_update_request_issued = FALSE;
                                         /*
                                          * TRUE if a triggered update request
                                          * is pending for the updater thread.
                                          */
static pthread_t vqec_updater_thread_id;


#define vqec_updater_polling_enabled() \
    (vqec_syscfg_get_ptr()->update_interval_max)
#define vqec_updater_get_in_progress()                                 \
    ((vqec_updater.next_index_request.seconds_until_update != -1) &&   \
     (vqec_updater.next_index_request.seconds_until_update <           \
      vqec_updater.next_index_request.update_window))

/*
 * Returns TRUE if CDI is enabled, FALSE otherwise.
 *
 * @param[out] boolean  - indicates whether CDI is enabled or not
 */
boolean
vqec_updater_get_cdi_enable (void)
{
    vqec_syscfg_t cfg;

    /*
     * NOTE:  This fetches the information from the system configuration--
     *        The updater does not currently store this information internally.
     */
    vqec_syscfg_get(&cfg);
    return (cfg.cdi_enable);
}

/*
 * Maps an updater status code to a string.
 *
 * @param[in]  status  - status to be described
 * @param[out] char *  - descriptive string for status
 */
const char *
vqec_updater_status2str(vqec_updater_status_t status)
{
    switch (status) {
    case VQEC_UPDATER_STATUS_UNINITIALIZED:
        return "uninitialized";
    case VQEC_UPDATER_STATUS_INITIALIZED:
        return "initialized";
    case VQEC_UPDATER_STATUS_RUNNING:
        return "running";
    case VQEC_UPDATER_STATUS_DYING:
        return "dying";
    default:
        return "unknown updater status";
    }
}

/*
 * Maps an updater result code to a string.
 *
 * @param[in]  result   - result to be described
 * @param[out] char *   - descriptive string for result
 */
const char *
vqec_updater_result2str(vqec_updater_result_err_t result)
{
    switch (result) {
    case VQEC_UPDATER_RESULT_ERR_OK:
        return "success";
    case VQEC_UPDATER_RESULT_ERR_MEM:
        return "memory allocation failure";
    case VQEC_UPDATER_RESULT_ERR_NA:
        return "update not attempted";
    case VQEC_UPDATER_RESULT_ERR_UNNECESSARY:
        return "update not necessary";
    case VQEC_UPDATER_RESULT_ERR_VCDS_LIST:
        return "could not retrieve VCDS list";
    case VQEC_UPDATER_RESULT_ERR_VCDS_COMM:
        return "communication error with VCDS";
    case VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_OLDVERSION:
        return "unsupported by VCDS version";
    case VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_UNEXPECTED:
        return "unexpected response from VCDS";
    case VQEC_UPDATER_RESULT_ERR_COMMIT:
        return "error in processing received file";
    default:
        return "unknown updater error";
    }

}

/*
 * Converts the supplied time value to a date format.
 * Callers should use the returned value immediately, as subsequent calls
 * to this function will overwrite the contents of the returned string.
 *
 * @param[in] last_update_tv  - time value to be described
 * @param[out] char *         - date-format string for last_update_tv, or
 *                              NULL if last_update_tv is (0,0)
 */ 
char *
vqec_updater_timeval2str (struct timeval last_update_tv)
{
    struct tm *last_update_tm;
#define TIME_STRLEN 80
    static char timestring[TIME_STRLEN];

    if (!last_update_tv.tv_sec && !last_update_tv.tv_usec) {
        return (NULL);
    }

    last_update_tm = localtime(&(last_update_tv.tv_sec));
    (void)strftime(timestring, sizeof(timestring), "%h %e %T", last_update_tm);
    return (timestring);
}


/*
 * Triggers an update to be handled by the updater service thread.
 * The update will occur after waiting for a random portion of the
 * configured "update_window".
 *
 * @param[in] force_update     - If TRUE, force an update regardless of
 *                               whether polled updates are configured.
 *                               If FALSE, only perform an update if
 *                               polled updates are not already configured.
 */
void 
vqec_updater_update (boolean force_update)
{
    if (!force_update && vqec_updater_polling_enabled()) {
        return;
    }

    /* Just set a flag--this will be monitored by the service thread */
    vqec_updater_update_request_issued = TRUE;
}

/*
 * Gets information about the status of updates.
 *
 * @param[out] status       - current status of updater
 * @param[out] in_progress  - TRUE if an update request is in progress
 *                             (from start of the randomized update window
 *                              chosen for the index file request until
 *                              updates for all necessary resources have
 *                              been requested, received, and processed--or
 *                              aborted, upon update failure).
 *                            FALSE otherwise
 * @param[out] seconds_until_next_update
 *                          - number of seconds until the next update
 *                            request (including the update window), or
 *                            -1 if no update is currently scheduled
 *                            (polling is disabled for the resource)
 * @param[out] attrcfg_last_commit_time
 *                          - time of last successful configuration update
 *                            of an attribute config file
 * @param[out] attrcfg_version
 *                          - current version string of attribute config file
 * @param[out] chancfg_last_commit_time
 *                          - time of last successful configuration update
 *                            of a channel config file
 * @param[out] chancfg_version
 *                          - current version string of channel config file
 */
vqec_error_t
vqec_updater_get_status (vqec_updater_status_t *status,
                         boolean *in_progress,
                         int32_t *seconds_until_next_update,
                         struct timeval *attrcfg_last_commit_time,
                         char **attrcfg_version,
                         struct timeval *chancfg_last_commit_time,
                         char **chancfg_version)

{
    if (status) {
        *status = vqec_updater.status;
    }
    if (in_progress) {
        *in_progress = vqec_updater_get_in_progress();
    }
    if (seconds_until_next_update) {
        *seconds_until_next_update = 
            vqec_updater.next_index_request.seconds_until_update;
    }
    if (attrcfg_last_commit_time) {
        *attrcfg_last_commit_time = vqec_updater.attrcfg.last_commit_time;
    }
    if (attrcfg_version) {
        *attrcfg_version = vqec_updater.attrcfg.vqec_version;
    }
    if (chancfg_last_commit_time) {
        *chancfg_last_commit_time = vqec_updater.chancfg.last_commit_time;
    }
    if (chancfg_version) {
        *chancfg_version = vqec_updater.chancfg.vqec_version;
    }
    return (VQEC_OK);
}

/*
 * Clears the updater counters.
 */
void
vqec_updater_clear_counters (void)
{
    /* Clear the updater counters */
    vqec_updater.total_update_attempts = 0;
    vqec_updater.total_update_failures = 0;
    vqec_updater.attrcfg.total_update_attempts = 0;
    vqec_updater.attrcfg.total_update_failures = 0;
    vqec_updater.chancfg.total_update_attempts = 0;
    vqec_updater.chancfg.total_update_failures = 0;
}

/*
 * Returns TRUE if the supplied result should be considered a failure
 * for stats display, or FALSE otherwise.
 *
 * @param[in] result    - result code
 * @param[out] boolean  - TRUE if 'result' denotes a failure, or
 *                        FALSE otherwise
 */ 
boolean
vqec_updater_result_is_failure (vqec_updater_result_err_t result)
{
    switch (result) {
    case VQEC_UPDATER_RESULT_ERR_MEM:
    case VQEC_UPDATER_RESULT_ERR_VCDS_LIST:
    case VQEC_UPDATER_RESULT_ERR_VCDS_COMM:
    case VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_OLDVERSION:
    case VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_UNEXPECTED:
    case VQEC_UPDATER_RESULT_ERR_COMMIT:
        return (TRUE);
    default:
        return (FALSE);
    }
}

/*
 * Write the contents in the buffer to a local file.
 *
 * @param[in] filename  - filename to which buffer is written
 * @param[in] buffer    - contains data to be written
 * @param[out] boolean  - TRUE returned if file was succesfully written
 *                        FALSE returned otherwise
 */
boolean 
vqec_updater_write_to_file (const char *filename,
                            const char *buffer)
{
    FILE *fp;
    
    if (!filename || filename[0] == '\0') {
        VQEC_LOG_ERROR("vqec_updater_write_to_file:: No "
                       "configuration file will be written out.\n");
        return (FALSE);
    }
    
    /* If no buffer to write, write an empty file */
    if (!buffer) {
        buffer = "";
    }

    /* Write the configuration data to the file if filename is given */
    fp = fopen(filename, "write");
    if (fp == NULL) {
        VQEC_LOG_ERROR("vqec_updater_write_to_file:: Failed to open the "
                       "file %s to write\n",
                       filename);
        return (FALSE);
    }
    
    if (fprintf(fp, "%s", buffer) < 0) {
        VQEC_LOG_ERROR("vqec_updater_write_to_file:: Failed to write "
                       "the file %s\n", filename);
        
        /* sa_ignore IGNORE_RETURN */
        remove(filename);
        return (FALSE);
    }
    
    /* sa_ignore IGNORE_RETURN */
    fflush(fp);
    fclose(fp);

    return (TRUE);
}


/*
 * Builds a string which may be used as the RTSP resource being
 * requested from a VCDS.  Format of the NULL-terminated string is:
 *   "<resource string>/<CNAME>" 
 *
 * NOTE:  The contents of the returned resource string should be used
 *        prior to this API being invoked again, since subsequent calls
 *        to this API will overwrite the contents of the string.
 *
 * @param[in] resource_name  - name of file to be requested
 * @param[in] use_cname      - TRUE if cname should be included in request
 *                             FALSE otherwise (for backward compatibility
 *                                              with an older VCDS)
 * @param[out] char *        - resource string
 */
const char *
vqec_updater_build_rtsp_obj_str (const char *resource_name,
                                 boolean use_cname)
{
    static char 
        rtsp_obj_str[VQEC_UPDATER_RESOURCE_NAME_MAX_LEN + 
                     VQEC_MAX_CNAME_LEN + 2];
    int len;
    
    len = strlcpy(rtsp_obj_str, resource_name, sizeof(rtsp_obj_str));
    if (use_cname) {
        len = strlcat(rtsp_obj_str, "/", sizeof(rtsp_obj_str));
        len = strlcat(rtsp_obj_str, vqec_updater.cname, sizeof(rtsp_obj_str));
    }
    if (len >= sizeof(rtsp_obj_str)) {
        return NULL;
    }
    return rtsp_obj_str;
}
                                            
/*
 * Callback to process a retrieved index file.
 * The file is parsed, and the version strings for each resource are
 * recorded in the updater's state.
 *
 * @param[in]   arg      not used
 * @param[out]  boolean  TRUE if file was succesfully processed
 *                       FALSE otherwise
 */
UT_STATIC boolean
fetch_index_cb (boolean arg)
{
    char *buffer = rtsp_get_response_body();
    char line_buffer[MAX_LINE_LENGTH+1];
    char *resource_name = NULL, *resource_version = NULL, *saveptr = NULL;
    boolean success = TRUE;
    int start_idx = 0;
    int total_buffer_len;

    if (!buffer) {
        buffer = "";
    }
    
    VQEC_DEBUG(VQEC_DEBUG_UPDATER,
               "Received index file, contents: '%s'\n", buffer);
    
    /*
     * Initialize the resources VQE-C should have, to be extracted 
     * from the index file.  If a resource is not listed in the
     * file, the VQE-C should not be using it.
     */
    strncpy(vqec_updater.attrcfg.vcds_version, VQEC_VERSION_FILE_NOT_AVAILABLE,
            MD5_CHECKSUM_STRLEN);
    strncpy(vqec_updater.chancfg.vcds_version, VQEC_VERSION_FILE_NOT_AVAILABLE,
            MD5_CHECKSUM_STRLEN);

    total_buffer_len = strlen(buffer);
    while ((start_idx = read_line(line_buffer, 
                                  MAX_LINE_LENGTH,
                                  buffer, 
                                  total_buffer_len,
                                  start_idx)) != -1) {
        /*
         * Parse the line, 
         * looking for "<resource_name> <resource_version>" format
         */
        resource_name = strtok_r(line_buffer, " ", &saveptr);
        resource_version = strtok_r(NULL, " \r\n", &saveptr);
        /* Verify the expected tokens were parsed, or else skip the line */
        if (!resource_name ||
            !resource_version ||
            (strlen(resource_version) != (MD5_CHECKSUM_STRLEN - 1))) {
            continue;
        }
        /* Look for known resources, ignoring unrecognized resources */
        if (!strncmp(resource_name, VQEC_UPDATER_RESOURCE_NAME_NETCFG,
                     strlen(VQEC_UPDATER_RESOURCE_NAME_NETCFG))) {
            strncpy(vqec_updater.attrcfg.vcds_version, resource_version,
                    MD5_CHECKSUM_STRLEN);
        } else if (!strncmp(resource_name, VQEC_UPDATER_RESOURCE_NAME_CHANCFG,
                            strlen(VQEC_UPDATER_RESOURCE_NAME_CHANCFG))) {
            strncpy(vqec_updater.chancfg.vcds_version, resource_version,
                    MD5_CHECKSUM_STRLEN);
        }
    }

    return (success);
}

/*
 * Processes an update to the attribute configuration file.
 *
 * If 'erase' is FALSE, then the updated file is accessible in a buffer
 * referenced by the rtsp_get_response_body() API.
 * If 'erase' is TRUE, then the file is to be erased.
 *
 * Attribute configurations are always written to a file--
 * it is a fatal error if this does not succeed.
 *
 * @param[in]   erase    TRUE to indicate the existing file should be erased
 *                       FALSE to indicate the existing file should be updated
 * @param[out]  boolean  TRUE if the update was succesfully processed
 *                       FALSE otherwise
 */
UT_STATIC boolean
vqec_updater_update_attrcfg (boolean erase)
{
    char *buffer = rtsp_get_response_body();
    boolean success = TRUE;
    boolean lock_acquired;
    int err;
    char index_checksum[MD5_CHECKSUM_STRLEN];
    static vqec_syscfg_t cfg;
    vqec_error_t status;
    char log_str[VQEC_LOGMSG_BUFSIZE];

    if (!buffer) {
        buffer = "";
    }

    /*
     * Time to commit the update.  We need to make sure that 
     * the commit occurs atomically with respect to any other
     * VQE-C APIs that may be executing in other threads.
     */
    lock_acquired = FALSE;
    if (vqec_updater.status == VQEC_UPDATER_STATUS_RUNNING) {
        /* 
         * VQE-C has already been initialized and may still be running,
         * so lock is needed
         */
        vqec_lock_lock(vqec_g_lock);
        lock_acquired = TRUE;
    }
    /*
     * At this point the possibilities are:
     *  o running w/  lock_acquired
     *  o init    w/o lock_acquired
     *  o dying   w/  lock_acquired
     *  o dying   w/o lock_acquired
     */
    if (vqec_updater.status == VQEC_UPDATER_STATUS_DYING) {
        success = FALSE;
    } else {
        if (erase) {
            if (vqec_updater.attrcfg.datafile[0] != '\0') {
                err = remove(vqec_updater.attrcfg.datafile);
                if (err < 0) {
                    VQEC_LOG_ERROR("vqec_updater_update_attrcfg:: Failed to "
                                   "remove the file %s\n",
                                   vqec_updater.attrcfg.datafile);
                    success = FALSE;
                }
                strncpy(index_checksum, VQEC_SYSCFG_CHECKSUM_UNAVAILABLE, 
                        MD5_CHECKSUM_STRLEN);
            }
        } else {
            success = vqec_updater_write_to_file(vqec_updater.attrcfg.datafile,
                                                 buffer);
            vqe_MD5ComputeChecksumStr(buffer, FALSE, index_checksum);
        }
        /*
         * Update the index file, if one is in use.
         */
        if (success && vqec_syscfg_get_ptr()->index_cfg_pathname[0]) {
            status = vqec_syscfg_index_write(VQEC_IFCLIENT_CONFIG_NETWORK,
                                             index_checksum);
            if (status != VQEC_OK) {
                remove(vqec_updater.attrcfg.datafile);
                success = FALSE;
            }
        }
    }

    if (success) {
        /* Update system configuration */
        vqec_syscfg_read(&cfg);
        vqec_syscfg_set(&cfg, TRUE);
    }

    if (lock_acquired) {
        vqec_lock_unlock(vqec_g_lock);
    }

    if (!success) {
        goto done;
    }

    /* Commit succeeded */
    gettimeofday(&vqec_updater.attrcfg.last_commit_time, NULL);
    if (erase) {
        snprintf(vqec_updater.attrcfg.vqec_version,
                 MD5_CHECKSUM_STRLEN, VQEC_VERSION_FILE_NOT_AVAILABLE);
    } else {
        strncpy(vqec_updater.attrcfg.vqec_version, index_checksum, 
                MD5_CHECKSUM_STRLEN);
    }
    if (vqec_info_logging()) {
        snprintf(log_str, VQEC_LOGMSG_BUFSIZE,
                 "CDI Network Configuration update received (checksum=%s)",
                 vqec_updater.attrcfg.vqec_version);
        syslog_print(VQEC_INFO, log_str);
    }

done:
    return (success);
}

/*
 * Processes an update to the channel configuration file.
 *
 * If 'erase' is FALSE, then the updated file is accessible in a buffer
 * referenced by the rtsp_get_response_body() API.
 * If 'erase' is TRUE, then the file is to be erased.
 *
 * Channel configurations are validated and committed upon receipt--
 * it is a fatal error if this does not succeed.
 *
 * @param[in]   erase    TRUE to indicate the existing file should be erased
 *                       FALSE to indicate the existing file should be updated
 * @param[out]  boolean  TRUE if file was succesfully processed
 *                       FALSE otherwise
 */
UT_STATIC boolean
vqec_updater_update_chancfg (boolean erase)
{
    int ret;
    cfg_ret_e cfg_error;
    uint32_t num_parsed, num_validated, total;
    struct sched_param sp;
    int32_t policy;
    boolean lock_acquired;
    boolean success = TRUE;
    char *buffer;
    char index_checksum[MD5_CHECKSUM_STRLEN];
    vqec_error_t status;
    char log_str[VQEC_LOGMSG_BUFSIZE];

    buffer = rtsp_get_response_body();
    if (!buffer || erase) {
        buffer = "";
    }

    /*
     * Give the channel manager the newly updated/erased channel lineup
     * for validation.  If validation fails, abort the update.
     */
    cfg_error = cfg_parse_all_channel_data(buffer);
    if (cfg_error != CFG_SUCCESS) {
        VQEC_LOG_ERROR("vqec_updater_update_chancfg:: Failed to parse the "
                       "configuration data\n");
        success = FALSE;
        goto done;
    }
    
    /*
     * Make any necessary updates to the file (no locks needed, as only
     * this thread should be accessing the channel config file).  
     * File-based errors are non-fatal, as the file is just a cache.
     */
    if (vqec_updater.chancfg.datafile[0] != '\0') {
        if (erase) {
            ret = remove(vqec_updater.chancfg.datafile);
            if (ret < 0) {                
                VQEC_LOG_ERROR("vqec_updater_update_chancfg:: Failed to "
                               "remove the file %s\n",
                               vqec_updater.chancfg.datafile);
            }
            strncpy(index_checksum, VQEC_SYSCFG_CHECKSUM_UNAVAILABLE, 
                    MD5_CHECKSUM_STRLEN);
        } else {
            (void)vqec_updater_write_to_file(vqec_updater.chancfg.datafile,
                                             buffer);
            vqe_MD5ComputeChecksumStr(buffer, FALSE, index_checksum);
        }
        /*
         * Update the index file, if one is in use.
         */
        if(vqec_syscfg_get_ptr()->index_cfg_pathname[0]) {
            status = vqec_syscfg_index_write(VQEC_IFCLIENT_CONFIG_CHANNEL,
                                             index_checksum);
            if (status != VQEC_OK) {
                remove(vqec_updater.chancfg.datafile);
            }
        }
    }

    /* 
     * Commit the newly retrieved lineup.
     */

    /*
     * Elevate priority of the thread to realtime so we can 
     * finish up the task of db-update at highest priority.
     * Assumes that update_commit() has not modified 
     * the thread priority.
     */
    ret = pthread_getschedparam(pthread_self(), &policy, &sp);
    if (!ret) {
        (void)vqec_pthread_set_priosched(pthread_self());
    } else {
        printf("=== Unable to retrieve sched attributes %s === \n", 
               strerror(ret));
    }

    /*
     * Time to commit the update.  We need to make sure that 
     * the commit occurs atomically with respect to any other
     * VQE-C APIs that may be executing in other threads.
     */
    lock_acquired = FALSE;
    if (vqec_updater.status == VQEC_UPDATER_STATUS_RUNNING) {
        /* 
         * VQE-C has already been initialized and may still be running,
         * so lock is needed
         */
        vqec_lock_lock(vqec_g_lock);
        lock_acquired = TRUE;
    }
    /*
     * At this point the possibilities are:
     *  o running w/  lock_acquired
     *  o init    w/o lock_acquired
     *  o dying   w/  lock_acquired
     *  o dying   w/o lock_acquired
     */
    if (vqec_updater.status == VQEC_UPDATER_STATUS_DYING) {
        cfg_error = CFG_FAILURE;
    } else {
        cfg_error = cfg_commit_update();
    }
    if (lock_acquired) {
        vqec_lock_unlock(vqec_g_lock);
    }

    /*
     * Revert thread priority back to it's nominal value.
     */
    if (!ret) {
        if (pthread_setschedparam(pthread_self(), policy, &sp)) {
            VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                       "=== Unable to reset sched attributes %s === \n", 
                       strerror(ret));
        }
        if (!pthread_getschedparam(pthread_self(), &policy, &sp)) {
            VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                       "=== pthread (%u) policy (%s), prio (%d) ===\n",
                       (uint32_t)pthread_self(), 
                       (policy == SCHED_OTHER) ? "low" : "realtime", 
                       sp.sched_priority);
        }
    }
    
    /* Abort upon failure of commit */
    if (cfg_error != CFG_SUCCESS) {
        VQEC_LOG_ERROR("vqec_updater_update_chancfg:: Failed to commit "
                       "the newly received channel configuration data\n");
        success = FALSE;
        goto done;
    }

    /* Commit succeeded */
    gettimeofday(&vqec_updater.chancfg.last_commit_time, NULL);
    if (erase) {
        snprintf(vqec_updater.chancfg.vqec_version,
                 MD5_CHECKSUM_STRLEN, VQEC_VERSION_FILE_NOT_AVAILABLE);
    } else {
        (void)vqe_MD5ComputeChecksumStr(buffer, FALSE,
                                    vqec_updater.chancfg.vqec_version);
    }
    if (vqec_info_logging()) {
        snprintf(log_str, VQEC_LOGMSG_BUFSIZE,
                 "CDI Channel Configuration update received (checksum=%s)",
                 vqec_updater.chancfg.vqec_version);
        syslog_print(VQEC_INFO, log_str);
    }
    /* Get the channel stats and report via debug */
    if (cfg_get_cfg_stats(&num_parsed, &num_validated, &total)
        == CFG_SUCCESS) {
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "Channel update summary: %d out of %d channels "
                   "passed the validation\n", num_validated, total);
    }
done:
    return (success);
}

/*
 * Fetches and processes a resource from a VCDS. 
 * 
 * @param[in]  resource_name     - name of file to be retrieved from VCDS
 *                                  (excludes CNAME)
 * @param[in]  use_cname         - TRUE if CNAME should be used in RTSP URL
 * @param[in]  accept_media_type - media type to accept in RTSP header
 * @param[in]  update_abort_time - time after which update should be aborted
 * @param[out] response_time     - [optional] amount of time it took to 
 *                                  receive the complete VCDS response
 * @param[in]  fetch_resource_cb - callback function to process resource
 *                                  contents after receipt
 * @param[out] vqec_updater_result_err_t
 *                               - result of update request
 */ 
vqec_updater_result_err_t
vqec_updater_fetch_resource (const char *resource_name,
                             boolean use_cname,
                             const char *accept_media_type,
                             abs_time_t update_abort_time,
                             rel_time_t *response_time,
                             boolean (*fetch_resource_cb)(boolean arg))
{
#define PROTOCOL "tcp"
#define PROTOCOL_CODE TCP /* for use with a specific rtsp server */
    vqec_updater_result_err_t err = VQEC_UPDATER_RESULT_ERR_OK;
    rtsp_ret_e err_rtsp = RTSP_SUCCESS;
    remote_server_t *rtsp_server = NULL; 
    remote_server_t **server_list = NULL;
    int i, total_servers = 1, rtsp_server_response_code;
    remote_server_t specific_rtsp_server;
    struct in_addr addr;
    abs_time_t response_time_start;
    char *rtsp_server_response_version = NULL;
    char server_version[VQEC_UPDATER_SERVER_VERSION_LEN];

    VQEC_ASSERT(resource_name && fetch_resource_cb);

    VQEC_DEBUG(VQEC_DEBUG_UPDATER,
               "Starting update of \"%s\"...\n", resource_name);
    /*
     * Build a VCDS server list to be contacted for the requested resource:
     *   a. if vcds_addr manually configured
     *        use it as the only server in the list
     *   b. if no vcds_addr supplied:
     *        if the request is for an index file,
     *          construct a list of VCDS's from DNS server and randomize
     *        else
     *          use the single VCDS that supplied the index file.
     */
    if (vqec_updater.vcds_addr) {

        /* VCDS manually configured:  build the list using just this one */
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "vqec_updater_fetch_resource:: Using manually configured "
                   "VQE channel configuration server ...\n");
        server_list = malloc(sizeof(remote_server_t*));
        if (!server_list) {
            VQEC_LOG_ERROR("vqec_updater_fetch_resource:: Failed to allocate"
                           " memory for server list\n");
            err = VQEC_UPDATER_RESULT_ERR_MEM;
            goto done;
        }
        specific_rtsp_server.type = PROTOCOL_CODE;
        addr.s_addr = vqec_updater.vcds_addr;
        /* sa_ignore DISABLE_RETURNS inet_ntop */
        inet_ntop(AF_INET, &addr.s_addr,
                  specific_rtsp_server.addr, NAME_LEN);
        specific_rtsp_server.port = vqec_updater.vcds_port;
        /* not setting host, priority, weight, and score */
        
        server_list[0] = &specific_rtsp_server;
        total_servers = 1;  /* just the one specified server */

    } else if (!strcmp(resource_name, VQEC_UPDATER_RESOURCE_NAME_INDEX)) {

        vqec_updater.last_index_request.vcds_addr[0] = '\0';
        vqec_updater.last_index_request.vcds_port = 0;

        /* VCDS not configured:  learn the list from the DNS server */
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "vqec_updater_fetch_resource:: Locating a list of "
                   "VQE channel configuration servers ...\n");

        rtsp_server = lookup_SRV(vqec_updater.domain_name,
                                 SERVICE_NAME,
                                 PROTOCOL);
        if (rtsp_server == NULL) {
            VQEC_LOG_ERROR("vqec_updater_fetch_resource:: Could not find any "
                           "VQE configuration servers from DNS query\n");
            err = VQEC_UPDATER_RESULT_ERR_VCDS_LIST;
            goto done;
        }

        /* Randomize the servers from the linked list */
        /* Figure out how many servers in the list */
        remote_server_t *server_p = rtsp_server;
        for ( ; server_p->next != NULL; server_p = server_p->next) {
            total_servers++;
            server_p->score = 0.0;
        }
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "vqec_updater_fetch_resource:: Total number of "
                   "VQE configuration servers in service = %d\n",
                   total_servers);
        
        /* Allocate a new array for the randomized server list */
        /* The memory will get freed after the server_list being used */
        server_list = malloc(sizeof(remote_server_t*) * total_servers);
        if (server_list == NULL) {
            VQEC_LOG_ERROR("vqec_updater_fetch_resource:: Failed to allocate "
                           "memory for randomized server list\n");
            err = VQEC_UPDATER_RESULT_ERR_MEM;
            goto done;
        }
        
        if (total_servers > 1) {
            /* Use NTP timestamp as the seed */
            /* for the random number generator */
            unsigned int seed = 
                abs_time_to_usec(ntp_to_abs_time(get_ntp_time()));
            srandom(seed);
            
            /* Allocate a server index array */
            int *server_index = NULL;
            if ((server_index = (int *) malloc(sizeof(int) * total_servers))
                == NULL) {
                VQEC_LOG_ERROR("vqec_updater_fetch_resource:: Failed to allocate"
                               " memory for server index array\n");
                err = VQEC_UPDATER_RESULT_ERR_MEM;
                goto done;
            }
            
            for (i = 0; i < total_servers; i++) {
                server_index[i] = i;
            }
            
            /* Randomly shuffle the array */
            int pick_one, temp;
            for (i = 0; i < total_servers; i++) {
                pick_one = random() / (RAND_MAX / total_servers);
                temp = server_index[i];
                server_index[i] = server_index[pick_one];
                server_index[pick_one] = temp;
            }
            
            /* Assign the servers according to the random list */
            server_p = rtsp_server;
            for (i = 0; i < total_servers; i++) {
                server_list[server_index[i]] = server_p;
                server_p = server_p->next;
            }
            free(server_index);
        } else {
            /* There is only one server out there */
            server_list[0] = rtsp_server;
        }
    } else {
        server_list = malloc(sizeof(remote_server_t*));
        if (!server_list) {
            VQEC_LOG_ERROR("vqec_updater_fetch_resource:: Failed to allocate"
                           " memory for server list\n");
            err = VQEC_UPDATER_RESULT_ERR_MEM;
            goto done;
        }
        specific_rtsp_server.type = PROTOCOL_CODE;
        strncpy(specific_rtsp_server.addr,
                vqec_updater.last_index_request.vcds_addr,
                NAME_LEN);
        specific_rtsp_server.port = vqec_updater.vcds_port;
        /* not setting host, priority, weight, and score */

        server_list[0] = &specific_rtsp_server;
        total_servers = 1;  /* just the one specified server */
    }

    /*
     * We now have a non-empty "server_list[]" containing "total_servers"
     * entries.   Loop through the RTSP server list and find the first
     * one successfully service our request.  "successfully" here means
     *   a) we can open a TCP connection to it, and
     *   b) we can send it a request and get back an answer from it
     */
    for (i = 0; i < total_servers; i++) {

        if (!server_list[i]->addr) {
            continue;
        }

        /*
         * Create a RTSP client, opening the TCP connection to a VCDS
         * If this fails, move on to the next VCDS.
         */
        strncpy(vqec_updater.last_index_request.vcds_addr,
                server_list[i]->addr, MAX_DOMAIN_NAME_LENGTH);
        vqec_updater.last_index_request.vcds_port = server_list[i]->port;
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "vqec_updater_fetch_resource:: "
                   "Contacting the RTSP server %s port %d ...\n",
                   vqec_updater.last_index_request.vcds_addr,
                   server_list[i]->port);
        err_rtsp = rtsp_client_init(vqec_updater.last_index_request.vcds_addr,
                                    server_list[i]->port);
        if (err_rtsp != RTSP_SUCCESS) {
            VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                       "vqec_updater_fetch_resource:: Failed to "
                       "connect the RTSP server %s\n",
                       vqec_updater.last_index_request.vcds_addr);
            continue;
        }

        /*
         * Send out the DESCRIBE request, and await the response
         * If this fails, move on to the next VCDS.
         * Record an approximate response time if requested by the caller.
         */
        response_time_start = get_sys_time();
        if (TIME_CMP_A(gt, response_time_start, update_abort_time)) {
            VQEC_LOG_ERROR("vqec_updater_fetch_resource:: Timeout while "
                           "getting the configuration data\n");
            rtsp_client_close();
            break;
        }
        err_rtsp = 
            rtsp_send_request(
                vqec_updater_build_rtsp_obj_str(resource_name, use_cname),
                accept_media_type,
                update_abort_time);
        if (err_rtsp != RTSP_SUCCESS) {
            VQEC_LOG_ERROR("vqec_updater_fetch_resource:: Failed to "
                           "get the configuration data\n");
            rtsp_client_close();
            continue;
        }
        if (response_time) {
            *response_time = TIME_SUB_A_A(get_sys_time(), response_time_start);
        }

        /*
         * Response received:  parse and validate the response body
         */
        rtsp_server_response_version = rtsp_get_response_server();
        if (rtsp_server_response_version) {
            strncpy(server_version,
                    rtsp_server_response_version,
                    VQEC_UPDATER_SERVER_VERSION_LEN);
            server_version[VQEC_UPDATER_SERVER_VERSION_LEN - 1] = '\0';
        }
        rtsp_server_response_code = rtsp_get_response_code();
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "vqec_updater_fetch_resource:: Received response code %d "
                   "for RTSP DESCRIBE from the RTSP server %s\n",
                   rtsp_server_response_code,
                   vqec_updater.last_index_request.vcds_addr);
        if (!strcmp(resource_name, VQEC_UPDATER_RESOURCE_NAME_INDEX) &&
            ((rtsp_server_response_code == MSG_404_NOT_FOUND) ||
             (rtsp_server_response_code == MSG_415_UNSUPPORTED_MEDIA_TYPE)) &&
            !rtsp_get_response_server()) {
            /*
             * VCDS neither supplied an index file nor identified its version.
             * From this we assume index files are not supported
             * due to the VCDS being an older version, and revert to
             * retrieving only the channel lineup without using a CNAME.
             */
            err = VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_OLDVERSION;
        } else if (rtsp_server_response_code == MSG_200_OK) {
            if ((*fetch_resource_cb)(FALSE)) {
                err = VQEC_UPDATER_RESULT_ERR_OK;
            } else {
                err = VQEC_UPDATER_RESULT_ERR_COMMIT;
            }
        } else {
            VQEC_LOG_ERROR("vqec_updater_fetch_resource::  Received "
                           "unexpected response code %d for RTSP DESCRIBE "
                           "from the RTSP server %s\n",
                           rtsp_server_response_code,
                           vqec_updater.last_index_request.vcds_addr);
            err = VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_UNEXPECTED;
        }

        /* Close the RTSP connection */
        rtsp_client_close();
        break;

    }

    if (i == total_servers) {
        /* All VCDS servers were tried unsuccessfully */
        err = VQEC_UPDATER_RESULT_ERR_VCDS_COMM;
        i = total_servers - 1;
    }
    /* Record some stats about the VCDS for index file requests */
    if (!strcmp(resource_name, VQEC_UPDATER_RESOURCE_NAME_INDEX)) {
        vqec_updater.last_index_request.vcds_servers_eligible = total_servers;
        vqec_updater.last_index_request.vcds_servers_attempted = i+1;
        if (rtsp_server_response_version) {
            strncpy(vqec_updater.last_index_request.server_version,
                    server_version, VQEC_UPDATER_SERVER_VERSION_LEN);
        } else {
            snprintf(vqec_updater.last_index_request.server_version,
                     VQEC_UPDATER_SERVER_VERSION_LEN,
                     "<unspecified>");
        }
    }

done:
    if (server_list != NULL) {
        free(server_list);
    }
    if (rtsp_server != NULL) {
        free_SRV();
    }
    return (err);
}


/*
 * Checks if an updated is needed for the specified resources,
 * and performs an update if one is needed.
 *
 * @param[in] update_index    - TRUE to request a new index file from VCDS
 *                               for determining most recent versions 
 *                               available on the VCDS
 *                              FALSE to use the last cached index file
 *                               for determining most recent versions
 *                               available on the VCDS
 * @param[in] update_attr     - TRUE to request update of attribute file
 * @param[in] update_chan     - TRUE to request update of channel config
 * @param[in] update_abort_time
 *                            - time after which updates should be aborted
 */
void
vqec_updater_request_update (boolean update_index,
                             boolean update_attr,
                             boolean update_chan,
                             abs_time_t update_abort_time)
{
    struct timeval time_tv;
    struct tm *time_tm;
    static char timestring1[TIME_STRLEN], timestring2[TIME_STRLEN];

    if (VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_UPDATER)) {
        time_tv = abs_time_to_timeval(update_abort_time);
        time_tm = localtime(&(time_tv.tv_sec));
        (void)strftime(timestring1, sizeof(timestring1), "%h %e %T", time_tm);
        (void)VQE_GET_TIMEOFDAY(&time_tv, 0);
        time_tm = localtime(&(time_tv.tv_sec));
        (void)strftime(timestring2, sizeof(timestring2), "%h %e %T", time_tm);
        buginf(TRUE, VQENAME_VQEC, FALSE,
               "Update requested at %s (abort at %s)\n",
               timestring2, timestring1);
     }

    gettimeofday(&vqec_updater.last_update_time, NULL);

    /* Get index file (if necessary) */
    if (update_index || 
        (vqec_updater.last_index_request.result != 
         VQEC_UPDATER_RESULT_ERR_OK)) {
        vqec_updater.last_index_request.result =
            vqec_updater_fetch_resource(
                VQEC_UPDATER_RESOURCE_NAME_INDEX, TRUE,
                MEDIA_TYPE_APP_PLAIN_TEXT, update_abort_time,
                NULL, fetch_index_cb);
        vqec_updater.total_update_attempts++;
        if (vqec_updater_result_is_failure(
                vqec_updater.last_index_request.result)) {
            vqec_updater.total_update_failures++;
        }
        if ((vqec_updater.last_index_request.result != 
             VQEC_UPDATER_RESULT_ERR_OK) &&
            (vqec_updater.last_index_request.result !=
             VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_OLDVERSION)) {

            if (update_attr) {
                vqec_updater.attrcfg.last_update_result = 
                    VQEC_UPDATER_RESULT_ERR_NA;
            }
            if (update_chan) {
                vqec_updater.chancfg.last_update_result = 
                    VQEC_UPDATER_RESULT_ERR_NA;
            }
            goto done;
        }
    }

    /* Retrieve an attribute update (if requested & needed) */
    if (update_attr) {
        if (vqec_updater.last_index_request.result ==
            VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_OLDVERSION) {
            /*
             * Older VCDS did not supply an index file.  As a consequence, 
             * VQE-C will stop using its attribute file (if any)
             * since the VCDS could be an older version which does
             * not supply attribute files.
             */
            if (strncmp(vqec_updater.attrcfg.vqec_version,
                        VQEC_VERSION_FILE_NOT_AVAILABLE,
                        MD5_CHECKSUM_STRLEN)) {
                /* VQE-C version must be deleted */
                vqec_updater.attrcfg.last_update_result =
                    (vqec_updater_update_attrcfg(TRUE) ? 
                     VQEC_UPDATER_RESULT_ERR_OK :
                     VQEC_UPDATER_RESULT_ERR_COMMIT);
                vqec_updater.attrcfg.total_update_attempts++;
                if (vqec_updater_result_is_failure(
                        vqec_updater.attrcfg.last_update_result)) {
                    vqec_updater.attrcfg.total_update_failures++;
                }
            } else {
                vqec_updater.attrcfg.last_update_result =
                    VQEC_UPDATER_RESULT_ERR_NA;
            }
        } else if (!strncmp(vqec_updater.attrcfg.vqec_version,
                            vqec_updater.attrcfg.vcds_version,
                            MD5_CHECKSUM_STRLEN)) { 
            /* VQE-C version matches VCDS version, nothing to do */
            vqec_updater.attrcfg.last_update_result =
                VQEC_UPDATER_RESULT_ERR_UNNECESSARY;
        } else {
            /* VQE-C and VCDS versions differ */
            if (!strncmp(vqec_updater.attrcfg.vcds_version,
                         VQEC_VERSION_FILE_NOT_AVAILABLE,
                         MD5_CHECKSUM_STRLEN)) {
                /* VQE-C version must be deleted */
                vqec_updater.attrcfg.last_update_result =
                    (vqec_updater_update_attrcfg(TRUE) ? 
                     VQEC_UPDATER_RESULT_ERR_OK :
                     VQEC_UPDATER_RESULT_ERR_COMMIT);
            } else {
                /* VQE-C version must be updated */
                vqec_updater.attrcfg.last_update_result =
                    vqec_updater_fetch_resource(
                        VQEC_UPDATER_RESOURCE_NAME_NETCFG, TRUE,
                        MEDIA_TYPE_APP_PLAIN_TEXT, update_abort_time,
                        &vqec_updater.attrcfg.last_response_time,
                        vqec_updater_update_attrcfg);
            }
            vqec_updater.attrcfg.total_update_attempts++;
            if (vqec_updater_result_is_failure(
                    vqec_updater.attrcfg.last_update_result)) {
                vqec_updater.attrcfg.total_update_failures++;
            }
        }
    }

    /* Retrieve a channel update (if requested & needed) */
    if (update_chan) {
        if (vqec_updater.last_index_request.result ==
            VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_OLDVERSION) {
            /*
             * Older VCDS did not supply an index file.  As a consequence,
             * VQE-C should update the channel lineup without specifying
             * the local C-NAME in the URL to accommodate an older VCDS.
             * Note that this may pull down updates from the VCDS
             * which are identical to the current version.
             */
            vqec_updater.chancfg.last_update_result =
                vqec_updater_fetch_resource(
                    VQEC_UPDATER_RESOURCE_NAME_CHANCFG, FALSE,
                    MEDIA_TYPE_APP_SDP, update_abort_time,
                    &vqec_updater.chancfg.last_response_time,
                    vqec_updater_update_chancfg);
            vqec_updater.chancfg.total_update_attempts++;
            if (vqec_updater_result_is_failure(
                    vqec_updater.chancfg.last_update_result)) {
                vqec_updater.chancfg.total_update_failures++;
            }
        } else if (!strncmp(vqec_updater.chancfg.vqec_version,
                     vqec_updater.chancfg.vcds_version,
                     MD5_CHECKSUM_STRLEN)) {
            /* VQE-C version matches VCDS version, nothing to do */
            vqec_updater.chancfg.last_update_result =
                VQEC_UPDATER_RESULT_ERR_UNNECESSARY;
        } else {
            /* VQE-C and VCDS versions differ */
            if (!strncmp(vqec_updater.chancfg.vcds_version,
                         VQEC_VERSION_FILE_NOT_AVAILABLE,
                         MD5_CHECKSUM_STRLEN)) {
                /* VQE-C version must be deleted */
                vqec_updater.chancfg.last_update_result =
                    (vqec_updater_update_chancfg(TRUE) ? 
                     VQEC_UPDATER_RESULT_ERR_OK :
                     VQEC_UPDATER_RESULT_ERR_COMMIT);
            } else {
                /* VQE-C version must be updated */
                vqec_updater.chancfg.last_update_result =
                    vqec_updater_fetch_resource(
                        VQEC_UPDATER_RESOURCE_NAME_CHANCFG, TRUE,
                        MEDIA_TYPE_APP_SDP, update_abort_time,
                        &vqec_updater.chancfg.last_response_time,
                        vqec_updater_update_chancfg);
            }
            vqec_updater.chancfg.total_update_attempts++;
            if (vqec_updater_result_is_failure(
                    vqec_updater.chancfg.last_update_result)) {
                vqec_updater.chancfg.total_update_failures++;
            }
        }
    }
done:
    return;
}


/* 1 second - chosen in light of the units for update_window being seconds */
#define VQEC_UPDATER_POLL_INTERVAL 1

/*
 * Returns a randomized amount x where 0 <= x < 'window'.
 *
 * @param[in] window - bound for the return value.
 * @param[out] int   - chosen random value
 */
UT_STATIC int
vqec_updater_get_random_time (int window)
{
    struct timeval seed;

    gettimeofday(&seed, NULL);
    srandom(seed.tv_usec);
    return (rand() % window);
}


/*
 * Schedules a time for a pending update request.
 *
 * @param[in] use_update_interval - TRUE means schedule the service out
 *                                   by one polling interval (+ update_window)
 *                                  FALSE means schedule the service out
 *                                   by just the update_window
 */
void
vqec_updater_schedule_request (boolean use_update_interval_max)
{
    vqec_updater.next_index_request.update_window =
        vqec_updater_get_random_time(vqec_syscfg_get_ptr()->update_window);
    vqec_updater.next_index_request.seconds_until_update =
        vqec_updater.next_index_request.update_window +
        (use_update_interval_max ? 
         vqec_syscfg_get_ptr()->update_interval_max : 0);
    VQEC_DEBUG(VQEC_DEBUG_UPDATER,
               "Scheduling next update for %d seconds"
               " (including window size = %d)...\n",
               vqec_updater.next_index_request.seconds_until_update,
               vqec_updater.next_index_request.update_window);
}

/*
 * VQE-C Updater thread
 *
 * Checks for and retrieves configuration resource updates from the VCDS on
 * an as needed/as requested basis.
 *
 * Once created, this thread is the only one that will initiate requests 
 * to the VCDS using the RTSP client APIs.  This ensures that all RTSP 
 * requests are serialized (a new request is not sent until any previous 
 * response has been received and processed, or timed out) so as to avoid
 * interleaved uses of the RTSP client's embedded internal state (e.g. 
 * RTSP response buffer), which is not protected by the VQE-C global lock.
 *
 * This thread does not require the VQE-C global lock during the majority
 * of the time that it runs.  Only upon successfully retrieving an update,
 * must the global lock be held to process (commit) the update.
 *
 * @param[in]  arg    - not used
 * @param[out] void * - not used
 */
UT_STATIC void *
vqec_updater_update_thread (void *arg)
{
    /*
     * If polling is enabled, compute the time until the first index request.
     */
    if (vqec_updater_polling_enabled()) {
        vqec_updater_schedule_request(TRUE);
    }

    /*
     * Run the updater, waking periodically to see if we need to do anything.
     * vqec_updater.seconds_until_update keeps track of time until an update,
     * and is checked upon thread awakening:
     *   -1  means no update is pending
     *        (e.g. polled updates scheduled or unserviced triggered updates)
     *   >0  means an update is pending
     *   0   not used upon awakening
     */
    while (1) {
        sleep(VQEC_UPDATER_POLL_INTERVAL);

        /* Terminate if requested */
        if (vqec_updater.status == VQEC_UPDATER_STATUS_DYING) {
            vqec_updater.status = VQEC_UPDATER_STATUS_UNINITIALIZED;
            break;
        }

        /* Schedule any explicit update requests made while we were asleep */
        if (vqec_updater_update_request_issued) {
            vqec_updater_update_request_issued = FALSE;
            if (!vqec_updater_get_in_progress()) {
                vqec_updater_schedule_request(FALSE);
            }
        }

        /* If no update is pending, then go back to sleep */
        if (vqec_updater.next_index_request.seconds_until_update == -1) {
            continue;
        }

        /* An update is pending:  subtract the elapsed time since last check */
        vqec_updater.next_index_request.seconds_until_update -=
            VQEC_UPDATER_POLL_INTERVAL;
        if (vqec_updater.next_index_request.seconds_until_update < 0) {
            vqec_updater.next_index_request.seconds_until_update = 0;
        }
        

        /* If time for next index request hasn't arrived, go back to sleep */
        if (vqec_updater.next_index_request.seconds_until_update > 0) {
            continue;
        }

        /* Time for an update, go initiate one */
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "Update time arrived, issuing request...\n");
        vqec_updater_request_update(
            TRUE, vqec_updater.attrcfg.datafile[0] != '\0', TRUE,
            TIME_ADD_A_R(get_sys_time(), VQEC_UPDATER_TIMEOUT_BACKGROUND));

        /* If polling is enabled, schedule the next update request */
        if (vqec_updater_polling_enabled()) {
            vqec_updater_schedule_request(TRUE);
        } else {
            vqec_updater.next_index_request.seconds_until_update = -1;
        }
    }

    return (NULL);
}

/*
 * Starts the updater background service.
 */
vqec_error_t vqec_updater_start (void)
{
    int err;

    /*
     * Use vqec_pthread_create() so that EXPLICIT_SCHED is always
     * used - if EXPLICIT_SCHED is not used, the default policy for
     * NPTL library is INHERIT_SCHED and therefore the thread will
     * be created with real-time priority if the parent has real-time prio.
     * Thread attributes are set appropriately in vqec_pthread_create with
     * priority set to SCHED_OTHER.
     */
    if ((err = vqec_pthread_create(&vqec_updater_thread_id,
                                   vqec_updater_update_thread,
                                   NULL))) {
        VQEC_LOG_ERROR("update thread failed to start (%s)\n", strerror(err));
        return VQEC_ERR_CREATETHREAD;
    }
    
    vqec_updater.status = VQEC_UPDATER_STATUS_RUNNING;
    return (VQEC_OK);
}

/*
 * Stops the updater service
 */
void
vqec_updater_stop (void)
{
    if (vqec_updater.status != VQEC_UPDATER_STATUS_RUNNING) {
        return;
    }

    /*
     * Set the updater thread to terminate, but don't wait for it to die.
     */
    (void)pthread_detach(vqec_updater_thread_id);
    vqec_updater.status = VQEC_UPDATER_STATUS_DYING;
}


/*
 * Callback for handling notifications that a configuration previously
 * supplied by the updater has become corrupted.
 *
 * Such events are handled by the updater
 *  1) clearing its record of affected file version, and
 *  2) requesting an update from the VCDS
 *
 * @params[in]  params  - event information
 */
static void
vqec_updater_event_cb (vqec_ifclient_config_event_params_t *params)
{
    char *vqec_version;

    if (params->event != VQEC_IFCLIENT_CONFIG_EVENT_CONFIG_INVALID) {
        goto done;
    }

    /* Clear the updater's record of configuration version in use by VQE-C */
    switch (params->config) {
    case VQEC_IFCLIENT_CONFIG_NETWORK:
        vqec_version = vqec_updater.attrcfg.vqec_version;
        break;
    case VQEC_IFCLIENT_CONFIG_CHANNEL:
        vqec_version = vqec_updater.chancfg.vqec_version;
        break;
    default:
        goto done;
    }
    snprintf(vqec_version, MD5_CHECKSUM_STRLEN, VQEC_VERSION_FILE_CORRUPTED);

    /* Request an update with the current version from the VCDS */
    vqec_updater_update(TRUE);

done:
    return;
}

/*
 * Displays information about the updater.
 */
void 
vqec_updater_show()
{
    char *timestr, timestr2[TIME_STRING_BUFF_SIZE];
#define FIELD_STRLEN 12
    char tmp[FIELD_STRLEN];
    
    CONSOLE_PRINTF("\nUpdater state:                                %s",
                   vqec_updater_status2str(vqec_updater.status));
    CONSOLE_PRINTF(" identity:                                    %s",
                   vqec_updater.cname);
    CONSOLE_PRINTF(" update window:                               %d",
                   vqec_syscfg_get_ptr()->update_window);
    CONSOLE_PRINTF(" polling:                                     %s",
                   vqec_updater_polling_enabled() ? "enabled" : "disabled");
    if (vqec_updater_polling_enabled()) {
        snprintf(tmp, FIELD_STRLEN, "%d",
                 vqec_syscfg_get_ptr()->update_interval_max);
    } else {
        snprintf(tmp, FIELD_STRLEN, "n/a");
    }
    CONSOLE_PRINTF(" poll interval (s):                           %s", tmp);
    if (vqec_updater.next_index_request.seconds_until_update > 0) {
        timestr =
            vqec_updater_timeval2str(
                abs_time_to_timeval(
                    TIME_ADD_A_R(get_sys_time(),   
                      TIME_MK_R(sec, 
                         vqec_updater.next_index_request.seconds_until_update)
                        )));
    } else {
        timestr = "<none scheduled>";
    }
    CONSOLE_PRINTF("Next update request:                          %s",
                   timestr);
    timestr = vqec_updater_timeval2str(vqec_updater.last_update_time);
    CONSOLE_PRINTF("Last update request:                          %s",
                   timestr ? timestr : "<unknown>");
    CONSOLE_PRINTF(" Servers attempted/eligible:                  %d/%d",
                   vqec_updater.last_index_request.vcds_servers_attempted,
                   vqec_updater.last_index_request.vcds_servers_eligible);
    switch (vqec_updater.last_index_request.result) {
    case VQEC_UPDATER_RESULT_ERR_OK:
    case VQEC_UPDATER_RESULT_ERR_VCDS_COMM:
    case VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_OLDVERSION:
    case VQEC_UPDATER_RESULT_ERR_VCDS_REPLY_UNEXPECTED:
    case VQEC_UPDATER_RESULT_ERR_COMMIT:
        CONSOLE_PRINTF(" VCDS selected for request:                   %s:%d",
                       vqec_updater.last_index_request.vcds_addr,
                       vqec_updater.last_index_request.vcds_port);
        if (vqec_updater.last_index_request.result !=
            VQEC_UPDATER_RESULT_ERR_VCDS_COMM) {
            CONSOLE_PRINTF(" VCDS version:                                %s",
                           vqec_updater.last_index_request.server_version);
        }
        break;
    default:
        CONSOLE_PRINTF(
            " VCDS selected for request:                   <unknown>");
        break;
    }
    CONSOLE_PRINTF(" Index file retrieval:                        %s",
                   vqec_updater_result2str(
                       vqec_updater.last_index_request.result));
    CONSOLE_PRINTF(" Network Config update result:                %s",
                   vqec_updater_result2str(
                       vqec_updater.attrcfg.last_update_result));
    CONSOLE_PRINTF(" Channel Config update result:                %s",
                   vqec_updater_result2str(
                       vqec_updater.chancfg.last_update_result));

    CONSOLE_PRINTF("Last successful update transfer times:");
    CONSOLE_PRINTF(" Network Config update response time (s):     %s",
                   rel_time_to_usec(vqec_updater.attrcfg.last_response_time) ?
                   rel_time_to_str(
                       vqec_updater.attrcfg.last_response_time,
                       timestr2, TIME_STRING_BUFF_SIZE) : "n/a");
    CONSOLE_PRINTF(" Channel Config update response time (s):     %s",
                   rel_time_to_usec(vqec_updater.chancfg.last_response_time) ?
                   rel_time_to_str(
                       vqec_updater.chancfg.last_response_time,
                       timestr2, TIME_STRING_BUFF_SIZE) : "n/a");

    CONSOLE_PRINTF("Updater counters:"); 
    CONSOLE_PRINTF(" Index retrieval attempts (failures):         %d (%d)",
                   vqec_updater.total_update_attempts,
                   vqec_updater.total_update_failures);
    CONSOLE_PRINTF(" Network Config update attempts (failures):   %d (%d)",
                   vqec_updater.attrcfg.total_update_attempts,
                   vqec_updater.attrcfg.total_update_failures);
    CONSOLE_PRINTF(" Channel Config update attempts (failures):   %d (%d)",
                   vqec_updater.chancfg.total_update_attempts,
                   vqec_updater.chancfg.total_update_failures);
}


/*
 * Deinitializes the VQE-C update manager,
 * including termination of the updater service thread if running. 
 */
void
vqec_updater_deinit (void)
{
    switch (vqec_updater.status) {
    case VQEC_UPDATER_STATUS_UNINITIALIZED:
    case VQEC_UPDATER_STATUS_DYING:
        return;
    case VQEC_UPDATER_STATUS_INITIALIZED:
        vqec_updater.status = VQEC_UPDATER_STATUS_UNINITIALIZED;
        break;
    case VQEC_UPDATER_STATUS_RUNNING:
        vqec_updater_stop();
        break;
    }
}

/*
 * Initializes the VQE-C update manager.
 *
 * @param[in]  syscfg        - system configuration file used for configuring
 *                             the updater
 * @param[out] vqec_error_t  - VQEC_OK if the updater was initialized,
 *                             VQEC_ERR_ALREADYINIITIALIZED if the updater
 *                              was already initialized or running, or 
 *                             an error code if initialization failed
 */
vqec_error_t
vqec_updater_init (vqec_syscfg_t *syscfg)
{
    vqec_error_t err = VQEC_OK;
    int delay = 0, res_err;
    char *vqec_version;
    vqec_ifclient_config_status_params_t params_status;
    vqec_ifclient_config_register_params_t params_register;

    /* If already initialized, don't re-initialize */
    if ((vqec_updater.status == VQEC_UPDATER_STATUS_INITIALIZED) ||
        (vqec_updater.status == VQEC_UPDATER_STATUS_RUNNING)) {
        err = VQEC_ERR_ALREADYINITIALIZED;
        goto done;
    }

    while (vqec_updater.status == VQEC_UPDATER_STATUS_DYING) {
        /*
         * The updater's state is still in use by a previous thread.
         * Give that thread at most 5 seconds to die, or else report
         * initialization failure of the updater.
         */
        sleep(1);
        if (++delay == 5) {
            err = VQEC_ERR_DESTROYTHREAD;
            goto done;
        }
    }

    memset(&vqec_updater, 0, sizeof(vqec_updater));

    /*
     * Top-level updater field initialization
     */
    if (syscfg->domain_name_override[0] != '\0') {
        /* A domain name has been provided for SRV lookups--use it */
        strncpy(vqec_updater.domain_name, syscfg->domain_name_override,
                MAX_DOMAIN_NAME_LENGTH);
    } else {
        /*
         * resolv.conf will be searched for a domain name for SRV lookups,
         * so initialize its APIs for future queries
         */
        res_err = res_init();
        if (!res_err) {
            /*
             * We will set RES_DNSRCH to the option so that old version
             * of res_search() will append the search domain name(s)
             *  before sending out the res_query().
             */
            _res.options |= RES_DNSRCH;
        } else {
            /*
             * Fatal error--can't look up the domain name for SRV queries
             */
            err = VQEC_ERR_UPDATE_INIT_FAILURE;
            goto done;
        }
    }
    vqec_updater.vcds_addr = syscfg->vcds_server_ip;
    vqec_updater.vcds_port = ntohs(syscfg->vcds_server_port);
    strncpy(vqec_updater.cname, vqec_get_cname(), VQEC_MAX_CNAME_LEN);
    vqec_updater.next_index_request.seconds_until_update = -1;
    vqec_updater.last_index_request.result = VQEC_UPDATER_RESULT_ERR_NA;

    /*
     * System attribute updater fields
     */
    strncpy(vqec_updater.attrcfg.datafile, syscfg->network_cfg_pathname,
            MAX_DB_NAME_LENGTH);
    vqec_updater.attrcfg.last_update_result = VQEC_UPDATER_RESULT_ERR_NA;
    params_status.config = VQEC_IFCLIENT_CONFIG_NETWORK;
    err = vqec_ifclient_config_status_ul(&params_status);
    if (err != VQEC_OK) {
        goto done;
    }
    if (params_status.status == VQEC_IFCLIENT_CONFIG_STATUS_INVALID) {
        vqec_version = VQEC_VERSION_FILE_CORRUPTED;
    } else {
        vqec_version = params_status.md5;
    }    
    snprintf(vqec_updater.attrcfg.vqec_version,
             MD5_CHECKSUM_STRLEN, vqec_version);
    snprintf(vqec_updater.attrcfg.vcds_version, MD5_CHECKSUM_STRLEN,
             VQEC_VERSION_FILE_NOT_AVAILABLE);
    
    /*
     * Channel config updater fields
     */
    strncpy(vqec_updater.chancfg.datafile, syscfg->channel_lineup,
            MAX_DB_NAME_LENGTH);
    vqec_updater.chancfg.last_update_result = VQEC_UPDATER_RESULT_ERR_NA;
    params_status.config = VQEC_IFCLIENT_CONFIG_CHANNEL;
    err = vqec_ifclient_config_status_ul(&params_status);
    if (err != VQEC_OK) {
        goto done;
    }
    if (params_status.status == VQEC_IFCLIENT_CONFIG_STATUS_INVALID) {
        vqec_version = VQEC_VERSION_FILE_CORRUPTED;
    } else {
        vqec_version = params_status.md5;
    }    
    snprintf(vqec_updater.chancfg.vqec_version,
             MD5_CHECKSUM_STRLEN, vqec_version);
    snprintf(vqec_updater.chancfg.vcds_version, MD5_CHECKSUM_STRLEN,
             VQEC_VERSION_FILE_NOT_AVAILABLE);
    
    /*
     * Register for callbacks from syscfg which indicate that 
     * a previously-retrieved config file has been corrupted.
     */
    params_register.config = VQEC_IFCLIENT_CONFIG_NETWORK;
    params_register.event_cb = vqec_updater_event_cb;
    err = vqec_ifclient_config_register_ul(&params_register);
    if ((err != VQEC_OK) && (err != VQEC_ERR_ALREADYREGISTERED)) {
        goto done;
    }
    params_register.config = VQEC_IFCLIENT_CONFIG_CHANNEL;
    err = vqec_ifclient_config_register_ul(&params_register);
    if ((err != VQEC_OK) && (err != VQEC_ERR_ALREADYREGISTERED)) {
        goto done;
    }

    vqec_updater.status = VQEC_UPDATER_STATUS_INITIALIZED;
    err = VQEC_OK;

done:
    return (err);
}

