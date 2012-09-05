/*
 *------------------------------------------------------------------
 * sdp_utils.c  -- Utility routines for the generic SDP parser.
 *
 * March 2001, D. Renee Revis
 *
 * Copyright (c) 2001-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include <ctype.h>
#include "sdp_os_defs.h"
#include "sdp.h"
#include "sdp_private.h"



sdp_mca_t *sdp_alloc_mca () {
    sdp_mca_t           *mca_p;

    /* Allocate resource for new media stream. */
    mca_p = (sdp_mca_t *)SDP_MALLOC(sizeof(sdp_mca_t));
    if (mca_p == NULL) {
        return (NULL);
    }
    /* Initialize mca structure */
    mca_p->media              = SDP_MEDIA_INVALID;
    mca_p->conn.nettype       = SDP_NT_INVALID;
    mca_p->conn.addrtype      = SDP_AT_INVALID;
    mca_p->conn.conn_addr[0]  = '\0';
    mca_p->conn.is_multicast  = FALSE;
    mca_p->conn.ttl           = 0;
    mca_p->conn.num_of_addresses = 0;
    mca_p->transport          = SDP_TRANSPORT_INVALID;
    mca_p->port               = SDP_INVALID_VALUE;
    mca_p->num_ports          = SDP_INVALID_VALUE;
    mca_p->vpi                = SDP_INVALID_VALUE;
    mca_p->vci                = 0;
    mca_p->vcci               = SDP_INVALID_VALUE;
    mca_p->cid                = SDP_INVALID_VALUE;
    mca_p->num_payloads       = 0;
    mca_p->sessinfo_found     = FALSE;
    mca_p->encrypt.encrypt_type  = SDP_ENCRYPT_INVALID;
    mca_p->media_attrs_p      = NULL;
    mca_p->next_p             = NULL;
    mca_p->bw.bw_data_count   = 0;
    mca_p->bw.bw_data_list    = NULL;

    return (mca_p);
}

/*
 * verify_sdescriptions_mki
 * 
 * Verifies the syntax of the MKI parameter.
 *
 * mki            = mki-value ":" mki-length 
 * mki-value      = 1*DIGIT 
 * mki-length     = 1*3DIGIT   ; range 1..128
 *
 * Inputs:
 *   buf      - ptr to start of MKI string assumes NULL 
 *              terminated string
 *   mkiValue - buffer to store the MKI value, assumes calling 
 *              function has provided memory for this.
 *   mkiLen   - integer to store the MKI length
 *
 * Outputs:
 *   Returns TRUE if syntax is correct and stores the 
 *   MKI value in mkiVal and stores the length in mkiLen.
 *   Returns FALSE otherwise.
 */

tinybool
verify_sdescriptions_mki (char *buf, char *mkiVal, u16 *mkiLen)
{

    char       *ptr,
               mkiValBuf[SDP_SRTP_MAX_MKI_SIZE_BYTES],
	       mkiLenBuf[4];
    int        idx = 0;
    
    ptr = buf;
    /* MKI must begin with a digit */
    if (!ptr || (!isdigit((int) *ptr))) {
        return FALSE;
    }
   
    /* scan until we reach a non-digit or colon */
    while (*ptr) {
        if (*ptr == ':') {
	    /* terminate the MKI value */
	    mkiValBuf[idx] = 0;
	    ptr++;
	    break;
	} else if ((isdigit((int) *ptr) && (idx < SDP_SRTP_MAX_MKI_SIZE_BYTES-1))) {
	     mkiValBuf[idx++] = *ptr;
	} else {
	     return FALSE;
	}
	   
	ptr++;
    }
    
    /* there has to be a mki length */
    if (*ptr == 0) {
        return FALSE;
    } 
	
    idx = 0;
    
    /* verify the mki length (max 3 digits) */
    while (*ptr) {
        if (isdigit((int) *ptr) && (idx < 3)) {
	    mkiLenBuf[idx++] = *ptr;
	} else {
	    return FALSE;
	}
	
	ptr++;
    }
    
    mkiLenBuf[idx] = 0;
    *mkiLen = atoi(mkiLenBuf);
    
    /* mki len must be between 1..128 */
    if (*mkiLen > 0 && *mkiLen <= 128) {
	 strcpy(mkiVal, mkiValBuf);
         return TRUE;
    } else {
         return FALSE;
    }
    
}

