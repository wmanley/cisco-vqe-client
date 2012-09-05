/********************************************************************
 * vqec_syslog_def.h
 *
 * This file defines the SYSLOG messages for VQE-C.
 *
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQEC_SYSLOG_DEF_H_
#define _VQEC_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_VQEC, LOG_VQEC, LOG_ERR);

/*  message priorities:
 *   LOG_EMERG    0  system is unusable
 *   LOG_ALERT    1  action must be taken immediately
 *   LOG_CRIT     2  critical conditions
 *   LOG_ERR      3  error conditions
 *   LOG_WARNING  4  warning conditions
 *   LOG_NOTICE   5  normal but significant condition
 *   LOG_INFO     6  informational
 *   LOG_DEBUG    7  debug-level messages
 */

syslog_msgdef(LOG_VQEC, VQEC_INVALIDARGS, LOG_CRIT,
              "invalid arguments passed to function: %s");
msgdef_explanation("invalid arguments passed to function: [chars].");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_MALLOC_FAILURE, LOG_CRIT, ": %s");
msgdef_explanation("malloc failed: [chars].");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_ERROR, LOG_ERR, ": %s");
msgdef_explanation("vqec error: [chars].");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_WARNING, LOG_WARNING, ": %s");
msgdef_explanation("vqec warning: [chars].");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_NOTICE, LOG_NOTICE, "%s");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_INFO, LOG_INFO, "%s");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_SYSTEM_ERROR, LOG_CRIT, ": %s%s");
msgdef_explanation("vqec system error: [chars].");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_SM_EVENT_UNEXPECTED, LOG_CRIT,
              "Unexpected event %s in state %s");
msgdef_explanation("rcc sm: [chars].");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);


syslog_msgdef(LOG_VQEC, VQEC_SM_EVENT_INVALID, LOG_CRIT,
              "Invalid state machine event %s");
msgdef_explanation("rcc sm: [chars].");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_RTP_SESSION_STATE_ERROR, LOG_ERR,
              "channel %s rtp session error (%s)");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);


syslog_msgdef(LOG_VQEC, VQEC_CHAN_ERROR, LOG_ERR,
              "Channel %s error (%s)");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_IPC_ERROR, LOG_ERR,
              "IPC communication failure, result (%d), response len (%u/%u), "
              "function num (%u/%u), response ver (%u)");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);             

syslog_msgdef(LOG_VQEC, VQEC_IPC_TIMEOUT_ERROR, LOG_ERR,
              "IPC communication timeout");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

/*
 * Outcomes of configuration updates
 */
syslog_msgdef(LOG_VQEC, VQEC_CONFIG_ERROR, LOG_ERR,
              "%s Configuration Update Error (%s)");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_CONFIG_UPDATE, LOG_INFO,
              "%s Configuration Update Received");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

/*
 * Errors while processing configuration files
 */
syslog_msgdef(LOG_VQEC, VQEC_SYSCFG_ERROR, LOG_ERR, "%s");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

/*
 * Errors related to specific configuration parameters
 */
syslog_msgdef(LOG_VQEC, VQEC_SYSCFG_PARAM_IGNORED, LOG_WARNING, "%s");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_SYSCFG_PARAM_DEPRECATED, LOG_INFO, "%s");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC, VQEC_SYSCFG_PARAM_INVALID, LOG_ERR, "%s");
msgdef_explanation(".");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

#ifdef HAVE_IPLM
#include "iplm_syslog_def.h"
#endif

#endif /* _VQEC_SYSLOG_DEF_H_ */
