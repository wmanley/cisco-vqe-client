/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2009-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Receive socket.
 *
 * Documents: 
 *
 *****************************************************************************/

#include "vqec_recv_socket.h"
#include "vqec_recv_socket_macros.h"
#include "vqec_pak.h"
#include "vqec_debug.h"
#include "linux/net.h"
#include "linux/skbuff.h"
#include "linux/ip.h"
#include "linux/udp.h"
#include <linux/version.h>
#include "net/sock.h"
#include <utils/vam_util.h>
#include <utils/zone_mgr.h>

#define RECV_SOCKET_ERR_STRLEN 256

static char addrbuf1[INET_ADDRSTRLEN];
static char addrbuf2[INET_ADDRSTRLEN];

static boolean vqec_sock_ref_open (struct sockaddr_in *saddr, 
                                   struct socket **ksock, 
                                   vqec_recv_sock_pool_t *pool);
static void vqec_sock_ref_close (struct sockaddr_in *saddr);

void 
vqec_recv_sock_perror (const char *errmsg, int32_t errnum)
{
    static char logbuf[RECV_SOCKET_ERR_STRLEN];
    snprintf(logbuf, RECV_SOCKET_ERR_STRLEN, "%s: errnum = %d", errmsg, errnum);
    syslog_print(VQEC_ERROR, logbuf);
}

typedef enum {
    VQEC_MCAST_JOIN,
    VQEC_MCAST_LEAVE
} vqec_mcast_op;

/**
 * Generates a string that describes the IP address specified by "ip".
 * The contents of the string are stored in the caller's choice of 
 * two internal buffers (numbered 0 and 1) which are overwritten with
 * subsequent calls that use the same buffer.  The pointer returned
 * references the generated string.
 *
 * @param[in]  ip       IP address to be described in the returned string
 * @param[in]  buf_num  Indicates which internal buffer to use (0 or 1)
 * @param[out] const char *  Pointer to the generated string
 */
const char *
inet_ntop_static_buf (in_addr_t ip, int buf_num)
{
    static char buf0[INET_ADDRSTRLEN];
    static char buf1[INET_ADDRSTRLEN];

    return (uint32_ntoa_r(ip, buf_num ? buf1 : buf0, INET_ADDRSTRLEN));
}

/*
 * Adds/removes membership in a given multicast group FOR A GIVEN SOURCE
 * ADDRESS.
 */
static boolean
vqec_recv_sock_mcast_op_membership_src (vqec_mcast_op  mcast_op,
                                        struct socket *ksock,
                                        struct in_addr mcast_group,
                                        struct in_addr rcv_if_address,
                                        struct in_addr filter_src_addr)
{
    boolean ret;
#if defined (IP_ADD_SOURCE_MEMBERSHIP) && defined (IP_DROP_SOURCE_MEMBERSHIP)
    struct ip_mreq_source mreq_src;
    int option_name;
    int sock_ret;

    ret = FALSE;
    option_name = 0;

    if (mcast_op == VQEC_MCAST_JOIN) {
        option_name = IP_ADD_SOURCE_MEMBERSHIP;
    } else {
        option_name = IP_DROP_SOURCE_MEMBERSHIP;
    }
    memset(&mreq_src, 0, sizeof(mreq_src));
    mreq_src.imr_interface = rcv_if_address.s_addr;
    mreq_src.imr_multiaddr = mcast_group.s_addr;
    mreq_src.imr_sourceaddr = filter_src_addr.s_addr;
    sock_ret = kernel_setsockopt(ksock, IPPROTO_IP, option_name,
                                 (char *)&mreq_src, sizeof(mreq_src));
    if (sock_ret < 0) {
        vqec_recv_sock_perror("setsockopt failed:", sock_ret);
        goto done;
    }
    ret = TRUE;
done:
#endif  /* IP_ADD/DROP_SOURCE_MEMBERSHIP */
    return (ret);
}
                                        
/*
 * Adds/removes membership in a given multicast group INDEPENDENT OF SOURCE
 * ADDRESS.
 */
