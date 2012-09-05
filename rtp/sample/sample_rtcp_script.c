/**------------------------------------------------------------------------
 * @brief
 * RTCP "script": text commands to modify RTCP compound pkt transmission.
 *
 * @file
 * sample_rtcp_script.c
 *
 * July 2007, Mike Lague.
 *
 * Copyright (c) 2007, 2009 by cisco Systems, Inc.
 * All rights reserved.
 *-------------------------------------------------------------------------
 */

#include <ctype.h>
#include "sample_rtcp_script.h"

/*
 * next_rtcp_script_cmd
 *
 * Read the next text command from the specified RTCP script file,
 * wrapping around to beginning, if requested.
 *
 * Parameters:
 * file        -- stream ptr (for an open file)
 * wrap        -- if TRUE, rewind to beginning of file on EOF.
 * buffer      -- ptr to buffer for the text command
 * buflen      -- length of buffer, in bytes.
 * Returns:    -- TRUE, if a command was successfully read into buffer;
 *                else FALSE.
 */
boolean next_rtcp_script_cmd (FILE *file, boolean wrap, 
                              char *buffer, int buflen)
{
    char *s = fgets(buffer, buflen, file);
    if (!s && wrap) {
        rewind(file);
        s = fgets(buffer, buflen, file);
    }
    return (s ? TRUE : FALSE);
}

/*
 * get_token_code
 *
 * Return a code corresponding to a token, by doing a case-insensitive
 * comparison against an array of pointers to valid tokens.
 *
 * Parameters:
 * token     -- ptr to null-terminated string, representing the token
 * list      -- array of ptrs to strings representing valid tokens;
 *              all strings are assumed to be lower-case; the last
 *              ptr in the array must be NULL, to mark the end of the list.
 * Returns:  -- idx+1, if the token is valid, and token matches list[idx];
 *              else 0, if the token is not valid.
 */

int get_token_code (char *token, char **list)
{
    int i;
    char ltoken[TOKENSIZ];

    for (i = 0; i < (TOKENSIZ - 1) && token[i] ; i++) {
        ltoken[i] = tolower(token[i]);
    }
    ltoken[i] = '\0';

    for (i = 0; list[i] != NULL ; i++) {
        if (strcmp(ltoken, list[i]) == 0) {
            break;
        }
    }
    if (list[i]) {
        return (i+1) ;
    } else {
        return (0);
    }
}

/*
 * parse_action
 *
 * Return a code correspoding to a token for an RTCP script "action".
 *
 * Parameters:
 * token         -- null-terminated string
 * Returns:      -- > 0, if token is a valid action.
 *                  = 0, if token is invalid.
 */
static char *action_list[] = 
    { "send", 
      "skip", 
      NULL 
    } ;

static rtcp_script_action_t parse_action (char *token)
{
    return ((rtcp_script_action_t)get_token_code(token, action_list));
}
        
/*
 * parse_qualifier
 *
 * Return a code corresponding to a token for an RTCP script "qualifier".
 *
 * Parameters:
 * token         -- null-terminated string
 * Returns:      -- > 0, if token is a valid qualifier.
 *                  = 0, if token is invalid.
 */

static char *qualifier_list[] = 
    { "with", 
      "without", 
      NULL
    } ;

static rtcp_qualifier_t parse_qualifier (char *token)
{
    return ((rtcp_qualifier_t)get_token_code(token, qualifier_list));
}
        
/*
 * parse_scope
 *
 * Return a code corresponding to a token for an RTCP script "scope".
 * If the scope is "msg", return the RTCP packet type for the indicated msg.
 *
 * Parameters:
 * token         -- null-terminated string
 * msg_type      -- on output, if scope is "msg", this points to the
 *                  specific RTCP packet type, else it points to 0.
 * Returns:      -- > 0, if token is a valid scope. One of:
 *                       RTCP_SCOPE_PKTLEN
 *                       RTCP_SCOPE_NEWMSG
 *                       RTCP_SCOPE_ALLMSG
 *                       RTCP_SCOPE_MSG
 *                  = 0, if token is invalid.
 */

