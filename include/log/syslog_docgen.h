/********************************************************************
 *
 *      File:   syslog_docgen.h
 *      Name:   Marco Di Benedetto
 *
 *      Description:
 *       Redefines the syslog macros to generate documentation rather
 *       than code.
 *
 *
 * Copyright (c) 1985-2004, 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *
 *
 * $Id: syslog_docgen.h,v 1.11 2003/11/15 23:01:04 marcodb Exp $
 * $Source: /auto/vwsvua/kirsten/sanos3_fix/VegasSW__isan__platform__common__syslogd/export/isan/syslog_docgen.h,v $
 * $Author: marcodb $
 *
 *********************************************************************/

#define	LOG_EMERG	0	/* system is unusable */
#define	LOG_ALERT	1	/* action must be taken immediately */
#define	LOG_CRIT	2	/* critical conditions */
#define	LOG_ERR		3	/* error conditions */
#define	LOG_WARNING	4	/* warning conditions */
#define	LOG_NOTICE	5	/* normal but significant condition */
#define	LOG_INFO	6	/* informational */
#define	LOG_DEBUG	7	/* debug-level messages */


#define syslog_facilitydef(fac_name, fac_sym,                        \
                           default_priority_filter)                  \
                           <FAC_DEF> fac_name </FAC_DEF>

#define syslog_msgdef(fac_sym, msg_name, priority, output_fmt)       \
            <MSG_DEF> fac_sym - priority - msg_name </MSG_DEF>   \
            <MSG_TEXT> output_fmt </MSG_TEXT>

#define syslog_msgdef_limit(fac_sym, msg_name, priority, output_fmt, limit)   \
            <MSG_DEF> fac_sym - priority - msg_name </MSG_DEF>   \
            <MSG_TEXT> output_fmt </MSG_TEXT>                    \
            <MSG_LIMIT> limit </MSG_LIMIT>                       \

#define syslog_lc_msgdef(fac_sym, card_name, msg_name, priority, output_fmt) \
            <MSG_LC_DEF> fac_sym- priority - msg_name </MSG_LC_DEF>     \
            <MSG_TEXT> output_fmt </MSG_TEXT>

#define msgdef_explanation(txt)        <MSG_EXPL> txt </MSG_EXPL>


#define SYSLOGDOC_NO_ACTION "No action is required."
#define SYSLOGDOC_INFO_ONLY \
        "This is an information message only; no action is required."
#define SYSLOGDOC_REBOOT_SERVER \
        "Reboot the VQE-S server."
#define SYSLOGDOC_RESTART_VQES \
        "Restart the VQE-S application."
#define SYSLOGDOC_RECUR_RESTART_VQES \
        "If this message recurs, restart the VQE-S application."
#define SYSLOGDOC_RESTART_VQES_CHANNELS \
        "Restart the VQE-S channels."
#define SYSLOGDOC_USER_GUIDE \
        "<i>Cisco CDA Visual Quality Experience Application User Guide</i>"
#define SYSLOGDOC_CHG_RESEND_CHANNELS \
        "Refer to the " SYSLOGDOC_USER_GUIDE " " \
        "for channel configuration information. " \
        "Modify the channel configuration, and resend it to the VQE-S " \
        "and/or VCDS servers. "
#define SYSLOGDOC_CHG_CONF \
        "Refer to the "SYSLOGDOC_USER_GUIDE" for information on configuring " \
        "VQE-S. Modify vcdb.conf and apply the configuration. If the condition" \
        "persists, contact your Cisco technical support representative," \
        "and provide the representative with the gathered information."
#define SYSLOGDOC_REPORT \
        "Issue the <b>vqereport</b> command to gather data" \
        "that may help identify the nature of the error."
#define SYSLOGDOC_CONTACT \
        "Contact your Cisco technical support representative," \
        "and provide the representative with the gathered information."
#define SYSLOGDOC_COPY \
        "Copy the error message exactly as it appears in the system log."
#define SYSLOGDOC_RECUR_COPY \
        "If this message recurs, copy the error message" \
        "exactly as it appears in the system log."
#define SYSLOGDOC_REPORT_COPY_CONTACT \
        "Copy the error message exactly as it appears in" \
        "the system log. Issue the <b>vqereport</b> command" \
        "to gather data that may help identify the nature of the error."  \
        "If you cannot determine the nature of the error from the error"  \
        "message text or from the <b>vqereport</b> command"  \
        "output, contact your Cisco technical support representative,"    \
        "and provide the representative with the gathered information."

#define msgdef_recommended_action(txt) <MSG_ACT> txt </MSG_ACT>


#define SYSLOGDOC_NO_DDTS "No DDTS."
#define SYSLOGDOC_LICENSE_REQUIRED "Please install the license file to continue using the feature."
#define msgdef_ddts_component(txt)     <MSG_DDTS> txt </MSG_DDTS>
