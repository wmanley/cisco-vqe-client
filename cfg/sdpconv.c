/*
 *------------------------------------------------------------------
 * Channel format conversion functions for VCPT
 *
 * April 2007, Dong Hsu
 *
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <string.h>
#include "sdpconv.h"
#include "cfg_channel.h"

static FILE *fp_g;

#define ORIG_PKT_SIZE 800

/* Function:    gen_xml_hdr
 * Description: Create the header of the xml file
 */
void gen_xml_hdr (const char *xmlFilename)
{
    fp_g = (void *) fopen(xmlFilename, "write");
    if (fp_g == NULL) {
        printf("Could not open the file %s\n",
               xmlFilename);
        exit (-1);
    }

    fprintf(fp_g, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp_g, 
            "<java version=\"1.6.0\" class=\"java.beans.XMLDecoder\">\n");
    fprintf(fp_g,
            "   <object class=\"com.cisco.iptv.vqe.vcpt.VcptConfig\">\n");

    fprintf(fp_g, "      <void method=\"encodeChannels\">\n");
}

/* Function:    print_name_value_pair
 * Description: Create the body of the xml file for each name and value
 */
void print_name_value_pair (const char* name, const char* value)
{
    fprintf(fp_g, "                  <void method=\"put\">\n");
    fprintf(fp_g, "                     <string>%s</string>\n", name);
    fprintf(fp_g, "                     <string>%s</string>\n", value);
    fprintf(fp_g, "                  </void>\n");
}



/* Function:    gen_xml_channel
 * Description: Create the body of the xml file for each channel
 */
