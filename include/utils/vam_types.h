/*------------------------------------------------------------------
 * VAM.  Basic types used throughout VAM application.
 *
 * April 2006, Josh Gahm
 *
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __VAM_TYPES_H__
#define __VAM_TYPES_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <pthread.h>
#include <netinet/in.h>
#include <limits.h>

#ifdef __VQEC__
#include <sys/un.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

typedef uint8_t boolean;

# ifndef SUCCESS
# define SUCCESS  0
# endif // SUCCESS

# ifndef FAILURE
# define FAILURE -1
# endif // FAILURE

#define TRUE 1
#define FALSE 0

#define VAM_PACKED __attribute__ ((__packed__))

#if __BYTE_ORDER == __BIG_ENDIAN
# define ntohll(x)	(x)
# define htonll(x)	(x)
#else
# if __BYTE_ORDER == __LITTLE_ENDIAN
#   define ntohll(x)	__bswap_64 (x)
#   define htonll(x)	__bswap_64 (x)
# endif
#endif

#ifndef GETSHORT
#define GETSHORT(p) (ntohs(*(p)))
#endif
#ifndef PUTSHORT
#define PUTSHORT(p,val) ((*p) = htons(val))
#endif

#ifndef GETLONG
#define GETLONG(p) (ntohl(*(p)))
#endif
#ifndef PUTLONG
#define PUTLONG(p,val) ((*p) = htonl(val))
#endif


#define ASSERT(exp,...) \
{ if (!(exp)) {fprintf(stderr, "FAILED ASSERTION\n");\
      fprintf(stderr,__VA_ARGS__); fprintf(stderr,"\n"); abort(); } }

#define MAX_BUFF  256
#define ASSERT_FATAL(_exp, _msgtype,...)         \
{ if (!(_exp)) {                                 \
      char _msgbuff[MAX_BUFF];                   \
      snprintf(_msgbuff, MAX_BUFF, __VA_ARGS__); \
      syslog_print(_msgtype, _msgbuff);          \
      abort(); }                                 \
}


/*
 * Class declaration stuff 
 *
 * DEFCLASS(FOO) expands to
 *
 *   struct FOO_fcns_;
 *   struct FOO_;
 *
 *   typedef struct FOO_fcns_ {
 *      __FOO_fcns
 *
 *   } FOO_fcns_t;
 *
 *   typedef struct FOO_ {
 *       FOO_fcns_t * __func_table;
 *       __FOO_members
 *   } FOO_t;
 *
 *   void FOO_set_fcns(FOO_fcns_t * table);
 *
 */
#define DEFCLASS(name) \
struct name ## _fcns_ ; \
struct name ## _ ; \
typedef struct name ## _fcns_ { \
    __ ## name ## _fcns \
} name ## _fcns_t;  \
typedef struct name ## _ { \
    name ## _fcns_t * __func_table; \
     __ ## name ## _members ;  \
} name ## _t; \
void name ## _set_fcns(name ## _fcns_t * table);

#define	min(a,b)	((a) < (b) ? (a) : (b))
#define	max(a,b)	((a) > (b) ? (a) : (b))

/*
 * Method call macro
 * 
 */

#define MCALL(obj,method,args...) (obj)->__func_table->method((obj) , ## args)

/* The following is assumed to have the same layout as a struct iovec.  See man readv. */
typedef struct vam_scatter_desc_
{
    void * addr;
    size_t len;
} vam_scatter_desc_t;

/*
 * Memory allocation.
 */
#define VQE_MALLOC_ATOMIC(x) malloc((x))
#define VQE_MALLOC(x) malloc((x))
#define VQE_CALLOC(x, y) calloc((x), (y))
#define VQE_FREE(x) free((x))

/*
 * Time-of-day system call.
 */
#define VQE_GET_TIMEOFDAY(x, y)                 \
    ({                                          \
        int32_t __result;                       \
        __result = gettimeofday((x), (y));      \
        if (-1 == __result) {                   \
            perror("gettimeofday");             \
        }                                       \
        __result;                               \
    })

#define VQE_FPRINTF(dev, format,...)            \
    fprintf(dev, format, ##__VA_ARGS__)
#define VQE_VFPRINTF(dev, format, ap)           \
    vfprintf(dev, format, ap)

/*
 * Mutual exclusion.
 */
typedef 
struct vqe_mutex_
{
    pthread_mutex_t l;
} vqe_mutex_t;
#define VQE_MUTEX_DEFINE(_name)                         \
    vqe_mutex_t _name = {PTHREAD_MUTEX_INITIALIZER}
#define VQE_MUTEX_LOCK(x)                       \
    pthread_mutex_lock(&((x)->l))
#define VQE_MUTEX_UNLOCK(x)                     \
    pthread_mutex_unlock(&((x)->l))

#define VQE_RAND_R(x) rand_r(x)
#define VQE_GETENV_SYSCALL(str) getenv(str)
#define VQE_SET_ERRNO(x) (errno = x)
#define VQE_DIV64(x, y) ((x) / (y))

#endif /* __CM_TYPES_H__ */
