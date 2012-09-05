/*
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <linux/sockios.h>
#include <sys/un.h>
#include <inttypes.h> 
#include "vqec_stream_output_thread_mgr.h"
#include "vqec_ifclient.h"
#include "vqec_syscfg.h"
#include "vqec_url.h"
#include <utils/vqe_port_macros.h>
#include <utils/strl.h>
#include "vqec_syslog_def.h"
#include "vqec_config_parser.h"
#include "vqec_pthread.h"
#include "vqec_lock_defs.h"
#include "vqec_debug_flags.h"
#include "vqec_debug.h"

#define VQEC_STREAM_OUTPUT_MAX_TUNER VQEC_SYSCFG_MAX_MAX_TUNERS

/*
 * The output stream client calls vqec_ifclient_tuner_recvmsg()
 * to copy the MPEG data into an array of buffers, where
 *   DEFAULT_BUFARRAY_SIZE = number of buffers in the array
 *   DEFAULT_BUF_SIZE      = size of each buffer
 * Each buffer in the array is filled with as many packets as will
 * fit, and then the next buffer in the array is used, until the
 * buffers are full or the timeout period expires.  We would keep that
 * this at 7 MPEG cells so that IP fragmentation and reassembly does
 * not result when sending these buffers to a socket.
 */
#define DEFAULT_BUFARRAY_SIZE   16
#define DEFAULT_BUF_SIZE        (VQEC_SYSCFG_DEFAULT_MAX_PAKSIZE)

#define VQEC_STREAM_OUTPUT_URL_LEN VQEC_MAX_URL_LEN
#define VQEC_STREAM_OUTPUT_TIMEOUT 100
#define VQEC_STREAM_OUTPUT_TXBUF_BYTES    (256 * 1024)
#define VQEC_STREAM_OUTPUT_SNDTIMEO_USECS (20 * 1000)

/*!
 * output_thread is indexed by tuner id internally
 */
static 
vqec_stream_output_thread_t *g_output_thread[VQEC_STREAM_OUTPUT_MAX_TUNER];

static int vqec_stream_output_module_initialized = 0;

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

UT_STATIC vqec_error_t vqec_thread_join(pthread_t thread_id, void **value_ptr) 
{
    if (pthread_join(thread_id, NULL) != 0) {
        return VQEC_ERR_JOINTHREAD;
    }
    return VQEC_OK;
}


vqec_stream_output_thread_t * 
vqec_stream_output_thread_get_by_tuner_id (vqec_tunerid_t tuner_id) 
{

    if (tuner_id < VQEC_STREAM_OUTPUT_MAX_TUNER) {        
        return g_output_thread[ tuner_id ];
    } else { 
        return NULL;
    }
}                                                                        

UT_STATIC int vqec_stream_output_get_ip_address_by_if (char * ifname, 
                                                       char * name, 
                                                       int size) 
{
    struct ifreq ifr;
    int sock;
    
    if (size < INET_ADDRSTRLEN) {
        return 0;
    }
    
    memset(name, 0, size);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    /* Get the interface IP address */ 
    (void)strlcpy(ifr.ifr_name, ifname, INET_ADDRSTRLEN);
    ifr.ifr_addr.sa_family = AF_INET;
    
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        close(sock);
        return 0;
    }
    
    close(sock);
    if (inet_ntop(AF_INET, 
                  &((((struct sockaddr_in *)
                      (&ifr.ifr_addr))->sin_addr).s_addr),
                  name, size) == NULL) {
        return 0;
    } else {
        return 1;
    }
}

void vqec_stream_output_thread_mgr_module_init (void) 
{

    int i;
    
    if (vqec_stream_output_module_initialized) {
        return;
    }

    for (i = 0; i < VQEC_STREAM_OUTPUT_MAX_TUNER; i++) {
        g_output_thread[ i ] = NULL;
    }

    vqec_stream_output_module_initialized = 1;
}

