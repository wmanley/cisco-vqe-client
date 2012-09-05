/*
 * vqec_igmpv3.h - Structure and Constant defintions for IGMP.
 *
 *
 * Copyright (c) 2007 by cisco Systems, Inc.
 * All rights reserved.
 * 
 */

#ifndef VQEC_IGMPV3_H
#define VQEC_IGMPV3_H

/*
 * Since we have to mimick previous versions in case we
 * have old version router/hosts on the network, the following
 * version definition is necessary.
 */
typedef enum igmp_versions {
    IGMP_VERSION1 = 1,
    IGMP_VERSION2 = 2,
    IGMP_VERSION3 = 3
}igmp_versions;

/*
 * IGMP fixed header.
 */
typedef struct igmptype_ {
    uint8_t      type;
    uint8_t      code;
    ushort     checksum;
    uint32_t address;
    char       data[0];
} igmptype;

/*
 * The only new message type for v3 is the IGMP report.
 */
#define IGMP_V3_REPORT_TYPE	0x22

/*
 * IGMP v3 packet definitions.
 */
typedef struct igmp_group_record_ {
    uint8_t        type;
    uint8_t        reserved;
    uint16_t      source_count;
    uint32_t   group;
    uint32_t   source[0];
} igmp_group_record;

typedef struct igmp_v3_report_type_ {
    uint8_t	type; 
    uint8_t	reserved1;
    uint16_t	checksum;
    uint16_t	reserved2;
    uint16_t      group_count;
    igmp_group_record group[0];
} igmp_v3_report_type;

typedef struct igmp_query_type_ {
    uint8_t	type;
    uint8_t	resp_time;
    uint16_t	checksum;
    uint32_t	group;
    uint8_t	reserved:4;
    uint8_t	srsp:1;
    uint8_t	qrv:3;
    uint8_t	qqi;
    uint16_t      source_count;
    uint32_t	source[0];
} igmp_query_type;

/*
 * IGMP v1 and v2 queries, reports and leaves.
 */
typedef struct igmp_v1v2_type_ {
    uint8_t	type;
    uint8_t	resp_time;
    uint16_t	checksum;
    uint32_t	group;
} igmp_v1v2_type;

/*
 * IGMPv3 report types and filter modes.
 */
#define MODE_IS_INCLUDE 1
#define MODE_IS_EXCLUDE 2
#define CHANGE_TO_INCLUDE_MODE 3
#define CHANGE_TO_EXCLUDE_MODE 4
#define ALLOW_NEW_SOURCES 5
#define BLOCK_OLD_SOURCES 6
 
#endif /* VQEC_IGMPV3_H */


           