static char *scope_list[] = 
    { "len",
      "msg",
      "all",
      
      "sr",
      "rr",
      "sdes",
      "bye",
      "app",
      "rtpfb",
      "psfb",
      "xr",
      "rsi",
      "pubports"
    };

static rtcp_scope_t parse_scope (char *token,
                                 rtcp_type_t *msg_type)
{
    rtcp_scope_t code = (rtcp_scope_t)get_token_code(token, scope_list);
    switch (code) {
    case RTCP_SCOPE_PKTLEN:
    case RTCP_SCOPE_NEWMSG:
    case RTCP_SCOPE_ALLMSG:
        *msg_type = 0;
        break;

    case RTCP_SCOPE_SR:
        *msg_type = RTCP_SR;
        break;
    case RTCP_SCOPE_RR:
        *msg_type = RTCP_RR;
        break;
    case RTCP_SCOPE_SDES:
        *msg_type = RTCP_SDES;
        break;
    case RTCP_SCOPE_BYE:
        *msg_type = RTCP_BYE;
        break;
    case RTCP_SCOPE_APP:
        *msg_type = RTCP_APP;
        break;
    case RTCP_SCOPE_RTPFB:
        *msg_type = RTCP_RTPFB;
        break;
    case RTCP_SCOPE_PSFB:
        *msg_type = RTCP_PSFB;
        break;
    case RTCP_SCOPE_XR:
        *msg_type = RTCP_XR;
        break;
    case RTCP_SCOPE_RSI:
        *msg_type = RTCP_RSI;
        break;
    case RTCP_SCOPE_PUBPORTS:
        *msg_type = RTCP_PUBPORTS;
        break;
    case RTCP_SCOPE_NONE:
    default:
        *msg_type = 0;
        break;
    }

    return (*msg_type ? RTCP_SCOPE_MSG : code);
}

/*
 * parse_field
 *
 * Return a code corresponding to a token for an RTCP script 
 * (header) "field".
 *
 * Parameters:
 * token         -- ptr to a null-terminated string.
 * Returns:      -- > 0, if token is a valid "field".
 *                  = 0, if token is invalid.
 */
static char *field_list[] = 
    { "v",
      "p",
      "c",
      "pt",
      "len",
      "ssrc",
      "data",
      NULL
    };
      
static rtcp_field_t parse_field (char *token)
{
    return ((rtcp_field_t)get_token_code(token, field_list));
}

/*
 * parse_byte_stream
 *
 * Convert a text string representing a byte stream into a byte array.
 *
 * Parameters:
 * token        -- ptr to null-terminated string, of the form
 *                 "01.02.03.ab", i.e., a repeating pattern of two hex digits
 *                 followed by a separator (typically, '.', but any 
 *                 character that is not a hex digit will do). 
 *                 No final separator is needed. 
 * output       -- ptr to byte array
 * output_size  -- length (in bytes) of byte array
 * Returns:     -- no. of output bytes.
 */
uint32_t parse_byte_stream (char *token,
                            uint8_t *output,
                            int output_size)
{
    uint8_t byte = 0;
    uint8_t *p = output;
    int nibble = 0;
    int i = 0;
    char c = 0;

    for (i = 0; token[i] && i < output_size; i++) {
        c = token[i] ;
        if (c >= '0' && c <= '9') {
            nibble++;
            byte = (byte << 4) | (c - '0');
        } else if  (c >= 'a' && c <= 'f') {
            nibble++;
            byte = (byte << 4) | ((c - 'a') + 10);
        } else if (c >= 'A' && c <= 'F') {
            nibble++;
            byte = (byte << 4) | ((c - 'A') + 10);
        } else if (nibble) {
            *p++ = byte;
            nibble = 0;
            byte = 0;
        }
    }
    if (nibble) {
        *p++ = byte;
    }
    return (p - output);
}

                                   
/*
 * parse_field_value
 *
 * Parse tokens from a "<field>=<value>" expression.
 * If <field> is "data", <value> is expected to be a byte stream,
 * else <value> is expected to be a hex or decimal integer.
 *
 * Parameters:
 * field         -- a token for an RTCP script "field".
 * value         -- a token for the corresponding field value.
 * p_modifier    -- ptr to "modifier" struct in which to store the results.
 */
