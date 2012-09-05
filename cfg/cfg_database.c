/*
 *------------------------------------------------------------------
 * Database access functions for configuration module
 *
 * May 2006, Dong Hsu
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <stdio.h>
#include <utils/vam_debug.h>
#include <utils/vam_time.h>
#include <utils/vmd5.h>
#include <sdp.h>
#include "cfg_database.h"

#include <log/vqe_cfg_syslog_def.h>
#include <log/vqe_cfg_debug.h>

#define MAX_MSG_LENGTH 256

/* Function:    cfg_db_open
 * Description: Open a configuration database
 *              For now, the configuration database is a text file, and in
 *              the future, it could be a real database such as MySQL.
 * Parameters:  cfg_db          Configuration database structure
 *              cfg_db_name     Name of the database
 *              type            Type of the database
 * Returns:     Success or failure
 */
cfg_db_ret_e cfg_db_open (cfg_database_t *cfg_db_p,
                          const char* cfg_db_name_p,
                          db_type_e type)
{
    cfg_db_ret_e result = CFG_DB_SUCCESS;

    /* Check on the database name */
    if (cfg_db_name_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                      "cfg_db_open:: Configuration database name "
                      "must be specified.\n");

        return CFG_DB_NO_NAME_SPECIFIED;
    }

    /* Save the database name */
    strncpy(cfg_db_p->db_name, cfg_db_name_p, MAX_DB_NAME_LENGTH);

    cfg_db_p->type = type;

    /* Open the configuration databse */
    switch (type) {
        case DB_FILE:
            cfg_db_p->db_file_p = (void *) fopen(cfg_db_name_p, "read");
            if (cfg_db_p->db_file_p == NULL) {
                VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                              "cfg_db_open:: Could not open database "
                              "file %s.\n",
                              cfg_db_name_p);

                /* Try to open the backup database */
                cfg_db_p->db_file_p = (void *) fopen(BACKUP_DB_NAME, "read");
                if (cfg_db_p->db_file_p == NULL) {
                    VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                  "cfg_db_open:: Could not open "
                                  "the backup database file %s.\n",
                                  BACKUP_DB_NAME);

                    result = CFG_DB_OPEN_FAILED;
                }
                else {
                    VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                  "cfg_db_open:: Database "
                                  "%s is opened\n", 
                                  BACKUP_DB_NAME);
                }

            }
            else {
                VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL, 
                              "cfg_db_open:: Database %s is opened with "
                              "file pointer %p\n",
                              cfg_db_name_p, cfg_db_p->db_file_p);
            }

            break;

        default:
            result = CFG_DB_FAILURE;
            VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                          "cfg_db_open:: Unspecified database type %d.\n",
                          type);
    }

    return result;
}


/* Function:    cfg_db_close
 * Description: Close the configuration database
 * Parameters:  cfg_db  Configuration database structure
 * Returns:     Success or failure
 */
cfg_db_ret_e cfg_db_close (cfg_database_t *cfg_db_p)
{
    cfg_db_ret_e result = CFG_DB_SUCCESS;

    /* Close the database */
    switch (cfg_db_p->type) {
        case DB_FILE:
            if (cfg_db_p->db_file_p != NULL) {
                VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                              "cfg_db_close:: Database %s is closed for "
                              "file pointer %p\n",
                              cfg_db_p->db_name, cfg_db_p->db_file_p);
                fclose((FILE*) cfg_db_p->db_file_p);
                cfg_db_p->db_file_p = NULL;
            }

            break;

        default:
            result = CFG_DB_FAILURE;
            VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                          "Unspecified database type %d.\n",
                          cfg_db_p->type);
    }

    return result;
}


/* Function:    cfg_db_read_channels
 * Description: Read all the channel info from database and store them in
 *              channel_mgr_t
 * Parameters:  cfg_db          Configuration database structure
 *              channel_mgr     Channel manager
 * Returns:     Success or failure
 */
