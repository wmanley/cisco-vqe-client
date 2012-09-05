/********************************************************************
 * vqes_mlb_syslog_def.h
 *
 * This file defines the SYSLOG messages for
 * VQE-S Multicast Load Balancer.
 *
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQES_MLB_SYSLOG_DEF_H_
#define _VQES_MLB_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_VQES_MLB, LOG_VQES_MLB, LOG_ERR);

syslog_msgdef(LOG_VQES_MLB, MLB_INIT_FAILURE, LOG_CRIT, 
              "MLB initialization has failed: %s. MLB exit.");
msgdef_explanation("Multicast Load Balancer process initialization has failed."
                   " The process is terminated and need to be investigated.");
msgdef_recommended_action("Process Monitor will automatically try to start "
                          "MLB again. If this message recurs, copy the error "
                          "message exactly as it appears in the VQE-S system "
                          "log. Issue the <b>vqereport</b> command to gather "
                          "data that may help identify the nature of the error."
                          " If you cannot determine the nature of the error "
                          "from the error message text or from the "
                          "<b>vqereport</b> command output, contact your Cisco "
                          "technical support representative, and provide the "
                          "representative with the gathered information.");

syslog_msgdef(LOG_VQES_MLB, MLB_SOCK_FAILURE, LOG_CRIT, 
              "MLB socket binding has failed: %s.");
msgdef_explanation("Multicast Load Balancer process initialization has failed"
                   " due to socket binding failure. The process is terminated "
                   "and need to be investigated.");
msgdef_recommended_action("Process Monitor will automatically try to start "
                          "MLB again. If this message recurs, try remove file "
                          "/var/tmp/.mlb_rpc as root and start vqe-s again. If "
                          "the problem is still there, copy the error message "
                          "exactly as it appears in the VQE-S system log. "
                          "Issue the <b>vqereport</b> command to gather data "
                          "that may help identify the nature of the error. If "
                          "you cannot determine the nature of the error from "
                          "the error message text or from the <b>vqereport</b>"
                          " command output, contact your Cisco technical "
                          "support representative, and provide the "
                          "representative with the gathered information.");

syslog_msgdef(LOG_VQES_MLB, MLB_MALLOC_FAILURE, LOG_ERR, ": %s");
msgdef_explanation("Multicast Load Balancer process failed to allocate "
                   "required memory for the reason specified in the error "
                   "message.");
msgdef_recommended_action(SYSLOGDOC_REPORT
                          "Check available system memory."
                          SYSLOGDOC_RESTART_VQES
                          "If the condition persists after VQE-S restart, "
                          SYSLOGDOC_REBOOT_SERVER);

syslog_msgdef(LOG_VQES_MLB, MLB_IP_CHANGE, LOG_WARNING,
              "Detected IP address change from %s to %s on interface %s");
msgdef_explanation("MLB detected an IP address change on one interface.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_VQES_MLB, MLB_NO_IP, LOG_ERR,
              "Failed to retrieve IP address of interface %s");
msgdef_explanation("MLB failed to retrieve an IP address for one interface, " 
                   "and marked it as inactive. MLB will monitor the status of "
                   "this interface and use it when it becomes active.");
msgdef_recommended_action("Refer to the \"Configuring VQE-S\" appendix of the "
                          SYSLOGDOC_USER_GUIDE ". If the interface was not "
                          "intended to be used by the VQE-S application, "
                          "remove its interface name from the "
                          "vqe.vqes.vqe_interfaces option in the vcdb.conf "
                          "file. Otherwise, refer to the " SYSLOGDOC_USER_GUIDE
                          " for information on configuring Ethernet interfaces."
                          " Check the configuration of the "
                          "interface and its status. If not as expected, "
                          "modify the interface configuration and/or bring "
                          "up the interface.");

syslog_msgdef(LOG_VQES_MLB, MLB_SOCK_INIT_FAILURE, LOG_ERR,
              "Failed to create MLB mcast socket for interface %s");
msgdef_explanation("MLB multicast socket initialization failed for an "
                   "interface; as a result it has been marked inactive. "
                   "MLB will automatically retry it later.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          "If the condition persists after VQE-S restart, "
                          SYSLOGDOC_REBOOT_SERVER);

syslog_msgdef(LOG_VQES_MLB, MLB_INACTIVE_INTF, LOG_ERR,
              "Interface \"%s\" is not up or not running.");
msgdef_explanation("Detected a network interface that is not up or not running."
                   " It will be ignored and used later when it is ready.");
msgdef_recommended_action("If the interface was not intended to be used by the"
                          " VQE-S application, "
                          "remove its interface name from the vcdb.conf file."
                          SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_MISSING_BOND_INTF, LOG_ERR,
              "Bond interface for member \"%s\" not found.");
msgdef_explanation("Detected a member interface that is not associated with a "
                   "bond. It will be ignored and used later when it is ready.");
msgdef_recommended_action("If the interface was not intended to be used by the"
                          " VQE-S application, "
                          "remove its interface name from the vcdb.conf file."
                          SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_NO_BANDWIDTH, LOG_ERR,
              "No interface has enough bandwidth to handle request to join "
              "%s/%s, it will be retried by MLB when bandwidth is available.");
msgdef_explanation("MLB does not have enough bandwidth to handle a join "
                   "request, the request has been queued for retry.");
msgdef_recommended_action("MLB cannot find an interface with enough available "
                          "bandwidth to service a new channel. If this is "
                          "due to one or more interface failures, restore the "
                          "failed interfaces. If interface status is as "
                          "expected, refer to the " SYSLOGDOC_USER_GUIDE " for "
                          "information on configuring Ethernet interfaces. "
                          "Check the interface configuration and "
                          "interface status of the VQE server. If not as "
                          "expected, modify the interface configuration and "
                          "bring up the interfaces. If the interface "
                          "configuration and status were as expected, check "
                          "the channel configuration for this server, to "
                          "verify that the total bandwidth of all channels "
                          "does not exceed the expected interface capacity. "
                          "If necessary, modify the channel configuration, and"
                          " resend it to the VQE-S and/or VCDS servers.");

syslog_msgdef(LOG_VQES_MLB, MLB_JOIN_MULTICAST, LOG_INFO,
              "Joined multicast group %s/%s on interface %i: %s");
msgdef_explanation("MLB joined new multicast membership.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_VQES_MLB, MLB_NEG_BANDWIDTH, LOG_ERR,
              "Invalid parameter: bandwidth must be a positive integer for "
              "multicast join requests, this request will be abandoned.");
msgdef_explanation("MLB requires a positive bandwith for multicast join "
                   "request. Invalid request will be abandoned. ");
msgdef_recommended_action(SYSLOGDOC_CHG_RESEND_CHANNELS);

syslog_msgdef(LOG_VQES_MLB, MLB_CORRUPT_DATA, LOG_CRIT,
              "MLB detected corrupted data structure: %s");
msgdef_explanation("MLB detected corrupted data. It should be restarted.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_MLB, MLB_CLIENT_OVERFLOW, LOG_CRIT,
              "MLB client table is full.");
msgdef_explanation("MLB has reached its limit on number of clients connected. "
                   "No new client will be able to connect to MLB before "
                   "some existing client disconnects. This condition should "
                   "never occur during normal operation.");
msgdef_recommended_action(SYSLOGDOC_REPORT
                          SYSLOGDOC_CONTACT
                          SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_MLB, MLB_IGMP_JOIN_ERR, LOG_ERR,
              "Failed to join multicast group %s on interface %s: %s");
msgdef_explanation("Failed to join a multicast group. MLB is ignoring the "
                   "error but this condition is abnormal.");
msgdef_recommended_action("Verify that the multicast address is valid and in "
                          "use. Verify that the configuration of the indicated"
                          " interface is correct. If corrections have been "
                          "made, restart the affected channel."
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_REPORT
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_VQES_MLB, MLB_IGMP_LEAVE_ERR, LOG_ERR,
              "Failed to leave multicast group %s on interface %s: %s");
msgdef_explanation("Failed to leave a multicast group. MLB is ignoring the "
                   "error but this condition is abnormal.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_REPORT_COPY_CONTACT);

syslog_msgdef(LOG_VQES_MLB, MLB_RECORD_NOT_FOUND, LOG_ERR,
              "Cannot find multicast group record %s for removal.");
msgdef_explanation("MLB cannot find the multicast group in the leave request, "
                   "the request will be ignored; however, this is abnormal.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_REPORT_COPY_CONTACT);

syslog_msgdef(LOG_VQES_MLB, MLB_CLIENT_DISCONNECT, LOG_WARNING,
              "Client %i disconnected unexpectedly, "
              "there might be some stale multicast memberships.");
msgdef_explanation("MLB detected an unexpected client disconnect. This should "
                   "not happen during normal VQES operation and may leave "
                   "stale multicast memberships and waste bandwidth.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_MLB, MLB_NEW_CLIENT, LOG_INFO,
              "Connected new client with socket id: %i");
msgdef_explanation("MLB accepted a new client.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_VQES_MLB, MLB_RPC_ERR, LOG_ERR,
              "MLB failed to execute an RPC %s request.");
msgdef_explanation("MLB failed to execute a client request. If the failure is "
                   "recoverable, MLB will automatically retry. Unrecoverable "
                   "failures are usually caused by wrong channel "
                   "configuration.");
msgdef_recommended_action("Verify the channel configuration."
                          SYSLOGDOC_RESTART_VQES_CHANNELS);

syslog_msgdef(LOG_VQES_MLB, MLB_RPC_UNKNOWN_CMD, LOG_ERR,
              "Unknown RPC command: %i.");
msgdef_explanation("MLB received an unknown RPC command and ignored it.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_VQES_MLB, MLB_RPC_RESPONSE_ERR, LOG_ERR,
              "MLB RPC response failed.");
msgdef_explanation("MLB failed to send RPC response back to client. This may "
                   "indicate that the client connection is abnormal.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_MLB, MLB_NIC_DOWN, LOG_ERR,
              "Network interface %s went down.");
msgdef_explanation("MLB detected an interface down event. The interface is "
                   "marked inactive and monitored.");
msgdef_recommended_action("If the 'interface down' event is unexpected, check "
                          "the interface configuration and status. Make any "
                          "needed corrections, and bring the interface up.");

syslog_msgdef(LOG_VQES_MLB, MLB_NIC_UP, LOG_ERR,
              "Network interface %s came up.");
msgdef_explanation("MLB detected an interface up event. The interface is "
              "marked active and will be used for future requests.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_VQES_MLB, NETLINK_SOCKET_INIT_ERR, LOG_CRIT,
              "Failed to initialize interface to Linux kernel: %s.");
msgdef_explanation("MLB failed to initialize interface to Linux kernel for the "
                   "specified reason. MLB will not function properly after "
                   "encountering this error.");
msgdef_recommended_action(SYSLOGDOC_REPORT_COPY_CONTACT
                          SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_MLB, NETLINK_SOCKET_BIND_ERR, LOG_ERR,
              "Failed to bind interface to Linux kernel: %s.");
msgdef_explanation("MLB failed to bind interface to Linux kernel for the "
                   "specified reason. MLB will not function properly after "
                   "encountering this error.");
msgdef_recommended_action(SYSLOGDOC_REPORT_COPY_CONTACT
                          SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_MLB, NETLINK_SOCKET_SEND_ERR, LOG_ERR,
              "Failed to send message to Linux kernel: %s.");
msgdef_explanation("MLB failed to send a message to Linux kernel for the "
                   "specified reason. MLB will not function properly after "
                   "encountering this error.");
msgdef_recommended_action(SYSLOGDOC_REPORT_COPY_CONTACT
                          SYSLOGDOC_RESTART_VQES);

syslog_msgdef(LOG_VQES_MLB, MLB_HAS_NO_INTERFACES, LOG_ERR,
              "The MLB is being run without any set interfaces.");
msgdef_explanation("The MLB should be called with a non-zero set of "
                   "interfaces listed in the VCDB ('--interface' option)");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_IFNAME_OVERFLOW, LOG_CRIT,
              "Interface names %s exceeds maximum length allowed. MLB exit.");
msgdef_explanation("MLB initialization has failed due to interface name length"
                   " exceeded maximum length allowed. MLB will abort init.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_INVALID_RESERV, LOG_CRIT,
              "Invalid unicast reservation ratio: %f. MLB exit.");
msgdef_explanation("MLB initialization has failed due to invalid unicast "
                   "reservation ratio. Operator must correct the configuration "
                   "file.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_INVALID_XMLRPC_PORT, LOG_CRIT,
              "Invalid XML RPC port: %i, MLB exit.");
msgdef_explanation("MLB initialization has failed due to invalid XMLRPC "
                   "port. Operator must correct the configuration file.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_INVALID_INTERVAL, LOG_CRIT,
              "Invalid poll interval: %i, MLB exit.");
msgdef_explanation("MLB initialization has failed due to invalid poll "
                   "interval. Operator must correct the configuration file.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_LACK_AVAIL_BW, LOG_WARNING,
              "Lack of available bandwidth: %s.");
msgdef_explanation("The available bandwidth on the physical interafce "
                   "is below the minimal required value.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_XMLPRC_ADD_ERR, LOG_CRIT,
              "MLB XMLRPC failed to add %s method to xmlrpc server thread.");
msgdef_explanation("MLB XML RPC server failed to add a method during init. "
                   "The XML remote interface will not function properly.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES
                          SYSLOGDOC_RECUR_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_VQES_MLB, MLB_LOCK_FAILURE, LOG_CRIT,
              "MLB failed to %s lock: %s.");
msgdef_explanation("MLB failed in a lock operation due to an internal "
                   "software error. It will not function properly and "
                   "must be restarted.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES);


/* ------- log messages for the MLB ethtool ----------- */

