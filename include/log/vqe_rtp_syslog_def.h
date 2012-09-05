/********************************************************************
 * vqe_rtp_syslog_def.h
 *
 * This file defines the SYSLOG messages for the VQE RTP/RTCP stack.
 *
 * Copyright (c) 2007-2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQE_RTP_SYSLOG_DEF_H_
#define _VQE_RTP_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_RTP, LOG_RTP, LOG_ERR);

syslog_msgdef(LOG_RTP, RTCP_MEM_INIT_FAILURE, LOG_CRIT, 
              "Failed to initialize RTCP memory pool %s");
msgdef_explanation("The initialization of the specified RTCP memory pool "
                   "has failed; the application (e.g., ERA) using the pool, "
                   "which is specified as part of the pool name, "
                   "will not function correctly.");
msgdef_recommended_action(SYSLOGDOC_REPORT_COPY_CONTACT
                          SYSLOGDOC_RESTART_VQES
                          "If this message recurs after restart, "
                          "reboot the VQE-S server.");

#define SYSLOGDOC_RTP_DBERR_ACTIONS   \
              SYSLOGDOC_RECUR_COPY    \
              SYSLOGDOC_REPORT        \
              SYSLOGDOC_CONTACT       \
              SYSLOGDOC_RESTART_VQES

syslog_msgdef(LOG_RTP, RTCP_CNAME_UPDATE_FAILURE, LOG_ERR,
              "Could not update the member of session %s:%u "
              "with SSRC 0x%08x from CNAME %s to CNAME %s: %s");
msgdef_explanation("An attempt to update the CNAME of a member of the "
                   "specified session failed, for the indicated reason. "
                   "In nearly all cases, this is due to an internal "
                   "software error. If this condition persists, messages "
                   "from the member (e.g., repair requests) may not be "
                   "processed correctly.");
msgdef_recommended_action(SYSLOGDOC_RTP_DBERR_ACTIONS);

syslog_msgdef(LOG_RTP, RTCP_CNAME_ROLLBACK_FAILURE, LOG_ERR,
              "Rollback failure: could not restore the member of session %s:%u "
              "with SSRC 0x%08x CNAME %s back to the member database, after a "
              "failure to update the CNAME to %s");
msgdef_explanation("An attempt to restore a member back to the per-sesssion "
                   "member database, with its original CNAME, has failed, "
                   "after an attempt to update the CNAME has failed. "
                   "In nearly all cases, this is due to an internal "
                   "software error. If this condition persists, messages "
                   "from the member (e.g., repair requests) may not be "
                   "processed correctly.");
msgdef_recommended_action(SYSLOGDOC_RTP_DBERR_ACTIONS);

syslog_msgdef(LOG_RTP, RTCP_SSRC_ROLLBACK_FAILURE, LOG_ERR,
              "Rollback failure: could not restore the member of session %s:%u "
              "with SSRC 0x%08x CNAME %s back to the member database, after a "
              "failure to update the SSRC to 0x%08x");
msgdef_explanation("An attempt to restore a member back to the per-sesssion "
                   "member database, with its original SSRC, has failed, "
                   "after an attempt to update the SSRC has failed. "
                   "In nearly all cases, this is due to an internal "
                   "software error. If this condition persists, messages "
                   "from the member (e.g., repair requests) may not be "
                   "processed correctly.");
msgdef_recommended_action(SYSLOGDOC_RTP_DBERR_ACTIONS);

syslog_msgdef(LOG_RTP, RTCP_LOCAL_SSRC_UPDATE_FAILURE, LOG_ERR,
              "Could not update the local SSRC of %s session %s:%u "
              "from SSRC 0x%08x to SSRC 0x%08x");
msgdef_explanation("An attempt to update the local SSRC of the specified "
                   "session has failed. In nearly all cases, this is due to "
                   "an internal software error. If this condition persists, "
                   "services of this session (e.g., RTCP report generation) "
                   "may not function properly.");
msgdef_recommended_action(SYSLOGDOC_RTP_DBERR_ACTIONS);

syslog_msgdef(LOG_RTP, RTCP_XR_NOT_INCLUDED_WARN, LOG_WARNING,
              "RTCP XR data (report block type %d) exceeds the given size "
              "in RTCP compound packet. Report block will not be included.");
msgdef_explanation("Size of RTCP XR report is larger than the given size.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_RTP, RTCP_XR_EXCEED_LIMIT_INFO, LOG_INFO,
              "RTCP XR data (block type %d, data size %d) exceeds given size "
              "%d in RTCP compound packet. Data will be trimmed "
              "to fit into the packet.");
msgdef_explanation("Size of RTCP XR report is larger than the given size and "
                   "XR data will be trimmed to fit into the given space.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_RTP, RTCP_XR_NOT_REPORTED_INFO, LOG_INFO,
              "%d packets not being reported in RTCP XR %s.");
msgdef_explanation("Number of packets is not reported in RTCP XR.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

/*----  Exporter Syslogs -----------------------------------*/