/*
 * verify_srtp_lifetime
 *
 * Verifies the Lifetime parameter syntax.
 *
 *  lifetime = ["2^"] 1*(DIGIT)
 *
 * Inputs:
 *   buf - pointer to start of lifetime string. Assumes string is 
 *         NULL terminated.
 * Outputs:
 *   Returns TRUE if syntax is correct. Returns FALSE otherwise.
 */
 
tinybool
verify_sdescriptions_lifetime (char *buf)
{

    char     *ptr;
    tinybool tokenFound = FALSE;
	    
    ptr = buf;
    if (!ptr || *ptr == 0) {
        return FALSE;
    }
    
    while (*ptr) {
        if (*ptr == '^') {
	    if (tokenFound) {
	        /* make sure we don't have multiple ^ */
                return FALSE;
            } else {
                tokenFound = TRUE;
                /* Lifetime is in power of 2 format, make sure first and second
		 * chars are 2^
		 */
		 
                if (buf[0] != '2' || buf[1] != '^') {
		    return FALSE;
                }
            }
        } else if (!isdigit((int) *ptr)) {
	           return FALSE;
        }
    
        ptr++;
	
    }
    
    /* Make sure if the format is 2^ that there is a number after the ^. */
    if (tokenFound) {
        if (strlen(buf) <= 2) {
	    return FALSE;
	}
    }
    
    return TRUE;
}


/*
 * sdp_validate_floating_point_string
 *
 * This function validates that the string passed in is of the form:
 * 1*DIGIT ["." 1*DIGIT]
 */
tinybool
sdp_validate_floating_point_string(const char *string_parm)
{
    tinybool retval = FALSE;

    if (string_parm && (*string_parm)) {
        while (isdigit((int)*string_parm)) {
            string_parm++;
        }

        if (*string_parm == '.') {
            string_parm++;
            while (isdigit((int)*string_parm)) {
                string_parm++;
            }
        } 

        if (*string_parm == '\0') {
            retval = TRUE;
        } else {
            retval = FALSE;
        }
    }

    return retval;
}

char *sdp_findchar (const char *ptr, char *char_list)
{
    int i;

    for (;*ptr != '\0'; ptr++) {
	for (i=0; char_list[i] != '\0'; i++) {
	    if (*ptr == char_list[i]) {
		return ((char *)ptr);
	    }
	}
    }
    return ((char *)ptr);
}

/* Locate the next token in a line.  The delim characters are passed in
 * as a param.  The token also will not go past a new line char or the 
 * end of the string.  Skip any delimiters before the token.
 */
char *sdp_getnextstrtok (const char *str, char *tokenstr, 
                         char *delim, sdp_result_e *result)
{
    char *b, *tmp;
    int   flag2moveon;

    if ((str == NULL) || (tokenstr == NULL) || (delim == NULL)) {
        *result = SDP_FAILURE;
        return( (char *)str );
    }

    /* Locate front of token, skipping any delimiters */
    for ( ; ((*str != '\0') && (*str != '\n') && (*str != '\r')); str++) {
        flag2moveon = 1;  /* Default to move on unless we find a delimiter */
        for (b=delim; *b; b++) {
            if (*str == *b) {
                flag2moveon = 0;
            }
        }
        if( flag2moveon ) {
            break;  /* We're at the beginning of the token */
        }
    }
   
    /* Make sure there's really a token present. */
    if (strlen(delim)>0 && ((*str == '\0') || (*str == '\n') || (*str == '\r'))) {
        *result = SDP_FAILURE;
        return( (char *)str );
    }

    /* Now locate end of token */
    flag2moveon = 0;
    tmp = tokenstr;

    while (((tokenstr-tmp) < SDP_MAX_STRING_LEN) &&
           (*str != '\0') && (*str != '\n') && (*str != '\r')) {
        for (b=delim; *b; b++) {
            if (*str == *b) {
                flag2moveon = 1;
                break;
            }
        }

        if( flag2moveon ) {
            break;
        } else {
            *tokenstr++ = *str++;
        }
    }

    *tokenstr = '\0';     /* mark end of token */
    if (tokenstr-tmp == SDP_MAX_STRING_LEN) {
        *result = SDP_FIELD_VALUE_OVERFLOW;
    }
    else {
        *result = SDP_SUCCESS;
    }

    return((char *)str);
}



/* Locate the next null ("-") or numeric token in a string.  The delim
 * characters are passed in as a param.  The token also will not go past
 * a new line char or the end of the string.  Skip any delimiters before
 * the token.
 */