static boolean
vqec_recv_sock_mcast_op_membership (vqec_mcast_op mcast_op,
                                    struct socket *ksock,
                                    struct in_addr mcast_group,
                                    struct in_addr rcv_if_address)
{
    boolean ret = FALSE;
    struct ip_mreqn mreqn;
    int option_name=0;
    int sock_ret;
    
    if (mcast_op == VQEC_MCAST_JOIN) {
        option_name = IP_ADD_MEMBERSHIP;
    } else {
        option_name = IP_DROP_MEMBERSHIP;
    }

    memset(&mreqn, 0, sizeof(mreqn));
    mreqn.imr_address.s_addr = rcv_if_address.s_addr;
    mreqn.imr_multiaddr.s_addr = mcast_group.s_addr;
    
    sock_ret = kernel_setsockopt(ksock, IPPROTO_IP, option_name,
                                 (char *)&mreqn, sizeof(mreqn));
    if (sock_ret < 0) {
        vqec_recv_sock_perror("setsockopt failed:", sock_ret);
        goto done;
    }
    ret = TRUE;
done:
    return (ret);
}

/*
 * filter_src_addr - callers may specify a source address if desired, or
 *                   INADDR_ANY if no filtering is desired on source address.
 *                   Actual source filtering will only occur if the kernel
 *                   supports it via IP_ADD_SOURCE_MEMBERSHIP socket option,
 *                   and is not reflected in the return code.
 */
static boolean
vqec_recv_sock_mcast_op (vqec_recv_sock_t *sock, 
                         struct in_addr filter_src_addr,
                         vqec_mcast_op mcast_op)
{
#define VQEC_RECV_SOCK_MCAST_OP_STRLEN INET_ADDRSTRLEN + 20
    struct in_addr mcast_group;
    struct in_addr rcv_if_address; 
    abs_time_t now;
    static char temp_filter_str[VQEC_RECV_SOCK_MCAST_OP_STRLEN];
    boolean ret = TRUE;
    boolean source_filtering = FALSE;
    char *join_or_leave_str=NULL;
    char *gettimeofday_err_str=NULL;

    memset(&mcast_group, 0, sizeof(struct in_addr));

#if defined (IP_ADD_SOURCE_MEMBERSHIP) && defined (IP_DROP_SOURCE_MEMBERSHIP)
    if (filter_src_addr.s_addr != INADDR_ANY) {
        source_filtering = TRUE;
    }
#endif /* IP_ADD/DROP_SOURCE_MEMBERSHIP */

    switch (mcast_op) {
    case VQEC_MCAST_JOIN:
        join_or_leave_str = "join";
        gettimeofday_err_str = "getjointime";
        break;
    case VQEC_MCAST_LEAVE:
        join_or_leave_str = "leave";
        gettimeofday_err_str = "getleavetime";
        break;
    default:
        ASSERT(0);
        return FALSE;
        break;
    }
    if (sock) {
        mcast_group = vqec_recv_sock_get_mcast_group(sock);
        rcv_if_address = vqec_recv_sock_get_rcv_if_address(sock);
        if (IN_MULTICAST(ntohl(mcast_group.s_addr))) {
            if (source_filtering) {
                ret = vqec_recv_sock_mcast_op_membership_src(mcast_op,
                                                             sock->ksock,
                                                             mcast_group,
                                                             rcv_if_address,
                                                             filter_src_addr);
            } else {
                ret = vqec_recv_sock_mcast_op_membership(mcast_op,
                                                         sock->ksock,
                                                         mcast_group,
                                                         rcv_if_address);
            }
        } else {
            ret = FALSE;
        }
    } else {
        ret = FALSE;
    }
    if (VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC)) {
        now = get_sys_time();
        if (!uint32_ntoa_r(mcast_group.s_addr,
                       addrbuf1, INET_ADDRSTRLEN)) {
            addrbuf1[0] = '\0';
        }
        if (source_filtering) {
            if (!uint32_ntoa_r(filter_src_addr.s_addr,
                           addrbuf2, INET_ADDRSTRLEN)) {
                (void)snprintf(temp_filter_str,
                               VQEC_RECV_SOCK_MCAST_OP_STRLEN,
                               " filter addr [?]");
            } else {
                (void)snprintf(temp_filter_str,
                               VQEC_RECV_SOCK_MCAST_OP_STRLEN,
                               " filter addr %s",
                               addrbuf2);
            }
        } else {
            temp_filter_str[0] = '\0';
        }
        VQEC_DEBUG(VQEC_DEBUG_RCC,
                   "%s %s mcast %s%s @ %lu (usec)\n",
                   ret ? "" : "FAILED ",
                   join_or_leave_str,
                   addrbuf1,
                   temp_filter_str,
                   TIME_GET_A(usec, now));
    }

    return ret;
}

