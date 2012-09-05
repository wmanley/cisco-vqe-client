/*
 *------------------------------------------------------------------
 * sdp_test.c  -- Test routines for the generic SDP parser.
 *
 * March 2001, D. Renee Revis
 *
 * Copyright (c) 2001 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include "sdp.h"
#include <stdio.h>

#define FALSE 0
#define TRUE  1

/*
 * defines, storage to track memory allocation. Used to check for SDP
 * library memory leaks
 */
#define MAX_MALLOCS 1000

typedef struct malloc_list_ {
    void *address;
    long bytes;
} malloc_list_t;

malloc_list_t malloc_list[MAX_MALLOCS];
long memory_in_use;
long memory_malloced;
int malloc_list_ptr;
int num_mallocs;

extern char *sdp_findchar(char *ptr, char *char_list);
void sdp_test_copy_sdp(void *sdp_in_p, void *sdp_out_p);
void sdp_test_copy_attrs(void *sdp_in_p, void *sdp_out_p, u16 level);
void sdp_test_copy_attrs2(void *sdp_in_p, void *sdp_out_p, u16 level);
void sdp_test_compare_fmtp(void *conf_p);
void sdp_test_copy_xcpar_attrs_method1 (void *sdp_in_p, void *sdp_out_p, 
                                        u16 level, u16 cap);
void sdp_test_copy_xcpar_attrs_method2 (void *sdp_in_p, void *sdp_out_p, 
                                        u16 level, u16 cap);

void *my_malloc(unsigned bytes);
void my_free(void *ap);
void my_memcheck_init(void);
void my_check_for_leaks(void);


