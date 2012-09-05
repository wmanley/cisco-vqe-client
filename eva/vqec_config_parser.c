/*
 * vqec_config_parser.c - Implements parsing of VQE-C system configuration 
 * files.
 *
 * Copyright (c) 2007-2009, 2011 by Cisco Systems, Inc.
 * All rights reserved.
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include "queue_plus.h"
#include "vam_types.h"
#include "strl.h"
#include "vqec_config_parser.h"

/* set this to nonzero to compile in selected debug printouts */
#define DEBUG_PRINTS 0

/**
 * Structure to store the indices of the newline characters within the
 * configuration buffer.  This is used to reverse lookup what line a given
 * section of the post-processed (whitespace and comments removed)
 * configuration buffer came from in the original file or buffer.  These
 * lookups are useful to indicate to the user where an error occurred.
 *
 * The table is laid out such that the index represents the line in the
 * original file or buffer, and the value at that index represents the index
 * within the post-processed configuration buffer.
 */
typedef struct newline_table_ {
    uint16_t *table;
    int size;
    char *buf_addr;
} newline_table_t;

static newline_table_t newline_table;


/**
 * Forward declarations for functions used throughout.
 */
static boolean vqec_config_parse_group(vqec_config_t *cfg,
                                       vqec_config_setting_t *parent,
                                       char *paramstr,
                                       char **endgroup);

/**
 * Destroy a given setting and all subsettings and their associated data
 * recursively.
 *
 * Note:  this does not actually free the setting itself, but will but will
 *        completely destroy and free all associated memory and subsettings.
 */
static void vqec_config_destroy_setting (vqec_config_setting_t *setting)
{
    vqec_config_setting_t *subsetting, *tmp;

    if (!setting) {
        return;
    }

    /* iterate through the subsettings and destroy and free each one */
    if (!VQE_TAILQ_EMPTY(&setting->subsetting_head)) {
        VQE_TAILQ_FOREACH_SAFE(subsetting, &setting->subsetting_head, list_qobj, tmp) {
            vqec_config_destroy_setting(subsetting);
            VQE_TAILQ_REMOVE(&setting->subsetting_head, subsetting, list_qobj);
            free(subsetting);
        }
    }
    free(setting->name);
    if (setting->type == VQEC_CONFIG_SETTING_TYPE_STRING &&
        setting->value_string) {
        free(setting->value_string);
    }

    return;
}

/**
 * Find a subsetting within a group or list.
 *
 * Usage Note
 * If a name is given, then the setting with that name within the queue will
 * be returned, if any; if none match the name, NULL will be returned.  If a
 * name is NOT given, then the element at the given index within the list will
 * be returned, if the list contains enough elements that such an element
 * should exist.
 */
static vqec_config_setting_t *
vqec_config_find_subsetting (vqec_config_setting_t *parent,
                             char *name,
                             int index)
{
    vqec_config_setting_t *tmp;
    int count = 0;
    char *dotidx = NULL, tmpname[VQEC_CONFIG_MAX_NAME_LEN];

    /**
     * support compound setting names such as "group1.element1", which
     * accesses a setting named "element1" which is an element of the group
     * "group1"
     */
    if (name) {
        dotidx = strchr(name, '.');
    }
    if (dotidx) {
        strlcpy(tmpname, name, dotidx - name + 1);
        /* get the group corresponding to the group name */
        tmp = vqec_config_find_subsetting(parent, tmpname, 0);
        if (tmp && strlen(dotidx) > 1) {
            /* find the setting name within the group */
            return (vqec_config_find_subsetting(tmp, dotidx + 1, 0));
        } else {
            return (NULL);
        }
    }

    /**
     * iterate through the group to find the setting with the matching name,
     * or the matching index if no name is provided
     */
    VQE_TAILQ_FOREACH(tmp, &parent->subsetting_head, list_qobj) {
        if (name && !strncmp(tmp->name, name, VQEC_CONFIG_MAX_NAME_LEN)) {
            /* this setting's name is a match; return it */
            return (tmp);
        }
        if (!name && count == index) {
            /* this setting's index is a match; return it */
            return (tmp);
        }
        count++;
    }

    return (NULL);
}

/**
 * This function will accept a pointer to a location in the configuration
 * text bufer, and will return the line number of the original file that
 * corresponds to the location in the buffer.
 */
static int vqec_config_get_lineno_from_address (char *addr)
{
    int index, line;

    if (!addr) {
        return (-1);
    }

    index = (int)(addr - newline_table.buf_addr);
    for (line = 0; line < newline_table.size; line++) {
        if (newline_table.table[line] > index) {
            return (line + 1);
        }
    }

    return (line);
}

/**
 * Initialize the configuration parser.
 */
boolean vqec_config_init (vqec_config_t *cfg)
{
    cfg->error_line = 0;
    cfg->error_text[0] = '\0';

    bzero(&cfg->root, sizeof(vqec_config_setting_t));
    VQE_TAILQ_INIT(&cfg->root.subsetting_head);

    cfg->root.type = VQEC_CONFIG_SETTING_TYPE_GROUP;

    return (TRUE);
}

