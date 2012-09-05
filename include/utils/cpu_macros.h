/*------------------------------------------------------------------
 *
 * July 2006, Atif Faheem
 *
 * Copyright (c) 2006 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __CPU_MACROS_H__
#define __CPU_MACROS_H__

#include <endian.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define LITTLEENDIAN 1
#endif // __BYTE_ORDER

/*
 * These inlines are used only for big-endian machines. 
*/

/*
 * getshort_inline
 *
 * read short from misaligned memory
 */
static inline
void putshort_inline(register void *ptr, unsigned short value)
{
    /*
     * The generated code turns out to be no worse than the
     * in-line assembly code. Besides, the in-line assembly
     * code causes some problems when further included by
     * putlong_inline(). So decided to use C code.
     */
    ((unsigned char *)ptr)[1] = (unsigned char)value;
    ((unsigned char *)ptr)[0] = (unsigned char)(value >> 8);
}

/*
 * putlong_inline
 *
 * write long to misaligned memory
 */
static inline
void putlong_inline(register void *ptr, register unsigned long value)
{
    putshort_inline(ptr, (unsigned short)(value >> 16));
    putshort_inline((unsigned short *)ptr+1, (unsigned short)value);
}

/*
 * getshort_inline
 *
 * read short from misaligned memory
 */
static inline
unsigned short getshort_inline(register void const *ptr)
{
    /*
     * The generated code turns out to be no worse than the
     * in-line assembly code. Besides, the in-line assembly
     * code causes some problems when further included by
     * getlong_inline(). So decided to use C code.
     */
    return ( ((unsigned char *)ptr)[0] << 8 | ((unsigned char *)ptr)[1] );
}

/*
 * getlong_inline
 *
 * write long to misaligned memory
 */
static inline
unsigned long getlong_inline(register void const *ptr)
{
    return( getshort_inline(ptr) << 16 | getshort_inline((unsigned short *)ptr+1) );
}

#define GETLONG(ptr)		getlong_inline(ptr)
#define PUTLONG(ptr, val)	putlong_inline((ptr), (val))


#endif // __CPU_MACROS_H__