int main (int argc, char *argv[])
{
    void         *sdp_in_p;
    void         *sdp_out_p;
    void         *conf_p;
    sdp_result_e  result;
    size_t        num_read;
    FILE         *file_p;
    char          buf_in[5000];
    char          buf_out[5000];
    char         *ptr;
    char         *endofbuf;

    if (argc < 2) {
        SDP_PRINT("\nSDP Test: filename not specified, exiting.\n");
        exit (-1);
    }

    my_memcheck_init(); /* setup memory checking code */

    /* Set up SDP configuration for this test application. */
    conf_p = sdp_init_config();

    sdp_appl_debug(conf_p, SDP_DEBUG_TRACE, FALSE);
    sdp_appl_debug(conf_p, SDP_DEBUG_WARNINGS, TRUE);
    sdp_appl_debug(conf_p, SDP_DEBUG_ERRORS, TRUE);

    sdp_require_version(conf_p, TRUE);
    sdp_require_owner(conf_p, FALSE);
    sdp_require_session_name(conf_p, FALSE);
    sdp_require_timespec(conf_p, FALSE);

    sdp_media_supported(conf_p, SDP_MEDIA_AUDIO, TRUE);
    //sdp_media_supported(conf_p, SDP_MEDIA_VIDEO, TRUE);
    //sdp_media_supported(conf_p, SDP_MEDIA_APPLICATION, TRUE);
    //sdp_media_supported(conf_p, SDP_MEDIA_DATA, TRUE);
    //sdp_media_supported(conf_p, SDP_MEDIA_CONTROL, TRUE);
    //sdp_media_supported(conf_p, SDP_MEDIA_NAS_RADIUS, TRUE);
    //sdp_media_supported(conf_p, SDP_MEDIA_NAS_TACACS, TRUE);
    //sdp_media_supported(conf_p, SDP_MEDIA_NAS_DIAMETER, TRUE);
    //sdp_media_supported(conf_p, SDP_MEDIA_NAS_L2TP, TRUE);
    //sdp_media_supported(conf_p, SDP_MEDIA_NAS_LOGIN, TRUE);
    //sdp_media_supported(conf_p, SDP_MEDIA_NAS_NONE, TRUE);

    sdp_nettype_supported(conf_p, SDP_NT_INTERNET, TRUE);
    //sdp_nettype_supported(conf_p, SDP_NT_ATM, TRUE);
    //sdp_nettype_supported(conf_p, SDP_NT_FR, TRUE);
    sdp_nettype_supported(conf_p, SDP_NT_LOCAL, TRUE);

    sdp_addrtype_supported(conf_p, SDP_AT_IP4, TRUE);
    //sdp_addrtype_supported(conf_p, SDP_AT_IP6, TRUE);
    //sdp_addrtype_supported(conf_p, SDP_AT_NSAP, TRUE);
    sdp_addrtype_supported(conf_p, SDP_AT_EPN, TRUE);
    //sdp_addrtype_supported(conf_p, SDP_AT_E164, TRUE);
    //sdp_addrtype_supported(conf_p, SDP_AT_GWID, TRUE);

    sdp_transport_supported(conf_p, SDP_TRANSPORT_RTPAVP, TRUE);
    sdp_transport_supported(conf_p, SDP_TRANSPORT_RTPAVPF, TRUE);
    //sdp_transport_supported(conf_p, SDP_TRANSPORT_UDP, TRUE);
    //sdp_transport_supported(conf_p, SDP_TRANSPORT_UDPTL, TRUE);
    //sdp_transport_supported(conf_p, SDP_TRANSPORT_CES10, TRUE);
    //sdp_transport_supported(conf_p, SDP_TRANSPORT_LOCAL, TRUE);
    //sdp_transport_supported(conf_p, SDP_TRANSPORT_AAL2_ITU, TRUE);
    //sdp_transport_supported(conf_p, SDP_TRANSPORT_AAL2_ATMF, TRUE);
    //sdp_transport_supported(conf_p, SDP_TRANSPORT_AAL2_CUSTOM, TRUE);
    //sdp_transport_supported(conf_p, SDP_TRANSPORT_AAL1AVP, TRUE);

    sdp_allow_choose(conf_p, SDP_CHOOSE_CONN_ADDR, TRUE);
    sdp_allow_choose(conf_p, SDP_CHOOSE_PORTNUM, TRUE);

    //sdp_show_config(conf_p);

    /* Create SDP structures for incoming SDP and the SDP we generate. */
    sdp_in_p  = sdp_init_description(conf_p);
    sdp_out_p = sdp_init_description(conf_p);

    sdp_set_string_debug(sdp_in_p, "\nSDP Test (Incoming):");
    sdp_set_string_debug(sdp_out_p, "\nSDP Test (Outgoing):");

    /* Read the SDP from the file specified. */
    file_p = fopen(argv[1], "read");
    if (file_p == NULL) {
        SDP_PRINT("\nError opening file.\n");
        exit (-1);
    }

    SDP_PRINT("\nReading %s...\n", argv[1]);
    num_read = fread(buf_in, sizeof(char), 5000, file_p);
    endofbuf = buf_in + num_read;
    /*SDP_PRINT("\nRead %d bytes.\n", num_read);*/
    ptr = buf_in;

    /* Test application allows initial comment lines preceeded by 
     * a '#' character. Remove these lines.  Note: The sdp_findchar
     * function is not available for application use (I'm cheating
     * here).  Please don't use this in your code :)
     */
    while (ptr[0] == '#') {
        ptr = sdp_findchar(ptr, "\n");
        ptr++;
    }
    /* Parse the description from the file. */
    result = sdp_parse(sdp_in_p, &ptr, (endofbuf - ptr));
    if (result != SDP_SUCCESS) {
        SDP_PRINT("\nError detected in parsing (%s).\n\n", 
                  sdp_get_result_name(result));
        exit (-1);
    }

    /* Copy all data to our outbound SDP structure. */
    sdp_test_copy_sdp(sdp_in_p, sdp_out_p);

    /* Show the incoming SDP. */
    //ptr = buf_out;
    //result = sdp_build(sdp_in_p, &ptr, 5000);
    //SDP_PRINT("\n\nIncoming SDP: \n%s\n", buf_out);

    /* Build the new SDP description. */
    ptr = buf_out;
    result = sdp_build(sdp_out_p, &ptr, 5000);
    SDP_PRINT("\n\nGenerated SDP: \n%s\n", buf_out);

    /* Test deleting a media line */
    //sdp_delete_media_line(sdp_out_p, 1);
    //ptr = buf_out;
    //result = sdp_build(sdp_out_p, &ptr, 5000);
    //SDP_PRINT("\n\nGenerated SDP: \n%s\n", buf_out);

    sdp_show_stats(conf_p);
    SDP_PRINT("\n\n");

    /* Test comparing fmtp attr ranges. */
    //sdp_test_compare_fmtp(conf_p);
    
    sdp_free_description(sdp_in_p);
    sdp_free_description(sdp_out_p);
    my_free(conf_p);
    my_check_for_leaks();

    exit (0);
}