static boolean vqec_config_is_index_in_literal (char *idx, char *str)
{
    char *idx_q, *idx_s, *tmp;
    boolean in_literal = FALSE;

    /* find the first quote */
    idx_q = strchr(str, '\"');
    /* find the first slash (which may be succeeded by a quote) */
    idx_s = strchr(str, '\\');
    while (idx_q && idx_q < idx) {
        if (idx_s && idx_s < idx_q) {
            /*
             * if there is a slash somewhere before the quote, then skip the
             * following character, find the next quote and slash again, and
             * continue
             */
            if (strlen(idx_s) > 2) {
                tmp = idx_s + 2;
            } else {
                break;
            }
        } else {
            /* otherwise, this is a string literal beginning or end */
            in_literal = !(in_literal);
            tmp = idx_q + 1;
        }
        idx_q = strchr(tmp, '\"');
        idx_s = strchr(tmp, '\\');
    }
        
    return (in_literal);
}

/**
 * This function removes all whitespace that is not contained within a string
 * literal in the context of the provided string.
 */
static void vqec_config_str_rmwhitespace (char *str, int len)
{
    int i = 0;

    while (str[i] != '\0' && i < len) {
        if (str[i] == ' '
            || str[i] == '\r'
            || str[i] == '\n'
            || str[i] == '\t') {
            if ((str[i] == ' ' || str[i] == '\t') &&
                vqec_config_is_index_in_literal(str + i, str)) {
                /* if a space is inside a string literal, just skip it */
                i++;
            } else {
                memmove(&str[i], &str[i + 1], len - i - 1);
                str[len - 1] = '\0';
                len--;
            }
        } else {
            i++;
        }
    }

    return;        
}

/**
 * Within a provided string, go through it left-to-right, looking for a quote
 * or a backslash character, and return the pointer to whichever one comes
 * first.  If neither a quote nor a backslash exist before the end of the
 * string, then NULL is returned.  The given string must be NUL-terminated.
 */
static char *vqec_config_find_quote_or_backslash (char *str)
{
    char *idx_q, *idx_s, *idx;

    /* find the first quote and first backslash */
    idx_q = strchr(str, '\"');
    idx_s = strchr(str, '\\');

    /* figure out which comes first */
    idx = (char *)(-1);  /* highest address in space */
    if (idx_s && idx_s < idx) {
        idx = idx_s;
    }
    if (idx_q && idx_q < idx) {
        idx = idx_q;
    }
    if (idx == (char *)(-1)) {
        /* no quote or backslash found */
        idx = NULL;
    }

    return (idx);
}

/**
 * This function will take a string which may have been read as multiple
 * consecutive strings and concatenates them together.  The following character
 * escape sequences are resolved here as well: '\"', '\\', '\f', '\n', '\r',
 * and '\t'.  This function returns TRUE on success; FALSE if there is a stray
 * quote character within the string.
 *
 * The general algorithm here is to go left-to-right looking for both quotes
 * and backslashes.  Then, when one is found, look at the following character
 * to figure out how to handle it.
 *
 * Note:  to guarantee proper functionality, this function requires the given
 *        string to be NUL-terminated.
 */
static boolean vqec_config_str_concat_and_escape (char *str)
{
    char *tmp;

    /* find the first quote or backslash */
    tmp = vqec_config_find_quote_or_backslash(str);

    while (tmp) {
        if (tmp[0] == '\"') {
            /* this is a quote */
            if (tmp[1] == '\"') {
                /* this is a double quote; concatenate */
                memmove(tmp,
                        tmp + strlen("\"\""),
                        strlen(tmp) - strlen("\"\""));
                tmp[strlen(tmp) - strlen("\"\"")] = '\0';
            } else {
                /* this is a stray quote; return FALSE as it's invalid */
                return (FALSE);
            }
        } else {
            /* this is a backslash; escape the next character */
            switch (tmp[1]) {
                case '\"':
                    tmp[0] = '\"';
                    break;
                case '\\':
                    tmp[0] = '\\';
                    break;
                case 'f':
                    tmp[0] = '\f';
                    break;
                case 'n':
                    tmp[0] = '\n';
                    break;
                case 'r':
                    tmp[0] = '\r';
                    break;
                case 't':
                    tmp[0] = '\t';
                    break;
                default:
                    /* unrecognized escape character; leave it and go on */
                    tmp = vqec_config_find_quote_or_backslash(tmp + 1);
                    continue;
            }

            /*
             * leave current character, and move the rest of the string forward
             * by 1 character and terminate it correctly
             */
            memmove(tmp + 1, tmp + 2, strlen(tmp) - 2);
            tmp[strlen(tmp) - 1] = '\0';
            tmp++;
        }

        /* find the next quote or backslash */
        tmp = vqec_config_find_quote_or_backslash(tmp);
    }

    return (TRUE);
}

/**
 * This is a version of strtok_r that does NOT treat consecutive delimiters as
 * a single delimiter.  Also, if the delimiter is within a string literal
 * within the context of the provided string, it will be ignored and the next
 * instance of the delimiter that is NOT within a string literal will be used
 * for tokenization instead.
 */
static char *
vqec_config_strtok_r (char *str, const char *delim, char **saveptr)
{
    char *delim_loc;

    if (!str) {
        str = *saveptr;
    }
    delim_loc = strstr(str, delim);
    while (delim_loc) {
        if (!vqec_config_is_index_in_literal(delim_loc, str)) {
            delim_loc[0] = '\0';
            *saveptr = delim_loc + strlen(delim);
            return (str);
        } else {
            if (strlen(delim_loc) > 1) {
                delim_loc = strstr(delim_loc + 1, delim);
            } else {
                delim_loc = NULL;
            }
        }
    }

    return (NULL);
}

/**
 * Given a string with unknown capitalization, convert it to be all lower-case.
 * This function guarantees NULL termination, so, at most, only len-1
 * characters will actually be processed.
 */
