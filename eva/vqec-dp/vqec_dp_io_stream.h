/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Input/Output stream interface.
 *
 * Documents:
 *
 *****************************************************************************/
#ifndef __VQEC_DP_IO_STREAM_H__
#define __VQEC_DP_IO_STREAM_H__

#include <vqec_dp_api_types.h>
#include <vqec_pak.h>
#include <strl.h>

/**
 * Forward declarations.
 */
struct vqec_dp_isops_;
struct vqec_dp_osops_;

/**
  * @defgroup iostream Input & Output Streams.
  * @{
  */

/*----------------------------------------------------------------------------
 * Common to input & output streams.
 *---------------------------------------------------------------------------*/

/**
 * Input / output stream error defines.
 *
 * NOTE:  If updating this list, please update the vqec_dp_stream_err2str()
 *        function also.
 */
typedef 
enum vqec_dp_stream_err_
{
    VQEC_DP_STREAM_ERR_OK,                 /*!< Success  */
    VQEC_DP_STREAM_ERR_NACKCAPA,           /*!< Capability negotiation failed */
    VQEC_DP_STREAM_ERR_NOSUCHSTREAM,       /*!< Stream not found */
    VQEC_DP_STREAM_ERR_INVALIDARGS,        /*!< Invalid arguments */
    VQEC_DP_STREAM_ERR_DUPFILTER,          /*!< Duplicate filter */
    VQEC_DP_STREAM_ERR_SERVICESHUT,        /*!< Service is shutdown */
    VQEC_DP_STREAM_ERR_ENCAPSMISMATCH,     /*!< Requested encaps not avail */
    VQEC_DP_STREAM_ERR_OSALREADYCONNECTED, /*!< Output Stream is connected */
    VQEC_DP_STREAM_ERR_OSALREADYBOUND,     /*!< Output Stream is bound */
    VQEC_DP_STREAM_ERR_FILTERUNSUPPORTED,  /*!< Applied filter not supported */
    VQEC_DP_STREAM_ERR_NOMEMORY,           /*!< Insufficient memory */
    VQEC_DP_STREAM_ERR_FILTERISCOMMITTED,  /*!< Filter is already committed */
    VQEC_DP_STREAM_ERR_FILTERNOTSET,       /*!<
                                            *!< Filter update requires
                                            *!< pre-existing filter.
                                            */
    VQEC_DP_STREAM_ERR_FILTERUPDATEUNSUPPORTED,
                                           /*!<
                                            *!< Filter updates only supported
                                            *!< on unicast streams.
                                            */
    VQEC_DP_STREAM_ERR_INTERNAL,           /*!< Internal error */
} vqec_dp_stream_err_t;

/**
 * Returns a descriptive error string corresponding to a vqec_dp_stream_err_t
 * error code.  If the error code is invalid, an empty string is returned.
 *
 * @param[in] err           Error code
 * @param[out] const char * Descriptive string corresponding to error code
 */ 
const char *
vqec_dp_stream_err2str(vqec_dp_stream_err_t err);

/**
 * Capabilities that may be supported by input and output streams. 
 * Capabilities are symmetric, i.e., if a particular capability can be
 * supported between an input and output stream, if both the input
 * and output stream support that capability.
 */
#define VQEC_DP_STREAM_CAPA_NULL  0x0
                                /*!< Empty capability value */
#define VQEC_DP_STREAM_CAPA_PUSH  0x1      
                                /*!< If data push is needed or supported */
#define VQEC_DP_STREAM_CAPA_PUSH_VECTORED 0x2
                                /*!< If vectored push is needed or supported */
#define VQEC_DP_STREAM_CAPA_PULL  0x4      
                                /*!< If data pull is needed or supported  */
#define VQEC_DP_STREAM_CAPA_BACKPRESSURE  0x8
                                /*!< If backpressure is needed or supported */
#define VQEC_DP_STREAM_CAPA_RAW 0x10       
                                /*!< If raw reads are needed or supported */
#define VQEC_DP_STREAM_CAPA_PUSH_POLL 0x20       
                                /*!< If a push can be initiated with polling */
#define VQEC_DP_STREAM_CAPA_MAX ((VQEC_DP_STREAM_CAPA_PUSH_POLL << 1) - 1)
                                /*<! Highest supported capability mask */