void sdp_test_compare_fmtp (void *conf_p)
{
    void         *sdp_p;
    sdp_ne_res_e  result;
    u16           sess_inst_num;
    u16           media_inst_num;

    /* Create a test sdp and add one fmtp attr at the session level. */
    sdp_p  = sdp_init_description(conf_p);
    sdp_add_new_attr(sdp_p, SDP_SESSION_LEVEL, 0, SDP_ATTR_FMTP, 
                     &sess_inst_num);

    /* Add another fmtp attr at a media level. */
    result = sdp_insert_media_line(sdp_p, 1);
    sdp_add_new_attr(sdp_p, 1, 0, SDP_ATTR_FMTP, &media_inst_num);

    /* Test #1 */
    /* Set the same non-contiguous range in both attrs and validate compare */
    sdp_attr_set_fmtp_range(sdp_p, SDP_SESSION_LEVEL, 0, sess_inst_num, 
                            1, 10);
    sdp_attr_set_fmtp_range(sdp_p, SDP_SESSION_LEVEL, 0, sess_inst_num, 
                            15, 50);
    sdp_attr_set_fmtp_range(sdp_p, SDP_SESSION_LEVEL, 0, sess_inst_num, 
                            92, 100);

    sdp_attr_set_fmtp_range(sdp_p, 1, 0, media_inst_num, 1, 10);
    sdp_attr_set_fmtp_range(sdp_p, 1, 0, media_inst_num, 15, 50);
    sdp_attr_set_fmtp_range(sdp_p, 1, 0, media_inst_num, 92, 100);

    result = sdp_attr_compare_fmtp_ranges(sdp_p, sdp_p, SDP_SESSION_LEVEL, 1, 
                                          0, 0, sess_inst_num, media_inst_num);
    if (result != SDP_FULL_MATCH) {
        SDP_PRINT("\nSDP Test: Compare fmtp failed first test\n");
    } else {
        SDP_PRINT("\nSDP Test: Compare fmtp passed first test\n");
    }

    /* Test #2 - Test clearing ranges */
    sdp_attr_clear_fmtp_range(sdp_p, SDP_SESSION_LEVEL, 0, 
                              sess_inst_num, 1, 10);
    sdp_attr_clear_fmtp_range(sdp_p, 1, 0, media_inst_num, 1, 10);

    result = sdp_attr_compare_fmtp_ranges(sdp_p, sdp_p, SDP_SESSION_LEVEL, 1, 
                                          0, 0, sess_inst_num, media_inst_num);
    if (result != SDP_FULL_MATCH) {
        SDP_PRINT("\nSDP Test: Compare fmtp failed second test\n");
    } else {
        SDP_PRINT("\nSDP Test: Compare fmtp passed second test\n");
    }

    /* Test #3 - Validate partial match.  Set events in one attr but
     * not the other. */
    sdp_attr_set_fmtp_range(sdp_p, SDP_SESSION_LEVEL, 0, sess_inst_num, 5, 10);

    result = sdp_attr_compare_fmtp_ranges(sdp_p, sdp_p, SDP_SESSION_LEVEL, 1, 
                                          0, 0, sess_inst_num, media_inst_num);
    if (result != SDP_PARTIAL_MATCH) {
        SDP_PRINT("\nSDP Test: Compare fmtp failed third test\n");
    } else {
        SDP_PRINT("\nSDP Test: Compare fmtp passed third test\n");
    }

    /* Test #4 - Validate no match.  Set all events in one attr and
     * none in the other. */
    sdp_attr_clear_fmtp_range(sdp_p, SDP_SESSION_LEVEL, 0, 
                              sess_inst_num, 0, 255);
    sdp_attr_set_fmtp_range(sdp_p, 1, 0, media_inst_num, 0, 255);

    result = sdp_attr_compare_fmtp_ranges(sdp_p, sdp_p, SDP_SESSION_LEVEL, 1, 
                                          0, 0, sess_inst_num, media_inst_num);
    if (result != SDP_NO_MATCH) {
        SDP_PRINT("\nSDP Test: Compare fmtp failed fourth test\n");
    } else {
        SDP_PRINT("\nSDP Test: Compare fmtp passed fourth test\n");
    }

    sdp_free_description(sdp_p);
}