static void vqec_config_str_to_lower_case (char *lowercase, char *orig, int len)
{
    int charidx;

    for (charidx = 0; charidx < (len - 1); charidx++) {
        lowercase[charidx] = tolower(orig[charidx]);
    }

    /* make sure the string is NULL terminated */
    lowercase[len - 1] = '\0';
}

/**
 * Detect the type of parameter this value is for, based on the format of
 * the value.
 */
static vqec_config_setting_type_t
vqec_config_detect_type (char *value)
{
    /* does it start with a quotation character? */
    if (value[0] == '\"') {
#if DEBUG_PRINTS
        printf("type: STRING\n");
#endif  /* DEBUG_PRINTS */
        return (VQEC_CONFIG_SETTING_TYPE_STRING);
    }

    /* does it start with '+', '-', or a number? */
    if (value[0] == '+' || value[0] == '-' || isdigit(value[0])) {
#if DEBUG_PRINTS
        printf("type: INT\n");
#endif  /* DEBUG_PRINTS */
        return (VQEC_CONFIG_SETTING_TYPE_INT);
    }

    /* does it start with 't', 'T', 'f', or 'F' ? */
    if (value[0] == 't' || value[0] == 'T' ||
        value[0] == 'f' || value[0] == 'F') {
#if DEBUG_PRINTS
        printf("type: BOOLEAN\n");
#endif  /* DEBUG_PRINTS */
        return (VQEC_CONFIG_SETTING_TYPE_BOOLEAN);
    }

    /* does it start with a '{'? */
    if (value[0] == '{') {
#if DEBUG_PRINTS
        printf("type: GROUP\n");
#endif  /* DEBUG_PRINTS */
        return (VQEC_CONFIG_SETTING_TYPE_GROUP);
    }

    /* does it start with a '('? */
    if (value[0] == '(') {
#if DEBUG_PRINTS
        printf("type: LIST\n");
#endif  /* DEBUG_PRINTS */
        return (VQEC_CONFIG_SETTING_TYPE_LIST);
    }

    /* otherwise, it's an invalid type */
#if DEBUG_PRINTS
    printf("type: INVALID\n");
#endif  /* DEBUG_PRINTS */
    return (VQEC_CONFIG_SETTING_TYPE_INVALID);
}

/**
 * Parse the value of the provided string, which should contain a value in the
 * format of a primary type.  Store the parsed value in the setting
 * appropriately.
 */
static boolean vqec_config_parse_value (vqec_config_t *cfg,
                                        vqec_config_setting_t *setting,
                                        char *value)
{
#define VQEC_CONFIG_INTEGER_VALUE_MAX_LEN 12
#define VQEC_CONFIG_BOOLEAN_STRING_LEN 6

    /* used for boolean decapitalizaion */
    char tmpstr[VQEC_CONFIG_BOOLEAN_STRING_LEN];
    int base;
    char int_str[VQEC_CONFIG_INTEGER_VALUE_MAX_LEN];

    switch (setting->type) {
        case VQEC_CONFIG_SETTING_TYPE_STRING:
            setting->value_string =
                malloc(strlen(value) - 1);
            if (!setting->value_string) {
                snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                         "Failed to allocate memory for value of \"%s\".",
                         setting->name ? setting->name : "<unknown>");
                cfg->error_line = vqec_config_get_lineno_from_address(value);
                return (FALSE);
            }
            strlcpy(setting->value_string,
                    ((char *)value + 1),
                    strlen(value) - 1);
            if (!vqec_config_str_concat_and_escape(setting->value_string)) {
                /* failed due to a stray quotation mark, therefore invalid */
                free(setting->value_string);
                setting->value_string = NULL;
                setting->type = VQEC_CONFIG_SETTING_TYPE_INVALID;
            }
            break;
        case VQEC_CONFIG_SETTING_TYPE_BOOLEAN:
            /* the whole word must exist with nothing else trailing it */
            vqec_config_str_to_lower_case(tmpstr, value, VQEC_CONFIG_BOOLEAN_STRING_LEN);
            if (!strncmp(tmpstr, "true", strlen("true") + 1) ||
                !strncmp(tmpstr, "false", strlen("false") + 1)) {
                setting->value_boolean = (value[0] == 't' || value[0] == 'T') ?
                    TRUE : FALSE;
            } else {
                /*
                 * there is something following the "true" or "false" which is
                 * incorrect; thus this is assumed to be an invalid type
                 */
                setting->type = VQEC_CONFIG_SETTING_TYPE_INVALID;
            }                
            break;
        case VQEC_CONFIG_SETTING_TYPE_INT:
            /* first check to see if it's in hexadecimal format */
            if (strlen(value) > 1 && value[0] == '0' && value[1] == 'x') {
                base = 16;
            } else {
                base = 10;
            }
            /* parse the integer */
            setting->value_int = (int)strtol(value, NULL, base);
            /* check to see if it was parsed correctly */
            if (base == 16) {
                snprintf(int_str, VQEC_CONFIG_INTEGER_VALUE_MAX_LEN,
                         "0x%x", setting->value_int);
            } else {  /* base 10 */
                snprintf(int_str, VQEC_CONFIG_INTEGER_VALUE_MAX_LEN,
                         "%d", setting->value_int);
            }
#if DEBUG_PRINTS
            printf("orig = \"%s\"\nread = \"%s\"\n", value, int_str);
#endif  /* DEBUG_PRINTS */
            if (strncmp(value, int_str, VQEC_CONFIG_INTEGER_VALUE_MAX_LEN)) {
                /* didn't parse correctly; assume it is an invalid type */
                setting->type = VQEC_CONFIG_SETTING_TYPE_INVALID;
            }                
            break;
        case VQEC_CONFIG_SETTING_TYPE_INVALID:
            /* nothing to do if it's an invalid type as it cannot be parsed */
            break;
        case VQEC_CONFIG_SETTING_TYPE_LIST:
        case VQEC_CONFIG_SETTING_TYPE_GROUP:
        default:
            /* these should not be reached */
            snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                     "Internal error; trying to parse list or group as value.");
            cfg->error_line = vqec_config_get_lineno_from_address(value);
            return (FALSE);
            break;
    }

    return (TRUE);
}

