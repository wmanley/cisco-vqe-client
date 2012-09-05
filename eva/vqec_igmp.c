/*
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "vqec_recv_socket.h"
#include "vqec_event.h"
#include <string.h>
#include <netinet/igmp.h>
#include <netinet/ip.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/if_ether.h>
#include "vqec_debug.h"
#include "vqec_igmp.h"
#include "vqec_url.h"
#include <cfg/cfgapi.h>
#include "vqec_ifclient.h"
#include "vqec_syscfg.h"
#include <utils/vam_util.h>
#include "vqec_igmpv3.h"
#include "vqec_syslog_def.h"
#include "vqec_assert_macros.h"
#include "vqec_lock_defs.h"
#include "raw_socket.h"
#include "socket_filter.h"
#include <net/ethernet.h>
#include "utils/vam_util.h"
#include <utils/strl.h>
#include "vqec_stream_output_thread_mgr.h"
#include "vqec_config_parser.h"
#include <linux/if_packet.h>

#define VQEC_IGMP_MAX_TUNER VQEC_SYSCFG_MAX_MAX_TUNERS

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

static vqec_igmp_t *s_igmp[VQEC_IGMP_MAX_TUNER];
static boolean s_igmp_module_initialized = FALSE;
UT_STATIC void vqec_igmp_do_join(vqec_tunerid_t tuner_id,
                                  struct in_addr group_ip);

UT_STATIC void vqec_igmp_do_leave(vqec_tunerid_t tuner_id,
                                   struct in_addr group_ip);

UT_STATIC vqec_igmp_t * vqec_igmp_get_entry_by_tuner_id (vqec_tunerid_t id) 
{

    if (id >= VQEC_IGMP_MAX_TUNER) {
        return NULL;
    }

    if (!s_igmp_module_initialized) {
        return NULL;
    }
    return s_igmp[ id ];
}

void vqec_igmp_filter_attach (vqec_recv_sock_t * sock, 
                              unsigned int stb_ip_addr) 
{
    struct in_addr cfg_src_addr;
    
    if (!sock) {
        return;
    }
    cfg_src_addr.s_addr = INADDR_ANY;
   
    if (attach_igmp_filter(sock->fd, stb_ip_addr) == -1) {
        syslog_print(VQEC_ERROR,"failed to attach igmp filter to socket");
    }
}

UT_STATIC channel_cfg_t * vqec_igmp_channel_search (struct in_addr group_ip)
{
    uint16_t total_channels;
    channel_cfg_t * channel = NULL;
    int i;
    struct in_addr search_ip;
    char temp[INET_ADDRSTRLEN];

    search_ip.s_addr = group_ip.s_addr;

    if (!inet_ntop(AF_INET, &search_ip.s_addr, temp, INET_ADDRSTRLEN)) {
        temp[0] = '\0';
    }

    total_channels = cfg_get_total_num_channels();            
    for (i = 0; i < total_channels; i++) {
        /* Retrieve both input and output info based on the handle */
        channel = cfg_get_channel_cfg_from_idx(i);
        VQEC_ASSERT(channel);
        if (search_ip.s_addr == channel->original_source_addr.s_addr) {
            VQEC_DEBUG(VQEC_DEBUG_IGMP,
                       "Channel %s found \n", temp);
            return channel;
        }
    }
    VQEC_DEBUG(VQEC_DEBUG_IGMP,
               "Channel %s not found \n", temp);
    return NULL;
}

