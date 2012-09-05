/*------------------------------------------------------------------
 * SDP services specific to IOS
 *
 * June 2001, D. Renee Revis
 *
 * Copyright (c) 2000-2002, 2004-2005 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include "../../osal/os_al.h"
#include "sdp.h"
#include "sdp_private.h"

#include "msg_sdp.c"


/******************************************************************/
/*  Required Platform Routines                                    */
/*                                                                */
/*     These routines are called from the common SDP code.        */
/*     They must be provided for each platform.                   */
/*                                                                */
/******************************************************************/
void sdp_log_errmsg (sdp_errmsg_e err_msg, char *str)
{
    switch (err_msg) {

    case SDP_ERR_INVALID_CONF_PTR:
        errmsg(&msgsym(CONFIG_PTR_ERROR, SDP));
        break;

    case SDP_ERR_INVALID_SDP_PTR:
        errmsg(&msgsym(SDP_PTR_ERROR, SDP));
        break;

    case SDP_ERR_INTERNAL:
        errmsg(&msgsym(INTERNAL, SDP), str);
        break;

    default:
        break;
    }
}

/*
 * sdp_dump_buffer 
 *
 * Utility to send _size_bytes of data from the string
 * pointed to by _ptr to the buginf function. This may make 
 * multiple buginf calls if the buffer is too large for buginf.
 */
void sdp_dump_buffer (char * _ptr, int _size_bytes)			
{      
    u16 _local_size_bytes = _size_bytes;			
    char *_local_ptr = _ptr;				

    while (_local_size_bytes > LOGDATA-1) {			
	buginf("%*s", LOGDATA-1, _local_ptr);		

        _local_size_bytes -= LOGDATA-1;			
        _local_ptr += LOGDATA-1;				
    }	
						
    buginf("%*s", _local_size_bytes, _local_ptr);	
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

/* There are currently no platform specific routines required. */