u32 sdp_getnextnumtok_or_null (const char *str, const char **str_end, 
                               char *delim, tinybool *null_ind,
                               sdp_result_e *result)
{
    char *b;
    char  tmp[SDP_MAX_STRING_LEN+1];
    char *tok = tmp;
    u32   numval=0;
    int   flag2moveon;

    if ((str == NULL)  || (str_end == NULL)) {
        *result = SDP_FAILURE;
        return(numval);
    }

    /* Locate front of token, skipping any delimiters */
    for ( ; ((*str != '\0') && (*str != '\n') && (*str != '\r')); str++) {
        flag2moveon = 1;  /* Default to move on unless we find a delimiter */
        for (b=delim; *b; b++) {
            if (*str == *b) {
                flag2moveon = 0;
            }
        }
        if( flag2moveon ) {
            break;  /* We're at the beginning of the token */
        }
    }

    /* Make sure there's really a token present. */
    if ((*str == '\0') || (*str == '\n') || (*str == '\r')) {
        *result = SDP_FAILURE;
        *str_end = (char *)str;
        return(numval);
    }

    /* Now locate end of token */
    flag2moveon = 0;

    while (((tok-tmp) < SDP_MAX_STRING_LEN) &&
           (*str != '\0') && (*str != '\n') && (*str != '\r')) {
        for (b=delim; *b; b++) {
            if (*str == *b) {
                flag2moveon = 1;
                break;
            }
        }
        if( flag2moveon ) {
            break;
        } else {
            *tok++ = *str++;
        }
    }

    *tok = '\0';     /* mark end of token */

    /* First see if its the null char ("-") */
    if (tmp[0] == '-') {
        *null_ind = TRUE;
        *result = SDP_SUCCESS;
        *str_end = (char *)str;
        return (0);
    } else {
        *null_ind = FALSE;
    }

    /* Convert to numeric */
    for (tok = tmp; *tok != '\0'; tok++) {
        if (!(*tok >= '0' && *tok <= '9')) {
            *result = SDP_FAILURE;
            *str_end = (char *)str;
            return(numval);
        } else {
            numval = (numval * 10) + (*tok - '0');
        }
    }

    *result = SDP_SUCCESS;
    *str_end = (char *)str;
    return(numval);
}


/* Locate the next numeric token in a string.  The delim characters are 
 * passed in as a param.  The token also will not go past a new line char 
 * or the end of the string.  Skip any delimiters before the token.
 */
u32 sdp_getnextnumtok (const char *str, const char **str_end, 
                       char *delim, sdp_result_e *result)
{
    char *b;
    char  tmp[SDP_MAX_STRING_LEN+1];
    char *tok = tmp;
    long   numval=0;
    int   flag2moveon;

    if ((str == NULL)  || (str_end == NULL)) {
        *result = SDP_FAILURE;
        return 0;
    }

    /* Locate front of token, skipping any delimiters */
    for ( ; ((*str != '\0') && (*str != '\n') && (*str != '\r')); str++) {
        flag2moveon = 1;  /* Default to move on unless we find a delimiter */
        for (b=delim; *b; b++) {
            if (*str == *b) {
                flag2moveon = 0;
            }
        }
        if( flag2moveon ) {
            break;  /* We're at the beginning of the token */
        }
    }

    /* Make sure there's really a token present. */
    if ((*str == '\0') || (*str == '\n') || (*str == '\r')) {
        *result = SDP_FAILURE;
        *str_end = (char *)str;
        return 0;
    }

    /* Now locate end of token */
    flag2moveon = 0;

    while (((tok-tmp) < SDP_MAX_STRING_LEN) &&
           (*str != '\0') && (*str != '\n') && (*str != '\r')) {
        for (b=delim; *b; b++) {
            if (*str == *b) {
                flag2moveon = 1;
                break;
            }
        }
        if( flag2moveon ) {
            break;
        } else {
            *tok++ = *str++;
        }
    }

    *tok = '\0';     /* mark end of token */

    /* Check it to see any non-numeric characters exist */
    for (tok = tmp; *tok != '\0'; tok++) {
        if (!(*tok >= '0' && *tok <= '9')) {
            *result = SDP_INVALID_PARAMETER;
            *str_end = (char *)str;
            return (numval);
        }
    }

    /* Convert to numeric */
    errno = 0;
    numval = strtol(tmp, (char**)NULL, 10);

    /* Check for various possible errors */
    if (errno == ERANGE && (numval == LONG_MAX || numval == LONG_MIN)) {
        *result = SDP_FIELD_VALUE_OVERFLOW;
    }
    else {
        *result = SDP_SUCCESS;
    }
     
   *str_end = (char *)str;
   return (u32)(numval);
}


