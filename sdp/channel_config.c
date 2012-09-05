/*
 *------------------------------------------------------------------
 * channel_config.c: Sample Channel configuration 
 *
 * Copyright (c) 2001-2007 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include "sdp.h"
#include <stdio.h>

#define FALSE 0
#define TRUE  1
#define MAX_SESSION_NAME  100
#define MAX_IP_ADDR_CHAR  15
#define MAX_BUFFER_SIZE 5000

extern char *sdp_findchar(char *ptr, char *char_list);
void *my_malloc(unsigned bytes);
void my_free(void *ap);

typedef struct channel_cfg_ {
    char               name[MAX_SESSION_NAME];

    /* Primary Stream Information */
    char*              primary_ip_addr[MAX_IP_ADDR_CHAR];
    unsigned short     primary_rtp_port;
    unsigned short     primary_rtcp_port;

    char*              fbt_address[MAX_IP_ADDR_CHAR]; 

    /* Repair Stream Information */
    unsigned short     repair_rtp_port;
    unsigned short     repair_rtcp_port;
} channel_cfg_t;

unsigned char store_channel_config (void *sdp_in, channel_cfg_t *channel);
void  print_channel_config (channel_cfg_t *channel);


int main (int argc, char *argv[])
{
    void         *sdp_in_p;
    void         *conf_p;
    sdp_result_e  result;
    size_t        num_read;
    FILE         *file_p;
    char          buf_in[MAX_BUFFER_SIZE];
    char         *ptr;
    char         *endofbuf;
    channel_cfg_t channel;

    if (argc < 2) {
        SDP_PRINT("\nSDP Test: filename not specified, exiting.\n");
        exit (-1);
    }


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
    sdp_media_supported(conf_p, SDP_MEDIA_VIDEO, TRUE);
    sdp_media_supported(conf_p, SDP_MEDIA_APPLICATION, TRUE);

    sdp_nettype_supported(conf_p, SDP_NT_INTERNET, TRUE);    

    sdp_addrtype_supported(conf_p, SDP_AT_IP4, TRUE);
    sdp_addrtype_supported(conf_p, SDP_AT_EPN, TRUE);
   
    sdp_transport_supported(conf_p, SDP_TRANSPORT_RTPAVP, TRUE);
    sdp_transport_supported(conf_p, SDP_TRANSPORT_RTPAVPF, TRUE);
    sdp_transport_supported(conf_p, SDP_TRANSPORT_UDP, TRUE);
    
    sdp_allow_choose(conf_p, SDP_CHOOSE_CONN_ADDR, TRUE);
    sdp_allow_choose(conf_p, SDP_CHOOSE_PORTNUM, TRUE);

    /* Create SDP structures for incoming SDP and the SDP we generate. */
    sdp_in_p  = sdp_init_description(conf_p);

    sdp_set_string_debug(sdp_in_p, "\nSDP Test (Incoming):");

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

    sdp_show_stats(conf_p);
    SDP_PRINT("\n\n");

    /* Store in a channel cfg */
    if (sdp_in_p != NULL) {
        if (!store_channel_config(sdp_in_p, &channel)) {
            SDP_PRINT("\n Channel Configuration Couldn't be created");
            exit (-1);
        } 
        print_channel_config (&channel);

        /* Free up stuff */
        sdp_free_description(sdp_in_p);
    }

    my_free(conf_p);
    
    exit (0);
}

/* Function:    store_channel_config
 * Description: Reads the parsed sdp configuration and store
 * Parameters:  sdp_in      The SDP handle returned by sdp_init_description.
 *              channel_cfg_t Channel configuration entry 
 * Returns:     True if parsed SDP passes sanity checking, else FALSE
 */