/**
 * Input or output stream statistics.
 */
typedef
struct vqec_dp_stream_stats_
{
    uint64_t packets;           /*!< Received or transmitted packets */
    uint64_t bytes;             /*!< Received or transmitted bytes */
    uint64_t drops;             /*!< Input or output drops */
} vqec_dp_stream_stats_t;

/**
 * Input stream opaque identifier.
 */
typedef vqec_dp_streamid_t vqec_dp_isid_t; 
/**
 * Output stream opaque identifier.
 */
typedef vqec_dp_streamid_t vqec_dp_osid_t;
/**
 * Invalid handle for input stream.
 */
#define VQEC_DP_INVALID_ISID  VQEC_DP_STREAMID_INVALID 
/**
 * Invalid handle for output stream.
 */
#define VQEC_DP_INVALID_OSID  VQEC_DP_STREAMID_INVALID 

/**
 * An "instance" of an input stream (IS) including the 
 * stream ID and it's operations object.
 */
typedef
struct vqec_dp_is_instance_
{

    vqec_dp_isid_t id;
    const struct vqec_dp_isops_ *ops;

} vqec_dp_is_instance_t;

/**
 * An "instance" of an output stream (OS) including the 
 * stream ID and it's operations object.
 */
typedef
struct vqec_dp_os_instance_
{

    vqec_dp_osid_t id;
    const struct vqec_dp_osops_ *ops;

} vqec_dp_os_instance_t;

/**
 * Indexes used for primary, repair, FEC 0, and FEC 1 input streams, and
 * output streams.  No distinction is made between row and column for FEC,
 * since it may not be known apriori which configuration corresponds to row
 * FEC, and which to column FEC.
 */
typedef enum vqec_dp_io_stream_type_
{
    VQEC_DP_IO_STREAM_TYPE_PRIMARY = 0,
    VQEC_DP_IO_STREAM_TYPE_REPAIR,
    VQEC_DP_IO_STREAM_TYPE_FEC_0,
    VQEC_DP_IO_STREAM_TYPE_FEC_1,
    VQEC_DP_IO_STREAM_TYPE_POSTREPAIR,
    VQEC_DP_IO_STREAM_TYPE_MAX,
} vqec_dp_io_stream_type_t;

#define VQEC_DP_IO_STREAM_TYPE_MIN (VQEC_DP_IO_STREAM_TYPE_PRIMARY)

/*
 * Objects to be used for input and output streams (and connections thereof) of
 * module instances for each channel in the VQEC DP.
 */

typedef struct vqec_dp_output_stream_obj_
{
    /*
     * Output stream instances.
     */
    vqec_dp_os_instance_t os[VQEC_DP_IO_STREAM_TYPE_MAX];
} vqec_dp_output_stream_obj_t;

typedef struct vqec_dp_input_stream_obj_
{
    /*
     * Input stream instances.
     */
    vqec_dp_is_instance_t is[VQEC_DP_IO_STREAM_TYPE_MAX];
} vqec_dp_input_stream_obj_t;

/* 
 * Maximum number of vectored packets to push to an IS from an OS at a time
 */
#define VQEC_DP_STREAM_PUSH_VECTOR_PAKS_MAX  64

/**
 * To-string functions for printing types and capabilities.
 */
#define VQEC_DP_STREAM_PRINT_BUF_SIZE 64

/**
 * Return a string representing the given capabilities.
 *
 * @param[in] capa Given set of capabilities.
 * @param[out] buf Buffer to print into.
 * @param[in] buf_len Length of buf.
 * @param[out] return Pointer to buf.
 */
const char *
vqec_dp_stream_capa2str(uint32_t capa,
                        char *buf,
                        int buf_len);

/**
 * Return a string representing the given encapsulation.
 *
 * @param[in] capa Given encapsulation.
 * @param[out] buf Buffer to print into.
 * @param[in] buf_len Length of buf.
 * @param[out] return Pointer to buf.
 */
const char *
vqec_dp_stream_encap2str(vqec_dp_encap_type_t encap,
                         char *buf,
                         int buf_len);

