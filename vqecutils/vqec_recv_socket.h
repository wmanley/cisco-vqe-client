/*------------------------------------------------------------------
 * VQEC.  Socket for receiving a channel
 *
 * Apri 2006, Rob Drisko
 *
 * Copyright (c) 2006-2009 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __VQEC_RECV_SOCKET_H__
#define __VQEC_RECV_SOCKET_H__


#include "vam_types.h"    /* for boolean, possibly others? */
#include "vqec_pak.h"

/* vqec_event_ incomplete type
 *
 * The struct vqec_event_ pointer in the vqec_recv_sock_t is used by
 * a subset of the users of vqec_recv_socket.h (specifically, those
 * in the control plane that create an event and store it in the
 * vqec_recv_sock_t structure).
 *
 * Note that the vqec socket libraries themselves do not depend upon it,
 * and thus this pointer field may be removed from the 
 * vqec_recv_sock_t structure in the future.  For now, "vqec_event_"
 * is defined as an incomplete structure type so that users of this
 * .h file which do not use events with sockets can compile w/o including
 * vqec_event.h.
 */
struct vqec_event_;

/*
 * ALL IP ADDRESSES AND PORTS ARE IN NETWORK BYTE ORDER THROUGHOUT
 */

/**
 * Receive socket structure 
 *
 * When binding the socket to a local address, address is stored as:
 *  unicast addr:    (addr) is in (rcv_if_address), mcast_group is 0.0.0.0
 *  multicast addr:  (addr, ifc_addr) is in (mcast_group, rcv_if_address)
 */
typedef struct vqec_recv_sock_ 
{
    /* PRIVATE -- Use API */
    struct in_addr rcv_if_address; /*!< receive interface address */
    in_port_t port;                /*!< bind port */
    boolean blocking;              /*!< blocking or not */
    struct in_addr mcast_group;    /*!< mcast group to listen to */
    struct {
        boolean        use_ip_address;  /* connect socket to remote IP? */
        boolean        use_port;        /* connect socket to remote port? */
        uint16_t       port;            /* remote port */
        struct in_addr ip_address;      /* remote IP address */
    } remote;
    uint32_t rcv_buff_bytes;       /*!< receive buff size for socket */
    uint8_t xmit_dscp;             /*!< DSCP to be used for transmitting */
    int fd;                        /*!< fd */
    struct socket *ksock;          /*!< kernel socket handle */
    struct vqec_event_ *vqec_ev;   /*!< event to drive read loop */
    int32_t ref_count;             /*!< ref count if sock is shared */
} vqec_recv_sock_t;

typedef struct vqec_recv_sock_pool_ {
    struct vqe_zone *sock_pool;
    struct vqe_zone *sock_ref_pool;
} vqec_recv_sock_pool_t;

/**
 * Creates a pool from which receive sockets 
 * can be created. Used by vqec_recv_sock_create.
 *
 * @param[in] pool_name     unique string identifier
 * @param[in] max_sockets   maximum of simultaneous sockets
 * @param[out] vqec_recv_sock_pool_t*   pointer to the new pool
 */
vqec_recv_sock_pool_t *
vqec_recv_sock_pool_create (char * pool_name, uint32_t max_sockets);

/**
 * Destroys a pool create by vqec_recv_sock_pool_create.
 * All associated resources are freed.
 *
 * @param[in] pool  pointer to the pool ready to be destroyed
 */
void
vqec_recv_sock_pool_destroy (vqec_recv_sock_pool_t * pool);

/**
 * Reserve an (addr, port) pair. After calling this function, 
 * vqec_recv_sock_create may be called with the reserved address 
 * and port number. If *port is zero a dynamic port number will
 * be chosen and returned using the port argument.
 *
 * @param[in]  addr     address on which to reserve port
 * @param[in]  port     port number to be reserved (may be zero)
 * @param[in]  pool     recv socket pool context
 * @param[out] addr     address reserved
 * @param[out] port     port number reserved
 * @param[out] boolean  TRUE if success, FALSE otherwise 
 */
boolean
vqec_recv_sock_open (in_addr_t *addr,
                     in_port_t *port,
                     vqec_recv_sock_pool_t *pool);

/**
 * Unreserve a previously reserved (addr, port) pair.
 * No effect if a match cannot be found.
 *
 * @param[in]  addr  previously reserved address
 * @param[in]  port  previously reserved port number
 */
void
vqec_recv_sock_close (in_addr_t addr,
                      in_port_t port);

#define INET_NTOP_STATIC_BUF0 0
#define INET_NTOP_STATIC_BUF1 1
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
inet_ntop_static_buf(in_addr_t ip, int buf_num);

vqec_recv_sock_t *
vqec_recv_sock_create(char * name,
                     in_addr_t rcv_if_address, 
                     in_port_t      port,
                     in_addr_t mcast_group,
                     boolean blocking,
                     uint32_t rcv_buff_bytes,
                     uint8_t xmit_dscp_value);

