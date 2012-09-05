/********************************************************************
 * vqes_dp_syslog_def.h
 *
 * This file defines the SYSLOG messages for the VQE-S Data Plance.
 *
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQES_DP_SYSLOG_DEF_H_
#define _VQES_DP_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_VQES_DP, LOG_VQES_DP, LOG_ERR);

#define DP_STARTUP_FAILURE_ACTION                                       \
"Check the VQE-S configuration for any errors." \
SYSLOGDOC_CHG_CONF                                                 \
"If changes to the system configuration have been made which "          \
"are not indicated as part of the standard VQE-S installation and " \
"configuration process, remove these configuration changes, then restart the VQE-S application.  " \
"If this message recurs after correcting any non-standard system configuration, reboot the VQE-S server. " \
SYSLOGDOC_REPORT_COPY_CONTACT

syslog_msgdef(LOG_VQES_DP, DP_INIT_FAILURE, LOG_CRIT, 
"Data Plane exiting due to an initialization failure (reason: %s).");
msgdef_explanation(
"The VQE-S Data Plane initialization has failed.  The Data Plane process will exit "
"and the VQE-S Process Monitor will attempt to restart it.  However, if subsequent "
"initialization attempts also fail, then the VQE-S application may be left in a non-operational "
"state.  The reason code indicated within the message may provide a more specific "
"indication of the initialization step that has failed.  In most cases, the failure "
"will be due to a problem resulting from a non-standard installation or configuration "
"of the VQE-S application.");
msgdef_recommended_action(DP_STARTUP_FAILURE_ACTION);

syslog_msgdef(LOG_VQES_DP, DP_BAD_OPTION_VALUE, LOG_WARNING,
"The value for option \"%s\" provided to the Data Plane process is out of range and has been ignored."
"  The valid range is [%d-%d].");
msgdef_explanation(
"One of the option values that has been passed to the Data Plane process at startup time is out of "
"range.  The Data Plane process will continue normal operation, though the illegal "
"option value that was passed will be ignored and the default value will be used instead. "
"The error message gives both the option whose value was out of range and the legal range "
"of values for the option.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_DP, DP_NO_OPTION_VALUE, LOG_WARNING,
"No value for option \"%s\" was provided to the Data Plane process."
"  The valid range is [%d-%d].");
msgdef_explanation(
"One of the options that has been passed to the Data Plane process at startup time did not have "
"a value specified. The Data Plane process will continue normal operation, though the default value "
"for the option will be used. "
"The error message gives both the option whose value was not specified and the legal range "
"of values for the option.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_DP, DP_BAD_OPTION, LOG_WARNING,
"The following option provided to the Data Plane process is invalid and has been ignored: \"%s\".");
msgdef_explanation(
"One of the options that has been passed to the Data Plane process at startup time is not recognized. "
"The Data Plane process will continue normal operation, though the invalid option will be ignored. "
"The option that is not recognized is shown within the error message.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_DP, DP_OBSOLETED_OPTION, LOG_INFO,
"The following option provided to the Data Plane process is obsoleted and has been ignored: \"%s\". "
"Use option \"%s\" instead.");
msgdef_explanation(
"One of the options that has been passed to the Data Plane process at startup time has been obsoleted "
"and is no longer recognized. The Data Plane process will continue normal operation, though the obsoleted "
"option will be ignored. The option that is obsoleted is shown within the error message, as well as the "
"correct option to use.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_DP, DP_ISOL_CPU_FAILURE, LOG_ERR,
"A problem occurred associating the Data Plane thread with an isolated CPU (reason: %s).");
msgdef_explanation(
"The Data Plane encountered an error when attempting to assign the high priority data handling "
"thread to an isolated CPU.  The Data Plane process will continue to function, though in some "
"cases it may fail to perform error repairs, particularly when heavily loaded, due to contention "
"between non-time-critical system activities and time-critical receipt and transmission of video "
"data.  Failure to assign the data handling thread to an isolated CPU may be due to:  no "
"CPUs being specified via the \"isolcpus\" kernel boot parameter in /boot/grub/grub.conf; "
"specifying an invalid \"setcpu\" option value for the VQE-S Data Plane process; "
"or a software installation problem which prevents the Data Plane initialization process from "
"finding an isolated CPU.  "
"The text of the error message gives a more specific reason for the failure which should help "
"to determine the more specific cause of the failure. ");
msgdef_recommended_action(
"Check to make sure that the file /boot/grub/grub.conf contains the kernel boot parameter string "
"<b>isolcpus=n</b>, where n is in the range 0..3.  If the setcpu option has been specified "
"in the vqes_data_plane section of the .vqes.conf file, make sure that its value matches the "
"value specified in the isolcpus parameter string in /boot/grub/conf or "
"remove the parameter from the configuration entirely. "
SYSLOGDOC_RESTART_VQES
"If the problem persists after performing these recommended actions, "
"copy the error message exactly as it appears in the VQE-S system log, "
"contact your Cisco technical support representative, and provide the representative "
"with the gathered information.");

syslog_msgdef(LOG_VQES_DP, DP_CORE_UTIL_EXCEEDS_LIMIT, LOG_WARNING,
"Maximum CPU utilization has been exceeded on core %d.");
msgdef_explanation(
"The Data Plane has exceeded the maximum utilization for the given core while reserving resources "
"for a new channel. The Data Plane process will continue to function, though in some "
"cases it may fail to perform error repairs or RCC operations, due to the core being overloaded. "
"A core overload condition may be transient if the amount of error repairs being handled by that "
"core is unusually high when the new channel is created, or the condition may be caused by too many "
"channels being configured, in which case the condition will persist.");
msgdef_recommended_action(
"Correct any non-standard system configuration or installation that has been performed. "
SYSLOGDOC_REPORT_COPY_CONTACT
SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_DP, DP_CORE_UTIL_CORRUPT, LOG_INFO,
"CPU utilization accounting is corrupt on core %d.");
msgdef_explanation(
"The Data Plane has encountered a corruption of the utilization accounting function for the "
"given core. This problem is self-correcting, and the Data Plane process will continue to function. "
"In all cases, this error condition results from a flaw in the software.");
msgdef_recommended_action(
SYSLOGDOC_REPORT_COPY_CONTACT
SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_DP, DP_INIT_WARN, LOG_WARNING, 
"An abnormal condition occured during Data Plane initialization, "
"but VQES operation should continue normally (reason: %s).");
msgdef_explanation(
"An unexpected, though relatively minor, issue has occurred during the initialization of the "
"Data Plane process.  The Data Plane should continue to function normally.  The text of the "
"error message gives a more specific indication of the error that has occurred.");
msgdef_recommended_action(
SYSLOGDOC_RECUR_RESTART_VQES
"In most cases, normal operation of the VQE-S application should continue.  However if this error condition "
"should occur and the VQE-S application does not function correctly, or if the error condition occurs "
"together with more serious error conditions, then copy the error message exactly as it appears in the VQE-S system log, "
"contact your Cisco technical support representative, and provide the representative with the gathered information."
);

syslog_msgdef(LOG_VQES_DP, DP_SHUTDOWN_FAILURE, LOG_WARNING, 
"Abnormal condition encountered while shutting down Data Plane (reason: %s, linux error: %m).");
msgdef_explanation(
"A problem was encountered in shutting down the Data Plane process.  The VQE-S Process Monitor "
"will normally restart the Data Plane and any other affected processes as required, and "
"normal VQE-S operation should continue.  The text of the error message gives more specific "
"information about which step of the shutdown process was being performed when the problem "
"was encountered and also a standard Linux error message associated with the step that failed.");
msgdef_recommended_action(
"In most cases, the normal operation of the VQE-S application will resume automatically after the Process Monitor restarts "
"the VQE-S application processes, and no further action is required.  However if the VQE-S application should fail to resume normal "
"operation after this error message has been seen, then restart the VQE-S application manually.");

syslog_msgdef(LOG_VQES_DP, DP_CMAPI_INIT_FAILURE, LOG_CRIT,
"Failed to initialize the Cache Manager API (reason: %s, linux error %m).");
msgdef_explanation("A critical failure was encountered when initializing the Cache Manager "
"API.  The Data Plane process will exit and will be restarted by the VQE-S Process Monitor."
"If the error recurs on multiple restarts, the VQE-S application may be left in an non-operational "
"state.  The text of the error message indicates which step in the initialization process "
"was being performed when the error occurred, and also a standard Linux error message "
"associated with the failed step.  In most cases, this type of failure indicates an "
"installation problem with the VQE-S application software or a flaw in the software itself.");
msgdef_recommended_action(DP_STARTUP_FAILURE_ACTION);

syslog_msgdef(LOG_VQES_DP, DP_INTERNAL_FORCED_EXIT, LOG_CRIT, 
"The Data Plane process is exiting due to an unexpected internal error condition, "
"resulting from a problem with the software (reason: %s).");
msgdef_explanation("The Data Plane process has encountered an unexpected, internal "
"error condition, and must exit.  The VQE-S Process Monitor will normally attempt "
"to restart the Data Plane process along with any other affected processes."
"However, if the condition should recur, the VQE-S application may eventually be left in a "
"non-operational state.  The text of the error message indicates a more specific "
"reason for the failure.  In all cases, this error condition results from a flaw "
"in the software.");
msgdef_recommended_action(
SYSLOGDOC_REPORT_COPY_CONTACT
SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_DP, DP_INTERNAL_CHANNEL_ERROR, LOG_ERR,
"The Data Plane process has encountered an internal software problem which may affect the "
"operation of channel %s (reason: %s).");
msgdef_explanation(
"The Data Plane process has encountered an error which may prevent the VQE-S application from "
"correctly performing its functions for a single channel.  The VQE-S will continue "
"operation, and should correctly perform its functions for other channels.  The text "
"of the error message indicates which channel was affected, and also a more specific "
"reason for the problem.  In most cases, this type of problem results only from a "
"flaw in the software.");
msgdef_recommended_action(
SYSLOGDOC_REPORT_COPY_CONTACT
SYSLOGDOC_RESTART_VQES);


syslog_msgdef(LOG_VQES_DP, DP_TOO_MANY_SOURCES, LOG_NOTICE,
"Could not recognize a new traffic source for channel %s because "
"the Data Plane is already receiving from the maximum number of sources  (limit %d) "
"for that channel as may be recognized at once for a single channel.");
msgdef_explanation(
"The Data Plane process has detected incoming RTP traffic from a new source, but must "
"ignore the traffic from that source because it is already receiving traffic from "
"too many other sources for that channel.  The VQE-S may continue normal operation if one of the sources "
"it has already recognized is the desired source, but may fail to operate correctly "
"for the channel if the new source is the one that it should be receiving traffic from."
"  Also, the new source will not be reported by the VQE-S application among its lists of known "
"sources, and this may make troubleshooting of network configuration problems "
"involving multiple sources more difficult.  The affected channel is reported in the "
"text of the error message.  All other channels are unaffected.  This problem is most likely due "
"to a network configuration issue which causes multiple sources to be sending simultaneouly "
"on a single channel.");
msgdef_recommended_action(
"If the VQE-S application is functioning correctly for the channel indicated in the error message, "
"then no action is required.  If the VQE-S application is not functioning correctly for the "
"channel indicated in the error message, then "
"correct any network configuration issue which might cause the VQE-S application to be detecting more "
"than one traffic source for the channel indicated in the error message.");

syslog_msgdef(LOG_VQES_DP, DP_TOO_MANY_SOURCES_GLOBAL, LOG_NOTICE,
"Could not recognize a new traffic source for channel %s because "
"the Data Plane is already receiving from the maximum number of sources  "
"that it is able to support across all channels.");
msgdef_explanation(
"The Data Plane process has detected incoming RTP traffic from a new source, but must "
"ignore the traffic from that source because it is already receiving traffic from "
"too many other sources, across all channels.  The VQE-S may continue normal operation if one of the sources "
"it has already recognized is the desired source, but may fail to operate correctly "
"for the channel if the new source is the one that it should be receiving traffic from."
"Also, the new source will not be reported by the VQE-S application among its lists of known "
"sources, and this may make troubleshooting of network configuration problems "
"involving multiple sources more difficult.  The affected channel is reported in the "
"text of the error message.  Other channels are not immediately affected.  However, "
"other channels on the system are likely to encounter the same problem as soon as "
"they attempt to recognize new traffic sources.  This problem is most likely due "
"to a network configuration issue which causes multiple sources to be sending simultaneouly "
"on a single channel. "
);
msgdef_recommended_action(
"Correct any network configuration issue which might cause the VQE-S application to be detecting "
"extra sources for any of the channels being received by the VQE-S application.");



syslog_msgdef(LOG_VQES_DP, DP_MALLOC_FAILURE, LOG_ERR, 
"Memory allocation of %d bytes failed.");
msgdef_explanation(
"The Data Plane process failed to allocate memory.  Additional error messages should "
"indicate the consequences of the failure.");
msgdef_recommended_action(
SYSLOGDOC_REPORT_COPY_CONTACT
SYSLOGDOC_RESTART_VQES);


syslog_msgdef(LOG_VQES_DP, DP_BAD_SOCKET_CALL, LOG_ERR, 
"Linux socket operation failed (operation: %s, linux error: %m).");
msgdef_explanation("An operation on a Linux socket has failed.  Additional error "
"messages should indicate the consequences of the failure.  The text of the "
"error message indicates what operation was being performed when the failure occurred "
"and also the Linux error message associated with the failure.");
msgdef_recommended_action(
"Perform any actions associated with the additional error messages that occur together with "
"this error message.  If any of these actions indicates that you should contact your Cisco "
"technical support representative, then copy this error message exactly as it appears in the VQE-S system log "
"and provide it, in addition to any other requested information, when you contact your "
"technical support representative.");

syslog_msgdef(LOG_VQES_DP, DP_CHANNEL_CREATE_FAILED, LOG_ERR, 
"Failed to create a channel (reason: %s).");
msgdef_explanation(
"The Data Plane failed to create a new channel.  The VQE-S will not be able to "
"perform any of its functions for the affected channel, but other channels should "
"not be affected.  The reason for the failure is given in the text of the error message. "
"It may be possible to successfully create the channel, through the user interface, "
"without restarting the VQE-S application,  once the reason for the failure "
"has been corrected.");
msgdef_recommended_action(
"Correct the configuration error for the affected channel in the channel lineup "
"provided to the VQE-S application.  The reason code included in the error message should "
"provide information which will help to identify the source of the configuration "
"error.  "
"In cases where the channel creation failure resulted from a transient condition on the "
"VQE-S rather than from a configuration error in the channel lineup, "
"restart the VQE-S channels.");

syslog_msgdef(LOG_VQES_DP, DP_CHANNEL_CREATE, LOG_INFO, "Successfully created channel %s.");
msgdef_explanation(
"The Data Plane has successfully created a channel.  The channel "
"is identified in the text of the message. The VQE-S will now be able to "
"perform all of its functions for this channel.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);


syslog_msgdef(LOG_VQES_DP, DP_REALTIME_WARN, LOG_WARNING, 
"An error occurred which may affect the real-time responsiveness of the VQE-S, "
"though it should perform most error repairs successfully (reason: %s).");
msgdef_explanation(
"The Data Plane process encountered an unexpected limitation which may affect its real-time performance. "
"The VQE-S may continue to perform its functions in most cases, but may fail to complete certain operations "
"reliably, particularly when under load.  The reason for the failure is given in the text of the message.");
msgdef_recommended_action(
"Correct any non-standard system configuration or installation that has been performed. "
SYSLOGDOC_REPORT_COPY_CONTACT
SYSLOGDOC_RESTART_VQES);



syslog_msgdef(LOG_VQES_DP, DP_INIT_INFO, LOG_INFO, "Initialization: %s");
msgdef_explanation("The Data Plane has successfully completed a step in its initialization. "
"Normal operation will continue.  The text of the message identifies the step that "
"was completed, and may also provide additional information that will be useful in tracing "
"the operation of the Data Plane process.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);


syslog_msgdef(LOG_VQES_DP, DP_CMAPI_ERROR, LOG_CRIT, 
"Error in communications between the Data Plane and a client (reason: %s).  "
"The VQE-S processes will be restarted.");
msgdef_explanation(
"Error detected in the Cache Manager Server API communications with the client process.  "
"The Data Plane process will exit and must be restarted by the Process Monitor.  "
"If the condition recurs, the VQE-S application may be left in a non-operational state.  "
"The text of the error message provides a more specific reason for the failure.  This failure "
"only results from an internal problem of the software.");
msgdef_recommended_action(
SYSLOGDOC_REPORT_COPY_CONTACT
SYSLOGDOC_RESTART_VQES);


syslog_msgdef(LOG_VQES_DP, DP_RTP_SRC_MGMT_ERROR, LOG_NOTICE,
"Failed to complete an operation on an RTP source for channel %s (reason: %s).");
msgdef_explanation(
"The RTP code within the Cache Manager failed "
"an operation on an RTP source.  Additional error messages will provide information on "
"the consequences of the failure.  In most cases, normal operation will continue."
);
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_DP, DP_BAD_ARG, LOG_ERR, 
"A bad parameter was passed to the Cache Manager API (detail: %s).");
msgdef_explanation(
"A bad parameter was passed to the Cache Manager API by a client process.  "
"Additional error messages should indicate the effect of the error.  "
"The text of the error message provides additional detail on the bad parameter value.");
msgdef_recommended_action(
"Perform any actions associated with the additional error messages that occur together with "
"this error message.  If any of these actions indicates that you should contact your Cisco "
"technical support representative, then copy this error message exactly as it appears in the VQE-S system log "
"and provide it, in addition to any other requested information, when you contact your "
"technical support representative.");

syslog_msgdef(LOG_VQES_DP, DP_LOST_UPCALL, LOG_NOTICE,
"A notification sent from the Cache Manager to a client process was lost (reason: %s). "
" Normal operation will continue.");
msgdef_explanation(
"A notification message sent from the Cache Manager within the Data Plane to a client process was lost. "
"The client process will detect the loss and re-synchronize its state with the Cache Manager shortly, "
"and normal operation will continue.  The client process may react somewhat more slowly than normal to "
"the changed state within the Cache Manager, and as a result the operation of the VQE-S application for a particular "
"channel may be impaired for a few seconds before normal operation for that channel is restored. "
"The text of the error message identifies a more specific reason for the lost message.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_DP, DP_HIST_OP_FAILURE, LOG_INFO,
"A histogram operation failed on one or more cores (operation: %s).");
msgdef_explanation(
"The Data Plane process encountered an error when performing an "
"operation on a histogram. Histogram operations are performed on all "
"dedicated cores, and this message indicates that the operation has failed "
"on one or more of the cores, thus the information gathered will not be "
"complete. The text of the error message provides more detail on what "
"action was being performed.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

/* MPEG Parser Syslog Messages */