/* See if the next token in a string is the choose character.  The delim 
 * characters are passed in as a param.  The check also will not go past 
 * a new line char or the end of the string.  Skip any delimiters before 
 * the token.
 */
tinybool sdp_getchoosetok (const char *str, char **str_end, 
                           char *delim, sdp_result_e *result)
{
    char *b;
    int   flag2moveon;

    if ((str == NULL)  || (str_end == NULL)) {
        *result = SDP_FAILURE;
        return(FALSE);
    }

    /* Locate front of token, skipping any delimiters */
    for ( ; ((*str != '\0') && (*str != '\n') && (*str != '\r')); str++) {
        flag2moveon = 1;  /* Default to move on unless we find a delimiter */
        for (b=delim; *b; b++) {
            if (*str == *b) {
                flag2moveon = 0;
            }
        }
        if( flag2moveon ) {
            break;  /* We're at the beginning of the token */
        }
    }

    /* Make sure there's really a token present. */
    if ((*str == '\0') || (*str == '\n') || (*str == '\r')) {
        *result = SDP_FAILURE;
        *str_end = (char *)str;
        return(FALSE);
    }

    /* See if the token is '$' followed by a delimiter char or end of str. */
    if (*str == '$') {
        str++;
        if ((*str == '\0') || (*str == '\n') || (*str == '\r')) {
            *result = SDP_SUCCESS;
            /* skip the choose char in the string. */
            *str_end = (char *)(str+1);
            return(TRUE);
        }
        for (b=delim; *b; b++) {
            if (*str == *b) {
                *result = SDP_SUCCESS;
                /* skip the choose char in the string. */
                *str_end = (char *)(str+1);
                return(TRUE);
            }
        }
    }

    /* If the token was not '$' followed by a delim, token is not choose */
    *result = SDP_SUCCESS;
    *str_end = (char *)str;
    return(FALSE);

}


/* Locate the next token in a line.  The delim characters are passed in
 * as a param.  The token also will not go past a new line char or the 
 * end of the string.  An empty character is returned if contiguous delimiters
 * are found with no token.
 */
char *sdp_getnextstrtok_noskip (const char *str, char *tokenstr, 
                         char *delim, sdp_result_e *result)
{
    char *b, *tmp;
    int   flag2moveon, delimiter_count;

    if ((str == NULL) || (tokenstr == NULL)) {
        *result = SDP_FAILURE;
        return( (char *)str );
    }

    /* Locate front of token. If Contiguous delimiters return '\0' */
    delimiter_count = 0;
    for ( ; ((*str != '\0') && (*str != '\n') && (*str != '\r')); str++) {
        flag2moveon = 1;  /* Default to move on unless we find a delimiter */
        for (b=delim; *b; b++) {
            if (*str == *b) {
                flag2moveon = 0;
		if (++delimiter_count > 1) {
		  *tokenstr = '\0';     /* mark end of token */
		  return( (char *)str );
		}
            }
        }
        if( flag2moveon ) {
            break;  /* We're at the beginning of the token */
        }
    }
   
    /* Make sure there's really a token present. */
    if ((*str == '\0') || (*str == '\n') || (*str == '\r')) {
        *result = SDP_FAILURE;
        return( (char *)str );
    }

    /* Now locate end of token */
    flag2moveon = 0;
    tmp = tokenstr;

    while (((tokenstr-tmp) < SDP_MAX_STRING_LEN) &&
           (*str != '\0') && (*str != '\n') && (*str != '\r')) {
        for (b=delim; *b; b++) {
            if (*str == *b) {
                flag2moveon = 1;
                break;
            }
        }

        if( flag2moveon ) {
            break;
        } else {
            *tokenstr++ = *str++;
        }
    }

    *tokenstr = '\0';     /* mark end of token */

    *result = SDP_SUCCESS;
    return((char *)str);
}

/*
 * SDP Crypto Utility Functions.
 *
 * First a few common definitions.
 */

/* 
 * Constants 
 *
 * crypto_string = The string used to identify the start of sensative 
 *	crypto data.
 *
 * inline_string = The string used to identify the start of key/salt
 *	crypto data.
 *
 * star_string = The string used to overwrite sensative data.
 *
 * '*_strlen' = The length of '*_string' in bytes (not including '\0')
 */ 