/*----------------------------------------------------------------------------
 * Input streams.
 *---------------------------------------------------------------------------*/


/**
 * Input stream status ADT.
 */
typedef 
struct vqec_dp_is_status_
{

    vqec_dp_osid_t os;          /*!< Connected output stream id */
    vqec_dp_stream_stats_t stats;
                                /*!< Stream statistics */

} vqec_dp_is_status_t;

/**
 * Input stream operations data structure. Not all methods are mandatory
 * for all input stream implementations, as described in the documentation
 * below. 
 */
typedef 
struct vqec_dp_isops_
{	
    /**
     * Get the capability flags for the input stream. 
     *
     * @param[in] os A valid input stream handle.
     * @param[out] int32_t Stream's capability flags.
     */
    int32_t (*capa)(vqec_dp_isid_t is);

    /**
     * Initiate a connection from the input stream to the given
     * output stream. The input stream shall first get the
     * capabilities of the output stream, compare them to it's
     * own capabilities, and request a preferred subset of their
     * intersection for the connection by invoking the 
     * accept_connect method on the output stream. If there
     * are no capabilities in the intersection, or the set is 
     * insufficient to build a service, the method must fail. 
     * It must not have any visible side-effects when it fails.
     *
     * Otherwise, the method will logically connect the input
     * and output streams together, and cache the output stream
     * handle, and operations interface. The output stream will
     * then deliver datagrams to the input stream through it's
     * own packet receive interface.
     *
     * @param[in] is A valid input stream handle.
     * @param[in] os The output stream handle to which a connection
     * should be established.
     * @param[in] ops* Output stream operations interface.
     * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK on
     * success.
     */
    vqec_dp_stream_err_t (*initiate_connect)(vqec_dp_isid_t is,
                                             vqec_dp_osid_t os, 
                                             const struct vqec_dp_osops_ *ops);

    /**
     * Qualified by the PUSH capability.
     *
     * Receive a packet from an output stream.  If the input handle 
     * is a valid input stream, and pak is a valid pointer, the method returns
     * ERR_OK. If an output stream has multiple input streams that it's
     * delivering a single packet to, the packet may be cloned before it is
     * delivered to each of them (implementation-specific). 
     *
     * @param[in] is Input stream's handle to which packet is delivered.
     * @param[in] pak Packet pointer.
     * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK when
     * given a valid input stream handle, and a valid packet pointer.
     */
    vqec_dp_stream_err_t (*receive)(vqec_dp_isid_t is, vqec_pak_t *pak); 

    /**
     * Qualified by the PUSH capability.
     *
     * Receive a vector of packets from an output stream.  If the input handle 
     * is a valid input stream, and pakvec is a valid pointer array, the 
     * method returns ERR_OK. If an output stream has multiple input streams
     * that it's delivering a single packet to, a packet may be cloned before
     * it is delivered to each of them (implementation-specific). 
     *
     * @param[in] is Input stream's handle to which packet is delivered.
     * @param[in] pakvec Packet vector pointer.
     * @param[in] vecnum Number of packets in the vector.
     * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK when
     * given a valid input stream handle, and a valid packet pointer array.
     */
    vqec_dp_stream_err_t (*receive_vec)(vqec_dp_isid_t is, 
                                        vqec_pak_t *pakvec[],
                                        uint32_t vecnum); 

    /**
     * Get stream status for an input stream. This includes the id of
     * (any) connected output streams, and statistics. An implementation 
     * may choose not to keep per input statistics. The statistics should be 
     * reset to 0 for that case.
     *
     * @param[in] is A valid input stream handle.
     * @param[in] cumulative Boolean flag to retrieve cumulative stats
     * @param[out] status Status structure for the result.
     * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK on
     * success.
     */
    vqec_dp_stream_err_t (*get_status)(vqec_dp_isid_t is,
                                       vqec_dp_is_status_t *stat, 
                                       boolean cumulative);

    /**
     * Get the encapsulation type for the input stream. 
     *
     * @param[in] os A valid input stream handle.
     * @param[out] dp_encap_type_t Stream's encapsulation type.
     */
    vqec_dp_encap_type_t (*encap)(vqec_dp_isid_t is);

} vqec_dp_isops_t;