static void parse_field_value (char *field,
                               char *value,
                               rtcp_msg_modifier_t *p_modifier)
{
    rtcp_field_t code = RTCP_FIELD_NONE;
    char *p_end = NULL;

    code = parse_field(field);
    switch (code) {
    case RTCP_FIELD_NONE:
        break;
    case RTCP_FIELD_DATA:
        p_modifier->field_mask |= (1 << code);
        p_modifier->msg_data_len = 
            parse_byte_stream(value, p_modifier->msg_data, RTCP_MSG_DATA_SIZE);
        break;
    default:
        p_modifier->field_mask |= (1 << code);
        p_modifier->field_value[code] = strtoul(value, &p_end, 0);
        break;
    }
}

/*
 * parse_msg_modifier
 *
 * Parse an RTCP script "message modifier" token:
 * If the specified qualifier is RTCP_QUALIFIER_WITHOUT, the token
 * is expected to be of the form
 *    <scope>
 * else it is expected to be of the form
 *    <scope>:<field>=<value>
 *
 * Parameters:
 * token       -- ptr to null-terminated "message modifier" string
 * qualifier   -- RTCP_QUALIFIER_WITH or RTCP_QUALIFIER_WITHOUT
 * cmd         -- ptr to struct in which to store the results.
 */
void parse_msg_modifier (char *token,
                         rtcp_qualifier_t qualifier,
                         rtcp_script_cmd_t *cmd)
{
    char scope[TOKENSIZ];
    char field[TOKENSIZ];
    char value[TOKENSIZ];

    int num_subtokens = 0;
    boolean with = FALSE;
    rtcp_scope_t scope_code = 0;
    rtcp_type_t msg_type = 0;
    rtcp_msg_modifier_t *p_modifier = NULL;

    switch (qualifier) {
    case RTCP_QUALIFIER_WITHOUT:
        with = FALSE;
        break;
    case RTCP_QUALIFIER_WITH:
    default:
        with = TRUE;
        break;
    }

    num_subtokens = sscanf(token, "%32[^:]:%32[^=]=%1024s", scope, field, value);
    if (num_subtokens < 1) {
        return;
    }
    p_modifier = NULL;
    scope_code = parse_scope(scope, &msg_type);
    switch (scope_code) {
    case RTCP_SCOPE_MSG:
        if (!with) {
            rtcp_set_msg_mask(&cmd->omit_msgs, msg_type);
            return;
        }
        if (num_subtokens < 3) {
            return;
        }
        rtcp_set_msg_mask(&cmd->modify_msgs, msg_type);
        p_modifier = &cmd->modifiers[RTCP_MSGTYPE_IDX(msg_type)];
        break;
    case RTCP_SCOPE_NEWMSG:
        if (!with) {
            return;
        }
        if (num_subtokens < 3) {
            return;
        }
        cmd->new_msg = TRUE;
        p_modifier = &cmd->new_msg_data;
        break;
    case RTCP_SCOPE_ALLMSG:
        if (!with) {
            cmd->omit_msgs = 0xffffffff;
            return;
        }
        if (num_subtokens < 3) {
            return;
        }
        cmd->global_chg = TRUE;
        p_modifier = &cmd->all_msg_data;
        break;
    case RTCP_SCOPE_PKTLEN:
        if (!with) {
            return;
        }
        if (num_subtokens < 2) {
            return;
        }
        cmd->chg_len = TRUE;
        cmd->length = atoi(field);
        break;
    default:
        break;
    }

    if (p_modifier) {
        parse_field_value(field, value, p_modifier);
    }
}

/*
 * parse_rtcp_script_cmd
 *
 * Parse an RTCP script command, of the general form
 * <action> [<qualifier> <scope>:<fld>=<val> [ ... <scopeN>:<fldN>=<valN>]]
 *
 * Parameters:
 * buffer         -- ptr to null-terminated string
 * cmd            -- ptr to struct in which to store results
 */
