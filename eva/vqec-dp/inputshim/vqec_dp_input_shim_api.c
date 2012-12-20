/*
 * Copyright (c) 2008-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * The VQE-C input shim receives packets on a STB/host platform and forwards
 * them to the core VQE-C data plane module.  Since different STB/host
 * platforms may have different interfaces with the network, it is expected
 * that this file may require customization (e.g. in the context of
 * integrating VQE-C into a linux kernel) on a given platform.
 *  
 * The VQE-C input shim receives designated packet streams from the network
 * and passes them via APIs to the core VQE-C dataplane APIs.  Packets are
 * passed from the "output stream" objects on which they are received to
 * input stream objects using API function pointers supplied to the 
 * input shim during stream creation and initialization.
 *
 * File organization:
 *   1. types and #defines
 *   2. filter entry functions
 *   3. output stream functions
 *   4. input shim APIs
 */

#include "vqec_dp_input_shim_private.h"
#include "id_manager.h"
#include "vqec_dp_common.h"
#include "vqec_dp_utils.h"
#include "vqec_dp_debug_utils.h"
#include <utils/zone_mgr.h>

/* included for unit testing purposes */
#ifdef _VQEC_DP_UTEST
#include "test_dp_interposers.h"
#endif

/*
 * The macro below can be defined to print out additional debugging info
 * during testing/debugging.  Since it is not suitable for use in production
 * environments (output is only meaningful to a developer and may be on a
 * per-packet basis), support for it is not compiled in by default.
 *
 * Any debugs which are needed in a production environment should use
 * VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM, ...) instead.
 */

#if 0
#define VQE_PRINT printk
#define VQEC_DP_INPUT_SHIM_DEBUG(arg...) VQE_PRINT(arg);
#else
#define VQEC_DP_INPUT_SHIM_DEBUG(arg...)
#endif

/*
 * Temporary storage used for building debug strings
 */
#define DEBUG_STR_LEN 200
static char s_debug_str[DEBUG_STR_LEN];
static char s_debug_str2[DEBUG_STR_LEN];
#define DEBUG_STR_LEN_SHORT 20
static char s_debug_str_short[DEBUG_STR_LEN_SHORT];
static char s_debug_str_short2[DEBUG_STR_LEN_SHORT];

/* Global parameters cached for socket creation/manipulation */
static uint32_t vqec_dp_input_shim_max_paksize;
static uint32_t vqec_dp_input_shim_pakpool_size;

/* Global status of the input shim */
vqec_dp_input_shim_status_t 
vqec_dp_input_shim_status = { TRUE, 0, 0, 0 };

/* Output Stream ID table */
static id_table_key_t vqec_dp_input_shim_os_id_table_key = 
    ID_MGR_TABLE_KEY_ILLEGAL;

/*
 * In case no packets are available from the packet pool, the input shim
 * maintains storage for a single packet of its own.  This is used primarily
 * for draining the sockets if a pak cannot be allocated.
 */
static vqec_pak_t *s_vqec_dp_input_shim_pak = NULL;

/*
 * Allocation zones.
 */
static struct vqe_zone *s_vqec_filter_entry_pool = NULL;
static struct vqe_zone *s_vqec_input_shim_os_pool = NULL;
static struct vqe_zone *s_input_shim_pak_pool = NULL;
static struct vqe_zone *s_input_shim_filter_table_pool = NULL;
static vqec_recv_sock_pool_t *s_vqec_recv_sock_pool = NULL;

/*
 * Initialize the filter entry table and number of scheduling classes.
 */
uint32_t vqec_dp_input_shim_num_scheduling_classes = 0;
vqec_dp_scheduling_class_t *vqec_dp_input_shim_filter_table = NULL;

/* creates an empty filter entry */
vqec_filter_entry_t *
vqec_dp_input_shim_filter_entry_create (void)
{
    vqec_filter_entry_t *filter_entry;

    filter_entry = (vqec_filter_entry_t *)
        zone_acquire(s_vqec_filter_entry_pool);
    if (!filter_entry) {
        return NULL;
    }
    memset(filter_entry, 0, sizeof(vqec_filter_entry_t));

    vqec_dp_input_shim_status.num_filters++;
    return (filter_entry);
}

/*
 * destroys a filter entry, returns resources it holds
 *
 * @param[in] filter_entry filter entry to be destroyed
 *
 * NOTE:  the filter entry MUST be removed from any databases prior
 *        to calling this function; otherwise, any pointers to this
 *        filter entry will become invalid with its destruction
 */
void
vqec_dp_input_shim_filter_entry_destroy (vqec_filter_entry_t *filter_entry)
{
    if (!filter_entry) {
        return;
    }
    if (filter_entry->socket) {
        vqec_recv_sock_destroy_in_pool(s_vqec_recv_sock_pool,
                                       filter_entry->socket);
    }
    if (filter_entry->extra_igmp_socket) {
        vqec_recv_sock_destroy_in_pool(s_vqec_recv_sock_pool,
                                       filter_entry->extra_igmp_socket);
    }
    zone_release(s_vqec_filter_entry_pool, filter_entry);
    vqec_dp_input_shim_status.num_filters--;
}

/*
 * vqec_dp_input_shim_run_service_filter_entry()
 *
 * Collects packets which have arrived for a bound output stream, and
 * forwards them to any connected input streams.
 *
 * @param[in] filter_entry  Filter whose output stream is to be processed
 */
void
vqec_dp_input_shim_run_service_filter_entry (vqec_filter_entry_t *filter_entry)
{
    vqec_dp_input_shim_os_t *os;
    vqec_pak_t    *pak=NULL;
    vqec_pak_t    *pak_array[VQEC_DP_STREAM_PUSH_VECTOR_PAKS_MAX];
    int32_t        read_len;
    int32_t        num_paks_in_array, i;
    int32_t        num_bytes_in_array;
    boolean        potentially_more_pkts = TRUE;
    boolean        static_pkt_in_use = FALSE;
    
    VQEC_DP_ASSERT_FATAL(filter_entry, "inputshim");
    os = filter_entry->os;
 
    VQEC_DP_INPUT_SHIM_DEBUG(
        "processing filter entry for OS ID '0x%08x'...", os->os_id);

    /*
     * Avoid processing this filter if its packets will not be
     * received by an input stream.
     */
    if (!os || !os->is_ops ||
        ((os->is_capa & VQEC_DP_STREAM_CAPA_PUSH_VECTORED) && 
         !os->is_ops->receive_vec) ||
        ((os->is_capa & VQEC_DP_STREAM_CAPA_PUSH) && 
         !os->is_ops->receive)) {
        return;
    }
    
    /*
     * Overall strategy is:
     *  1. Read up to a full vector of packets from the socket into an array
     *  2. Forward the packet(s) from the array to the Input Stream
     *  3. Repeat until the socket runs dry.
     */
    do {        

        /* Initialize array as empty */
        num_paks_in_array = 0;
        num_bytes_in_array = 0;

        /* Read packets from the socket to the array */
        while (num_paks_in_array < VQEC_DP_STREAM_PUSH_VECTOR_PAKS_MAX) {

            /* alloc without a particle */
            pak = vqec_pak_alloc_no_particle();

            if (!pak) {
                /*
                 * Out of packet buffers at the moment.  Use a static buffer 
                 * to read from the socket, and then discard the packet.
                 * 
                 * The "socket draining" behavior is primarily targeted to 
                 * help recovery from the scenario where VQE-C packet buffers
                 * are not available for an extended period of time, in which
                 * case VQE-C socket buffers would fill up considerably 
                 * (if not drained).  By draining the sockets, the likelihood
                 * of later pushing stale data into the decoder is minimized.
                 */
                pak = s_vqec_dp_input_shim_pak;
                static_pkt_in_use = TRUE;
            }

            /*
             * Set the pak buffer pointer.  This is only necessary in userspace,
             * as in kernel space, this pointer will be replaced by a pointer
             * to the skb's buffer.  However, in userspace, this pointer needs
             * to be set so vqec_recv_sock_read_pak() knows where to write the
             * data that is received from the socket.
             */
            pak->buff = (char *)(pak + 1);

            read_len = vqec_recv_sock_read_pak(filter_entry->socket,
                                               pak);

            if (read_len < 1) {
                /*
                 * The socket has been fully drained.  So let's stop trying to
                 * read packets, and go pass along any we may have collected.
                 */
                potentially_more_pkts = FALSE;
                if (!static_pkt_in_use) {
                    vqec_pak_free(pak);
                }
                break;
            }
            if (static_pkt_in_use) {
                /*
                 * Free any kernel memory associated with the static packet,
                 * but skip other processing of it.  Instead, just go try
                 * again to allocate a new packet and read from the socket.
                 */
                vqec_pak_free_skb(pak); 
                static_pkt_in_use = FALSE;
                os->stats.drops++;
                /* This is also an overrun in the TR-135 sense */
                vqec_dp_input_shim_status.tr135_overruns++;
                continue;
            }

            /* Add the packet to the array. */
            pak_array[num_paks_in_array] = pak;

            /* Keep track of what's in the array */
            num_paks_in_array++;
            num_bytes_in_array += vqec_pak_get_content_len(pak);
        }

        VQEC_DP_INPUT_SHIM_DEBUG(
            "collected %u paks\n", num_paks_in_array);
        /* Forward the held packets to the Input Stream */
        if (num_paks_in_array) {
            if (os->is_capa & VQEC_DP_STREAM_CAPA_PUSH_VECTORED) {
                VQEC_DP_INPUT_SHIM_DEBUG("    dumping via vector...\n");
                if (os->is_ops->receive_vec(os->is_id,
                                            (vqec_pak_t **)pak_array,
                                            num_paks_in_array) !=
                    VQEC_DP_STREAM_ERR_OK) {
                    vqec_dp_input_shim_status.num_pkt_errors +=
                        num_paks_in_array;
                }
                for (i=0; i<num_paks_in_array; i++) {
                    vqec_pak_free(pak_array[i]);
                }
            } else {
                VQEC_DP_INPUT_SHIM_DEBUG("   dumping via single...\n");
                for (i=0; i<num_paks_in_array; i++) {
                    if (os->is_ops->receive(os->is_id, pak_array[i]) !=
                        VQEC_DP_STREAM_ERR_OK) {
                        vqec_dp_input_shim_status.num_pkt_errors++;
                    }
                    vqec_pak_free(pak_array[i]);
                }
            }
            if (VQEC_DP_GET_DEBUG_FLAG(VQEC_DP_DEBUG_COLLECT_STATS)) {
                os->stats.packets += num_paks_in_array;
                os->stats.bytes += num_bytes_in_array;
            }
        }

    } while (potentially_more_pkts);
}

/*
 * vqec_dp_input_shim_run_service()
 *
 * Service the input shim's output streams.
 *
 * This API is intended to be invoked periodically, with an "elapsed_time"
 * parameter indicating the amount of time (in milliseconds) that has elapsed
 * since the previous invocation.  All scheduling classes whose intervals
 * have terminated within the elapsed time period will be processed (any
 * packets that have been buffered for an OS assigned to the scheduling class
 * will be forwared to its connected IS).
 *
 * If invoking this API for the first time, the value passed for the 
 * "elapsed_time" parameter is not significant--all scheduling classes will 
 * be processed.
 *
 * E.g. Assume two scheduling classes:
 *           class A:  interval of 20ms
 *           class B:  interval of 40ms
 *      These classes will be serviced at the instances shown below,
 *      if given the sequence of calls to this API:
 *           absolute time        call                   serviced classes
 *           -------------        ----------------       ----------------
 *           t                    run_service(x):        class A, B
 *           t + 20               run_service(20):       class A
 *           t + 40               run_service(20):       class A, B
 *           t + 60               run_service(20):       class A
 *           t + 70               run_service(10):
 *           t + 85               run_service(15):       class A, B
 *           t + 100              run_service(15);
 *           t + 105              run_service(5);        class A
 *           t + 160              run_service(55);       class A, B
 *
 * @param[in] elapsed_time  Amount of time which has elapsed since the previous
 *                          call to this function following input shim startup
 *                          (if applicable).
 */
