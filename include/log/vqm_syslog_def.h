/********************************************************************
 * vqm_syslog_def.h
 *
 * This file defines the SYSLOG messages for VQM.
 *
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef _VQM_SYSLOG_DEF_H_
#define _VQM_SYSLOG_DEF_H_

#include <log/syslog_macros.h>
#include <log/vqe_id.h>

syslog_facilitydef(VQENAME_VQM, LOG_VQM, LOG_ERR);

/* Alert messages */

/* Critical messages */
syslog_msgdef(LOG_VQM, VQM_START, LOG_CRIT, "Video Quality Monitor (VQM) "
              "has (re)started");
msgdef_explanation("This message is used to indicate that the Video Quality "
                   "Monitoring application has just been (re)started. This "
                   "is either expected behavior from starting the process "
                   "or VQM exited unexpectedly and has restarted.");
msgdef_recommended_action("If the VQM application has just been manually "
                          "started or automatically started after bin installation, "
                          "no action is required. "
			  "Otherwise, copy the error message exactly as it appears in the "
                          "VQE-S system log.");


syslog_msgdef(LOG_VQM, VQM_DB_INIT_FAILURE, LOG_CRIT,
              "Failed to initialize the database, error (%u): %s.");
msgdef_explanation("The initialization of the database connection has "
                   "failed, and VQM process has exited.");
msgdef_recommended_action("Determine if the MySQL server has been started, "
	                  "and is running correctly. If it is not operating "
			  "properly, restart the MySQL server with the "
			  "command: /sbin/service mysqld restart");

syslog_msgdef(LOG_VQM, VQM_DB_CONNECT_FAILURE, LOG_CRIT,
              "Failed to connect to the database, error (%u): %s.");
msgdef_explanation("The database connection has failed, and VQM process "
		   "has exited.");
msgdef_recommended_action("Check whether the root user password and database "
		          "name for the database connection is correct. "
			  "Check whether the specified database name exists."
			  "Make sure they are consistent with the value in "
                          "VQM configuration file: /etc/opt/vqes/vqm.cfg");

syslog_msgdef(LOG_VQM, VQM_DB_CREATE_FAILURE, LOG_CRIT,
              "Failed to create the VQM database schema, error (%u): %s.");
msgdef_explanation("The database schema creation has failed, and VQM "
	           "process has exited.");
msgdef_recommended_action(SYSLOGDOC_COPY
	                  SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_VQM, VQM_SOCKET_FAILURE, LOG_CRIT,
              "Socket operation (%s) has encountered an error: %s");
msgdef_explanation("A socket error has been encountered, and VQM process "
 	           "has exited.");
msgdef_recommended_action(SYSLOGDOC_COPY
                          SYSLOGDOC_CONTACT);
	
/* Error messages */

syslog_msgdef(LOG_VQM, VQM_DB_QUERY_ERR, LOG_ERR,
              "Failed to execute database query. %s");
msgdef_explanation("VQM failed to run a SQL query to either create the "
	           "database schema or call a stored procedure to insert data "
		   "into the database. VQM process has exited.")
msgdef_recommended_action(SYSLOGDOC_COPY
                          SYSLOGDOC_CONTACT);

syslog_msgdef(LOG_VQM, VQM_DB_ROWS_ERR, LOG_ERR,
              "Database exceeds the size limit and the VQM process has exited");
msgdef_explanation("The number of the records in the database exceeds the "
	           "maximum allowed size and the VQM process must stop "
		   "data insertions.")
msgdef_recommended_action("Follow the database maintenance procedure to "
	                  "back up and clear the database.");

syslog_msgdef(LOG_VQM, VQM_CNAME_HASH_ERR, LOG_ERR,
              "VQM failed to create hash element entry for cname (%s). "
	      "VQM process has exited.");
msgdef_explanation("VQM can't filter by CNAME before inserting the data into "
	           "the database due to the CNAME map load failure. VQM process"
		   " must stop data insertions." );
msgdef_recommended_action("Check if you type in the correct cnames in cname "
	                  "list file.");

syslog_msgdef(LOG_VQM, VQM_EXCEED_MAX_HASH_ERR, LOG_ERR,
              "VQM cname lists (%d) exceeds the maximum limit (%d) "
	      "and the VQM process has exited");
msgdef_explanation("The number of CNAMEs in the cname list file exceeds "
	           "the maximum allowed limit and the VQM process must "
		   "stop data insertions.")
msgdef_recommended_action("Make the number of CNAMEs in the file  "
	                  "within the limit.");