UT_STATIC uint32_t vqec_igmp_process_igmp_pak (vqec_tunerid_t tuner_id,
                                               char *buf, int recv_len)
{
    struct ip * ip;
    struct igmp *igmp;
    igmp_v3_report_type *igmpv3rp;
    igmp_group_record *igmpv3gr;
    int ipdatalen, iphdrlen, igmpdatalen, _gr_;
 
    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];
    char group[INET_ADDRSTRLEN];

    struct in_addr src_ip;
    struct in_addr dest_ip;
    struct in_addr group_ip;
    char temp[ INET_ADDRSTRLEN ];
    vqec_igmp_t * vqec_igmp;
   
    if (!buf || recv_len <= 0) {
        return VQEC_ERR_INVALIDARGS;
    }
    
    vqec_igmp = vqec_igmp_get_entry_by_tuner_id(tuner_id);
    if (!vqec_igmp || !vqec_igmp->proxy) {
        return VQEC_ERR_INVALIDARGS;
    }

    ip = (struct ip *) buf;
    src_ip = ip->ip_src;
    dest_ip = ip->ip_dst;
       
    if (!inet_ntop(AF_INET, &(ip->ip_src.s_addr), src, INET_ADDRSTRLEN)) {
        src[0] = '\0';
    }
    if (!inet_ntop(AF_INET, &(ip->ip_dst.s_addr), dst, INET_ADDRSTRLEN)) {
        dst[0] = '\0';
    }
    
    /* This is most likely a message from the kernel indicating that
     * a new src grp pair message has arrived and so, it would be
     * necessary to install a route into the kernel for this.
     */
    if (ip->ip_p == 0) {
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "kernel request not accurate");
        return VQEC_ERR_UNKNOWN;        
    }
    
    iphdrlen = ip->ip_hl << 2;
    ipdatalen = ntohs(ip->ip_len) - iphdrlen;
    igmp = (struct igmp *) &buf[iphdrlen];
    group_ip = igmp->igmp_group;
    if (!inet_ntop(AF_INET, &(igmp->igmp_group.s_addr), group, 
                   INET_ADDRSTRLEN)) {
        group[0] = '\0';
    }
    igmpdatalen = ipdatalen - IGMP_MINLEN;

    VQEC_DEBUG(VQEC_DEBUG_IGMP,
               "\nReceived igmp packet src %s dst %s group %s \n",
               src, dst, group);        
    
    /* check if we enabled proxy or not */
    if (!vqec_igmp->proxy) {
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "\nDrop igmp packet, proxy is not enabled \n");
        return VQEC_ERR_INVALIDARGS;
    }
    
    if (vqec_igmp->stb_addr.s_addr == 0) {
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "\nDrop igmp packet, stb addr is not configured\n");
        return VQEC_ERR_UNKNOWN;
    }

    if (vqec_igmp->stb_addr.s_addr != src_ip.s_addr) {
        if (!inet_ntop(AF_INET, &vqec_igmp->stb_addr.s_addr, 
                       temp, INET_ADDRSTRLEN)) {
            temp[0] = '\0';
        }

        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "\nDrop igmp packet form other stb, our stb addr is "
                   "%s \n", temp);
        return VQEC_ERR_UNKNOWN;
    }

    switch (igmp->igmp_type) {
    case IGMP_MEMBERSHIP_QUERY:
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "received IGMP_MEMBERSHIP_QUERY packet from %s to %s "
                   "for group %s\n", src, dst, group);
        break;
    case IGMP_V1_MEMBERSHIP_REPORT:
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "received IGMP_v1_MEMBERSHIP_REPORT packet from %s to %s "
                   "for group %s\n", src, dst, group);
        break;
        
    case IGMP_V2_MEMBERSHIP_REPORT:
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "received IGMP_v2_MEMBERSHIP_REPORT packet from %s to %s "
                   "for group %s\n", src, dst, group);
        vqec_igmp_do_join(tuner_id, group_ip);
        break;
    case IGMP_V3_REPORT_TYPE:
        igmpv3rp = (igmp_v3_report_type *)(buf + iphdrlen);
        igmpv3gr = igmpv3rp->group;
              
        for (_gr_=0; _gr_ < ntohs(igmpv3rp->group_count); _gr_++) {
            group_ip.s_addr = igmpv3gr->group;
            if (!inet_ntop(AF_INET, &group_ip.s_addr, group, 
                           INET_ADDRSTRLEN)) {
                group[0] = '\0';
            }

            VQEC_DEBUG(VQEC_DEBUG_IGMP,
                       "received IGMP_v3_MEMBERSHIP_REPORT packet "
                       "from %s to %s "
                       "for group %s type %d group count %d "
                       "source count %d\n", 
                       src, dst, group, igmpv3gr->type,
                       ntohs(igmpv3rp->group_count),
                       igmpv3gr->source_count);
            
            switch (igmpv3gr->type) {
                /* roughly eqivalent to IGMP V2 join */
            case CHANGE_TO_INCLUDE_MODE:
                /*
                  Normally, when user wants to join a channel, it
                  sends v3 report include mode.  when user wants to
                  leave a channel, it sends v3 report exclude mode.
                  But it's also valid, if in exclude mode, with source
                  count 0, that's in fact a join to all sources!! if
                  in include mode, with souce count 0, that means
                  leave all souces!!!
                */
                if (igmpv3gr->source_count) {
                    vqec_igmp_do_join(tuner_id, group_ip);
                } else {
                    vqec_igmp_do_leave(tuner_id, group_ip);
                }
                break;
                
                /* roughly eqivalent to IGMP V2 leave */
            case CHANGE_TO_EXCLUDE_MODE:
                /*
                  Normally, when user wants to join a channel, it
                  sends v3 report include mode.  when user wants to
                  leave a channel, it sends v3 report exclude mode.
                  But it's also valid, if in exclude mode, with source
                  count 0, that's in fact a join to all sources!! if
                  in include mode, with souce count 0, that means
                  leave all souces!!!
                */
                if (igmpv3gr->source_count) {
                    vqec_igmp_do_leave(tuner_id, group_ip);
                } else {
                    vqec_igmp_do_join(tuner_id, group_ip);
                }
                break;
                
                /* unique to V3, not seen from STB, ignore for now */
            case ALLOW_NEW_SOURCES:
                vqec_igmp_do_join(tuner_id, group_ip);
                break;
                
                /* unique to V3, not seen from STB, ignore for now */
            case BLOCK_OLD_SOURCES:
                break;
                
            default:
                VQEC_DEBUG(VQEC_DEBUG_IGMP, 
                           "Got igmp v3 packet with unknown type\n");
                break;
            }
            
            igmpv3gr = igmpv3gr + 
                (ntohs(igmpv3gr->source_count)-1)*
                (sizeof(struct in_addr)) + 
                sizeof(igmp_group_record);
        }
        
        break;
    case IGMP_V2_LEAVE_GROUP:
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "received IGMP_V2_LEAVE_GROUP packet from %s to %s for "
                   "group %s\n", src, dst, group);
        vqec_igmp_do_leave(tuner_id, group_ip);
        break;
        
    case IGMP_DVMRP:
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "received IGMP_DVMRP packet from %s to %s for group %s\n", 
                   src, dst, group);                   
        break;
    case IGMP_PIM:
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "received IGMP_PIM packet from %s to %s for group %s\n", 
                   src, dst, group);
        break;
    case IGMP_TRACE:
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "received IGMP_TRACE packet from %s to %s for group %s\n", 
                   src, dst, group);
        break;
    case IGMP_MTRACE_RESP:
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "received IGMP_MTRACE_RESP packet from %s to %s for "
                   "group %s\n", src, dst, group);
        break;
    case IGMP_MTRACE:
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "received IGMP_MTRACE packet from %s to %s for group %s\n", 
                   src, dst, group);
        break;
    case IGMP_MAX_HOST_REPORT_DELAY:
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "received IGMP_MAX_HOST_REPORT_DELAY packet from %s to %s "
                   "for group %s\n", src, dst, group);
        break;
    default:
        VQEC_DEBUG(VQEC_DEBUG_IGMP,
                   "ignoring unknown IGMP message type %x from %s to %s for "
                   "group %s", igmp->igmp_type, src, dst, group);
        break;
    }            

    return VQEC_OK;
}