void sdp_test_copy_sdp (void *sdp_in_p, void *sdp_out_p) 
{
    u16                i, j, k;
    u16                num_payloads;
    u16                num_profiles;
    u16                num_media_lines;
    u32                payload;
    sdp_payload_ind_e  indicator;
    sdp_result_e       result;
    sdp_port_format_e  port_format;
    sdp_transport_e    transport;

    /* Validate and copy version. */
    if (sdp_version_valid(sdp_in_p) == TRUE) {
        result = sdp_set_version(sdp_out_p, sdp_get_version(sdp_in_p));
    } else {
        SDP_PRINT("\nSDP copy: Version is invalid.\n");
    }

    /* Validate and copy owner. */
    if (sdp_owner_valid(sdp_in_p) == TRUE) {
        result = sdp_set_owner_username(sdp_out_p, 
                                       sdp_get_owner_username(sdp_in_p));
        result = sdp_set_owner_sessionid(sdp_out_p, 
                                       sdp_get_owner_sessionid(sdp_in_p));
        result = sdp_set_owner_version(sdp_out_p, 
                                       sdp_get_owner_version(sdp_in_p));
        result = sdp_set_owner_network_type(sdp_out_p, 
                                       sdp_get_owner_network_type(sdp_in_p));
        result = sdp_set_owner_address_type(sdp_out_p, 
                                       sdp_get_owner_address_type(sdp_in_p));
        result = sdp_set_owner_address(sdp_out_p, 
                                       sdp_get_owner_address(sdp_in_p));
    } else {
        SDP_PRINT("\nSDP copy: Owner is invalid.\n");
    }

    /* Validate and copy session name. */
    if (sdp_session_name_valid(sdp_in_p) == TRUE) {
        result = sdp_set_session_name(sdp_out_p, 
                                      sdp_get_session_name(sdp_in_p));
    } else {
        SDP_PRINT("\nSDP copy: Session name is invalid.\n");
    }
        
    /* Validate and copy timespec. */
    if (sdp_timespec_valid(sdp_in_p) == TRUE) {
        result = sdp_set_time_start(sdp_out_p, 
                                    sdp_get_time_start(sdp_in_p));
        result = sdp_set_time_stop(sdp_out_p, 
                                   sdp_get_time_stop(sdp_in_p));
    } else {
        SDP_PRINT("\nSDP copy: Timespec is invalid.\n");
    }

    /* Validate and copy encryption spec. */
    if (sdp_encryption_valid(sdp_in_p, SDP_SESSION_LEVEL) == TRUE) {
        result = sdp_set_encryption_method(sdp_out_p, SDP_SESSION_LEVEL,
                    sdp_get_encryption_method(sdp_in_p, SDP_SESSION_LEVEL));
        result = sdp_set_encryption_key(sdp_out_p, SDP_SESSION_LEVEL,
                    sdp_get_encryption_key(sdp_in_p, SDP_SESSION_LEVEL));
    }
        
    /* Validate and copy connection info. */
    if (sdp_connection_valid(sdp_in_p, SDP_SESSION_LEVEL) == TRUE) {
        result = sdp_set_conn_nettype(sdp_out_p, SDP_SESSION_LEVEL,
                        sdp_get_conn_nettype(sdp_in_p, SDP_SESSION_LEVEL));
        result = sdp_set_conn_addrtype(sdp_out_p, SDP_SESSION_LEVEL,
                        sdp_get_conn_addrtype(sdp_in_p, SDP_SESSION_LEVEL));
        result = sdp_set_conn_address(sdp_out_p, SDP_SESSION_LEVEL,
                        sdp_get_conn_address(sdp_in_p, SDP_SESSION_LEVEL));
    }

    /* Copy any session level attributes. */
    sdp_test_copy_attrs(sdp_in_p, sdp_out_p, SDP_SESSION_LEVEL);

    /* Validate and copy media info. */
    num_media_lines = sdp_get_num_media_lines(sdp_in_p);
    for (i=1; i <= num_media_lines; i++) {
        /* Validate and copy media line specific info. */
        result = sdp_insert_media_line(sdp_out_p, i);
        if (result != SDP_SUCCESS) {
            SDP_PRINT("\nSDP TEST: Failure inserting new media line");
            return;
        }
        result = sdp_set_media_type(sdp_out_p, i, 
                                    sdp_get_media_type(sdp_in_p, i));
        port_format = sdp_get_media_port_format(sdp_in_p, i);
        result = sdp_set_media_port_format(sdp_out_p, i, port_format);

        switch (port_format) {
        case SDP_PORT_NUM_ONLY: 
            result = sdp_set_media_portnum(sdp_out_p, i,  
                                    sdp_get_media_portnum(sdp_in_p, i));
            break;

        case SDP_PORT_NUM_COUNT: 
            result = sdp_set_media_portnum(sdp_out_p, i,  
                                    sdp_get_media_portnum(sdp_in_p, i));
            result = sdp_set_media_portnum(sdp_out_p, i,  
                                    sdp_get_media_portnum(sdp_in_p, i));
            break;
        case SDP_PORT_VPI_VCI: 
            result = sdp_set_media_vpi(sdp_out_p, i,  
                                    sdp_get_media_vpi(sdp_in_p, i));
            result = sdp_set_media_vci(sdp_out_p, i,  
                                    sdp_get_media_vci(sdp_in_p, i));
            break;
        case SDP_PORT_VCCI: 
            result = sdp_set_media_vcci(sdp_out_p, i,  
                                    sdp_get_media_vcci(sdp_in_p, i));
            break;
        case SDP_PORT_NUM_VPI_VCI: 
            result = sdp_set_media_portnum(sdp_out_p, i,  
                                    sdp_get_media_portnum(sdp_in_p, i));
            result = sdp_set_media_vpi(sdp_out_p, i,  
                                    sdp_get_media_vpi(sdp_in_p, i));
            result = sdp_set_media_vci(sdp_out_p, i,  
                                    sdp_get_media_vci(sdp_in_p, i));
            break;
        case SDP_PORT_VCCI_CID: 
            result = sdp_set_media_vcci(sdp_out_p, i,  
                                    sdp_get_media_vcci(sdp_in_p, i));
            result = sdp_set_media_cid(sdp_out_p, i,  
                                    sdp_get_media_cid(sdp_in_p, i));
            break;
        case SDP_PORT_NUM_VPI_VCI_CID: 
            result = sdp_set_media_portnum(sdp_out_p, i,  
                                    sdp_get_media_portnum(sdp_in_p, i));
            result = sdp_set_media_vpi(sdp_out_p, i,  
                                    sdp_get_media_vpi(sdp_in_p, i));
            result = sdp_set_media_vci(sdp_out_p, i,  
                                    sdp_get_media_vci(sdp_in_p, i));
            result = sdp_set_media_cid(sdp_out_p, i,  
                                    sdp_get_media_cid(sdp_in_p, i));
            break;
        default:
            SDP_PRINT("\nSDP copy: Port number format not set.\n");
            break;
        }        

        transport = sdp_get_media_transport(sdp_in_p, i);
        if ((transport >= SDP_TRANSPORT_AAL2_ITU) &&
            (transport <= SDP_TRANSPORT_AAL2_CUSTOM)) {
            num_profiles = sdp_get_media_num_profiles(sdp_in_p, i);
            for (j=1; j <= num_profiles; j++) {
                result = sdp_add_media_profile(sdp_out_p, i,
                                  sdp_get_media_profile(sdp_in_p, i, j));
                num_payloads=sdp_get_media_profile_num_payload_types(sdp_in_p,
                                                                     i, j);
                for (k=1; k <= num_payloads; k++) {
                    payload = sdp_get_media_profile_payload_type(sdp_in_p, 
                                                         i, j, k, &indicator);
                    sdp_add_media_profile_payload_type(sdp_out_p, i, j, 
                                                       payload, indicator);
                    
                }
            }
        } else {
            result = sdp_set_media_transport(sdp_out_p, i, transport);
            num_payloads = sdp_get_media_num_payload_types(sdp_in_p, i);
            for (j=1; j <= num_payloads; j++) {
                payload = sdp_get_media_payload_type(sdp_in_p, i, j, 
                                                     &indicator);
                result = sdp_add_media_payload_type(sdp_out_p, i, payload, 
                                                    indicator);
            }
        }

        /* Validate and copy c= connection line info. */
        if (sdp_connection_valid(sdp_in_p, i) == TRUE) {
            result = sdp_set_conn_nettype(sdp_out_p, i,
                                          sdp_get_conn_nettype(sdp_in_p, i));
            result = sdp_set_conn_addrtype(sdp_out_p, i,
                                           sdp_get_conn_addrtype(sdp_in_p, i));
            result = sdp_set_conn_address(sdp_out_p, i,
                                          sdp_get_conn_address(sdp_in_p, i));
        }

        /* Validate and copy encryption spec. */
        if (sdp_encryption_valid(sdp_in_p, i) == TRUE) {
            result = sdp_set_encryption_method(sdp_out_p, i,
                                sdp_get_encryption_method(sdp_in_p, i));
            result = sdp_set_encryption_key(sdp_out_p, i,
                                sdp_get_encryption_key(sdp_in_p, i));
        }

        /* Copy any media level attributes. */
        sdp_test_copy_attrs(sdp_in_p, sdp_out_p, i);
    }            
}