/**
 * Parse a list configuration structure recursively.
 *
 * @param[in]  cfg  Pointer to config structure.
 * @param[in]  parent  Parent setting that this list is a part of.
 * @param[in]  paramstr  Pointer to the current position in the configuration
 *                       data buffer.
 * @param[out]  endlist  Pointer to the position in the configuration data
 *                       buffer that indicates the end of this list.
 */
static boolean vqec_config_parse_list (vqec_config_t *cfg,
                                       vqec_config_setting_t *parent,
                                       char *paramstr,
                                       char **endlist)
{
    vqec_config_setting_t *setting = NULL;
    char *paramval_ptr;
    char *curchar;  /* reentrant value */
    int stridx = 0;
    int param_list_len;
    boolean end_of_list = FALSE;
    vqec_config_setting_type_t list_type = 0;
    boolean list_type_set = FALSE;

    if (!parent) {
        snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                 "Internal error; invalid list argument to parse.");
        cfg->error_line = vqec_config_get_lineno_from_address(paramstr);
        return (FALSE);
    }

    curchar = paramstr;

    /* process the parameters list string */
    param_list_len = strlen(paramstr);
    while (stridx < param_list_len) {
        /* detect end of group */
        if (end_of_list) {
            if (endlist) {
                /* return the first character after the closing ')' */
                *endlist = curchar;
            }
            return (TRUE);
        }
        /* there are no parameter names in a list */
        setting = malloc(sizeof(vqec_config_setting_t));
        if (!setting) {
            snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                     "Failed to allocate memory for setting.");
            cfg->error_line = vqec_config_get_lineno_from_address(curchar);
            goto bail;
        }
        bzero(setting, sizeof(vqec_config_setting_t));
        VQE_TAILQ_INIT(&setting->subsetting_head);
        setting->type = vqec_config_detect_type(curchar);
        if (!list_type_set) {
            /* cache type of list for error checking */
            list_type = setting->type;
            list_type_set = TRUE;
        } else {
            /* compare value type with list type */
            if (setting->type != list_type) {
                snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                     "Inconsistent value types within list.");
            cfg->error_line = vqec_config_get_lineno_from_address(curchar);
            goto bail;
            }
        }
        if (setting->type == VQEC_CONFIG_SETTING_TYPE_GROUP) {
            /* parse group, excluding the first '{' character */
#if DEBUG_PRINTS
            printf("recursing to parse group, curchar = %u %c\n",
                   (uint)curchar, curchar[0]);
#endif  /* DEBUG_PRINTS */
            if (!vqec_config_parse_group(cfg,
                                         setting,
                                         curchar + 1,
                                         &curchar)) {
                /* error text and line should already be set */
                goto bail;
            }
#if DEBUG_PRINTS
            printf("returned from parsing group, curchar = %u %c\n",
                   (uint)curchar, curchar[0]);
#endif  /* DEBUG_PRINTS */
            /**
             * a group within a list will have either a ',' or ')' after it, so
             * this character needs to be processed before the next parameter
             * can be parsed
             */
            if (curchar[0] == ',') {
                /* delimiter, skip and continue to parse */
                curchar++;
            } else if (curchar[0] == ')') {
                /* end of list, skip and return */
                curchar++;
                end_of_list = TRUE;
            }
        } else if (setting->type == VQEC_CONFIG_SETTING_TYPE_LIST) {
            /* parse group, excluding the first '(' character */
#if DEBUG_PRINTS
            printf("recursing to parse list, curchar = %u %c\n",
                   (uint)curchar, curchar[0]);
#endif  /* DEBUG_PRINTS */
            if (!vqec_config_parse_list(cfg,
                                        setting,
                                        curchar + 1,
                                        &curchar)) {
                /* error text and line should already be set */
                goto bail;
            }
#if DEBUG_PRINTS
            printf("returned from parsing list, curchar = %u %c\n",
                   (uint)curchar, curchar[0]);
#endif  /* DEBUG_PRINTS */
            /**
             * a list within a list will have either a ',' or ')' after it, so
             * this character needs to be processed before the next parameter
             * can be parsed
             */
            if (curchar[0] == ',') {
                /* delimiter, skip and continue to parse */
                curchar++;
            } else if (curchar[0] == ')') {
                /* end of list, skip and return */
                curchar++;
                end_of_list = TRUE;
            }
        } else {
            /*
             * normal "value," or "value)" case: figure out which and
             * parse the value
             */
            if (strchr(curchar, ',') > strchr(curchar, ')')
                || !strchr(curchar, ',')) {
                end_of_list = TRUE;
            }
                
            paramval_ptr = vqec_config_strtok_r(NULL,
                                    /* use ',' or ')', whichever comes next */
                                    end_of_list ?
                                    ")" : ",",
                                    &curchar);
            if (paramval_ptr) {
                /* parse the actual value */
                if (!vqec_config_parse_value(cfg, setting, paramval_ptr)) {
                    /* error text and line already set within parse_value */
                    goto bail;
                }
            } else {
                snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                         "No value for list element.");
                cfg->error_line =
                    vqec_config_get_lineno_from_address(paramval_ptr);
                goto bail;
            }
        }

        /* add this setting to the queue in this group */
        VQE_TAILQ_INSERT_TAIL(&parent->subsetting_head, setting, list_qobj);