void
vqec_dp_input_shim_run_service (uint16_t elapsed_time)
{
    vqec_filter_entry_t *filter_entry;
    uint32_t i;

    for (i=0; i<vqec_dp_input_shim_num_scheduling_classes; i++) {
        if (vqec_dp_input_shim_filter_table[i].remaining) {
            /*
             * Only deduct the elapsed time if this is not the first service
             * call.  If it is the first service call, the class will be
             * serviced regardless of the elapsed time parameter.
             */
            vqec_dp_input_shim_filter_table[i].remaining -= elapsed_time;
        }
        if (vqec_dp_input_shim_filter_table[i].remaining <= 0) {
            VQEC_DP_INPUT_SHIM_DEBUG("\nProcessing filter class %u...\n", i);
            VQE_LIST_FOREACH(filter_entry,
                         &vqec_dp_input_shim_filter_table[i].filters,
                         list_obj) {                
                vqec_dp_input_shim_run_service_filter_entry(filter_entry);
            }
            vqec_dp_input_shim_filter_table[i].remaining =
                vqec_dp_input_shim_filter_table[i].interval;
        }
    }
}

/*
 * vqec_dp_input_shim_os_id_to_os()
 *
 * @param[in]  os_id   OS ID value
 * @return             pointer to corresponding os object, or
 *                     NULL if no associated os object
 */
vqec_dp_input_shim_os_t *
vqec_dp_input_shim_os_id_to_os (const vqec_dp_osid_t os_id)
{
    uint ret_code;

    return (id_to_ptr(os_id, &ret_code, vqec_dp_input_shim_os_id_table_key));
}


/**
 * vqec_dp_input_shim_filter_fd()
 *
 * Return the socket file descriptor of the filter
 * for the specified inputshim output stream.
 *
 * @param[in]  os_id   OS ID value.
 * @return     int     file descriptor
 *                     -1 if not found.
 */
int vqec_dp_input_shim_filter_fd(vqec_dp_osid_t os_id)
{
    vqec_dp_input_shim_os_t *p_os;

    p_os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!p_os) {
        return -1;
    }

    if (!p_os->filter_entry ||
        !p_os->filter_entry->socket) {
        return -1;
    }

    return (p_os->filter_entry->socket->fd);
}

/*
 * vqec_dp_input_shim_os_capa()
 *
 * Get the capability flags for the output stream.   This identifies the
 * ways in which the output stream may pass packets to an input stream.
 *
 * @param[in]  os        A valid output stream handle.
 * @param[out] int32_t   Stream's capability flags.
 */
int32_t
vqec_dp_input_shim_os_capa (vqec_dp_osid_t os_id)
{
    vqec_dp_input_shim_os_t *os;

    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os) {
        return VQEC_DP_STREAM_CAPA_NULL;
    }

    return (os->capa);
}

/*
 * vqec_dp_input_shim_os_accept_connect()
 *
 * Connects an output stream to the given input stream.  This implies
 * caching the input stream operations interface, and delivering datagrams
 * to the input stream through that interface upon their arrival.
 *
 * An OS may only be connected to one IS.  If an IS is already connected
 * to the given OS, this function will fail.  The "disconnect" API may be
 * used to disconnect an IS from an OS.
 *
 * In addition to connecting an OS to an IS, it is necessary to "bind" the OS
 * to a filter (which defines the traffic stream of this OS) before traffic
 * may be passed from OS -> IS.
 *
 * Parameters:
 *  @param[in]  os_id  A valid handle indicating the output stream which is
 *                      to be connected to an IS.
 *  @param[in]  is_id  A valid handle identifying the input stream to be 
 *                      connected to the OS.
 *  @param[in]  ops    The operations interface of the input stream.
 *                      The caller MUST ensure this memory is not freed or
 *                      overwritten for the lifespan of this output stream,
 *                      as its address will be cached internally.
 *                      This operations interface includes the "receive" API,
 *                      which is used to deliver packets to the connected IS.
 *  @param[in]  encap  The expected encapsulation type for traffic passed from
 *                      the OS to IS.  The value may represent UDP or RTP;
 *                      and is validated against the encapsulation type that
 *                      was defined for the OS upon its creation.  If the
 *                      type encapsulation values do not match, the
 *                      VQEC_DP_STREAM_ERR_ENCAPSMISMATCH error code is 
 *                      returned.
 *  @param[in]  req_capa Specifies how the OS will deliver its stream to IS.
 */
vqec_dp_stream_err_t
vqec_dp_input_shim_os_accept_connect (vqec_dp_osid_t os_id,
                                      vqec_dp_isid_t is_id, 
                                      const vqec_dp_isops_t *ops, 
                                      vqec_dp_encap_type_t encap,
                                      int32_t req_capa)
{
    vqec_dp_stream_err_t status = VQEC_DP_STREAM_ERR_OK;
    vqec_dp_input_shim_os_t *os;

    /* Verify the input shim is operational */      
    if (vqec_dp_input_shim_status.is_shutdown) {
        status = VQEC_DP_STREAM_ERR_SERVICESHUT;
        goto done;
    }

    /* Validate the parameters */
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os || !ops || (req_capa > VQEC_DP_STREAM_CAPA_MAX) ||
        ((req_capa & VQEC_DP_STREAM_CAPA_PUSH_VECTORED) && 
         !ops->receive_vec) ||
        ((req_capa & VQEC_DP_STREAM_CAPA_PUSH) && !ops->receive) ||
        ((req_capa & VQEC_DP_STREAM_CAPA_PUSH_POLL) &&
         !(ops->receive || ops->receive_vec))) {
        status = VQEC_DP_STREAM_ERR_INVALIDARGS;
        goto done;
    }
    if (encap != os->encaps) {
        status = VQEC_DP_STREAM_ERR_ENCAPSMISMATCH;
        goto done;
    }
    if (req_capa & ~os->capa) {
        status = VQEC_DP_STREAM_ERR_NACKCAPA;
        goto done;
    }
    if (os->is_id != VQEC_DP_INVALID_ISID) {
        status = VQEC_DP_STREAM_ERR_OSALREADYCONNECTED;
        goto done;
    }

    /* Connection can succeed, store associated IS information */ 
    os->is_id = is_id;
    os->is_ops = ops;
    os->is_capa = req_capa;
 
done:
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM, "%s(os_id=0x%08x, is_id=%u, ops=%p,"
                  " encap=%s, req_capa=%s)%s\n",
                  __FUNCTION__, os_id, is_id, ops,
                  vqec_dp_stream_encap2str(encap, s_debug_str, DEBUG_STR_LEN),
                  vqec_dp_stream_capa2str(req_capa, s_debug_str2, 
                                          DEBUG_STR_LEN),
                  vqec_dp_stream_err2str_complain_only(status));
    return (status);
}

/*
 * vqec_dp_input_shim_os_disconnect()
 * 
 * Disconnect the output stream from any input stream that
 * it may have been connected to via an earlier connect call.
 *
 * @param[in]   os  A valid output stream handle.
 */
void
vqec_dp_input_shim_os_disconnect (vqec_dp_osid_t os_id)
{
    vqec_dp_input_shim_os_t *os;

    /* Verify the input shim is operational */      
    if (vqec_dp_input_shim_status.is_shutdown) {
        goto done;
    }

    /* Validate the parameter */
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os) {
        goto done;
    }

    /* Disconnect the OS from its IS, if any */
    os->is_id = VQEC_DP_INVALID_ISID;
    os->is_ops = NULL;
    os->is_capa = VQEC_DP_STREAM_CAPA_NULL;
    
done:
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM, "%s(os_id=0x%08x)\n",
                  __FUNCTION__, os_id);
    return;
}


/*
 * vqec_dp_input_shim_os_bind()
 *
 * Binds an input filter to an output stream.  The input filter identifies
 * the set of arriving traffic which constitutes the output stream.
 *
 * The passed filter must NOT match traffic which also matches any existing
 * filter.  In other words, the set of traffic streams matched by any two 
 * filters must be disjoint.  If this assumption is not met, the behavior 
 * of overlapping filters is unspecified.
 *
 * Source information (ip address and/or port) may be used in filtering
 * if requested via the filter's "src_ip_filter" and "src_port_filter" 
 * fields, with the following caveats for multicast streams:
 *   1. requests to filter based on source IP address are supported only
 *       if the kernel version used supports this capability (true with
 *       recent kernel versions), and
 *   2. requests to filter based on source port are not supported
 * and the following caveat for unicast streams:
 *   1. if a source port filter is supplied then a source address
 *       filter must also be supplied
 *
 * Callers may specify a 0 for the destination port when using a unicast
 * destination address.  In this case, an unused destination port will 
 * be allocated and kept open as long as the output stream remains bound.
 * The allocated port must be returned back to the caller in *port.
 *
 * Callers may supply the value INADDR_ANY for the destination IP address.
 * The meaning of INADDR_ANY as a destination IP address differs as follows:
 *
 *    If a source address filter and source port filter are NOT supplied, 
 *    then only packets arriving that have filter's destination port and a
 *    destination IP address that matches the address configured on *any* 
 *    interface on the box will be considered a match.
 *
 *    If a source address filter and source port filter ARE supplied,
 *    then ONLY packets arriving that have the filter's destination port and
 *    the "designated" interface's IP address will be considered a match.
 *    Here, the "designated" interface is chosen via a local route lookup
 *    (done at the time of filter binding) to see which interface would be
 *    used if sending to the IP address supplied as the filter's source
 *    IP address.  If the local routing decision were to change such that
 *    a different interface would be used when sending a packet to the
 *    filter's source IP address, then the behavior of the filter is
 *    unspecified.
 * 
 * The scheduling class of a filter is used in conjunction with the
 * vqec_dp_input_shim_startup() and vqec_dp_input_shim_run_service() APIs.
 * When the vqec_dp_input_shim_run_service() API is called with an 
 * "elapsed_time" parameter, the set of scheduling classes that need to be
 * serviced is determined.  If the supplied filter has been assigned to
 * such a scheduling class, it will be serviced in that context. 
 *
 * @param[in]   os_id              A valid output stream handle.
 * @param[in]   fil                Pointer to the input filter (with IP
 *                                  addresses and ports in network byte order)
 * @param[out]  *port              If the filter destination port is 0,
 *                                  an unused port will be allocated and
 *                                  returned to the user in this parameter.
 *                                  (in network byte order)
 * @param[out]  *sock_fd           If the socket is used for the filter,
 *                                  return the fd of that socket for use
 *                                  in NAT injection.
 * @param[in]   so_rcvbuf          Maximum number of arriving bytes that
 *                                  may be buffered by the OS, prior to it
 *                                  being serviced.  Arriving bytes in excess
 *                                  of this limit will be dropped.
 * @param[in]   scheduling_class   Associates the filter with the given
 *                                  scheduling_class (e.g. 0,1,2,3...)
 *                                  See the vqec_dp_input_shim_run_service() 
 *                                  function header for details on how this
 *                                  is used.
 * @param[in]   xmit_dscp_value    DSCP value to be used for all pkts
 *                                  transmitted to this stream's source,
 *                                  if any.  Packets can be transmitted
 *                                  to the stream's source via the 
 *                                  returned "sock_fd" value.  The API
 *                                  enforces the requested dscp_value by
 *                                  assigning the DSCP as a socket option.
 *                                  If an elevated DSCP cannot be assigned,
 *                                  an error is logged, and the
 *                                  default/best-effort DSCP is used.
 * @param[out]  vqec_dp_stream_err_t  Returns STREAM_ERR_OK on success.
 */
