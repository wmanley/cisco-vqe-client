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
 * Description: Module main.
 *
 * Documents: 
 *
 *****************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/inet.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "vqec_dp_api.h"
#include "vqec_dp_output_shim_private.h"
#include "vqec_dp_oshim_read_api.h"
#include "vqec_recv_socket_macros.h"

#define IPV4_ADDRESS "255.255.255.255"
static unsigned int cp_tuner_id = 0;
static char dest_ipaddr[sizeof(IPV4_ADDRESS) + 1];
static char src_ipaddr[sizeof(IPV4_ADDRESS) + 1];
static unsigned short dest_port;
static uint32_t iobufs = 16;
static uint32_t iobuf_size = 2048;
static int32_t timeout = -1;

module_param(cp_tuner_id, uint, 0644);
MODULE_PARM_DESC(cp_tuner_id, "cp tuner identifier");

module_param_string(dest_ipaddr, dest_ipaddr, sizeof(IPV4_ADDRESS) + 1, 0644);
MODULE_PARM_DESC(dest_ipaddr, "destination IP address");

module_param_string(src_ipaddr, src_ipaddr, sizeof(IPV4_ADDRESS) + 1, 0644);
MODULE_PARM_DESC(dest_ipaddr, "source IP address");

module_param(dest_port, ushort, 0644);
MODULE_PARM_DESC(dest_port, "destination IP port");

module_param(iobufs, uint, 0644);
MODULE_PARM_DESC(iobufs, "number of iobufs");

module_param(iobuf_size, uint, 0644);
MODULE_PARM_DESC(iobuf_size, "size of each iobuf");

module_param(timeout, int, 0644);
MODULE_PARM_DESC(timeout, "timeout value for recvmsg");

typedef 
struct start_params_
{
    struct socket *sk;
    int cp_tuner_id;
    struct completion *completion;
    uint32_t iobufs;
    uint32_t iobuf_size;
    int32_t timeout;
} start_params_t;

static int s_reader_task;
static boolean s_reader_task_running = FALSE, s_reader_task_stop;

size_t udp_tx_pak (struct socket *sk,
                   void *buff,
                   size_t len)
{
    int err;
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t old_fs;

    iov.iov_base = buff;
    iov.iov_len = len;

    msg.msg_name = NULL;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_namelen = 0;
    msg.msg_flags = MSG_DONTWAIT;

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    err = sock_sendmsg(sk, &msg, len);
    set_fs(old_fs);

    if (err < 0) {
        return (-1);
    }

    return (len);
}

static struct socket *
udp_tx_sock_create (struct in_addr if_address, 
                    uint16_t port,
                    struct in_addr dest_addr,
                    uint32_t bufsize)
{
    struct socket *sk;
    struct sockaddr_in saddr;
    char loop = 0;
    int err = 0;

    do {
        err = sock_create(PF_INET, SOCK_DGRAM, 0, &sk);
        if (err < 0) {
            break;
        }

        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_addr = if_address;
        saddr.sin_family = AF_INET;
        saddr.sin_port = 0;    
        err = kernel_bind(sk, 
                          (struct sockaddr *) &saddr,
                          sizeof(saddr));
        if (err < 0) {
            break;
        }

        err = kernel_setsockopt(sk, 
                                IPPROTO_IP, 
                                IP_MULTICAST_LOOP, 
                                &loop, 
                                sizeof(loop));
        if (err < 0) {
            break;
        }

        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_addr = dest_addr;
        saddr.sin_port = port;
        err = kernel_connect(sk,
                             (struct sockaddr *)&saddr, 
                             sizeof(saddr), 
                             0);
        if (err < 0) {
            break;
        }

        if (bufsize) {
            err = kernel_setsockopt(sk, 
                                    SOL_SOCKET,
                                    SO_SNDBUF, 
                                    (char *)&bufsize,
                                    sizeof(bufsize));
            if (err < 0) {
                break;
            }
        }

    } while (0);

    if (err < 0) {
        kernel_sock_shutdown(sk, SHUT_RDWR);
        sock_release(sk);
        sk = NULL;
    }
    
    return (sk);
}