#if DEBUG_PRINTS
        printf("old stridx = %d\n", stridx);
#endif  /* DEBUG_PRINTS */
        if (stridx == curchar - paramstr) {
            break;
        } else {
            stridx = curchar - paramstr;
        }
#if DEBUG_PRINTS
        printf("new stridx = %d; strlen(paramstr+stridx) = %d; "
               "param_list_len = %d\n",
               curchar - paramstr, strlen(paramstr + stridx), param_list_len);
#endif  /* DEBUG_PRINTS */
    }

    return (TRUE);

bail:
    if (setting) {
        vqec_config_destroy_setting(setting);
        free(setting);
    }
    return (FALSE);
}

/**
 * Validate the given parameter name.  The parameter name must conform to the
 * format defined by the regex:
 *     [A-Za-z\*][-A-Za-z0-9_\*]*
 * If the parameter name is of the correct format, TRUE is returned; otherwise
 * FALSE is returned.
 *
 * This function will analyze the name character by character to make sure that
 * all characters conform to the pattern.
 */
static boolean vqec_config_validate_param_name (char *pname)
{
    int idx, len;
    char c;

    len = strlen(pname);

    /* first character must match [A-Za-z\*] */
    if (!isalpha(pname[0]) &&
        pname[0] != '*') {
        return (FALSE);
    }

    /* rest of characters must match [-A-Za-z0-9_\*] */
    for (idx = 1; idx < len; idx++) {
        c = pname[idx];
        if (!isalnum(c) &&
            c != '-' &&
            c != '_' &&
            c != '*') {
            return (FALSE);
        }
    }

    return (TRUE);
}

/**
 * Parse a group configuration structure recursively.
 *
 * @param[in]  cfg  Pointer to config structure.
 * @param[in]  parent  Parent setting that this list is a part of.
 * @param[in]  paramstr  Pointer to the current position in the configuration
 *                       data buffer.
 * @param[out]  endlist  Pointer to the position in the configuration data
 *                       buffer that indicates the end of this list.
 */
static boolean vqec_config_parse_group (vqec_config_t *cfg,
                                        vqec_config_setting_t *parent,
                                        char *paramstr,
                                        char **endgroup)
{
    char *param_name;
    int param_namelen;
    vqec_config_setting_t *setting = NULL;
    char *paramval_ptr;
    char *curchar = paramstr;  /* reentrant value */
    int stridx = 0;
    int first_pass = TRUE;
    int param_list_len;

    if (!parent) {
        snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                 "Internal error; invalid list argument to parse.");
        cfg->error_line = vqec_config_get_lineno_from_address(paramstr);
        return (FALSE);
    }
    /* process the parameters list string */
    param_list_len = strlen(paramstr);
    while (stridx < param_list_len) {
        /* detect end of group */
        if (!first_pass && curchar[0] == '}') {
            if (endgroup) {
                /* return the first character after the closing '}' */
                *endgroup = curchar + 1;
            }
            return (TRUE);
        }
        /* parse parameter name */
        param_name = (char *)vqec_config_strtok_r(first_pass ? paramstr : NULL, "=", &curchar);
        if (param_name) {
#if DEBUG_PRINTS
            printf("param_name = \"%s\"\n", param_name);
#endif  /* DEBUG_PRINTS */
            if (strchr(param_name, ';')) {
                snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                         "Syntax error:  no assignment made.");
                cfg->error_line = vqec_config_get_lineno_from_address(param_name);
                return (FALSE);
            }
            if (!vqec_config_validate_param_name(param_name)) {
                snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                         "Invalid parameter name \"%s\".", param_name);
                cfg->error_line = vqec_config_get_lineno_from_address(param_name);
                return (FALSE);
            }                
            if (vqec_config_find_subsetting(parent, param_name, 0)) {
                snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                         "Duplicate setting found for \"%s\".", param_name);
                cfg->error_line = vqec_config_get_lineno_from_address(param_name);
                return (FALSE);
            }
        } else {
#if DEBUG_PRINTS
            printf(" name: ---\n");
#endif  /* DEBUG_PRINTS */
            snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                     "Cannot parse parameter name.");
            cfg->error_line = vqec_config_get_lineno_from_address(curchar);
            return (FALSE);
        }
        setting = malloc(sizeof(vqec_config_setting_t));
        if (!setting) {
            snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                     "Failed to allocate memory for parameter name.");
            cfg->error_line = vqec_config_get_lineno_from_address(curchar);
            return (FALSE);
        }
        bzero(setting, sizeof(vqec_config_setting_t));
        VQE_TAILQ_INIT(&setting->subsetting_head);

        /* include an extra char for NULL terminator */
        param_namelen = (strlen(param_name) + 0 < VQEC_CONFIG_MAX_NAME_LEN) ?
            strlen(param_name) + 1 : VQEC_CONFIG_MAX_NAME_LEN;
        setting->name = malloc(param_namelen);
        strlcpy(setting->name, param_name, param_namelen);

        setting->type = vqec_config_detect_type(curchar);
        if (setting->type == VQEC_CONFIG_SETTING_TYPE_GROUP) {
            /* parse group, excluding the first '{' character */
#if DEBUG_PRINTS
            printf("recursing to parse group, curchar = %u %c\n",
                   (uint)curchar, curchar[0]);
#endif  /* DEBUG_PRINTS */
            if (!vqec_config_parse_group(cfg,
                                         setting,
                                         curchar + 1,
                                         &curchar)) {
                /* error text and line should already be set */
                goto bail;
            }
            
            /**
             * a group within a group will always have a ';' after it, so
             * this character needs to be skipped over before the next
             * parameter can be parsed
             */
            curchar++;
#if DEBUG_PRINTS
            printf("returned from parsing group, curchar = %u %c\n",
                   (uint)curchar, curchar[0]);
#endif  /* DEBUG_PRINTS */
        } else if (setting->type == VQEC_CONFIG_SETTING_TYPE_LIST) {
            /* parse group, excluding the first '(' character */
#if DEBUG_PRINTS
            printf("recursing to parse list, curchar = %u %c\n",
                   (uint)curchar, curchar[0]);
#endif  /* DEBUG_PRINTS */
            if (!vqec_config_parse_list(cfg,
                                        setting,
                                        curchar + 1,
                                        &curchar)) {
                /* error text and line should already be set */
                goto bail;
            }
            /**
             * a list within a group will always have a ';' after it, so
             * this character needs to be skipped over before the next
             * parameter can be parsed
             */
            curchar++;
#if DEBUG_PRINTS
            printf("returned from parsing list, curchar = %u %c\n",
                   (uint)curchar, curchar[0]);
#endif  /* DEBUG_PRINTS */
        } else {
            /* normal "key = value;" pair case: parse parameter value */
            paramval_ptr = vqec_config_strtok_r(NULL, ";", &curchar);
            if (paramval_ptr) {
                /* parse the actual value */
                if (!vqec_config_parse_value(cfg, setting, paramval_ptr)) {
                    /* error text and line already set within parse_value */
                    goto bail;
                }
            } else {
                snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                         "No assigned value for setting \"%s\".",
                         setting->name ? setting->name : "<unknown>");
                cfg->error_line =
                    vqec_config_get_lineno_from_address(paramval_ptr);
                goto bail;
            }
        }

        /* add this setting to the queue in this group */
        VQE_TAILQ_INSERT_TAIL(&parent->subsetting_head, setting, list_qobj);

