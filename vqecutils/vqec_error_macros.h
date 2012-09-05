/********************************************************************
 * vqec_error_macros.h
 * 
 * Definitions of the ARR macros for VQE errors.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 **********************************************************************/

/* Clean up first */
#undef ARR_BEGIN
#undef ARR_ELEM
#undef ARR_DONE

#if !defined(__VQEC_ERROR_ITEM__)
#define __VQEC_ERROR_ITEM__
typedef struct vqec_error_item_ 
{
    const char *name;
    const char *helpstr;
} vqec_error_item_t;
#endif /* __VQEC_ERROR_ITEM__ */

#if defined(__DECLARE_ERROR_ARR__)

 #define ARR_BEGIN(arr) vqec_error_item_t arr[] = {
 #define ARR_ELEM(name, help) {#name, help},
 #define ARR_DONE {NULL, NULL} };

#elif !defined(__DECLARE_ERROR_ARR_NUMS__)

 #define __DECLARE_ERROR_ARR_NUMS__
 #define ARR_BEGIN(arr) typedef enum { 
 #define ARR_ELEM(name, help) name,
 #define ARR_DONE } vqec_error_t;

#else

 #define ARR_BEGIN(arr)
 #define ARR_ELEM(name, help)
 #define ARR_DONE

#endif /* __DECLARE_ERROR_ARR__ */