static int32_t
vqec_reader_loop (void *task_params) 
{
    int32_t bytes_read, i;
    vqec_iobuf_t *m_iobuf;
    start_params_t params;
    vqec_dp_error_t err;

    /* 
     * -- reparent task to init().
     * -- accept SIGKILL.
     * -- signal parent to notify of thread startup. 
     */
    daemonize("vqec_reader_loop");
    allow_signal(SIGKILL);
    __set_current_state(TASK_INTERRUPTIBLE);
    params =  *(start_params_t *)task_params;
    complete(params.completion);

    m_iobuf = kmalloc(sizeof(vqec_iobuf_t) * params.iobufs, GFP_KERNEL);
    if (!m_iobuf) {
        return (0);
    }

    for (i = 0; i < params.iobufs; i++) {
        m_iobuf[i].buf_ptr = kmalloc(params.iobuf_size, GFP_KERNEL);
        m_iobuf[i].buf_len = params.iobuf_size;
    }

    while (TRUE) {
        flush_signals(current);
        bytes_read = 0;
        err = vqec_ifclient_tuner_recvmsg(params.cp_tuner_id,
                                          m_iobuf,
                                          1,
                                          &bytes_read,
                                          params.timeout);
        if (!bytes_read) {
            msleep(20);  /* sleep for 20 msec */
        } else if (params.sk) {
            udp_tx_pak(params.sk, 
                       m_iobuf[0].buf_ptr,
                       m_iobuf[0].buf_wrlen);
        }
        if (s_reader_task_stop) {
            s_reader_task_stop = FALSE;
            break;
        }
    }

    if (params.sk) {
        kernel_sock_shutdown(params.sk, SHUT_RDWR);
        sock_release(params.sk);
    }
    for (i = 0; i < params.iobufs; i++) {
        kfree(m_iobuf[i].buf_ptr);
    }
    kfree(m_iobuf);
    s_reader_task_running = FALSE;
    return (0);
}

static int32_t vqec_reader_start (test_vqec_reader_params_t *p)
{
    struct completion wait_reader_spawned;
    struct in_addr src, dst;
    uint16_t port;
    int tunerid;
    uint32_t use_iobufs;
    uint32_t use_iobuf_size;
    int32_t use_timeout;
    struct socket *sk;
    start_params_t params;

    if (!p) {
        return (0);
    }

    if (p->use_module_params) {
        /* use the values from the module params */
        src.s_addr = in_aton(src_ipaddr);
        dst.s_addr = in_aton(dest_ipaddr);
        if (!src.s_addr || !dst.s_addr) {
            printk("test_reader: failed to parse src/dest addresses\n");
            return (0);
        }
        port = dest_port;
        tunerid = cp_tuner_id;
        use_iobufs = iobufs;
        use_iobuf_size = iobuf_size;
        use_timeout = timeout;
    } else {
        /* use the values from argument params */
        src.s_addr = p->srcaddr;
        dst.s_addr = p->dstaddr;
        port = p->dstport;
        tunerid = p->tid;
        use_iobufs = p->iobufs;
        use_iobuf_size = p->iobuf_size;
        use_timeout = p->timeout;
        if (!port) {
            /* use default port if none specified */
            port = dest_port;
        }
    }

    if (!src.s_addr || !dst.s_addr) {
        sk = NULL;
    } else {
        sk = udp_tx_sock_create(src, htons(port), dst, 0);
        if (!sk) {
            printk("test_reader: failed to create socket\n");
            return (0);
        }
    }

    if (s_reader_task_running) {
        return (-1);
    }
    s_reader_task_running = TRUE;

    params.sk = sk;
    params.cp_tuner_id = tunerid;
    params.completion = &wait_reader_spawned;
    params.iobufs = use_iobufs;
    params.iobuf_size = use_iobuf_size;
    params.timeout = use_timeout;
    init_completion(&wait_reader_spawned);
    s_reader_task = kernel_thread(vqec_reader_loop, 
                                  &params, 
                                  0);
    if (s_reader_task >= 0) {
        wait_for_completion(&wait_reader_spawned);
    }

    return (0);
}

static void vqec_reader_stop (void)
{
    if (s_reader_task_running) {
        s_reader_task_stop = TRUE;
    }
}

void
vqec_dp_register_reader_callbacks(
    int32_t (*reader_start)(test_vqec_reader_params_t *params),
    void (*reader_stop)(void));

/**---------------------------------------------------------------------------
 * Initialize the kernel module - invoked when the module is inserted.  The
 * parameters supported by the kernel module are described above.
 *
 * This module must be either, requested by the vqec_dp module, or the vqec_dp
 * module must be present in the kernel before this module is loaded.
 *
 * @param[out] int32_t Exit code.
 *---------------------------------------------------------------------------*/
static int32_t __init 
vqec_reader_kmod_init (void)
{
    test_vqec_reader_params_t params;

    memset(&params, 0, sizeof(test_vqec_reader_params_t));
    params.use_module_params = TRUE;

    /* register control callbacks with the vqec_dp module */
    vqec_dp_register_reader_callbacks(vqec_reader_start,
                                      vqec_reader_stop);

    vqec_reader_start(&params);

    return (0);
}


/**---------------------------------------------------------------------------
 * Cleanup the kernel module - nominally invoked from rmmod.
 *---------------------------------------------------------------------------*/
static void __exit
vqec_reader_kmod_cleanup (void)
{
    vqec_reader_stop();

    /* wait for reader task to stop running before exiting */
    while (s_reader_task_running) {
        msleep(10);
    }
}

module_init(vqec_reader_kmod_init);
module_exit(vqec_reader_kmod_cleanup);

MODULE_LICENSE("BSD");
