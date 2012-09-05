/*
 *------------------------------------------------------------------
 * sdp_os_defs.h
 *
 * Specific platform definitions for SDP to provide portability
 *
 * September 2004, Judy Gulla
 *
 * Copyright (c) 2004-2006 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _SDP_OS_DEFS_H_
#define _SDP_OS_DEFS_H_


/********************************/
/* SDP OS Specific Definitions */
/********************************/


/* Define SDP_UNIX version if compiling for unix */
/*#define SDP_UNIX*/
#define SDP_LINUX

/****************************************************/
/* Add all include files for possible OS types      */
/****************************************************/

#ifdef TARGET_CISCO

#include "../../osal/os_al.h"

#endif
#ifdef SDP_UNIX

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#endif
#ifdef SDP_LINUX

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <strings.h>
#include <string.h>

#endif

#ifdef TARGET_CISCO_GPP
#include "sdp-gpp/sdp_gpp_defs.h"
#endif



/****************************************************/
/* Required defines for all possible OS types       */
/****************************************************/


#ifdef TARGET_CISCO

#define SDP_NAME "SDP Library"  // Used with malloc_named

#endif

#ifdef SDP_UNIX
#define TRUE     1
#define FALSE    0
#endif

#ifdef SDP_LINUX
#define TRUE     1
#define FALSE    0
#endif

#ifdef TARGET_CISCO_GPP
#define TRUE     1
#define FALSE    0
#endif




/****************************************************/
/* Define required macros for all possible OS types */
/****************************************************/

#ifdef TARGET_CISCO
#define SDP_ERROR     buginf
#define SDP_WARN      buginf
#define SDP_PRINT     buginf
#define SDP_MALLOC(x) malloc_named(x, SDP_NAME)
#define SDP_FREE      free
#define SDP_MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#if (defined SDP_UNIX ) || (defined SDP_LINUX)
#define SDP_ERROR     printf
#define SDP_WARN      printf
#define SDP_PRINT     printf
#define SDP_MIN(a,b) (((a) < (b)) ? (a) : (b))

#ifdef HARNESS 
#define SDP_MALLOC(x) mc_malloc(x)
#define SDP_FREE      mc_free
#define SDP_STRDUP(x) (char *)mc_strdup(x)
#else 
#define SDP_MALLOC(x) my_malloc(x)
#define SDP_FREE      my_free
/*#define SDP_MALLOC(x) calloc(1,x)
#define SDP_FREE      free */
#define SDP_STRDUP(x) (char *)strdup(x) 
#endif

#endif

#ifdef TARGET_CISCO_GPP
#define SDP_ERROR     printf
#define SDP_WARN      printf
#define SDP_PRINT     printf
#define SDP_MALLOC(x) calloc(1,x)
#define SDP_FREE      free
#define buginf        printf
#define SDP_MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif



/****************************************************/
/* Define required types for all possible OS types  */
/****************************************************/

#ifdef TARGET_CISCO

typedef unsigned char    u8;
typedef unsigned short   u16;
typedef unsigned long    u32;

#endif
#ifdef SDP_UNIX

#ifndef _HARNESS_DPM_H_
typedef unsigned char    tinybool;
#endif
typedef unsigned char    u8;
typedef unsigned short   u16;
typedef unsigned long    u32;

typedef signed long      int32;
typedef signed short     int16;

#endif
#ifdef SDP_LINUX
typedef unsigned char    tinybool;
typedef unsigned char    u8;
typedef unsigned short   u16;
typedef unsigned long    u32;
typedef unsigned short   ushort;
typedef unsigned long    ulong;
typedef signed long      int32;
typedef signed short     int16;

#endif

#ifdef TARGET_CISCO_GPP
typedef unsigned char    tinybool;
typedef unsigned char    u8;
typedef unsigned short   u16;
typedef unsigned long    u32;
typedef signed long      int32;
typedef signed short     int16;
#endif

/****************************************************/
/* OS specific required information.                */
/****************************************************/

#ifdef SDP_UNIX 
extern char *sstrncpy (char *dst, char const *src, unsigned long max);
#endif

#ifdef SDP_LINUX
extern char *sstrncpy (char *dst, char const *src, unsigned long max);
extern void *my_malloc(unsigned bytes);
extern void my_free(void *ap);
#endif

#ifdef TARGET_CISCO_GPP
extern char *sstrncpy (char *dst, char const *src, unsigned long max);
#endif

#endif /* _SDP_OS_DEFS_H_ */
