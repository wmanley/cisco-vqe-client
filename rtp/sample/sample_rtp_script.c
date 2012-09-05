/**------------------------------------------------------------------------
 * @brief
 * RTP "script": text commands to modify RTP pkt transmission.
 *
 * @file
 * sample_rtp_script.c
 *
 * July 2007, Mike Lague.
 *
 * Copyright (c) 2007-2009 by cisco Systems, Inc.
 * All rights reserved.
 *-------------------------------------------------------------------------
 */

#include <ctype.h>
#include "sample_rtp_script.h"
#include "sample_rtcp_script.h"
#include "../fec_fast_type.h"

/*
 * next_rtp_script_cmd
 *
 * Read the next text command from the specified RTP script file,
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
boolean next_rtp_script_cmd (FILE *file, boolean wrap, 
                              char *buffer, int buflen)
{
    /* just reuse the RTCP version of the function */
    return (next_rtcp_script_cmd(file, wrap, buffer, buflen));
}

/*
 * parse_rtp_action
 *
 * Return a code correspoding to a token for an RTP script "action".
 *
 * Parameters:
 * token         -- null-terminated string
 * Returns:      -- > 0, if token is a valid action.
 *                  = 0, if token is invalid.
 */
static char *rtp_action_list[] = 
    { "send", 
      "skip", 
      NULL 
    } ;

static rtp_script_action_t parse_rtp_action (char *token)
{
    return ((rtp_script_action_t)get_token_code(token, rtp_action_list));
}
        
/*
 * parse_rtp_qualifier
 *
 * Return a code corresponding to a token for an RTP script "qualifier".
 *
 * Parameters:
 * token         -- null-terminated string
 * Returns:      -- > 0, if token is a valid qualifier.
 *                  = 0, if token is invalid. 
 */

static char *rtp_qualifier_list[] = 
    { "with", 
      NULL
    } ;

static rtp_qualifier_t parse_rtp_qualifier (char *token)
{
    return ((rtp_qualifier_t)get_token_code(token, rtp_qualifier_list));
}
        
/*
 * parse_rtp_scope
 *
 * Return a code corresponding to a token for an RTP script "scope".
 *
 * Parameters:
 * token         -- null-terminated string
 * Returns:      -- > 0, if token is a valid scope.
 *                  = 0, if token is invalid.
 */

static char *rtp_scope_list[] = 
    { "hdr",
      "csrc",
      "ext",
      "fec",
      "pad",
      "len",
      NULL
    };

static rtp_scope_t parse_rtp_scope (char *token)
{
    return ((rtp_scope_t)get_token_code(token, rtp_scope_list));
}

/*
 * parse_rtp_hdr_field
 *
 * Return a code corresponding to a token for an RTP script header field
 *
 * Parameters:
 * token         -- ptr to a null-terminated string.
 * Returns:      -- > 0, if token is a valid "field".
 *                  = 0, if token is invalid.
 */

static char *rtp_hdr_field_list[] = 
    { "v",
      "p",
      "x",
      "cc",
      "m",
      "pt",
      "seq",
      "ts",
      "ssrc",
      NULL
    };
      
static rtp_field_t parse_rtp_hdr_field (char *token)
{
    return ((rtp_field_t)get_token_code(token, rtp_hdr_field_list));
}

/*
 * parse_rtp_ext_field
 *
 * Return a code corresponding to a token for an RTP script hdr extension field
 *
 * Parameters:
 * token         -- ptr to a null-terminated string.
 * Returns:      -- > 0, if token is a valid "field".
 *                  = 0, if token is invalid.
 */

static char *rtp_ext_field_list[] = 
    { "data",
      NULL
    };

static rtp_ext_field_t parse_rtp_ext_field (char *token) 
{
    return ((rtp_ext_field_t)get_token_code(token, rtp_ext_field_list));
}

/*
 * parse_rtp_hdr_field_value
 *
 * Parse tokens from a "<field>=<value>" expression, for an RTP hdr modifier.
 * <value> is expected to be a hex or decimal integer (positive or negative)
 *
 * Parameters:
 * field         -- a token for an RTP script header field.
 * value         -- a token for the corresponding field value.
 * p_modifier    -- ptr to "modifier" struct in which to store the results.
 */
