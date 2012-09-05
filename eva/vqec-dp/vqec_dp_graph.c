/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: VQE-C Graph Builder implementation.
 *
 * Documents:
 *
 *****************************************************************************/

#include <vqec_dp_graph.h>
#include <vqec_dp_input_shim_api.h>
#include <utils/id_manager.h>
#include <utils/zone_mgr.h>

#ifdef _VQEC_DP_UTEST
#define UT_STATIC
#else
#define UT_STATIC static
#endif

static struct vqe_zone *s_vqec_graph_pool = NULL;

/* graph pointer caching, used to clean up stale state due to a sudden death */
static struct vqe_zone *s_graph_cache_pool = NULL;
static vqec_dp_graph_t **s_graph_cache = NULL;
static int graph_index = 0, s_graphs_max;

/* Graph ID table - stores the IDs for all the created graphs */
UT_STATIC id_table_key_t vqec_dp_graph_id_table_key =
    ID_MGR_TABLE_KEY_ILLEGAL;

/*
 * vqec_dp_graph_id_to_graph()
 *
 * @param[in]  id      Graph ID value
 * @return             pointer to corresponding graph object, or
 *                     NULL if no associated graph object
 */
UT_STATIC vqec_dp_graph_t *
vqec_dp_graph_id_to_graph (const vqec_dp_graphid_t id)
{
    uint ret_code;

    return (id_to_ptr(id, &ret_code, vqec_dp_graph_id_table_key));
}

#define VQEC_GRAPH_IDTABLE_BUCKETS 1
#define VQEC_GRAPH_MIN_GRAPHS 2  /* Id manager limitation */
UT_STATIC vqec_dp_error_t vqec_dp_graph_init_id_table (uint32_t channels)
{
    uint32_t graphs;

    graphs = channels;
    if (graphs < VQEC_GRAPH_MIN_GRAPHS) {
        graphs = VQEC_GRAPH_MIN_GRAPHS;
    }

    vqec_dp_graph_id_table_key =
        id_create_new_table(VQEC_GRAPH_IDTABLE_BUCKETS,
                            graphs);
    if (vqec_dp_graph_id_table_key == ID_MGR_TABLE_KEY_ILLEGAL) {
        return VQEC_DP_ERR_NOMEM;
    }

    return VQEC_DP_ERR_OK;
}

UT_STATIC void vqec_dp_graph_deinit_id_table (void)
{
    /* free the ID manager's graph ID table */
    id_destroy_table(vqec_dp_graph_id_table_key);
    vqec_dp_graph_id_table_key = ID_MGR_TABLE_KEY_ILLEGAL;
}

/*
 * Initialize the graph module.
 */
vqec_dp_error_t
vqec_dp_graph_init_module (vqec_dp_module_init_params_t *params)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (!params) {
        err = VQEC_DP_ERR_INVALIDARGS;
        goto done;
    }

    /* allocate resource zone */
    if (s_vqec_graph_pool) {
        err = VQEC_DP_ERR_ALREADY_INITIALIZED;
        goto done;
    }
    s_vqec_graph_pool = zone_instance_get_loc(
                            "dp_graph_pool",
                            O_CREAT,
                            sizeof(vqec_dp_graph_t),
                            params->max_channels,
                            NULL, NULL);
    if (!s_vqec_graph_pool) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "failed to allocate graph pool");
        err = VQEC_DP_ERR_NOMEM;
        goto done;
    }

    /* allocate and acquire zone for graph pointer cache */
    s_graph_cache_pool = zone_instance_get_loc(
        "graph_ptr_pool",
        O_CREAT,
        sizeof(vqec_dp_graph_t *) * params->max_channels,
        1,
        NULL, NULL);
    if (!s_graph_cache_pool) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "failed to allocate graph ptr cache pool");
        err = VQEC_DP_ERR_NOMEM;
        goto done;
    }
    s_graph_cache = zone_acquire(s_graph_cache_pool);
    if (!s_graph_cache) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "failed to acquire graph ptr cache pool");
        err = VQEC_DP_ERR_NOMEM;
        goto done;
    }

    /* initialize id_mgr table */
    if (VQEC_DP_ERR_OK != vqec_dp_graph_init_id_table(params->max_channels)) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "failed to init graph id table");
        err = VQEC_DP_ERR_INTERNAL;
        goto done;
    }

    /* set max number of graphs */
    s_graphs_max = params->max_channels;

  done:
    if (err != VQEC_DP_ERR_OK &&
        err != VQEC_DP_ERR_ALREADY_INITIALIZED) {
        if (s_vqec_graph_pool) {
            (void) zone_instance_put(s_vqec_graph_pool);
            s_vqec_graph_pool = NULL;
        }
        if (s_graph_cache_pool) {
            (void)zone_instance_put(s_graph_cache_pool);
            s_graph_cache_pool = NULL;
        }
            
    }
    return err;
}