boolean parse_rtcp_script_cmd (char *buffer, rtcp_script_cmd_t *cmd) 
{
    char *t;
    char *saveptr;
    rtcp_qualifier_t qualifier = RTCP_QUALIFIER_NONE;

    memset(cmd, 0, sizeof(rtcp_script_cmd_t));

    t = strtok_r(buffer, " \n", &saveptr);
    cmd->action = RTCP_SCRIPT_NONE;
    if (t) {
        cmd->action = parse_action(t);
    }
    if (cmd->action == RTCP_SCRIPT_NONE) {
        return (FALSE);
    }

    t = strtok_r(NULL, " \n", &saveptr);
    if (t) {
        qualifier = parse_qualifier(t);
    }
    if (qualifier == RTCP_QUALIFIER_NONE) {
        return (TRUE);
    }

    while ((t = strtok_r(NULL, " \n", &saveptr)) != NULL) {
        parse_msg_modifier(t, qualifier, cmd);
    }
    return (TRUE);
}

static rtcp_type_t msg_list[] = 
    { 
        RTCP_SR, 
        RTCP_RR, 
        RTCP_SDES,
        RTCP_PUBPORTS,
        RTCP_APP,
        RTCP_RTPFB,
        RTCP_PSFB,
        RTCP_XR,
        RTCP_RSI,
        RTCP_BYE
    } ;

#define NUM_MSGTYPES (sizeof(msg_list) / sizeof(rtcp_type_t))

/*
 * rtcp_find_msg
 *
 * Find the beginning of an RTCP message with the specified packet type,
 * within a compound packet buffer.
 *
 * Parameters:
 * buffer     -- ptr to compound packet buffer
 * buflen     -- size in bytes of buffer
 * type       -- RTCP packet type to search for
 * Returns:   -- ptr to start of individual RTCP packet, or NULL if not found.
 */

static uint8_t *rtcp_find_msg (uint8_t *buffer,
                               uint32_t buflen,
                               rtcp_type_t type)
{
    rtcptype *p_rtcp = (rtcptype *)buffer;
    int len = (int)buflen;
    uint16_t params, len_hs;

    if (len < sizeof(rtcptype)) {
        return (NULL);
    }
    params = ntohs(p_rtcp->params);
    while (rtcp_get_type(params) != type) {
        len_hs = ntohs(p_rtcp->len);
        len -= ((len_hs + 1) * 4);
        if (len < sizeof(rtcptype)) {
            break;
        }
        p_rtcp = (rtcptype *) ((uint8_t *)p_rtcp + (len_hs + 1) * 4);
        params = ntohs(p_rtcp->params);
    }

    return (len >= sizeof(rtcptype) ? (uint8_t *)p_rtcp : NULL);
}

/*
 * rtcp_copy_msg
 *
 * Copy an individual RTCP packet from one buffer to another.
 *
 * Parameters:
 * msg        -- ptr to start of packet
 * out        -- ptr to destination buffer
 * Returns:   -- no. of bytes copied
 */
static uint32_t rtcp_copy_msg (uint8_t *msg,
                               uint8_t *out)
{
    rtcptype *p_rtcp = (rtcptype *)msg;
    int wlen = ntohs(p_rtcp->len) + 1 ;
    uint32_t *src = (uint32_t *)msg;
    uint32_t *dst = (uint32_t *)out;
    int i = 0;
    
    for (i = 0; i < wlen ; i++) {
        *dst++ = *src++;
    }

    return (wlen * sizeof(uint32_t));
}

/*
 * rtcp_init_msg
 *
 * Initialize an RTCP msg (packet) header with the packet type
 *
 * Parameters:
 * type            -- RTCP message type 
 * out             -- RTCP header buffer
 * Returns:        -- size in bytes of RTCP header.
 */
uint32_t rtcp_init_msg (rtcp_type_t type,
                        uint8_t *out)
{
    rtcptype *p_rtcp = (rtcptype *)out;
    uint16_t params = 0;


    memset(out, 0, sizeof(rtcptype));
    params = rtcp_set_version(params);
    params = rtcp_set_type(params, type);
    p_rtcp->params = htons(params);

    return (sizeof(rtcptype));
}

/*
 * rtcp_modify_msg
 *
 * Modify an individual RTCP msg (packet), 
 * according to a "message modifier" in a RTCP script command
 *
 * Parameters:
 * msg        -- RTCP packet buffer
 * outlen     -- current length in bytes of the packet
 * Returns:   -- new length in bytes of the packet
 */