static void parse_rtp_hdr_field_value (char *field,
                                       char *value,
                                       rtp_hdr_modifier_t *p_modifier)
{
    rtp_field_t code = RTP_FIELD_NONE;
    char *p_end = NULL;

    code = parse_rtp_hdr_field(field);
    switch (code) {
    case RTP_FIELD_NONE:
        break;
    default:
        p_modifier->field_mask |= (1 << code);
        switch (value[0]) {
        case '+':
            p_modifier->field_value[code] = strtoul(value, &p_end, 0);
            p_modifier->value_type[code] = RTP_VT_INCREMENT;
            break;
        case '-':
            p_modifier->field_value[code] = -(strtoul(value, &p_end, 0));
            p_modifier->value_type[code] = RTP_VT_DECREMENT;
            break;
        default:
            p_modifier->field_value[code] = strtoul(value, &p_end, 0);
            p_modifier->value_type[code] = RTP_VT_REPLACE;
            break;
        }
        break;
    }
}

/*
 * parse_rtp_csrc_values
 *
 * Parse a series of RTP CSRC values.
 * 
 * Parameters:
 * field      -- ptr to null-terminated string of the form
 *               <value>[...,<valueN>]
 *               where values are hex or decimal integers.
 * csrc_info  -- ptr to struct in which to store the results.
 */
static void parse_rtp_csrc_values (char *field, 
                                   rtp_csrc_t *csrc_info)
{
    int i = 0;
    char *search = NULL;
    char *token = NULL;
    char *endptr = NULL;
    char *saveptr = NULL;

    for (search = field, i = 0; i < MAX_CSRCC ; search = NULL, i++) {
        token = strtok_r(search, ",", &saveptr);
        if (token) {
            csrc_info->csrc[i] = strtoul(token, &endptr, 0);
        } else {
            break;
        }
    }
    csrc_info->num_csrcs = i;
}


/*
 * parse_rtp_ext_field_value
 *
 * Parse tokens from a "<field>=<value>" expression, for an RTP extension
 * header modification. If <field> is "data", <value> is expected to be a 
 * byte stream.
 *
 * Parameters:
 * field         -- a token for an RTCP script hdr extension field.
 * value         -- a token for the corresponding field value.
 * ext           -- ptr to "extension" struct in which to store the results.
 */
static void parse_rtp_ext_field_value (char *field, 
                                       char *value, 
                                       rtp_ext_hdr_t *ext) 
{
    rtp_ext_field_t code = RTP_EXTF_NONE;

    code = parse_rtp_ext_field(field);
    switch (code) {
    case RTP_EXTF_NONE:
        break;
    case RTP_EXTF_DATA:
    default:
        ext->ext_data_len = parse_byte_stream(value, 
                                              &ext->ext_data[0], 
                                              RTP_MSG_DATA_SIZE);
        break;
    }
}

/*
 * parse_rtp_fec_field
 *
 * Return a code corresponding to a token for an RTP script FEC field
 *
 * Parameters:
 * token         -- ptr to a null-terminated string.
 * Returns:      -- > 0, if token is a valid "field".
 *                  = 0, if token is invalid.
 */

static char *rtp_fec_field_list[] = 
    { "snb",
      "lenr",
      "e",
      "ptr",
      "mask",
      "tsr",
      "x",
      "d",
      "type",
      "idx",
      "off",
      "na",
      "snbe",
      NULL
    };
      
static rtp_fec_t parse_rtp_fec_field (char *token)
{
    return ((rtp_fec_t)get_token_code(token, rtp_fec_field_list));
}

/*
 * parse_rtp_fec_field_value
 *
 * Parse tokens from a "<field>=<value>" expression, for an RTP FEC modifier.
 * <value> is expected to be a hex or decimal integer (positive or negative)
 *
 * Parameters:
 * field         -- a token for an RTP script header field.
 * value         -- a token for the corresponding field value.
 * p_modifier    -- ptr to "modifier" struct in which to store the results.
 */
