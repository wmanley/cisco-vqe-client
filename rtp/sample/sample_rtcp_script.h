/**------------------------------------------------------------------------
 * @brief
 * Declarations/definitions for RTCP "script": cmds to modify RTCP msg xmits.
 *
 * @file
 * sample_rtcp_script.h
 *
 * July 2007, Mike Lague.
 *
 * Copyright (c) 2007, 2009 by cisco Systems, Inc.
 * All rights reserved.
 *-------------------------------------------------------------------------
 */

#ifndef __SAMPLE_RTCP_SCRIPT__
#define __SAMPLE_RTCP_SCRIPT__

#include <stdio.h>
#include "../rtcp.h"

#define TOKENSIZ 1024

typedef enum rtcp_script_action_t_ {
    RTCP_SCRIPT_NONE = 0,
    RTCP_SCRIPT_SEND,
    RTCP_SCRIPT_SKIP
} rtcp_script_action_t;

typedef enum rtcp_qualifier_t_ {
    RTCP_QUALIFIER_NONE = 0,
    RTCP_QUALIFIER_WITH,
    RTCP_QUALIFIER_WITHOUT
} rtcp_qualifier_t;

typedef enum rtcp_field_t_ {
    RTCP_FIELD_NONE = 0,
    RTCP_FIELD_VERSION, /* this must be first */
    RTCP_FIELD_PADDING,
    RTCP_FIELD_COUNT,
    RTCP_FIELD_PT,
    RTCP_FIELD_LEN,
    RTCP_FIELD_SSRC,
    RTCP_FIELD_DATA   /* this must be last */
} rtcp_field_t;

typedef struct rtcp_msg_modifier_t_ {
    uint32_t field_mask;
    uint32_t field_value[RTCP_FIELD_DATA+1];
    uint32_t msg_data_len;
#define RTCP_MSG_DATA_SIZE 1024
    uint8_t  msg_data[RTCP_MSG_DATA_SIZE];
} rtcp_msg_modifier_t;
    
typedef enum rtcp_scope_t_ {
    RTCP_SCOPE_NONE = 0,
    RTCP_SCOPE_PKTLEN,
    RTCP_SCOPE_NEWMSG,
    RTCP_SCOPE_ALLMSG,

    RTCP_SCOPE_SR,
    RTCP_SCOPE_RR,
    RTCP_SCOPE_SDES,
    RTCP_SCOPE_BYE,
    RTCP_SCOPE_APP,
    RTCP_SCOPE_RTPFB,
    RTCP_SCOPE_PSFB,
    RTCP_SCOPE_XR,
    RTCP_SCOPE_RSI,
    RTCP_SCOPE_PUBPORTS,
    
    RTCP_SCOPE_MSG  /* this must be last */
} rtcp_scope_t;

typedef struct rtcp_script_cmd_t_ {
    rtcp_script_action_t action;

    rtcp_msg_mask_t omit_msgs;

    rtcp_msg_mask_t modify_msgs;
    rtcp_msg_modifier_t modifiers[RTCP_NUM_MSGTYPES];

    boolean global_chg;
    rtcp_msg_modifier_t all_msg_data;

    boolean new_msg;
    rtcp_msg_modifier_t new_msg_data;

    boolean chg_len;
    int length;  /* >= 0  => use value as compound pkt len
                     < 0  => decrement compound pkt len */
} rtcp_script_cmd_t;

boolean next_rtcp_script_cmd(FILE *file, boolean wrap, 
                             char *buffer, int buflen);
int get_token_code(char *token, char **list);
uint32_t parse_byte_stream(char *token,
                           uint8_t *output,
                           int output_size);
boolean parse_rtcp_script_cmd(char *buffer, rtcp_script_cmd_t *cmd);
uint32_t process_rtcp_script_cmd(uint8_t *buffer, 
                                 uint32_t bufflen,
                                 rtcp_script_cmd_t *cmd);

#endif  /* __SAMPLE_RTCP_SCRIPT__ */