UT_STATIC void vqec_igmp_event_handler_internal (int fd, short event, 
                                                 vqec_tunerid_t tuner_id) 
{
    int buf_len = VQEC_SYSCFG_DEFAULT_MAX_PAKSIZE;
    char buf[VQEC_SYSCFG_DEFAULT_MAX_PAKSIZE];
    int recv_len;
    vqec_igmp_t * vqec_igmp;
    uint32_t rv;

    /*
     * Try to process all the packets in this socket. Exit when there
     * is no packet to process. Also, make sure fd is non blocking.
     */
    while (1) {
        vqec_igmp = vqec_igmp_get_entry_by_tuner_id(tuner_id);
        if (!vqec_igmp || !vqec_igmp->proxy) {
            return;
        }
        recv_len = recvfrom(fd, buf, buf_len, 0, 0, 0);
        if (recv_len <= 0) {
            return;
        }
        /* 
         * Note that we have a SOCK_RAW socket type and an AF_PACKET Domain
         * which leads to the socket buffer being populated by the packet
         * including the ethernet header. We need to strip off this ethernet
         * header when attempting to process the igmp packet.
         */
        rv = vqec_igmp_process_igmp_pak(tuner_id, &buf[ ETHER_HDR_LEN ], 
                                        recv_len-ETHER_HDR_LEN);
    }
}