void vqec_stream_output_thread_mgr_module_deinit (void) 
{
    int i;
    vqec_stream_output_thread_t *output;

    for (i = 0; i < VQEC_STREAM_OUTPUT_MAX_TUNER; i++) {
        output = g_output_thread[ i ];
        
        if (!output) {
            continue;
        }

        assert(output);

        /* if the thread is there, set the changed value and wait the thread 
         * exit
         */
        if (output->thread_id != -1) {
            vqec_lock_lock(vqec_stream_output_lock);
            output->exit = 1;
            vqec_lock_unlock(vqec_stream_output_lock);
            if (vqec_thread_join(output->thread_id, NULL) != VQEC_OK) {
                VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT, 
                           "Failed to join output thread\n");
            }
        }

        free(output);
        g_output_thread[ i ] = NULL;
    }
    vqec_stream_output_module_initialized = 0;
}

void vqec_stream_output_thread_mgr_init_startup_streams (char *filename)
{
    vqec_config_t l_config_cfg;
    vqec_config_setting_t *r_setting, *r_setting2;

    vqec_error_t err;

    int num_tuners = 0;
    int cur_idx = 0;
    vqec_config_setting_t *cur_tuner;
    vqec_config_setting_t *temp_setting;
    const char *temp_str = 0;
    char cur_name[VQEC_MAX_NAME_LEN];
    char cur_url[VQEC_MAX_NAME_LEN];
    char cur_if[VQEC_MAX_NAME_LEN];          
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    int max_tuners, temp_int;

    memset(cur_name, 0, VQEC_MAX_NAME_LEN);
    memset(cur_url, 0, VQEC_MAX_NAME_LEN);
    memset(cur_if, 0, VQEC_MAX_NAME_LEN);

    /* Initialize the vqec_config configuration */
    vqec_config_init(&l_config_cfg);

    if (!filename) {
        return;
    }
    
    /* Load the file */
    printf("looking for stream-outputs in config %s..\n", filename);
    if (!vqec_config_read_file(&l_config_cfg, filename)) {
        printf("failed to load configuration file[%s] - (%s)\n",
               filename, l_config_cfg.error_text);
        return;
    } else {
        r_setting = vqec_config_lookup(&l_config_cfg, "tuner_list");
        if (r_setting) {
            /* Read the config file for max_tuners */
            max_tuners = VQEC_SYSCFG_DEFAULT_MAX_TUNERS;
            r_setting2 = vqec_config_lookup(&l_config_cfg, "max_tuners");
            if (r_setting2) {
                temp_int = vqec_config_setting_get_int(r_setting2);
                if ((temp_int >= VQEC_SYSCFG_MIN_MAX_TUNERS) &&
                    (temp_int <= VQEC_SYSCFG_MAX_MAX_TUNERS)) {
                    max_tuners = temp_int;
                }
            }
            num_tuners = vqec_config_setting_length(r_setting);
            if (num_tuners > max_tuners) {
                num_tuners = max_tuners;
            }

            for (cur_idx = 0; cur_idx < num_tuners; cur_idx++) {

                /* get the cur_idxth tuner in the list of tuners */
                cur_tuner = vqec_config_setting_get_elem(r_setting, cur_idx);

                /* get the params from this tuner */
                temp_setting = vqec_config_setting_get_member(cur_tuner,
                                                              "name");
                if (temp_setting) {
                    temp_str = vqec_config_setting_get_string(temp_setting);
                    if (temp_str) {
                        (void)strlcpy(cur_name, temp_str, VQEC_MAX_NAME_LEN);
                    }
                }

                temp_setting =
                    vqec_config_setting_get_member(cur_tuner,
                                                   "stream_output_url");
                if (temp_setting) {
                    temp_str = vqec_config_setting_get_string(temp_setting);
                    if (temp_str) {
                        (void)strlcpy(cur_url, temp_str, VQEC_MAX_NAME_LEN);
                    }
                    temp_setting =
                        vqec_config_setting_get_member(cur_tuner, 
                                                       "stream_output_if");
                    if (temp_setting) {
                        temp_str = vqec_config_setting_get_string(temp_setting);
                        if (temp_str) {
                            (void)strlcpy(cur_if, temp_str, VQEC_MAX_NAME_LEN);
                        }

                        /* start the output stream */
                        err = vqec_stream_output(cur_name, cur_if, cur_url);
                        if (err != VQEC_OK) {
                            snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE ,
                                     "Error in stream output on tuner "
                                     "\"%s\" (%s)\n", cur_name, 
                                     vqec_err2str(err)); 
                            syslog_print(VQEC_ERROR, msg_buf);
                        }
                    }
                }
            }
        }
    }
}