/**
 * Deinitialize the graph module.
 *
 * NOTE:  Graph instances must be destroyed individually by their creators
 *        before this call is made, or else the pointers to these instances
 *        will be lost.
 */
void vqec_dp_graph_deinit_module (void)
{
    int i;

    /* destroy any remaining allocations in graph pool */
    for (i = 0; i < graph_index; i++) {
        if (s_graph_cache[i]) {
            vqec_dp_graph_destroy(s_graph_cache[i]->id);
        }
    }

    /* deinitialize id_mgr table */
    vqec_dp_graph_deinit_id_table();

    /* dealloc resource zone */
    if (s_vqec_graph_pool) {
        (void) zone_instance_put(s_vqec_graph_pool);
        s_vqec_graph_pool = NULL;
    }

    /* release and dealloc graph cache zone */
    if (s_graph_cache) {
        zone_release(s_graph_cache_pool, s_graph_cache);
        s_graph_cache = NULL;
    }
    if (s_graph_cache_pool) {
        (void)zone_instance_put(s_graph_cache_pool);
        s_graph_cache_pool = NULL;
    }
}

/**
 * Deinitialize the graph. The graph will be stopped, and all resources
 * will be released.
 */
UT_STATIC vqec_dp_error_t
vqec_dp_graph_deinit (vqec_dp_graph_t *graph)
{
    if (!graph) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    /* destroy outputshim streams */
    if (graph->outputshim_input.is[graph->outputshim_stream_type].id
        != VQEC_DP_INVALID_ISID) {
        vqec_dp_output_shim_destroy_is(
            graph->outputshim_input.is[graph->outputshim_stream_type].id);
    }

    /* destroy the dpchan_t instance */
    if (graph->chanid != VQEC_DP_CHANID_INVALID) {
        vqec_dpchan_destroy(graph->chanid);
    }
    graph->chanid = VQEC_DP_CHANID_INVALID;

    /* destroy input shim streams */
    vqec_dp_input_shim_destroy_instance(&graph->inputshim_output);

    return VQEC_DP_ERR_OK;
}

/**
 * Initialize the graph, i.e., build all components, but do not start the
 * dataflow.
 */
