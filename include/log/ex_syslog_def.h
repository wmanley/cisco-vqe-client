/********************************************************************
 * ex_syslog_def.h
 *
 * Derived from DC-OS include/isan/ex_syslog_def.h
 * 
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/
#ifndef _EX_SYSLOG_DEF_H
#define _EX_SYSLOG_DEF_H

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_EX_DEBUG, LOG_EX_SYSLOG, LOG_DEBUG);

syslog_msgdef(LOG_EX_SYSLOG, EMER_MSG, LOG_EMERG, "Emergency Message - %d");
msgdef_explanation("A Dummy emergency message to test syslogd from ex_syslog."
                   "[dec] is the message count");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_EX_SYSLOG, ALERT_MSG, LOG_ALERT, "Alert Message - %s %d");
msgdef_explanation("A Dummy alert message to test syslogd from ex_syslog."
                   "[chars] is the a dummy text. [dec] is the message count");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_EX_SYSLOG, CRIT_MSG, LOG_CRIT, "Critical Message - %d");
msgdef_explanation("A Dummy critical message to test syslogd from ex_syslog."
                   "[dec] is the message count");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_EX_SYSLOG, ERR_MSG, LOG_ERR, "Error Message - %d");
msgdef_explanation("A Dummy error message to test syslogd from ex_syslog."
                   "[dec] is the message count");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_EX_SYSLOG, WARNING_MSG, LOG_WARNING, "Warning Message - %d");
msgdef_explanation("A Dummy warning message to test syslogd from ex_syslog."
                   "[dec] is the message count");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_EX_SYSLOG, NOTICE_MSG, LOG_NOTICE, "Notice Message - %d");
msgdef_explanation("A Dummy notice message to test syslogd from ex_syslog."
                   "[dec] is the message count");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_EX_SYSLOG, INFO_MSG, LOG_INFO, "Info Message - %d");
msgdef_explanation("A Dummy info message to test syslogd from ex_syslog."
                   "[dec] is the message count");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_EX_SYSLOG, DEBUG_MSG, LOG_DEBUG, "Debug Message - %d");
msgdef_explanation("A Dummy debug message to test syslogd from ex_syslog."
				   "[dec] is the message count");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION
                          SYSLOGDOC_INFO_ONLY
                          SYSLOGDOC_REBOOT_SERVER
                          SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_RECUR_RESTART_VQES
                          SYSLOGDOC_RESTART_VQES_CHANNELS
                          SYSLOGDOC_CHG_RESEND_CHANNELS
                          SYSLOGDOC_CHG_CONF
                          SYSLOGDOC_REPORT
                          SYSLOGDOC_CONTACT
                          SYSLOGDOC_COPY
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_REPORT_COPY_CONTACT
                          );


#endif /* _EX_SYSLOG_DEF_H */
