/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Kernel time.
 *
 * Documents: 
 *
 *****************************************************************************/

#ifndef __VAM_TIME_H__
#define __VAM_TIME_H__

#include "vam_types.h"

/* 
 * Absolute time since  00:00:00 UTC, January 1, 1970 (see man 2 time). 
 */

typedef struct abs_time_
{
    uint64_t usec; /* OPAQUE.  Use api! */
} abs_time_t;

/*
 * Relative time.
 */

typedef struct rel_time_
{
    int64_t usec; /*OPAQUE! */
} rel_time_t;


/* Create abs_time_t from other representations. */

static inline abs_time_t sec_to_abs_time(uint64_t sec)
{
    abs_time_t result;
    result.usec = 1000000 * sec;
    return result;
}



static inline abs_time_t msec_to_abs_time(uint64_t msec)
{
    abs_time_t result;
    result.usec = 1000 * msec;
    return result;
}



static inline abs_time_t usec_to_abs_time(uint64_t usec)
{
    abs_time_t result;
    result.usec = usec;
    return result;
}



static inline abs_time_t nsec_to_abs_time(uint64_t nsec)
{
    abs_time_t result;
    result.usec = VQE_DIV64(nsec, 1000);
    return result;
}



static inline abs_time_t timeval_to_abs_time(struct timeval tv)
{
    abs_time_t result;
    result.usec = ((uint64_t)tv.tv_sec)*1000000 + tv.tv_usec;
    return result;
}

/* Convert abs_time_t to other representations. */
static inline uint64_t abs_time_to_sec(abs_time_t t)
{
    return (VQE_DIV64(t.usec, 1000000));
}



static inline uint64_t abs_time_to_msec(abs_time_t t)
{
    return (VQE_DIV64(t.usec, 1000));
}


static inline uint64_t abs_time_to_usec(abs_time_t t)
{
    return t.usec;
}


static inline uint64_t abs_time_to_nsec(abs_time_t t)
{
    return t.usec*1000;
}


static inline struct timeval abs_time_to_timeval(abs_time_t t)
{
    struct timeval tv;
    uint64_t secs = VQE_DIV64(t.usec, 1000000);
    tv.tv_sec = secs;
    tv.tv_usec = t.usec - secs*1000000;
    return tv;
}

/* Comparison operations for abs_time_t */
static inline boolean abs_time_lt(abs_time_t t0, abs_time_t t1)
{
    return t0.usec < t1.usec;
}

static inline boolean abs_time_gt(abs_time_t t0, abs_time_t t1)
{
    return t0.usec > t1.usec;
}
static inline boolean abs_time_le(abs_time_t t0, abs_time_t t1)
{
    return t0.usec <= t1.usec;
}

static inline boolean abs_time_ge(abs_time_t t0, abs_time_t t1)
{
    return t0.usec >= t1.usec;
}

static inline boolean abs_time_eq(abs_time_t t0, abs_time_t t1)
{
    return t0.usec == t1.usec;
}
static inline boolean abs_time_ne(abs_time_t t0, abs_time_t t1)
{
    return t0.usec != t1.usec;
}



/* Create rel_time_t from other representations. */

static inline rel_time_t sec_to_rel_time(int64_t sec)
{
    rel_time_t result;
    result.usec = 1000000 * sec;
    return result;
}

static inline rel_time_t msec_to_rel_time(int64_t msec)
{
    rel_time_t result;
    result.usec = 1000*msec;
    return result;
}
static inline rel_time_t usec_to_rel_time(int64_t usec)
{
    rel_time_t result;
    result.usec = usec;
    return result;
}

static inline rel_time_t nsec_to_rel_time(int64_t nsec)
{
    rel_time_t result;
    result.usec = VQE_DIV64(nsec, 1000);
    return result;
}



/* 
 * pcr_to_rel_time
 *
 * Convert 90KHz clock tics to relative time
 */
static inline rel_time_t pcr_to_rel_time(int64_t pcr)
{

/* The tic rate for pcr is 90Khz, so tic time in usec is 
 * 1,000,000/90,000.  Approximate the conversion by 
 *
 * (tics * ((100 << 25) / 9)) >> 25
 * 
 * for good precision without division.
 * $$$assumes pcr small to prevent overflow.
 */

#define PCR_TO_USEC_SHIFT 25
#define PCR_TO_USEC_MULT ((((int64_t)100) << PCR_TO_USEC_SHIFT) / 9)
    rel_time_t result;
    result.usec = (pcr * PCR_TO_USEC_MULT) >> PCR_TO_USEC_SHIFT;
    return result;
}


static inline rel_time_t timeval_to_rel_time(struct timeval * tv)
{
    rel_time_t result;
    result.usec = ((uint64_t)tv->tv_sec)*1000000 + tv->tv_usec;
    return result;
}

/* Convert rel_time_t to other representations. */
static inline int64_t rel_time_to_sec(rel_time_t t)
{
    return (VQE_DIV64(t.usec, 1000000));
}

static inline int64_t rel_time_to_msec(rel_time_t t)
{
    return (VQE_DIV64(t.usec, 1000));
}

