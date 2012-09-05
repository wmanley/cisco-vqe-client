/*------------------------------------------------------------------
 * SDP services specific to Unix
 *
 * June 2001, D. Renee Revis
 *
 * Copyright (c) 2000-2001, 2005 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include "sdp_os_defs.h"
#include "sdp.h"
#include "sdp_private.h"


/******************************************************************/
/*  Required Platform Routines                                    */
/*                                                                */
/*     These routines are called from the common SDP code.        */
/*     They must be provided for each platform.                   */
/*                                                                */
/******************************************************************/
void sdp_log_errmsg (sdp_errmsg_e errmsg, char *str)
{
    switch (errmsg) {

    case SDP_ERR_INVALID_CONF_PTR:
        SDP_ERROR("\nSDP: Invalid Config pointer.");
        break;

    case SDP_ERR_INVALID_SDP_PTR:
        SDP_ERROR("\nSDP: Invalid SDP pointer.");
        break;

    case SDP_ERR_INTERNAL:
        SDP_ERROR("\nInternal SDP error: %s", str);
        break;

    default:
        break;
    }
}


/******************************************************************/
/*                                                                */
/*  Platform Specific Routines                                    */
/*                                                                */
/*    These routines are only used in this particular platform.   */
/*    They are called from the required platform specific         */
/*    routines provided below, not from the common SDP code.      */
/*                                                                */
/******************************************************************/

/*
 * This is cisco's *safe* version of strncpy.  Note that it will always NULL
 * terminate the string if the source is greater than or equal to max.  In 
 * these situations, ANSI strncpy would copy the string up to the max and NOT 
 * terminate the string.   For historical reference, see: 
 * /csc/Docs/strncpy.flames
 */
/* */char *
sstrncpy (char *dst, char const *src, unsigned long max)
{
    char *orig_dst = dst;

    while ((max-- > 1) && (*src)) {
	*dst = *src;
	dst++;
	src++;
    }
    *dst = '\0';

    return(orig_dst);
}
/* */
#define LOGDATA 100

void sdp_dump_buffer (char * _ptr, int _size_bytes)			
{      
    u16 _local_size_bytes = _size_bytes;			
    char *_local_ptr = _ptr;				

    while (_local_size_bytes > LOGDATA-1) {			
	printf("%.*s", LOGDATA-1, _local_ptr);		

        _local_size_bytes -= LOGDATA-1;			
        _local_ptr += LOGDATA-1;				
    }	
						
    printf("%.*s", _local_size_bytes, _local_ptr);	
}