vqec_dp_stream_err_t
vqec_dp_input_shim_os_bind (vqec_dp_osid_t os_id,
                            vqec_dp_input_filter_t *fil,
                            uint16_t *port,
                            int *sock_fd,
                            uint32_t so_rcvbuf,
                            uint32_t scheduling_class,
                            uint8_t xmit_dscp_value)
{
    in_addr_t rcv_if_address;
    in_addr_t mcast_group;
    struct in_addr saddr;
    vqec_dp_stream_err_t status = VQEC_DP_STREAM_ERR_OK;
    vqec_dp_input_shim_os_t *os;
    vqec_filter_entry_t *filter_entry = NULL;
    vqec_recv_sock_t *vqec_recv_sock = NULL;
    vqec_recv_sock_t *extra_igmp_sock = NULL;
    int modified_so_rcvbuf;

    if (vqec_dp_input_shim_status.is_shutdown) {
        status = VQEC_DP_STREAM_ERR_SERVICESHUT;
        goto done;
    }
    
    /*
     * Validate parameters
     *
     * Checks for any of the following invalid situations:
     *  1. Supplied OS ID doesn't map to a valid OS
     *  2. Filter is not supplied
     *  3. Filter destination port is zero and port parameter (used to 
     *      return an internally allocated port) is NULL
     *  4. Scheduling class exceeds the limit configured at startup
     */
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os || 
        !fil ||
        (!port && !ntohs(fil->u.ipv4.dst_port)) ||
        (scheduling_class >= vqec_dp_input_shim_num_scheduling_classes)) {
        status = VQEC_DP_STREAM_ERR_INVALIDARGS;
        goto done;
    }
    /*
     * Check for any of the following unsupported filter situations:
     *  1. Filter protocol is not UDP
     *  2. Filter destination addr is multicast and destination port is zero
     *  3. Filter destination addr is multicast and a src port filter is
     *     requested
     *  4. Filter destination is unicast, a src port filter is requested, 
     *     but a src addr filter is not
     */
    if (fil->proto != INPUT_FILTER_PROTO_UDP ||
        (IN_MULTICAST(ntohl(fil->u.ipv4.dst_ip)) &&
         !ntohs(fil->u.ipv4.dst_port)) ||
        (IN_MULTICAST(ntohl(fil->u.ipv4.dst_ip)) &&
         fil->u.ipv4.src_port_filter) ||
        (!IN_MULTICAST(ntohl(fil->u.ipv4.dst_ip)) &&
         (fil->u.ipv4.src_port_filter && !fil->u.ipv4.src_ip_filter))) {
        status = VQEC_DP_STREAM_ERR_FILTERUNSUPPORTED;
        goto done;
    }
    /* Verify filter is not already bound (caller must unbind first if so) */
    if (os->filter_entry) {
        status = VQEC_DP_STREAM_ERR_OSALREADYBOUND;
        goto done;
    }
    
    /* Create a filter entry to store the binding */
    filter_entry = vqec_dp_input_shim_filter_entry_create();
    if (!filter_entry) {
        status = VQEC_DP_STREAM_ERR_NOMEMORY;
        goto done;
    }

    /* Validation succeeded:  create and bind the socket */
    if (IN_MULTICAST(ntohl(fil->u.ipv4.dst_ip))) {
        rcv_if_address = fil->u.ipv4.dst_ifc_ip;
        mcast_group = fil->u.ipv4.dst_ip;
    } else {
        rcv_if_address = fil->u.ipv4.dst_ip;
        mcast_group = 0;
    }
    vqec_recv_sock = vqec_recv_sock_create_in_pool(
                                           s_vqec_recv_sock_pool,
                                           "",
                                           rcv_if_address,
                                           fil->u.ipv4.dst_port,
                                           mcast_group,
                                           FALSE,
                                           so_rcvbuf,
                                           xmit_dscp_value);
    if (!vqec_recv_sock) {
        status = VQEC_DP_STREAM_ERR_INTERNAL;
        goto done;
    }
    filter_entry->socket = vqec_recv_sock;

    /*
     * The reference implementation of the input and output shim share the
     * memory allocated for the vqec_recv_socket.  The vqec_recv_socket is used
     * to filter packets (from above) using the IP stack at the socket layer
     * within the input shim and then receive them into this socket memory.
     * The same packet data memory is used throughout the rest of VQE-C,
     * including the output shim.
     *
     * Socket accounting normally accounts for buffering via the setsockopt()
     * function call.  However, this call only permits the socket buffer to be
     * resized to a maximum of 1 Mbyte, which is not enough to encompass the
     * desired VQE-C packet cache.  Rather than requiring the modification of
     * kernel source to expand the amount of memory that a socket can use, the
     * code below directly modifies the socket parameter that is normally
     * modified by the setsockopt() processing.
     */
    /* adjust SO_RCVBUF without restrictions (works in kernel-space only) */
    modified_so_rcvbuf = so_rcvbuf +  
        (vqec_dp_input_shim_pakpool_size *
         (vqec_dp_input_shim_max_paksize + vqec_recv_sock_get_pak_overhead()));
    (void)vqec_recv_sock_set_so_rcvbuf(vqec_recv_sock,
                                       (so_rcvbuf ? TRUE : FALSE),
                                       modified_so_rcvbuf);

    if (IN_MULTICAST(ntohl(fil->u.ipv4.dst_ip))) {
        /*
         * multicast destination:
         *   1. if requested, apply source filter of (addr)
         *   2. perform multicast join to receive traffic
         */
        saddr.s_addr = (fil->u.ipv4.src_ip_filter ? 
                        fil->u.ipv4.src_ip : INADDR_ANY);
        if (!vqec_recv_sock_mcast_join(vqec_recv_sock, saddr)) {
            status = VQEC_DP_STREAM_ERR_INTERNAL;
            goto done;
        }
    } else {
        /* unicast destination:
         *   1. if requested, apply source filter using (addr, port)
         *   2. if requested, join extra igmp ip to stop previous mcast
         */
        if (fil->u.ipv4.src_ip_filter || fil->u.ipv4.src_port_filter) {
            if (!vqec_recv_sock_connect(vqec_recv_sock,
                                        fil->u.ipv4.src_ip_filter,
                                        fil->u.ipv4.src_ip,
                                        fil->u.ipv4.src_port_filter,
                                        fil->u.ipv4.src_port)) {
                status = VQEC_DP_STREAM_ERR_INTERNAL;
                goto done;
            }
        }

        /* If using an extra igmp multicast ip, bind to it here */
        if (fil->u.ipv4.rcc_extra_igmp_ip) {
            extra_igmp_sock = vqec_recv_sock_create_in_pool(
                s_vqec_recv_sock_pool,
                "",
                fil->u.ipv4.dst_ifc_ip,
                fil->u.ipv4.dst_port,
                fil->u.ipv4.rcc_extra_igmp_ip,
                FALSE,
                0 /* so_rcvbuf */,
                0 /* xmit_dscp_value */);
            if (!extra_igmp_sock) {
                /* Allocation failure */
                status = VQEC_DP_STREAM_ERR_INTERNAL;
                goto done;
            }
            if (!vqec_recv_sock_mcast_join(extra_igmp_sock, (struct in_addr){0})) {
                vqec_recv_sock_destroy_in_pool(s_vqec_recv_sock_pool,
                                               extra_igmp_sock);
                status = VQEC_DP_STREAM_ERR_INTERNAL;
                goto done;
            }
            filter_entry->extra_igmp_socket = extra_igmp_sock;
        }
    }
    
    /* Success:  assign the remaining parameters to the filter entry */
    filter_entry->scheduling_class = scheduling_class;
    filter_entry->xmit_dscp_value = xmit_dscp_value;
    memcpy(&filter_entry->filter, fil, sizeof(vqec_dp_input_filter_t));
    if (!ntohs(filter_entry->filter.u.ipv4.dst_port)) {
        filter_entry->filter.u.ipv4.dst_port =
            vqec_recv_sock_get_port(vqec_recv_sock);
    }
    filter_entry->os = os;

    /* Insert the filter entry into the filter table */
    VQE_LIST_INSERT_HEAD(
        &vqec_dp_input_shim_filter_table[scheduling_class].filters,
        filter_entry, list_obj);

    /* 
     * Filter entry is now in bind-committed state, i.e., it is on the 
     * scheduler list, and must be removed off that list at unbind.
     */
    filter_entry->committed = TRUE;

    /* Link to the filter from the OS */
    os->filter_entry = filter_entry;

done:
    if (status != VQEC_DP_STREAM_ERR_OK) {
        vqec_dp_input_shim_filter_entry_destroy(filter_entry);
        if (port) {
            *port = htons(0);
        }
    } else {
        if (!ntohs(fil->u.ipv4.dst_port)) {
            *port = vqec_recv_sock_get_port(vqec_recv_sock);
        }
    }
    if (port) {
        snprintf(s_debug_str_short, DEBUG_STR_LEN_SHORT, "%u", *port);
    } else {
        snprintf(s_debug_str_short, DEBUG_STR_LEN_SHORT, "NULL");
    }
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM, "%s(os_id=0x%08x, filter=%s, "
                  "port=%s, so_rcvbuf=%u, "
                  "scheduling_class=%u, xmit_dscp_value=%u)%s\n",
                  __FUNCTION__, os_id,
                  vqec_dp_input_filter_to_str(fil, TRUE, TRUE, TRUE),
                  s_debug_str_short, so_rcvbuf,
                  scheduling_class, xmit_dscp_value,
                  vqec_dp_stream_err2str_complain_only(status));
    return (status);
}

/*
 * vqec_dp_input_shim_os_unbind()
 *
 * Removes any previous binding for the given output stream.
 *
 * @param[in] os_id A valid output stream handle.
 */
void
vqec_dp_input_shim_os_unbind (vqec_dp_osid_t os_id)
{
    vqec_dp_input_shim_os_t *os;

    if (vqec_dp_input_shim_status.is_shutdown) {
        goto done;
    }
    
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os || !os->filter_entry) {
        goto done;
    }

    if (os->filter_entry->committed) {
        /* Entry is on the scheduler list iff the entry was committed. */
        VQE_LIST_REMOVE(os->filter_entry, list_obj);
    }
    vqec_dp_input_shim_filter_entry_destroy(os->filter_entry);
    os->filter_entry = NULL;

done:
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM, "%s(os_id=0x%08x)\n",
                  __FUNCTION__, os_id);
    return;
}

/*
 * vqec_dp_input_shim_os_get_status()
 *
 * Get stream status for an output stream. This includes the number of
 * connected input streams, the filter binding (if any), and
 * statistics. An implementation may choose not to keep per output
 * statistics. The statistics should be reset to 0 for that case.
 *
 * @param[in] os A valid output stream handle.
 * @param[out] status Status structure for the result.
 * @param[out] vqec_dp_stream_err_t Returns STREAM_ERR_OK on success.
 */
