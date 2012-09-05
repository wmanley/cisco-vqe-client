/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: DP-specific syslog definitions.
 *
 * Documents: Syslogs defined for the dataplane. Syslogs must not be used for
* per-packet errors or events.
 *
 *****************************************************************************/

#ifndef __VQEC_DP_SYSLOG_H__
#define __VQEC_DP_SYSLOG_H__

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

#define SYSLOGDOC_RESTART_VQEC \
        "Restart the VQE-C application."


syslog_facilitydef(VQENAME_VQEC_DP, LOG_VQEC_DP, LOG_ERR);


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

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_INVALIDARGS, LOG_CRIT,
              "Invalid arguments passed to function: %s.");
msgdef_explanation("Invalid arguments passed to function: [chars].");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_MALLOC_FAILURE, LOG_ERR, 
              "Memory allocation of %d bytes failed (%s).");
msgdef_explanation(
    "The Data Plane process failed to allocate memory. Additional error "
    "messages should indicate the consequences of the failure."); 
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_OUTPUTSHIM_ERROR, LOG_ERR, 
              "Outputshim operational error (%s).");
msgdef_explanation(
    "The Data Plane outputshim component had an error. The associated string "
    "provides details on the exact nature of the error.");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_INTERNAL_FORCED_EXIT, LOG_CRIT, 
              "The Data Plane is exiting due to an unexpected internal error "
              "condition, resulting from a problem with the "
              "software (reason: %s).");
msgdef_explanation(
    "The Data Plane has encountered an unexpected, internal error condition, "
    "and must exit.  The text of the error message indicates a more specific "
    "reason for the failure.  In all cases, this error condition results from a "
    "flaw in the software.");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_CHAN_IS_CREATE_FAILED, LOG_ERR, 
              "The creation of an input stream for the dataplane channel %s has "
              "failed because of an error (reason: %s).");
msgdef_explanation(
    "The Data Plane channel component has encountered an unexpected, internal "
    "error condition.  The text of the error message indicates a more specific "
    "reason for the failure.");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_CHAN_TOO_MANY_SOURCES, LOG_ERR, 
              "The number of RTP receiver sources that can be created for  "
              "the receiver have exceeded the maximum allowable bound of %d.");
msgdef_explanation(
    " ");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_CHAN_TOO_MANY_SOURCES_GLOBAL, LOG_ERR, 
              "The number of RTP receiver sources that can be created globally  "
              "has been exceeded.");
msgdef_explanation(
    " ");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_CHAN_NEW_SOURCE_SYNC_ERROR, LOG_ERR, 
              "Failure in synchronizing the RTP sequence number space for new "
              "source %s for channel %s.");
msgdef_explanation(
    "Insufficient information is available to synchronize the new source "
    "with the old source during a source failover event.");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_INIT_FAILURE, LOG_CRIT, 
              "Initialization of the dataplane failed (%s).");
msgdef_explanation(
    " ");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_CHAN_RTP_GENERAL_FAILURE, LOG_ERR, 
              "Failure in RTP processing for stream %s, with reason %s.");
msgdef_explanation(
    " ");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_LOST_IRQ, LOG_ERR, 
              "Upcall IPC message dropped (%s).");
msgdef_explanation(
    " ");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);


syslog_msgdef(LOG_VQEC_DP, VQEC_DP_ERROR, LOG_ERR, 
              "%s.");
msgdef_explanation(
    " ");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_LOST_UPCALL, LOG_ERR, 
              "%s.");
msgdef_explanation(
    " ");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);

syslog_msgdef(LOG_VQEC_DP, VQEC_DP_CHAN_ERROR, LOG_ERR, 
              "Channel %s: [%s] %s.");
msgdef_explanation(
    " ");
msgdef_recommended_action(
    SYSLOGDOC_REPORT_COPY_CONTACT
    SYSLOGDOC_RESTART_VQEC);

#endif /* VQEC_DP_SYSLOG_H__ */