syslog_msgdef(LOG_RTP, EXP_DUPLICATE_INIT, LOG_WARNING,
	      "Duplicate initialization of Exporter; routine called: %s "
	      "current state: %d");
msgdef_explanation("An Exporter initialization routine has been called more "
		   "than once;  duplicate calls are ignored.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_REPORT
			  SYSLOGDOC_CONTACT
			  SYSLOGDOC_RECUR_RESTART_VQES);


syslog_msgdef(LOG_RTP, EXP_CONFIG_ERROR, LOG_ERR,
	      "Exporter failed to initialize due to invalid or missing "
	      "configuration; reason: %s");
msgdef_explanation("The Exporter failed to initialize because Exporter "
		   "configuration information is either missing from or "
		   "incorrect in the VQE-S configuration file (vcdb.conf). "
		   "The reason string identifies which parameter is missing "
                   "or invalid.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_RTP, EXP_INIT_FAIL_RETRY, LOG_ERR,
	      "Exporter failed to initialize due to a potentially temporary "
	      "condition; reason: %s");
msgdef_explanation("The Exporter failed to initialize due to a potentially "
		   "temporary system condition such as lack of available "
		   "memory, or failure to start a timer.  The Exporter will "
		   "periodically retry its initialization, but if this "
		   "condition persists, the Exporter will not function.");
msgdef_recommended_action("If this condition persists, contact your Cisco "
			  "technical support representative, provide the "
			  "representative with this error message exactly as "
			  "it appears in the VQE-S system log, and restart the"
			  "VQE-S application.");


syslog_msgdef(LOG_RTP, EXP_INIT_FAIL_BAD_RETURN, LOG_ERR,
	      "Exporter failed to initialize due to unexpected return value: "
	      "calling function: %s, called function: %s, returned value: %d");
msgdef_explanation("The Exporter failed to initialize because it "
		   "received an undefined return code from an internal "
		   "function call. The Exporter will periodically retry the "
		   "initialization, but if this condition persists, the "
		   "Exporter will not function.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_REPORT
			  SYSLOGDOC_CONTACT
			  SYSLOGDOC_RECUR_RESTART_VQES);


syslog_msgdef(LOG_RTP, EXP_INIT_FAIL_SOCKET_INIT, LOG_ERR,
	      "Exporter failed to initialize due to an error "
	      "creating or initializing the TCP socket; "
	      "failed operation: %s, failure reason: %s");
msgdef_explanation("The Exporter failed to initialize because it "
		   "received an error from a system socket initialization "
		   "function call. The Exporter will periodically retry the "
		   "initialization, but if this condition persists, the "
		   "Exporter will not function.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_REPORT
			  SYSLOGDOC_CONTACT
			  SYSLOGDOC_RECUR_RESTART_VQES);

syslog_msgdef(LOG_RTP, EXP_SOCKET_CLEANUP_ERROR, LOG_ERR,
	      "Exporter experienced an error on socket cleanup; "
	      "function called: %s, cleanup operation being performed: %s");
msgdef_explanation("The Exporter received an error return code from an "
		   "internal function called during socket cleanup. "
		   "This error is likely benign.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_REPORT
			  SYSLOGDOC_CONTACT
			  SYSLOGDOC_RECUR_RESTART_VQES);


