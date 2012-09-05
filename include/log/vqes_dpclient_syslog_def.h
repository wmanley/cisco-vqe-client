/********************************************************************
 * vqes_dpclient_syslog_def.h
 *
 * This file defines the SYSLOG messages for the VQE-S Data Plane Client.
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQES_DPCLIENT_SYSLOG_DEF_H_
#define _VQES_DPCLIENT_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_VQES_DPCLIENT, LOG_VQES_DPCLIENT, LOG_ERR);

syslog_msgdef(LOG_VQES_DPCLIENT, DPC_INIT_INFO, LOG_INFO, "Initialization: %s");
msgdef_explanation(
"The Cache Manager Client API has successfully completed a step in its initialization. "
"Normal operation will continue.  The text of the message indicates which step "
"was successfully completed.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_VQES_DPCLIENT, DPC_CMAPI_INIT_ERROR, LOG_CRIT,
              "Initialization failed (reason: %s, linux error: %m).");
msgdef_explanation(
"A severe error was encountered while attempting to initialize the Cache Manager Client API. "
"The affected process will terminate and be restarted by the VQE-S Process Monitor.  "
"However if the failure should recur then the VQE-S application may be left in a non-operational state. "
"The text of the error message provides additional detail on the operation that failed and "
"on the Linux error message associated with the failure. ");
msgdef_recommended_action(
SYSLOGDOC_REPORT_COPY_CONTACT
SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_DPCLIENT, DPC_CMAPI_ERROR, LOG_CRIT,
"Communications problem with VQE Cache Manager (reason: %s, linux error: %m).  "
"VQE-S processes must restart.");
msgdef_explanation(
"A severe error was encountered in the communication between the Cache Manager Client API "
"and the Data Plane process.  The process running the client API and other affected processes "
"will be restarted by the VQE-S Process Monitor.  If the error should recur, the VQE-S application may be left "
"in a non-operational state.  This error normally indicates an internal problem with the software. "
"The text of the error message provides additional detail on the reason for the error and on "
"the associated Linux error message.");
msgdef_recommended_action(
SYSLOGDOC_REPORT_COPY_CONTACT
SYSLOGDOC_RESTART_VQES);

#endif /* _VQES_DPCLIENT_SYSLOG_DEF_H_ */