static void parse_rtp_fec_field_value (char *field,
                                       char *value,
                                       rtp_fec_modifier_t *p_modifier)
{
    rtp_fec_t code = RTP_FEC_NONE;
    char *p_end = NULL;

    code = parse_rtp_fec_field(field);
    switch (code) {
    case RTP_FEC_NONE:
        break;
    default:
        p_modifier->field_mask |= (1 << code);
        switch (value[0]) {
        case '+':
            p_modifier->field_value[code] = strtoul(value, &p_end, 0);
            p_modifier->value_type[code] = RTP_VT_INCREMENT;
            break;
        case '-':
            p_modifier->field_value[code] = -(strtoul(value, &p_end, 0));
            p_modifier->value_type[code] = RTP_VT_DECREMENT;
            break;
        default:
            p_modifier->field_value[code] = strtoul(value, &p_end, 0);
            p_modifier->value_type[code] = RTP_VT_REPLACE;
            break;
        }
        break;
    }
}

/*
 * parse_rtp_modifier_data
 *
 * Parse an RTP script "message modifier" token, of the general form:
 *    <scope>:<field>=<value>
 * or one of following other forms:
 *    csrc:<val1>[,<val2>[...,<valN>]]
 *    len:<value>
 *
 * Parameters:
 * token       -- ptr to null-terminated "message modifier" string.
 * cmd         -- ptr to struct in which to store the results.
 */
static void parse_rtp_modifier_data (char *token,
                                     rtp_script_cmd_t *cmd)
{
    char scope[TOKENSIZ];
    char field[TOKENSIZ];
    char value[TOKENSIZ];

    int num_subtokens = 0;
    rtp_scope_t scope_code = 0;

    num_subtokens = sscanf(token, "%64[^:]:%64[^=]=%1024s", scope, field, value);
    if (num_subtokens < 2) {
        return;
    }

    scope_code = parse_rtp_scope(scope);
    switch (scope_code) {
    case RTP_SCOPE_HDR:
        if (num_subtokens < 3) {
            return;
        }
        parse_rtp_hdr_field_value(field, value, &cmd->hdr_mods);
        break;
    case RTP_SCOPE_CSRC:
        parse_rtp_csrc_values(field, &cmd->csrc_info);
        break;
    case RTP_SCOPE_EXT:
        if (num_subtokens < 3) {
            return;
        }
        parse_rtp_ext_field_value(field, value, &cmd->ext_info);
        break;
    case RTP_SCOPE_FEC:
        if (num_subtokens < 3) {
            return;
        }
        parse_rtp_fec_field_value(field, value, &cmd->fec_mods);
        break;
    case RTP_SCOPE_PAD:
        cmd->len_info.pad = atoi(field);
        break;
    case RTP_SCOPE_LEN:
        cmd->len_info.chg_len = TRUE;
        cmd->len_info.length = atoi(field);
        if (cmd->len_info.length > RTCP_PAK_SIZE) {
            cmd->len_info.length = RTCP_PAK_SIZE;
        }
        break;
    default:
        break;
    }
}


/*
 * parse_rtp_script_cmd
 *
 * Parse an RTP script command, of the general form
 * <action> [<qualifier> <scope>:<fld>=<val> [ ... <scopeN>:<fldN>=<valN>]]
 *
 * Parameters:
 * buffer         -- ptr to null-terminated string
 * cmd            -- ptr to struct in which to store results
 */
boolean parse_rtp_script_cmd (char *buffer, rtp_script_cmd_t *cmd) 
{
    char *t;
    char *saveptr;
    rtp_qualifier_t qualifier = RTP_QUALIFIER_NONE;

    memset(cmd, 0, sizeof(rtp_script_cmd_t));

    t = strtok_r(buffer, " \n", &saveptr);
    cmd->action = RTP_SCRIPT_NONE;
    if (t) {
        cmd->action = parse_rtp_action(t);
    }
    if (cmd->action == RTP_SCRIPT_NONE) {
        return (FALSE);
    }

    t = strtok_r(NULL, " \n", &saveptr);
    if (t) {
        qualifier = parse_rtp_qualifier(t);
    }
    if (qualifier == RTP_QUALIFIER_NONE) {
        return (TRUE);
    }

    while ((t = strtok_r(NULL, " \n", &saveptr)) != NULL) {
        parse_rtp_modifier_data(t, cmd);
    }
    return (TRUE);
}

/*
 * rtp_modify_hdr
 *
 * Modify the header of an RTP packet, according to an RTP "message modifier".
 *
 * Parameters:
 * msg            -- ptr to packet buffer
 * outlen         -- length in bytes of packet hdr
 * p_modifier     -- ptr to RTP message modifier info
 * Returns:       -- new length of packet hdr
 */