vqec_stream_output_sock_t *
vqec_stream_output_udp_source_create(char * name,
                                     vqec_stream_output_sock_t * mem,
                                     struct in_addr if_address, 
                                     uint16_t       port,
                                     struct in_addr dest_addr,
                                     int blocking,
                                     uint32_t snd_buff_bytes)
{
    vqec_stream_output_sock_t * result =
        mem ? mem : malloc(sizeof(vqec_stream_output_sock_t));
    struct sockaddr_in saddr;

    int on = 1;
    unsigned char loop = 0;
    struct timeval tv;

    if (! result)
        return NULL;
    
    memset(result, 0, sizeof(vqec_stream_output_sock_t));

    if (mem)
        result->caller_provided_mem = 1;

    if ((result->fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
        VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"could not open UDP socket\n");
        if (! result->caller_provided_mem)
            free(result);
        return NULL;
    }
    
    /* remember arguments */
    result->output_if_address = if_address;
    result->port = port;
    result->blocking = blocking;
    result->send_buff_bytes = snd_buff_bytes;
    result->dest_addr = dest_addr;

    memset(&saddr, 0, sizeof(saddr));

    if (setsockopt(result->fd, SOL_SOCKET, SO_REUSEADDR, 
                   &on, sizeof(on)) == -1) {
        VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"setsockopt\n");
        goto bail;
    }

    saddr.sin_addr = if_address;          
    saddr.sin_family = AF_INET;
    saddr.sin_port = 0;
    
    if (bind(result->fd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
        VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"bind");
        goto bail;
    }    

    if (setsockopt(result->fd, IPPROTO_IP, IP_MULTICAST_LOOP, 
                   &loop, sizeof(loop)) == -1) {
        VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"setsockopt\n");
        goto bail;
    }

    if (setsockopt(result->fd, SOL_SOCKET, 
                   SO_TIMESTAMP, &on, sizeof(on)) == -1) {
        VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"setsockopt\n");
        goto bail;
    }

    if (snd_buff_bytes) {
        if (setsockopt(result->fd, SOL_SOCKET,
                       SO_SNDBUF, &snd_buff_bytes, 
                       sizeof(snd_buff_bytes)) == -1) {
            VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"setsockopt\n");
            goto bail;
        }
    }

    if (! blocking) {
        if (fcntl(result->fd, F_SETFL, O_NONBLOCK) == -1) {
            VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"fcntl\n");
            goto bail;
        }
    }

    tv.tv_sec = 0;
    tv.tv_usec = VQEC_STREAM_OUTPUT_SNDTIMEO_USECS;
    if (setsockopt(result->fd, SOL_SOCKET, 
                   SO_SNDTIMEO, &tv, sizeof(tv)) == -1) {
        VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"setsockopt\n");
        goto bail;
    }

    return result;

 bail:
    close(result->fd);
    if (! result->caller_provided_mem)
        free(result);
    return NULL;
  
}

UT_STATIC 
void vqec_stream_output_send_to_proxy (vqec_iobuf_t *buf,
                                       int32_t iobuf_num,
                                       int32_t bytes,
                                       vqec_stream_output_sock_t *sock)
{
    struct sockaddr_in dest_addr;
    int i, cum = 0;

    dest_addr.sin_addr = sock->dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = sock->port;

    for (i = 0; i < iobuf_num && bytes > 0; i++) {
        if (sendto(sock->fd, buf[i].buf_ptr, 
                   buf[i].buf_wrlen, 0,
                   (struct sockaddr *)&dest_addr, 
                   sizeof(struct sockaddr_in)) == -1) {
            sock->output_drops++;
        } else {
            sock->outputs++;
        }
        bytes -= buf[i].buf_wrlen;
        cum += buf[i].buf_wrlen;
    }
}