static inline int64_t rel_time_to_usec(rel_time_t t)
{
    return t.usec;
}

static inline int64_t rel_time_to_nsec(rel_time_t t)
{
    return t.usec*1000;
}

/* 
 * rel_time_to_pcr
 *
 * Convert relative time to 90KHz clock tics.
 */
static inline int64_t rel_time_to_pcr(rel_time_t t)
{

/* The tic rate for pcr is 90Khz, so tic time in usec is 
 * 1,000,000/90,000.  $$$ only works for small t.
 *
 */
#define USEC_TO_PCR_SHIFT 27
#define USEC_TO_PCR_MULT ((((uint64_t)9) << USEC_TO_PCR_SHIFT) / 100)
    return
        (t.usec*USEC_TO_PCR_MULT)>>USEC_TO_PCR_SHIFT;
}


/* The following may give strange results if t < 0 */
static inline struct timeval rel_time_to_timeval(rel_time_t t)
{
    struct timeval tv;
    int64_t secs = VQE_DIV64(t.usec, 1000000);
    tv.tv_sec = secs;
    tv.tv_usec = t.usec - secs*1000000;
    return tv;
}

/* Comparison operations for rel_time_t */
static inline boolean rel_time_lt(rel_time_t t0, rel_time_t t1)
{
    return t0.usec < t1.usec;
}

static inline boolean rel_time_gt(rel_time_t t0, rel_time_t t1)
{
    return t0.usec > t1.usec;
}
static inline boolean rel_time_le(rel_time_t t0, rel_time_t t1)
{
    return t0.usec <= t1.usec;
}

static inline boolean rel_time_ge(rel_time_t t0, rel_time_t t1)
{
    return t0.usec >= t1.usec;
}

static inline boolean rel_time_eq(rel_time_t t0, rel_time_t t1)
{
    return t0.usec == t1.usec;
}
static inline boolean rel_time_ne(rel_time_t t0, rel_time_t t1)
{
    return t0.usec != t1.usec;
}



/* (abs_time_t + rel_time_t) is an abs_time_t */
static inline abs_time_t abs_plus_rel_time(abs_time_t time, rel_time_t offset)
{
    time.usec += offset.usec;
    return time;
}
static inline abs_time_t abs_minus_rel_time(abs_time_t time, rel_time_t offset)
{
    time.usec -= offset.usec;
    return time;
}

/* (abs_time_t - abs_time_t) is a rel_time_t */
static inline rel_time_t abs_minus_abs_time(abs_time_t t0, abs_time_t t1)
{
    rel_time_t result;
    result.usec = t0.usec - t1.usec;
    return result;
}


static inline rel_time_t rel_plus_rel_time(rel_time_t t0, rel_time_t t1)
{
    rel_time_t result;
    result.usec = t0.usec + t1.usec;
    return result;
}

static inline rel_time_t rel_minus_rel_time(rel_time_t t0, rel_time_t t1)
{
    rel_time_t result;
    result.usec = t0.usec - t1.usec;
    return result;
}
static inline rel_time_t rel_time_neg(rel_time_t t)
{
    t.usec = -t.usec;
    return t;
}

static inline rel_time_t rel_time_mult_int(rel_time_t t, int factor) 
{
    t.usec *= factor;
    return t;
}
static inline rel_time_t rel_time_div_int(rel_time_t t, int divisor) 
{
    t.usec = VQE_DIV64(t.usec, divisor);
    return t;
}

static inline rel_time_t rel_time_lshift(rel_time_t t, uint8_t shift) 
{
    t.usec <<= shift;
    return t;
}

static inline rel_time_t rel_time_rshift(rel_time_t t, uint8_t shift) 
{
    t.usec >>= shift;
    return t;
}

/*
 * Computes (factor0 * factor1) / divisor.
 * Avoiding overflow/underflow is the CALLER's responsibility:
 *   - (factor0 * factor1) should fit into a uint64_t
 *   - divisor should fit into a uint32_t
 */
static inline rel_time_t rel_time_mult_rel_div_rel(rel_time_t factor0,
                                                   rel_time_t factor1,
                                                   rel_time_t divisor)
{
    rel_time_t result;

    result.usec = factor0.usec * factor1.usec;
    do_div(result.usec, (uint32_t)divisor.usec);
    return result;
}

/* Read the system time and return it as an absolute time */
static inline abs_time_t get_sys_time(void)
{
    struct timeval tv;
    (void)VQE_GET_TIMEOFDAY(&tv, 0);

    return timeval_to_abs_time(tv);
}

/* 
 * Macro interface for manipulating time values
 */

/* Relative time constants for second, microsecond and millisecond. */

#define SEC ((rel_time_t) { 1000000 } )
#define USEC ((rel_time_t) {1} )
#define MSEC ((rel_time_t) {1000})

/* Macros for building other relative time constants.  
 * Examples:
 * 
 * SECS(5) --> 5 seconds
 * MSECS(100) --> 100 milliseconds
 */
#define SECS(secs) ((rel_time_t) { 1000000*((int64_t)(secs)) })
#define USECS(usecs) ((rel_time_t) { (int64_t)(usecs) })
#define MSECS(msecs) ((rel_time_t) { 1000*((int64_t)(msecs)) })