static uint32_t rtp_modify_hdr (uint8_t *msg,
                                uint32_t outlen,
                                rtp_hdr_modifier_t *p_modifier)
{
    int i;
    rtpfasttype_t *rtphdr = (rtpfasttype_t *)msg;
    uint32_t rlen = outlen;
    uint32_t old_value = 0;
    uint32_t mod_value = 0;
    uint32_t new_value = 0;
    uint16_t new_value_s = 0;

    for (i = RTP_FIELD_VERSION ; i < RTP_FIELD_LAST ; i++) {
        if (p_modifier->field_mask & (1 << i)) {
            switch (i) {
            case RTP_FIELD_VERSION:
                old_value = RTP_VERSION(rtphdr);
                break;
            case RTP_FIELD_PADDING:
                old_value = RTP_PADDING(rtphdr);
                break;
            case RTP_FIELD_EXTENSION:
                old_value = RTP_EXT(rtphdr);
                break;
            case RTP_FIELD_CC:
                old_value = RTP_CSRCC(rtphdr);
                break;
            case RTP_FIELD_MARKER:
                old_value = RTP_MARKER(rtphdr);
                break;
            case RTP_FIELD_PT:
                old_value = RTP_PAYLOAD(rtphdr);
                break;
            case RTP_FIELD_SEQ:
                old_value = ntohs(rtphdr->sequence);
                break;
            case RTP_FIELD_TS:
                old_value = ntohl(rtphdr->timestamp);
                break;
            case RTP_FIELD_SSRC:
                old_value = ntohl(rtphdr->ssrc);
                break;
            default:
                break;
            }
            mod_value = p_modifier->field_value[i];
            switch (p_modifier->value_type[i]) {
            case RTP_VT_REPLACE:
                new_value = mod_value;
                break;
            case RTP_VT_INCREMENT:
                new_value = old_value + mod_value;
                break;
            case RTP_VT_DECREMENT:
                new_value = old_value - mod_value;
                break;
            default:
                break;
            }
            new_value_s = (uint16_t)(new_value & 0xffff);
            switch (i) {
            case RTP_FIELD_VERSION:
                SET_RTP_VERSION(rtphdr, new_value_s);
                break;
            case RTP_FIELD_PADDING:
                SET_RTP_PADDING(rtphdr, new_value_s);
                break;
            case RTP_FIELD_EXTENSION:
                SET_RTP_EXT(rtphdr, new_value_s);
                break;
            case RTP_FIELD_CC:
                SET_RTP_CSRCC(rtphdr, new_value_s);
                break;
            case RTP_FIELD_MARKER:
                SET_RTP_MARKER(rtphdr, new_value_s);
                break;
            case RTP_FIELD_PT:
                SET_RTP_PAYLOAD(rtphdr, new_value_s);
                break;
            case RTP_FIELD_SEQ:
                rtphdr->sequence = htons((uint16_t)new_value);
                break;
            case RTP_FIELD_TS:
                rtphdr->timestamp = htonl(new_value);
                break;
            case RTP_FIELD_SSRC:
                rtphdr->ssrc = htonl(new_value);
                break;
            default:
                break;
            }
        }
    }

    return (rlen);
}


/*
 * rtp_modify_csrc
 *
 * Modify the CSRC information in an RTP packet.
 *
 * Parameters:
 * msg        -- ptr to CSRC values in packet (or where they should go)
 * outlen     -- current length in bytes of CSRC data (0 if none)
 * csrc_info  -- ptr to new CSRC info
 * Returns:   -- new length of CSRC data
 */
static uint32_t rtp_modify_csrc (uint8_t *msg,
                                 uint32_t outlen,
                                 rtp_csrc_t *csrc_info)
{
    uint32_t *p_csrc = (uint32_t *)msg;
    int i = 0;
    uint32_t rlen = 0;

    for (i = 0 ; i < csrc_info->num_csrcs ; i++) {
        p_csrc[i] = htonl(csrc_info->csrc[i]);
        rlen += sizeof(uint32_t);
    }

    return (rlen);
}

/*
 * rtp_modify_ext
 *
 * Modify the RTP extenstion header in an RTP packet.
 *
 * Parameters:
 * msg        -- ptr to RTP extension hdr (or to where it should go)
 * outlen     -- current length in bytes of extension hdr (0 if none)
 * ext_info   -- ptr to new extension hdr info
 * Returns:   -- new length of extension header data.
 */