UT_STATIC vqec_dp_error_t
vqec_dp_graph_init (vqec_dp_graph_t *graph,
                    vqec_dp_chan_desc_t *desc,
                    vqec_dp_graphinfo_t *graphinfo)
{
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (!graph || !desc) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    /* add all the appropriate input shim streams */
    if ((err = vqec_dp_input_shim_create_instance(desc,
                                                  &graph->inputshim_output))
        != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(ERROR,
                     "failed to create input shim instance");
        memset(&graph->inputshim_output, 0, sizeof(graph->inputshim_output));
        goto bail;
    }

    /*
     * this is the setting needed for fallback mode; if the graph is not in
     * fallback mode, this will be modified
     */
    graph->outputshim_stream_type = VQEC_DP_IO_STREAM_TYPE_PRIMARY;

    if (!desc->fallback) {
        /* create a dpchan_t instance and setup input/output streams */
        graph->chanid =
            vqec_dpchan_create(
                desc,
                &graph->dpchan_input,
                &graph->dpchan_output.os[VQEC_DP_IO_STREAM_TYPE_POSTREPAIR]);
        if (graph->chanid == VQEC_DP_CHANID_INVALID) {
            VQEC_DP_SYSLOG_PRINT(ERROR,
                         "failed to create dpchan instance");
            memset(&graph->dpchan_input, 0, sizeof(graph->dpchan_input));
            memset(&graph->dpchan_output, 0, sizeof(graph->dpchan_output));
            err = VQEC_DP_ERR_NOMEM;
            goto bail;
        }
        graph->outputshim_stream_type = VQEC_DP_IO_STREAM_TYPE_POSTREPAIR;
    }

    /* create the output shim instance */
    if ((err = vqec_dp_output_shim_create_is(
             &graph->outputshim_input.is[graph->outputshim_stream_type],
             desc->strip_rtp ? VQEC_DP_ENCAP_UDP : VQEC_DP_ENCAP_RTP))
        != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "can't create outputshim IS");
        memset(&graph->outputshim_input, 0, sizeof(graph->outputshim_input));
        goto bail;
    }

    /* get the needed data to return to control plane */
    if (graphinfo) {
        graphinfo->id =
            graph->id;
        /* Input Shim info */
        graphinfo->inputshim.prim_rtp_id =
            graph->inputshim_output.os[VQEC_DP_IO_STREAM_TYPE_PRIMARY].id;
        graphinfo->inputshim.repair_rtp_id =
            graph->inputshim_output.os[VQEC_DP_IO_STREAM_TYPE_REPAIR].id;
        graphinfo->inputshim.fec0_rtp_id =
            graph->inputshim_output.os[VQEC_DP_IO_STREAM_TYPE_FEC_0].id;
        graphinfo->inputshim.fec1_rtp_id =
            graph->inputshim_output.os[VQEC_DP_IO_STREAM_TYPE_FEC_1].id;
        graphinfo->inputshim.rtp_eph_rtx_port =
            vqec_dp_input_shim_get_eph_port(
                graph->inputshim_output.os[VQEC_DP_IO_STREAM_TYPE_REPAIR].id);
        /* DP Chan info */
        graphinfo->dpchan.dpchan_id =
            graph->chanid;
        graphinfo->dpchan.prim_rtp_id =
            graph->dpchan_input.is[VQEC_DP_IO_STREAM_TYPE_PRIMARY].id;
        graphinfo->dpchan.repair_rtp_id =
            graph->dpchan_input.is[VQEC_DP_IO_STREAM_TYPE_REPAIR].id;
        graphinfo->dpchan.fec0_rtp_id =
            graph->dpchan_input.is[VQEC_DP_IO_STREAM_TYPE_FEC_0].id;
        graphinfo->dpchan.fec1_rtp_id =
            graph->dpchan_input.is[VQEC_DP_IO_STREAM_TYPE_FEC_1].id;
        /* Output Shim info */
        graphinfo->outputshim.postrepair_id =
            graph->outputshim_input.is[graph->outputshim_stream_type].id;
    }

    return VQEC_DP_ERR_OK;

bail:
    vqec_dp_graph_deinit(graph);

    return (err);
}

/**
 * Connect two module stream objects together.
 */
UT_STATIC vqec_dp_error_t
vqec_dp_graph_connect_streams (vqec_dp_output_stream_obj_t *output,
                               vqec_dp_input_stream_obj_t *input)
{
    int i;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    vqec_dp_stream_err_t stream_err;

    for (i = VQEC_DP_IO_STREAM_TYPE_MIN; i < VQEC_DP_IO_STREAM_TYPE_MAX; i++) {
        if (output->os[i].id != VQEC_DP_INVALID_OSID && output->os[i].ops &&
            input->is[i].id != VQEC_DP_INVALID_ISID && input->is[i].ops) {

            stream_err = input->is[i].ops->initiate_connect(input->is[i].id,
                                                            output->os[i].id,
                                                            output->os[i].ops);
            if (stream_err != VQEC_DP_STREAM_ERR_OK) {
                VQEC_DP_SYSLOG_PRINT(ERROR, "cannot connect streams "
                             "(initiate_connect failed)");
                err = VQEC_DP_ERR_GRAPHCONNECT;
                break;
            }

        }
    }

    return (err);
}

