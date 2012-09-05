/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Data-plane output shim interface.
 *
 * Documents:
 *
 *****************************************************************************/
#ifndef __VQEC_DP_OUTPUT_SHIM_API_H__
#define __VQEC_DP_OUTPUT_SHIM_API_H__

#include <vqec_dp_api.h>
#include <vqec_dp_api_types.h>
#include <vqec_dp_io_stream.h>

/**
 * @defgroup vqec_dp_output_shim_api Output Shim API's.
 * @{
*/

#if 0  /* this will only become relevent later for kernel-mode */
/**
 * Structure describing a tuner address identifier for a VQE client socket.
 * (only for kernel dataplanes).
 */  
typedef
struct vqec_sockaddr_tuner_
{
    /**
     * Address family: AF_INET.
     */
    sa_family_t sin_family;
    /**
     * Tuner identifier which identifies a tuner in the kernel dataplane.
     */
    int32_t tid;
    /**
     * Pad to size of `struct sockaddr'. 
     */
  unsigned char __pad[__SOCK_SIZE__ - sizeof(short int) - sizeof(int32_t) ];

} vqec_sockaddr_tuner_t;
#endif

/**
 * Start the output shim services. User's of the shim must call this 
 * method prior to using it's services for the 1st time, or restarting it
 * after a shutdown. The behavior may be assumed unpredictable
 * if these guidelines are not followed. The shim may allocate, and 
 * initialize internal resources upon this method's invocation.
 *
 * @param[in] params Initialization parameters.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success.
 */
vqec_dp_error_t
vqec_dp_output_shim_startup(vqec_dp_module_init_params_t *params);

/**
 * Shutdown output shim. If the shim is already shutdown
 * no action should be taken. The following behavior is expected:
 *
 * (a) Allocation of new resources should no longer be
 * allowed.
 *
 * (b) If there are existing tuners or input streams, the creator of
 *  these streams, which has an implicit reference on them, must 
 * destroy them explicitly, and they must not be destroyed 
 * at this time. The expected behavior is that all packets delivered
 * to the shim shall be dropped, and the shim will flush
 * it's internal queues. It must also disallow new connections, 
 * and creation of tuners in the post-shutdown state.
 *
 * (c) All other resources must be released.
 */
void vqec_dp_output_shim_shutdown(void);


/**
 * Create a new input stream, and return an opaque handle to it.
 * If resources cannot be allocated for the stream, an
 * invalid handle must be returned.
 *
 * @param[out] isid  Pointer to a valid IS handle on success, and an
 * invalid handle on failure.
 * @param[out] is_ops Pointer to the constant data structure representing
 * the set of methods defining the input stream interface, or NULL on failure
 * @param[in] encap Defines the stream encapsulation for this IS
 * @param[out] vqec_dp_input_shim_err_t Returns VQEC_DP_ERR_OK
 * on success.
 */
vqec_dp_error_t
vqec_dp_output_shim_create_is(vqec_dp_is_instance_t *is,
                              vqec_dp_encap_type_t encap);

/**
 * Destroy the specified input stream. If any tuners are attached to this
 * input stream, that binding shall be removed.
 * All resources allocated for the stream should be released.
 *
 * @param[in] id Handle of the input stream to destroy.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success. 
 */
vqec_dp_error_t vqec_dp_output_shim_destroy_is(vqec_dp_isid_t id);

/**
 * Establish a binding between the given IS and the tuner
 * identifier. The tuner's queue will be added to the list of queues
 * to which the IS enqueues incoming packets.
 *
 * @param[in] id Identifier of the input stream.
 * @param[in] tid Identifier of the tuner.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on
 * success. 
 */
vqec_dp_error_t
vqec_dp_output_shim_map_is_to_dptuner(vqec_dp_isid_t is, 
                                      vqec_dp_tunerid_t tid);

/**
 * Destroy the binding between the given tuner identifier and its mapped
 * IS.
 *
 * @param[in] tid Identifier of the tuner.
 * @param[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK
 * on success. 
 */
vqec_dp_error_t
vqec_dp_output_shim_unmap_dptuner(vqec_dp_tunerid_t tid);

/**
 * Cache channel info inside the tuner.
 *
 * @param[in] tid  Identifier of the tuner.
 * @param[in] chan  Pointer to the channel description structure.
 * @param[out] vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_output_shim_tuner_cache_channel(vqec_dp_tunerid_t tid,
                                        vqec_dp_chan_desc_t *chan);

#ifdef HAVE_SCHED_JITTER_HISTOGRAM
/**
 * Publish output jitter for all active tuners.
 *
 * @param[in] bufp buffer pointer in which to publish the data.
 * @param[in] len Length of buffer.
 * @param[out] int32_t Number of bytes written.
 */
int32_t
vqec_dp_output_shim_hist_publish(vqec_dp_outputshim_hist_t type,
                                        char *bufp,
                                        int32_t len);

#endif  /* HAVE_SCHED_JITTER_HISTOGRAM */
/**
 * @}
 */

#endif /* __VQEC_DP_OUTPUT_SHIM_API_H__ */
