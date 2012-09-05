/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File:  vqec_dp_input_shim_api.h
 *
 * Description: Input shim interface.
 *
 * Documents:
 *
 *****************************************************************************/
#ifndef __VQEC_DP_INPUT_SHIM_API_H__
#define __VQEC_DP_INPUT_SHIM_API_H__

#include <vqec_dp_api_types.h>
#include <vqec_dp_io_stream.h>
#include <vqec_dp_tlm.h>

/**
 * @defgroup ishim Input Shim.
 * @{
 */

/*
 * API re-entrancy
 *
 * None of the input shim APIs are expected to be re-entrant.  It is the 
 * caller's responsibility to ensure that concurrent access to the 
 * input shim's data structures are prevented.
 *
 * API endianness
 *
 * All IP addresses and ports that are passed through the input shim APIs
 * are assumed to be in NETWORK byte order
 */

/**
 * Start the input shim services. Users of the input shim must call this 
 * method prior to using it's services for the 1st time, or restarting it
 * after a shutdown. The behavior may be assumed unpredictable
 * if these guidelines are not followed. The shim may allocate, and 
 * initialize internal resources upon this method's invocation.
 *
 * @param[in] params  Startup configuration parameters.
 *                    See the type definition comments for usage information.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK 
 *                                      on success. 
 */
vqec_dp_error_t
vqec_dp_input_shim_startup(vqec_dp_module_init_params_t *params);

/**
 * Shutdown input shim. If the input shim is already shutdown
 * no action should be taken. The following behavior is expected:
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
vqec_dp_input_shim_shutdown(void);

/**
 * Create a new per-channel input shim instance.  Return an input shim output
 * object with the created output stream information.
 *
 * @param[in] desc  Channel descriptor.
 * @param[in] obj  Pointer to module output object.
 * @param[out] eph_repair_port  The ephemeral port of the repair socket.
 * @param[out] sock_fd  The FD of the repair socket.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_input_shim_create_instance(vqec_dp_chan_desc_t *desc,
                                   vqec_dp_output_stream_obj_t *obj);

/**
 * Destroy a per-channel input shim instance.
 *
 * @param[in] obj  Pointer to module output object.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_input_shim_destroy_instance(vqec_dp_output_stream_obj_t *obj);

/**
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
vqec_dp_input_shim_create_os(vqec_dp_osid_t *osid,
                             const vqec_dp_osops_t **os_ops,
                             vqec_dp_encap_type_t encap);

/**
 * Destroy the specified output stream. If there is a filter binding it should
 * be broken. All resources allocated for the stream should be released.
 *
 * @param[in] osid Handle of the output stream to destroy.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success. 
 */
vqec_dp_error_t
vqec_dp_input_shim_destroy_os(vqec_dp_osid_t osid);

/**
 * Part of the iterator to scan all output streams that have been created.
 * This method returns the opaque handle of the "first" output stream. The 
 * ordering between "first" and "next" is impelementation-specific. If there 
 * are no output streams, an invalid handle is returned.
 *
 * @param[out] vqec_os_id_t Returns either the handle of the first os or
 * the invalid handle if there are no output streams.
 */
vqec_dp_osid_t
vqec_dp_input_shim_get_first_os(void);

/**
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
vqec_dp_input_shim_get_next_os(vqec_dp_osid_t prev_os);

/**
 * Get the ephemeral RTP repair port associated with a (repair) stream.
 *
 * @param[in]  osid  ID of the (repair) output stream.
 * @param[out]  in_port_t  Returns a port number, or 0 if failed.
 */
in_port_t
vqec_dp_input_shim_get_eph_port(vqec_dp_osid_t osid);

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
vqec_dp_input_shim_repair_inject(vqec_dp_osid_t osid,
                                 in_addr_t remote_addr,
                                 in_port_t remote_port,
                                 char *bufp,
                                 uint16_t len);

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
vqec_dp_input_shim_primary_inject(vqec_dp_osid_t osid,
                                 in_addr_t remote_addr,
                                 in_port_t remote_port,
                                 char *bufp,
                                 uint16_t len);

/**
 * Service the input shim's output streams.
 *
 * This API is intended to be invoked periodically, with an "elapsed_time"
 * parameter indicating the amount of time (in milliseconds) that has elapsed
 * since the previous invocation.  All scheduling classes whose intervals
 * have terminated within the elapsed time period will be processed (any
 * packets that have arrived for an OS assigned to the scheduling class will
 * be forwared to its connected IS).
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
 *                          (if applicable)
 */
void
vqec_dp_input_shim_run_service(uint16_t elapsed_time);

/**
 * @}
 */

#endif /* __VQEC_DP_INPUT_SHIM_API_H__ */
