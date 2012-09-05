/********************************************************************
 * msgen_syslog_def.h
 *
 * This file defines the SYSLOG messages for the msgen tool
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _MSGEN_SYSLOG_DEF_H_
#define _MSGEN_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

/* Make use of the VQES_CP facility */
//syslog_facilitydef(VQENAME_VQES_CP, LOG_VQES_CP, LOG_ERR);

/* Message set #1 */
syslog_msgdef_limit(LOG_VQES_CP, MSGEN_ERR_MSG_RL_1, LOG_ERR,
                    "#1 rate-limited error msg: %s",
                    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation("A rate-limited test error message");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef_limit(LOG_VQES_CP, MSGEN_WARN_MSG_RL_1, LOG_WARNING,
                    "#1 rate-limited warning msg: %s",
                    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation("A rate-limited test warning message");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef_limit(LOG_VQES_CP, MSGEN_INFO_MSG_RL_1, LOG_INFO,
                    "#1 rate-limited info msg: %s",
                    MSGDEF_LIMIT_FAST);
msgdef_explanation("A rate-limited test info message");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_CP, MSGEN_INFO_MSG_1, LOG_INFO,
              "#1 info msg: %s");
msgdef_explanation("A normal info message without rate-limiting");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

/* Message set #2 */
syslog_msgdef_limit(LOG_VQES_CP, MSGEN_ERR_MSG_RL_2, LOG_ERR,
                    "#2 rate-limited error msg: %s",
                    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation("A rate-limited test error message");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef_limit(LOG_VQES_CP, MSGEN_WARN_MSG_RL_2, LOG_WARNING,
                    "#2 rate-limited warning msg: %s",
                    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation("A rate-limited test warning message");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef_limit(LOG_VQES_CP, MSGEN_INFO_MSG_RL_2, LOG_INFO,
                    "#2 rate-limited info msg: %s",
                    MSGDEF_LIMIT_FAST);
msgdef_explanation("A rate-limited test info message");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQES_CP, MSGEN_INFO_MSG_2, LOG_INFO,
              "#2 info msg: %s");
msgdef_explanation("A normal info message without rate-limiting");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

/* Message set #3: slow msgs */
syslog_msgdef_limit(LOG_VQES_CP, MSGEN_ERR_SLOW_MSG, LOG_ERR,
                    "rate-limited slow error msg: %s",
                    MSGDEF_LIMIT_SLOW);
msgdef_explanation("A rate-limited slow test error message");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef_limit(LOG_VQES_CP, MSGEN_WARN_SLOW_MSG, LOG_WARNING,
                    "rate-limited slow warning msg: %s",
                    MSGDEF_LIMIT_SLOW);
msgdef_explanation("A rate-limited slow test warning message");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);


#endif /* _MSGEN_SYSLOG_DEF_H_ */