#if DEBUG_PRINTS
        printf("old stridx = %d\n", stridx);
#endif  /* DEBUG_PRINTS */
        if (stridx == curchar - paramstr) {
            break;
        } else {
            stridx = curchar - paramstr;
        }
#if DEBUG_PRINTS
        printf("new stridx = %d; strlen(paramstr+stridx) = %d; "
               "param_list_len = %d\n",
               curchar - paramstr, strlen(paramstr + stridx),
               param_list_len);
#endif  /* DEBUG_PRINTS */
        first_pass = FALSE;
    }

    return (TRUE);

bail:
    if (setting) {
        vqec_config_destroy_setting(setting);
        free(setting);
    }
    return (FALSE);
}

/**
 * Find the first comment character in a given string.  If found, a pointer to
 * this character in the string will be returned; otherwise NULL will be
 * returned.
 */
static char *
vqec_config_find_comment_str (char *str)
{
    char *comment, *charpound, *charslash, *charmulti;

    charpound = strchr(str, '#');
    charslash = strstr(str, "//");
    charmulti = strstr(str, "/*");

    comment = (char *)(-1);  /* highest address in space */
    if (charpound && charpound < comment) {
        comment = charpound;
    }
    if (charslash && charslash < comment) {
        comment = charslash;
    }
    if (charmulti && charmulti < comment) {
        comment = charmulti;
    }
    if (comment == (char *)(-1)) {
        /* no comment found */
        comment = NULL;
    }

    return (comment);
}

/**
 * Read the comfiguration file from the stream pointer, and store it in the
 * configuration buffer of the given length, if provided.  If a buffer is
 * provided, the length that has been written to that buffer is returned.
 * Otherwise, if no buffer is provided, the length that WOULD be written to a
 * buffer of sufficient size will be returned.
 *
 * The processing in this function consists of first counting the number of
 * lines in the configuration file.  Then, the file is iterated through and all
 * comments and whitespace are removed, and if a configuration buffer is
 * provided, this processed configuration data is stored in this buffer.  If
 * no configuration buffer is provided, then a running total of the number of
 * characters that WOULD be added to such a buffer is kept instead.
 * Additionally, whenever a newline character is found, the index of that
 * character in the buffer is stored to be later referenced when the parsing
 * logic needs to find what line a particular index in the buffer was on in
 * the original configuration file.
 *
 * @param[in]    f                        : pointer to config file
 * @param[in]    config_buf               : buffer into which trimmed
 *                                          configuration is written [optional]
 * @param[in]    config_len               : size of 'config_buf' (in bytes) 
 * @return       Returns an integer:
 *                 <-1>                   : failure (invalid parameters)
 *                 <-2>                   : failure (memory allocation failure)
 *                 <non-negative number>  : success (number = size of config
 *                                                   with comments and 
 *                                                   whitespace removed)
 */
