/********************************************************************
 * vqe_cfg_syslog_def.h
 *
 * This file defines the SYSLOG messages for the VQE CFG component.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQE_CFG_SYSLOG_DEF_H_
#define _VQE_CFG_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_LIBCFG, LOG_LIBCFG, LOG_ERR);

/* Emergencies */

/* Alerts */

/* Critical Errors */
syslog_msgdef(LOG_LIBCFG, CFG_CREATE_HASH_TABLE_CRIT, LOG_CRIT,
              "Failed to create the hash table for session keys.");
msgdef_explanation("A software error involving creation of hash table has occurred.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_LIBCFG, CFG_FILE_OPEN_CRIT, LOG_CRIT,
              "Failed to open channel configuration file %s.");
msgdef_explanation("The channel configuration file either does not exist or has no read permission.");
msgdef_recommended_action("Check the location or the permissions of the channel configuration file and make any necessary corrections.");

syslog_msgdef(LOG_LIBCFG, CFG_FILE_READ_CRIT, LOG_CRIT,
              "Failed to process channel configuration file %s.");
msgdef_explanation("A software error has occurred during the processing of the channel configuration file.");
msgdef_recommended_action("Check the contents of the channel configuration file and make any necessary corrections.");


/* Major Errors */
syslog_msgdef(LOG_LIBCFG, CFG_FILE_PROCESS_ERR, LOG_ERR,
              "Channel process failed due to %s");
msgdef_explanation("Some channel configuration data failed validation checking.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_FILE_CLOSE_ERR, LOG_ERR,
              "Failed to close the channel configuration file %s.");
msgdef_explanation("A software error has occurred during the close of the channel configuration file.");
msgdef_recommended_action(SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_LIBCFG, CFG_FILE_WRITE_ERR, LOG_ERR,
              "Failed to write the channel configuration file %s.");
msgdef_explanation("A software error has occurred during the writing of the channel configuration file.");
msgdef_recommended_action("Check the permissions of the channel configuration file and make any necessary corrections.");

syslog_msgdef(LOG_LIBCFG, CFG_DESTROY_ALL_ERR, LOG_ERR,
              "Failed to destroy all the channel configuration data.");
msgdef_explanation("A software error has occurred during the cleanup of the channel configuration data structure.");
msgdef_recommended_action(SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_LIBCFG, CFG_SAVE_ERR, LOG_ERR,
              "The channel configuration file %s does not have write permission.");
msgdef_explanation("The channel configuration file has insufficient file permissions.");
msgdef_recommended_action("Check the permissions of the channel configuration file and make any necessary corrections.");

syslog_msgdef(LOG_LIBCFG, CFG_MALLOC_ERR, LOG_ERR,
              "The channel configuration module failed to allocate memory.");
msgdef_explanation("A software error has occurred during the allocation of memory.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_LIBCFG, CFG_CREATE_KEY_ERR, LOG_ERR,
              "The channel configuration module failed to create or add the key %s to the hash table.");
msgdef_explanation("A software error has occurred during the key creation.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_LIBCFG, CFG_DELETE_KEY_ERR, LOG_ERR,
              "The channel configuration module failed to delete the key %s from the hash table.");
msgdef_explanation("A software error has occurred during the key deletion.");
msgdef_recommended_action(SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_LIBCFG, CFG_VALIDATION_ERR, LOG_ERR,
              "Channel %s failed validation due to %s");
msgdef_explanation("Channel configuration data failed semantic validation.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_UPDATE_ERR, LOG_ERR,
              "Channel update failed due to %s");
msgdef_explanation("Some channel configuration data failed validation checking.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_ADD_ERR, LOG_ERR,
              "Channel %s failed to add due to the same channel already being in the system.");
