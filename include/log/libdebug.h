/********************************************************************
 * libdebug.h
 *
 * Macros and Data structure templates for the VQE Debug Library
 * Derived from DC-OS include/isan/libdebug.h
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 **********************************************************************/

#ifndef __LIBDEBUG_H__
#define __LIBDEBUG_H__

#include <utils/vam_types.h>

/* Defines for the field "type" in the structure below */
typedef enum debug_filter_type_t_ {
    DEBUG_FILTER_TYPE_CHANNEL=1,  /* Filter based on channel IP:PORT */
    DEBUG_FILTER_TYPE_STB_IP,     /* Filter based on STB IP address */
    DEBUG_MAX_FILTER_TYPE         /* Add new types before this line */
} debug_filter_type_t;



/*******************************************************************/
/* Note: in DC-OS, thses macros are defined with syserr macros  */
#define DEBUG_FILTER_FLAG_FALSE  -1
#define DEBUG_FILTER_BAD_TYPE    -2
#define DEBUG_FILTER_NO_MEM      -3
/*******************************************************************/
#define DEBUG_FILTER_NOT_SET     -4


/* Macro to be used to concatenate a uint32_t IP address and a uint16_t 
 * port number into a uint64_t valure.
 */
#define CONCAT_U32_U16_TO_U64(u32, u16)      \
    ((uint64_t)(u32)<<16 | (u16))

typedef struct debug_filter_item_t_ {
    debug_filter_type_t type;
    /* Opaque filter val:
     *   For FILTER_TYPE_CHANNEL, 16 LSB is the port number; 32 MSB is the 
     *          channel IP address. e.g:
     *             val = ((uint64_t)chan_ip<<16) | chan_port;
     *
     *   For FITLER_STB_IP, 32 LSB is the STB IP address
     */
    uint64_t val; 
} debug_filter_item_t;


/*
 * debug_item_t defines a standard way of describing a debugging
 * flag. Most protocols keep their debugging flags in special
 * cells, which are pointed to by the members of arrays of entries
 * of this type. The generic debugging support provides a few routines
 * for dealing with such arrays. By convention, the end of an array
 * is flagged by a null flag pointer.
 */
typedef struct debug_item_t_ {
    boolean *var;          /* Address of on/off flag */
    char *desc;         /* Descriptive text about what to debug */
    uint32_t flag;     /* Whether this flag is part of DEBUG_ALL etc., */
    debug_filter_item_t *filter; /* Filter for this flag */
} debug_item_t;


/* Defines for the item "flag" above */



/* Macros to manipulate the debug array */

#define DEBUG_SET_FLAG(arr,elem,flag) \
    {                                 \
        (*(arr[elem].var) = (flag));  \
        if (flag == FALSE) {          \
            free(arr[elem].filter);   \
            arr[elem].filter = NULL;  \
	}                             \
    }


/* Function prototypes */
void buginf(boolean, char *, boolean, char *, ...);
void buginf_func(boolean, char *, char *,boolean, char *, ...);
boolean debug_check_element(debug_item_t *, int32_t, debug_filter_item_t *);

int32_t debug_set_filter(debug_item_t *, int32_t, debug_filter_item_t *);
void debug_reset_filter(debug_item_t *, int32_t);

int32_t debug_set_flag(debug_item_t *, int32_t, char *, char *);
int32_t debug_reset_flag(debug_item_t *, int32_t, char *, char *);

/* Get the state of a flag, and the associated filter */
int32_t debug_get_flag_state(debug_item_t *, int32_t, boolean *,
                             debug_filter_item_t **);


#endif /* LIBDEBUG_H */
