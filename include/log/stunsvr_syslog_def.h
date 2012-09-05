/********************************************************************
 * stunsvr_syslog_def.h
 *
 * This file defines the SYSLOG messages for the STUN Server.
 *
 * October 2007, Donghai Ma
 * 
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _STUNSVR_SYSLOG_DEF_H_
#define _STUNSVR_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_STUN_SERVER, LOG_STUN_SERVER, LOG_ERR);

/* Alert messages */

/* Critical messages */
syslog_msgdef(LOG_STUN_SERVER, SS_INTERNAL_FORCED_EXIT, LOG_CRIT,
              "The STUN Server process is exiting due to an unexpected "
              "internal error condition, resulting from a problem with the "
              "software (reason: %s).");
msgdef_explanation("The STUN Server process has encountered an unexpected, "
                   "internal error condition, and must exit.  The VQE-S "
                   "Process Monitor will normally attempt to restart the "
                   "STUN Server process along with any other affected "
                   "processes. However, if the condition should recur, the "
                   "VQE-S may eventually be left in a non-operational state. "
                   "The text of the error message indicates a more specific "
                   "reason for the failure.  In all cases, this error "
                   "condition results from a flaw in the software.");
msgdef_recommended_action(SYSLOGDOC_REPORT_COPY_CONTACT);

syslog_msgdef(LOG_STUN_SERVER, SS_INIT_FAILURE_CRIT, LOG_CRIT,
              "STUN Server initialization failed due to %s");
msgdef_explanation("A software error has occurred during the initialization "
                   "of the STUN Server process and the process will not "
                   "start.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);


/* Error messages */
syslog_msgdef(LOG_STUN_SERVER, SS_SYSCALL_ERROR, LOG_ERR,
              "A system call failed on STUN Server: %s");
msgdef_explanation("A system call has failed on the STUN Server. The reason "
                   "for the failure is provided in the log message.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);


/* Warning messages */
syslog_msgdef(LOG_STUN_SERVER, SS_INVALID_OPTION_WARN, LOG_WARNING, 
              "Invalid command line option \"%s\" is present.");
msgdef_explanation("An illegal command line option is detected.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_STUN_SERVER, SS_INVALID_OPTION_VALUE, LOG_WARNING,
              "The value %d for option \"%s\" is out of range and has been "
              "ignored. The valid range is [%d-%d]. The default value of %d "
              "is used instead.");
msgdef_explanation("One of the option values that has been passed to the "
                   "STUN Server process at startup time is out of range. "
                   "The STUN Server process will continue normal operation, "
                   "though the illegal option value that was passed will be "
                   "ignored and the default value will be used instead. "
                   "The error message gives both the option whose value was "
                   "out of range and the legal range of values for the option.");
msgdef_recommended_action("Specify the appropriate value for the parameter in the "
                          "stun_server section of the VQE-S configuration file."
                          SYSLOGDOC_CHG_CONF);


/* Info messages */
syslog_msgdef(LOG_STUN_SERVER, SS_STARTUP_COMPLETE_INFO, LOG_INFO,
              "STUN Server has (re)started");
msgdef_explanation("This is an informational message logged when the "
                   "STUN Server finishes startup sequence successfully.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

/* Notice messages */


#endif /* _STUNSVR_SYSLOG_DEF_H_ */
