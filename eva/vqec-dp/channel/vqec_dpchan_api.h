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
 * Description: VQE-C Dataplane Channel external API for the graph builder.
 *
 * Documents:
 *
 *****************************************************************************/
#ifndef __VQEC_DPCHAN_API_H__
#define __VQEC_DPCHAN_API_H__

/**
 * Create a dpchan instance.
 *
 * @param[in] desc Channel description info from the dataplane.
 * @param[out] dpchan_input_obj The DP chan input stream module object.
 * @param[out] dpchan_output_obj The DP chan output stream module object.
 * @return Identifier of the dp channel instance.
 */
vqec_dp_chanid_t 
vqec_dpchan_create(vqec_dp_chan_desc_t *desc,
                   vqec_dp_input_stream_obj_t *dpchan_input_obj,
                   vqec_dp_os_instance_t *dpchan_os);

/**
 * Destroy a dpchan instance.
 * @param[in] chanid Identifier of the dpchan instance to be destroyed.
 */
void 
vqec_dpchan_destroy(vqec_dp_chanid_t chanid);

/**
 * Initialize the channel module.
 *
 * @param[in] params Dataplane initialization parameters.
 */
vqec_dp_error_t
vqec_dpchan_module_init(vqec_dp_module_init_params_t *params);

/**
 * Uninitialize the channel module. 
 */
void
vqec_dpchan_module_deinit(void);

/**
 * Event handler for the channel module; called periodically from the top
 * level to invoke channel services.
 * @param[in] cur_time Current absolute time.
 */
void 
vqec_dpchan_poll_ev_handler(abs_time_t cur_time);

#endif /* __VQEC_DPCHAN_API_H__ */