/* Note: There are two basic ways to process attributes with the library.
 * First is to determine the total number of attributes at each level, then
 * step through each one, finding out what type of attr it is and then
 * processing.  The second method is to process each attribute type at
 * a time.  The application determines the number of attributes of a 
 * particular type at a given level and then processes them one at a time.
 * This second approach is used with this copy routine.  The disadvantage
 * is that the outgoing SDP won't necessary have the attributes listed in
 * the same order as the incoming SDP.  For this test appl it doesn't
 * really matter, but for other applications it may. */
void sdp_test_copy_attrs (void *sdp_in_p, void *sdp_out_p, u16 level) 
{
    u16                i, j, k;
    u16                cap_num;
    u16                num_attr_inst;
    u16                num_payloads;
    u16                payload;
    u16                inst_num;
    sdp_result_e       result;
    sdp_payload_ind_e  ind;

    for (i=0; i < SDP_MAX_ATTR_TYPES; i++) {
        result = sdp_attr_num_instances(sdp_in_p, level, 0, i, &num_attr_inst);
        for (j=1; j <= num_attr_inst; j++) {
            sdp_add_new_attr(sdp_out_p, level, 0, i, &inst_num);
            switch (i) {

            case SDP_ATTR_BEARER:
            case SDP_ATTR_CALLED:
            case SDP_ATTR_CONN_TYPE:
            case SDP_ATTR_DIALED:
            case SDP_ATTR_DIALING:
            case SDP_ATTR_FRAMING:
                /* Simple string attributes. */
                sdp_attr_set_simple_string(sdp_out_p, i, level, 0, j,
                        sdp_attr_get_simple_string(sdp_in_p, i, level, 0, j));
                break;

            case SDP_ATTR_EECID:
            case SDP_ATTR_PTIME:
            case SDP_ATTR_T38_VERSION:
            case SDP_ATTR_T38_MAXBITRATE:
            case SDP_ATTR_T38_MAXBUFFER:
            case SDP_ATTR_T38_MAXDGRAM:
            case SDP_ATTR_X_SQN:
            case SDP_ATTR_TC1_PAYLOAD_BYTES:
            case SDP_ATTR_TC1_WINDOW_SIZE:
            case SDP_ATTR_TC2_PAYLOAD_BYTES:
            case SDP_ATTR_TC2_WINDOW_SIZE:
            case SDP_ATTR_RTCP:
                /* Simple u32 attributes. */
                sdp_attr_set_simple_u32(sdp_out_p, i, level, 0, j,
                        sdp_attr_get_simple_u32(sdp_in_p, i, level, 0, j));
                break;

            case SDP_ATTR_T38_FILLBITREMOVAL:
            case SDP_ATTR_T38_TRANSCODINGMMR:
            case SDP_ATTR_T38_TRANSCODINGJBIG:
                /* Simple boolean attributes. */
                sdp_attr_set_simple_boolean(sdp_out_p, i, level, 0, j,
                        sdp_attr_get_simple_boolean(sdp_in_p, i, level, 0, j));
                break;

            case SDP_ATTR_INACTIVE:
            case SDP_ATTR_RECVONLY:
            case SDP_ATTR_SENDONLY:
            case SDP_ATTR_SENDRECV:
                /* No parameters for attribute. */
                break;
            case SDP_ATTR_FMTP:
                sdp_attr_set_fmtp_payload_type(sdp_out_p, level, 0, j,
                        sdp_attr_get_fmtp_payload_type(sdp_in_p, level, 0, j));
                sdp_attr_copy_fmtp_ranges(sdp_in_p, sdp_out_p, level, level,
                                          0, 0, j, j);
                break;
            case SDP_ATTR_QOS:
            case SDP_ATTR_SECURE:
            case SDP_ATTR_X_PC_QOS:
            case SDP_ATTR_X_QOS:
                sdp_attr_set_qos_strength(sdp_out_p, level, 0, i, j,
                         sdp_attr_get_qos_strength(sdp_in_p, level, 0, i, j));
                sdp_attr_set_qos_direction(sdp_out_p, level, 0, i, j,
                         sdp_attr_get_qos_direction(sdp_in_p, level, 0, i, j));
                sdp_attr_set_qos_confirm(sdp_out_p, level, 0, i, j,
                         sdp_attr_get_qos_confirm(sdp_in_p, level, 0, i, j));
                break;
            case SDP_ATTR_RTPMAP:
                sdp_attr_set_rtpmap_payload_type(sdp_out_p, level, 0, j,
                     sdp_attr_get_rtpmap_payload_type(sdp_in_p, level, 0, j));
                sdp_attr_set_rtpmap_encname(sdp_out_p, level, 0, j,
                     sdp_attr_get_rtpmap_encname(sdp_in_p, level, 0, j));
                sdp_attr_set_rtpmap_clockrate(sdp_out_p, level, 0, j,
                     sdp_attr_get_rtpmap_clockrate(sdp_in_p, level, 0, j));
                sdp_attr_set_rtpmap_num_chan(sdp_out_p, level, 0, j,
                     sdp_attr_get_rtpmap_num_chan(sdp_in_p, level, 0, j));
                break;
            case SDP_ATTR_SUBNET:
                sdp_attr_set_subnet_nettype(sdp_out_p, level, 0, j,
                          sdp_attr_get_subnet_nettype(sdp_in_p, level, 0, j));
                sdp_attr_set_subnet_addrtype(sdp_out_p, level, 0, j,
                          sdp_attr_get_subnet_addrtype(sdp_in_p, level, 0, j));
                sdp_attr_set_subnet_addr(sdp_out_p, level, 0, j,
                          sdp_attr_get_subnet_addr(sdp_in_p, level, 0, j));
                sdp_attr_set_subnet_prefix(sdp_out_p, level, 0, j,
                          sdp_attr_get_subnet_prefix(sdp_in_p, level, 0, j));
                break;
            case SDP_ATTR_T38_RATEMGMT:
                sdp_attr_set_t38ratemgmt(sdp_out_p, level, 0, j,
                       sdp_attr_get_t38ratemgmt(sdp_in_p, level, 0, j));
                break;
            case SDP_ATTR_T38_UDPEC:
                sdp_attr_set_t38udpec(sdp_out_p, level, 0, j,
                       sdp_attr_get_t38udpec(sdp_in_p, level, 0, j));
                break;
            case SDP_ATTR_X_CAP:
                sdp_attr_set_xcap_media_type(sdp_out_p, level, j,
                       sdp_attr_get_xcap_media_type(sdp_in_p, level, j));
                sdp_attr_set_xcap_transport_type(sdp_out_p, level, j,
                      sdp_attr_get_xcap_transport_type(sdp_in_p, level, j));
                num_payloads = sdp_attr_get_xcap_num_payload_types(sdp_in_p,
                                                                   level, j);
                for (k=1; k <= num_payloads; k++) {
                    payload = sdp_attr_get_xcap_payload_type(sdp_in_p, level, 
                                                             j, k, &ind);
                    sdp_attr_add_xcap_payload_type(sdp_out_p, level, j,
                                                   payload, ind);
                }
                cap_num = sdp_attr_get_xcap_first_cap_num(sdp_in_p, level, j);
                //sdp_test_copy_xcpar_attrs_method1(sdp_in_p, sdp_out_p, 
                //                                  level, cap_num);
                sdp_test_copy_xcpar_attrs_method2(sdp_in_p, sdp_out_p, 
                                                  level, cap_num);
                break;
            case SDP_ATTR_X_CPAR:
                /* Can't process these as a group because they are associated
                 * with specific X-cap attributes. Must be copied along
                 * with the specific X-cap attr. */
                break;
            case SDP_ATTR_X_PC_CODEC:
                num_payloads = sdp_attr_get_pccodec_num_payload_types(sdp_in_p,
                                                                  level, 0, j);
                for (k=1; k <= num_payloads; k++) {
                sdp_attr_add_pccodec_payload_type(sdp_out_p, level, 0, j,
                  sdp_attr_get_pccodec_payload_type(sdp_in_p, level, 0, j, k));
                }
                break;
            }
        }
    }

    /* Add another direction parameter so we can verify the parser
     * figured out the right direction.
     */

    /*
    direction = sdp_get_media_direction(sdp_in_p, level, 0);
    switch (direction) {
    case SDP_DIRECTION_INACTIVE:
        sdp_add_new_attr(sdp_out_p, level, 0, SDP_ATTR_INACTIVE);
        break;
    case SDP_DIRECTION_SENDONLY:
        sdp_add_new_attr(sdp_out_p, level, 0, SDP_ATTR_SENDONLY);
        break;
    case SDP_DIRECTION_RECVONLY:
        sdp_add_new_attr(sdp_out_p, level, 0, SDP_ATTR_RECVONLY);
        break;
    case SDP_DIRECTION_SENDRECV:
        sdp_add_new_attr(sdp_out_p, level, 0, SDP_ATTR_SENDRECV);
        break;

    }
    */

}

