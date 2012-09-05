/*------------------------------------------------------------------
 * VQEC. Socket for receiving a channel
 *
 * February 2006, Rob Drisko
 *
 * Copyright (c) 2006-2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "vam_util.h"
#include "vqec_assert_macros.h"
#include "vqec_debug.h"
#include "vqec_recv_socket.h"
#include <utils/zone_mgr.h>

#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#include "test_vqec_interposers_common.h"
#endif

#define RECV_SOCKET_ERR_STRLEN 256

static boolean vqec_sock_ref_open (struct sockaddr_in *saddr,
                                   int *fd,
                                   vqec_recv_sock_pool_t *pool);
static void vqec_sock_ref_close (struct sockaddr_in *saddr);

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
    struct in_addr addr;
    static char buf0[INET_ADDRSTRLEN];
    static char buf1[INET_ADDRSTRLEN];

    addr.s_addr = (ip);
    return (inet_ntop(AF_INET, &addr, buf_num ? buf1 : buf0, INET_ADDRSTRLEN));
}

void 
vqec_recv_sock_perror (const char *errmsg, int32_t errnum)
{
    char errbuf[RECV_SOCKET_ERR_STRLEN];
    char logbuf[RECV_SOCKET_ERR_STRLEN];
    if (!strerror_r(errno, errbuf, RECV_SOCKET_ERR_STRLEN)) {
        snprintf(logbuf, RECV_SOCKET_ERR_STRLEN, "%s: %s", errmsg, errbuf);
        syslog_print(VQEC_ERROR, logbuf);
    }
}

typedef enum {
    VQEC_MCAST_JOIN,
    VQEC_MCAST_LEAVE
} vqec_mcast_op;

/*
 * Adds/removes membership in a given multicast group FOR A GIVEN SOURCE
 * ADDRESS.
 */
static boolean
vqec_recv_sock_mcast_op_membership_src (vqec_mcast_op  mcast_op,
                                        int fd,
                                        struct in_addr mcast_group,
                                        struct in_addr rcv_if_address,
                                        struct in_addr filter_src_addr)
{
    boolean ret = FALSE;
#ifdef IP_ADD_SOURCE_MEMBERSHIP
#ifdef IP_DROP_SOURCE_MEMBERSHIP
    struct ip_mreq_source mreq_src;
    int option_name=0;
    int sock_ret;
    
    if (mcast_op == VQEC_MCAST_JOIN) {
        option_name = IP_ADD_SOURCE_MEMBERSHIP;
    } else {
        option_name = IP_DROP_SOURCE_MEMBERSHIP;
    }
    memset(&mreq_src, 0, sizeof(mreq_src));
    mreq_src.imr_interface.s_addr = rcv_if_address.s_addr;
    mreq_src.imr_multiaddr.s_addr = mcast_group.s_addr;
    mreq_src.imr_sourceaddr.s_addr = filter_src_addr.s_addr;
    sock_ret = setsockopt(fd, IPPROTO_IP, option_name,
                          &mreq_src, sizeof(mreq_src));
    if (sock_ret == -1) {
        vqec_recv_sock_perror("setsockopt failed:", errno);
        goto done;
    }
    ret = TRUE;
done:
#endif  /* IP_DROP_SOURCE_MEMBERSHIP */
#endif  /* IP_ADD_SOURCE_MEMBERSHIP */
    return (ret);
}
                                        
/*
 * Adds/removes membership in a given multicast group INDEPENDENT OF SOURCE
 * ADDRESS.
 */