static int vqec_config_read_and_process (FILE *f,
                                         char *config_buf,
                                         int config_len)
{
    char *comment, *endcomment;
    char *line;
    boolean in_multiline = FALSE;
    int numlines = 0, numchars = 0, charsperline = 0, max_charsperline = 0;
    int c;
    int i = 0;

    if (!f) {
        return (-1);
    }

    /* count the number of lines and characters */
    rewind(f);
    c = fgetc(f);
    while (c != EOF) {
        charsperline++;
        if (c == '\n') {
            numlines++;
            if (charsperline > max_charsperline) {
                max_charsperline = charsperline;
            }
            charsperline = 0;
        }
        c = fgetc(f);
    }
    if (charsperline > max_charsperline) {
        max_charsperline = charsperline;
    }
    rewind(f);

    /* allocate buffers based on stats */
    line = malloc(max_charsperline + 2 /* newline and NULL-term */);
    if (!line) {
        return (-2);
    }
    if (config_buf) {
        newline_table.table = malloc(sizeof(uint16_t) * (numlines + 1));
        if (!newline_table.table) {
            free(line);
            return (-2);
        }
        newline_table.size = numlines + 1;
        bzero(newline_table.table, sizeof(uint16_t) * (numlines + 1));
        bzero(config_buf, config_len);
    }

    while (fgets(line, max_charsperline + 2, f)) {

        /* remove comments */
        if (in_multiline) {
            /*
             * if currently inside a multi-line comment, look for the ending
             * comment string.  if it's here, remove the commented portion and
             * continue on as normal; otherwise, just skip this line.
             */
            endcomment = strstr(line, "*/");
            if (endcomment) {
                memmove(line,
                        endcomment + strlen("*/"),
                        strlen(endcomment) - strlen("*/") + 1);
                in_multiline = FALSE;
            } else {
                if (newline_table.table) {
                    newline_table.table[i] = (uint16_t)strlen(config_buf);
                    i++;
                }
                continue;
            }
        }
        comment = vqec_config_find_comment_str(line);
        while (comment) {
            if (!vqec_config_is_index_in_literal(comment, line)) {
                if (!strncmp(comment, "/*", strlen("/*"))) {
                    /* process multi-line style comment */
                    endcomment = strstr(comment, "*/");
                    if (endcomment) {
                        memmove(comment,
                                endcomment + strlen("*/"),
                                strlen(endcomment) - strlen("*/") + 1);
                    } else {
                        comment[0] = '\0';
                        in_multiline = TRUE;
                    }
                } else {
                    /* process single-line style comment */
                    comment[0] = '\0';
                    break;
                }
            } else {
                /* in a literal; skip it and move on */
                if (comment[0] == '#') {
                    comment += 1;
                } else {
                    comment += 2;
                }                
            }

            /* find the next comment string */
            comment = vqec_config_find_comment_str(comment);
        }

        /* trim white space */
        vqec_config_str_rmwhitespace(line, max_charsperline);

        if (config_buf) {
            /* append to existing parameters list string */
            if (strlen(config_buf) + strlen(line) < config_len) {
                strlcat(config_buf, line, config_len);
                newline_table.table[i] = (uint16_t)strlen(config_buf);
                i++;
#if DEBUG_PRINTS
                printf("added line to config param list; "
                       "%d of %d bytes used, added %d bytes; line = \"%s\"\n",
                       strlen(config_buf), config_len, strlen(line), line);
#endif  /* DEBUG_PRINTS */
            } else {
#if DEBUG_PRINTS
                printf("not enough space in config param list; "
                       "%d of %d bytes used, and trying to add %d bytes\n",
                       strlen(config_buf), config_len, strlen(line));
#endif  /* DEBUG_PRINTS */
            }
        } else {
            /* tally up how large the buffer needs to be */
            numchars += strlen(line);
        }
    }

    if (numchars) {
        /* add one more for NULL terminator */
        numchars++;

#if DEBUG_PRINTS
        printf("config file stats:\n"
               " num chars:      %d\n"
               " num lines:      %d\n"
               " max chars/line: %d\n",
               numchars, numlines, max_charsperline);
#endif  /* DEBUG_PRINTS */
    }

    free(line);
    return (config_buf ? strlen(config_buf) + 1 : numchars);
}

/**
 * Read the configuration file as a FILE pointer.
 */
static boolean vqec_config_read_file_ptr (vqec_config_t *cfg,
                                          FILE *file)
{
    char *config_buf = NULL;
    int result, buflen, wr_len;
    boolean status;

    /* first determine the trimmed size of the config */
    result = vqec_config_read_and_process(file, NULL, 0);
    if (result < 0) {
        snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                 "Failed to read configuration file, error %d", result);
        status = FALSE;
        goto done;
    } else if (!result) {
        /* Empty config; no update needed to 'cfg' */
        status = TRUE;
        goto done;
    } else {
        buflen = result;
    }

    /* now allocate a buffer for it */
    config_buf = malloc(buflen);
    if (!config_buf) {
        snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                 "Failed to allocate buffer for configuration data.");
        cfg->error_line = 0;
        status = FALSE;
        goto done;
    }

    /* now actually read the file into the buffer, trimmed */
    newline_table.buf_addr = config_buf;
    result = vqec_config_read_and_process(file,
                                          config_buf,
                                          buflen);
    if (result < 0) {
        snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                 "Failed to read configuration file, error %d", result);
        status = FALSE;
        goto done;
    } else {
        wr_len = result;
    }

    if (buflen != wr_len) {
        snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                 "Buffer length mismatch! (buflen = %d, wr_len = %d\n",
                 buflen, wr_len);
        cfg->error_line = 0;
    }

    status = vqec_config_parse_group(cfg, &cfg->root, config_buf, NULL);

 done:
    if (config_buf) {
        free(config_buf);
    }
    /*
     * this is allocated during the second call to vqec_config_read_and_process
     */
    if (newline_table.table) {
        free(newline_table.table);
    }
    newline_table.table = NULL;
    newline_table.buf_addr = NULL;

    return (status);
}

/**
 * Read a configuration file and parse its parameters and values.
 */