/* NOTE: If you use this routine instead of sdp_test_copy_attrs(), the
 * output will not match exactly because the attributes will be ordered
 * differently. */

/* Note: There are two basic ways to process attributes with the library.
 * First is to determine the total number of attributes at each level, then
 * step through each one, finding out what type of attr it is and then
 * processing. This is the method used with this routine.  It also uses
 * the sdp_copy_attr function rather than the get/set APIs.*/  
void sdp_test_copy_attrs2 (void *sdp_in_p, void *sdp_out_p, u16 level) 
{
    u16                i, k;
    u16                cap_num;
    u16                num_attrs;
    u16                num_payloads;
    u16                payload;
    u16                src_inst_num;
    u16                dst_inst_num;
    sdp_attr_e         attr_type;
    sdp_direction_e    direction;
    sdp_payload_ind_e  ind;

    sdp_get_total_attrs(sdp_in_p, level, 0, &num_attrs);

    for (i=1; i <= num_attrs; i++) {
        sdp_get_attr_type(sdp_in_p, level, 0, i, &attr_type, &src_inst_num);

        if ((attr_type != SDP_ATTR_X_CAP) && 
            (attr_type != SDP_ATTR_X_CPAR)) {
            sdp_copy_attr(sdp_in_p, sdp_out_p, level, level, 0, 0, 
                          attr_type, src_inst_num);
        } else if (attr_type == SDP_ATTR_X_CAP) {
            sdp_add_new_attr(sdp_out_p, level, 0, attr_type, &dst_inst_num);
            sdp_attr_set_xcap_media_type(sdp_out_p, level, dst_inst_num,
                  sdp_attr_get_xcap_media_type(sdp_in_p, level, src_inst_num));
            sdp_attr_set_xcap_transport_type(sdp_out_p, level, dst_inst_num,
                        sdp_attr_get_xcap_transport_type(sdp_in_p, level, 
                                                         src_inst_num));
            num_payloads = sdp_attr_get_xcap_num_payload_types(sdp_in_p,
                                                         level, src_inst_num);
            for (k=1; k <= num_payloads; k++) {
                payload = sdp_attr_get_xcap_payload_type(sdp_in_p, level, 
                                                        src_inst_num, k, &ind);
                sdp_attr_add_xcap_payload_type(sdp_out_p, level, dst_inst_num,
                                               payload, ind);
            }
            cap_num = sdp_attr_get_xcap_first_cap_num(sdp_in_p, level, 
                                                      src_inst_num);
            //sdp_test_copy_xcpar_attrs_method1(sdp_in_p, sdp_out_p, 
            //                                  level, cap_num);
            sdp_test_copy_xcpar_attrs_method2(sdp_in_p, sdp_out_p, 
                                              level, cap_num);
        }
    }
}

