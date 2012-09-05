/********************************************************************
 * vqes_pm_syslog_def.h
 *
 * This file defines the SYSLOG messages for the VQE-S Process Monitor.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQES_PM_SYSLOG_DEF_H_
#define _VQES_PM_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_VQES_PM, LOG_VQES_PM, LOG_ERR);

/* Critical errors */
syslog_msgdef(LOG_VQES_PM, PM_INIT_START, LOG_CRIT, "Process Monitor "
              "has (re)started");
msgdef_explanation("This message is used to indicate that the VQE-S Health "
                   "Monitoring Application has just been (re)started. This "
                   "is either expected behavior from starting the Process "
                   "Monitor or PM exited unexpectedly and has restarted.");
msgdef_recommended_action("If the VQE-S application has just been manually "
                          "started, or the VQE-S server has just come up, "
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


syslog_msgdef(LOG_VQES_PM, PM_INIT_FAILURE, LOG_CRIT, "Process Monitor "
              "initialization has failed for the specified reason: %s");
msgdef_explanation("Process Monitor initialization has failed. A reason "
                   "code describes the specific error encountered. A common "
                   "cause is insufficient user permissions.");
msgdef_recommended_action("Process Monitor may not be running with proper "
                          "permissions. Refer to the \"Troubleshooting "
                          "VQE Software Components\" chapter of the "
                          SYSLOGDOC_USER_GUIDE " for information on "
                          "correcting permissions and replacing lost files, "
                          "and perform the indicated corrective actions."
                          SYSLOGDOC_RESTART_VQES
                          "If this message recurs, copy "
                          "the error message exactly as it appears in the "
                          "VQE-S system log. Issue the <b>vqereport</b>"
                          " command to gather data that may help identify the "
                          "nature of the error. If you cannot determine the "
                          "nature of the error from the error message text or "
                          "from the <b>vqereport</b> command "
                          "output, contact your Cisco technical support "
                          "representative, and provide the representative "
                          "with the gathered information.");

syslog_msgdef(LOG_VQES_PM, PM_PROC_START_FAILURE, LOG_CRIT, "Could not start "
              "%s ");
msgdef_explanation("Process Monitor could not start the specified process.");
msgdef_recommended_action("This executable file for the specified process may "
                          "be missing or have incorrect permissions. Refer to "
                          "the \"Troubleshooting VQE Software Components\" chapter "
                          "of the " SYSLOGDOC_USER_GUIDE " for information on "
                          "correcting permissions and replacing lost files, "
                          "and perform the indicated corrective actions."
                          SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_PM, PM_PROC_STOP_FAILURE, LOG_CRIT, "Could not stop "
              "%s");
msgdef_explanation("Process Monitor could not stop the specified process. The "
                   "process is being stopped because another process stopped "
                   "or a shutdown was requested.");
msgdef_recommended_action(SYSLOGDOC_REBOOT_SERVER);

syslog_msgdef(LOG_VQES_PM, PM_CFG_PARSE_FAILURE, LOG_CRIT, "Configuration "
              "file syntax error \"%s\" on line %d in file %s");
msgdef_explanation("Process Monitor encountered the specified syntax error on "
                   "the line specified. The VQE-S application could not "
                   "be successfully started.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_PM, PM_CFG_FILE_MISSING, LOG_CRIT, "Cannot open "
              "configuration file %s");
msgdef_explanation("The VQE-S Configuration File is missing or has "
                   "incorrect permissions. The VQE-S application could not"
                   "be successfully started.");
msgdef_recommended_action("Refer to the \"Troubleshooting VQE Software "
                          " Components\" chapter of the " 
                          SYSLOGDOC_USER_GUIDE " for information on "
                          "correcting permissions and replacing lost files, "
                          "and perform the indicated corrective actions."
                          SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_PM, PM_CFG_FAILURE, LOG_CRIT, "Missing or invalid "
              "information in configuration file: %s");
msgdef_explanation("Missing or invalid information detected while parsing the "
                   "configuration file. The error string will provide greater "
                   "detail on the specfic error that occurred. The VQE-S "
                   "application could not be successfully started.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);


syslog_msgdef(LOG_VQES_PM, PM_CRIT_ERROR, LOG_CRIT, "Process Monitor exiting "
              "due to error: %s");
msgdef_explanation("Critical error in Process Monitor. Process Monitor "
                   "has encountered an internal error from which it cannot "
                   "recover.");
msgdef_recommended_action(SYSLOGDOC_REPORT_COPY_CONTACT);

/* Errors */
syslog_msgdef(LOG_VQES_PM, PM_NONCRIT_ERROR, LOG_ERR, "Process Monitor"
              "continuing after internal error: %s");
msgdef_explanation("Process Monitor encountered a potentially "
                   "recoverable error and is continuing to run.");
msgdef_recommended_action(SYSLOGDOC_RECUR_COPY
                          "Issue the <b>vqereport</b> command"
                          "to gather data that may help identify the nature "
                          "of the error. Contact your Cisco technical support "
                          "representative, and provide the representative "
                          "with the gathered information."
                          SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_PM, PM_PROCESS_STOPPED, LOG_ERR, "Process %s has "
              "stopped, attempting to restart it.");
msgdef_explanation("The specified process has exited. Process Monitor will "
                   "attempt to restart. ");
msgdef_recommended_action(SYSLOGDOC_REPORT_COPY_CONTACT);

syslog_msgdef(LOG_VQES_PM, PM_PROCESS_POST_FAIL, LOG_ERR, 
              "Post-processing for process %s did not complete");
msgdef_explanation("The post-processing command for the specified process did "
                   "not complete successfully. This often indicates that"
                   "some feedback target addresses were not "
                   "removed properly from the routing table.");
msgdef_recommended_action("Use the VQE-S Application Monitoring Tool to " 
                          "enable the PM_DEBUG_PROC_ERR and "
                          "PM_DEBUG_PROC_DETL debug flags. After the "
                          "log message recurs, issue the <b>vqereport</b> "
                          "command to gather data that may help identify the "
                          "nature of the error. Contact your Cisco technical "
                          "support representative, and  provide the "
                          "representative with the gathered information."
                          SYSLOGDOC_REBOOT_SERVER);

/*Warning messages */
syslog_msgdef(LOG_VQES_PM, PM_ALREADY_RUNNING, LOG_WARNING, 
              "Process Monitor is already running.");
msgdef_explanation("An attempt was made to start further instances of Process "
                   "Monitor while one is already running.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

/* Info messages */
syslog_msgdef(LOG_VQES_PM, PM_PROCESS_STOPPING, LOG_INFO, "Process Monitor "
              "attempting to stop %s");
msgdef_explanation("Process Monitor is attempting to stop a specified "
                   "process. Process Monitor is either shutting down by "
                   "request or is currently restarting all VQE-S processes "
                   "to maintain system health.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_VQES_PM, PM_RESTART_SUCCESS, LOG_INFO, "Successfully "
              "restarted %s");
msgdef_explanation("The specified process was successfully restarted by "
                   "Process Monitor.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_VQES_PM, PM_EXIT, LOG_INFO, "Process Monitor Exiting");
msgdef_explanation("Process Monitor exiting. This message will be seen "
                   "whenever Process Monitor exits, if a "
                   "shutdown has been requested or if an unrecoverable error "
                   "has occurred.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

#endif /* _VQES_PM_SYSLOG_DEF_H_ */