/**
 * Disconnect anything connected to a module stream object.
 */
UT_STATIC void
vqec_dp_graph_disconnect_streams (vqec_dp_output_stream_obj_t *output)
{
    int i;

    for (i = VQEC_DP_IO_STREAM_TYPE_MIN; i < VQEC_DP_IO_STREAM_TYPE_MAX; i++) {
        if (output->os[i].id != VQEC_DP_INVALID_ISID && output->os[i].ops) {
            output->os[i].ops->disconnect(output->os[i].id);
        }
    }
}

/**
 * Remove all of the IS-OS connections in the graph. The graph must have
 * been initialized prior to invoking this method.
 */
UT_STATIC vqec_dp_error_t
vqec_dp_graph_disconnect (vqec_dp_graph_t *graph)
{
    vqec_dp_os_instance_t dpchan_os;

    if (!graph) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    /* disconnect each inputshim OS */
    vqec_dp_graph_disconnect_streams(&graph->inputshim_output);

    /* disconnect the dpchan OS */
    dpchan_os = graph->dpchan_output.os[VQEC_DP_IO_STREAM_TYPE_POSTREPAIR];
    if (dpchan_os.id != VQEC_DP_INVALID_ISID && dpchan_os.ops) {
        dpchan_os.ops->disconnect(dpchan_os.id);
    }

    return VQEC_DP_ERR_OK;
}

/**
 * Make the IS-OS connections in the graph.  The graph must have
 * been initialized prior to invoking this method.
 */
UT_STATIC vqec_dp_error_t
vqec_dp_graph_connect (vqec_dp_graph_t *graph,
                       boolean fallback)
{
    vqec_dp_os_instance_t dpchan_os;
    vqec_dp_is_instance_t outputshim_is;
    vqec_dp_stream_err_t stream_err;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (!graph) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    if (fallback) {
        /* connect the streams between the inputshim and outputshim */
        err = vqec_dp_graph_connect_streams(&graph->inputshim_output,
                                            &graph->outputshim_input);
        if (err != VQEC_DP_ERR_OK) {
            VQEC_DP_SYSLOG_PRINT(ERROR,
                         "cannot connect inputshim and outputshim objects");
            err = VQEC_DP_ERR_GRAPHCONNECT;
            goto bail;
        }
    } else {
        /* connect the streams between the inputshim and dpchan */
        err = vqec_dp_graph_connect_streams(&graph->inputshim_output,
                                            &graph->dpchan_input);
        if (err != VQEC_DP_ERR_OK) {
            VQEC_DP_SYSLOG_PRINT(ERROR,
                         "cannot connect inputshim and dpchan objects");
            err = VQEC_DP_ERR_GRAPHCONNECT;
            goto bail;
        }

        /* connect the outputshim IS with the dpchan OS */
        dpchan_os =
            graph->dpchan_output.os[VQEC_DP_IO_STREAM_TYPE_POSTREPAIR];
        outputshim_is =
            graph->outputshim_input.is[graph->outputshim_stream_type];
        if (dpchan_os.id != VQEC_DP_INVALID_OSID && dpchan_os.ops &&
            outputshim_is.id != VQEC_DP_INVALID_ISID && outputshim_is.ops) {

            stream_err = outputshim_is.ops->initiate_connect(outputshim_is.id,
                                                             dpchan_os.id,
                                                             dpchan_os.ops);
            if (stream_err != VQEC_DP_STREAM_ERR_OK) {
                VQEC_DP_SYSLOG_PRINT(ERROR, "cannot connect dpchan and "
                                     "outputshim streams");
                err = VQEC_DP_ERR_GRAPHCONNECT;
                goto bail;
            }
        }
    }

bail:
    if (err != VQEC_DP_ERR_OK) {
        (void)vqec_dp_graph_disconnect(graph);
    }
    return (err);
}

