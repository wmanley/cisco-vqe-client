/*------------------------------------------------------------------
 * VQE PCAP functions
 *
 * July 2008, Dong Hsu
 *
 * Copyright (c) 2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <string.h>
#include "vqe_pcap.h"
#include "vam_time.h"

#define	PCAP_MAGIC		0xa1b2c3d4
#define MIN_ETHERNET_LENGTH     60

/* sa_ignore DISABLE_RETURNS fflush */

/* Link-layer type; see net/bpf.h for details */
static unsigned long pcap_link_type = 1;   /* Default is DLT-EN10MB */

static hdr_ethernet_t HDR_ETHERNET = {
    {0x02, 0x02, 0x02, 0x02, 0x02, 0x02},
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
    0x8};

static hdr_ip_t HDR_IP = {0x45, 0, 0, 0x3412, 0, 0, 0xff, 
                          17, 0, 0x100007f, 0x200007f};

static hdr_udp_t HDR_UDP = {0x7820, 0x7820, 0, 0};

static uint16_t checksum (void *buf, unsigned long count)
{
    unsigned long sum = 0;
    uint16_t *addr = buf;

    while( count > 1 )  {
        /*  This is the inner loop */
        sum += ntohs(* (uint16_t *) addr);
	addr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if( count > 0 ) {
        sum += ntohs(* (uint8_t *) addr);
    }

    /*  Fold 32-bit sum to 16 bits */
    while (sum>>16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return htons(~sum);
}


/**
 * Open a PCAP file based on the given filename
 *
 * @param pcal_filename Name of the pcap file
 * @return a successful opened file pointer
 */
FILE *vqe_pcap_open_file (const char *pcap_filename)
{
    FILE *fp;
    pcap_hdr_t pcap_file_hdr;

    fp = fopen(pcap_filename, "w");
    if (fp == NULL) {
        fprintf(stderr, "vqe_pcap_open_file:: Cannot open file [%s] for "
                "writing\n", pcap_filename);
        return NULL;
    }

    pcap_file_hdr.magic = htonl(PCAP_MAGIC);
    /* current "libpcap" format is 2.4 */
    pcap_file_hdr.version_major = htons(2);
    pcap_file_hdr.version_minor = htons(4);
    pcap_file_hdr.thiszone = htonl(0);
    pcap_file_hdr.sigfigs = htonl(0);
    pcap_file_hdr.snaplen = htonl(USHRT_MAX);
    pcap_file_hdr.network = htonl(pcap_link_type);
    if (fwrite(&pcap_file_hdr, 1, sizeof(pcap_file_hdr), fp)
        != sizeof(pcap_file_hdr)) {
        fclose(fp);
        return NULL;
    }
    fflush(fp);

    return fp;
}


/**
 * Close the PCAP file
 *
 * @param fp - file pointer
 * @return Success or failure
 */
vqe_pcap_ret_t vqe_pcap_close_file (FILE *fp)
{
    if (fp) {
        if (fclose(fp) == EOF) {
            return VQE_PCAP_FAILURE;
        } else {
            return VQE_PCAP_SUCCESS;
        }
    } else {
        return VQE_PCAP_SUCCESS;        
    }
}


/**
 * Write the packet in udp to the PCAP file
 *
 * @param fp - file pointer
 * @param data_p - packet data pointer
 * @param length - packet data length
 * @return Status for packet writing
 */
vqe_pcap_ret_t vqe_pcap_udp_write (FILE *fp, char *data_p, uint32_t data_len)
{
    pcaprec_hdr_t pcap_rec_hdr;
    int length = 0;
    int udp_length = 0;
    int ip_length = 0;
    int eth_trailer_length = 0;
    char tempbuf[MIN_ETHERNET_LENGTH];
    abs_time_t now = get_sys_time();
    struct timeval tv = TIME_GET_A(timeval, now);

    if (fp == NULL) {
        fprintf(stderr, "vqe_pcap_write:: File is not open for write\n");
        return VQE_PCAP_FILE_NOT_OPEN;
    }

    if (data_p == NULL || data_len == 0) {
        fprintf(stderr, "vqe_pcap_write:: No data for write\n");
        return VQE_PCAP_NO_DATA_TO_WRITE;
    }

    /* Compute packet length */
    length = data_len;
    length += sizeof(HDR_UDP); /* Add UDP header length */
    udp_length = length;
    length += sizeof(HDR_IP); /* Add IP header length */
    ip_length = length;
    length += sizeof(HDR_ETHERNET); /* Add Ethernet header length */
    /* Pad trailer if the length is less than minimum required */
    if (length < MIN_ETHERNET_LENGTH) {
        eth_trailer_length = MIN_ETHERNET_LENGTH - length;
        length = MIN_ETHERNET_LENGTH;
    }

    /* Write PCap header */
    pcap_rec_hdr.ts_sec = htonl(tv.tv_sec);
    pcap_rec_hdr.ts_usec = htonl(tv.tv_usec);
    pcap_rec_hdr.incl_len = htonl(length);
    pcap_rec_hdr.orig_len = htonl(length);
    fwrite(&pcap_rec_hdr, sizeof(pcap_rec_hdr), 1, fp);
    
    /* Write Ethernet header */
    fwrite(&HDR_ETHERNET, sizeof(HDR_ETHERNET), 1, fp);

    /* Write IP header */
    HDR_IP.packet_length = htons(ip_length);
    HDR_IP.hdr_checksum = htons(checksum(&HDR_IP, sizeof(HDR_IP)));
    fwrite(&HDR_IP, sizeof(HDR_IP), 1, fp);

    /* Write UDP header */
    HDR_UDP.length = htons(udp_length);
    HDR_UDP.checksum = htons(0);
    fwrite(&HDR_UDP, sizeof(HDR_UDP), 1, fp);

    /* Write packet */
    fwrite(data_p, data_len, 1, fp);

    /* Write Ethernet trailer */
    if (eth_trailer_length > 0) {
        memset(tempbuf, 0, eth_trailer_length);
        fwrite(tempbuf, eth_trailer_length, 1, fp);
    }
    fflush(fp);

    return VQE_PCAP_SUCCESS;
}