void vqec_igmp_event_handler (const vqec_event_t * const evptr,
                              int fd, short event, void *event_params) 
{

    vqec_igmp_t * vqec_igmp = event_params;
    VQEC_ASSERT(vqec_igmp);

    vqec_igmp_event_handler_internal(fd, event, vqec_igmp->tuner_id);
}

void vqec_igmp_sock_destroy (vqec_recv_sock_t * sock)
{

    assert(sock->ref_count > 0);
    sock->ref_count--;

    if (sock->ref_count > 0) {
        return;
    }

    if (close(sock->fd) == -1) {
        syslog_print(VQEC_SYSTEM_ERROR, "close ", strerror(errno));
    }

    free(sock);
}

/* Helper function to bind a socket file descriptor to a Ethernet interface */
boolean bind_to_if (int sock_fd, char *if_name)
{
    struct ifreq ifr;
    struct sockaddr_ll sa;

    strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
    if (ioctl(sock_fd, SIOCGIFINDEX, &ifr) < 0) {
        syslog_print(VQEC_SYSTEM_ERROR, "ioctl SIOCGIFINDEX", strerror(errno));
        return (FALSE);
    }

    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_IP);
    sa.sll_ifindex = ifr.ifr_ifindex;
    if (bind(sock_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        syslog_print(VQEC_SYSTEM_ERROR, "bind", strerror(errno));
        return (FALSE);
    }
    return TRUE;
} 

/* 
 * This function creates the socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP) socket
 * The purpose of the which is to sniff IGMPv1, IGMPv2 and IGMPv3 packets.
 * We currently do not support IGMPv1 but we want to display a message that
 * IGMPv1 report was received and consequently need to enable the sniffing of
 * IGMPv1 packets. 
 */
void vqec_igmp_sock_create (char * name, char * if_name,
                            in_addr_t output_if, vqec_igmp_t * ptr_vqec_igmp_t)
 
{
    vqec_recv_sock_t * result = malloc(sizeof(vqec_recv_sock_t));
    struct sockaddr_in saddr;
    int on = 1;
    struct in_addr cfg_src_addr;
    struct ifreq ifr;
    boolean retval;

    if (!result) {
        return;
    }
   
    cfg_src_addr.s_addr = INADDR_ANY;
    memset(result, 0, sizeof(vqec_recv_sock_t));

    /* We will use this socket for the sniffing */
    if ((result->fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) == -1) {
        syslog_print(VQEC_SYSTEM_ERROR, "socket (sniffer)", strerror(errno));
        goto bail;
    }

    retval = bind_to_if(result->fd, if_name);
    if (!retval) {
        goto bail;
    }
    strncpy(ifr.ifr_name, if_name , IF_NAMESIZE);

    /* Set the network card interface to multicast promiscuous mode */
    if (ioctl(result->fd, SIOCGIFFLAGS, &ifr ) == -1 ) {
        syslog_print(VQEC_SYSTEM_ERROR,"Can't get interface flags",
                     strerror(errno));
        goto bail;
    }
    ifr.ifr_flags |=  IFF_ALLMULTI;

    if (ioctl(result->fd, SIOCSIFFLAGS, &ifr) == -1 ) {
        syslog_print(VQEC_SYSTEM_ERROR, "ioctl (sniffer)", strerror(errno));
    }
    
    if (setsockopt(result->fd, SOL_SOCKET, SO_REUSEADDR, 
                   &on, sizeof(on)) == -1) {
        syslog_print(VQEC_SYSTEM_ERROR, 
                     "setsockopt SO_REUSEADDR (sniffer)", strerror(errno));
        goto bail;
    }

    /*
     * make sure socket is non blocking.
     */
    if (fcntl(result->fd, F_SETFL, O_NONBLOCK) == -1) {
        syslog_print(VQEC_SYSTEM_ERROR, "fcntl (sniffer)", strerror(errno));
        goto bail;
    }

    /* Attach the BPF to the socket */
    vqec_igmp_filter_attach(result, htonl(ptr_vqec_igmp_t->stb_addr.s_addr));

    result->ref_count = 1;

    result->rcv_if_address.s_addr = output_if;

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_addr.s_addr = output_if;
    saddr.sin_family = AF_INET;
    saddr.sin_port = 0;
    
    if (setsockopt(result->fd, SOL_SOCKET, SO_REUSEADDR, 
                   &on, sizeof(on)) == -1) {
        syslog_print(VQEC_SYSTEM_ERROR, 
                     "setsockopt SO_REUSEADDR (sniffer)", strerror(errno));
        goto bail;
    }

    /* make sure socket is non blocking. */
    if (fcntl(result->fd, F_SETFL, O_NONBLOCK) == -1) {
        syslog_print(VQEC_SYSTEM_ERROR, "fcntl (sniffer)", strerror(errno));
        goto bail;
    }
    
    result->ref_count = 1;
    ptr_vqec_igmp_t->igmp_sock = result;
    return;

bail:
    if (result) {
        close(result->fd);
        free(result);
    }
    return;      
}

