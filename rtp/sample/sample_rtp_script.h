/**------------------------------------------------------------------------
 * @brief
 * Declarations/definitions for RTP "script": cmds to modify RTP msg xmits.
 *
 * @file
 * sample_rtp_script.h
 *
 * July 2007, Mike Lague.
 *
 * Copyright (c) 2007-2009 by cisco Systems, Inc.
 * All rights reserved.
 *-------------------------------------------------------------------------
 */

#ifndef __SAMPLE_RTP_SCRIPT__
#define __SAMPLE_RTP_SCRIPT__

#include <stdio.h>
#include "../rtcp.h"
#include "sample_rtcp_script.h"

typedef enum rtp_script_action_t_ {
    RTP_SCRIPT_NONE = 0,
    RTP_SCRIPT_SEND,
    RTP_SCRIPT_SKIP
} rtp_script_action_t;

typedef enum rtp_qualifier_t_ {
    RTP_QUALIFIER_NONE = 0,
    RTP_QUALIFIER_WITH
} rtp_qualifier_t;

typedef enum rtp_field_t_ {
    RTP_FIELD_NONE = 0,
    RTP_FIELD_VERSION, /* this must be first */
    RTP_FIELD_PADDING,
    RTP_FIELD_EXTENSION,
    RTP_FIELD_CC,
    RTP_FIELD_MARKER,
    RTP_FIELD_PT,
    RTP_FIELD_SEQ,
    RTP_FIELD_TS,
    RTP_FIELD_SSRC,
    RTP_FIELD_LAST     /* this must be last */
} rtp_field_t;

typedef enum rtp_ext_field_t_ {
    RTP_EXTF_NONE = 0,
    RTP_EXTF_DATA
} rtp_ext_field_t;

typedef enum rtp_value_type_t_ {
    RTP_VT_REPLACE = 1,
    RTP_VT_INCREMENT,
    RTP_VT_DECREMENT
} rtp_value_type_t;

typedef struct rtp_hdr_modifier_t_ {
    uint32_t field_mask;
    uint32_t field_value[RTP_FIELD_LAST];
    uint32_t value_type[RTP_FIELD_LAST];
} rtp_hdr_modifier_t;
    
typedef struct rtp_ext_hdr_t_ {
    uint32_t ext_data_len;
#define RTP_MSG_DATA_SIZE 1024
    uint8_t  ext_data[RTP_MSG_DATA_SIZE];
} rtp_ext_hdr_t;

typedef struct rtp_csrc_t_ {
    uint32_t num_csrcs;
    uint32_t csrc[MAX_CSRCC];
} rtp_csrc_t;

typedef enum rtp_fec_t_ {
    RTP_FEC_NONE = 0,
    RTP_FEC_SNBASE_LOW_BITS, /* this must be first */
    RTP_FEC_LENGTH_RECOVERY,
    RTP_FEC_E,
    RTP_FEC_PT_RECOVERY,
    RTP_FEC_MASK,
    RTP_FEC_TS_RECOVERY,
    RTP_FEC_X,
    RTP_FEC_D,
    RTP_FEC_TYPE,
    RTP_FEC_INDEX,
    RTP_FEC_OFFSET,
    RTP_FEC_NA,
    RTP_FEC_SNBASE_EXT_BITS,
    RTP_FEC_LAST            /* this must be last */
} rtp_fec_t;

typedef struct rtp_fec_modifier_t_ {
    uint32_t field_mask;
    uint32_t field_value[RTP_FEC_LAST];
    uint32_t value_type[RTP_FEC_LAST];
} rtp_fec_modifier_t;
    
typedef struct rtp_len_t_ {
    int pad;     /* >0 => value is no. of padding bytes to add:
                          all padding bytes except the last are zero;
                          the last padding byte has the padding count.
                     0 => no padding
                    <0 => decrement compound pkt len */
    boolean chg_len;
    int length;  /* >= 0  => use value as compound pkt len
                     < 0  => decrement compound pkt len */
} rtp_len_t;

typedef enum rtp_scope_t_ {
    RTP_SCOPE_NONE = 0,
    RTP_SCOPE_HDR,
    RTP_SCOPE_CSRC,
    RTP_SCOPE_EXT,
    RTP_SCOPE_FEC,
    RTP_SCOPE_PAD,
    RTP_SCOPE_LEN,
    RTP_SCOPE_LAST  /* this must be last */
} rtp_scope_t;

typedef struct rtp_script_cmd_t_ {
    rtp_script_action_t action;
    rtp_hdr_modifier_t  hdr_mods;
    rtp_csrc_t          csrc_info;
    rtp_ext_hdr_t       ext_info;
    rtp_fec_modifier_t  fec_mods;
    rtp_len_t           len_info;
} rtp_script_cmd_t;

boolean next_rtp_script_cmd(FILE *file, boolean wrap, 
                            char *buffer, int buflen);
boolean parse_rtp_script_cmd(char *buffer, rtp_script_cmd_t *cmd);
uint32_t process_rtp_script_cmd(uint8_t *buffer, 
                                uint32_t bufflen,
                                rtp_script_cmd_t *cmd,
                                uint32_t *hdrlen);

#endif  /* __SAMPLE_RTCP_SCRIPT__ */