cfg_db_ret_e cfg_db_read_channels (cfg_database_t *cfg_db_p,
                                  channel_mgr_t *channel_mgr_p)
{
    cfg_channel_ret_e status;
    char        id_buffer[MAX_LINE_LENGTH];
    char        line_buffer[MAX_LINE_LENGTH];
    char        session_buffer[MAX_SESSION_SIZE];
    void        *sdp_cfg_p;
    void        *sdp_p;
    idmgr_id_t  handle;
    boolean     in_session = FALSE;
    uint32_t    total_session_length = 0;
    boolean     skip = FALSE;
    uint32_t    chksum;
    boolean     syntax_error = FALSE;

    if (cfg_db_p->db_file_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                      "cfg_db_read_channels:: Database %s is not opened.\n",
                      cfg_db_p->db_name);
        return CFG_DB_NOT_OPEN;
    }

    /* The decision has been made that each channel configuration info 
       is represented by a SDP description regardless being stored in a
       file, database or fetched remotely. In the case of using a plain
       file, each SDP description is separated by a session divider. */

    /* Set up a default SDP configuration for all the sessions */
    sdp_cfg_p = sdp_init_config();

    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_TRACE, FALSE);
    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_WARNINGS, FALSE);
    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_ERRORS, FALSE);

    sdp_require_version(sdp_cfg_p, TRUE);
    sdp_require_owner(sdp_cfg_p, TRUE);
    sdp_require_session_name(sdp_cfg_p, TRUE);
    sdp_require_timespec(sdp_cfg_p, TRUE);

    sdp_media_supported(sdp_cfg_p, SDP_MEDIA_VIDEO, TRUE);

    sdp_nettype_supported(sdp_cfg_p, SDP_NT_INTERNET, TRUE);    
    sdp_addrtype_supported(sdp_cfg_p, SDP_AT_IP4, TRUE);
   
    sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_UDP, TRUE);
    sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_RTPAVP, TRUE);
    sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_RTPAVPF, TRUE);
    
    sdp_allow_choose(sdp_cfg_p, SDP_CHOOSE_CONN_ADDR, TRUE);
    sdp_allow_choose(sdp_cfg_p, SDP_CHOOSE_PORTNUM, TRUE);

    /* Parse all the data contents */
    switch (cfg_db_p->type) {
        case DB_FILE:
            memset(session_buffer, 0, MAX_SESSION_SIZE);
            memset(id_buffer, 0, MAX_LINE_LENGTH);
            total_session_length = 0;
            while (fgets(line_buffer, MAX_LINE_LENGTH, 
                         (FILE *) cfg_db_p->db_file_p) != NULL) {
                if (line_buffer[0] == '-' && line_buffer[1] == '-') {
                    /* Found the session boundary */
                    VQE_CFG_DEBUG(CFG_DEBUG_SDP, NULL,
                                  "cfg_db_read_channels:: Session = \n%s\n",
                                  session_buffer);
                    in_session = FALSE;

                    /* We will not count the empty session as one channel */
                    if (session_buffer[0] == '\0') {
                        continue;
                    }

                    channel_mgr_p->num_input_channels++;

                    /* Create a SDP structure for this session */
                    sdp_p  = sdp_init_description(sdp_cfg_p);
                    VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                  "cfg_db_read_channels:: Creating SDP session"
                                  " %p\n", sdp_p);

                    if (sdp_p == NULL) {
                        syslog_print(CFG_MALLOC_ERR);
                        return CFG_DB_MALLOC_ERR;
                    }
                       
                    /* Parse and validate SDP description in the session */
                    if (cfg_db_parse_SDP(sdp_p, session_buffer,
                                         &syntax_error)) {
                        /* Increase number of channels passing the parser */
                        channel_mgr_p->num_parsed++;

                        /* Compute MD5 checksum */
                        chksum = cfg_db_checksum(session_buffer,
                                                 strlen(session_buffer));
                        
                        /* Store it in the channel manager */
                        status = cfg_channel_add(sdp_p, 
                                                 channel_mgr_p, 
                                                 &handle, chksum);
                        if (status != CFG_CHANNEL_SUCCESS) {
                            if (status != CFG_CHANNEL_EXIST) {
                                VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                              "cfg_db_read_channels:: Failed "
                                              "to add the channel.\n");
                                VQE_CFG_DEBUG(CFG_DEBUG_SDP, NULL,
                                              "SDP Session = \n%s\n", 
                                              session_buffer);
                            }
                        }
                        else {
                            /* Increase number of channels passing */
                            /* the validation */
                            channel_mgr_p->num_validated++;

                            VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                          "cfg_db_read_channels:: "
                                          "New channel 0x%lx is being added\n",
                                          handle);

                        }

                    }
                    else {
                        if (syntax_error) {
                            channel_mgr_p->num_syntax_errors++;
                        }
                        VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                      "cfg_db_read_channels:: Failed to "
                                      "create SDP session for the channel.\n");
                        VQE_CFG_DEBUG(CFG_DEBUG_SDP, NULL, 
                                      "SDP Session = \n%s\n", session_buffer);
                    }

                    if (sdp_p) {
                        /* Delete the SDP data */
                        VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                      "cfg_db_read_channels:: Deleting SDP "
                                      "session %p\n",
                                      sdp_p);
                        sdp_free_description(sdp_p);
                    }

                    /* Reset the seesion buffer for new session */
                    memset(session_buffer, 0, MAX_SESSION_SIZE);
                    memset(id_buffer, 0, MAX_LINE_LENGTH);
                    total_session_length = 0;
                    skip = FALSE;
                }
                else {
                    /* Strip out the commented lines or blank lines */
                    if (line_buffer[0] != '#' &&
                        line_buffer[0] != '\r' &&
                        line_buffer[0] != '\n' &&
                        line_buffer[0] != ' ') {
                        if (line_buffer[0] == 'v' && line_buffer[1] == '=') {
                            if (in_session == TRUE) {
                                syslog_print(CFG_NO_SESSION_DIVIDER_WARN,
                                             id_buffer);
                            }
                            in_session = TRUE;
                        }

                        /* Remember the o= line */
                        if (line_buffer[0] == 'o' && line_buffer[1] == '=') {
                            strncpy(id_buffer, line_buffer,
                                    strlen(line_buffer)-1);
                        }

                        total_session_length += strlen(line_buffer);
                        if (total_session_length >= MAX_SESSION_SIZE) {
                            if (skip == FALSE) {
                                syslog_print(CFG_EXCEED_BUFFER_SIZE_WARN,
                                             id_buffer, MAX_SESSION_SIZE);
                            }
                            skip = TRUE;
                        }
                        else {
                            strncat(session_buffer, line_buffer, 
                                    strlen(line_buffer));
                        }
                    }
                }
            }
            
            /* Process the last session */
            if (session_buffer[0] != '\r' &&
                session_buffer[0] != '\n' &&
                session_buffer[0] != '\0') {
                VQE_CFG_DEBUG(CFG_DEBUG_SDP, NULL,
                              "cfg_db_read_channels:: Session = \n%s\n",
                              session_buffer);
                channel_mgr_p->num_input_channels++;

                /* Make sure that there is a null termination at the end */
                if (total_session_length < MAX_SESSION_SIZE-1) {
                    if (session_buffer[strlen(session_buffer)] != '\0') {
                        session_buffer[strlen(session_buffer)] = '\0';
                    }
                }

                /* Create a SDP structure for this session */
                sdp_p  = sdp_init_description(sdp_cfg_p);
                VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                              "cfg_db_read_channels:: Creating SDP session "
                              "%p\n", sdp_p);

                if (sdp_p == NULL) {
                    syslog_print(CFG_MALLOC_ERR);
                    return CFG_DB_MALLOC_ERR;
                }
                       
                /* Parse and validate SDP description in the session */
                if (cfg_db_parse_SDP(sdp_p, session_buffer, &syntax_error)) {
                    /* Increase number of channels passing the parser */
                    channel_mgr_p->num_parsed++;

                    /* Compute MD5 checksum */
                    chksum = cfg_db_checksum(session_buffer,
                                             strlen(session_buffer));
                        
                    /* Store it in the channel manager */
                    status = cfg_channel_add(sdp_p, channel_mgr_p,
                                             &handle, chksum);
                    if (status != CFG_CHANNEL_SUCCESS) {
                        if (status != CFG_CHANNEL_EXIST) {
                            VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                          "cfg_db_read_channels:: Failed to "
                                          "add the channel.\n");
                            VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL, 
                                          "SDP Session = \n%s\n", 
                                          session_buffer);
                        }
                    }
                    else {
                        /* Increase number of channels passing */
                        /* the validation */
                        channel_mgr_p->num_validated++;

                        VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                      "cfg_db_read_channels:: New channel "
                                      "0x%lx is being added\n",
                                      handle);
                    }
                }
                else {
                    if (syntax_error) {
                        channel_mgr_p->num_syntax_errors++;
                    }
                    VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                  "cfg_db_read_channels:: Failed to "
                                  "create SDP session for the channel.\n");
                    VQE_CFG_DEBUG(CFG_DEBUG_SDP, NULL, 
                                  "SDP Session = \n%s\n", session_buffer);
                }

                /* Delete the SDP data */
                if (sdp_p) {
                    VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                  "cfg_db_read_channels:: Deleting SDP "
                                  "session %p\n",
                                  sdp_p);
                    sdp_free_description(sdp_p);
                }
            }

            break;

        default:
            VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                          "cfg_db_read_channels:: Unspecified database "
                          "type %d.\n",
                          cfg_db_p->type);
            return CFG_DB_NOT_SUPPORTED;

    }

    /* Free up stuff */
    my_free(sdp_cfg_p);

    return CFG_DB_SUCCESS;
}