/*----------------------------------------------------------------------------
 * Output streams.
 *---------------------------------------------------------------------------*/

/**
 * Output stream status ADT.
 */
typedef 
struct vqec_dp_os_status_
{

    uint16_t is_cnt;            /*!< number of connected input streams */
    vqec_dp_input_filter_t filter;
                                /*!< Input filter binding (if any)  */
    vqec_dp_stream_stats_t stats;
                                /*!< Stream statistics */

} vqec_dp_os_status_t;

/**
 * Output stream operations data structure. Not all methods are mandatory
 * for all output stream implementations, as described in the documentation
 * below. 
 */
typedef 
struct vqec_dp_osops_
{
    /**
     * Get the capability flags for the output stream. 
     *
     * @param[in] os A valid output stream handle.
     * @param[out] int32_t Stream's capability flags.
     */
    int32_t (*capa)(vqec_dp_osid_t os);

    /**
     * Connect the given input and output streams together. If the 
     * capabilities requested in the parameter req_capa are either 
     * ambiguous, e.g., both data push and data pull or cannot be
     * supported, the method must fail. It must not have any
     * visible side-effects when it fails - a subsequent connect with
     * modified & supported capabilities must be allowed to succeed.
     *
     * Otherwise, the method must logically connect the output
     * and input streams together, cache the input stream operations
     * interface reference, and deliver datagrams to the
     * input stream through that interface.
     *
     * An implementation may accept multiple connections on
     * one output stream, i.e., allow multiple input streams to 
     * connect to it. For this particular case, the packets
     * delivered to input streams may be cloned (implementation specific).
     *
     * @param[in] os A valid output stream handle.
     * @param[in] is A valid input stream handle.
     * @param[in] ops* Input stream operations interface.
     * @param[in] encap Requested encapsulation type.
     * @param[in] req_capa Requested capabilities.
     * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK on
     * success.
     */
    vqec_dp_stream_err_t (*accept_connect)(vqec_dp_osid_t os,
                                           vqec_dp_isid_t is, 
                                           const vqec_dp_isops_t *ops, 
                                           vqec_dp_encap_type_t encap,
                                           int32_t req_capa);

    /**
     * Disconnect the output stream from any input stream that
     * it may have been connected to via an earlier connect call.
     *
     * @param[in] os A valid output stream handle.
     */
    void (*disconnect)(vqec_dp_osid_t os);

    /**
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
     *   1. if either a source address or source port filter is supplied,
     *       then BOTH must be supplied
     *
     * Callers may specify a 0 for the destination port when using a unicast
     * destination address.  In this case, an unused destination port will 
     * be allocated and kept open as long as the output stream remains bound.
     * The allocated port must be returned back to the caller in *port.
     *
     * Callers may give the value INADDR_ANY for the destination IP address.
     * The meaning of INADDR_ANY as a destination IP address differs as 
     * follows:
     *
     *    If a source address filter and source port filter are NOT supplied, 
     *    then only packets arriving that have filter's destination port and a
     *    destination IP address that matches the address configured on *any* 
     *    interface on the box will be considered a match.
     *
     *    If a source address filter and source port filter ARE supplied, then
     *    ONLY packets arriving that have the filter's destination port and
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
     *                                  addresses and ports in network byte 
     *                                  order)
     * @param[out]  *port              If the filter destination port is 0,
     *                                  an unused port will be allocated and
     *                                  returned to the user in this parameter.
     *                                  (in network byte order)
     * @param[ou]   *sock_fd           If the socket is used for the filter,
     *                                  return the fd of that socket for use
     *                                  in NAT injection.
     * @param[in]   so_rcvbuf          Maximum number of arriving bytes that
     *                                  may be buffered by the OS, prior to it
     *                                  being serviced.  Arriving bytes in 
     *                                  excess of this limit will be dropped.
     * @param[in]   scheduling_class   Associates the filter with the given
     *                                  scheduling_class (e.g. 0,1,2,3...).
     *                                  See vqec_dp_input_shim_run_service() 
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
    vqec_dp_stream_err_t (*bind)(vqec_dp_osid_t os, 
                                 vqec_dp_input_filter_t *fil,
                                 uint16_t *port,
                                 int32_t  *sock_fd,
                                 uint32_t so_rcvbuf,
                                 uint32_t scheduling_class,
                                 uint8_t xmit_dscp_value);

    /**
     * Remove any previous binding for the given output stream.
     *
     * @param[in] os A valid output stream handle.
     */
    void (*unbind)(vqec_dp_osid_t os);