UT_STATIC vqec_stream_output_sock_t *
vqec_stream_output_create_proxy (in_addr_t intf, 
                                 in_addr_t dest_addr, 
                                 in_port_t port) 
{
    struct in_addr output_if_address;
    uint16_t output_port;
    struct in_addr taddr;
    vqec_stream_output_sock_t *output_sock = NULL;

    memset(&output_if_address, 0, sizeof(struct in_addr));    
    output_if_address.s_addr = intf;

    taddr.s_addr = dest_addr;
    output_port = port;

    if (output_if_address.s_addr != 0) {        
        /* 
         * Create blocking socket -  however send timeout is specified 
         * on the socket so it won't block indefinitely 
         */
        output_sock = vqec_stream_output_udp_source_create("out_socket",
                                                           NULL,
                                                           output_if_address,
                                                           output_port,
                                                           taddr,
                                                           1, 
                                                           VQEC_STREAM_OUTPUT_TXBUF_BYTES);
        if (!output_sock) {
            return (NULL);
        }
        
    }
    return (output_sock);
}

void vqec_stream_output_thread_exit_ul (vqec_stream_output_thread_t *output) 
{

    output->thread_id = -1;
    output->output_prot = 0;
    output->output_dest = INADDR_ANY;
    output->output_port = 0;
    output->output_ifaddr = 0;
    if (output->output_sock && close(output->output_sock->fd) == -1) {
        VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"failed to close socket\n");
    }
    if (output->output_sock && !output->output_sock->caller_provided_mem) {
        free(output->output_sock);
        output->output_sock = NULL;
    }
    output->changed = 0;
    output->exit = 0;
    output->bind = 0;
    output->unbind = 0;
}

/**
 * The following three functions are used to support VQE-C fast fill feature 
 * in IGMP proxy case.
 * 
 * Please note, since the fastfill API needs three callback functions,
 * here, we create three dummy functions to satisfy the API.
 */
static void
fastfill_start_handler (int32_t context_id,
                        vqec_ifclient_fastfill_params_t *params)
{
    if (VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC)) {
        CONSOLE_PRINTF("[IGMP-FASTFILL]: started @ abs_time = %llu ms\n", 
                       TIME_GET_A(msec,get_sys_time()));
    }
}

static void
fastfill_abort_handler (int32_t context_id,
                        vqec_ifclient_fastfill_status_t *status)
{

    if (VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC)) {
        CONSOLE_PRINTF("[IGMP-FASTFILL]: aborted @ abs_time = %llu ms\n", 
                       TIME_GET_A(msec,get_sys_time()));
    }
}

static void
fastfill_done_handler (int32_t context_id,
                       vqec_ifclient_fastfill_status_t *status)
{

    if (VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC)) {
        CONSOLE_PRINTF("[IGMP-FASTFILL]: finished @ abs_time = %llu ms\n", 
                       TIME_GET_A(msec,get_sys_time()));
    }
}

static vqec_ifclient_fastfill_cb_ops_t fastfill_ops =
{
    fastfill_start_handler,
    fastfill_abort_handler,
    fastfill_done_handler
};

