/********************************************************************
 * vqe_rtsp_syslog_def.h
 *
 * This file defines the SYSLOG messages for the VQE RTSP Server.
 *
 * Copyright (c) 2007-2012 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQE_RTSP_SYSLOG_DEF_H_
#define _VQE_RTSP_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_RTSP, LOG_RTSP, LOG_ERR);

/* Alert messages */
syslog_msgdef(LOG_RTSP, RTSP_PROCESS_ALERT, LOG_ALERT,
              "VCDS process has failed to start.");
msgdef_explanation("VCDS process has encountered an error and will not be available for use.");
msgdef_recommended_action("Check the reason in the system error message. "
                          "Modify VCDServer.cfg if necessary, and restart "
                          "the VCDServer process using service "
                          "command."
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);


/* Critical messages */
syslog_msgdef(LOG_RTSP, RTSP_CONTENT_MODIFIED_CRIT, LOG_CRIT,
              "File %s is either corrupted or has been modified manually.");
msgdef_explanation("The contents in the file are either corrupted "
                   "or have been modified manually.");
msgdef_recommended_action("Re-send the file with the provisioning tool."
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_RTSP, RTSP_CHECKSUM_CREATE_CRIT, LOG_CRIT,
              "VCDS failed to create MD5 checksum for %s.");
msgdef_explanation("A software error has occurred during checksum creation.");
msgdef_recommended_action("Re-send the file with the provisioning tool."
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);



/* Error messages */
syslog_msgdef(LOG_RTSP, RTSP_INIT_ERR, LOG_ERR,
              "VCDS initialization has failed due to %s");
msgdef_explanation("VCDS initialization has encountered an error and will not be available for use.");
msgdef_recommended_action("Check the reason in the system error message. "
                          "Modify VCDServer.cfg if necessary, and restart "
                          "the VCDServer process."
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_RTSP, RTSP_MALLOC_ERR, LOG_ERR,
              "VCDS failed to malloc necessary memory for internal use.");
msgdef_explanation("A software error has occurred during the memory allocation and VCDS will not be available for use.");
msgdef_recommended_action("Restart the VCDServer process."
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_RTSP, RTSP_CFG_FILE_PARSE_ERR, LOG_ERR,
              "Configuration syntax error \"%s\" on line %d in file %s");
msgdef_explanation("VCDS encountered the specified syntax error on "
                   "the line specified. The VCDS application could not "
                   "be successfully started.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_RTSP, RTSP_CFG_FAILURE, LOG_ERR,
              "Missing or invalid information in configuration file: %s");
msgdef_explanation("Missing or invalid information detected while parsing the "
                   "configuration file. The error string will provide greater "
                   "detail on the specfic error that occurred. The VCDS "
                   "application could not be successfully started.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_RTSP, RTSP_FILE_WRITE_ERR, LOG_ERR,
              "The configuration file %s is not writable.");
msgdef_explanation("The configuration file for VCDS does not have write permission.");
msgdef_recommended_action("Check the permissions of the configuration file and make any necessary corrections.");

syslog_msgdef(LOG_RTSP, RTSP_FILE_READ_ERR, LOG_ERR,
              "The configuration file %s is not readable.");
msgdef_explanation("The configuration file for VCDS does not have read permission.");
msgdef_recommended_action("Check the location or permissions of the configuration file and make any necessary corrections.");

syslog_msgdef(LOG_RTSP, RTSP_FILE_PARSE_ERR, LOG_ERR,
              "The configuration file %s failed to parse correctly.");
msgdef_explanation("The configuration file for VCDS failed to pass parsing and validation.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_RTSP, RTSP_PROCESS_ERR, LOG_ERR,
              "VCDS has failed to process RTSP requests due to %s");
msgdef_explanation("VCDS has encountered an error and will not be available for use.");
msgdef_recommended_action("Check the reason in the system error message. "
                          "Modify VCDServer.cfg if necessary, and restart "
                          "the VCDServer process."
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_RTSP, RTSP_CFG_UPDATE_ERR, LOG_ERR,
              "Failed to update the configuration due to %s");
msgdef_explanation("A software error has occurred during the update of "
                   "configuration and the configuration has not been "
                   "updated.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_RTSP, RTSP_EXCEED_MAX_ERR, LOG_ERR,
              "The group IDs in the group mapping file has exceeded the maximum number [%d] allowed.");