static const char crypto_string[] = "X-crypto:";
static const int crypto_strlen = sizeof(crypto_string) - 1;
static const char inline_string[] = "inline:";
static const int inline_strlen = sizeof(inline_string) - 1;
/* 40 characters is the current maximum for a Base64 encoded key/salt */
static const char star_string[] = "****************************************";
static const int star_strlen = sizeof(star_string) - 1;

/*
 * MIN_CRYPTO_STRING_SIZE_BYTES = This value defines the minimum
 *	size of a string that could contain a key/salt. This value
 *	is used to skip out of parsing when there is no reasonable
 *	assumption that sensative data will be found. The general
 *	format of a SRTP Key Salt in SDP looks like:
 *
 * X-crypto:<crypto_suite_name> inline:<master_key_salt>||
 *
 *	if <crypto_suite_name> and <master_key_salt> is at least
 *	one character and one space is used before the "inline:",
 *	then this translates to a size of (aligned by collumn from
 *	the format shown above):
 * 
 * 9+       1+                 1+7+    1+                2 = 21
 *
 */
#define MIN_CRYPTO_STRING_SIZE_BYTES 21

/* 
 * Utility macros 
 *
 * CHAR_IS_WHITESPACE = macro to determine if the passed _test_char
 *	is whitespace.
 *
 * SKIP_WHITESPACE = Macro to advance _cptr to the next non-whitespace 
 *	character. _cptr will not be advanced past _max_cptr.
 *
 * FIND_WHITESPACE = Macro to advance _cptr until whitespace is found.
 *	_cptr will not be advanced past _max_cptr.
 */
#define CHAR_IS_WHITESPACE(_test_char) \
    ((((_test_char)==' ')||((_test_char)=='\t'))?1:0)

#define SKIP_WHITESPACE(_cptr, _max_cptr)	    \
    while ((_cptr)<=(_max_cptr)) {		    \
	if (!CHAR_IS_WHITESPACE(*(_cptr))) break;   \
	(_cptr)++;				    \
    }

#define FIND_WHITESPACE(_cptr, _max_cptr)	    \
    while ((_cptr)<=(_max_cptr)) {		    \
	if (CHAR_IS_WHITESPACE(*(_cptr))) break;    \
	(_cptr)++;				    \
    }

/* Function:    sdp_crypto_debug
 * Description: Check the passed buffer for sensitive data that should
 *		not be output (such as SRTP Master Key/Salt) and output
 *		the buffer as debug. Sensitive data will be replaced 
 *		with the '*' character(s). This function may be used 
 *		to display very large buffers so this function ensures
 *		that buginf is not overloaded.
 * Parameters:  buffer		pointer to the message buffer to filter.
 *              length_bytes	size of message buffer in bytes.
 * Returns:     Nothing.
 */
void sdp_crypto_debug (char *buffer, ulong length_bytes)
{
    char *current, *start;
    char *last = buffer + length_bytes;
    int result;

    /*
     * For SRTP Master Key/Salt has the form:
     * X-crypto:<crypto_suite_name> inline:<master_key_salt>||
     * Where <master_key_salt> is the data to elide (filter).
     */
    for (start=current=buffer; 
	 current<=last-MIN_CRYPTO_STRING_SIZE_BYTES; 
	 current++) {
	if ((*current == 'x') || (*current == 'X')) {
	    result = strncasecmp(current, crypto_string, crypto_strlen);
	    if (!result) {
		current += crypto_strlen;
		if (current > last) break;

		/* Skip over crypto suite name */
		FIND_WHITESPACE(current, last);

		/* Skip over whitespace */
		SKIP_WHITESPACE(current, last);

		/* identify inline keyword */
		result = strncasecmp(current, inline_string, inline_strlen);
		if (!result) {
		    int star_count = 0;

		    current += inline_strlen;
		    if (current > last) break;

		    sdp_dump_buffer(start, current - start);
		    start = current;

		    /* Hide sensitive key/salt data */
		    while (current<=last) {
			if (*current == '|' || *current == '\n') {
			    /* Done, print the stars */
			    while (star_count > star_strlen) {
				/* 
				 * This code is only for the case where
				 * too much base64 data was supplied
				 */
				sdp_dump_buffer((char*)star_string, star_strlen);
				star_count -= star_strlen;
			    }
			    sdp_dump_buffer((char*)star_string, star_count);
			    break;
			} else {
			    star_count++;
			    current++;
			}
		    }
		    /* Update start pointer */
		    start=current;
		}
	    }
	}
    }

    if (last > start) {
	/* Display remainder of buffer */
	sdp_dump_buffer(start, last - start);
    }
}

