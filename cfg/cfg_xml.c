/*
 *------------------------------------------------------------------
 * Converting Channel Configuration from SDP format to XML format
 * for VQE Channel Provision Tool
 *
 * April 2007, Dong Hsu
 *
 * Copyright (c) 2006-2007 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include "cfgapi.h"
#include "sdpconv.h"

/*Include CFG Library syslog header files */
#include <log/vqe_cfg_syslog_def.h>
#include <log/vqe_cfg_debug.h>

int main (int argc, char *argv[])
{
    uint16_t i;
    uint16_t total_channels;
    idmgr_id_t *handles_p = NULL;
    channel_cfg_t *channel_p;

    if (argc < 3) {
        printf("\nUsage: sdp2xml sdp_cfg_filename vcpt_xml_filename\n\n");
        exit(-1);
    }

    /* Initialize the logging facility */
    int loglevel = LOG_ERR;

    syslog_facility_open(LOG_LIBCFG, LOG_CONS);
    syslog_facility_filter_set(LOG_LIBCFG, loglevel);

    //VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_MGR);
    //VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_DB);
    //VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_CHANNEL);
    //VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_SDP);

    /* Initialize the configuration module */
    if (cfg_init(argv[1]) != CFG_SUCCESS) {
        printf("sdp2xml:: Configuration module initialization failed.\n");
        exit(-1);
    }

    /* Get all the channels from the database */
    if (cfg_get_all_channels(&handles_p, &total_channels) != CFG_SUCCESS) {
        printf("sdp2xml:: Configuration module failed to get all the channels.\n");
        exit(-1);
    }

    /* Generate xml header */
    gen_xml_hdr(argv[2]);

    for ( i = 0; i < total_channels; i++ ) {
        channel_p = cfg_get_channel_cfg_from_hdl(*handles_p);
        if (channel_p && channel_p->active) {
            gen_xml_channel(channel_p);
        }
        handles_p++;
    }

    /* Generate xml end */
    gen_xml_end(argv[2]);

    if (cfg_shutdown() != CFG_SUCCESS) {
        printf("sdp2xml:: Configuration module shutdown failed.\n");
        exit(-1);
    }

    exit(0);
}