static uint32_t rtcp_modify_msg (uint8_t *msg,
                                 uint32_t outlen,
                                 rtcp_msg_modifier_t *p_modifier)
{
    int i;
    rtcptype *p_rtcp = (rtcptype *)msg;
    uint16_t params = 0;
    uint32_t rlen = outlen;
    uint16_t value;

    for (i = RTCP_FIELD_VERSION ; i <= RTCP_FIELD_DATA ; i++) {
        if (p_modifier->field_mask & (1 << i)) {

            value = p_modifier->field_value[i];
            switch (i) {
            case RTCP_FIELD_VERSION:
                params = ntohs(p_rtcp->params) & 0x3FFF;
                params = params | ((value & 3) << 14) ;
                p_rtcp->params = htons(params);
                break;
            case RTCP_FIELD_PADDING:
                params = ntohs(p_rtcp->params) & 0xDFFF;
                params = params | ((value & 1) << 13) ;
                p_rtcp->params = htons(params);
                break;
            case RTCP_FIELD_COUNT:
                params = ntohs(p_rtcp->params) & 0xE0FF;
                params = rtcp_set_count(params, value);
                p_rtcp->params = htons(params);
                break;
            case RTCP_FIELD_PT:
                params = ntohs(p_rtcp->params) & 0xFF00;
                params = rtcp_set_type(params, value);
                p_rtcp->params = htons(params);
                break;
            case RTCP_FIELD_LEN:
                p_rtcp->len = htons(value);
                break;
            case RTCP_FIELD_SSRC:
                p_rtcp->ssrc = htonl(p_modifier->field_value[i]);
                break;
            case RTCP_FIELD_DATA:
                memcpy(p_rtcp, p_modifier->msg_data, p_modifier->msg_data_len);
                rlen = p_modifier->msg_data_len;
                break;
            }
        }
    }

    return (rlen);
}

/*
 * process_rtcp_script_cmd
 *
 * Modify an RTCP compound packet according to an RTCP script command
 * 
 * Parameters:
 * buffer       -- ptr to compound packet buffer
 * buflen       -- length in bytes of compound packet
 * cmd          -- ptr to parsed form of script cmd
 * Returns      -- new length of compound packet:
 *                 0 means the packet should not be sent.
 */
uint32_t process_rtcp_script_cmd (uint8_t *buffer, 
                                  uint32_t bufflen,
                                  rtcp_script_cmd_t *cmd)
{
    int i;
    uint8_t outbuf[RTCP_PAK_SIZE];
    uint32_t outlen = 0;
    uint8_t *msg = NULL;
    uint8_t *out = outbuf;
    boolean omit = FALSE;
    boolean modify = FALSE;
    rtcp_type_t type = 0;

    if (cmd->action == RTCP_SCRIPT_SKIP) {
        return (0);
    }

    for (i = 0 ; i < NUM_MSGTYPES ; i++) {
        type = msg_list[i];
        msg = rtcp_find_msg(buffer, bufflen, type);
        omit = rtcp_is_msg_set(cmd->omit_msgs, type);
        modify = rtcp_is_msg_set(cmd->modify_msgs, type);
        if (msg && !omit) {
            outlen = rtcp_copy_msg(msg, out);
        } else if (modify) {
            outlen = rtcp_init_msg(type, out);
        } else {
            outlen = 0;
        }
        if (modify) {
            outlen = rtcp_modify_msg(out, outlen, 
                                     &cmd->modifiers[RTCP_MSGTYPE_IDX(type)]);
        }
        if (cmd->global_chg && outlen) {
            outlen = rtcp_modify_msg(out, outlen, 
                                     &cmd->all_msg_data);
        }
        out += outlen;
    }

    if (cmd->new_msg) {
        outlen = rtcp_modify_msg(out, 0, &cmd->new_msg_data);
        out += outlen;
    }

    outlen = out - outbuf;
    if (cmd->chg_len) {
        if (cmd->length > 0) {
            outlen = cmd->length;
        } else if (cmd->length < 0) {
            outlen += cmd->length;
        }
    }

    memcpy(buffer, outbuf, outlen);
    return (outlen);
}