static boolean
vqec_recv_sock_mcast_op_membership (vqec_mcast_op mcast_op,
                                    int fd,
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
    
    sock_ret = setsockopt(fd, IPPROTO_IP, option_name, &mreqn, sizeof(mreqn));
    if (sock_ret == -1) {
        vqec_recv_sock_perror("setsockopt failed:", errno);
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
static
boolean vqec_recv_sock_mcast_op (vqec_recv_sock_t *sock, 
                                 struct in_addr filter_src_addr,
                                 vqec_mcast_op mcast_op)
{
#define VQEC_RECV_SOCK_MCAST_OP_STRLEN INET_ADDRSTRLEN + 20
    struct in_addr mcast_group; 
    struct in_addr rcv_if_address; 
    struct timeval now;
    char temp[INET_ADDRSTRLEN];
    char temp_filter_str_addr[INET_ADDRSTRLEN];
    char temp_filter_str[VQEC_RECV_SOCK_MCAST_OP_STRLEN];
    boolean ret = TRUE;
    boolean source_filtering = FALSE;
    char *join_or_leave_str=NULL;
    char *gettimeofday_err_str=NULL;

#ifdef IP_ADD_SOURCE_MEMBERSHIP
#ifdef IP_DROP_SOURCE_MEMBERSHIP
    if (filter_src_addr.s_addr != INADDR_ANY) {
        source_filtering = TRUE;
    }
#endif /* IP_DROP_SOURCE_MEMBERSHIP */
#endif /* IP_ADD_SOURCE_MEMBERSHIP */

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
        VQEC_ASSERT(0);
        return FALSE;
        break;
    }
    if (sock) {
        mcast_group = vqec_recv_sock_get_mcast_group(sock);
        rcv_if_address = vqec_recv_sock_get_rcv_if_address(sock);
        if (IN_MULTICAST(ntohl(mcast_group.s_addr))) {
            if (source_filtering) {
                ret = vqec_recv_sock_mcast_op_membership_src(mcast_op,
                                                             sock->fd,
                                                             mcast_group,
                                                             rcv_if_address,
                                                             filter_src_addr);
            } else {
                ret = vqec_recv_sock_mcast_op_membership(mcast_op,
                                                         sock->fd,
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
        memset(&now, 0, sizeof(now));
        if (gettimeofday(&now, 0) == -1) {
            vqec_recv_sock_perror(gettimeofday_err_str, errno);
        }
        if (!inet_ntop(AF_INET, &mcast_group.s_addr,
                       temp, INET_ADDRSTRLEN)) {
            temp[0] = '\0';
        }
        if (source_filtering) {
            if (!inet_ntop(AF_INET, &filter_src_addr.s_addr,
                           temp_filter_str_addr, INET_ADDRSTRLEN)) {
                (void)snprintf(temp_filter_str,
                               VQEC_RECV_SOCK_MCAST_OP_STRLEN,
                               " filter addr [?]");
            } else {
                (void)snprintf(temp_filter_str,
                               VQEC_RECV_SOCK_MCAST_OP_STRLEN,
                               " filter addr %s",
                               temp_filter_str_addr);
            }
        } else {
            temp_filter_str[0] = '\0';
        }
        VQEC_DEBUG(VQEC_DEBUG_RCC,
                   "%s %s mcast %s%s @ %lu/%lu (sec/usec)\n",
                   ret ? "" : "FAILED ",
                   join_or_leave_str,
                   temp,
                   temp_filter_str,
                   now.tv_sec, 
                   now.tv_usec);
    }

    return ret;
}

boolean vqec_recv_sock_mcast_join(vqec_recv_sock_t *sock, 
                                  struct in_addr filter_src_addr)
{
    return vqec_recv_sock_mcast_op(sock, 
                                  filter_src_addr,
                                  VQEC_MCAST_JOIN);
}

boolean vqec_recv_sock_mcast_leave(vqec_recv_sock_t *sock,
                                   struct in_addr filter_src_addr)
{
    return vqec_recv_sock_mcast_op(sock, 
                                  filter_src_addr,
                                  VQEC_MCAST_LEAVE);
}

/**
 vqec_recv_sock_create
 Create a socket for receiving data.
 This socket will be created with the following parameters:
 - SO_REUSEADDR = on (if multicast)
 - SO_TIMESTAMP = on
 - SO_RCVBUF (passed in as arg)
 - O_NONBLOCK (passed in as arg)
 The packets being received by this socket must have timestamps
 on them as they are used in our code later.
 @param[in] name Name of this socket.
 @param[in] rcv_if_address ip address of receive interface.
 @param[in] port the port we want to bind/receive packet.
 @param[in] mcast_group mcast address we want to listen
 @param[in] blocking blocking or not
 @param[in] rcv_buff_bytes Buffer size pass to socket.
 @param[in] evh Function pointer of the event handler. Tell us how we want
 to process the received packet.
 @param[in] event_data pointer of the data will pass to event handler.
 @return pointer of created socket.
*/
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
                     vqec_recv_sock_pool_t *pool,
                     char * name,
                     in_addr_t rcv_if_address, 
                     uint16_t         port,
                     in_addr_t mcast_group,
                     boolean blocking,
                     uint32_t rcv_buff_bytes,
                     uint8_t xmit_dscp_value)
{
    vqec_recv_sock_t * result;
    struct sockaddr_in saddr;
    char buf[INET_ADDRSTRLEN];
    char buf2[INET_ADDRSTRLEN];
    int on = 1;
    int dscp = (xmit_dscp_value << DSCP_SHIFT);

    /* Debug print out */
    if (!inet_ntop(AF_INET, &rcv_if_address, buf, INET_ADDRSTRLEN)) {
        buf[0] = '\0';
    }
    if (!inet_ntop(AF_INET, &mcast_group, buf2, INET_ADDRSTRLEN)) {
        buf2[0] = '\0';
    }
    VQEC_DEBUG(VQEC_DEBUG_RCC,
               "vqec_recv_sock_create: if_addr=%s port=%d "
               "mcast=%s block=%d rcv_buff=%u\n",
               buf,
               ntohs(port),
               buf2,
               blocking,
               rcv_buff_bytes);

    /* Allocate receive socket */
    if (pool) {
        result = (vqec_recv_sock_t *) zone_acquire(pool->sock_pool);
    } else {
        result = malloc(sizeof(vqec_recv_sock_t));
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
    if (!vqec_sock_ref_open(&saddr, &result->fd, pool)) {
        if (pool) {
            zone_release(pool->sock_pool, result);
        } else {
            free(result);
        }
        return NULL;
    }

    /* Store the socket's local port, if dynamically assigned */
    if (!port) {
        result->port = saddr.sin_port;
    }

    if (setsockopt(result->fd, SOL_SOCKET, 
                   SO_TIMESTAMP, &on, sizeof(on)) == -1) {
        vqec_recv_sock_perror("setsockopt", errno);
        goto bail;
    }

    if (setsockopt(result->fd, IPPROTO_IP, IP_TOS, &dscp, sizeof(dscp))) {
        vqec_recv_sock_perror("setsockopt", errno);
        syslog_print(VQEC_ERROR, "Might need root privileges to transmit"
                     " with elevated DSCP values, continuing with "
                     "DSCP value of 0 instead...");
    }

    if (rcv_buff_bytes) {
        if (setsockopt(result->fd, SOL_SOCKET,
                       SO_RCVBUF, &rcv_buff_bytes, sizeof(rcv_buff_bytes)) == -1) {
            vqec_recv_sock_perror("setsockopt", errno);
            goto bail;
        }
    }

    if (! blocking) {
        if (fcntl(result->fd, F_SETFL, O_NONBLOCK) == -1) {
            vqec_recv_sock_perror("fcntl", errno);
            goto bail;
        }
    }

    result->ref_count = 1;

    return result;

 bail:
    vqec_sock_ref_close(&saddr);
    free(result);
    return NULL;
  
}

/**
 * Return the per-packet overhead due to storing a buffer associated with a
 * single packet.  Currently, only the kernel-space implementation has an
 * overhead associated with each packet buffer.
 *
 * @param[out]  returns the per-packet overhead in bytes.
 */
uint32_t vqec_recv_sock_get_pak_overhead (void)
{
    return (0);
}

/**
 * This API is only applicable when used in kernel-space.  Therefore, this
 * user-space implementation is a no-op.
 */
boolean
vqec_recv_sock_set_so_rcvbuf (vqec_recv_sock_t *sock,
                              boolean replace,
                              int so_rcvbuf)
{
    return (TRUE);
}

void vqec_recv_sock_destroy (vqec_recv_sock_t * sock)
{
    vqec_recv_sock_destroy_in_pool(NULL, sock);
}

void vqec_recv_sock_destroy_in_pool (vqec_recv_sock_pool_t * pool,
                                     vqec_recv_sock_t * sock)
{
    struct sockaddr_in saddr;

    VQEC_ASSERT(sock->ref_count > 0);
    sock->ref_count--;

    if (sock->ref_count > 0) {
        return;
    }

    memset(&saddr, 0, sizeof(saddr));
    if (IN_MULTICAST(ntohl(sock->mcast_group.s_addr))) {
        saddr.sin_addr = sock->mcast_group;
    } else {
        saddr.sin_addr = sock->rcv_if_address;
    }
    saddr.sin_family = AF_INET;
    saddr.sin_port = sock->port;

    vqec_sock_ref_close(&saddr);

    if (pool) {
        zone_release(pool->sock_pool, sock);
    } else {
        free(sock);
    }
}

/**
 vqec_recv_sock_connect

 Establishes a socket connection as needed to ensure only packets matching
 the (addr, port) source address pair supplied will be received by the socket.
 Both the socket's existing (addr, port) connection (if any) and requested
 (addr, port) connection are used to disconnect and/or reconnect the socket
 as appropriate.
 
 The socket MUST be bound to a unicast address.
 Returns FALSE if the socket is bound to a multicast address, or
 if the socket connect() call fails.
 Connection implies that only packets matching the (addr, port) source
 address pair supplied will be received by the socket. 

 @param[in] sock pointer to socket.
 @param[in] use_remote_addr indicates whether remote_addr should be used
 @param[in] remote_addr remote address to be used for connect()
 @param[in] use_remote_port indicates whether remote_port should be used
 @param[in] remote_port remote port to be used for connect()
*/
boolean
vqec_recv_sock_connect (vqec_recv_sock_t  *sock,
                        boolean           use_remote_addr,
                        in_addr_t         remote_addr,
                        boolean           use_remote_port,
                        uint16_t          remote_port)
{
/* this includes the port number plus error text */
#define ERRBUFLEN INET_ADDRSTRLEN + 30
    char tmpbuf[INET_ADDRSTRLEN];
    char tmpbuf2[ERRBUFLEN];
    struct sockaddr *addr, addr_unspec;
    struct sockaddr_in saddr;
    boolean status = TRUE;
    int addr_size;

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

#undef VQEC_DEBUG_CONNECT
#if VQEC_DEBUG_CONNECT
    struct sockaddr_in local_saddr, remote_saddr;
    char localaddr_str[INET_ADDRSTRLEN];
    char remoteaddr_str[INET_ADDRSTRLEN];
    socklen_t saddr_length = sizeof(struct sockaddr_in);
    
    if(getsockname(sock->fd, (struct sockaddr *)&local_saddr, 
                   &saddr_length) == -1) {        
        vqec_recv_sock_perror("getsockname", errno);
        goto done;
    }
    printf("\n CONNECT socket request:\n"
           "\n   local binding:            (%s:%u)\n"
           "\n   requested remote binding: (%s:%u)\n",
           inet_ntop(AF_INET, &local_saddr.sin_addr, localaddr_str,
                     INET_ADDRSTRLEN),
           ntohs(local_saddr.sin_port),
           use_remote_addr ? 
           inet_ntop(AF_INET, &saddr.sin_addr, remoteaddr_str,
                     INET_ADDRSTRLEN) : "<any>",
           use_remote_port ? 
           ntohs(saddr.sin_port) : 0);
    
    if(getpeername(sock->fd, (struct sockaddr *)&remote_saddr, 
                   &saddr_length) == -1) {        
        if (errno == ENOTCONN) {
            memset(&remote_saddr, 0, sizeof(struct sockaddr));
        } else {
            vqec_recv_sock_perror("getpeername", errno);            
        }
    }
    printf("\n PRE-CONNECT socket info\n"
           "\n   retrieved remote binding: (%s:%u)\n",
           inet_ntop(AF_INET, &remote_saddr.sin_addr, remoteaddr_str,
                         INET_ADDRSTRLEN),
           ntohs(remote_saddr.sin_port));
#endif

    if (connect(sock->fd, addr, addr_size) == -1) {
        inet_ntop(((struct sockaddr_in *)addr)->sin_family,
                  &((struct sockaddr_in *)addr)->sin_addr,
                  tmpbuf, INET_ADDRSTRLEN);
        snprintf(tmpbuf2, ERRBUFLEN, "connect (remote %s:%u)",
                 tmpbuf, ntohs(((struct sockaddr_in *)addr)->sin_port));
        vqec_recv_sock_perror(tmpbuf2, errno);
        status = FALSE;
        goto done;
    }
    sock->remote.use_ip_address = use_remote_addr;
    sock->remote.use_port = use_remote_port;
    sock->remote.port = remote_port;
    sock->remote.ip_address.s_addr = remote_addr;

#if VQEC_DEBUG_CONNECT
    if(getpeername(sock->fd, (struct sockaddr *)&remote_saddr, 
                   &saddr_length) == -1) {        
        vqec_recv_sock_perror("getpeername", errno);
        goto done;
    }
    printf("\n POST-CONNECT socket info\n"
           "\n   retrieved remote binding: (%s:%u)\n",
           inet_ntop(AF_INET, &remote_saddr.sin_addr, remoteaddr_str,
                         INET_ADDRSTRLEN),
           ntohs(remote_saddr.sin_port));
#endif

done:
    return (status);
}

/**
 vqec_recv_sock_read_pak
 Read packet data from a socket.
 @param[in] sock pointer of socket.
 @param[in/out] pak pointer to pak structure to put read data into.
 @return Number of bytes received from socket.  If fail, return < 1.
*/
int vqec_recv_sock_read_pak (vqec_recv_sock_t *sock,
                             vqec_pak_t *pak)
{

#define MAX_CMSG_BUF_SIZE 1024 

    struct msghdr   msg;
    struct sockaddr_in saddr;
    struct iovec vec;
    char ctl_buf[MAX_CMSG_BUF_SIZE];
    struct cmsghdr *cmsg;
    void * cmsg_data;
    int bytes_received;

    if (!sock || !pak) {
        return (0);
    }

    vec.iov_base = pak->buff;
    vec.iov_len = pak->alloc_len;

    memset(&msg, 0, sizeof(msg));
 
    msg.msg_name = &saddr;
    msg.msg_namelen = sizeof(saddr);
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1;

    msg.msg_control = ctl_buf;
    msg.msg_controllen = sizeof(ctl_buf);    

    /* will fill the buffer associated with the pak */
    if ((bytes_received = recvmsg(sock->fd, &msg, 0)) == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            pak->buff_len = bytes_received;
            return (0);
        } else {
            vqec_recv_sock_perror("recvmsg", errno);
            return (0);
        }
    }

    /* set the original rcv_ts of the received pak */
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
       if (cmsg->cmsg_level == SOL_SOCKET 
            && cmsg->cmsg_type == SO_TIMESTAMP) {       

           cmsg_data = CMSG_DATA(cmsg);
            if (cmsg_data) {
                pak->rcv_ts = 
                    timeval_to_abs_time(*((struct timeval *)cmsg_data));
            } else {
                return (0);
            }

        } else {
           return (0);
        }
    }

    /* set the source address and port of the pak */
    pak->src_addr = saddr.sin_addr;
    pak->src_port = saddr.sin_port;

    /* set the head offset and received length of the pak buffer */
    pak->head_offset = 0;
    pak->buff_len = bytes_received;

    return (bytes_received);
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
/* this includes the port number plus error text */
#define ERRBUFLEN INET_ADDRSTRLEN + 30
    size_t txlen = 0;
    char tmpbuf[INET_ADDRSTRLEN];
    char tmpbuf2[ERRBUFLEN];

    if (!sock) {
        return (-1);
    }

    txlen = sendto(sock->fd, buff, len, flags, addr, addr_len);
    if (len != txlen) {
        inet_ntop(((struct sockaddr_in *)addr)->sin_family,
                  &((struct sockaddr_in *)addr)->sin_addr,
                  tmpbuf, INET_ADDRSTRLEN);
        snprintf(tmpbuf2, ERRBUFLEN, "sendto (dest %s:%u)",
                 tmpbuf, ntohs(((struct sockaddr_in *)addr)->sin_port));
        vqec_recv_sock_perror(tmpbuf2, errno);
    }

    return (txlen);
}

/* Create a UDP socket for sending */
int vqec_send_socket_create(in_addr_t sa,
                            in_port_t sp,
                            uint8_t xmit_dscp_value)
{
    int ret;    
    struct sockaddr_in me;
    int dscp = (xmit_dscp_value << DSCP_SHIFT);
    
    char buf[INET_ADDRSTRLEN];
    struct in_addr src;

    src.s_addr = sa;
    if (!inet_ntop(AF_INET, &src, buf, INET_ADDRSTRLEN)) {
        buf[0] = '\0';
    }

    VQEC_DEBUG(VQEC_DEBUG_RCC,
               "Passed in source:%s:%d\n", buf, ntohs(sp));

    
    /* Create a rudimentary socket */
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        vqec_recv_sock_perror("open", errno);
        return(-1);
    }

    bzero(&me, sizeof(me));
    me.sin_family = AF_INET;
    me.sin_addr.s_addr = sa;
    me.sin_port = sp;

    if (setsockopt(fd, IPPROTO_IP, IP_TOS, &dscp, sizeof(dscp))) {
        vqec_recv_sock_perror("setsockopt", errno);
        syslog_print(VQEC_ERROR, "Might need root privileges to transmit"
                     " with elevated DSCP values, continuing with "
                     "DSCP value of 0 instead...");
    }

    ret = bind(fd, (struct sockaddr *)&me, sizeof(me));
    if (ret == -1) {
        vqec_recv_sock_perror("bind", errno);
        close(fd);
        return ret;
    }

    VQEC_DEBUG(VQEC_DEBUG_RCC,
               "Connected a socket (for sending) from %s:%d\n",
               buf, ntohs(me.sin_port));
    
    return fd;
}

void vqec_recv_sock_ref (vqec_recv_sock_t * sock)
{
    VQEC_ASSERT(sock);
    sock->ref_count++;
}


/**---------------------------------------------------------------------------
 * Create a named UNIX socket in the local domain.
 *
 * @param[in] Named socket reference.
 * @param[out] int32_t Identifier of the socket created.
 *---------------------------------------------------------------------------*/                                  
int32_t 
vqec_create_named_socket (struct sockaddr_un *name)
{
    int32_t sock = -1;
    size_t size;
     
    /* Create the socket. */
    sock = socket(PF_LOCAL, SOCK_DGRAM, 0);
    if (sock == -1) {
        vqec_recv_sock_perror("socket", errno);
        goto bail;
    }

    /* 
     * The size of the address is the offset of the start of the filename,
     *   plus its length,
     *   plus one for the terminating null byte.
     * Alternatively you can just do:
     *   size = SUN_LEN (name);
     */
    size = SUN_LEN(name);

    /* 
     * Must remove the sockpath file before binding. Otherwise bind() would
     * return address already in use error.
     */
    unlink(name->sun_path);

    if (bind(sock, (struct sockaddr *) name, size) < 0) {
        vqec_recv_sock_perror("bind", errno);
        goto bail;
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
        vqec_recv_sock_perror("fcntl", errno);
       goto bail;
    }

    return (sock);

bail:
    if (sock != -1) {
        close(sock);
    }

    return (-1);
}

/*----------------------------------------------------------------------------
 * Reserved Socket API 
 *--------------------------------------------------------------------------*/
typedef struct vqec_sock_ref_ {
    struct sockaddr_in saddr;
    int fd;
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
static vqec_sock_ref_t* vqec_sock_ref_find (struct sockaddr_in *saddr)
{
    vqec_sock_ref_t *sock_ref, *sock_ref_nxt;

    /* find existing vqec_sock_ref_t which matches saddr */
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
 * @param[in]  fd      file descriptor of the new socket
 * @param[in]  pool    pool from which to allocate socket ref
 * @param[out] boolean TRUE if successful, FALSE otherwise
 */
static boolean vqec_sock_ref_open (struct sockaddr_in *saddr,
                                   int *fd,
                                   vqec_recv_sock_pool_t *pool)
{
    vqec_sock_ref_t *sock_ref;
    socklen_t saddr_length = sizeof(struct sockaddr_in);
    int on = 1;

    if (!saddr || !fd) {
        return FALSE;
    }

    if (!sock_list_initialized) {
        VQE_TAILQ_INIT(&sock_list);
        sock_list_initialized = TRUE;
    }

    *fd = -1;
    sock_ref = vqec_sock_ref_find(saddr);
    if (sock_ref) {
        /* Socket reference exists, return */
        *saddr = sock_ref->saddr;
        *fd = sock_ref->fd;
        sock_ref->refcnt++;
    } else {
        /* Allocate new socket reference */
        if (pool) {
            sock_ref = (vqec_sock_ref_t *)zone_acquire(pool->sock_ref_pool);
        } else {
            sock_ref = (vqec_sock_ref_t *)malloc(sizeof(vqec_sock_ref_t));
        }
        if (!sock_ref) {
            goto bail;
        }
        /* Create datagram socket */
        if ((*fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
            vqec_recv_sock_perror("socket", errno);
            goto bail;
        }
        if (IN_MULTICAST(ntohl(saddr->sin_addr.s_addr))) {
            if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR,
                           &on, sizeof(on)) == -1) {
                vqec_recv_sock_perror("setsockopt", errno);
                goto bail;
            }
        }
        /* Bind socket */
        if (bind(*fd, (struct sockaddr *) saddr, sizeof(*saddr)) == -1) {
            vqec_recv_sock_perror("bind", errno);
            goto bail;
        }
        /* Fetch socket info */
        if (getsockname(*fd, (struct sockaddr *)saddr, &saddr_length)
           == -1) {
            vqec_recv_sock_perror("getsockname", errno);
            goto bail;
        }
        sock_ref->saddr = *saddr;
        sock_ref->fd = *fd;
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
            free(sock_ref);
        }
    }
    if (*fd != -1) {
        close(*fd);
    }
    return FALSE;
}

/**
 * Close an internal socket reference.
 *
 * @param[in]  saddr   socket address of existing ref
 */
static void vqec_sock_ref_close (struct sockaddr_in *saddr)
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

    close(sock_ref->fd);

    if (sock_ref->pool) {
        zone_release(sock_ref->pool->sock_ref_pool, sock_ref);
    } else {
        free(sock_ref);
    }
}

boolean
vqec_recv_sock_open (in_addr_t *addr,
                     in_port_t *port,
                     vqec_recv_sock_pool_t *pool)
{
    struct sockaddr_in saddr;
    int fd;
    boolean rv;

    saddr.sin_addr.s_addr = *addr;
    saddr.sin_port = *port;
    saddr.sin_family = AF_INET;

    rv = vqec_sock_ref_open(&saddr, &fd, pool);

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

    recv_sock_pool = malloc(sizeof(vqec_recv_sock_pool_t));
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
        free(pool);
    }
}