msgdef_explanation("Channel configuration uses the same identifier as another channel.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);



/* Warnings */
syslog_msgdef(LOG_LIBCFG, CFG_NO_FILE_WARN, LOG_WARNING,
              "The channel configuration file %s does not exist.");
msgdef_explanation("The channel configuration file cannot be found.");
msgdef_recommended_action("Check the location of the channel configuration file and make any necessary corrections.");

syslog_msgdef(LOG_LIBCFG, CFG_NO_SESSION_DIVIDER_WARN, LOG_WARNING,
              "The channel configuration [%s] does not contain a session divider");
msgdef_explanation("Each channel configuration data must be separated by a session divider in the channel configuration file. Otherwise, a channel configuration might be skipped during data parsing.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_EXCEED_BUFFER_SIZE_WARN, LOG_WARNING,
              "The size of channel configuration [%s] exceeds the maximum size %d allocated");
msgdef_explanation("The size of each channel configuration data must be less than the maximum size allocated. Otherwise, a channel configuration might be skipped during data parsing.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_SDP_PARSE_WARN, LOG_WARNING,
              "Failed to parse SDP content due to %s");
msgdef_explanation("A parsing error has occurred during the parse of a channel configuration data from SDP syntax.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_ILLEGAL_SDP_SYNTAX_WARN, LOG_WARNING,
              "Illegal SDP syntax: %s");
msgdef_explanation("The specified SDP description has illegal syntax.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_REMOVE_FROM_MAP_WARN, LOG_WARNING,
              "Failed to remove channel %s from the channel map.");
msgdef_explanation("A software error has occurred during the remove of a channel from the channel map.");
msgdef_recommended_action(SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_LIBCFG, CFG_ADD_TO_MAP_WARN, LOG_WARNING,
              "Failed to add channel %s to the channel map.");
msgdef_explanation("A software error has occurred during the addition of a channel to the channel map.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_DELETE_WARN, LOG_WARNING,
              "Failed to delete a channel from the channel manager.");
msgdef_explanation("A software error has occurred during the deletion of a channel from the channel manager.");
msgdef_recommended_action(SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_LIBCFG, CFG_ADD_WARN, LOG_WARNING,
              "Failed to add channel %s to the channel manager.");
msgdef_explanation("A software error has occurred during the addition of a channel to the channel manager.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_REACH_MAX_WARN, LOG_WARNING,
              "The channel configuration data has exceeded the maximum allowable size [%d channels].");
msgdef_explanation("The channel configuration data has exceeded the indicated maximum size.");
msgdef_recommended_action("Reduce the number of channels provisioned.");

syslog_msgdef(LOG_LIBCFG, CFG_DUPLICATED_FBT_ADDR_WARN, LOG_WARNING,
              "Channel %s has the same feedback target address or port as other channels.");
msgdef_explanation("The same feedback target address or port is used by more than one channel in the channel configuration file.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_DUPLICATED_SRC_ADDR_WARN, LOG_WARNING,
              "Channel %s has the same original source address or port as other channels.");
msgdef_explanation("The same source address or port is used by more than one channel in the channel configuration file.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_DUPLICATED_RTX_ADDR_WARN, LOG_WARNING,
              "Channel %s has the same retransmission address or port as other channels.");
msgdef_explanation("The same retransmission address or port is used by more than one channel in the channel configuration file.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_LIBCFG, CFG_VALIDATION_WARN, LOG_WARNING,
              "Channel %s has %s");
msgdef_explanation("Channel configuration data has unsupported data.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_LIBCFG, CFG_OLD_VERSION_WARN, LOG_WARNING,
              "Configuration module has received an older version of "
              "a configuration for channel %s than it currently has. "
              "The older version will be ignored.");
msgdef_explanation("Channel configuration data receiving is older than the one in the system.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);


/* Notices */

/* Info */
syslog_msgdef(LOG_LIBCFG, CFG_INIT_COMPLETED_INFO, LOG_INFO,
              "Configuration module is successfully initialized.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_LIBCFG, CFG_CLOSE_COMPLETED_INFO, LOG_INFO,
              "Configuration module is successfully shutdown.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_LIBCFG, CFG_UPDATE_COMPLETED_INFO, LOG_INFO,
              "Configuration module is successfully updated.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

#endif /* _VQE_CFG_SYSLOG_DEF_H_ */