static uint32_t rtp_modify_ext (uint8_t *msg,
                                uint32_t outlen,
                                rtp_ext_hdr_t *ext_info)
{
    if (ext_info->ext_data_len) {
        memcpy(msg, ext_info->ext_data, ext_info->ext_data_len);
    }
    return (ext_info->ext_data_len);
}

/*
 * define macros to set fields not used by VQE-C FEC
 */
#define SET_FEC_MASK(ptr, value) (PUTLONG(&((ptr)->combined1), \
                                 (GETLONG(&((ptr)->combined1)) \
                                          & 0xff000000) |      \
                                 (((value) & 0xffffff) << 0 )))
#define SET_FEC_SNBASE_EXT(ptr, value)    (PUTSHORT(&((ptr)->combined3), \
                                          (GETSHORT(&((ptr)->combined3)) \
                                                    & 0xff00) |        \
                                          (((value) & 0xff) << 0)))
#define SET_FEC_INDEX(ptr, value)  (PUTSHORT(&((ptr)->combined2), \
                                     (GETSHORT(&((ptr)->combined2)) \
                                               & 0xf8ff) |        \
                                     (((value) & 0x07) << 8)))

/*
 * rtp_modify_fec
 *
 * Modify the FEC hdr of an RTP packet, according to an RTP "FEC modifier".
 *
 * Parameters:
 * msg            -- ptr to FEC header
 * outlen         -- length in bytes of FEC header
 * p_modifier     -- ptr to RTP FEC modifier info
 * Returns:       -- new length of FEC header
 */
uint32_t rtp_modify_fec (uint8_t *msg,
                                uint32_t outlen,
                                rtp_fec_modifier_t *p_modifier)
{
    int i;
    fecfasttype_t *fechdr = (fecfasttype_t *)msg;
    uint32_t rlen = sizeof(fecfasttype_t);
    uint32_t old_value = 0;
    uint32_t mod_value = 0;
    uint32_t new_value = 0;
    uint16_t new_value_s = 0;

    for (i = RTP_FEC_SNBASE_LOW_BITS ; i < RTP_FEC_LAST ; i++) {
        if (p_modifier->field_mask & (1 << i)) {
            switch (i) {
            case RTP_FEC_SNBASE_LOW_BITS:
                old_value = ntohs(fechdr->snbase_low_bits);
                break;
            case RTP_FEC_LENGTH_RECOVERY:
                old_value = ntohs(fechdr->length_recovery);
                break;
            case RTP_FEC_E:
                old_value = FEC_E_BIT(fechdr);
                break;
            case RTP_FEC_PT_RECOVERY:
                old_value = FEC_PT_RECOVERY(fechdr);
                break;
            case RTP_FEC_MASK:
                old_value = FEC_MASK(fechdr);
                break;
            case RTP_FEC_TS_RECOVERY:
                old_value = ntohl(fechdr->ts_recovery);
                break;
            case RTP_FEC_X:
                old_value = FEC_X_BIT(fechdr);
                break;
            case RTP_FEC_D:
                old_value = FEC_D_BIT(fechdr);
                break;
            case RTP_FEC_TYPE:
                old_value = FEC_TYPE(fechdr);
                break;
            case RTP_FEC_INDEX:
                old_value = FEC_INDEX(fechdr);
                break;
            case RTP_FEC_OFFSET:
                old_value = FEC_OFFSET(fechdr);
                break;
            case RTP_FEC_NA:
                old_value = FEC_NA(fechdr);
                break;
            case RTP_FEC_SNBASE_EXT_BITS:
                old_value = FEC_SNBASE_EXT(fechdr);
                break;
            default:
                break;
            }
            mod_value = p_modifier->field_value[i];
            switch (p_modifier->value_type[i]) {
            case RTP_VT_REPLACE:
                new_value = mod_value;
                break;
            case RTP_VT_INCREMENT:
                new_value = old_value + mod_value;
                break;
            case RTP_VT_DECREMENT:
                new_value = old_value - mod_value;
                break;
            default:
                break;
            }
            new_value_s = (uint16_t)(new_value & 0xffff);
            switch (i) {
            case RTP_FEC_SNBASE_LOW_BITS:
                fechdr->snbase_low_bits = htons(new_value_s);
                break;
            case RTP_FEC_LENGTH_RECOVERY:
                fechdr->length_recovery = htons(new_value_s);
                break;
            case RTP_FEC_E:
                SET_FEC_E_BIT(fechdr, new_value);
                break;
            case RTP_FEC_PT_RECOVERY:
                SET_FEC_PT_RECOVERY(fechdr, new_value);
                break;
            case RTP_FEC_MASK:
                SET_FEC_MASK(fechdr, new_value);
                break;
            case RTP_FEC_TS_RECOVERY:
                fechdr->ts_recovery = htonl(new_value);
                break;
            case RTP_FEC_X:
                SET_FEC_X_BIT(fechdr, new_value);
                break;
            case RTP_FEC_D:
                SET_FEC_D_BIT(fechdr, new_value);
                break;
            case RTP_FEC_TYPE:
                SET_FEC_TYPE(fechdr, new_value);
                break;
            case RTP_FEC_INDEX:
                SET_FEC_INDEX(fechdr, new_value);
                break;
            case RTP_FEC_OFFSET:
                SET_FEC_OFFSET(fechdr, new_value);
                break;
            case RTP_FEC_NA:
                SET_FEC_NA(fechdr, new_value);
                break;
            case RTP_FEC_SNBASE_EXT_BITS:
                SET_FEC_SNBASE_EXT(fechdr, new_value);
                break;
            default:
                break;
            }
        }
    }

    return (rlen);
}

