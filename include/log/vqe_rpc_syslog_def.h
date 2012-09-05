/********************************************************************
 * vqe_rpc_syslog_def.h
 *
 * This file defines the SYSLOG messages for the VQE RPC mechanism.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQE_RPC_SYSLOG_DEF_H_
#define _VQE_RPC_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_RPC, LOG_RPC, LOG_ERR);

syslog_msgdef(LOG_RPC, RPC_PROCESS_ABORT, LOG_CRIT, 
              "RPC process abort: %s");
msgdef_explanation("An RPC (remote procedure call) client or server process "
                   "has aborted, due to a previous unrecoverable internal "
                   "software error, specified in an earlier system message. "
                   "The process will be restarted.");
msgdef_recommended_action(SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_RPC, RPC_INTERNAL_ERR, LOG_ERR, 
              "RPC internal error: %s");
msgdef_explanation("The RPC (remote procedure call) mechanism has experienced "
                   "an internal software error, for the specified reason.");
msgdef_recommended_action(SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_RPC, RPC_MESSAGE_ERR, LOG_ERR, 
              "RPC message error: %s");
msgdef_explanation("The RPC (remote procedure call) mechanism has detected "
                   "an error in an RPC message, for the specified reason.");
msgdef_recommended_action(SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_RPC, RPC_PROCESS_EXIT, LOG_WARNING, 
              "RPC %s process has exited: %s");
msgdef_explanation("An RPC (remote procedure call) client or server process "
                   "has exited, for the specified reason. This may occur "
                   "when the VQE-S service is stopped or restarted, but "
                   "may also occur when the other process has aborted "
                   "due to an error. The process will be restarted.");
msgdef_recommended_action("If this message is not related to VQE-S service "
                          "stop or restart, copy the error message (and any "
                          "related ones) exactly as they appears in the "
                          "system log. "
                          SYSLOGDOC_CONTACT);

#endif /* _VQE_RPC_SYSLOG_DEF_H_ */