boolean vqec_recv_sock_mcast_join (vqec_recv_sock_t *sock, 
                                   struct in_addr filter_src_addr)
{
    return vqec_recv_sock_mcast_op(sock, 
                                   filter_src_addr,
                                   VQEC_MCAST_JOIN);
}

boolean vqec_recv_sock_mcast_leave (vqec_recv_sock_t *sock,
                                    struct in_addr filter_src_addr)
{
    return vqec_recv_sock_mcast_op(sock, 
                                   filter_src_addr,
                                   VQEC_MCAST_LEAVE);
}

vqec_recv_sock_t *
vqec_recv_sock_create(char * name,
                     in_addr_t rcv_if_address,
                     uint16_t         port,
                     in_addr_t mcast_group,
                     boolean blocking,
                     uint32_t rcv_buff_bytes,
                     uint8_t xmit_dscp_value)
{
    return vqec_recv_sock_create_in_pool(
                                 NULL,
                                 name,
                                 rcv_if_address,
                                 port,
                                 mcast_group,
                                 blocking,
                                 rcv_buff_bytes,
                                 xmit_dscp_value);
}

vqec_recv_sock_t * 
vqec_recv_sock_create_in_pool(
                     vqec_recv_sock_pool_t * pool,
                     char * name,
                     in_addr_t rcv_if_address, 
                     uint16_t         port,
                     in_addr_t mcast_group,
                     boolean blocking,
                     uint32_t rcv_buff_bytes,
                     uint8_t xmit_dscp_value)
{
    vqec_recv_sock_t *result;
    struct sockaddr_in saddr;
    int on;
    int dscp;
    int err;

    on = 1;
    dscp = (xmit_dscp_value << DSCP_SHIFT);

    /* Debug print out */
    if (!uint32_ntoa_r(rcv_if_address, addrbuf1, INET_ADDRSTRLEN)) {
        addrbuf1[0] = '\0';
    }
    if (!uint32_ntoa_r(mcast_group, addrbuf2, INET_ADDRSTRLEN)) {
        addrbuf2[0] = '\0';
    }
    VQEC_DEBUG(VQEC_DEBUG_RCC,
               "vqec_recv_sock_create: if_addr=%s port=%d "
               "mcast=%s block=%d rcv_buff=%u\n",
               addrbuf1,
               ntohs(port),
               addrbuf2,
               blocking,
               rcv_buff_bytes);

    /* Allocate vqec_recv_sock_t */
    if (pool) {
        result = (vqec_recv_sock_t *) zone_acquire(pool->sock_pool);
    } else {
        result = kmalloc(sizeof(vqec_recv_sock_t), GFP_KERNEL);
    }
    if (! result) {
        return NULL;
    }
    memset(result, 0, sizeof(vqec_recv_sock_t));
    
    /* remember arguments */
    result->rcv_if_address.s_addr = rcv_if_address;
    result->port = port;
    result->blocking = blocking;
    result->rcv_buff_bytes = rcv_buff_bytes;
    result->mcast_group.s_addr = mcast_group;

    /* Initialize socket address */
    memset(&saddr, 0, sizeof(saddr));
    if (IN_MULTICAST(ntohl(mcast_group))) {
        saddr.sin_addr.s_addr = mcast_group;
    } else {
        saddr.sin_addr.s_addr = rcv_if_address;
    }
    saddr.sin_family = AF_INET;
    saddr.sin_port = port;

    /* Open (alloc and bind) receive socket */
    if (!vqec_sock_ref_open(&saddr, &result->ksock, pool)) {
        if (pool) {
            zone_release(pool->sock_pool, result);
        } else {
            kfree(result);
        }
        return NULL;
    }

    /* Store the socket's local port, if dynamically assigned */
    if (!port) {
        result->port = saddr.sin_port;
    }

    err = kernel_setsockopt(result->ksock, SOL_SOCKET, 
                            SO_TIMESTAMP, (char *)&on, sizeof(on));
    if (err < 0) {
        vqec_recv_sock_perror("setsockopt timestamp", err);
        goto bail;
    }

    err = kernel_setsockopt(result->ksock, IPPROTO_IP,
                            IP_TOS, (char *)&dscp, sizeof(dscp));
    if (err < 0) {
        vqec_recv_sock_perror("setsockopt tos", err);
        syslog_print(VQEC_ERROR, "Might need root privileges to transmit"
                     " with elevated DSCP values, continuing with "
                     "DSCP value of 0 instead...");
    }

    if (rcv_buff_bytes) {
        err = kernel_setsockopt(result->ksock, SOL_SOCKET,
                                SO_RCVBUF, (char *)&rcv_buff_bytes,
                                sizeof(rcv_buff_bytes));
        if (err < 0) {
            vqec_recv_sock_perror("setsockopt rcvbuf", err);
            goto bail;
        }
    }

    result->ref_count = 1;

    return (result);

bail:
    vqec_sock_ref_close(&saddr);
    if (pool) {
        zone_release(pool->sock_pool, result);
    } else {
        kfree(result);
    }
    return (NULL);
}