vqec_dp_stream_err_t
vqec_dp_input_shim_os_get_status (vqec_dp_osid_t os_id,
                                  vqec_dp_os_status_t *stat)
{
    vqec_dp_input_shim_os_t *os;
    vqec_dp_stream_err_t status = VQEC_DP_STREAM_ERR_OK;
    
    /* Validate parameters */
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os || !stat) {
        status = VQEC_DP_STREAM_ERR_INVALIDARGS;
        goto done;
    }

    memset(stat, 0, sizeof(vqec_dp_os_status_t));
    stat->is_cnt = 1;
    if (os->filter_entry) {
        memcpy(&stat->filter,
               &os->filter_entry->filter, sizeof(vqec_dp_input_filter_t));
    }
    memcpy(&stat->stats, &os->stats, sizeof(vqec_dp_stream_stats_t));
    
done:
    return status;
}
    

/*
 * vqec_dp_input_shim_os_get_connected_is()
 *
 * Retrieves the set of input streams associated with the given
 * output stream.
 *
 * Callers supply an isid_array (of size isid_array_len) into which
 * IS IDs for OS are copied.  The number of IS IDs copied into the
 * array is bound by the array size (specified by isid_array_len),
 * and returned via the num_isids parameter.
 *
 * If there are more IS IDs for the OS that were not copied into 
 * the isid_array (due to space limitations), then the more parameter 
 * will be set to TRUE upon return of the function.  The caller may 
 * then call the function again, with is_id_last assigned the ID value
 * of the last ID returned in the previous call.  (If this is the first
 * call, is_id_last should be VQEC_DP_INVALID_ISID to start from the
 * beginning of the list.)  Should the OS to IS stream mapping change
 * between individual calls to this function, callers may not retrieve
 * an atomic snapshot of the OS to IS mapping.
 *
 * @param[in] os_id       The OS whose ISes are to be retrieved
 * @param[in] is_id_last  The last IS retrieved from the previous call
 *                         of a sequence (which returned more==TRUE), or 
 *                         VQEC_DP_INVALID_ISID for the initial call
 *                         (to retrieve IS IDs starting at the beginning)
 * @param[out] isid_array      Array into which the ISes of os_id are copied
 * @param[in]  isid_array_len  Size of isid_array (in elements)
 * @param[out] num_isids       Number of IS IDs copied into isid_array
 * @param[out] more            TRUE if there are more IS IDs to be retrieved
 *                              for OS, or FALSE otherwise.
 * @param[out] vqec_dp_stream_err_t  Success/failure status of request
 */ 
vqec_dp_stream_err_t
vqec_dp_input_shim_os_get_connected_is (vqec_dp_osid_t os_id,
                                        vqec_dp_isid_t is_id_last,
                                        vqec_dp_isid_t *isid_array,
                                        uint32_t isid_array_len,
                                        uint32_t *num_isids,
                                        boolean *more)
{
    vqec_dp_input_shim_os_t *os;
    vqec_dp_stream_err_t status;

    /* Validate parameters */
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os || !isid_array || !isid_array_len || !num_isids || !more) {
        status = VQEC_DP_STREAM_ERR_INVALIDARGS;
        goto done;
    }

    /*
     * Output Streams can only have 0 or 1 connected input stream right now.
     * Copy its IS ID into the first position in the array.
     */
    if ((is_id_last == VQEC_DP_INVALID_ISID) &&
        (os->is_id != VQEC_DP_INVALID_ISID)) {
        isid_array[0] = os->is_id;
        *num_isids = 1;
    } else {
        *num_isids = 0;
    }
    *more = FALSE;
    status = VQEC_DP_STREAM_ERR_OK;
    
done:
    return (status);
}


/*
 * vqec_dp_input_shim_os_supported_encaps()
 *
 * Enumerate the encapsulation types that the stream supports.
 *
 * @param[in] os A valid output stream handle.
 * @param[out] encaps The array in which supported encapsulation
 * types are returned. The length of this array is given by *num 
 * on input; *num is updated to to the number of entries actually
 * written in encaps by this method.
 * @param[in / out] num When the method is invoked, contains 
 * the size of the encaps array. Before this method returns it 
 * will update *num with the number of types that are being
 * returned in the encaps array. 
 * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK if
 * no errors were encountered during the execution of this method.
 */
vqec_dp_stream_err_t
vqec_dp_input_shim_os_supported_encaps (vqec_dp_osid_t os_id, 
                                        vqec_dp_encap_type_t *encaps,
                                        uint32_t *num)
{
    vqec_dp_input_shim_os_t *os;

    /* Validate parameters */
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os || !encaps || !num || !*num) {
        return VQEC_DP_STREAM_ERR_INVALIDARGS;
    }

    /* Return the encapsulation defined for the OS */
    encaps[0] = os->encaps;
    *num = 1;

    return VQEC_DP_STREAM_ERR_OK;
}


/**
 * This method differs from the bind method above in two respects:
 *  (a) The method will reserve resources for the filter which is
 *      to be bound, but it will not acutally "enable" or "program"
 *      the filter to receive inbound packets. A user will call
 *      bind_commit to complete the binding.
 *  (b) This method does not support allocation of ephemeral
 *      ports, or return of a socket descriptor, since allocation 
 *      of ports impossible until e.g., a socket is created.
 */
vqec_dp_stream_err_t
vqec_dp_input_shim_os_bind_reserve (vqec_dp_osid_t os_id,
                                    vqec_dp_input_filter_t *fil,
                                    uint32_t so_rcvbuf,
                                    uint32_t scheduling_class,
                                    uint8_t xmit_dscp_value)
{
    vqec_dp_stream_err_t status = VQEC_DP_STREAM_ERR_OK;
    vqec_dp_input_shim_os_t *os;
    vqec_filter_entry_t *filter_entry = NULL;
    vqec_recv_sock_t *extra_igmp_sock = NULL;

    if (vqec_dp_input_shim_status.is_shutdown) {
        status = VQEC_DP_STREAM_ERR_SERVICESHUT;
        goto done;
    }
    
    /*
     * Validate parameters
     *
     * Checks for any of the following invalid situations:
     *  1. Supplied OS ID doesn't map to a valid OS
     *  2. Filter is not supplied
     *  3. Scheduling class exceeds the limit configured at startup
     */
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os || 
        !fil ||
        (scheduling_class >= vqec_dp_input_shim_num_scheduling_classes)) {
        status = VQEC_DP_STREAM_ERR_INVALIDARGS;
        goto done;
    }

    /*
     * Check for any of the following unsupported filter situations:
     *  1. Filter protocol is not UDP
     *  2. Filter destination addr is multicast and destination port is zero
     *  3. Filter destination addr is multicast and a src port filter is
     *     requested
     *  4. Filter destination is unicast, a src port filter is requested
     *     but a src addr filter is not
     */
    if (fil->proto != INPUT_FILTER_PROTO_UDP ||
        (IN_MULTICAST(ntohl(fil->u.ipv4.dst_ip)) &&
         !ntohs(fil->u.ipv4.dst_port)) ||
        (IN_MULTICAST(ntohl(fil->u.ipv4.dst_ip)) &&
         fil->u.ipv4.src_port_filter) ||
        (!IN_MULTICAST(ntohl(fil->u.ipv4.dst_ip)) &&
         (fil->u.ipv4.src_port_filter && !fil->u.ipv4.src_ip_filter))) {
        status = VQEC_DP_STREAM_ERR_FILTERUNSUPPORTED;
        goto done;
    }
    /* Verify filter is not already bound (caller must unbind first if so) */
    if (os->filter_entry) {
        status = VQEC_DP_STREAM_ERR_OSALREADYBOUND;
        goto done;
    }
    
    /* Create a filter entry to store the binding */
    filter_entry = vqec_dp_input_shim_filter_entry_create();
    if (!filter_entry) {
        status = VQEC_DP_STREAM_ERR_NOMEMORY;
        goto done;
    }

    /* If using an extra igmp multicast ip, bind to it here */
    if (fil->u.ipv4.rcc_extra_igmp_ip) {
        extra_igmp_sock = vqec_recv_sock_create_in_pool(
            s_vqec_recv_sock_pool,
            "",
            fil->u.ipv4.dst_ifc_ip,
            fil->u.ipv4.dst_port,
            fil->u.ipv4.rcc_extra_igmp_ip,
            FALSE,
            0 /* so_rcvbuf */,
            0 /* xmit_dscp_value */);
        if (!extra_igmp_sock) {
            /* Allocation failure */
            status = VQEC_DP_STREAM_ERR_INTERNAL;
            goto done;
        }
        if (!vqec_recv_sock_mcast_join(extra_igmp_sock, (struct in_addr){0})) {
            vqec_recv_sock_destroy_in_pool(s_vqec_recv_sock_pool,
                                           extra_igmp_sock);
            status = VQEC_DP_STREAM_ERR_INTERNAL;
            goto done;
        }
        filter_entry->extra_igmp_socket = extra_igmp_sock;
    }
    
    /* Success:  assign the remaining parameters to the filter entry */
    filter_entry->scheduling_class = scheduling_class;
    filter_entry->xmit_dscp_value = xmit_dscp_value;
    memcpy(&filter_entry->filter, fil, sizeof(vqec_dp_input_filter_t));
    filter_entry->os = os;
    filter_entry->so_rcvbuf = so_rcvbuf;
    /* The filter is not committed yet. */
    filter_entry->committed = FALSE;

    /* Link to the filter from the OS */
    os->filter_entry = filter_entry;

done:
    if (status != VQEC_DP_STREAM_ERR_OK) {
        vqec_dp_input_shim_filter_entry_destroy(filter_entry);
    } 
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM, "%s(os_id=0x%08x, filter=%s, "
                  "so_rcvbuf=%u, scheduling_class=%u, xmit_dscp_value=%u)%s\n",
                  __FUNCTION__, os_id,
                  vqec_dp_input_filter_to_str(fil, TRUE, TRUE, TRUE),
                  so_rcvbuf, scheduling_class, xmit_dscp_value,
                  vqec_dp_stream_err2str_complain_only(status));
    return (status);
}


/**
 * Commit any previously "reserved" bind for the OS. 
 *
 * @param[in]   os_id              A valid output stream handle.
 * @param[out]  *port              If the filter (provided during the 
 *                                  (reserved) destination port was 0,
 *                                  an unused port will be allocated and
 *                                  returned to the user in this parameter.
 *                                  (in network byte order)
 * @param[out]   *sock_fd          If a socket is used for the filter,
 *                                  return the fd of that socket for use
 *                                  in NAT injection.
 */