UT_STATIC void * vqec_stream_output_thread_loop (void *arg) 
{
    vqec_stream_output_thread_t *output;
    vqec_error_t error;
    int rcvlen = 0, timeout = VQEC_STREAM_OUTPUT_TIMEOUT, i;
    vqec_iobuf_t m_iobuf_array[DEFAULT_BUFARRAY_SIZE];
    char *m_iobuf_buf;
    vqec_stream_output_sock_t *sock;
    struct timespec ts_delay;
    vqec_sdp_handle_t sdp_handle;
    static boolean alerted_rpc_error = FALSE;
    vqec_bind_params_t *bp = NULL;

    ts_delay.tv_sec = 0;
    ts_delay.tv_nsec = 20 * 1000 * 1000;

    assert(arg);
    output = arg;
    
    m_iobuf_buf = 
        calloc(1, sizeof(char) * DEFAULT_BUFARRAY_SIZE * DEFAULT_BUF_SIZE);
    assert(m_iobuf_buf);
    for (i = 0; i < DEFAULT_BUFARRAY_SIZE; i++) {
        m_iobuf_array[i].buf_len = DEFAULT_BUF_SIZE;
        m_iobuf_array[i].buf_ptr = &m_iobuf_buf[DEFAULT_BUF_SIZE * i];
    }

    vqec_lock_lock(vqec_stream_output_lock);
    assert(!output->output_sock);    
    if (output->output_dest) {
        if (!(sock = vqec_stream_output_create_proxy(output->output_ifaddr,
                                                     output->output_dest, 
                                                     output->output_port))) {
            syslog_print(VQEC_ERROR, "Failed to create proxy socket!");
            assert(0);
        }
    } else {
        sock = NULL;
    }
    output->output_sock = sock;
    vqec_lock_unlock(vqec_stream_output_lock);

    while (1) {
        if (output->exit) {
            free(m_iobuf_buf);
            vqec_lock_lock(vqec_stream_output_lock);
            vqec_stream_output_thread_exit_ul(output);
            vqec_lock_unlock(vqec_stream_output_lock);
            return NULL;
        }

        if (output->changed) {
            vqec_lock_lock(vqec_stream_output_lock);
            assert(sock == output->output_sock);
            /*
             * destroy the old sock, and create a new one
             */
            if (sock &&
                (sock->output_if_address.s_addr != output->output_ifaddr ||
                 sock->dest_addr.s_addr != output->output_dest ||
                 sock->port != output->output_port)) {
                if (close(output->output_sock->fd) == -1) {
                    VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,
                               "failed to close socket\n");
                }
                if (!output->output_sock->caller_provided_mem) {
                    free(output->output_sock);
                    output->output_sock = NULL;
                }

                sock = NULL;
            }
            if (!sock && output->output_dest && output->output_port) {
                if (!(sock = 
                      vqec_stream_output_create_proxy(output->
                                                      output_ifaddr,
                                                      output->output_dest, 
                                                      output->
                                                      output_port))) {
                    syslog_print(VQEC_ERROR, 
                                 "Failed to create proxy socket!");
                    assert(0);
                }
            }

            output->output_sock = sock;
            output->changed = 0;
            vqec_lock_unlock(vqec_stream_output_lock);
        }

        if (output->unbind) {
            if (vqec_ifclient_tuner_unbind_chan(output->tuner_id) == VQEC_OK) {
                VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT, "Channel leave SUCCESS\n");
            } else {
                syslog_print(VQEC_ERROR, "Channel leave FAILURE\n");
            } 
            
            vqec_lock_lock(vqec_stream_output_lock);
            output->unbind = 0;
            vqec_lock_unlock(vqec_stream_output_lock);
        }

        if (output->bind) {
            sdp_handle = vqec_ifclient_alloc_sdp_handle_from_url(output->url);
            if (sdp_handle == NULL) {
                syslog_print(VQEC_ERROR, 
                             "failed to acquire cfg handle from url\n");
            }

            /**
             * Since before tuner_bind, we have no information of the 
             * RCC and fastfill flags defined in channel, we can only 
             * "prepare to do fastfill and RCC". VQE-C will hanle tuber
             * bind properly based on the flags in vqec.cfg and CLI, 
             * as well as flag in channel config file. 
             *
             * After tuner_bind, we added debug information to indicate
             * if RCC or fastfill was performed to help debug possible 
             * issues. 
             */
            bp = vqec_ifclient_bind_params_create();
            if (bp != NULL) {
                VQEC_DEBUG(VQEC_DEBUG_RCC, "[IGMP] RCC set in BP\n"); 
                vqec_ifclient_bind_params_enable_fastfill(bp);
                (void)vqec_ifclient_bind_params_set_fastfill_ops(bp, 
                                                                 &fastfill_ops);
                VQEC_DEBUG(VQEC_DEBUG_RCC,
                           "[IGMP] fastfill set in BP \n");
            } else {
                syslog_print(VQEC_ERROR, "Failed to create bind param!");
            }
           
           switch (vqec_ifclient_tuner_bind_chan(output->tuner_id,
                                                 sdp_handle, bp)) {
                case VQEC_OK:
                    VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT, 
                               "Channel join SUCCESS\n");
                    break;
                case VQEC_ERR_DUPLICATECHANNELTUNEREQ:
                    break;
                default:
                    syslog_print(VQEC_ERROR, "Channel join FAILURE\n");
                    break;
           }

           if (VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC)) {
               if (vqec_ifclient_tuner_is_fastfill_enabled(output->tuner_id)) {
                   CONSOLE_PRINTF("[IGMP-FASTFILL]: fastfill was inited\n");
               } else if (vqec_ifclient_tuner_is_rcc_enabled(
                              output->tuner_id)) {
                   CONSOLE_PRINTF("[IGMP-FASTFILL]: RCC was inited\n");
               } else {
                   CONSOLE_PRINTF("[IGMP-FASTFILL]: CC w/o RCC and fastfill\n");
               }
           }

           if (bp != NULL) {
               vqec_ifclient_bind_params_destroy(bp);
               bp = NULL;
           }

            vqec_lock_lock(vqec_stream_output_lock);
            output->bind = 0;
            vqec_lock_unlock(vqec_stream_output_lock);
            vqec_ifclient_free_sdp_handle(sdp_handle);
            sdp_handle=NULL;
        }

        for (i = 0; i < DEFAULT_BUFARRAY_SIZE; i++) {
            m_iobuf_array[i].buf_len = DEFAULT_BUF_SIZE;
            m_iobuf_array[i].buf_ptr = &m_iobuf_buf[DEFAULT_BUF_SIZE * i];
        }	
        error = vqec_ifclient_tuner_recvmsg(output->tuner_id, 
                                            m_iobuf_array, 
                                            DEFAULT_BUFARRAY_SIZE,
                                            &rcvlen,
                                            timeout);
        if (error == VQEC_ERR_NOSUCHTUNER) {
            /* Tuner deleted, just exit the output thread */
            vqec_lock_lock(vqec_stream_output_lock);
            output->exit = 1;
            vqec_lock_unlock(vqec_stream_output_lock);
        } else if (error != VQEC_OK) {
            if ((error == VQEC_ERR_NO_RPC_SUPPORT) && !alerted_rpc_error) {
                syslog_print(VQEC_ERROR,
                             "RPC for vqec_ifclient_tuner_recvmsg() "
                             "is unsupported; check system configuration "
                             "setting for 'deliver_paks_to_user'\n");
                alerted_rpc_error = TRUE;
            }
            if (nanosleep(&ts_delay, NULL) == -1) {
                VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"nanosleep\n");
            }
        } else if (sock) {
            vqec_stream_output_send_to_proxy(m_iobuf_array, 
                                             DEFAULT_BUFARRAY_SIZE,
                                             rcvlen, 
                                             sock);
        }
    }
}