    /**
     * Get stream status for an output stream. This includes the number of
     * connected input streams, the filter binding (if any), and
     * statistics. An implementation may choose not to keep per output
     * statistics. The statistics should be reset to 0 for that case.
     *
     * @param[in] os A valid output stream handle.
     * @param[out] status Status structure for the result.
     * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK on
     * success.
     */
    vqec_dp_stream_err_t (*get_status)(vqec_dp_osid_t os,
                                       vqec_dp_os_status_t *stat);
    
    /**
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
     * @param[out] isid_array      Array into which the ISes of os_id are 
     *                              copied
     * @param[in]  isid_array_len  Size of isid_array (in elements)
     * @param[out] num_isids       Number of IS IDs copied into isid_array
     * @param[out] more            TRUE if there are more IS IDs to be
     *                              retrieved for OS, or FALSE otherwise.
     * @param[out] vqec_dp_stream_err_t  Success/failure status of request
     */ 
    vqec_dp_stream_err_t (*get_connected_is)(vqec_dp_osid_t os_id,
                                             vqec_dp_isid_t is_id_last,
                                             vqec_dp_isid_t *isid_array,
                                             uint32_t isid_array_len,
                                             uint32_t *num_isids,
                                             boolean *more);

    /**
     * Qualified by the BACKPRESSURE capability.
     *
     * This method is invoked by the input stream connected to
     * an output stream to backpressure a "push"-styled packet transfer
     * interface. If a particular output stream implementation supports
     * backpressure, then it must maintain a Q per input stream which 
     * is connected to the output stream to support this feature.  
     * The input stream will assert backpressure by calling this
     * method with is_active set to TRUE, and de-assert it by invoking
     * the method with is_active set to FALSE. 
     * 
     * (Reserved for future enhancement - used when the BACPRESSURE
     * capability flags are set)..
     *
     * @param[in] os A valid output stream handle.
     * @param[in] is_active Input stream is asserting backpressure when
     * is_active is TRUE, and de-asserting it when is_active is FALSE.
     */
    void (*backpressure)(vqec_dp_isid_t os, boolean is_active);

    /**
     * Qualified by the PULL capability.
     *
     * Pull mechanism to read datagrams from a output stream. To
     * support this capability the output stream must maintain a Q
     * per connected input stream, and shall block a calling input
     * stream context (thread) if the Q is empty, based on the value
     * of the timeout parameter. 
     *
     * Each received datagram is copied into one buffer in the 
     * iobuf input array. If the received datagram is too big to fit in
     * the buffer, it is truncated, and the TRUNCATED buffer flag is
     * set. The buffer's write length field is set to the length of data
     * that has been copied.
     *
     * *bytes_read is updated to return the total number of bytes
     *  that have copied. If no bytes are copied, it is set to 0.
     *
     * If no datagrams are available and:
     *  
     * (a) timeout is 0, it returns immediately. *bytes_read is 0.
     *
     * (b) timeout is -1, it blocks until at least 1 datagram is available.
     *
     * (c)  0 < timeout < MAX_ALLOWED_TIMEOUT, it blocks until at least
     * 1 datagram is available or the timeout is reached.
     * 
     * @param[in] os A valid output stream handle.
     * @param[in / out] iobuf Pointer to the output buffer array. The buffer
     * write lengths are updated as individual datagrams are copied.
     * @param[in] count Number of buffer elements in the iobuf array.
     * @param[out] bytes_read* Aggregate number of bytes copied into
     * the buffer array.
     * @param[in] timeout_msec Time in msec that the method should
     * block the caller to wait for the arrival of at least 1 datagram.
     * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK if
     * no errors were encountered during the execution of this method.
     */
    vqec_dp_stream_err_t (*read)(vqec_dp_isid_t os, 
                         vqec_iobuf_t *iobuf, 
                         uint32_t count, 
                         uint32_t *bytes_read,
                         int32_t timeout_msec);