syslog_msgdef(LOG_VQES_DP, MP_INIT_FAIL, LOG_CRIT, 
              "The MPEG Parser failed to initialize properly: %s");
msgdef_explanation("The MPEG Parser failed to initialize properly. A possible "
                   "cause could be insufficient memory.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_DP, MP_CHANNEL_ADD_FAIL, LOG_ERR, 
              "The MPEG Parser failed to add a channel(%x): %s");
msgdef_explanation("The MPEG Parser failed to add a channel. A possible "
                   "cause could be insufficient memory or misconfigured "
                   "channel.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_DP, MP_CHANNEL_DEL_FAIL, LOG_ERR, 
              "The MPEG Parser failed to delete a channel(%x): %s");
msgdef_explanation("The MPEG Parser failed to delete a channel properly. "
                   "A possible cause could be ... XXX");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_DP, MP_STATS_GET_FAIL, LOG_ERR, 
              "The MPEG Parser could not retrieve statistics "
              "on channel %x: %s");
msgdef_explanation("The MPEG Parser could get statistics on the "
                   "given channel.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_DP, MP_GLOBAL_STATS_GET_FAIL, LOG_ERR, 
              "The MPEG Parser could not retrieve global statistics: %s");
msgdef_explanation("The MPEG Parser could get global statistics");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_DP, MP_LOW_PERF_OPTION, LOG_WARNING,
              "The VQE-S has been started with a performance affecting option "
              "enabled. This option removes a performance enhancing internal "
              "limit of the VQE-S and could lead to system instability or "
              "interruption of Error Repair and Rapid Channel Change "
              "functionality.");