/* Function that creates IGMP proxy related sockets, events and memberships */
boolean vqec_igmp_create (vqec_tunerid_t tuner_id, char *if_name,
                          in_addr_t output_if) 
{
    
    vqec_igmp_t * vqec_igmp;

    vqec_igmp = vqec_igmp_get_entry_by_tuner_id(tuner_id);
    if (!vqec_igmp) {
        return FALSE;
    }

    if (vqec_igmp->proxy) {
        vqec_igmp_destroy(tuner_id);
        vqec_igmp->proxy = FALSE;
    }

    if (vqec_igmp->igmp_ev || vqec_igmp->igmp_sock) {
        /* Should already be cleaned. Something is wrong, return */
        syslog_print(VQEC_ERROR, "vqec_igmp is not clean in vqec_igmp_create");
        return FALSE;
    }

    /* Create sockets and populate the vqec_igmp_t */
    vqec_igmp_sock_create("IGMP", if_name, output_if, vqec_igmp);
   
    /* Create an event and associate a callback with that event,
     * and add that event to the list of events using "event_start"
     */
    if (vqec_igmp->igmp_sock) {
        if (!vqec_event_create(&vqec_igmp->igmp_ev,
                               VQEC_EVTYPE_FD,
                               VQEC_EV_RECURRING | VQEC_EV_READ,
                               vqec_igmp_event_handler,
                               vqec_igmp->igmp_sock->fd,                     
                               vqec_igmp) ||
            !vqec_event_start(vqec_igmp->igmp_ev, NULL)) {
            vqec_event_destroy(&vqec_igmp->igmp_ev);
            vqec_igmp_sock_destroy(vqec_igmp->igmp_sock);
            VQEC_DEBUG(VQEC_DEBUG_IGMP, "failed to add igmp event");
            return FALSE;
        }
    } else {
        VQEC_DEBUG(VQEC_DEBUG_IGMP, "failed to create igmp_sock\n");
        return FALSE;
    }
    
    vqec_igmp->proxy = TRUE;
    return TRUE;
}

void vqec_igmp_destroy (vqec_tunerid_t tuner_id) {
    vqec_igmp_t * vqec_igmp;

    vqec_igmp = vqec_igmp_get_entry_by_tuner_id(tuner_id);
    if (!vqec_igmp) {
        return;
    }

    if (vqec_igmp->proxy) {
        if (vqec_igmp->igmp_ev) {
            vqec_event_destroy(&vqec_igmp->igmp_ev);
            vqec_igmp->igmp_ev = NULL;
        }

        if (vqec_igmp->igmp_sock) {
            vqec_recv_sock_destroy(vqec_igmp->igmp_sock);
            vqec_igmp->igmp_sock = NULL;
        }
    }
    vqec_igmp->tuner_id = VQEC_TUNERID_INVALID;
    vqec_igmp->proxy = FALSE;
}

UT_STATIC void vqec_igmp_do_join (vqec_tunerid_t tuner_id, 
                                  struct in_addr group_ip) 
{
    channel_cfg_t * channel;
    
    char *chan;
    int len = VQEC_MAX_URL_LEN;

    vqec_igmp_t *vqec_igmp;

    vqec_igmp = vqec_igmp_get_entry_by_tuner_id(tuner_id);
    if (!vqec_igmp) {
        return;
    }
    
    channel = vqec_igmp_channel_search(group_ip);
    chan = malloc(VQEC_MAX_URL_LEN);
    if (!chan) {
        return;
    }
    memset(chan, 0, VQEC_MAX_URL_LEN);   

    if (channel) {

        if (!vqec_url_build(VQEC_PROTOCOL_RTP, 
                            channel->original_source_addr.s_addr,
                            channel->original_source_port,
                            chan,
                            len)) {
            VQEC_DEBUG(VQEC_DEBUG_IGMP,
                       "Channel url build FAILED\n");        
            free(chan);
            return;
        }
        
        if (vqec_igmp->channel_join_call_back_func) {
            vqec_igmp->channel_join_call_back_func(chan,
                                                   vqec_igmp->tuner_id,
                                                   vqec_igmp->output_ifaddr,
                                                   group_ip.s_addr,
                                                   channel->original_source_port,
                                                   vqec_igmp->channel_join_call_back_data);
        }
    }
    free(chan);
}