syslog_msgdef(LOG_RTP, EXP_SHUTDOWN_ERROR, LOG_ERR,
	      "Exporter experienced an error on shutdown; function called: %s,"
	      " shutdown operation being performed: %s");
msgdef_explanation("The Exporter received an error return code from an "
		   "internal function called during shutdown.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_REPORT
			  SYSLOGDOC_CONTACT
			  SYSLOGDOC_RECUR_RESTART_VQES);


syslog_msgdef(LOG_RTP, EXP_EXPORT_FAIL_BAD_INPUT, LOG_ERR,
	      "Exporter failed to export a packet due to bad input argument; "
	      "function: %s, bad argument: %s, argument value: %d");
msgdef_explanation("The Exporter failed to export a packet because it "
		   "was called with an invalid input argument.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_REPORT
			  SYSLOGDOC_CONTACT
			  SYSLOGDOC_RECUR_RESTART_VQES);


syslog_msgdef(LOG_RTP, EXP_EXPORT_FAIL_BAD_RETURN, LOG_ERR,
	      "Exporter failed to export a packet due to bad return value: "
	      "calling function: %s, called function: %s, returned value: %d");
msgdef_explanation("The Exporter failed to export a packet because it "
		   "received a bad return code from an internal function "
		   "call.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_REPORT
			  SYSLOGDOC_CONTACT
			  SYSLOGDOC_RECUR_RESTART_VQES);


syslog_msgdef(LOG_RTP, EXP_EXPORT_FAIL_INVALID_STATE, LOG_ERR,
	      "Exporter failed to export a packet "
	      "due to being called while in an invalid state; "
	      "function called: %s, current state: %d");
msgdef_explanation("The Exporter failed to export a packet because "
		   "its export function was called while the Exporter "
		   "was in an invalid state.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_REPORT
			  SYSLOGDOC_CONTACT
			  SYSLOGDOC_RECUR_RESTART_VQES);


syslog_msgdef(LOG_RTP, EXP_UI_FAIL_BAD_INPUT, LOG_ERR,
	      "Exporter failed to return status information to the user "
	      "interface due to bad input argument; function called: %s, bad "
	      "argument: %s, argument value: %d");
msgdef_explanation("The Exporter failed to respond to a query from the user "
		   "interface because it was called with an invalid input "
		   "argument.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_REPORT
			  SYSLOGDOC_CONTACT
			  SYSLOGDOC_RECUR_RESTART_VQES);


syslog_msgdef(LOG_RTP, EXP_UI_FAIL_UNINIT_STATE, LOG_ERR,
	      "Exporter failed to return status information to user interface "
	      "due to being called before it has initialized; "
	      "function called: %s.");
msgdef_explanation("The Exporter failed to respond to a query from the user "
		   "interface because its UI function was called before "
		   "Exporter was initialized.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_REPORT
			  SYSLOGDOC_CONTACT
			  SYSLOGDOC_RECUR_RESTART_VQES);


syslog_msgdef(LOG_RTP, EXP_UI_FAIL_INVALID_STATE, LOG_ERR,
	      "Exporter failed to return status information to user interface "
	      "due to being called when in an undefined state; "
	      "function called: %s, current state: %d");
msgdef_explanation("The Exporter failed to respond to a query from the user "
		   "interface because its UI function was called when "
		   "Exporter was in an undefined state.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_REPORT
			  SYSLOGDOC_CONTACT
			  SYSLOGDOC_RECUR_RESTART_VQES);

syslog_msgdef(LOG_RTP, EXP_EXPORT_FAIL_TOO_BIG, LOG_ERR,
	      "Exporter failed to export oversized RTCP report of %d bytes. "
	      "Maximum allowed size is %d");
msgdef_explanation("The Exporter failed to export an RTCP packet because "
		   "it was larger than the maximum allowable size.  Other "
		   "packets will continue to be exported.");
msgdef_recommended_action(SYSLOGDOC_COPY
			  SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_RTP, EXP_FILTERING_NACKS, LOG_INFO,
	      "Exporter is configured to drop RTCP NACKs");
msgdef_explanation("Exporter is configured to drop "
                   "RTCP Compound packets with Generic NACKs");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);


#endif /* _VQE_RTP_SYSLOG_DEF_H_ */