vqec_dp_stream_err_t 
vqec_dp_input_shim_os_bind_commit (vqec_dp_osid_t os_id, 
                                   uint16_t *port,
                                   int32_t  *sock_fd)
{
    in_addr_t rcv_if_address;
    in_addr_t mcast_group;
    struct in_addr saddr;
    vqec_dp_stream_err_t status = VQEC_DP_STREAM_ERR_OK;
    vqec_dp_input_shim_os_t *os;
    vqec_dp_input_filter_t *fil;
    vqec_recv_sock_t *vqec_recv_sock = NULL, *extra_igmp_sock;
    int modified_so_rcvbuf;
    
    if (vqec_dp_input_shim_status.is_shutdown) {
        status = VQEC_DP_STREAM_ERR_SERVICESHUT;
        goto done;
    }
    
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os || 
        !os->filter_entry ||
        !sock_fd ||
        (!port && !ntohs(os->filter_entry->filter.u.ipv4.dst_port)))  {
        status = VQEC_DP_STREAM_ERR_INVALIDARGS;
        goto done;
    }

    fil = &os->filter_entry->filter;
    *sock_fd = -1;

    /* If a socket is already bound fail the call - caller must unbind first */
    if (os->filter_entry->socket) {
        status = VQEC_DP_STREAM_ERR_OSALREADYBOUND;
        goto done;
    }

    /* The filter-entry must be in non-committed state. */
    if (os->filter_entry->committed) {
        status = VQEC_DP_STREAM_ERR_FILTERISCOMMITTED;
        goto done;
    }

    /* Validation succeeded:  create and bind the socket */
    if (IN_MULTICAST(ntohl(fil->u.ipv4.dst_ip))) {
        rcv_if_address = fil->u.ipv4.dst_ifc_ip;
        mcast_group = fil->u.ipv4.dst_ip;
    } else {
        rcv_if_address = fil->u.ipv4.dst_ip;
        mcast_group = 0;
    }

    /* If the extra igmp ip is in use, unbind it first */
    extra_igmp_sock = os->filter_entry->extra_igmp_socket;
    if (extra_igmp_sock) {
        if (!vqec_recv_sock_mcast_leave(extra_igmp_sock, (struct in_addr){0})) {
            status = VQEC_DP_STREAM_ERR_INTERNAL;
            goto done;
        }
        vqec_recv_sock_destroy_in_pool(s_vqec_recv_sock_pool,
                                       extra_igmp_sock);
        os->filter_entry->extra_igmp_socket = NULL;
    }

    vqec_recv_sock = vqec_recv_sock_create_in_pool(
                                           s_vqec_recv_sock_pool,
                                           "",
                                           rcv_if_address,
                                           fil->u.ipv4.dst_port,
                                           mcast_group,
                                           FALSE,
                                           os->filter_entry->so_rcvbuf,
                                           os->filter_entry->xmit_dscp_value);
    if (!vqec_recv_sock) {
        /* Allocation failure - state of the filter entry is unchanged. */
        status = VQEC_DP_STREAM_ERR_INTERNAL;
        goto done;
    }

    /* adjust SO_RCVBUF without restrictions (works in kernel-space only) */
    modified_so_rcvbuf = os->filter_entry->so_rcvbuf + 
        (vqec_dp_input_shim_pakpool_size *
         (vqec_dp_input_shim_max_paksize + vqec_recv_sock_get_pak_overhead()));
    (void)vqec_recv_sock_set_so_rcvbuf(vqec_recv_sock,
                                       (os->filter_entry->so_rcvbuf ?
                                        TRUE : FALSE),
                                       modified_so_rcvbuf);

    if (IN_MULTICAST(ntohl(fil->u.ipv4.dst_ip))) {
        /*
         * multicast destination:
         *   1. if requested, apply source filter of (addr)
         *   2. perform multicast join to receive traffic
         */
        saddr.s_addr = (fil->u.ipv4.src_ip_filter ? 
                        fil->u.ipv4.src_ip : INADDR_ANY);
        if (!vqec_recv_sock_mcast_join(vqec_recv_sock, saddr)) {
            status = VQEC_DP_STREAM_ERR_INTERNAL;
            goto done;
        }
    } else {
        /* unicast destination:
         *   1. if requested, apply source filter using (addr, port)
         */
        if (fil->u.ipv4.src_ip_filter || fil->u.ipv4.src_port_filter) {
            if (!vqec_recv_sock_connect(vqec_recv_sock,
                                        fil->u.ipv4.src_ip_filter,
                                        fil->u.ipv4.src_ip,
                                        fil->u.ipv4.src_port_filter,
                                        fil->u.ipv4.src_port)) {
                status = VQEC_DP_STREAM_ERR_INTERNAL;
                goto done;
            }
        }
    }
    
    /* 
     * Success:  assign the remaining parameteters to the filter entry:
     * the socket field is set at this point - it is assumed that there are no
     * further failures past this code point.
     */
    os->filter_entry->socket = vqec_recv_sock;
    if (!ntohs(fil->u.ipv4.dst_port)) {
        fil->u.ipv4.dst_port =
            vqec_recv_sock_get_port(vqec_recv_sock);
    }

    /* Insert the filter entry into the filter table */
    VQE_LIST_INSERT_HEAD(
        &vqec_dp_input_shim_filter_table[
            os->filter_entry->scheduling_class].filters,
        os->filter_entry, list_obj);
    
    /* Filter now in committed state. */
    os->filter_entry->committed = TRUE;

done:
    if (status != VQEC_DP_STREAM_ERR_OK) {
        if (port) {
            *port = htons(0);
        }        
        if (vqec_recv_sock) {
            vqec_recv_sock_destroy_in_pool(s_vqec_recv_sock_pool,
                                           vqec_recv_sock);
        }
    } else {
        if (!ntohs(fil->u.ipv4.dst_port)) {
            *port = vqec_recv_sock_get_port(vqec_recv_sock);
        }
        *sock_fd = vqec_recv_sock_get_fd(vqec_recv_sock);
    }
    if (port) {
        snprintf(s_debug_str_short, DEBUG_STR_LEN_SHORT, "%u", *port);
    } else {
        snprintf(s_debug_str_short, DEBUG_STR_LEN_SHORT, "NULL");
    }
    if (sock_fd) {
        snprintf(s_debug_str_short2, DEBUG_STR_LEN_SHORT, "%d", *sock_fd);
    } else {
        snprintf(s_debug_str_short2, DEBUG_STR_LEN_SHORT, "NULL");        
    }
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM, "%s(os_id=0x%08x, "
                  "port=%s, sock_fd=%s)%s\n",
                  __FUNCTION__, os_id,
                  s_debug_str_short, s_debug_str_short2,
                  vqec_dp_stream_err2str_complain_only(status));
    return (status);    
}

/**
 * Updates a output stream's existing binding.  Currently, just the OS 
 * source filter (IPv4 address and ports) may be updated.  
 *
 * The supplied OS must be bound and may be in either the reserved or 
 * committed state.
 *
 * See the function header comment of the vqec_dp_input_shim_os_bind() API
 * for usage and restrictions on supported source filters.
 *
 * Note:  This API supports source filter updates for unicast streams only.
 *
 * @param[in]   os_id              A valid output stream handle.
 * @param[in]   fil                Pointer to the input filter (with IP
 *                                  addresses and ports in network byte 
 *                                  order).  Only the source filter
 *                                  fields will be used.
 * @param[out]  vqec_dp_stream_err_t  Returns STREAM_ERR_OK on success.
 */
vqec_dp_stream_err_t
vqec_dp_input_shim_os_bind_update (vqec_dp_osid_t os_id,
                                   vqec_dp_input_filter_t *fil)
{
    vqec_dp_stream_err_t status = VQEC_DP_STREAM_ERR_OK;
    vqec_dp_input_shim_os_t *os;

    if ((os_id == VQEC_DP_INVALID_OSID) || !fil) {
        status = VQEC_DP_STREAM_ERR_INVALIDARGS;
        goto done;
    }

    if (vqec_dp_input_shim_status.is_shutdown) {
        status = VQEC_DP_STREAM_ERR_SERVICESHUT;
        goto done;
    }
    
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os) {
        status = VQEC_DP_ERR_NOSUCHSTREAM;
        goto done;
    }

    /* Verify OS is already bound to a source filter */
    if (!os->filter_entry) {
        status = VQEC_DP_STREAM_ERR_FILTERNOTSET;
        goto done;
    }

    /*
     * Verify filter update is being applied on a unicast stream.
     */
    if (IN_MULTICAST(ntohl(os->filter_entry->filter.u.ipv4.dst_ip))) {
        status = VQEC_DP_STREAM_ERR_FILTERUPDATEUNSUPPORTED;
        goto done;
    }

    /*
     * Check for any of the following unsupported filter situations:
     *  1. Filter destination is unicast, a src port filter is requested
     *     but a src addr filter is not
     */
    if (fil->u.ipv4.src_port_filter && !fil->u.ipv4.src_ip_filter) {
        status = VQEC_DP_STREAM_ERR_FILTERUNSUPPORTED;
        goto done;
    }

    /* If filter is committed, attempt to update socket */
    if (os->filter_entry->committed) {
        /*
         * unicast destination:
         *   1. apply source filter using (addr, port)
         */
        if (!vqec_recv_sock_connect(os->filter_entry->socket,
                                    fil->u.ipv4.src_ip_filter,
                                    fil->u.ipv4.src_ip,
                                    fil->u.ipv4.src_port_filter,
                                    fil->u.ipv4.src_port)) {
            status = VQEC_DP_STREAM_ERR_INTERNAL;
            goto done;
        }
    }
    
    /* Update source filter entry */
    os->filter_entry->filter.u.ipv4.src_ip_filter = 
        fil->u.ipv4.src_ip_filter;
    os->filter_entry->filter.u.ipv4.src_ip = fil->u.ipv4.src_ip;
    os->filter_entry->filter.u.ipv4.src_port_filter = 
        fil->u.ipv4.src_port_filter;
    os->filter_entry->filter.u.ipv4.src_port = fil->u.ipv4.src_port;

 done:
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM, "%s(os_id=0x%08x, filter=%s)%s\n",
                  __FUNCTION__, os_id, 
                  vqec_dp_input_filter_to_str(fil, FALSE, TRUE, FALSE),
                  vqec_dp_stream_err2str_complain_only(status));
    return (status);
}

/**
 * Poll the OS on-demand for any data packets. This service is 
 * qualified by the PUSH_POLL flag. Upon calling this method the
 * data packets will be delivered within the context of this
 * method by the inputshim. 
 *
 * @param[in]   os_id              A valid output stream handle.
 */
vqec_dp_stream_err_t 
vqec_dp_input_shim_os_poll_push (vqec_dp_osid_t os_id)
{
    vqec_dp_stream_err_t status = VQEC_DP_STREAM_ERR_OK;
    vqec_dp_input_shim_os_t *os;
    
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os || 
        !os->filter_entry ||
        !os->filter_entry->socket) {
        status = VQEC_DP_STREAM_ERR_INVALIDARGS;        
        goto done;
    }

    vqec_dp_input_shim_run_service_filter_entry(os->filter_entry); 
done:
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM, "%s(os_id=0x%08x)%s\n",
                  __FUNCTION__, os_id,
                  vqec_dp_stream_err2str_complain_only(status));
    return (status); 
}


/*
 * vqec_dp_input_shim_os_ops
 *
 * The input shim implements an API for using its Output Stream objects.  
 * This API (used by Input Stream objects) is defined by the function
 * pointers below.
 */
static const vqec_dp_osops_t vqec_dp_input_shim_os_ops = {
    .capa = vqec_dp_input_shim_os_capa,
    .accept_connect = vqec_dp_input_shim_os_accept_connect,
    .disconnect = vqec_dp_input_shim_os_disconnect,
    .bind = vqec_dp_input_shim_os_bind,
    .unbind = vqec_dp_input_shim_os_unbind,
    .get_status = vqec_dp_input_shim_os_get_status,
    .get_connected_is = vqec_dp_input_shim_os_get_connected_is,
    .backpressure = NULL,
    .read = NULL,
    .read_raw = NULL,
    .supported_encaps = vqec_dp_input_shim_os_supported_encaps,
    .bind_reserve = vqec_dp_input_shim_os_bind_reserve,
    .bind_commit = vqec_dp_input_shim_os_bind_commit,
    .bind_update = vqec_dp_input_shim_os_bind_update,
    .poll_push = vqec_dp_input_shim_os_poll_push 
};