vqec_recv_sock_t *
vqec_recv_sock_create_in_pool(
                     vqec_recv_sock_pool_t * pool,
                     char * name,
                     in_addr_t rcv_if_address, 
                     in_port_t      port,
                     in_addr_t mcast_group,
                     boolean blocking,
                     uint32_t rcv_buff_bytes,
                     uint8_t xmit_dscp_value);

uint32_t
vqec_recv_sock_get_pak_overhead(void);

boolean
vqec_recv_sock_set_so_rcvbuf(vqec_recv_sock_t *sock,
                             boolean replace,
                             int so_rcvbuf);

boolean
vqec_recv_sock_connect(vqec_recv_sock_t  *sock,
                       boolean           use_remote_addr,
                       in_addr_t         remote_addr,
                       boolean           use_remote_port,
                       uint16_t          remote_port);

static inline int
vqec_recv_sock_get_fd(vqec_recv_sock_t * sock)
{ return sock->fd; }

static inline void
vqec_recv_sock_set_fd(vqec_recv_sock_t * sock,
                      int fd)
{ sock->fd = fd; }

static inline struct in_addr 
vqec_recv_sock_get_rcv_if_address(vqec_recv_sock_t * sock)
{ return sock->rcv_if_address; }

static inline void
vqec_recv_sock_set_rcv_if_address(vqec_recv_sock_t * sock,
                                  struct in_addr rcv_if_address)
{ sock->rcv_if_address.s_addr = rcv_if_address.s_addr; }

static inline uint16_t
vqec_recv_sock_get_port(vqec_recv_sock_t * sock)
{ return sock->port; }

static inline void
vqec_recv_sock_set_port(vqec_recv_sock_t * sock,
                        uint16_t port)
{ sock->port = port; }

static inline boolean
vqec_recv_sock_get_blocking(vqec_recv_sock_t * sock)
{ return sock->blocking; }

static inline uint32_t 
vqec_recv_sock_get_rcv_buff_bytes(vqec_recv_sock_t * sock)
{ return sock->rcv_buff_bytes; }

static inline struct in_addr 
vqec_recv_sock_get_mcast_group(vqec_recv_sock_t * sock)
{ return sock->mcast_group; }

static inline void
vqec_recv_sock_set_mcast_group(vqec_recv_sock_t * sock,
                               struct in_addr mcast_group)
{ sock->mcast_group.s_addr = mcast_group.s_addr; }

/**
   Destroy a socket.
   @param[in] sock pointer of socket to destory.
 */
void vqec_recv_sock_destroy(vqec_recv_sock_t * sock);

void vqec_recv_sock_destroy_in_pool(
                            vqec_recv_sock_pool_t * pool,
                            vqec_recv_sock_t * sock);

/**
 vqec_recv_sock_read_pak
 Read packet data from a socket.
 @param[in] sock pointer of socket.
 @param[in/out] pak pointer to pak structure to put read data into.
 @return Number of bytes received from socket.  If fail, return < 1.
*/
int vqec_recv_sock_read_pak(vqec_recv_sock_t *sock,
                            vqec_pak_t *pak);


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
int vqec_recv_sock_read(vqec_recv_sock_t *sock,
                        struct timeval *rcv_time,
                        void *buf,
                        uint32_t buf_len,
                        struct in_addr *src_addr,
                        uint16_t *src_port);

/**
 * Send a buffer to a particular address through a given socket.
 */
size_t vqec_recv_sock_sendto(vqec_recv_sock_t *sock,
                             void *buff,
                             size_t len,
                             unsigned flags,
                             struct sockaddr *addr,
                             int addr_len);

/**
   Send the multicast igmp join for a group this socket listen to.
   @param[in] sock pointer of this socket
*/
boolean vqec_recv_sock_mcast_join(vqec_recv_sock_t *sock,
                                  struct in_addr filter_src_addr);

/**
   Send the multicast igmp leave for a group this socket listen to.
   @param[in] sock pointer of the socket.
*/
boolean vqec_recv_sock_mcast_leave(vqec_recv_sock_t *sock,
                                   struct in_addr filter_src_addr);

/**
   Create a UDP socket for sending
   @param[in] sa source ip address
   @param[in] sp source port
   @return fd 
 */
int vqec_send_socket_create(in_addr_t sa,
                            in_port_t sp,
                            uint8_t xmit_dscp_value);

/**
   Increase ref count of socket
   @param[in] sock pointer of the socket
 */
void vqec_recv_sock_ref(vqec_recv_sock_t * sock);

/**
 * Create a named UNIX socket in the local domain.
 *
 * @param[in] Named socket reference.
 * @param[out] int32_t Identifier of the socket created.
 **/ 
int32_t 
vqec_create_named_socket (struct sockaddr_un *name);

/**
* Log a socket-specific syslog error message.
*
* @param[in] errmsg Pointer to a constant error string.
* @param[in] errnum errno value.
*/
void 
vqec_recv_sock_perror (const char *errmsg, int32_t errnum);

#endif /* __VQEC_RECV_SOCKET_H__ */