/* Constant 0 values for absolute and relative time. */
#define ABS_TIME_0 ((abs_time_t) { 0 })
#define REL_TIME_0 ((rel_time_t) { 0 })

/*
 * Create an absolute time from another type.  Legal other types are:
 * 
 * sec -- seconds
 * msec -- milliseconds
 * usec -- microseconds
 * nsec -- nanoseconds
 * timeval -- a timeval structure, as used in gettimeofday.
 *
 * The result is a rvalue.
 *
 * Examples:
 *
 * // Convert system timeval to an abs_time_t 
 * abs_time_t now;
 * struct timeval tv;
 * gettimeofday(&tv);
 * now = TIME_MK_A(timeval, tv);
 *
 * // Convert microsecs since base date to an abs_time_t
 *
 * t = TIME_MK_A(usec, microsec_value);
 */
#define TIME_MK_A(unit,arg) (unit ## _to_abs_time(arg))

/*
 * Convert an abs_time_t to another representation.  Inverse to
 * TIME_MK_A, with similar usage.  Example:
 *
 * // Convert abs_time_t to a timeval
 * abs_time_t now = get_sys_time();
 * struct timeval tv;
 * 
 * tv = TIME_GET_A(timeval, now);
 *
 */
#define TIME_GET_A(unit,arg) (abs_time_to_ ## unit(arg))

/*
 * Comparison operators for two abs_time_t.  Returns a boolean
 *
 * Usage:
 *
 *  TIME_CMP_A(op, t0, t1) 
 *
 * returns TRUE if (t0 op t1) where op is one of
 *
 * eq
 * lt
 * le
 * gt
 * ge
 * ne
 * 
 */


#define TIME_CMP_A(op, t0, t1) (abs_time_ ## op((t0), (t1)))

/*
 * Conversions and comparisons for rel_time_t values.  See the
 * corresponding abs_time_t macros for usage.
 */

#define TIME_MK_R(unit,arg) (unit ## _to_rel_time(arg))
#define TIME_GET_R(unit,arg) (rel_time_to_ ## unit(arg))
#define TIME_CMP_R(op, t0, t1) (rel_time_ ## op((t0), (t1)))

/*
 * Arithmetic operations on absolute and relative time.  the macro
 * names indicate the operation and types of the arguments:
 *
 * TIME_op_type0{_type1}
 *
 * performs "op" on argument(s) of type type0 (and type1),
 * where op is one of:
 *
 * add -- addition
 * sub -- subtraction
 * mul -- multiplication
 * div -- division
 * mod -- modulus
 * neg -- unary negation
 * abs -- absolute value
 * lshift -- multiplication by a power of 2
 * rshift -- division by a power of 2
 *
 * and type0 (and type1) are:
 *
 * A -- absolute time
 * R -- relative time
 * I -- integer
 *
 * The result type is implicit, e.g. an absolute time plus a relative
 *time is an absolute time, an absolute time minus an absolute time is
 *a relative time, etc.
 */

#define TIME_ADD_A_R(time, offset) (abs_plus_rel_time((time), (offset)))
#define TIME_SUB_A_R(time, offset) (abs_minus_rel_time((time), (offset)))
#define TIME_SUB_A_A(t0, t1) (abs_minus_abs_time((t0), (t1)))
#define TIME_ADD_R_R(t0, t1) (rel_plus_rel_time((t0), (t1)))
#define TIME_SUB_R_R(t0, t1) (rel_minus_rel_time((t0), (t1)))
#define TIME_NEG_R(t) (rel_time_neg(t))
#define TIME_ABS_R(t) (TIME_CMP_R(lt, t, REL_TIME_0) ? TIME_NEG_R(t) : t)
#define TIME_MULT_R_I(t, factor) (rel_time_mult_int((t), (factor)))
#define TIME_DIV_R_I(t, divisor) (rel_time_div_int((t), (divisor)))
#define TIME_LSHIFT_R(t, shift) (rel_time_lshift((t), (shift)))
#define TIME_RSHIFT_R(t, shift) (rel_time_rshift((t), (shift)))

/*
 * Computes (factor0 * factor1) / divisor.
 * Avoiding overflow/underflow is the CALLER's responsibility:
 *   - (factor0 * factor1) should fit into a uint64_t
 *   - divisor should fit into a uint32_t
 */
#define TIME_MULT_R_R_DIV_R(factor0, factor1, divisor) \
    (rel_time_mult_rel_div_rel((factor0), (factor1), (divisor)))

#define IS_ABS_TIME_ZERO(t) (TIME_CMP_A(eq, (t), ABS_TIME_0) )

/*
 * Convert absolute and relative times to printable strings
 */
const char * abs_time_to_str(abs_time_t t, char * buff, uint32_t buff_len);
const char * rel_time_to_str(rel_time_t t, char * buff, uint32_t buff_len);

/*
 * Recommended size to use for a buffer to format date and time strings into in the above fcns.
 */
#define TIME_STRING_BUFF_SIZE 80 /*$$$ get the correct num here */

#endif /* __VAM_TIME_H__ */