syslog_msgdef(LOG_VQES_MLB, MLB_ETHTOOL_ERR, LOG_ERR,
              "MLB link status monitoring returned error for interface %s.");
msgdef_explanation("MLB cannot get parameter for interface for the specified "
                   "reason. This will make the interface unusable.");
msgdef_recommended_action("Please check your Ethernet hardware and driver and "
                          "make sure they are working properly."
                          SYSLOGDOC_REPORT);

syslog_msgdef(LOG_VQES_MLB, MLB_ETHTOOL_UNKNOWN_SPEED, LOG_ERR,
              "MLB link status monitoring returned unknown link speed %i "
              "for interface %s.");
msgdef_explanation("MLB link status monitoring failed to recognize link speed "
                   "of an interface. The interface cannot be used by MLB.");
msgdef_recommended_action("Please check your Ethernet hardware and driver and "
                          "make sure they are working properly."
                          SYSLOGDOC_REPORT);

syslog_msgdef(LOG_VQES_MLB, MLB_ETHTOOL_UNKNOWN_DUPLEX, LOG_ERR,
              "MLB link status monitoring returned an unknown duplex "
              "status %i for %s.");
msgdef_explanation("MLB link status monitoring failed to recognize link "
                   "duplex status of an interface. It will not affect MLB "
                   "but is abnormal.");
