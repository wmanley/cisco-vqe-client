/********************************************************************
 * example_debug.h
 *
 * This file contains source code that describes how to use
 * the simple VQE Debug Library API.
 *
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#ifndef __EXAMPLE_DEBUG_H__
#define __EXAMPLE_DEBUG_H__

#include "example_debug_flags.h"

#define __DECLARE_DEBUG_NUMS__
#include "example_debug_flags.h"



/*
 *------------------------------------------------------------------------
 * Macros to do debug logging: wrap around buginf() 
 *------------------------------------------------------------------------
 */
#define EX_DEBUG_SET_DEBUG_FLAG(type)                           \
    debug_set_flag(ex_debug_arr, type,  VQENAME_EX_DEBUG, NULL)
#define EX_DEBUG_RESET_DEBUG_FLAG(type)                         \
    debug_reset_flag(ex_debug_arr, type,  VQENAME_EX_DEBUG, NULL)
#define EX_DEBUG_DEBUG(type, arg...)                            \
    do {                                                        \
        if (debug_check_element(ex_debug_arr, type, NULL)) {    \
            buginf(TRUE, VQENAME_EX_DEBUG, FALSE, arg);         \
        }                                                       \
    } while (0)


#endif /* __EXAMPLE_DEBUG_H__ */