/*
 * process_rtp_script_cmd
 *
 * Modify an RTP packet according to an RTP script command
 * 
 * Parameters:
 * buffer       -- ptr to packet buffer
 * buflen       -- length in bytes of packet
 * cmd          -- ptr to parsed form of script cmd
 * outhdrlen    -- on output, ptr to new packet header length
 * Returns      -- new length of packet:
 *                 0 means the packet should not be sent.
 */
uint32_t process_rtp_script_cmd (uint8_t *buffer, 
                                 uint32_t bufflen,
                                 rtp_script_cmd_t *cmd,
                                 uint32_t *outhdrlen)
{
    uint8_t outbuf[RTCP_PAK_SIZE];
    uint32_t outlen = 0;
    uint8_t *out = outbuf;
    rtpfasttype_t *rtphdr = (rtpfasttype_t *)buffer;
    uint32_t tot_hdr_len;
    uint32_t hdr_len;
    uint32_t csrc_len;
    uint32_t ext_len;
    uint32_t fec_len = sizeof(fecfasttype_t);
    uint32_t data_len;
    int pad;
    int i;

    if (cmd->action == RTP_SCRIPT_SKIP) {
        return (0);
    }

    tot_hdr_len = RTPHEADERBYTES(rtphdr);
    hdr_len = MINRTPHEADERBYTES;
    csrc_len = RTP_CSRC_LEN(rtphdr);
    ext_len = tot_hdr_len - (hdr_len + csrc_len);
    data_len = bufflen - tot_hdr_len;

    memcpy(out, buffer, hdr_len);
    out += rtp_modify_hdr(out, hdr_len, &cmd->hdr_mods);

    if (csrc_len) {
        memcpy(out, buffer + hdr_len, csrc_len);
    }
    out += rtp_modify_csrc(out, csrc_len, &cmd->csrc_info);

    if (ext_len) {
        memcpy(out, buffer + hdr_len + csrc_len, ext_len);
    }
    out += rtp_modify_ext(out, ext_len, &cmd->ext_info);

    memcpy(out, buffer + tot_hdr_len, data_len);
    (void)rtp_modify_fec(out, fec_len, &cmd->fec_mods);
    out += data_len;

    pad = cmd->len_info.pad;
    if (pad) {
        if (pad > 0) {
            for (i = 0; i < pad - 1; i++) {
                *out++ = 0;
            }
            *out++ = pad;
        } else {
            out += pad;
        }
    }

    outlen = out - outbuf;

    if (cmd->len_info.chg_len) {
        if (cmd->len_info.length > 0) {
            outlen = cmd->len_info.length;
        } else if (cmd->len_info.length < 0) {
            outlen += cmd->len_info.length;
        }
    }

    memcpy(buffer, outbuf, outlen);
    *outhdrlen = RTPHEADERBYTES(rtphdr);
    return (outlen);
}