/**
 * Return the per-packet overhead due to storing a buffer associated with a
 * single packet.
 *
 * @param[out]  returns the per-packet overhead in bytes.
 */
uint32_t vqec_recv_sock_get_pak_overhead (void)
{
    return (sizeof(struct sk_buff));
}

/**
 * Directly set the maximum memory allocation for the socket.  This will
 * circumvent the min/max limitations imposed by using setsockopt() to do this.
 *
 * This API is only applicable when used in kernel-space.
 *
 * @param[in]  sock  Pointer to the socket structure to modify.
 * @param[in]  so_rcvbuf  Size to set the SO_RCVBUF to.
 * @param[in]  replace  If TRUE, the given value will replace the old;
 *                      if FALSE, the given value is added to the old.
 * @param[out]  returns TRUE on success.
 */
boolean
vqec_recv_sock_set_so_rcvbuf (vqec_recv_sock_t *sock,
                              boolean replace,
                              int so_rcvbuf)
{
    int sk_rcvbuf_size;

    if (sock &&
        sock->ksock &&
        sock->ksock->sk) {

        if (replace) {
            sk_rcvbuf_size = so_rcvbuf;
        } else {
            sk_rcvbuf_size = so_rcvbuf + (sock->ksock->sk->sk_rcvbuf);
        }

        sock->ksock->sk->sk_rcvbuf = sk_rcvbuf_size;

        return (TRUE);
    }

    return (FALSE);
}

void vqec_recv_sock_destroy (vqec_recv_sock_t * sock)
{
    vqec_recv_sock_destroy_in_pool(NULL, sock);
}

void vqec_recv_sock_destroy_in_pool (vqec_recv_sock_pool_t * pool,
                                     vqec_recv_sock_t * sock)
{
    struct sockaddr_in saddr;
    
    ASSERT(sock->ref_count > 0);
    sock->ref_count--;

    if (sock->ref_count > 0) {
        return;
    }

    memset(&saddr, 0, sizeof(saddr));
    if (IN_MULTICAST(ntohl(sock->mcast_group.s_addr))) {
        saddr.sin_addr.s_addr = sock->mcast_group.s_addr;
    } else {
        saddr.sin_addr.s_addr = sock->rcv_if_address.s_addr;
    }
    saddr.sin_family = AF_INET;
    saddr.sin_port = sock->port;

    vqec_sock_ref_close(&saddr);

    if (pool) {
        zone_release(pool->sock_pool, sock);
    } else {
        kfree(sock);
    }
}