msgdef_explanation("The group IDs in the group mapping file has exceeded the maximum number allowed.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);


/* Warning messages */
syslog_msgdef(LOG_RTSP, RTSP_ILLEGAL_GROUP_ID_WARN, LOG_WARNING,
              "Group ID %s in %s is an illegal value. It will not be used by VCDS.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_RTSP, RTSP_ILLEGAL_NETCFG_WARN, LOG_WARNING,
              "[%s] in vqec network config has illegal value or syntax. It will not be used by VCDS.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_RTSP, RTSP_CHECKSUM_MISSING_WARN, LOG_WARNING,
              "VCDS could not find MD5 checksum for %s.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_RTSP, RTSP_FILE_MISSING_WARN, LOG_WARNING,
              "File %s is missing.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_RTSP, RTSP_REQUEST_TOO_BIG_WARN, LOG_WARNING,
              "VCDS has received a request exceeding the buffer length of "
              "%d bytes.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_RTSP, RTSP_ILLEGAL_REQUEST_WARN, LOG_WARNING,
              "VCDS has received an illegal request.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_RTSP, RTSP_CONTENT_NOT_FOUND_WARN, LOG_WARNING,
              "VCDS does not have corresponding content for the request.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_RTSP, RTSP_SERVICE_NOT_AVAILABLE_WARN, LOG_WARNING,
              "VCDS failed to load all the contents and service is not "
              "available.");
msgdef_explanation("Check the syslog and resend the content file(s) to VCDS.");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

/* For now we will keep the following two messages at WARNING level.
   However, if the defult log setting is changed to NOTICE, we should
   change them to NOTICE level. */
syslog_msgdef(LOG_RTSP, RTSP_START, LOG_WARNING,
              "VCDS process has (re)started");
msgdef_explanation("This message is used to indicate that the VCDS "
                   "Application has just been (re)started. This "
                   "is either expected behavior from starting the VCDS "
                   "service or VCDS exited unexpectedly and has restarted.");
msgdef_recommended_action("If the VDS application has just been manually "
                          "started, or the VCDS server has just come up, "
                          "no action is required. Otherwise, copy "
                          "the error message exactly as it appears in the "
                          "VQE-S system log. Issue the <b>vqereport</b>"
                          "command to gather data that may help identify the "
                          "nature of the error. If you cannot determine the "
                          "nature of the error from the error message text or "
                          "from the <b>vqereport</b> command "
                          "output, contact your Cisco technical support "
                          "representative, and provide the representative "
                          "with the gathered information.");

syslog_msgdef(LOG_RTSP, RTSP_EXIT, LOG_WARNING,
              "VCDS has received a SIGTERM signal and is exiting now.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_RTSP, RTSP_IN_SERVICE, LOG_WARNING,
              "VCDS is in full service now.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);


/* Notice messages */
syslog_msgdef(LOG_RTSP, RTSP_NAME_PORT_NOTICE, LOG_NOTICE,
              "VCDS is running on server %s and using port %d.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_RTSP, RTSP_CFG_READ_NOTICE, LOG_NOTICE,
              "VCDS reads in %s.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_RTSP, RTSP_NO_DATA_NOTICE, LOG_NOTICE,
              "No data exists in %s.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_RTSP, RTSP_CHANNEL_UPDATE_NOTICE, LOG_NOTICE,
              "VCDS receives a channel lineup update ...");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_RTSP, RTSP_VQEC_ATTR_UPDATE_NOTICE, LOG_NOTICE,
              "VCDS receives a vqec attribute update ...");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_RTSP, RTSP_VQEC_MAP_UPDATE_NOTICE, LOG_NOTICE,
              "VCDS receives a vqec group mapping %s update ...");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_RTSP, RTSP_REACH_MAX_CONNECT_NOTICE, LOG_NOTICE,
              "VCDS has reached the maximum number of client connections "
              "[%d] allowed.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_RTSP, RTSP_TURN_ON_LISTENER_NOTICE, LOG_NOTICE,
              "VCDS has room to receive new client connections.");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_RTSP, RTSP_PROCESS_NOTICE, LOG_NOTICE,
              "VCDS has lost the connection to one of the clients %s");
msgdef_explanation("This is an informational message.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

#endif /* _VQE_RTSP_SYSLOG_DEF_H_ */