/* Function:    cfg_db_write_channels
 * Description: Write all the channel info to database
 * Parameters:  channel_mgr     channel manager
 * Returns:     Success or failure
 */
cfg_db_ret_e cfg_db_write_channels (cfg_database_t *cfg_db_p,
                                   channel_mgr_t *channel_mgr_p)
{
    uint16_t i;
    channel_cfg_t *channel_p;
    char session_buffer[MAX_SESSION_SIZE];


    VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                  "cfg_db_write_channels:: Writing all the configuration "
                  "to database\n");

    /* Move the current database to a backup one */
    if (cfg_db_close(cfg_db_p) != CFG_DB_SUCCESS) {
        VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                      "cfg_db_write_channels:: Could not close "
                      "database file %s.\n",
                      cfg_db_p->db_name);
    }
    else {
        if (rename(cfg_db_p->db_name, BACKUP_DB_NAME)) {
            VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                          "cfg_db_write_channels:: Could not move "
                          "the database file %s to the backup %s.\n",
                          cfg_db_p->db_name, BACKUP_DB_NAME);
        }        
    }
    
    /* Open the configuration databse for write */
    cfg_db_p->db_file_p = (void *) fopen(cfg_db_p->db_name, "write");
    if (cfg_db_p->db_file_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                     "cfg_db_write_channels:: Could not open database "
                      "file %s.\n",
                      cfg_db_p->db_name);
        return CFG_DB_OPEN_FAILED;
    }
    else {
        VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                      "cfg_db_write_channels:: database %s is "
                      "opened for write with file pointer %p\n",
                      cfg_db_p->db_name, cfg_db_p->db_file_p);
    }

    /* Output the configuration data to the database */
    fprintf(cfg_db_p->db_file_p, "#\n");
    fprintf(cfg_db_p->db_file_p, "# Channel Configuration File\n");
    fprintf(cfg_db_p->db_file_p, "#\n\n");

    for (i = 0; i < channel_mgr_p->total_num_channels; i++) {
        channel_p = cfg_channel_get(channel_mgr_p,
                                    channel_mgr_p->handles[i]);

        if (channel_p != NULL) {
            if (channel_p->active) {
                if (cfg_db_create_SDP(channel_p, session_buffer, 
                                      MAX_SESSION_SIZE) == CFG_DB_SUCCESS) {
                    fprintf(cfg_db_p->db_file_p, "%s", session_buffer);

                    if (i != channel_mgr_p->total_num_channels-1) {
                        /* Add a session separator here */
                        fprintf(cfg_db_p->db_file_p, SESSION_SEPARATOR);
                    }
                }
                else {
                    VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                                  "cfg_db_write_channels:: Failed to "
                                  "create SDP syntax for channel %d.\n",
                                  i);
                }
            }
        }
    }

    if (fflush((FILE*) cfg_db_p->db_file_p)) {
        VQE_CFG_DEBUG(CFG_DEBUG_DB, NULL,
                      "cfg_db_write_channels:: Failed to flush the data "
                      "to database %s.\n",
                      cfg_db_p->db_name);
    }

    return CFG_DB_SUCCESS;
}


