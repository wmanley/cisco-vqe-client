/********************************************************************
 * vqes_mlbclient_syslog_def.h
 *
 * This file defines the SYSLOG messages for the VQE-S MLB Client.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQES_MLBCLIENT_SYSLOG_DEF_H_
#define _VQES_MLBCLIENT_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_VQES_MLBCLIENT, LOG_VQES_MLBCLIENT, LOG_ERR);

syslog_msgdef(LOG_VQES_MLBCLIENT, MLBCLIENT_INIT_ERR, LOG_CRIT,
              "MLB client failed to initialize %s.");
msgdef_explanation("MLB client failed to initialize. Application will not "
                   "function properly.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_VQES_MLBCLIENT, MLBCLIENT_LOCK_ERR, LOG_CRIT,
              "MLB client failed to %s rpc_lock.");
msgdef_explanation("Error detected during lock operation. Application will "
                   "not operate properly and must be restarted.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_VQES_MLBCLIENT, MLBCLIENT_SEND_ERR, LOG_ERR,
              "MLB client RPC send failed.");
msgdef_explanation("Failed to send an RPC command to MLB service, application"
                   " will not function properly under such condition.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_VQES_MLBCLIENT, MLBCLIENT_RPC_READ_ERR, LOG_ERR,
              "MLB client socket read error: %s.");
msgdef_explanation("Failed to read data from remote service, MLB client "
                   "will continue to function but this is abnormal.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_VQES_MLBCLIENT, MLBCLIENT_RETURN_CODE, LOG_INFO,
              "MLB_API got return code: %i.");
msgdef_explanation("Return code from MLB service, -1 means failure, 0 means "
                   "success, 1 means recoverable failure, 2 means unrecoverable "
                   "failure, 3 means invalid command.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

#endif /* _VQES_MLBCLIENT_SYSLOG_DEF_H_ */