/**
 * Add a new tuner to a channel's flowgraph. 
 * [The graph does not internally maintain which / how many
 * tuners are bound to the IS.]. This call basically invokes
 * map_is_to_dptuner() on the outputshim.
 */
vqec_dp_error_t
vqec_dp_graph_add_tuner (vqec_dp_graphid_t id, 
                         vqec_dp_tunerid_t tid)
{
    vqec_dp_graph_t *graph;
    vqec_dp_is_instance_t outputshim_is;

    /* get pointer from id_mgr */
    graph = vqec_dp_graph_id_to_graph(id);
    if (!graph) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    /* get the IS instance associated with the outputshim */
    outputshim_is = graph->outputshim_input.is[graph->outputshim_stream_type];
    if (outputshim_is.id == VQEC_DP_INVALID_ISID) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    /* associate the tuner with the given ID with this new IS */
    return vqec_dp_output_shim_map_is_to_dptuner(outputshim_is.id, tid);
}


/**
 * Remove a tuner from a channel's flowgraph. 
 */
vqec_dp_error_t
vqec_dp_graph_del_tuner (vqec_dp_tunerid_t tid)
{
    return vqec_dp_output_shim_unmap_dptuner(tid);
}

/**
 * Build the flowgraph for the given channel descriptor. A new
 * graph may be built, or an existing flowgraph may be used. A new
 * graph is built per channel unless SSRC multiplexing is in use. 
 * The flow of data through the graph is started, once the graph
 * has been built within the context of this call. The identifier of
 * the flowgraph which contains the channel is returned in *id.
 */
vqec_dp_error_t
vqec_dp_graph_create (vqec_dp_chan_desc_t *chan,
                      vqec_dp_tunerid_t tid, 
                      vqec_dp_encap_type_t tuner_encap, 
                      vqec_dp_graphinfo_t *graphinfo)
{
    vqec_dp_graph_t *graph;
    vqec_dp_graphid_t id;
    vqec_dp_error_t ret = VQEC_DP_ERR_OK;

    if (!chan) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    /* if the graph doesn't already exist, create a new one */
    graph = (vqec_dp_graph_t *) zone_acquire(s_vqec_graph_pool);
    if (!graph || (graph_index == s_graphs_max)) {
        id = VQEC_DP_GRAPHID_INVALID;
        ret = VQEC_DP_ERR_NOMEM;
        goto bail;
    }
    s_graph_cache[graph_index] = graph;
    graph_index++;
    memset(graph, 0, sizeof(vqec_dp_graph_t));

    /* 
     * Register graph in id_mgr - must have a channel id at time of
     * channel init.
     */ 
    id = id_get(graph, vqec_dp_graph_id_table_key);
    if (id == VQEC_DP_GRAPHID_INVALID) {
        ret = VQEC_DP_ERR_NOMEM;
        goto bail;
    }
    graph->id = id;

    /* initialize graph and build all components */
    if (vqec_dp_graph_init(graph, chan, graphinfo) != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "graph initialization failed");
        ret = VQEC_DP_ERR_NOMEM;
        goto bail;
    }

    /* add_tuner() to the graph */
    if (vqec_dp_graph_add_tuner(graph->id, tid) != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "graph add_tuner failed");
        ret = VQEC_DP_ERR_NOMEM;
        goto bail;
    }

    /* cache channel info inside the tuner */
    vqec_dp_output_shim_tuner_cache_channel(tid, chan);

    /* make IS-OS connections */
    if (vqec_dp_graph_connect(graph, chan->fallback) != VQEC_DP_ERR_OK) {
        VQEC_DP_SYSLOG_PRINT(ERROR, "graph connection failed");
        ret = VQEC_DP_ERR_NOMEM;
        goto bail;
    }

    return VQEC_DP_ERR_OK;

bail:
    if (id != VQEC_DP_GRAPHID_INVALID) {
        vqec_dp_graph_destroy(id);
        id = VQEC_DP_GRAPHID_INVALID;
    } else if (graph) {
        zone_release(s_vqec_graph_pool, graph);
        s_graph_cache[graph_index] = NULL;
        graph_index--;
    }

    return ret;
}