boolean vqec_config_read_file (vqec_config_t *cfg,
                               const char *filepath)
{
    FILE *fp;
    boolean status;

    if (!cfg || !filepath) {
        if (cfg) {
            snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                     "Invalid filename argument.");
            cfg->error_line = 0;
        }
        return (FALSE);
    }

    fp = fopen(filepath, "r");
    if (fp) {
        status = vqec_config_read_file_ptr(cfg, fp);
        fclose(fp);
        return (status);
    } else {
        snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                 "Cannot open file.");
        cfg->error_line = 0;
        return (FALSE);
    }
}

/**
 * Read a buffer containing configuration information and parse its parameters
 * and values.
 */
boolean vqec_config_read_buffer (vqec_config_t *cfg,
                                 const char *buffer)
{
    FILE *fp = NULL;
    boolean status;

    if (!cfg || !buffer) {
        if (cfg) {
            snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                     "Invalid buffer argument.");
            cfg->error_line = 0;
        }
        return (FALSE);
    }

    /*
     * Check for an empty configuration buffer.  An empty configuration
     * is valid, but fmemopen() will not generate a file pointer for an
     * empty buffer.  So a buffer of " " is used instead, since it is
     * functionally equivalent to an empty buffer.
     */
    if (!buffer[0]) {
        buffer = " ";
    }
    fp = fmemopen((char *)buffer, strlen(buffer), "r");
    if (fp) {
        status = vqec_config_read_file_ptr(cfg, fp);
        fclose(fp);
        return (status);
    } else {
        snprintf(cfg->error_text, VQEC_CONFIG_ERROR_STRLEN,
                 "Cannot allocate file pointer for buffer");
        cfg->error_line = 0;
        return (FALSE);
    }
}

/**
 * Look up a configuration setting by its parameter name.
 */
vqec_config_setting_t *vqec_config_lookup (vqec_config_t *cfg,
                                           char *name)
{
    if (!cfg || !name) {
        return (NULL);
    }

    return (vqec_config_find_subsetting(&cfg->root, name, 0));
}

/**
 * Determine the type of a particular setting.
 */
vqec_config_setting_type_t
vqec_config_setting_type (vqec_config_setting_t *setting)
{
    if (setting) {
        return (setting->type);
    } else {
        return (VQEC_CONFIG_SETTING_TYPE_INVALID);
    }
}

/**
 * Determine the length (number of elements) of a particular group or list
 * format configuration setting.
 */
int vqec_config_setting_length (vqec_config_setting_t *setting)
{
    vqec_config_setting_t *tmp;
    int len = 0;

    if (!setting ||
        !(setting->type == VQEC_CONFIG_SETTING_TYPE_LIST ||
          setting->type == VQEC_CONFIG_SETTING_TYPE_GROUP)) {
        return (0);
    }

    /* iterate through the list to see how many elements there are */
    VQE_TAILQ_FOREACH(tmp, &setting->subsetting_head, list_qobj) {
        len++;
    }

    return (len);
}

/**
 * Retrieve an element of a list by its index.
 */
vqec_config_setting_t *
vqec_config_setting_get_elem (vqec_config_setting_t *setting,
                              int index)
{
    if (setting && setting->type == VQEC_CONFIG_SETTING_TYPE_LIST) {
        return (vqec_config_find_subsetting(setting, NULL, index));
    } else {
        return (NULL);
    }
}

/**
 * Retrieve a member of a group by its name.
 */
vqec_config_setting_t *
vqec_config_setting_get_member (vqec_config_setting_t *setting,
                                char *name)
{
    if (setting && setting->type == VQEC_CONFIG_SETTING_TYPE_GROUP) {
        return (vqec_config_find_subsetting(setting, name, 0));
    } else {
        return (NULL);
    }
}

/**
 * Retrieve the value of a string setting.
 *
 * @param[in]  setting  Setting to have its value retrieved.
 * @return  Returns a pointer to the string value of the setting.
 */
char *vqec_config_setting_get_string (vqec_config_setting_t *setting)
{
    if (!setting || setting->type != VQEC_CONFIG_SETTING_TYPE_STRING) {
        return (NULL);
    }

    return (setting->value_string);
}

/**
 * Retrieve the value of a boolean setting.
 *
 * @param[in]  setting  Setting to have its value retrieved.
 * @return  Returns TRUE or FALSE in accordance with the value of the setting.
 */
boolean vqec_config_setting_get_bool (vqec_config_setting_t *setting)
{
    if (!setting || setting->type != VQEC_CONFIG_SETTING_TYPE_BOOLEAN) {
        return (FALSE);
    }

    return (setting->value_boolean);
}

/**
 * Retrieve the value of a signed integer setting.
 *
 * @param[in]  setting  Setting to have its value retrieved.
 * @return  Returns the signed integer value of the setting.
 */
int vqec_config_setting_get_int (vqec_config_setting_t *setting)
{
    if (!setting || setting->type != VQEC_CONFIG_SETTING_TYPE_INT) {
        return (0);
    }

    return (setting->value_int);
}

/**
 * Destroy all information stored in a configuration parser instance.
 *
 * @param[in]  cfg  Instance of configuration parser.
 * @return  Returns TRUE if the parser was destroyed successfully; FALSE
 *          otherwise.
 */
boolean vqec_config_destroy (vqec_config_t *cfg)
{

    if (!cfg) {
        return (FALSE);
    }

    /* walk the tree and free all allocated memory */
    vqec_config_destroy_setting(&cfg->root);
    cfg->error_line = 0;
    cfg->error_text[0] = '\0';

    return (TRUE);
}