msgdef_explanation("When this option is enabled, the VQE-S ignores a limit "
                   "used in processing video data. This may cause the VQE-S "
                   "to consume a more than nominal amount of CPU time. In "
                   "this mode the VQE-S functionality may be affected.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_DP, MP_OPTION_ENABLED, LOG_WARNING,
              "The VQE-S has been started with the following option "
              "enabled: %s");
msgdef_explanation("When this message is displayed, the VQE-S is running with "
                   "the listed option enabled. This should match the desired "
                   "VQE-S configuration.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);


syslog_msgdef(LOG_VQES_DP, MP_INIT_START, LOG_INFO, 
              "Initializing the MPEG Parser");
msgdef_explanation("Beginning MPEG Parser initialization. If it fails at "
                   "a later stage, another syslog will be displayed "
                   "indicating the reason for failure.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_DP, MP_CHANNEL_ADD_GOOD, LOG_INFO, 
              "The MPEG Parser has successfully added channel %x");
msgdef_explanation("The MPEG Parser has successfully added the given channel");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_DP, MP_CHANNEL_DEL_GOOD, LOG_INFO, 
              "The MPEG Parser has successfully deleted channel %x");
msgdef_explanation("The MPEG Parser has successfully deleted the given channel");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

/* End MPEG Parser Syslogs */


#ifdef HAVE_SOURCE_MODE

syslog_msgdef(LOG_VQES_DP, DP_NORTC, LOG_INFO, ": %s %m");
msgdef_explanation("Not able to use the real-time clock for sleeping [chars] [chars].");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION
"Not in FCS image.");

syslog_msgdef(LOG_VQES_DP, DP_RTC_FAILED, LOG_ERR, ":  %m");
msgdef_explanation("The real-time clock has failed and subsequent sleeps will"
" be less precise.  The traffic shape may be affected. [chars] ");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION
"Not in FCS image");

syslog_msgdef(LOG_VQES_DP, DP_OUTPUT_STREAM_CREATE_FAILED, LOG_ERR, ": %s");
msgdef_explanation("Failed to create an output stream: [chars].");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION
"Not in FCS image");

#endif /* HAVE_SOURCE_MODE */


#endif /* _VQES_DP_SYSLOG_DEF_H_ */
