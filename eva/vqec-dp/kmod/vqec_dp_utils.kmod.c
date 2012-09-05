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
 * Description: Dataplane utility methods for IPC.
 *
 * Documents: 
 *
 *****************************************************************************/

#include "vqec_dp_api.h"
#include "vqec_dp_utils.h"
#include "vqec_dp_tlm.h"
#include "linux/net.h"
#include "linux/version.h"
#include "net/sock.h"
#include "linux/skbuff.h"
#include "linux/ip.h"
#include "linux/udp.h"

#define SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path)  \
		      + strlen ((ptr)->sun_path))

typedef
struct vqec_dp_upcall_info_
{
    /**
     * Upcall socket.
     */
    struct socket *sock;
    /**
     * Upcall socket address.
     */
    struct sockaddr_un sock_addr;
    /**
     * Number of upcalls.
     */
    uint32_t upcall_count;

} vqec_dp_upcall_info_t;

static vqec_dp_upcall_info_t s_irq_upcall_info, s_pak_upcall_info;


/**---------------------------------------------------------------------------
 * Enqueue an upcall IRQ event for the control-plane on the upcall socket.
 * The API version is added as the header for each upcall message for
 * control-plane sanity check. 
 *
 * @param[in] ev Pointer to the upcall event.
 * @param[out] boolean Returns true if the IRQ event is successfully written 
 * to the socket.
 *---------------------------------------------------------------------------*/ 
boolean
vqec_dp_ipcserver_tx_upcall_irq (vqec_dp_upcall_irq_event_t *ev)
{
#define VQEC_IRQ_IOV_CNT 2
    vqec_dp_upcall_info_t *upc_info;
    int32_t len, err, iov_cnt = 0;
    boolean ret = FALSE;
    struct msghdr msg;
    struct iovec iov[VQEC_IRQ_IOV_CNT];
    vqec_dp_api_version_t ver = VQEC_DP_API_VERSION;
    vqec_dp_tlm_t *tlm = vqec_dp_tlm_get(); 
    mm_segment_t old_fs;

    if (!ev || 
        !tlm) {
        return (ret);
    }

    upc_info = &s_irq_upcall_info;

    if (upc_info->sock) {

        memset(&msg, 0, sizeof(msg));
        iov[iov_cnt].iov_base = &ver;  /* version sub-block */
        iov[iov_cnt].iov_len = sizeof(ver);
        iov_cnt++;
        
        iov[iov_cnt].iov_base = ev;  /* event sub-block */
        iov[iov_cnt].iov_len = sizeof(*ev);
        iov_cnt++;
        
        msg.msg_iov = iov;
        msg.msg_iovlen = iov_cnt;
        
        msg.msg_name = (void *)&upc_info->sock_addr;
        msg.msg_namelen = SUN_LEN(&upc_info->sock_addr);
        msg.msg_flags = MSG_DONTWAIT;
        
        len = sizeof(ver) + sizeof(*ev);
        old_fs = get_fs();
        set_fs(KERNEL_DS);
        err = sock_sendmsg(upc_info->sock, 
                           &msg,
                           len);
        set_fs(old_fs);
        if ((err < 0) || (err != len)) {
            /* failed to enQ IRQ to the control-plane. */
            VQEC_DP_TLM_CNT(irqs_dropped, tlm); 
            VQEC_DP_SYSLOG_PRINT(LOST_UPCALL, 
                                 "socket enQ for upcall IRQ event failed"); 
        } else {
            /* successfully enQd IRQ to the control-plane. */
            upc_info->upcall_count++;
            VQEC_DP_TLM_CNT(irqs_sent, tlm);
            ret = TRUE;
        }
    } else {
        /* don't have a socket setup. */
        VQEC_DP_TLM_CNT(irqs_dropped, tlm);           
        VQEC_DP_SYSLOG_PRINT(LOST_UPCALL, 
                             "no socket decriptor to send IRQ upcall");
    }

    return (ret);
}