UT_STATIC void vqec_stream_output_thread_bind_chan (vqec_tunerid_t tuner_id,
                                                    char * url) 
{
    vqec_stream_output_thread_t *output;
    
    if (!url) {
        return;
    }

    vqec_lock_lock(vqec_stream_output_lock);
    output = vqec_stream_output_thread_get_by_tuner_id(tuner_id);
    
    if (output) {
        memcpy(output->url, url, VQEC_STREAM_OUTPUT_URL_LEN);
        output->bind = 1;
    }
    vqec_lock_unlock(vqec_stream_output_lock);
}

UT_STATIC void vqec_stream_output_thread_unbind_chan (vqec_tunerid_t tuner_id,
                                                      char * url) 
{
    vqec_stream_output_thread_t *output;
    
    vqec_lock_lock(vqec_stream_output_lock);
    output = vqec_stream_output_thread_get_by_tuner_id(tuner_id);
    
    if (output) {
        output->unbind = 1;
    }
    vqec_lock_unlock(vqec_stream_output_lock);
}

vqec_error_t vqec_stream_output_thread_destroy (vqec_tunerid_t tuner_id) 
{

    vqec_stream_output_thread_t * output;

    vqec_lock_lock(vqec_stream_output_lock);
    output = vqec_stream_output_thread_get_by_tuner_id(tuner_id);

    if (output) {
        g_output_thread[tuner_id] = NULL;
        output->exit = 1;
        vqec_lock_unlock(vqec_stream_output_lock);
        if (vqec_thread_join(output->thread_id, NULL) != VQEC_OK) {
            return VQEC_ERR_JOINTHREAD;
        }
        free(output);
    } else {
        vqec_lock_unlock(vqec_stream_output_lock);
    }
    return VQEC_OK;
}