boolean
vqec_recv_sock_connect (vqec_recv_sock_t  *sock,
                        boolean           use_remote_addr,
                        in_addr_t         remote_addr,
                        boolean           use_remote_port,
                        uint16_t          remote_port)
{
    int err, addr_size;
    struct sockaddr *addr, addr_unspec;
    struct sockaddr_in saddr;
    boolean status;

    status = TRUE;
    if (IN_MULTICAST(ntohl(sock->mcast_group.s_addr))) {
        status = FALSE;
        goto done;
    }

    /* Load the connection address */
    if (!use_remote_addr && !use_remote_port) {
        /* Socket should be unconnected */
        memset(&addr_unspec, 0, sizeof(addr_unspec));
        addr_unspec.sa_family = AF_UNSPEC;
        addr = &addr_unspec;
        addr_size = sizeof(addr_unspec);        
    } else {
        /* Socket should be connected */
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = (use_remote_addr ? remote_addr : htonl(0));
        saddr.sin_port = (use_remote_port ? remote_port : htons(0));
        addr = (struct sockaddr *)&saddr;
        addr_size = sizeof(saddr);
    }

    err = kernel_connect(sock->ksock, addr, addr_size, 0);
    if (err < 0) {
        vqec_recv_sock_perror("connect", err);
        status = FALSE;
        goto done;
    }
    sock->remote.use_ip_address = use_remote_addr;
    sock->remote.use_port = use_remote_port;
    sock->remote.port = remote_port;
    sock->remote.ip_address.s_addr = remote_addr;

done:
    return (status);
}

