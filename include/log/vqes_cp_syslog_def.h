/********************************************************************
 * vqes_cp_syslog_def.h
 *
 * This file defines the SYSLOG messages for the VQE-S Control Plane.
 *
 * Copyright (c) 2007-2011 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQES_CP_SYSLOG_DEF_H_
#define _VQES_CP_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

#define LOG_VQES_CP_ANY_SERVICE  "Error Repair or Rapid Channel Change"
#define LOG_VQES_CP_ALL_SERVICES "Error Repair and Rapid Channel Change"

syslog_facilitydef(VQENAME_VQES_CP, LOG_VQES_CP, LOG_ERR);

/* Alert messages */

/* Critical messages */
syslog_msgdef(
    LOG_VQES_CP, CP_MISSING_CFG_CRIT, LOG_CRIT, 
    "The channel configuration file for Control Plane process has not been "
    "specified.");
msgdef_explanation(
    "No channel configuration filename is provided and the Control Plane "
    "process will not start.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);


syslog_msgdef(
    LOG_VQES_CP, CP_INIT_FAILURE_CRIT, LOG_CRIT, 
    "Control Plane initialization failed due to %s");
msgdef_explanation(
    "A software error has occurred during the initialization of Control Plane "
    "process and the process will not start.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_CONTACT);


/* Error messages */
syslog_msgdef(
    LOG_VQES_CP, CP_MALLOC_ERR, LOG_ERR, 
    "Control Plane failed to allocate necessary memory.");
msgdef_explanation(
    "A software error has occurred during the allocation of memory and the "
    "Control Plane process will not start.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, CP_SHUTDOWN_ERR, LOG_ERR, 
    "Control Plane shutdown failed due to %s");
msgdef_explanation(
    "A software error has occurred during the shutdown of Control Plane "
    "process.");
msgdef_recommended_action(
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, CP_CHANNEL_CREATE_ERR, LOG_ERR,
    "Failed to create a channel due to %s");
msgdef_explanation(
    "A software error has occurred during the creation of a channel and the "
    "channel will not be created.");
msgdef_recommended_action(
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, CP_CHANNEL_DELETE_ERR, LOG_ERR,
    "Failed to delete a channel due to %s");
msgdef_explanation(
    "A software error has occurred during the deletion of a channel.");
msgdef_recommended_action(
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, CP_CHANNEL_UPDATE_ERR, LOG_ERR,
    "Failed to update the channel configuration due to %s");
msgdef_explanation(
    "A software error has occurred during the update of channel configuration "
    "and the channel configuration has not been updated.");
msgdef_recommended_action(
    SYSLOGDOC_CHG_RESEND_CHANNELS
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, CP_SOCKET_OPERATION_ERR, LOG_ERR,
    "Failed to %s on socket with error %s.");
msgdef_explanation(
    "A software error has occurred during the socket operation.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, CP_INTERFACE_ERR, LOG_ERR,
    "Failed to find the interface for %s.");
msgdef_explanation(
    "A software error has occurred during finding interface by name.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_CONTACT);

/* Warning messages */
syslog_msgdef(
    LOG_VQES_CP, CP_XMLRPC_REMOTE_WARN, LOG_WARNING,
    "CAUTION: INSECURE REMOTE ACCESS TO XML-RPC FUNCTIONS ENABLED");
msgdef_explanation(
    "The remote-xmlrpc command line switch is active,"
    " allowing insecure remote access to the xml-rpc server.");
msgdef_recommended_action(
    "Remove the remote-xmlrpc parameter from the vcdb.conf file and apply the "
    "config.");

syslog_msgdef(
    LOG_VQES_CP, CP_REACH_MAX_WARN, LOG_WARNING,
    "The Control Plane has exceeded the maximum allowable size [%d channels].");
msgdef_explanation(
    "The Control Plane has exceeded the indicated maximum size.");
msgdef_recommended_action(
    "Reduce the number of channels provisioned.");

syslog_msgdef(
    LOG_VQES_CP, CP_REMOVE_CHN_WARN, LOG_WARNING,
    "The Control Plane has removed the channel - %s");
msgdef_explanation(
    "The Control Plane has removed a channel based on the channel "
    "configuration.");
msgdef_recommended_action(
    SYSLOGDOC_NO_ACTION);

syslog_msgdef(
    LOG_VQES_CP, CP_VQM_ADDRESS_WARN, LOG_WARNING, 
    "'vqm-address' is deprecated, please replace with 'vqm-host' in VQE-S"
    "config file");
msgdef_explanation(
    "A deprecated argument was used in the command line.");
msgdef_recommended_action(
    SYSLOGDOC_CHG_CONF);

syslog_msgdef(
    LOG_VQES_CP, CP_VQM_HOST_NAME_WARN, LOG_WARNING, 
    "VQM host configuration appears twice in config file -- ignoring second "
    "'vqm-address =' or 'vqm-host =' line in VQE-S config file");
msgdef_explanation(
    "Multiple entries for the same option are found in the command line.");
msgdef_recommended_action(
    SYSLOGDOC_NO_ACTION);

syslog_msgdef(
    LOG_VQES_CP, CP_INVALID_OPTION_WARN, LOG_WARNING, 
    "Illegal command line option found: %s is ignored.");
msgdef_explanation(
    "An illegal command line option has been detected in the "
    "vqes_control_plane section of the .vqes.conf file.");
msgdef_recommended_action("Investigate and fix any error in .vqes.conf."
    SYSLOGDOC_CHG_CONF);

syslog_msgdef(
    LOG_VQES_CP, CP_DEPRECATED_OPTION_WARN, LOG_WARNING, 
    "Deprecated command line option found: %s.");
msgdef_explanation(
    "A deprecated command line option is found in the "
    "vqes_control_plane section of the .vqes.conf file.");
msgdef_recommended_action(
    SYSLOGDOC_CHG_CONF);

syslog_msgdef(
    LOG_VQES_CP, CP_INIT_WARN, LOG_WARNING, 
    "Control Plane failed to %s during the initialization.");
msgdef_explanation(
    "A software error has occurred during the initialization of Control Plane "
    "module.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, CP_BUFFER_SIZE_WARN, LOG_WARNING,
    "Input stream cache size is equal to zero for channel %s.");
msgdef_explanation(
    "Internal computation has resulted a zero-size buffer.");
msgdef_recommended_action(
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_CONTACT);



syslog_msgdef(
    LOG_VQES_CP, CP_INIT_COMPLETE_INFO, LOG_INFO,
    "Control Plane initialization has completed successfully.");
msgdef_explanation(
    "This is an informational message.");
msgdef_recommended_action(
    SYSLOGDOC_INFO_ONLY);

syslog_msgdef(
    LOG_VQES_CP, CP_UPDATE_COMPLETE_INFO, LOG_INFO,
    "Control Plane has successfully updated the channel configuration.");
msgdef_explanation(
    "This is an informational message.");
msgdef_recommended_action(
SYSLOGDOC_INFO_ONLY);

syslog_msgdef(
    LOG_VQES_CP, CP_UPDATE_START_INFO, LOG_INFO,
    "Control Plane has received a channel update request.");
msgdef_explanation(
    "This is an informational message.");
msgdef_recommended_action(
    SYSLOGDOC_INFO_ONLY);

syslog_msgdef(
    LOG_VQES_CP, CP_UPDATE_INFO, LOG_INFO,
    "Control Plane is updating a channel - %s");
msgdef_explanation(
    "This is an informational message.");
msgdef_recommended_action(
    SYSLOGDOC_NO_ACTION);

syslog_msgdef(
    LOG_VQES_CP, CP_OBSOLETED_OPTION_INFO, LOG_INFO, 
    "Obsoleted command line option found: %s.");
msgdef_explanation(
    "A obsoleted command line option is found in the "
    "vqes_control_plane section of the .vqes.conf file.");
msgdef_recommended_action(
    SYSLOGDOC_NO_ACTION);


/* ERA component critical messages */
syslog_msgdef(
    LOG_VQES_CP, CP_INTERNAL_FORCED_EXIT, LOG_CRIT, 
    "The Control Plane process is exiting due to an unexpected internal error "
    "condition, resulting from a problem with the software (reason: %s).");
msgdef_explanation(
    "The Control Plane process has encountered an unexpected, internal error "
    "condition, and must exit.  The VQE-S Process Monitor will normally "
    "attempt to restart the Control Plane process along with any other "
    "affected processes. However, if the condition should recur, the VQE-S "
    "may eventually be left in a non-operational state. The text of the error "
    "message indicates a more specific reason for the failure.  In all cases, "
    "this error condition results from a flaw in the software.");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT);


/* Warning for configuration conflicts at startup time, hence no rate limit */
syslog_msgdef(
    LOG_VQES_CP, CP_CONFIG_CONFLICT_NO_RL, LOG_WARNING,
    "Inconsistent configuration: %s");
msgdef_explanation(
    "Configuration conflict is found at startup time. For example, when RCC "
    "is disabled on VQE-S but enabled on a channel, or when VQE-S is set to "
    "use Aggressive RCC Mode but ER is disabled on a channel.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF)

/* Warning for configuration conflicts at run time, hence rate limited */
syslog_msgdef_limit(
    LOG_VQES_CP, CP_CONFIG_CONFLICT_SLOW, LOG_WARNING,
    "Inconsistent configuration: %s",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "Configuration conflict is found. For example, when VQE-S is in "
    "Aggressive RCC Mode but ER is disabled on a channel; or when "
    "max-client-bw is set such than the calculated e-factor is lower than "
    "the minimum value for a channel");
msgdef_recommended_action(
    "If this condition persists, restart the affected channel or restart "
    "the VQE-S application.");


/* ERA component error messages */
syslog_msgdef_limit(
    LOG_VQES_CP, ERA_MALLOC_FAILURE, LOG_ERR,
    "ERA component malloc failure: %s", MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "Control Plane ERA component memory allocation failure");
msgdef_recommended_action(
    SYSLOGDOC_REPORT
    "Correct the error (if possible). "
    SYSLOGDOC_RESTART_VQES
    "If this condition persists, reboot the VQE-S server.");

syslog_msgdef(
    LOG_VQES_CP, ERA_INIT_FAILURE, LOG_ERR,
    "ERA component initialization failure: %s");
msgdef_explanation(
    "Control Plane ERA component failed initialization");
msgdef_recommended_action(
    SYSLOGDOC_REPORT
    "Correct the error (if possible). "
    SYSLOGDOC_RESTART_VQES
    "If this condition persists, reboot the VQE-S server.");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_DP_API_FAILURE, LOG_ERR,
    "Failed to %s: return code=%d",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "A Data Plane API call has returned error.");
msgdef_recommended_action(
    SYSLOGDOC_REPORT
    "Correct the error (if possible). "
    SYSLOGDOC_RESTART_VQES
    "If this condition persists, reboot the VQE-S server.");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_RCC_INT_ERROR, LOG_ERR,
    "ERA internal error during an RCC: %s",
    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation(
    "An internal error occurs during an RCC processing.");
msgdef_recommended_action(
    SYSLOGDOC_REPORT
    "Correct the error (if possible). "
    SYSLOGDOC_RESTART_VQES
    "If this condition persists, reboot the VQE-S server.");

syslog_msgdef(
    LOG_VQES_CP, ERA_SOCK_OP_ERROR, LOG_ERR,
    "Socket operation error: %s");
msgdef_explanation(
    "Control Plane ERA component has encountered an error while doing "
    "socket operation.");
msgdef_recommended_action(
    SYSLOGDOC_REPORT
    "Correct the error (if possible). "
    SYSLOGDOC_RESTART_VQES
    "If this condition persists, reboot the VQE-S server.");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_INVALID_RTCP_PKT, LOG_ERR,
    "Invalid RTCP packet received from %s - %s",
    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation(
    "Control Plane ERA component has detected that it received "
    "an invalid RTCP packet. The invalid RTCP packet is "
    "dropped and requests for " LOG_VQES_CP_ANY_SERVICE " are dropped if "
    "the dropped packet contains " LOG_VQES_CP_ANY_SERVICE " requests.");
msgdef_recommended_action(
    "If this message recurs, use a packet sniffing tool (e.g., tshark or "
    "tcpdump) to capture RTCP packets from the client indicated in the error "
    "message." SYSLOGDOC_CONTACT);

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_INVALID_RTCP_MSG, LOG_ERR,
    "Invalid RTCP %s message received from %s",
    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation(
    "Control Plane ERA component has detected that it received an invalid "
    "RTCP message within a RTCP compound packet. The ERA component will drop "
    "the invalid message. Depending on the message type, a request for "
    LOG_VQES_CP_ANY_SERVICE "is ignored, or possible RTP/RTCP retransmission "
    "port changes are ignored (for PUBPORTS message)");
msgdef_recommended_action(
    "If this message recurs, use a packet sniffing tool (e.g., tshark or "
    "tcpdump) to capture RTCP packets from the client indicated in the error "
    "message." SYSLOGDOC_CONTACT);


syslog_msgdef_limit(
    LOG_VQES_CP, ERA_SEND_PKT_FAILURE, LOG_ERR,
    "Failed to send a %s packet to %s",
    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation(
    "Control Plane ERA component failed to send a packet to a client. The "
    "RTCP NULL APP packet indicates that Rapid Channel Change (RCC) is not "
    "feasible at this time. Failure to send a NULL APP packet means the "
    "client will have to wait for some extra amount of time, i.e. APP timeout,"
    " before it starts a normal channel change. Failure to send a RTCP APP "
    "packet during RCC processing means the client will not be able to "
    "perform an RCC; it will perform a normal channel change instead, "
    "after the APP timeout interval.");
msgdef_recommended_action(
    "If this condition persists, restart the VQE-S application. If this "
    "condition persists after restart, reboot the VQE-S server.");


syslog_msgdef_limit(
    LOG_VQES_CP, CP_CHAN_FSM_ERROR, LOG_ERR,
    "Per-channel FSM error: %s", 
    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation(
    "This message logs the errors that occur inside the Control Plane "
    "per-channel Finite State Machine. The errors will most likely leave a "
    "channel in a state not consistent with the current RTP source state.");
msgdef_recommended_action(
    "If this condition persists, restart the affected channel or restart "
    "the VQE-S application.");


syslog_msgdef_limit(
    LOG_VQES_CP, CP_CHAN_FSM_INVALID_EVENT, LOG_ERR,
    "Per-channel FSM has received an invalid event: %s",
    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation(
    "This message log the errors that an invalid event is received in the "
    "Control Plane per-channel Finite State Machine. The invalid event is "
    "ignored.");
msgdef_recommended_action(
    "If this condition persists, restart the affected channel or restart the "
    "VQE-S application.");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_RTCP_SESSION_CREATION_FAILURE, LOG_ERR,
    "RTCP session creation failure: %s",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "ERA component failed to create a RTCP session.");
msgdef_recommended_action(
    SYSLOGDOC_RECUR_RESTART_VQES
    SYSLOGDOC_REPORT_COPY_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, ERA_INVALID_PARAMETER, LOG_ERR,
    "User specified parameter not valid: %s");
msgdef_explanation(
    "A user-specified ERA parameter is not valid. ERA component maintains a "
    "default value for each parameter. If the specified value is invalid, the "
    "default value will be used instead.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(
    LOG_VQES_CP, ERA_DATA_ERROR, LOG_ERR,
    "Internal data error: %s");
msgdef_explanation(
    "Internal ERA data error is detected. This error message is logged when a "
    "data error does not warrant an assertion. For example during stats "
    "gathering if a stat structure is null, we will give up on providing "
    "counters by logging this message and continue.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_REPORT
    SYSLOGDOC_CONTACT);


syslog_msgdef_limit(
    LOG_VQES_CP, ERA_RCC_ERROR, LOG_ERR,
    "Rapid Channel Change error: %s",
    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation(
    "Error occurred in Control Plane ERA component while servicing a "
    "Rapid Channel Change request.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_REPORT
    SYSLOGDOC_CONTACT);


syslog_msgdef_limit(
    LOG_VQES_CP, ERA_RTCP_CB_ERROR, LOG_ERR,
    "RTCP callback function failed to %s: %s",
    MSGDEF_LIMIT_GLACIAL);
msgdef_explanation(
    "Error returned from a RTCP utility callback function when trying to add "
    "or lookup a member within a RTCP session. The occurrence of this error "
    "normally suggests that a repair session (mapped one-to-one to a member) "
    "is not available and that " LOG_VQES_CP_ANY_SERVICE "functionality is " 
    "not available for the client.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_REPORT
    SYSLOGDOC_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, ERA_LIBEVENT_ERROR, LOG_ERR,
    "libevent function error: %s");
msgdef_explanation(
    "Error returned from a libevent function. Control Plane makes use of "
    "libevent framework to dispatch socket and timer events. This low level "
    "libevent error normally means the packet receiving and timer "
    "functionality would not work across the channels. In turn, "
    LOG_VQES_CP_ANY_SERVICE "would not work across the channels.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_REPORT
    SYSLOGDOC_CONTACT);

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_RTCP_SEND_SOCK_ERROR, LOG_ERR,
    "Failed to create a socket to send RTCP packets from %s:%d- %s",
    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation(
    "This message is logged when ERA component fails to create a socket to "
    "send RTCP packets. When this error occurs to a channel, the RTCP session "
    "for channel will not be setup to perform " LOG_VQES_CP_ANY_SERVICE ".");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_REPORT
    SYSLOGDOC_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, ERA_RECV_SOCK_SETUP_FAILURE, LOG_ERR,
    "Failed to create a receiving libevent socket on %s:%d- %s");
msgdef_explanation(
    "This message is logged when the ERA component fails to create a "
    "receiving libevent socket. There are different RTCP receiving sockets: "
    "one is to receive upstream RTCP packets, one is to receive downstream "
    "RTCP pkts on the primary RTCP session, and the other is to receive "
    "downstream RTCP packets on the repair sessions. Failure to create any "
    "one of above receiving sockets will result in no RTCP session being "
    "created for a channel and there will be no " LOG_VQES_CP_ANY_SERVICE 
    " service for the channel.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_REPORT
    SYSLOGDOC_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, ERA_TIMER_FAILURE, LOG_ERR,
    "Failed to create the ERA timer: %s");
msgdef_explanation(
    "This message is logged when the ERA component fails to create the ERA "
    "timer. The ERA Timer does the Data Plane upcall event generation number "
    "checking and Error Repair rate sampling and calculation. When this timer "
    "fails, except for the above functionality, " LOG_VQES_CP_ALL_SERVICES
    " will still be functional for all channels.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_REPORT
    SYSLOGDOC_CONTACT);

syslog_msgdef(
    LOG_VQES_CP, ERA_RTCP_TIMER_FAILURE, LOG_ERR,
    "Failed to create a RTCP timer: %s");
msgdef_explanation(
    "This message is logged when the ERA component fails to create a RTCP "
    "timer. When a RTCP timer fails to setup during channel creation, the "
    "channel RTCP session will not be created. And there will be no " 
    LOG_VQES_CP_ANY_SERVICE " functionality available for the channel.");
msgdef_recommended_action(
    SYSLOGDOC_RESTART_VQES_CHANNELS
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_REPORT
    SYSLOGDOC_CONTACT);

syslog_msgdef(
   LOG_VQES_CP, BWC_INIT_FAILURE, LOG_ERR,
   "Bandwidth Broker Client initialization failure: %s");
msgdef_explanation(
   "Control Plane Bandwidth Broker Client failed initialization");
msgdef_recommended_action(
   SYSLOGDOC_REPORT
   SYSLOGDOC_CONTACT
   "Correct the error (if possible). "
   SYSLOGDOC_RESTART_VQES);

syslog_msgdef(
   LOG_VQES_CP, BWC_DUP_INIT, LOG_ERR,
   "Duplicate initialization of BwB Client.");
msgdef_explanation(
   "Control Plane BwB Client initialization routine"
   " had been called more than once; duplicate calls"
   " are ignored.");
msgdef_recommended_action(
   SYSLOGDOC_COPY
   SYSLOGDOC_REPORT
   SYSLOGDOC_CONTACT
   SYSLOGDOC_RECUR_RESTART_VQES);

/* ERA component warning level messages */
syslog_msgdef_limit(
    LOG_VQES_CP, ERA_WARNING, LOG_WARNING,
    "ERA warning message: %s",
    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation(
    "This is a warning message from the Control Plane ERA component. "
    LOG_VQES_CP_ANY_SERVICE " service is not compromised when this "
    "condition occurs.");
msgdef_recommended_action(
    SYSLOGDOC_RECUR_COPY
    SYSLOGDOC_REPORT
    SYSLOGDOC_CONTACT);

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_ADD_BITMAP_RB_FAILED, LOG_WARNING,
    "Failed to add bitmap repair burst for client %s on channel %s:%d - "
    "return code=%d",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "Error reported from the Data Plane that an Error Repair repair burst is "
    "not able to be scheduled. This would normally happen when the number of "
    "active ER requests is larger than a value (20) set on the system, which "
    "suggests that the packet drop on the client access link is very bursty.");
msgdef_recommended_action(
    "If this occurs frequently, you many want to contact your Cisco technical "
    "support representative.");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_REFUSE_RCC_REQUEST_INVALID_E, LOG_WARNING,
    "No excess bandwidth available for client %s on channel %s:%d: refuse "
    "the RCC request",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "RCC request from a STB client is refused since no excess bandwidth "
    "is available for the client.");
msgdef_recommended_action(
    "Make sure the excess bandwidth fraction (also known as e-factor) is "
    "configured properly on VQE-S. If max-client-bw is configured on VQE-S, "
    "make sure that it is at least 5% over the highest bit rate of the "
    "VQE-S channels.");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_DROP_ER_REQUEST_INVALID_E, LOG_WARNING,
    "No excess bandwidth available for client %s on channel %s:%d: dropped %d "
    "bitmaps and %d repair packets",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "Error Repair requests from a STB client are dropped since no excess "
    "bandwidth is available for the client.");
msgdef_recommended_action(
    "Make sure the excess bandwidth fraction (also known as e-factor) is "
    "configured properly on VQE-S. If max-client-bw is configured on VQE-S, "
    "make sure that it is at least 5% over the highest bit rate of the "
    "VQE-S channels.");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_CLIENT_PKT_TB_RATE_LIMITED, LOG_WARNING,
    "Per-client packet policer has dropped %d repair packets (%d bitmaps) "
    "for client %s on channel %s:%d",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "Error Repair requests from a STB client are rate limited by the "
    "per-client packet policer whose parameters are configurable via "
    "the VQE-S configuration file.");
msgdef_recommended_action(
    "Understand the STB packet drop pattern and tune the per-client policer "
    "parameter values if necessary.");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_GLOBAL_PKT_TB_RATE_LIMITED, LOG_WARNING,
    "Global packet policer has dropped %d repair packets (%d bitmaps) "
    "for client %s on channel %s:%d",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "The aggregated ER packet request rate has exceeded the limiting rate "
    "set for the global ER packet policer, which is set at 50K pkts/second.");