UT_STATIC void vqec_stream_output_thread_create (vqec_tunerid_t tuner_id,
                                                 short output_prot,
                                                 in_addr_t output_dest,
                                                 in_port_t output_port,
                                                 in_addr_t output_if) 
{
    int i, result;
    vqec_stream_output_thread_t *output;

    vqec_lock_lock(vqec_stream_output_lock);
    /*
     * First need to check if any tuner already stream to this dest
     * Only perform check if streaming to a real destination
     */
    if (output_dest) {
        for (i = 0; i < VQEC_STREAM_OUTPUT_MAX_TUNER; i++) {
            output = vqec_stream_output_thread_get_by_tuner_id(i);

            if (!output) {
                continue;        
            }

            assert(output);
            if (output->output_prot == output_prot &&
                output->output_dest == output_dest &&
                output->output_port == output_port) {
                assert(output->thread_id != -1);
                if (fflush(stdout) != 0) {
                    VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"fflush\n");
               }
                vqec_lock_unlock(vqec_stream_output_lock);
                return;
            }
        }
    }

    output = vqec_stream_output_thread_get_by_tuner_id(tuner_id);
    if (!output) {
        output = 
            (vqec_stream_output_thread_t *)
            calloc(1,
                   (sizeof(vqec_stream_output_thread_t)));
        if (!output) {
            VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"malloc failure in output\n");
            vqec_lock_unlock(vqec_stream_output_lock);
            return;
        }       
        
        output->tuner_id = tuner_id;
        output->thread_id = -1;        
        /* remember the params */     
        output->output_ifaddr = 0;
        output->output_prot = 0;
        output->output_dest = INADDR_ANY;
        output->output_port = 0;
        output->output_sock = NULL;
        output->changed = 0;
        
        g_output_thread[ tuner_id ] = output;
    }

    assert(output);

    output->tuner_id = tuner_id;
    output->output_prot = output_prot;
    output->output_dest = output_dest;
    output->output_port = output_port;
    output->output_ifaddr = output_if;

    
    if (output->thread_id == -1) {

        /* create pthread, and elevate priority to real-time. */
        if (!(result = vqec_pthread_create(&output->thread_id, 
                                           vqec_stream_output_thread_loop, output))) {
                        
            (void)vqec_pthread_set_priosched(output->thread_id);
        }
    }

    output->changed = 1;

    vqec_lock_unlock(vqec_stream_output_lock);
}

