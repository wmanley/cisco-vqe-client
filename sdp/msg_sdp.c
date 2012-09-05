/*------------------------------------------------------------------
 * Logger messages for SDP Parser
 *
 * June 2001, D. Renee Revis
 *
 * Copyright (c) 2001, 2002 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#define DEFINE_MESSAGES TRUE
#include "../../osal/os_al.h"

facdef(SDP);
msgdef_section("Session Description Protocol (SDP) error messages.");

msgdef_limit(CONFIG_PTR_ERROR, SDP, LOG_ERR, MSG_TRACEBACK,
    "Received invalid config pointer from application. Unable to process.",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "The SDP library application has an invalid configuration pointer. "
    "The SDP library is therefore unable to handle the request that it "
    "received.  The configuration of the application will not be as "
    "expected and SDP parsing errors may result, even for valid SDPs. ");
msgdef_recommended_action(LOG_STD_ACTION);
msgdef_ddts_component("voice-sdp");
msgdef_tac_details(
    "This error indicates a problem with the application attempting to "
    "use the SDP library.  It may indicate that memory has been corrupted. "
    "Write up a DDTS with the stack trace information and any available "
    "information for reproducing the problem. Also note which voice "
    "applications are configured in the router that use SDP - e.g., MGCP, "
    "SIP, etc. ");

msgdef_limit(SDP_PTR_ERROR, SDP, LOG_ERR, MSG_TRACEBACK,
    "Received invalid SDP pointer from application. Unable to process.",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "The SDP library application has an invalid SDP pointer.  The SDP "
    "library is therefore unable to handle the request that it received. "
    "SDP parsing or build errors may result. ");
msgdef_recommended_action(LOG_STD_ACTION);
msgdef_ddts_component("voice-sdp");
msgdef_tac_details(
    "This error indicates a problem with the application attempting to "
    "use the SDP library.  It may indicate that memory has been corrupted. "
    "Write up a DDTS with the stack trace information and any available "
    "information for reproducing the problem. Also note any currently "
    "active voice applications that use SDP - e.g., MGCP, SIP, etc. ");

msgdef_limit(INTERNAL, SDP, LOG_ERR, MSG_TRACEBACK,
             "%s", MSGDEF_LIMIT_FAST);
msgdef_explanation(
    "An internal software error has occurred.");
msgdef_recommended_action(LOG_STD_SH_TECH_ACTION);
msgdef_ddts_component("voice-sdp");
msgdef_tac_details(
    "This is a catch-all message for any internal code condition that "
    "should never be seen in the field (e.g. the default of a case "
    "statement that should have all valid cases defined)."
    LOG_STD_DDTS_TAC_DETAILS);