msgdef_recommended_action("Please check your Ethernet hardware and driver and "
                          "make sure they are working properly."
                          SYSLOGDOC_REPORT);

syslog_msgdef(LOG_VQES_MLB, MLB_ETHTOOL_FULL_DUPLEX, LOG_INFO,
              "Link %s is running in full-duplex mode.");
msgdef_explanation("MLB detected link is running in full-duplex mode.");
msgdef_recommended_action(SYSLOGDOC_INFO_ONLY);

syslog_msgdef(LOG_VQES_MLB, MLB_ETHTOOL_HALF_DUPLEX, LOG_WARNING,
              "Link %s is running in half-duplex mode.");
msgdef_explanation("MLB detected link is running in half-duplex mode. This "
                   "does not affect function but does affect performance.");
msgdef_recommended_action("If half-duplex mode is not expected, check the wire "
                          "and remote port configuration."
                          SYSLOGDOC_REPORT);


/* ------- log messages for the MLB command-line ----------- */

syslog_msgdef(LOG_VQES_MLB, MLB_IFS_NAME_IS_TOO_LONG, LOG_ERR,
              "MLB interface name '%s' is too long."
              " All interface names must less than %d characters long.");
msgdef_explanation("An MLB interface name was too long, "
                   "and therefore was ignored.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_IFS_NAME_IS_REPEATED, LOG_ERR,
              "The MLB interface name '%s' already exists");
