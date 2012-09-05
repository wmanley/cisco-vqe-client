/*------------------------------------------------------------------
 * VQE-C Updater API.
 *
 *
 * Copyright (c) 2007-2009 by cisco Systems, Inc.
 * All rights reserved.
 *----------------------------------------------------------------*/

#ifndef VQEC_UPDATER_H
#define VQEC_UPDATER_H

#include <stdint.h>
#include <vam_types.h>
#include "vqec_ifclient_defs.h"
#include "vqec_error.h"
#include "vqec_syscfg.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Status codes for the VQE-C updater */
typedef enum vqec_updater_status_ {
    VQEC_UPDATER_STATUS_UNINITIALIZED = 0,
    VQEC_UPDATER_STATUS_INITIALIZED,
    VQEC_UPDATER_STATUS_RUNNING,
    VQEC_UPDATER_STATUS_DYING
} vqec_updater_status_t;

/*
 * Returns TRUE if CDI is enabled, FALSE otherwise.
 *
 * @param[out] boolean  - indicates whether CDI is enabled or not
 *
 */
boolean
vqec_updater_get_cdi_enable(void);

/*
 * Converts the supplied time value to a date format.
 * Callers should use the returned value immediately, as subsequent calls
 * to this function will overwrite the contents of the returned string.
 *
 * @param[in] last_update_tv  - time value to be described
 * @param[out] char *         - date-format string for timeval
 */ 
char *
vqec_updater_timeval2str(struct timeval last_update_tv);

/*----------------------------------------------------------------------------
 * Triggers an update to be handled by the updater service thread.
 * The update will occur after waiting for a random portion of the
 * configured "update_window".
 *
 * @param[in] force_update     - If TRUE, force an update regardless of
 *                               whether polled updates are configured.
 *                               If FALSE, only perform an update if
 *                               polled updates are not already configured.
 *--------------------------------------------------------------------------*/
void vqec_updater_update(boolean force_update);

/*
 * Define timeout periods for receiving VCDS configuration updates.
 *
 * A small timeout is defined for use during init to avoid delaying
 * STB initialization,
 * A higher timeout is defined for background updates.
 */
#define VQEC_UPDATER_TIMEOUT_INIT        SECS(30)
#define VQEC_UPDATER_TIMEOUT_BACKGROUND  SECS(600)

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
vqec_updater_request_update(boolean update_index,
                            boolean update_attr,
                            boolean update_chan,
                            abs_time_t update_abort_time);

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
 * @param[out] attrcfg_last_update_time
 *                          - time of last successful configuration update
 *                            of an attribute config file
 * @param[out] attrcfg_version
 *                          - current version string of attribute config file
 * @param[out] chancfg_last_update_time
 *                          - time of last successful configuration update
 *                            of a channel config file
 * @param[out] chancfg_version
 *                          - current version string of channel config file
 */
vqec_error_t
vqec_updater_get_status(vqec_updater_status_t *status,
                        boolean *in_progress,
                        int32_t *seconds_until_next_update,
                        struct timeval *attrcfg_last_update_time,
                        char **attrcfg_version,
                        struct timeval *chancfg_last_update_time,
                        char **chancfg_version);

/*
 * Clears the updater counters.
 */
void
vqec_updater_clear_counters(void);

/*----------------------------------------------------------------------------
 * Start the VQE-C updater service thread, which will wake once per second
 * to see if there are any configuration updates needed (system configuration
 * or channel configuration).
 *
 * @return Either VQEC_OK or VQEC_ERR_CREATETHREAD, depending on whether or
 * not the VQE-C updater thread was created successfully, respectively.
 *--------------------------------------------------------------------------*/
vqec_error_t vqec_updater_start(void);

/*
 * Initialize the VQE-C updater with its configuration settings.
 * Once initialized, the updater may not be re-initialized.
 *
 * @param[in]  cfg        - pointer to configuration to be used for
 *                          initializing the updater.
 * @param[out] vqec_err_t - VQEC_OK upon success, or a return code indicating
 *                          reason for failure.
 */
vqec_error_t
vqec_updater_init(vqec_syscfg_t *cfg);

/*
 * De-Initializes the VQE-C update manager,
 * including termination of the updater service thread if running. 
 */
void
vqec_updater_deinit(void);

/*
 * Displays information about the updater.
 */
void vqec_updater_show(void);
        
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VQEC_UPDATER_H */
