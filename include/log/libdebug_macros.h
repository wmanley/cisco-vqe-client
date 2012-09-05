/********************************************************************
 * libdebug_macros.h
 * 
 * Definitions of the ARR macros of the VQE Debug Library.
 * Derived from DC-OS include/isan/libdebug_macros.h
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 **********************************************************************/

#include <log/libdebug.h>

/* Clean up first */
#undef ARR_BEGIN
#undef ARR_ELEM
#undef ARR_DONE
#undef ARR_ELEM_NO_DECL

#if defined(__DECLARE_DEBUG_ARR__)

#define ARR_BEGIN(arr) debug_item_t arr[] = {
#define ARR_ELEM(var,code,desc,flag) {&var, desc, flag, NULL},
#define ARR_ELEM_NO_DECL(var,code,desc,flag) {&var, desc, flag, NULL},
#define ARR_DONE {(boolean *) NULL, (char *) NULL, 0, NULL} };

#elif defined(__DECLARE_DEBUG_NUMS__)

#define ARR_BEGIN(arr) enum { 
#define ARR_ELEM(var,code,desc,flag) code,
#define ARR_ELEM_NO_DECL(var,code,desc,flag) code,
#define ARR_DONE };

#elif defined(__DECLARE_DEBUG_EXTERN_VARS__)

#define ARR_BEGIN(arr) extern debug_item_t arr[];
#define ARR_ELEM(var,code,desc,flag) extern boolean var;
#define ARR_ELEM_NO_DECL(var,code,desc,flag) extern boolean var;
#define ARR_DONE /* No Definition */

#else

#define ARR_BEGIN(arr) extern debug_item_t arr[];
#define ARR_ELEM(var,code,desc,flag) boolean var;
#define ARR_ELEM_NO_DECL(var,code,desc,flag) /* No Definition */
#define ARR_DONE /* No Definition */

#endif

/* Some other include file might want to reuse us... */
#undef __DECLARE_DEBUG_ARR__
#undef __DECLARE_DEBUG_NUMS__
#undef __DECLARE_DEBUG_EXTERN_VARS__