msgdef_explanation("The MLB interface name already appeared earlier in "
                   "the list, and therefore was ignored.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_IFS_NUMBER_OVERFLOW, LOG_CRIT,
              "There are too many MLB interfaces. MLB exit."
              "You cannot list more than %d interfaces.");
msgdef_explanation("The MLB interface list has overflowed. "
                   "All the interfaces beyond this point are ignored.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_DUPLICATE_COMMAND_LINE_OPTION, LOG_CRIT,
              "The MLB command-line option '-%c' has been repeated. MLB exit.");
msgdef_explanation("All the MLB command-line arguments must appear no more "
                   "than once. If an argument requires multiple values, you "
                   "must concatinate them together after a single argument.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_SKIPPED_COMMAND_LINE_ARG, LOG_ERR,
              "The MLB skipped over %d command-line string(s): %s");
msgdef_explanation("All the MLB command-line arguments must have no more "
                   "than one white-space-delimited string following them. "
                   "Make sure that if the string contains a white-space "
                   "that is it surrounded with quotation marks.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF 
                          " Please note the entire command-line. "
                          "This can be see by running: 'ps -ef | grep mlb'");

syslog_msgdef(LOG_VQES_MLB, MLB_INVALID_COMMAND_LINE_ARG, LOG_CRIT,
              "The MLB command-line argument '%s' is invalid. MLB exit.");