unsigned char store_channel_config (void *sdp_in, channel_cfg_t *channel)
{
    char  *name;
    char  *conn_address;
    char  fbt_address[MAX_IP_ADDR_CHAR], dest_address[MAX_IP_ADDR_CHAR];
    int32 primary_rtp_port, repair_rtp_port;
    char *rtp_enc;
    sdp_rtcp_unicast_mode_e  mode;
    sdp_src_filter_mode_e  filter_mode;
    int32 apt=0;
    u16 inst, i;
    sdp_payload_ind_e indicator;
    u32 payload_num;

    /* Get the name of the channel */
    name = (char *) sdp_get_session_name(sdp_in);
    if (!name) {
        SDP_PRINT("\n The session name is not provided");
        return FALSE;
    }

    /* Get the primary multicast address */
    conn_address = (char *)sdp_get_conn_address(sdp_in, SDP_SESSION_LEVEL);
    if (!conn_address || !sdp_is_mcast_addr(sdp_in, SDP_SESSION_LEVEL)) {
        SDP_PRINT ("\n The connection address not provided in session level");
        return FALSE;
    }
    
    mode = sdp_get_rtcp_unicast_mode(sdp_in, SDP_SESSION_LEVEL, 0, 1);
    if (mode != SDP_RTCP_UNICAST_MODE_RSI) {
        SDP_PRINT ("\n The RTCP unicast RSI is not turned off");
        return FALSE;
    }
    
    /* The first media should be of encoding MP2T */
    rtp_enc = (char *)sdp_attr_get_rtpmap_encname (sdp_in, 1, 0, 1);
    if (strncasecmp(rtp_enc, "MP2T", strlen("MP2T")+1)) {
        SDP_PRINT ("\n The primary payload type is not MPEG2 Transport");
        return FALSE;
    }

    /* Get the payload type number */
    payload_num = sdp_get_media_payload_type (sdp_in, 1, 1, &indicator);

    /* Get the primary RTP port */
    primary_rtp_port = (int32)sdp_get_media_portnum (sdp_in, 1);
    if (primary_rtp_port == SDP_INVALID_VALUE) {
        SDP_PRINT("\n The primary rtp port is invalid");
    }

    /* Get the FBT */
    if (sdp_get_filter_destination_attributes(sdp_in, SDP_SESSION_LEVEL,
                                              0, 1, SDP_NT_INTERNET,
                                              SDP_AT_IP4, dest_address) != SDP_SUCCESS) {
        SDP_PRINT("\n Could not get destination address");
    }

    filter_mode = sdp_get_source_filter_mode(sdp_in, SDP_SESSION_LEVEL, 0, 1);
    
    /* The destination address in the filter should be equal to 
     * the address in the connection line */
    if ((!strncmp (conn_address, dest_address, 100)) && 
        (filter_mode == SDP_SRC_FILTER_INCL)) {
        if (sdp_get_filter_source_address(sdp_in, SDP_SESSION_LEVEL, 
                                          0, 1, 0, fbt_address) != SDP_SUCCESS) {
            SDP_PRINT("\n Could not get source address");
        }
    } else {
        SDP_PRINT("\nInvalid Feedback target address");
        return FALSE;
    }

    /* The second media should be of encoding 'rtx' */
    rtp_enc = (char *)sdp_attr_get_rtpmap_encname(sdp_in, 2, 0, 1);
    if (rtp_enc != NULL) {
        if (strncmp(rtp_enc, "rtx", strlen("rtx")+1)) {
            SDP_PRINT ("The media encoding of 2nd media instance should be 'rtx'");
            return FALSE;
        }
    }

    /* Get the repair RTP/RTCP ports */
    repair_rtp_port = (int32)sdp_get_media_portnum (sdp_in, 2);
    if (repair_rtp_port == SDP_INVALID_VALUE) {
        SDP_PRINT("\n Invalid repair RTP port");
        return FALSE;
    }

    /* check apt value exists and corresponds to the primary payload
     * number */
    if (sdp_attr_num_instances(sdp_in,2, 0, SDP_ATTR_FMTP, &inst) 
        == SDP_SUCCESS) { 
        for (i = 1; i < inst+1; i++) {
            if (sdp_attr_fmtp_get_fmtp_format (sdp_in, 2, 0, i) == 
                SDP_FMTP_RTP_RETRANS) {
                if (sdp_attr_get_fmtp_apt (sdp_in, 2, 0, i) > 0) {
                    apt = sdp_attr_get_fmtp_apt (sdp_in, 2, 0, i);
                    /*  SDP_PRINT( "\nThe apt value is %u", apt); */
                }
            }
        }
    }

    if (!apt) {
        SDP_PRINT ("\napt value unspecified");
        return FALSE;
    }
    if (apt != payload_num) {
        SDP_PRINT ("\n apt value doesn't correspond to primary payload type");
        return FALSE;
    }

    /* We are here, that means we passed all the sanity checking in the SDP
     * configuration */
    strncpy ((char *)channel->name, name, strlen(name));
    strncpy((char *)channel->primary_ip_addr, conn_address, strlen(conn_address));
    channel->primary_rtp_port = primary_rtp_port;
    channel->primary_rtcp_port = channel->primary_rtp_port + 1;
    strncpy((char *)channel->fbt_address,fbt_address, strlen(fbt_address));
    channel->repair_rtp_port = repair_rtp_port;
    channel->repair_rtcp_port = repair_rtp_port + 1;
    return (TRUE);

}

/* Function:    print_channel_config
 * Description: prints out the channel params store in passed channel config
 * Parameters:  channel_cfg_t Channel configuration entry 
 * Returns:     None
 */
void print_channel_config (channel_cfg_t *channel)
{
    SDP_PRINT("\n The session name: %s", channel->name);
    SDP_PRINT("\n The Primary Address: %s", (char *)channel->primary_ip_addr);
    SDP_PRINT("\n The Primary RTP port is: %d", channel->primary_rtp_port);
    SDP_PRINT("\n The Primary RTP port is: %d", channel->primary_rtcp_port);
    SDP_PRINT("\n The FBT Address: %s", (char *)channel->fbt_address);
    SDP_PRINT("\n The Repair RTP port is: %d", channel->repair_rtp_port);
    SDP_PRINT("\n The Repari RTP port is: %d\n\n", channel->repair_rtcp_port);
}


void *my_malloc (unsigned bytes)
{
    return( malloc(bytes));
}

void my_free (void *ptr)
{
    free (ptr);
}