/* 
 * sdp_debug_msg_filter
 *
 * DESCRIPTION
 *     Check the passed message buffer for sensitive data that should
 *     not be output (such as SRTP Master Key/Salt). Sensitive data
 *     will be replaced with the '*' character(s).
 *     
 * PARAMETERS
 *     buffer: pointer to the message buffer to filter.
 *
 *     length_bytes: size of message buffer in bytes.
 *
 * RETURN VALUE
 *     The buffer modified.
 */
char * sdp_debug_msg_filter (char *buffer, ulong length_bytes)
{
    char *current;
    char *last = buffer + length_bytes;
    int result;

    SDP_PRINT("\nsdp_debug_msg_filter: Eliding sensitive data from debug output");
    
    /*
     * For SRTP Master Key/Salt has the form:
     * X-crypto:<crypto_suite_name> inline:<master_key_salt>||
     * Where <master_key_salt> is the data to elide (filter).
     */
    for (current=buffer;
	 current<=last-MIN_CRYPTO_STRING_SIZE_BYTES;
	 current++) {
	if ((*current == 'x') || (*current == 'X')) {
	    result = strncasecmp(current, crypto_string, crypto_strlen);
	    if (!result) {
		current += crypto_strlen;
		if (current > last) break;

		/* Skip over crypto suite name */
		FIND_WHITESPACE(current, last);

		/* Skip over whitespace */
		SKIP_WHITESPACE(current, last);

		/* identify inline keyword */
		result = strncasecmp(current, inline_string, inline_strlen);
		if (!result) {
		    current += inline_strlen;
		    if (current > last) break;

		    /* Hide sensitive key/salt data */
		    while (current<=last) {
			if (*current == '|' || *current == '\n') {
			    /* Done */
			    break;
			} else {
			    *current = '*';
			    current++;
			}
		    }
		}
	    }
	}
    }

    return buffer;
}


/* Function:    sdp_checkrange
 * Description: This checks the range of a ulong value to make sure its 
 *              within the range of 0 and 4Gig. stroul cannot be used since
 *              for values greater greater than 4G, stroul will either wrap 
 *              around or return ULONG_MAX. 
 * Parameters:  sdp_p       Pointer to the sdp structure
 *              num         The number to check the range for
 *              u_val       This variable get populated with the ulong value
 *                          if the number is within the range. 
 * Returns:     tinybool - returns TRUE if the number passed is within the
 *                         range, FALSE otherwise
 */
tinybool sdp_checkrange (sdp_t *sdp_p, char *num, ulong *u_val)  
{
    ulong l_val;
    char *endP = NULL;
    *u_val = 0;

    if (!num || !*num) {
        return FALSE;
    }

    if (*num == '-') {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s ERROR: Parameter value is a negative number: %s",
                      sdp_p->debug_str, num);
        }
        return FALSE;
    }

    l_val = strtoul(num, &endP, 10);
    if (*endP == '\0') {

        if (l_val > 4294967295UL) {
	    if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
		SDP_ERROR("%s ERROR: Parameter value: %s is greater than 4294967295",
			  sdp_p->debug_str, num);
	    }
	    return FALSE;
	}

	if (l_val == 4294967295UL) {
	    /*
	     * On certain platforms where ULONG_MAX is equivalent to
	     * 4294967295, strtoul will return ULONG_MAX even if the the
	     * value of the string is greater than 4294967295. To detect
	     * that scenario we make an explicit check here.
	     */
	    if (strcmp("4294967295", num)) {
		if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
		    SDP_ERROR("%s ERROR: Parameter value: %s is greater than 4294967295",
			      sdp_p->debug_str, num);
		}
		return FALSE;
	    }
	}
    }
    *u_val = l_val;
    return TRUE;
}


tinybool sdp_check_for_non_numeric (const char *ptr)
{
    int i;

    for (i = 0; i < (int)strlen(ptr); i++) {
        if (!(ptr[i] >= '0' && ptr[i] <= '9')) {
            return TRUE;
        }
    }

    return FALSE;
}


#undef CHAR_IS_WHITESPACE
#undef SKIP_WHITESPACE
#undef FIND_WHITESPACE