void sdp_test_copy_xcpar_attrs_method1 (void *sdp_in_p, void *sdp_out_p, 
                                        u16 level, u16 cap)
{
    u16          i, j;
    u16          num_attrs;
    u16          inst_num;


    for (i=0; i < SDP_MAX_ATTR_TYPES; i++) {
        sdp_attr_num_instances(sdp_in_p, level, cap, i, &num_attrs);
        for (j=1; j <= num_attrs; j++) {
            sdp_add_new_attr(sdp_out_p, level, cap, i, &inst_num);
            switch (i) {
            case SDP_ATTR_RTPMAP:
                sdp_attr_set_rtpmap_payload_type(sdp_out_p, level, cap, j,
                  sdp_attr_get_rtpmap_payload_type(sdp_in_p, level, cap, j));
                sdp_attr_set_rtpmap_encname(sdp_out_p, level, cap, j,
                  sdp_attr_get_rtpmap_encname(sdp_in_p, level, cap, j));
                sdp_attr_set_rtpmap_clockrate(sdp_out_p, level, cap, j,
                  sdp_attr_get_rtpmap_clockrate(sdp_in_p, level, cap, j));
                sdp_attr_set_rtpmap_num_chan(sdp_out_p, level, cap, j,
                  sdp_attr_get_rtpmap_num_chan(sdp_in_p, level, cap, j));
                break;
            case SDP_ATTR_FMTP:
                sdp_attr_set_fmtp_payload_type(sdp_out_p, level, cap, j,
                  sdp_attr_get_fmtp_payload_type(sdp_in_p, level, cap, j));
                sdp_attr_copy_fmtp_ranges(sdp_in_p, sdp_out_p, level, level,
                  cap, cap, j, j);
                break;
            default:
                break;
            }
        }
    }
}