syslog_msgdef(LOG_VQM, VQM_CNAME_FILE_OPEN_ERR, LOG_ERR,
              "VQM failed to open cname list file (%s). "
	      "VQM process has exited.");
msgdef_explanation("VQM can't open CNAME list file for CNAME filtering. VQM process"
		   " must stop data insertions." );
msgdef_recommended_action("Check if you have read access to the cname "
	                  "list file.");

syslog_msgdef(LOG_VQM, VQM_HASH_TBL_ERR, LOG_ERR,
              "VQM failed to create hash table for the cname lists. "
	      "VQM process has exited.");
msgdef_explanation("VQM can't filter by CNAME before inserting the data into "
	           "the database due to the CNAME map load failure. VQM process"
		   " must stop data insertions." );
msgdef_recommended_action(SYSLOGDOC_COPY
                          SYSLOGDOC_CONTACT);


/* Warning messages */
syslog_msgdef(LOG_VQM, VQM_CFG_SYNTAX_ERR, LOG_WARNING, 
              "Configuration file syntax error (%s) at line %d in file %s");
msgdef_explanation("VQM encountered the specified syntax error on "
                   "the line specified in the configuration file. "
                   "VQM process will be started "
                   "with the default configuration.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQM, VQM_CFG_FILE_MISSING, LOG_WARNING,
              "Cannot access configuration file: %s");
msgdef_explanation("The VQM configuration file is missing. VQM process will "
                   "be started with the default configuration.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQM, VQM_CFG_WRONG_PERMISSION, LOG_WARNING,
              "Cannot open configuration file: %s");
msgdef_explanation("The VQM configuration file has wrong permission. VQM "
                   "process will be started with the default configuration.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQM, VQM_CFG_BAD_TYPE, LOG_WARNING, 
              "Wrong type is detected. %s");
msgdef_explanation("Invalid type is detected while parsing "
                   "VQM configuration file. VQM process will use "
                   "the default value.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQM, VQM_CFG_BAD_VALUE, LOG_WARNING, 
              "The specified value is out of range. %s");
msgdef_explanation("Invalid value is detected while parsing "
                   "VQM configuration file. VQM process will use "
                   "the default value.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);


syslog_msgdef(LOG_VQM, VQM_RECV_MISSED_PKT, LOG_WARNING,
              "Received a missed packet counter report. It indicates "
	      "that there are RTCP reports being dropped on the VQE server "
	      "side. ");
msgdef_explanation("VQM process received an exported report with the type "
            	   "- missed packet counters. It indicates that there are "
		   "RTCP reports being dropped between VQM and VQE server.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQM, VQM_INVALID_RTCP, LOG_WARNING, 
              "Received an invalid RTCP packet: %s.");
msgdef_explanation("VQM process received a RTCP packet with wrong length "
                   "or invalid type. "
                   "VQM process will drop this packet.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQM, VQM_INVALID_RTCP_VER, LOG_WARNING, 
              "Received a RTCP packet with invalid version. "
              "RTCP version = %d, packet source IP address = %s.");
msgdef_explanation("VQM process received a RTCP packet with invalid version. "
                   "The valid version number is 2. "
		   "VQM process will drop this packet.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQM, VQM_DUPLICATE_CNAME_WARNING, LOG_WARNING, 
	      "Found a duplicated cname (%s) in the hash "
              "table for cname mapping.\n");
msgdef_explanation("VQM process found a duplicate cname in the cname list file. "
		   "VQM process will ignore the duplicate cname for filtering.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);


/* Info messages */
syslog_msgdef(LOG_VQM, VQM_RECV_UNKNOWN_PKT, LOG_INFO,
              "Received an unknown packet.");
msgdef_explanation("VQM process received an exported report with an "
                   "unknown type.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQM, VQM_CFG_INFO, LOG_INFO, 
              "Missing information is detected while parsing the config file. "
	      "%s");
msgdef_explanation("Missing information is detected while parsing "
                   "VQM configuration file. VQM process will use "
                   "the default value.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);

syslog_msgdef(LOG_VQM, VQM_EXIT, LOG_INFO, "VQM process exiting");
msgdef_explanation("VQM process exiting. This message will be seen "
                   "whenever VQM exits, if a shutdown has been requested "
		   "or if an unrecoverable error has occurred.");
msgdef_recommended_action(SYSLOGDOC_NO_ACTION);


/* Notice messages */

#endif /* _VQM_SYSLOG_DEF_H_ */