/* Function:    cfg_db_parse_SDP
 * Description: Parse SDP syntax
 * Parameters:  sdp_in_p  allocated SDP internal data structure pointer
 *              syntax_error set to TRUE if a SDP syntax error was encountered,
 *                           set to FALSE otherwise
 * Returns:     true or false
 */
boolean cfg_db_parse_SDP (void *sdp_in_p, char* data_p, boolean *syntax_error)
{
    sdp_result_e        result;
    char message_buffer[MAX_MSG_LENGTH];
    char *input_data_p;

    if (syntax_error) {
        *syntax_error = FALSE;
    }

    if (sdp_in_p && data_p && strlen(data_p) != 0) {
#ifdef DEBUG
        sdp_set_string_debug(sdp_in_p, "\nSDP Validation:");
#endif

        /* Parse the description from the buffer */
        input_data_p = data_p;
        result = sdp_parse(sdp_in_p, &data_p, strlen(data_p));
        if (result != SDP_SUCCESS) {
            if (result == SDP_INVALID_SDP_PTR ||
                result == SDP_NO_RESOURCE ||
                result == SDP_NULL_BUF_PTR) {
                snprintf(message_buffer, MAX_MSG_LENGTH, "%s.",
                         sdp_get_result_name(result));
                syslog_print(CFG_SDP_PARSE_WARN, message_buffer);
            }
            else {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "%s. Check 'RFC4566-Session Description "
                         "Protocol' for correct syntax.", 
                         sdp_get_result_name(result));
                syslog_print(CFG_SDP_PARSE_WARN, message_buffer);
                syslog_print(CFG_ILLEGAL_SDP_SYNTAX_WARN, input_data_p);
                if (syntax_error) {
                    *syntax_error = TRUE;
                }
            }
            return FALSE;
        }

#ifdef DEBUG
        sdp_show_stats(sdp_cfg_p);
        SDP_PRINT("\n\n");
#endif
    }

    return TRUE;
}