UT_STATIC void vqec_igmp_do_leave (vqec_tunerid_t tuner_id, 
                                   struct in_addr group_ip) 
{
    channel_cfg_t * channel;
    char *chan;
    int len = VQEC_MAX_URL_LEN;
    vqec_igmp_t *vqec_igmp;

    vqec_igmp = vqec_igmp_get_entry_by_tuner_id(tuner_id);
    if (!vqec_igmp) {
        return;
    }

    /* Try to send igmp leave */
    channel = vqec_igmp_channel_search(group_ip);

    chan = malloc(VQEC_MAX_URL_LEN);
    if (!chan) {
        return;
    }
    memset(chan, 0, VQEC_MAX_URL_LEN);
    if (channel) {

        if (!vqec_url_build(VQEC_PROTOCOL_RTP, 
                              channel->original_source_addr.s_addr,
                              channel->original_source_port,
                              chan,
                              len)) {
            VQEC_DEBUG(VQEC_DEBUG_IGMP,
                       "Channel url build FAILED\n");
            free(chan);
            return;
        }

        if (vqec_igmp->channel_leave_call_back_func) {
            vqec_igmp->channel_leave_call_back_func(chan,
                                                    vqec_igmp->tuner_id,
                                                    vqec_igmp->output_ifaddr,
                                                    group_ip.s_addr,
                                                    channel->original_source_port,
                                                    vqec_igmp->channel_leave_call_back_data);
        }
    }
    free(chan);
}

boolean vqec_igmp_module_init (void) 
{
    vqec_syscfg_t cfg;
    int i;
    vqec_syscfg_get(&cfg);
    
    if (s_igmp_module_initialized) {
        return TRUE;
    }

    if (!cfg.max_tuners) {
        return FALSE;
    }

    for (i = 0; i < cfg.max_tuners; i++) {
        s_igmp[ i ] = (vqec_igmp_t *)calloc(1,
                                            (sizeof(vqec_igmp_t)));
        if (!s_igmp[i]) {
            vqec_igmp_module_deinit();
            return FALSE;
        }

        s_igmp[ i ]->tuner_id = i;
        /* 
         * Init the s_igmp[i]->proxy state to FALSE. It may be changed
         * later if igmp_proxy_init is called and/or the proxy for this
         * tuner is initialized from the config file
         */
        s_igmp[ i ]->proxy = FALSE;
    }

    s_igmp_module_initialized = TRUE;
    return TRUE;
}

void vqec_igmp_module_deinit (void) 
{
    vqec_syscfg_t cfg;
    int i;
    vqec_igmp_t * igmp;

    vqec_syscfg_get(&cfg);

    if (!s_igmp_module_initialized) {
        return;
    }

    if (!cfg.max_tuners) {
        return;
    }
    
    for (i = 0; i < cfg.max_tuners; i++) {
        igmp = s_igmp[ i ];

        if (igmp) {
            if (igmp->igmp_ev) {
                vqec_event_destroy(&igmp->igmp_ev);
            }
            
            if (igmp->igmp_sock) {
                vqec_recv_sock_destroy(igmp->igmp_sock);
            }
            free(igmp);
        }
        s_igmp[ i ] = NULL;
    }
    s_igmp_module_initialized = FALSE;
}

/*
 * if stb_addr is 0, means destroy igmp entry.
 */
