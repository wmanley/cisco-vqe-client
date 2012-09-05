/*------------------------------------------------------------------
 * VQE PCAP functions
 *
 * July 2008, Dong Hsu
 *
 * Copyright (c) 2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */


#ifndef __VQE_PCAP_H__
#define __VQE_PCAP_H__

#include "vam_types.h"
#include "vam_util.h"

#define VQM_DEFAULT_PORT 8312

typedef struct {
    uint8_t  dest_addr[6];
    uint8_t  src_addr[6];
    uint16_t l3pid;
} hdr_ethernet_t;

typedef struct {
    uint8_t  ver_hdrlen;
    uint8_t  dscp;
    uint16_t packet_length;
    uint16_t identification;
    uint8_t  flags;
    uint8_t  fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t hdr_checksum;
    uint32_t src_addr;
    uint32_t dest_addr;
} hdr_ip_t;

typedef struct {
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} hdr_udp_t;

/* "libpcap" file header (minus magic number). */
typedef struct pcap_hdr {
    uint32_t	magic;		/* magic */
    uint16_t	version_major;	/* major version number */
    uint16_t	version_minor;	/* minor version number */
    uint32_t	thiszone;	/* GMT to local correction */
    uint32_t	sigfigs;	/* accuracy of timestamps */
    uint32_t	snaplen;	/* max length of captured packets, in octets */
    uint32_t	network;	/* data link type */
} pcap_hdr_t;

/* "libpcap" record header. */
typedef struct pcaprec_hdr {
    uint32_t    ts_sec;		/* timestamp seconds */
    uint32_t	ts_usec;	/* timestamp microseconds */
    uint32_t	incl_len;	/* number of octets of packet saved in file */
    uint32_t	orig_len;	/* actual length of packet */
} pcaprec_hdr_t;

typedef enum vqe_pcap_ret_ {
    VQE_PCAP_SUCCESS,
    VQE_PCAP_FAILURE,
    VQE_PCAP_FILE_NOT_OPEN,
    VQE_PCAP_NO_DATA_TO_WRITE,
    VQE_PCAP_EXCEED_MAX
} vqe_pcap_ret_t;



/**
 * Open a PCAP file based on the given filename
 *
 * @param pcal_filename Name of the pcap file
 * @return a successful opened file pointer
 */
extern FILE *vqe_pcap_open_file(const char *pcap_filename);

/**
 * Close the PCAP file
 *
 * @param fp - file pointer
 * @return Success or failure
 */
vqe_pcap_ret_t vqe_pcap_close_file(FILE *fp);

/**
 * Write the entire packet (no fragmented ) in udp to the PCAP file
 *
 * @param fp - file pointer
 * @param data_p - packet data pointer
 * @param length - packet data length
 * @return Status for packet writing
 */
vqe_pcap_ret_t vqe_pcap_udp_write(FILE *fp, char *data_p, uint32_t length);

#endif /* __VQE_PCAP_H__ */