#define STATUS_LEN 10

/* Function:    cfg_db_create_SDP
 * Description: Create a SDP description
 * Parameters:  
 * Returns:     Success or failure
 */
cfg_db_ret_e cfg_db_create_SDP (channel_cfg_t *channel_p, 
                                char* data_p, 
                                int length)
{
    char username[SDP_MAX_STRING_LEN];
    char sessionid[SDP_MAX_STRING_LEN];
    char creator_addr[SDP_MAX_STRING_LEN];
    char o_line[SDP_MAX_STRING_LEN];
    char original_source_address[MAX_IP_ADDR_CHAR];
    uint16_t original_source_port;
    uint16_t original_source_rtcp_port;
    char re_sourced_address[MAX_IP_ADDR_CHAR];
    uint16_t re_sourced_rtp_port;
    uint16_t re_sourced_rtcp_port;
    char fbt_address[MAX_IP_ADDR_CHAR];
    uint16_t rtx_rtp_port;
    uint16_t rtx_rtcp_port;
    char status1[STATUS_LEN], status2[STATUS_LEN], status3[STATUS_LEN];
    char rtcp_fb_control1[RTCP_FB_CTL_SIZE];
    char rtcp_fb_control2[RTCP_FB_CTL_SIZE];
    char group[GROUP_SIZE];
    char fec_group[GROUP_SIZE];
    char tmp[INET_ADDRSTRLEN];
    char rtcp_nack_msg[RTCP_NACK_MSG_SIZE];
    char rtcp_nack_pli_msg[RTCP_NACK_MSG_SIZE];
    char orig_rs_bw[BW_SIZE];
    char orig_rr_bw[BW_SIZE];
    char orig_per_rcvr_bw[BW_SIZE];
    char re_srcd_rs_bw[BW_SIZE];
    char re_srcd_rr_bw[BW_SIZE];
    char rtx_rs_bw[BW_SIZE];
    char rtx_rr_bw[BW_SIZE];

    char fec_session[FEC_SESSION_SIZE];
    char fec1_address[MAX_IP_ADDR_CHAR];
    char src_for_fec1[MAX_IP_ADDR_CHAR];
    char fec2_address[MAX_IP_ADDR_CHAR];
    char src_for_fec2[MAX_IP_ADDR_CHAR];

    strncpy(original_source_address,
            inet_ntop(AF_INET, &channel_p->original_source_addr,
                      tmp, INET_ADDRSTRLEN),
            MAX_IP_ADDR_CHAR);
    original_source_port = ntohs(channel_p->original_source_port);
    original_source_rtcp_port = ntohs(channel_p->original_source_rtcp_port);

    strncpy(re_sourced_address,
            inet_ntop(AF_INET, &channel_p->re_sourced_addr,
                      tmp, INET_ADDRSTRLEN),
            MAX_IP_ADDR_CHAR);
    re_sourced_rtp_port = ntohs(channel_p->re_sourced_rtp_port);
    re_sourced_rtcp_port = ntohs(channel_p->re_sourced_rtcp_port);

    strncpy(fbt_address,
            inet_ntop(AF_INET, &channel_p->fbt_address,
                      tmp, INET_ADDRSTRLEN),
            MAX_IP_ADDR_CHAR);
    rtx_rtp_port = ntohs(channel_p->rtx_rtp_port);
    rtx_rtcp_port = ntohs(channel_p->rtx_rtcp_port);

    memset(group, 0, GROUP_SIZE);
    if (channel_p->role == SSM_DS) {
        strncpy(status1, "sendonly", STATUS_LEN);
        strncpy(status2, "inactive", STATUS_LEN);
        strncpy(status3, "inactive", STATUS_LEN);
    }
    else if (channel_p->role == VAM) {
        if (channel_p->mode == SOURCE_MODE) {
            strncpy(status1, "recvonly", STATUS_LEN);
            strncpy(status2, "sendonly", STATUS_LEN);
            strncpy(status3, "sendonly", STATUS_LEN);
            snprintf(group, GROUP_SIZE, GROUP, "2 3");
        }
        else if (channel_p->mode == LOOKASIDE_MODE) {
            strncpy(status1, "recvonly", STATUS_LEN);
            strncpy(status2, "inactive", STATUS_LEN);
            strncpy(status3, "sendonly", STATUS_LEN);
            snprintf(group, GROUP_SIZE, GROUP, "1 3");
        }
    }
    else if (channel_p->role == STB) {
        if (channel_p->mode == SOURCE_MODE) {
            strncpy(status1, "inactive", STATUS_LEN);
            strncpy(status2, "recvonly", STATUS_LEN);
            strncpy(status3, "recvonly", STATUS_LEN);
            snprintf(group, GROUP_SIZE, GROUP, "2 3");
        }
        else if (channel_p->mode == LOOKASIDE_MODE) {
            strncpy(status1, "recvonly", STATUS_LEN);
            strncpy(status2, "inactive", STATUS_LEN);
            strncpy(status3, "recvonly", STATUS_LEN);
            snprintf(group, GROUP_SIZE, GROUP, "1 3");
        }
        else if (channel_p->mode == RECV_ONLY_MODE) {
            strncpy(status1, "recvonly", STATUS_LEN);
            strncpy(status2, "inactive", STATUS_LEN);
            strncpy(status3, "inactive", STATUS_LEN);
        }
    }

    memset(rtcp_fb_control1, 0, RTCP_FB_CTL_SIZE);
    memset(rtcp_fb_control2, 0, RTCP_FB_CTL_SIZE);
    memset(rtcp_nack_msg, 0, RTCP_NACK_MSG_SIZE);
    memset(rtcp_nack_pli_msg, 0, RTCP_NACK_MSG_SIZE);
    if (channel_p->mode == SOURCE_MODE) {
        if (channel_p->role != SSM_DS) {
            if (channel_p->er_enable == TRUE) {
                snprintf(rtcp_nack_msg,
                         RTCP_NACK_MSG_SIZE,
                         RTCP_NACK_MSG,
                         channel_p->re_sourced_payload_type);
            }

            if (channel_p->fcc_enable == TRUE) {
                snprintf(rtcp_nack_pli_msg,
                         RTCP_NACK_MSG_SIZE,
                         RTCP_NACK_PLI_MSG,
                         channel_p->re_sourced_payload_type);
            }

            snprintf(rtcp_fb_control2,
                     RTCP_FB_CTL_SIZE,
                     RTCP_FB_CTL,
                     re_sourced_rtcp_port,
                     fbt_address,
                     rtcp_nack_msg,
                     rtcp_nack_pli_msg);
        }
        else {
            snprintf(rtcp_fb_control1,
                     RTCP_FB_CTL_SIZE,
                     "a=rtcp:%d\n",
                     original_source_rtcp_port);
        }
    }
    else if (channel_p->mode == LOOKASIDE_MODE) {
        if (channel_p->er_enable == TRUE) {
            snprintf(rtcp_nack_msg,
                     RTCP_NACK_MSG_SIZE,
                     RTCP_NACK_MSG,
                     channel_p->original_source_payload_type);
        }

        if (channel_p->fcc_enable == TRUE) {
            snprintf(rtcp_nack_pli_msg,
                     RTCP_NACK_MSG_SIZE,
                     RTCP_NACK_PLI_MSG,
                     channel_p->original_source_payload_type);
        }

        snprintf(rtcp_fb_control1,
                 RTCP_FB_CTL_SIZE,
                 RTCP_FB_CTL,
                 original_source_rtcp_port,
                 fbt_address,
                 rtcp_nack_msg,
                 rtcp_nack_pli_msg);
    }
    else {
        if (channel_p->fbt_address.s_addr == 0) {
            snprintf(rtcp_fb_control1,
                     RTCP_FB_CTL_SIZE,
                     "a=rtcp:%d\n",
                     original_source_rtcp_port);
        }
        else {
            snprintf(rtcp_fb_control1,
                     RTCP_FB_CTL_SIZE,
                     "a=rtcp:%d IN IP4 %s\n",
                     original_source_rtcp_port,
                     fbt_address);
        }
    }

    memset(fec_session, 0, FEC_SESSION_SIZE);
    memset(fec_group, 0, GROUP_SIZE);
    if (channel_p->fec_enable == TRUE) {
        strncpy(fec1_address,
                inet_ntop(AF_INET,
                          &channel_p->fec_stream1.multicast_addr,
                          tmp, INET_ADDRSTRLEN),
                MAX_IP_ADDR_CHAR);
        strncpy(src_for_fec1,
                inet_ntop(AF_INET,
                          &channel_p->fec_stream1.src_addr,
                          tmp, INET_ADDRSTRLEN),
                MAX_IP_ADDR_CHAR);
        strncpy(fec2_address,
                inet_ntop(AF_INET,
                          &channel_p->fec_stream2.multicast_addr,
                          tmp, INET_ADDRSTRLEN),
                MAX_IP_ADDR_CHAR);
        strncpy(src_for_fec2,
                inet_ntop(AF_INET,
                          &channel_p->fec_stream2.src_addr,
                          tmp, INET_ADDRSTRLEN),
                MAX_IP_ADDR_CHAR);

        if (channel_p->fec_mode == FEC_1D_MODE) {
            snprintf(fec_session, FEC_SESSION_SIZE,
                     FEC_SESSION,
                     ntohs(channel_p->fec_stream1.rtp_port),
                     channel_p->fec_stream1.payload_type,
                     fec1_address,
                     (channel_p->role == STB) ? "recvonly" : "sendonly",
                     fec1_address,
                     src_for_fec1,
                     channel_p->fec_stream1.payload_type,
                     "parityfec",
                     ntohs(channel_p->fec_stream1.rtcp_port),
                     ntohs(channel_p->fec_stream2.rtp_port),
                     channel_p->fec_stream2.payload_type,
                     fec2_address,
                     "inactive",
                     fec2_address,
                     src_for_fec2,
                     channel_p->fec_stream2.payload_type,
                     "2dparityfec",
                     ntohs(channel_p->fec_stream2.rtcp_port));

            if (channel_p->mode == LOOKASIDE_MODE ||
                channel_p->mode == RECV_ONLY_MODE) {
                snprintf(fec_group, GROUP_SIZE, FEC_GROUP, "1 4");
            }
            else if (channel_p->mode == SOURCE_MODE) {
                snprintf(fec_group, GROUP_SIZE, FEC_GROUP, "2 4");
            }
        }
        else {
            snprintf(fec_session, FEC_SESSION_SIZE,
                     FEC_SESSION,
                     ntohs(channel_p->fec_stream1.rtp_port),
                     channel_p->fec_stream1.payload_type,
                     fec1_address,
                     (channel_p->role == STB) ? "recvonly" : "sendonly",
                     fec1_address,
                     src_for_fec1,
                     channel_p->fec_stream1.payload_type,
                     "2dparityfec",
                     ntohs(channel_p->fec_stream1.rtcp_port),
                     ntohs(channel_p->fec_stream2.rtp_port),
                     channel_p->fec_stream2.payload_type,
                     fec2_address,
                     (channel_p->role == STB) ? "recvonly" : "sendonly",
                     fec2_address,
                     src_for_fec2,
                     channel_p->fec_stream2.payload_type,
                     "2dparityfec",
                     ntohs(channel_p->fec_stream2.rtcp_port));

            if (channel_p->mode == LOOKASIDE_MODE ||
                channel_p->mode == RECV_ONLY_MODE) {
                snprintf(fec_group, GROUP_SIZE, FEC_GROUP, "1 4 5");
            }
            else if (channel_p->mode == SOURCE_MODE) {
                snprintf(fec_group, GROUP_SIZE, FEC_GROUP, "2 4 5");
            }
        }
    }

    /* Create bandwidth lines */
    memset(orig_rs_bw, 0, BW_SIZE);
    if (channel_p->original_rtcp_sndr_bw != -1) {
        snprintf(orig_rs_bw, BW_SIZE, RS_BW, channel_p->original_rtcp_sndr_bw);
    }

    memset(orig_rr_bw, 0, BW_SIZE);
    if (channel_p->original_rtcp_rcvr_bw != -1) {
        snprintf(orig_rr_bw, BW_SIZE, RR_BW, channel_p->original_rtcp_rcvr_bw);
    }

    memset(orig_per_rcvr_bw, 0, BW_SIZE);
    if (channel_p->original_rtcp_per_rcvr_bw != -1) {
        snprintf(orig_per_rcvr_bw, BW_SIZE, PER_RCVR_BW,
                 channel_p->original_source_payload_type,
                 channel_p->original_rtcp_per_rcvr_bw);
    }

    memset(re_srcd_rs_bw, 0, BW_SIZE);
    if (channel_p->re_sourced_rtcp_sndr_bw != -1) {
        snprintf(re_srcd_rs_bw, BW_SIZE, RS_BW,
                 channel_p->re_sourced_rtcp_sndr_bw);
    }

    memset(re_srcd_rr_bw, 0, BW_SIZE);
    if (channel_p->re_sourced_rtcp_rcvr_bw != -1) {
        snprintf(re_srcd_rr_bw, BW_SIZE, RR_BW,
                 channel_p->re_sourced_rtcp_rcvr_bw);
    }

    memset(rtx_rs_bw, 0, BW_SIZE);
    if (channel_p->repair_rtcp_sndr_bw != -1) {
        snprintf(rtx_rs_bw, BW_SIZE, RS_BW, channel_p->repair_rtcp_sndr_bw);
    }

    memset(rtx_rr_bw, 0, BW_SIZE);
    if (channel_p->repair_rtcp_rcvr_bw != -1) {
        snprintf(rtx_rr_bw, BW_SIZE, RR_BW, channel_p->repair_rtcp_rcvr_bw);
    }

    memset(username, 0, SDP_MAX_STRING_LEN);
    memset(sessionid, 0, SDP_MAX_STRING_LEN);
    memset(creator_addr, 0, SDP_MAX_STRING_LEN);
    cfg_channel_parse_session_key(channel_p,
                                  username, 
                                  sessionid,
                                  creator_addr);

    /* Reconstruct the o= line in SDP syntax */
    snprintf(o_line, MAX_KEY_LENGTH, "%s %s %llu IN IP4 %s",
             username, sessionid, channel_p->version, creator_addr);

    /* Output everything in a buffer */
    snprintf(data_p,
             length,
             SDP_TEMPLATE,
             o_line,
             channel_p->name,
             channel_p->name,
             group,
             fec_group,
             original_source_port,
             (channel_p->source_proto == UDP_STREAM) ? "udp" :
             ((channel_p->mode == LOOKASIDE_MODE) ? "RTP/AVPF" : "RTP/AVP"),
             channel_p->original_source_payload_type,
             original_source_address,
             channel_p->bit_rate/1000,
             orig_rs_bw,
             orig_rr_bw,
             orig_per_rcvr_bw,
             status1,
             original_source_address,
             inet_ntop(AF_INET, &channel_p->src_addr_for_original_source,
                       tmp, INET_ADDRSTRLEN),
             channel_p->original_source_payload_type,
             rtcp_fb_control1,
             re_sourced_rtp_port,
             channel_p->re_sourced_payload_type,
             re_sourced_address,
             re_srcd_rs_bw,
             re_srcd_rr_bw,
             status2,
             re_sourced_address,
             fbt_address,
             channel_p->re_sourced_payload_type,
             rtcp_fb_control2,
             rtx_rtp_port,
             channel_p->rtx_payload_type,
             fbt_address,
             rtx_rs_bw,
             rtx_rr_bw,
             status3,
             channel_p->rtx_payload_type,
             rtx_rtcp_port,
             channel_p->rtx_payload_type,
             channel_p->rtx_apt,
             channel_p->rtx_payload_type,
             channel_p->rtx_time,
             fec_session);

    return CFG_DB_SUCCESS;
}

