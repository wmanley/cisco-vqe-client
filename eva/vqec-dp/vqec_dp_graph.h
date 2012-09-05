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
 * Description: VQE-C DP specific Graph Interface.  The VQE-C graph is
 *              responsible for creating/destroying, connecting/disconnecting,
 *              and operating on nodes in the dataplane such as the input shim,
 *              output shim, and dp channel nodes.
 *
 * Documents:
 *
 *****************************************************************************/
#ifndef __VQEC_DP_GRAPH_H__
#define __VQEC_DP_GRAPH_H__

#include <vqec_dp_io_stream.h>
#include <vqec_dp_output_shim_api.h>
#include <vqec_dpchan_api.h>
#include <vqec_dp_api_types.h>

/**
 * VQEC DP graph structure.
 */
typedef struct vqec_dp_graph_ {
    /**
     * Graph ID.
     */
    uint32_t id;

    /**
     * Input shim output object.
     */
    vqec_dp_output_stream_obj_t inputshim_output;

    /**
     * DP chan input object.
     */
    vqec_dp_input_stream_obj_t dpchan_input;
    /**
     * DP chan output object.
     */
    vqec_dp_output_stream_obj_t dpchan_output;
    /**
     * DP chan ID.
     */
    vqec_dp_chanid_t chanid;

    /**
     * Output shim input object.
     */
    vqec_dp_input_stream_obj_t outputshim_input;

    /**
     * Specifies the type of input stream that will be used for the outputshim.
     */
    int outputshim_stream_type;


} vqec_dp_graph_t;

/**
 * Initialize the graph module.  This must be called before any graph
 * operations are performed.
 *
 * @param[in]  params  VQE-C DP initialization parameters.
 * @param[out]  vqec_dp_error_t  Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_dp_graph_init_module(vqec_dp_module_init_params_t *params);

/**
 * Deinitialize the graph module.  After this is called, other graph
 * operations will not work properly.
 */
void vqec_dp_graph_deinit_module(void);

#endif /* __VQEC_DP_GRAPH_H__ */    

