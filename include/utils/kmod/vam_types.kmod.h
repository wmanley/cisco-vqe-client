/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Kernel module basic types.
 *
 * Documents: 
 *
 *****************************************************************************/

#ifndef __VAM_TYPES_H__
#define __VAM_TYPES_H__

#include <linux/module.h>
#include <asm/byteorder.h>
#include <linux/fcntl.h>
#include <linux/random.h>
#include <linux/in.h>
#include <linux/ctype.h>
#include <linux/un.h>

typedef uint8_t boolean;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

#ifndef USHRT_MAX
#define USHRT_MAX 0xFFFF
#endif

#ifndef RAND_MAX
#define RAND_MAX 0xFFFFFFFF
#endif

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 32
#endif

# ifndef SUCCESS
# define SUCCESS  0
# endif // SUCCESS

# ifndef FAILURE
# define FAILURE -1
# endif // FAILURE

#define TRUE 1
#define FALSE 0

#define VAM_PACKED __attribute__ ((__packed__))

#   define ntohll(x)	be64_to_cpu(x)
#   define htonll(x)	cpu_to_be64(x)

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

#ifdef __BIG_ENDIAN
#define __BYTE_ORDER __BIG_ENDIAN
#else
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

/* 
 * If CONFIG_BUG [lib/bug.c] is not defined, then BUG() is empty
 * and thus ASSERT() will be effectively turned off.
 */
#define ASSERT(exp,...)                                                    \
    {                                                                   \
        if (!(exp)) {                                                   \
            printk("FAILED ASSERTION in %s:%u\n", __FUNCTION__, __LINE__); \
            BUG();                                                      \
        }                                                               \
    }                                                                   \
        

#define ASSERT_MAX_BUFF 128
extern char g_vqec_assertbuff[ASSERT_MAX_BUFF];
#define ASSERT_FATAL(exp, type,...)                                     \
    ({                                                                  \
        if (!(exp)) {                                                   \
            snprintf(g_vqec_assertbuff, ASSERT_MAX_BUFF, __VA_ARGS__);                  \
            printk("FAILED ASSERTION %s(%d)\n", __FUNCTION__, __LINE__); \
            syslog_print(type, g_vqec_assertbuff);                               \
            BUG();                                                      \
        }                                                               \
    })


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
#define VQE_MALLOC_ATOMIC(x) kmalloc((x), GFP_ATOMIC)
#define VQE_MALLOC(x) kmalloc((x), GFP_KERNEL)
#define VQE_FREE(x) kfree((x))

/*
 * Time-of-day system call.
 */
/*
 * Time-of-day system call.
 */
#define VQE_GET_TIMEOFDAY(x, y)                 \
    ({                                          \
        int32_t __result = 0;                   \
        do_gettimeofday((x));                   \
        __result;                               \
    })

#define VQE_FPRINTF(dev, format,...)            \
    printk(format, ##__VA_ARGS__)
#define VQE_VFPRINTF(dev, format, ap)           \
    vprintk(format, ap)

/*
 * Mutual exclusion.
 */
typedef 
struct vqe_mutex_
{
    spinlock_t l;
    unsigned long flags;
} vqe_mutex_t;
#define VQE_MUTEX_DEFINE(_name)                 \
    vqe_mutex_t _name = {SPIN_LOCK_UNLOCKED}
#define VQE_MUTEX_LOCK(x)                       \
    spin_lock_irqsave(&((x)->l), (x)->flags)
#define VQE_MUTEX_UNLOCK(x)                             \
    spin_unlock_irqrestore(&((x)->l), (x)->flags)

#define VQE_RAND_R(x)                           \
    ({                                          \
        int32_t _randb;                                  \
        *(x) = *(x);                                     \
        get_random_bytes(&_randb, sizeof(_randb));       \
        _randb;                                          \
    })

#define VQE_GETENV_SYSCALL(str) NULL
#define VQE_SQRT(x) int_sqrt((int32_t)(x))
#define VQE_SET_ERRNO(x)
#define perror(str) 
#define VQE_DIV64(x, y)                                         \
    ({                                                          \
        uint64_t __d = abs((x));                                \
        uint32_t __v = abs((y));                                \
        typeof((x)) __x;                                        \
        uint32_t __rem;                                         \
        __rem = do_div(__d, __v);                               \
        __x = __d;                                              \
        if ((((x) > 0) && ((y) < 0)) ||                         \
            (((x) < 0) && ((y) > 0))) {                         \
            __x = -1 * __x;                                     \
        }                                                       \
        __x;                                                    \
    })

/**
 * Implementation of bsearch for vam_hist.c - in stdlib.h!
 */
typedef int (*compar_fn_t) (const void *, const void *);
void *bsearch(void *__key, void *__base,
              size_t nmemb, size_t size, compar_fn_t compar);

/**
 * Stub sendto in inputshim for now - sendto() should be pushed via a
 * socket abstraction into vqec_recv_socket.c
 */
static inline ssize_t
sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *to, int socklen)
{
    return (0);
}

#endif /* __VAM_TYPES_H__ */