void *my_malloc (unsigned bytes)
{
    void *ptr = NULL;
    ptr = (void *) malloc(bytes);
    if (ptr) {
        memset(ptr, 0, bytes);
    }

    return ptr;
}

void my_free (void *ptr)
{
    if (ptr != NULL) {
        free(ptr);
    }
}




/* Function:    cfg_db_checksum
 * Description: Compute MD5 checksum on channel SDP content
 * Parameters:  sdp_buffer      pointer to SDP content
 *              length          length of the buffer
 * Returns:     MD5 checksum
 */
#define MD_CTX struct MD5Context
#define MDInit vqe_MD5Init
#define MDUpdate vqe_MD5Update
#define MDFinal vqe_MD5Final

#define DIGEST_BYTES  16
#define DIGEST_LWORDS  4

uint32_t cfg_db_checksum (char *sdp_buffer, int length)
{
    MD_CTX context;
    union {
        unsigned char c[DIGEST_BYTES];
        uint32_t x[DIGEST_LWORDS];
    } digest;
    uint32_t r;
    int32_t i;
    MDInit (&context);
    MDUpdate (&context, (unsigned char *)sdp_buffer, length);
    MDFinal ((unsigned char *)&digest, &context);
    r = 0;
    for (i = 0; i < 3; i++) {
        r ^= digest.x[i];
    }

    return r;
}