void gen_xml_channel (channel_cfg_t *channel_p)
{
    char buffer[MAX_NAME_LENGTH];
    char tmp[INET_ADDRSTRLEN];
    int max_receivers = 1000;
    int rtcp_interval = 15;
    int prbw_value;
    char username[SDP_MAX_STRING_LEN];
    char sessionid[SDP_MAX_STRING_LEN];
    char creator_addr[SDP_MAX_STRING_LEN];

    if (channel_p) {
        memset(username, 0, SDP_MAX_STRING_LEN);
        memset(sessionid, 0, SDP_MAX_STRING_LEN);
        memset(creator_addr, 0, SDP_MAX_STRING_LEN);
        cfg_channel_parse_session_key(channel_p,
                                      username, 
                                      sessionid,
                                      creator_addr);

        fprintf(fp_g, "         <void method=\"put\">\n");
        fprintf(fp_g, "            <string>%s</string>\n", sessionid);
        fprintf(fp_g, 
                "            <object class=\"com.cisco.iptv.vqe.vcpt.VcptChannel\">\n"); 

        fprintf(fp_g, "               <void method=\"encodeParams\">\n");

        /* Compute max_receivers and rtcp_interval from the bandwidth */
        prbw_value = channel_p->original_rtcp_per_rcvr_bw;
        if (prbw_value != 0) {
            rtcp_interval = ORIG_PKT_SIZE / prbw_value;
            max_receivers = channel_p->original_rtcp_rcvr_bw / prbw_value;
        }

        /* Details for each channel */
        print_name_value_pair("chnlName", channel_p->name);

        snprintf(buffer, MAX_NAME_LENGTH, "%s", username);
        print_name_value_pair("chnlUserName", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%s", sessionid);
        print_name_value_pair("chnlId", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%s", creator_addr);
        print_name_value_pair("chnlHostIp", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%llu", channel_p->version);
        print_name_value_pair("chnlVersion", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%d", max_receivers);
        print_name_value_pair("chnlNumReceivers", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%d", rtcp_interval);
        print_name_value_pair("chnlRtcpInterval", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%s",
                 (channel_p->mode == SOURCE_MODE) ?
                 "Source" : "Lookaside");
        print_name_value_pair("chnlMode", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%s",
                 inet_ntop(AF_INET, &channel_p->original_source_addr,
                           tmp, INET_ADDRSTRLEN));
        print_name_value_pair("chnlOSMcastIp", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%d",
                 ntohs(channel_p->original_source_port));
        print_name_value_pair("chnlOSRtpPort", buffer);
        
        snprintf(buffer, MAX_NAME_LENGTH, "%d",
                 channel_p->original_source_payload_type);
        print_name_value_pair("chnlOSPayloadTyp", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%s",
                 inet_ntop(AF_INET, &channel_p->src_addr_for_original_source,
                           tmp, INET_ADDRSTRLEN));
        print_name_value_pair("chnlOSSrcIp", buffer);


        if (channel_p->source_proto == RTP_STREAM) {
            snprintf(buffer, MAX_NAME_LENGTH, "%d",
                     ntohs(channel_p->original_source_rtcp_port));
            print_name_value_pair("chnlOSRtcpPort", buffer);
        }
        else {
            print_name_value_pair("chnlOSRtcpPort", "0");
        }

        if (channel_p->source_proto == UDP_STREAM) {
            print_name_value_pair("chnlProtocol", "UDP");
        }
        else {
            print_name_value_pair("chnlProtocol", "RTP");
        }

        snprintf(buffer, MAX_NAME_LENGTH, "%d", channel_p->bit_rate/1000);
        print_name_value_pair("chnlOSBitRate", buffer);

        if (channel_p->mode == SOURCE_MODE) {
            snprintf(buffer, MAX_NAME_LENGTH, "%s",
                     inet_ntop(AF_INET, &channel_p->re_sourced_addr,
                               tmp, INET_ADDRSTRLEN));
            print_name_value_pair("chnlRSMcastIp", buffer);

            snprintf(buffer, MAX_NAME_LENGTH, "%d",
                     ntohs(channel_p->re_sourced_rtp_port));
            print_name_value_pair("chnlRSRtpPort", buffer);

            snprintf(buffer, MAX_NAME_LENGTH, "%d",
                     ntohs(channel_p->re_sourced_rtcp_port));
            print_name_value_pair("chnlRSRtcpPort", buffer);
        }
        else {
            print_name_value_pair("chnlRSMcastIp", "...");
        }

        snprintf(buffer, MAX_NAME_LENGTH, "%s",
                 inet_ntop(AF_INET, &channel_p->fbt_address,
                           tmp, INET_ADDRSTRLEN));
        print_name_value_pair("chnlRSSrcIp", buffer);
        
        snprintf(buffer, MAX_NAME_LENGTH, "%s",
                 inet_ntop(AF_INET, &channel_p->fbt_address,
                           tmp, INET_ADDRSTRLEN));
        print_name_value_pair("chnlRtrFbtIp", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%s",
                 inet_ntop(AF_INET, &channel_p->fbt_address,
                           tmp, INET_ADDRSTRLEN));
        print_name_value_pair("chnlRtrSrcIp", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%d",
                 ntohs(channel_p->rtx_rtp_port));
        print_name_value_pair("chnlRtrRtpPort", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%d",
                 ntohs(channel_p->rtx_rtcp_port));
        print_name_value_pair("chnlRtrRtcpPort", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%s",
                 (channel_p->er_enable) ? "yes" : "no");
        print_name_value_pair("chnlErrRpr", buffer);

        snprintf(buffer, MAX_NAME_LENGTH, "%s",
                 (channel_p->fcc_enable) ? "yes" : "no");
        print_name_value_pair("chnlRCC", buffer);

        if (channel_p->fec_enable) {
            print_name_value_pair("chnlErrOptionFECCol", "yes");
            
            snprintf(buffer, MAX_NAME_LENGTH, "%s",
                     inet_ntop(AF_INET, &channel_p->fec_stream1.multicast_addr,
                               tmp, INET_ADDRSTRLEN));
            print_name_value_pair("chnlFECColMcastIp", buffer);

            snprintf(buffer, MAX_NAME_LENGTH, "%s",
                     inet_ntop(AF_INET, &channel_p->fec_stream1.src_addr,
                               tmp, INET_ADDRSTRLEN));
            print_name_value_pair("chnlFECColSrcIp", buffer);

            snprintf(buffer, MAX_NAME_LENGTH, "%d",
                     ntohs(channel_p->fec_stream1.rtp_port));
            print_name_value_pair("chnlFECColRtpPort", buffer);
            
            snprintf(buffer, MAX_NAME_LENGTH, "%d",
                     ntohs(channel_p->fec_stream1.rtcp_port));
            print_name_value_pair("chnlFECColRtcpPort", buffer);

            if (channel_p->fec_mode == FEC_2D_MODE) {
                print_name_value_pair("chnlErrOptionFECRow", "yes");
            
                snprintf(buffer, MAX_NAME_LENGTH, "%s",
                         inet_ntop(AF_INET,
                                   &channel_p->fec_stream2.multicast_addr,
                                   tmp, INET_ADDRSTRLEN));
                print_name_value_pair("chnlFECRowMcastIp", buffer);

                snprintf(buffer, MAX_NAME_LENGTH, "%s",
                         inet_ntop(AF_INET, &channel_p->fec_stream2.src_addr,
                                   tmp, INET_ADDRSTRLEN));
                print_name_value_pair("chnlFECRowSrcIp", buffer);

                snprintf(buffer, MAX_NAME_LENGTH, "%d",
                         ntohs(channel_p->fec_stream2.rtp_port));
                print_name_value_pair("chnlFECRowRtpPort", buffer);
            
                snprintf(buffer, MAX_NAME_LENGTH, "%d",
                         ntohs(channel_p->fec_stream2.rtcp_port));
                print_name_value_pair("chnlFECRowRtcpPort", buffer);
            }
            else {
                print_name_value_pair("chnlErrOptionFECRow", "no");
            }

        }

        if (channel_p->original_rtcp_xr_loss_rle == 0xffff &&
            channel_p->original_rtcp_xr_per_loss_rle == 0xffff &&
            channel_p->original_rtcp_xr_stat_flags == 0 &&
            channel_p->original_rtcp_xr_multicast_acq == 0) {
            print_name_value_pair("chnlXR", "no");
        }
        else {
            print_name_value_pair("chnlXR", "yes");
        }

        if (channel_p->mode == RECV_ONLY_MODE &&
            channel_p->original_rtcp_sndr_bw == 0 &&
            channel_p->original_rtcp_rcvr_bw == 0 &&
            channel_p->original_rtcp_per_rcvr_bw == 0) {
            print_name_value_pair("chnlRtcp", "no");
        }
        else {
            print_name_value_pair("chnlRtcp", "yes");
        }

        if (channel_p->original_rtcp_rsize) {
            print_name_value_pair("chnlRtcpRsize", "yes");
        } else {
            print_name_value_pair("chnlRtcpRsize", "no");
        }

        fprintf(fp_g, "               </void>\n");
        fprintf(fp_g, "            </object>\n");
        fprintf(fp_g, "         </void>\n");
    }
}



/* Function:    gen_xml_end
 * Description: Create the end of the xml file
 */
void gen_xml_end (const char *filename)
{
    fprintf(fp_g, "      </void>\n");

    fprintf(fp_g, "      <void method=\"encodeParams\">\n");

    fprintf(fp_g, "         <void method=\"put\">\n");
    fprintf(fp_g, "            <string>cfgLastUpdate</string>\n");
    fprintf(fp_g, "            <string></string>\n");
    fprintf(fp_g, "         </void>\n");

    fprintf(fp_g, "         <void method=\"put\">\n");
    fprintf(fp_g, "            <string>cfgVersion</string>\n");
    fprintf(fp_g, "            <string>v1.0</string>\n");
    fprintf(fp_g, "         </void>\n");

    fprintf(fp_g, "         <void method=\"put\">\n");
    fprintf(fp_g, "            <string>cfgName</string>\n");
    fprintf(fp_g, "            <string>%s</string>\n", filename);
    fprintf(fp_g, "         </void>\n");

    fprintf(fp_g, "      </void>\n");
    fprintf(fp_g, "   </object>\n");
    fprintf(fp_g, "</java>\n");
    fclose(fp_g);
}