/**---------------------------------------------------------------------------
 * Eject a packet to the control-plane - this is done on a best-effort basis,
 * i.e, if the socket is full the packet is dropped. 
 * The API version is added as the header for each upcall message for
 * control-plane sanity check. 
 *
 * @param[in] hdr Pointer to the packet header to be appended to the contents.
 * @param[in] pak Pointer to the packet.
 * @param[out] boolean Returns true if contents are successfully written.
 *---------------------------------------------------------------------------*/ 
static char s_nat_rspbuf[VQEC_DP_IPC_MAX_PAKSIZE];
boolean
vqec_dp_ipcserver_eject_pak (vqec_dp_pak_hdr_t *hdr, vqec_pak_t *pak)
{
#define VQEC_EJECT_IOV_CNT 3
    vqec_dp_upcall_info_t *pak_upc_info;
    int32_t len, err, iov_cnt = 0;
    boolean ret = FALSE;
    struct msghdr msg;
    struct iovec iov[VQEC_EJECT_IOV_CNT];
    vqec_dp_api_version_t ver = VQEC_DP_API_VERSION;
    vqec_dp_tlm_t *tlm = vqec_dp_tlm_get(); 
    mm_segment_t old_fs;
    uint32_t buf_len;

    if (!hdr || 
        !pak || 
        !pak->skb ||
        !tlm) {
        return (ret);
    }

    pak_upc_info = &s_pak_upcall_info;
    if (!pak_upc_info->sock) { 
        /* don't have a socket setup. */
        VQEC_DP_TLM_CNT(ejects_dropped, tlm);
        VQEC_DP_SYSLOG_PRINT(LOST_UPCALL, 
                             "no socket decriptor to send ejected packet");
        return (ret);
    }	
	
    buf_len = ((struct sk_buff *)(pak->skb))->len - sizeof(struct udphdr);
    if (!buf_len || buf_len > VQEC_DP_IPC_MAX_PAKSIZE) {
        VQEC_DP_TLM_CNT(ejects_dropped, tlm);	
        VQEC_DP_SYSLOG_PRINT(LOST_UPCALL, 	
                             "exceeds maximum packet size");
        return (ret);	
    }	

    err = skb_copy_bits(pak->skb, 
                        sizeof(struct udphdr),
                        s_nat_rspbuf, 
                        buf_len);
    if (err < 0) {
        /* failed to enQ. */
        VQEC_DP_TLM_CNT(ejects_dropped, tlm); 
        VQEC_DP_SYSLOG_PRINT(LOST_UPCALL, 
                             "copy of socket buffer failed");
        return (ret);
    }

        
    memset(&msg, 0, sizeof(msg));
    iov[iov_cnt].iov_base = &ver;  /* version sub-block */
    iov[iov_cnt].iov_len = sizeof(ver);
    iov_cnt++;

    iov[iov_cnt].iov_base = hdr;  /* packet header sub-block */
    iov[iov_cnt].iov_len = sizeof(*hdr);
    iov_cnt++;
    
    iov[iov_cnt].iov_base = s_nat_rspbuf;  /* packet buffer sub-block */
    iov[iov_cnt].iov_len = buf_len;
    iov_cnt++;
    
    msg.msg_iov = iov;
    msg.msg_iovlen = iov_cnt;
    
    msg.msg_name = (void *)&pak_upc_info->sock_addr;
    msg.msg_namelen = SUN_LEN(&pak_upc_info->sock_addr);
    msg.msg_flags = MSG_DONTWAIT;
    
    len = sizeof(ver) + sizeof(*hdr) + buf_len;
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    err = sock_sendmsg(pak_upc_info->sock,
                       &msg,
                       len);
    set_fs(old_fs);
    if ((err < 0) || 
        (err != len)) {
        /* failed to enQ. */
        VQEC_DP_TLM_CNT(ejects_dropped, tlm); 
        VQEC_DP_SYSLOG_PRINT(LOST_UPCALL, 
                             "Socket enQ for ejected packet failed");
        
    } else {
        /* successfully enQd IRQ to the control-plane. */
        pak_upc_info->upcall_count++;
        VQEC_DP_TLM_CNT(ejects_sent, tlm);
        ret = TRUE;        
    }

    
    return (ret);
}


