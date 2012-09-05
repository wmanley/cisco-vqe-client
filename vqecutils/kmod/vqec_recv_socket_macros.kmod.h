/*------------------------------------------------------------------
 * VQE-C socket macros for kernel APIs.
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __VQEC_RECV_SOCKET_MACROS_H__
#define __VQEC_RECV_SOCKET_MACROS_H__

/**
 * The following kernel APIs have been verified in the kernel source back to
 * version 2.6.11 and up to 2.6.31, and they should work fine at least within
 * that range.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#define kernel_setsockopt(sock, level, optname, optval, optlen) \
    ({                                                          \
        mm_segment_t old_fs = get_fs();                         \
        int err;                                                \
        set_fs(KERNEL_DS);                                      \
        if ((level) == SOL_SOCKET) {                            \
            err = sock_setsockopt((sock), (level), (optname),   \
                                  (optval), (optlen));          \
        } else {                                                \
            err = (sock)->ops->setsockopt((sock), (level),      \
                                          (optname), (optval),  \
                                          (optlen));            \
        }                                                       \
        set_fs(old_fs);                                         \
        err;                                                    \
    })
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#define kernel_getsockname(sock, addr, addrlen)                 \
    (sock)->ops->getname((sock), (addr), (addrlen), 0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#define kernel_bind(sock, addr, addrlen)                        \
    (sock)->ops->bind((sock), (addr), (addrlen))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#define kernel_connect(sock, addr, addrlen, flags)              \
    (sock)->ops->connect((sock), (addr), (addrlen), (flags))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
#define kernel_sock_shutdown(sock, how) (sock)->ops->shutdown((sock), (how))
enum sock_shutdown_cmd {
    SHUT_RD = 0,
    SHUT_WR = 1,
    SHUT_RDWR = 2,
};
#endif

#endif  /* __VQEC_RECV_SOCKET_MACROS_H__ */