vqec_error_t
vqec_proxy_igmp_join (const char *name,
                      char *if_name,
                      in_addr_t stb_addr,
                      void (*channel_join_call_back_func),
                      void * channel_join_call_back_data,
                      void (*channel_leave_call_back_func),
                      void * channel_leave_call_back_data) 
{    
    vqec_tunerid_t tuner_id;
    vqec_igmp_t * vqec_igmp;
    char temp[INET_ADDRSTRLEN];
    struct in_addr temp_addr;

    tuner_id = vqec_ifclient_tuner_get_id_by_name(name);
    if (tuner_id == VQEC_TUNERID_INVALID) {
        return VQEC_ERR_NOSUCHTUNER;
    }

    vqec_lock_lock(vqec_g_lock);

    if(!get_ip_address_by_if(if_name, temp, INET_ADDRSTRLEN)) {
        vqec_lock_unlock(vqec_g_lock);
        return VQEC_ERR_INVALIDARGS;
    }

    vqec_igmp = vqec_igmp_get_entry_by_tuner_id(tuner_id);
    if (!vqec_igmp) {
        vqec_lock_unlock(vqec_g_lock);
        return VQEC_ERR_INVALIDARGS;
    }

    vqec_igmp->tuner_id = tuner_id;
    vqec_igmp->stb_addr.s_addr = stb_addr;
    if (!inet_aton(temp, &temp_addr)) {
        vqec_lock_unlock(vqec_g_lock);
        return VQEC_ERR_INVALIDARGS;        
    }
    vqec_igmp->output_ifaddr = temp_addr.s_addr;
    vqec_igmp->channel_join_call_back_func = channel_join_call_back_func;
    vqec_igmp->channel_join_call_back_data = channel_join_call_back_data;
    vqec_igmp->channel_leave_call_back_func = channel_leave_call_back_func;
    vqec_igmp->channel_leave_call_back_data = channel_leave_call_back_data;

    if (stb_addr != 0) {
        if (vqec_igmp_create(tuner_id, if_name, vqec_igmp->output_ifaddr)) {
            vqec_igmp->proxy = TRUE;
            (void) strlcpy(vqec_igmp->output_if_name, if_name, 
                           strlen(if_name) + 1);
        } else {
            vqec_lock_unlock(vqec_g_lock);
            return VQEC_ERR_UNKNOWN;
        }
    } else {
        vqec_igmp_destroy(tuner_id);
        vqec_igmp->proxy = FALSE;
        vqec_lock_unlock(vqec_g_lock);
        return VQEC_OK_IGMP_PROXY_DISABLED;
    }
    vqec_lock_unlock(vqec_g_lock);
    return VQEC_OK;
}

/* Show the IGMP proxy related stats for the tuner */
void vqec_cmd_show_proxy_igmp_stats (vqec_tunerid_t id)
{
    char name[VQEC_MAX_TUNER_NAMESTR_LEN];
    vqec_igmp_t *vqec_igmp;
    struct in_addr in;
    char temp[INET_ADDRSTRLEN];
    char url[VQEC_MAX_URL_LEN];
    vqec_stream_output_thread_t *output;

    /* Get Tuner name by ID */
    name[0] = '\0';
    if (vqec_ifclient_tuner_get_name_by_id(
            id, name, VQEC_MAX_TUNER_NAMESTR_LEN) != VQEC_OK) {
         return;
    }
    vqec_lock_lock(vqec_g_lock);
    if ((vqec_igmp = vqec_igmp_get_entry_by_tuner_id(id)) && vqec_igmp->proxy) {
        /* Stringify the STB IP address */
        in.s_addr = vqec_igmp->stb_addr.s_addr;
        if (!inet_ntop(AF_INET, &in.s_addr, temp, INET_ADDRSTRLEN)) {
            vqec_lock_unlock(vqec_g_lock);
            return;
        }
        CONSOLE_PRINTF("Tuner name:                 %s\n", name);
        CONSOLE_PRINTF(" IGMP Proxy State:          Enabled\n"); 
        CONSOLE_PRINTF(" VQEC Interface:            %s\n", 
                       vqec_igmp->output_if_name);
        CONSOLE_PRINTF(" STB IP Address:            %s\n", temp);
        /* Show stream output stats, all in one place */
        output = vqec_stream_output_thread_get_by_tuner_id(id);
        if (output && output->output_sock) {
            (void)vqec_url_build(output->output_prot,
                                 output->output_dest,
                                 output->output_port,
                                 url,
                                 VQEC_MAX_URL_LEN);
            CONSOLE_PRINTF(" destination URL:           %s\n", url);
            CONSOLE_PRINTF(" packets sent:              %lld\n"
                           " packets dropped:           %lld\n",
                           output->output_sock->outputs,
                           output->output_sock->output_drops);
        }
    } else {
        CONSOLE_PRINTF("Tuner name:                 %s\n", name);
        CONSOLE_PRINTF(" IGMP Proxy State:          Disabled\n"); 
    }
    vqec_lock_unlock(vqec_g_lock);
}

