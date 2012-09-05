/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: DP top level debug counters declaration.
 *
 * Documents: Copied from vqes-cm.
 *
 *****************************************************************************/

/*
 * @note
 * This file defines a bunch of event counters, for keeping track of
 * low-level events that can occur in a particular object (depending
 * on the file name for which object).  The counters that are defined
 * are simply listed here as two arguments to a macro invocation.  The
 * first argument to the macro invocation is the name of the counter.
 * The second argument is a text string describing what the counter
 * means.  
 * 
 * The macro itself has no fixed meaning, but is defined in different
 * ways in different places in the code to generate repetitive code
 * for managing the counter.  The idea is that by separating the
 * declaration of the counter, here in this file, from all the various
 * things that need to be done to or with the counter (declare, initialize,
 * print, return in xml-rpc, return through cm-api, maybe clear)
 * scattered in various places in the code, we can get to a point
 * where all we need to do to add a new counter is to (i) add a line
 * for it here; (ii) bump the counter at the appropriate place in the
 * code.  For example, the following code might be used to initialize
 * all of the counters, assuming that there is a structure
 * top->counters containing a field for each counter:
 * 
 *
 * #define VQEC_DP_TLM_CNT_DECL(name,descr) \
 *        vqec_dp_ev_cnt_init(&(top->counters.name), NULL, NULL, 0);
 *
 * #include "vqec_dp_top_cnt_decl.h"
 * #undef VQEC_DP_TLM_CNT_DECL
 *
 * The structure  in this example would have its type declared
 * using something like this:
 *
 * typedef struct vqec_dp_tlm_debug_counters_ {
 *
 * #define VQEC_DP_TLM_CNT_DECL(name,descr) \
 * uint32_t name;
 * #include "vqec_dp_top_cnt_decl.h"
 * #undef VQEC_DP_TLM_CNT_DECL
 * 
 * } vqec_dp_tlm_debug_counters_t;
 *
 * To use this mechanism, this include file is intended to be included
 * multiple times at different places in the code, in the scope of
 * different definitions of the macro (hence no multiple-include
 * guards on this file).
 */


/* No multiple-include guards on this file. */

VQEC_DP_TLM_CNT_DECL(graph_create, 
                     "Created a graph");

VQEC_DP_TLM_CNT_DECL(graph_destroy,
                     "Destroyed a graph");

VQEC_DP_TLM_CNT_DECL(graph_create_failed,
                     "Creation of a graph failed");

VQEC_DP_TLM_CNT_DECL(channel_create,
                     "Created a Dataplane channel");

VQEC_DP_TLM_CNT_DECL(channel_destroy,
                     "Destroyed a Dataplane channel");

VQEC_DP_TLM_CNT_DECL(channel_create_failed,
                     "Dataplane channel creation failed");

VQEC_DP_TLM_CNT_DECL(rtp_src_limit_exceeded,
                     "Creation of a RTP source entry failed - receiver limit exceeded");

VQEC_DP_TLM_CNT_DECL(rtp_src_table_full,
                     "Creation of a RTP source entry failed - source table is full");

VQEC_DP_TLM_CNT_DECL(rtp_src_aged_out,
                     "Sources deleted since they were aged out");

VQEC_DP_TLM_CNT_DECL(rtp_src_creates,
                     "Sources created");

VQEC_DP_TLM_CNT_DECL(rtp_src_destroys,
                     "Sources destroyed");

VQEC_DP_TLM_CNT_DECL(input_stream_limit_exceeded,
                     "Too many RTP input stream");

VQEC_DP_TLM_CNT_DECL(input_stream_eject_paks, 
                     "Packets ejected to control-plane");

VQEC_DP_TLM_CNT_DECL(input_stream_eject_paks_failure, 
                     "Failed ejects to control-plane");

VQEC_DP_TLM_CNT_DECL(input_stream_creates, 
                     "Input stream creates");

VQEC_DP_TLM_CNT_DECL(input_stream_deletes, 
                     "Input stream deletions");

VQEC_DP_TLM_CNT_DECL(os_connect_failure, 
                     "IS-to-OS connect failures");

VQEC_DP_TLM_CNT_DECL(irqs_dropped, 
                     "Dropped IRQs");

VQEC_DP_TLM_CNT_DECL(irqs_sent, 
                     "Sent IRQs");

VQEC_DP_TLM_CNT_DECL(ejects_dropped, 
                     "Dropped Ejects");

VQEC_DP_TLM_CNT_DECL(ejects_sent, 
                     "Sent Ejects");

VQEC_DP_TLM_CNT_DECL(xr_stats_malloc_fails, 
                     "XR stats malloc failures");

VQEC_DP_TLM_CNT_DECL(ssrc_filter_drops, 
                     "SSRC filter drops on repair session");

VQEC_DP_TLM_CNT_DECL(repair_filter_drops, 
                     "OSN or first sequence filter drops on repair session");

VQEC_DP_TLM_CNT_DECL(system_clock_jumpbacks, 
                     "Number of times system clock has been detected moving "
                     "backwards");