/**
 * Delete a channel from a flowgraph. If this is the only channel
 * mapped to a flowgraph, the flowgraph is deleted as well.
 */
vqec_dp_error_t
vqec_dp_graph_destroy (vqec_dp_graphid_t id)
{
    vqec_dp_graph_t *graph;

    /* get pointer from id_mgr */
    graph = vqec_dp_graph_id_to_graph(id);
    if (!graph) {
        return VQEC_DP_ERR_INVALIDARGS;
    }

    /* destroy any existing components in the graph */
    vqec_dp_graph_deinit(graph);

    /* remove handle from id_mgr */
    id_delete(id, vqec_dp_graph_id_table_key);

    zone_release(s_vqec_graph_pool, graph);
    graph_index--;
    s_graph_cache[graph_index] = NULL;

    return VQEC_DP_ERR_OK;
}

/**---------------------------------------------------------------------------
 * Inject a packet through the socket filter that is bound to the inputshim
 * output stream for the repair session. A hack used for injecting NAT packets
 * from the control-plane. Hence this is a "turn"-ed  message, i.e., the
 * message is being sent back towards the "inbound" path.
 * 
 * @param[in] id Identifier of a dataplane graph.
 * @param[in] remote_addr Remote IP address.
 * @param[in] remote_port Remote IP port.
 * @param[in] bufp Pointer to the buffer.
 * @param[in] len Length of the content buffer.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_graph_repair_inject (vqec_dp_graphid_t id,
                             in_addr_t remote_addr,
                             in_port_t remote_port,
                             char *bufp,
                             uint32_t len)
{
    vqec_dp_graph_t *graph;
    vqec_dp_osid_t osid;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (id == VQEC_DP_GRAPHID_INVALID || !bufp ||!len || !remote_port) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    graph = vqec_dp_graph_id_to_graph(id);
    if (!graph) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    osid = graph->inputshim_output.os[VQEC_DP_IO_STREAM_TYPE_REPAIR].id;
    if (osid == VQEC_DP_INVALID_OSID) {
        err = VQEC_DP_ERR_NOSUCHSTREAM;
        return (err);
    }

    return vqec_dp_input_shim_repair_inject(osid,
                                            remote_addr,
                                            remote_port,
                                            bufp,
                                            len);
}

/**---------------------------------------------------------------------------
 * Inject a packet through the socket filter that is bound to the inputshim
 * output stream for the primary session. A hack used for injecting NAT packets
 * from the control-plane. Hence this is a "turn"-ed  message, i.e., the
 * message is being sent back towards the "inbound" path.
 * 
 * @param[in] id Identifier of a dataplane graph.
 * @param[in] remote_addr Remote IP address.
 * @param[in] remote_port Remote IP port.
 * @param[in] bufp Pointer to the buffer.
 * @param[in] len Length of the content buffer.
 * @param[out] vqec_dp_error_t Returns DP_ERR_OK if the operation succeeds,
 * error code otherwise. 
 *---------------------------------------------------------------------------*/
vqec_dp_error_t
vqec_dp_graph_primary_inject (vqec_dp_graphid_t id,
                             in_addr_t remote_addr,
                             in_port_t remote_port,
                             char *bufp,
                             uint32_t len)
{
    vqec_dp_graph_t *graph;
    vqec_dp_osid_t osid;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;

    if (id == VQEC_DP_GRAPHID_INVALID || !bufp ||!len || !remote_port) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    graph = vqec_dp_graph_id_to_graph(id);
    if (!graph) {
        err = VQEC_DP_ERR_INVALIDARGS;
        return (err);
    }

    osid = graph->inputshim_output.os[VQEC_DP_IO_STREAM_TYPE_PRIMARY].id;
    if (osid == VQEC_DP_INVALID_OSID) {
        err = VQEC_DP_ERR_NOSUCHSTREAM;
        return (err);
    }

    return vqec_dp_input_shim_primary_inject(osid,
                                            remote_addr,
                                            remote_port,
                                            bufp,
                                            len);
}