    /**
     * Qualified by the presence of both PULL and RAW capabilities.
     *
     * Pull mechanism to read multiple datagrams contiguously into a
     * single buffer. 
     *
     * To support this capability the output stream must maintain a Q
     * per connected input stream, and shall block a calling input
     * stream context (thread) if the Q is empty, based on the value
     * of the timeout parameter. 
     *
     * The length of the buffer bufp, is provided in the input parameter
     * *len. Available datagrams are copied into the buffer until the
     * remaining length of the buffer is insufficient to hold the next full 
     * datagram, or there aren't any additional datagrams.  
     *
     * *len is updated to return the total number of bytes
     *  that have copied. If no bytes are copied, it is set to 0.
     *
     * If no datagrams are available and:
     *  
     * (a) timeout is 0, it returns immediately. *len is 0.
     *
     * (b) timeout is -1, it blocks until at least 1 datagram is available.
     *
     * (c)  0 < timeout < MAX_ALLOWED_TIMEOUT, it blocks until at least
     * 1 datagram is available or the timeout is reached.
     * 
     * @param[in] os A valid output stream handle.
     * @param[out] bufp Pointer to the output data buffer.
     * @param[in / out] len* Upon input, the total length of the buffer.
     * Upon return from the call, it is set the aggregate number of bytes 
     * copied into the buffer.
     * @param[in] timeout_msec Time in msec that the method should
     * block the caller to wait for the arrival of at least 1 datagram.
     * @param[out] vqec_dp_stream_err_t Returns VQEC_DP_STREAM_ERR_OK if
     * no errors were encountered during the execution of this method.
     */
    vqec_dp_stream_err_t (*read_raw)(vqec_dp_isid_t os, uint8_t *bufp, 
                                  uint32_t *len, int32_t timeout_msec); 

    /**
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
    vqec_dp_stream_err_t (*supported_encaps)(vqec_dp_osid_t os, 
		vqec_dp_encap_type_t *encaps, uint32_t *num);

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
    vqec_dp_stream_err_t (*bind_reserve)(vqec_dp_osid_t os, 
                                         vqec_dp_input_filter_t *fil,
                                         uint32_t so_rcvbuf,
                                         uint32_t scheduling_class,
                                         uint8_t xmit_dscp_value);


    /**
     * Commit any previously "reserved" bind for the OS. 
     *
     * @param[in]   os_id              A valid output stream handle.
     * @param[out]  *port              If the filter (provided during the 
     *                                  (reserved) destination port was 0,
     *                                  an unused port will be allocated and
     *                                  returned to the user in this parameter.
     *                                  (in network byte order)
     * @param[ou]   *sock_fd           If a socket is used for the filter,
     *                                  return the fd of that socket for use
     *                                  in NAT injection.
     */
    vqec_dp_stream_err_t (*bind_commit)(vqec_dp_osid_t os, 
                                        uint16_t *port,
                                        int32_t  *sock_fd);

   /**
     * Updates an output stream's existing binding.  Currently, just the OS 
     * source filter may be updated.  The OS may be in either the reserved or
     * committed state.
     *
     * See the function header comment of the "bind" method for usage and
     * restrictions on supported source filters.
     *
     * @param[in]   os_id              A valid output stream handle.
     * @param[in]   fil                Pointer to the input filter (with IP
     *                                  addresses and ports in network byte 
     *                                  order).  Only the source filter
     *                                  fields will be used.
     * @param[out]  vqec_dp_stream_err_t  Returns STREAM_ERR_OK on success.
     */
    vqec_dp_stream_err_t (*bind_update)(vqec_dp_osid_t os, 
                                        vqec_dp_input_filter_t *fil);

    /**
     * Poll the OS on-demand for any data packets. This service is 
     * qualified by the PUSH_POLL flag. Upon calling this method the
     * data packets will be delivered within the context of this
     * method by the inputshim. 
     *
     * @param[in]   os_id              A valid output stream handle.
     */
    vqec_dp_stream_err_t (*poll_push)(vqec_dp_osid_t os);

} vqec_dp_osops_t;

/**
 * @}
 */

#endif /* __VQEC_DP_IO_STREAM_H__ */