msgdef_recommended_action(
    "Reduce the aggregated ER packet request rate at the VQE Server, "
    "for example, by adding VQE Servers to distribute the ER request load. "
    "You many want to contact your Cisco technical support representative to "
    "discuss the solution.");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_GLOBAL_BITMAP_TB_RATE_LIMITED, LOG_WARNING,
    "Global bitmap policer has dropped %d ER bitmaps (%d repair packets) "
    "for client %s on channel %s:%d",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation(
    "The aggregated ER bitmap request rate has exceeded the limiting rate "
    "set for the global ER bitmap policer, which is set at 10K bitmaps/second.");
msgdef_recommended_action(
    "Reduce the aggregated ER bitmap request rate at the VQE Server, "
    "for example, by adding VQE Servers to distribute the ER request load. "
    "You many want to contact your Cisco technical support representative to "
    "discuss the solution.");

syslog_msgdef(
    LOG_VQES_CP, ERA_CHAN_STATE_CHANGED, LOG_WARNING,
    "Changed state to %s for channel %s: %s");
msgdef_explanation(
    "This message indicates the state for a channel has changed, either from "
    "inactive to active or from active to inactive. A channel will become "
    "inactive when the channel source is no longer available. An inactive "
    "channel is not able to provide " LOG_VQES_CP_ANY_SERVICE " service.");