/*
 * vqec_dp_input_shim_startup()
 *
 * Start the input shim services. Users of the input shim must call this 
 * method prior to using its services for the 1st time, or restarting it
 * after a shutdown. The behavior may be assumed unpredictable
 * if these guidelines are not followed. The shim may allocate, and 
 * initialize internal resources upon this method's invocation.
 *
 * @param[in] params  Startup configuration parameters.
 *                    See the type definition comments for usage information.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_input_shim_startup (vqec_dp_module_init_params_t *params)
{
    vqec_dp_error_t status = VQEC_DP_ERR_OK;
    int i;
    
    if (!params->max_paksize ||
        !params->max_channels ||
        !params->max_streams_per_channel ||
        (!params->scheduling_policy.max_classes || 
         (params->scheduling_policy.max_classes >
          VQEC_DP_SCHEDULING_CLASSES_MAX))) {
        status = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }
    for (i=0; i<params->scheduling_policy.max_classes; i++) {
        if (!params->scheduling_policy.polling_interval[i]) {
            status = VQEC_DP_ERR_INVALIDARGS;
            goto done;
        }
    }
    if (!vqec_dp_input_shim_status.is_shutdown) {
        status = VQEC_DP_ERR_ALREADY_INITIALIZED;
        goto done;
    }

    /* Allocate resource zones */

    s_vqec_filter_entry_pool = zone_instance_get_loc(
                        "filter_entry",
                        O_CREAT,
                        sizeof(vqec_filter_entry_t),
                        params->max_channels * params->max_streams_per_channel,
                        NULL, NULL);
    if (!s_vqec_filter_entry_pool) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }

    s_vqec_input_shim_os_pool = zone_instance_get_loc(
                        "ishim_os_pool",
                        O_CREAT,
                        sizeof(vqec_dp_input_shim_os_t),
                        params->max_channels * params->max_streams_per_channel,
                        NULL, NULL);
    if (!s_vqec_input_shim_os_pool) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }

    s_vqec_recv_sock_pool = vqec_recv_sock_pool_create(
                        "ishim_recvsock",
                        params->max_channels * params->max_streams_per_channel);
    if (!s_vqec_recv_sock_pool) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }

    /*
     * Upon creating an output stream ID database, the maximum size of
     * the database is defined up front, in terms of:
     *     numTableSlot  - number of slots in the OS ID table
     *     numIDsPerSlot - number of OS IDs within each slot
     * The maximum number of OS IDs available is thus bound by
     *     numTableSlot x numIDsPerSlot
     * The ID manager allocates memory in two instances:
     *   1) upon table creation (by allocating table storage for
     *      numTableSlot pointer entries), and
     *   2) upon OS ID allocation (by allocating a new slot with
     *      numIDsPerSlot, if no unused IDs exist within slots
     *      already allocated)
     *
     * NOTE:  since the minimum value for numIDsPerSlot is 2,
     *        ensure this value upon creating the table. 
     */
    vqec_dp_input_shim_os_id_table_key =
        id_create_new_table(params->max_channels,
                            params->max_streams_per_channel >= 2 ?
                            params->max_streams_per_channel : 2);
    if (vqec_dp_input_shim_os_id_table_key == ID_MGR_TABLE_KEY_ILLEGAL) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }
    VQE_LIST_INIT(&vqec_dp_input_shim_os_list);

    /* Cache the pak pool ID, and allocate/init a static packet */
    s_input_shim_pak_pool = zone_instance_get_loc(
                                "ishim_pak_pool",
                                O_CREAT,
                                sizeof(vqec_pak_t) + params->max_paksize,
                                1, NULL, NULL);
    if (!s_input_shim_pak_pool) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }
    s_vqec_dp_input_shim_pak = 
        (vqec_pak_t *) zone_acquire(s_input_shim_pak_pool); 
    if (!s_vqec_dp_input_shim_pak) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }
    *((int32_t *)&s_vqec_dp_input_shim_pak->alloc_len) = params->max_paksize;
    s_vqec_dp_input_shim_pak->buff = (char *)(s_vqec_dp_input_shim_pak+1);

    s_input_shim_filter_table_pool = zone_instance_get_loc(
                                "ishim_filttab",
                                O_CREAT,
                                params->scheduling_policy.max_classes *
                                    sizeof(vqec_dp_scheduling_class_t),
                                1, NULL, NULL);
    if (!s_input_shim_filter_table_pool) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }
    vqec_dp_input_shim_filter_table = 
        (vqec_dp_scheduling_class_t *) zone_acquire(s_input_shim_filter_table_pool);
    if (!vqec_dp_input_shim_filter_table) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }
    vqec_dp_input_shim_num_scheduling_classes = 
        params->scheduling_policy.max_classes;
    for (i = 0; i<vqec_dp_input_shim_num_scheduling_classes; i++) {
        vqec_dp_input_shim_filter_table[i].interval =
            params->scheduling_policy.polling_interval[i];
        vqec_dp_input_shim_filter_table[i].remaining = 0;
        VQE_LIST_INIT(&vqec_dp_input_shim_filter_table[i].filters);
    }

    vqec_dp_input_shim_max_paksize = params->max_paksize;
    vqec_dp_input_shim_pakpool_size = params->pakpool_size;

    /* Note:  input shim stats persist across shutdown/startup sequences */
    vqec_dp_input_shim_status.is_shutdown = FALSE;

done:
    if ((status != VQEC_DP_ERR_OK) &&
        (status != VQEC_DP_ERR_ALREADY_INITIALIZED))
    {
        if (s_vqec_filter_entry_pool) {
            (void) zone_instance_put(s_vqec_filter_entry_pool);
            s_vqec_filter_entry_pool = NULL;
        }
        if (s_vqec_input_shim_os_pool) {
            (void) zone_instance_put(s_vqec_input_shim_os_pool);
            s_vqec_input_shim_os_pool = NULL;
        }
        if (s_vqec_recv_sock_pool) {
            vqec_recv_sock_pool_destroy(s_vqec_recv_sock_pool);
            s_vqec_recv_sock_pool = NULL;
        }
        if (vqec_dp_input_shim_os_id_table_key != ID_MGR_TABLE_KEY_ILLEGAL) {
            id_destroy_table(vqec_dp_input_shim_os_id_table_key);
            vqec_dp_input_shim_os_id_table_key = ID_MGR_TABLE_KEY_ILLEGAL;
        }
        if (vqec_dp_input_shim_filter_table) {
            zone_release(s_input_shim_filter_table_pool, 
                         vqec_dp_input_shim_filter_table);
            vqec_dp_input_shim_filter_table = NULL;
        }
        if (s_input_shim_filter_table_pool) {
            (void) zone_instance_put(s_input_shim_filter_table_pool);
            s_input_shim_filter_table_pool = NULL;
        }
        if (s_vqec_dp_input_shim_pak) {
            zone_release(s_input_shim_pak_pool, s_vqec_dp_input_shim_pak);
            s_vqec_dp_input_shim_pak = NULL;
        }
        if (s_input_shim_pak_pool) {
            (void) zone_instance_put(s_input_shim_pak_pool);
            s_input_shim_pak_pool = NULL;
        }
    }
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM,
                  "%s(max_channels=%u, max_streams_per_channel=%u, "
                  "max_classes=%u)%s\n",
                  __FUNCTION__, params->max_channels,
                  params->max_streams_per_channel,
                  params->scheduling_policy.max_classes,
                  vqec_dp_err2str_complain_only(status));
    return (status);
}

/*
 * vqec_dp_input_shim_destroy_os_internal()
 *
 * Destroys an OS, and frees its associated resources.
 * NOTE:  the OS MUST be removed from any databases prior
 *        to calling this function; otherwise, any pointers to this
 *        OS will become invalid with its destruction
 */
void
vqec_dp_input_shim_destroy_os_internal (vqec_dp_input_shim_os_t *os)
{
    id_delete(os->os_id, vqec_dp_input_shim_os_id_table_key);
    if (os->filter_entry) {
        if (os->filter_entry->committed) {
            VQE_LIST_REMOVE(os->filter_entry, list_obj);
        }
        vqec_dp_input_shim_filter_entry_destroy(os->filter_entry);
    }
    zone_release(s_vqec_input_shim_os_pool, os);
    vqec_dp_input_shim_status.os_destroys++;
}

/*
 * vqec_dp_input_shim_shutdown()
 *
 * Shutdown input shim. If the input shim is already shutdown
 * no action will be taken. The following behavior is expected:
 *
 * (a) Allocation of new resources should no longer be
 * allowed.
 *
 * (b) If there are existing output streams, the creator of these
 * streams, which has an implicit reference on them, must 
 * destroy them explicitly, and they must not be destroyed 
 * at this time. The expected behavior is that the input shim
 * will stop delivering new packets to input streams, and
 * flush it's internal queues. It must also disallow new connections, 
 * and creation of filter bindings in the post-shutdown state.
 *
 * (c) All other resources must be released.
 */
void
vqec_dp_input_shim_shutdown (void)
{
    vqec_dp_input_shim_os_t *os, *os_next;

    if (vqec_dp_input_shim_status.is_shutdown) {
        goto done;
    }
    
    /* Walk the OS database and destroy the OSes */
    VQE_LIST_FOREACH_SAFE(os, &vqec_dp_input_shim_os_list, list_obj, os_next) {
        VQE_LIST_REMOVE(os, list_obj);
        vqec_dp_input_shim_destroy_os_internal(os);        
    }

    /*
     * Free the ID manager's OS table.
     */
    id_destroy_table(vqec_dp_input_shim_os_id_table_key);
    vqec_dp_input_shim_os_id_table_key = ID_MGR_TABLE_KEY_ILLEGAL;

    vqec_dp_input_shim_num_scheduling_classes = 0;

    zone_release(s_input_shim_filter_table_pool, 
                 vqec_dp_input_shim_filter_table);
    vqec_dp_input_shim_filter_table = NULL;
    (void) zone_instance_put(s_input_shim_filter_table_pool);
    s_input_shim_filter_table_pool = NULL;

    zone_release(s_input_shim_pak_pool, s_vqec_dp_input_shim_pak);
    s_vqec_dp_input_shim_pak = NULL;
    (void) zone_instance_put(s_input_shim_pak_pool);
    s_input_shim_pak_pool = NULL;

    /* Deallocate resource zones */
    (void) zone_instance_put(s_vqec_filter_entry_pool);
    s_vqec_filter_entry_pool = NULL;
    (void) zone_instance_put(s_vqec_input_shim_os_pool);
    s_vqec_input_shim_os_pool = NULL;
    vqec_recv_sock_pool_destroy(s_vqec_recv_sock_pool);
    s_vqec_recv_sock_pool = NULL;

    vqec_dp_input_shim_status.is_shutdown = TRUE;

done:
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM, "%s()\n", __FUNCTION__);
    return;
}

/**
 * Create input shim OS and bind it.
 */
static boolean
vqec_dp_inputshim_setup_stream (struct vqec_dp_input_stream_ *streaminfo,
                                vqec_dp_os_instance_t *os,
                                boolean commit_bind)
{
    in_port_t l_port;
    int l_fd;
    
    /* create the OS */
    if (vqec_dp_input_shim_create_os(&os->id,
                                     &os->ops,
                                     VQEC_DP_ENCAP_RTP)
        != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "can't create inputshim OS");
        return FALSE;
    }

    /* bind the OS with the bind configuration; capture the eph_repair_port */
    if (!os->ops->bind) {
        return FALSE;
    }

    if (commit_bind) {

        if (VQEC_DP_STREAM_ERR_OK ==
            os->ops->bind(os->id,
                          &streaminfo->filter,
                          &l_port,
                          &l_fd,
                          streaminfo->so_rcvbuf,
                          streaminfo->scheduling_class,
                          streaminfo->rtcp_dscp_value)) {
            return TRUE;
        }

    } else {

        if (VQEC_DP_STREAM_ERR_OK ==
            os->ops->bind_reserve(os->id,
                                  &streaminfo->filter,
                                  streaminfo->so_rcvbuf,
                                  streaminfo->scheduling_class,
                                  streaminfo->rtcp_dscp_value)) {
            return TRUE;
        }

    }

    return FALSE;
}

