/********************************************************************
 * vqes_fbt_addr_syslog_def.h
 *
 * This file defines the SYSLOG messages for the FBT address utilities
 * on the Control Plane.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQES_FBT_ADDR_SYSLOG_DEF_H_
#define _VQES_FBT_ADDR_SYSLOG_DEF_H_

#include "vqes_cp_syslog_def.h"

syslog_msgdef(LOG_VQES_CP, VQE_FBT_GEN_ERROR, LOG_ERR,
              "Error when performing an operation on "
              "a Feedback Target address (detail: %s, linux error: %m).");
msgdef_explanation(
    "The Control Plane process encountered an error when performing an "
    "operation on a Feedback Target address.  Additional error messages "
    "should indicate whether the condition caused the overall request to fail "
    "or whether the Control Plane process was able to take corrective action, "
    "and what the consequence of the failure will be. The text of the "
    "error message provides more detail on what action was being performed, "
    "and on the Linux error condition that was encountered.");
msgdef_recommended_action(
    "Perform any actions associated with the additional error messages that "
    "occur together with this error message.  If any of these actions "
    "indicates that you should contact your Cisco technical support "
    "representative, then copy this error message exactly as it appears "
    "in the VQE-S system log and provide it, in addition to any other "
    "requested information, when you contact your technical support "
    "representative.");

syslog_msgdef(LOG_VQES_CP, VQE_FBT_REQ_ERROR, LOG_ERR,
              "Failed to complete operation on a Feedback Target address: "
              "%s %s %s.");
msgdef_explanation(
    "The Control Plane failed to complete a requested operation on a "
    "Feedback Target address.  The failure may have left some Feedback Target "
    "addresses installed which should have been removed, or may have left "
    "some Feedback Target addresses removed which should have been installed. "
    "In the case of extra Feedback Target addresses the VQE-S application may "
    "advertise services for channels for which it is not actually able to "
    "provide service.  This may cause VQE-S operations which could have been "
    "directed to a different VQE-S to be incorrectly directed to the VQE-S "
    "which cannot provide the service.  In the case of missing Feedback "
    "Target addresses, clients may not be able to send requests to the VQE-S "
    "which could have successfully serviced the requests.  In either case, "
    "some VQE-S operations which could have been successfully completed may "
    "fail.  The text of the error message identifies the operation that was "
    "not completed successfully, the Feedback Target address, and some "
    "additional detail on the cause of the failure.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_CP, VQE_FBT_REQ_INFO, LOG_INFO,
              "Successfully completed operation on Feedback Target address: "
              "%s %s %s.");
msgdef_explanation(
    "The Control Plane successfully completed an operation on a Feedback "
    "Target address. Normal operation will continue. The completed operation, "
    "the Feedback Target address and some additional detail are provided in "
    "the message.");
msgdef_recommended_action(
    SYSLOGDOC_INFO_ONLY);

#endif /* _VQES_FBT_ADDR_SYSLOG_DEF_H_ */