msgdef_recommended_action(
    "If the channel source state change is expected (e.g., the channel is "
    "'inactive' because the headend is no longer distributing the channel "
    "content), then no action is required. If the state change is unexpected, "
    "use the VQE-S Application Monitoring Tool to check the status of the "
    "channel input stream. If the channel status is not as expected, reset "
    "the channel, or restart the VQE-S application. If the channel status "
    "remains unexpected, issue the <b>vqereport</b> command to gather data "
    "the may help identify the nature of the error."
    SYSLOGDOC_CONTACT);


syslog_msgdef(
    LOG_VQES_CP, CP_CHAN_FSM_HANDLE_MISMATCH, LOG_WARNING,
    "Per-channel FSM handle mismatch warning: %s");
msgdef_explanation(
    "This message indicates a potential issue inside the Control Plane "
    "per-channel Finite State Machine. It is expected and harmless if seen "
    "shortly after the VQE-S channel configuration file has been modified. "
    "However, if no such change has occurred recently, it may indicate "
    "that the channel is not operating correctly.");
msgdef_recommended_action(
    "If this condition persists, reset the affected channel or restart the "
    "VQE-S application. Contact your technical support representative "
    "and supply the text of this syslog message.");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_ER_CAPACITY_REACHED, LOG_WARNING,
    "An Error Repair request was rejected because the VQE-S does not have the "
    "capacity to service the request",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation("The VQE-S cannot service an Error Repair request because "
                   "a capacity limit has been reached.");