msgdef_explanation("All the MLB command-line arguments must be of the form "
                   "of either '-x [ARG]' or '--full-name-x [ARG]'.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_UNKNOWN_OPTION, LOG_ERR,
              "MLB ignored an option that is either unrecognized or "
              "is missing an argument: %s");
msgdef_explanation("MLB detected unrecognized option and ignored it."
                   "This error can also be caused by ending the command-line"
                   "with an option that requires an additional argument.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);


syslog_msgdef(LOG_VQES_MLB, MLB_COMMAND_LINE_ROUTING_MISCONFIG, LOG_ERR,
              "MLB command-line sets %d static candidate route(s) while using "
              "dynamic routing. Will ignore the candidate routes.");
msgdef_explanation("Static candidate routes were set, while also having "
                   "dynamic routing. The MLB therefore ignores the given "
                   "candidate routes and assumes dynamic routing.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);


syslog_msgdef(LOG_VQES_MLB, MLB_COMMAND_LINE_INTF_LIST_REDUNDANCY, LOG_WARNING,
              "MLB command-line sets both VQE traffic interface list and "
              "separate ingest/service interface lists. Will ignore the "
              "ingest and service interface lists.");
msgdef_explanation("VQE traffic interfaces were specified while also "
                   "specifying separate ingest and service interfaces. "
                   "The MLB therefore ignores the separate ingest and "
                   "service interfaces and implements the given VQE "
                   "traffic interfaces.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_COMMAND_LINE_INTF_ROLE_LIST_MISCONFIG, LOG_ERR,
              "MLB command-line sets only ingest interfaces or service "
              "interfaces, but not both. Both ingest and services interfaces "
              "are required for proper operation.");
msgdef_explanation("Only ingest interfaces or service interfaces were "
                   "configured, but both are required for proper "
                   "operation.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_COMMAND_LINE_BOND_INTF_MISCONFIG, LOG_ERR,
              "MLB command-line sets different member lists for bond interface "
              "%s. Previous member interface list was %s. New member list is "
              "%s. New member list will be ignored.");
msgdef_explanation("Any reference to the member interface list for a bond "
                   "interface must be same for all command line arguments.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_COMMAND_LINE_TWO_BOND_MISCONFIG, LOG_ERR,
              "MLB command-line sets member interface %s in both bonds "
              "%s and %s. A member interface may only belong to one bond, "
              "ignoring the second bond configuration.");
msgdef_explanation("A member interface must only belong to one bond.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_COMMAND_LINE_NONBOND_BOND_MISCONFIG, LOG_ERR,
              "MLB command-line sets interface %s as both an individual "
              "(nonbonded) interface and as a member of bond interface %s. "
              "An interface can either be bonded or nonbonded, but not both. "
              "Ignoring the bond configuration.");
msgdef_explanation("An interface can either be bonded or nonbonded, but not "
                   "both.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_COMMAND_LINE_BOND_NONBOND_MISCONFIG, LOG_ERR,
              "MLB command-line sets interface %s as both a member of bond "
              "interface %s and as an individual (nonbonded) interface. "
              "An interface can either be bonded or nonbonded, but not both. "
              "Ignoring the nonbond configuration.");
msgdef_explanation("An interface can either be bonded or nonbonded, but not "
                   "both.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_MISSING_ROUTE_CANDIDATES, LOG_WARNING,
              "MLB uses static routing but does not have any candidate "
              "routes.");
msgdef_explanation("MLB is using static routing, but does not have any "
                   "candidate routes. This is unusual, but may not be an "
                   "error.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_ROUTE_FILE_OPEN_ERROR, LOG_WARNING,
              "A problem was encountered while attempting to open the static "
              "route file '%s'. None of the static routes will be operational.");
msgdef_explanation("MLB was unable to successfully open the specified static "
                   "route file. As a result, none of the static routes will be "
                   "installed and managed.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_ROUTE_FILE_PARSE_ERROR, LOG_WARNING,
              "A problem was encountered while parsing the static route file "
              "'%s'. Some or all of the static routes may not be operational.");
msgdef_explanation("MLB was unable to successfully parse all static routes "
                   "in the specified static route file. As a result, not all "
                   "of the static routes will be installed and managed.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);

syslog_msgdef(LOG_VQES_MLB, MLB_TOO_MANY_STATIC_ROUTES, LOG_WARNING,
              "The number of static routes has exceeded the maximum allowed, "
              "which is %d. The following static routes were ignored: %s");
msgdef_explanation("There are too many static routes configured. As a result, "
                   "not all of the static routes will be installed and "
                   "managed. The static routes which were ignored are listed.");
msgdef_recommended_action(SYSLOGDOC_CHG_CONF);


/* ------- log messages for the route monitoring code ----------- */

syslog_msgdef(LOG_VQES_MLB, MLB_COMMAND_LINE_OVERFLOW, LOG_ERR, 
              "MLB command line overflow: %s");
msgdef_explanation("An MLB command line parameter is too long.");
msgdef_recommended_action("Correct the VQE-S configuration, then restart "
                          "the VQE-S service.");


syslog_msgdef(LOG_VQES_MLB, MLB_RETURNED_INVALID_FORMAT, LOG_ERR, 
              "An MLB system call returned a string with an invalid format: "
              "%s");
msgdef_explanation("The system call utility is of a wrong version or "
                   "the OS/routing table has an invalid configuration");
msgdef_recommended_action(SYSLOGDOC_REPORT_COPY_CONTACT);


syslog_msgdef(LOG_VQES_MLB, MLB_NEXTHOP_STRING_OVERFLOW, LOG_ERR, 
              "MLB string overflow: '%s'");
msgdef_explanation("MLB attempted to concatenate too many gateways "
                   "for the same IP prefix address.");
msgdef_recommended_action("Remove some of the gateways routed for this "
                          "prefix address (see first address in string). "
                          "You may want to associate them instead with "
                          "more specific IP prefixes.");


syslog_msgdef(LOG_VQES_MLB, MLB_CONST_STRING_OVERFLOW, LOG_ERR, 
              "MLB constant string overflow: '%s'");
msgdef_explanation("MLB attempted to write a fix-length command string that "
                   "is too long.");
msgdef_recommended_action(SYSLOGDOC_REPORT_COPY_CONTACT);


syslog_msgdef(LOG_VQES_MLB, MLB_SHARED_MEM_FAILURE, LOG_ERR, 
              "MLB shared memory failure: %s");
msgdef_explanation("MLB failed to write to the shared memory."
                   "OS may not have sufficient memory, "
                   "or this memory segment may be in use by another process.");
msgdef_recommended_action(SYSLOGDOC_RESTART_VQES);


syslog_msgdef(LOG_VQES_MLB, MLB_SYSTEM_CALL_FAILURE, LOG_ERR, 
              "MLB system call failure: %s");
msgdef_explanation("An MLB system call failed to execute properly."
                   "The OS may not have sufficient resources. "
                   "Verify the status of memory, hard disk quota, and CPU.");
msgdef_recommended_action("If disk quota is full, free hard disk space. "
                          "Otherwise, "SYSLOGDOC_REBOOT_SERVER);


syslog_msgdef(LOG_VQES_MLB, MLB_PIPE_READ_FAILURE, LOG_ERR, 
              "MLB pipe read failure: %s");
msgdef_explanation("MLB failed to create or read information from a system "
                   "call pipe. The OS may not have sufficient resources. "
                   "Verify status of memory, hard disk quota, and CPU.");
msgdef_recommended_action("If disk quota is full, free hard disk space. "
                          "Otherwise, "SYSLOGDOC_REBOOT_SERVER);


syslog_msgdef(LOG_VQES_MLB, MLB_OUT_OF_BOUND_SYSTEM_INFO, LOG_ERR, 
              "MLB system information is out of bound: %s");
msgdef_explanation("MLB read an invalid value in the routing table. "
                   "This is likely due to an unsupported routing option that "
                   "could not be parsed.");
msgdef_recommended_action(SYSLOGDOC_REBOOT_SERVER
                          SYSLOGDOC_REPORT_COPY_CONTACT);


syslog_msgdef(LOG_VQES_MLB, MLB_ROUTING_TABLE_UPDATE, LOG_NOTICE, 
              "MLB updated the routing table: %s");
msgdef_explanation("MLB added or deleted routes from the routing table");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);


#endif /* _VQES_MLB_SYSLOG_DEF_H_ */