/**
 * Create a new per-channel input shim instance.  Return an input shim output
 * object with the created output stream information.
 */
vqec_dp_error_t
vqec_dp_input_shim_create_instance (vqec_dp_chan_desc_t *desc,
                                    vqec_dp_output_stream_obj_t *obj)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    /* create inputshim streams for primary and, as needed: repair, fec0, fec1 */
    if (!vqec_dp_inputshim_setup_stream(&desc->primary,
                                        &obj->os[
                                            VQEC_DP_IO_STREAM_TYPE_PRIMARY],
                                        !desc->en_rcc)) {
        err = VQEC_DP_ERR_NOMEM;
        goto bail;
    }

    if (desc->en_repair || desc->en_rcc) {
        if (!vqec_dp_inputshim_setup_stream(&desc->repair,
                                            &obj->os[
                                                VQEC_DP_IO_STREAM_TYPE_REPAIR],
                                            TRUE)) {
            err = VQEC_DP_ERR_NOMEM;
            goto bail;
        }
    }

    if (desc->en_fec0) {
        if (!vqec_dp_inputshim_setup_stream(&desc->fec_0,
                                            &obj->os[
                                                VQEC_DP_IO_STREAM_TYPE_FEC_0],
                                            !desc->en_rcc)) {
            err = VQEC_DP_ERR_NOMEM;
            goto bail;
        }
    }

    if (desc->en_fec1) {
        if (!vqec_dp_inputshim_setup_stream(&desc->fec_1,
                                            &obj->os[
                                                VQEC_DP_IO_STREAM_TYPE_FEC_1],
                                            !desc->en_rcc)) {
            err = VQEC_DP_ERR_NOMEM;
            goto bail;
        }
    }

    return (err);

bail:
    if (obj->os[VQEC_DP_IO_STREAM_TYPE_PRIMARY].id != VQEC_DP_INVALID_OSID) {
        vqec_dp_input_shim_destroy_os(
            obj->os[VQEC_DP_IO_STREAM_TYPE_PRIMARY].id);
    }
    if (obj->os[VQEC_DP_IO_STREAM_TYPE_REPAIR].id != VQEC_DP_INVALID_OSID) {
        vqec_dp_input_shim_destroy_os(
            obj->os[VQEC_DP_IO_STREAM_TYPE_REPAIR].id);
    }
    if (obj->os[VQEC_DP_IO_STREAM_TYPE_FEC_0].id != VQEC_DP_INVALID_OSID) {
        vqec_dp_input_shim_destroy_os(obj->os[VQEC_DP_IO_STREAM_TYPE_FEC_0].id);
    }
    if (obj->os[VQEC_DP_IO_STREAM_TYPE_FEC_1].id != VQEC_DP_INVALID_OSID) {
        vqec_dp_input_shim_destroy_os(obj->os[VQEC_DP_IO_STREAM_TYPE_FEC_1].id);
    }

    memset(obj, 0, sizeof(*obj));

    return (err);
}

/**
 * Destroy a per-channel input shim instance.
 */
vqec_dp_error_t
vqec_dp_input_shim_destroy_instance (vqec_dp_output_stream_obj_t *obj)
{
    if (!obj) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    if (obj->os[VQEC_DP_IO_STREAM_TYPE_PRIMARY].id != VQEC_DP_INVALID_OSID) {
        vqec_dp_input_shim_destroy_os(
            obj->os[VQEC_DP_IO_STREAM_TYPE_PRIMARY].id);
    }
    if (obj->os[VQEC_DP_IO_STREAM_TYPE_REPAIR].id != VQEC_DP_INVALID_OSID) {
        vqec_dp_input_shim_destroy_os(
            obj->os[VQEC_DP_IO_STREAM_TYPE_REPAIR].id);
    }
    if (obj->os[VQEC_DP_IO_STREAM_TYPE_FEC_0].id != VQEC_DP_INVALID_OSID) {
        vqec_dp_input_shim_destroy_os(obj->os[VQEC_DP_IO_STREAM_TYPE_FEC_0].id);
    }
    if (obj->os[VQEC_DP_IO_STREAM_TYPE_FEC_1].id != VQEC_DP_INVALID_OSID) {
        vqec_dp_input_shim_destroy_os(obj->os[VQEC_DP_IO_STREAM_TYPE_FEC_1].id);
    }

    return VQEC_DP_ERR_OK;
}

/**
 * Get the ephemeral RTP repair port associated with a (repair) stream.
 *
 * @param[in]  osid  ID of the (repair) output stream.
 * @param[out]  in_port_t  Returns a port number, or 0 if failed.
 */
#define VQEC_INPUT_SHIM_EPH_PORT_UNKNOWN (0)
in_port_t
vqec_dp_input_shim_get_eph_port (vqec_dp_osid_t osid)
{
    vqec_dp_input_shim_os_t *p_os;

    p_os = vqec_dp_input_shim_os_id_to_os(osid);

    if (!p_os ||
        !p_os->filter_entry ||
        !p_os->filter_entry->socket) {
        return (VQEC_INPUT_SHIM_EPH_PORT_UNKNOWN);
    }

    return p_os->filter_entry->socket->port;
}

/**---------------------------------------------------------------------------
 * Inject a packet through the socket filter that is bound to the inputshim
 * output stream for the repair session. A hack used for injecting NAT packets
 * from the control-plane. Hence this is a "turn"-ed  message, i.e., the
 * message is being sent back towards the "inbound" path.
 * 
 * @param[in] osid Identifier of a output stream.
 * @param[in] remote_addr Remote IP address.
 * @param[in] remote_port Remote IP port.
 * @param[in] bufp Pointer to the buffer.
 * @param[in] len Length of the content buffer.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_input_shim_repair_inject (vqec_dp_osid_t osid,
                                  in_addr_t remote_addr,
                                  in_port_t remote_port,
                                  char *bufp,
                                  uint16_t len)
{
    int32_t tx_len;  
    struct sockaddr_in dest_addr;
    vqec_dp_input_shim_os_t *p_os;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    p_os = vqec_dp_input_shim_os_id_to_os(osid);
    if (!p_os) {
        err = VQEC_DP_ERR_NOSUCHSTREAM;
        return (err);
    }

    if (!p_os->filter_entry ||
        !p_os->filter_entry->socket) {
        err = VQEC_DP_ERR_NOT_FOUND;
        return (err);
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = remote_addr;
    dest_addr.sin_port = remote_port;
        
    tx_len = vqec_recv_sock_sendto(p_os->filter_entry->socket,
                                   bufp, 
                                   len,
                                   0,
                                   (struct sockaddr *)&dest_addr,
                                   sizeof(struct sockaddr_in));
    if (len != tx_len) {
        err = VQEC_DP_ERR_INTERNAL;
    }

    return (err);
}

/**---------------------------------------------------------------------------
 * Inject a packet through the socket filter that is bound to the inputshim
 * output stream for the primary session. A hack used for injecting NAT packets
 * from the control-plane. Hence this is a "turn"-ed  message, i.e., the
 * message is being sent back towards the "inbound" path.
 * 
 * @param[in] osid Identifier of a output stream.
 * @param[in] remote_addr Remote IP address.
 * @param[in] remote_port Remote IP port.
 * @param[in] bufp Pointer to the buffer.
 * @param[in] len Length of the content buffer.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 *---------------------------------------------------------------------------*/

vqec_dp_error_t
vqec_dp_input_shim_primary_inject (vqec_dp_osid_t osid,
                                   in_addr_t remote_addr,
                                   in_port_t remote_port,
                                   char *bufp,
                                   uint16_t len)
{
    return vqec_dp_input_shim_repair_inject(osid,
                                            remote_addr,
                                            remote_port,
                                            bufp,len);
}


/*
 * vqec_dp_input_shim_create_os()
 *
 * Create a new output stream, and return an opaque handle to it.
 * If resources cannot be allocated for the stream, an
 * invalid handle must be returned. 
 * 
 * @param[out] osid  Pointer to a valid os handle on success, and an
 * invalid handle on failure.
 * @param[out] os_ops Pointer to the constant data structure representing
 * the set of methods defining the output stream interface, or NULL on failure
 * @param[in] encap Defines the stream encapsulation for this OS
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK 
 * on success. 
 */
vqec_dp_error_t
vqec_dp_input_shim_create_os (vqec_dp_osid_t *osid,
                              const vqec_dp_osops_t **os_ops,
                              vqec_dp_encap_type_t encap)
{
    vqec_dp_error_t status = VQEC_DP_ERR_OK;
    vqec_dp_input_shim_os_t *os = NULL;

    if (!osid || !os_ops ||
        ((encap != VQEC_DP_ENCAP_RTP) && (encap != VQEC_DP_ENCAP_UDP))) {
        status = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    if (vqec_dp_input_shim_status.is_shutdown) {
        status = VQEC_DP_ERR_SHUTDOWN;
        goto done;
    }

    os = (vqec_dp_input_shim_os_t *)
        zone_acquire(s_vqec_input_shim_os_pool);
    if (!os) {
        status = VQEC_DP_ERR_NOMEM;
        goto done;
    }
    memset(os, 0, sizeof(vqec_dp_input_shim_os_t));

    *osid = id_get(os, vqec_dp_input_shim_os_id_table_key);
    if (*osid == VQEC_DP_INVALID_OSID) {
        status = VQEC_DP_ERR_NOMORESTREAMS;
        goto done;
    }

    /* OS creation successful */
    os->capa = (VQEC_DP_STREAM_CAPA_PUSH | 
                VQEC_DP_STREAM_CAPA_PUSH_VECTORED |
                VQEC_DP_STREAM_CAPA_PUSH_POLL);
    os->os_id = *osid;
    os->encaps = encap;
    os->is_id = VQEC_DP_INVALID_ISID;
    os->is_ops = NULL;
    VQE_LIST_INSERT_HEAD(&vqec_dp_input_shim_os_list, os, list_obj);
    vqec_dp_input_shim_status.os_creates++;    

    *os_ops = &vqec_dp_input_shim_os_ops;

done:
    if (status != VQEC_DP_STREAM_ERR_OK) {
        if (os) {
            zone_release(s_vqec_input_shim_os_pool, os);
        }
        if (osid && *osid != VQEC_DP_INVALID_OSID) {
            id_delete(*osid, vqec_dp_input_shim_os_id_table_key);
            *osid = VQEC_DP_INVALID_OSID;
        }
        if (os_ops) {
            *os_ops = NULL;
        }
    }
    if (osid) {
        if (osid == VQEC_DP_INVALID_OSID) {
            snprintf(s_debug_str_short, DEBUG_STR_LEN_SHORT, "<invalid>");
        } else {
            snprintf(s_debug_str_short, DEBUG_STR_LEN_SHORT, "0x%08x", *osid);
        }
    } else {
        snprintf(s_debug_str_short, DEBUG_STR_LEN_SHORT, "NULL");
    }
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM,
                  "%s(osid=%s, os_ops=%p, encap=%s)%s\n",
                  __FUNCTION__, s_debug_str_short, os_ops,
                  vqec_dp_stream_encap2str(encap, s_debug_str, DEBUG_STR_LEN),
                  vqec_dp_err2str_complain_only(status));
    return (status);
}