msgdef_recommended_action("Consider adding additional VQE-S capacity");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_RCC_CAPACITY_REACHED, LOG_WARNING,
    "A Rapid Channel Change request was rejected because the VQE-S does not "
    "have the capacity to service the request",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation("The VQE-S cannot service a Rapid Channel Change request "
                   "because a capacity limit has been reached.");
msgdef_recommended_action("Consider connecting and activating any Ethernet "
                          "interfaces that are not yet in use. "
                          "Consider adding additional VQE-S capacity");

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_RTCP_MAX_CLIENT, LOG_WARNING,
    "The maximum number of clients for a single VQE Server has been reached. "
    "ER and RCC requests coming from the excess clients will be ignored.",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation("The VQE-S cannot service an Error Repair or Rapid Channel "
                   "Change request because a capacity limit has been "
                   "reached.");
msgdef_recommended_action("Consider adding additional VQE-S capacity");


syslog_msgdef_limit(
    LOG_VQES_CP, ERA_LONG_RCC, LOG_WARNING,
    "Rapid Channel Change Time of %u ms reported from client %s",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation("A VQE client reported an channel change time "
                   "exceeding reasonable estimates");
msgdef_recommended_action("Contact your technical support representative "
                          "and supply the text of this syslog message.");

/* ERA component info level messages */
syslog_msgdef_limit(
    LOG_VQES_CP, ERA_INFO, LOG_INFO,
    "ERA informational message: %s",
    MSGDEF_LIMIT_MEDIUM);
msgdef_explanation(
    "This is an informational message from the ERA component.");
msgdef_recommended_action(
    SYSLOGDOC_INFO_ONLY);

syslog_msgdef_limit(
    LOG_VQES_CP, ERA_OUTBW_MGMT, LOG_INFO,
    "Output bandwidth manager information: %s",
    MSGDEF_LIMIT_SLOW);
msgdef_explanation("This message provides information about unusual, but normal conditions"
                   " in the output bandwidth manager.");
msgdef_recommended_action(
   SYSLOGDOC_INFO_ONLY);


#endif /* _VQES_CP_SYSLOG_DEF_H_ */