vqec_error_t 
vqec_set_igmp_proxy_param (char *tuner_name, char *stb_if, char *stb_ip) 
{
    in_addr_t stbaddr;
    vqec_error_t err;
    if (!vqec_url_ip_parse(stb_ip, &stbaddr)) {
        return VQEC_ERR_INVALIDARGS;
    }
    err = vqec_proxy_igmp_join(tuner_name, stb_if,
                               stbaddr,
                               vqec_stream_output_channel_join_call_back_func,
                               NULL,
                               vqec_stream_output_channel_leave_call_back_func,
                               NULL);
    return err;
}

/* 
 * Go through the config file to check if the parameters for the IGMP
 * Proxy have been enabled. It is expected that the filename passed to this
 * function be the same as the system configuration used to configure the
 * rest of vqec. If the filename is different, it may be observed that the 
 * tuners that are non-existent as far as the other system configuration is
 * concerned, would be ignored. The function is expected to work 
 * as long as the number of tuners in the system config is less than 
 * VQEC_IGMP_MAX_TUNERS
 */
void vqec_igmp_proxy_init (char *filename)
{
    vqec_config_t l_config_cfg;
    vqec_config_setting_t *r_setting, *r_setting2;

    vqec_error_t err;

    int num_tuners = 0;
    int idx = 0;
    vqec_config_setting_t *cur_tuner_settings;
    vqec_config_setting_t *temp_setting, *temp_setting2;
    const char *temp_str = 0;
    char tuner_name[VQEC_MAX_NAME_LEN];
    char stb_ip_address[VQEC_MAX_NAME_LEN];
    char stb_if[VQEC_MAX_NAME_LEN];
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    int max_tuners, temp_int;

    memset(tuner_name, 0, VQEC_MAX_NAME_LEN);
    memset(stb_ip_address, 0, VQEC_MAX_NAME_LEN);
    memset(stb_if, 0, VQEC_MAX_NAME_LEN);

    /* Initialize the vqec_config configuration */
    vqec_config_init(&l_config_cfg);

    if (!filename) {
        return;
    }

    /* Load the file */
    printf("looking for igmp-proxy parameters in config %s..\n", filename);
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

            for (idx = 0; idx < num_tuners; idx++) {

                s_igmp[ idx ]->proxy = FALSE; 

                /* get the cur_idxth tuner in the list of tuners */
                cur_tuner_settings = vqec_config_setting_get_elem(r_setting, idx);

                /* get the params from this tuner */
                temp_setting =
                    vqec_config_setting_get_member(cur_tuner_settings,
                                                   "name");
                if (temp_setting) {
                    temp_str = vqec_config_setting_get_string(temp_setting);
                    if (temp_str) {
                        (void)strlcpy(tuner_name, temp_str, VQEC_MAX_NAME_LEN);
                    }
                }

                /* Populate msg_buf with the default error message */    
                snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE ,
                         "Error in setting igmp proxy for "
                         "\"%s\". Disabling igmp proxy for: "
                         "\"%s\"\n", tuner_name, tuner_name);

                temp_setting = 
                    vqec_config_setting_get_member(cur_tuner_settings,
                                                   "igmp_proxy_stb_if");

                temp_setting2 = 
                    vqec_config_setting_get_member(cur_tuner_settings,
                                                   "igmp_proxy_stb_ip_address");
                if (temp_setting && temp_setting2) {
                    temp_str = vqec_config_setting_get_string(temp_setting);
                    if (temp_str) {
                        (void)strlcpy(stb_if, temp_str, VQEC_MAX_NAME_LEN);
                    }
                    
                    temp_str = vqec_config_setting_get_string(temp_setting2);
                    if (temp_str) {
                        (void)strlcpy(stb_ip_address, temp_str, 
                                      VQEC_MAX_NAME_LEN);
                    }

                    err = vqec_set_igmp_proxy_param(tuner_name, stb_if, 
                                                    stb_ip_address);
                    switch(err) {
                        case (VQEC_OK):
                            s_igmp[ idx ]->proxy = TRUE; 
                            break;
                        case (VQEC_OK_IGMP_PROXY_DISABLED):
                            break;
                        default:
                            snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE ,
                                     "Error in setting igmp proxy for "
                                     "tuner \"%s\" (%s)\n", tuner_name,
                                     vqec_err2str(err));
                            syslog_print(VQEC_ERROR, msg_buf);
                    }
                } else if (temp_setting || temp_setting2) {
                    /* Error case : One param was found, but not both */
                    printf(msg_buf);
                }
            }
        }
    }
}