int vqec_recv_sock_read_pak (vqec_recv_sock_t *sock,
                             vqec_pak_t *pak)
{
#define MAX_CMSG_BUF_SIZE 1024 

    int err;
    static boolean printed_err = FALSE;
    struct sockaddr_in saddr;
    struct sk_buff *skb;
    struct iphdr *ip;
    struct udphdr *udp;
    struct timeval tv;

    memset(&saddr, 0, sizeof(struct sockaddr_in));

    /* get the skbuff from the socket */
    skb = skb_recv_datagram(sock->ksock->sk, 0, (sock->blocking ? 0 : 1), &err);
    if (!skb) {
        pak->buff_len = 0;
        if (err < 0 &&
            err != -EAGAIN && err != -EWOULDBLOCK) {
            vqec_recv_sock_perror("recvmsg", err);
        }
        return (0);
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
    ip = skb->nh.iph;
    udp = skb->h.uh;
#else
    ip = (struct iphdr *)skb->network_header;
    udp = (struct udphdr *)skb->transport_header;
#endif  /* LINUX_VERSION_CODE */

    /* deal with the *buff pointer and data */
#ifndef PAK_BUF_CONTIG 
    if (skb->data_len) {
        /* 
         * Since skb->data_len is nonzero, we know that the packet data is
         * fragmented across one or more pages.
         */
        /*
         * skb_linearize() will create a linearized version of the received skb.
         *
         * TODO:  This currently copies the entire buffer which is inefficient.
         *        This should only need the RTP header to be copied, but some
         *        code (FEC, etc.) will need to be rewritten to access the data
         *        buffer through fragmentation-aware APIs.
         */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
        err = skb_linearize(skb, GFP_ATOMIC);
#else
        err = skb_linearize(skb);
#endif  /* LINUX_VERSION_CODE */
        if ((err < 0) && (!printed_err)) {
            if (err == -ENOMEM) {
                syslog_print(VQEC_ERROR,
                             "error (NOMEM) linearizing fragmented skb!\n");
            } else {
                syslog_print(VQEC_ERROR,
                             "error (unknown) linearizing fragmented skb!\n");
            }
            printed_err = TRUE;
            return (0);
        }
    }
#endif  /* PAK_BUF_CONTIG */

    /* now we can just use the skb's linear buffer directly */
    pak->buff = skb->data + sizeof(struct udphdr);
    pak->rtp = (rtpfasttype_t *)pak->buff;

    /* cache pointers to the associated skb and socket */
    pak->skb = (void *)skb;
    pak->sk  = (void *)sock->ksock->sk;

    /* Perform any needed updates to the pak structure */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
    tv = skb->stamp;
#else
    skb_get_timestamp(skb, &tv);
#endif  /* LINUX_VERSION_CODE */
    if (tv.tv_sec == 0) {
        (void)VQE_GET_TIMEOFDAY(&tv, 0);
    }

    /* set original rx ts in pak */
    pak->rcv_ts = timeval_to_abs_time(tv);

    /* set source address and port for the pak */
    pak->src_addr.s_addr = ip->saddr;
    pak->src_port = udp->source;

    /* set the head offset and received length of the pak buffer */
    pak->head_offset = 0;
    pak->buff_len = skb->len - sizeof(struct udphdr);

    return (pak->buff_len);
}

/**
 vqec_recv_sock_read
 Read data from a socket.
 @param[in] sock pointer of socket.
 @param[out] rcv_time Kernel socket timestamp of packet received time.
 @param[in] buf Buffer to hold read content.
 @param[in] buf_len Length of buffer size.
 @param[out] src_addr src address of incoming packet.
 @param[out] src_port src port of incoming packet.
 @return Number of bytes received from socket.  If fail, return < 1.
*/
int vqec_recv_sock_read (vqec_recv_sock_t *sock,
                         struct timeval *rcv_time,
                         void *buf,
                         uint32_t buf_len,
                         struct in_addr *src_addr,
                         uint16_t *src_port)
{
    static vqec_pak_t pak;
    int read_len;

    if (!sock || !rcv_time || !buf || !buf_len || !src_addr || !src_port) {
        return (0);
    }

    /* set up static pak */
    pak.buff = buf;
    *((uint32_t *)&pak.alloc_len) = buf_len;

    read_len = vqec_recv_sock_read_pak(sock, &pak);

    if (read_len) {
        *rcv_time = abs_time_to_timeval(pak.rcv_ts);
        *src_addr = pak.src_addr;
        *src_port = pak.src_port;
    }

    return (read_len);
}

/**
 * Send a buffer to a particular address through a given socket.
 */
size_t vqec_recv_sock_sendto (vqec_recv_sock_t *sock,
                              void *buff,
                              size_t len,
                              unsigned flags,
                              struct sockaddr *addr,
                              int addr_len)
{
        int err;
        struct msghdr msg;
        struct iovec iov;
        mm_segment_t old_fs;

        if (!sock || !buff || !addr) {
            return (-1);
        }

        iov.iov_base = buff;
        iov.iov_len = len;
        msg.msg_name = addr;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        if (addr) {
            msg.msg_namelen = addr_len;
        } else {
            msg.msg_namelen = 0;
        }
        if (!sock->blocking) {
            flags |= MSG_DONTWAIT;
        }
        msg.msg_flags = flags;

        old_fs = get_fs();
        set_fs(KERNEL_DS);
        err = sock_sendmsg(sock->ksock, &msg, len);
        set_fs(old_fs);
        if (err < 0) {
            vqec_recv_sock_perror("sock_sendmsg", err);
            return (-1);
        }

        return (len);
}

void vqec_recv_sock_ref (vqec_recv_sock_t * sock)
{
    ASSERT(sock);
    sock->ref_count++;
}

/* TODO:  this needs to be fixed; it is used for upcalls */
int32_t 
vqec_create_named_socket (struct sockaddr_un *name)
{
    return (-1);
}


/*----------------------------------------------------------------------------
 * Reserved Socket API
 *--------------------------------------------------------------------------*/
typedef struct vqec_sock_ref_ {
    struct sockaddr_in saddr;
    struct socket *ksock;
    uint32_t refcnt;
    vqec_recv_sock_pool_t *pool;
    VQE_TAILQ_ENTRY(vqec_sock_ref_) le;
} vqec_sock_ref_t;
typedef VQE_TAILQ_HEAD(vqec_sock_list_, vqec_sock_ref_) vqec_sock_list_t;

static vqec_sock_list_t sock_list;
static boolean sock_list_initialized = FALSE;

/**
 * Find an internal socket reference.
 *
 * @param[in]  saddr  socket address to or existing ref
 * @param[out] vqec_sock_ref_t*  socket ref matching saddr or NULL
 */
static vqec_sock_ref_t* 
vqec_sock_ref_find (struct sockaddr_in *saddr)
{
    vqec_sock_ref_t *sock_ref, *sock_ref_nxt;

    VQE_TAILQ_FOREACH_SAFE(sock_ref, &sock_list, le, sock_ref_nxt) {
        if (sock_ref->saddr.sin_port == saddr->sin_port &&
            sock_ref->saddr.sin_addr.s_addr == saddr->sin_addr.s_addr) {
            return sock_ref;
        }
    }
    return NULL;
}

/**
 * Open an internal socket reference.
 *
 * @param[in]  saddr   socket address to bind
 * @param[in]  ksock   kernel socket pointer
 * @param[in]  pool    pool from which to allocate socket ref
 * @param[out] boolean TRUE if successful, FALSE otherwise
 */
static boolean 
vqec_sock_ref_open (struct sockaddr_in *saddr,
                    struct socket **ksock,
                    vqec_recv_sock_pool_t *pool)
{
    vqec_sock_ref_t *sock_ref;
    int saddr_length;
    int on;
    int err;

    saddr_length = sizeof(struct sockaddr_in);
    on = 1;

    if (!saddr || !ksock) {
        return FALSE;
    }

    if (!sock_list_initialized) {
        VQE_TAILQ_INIT(&sock_list);
        sock_list_initialized = TRUE;
    }

    *ksock = NULL;
    sock_ref = vqec_sock_ref_find(saddr);
    if (sock_ref) {
        *saddr = sock_ref->saddr;
        *ksock = sock_ref->ksock;
        sock_ref->refcnt++;
    } else {
        if (pool) {
            sock_ref = (vqec_sock_ref_t *)zone_acquire(pool->sock_ref_pool);
        } else {
            sock_ref = (vqec_sock_ref_t *)kmalloc(sizeof(vqec_sock_ref_t),
                                                  GFP_KERNEL);
        }
        if (!sock_ref) {
            goto bail;
        }
        err = sock_create(PF_INET, SOCK_DGRAM, 0, ksock);
        if (err < 0) {
            vqec_recv_sock_perror("socket", err);
            goto bail;
        }
        if (IN_MULTICAST(ntohl(saddr->sin_addr.s_addr))) {
            err = kernel_setsockopt(*ksock, SOL_SOCKET, SO_REUSEADDR, 
                                    (char *)&on, sizeof(on));
            if (err < 0) {
                vqec_recv_sock_perror("setsockopt reuseaddr", err);
                goto bail;
            }
        }
        err = kernel_bind(*ksock, (struct sockaddr *) saddr,
                          sizeof(*saddr));
        if (err < 0) {
            vqec_recv_sock_perror("bind", err);
            goto bail;
        }
        err = kernel_getsockname(*ksock, (struct sockaddr *)saddr,
                                 &saddr_length);
        if (err < 0) {
            vqec_recv_sock_perror("getsockname", err);
            goto bail;
        }
        sock_ref->saddr = *saddr;
        sock_ref->ksock = *ksock;
        sock_ref->refcnt = 1;
        sock_ref->pool = pool;
        VQE_TAILQ_INSERT_TAIL(&sock_list, sock_ref, le);
    }

    return TRUE;

  bail:
    if (sock_ref) {
        if (pool) {
            zone_release(pool->sock_ref_pool, sock_ref);
        } else {
            kfree(sock_ref);
        }
    }
    if (*ksock) {
        kernel_sock_shutdown(*ksock, SHUT_RDWR);
        sock_release(*ksock);
    }
    return FALSE;
}

/**
 * Close an internal socket reference.
 *
 * @param[in]  saddr   socket address of existing ref
 */
static void 
vqec_sock_ref_close (struct sockaddr_in *saddr)
{
    vqec_sock_ref_t *sock_ref;

    sock_ref = vqec_sock_ref_find(saddr);
    if (!sock_ref) {
        return;
    }

    sock_ref->refcnt--;
    if (sock_ref->refcnt > 0) {
        return;
    }

    VQE_TAILQ_REMOVE(&sock_list, sock_ref, le);

    kernel_sock_shutdown(sock_ref->ksock, SHUT_RDWR);
    sock_release(sock_ref->ksock);

    if (sock_ref->pool) {
        zone_release(sock_ref->pool->sock_ref_pool, sock_ref);
    } else {
        kfree(sock_ref);
    }
}

boolean
vqec_recv_sock_open (in_addr_t *addr,
                     in_port_t *port,
                     vqec_recv_sock_pool_t *pool)
{
    struct sockaddr_in saddr;
    struct socket *ksock;
    boolean rv;

    saddr.sin_addr.s_addr = *addr;
    saddr.sin_port = *port;
    saddr.sin_family = AF_INET;

    rv = vqec_sock_ref_open(&saddr, &ksock, pool);

    *addr = saddr.sin_addr.s_addr;
    *port = saddr.sin_port;

    return rv;
}

void
vqec_recv_sock_close (in_addr_t addr,
                      in_port_t port)
{
    struct sockaddr_in saddr;

    saddr.sin_addr.s_addr = addr;
    saddr.sin_port = port;
    saddr.sin_family = AF_INET;

    vqec_sock_ref_close(&saddr);
}

/*----------------------------------------------------------------------------
 * Receive Socket Pool API
 *--------------------------------------------------------------------------*/
vqec_recv_sock_pool_t *
vqec_recv_sock_pool_create(char *pool_name, uint32_t max_sockets)
{
    vqec_recv_sock_pool_t *recv_sock_pool;
    char buf[ZONE_MAX_NAME_LEN];

    if (max_sockets <= 0) {
        return NULL;
    }

    recv_sock_pool = kmalloc(sizeof(vqec_recv_sock_pool_t), GFP_KERNEL);
    if (!recv_sock_pool) {
        return NULL;
    }
    memset(recv_sock_pool, 0, sizeof(vqec_recv_sock_pool_t));

    recv_sock_pool->sock_pool = zone_instance_get_loc(pool_name,
                                                      O_CREAT,
                                                      sizeof(vqec_recv_sock_t),
                                                      max_sockets,
                                                      NULL, NULL);
    if (!recv_sock_pool->sock_pool) {
        goto bail;
    }

    snprintf(buf, ZONE_MAX_NAME_LEN, "%s%s", "ref", pool_name);
    recv_sock_pool->sock_ref_pool = zone_instance_get_loc(buf,
                                                       O_CREAT,
                                                       sizeof(vqec_sock_ref_t),
                                                       2*max_sockets,
                                                       NULL, NULL);
    if (!recv_sock_pool->sock_ref_pool) {
        goto bail;
    }

    return recv_sock_pool;

  bail:
    vqec_recv_sock_pool_destroy(recv_sock_pool);
    return NULL;
}

void
vqec_recv_sock_pool_destroy(vqec_recv_sock_pool_t *pool) {
    if (pool) {
        if (pool->sock_pool) {
            (void)zone_instance_put(pool->sock_pool);
        }
        if (pool->sock_ref_pool) {
            (void)zone_instance_put(pool->sock_ref_pool);
        }
        kfree(pool);
    }
}