/**---------------------------------------------------------------------------
 * Create an upcall socket.
 * 
 * @param[in] info Pointer to the upcall information structure.
 * @param[in] addr Pointer to the socket address structure.
 * @param[out] boolean Returns true if the socket is successfully created.
 *---------------------------------------------------------------------------*/ 
static boolean
vqec_dp_ipcserver_create_upcall_sock (vqec_dp_upcall_info_t *upcall_info,
                                      struct sockaddr_un * addr)
{
    boolean ret = TRUE;
    int err;

    memcpy(&upcall_info->sock_addr, addr, sizeof(struct sockaddr_un));
    err = sock_create(AF_UNIX, SOCK_DGRAM, 0, &upcall_info->sock);
    if (err < 0) {
        VQEC_DP_SYSLOG_PRINT(INIT_FAILURE, 
                             "could not create upcall socket");
        ret = FALSE;
    } 
    
    return (ret);
}


/**---------------------------------------------------------------------------
 * Close an existing upcall socket.
 * 
 * @param[in] info Pointer to the upcall information structure.
 *---------------------------------------------------------------------------*/ 
static void
vqec_dp_ipcserver_close_upcall_sock (vqec_dp_upcall_info_t *upcall_info)
{
    if (upcall_info->sock) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
        upcall_info->sock->ops->shutdown(upcall_info->sock, 0);
#else
        kernel_sock_shutdown(upcall_info->sock, SHUT_RDWR);
#endif
        sock_release(upcall_info->sock); 
        upcall_info->sock = NULL;
    }
}


/**---------------------------------------------------------------------------
 * Setup upcall sockets / destination addresses for the sockets.
 *
 * @param[in] type Type of the socket being setup.
 * @param[in] addr Destination address for the socket.
 * @param[in] len Length of destination address (a redundant parameter if
 * the input address is strongly-typed).
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_create_upcall_socket (vqec_dp_upcall_socktype_t type,
                              vqec_dp_upcall_sockaddr_t  *addr, 
                              uint16_t len)
{
    boolean ret = FALSE;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (!addr ||
        type < VQEC_UPCALL_SOCKTYPE_MIN ||
        type > VQEC_UPCALL_SOCKTYPE_MAX) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    
    switch (type) {
    case VQEC_UPCALL_SOCKTYPE_IRQEVENT:
        ret = vqec_dp_ipcserver_create_upcall_sock(&s_irq_upcall_info, 
                                                   &addr->sock);
        break;

    case VQEC_UPCALL_SOCKTYPE_PAKEJECT:
        ret = vqec_dp_ipcserver_create_upcall_sock(&s_pak_upcall_info, 
                                                   &addr->sock);
        break;

    default:
        VQEC_DP_ASSERT_FATAL(FALSE, "Bad code path");
        break;
    }

    if (!ret) {
        err = VQEC_DP_ERR_INTERNAL;
    }

    return (err);
}


/**---------------------------------------------------------------------------
 * Close all upcall sockets.
 *
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
vqec_dp_error_t
vqec_dp_close_upcall_sockets (void) 
{
    vqec_dp_ipcserver_close_upcall_sock(&s_irq_upcall_info);
    vqec_dp_ipcserver_close_upcall_sock(&s_pak_upcall_info);
    memset(&s_irq_upcall_info, 0, sizeof(s_irq_upcall_info));
    memset(&s_pak_upcall_info, 0, sizeof(s_pak_upcall_info));

    return (VQEC_DP_ERR_OK);
}