void sdp_test_copy_xcpar_attrs_method2 (void *sdp_in_p, void *sdp_out_p, 
                                        u16 level, u16 cap)
{
    u16        i;
    u16        total_attrs;
    u16        src_inst;
    u16        dst_inst;
    sdp_attr_e attr_type;

    sdp_get_total_attrs(sdp_in_p, level, cap, &total_attrs);
    for (i=1; i <= total_attrs; i++) {
        sdp_get_attr_type(sdp_in_p, level, cap, i, &attr_type, &src_inst);
        sdp_add_new_attr(sdp_out_p, level, cap, attr_type, &dst_inst);
        switch (attr_type) {
        case SDP_ATTR_RTPMAP:
            sdp_attr_set_rtpmap_payload_type(sdp_out_p, level, cap, dst_inst,
                sdp_attr_get_rtpmap_payload_type(sdp_in_p, level, 
                                                 cap, src_inst));
            sdp_attr_set_rtpmap_encname(sdp_out_p, level, cap, dst_inst,
                sdp_attr_get_rtpmap_encname(sdp_in_p, level, cap, src_inst));
            sdp_attr_set_rtpmap_clockrate(sdp_out_p, level, cap, dst_inst,
                sdp_attr_get_rtpmap_clockrate(sdp_in_p, level, cap, src_inst));
            sdp_attr_set_rtpmap_num_chan(sdp_out_p, level, cap, dst_inst,
                sdp_attr_get_rtpmap_num_chan(sdp_in_p, level, cap, src_inst));
            break;
        case SDP_ATTR_FMTP:
            sdp_attr_set_fmtp_payload_type(sdp_out_p, level, cap, dst_inst,
                sdp_attr_get_fmtp_payload_type(sdp_in_p, level, 
                                               cap, src_inst));
            sdp_attr_copy_fmtp_ranges(sdp_in_p, sdp_out_p, level, level,
                cap, cap, src_inst, dst_inst);
            break;
        default:
            break;
        }
    }
}

void my_memcheck_init (void)
{
    int i;

    for (i = 0; i < MAX_MALLOCS; i++) {
        malloc_list[i].address = NULL;
        malloc_list[i].bytes = 0;
    }

    memory_in_use = 0;
    memory_malloced = 0;
    malloc_list_ptr = 0;
    num_mallocs = 0;
}

void my_check_for_leaks (void)
{
    int i;

    printf("\n number of allocations = %u", num_mallocs);
    printf("\n total amount allocated = %u", memory_malloced);
    printf("\n total amount freed = %u", (memory_malloced - memory_in_use));

    if (memory_in_use) {
        printf("\n\n MEMORY LEAK DETECTED!!!\n");
        for (i = 0; i < MAX_MALLOCS ; i++) {
            if (malloc_list[i].address) {
                printf("\n  %u bytes not freed", malloc_list[i].bytes);
            }
        }
        printf("\n");
    }
    printf("\n");
}

void *my_malloc (unsigned bytes)
{
    void *ptr;
    int i = 0;

    ptr = malloc(bytes);
    memory_malloced += bytes;
    memory_in_use += bytes;
    num_mallocs++;

    /*
     * search for free spot in malloc tracking list
     */
    while (malloc_list[malloc_list_ptr].address != NULL) {
        malloc_list_ptr++;

        /* circular wrap */
        if (malloc_list_ptr >= MAX_MALLOCS) {
            malloc_list_ptr = 0;
        }

        if (++i > MAX_MALLOCS) {
            printf("\n%s() malloc list exhausted!\n", __FUNCTION__);
            return (NULL);
        }
    }
    malloc_list[malloc_list_ptr].address = ptr;
    malloc_list[malloc_list_ptr].bytes = bytes;

    return (ptr);
}

void my_free (void *ptr)
{
    int i = 0;
    /*
     * find this block of memory in the malloc tracking list
     */
    while (malloc_list[malloc_list_ptr].address != ptr) {
        malloc_list_ptr--;

        /* circular wrap */
        if (malloc_list_ptr < 0) {
            malloc_list_ptr = MAX_MALLOCS - 1;
        }
        if (++i > MAX_MALLOCS) {
            printf("\n%s() malloc list exhausted!\n", __FUNCTION__);
            return;
        }
    }

    memory_in_use -= malloc_list[malloc_list_ptr].bytes;
    malloc_list[malloc_list_ptr].address = NULL;
}