vqec_error_t
vqec_stream_output (char *name,
                    char *output_if,
                    char *url) 
{
    vqec_protocol_t prot = VQEC_PROTOCOL_UNKNOWN;
    in_addr_t addr = 0;
    in_port_t port = 0;
    char temp[ INET_ADDRSTRLEN ];
    struct in_addr temp_addr;
    vqec_tunerid_t tuner_id;

    if (!name) {
        return VQEC_ERR_NOSUCHTUNER;
    }

    tuner_id = vqec_ifclient_tuner_get_id_by_name(name);
    if (tuner_id < 0) {
        return VQEC_ERR_NOSUCHTUNER;
    }

    /* Null output case */
    if (!strncmp(output_if, "null", 4)) {
        memset(&temp_addr, 0, sizeof(temp_addr));

    /* Non-Null output case */
    } else {
        if (!vqec_stream_output_get_ip_address_by_if(output_if,
                                                     temp, 
                                                     INET_ADDRSTRLEN)) {
            return VQEC_ERR_INVALIDARGS;
        }

        if (!inet_aton(temp, &temp_addr)) {
            return VQEC_ERR_INVALIDARGS;
        }

        if (!url || !vqec_url_parse(url, 
                                    &prot,
                                    &addr,
                                    &port)) {
            return VQEC_ERR_CHANNELPARSE;
        }

        if (!addr) {
            if (vqec_stream_output_thread_destroy(tuner_id)!=VQEC_OK) {
                return VQEC_ERR_DESTROYTHREAD;
            }
            return VQEC_OK;
        } 
    }

    if (fflush(stdout) != 0) {
        VQEC_DEBUG(VQEC_DEBUG_STREAM_OUTPUT,"fflush\n");
    }

    if (tuner_id < VQEC_STREAM_OUTPUT_MAX_TUNER) {
        vqec_stream_output_thread_create(tuner_id, prot, addr, port,
                                         temp_addr.s_addr);    
    }    
    return VQEC_OK;
}

/*
 * Call back function when there is a channel change from remote control
 * captured by igmp module.
 */
void 
vqec_stream_output_channel_join_call_back_func (char *url, 
                                                vqec_tunerid_t tuner_id,
                                                in_addr_t output_ifaddr,
                                                in_addr_t output_dest,
                                                in_port_t output_port,
                                                void * call_back_data) 
{
    vqec_stream_output_thread_create(tuner_id,
                                     VQEC_PROTOCOL_RTP,
                                     output_dest, 
                                     output_port,
                                     output_ifaddr);
    vqec_stream_output_thread_bind_chan(tuner_id, url);
}

/*
 * Call back function when there is a channel leave from remote control
 * captured by igmp module.
 */
void 
vqec_stream_output_channel_leave_call_back_func (char *url, 
                                                 vqec_tunerid_t tuner_id,
                                                 in_addr_t output_ifaddr,
                                                 in_addr_t output_dest,
                                                 in_port_t output_port,
                                                 void *call_back_data)
{
    vqec_stream_output_thread_unbind_chan(tuner_id, url);
    //vqec_stream_output_thread_destroy(tuner_id);
}

void 
vqec_stream_output_mgr_show_stats (vqec_tunerid_t id) 
{
    vqec_stream_output_thread_t *output;
    char name[VQEC_MAX_TUNER_NAMESTR_LEN];
    char url[VQEC_MAX_URL_LEN];

    vqec_lock_lock(vqec_stream_output_lock);

    name[0] = '\0';
    if (vqec_ifclient_tuner_get_name_by_id(
            id, name, VQEC_MAX_TUNER_NAMESTR_LEN) == VQEC_OK) {

        output = vqec_stream_output_thread_get_by_tuner_id(id);
        if (output && output->output_sock) {
            (void)vqec_url_build(output->output_prot,
                                 output->output_dest,
                                 output->output_port,
                                 url,
                                 VQEC_MAX_URL_LEN);

            CONSOLE_PRINTF("Tuner name:                 %s\n", name);
            CONSOLE_PRINTF(" destination URL:           %s\n", url);
            CONSOLE_PRINTF(" packets sent:              %lld\n"
                           " packets dropped:           %lld\n",
                           output->output_sock->outputs,
                           output->output_sock->output_drops);
        } else if (output) {
            CONSOLE_PRINTF("Tuner name:                 %s\n", name);
            CONSOLE_PRINTF(" destination URL:           null\n");
        }
    }

    vqec_lock_unlock(vqec_stream_output_lock);
}

void 
vqec_stream_output_mgr_clear_stats (vqec_tunerid_t id) 
{
    vqec_stream_output_thread_t *output;

    vqec_lock_lock(vqec_stream_output_lock);

    output = vqec_stream_output_thread_get_by_tuner_id(id);
    if (output && output->output_sock) {
        output->output_sock->outputs = 0;
        output->output_sock->output_drops = 0;
    }

    vqec_lock_unlock(vqec_stream_output_lock);
}