/*
 * vqec_dp_input_shim_destroy_os()
 *
 * Destroy the specified output stream. If there is a filter binding it should
 * be broken. All resources allocated for the stream should be released.
 *
 * @param[in] osid Handle of the output stream to destroy.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on 
 *                                      success. 
 */
vqec_dp_error_t
vqec_dp_input_shim_destroy_os (vqec_dp_osid_t os_id)
{
    vqec_dp_error_t status = VQEC_DP_ERR_OK;
    vqec_dp_input_shim_os_t *os;

    /* Validate arguments */
    if (os_id == VQEC_DP_INVALID_OSID) {
        status = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    /* Arguments are valid, destroy the OS if it exists */
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (os) {
        VQE_LIST_REMOVE(os, list_obj);
        vqec_dp_input_shim_destroy_os_internal(os);
    }

done:
    VQEC_DP_DEBUG(VQEC_DP_DEBUG_INPUTSHIM, "%s(os_id=0x%08x)%s\n",
                  __FUNCTION__, os_id,
                  vqec_dp_err2str_complain_only(status));
    return (status);
}

/*
 * vqec_dp_input_shim_get_next_os()
 *
 * Part of the iterator to scan all output streams that have been created.
 * This method returns the opaque handle of the "next" output stream  
 * given the "prev" handle. The ordering between "next" and "prev" is
 *  impelementation specific. If there are no more output streams, an 
 * invalid handle is returned.The following behavior is sought:
 *
 * (a)  Atomic semantics for the iterator may be enforced by the caller, if
 * needed, since a race exists between the addition of new output streams, 
 * and termination of the iteration. 
 * (b) If no output streams are created or destroyed, an implementation 
 * of the iterator must return each output stream's handle exactly once
 * before terminating.
 * (c) The behavior, when the caller does not impose atomic semantics,
 * and the collection of output streams is modified, is left unspecified.
 *
 * @param[in] prev_os Handle of the "previous" output stream handed out
 * by the iterator.
 * @param[out] vqec_os_id_t Returns either the handle of the next os or
 * the invalid handle if there are no additional output streams.
 */
vqec_dp_osid_t
vqec_dp_input_shim_get_next_os (vqec_dp_osid_t prev_os_id)
{
    vqec_dp_input_shim_os_t *os_prev, *os_next;
    vqec_dp_osid_t os_id_next = VQEC_DP_INVALID_OSID;

    os_prev = vqec_dp_input_shim_os_id_to_os(prev_os_id);
    if (!os_prev) {
        goto done;
    }
    
    os_next = VQE_LIST_NEXT(os_prev, list_obj);
    if (!os_next) {
        goto done;
    }

    os_id_next = os_next->os_id;

done:
    return (os_id_next);
}

/*
 * vqec_dp_input_shim_get_first_os()
 *
 * Part of the iterator to scan all output streams that have been created.
 * This method returns the opaque handle of the "first" output stream. The 
 * ordering between "first" and "next" is impelementation-specific. If there 
 * are no output streams, an invalid handle is returned.
 *
 * @param[out] vqec_os_id_t Returns either the handle of the first os or
 * the invalid handle if there are no output streams.
 */
vqec_dp_osid_t
vqec_dp_input_shim_get_first_os (void)
{
    vqec_dp_input_shim_os_t *os;
    vqec_dp_osid_t os_id = VQEC_DP_INVALID_OSID;

    os = VQE_LIST_FIRST(&vqec_dp_input_shim_os_list);
    if (!os) {
        goto done;
    }

    os_id = os->os_id;
done:
    return (os_id);
}

/*
 * vqec_dp_input_shim_get_status()
 *
 * Get status information from the input shim.
 *
 * @param[out] status The global status of the input shim.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success. 
 */
RPC vqec_dp_error_t
vqec_dp_input_shim_get_status (OUT vqec_dp_input_shim_status_t *status)
{
    vqec_dp_error_t status_retcode = VQEC_DP_ERR_OK;

    if (!status) {
        status_retcode = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    memcpy(status, &vqec_dp_input_shim_status,
           sizeof(vqec_dp_input_shim_status_t));

done:
    return (status_retcode);
}

/**
 * Get filter table from input shim module.
 *
 * @param[out] filters array of filters to be populated by this function
 * @param[in/out] num_filters as input, number of filters the passed-in
 *                            filter array can hold; as output, number of
 *                            filters that were actually copied into the array
 */
vqec_dp_error_t
vqec_dp_input_shim_get_filters (vqec_dp_display_ifilter_t *filters,
                                uint32_t count, uint32_t *num_filters)
{
    vqec_filter_entry_t *filter_entry;
    int i, j = 0;

    if (!filters || !num_filters) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    for (i = 0; i < vqec_dp_input_shim_num_scheduling_classes; i++) {
        VQE_LIST_FOREACH(filter_entry,
                     &vqec_dp_input_shim_filter_table[i].filters,
                     list_obj) {
            if (j == count) {
                break;
            }
            filters[j].filter = filter_entry->filter;
            filters[j].scheduling_class = filter_entry->scheduling_class;
            j++;
        }
    }

    *num_filters = j;
    return VQEC_DP_ERR_OK;
}

/**
 * Gets information about a single input shim output stream.
 *
 * @param[in] os_id            - output stream of interest
 * @param[out] stream          - stream information
 * @param[out] vqec_dp_error_t - VQEC_DP_ERR_OK if success, or
 *                               error code indicating failure reason
 */
vqec_dp_error_t
vqec_dp_input_shim_get_stream (vqec_dp_osid_t os_id,
                               vqec_dp_display_os_t *stream)
{
    vqec_dp_input_shim_os_t *os;
    
    if (!stream) {
        return VQEC_DP_ERR_INVALIDARGS;
    }
    os = vqec_dp_input_shim_os_id_to_os(os_id);
    if (!os) {
        return VQEC_DP_ERR_INVALIDARGS;
    }
    stream->os_id = os->os_id;
    stream->encaps = os->encaps;
    stream->capa = os->capa;
    stream->has_filter = FALSE;  /* set to TRUE next if exists */
    if (os->filter_entry) {
        stream->has_filter = TRUE;
        stream->filter.filter = os->filter_entry->filter;
        stream->filter.scheduling_class =
            os->filter_entry->scheduling_class;
    }
    stream->is_id = os->is_id;
    stream->packets = os->stats.packets;
    stream->bytes = os->stats.bytes;
    stream->drops = os->stats.drops;
    return VQEC_DP_ERR_OK;    
}

/**
 * Get stream information from the input shim module.
 * Can be used to get a set of streams the caller cares about
 * (via the "requested_streams" and "requested_streams_array_size"
 * parameters) or an arbitrary set not specified by ID (e.g. all streams).
 *
 * @param[in]  requested_streams - (optional) an array of input shim os_id
 *                                  values corresponding to the streams for
 *                                  which information is requested.
 *                                 NULL must be passed if the caller does not
 *                                  want to request particular streams by ID.
 *                                 If a request includes an os_id with
 *                                  value VQEC_DP_INVALID_OSID, then the
 *                                  corresponding entry in the "returned
 *                                  streams" array will be zero'd out.
 *                                 If a request is made for an os_id which
 *                                  cannot be found, processing aborts and
 *                                  the error code VQEC_DP_ERR_NOSUCHSTREAM
 *                                  is returned.
 * @param[in]  requested_streams_array_size -
 *                                 Size of "requested_streams" array, with 
 *                                  a value of 0 implying that the caller 
 *                                  does not want to request particular 
 *                                  streams by ID.
 * @param[out] returned_streams  - Array into which requested stream info
 *                                  is copied.  If a "requested_streams"
 *                                  array was supplied, then the returned
 *                                  streams in this array will correspond
 *                                  (based on index position) to IDs supplied
 *                                  in the "requested_streams" array.
 * @param[in]  returned_streams_array_size -
 *                                 Size of "returned_streams" array
 * @param[out] returned_streams_count -
 *                                 Number of streams whose information have
 *                                  been copied into the "returned_streams" 
 *                                  array.  Includes streams which have
 *                                  zero'd out based on a request for a
 *                                  stream with ID of VQEC_DP_INVALID_OSID.
 * @param[out] vqec_dp_error_t   - VQEC_DP_ERR_OK upon success, or
 *                                  an error code indicating reason for failure
 */
vqec_dp_error_t
vqec_dp_input_shim_get_streams (
    vqec_dp_isid_t        *requested_streams,
    uint32_t              requested_streams_array_size,
    vqec_dp_display_os_t  *returned_streams,
    uint32_t              returned_streams_array_size,
    uint32_t              *returned_streams_count)
{
    vqec_dp_osid_t osid;
    int i;
    int j = 0;  /* number of returned streams */
    vqec_dp_error_t status = VQEC_DP_ERR_OK;

    /*
     * Reject requests which:
     * 1. supply a "requested_streams" array w/o an array size
     * 2. supply no "requested_streams" array but give an array size
     * 3. supply no "returned_streams" array
     * 4. supply a "returned_streams" array size of 0
     * 5. supply no place to put a count of the number of streams returned
     */
    if ((requested_streams && !requested_streams_array_size) ||
        (!requested_streams && requested_streams_array_size) ||
        (!returned_streams) ||
        (!returned_streams_array_size) ||
        (!returned_streams_count)) {
        status = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    if (requested_streams) {
        /* Walk the streams requested by the caller */
        for (i = 0;
             i < requested_streams_array_size &&
                 (j < returned_streams_array_size);
             i++) {
            if (requested_streams[i] == VQEC_DP_INVALID_OSID) {
                memset(&returned_streams[j], 0, sizeof(vqec_dp_display_os_t));
            } else {
                status = vqec_dp_input_shim_get_stream(requested_streams[i],
                                                       &returned_streams[j]);
                if (status != VQEC_DP_ERR_OK) {
                    goto done;
                }
            }
            j++;
        }
    } else {
        /* Walk the existing streams by arbitrary order */
        for (osid = vqec_dp_input_shim_get_first_os();
             osid != VQEC_DP_INVALID_OSID && (j < returned_streams_array_size);
             osid = vqec_dp_input_shim_get_next_os(osid)) {
            status = vqec_dp_input_shim_get_stream(osid, &returned_streams[j]);
            if (status != VQEC_DP_ERR_OK) {
                goto done;
            }
            j++;
        }
    }
done:
    *returned_streams_count = j;
    return (status);
}

/**
 * Open an RTP socket in the dataplane. 
 *
 * @param[out]  addr  IP address of newly opened socket
 * @param[out]  port  port number of newly opened socket
 * @param[out]  vqec_dp_error_t  VQEC_DP_ERR_OK upon success
 */
vqec_dp_error_t
vqec_dp_input_shim_open_sock(in_addr_t *addr, in_port_t *port)
{
    if (vqec_recv_sock_open(addr, port, s_vqec_recv_sock_pool)) {
        return VQEC_DP_ERR_OK;
    } else {
        return VQEC_DP_ERR_INTERNAL;
    }
}

/**
 * Close an RTP socket in the dataplane.
 * 
 * @param[in]  addr  IP address of open socket
 * @param[in]  port  port number of open socket
 * @param[out] vqec_dp_error_t  VQEC_DP_ERR_OK upon success
 */
vqec_dp_error_t
vqec_dp_input_shim_close_sock(in_addr_t addr, in_port_t port)
{
    vqec_recv_sock_close(addr, port);
    return VQEC_DP_ERR_OK;
}

