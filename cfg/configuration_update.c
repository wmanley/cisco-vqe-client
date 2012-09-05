/*
 *------------------------------------------------------------------
 * Configuration Update Test
 *
 * May 2006, Dong Hsu
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <utils/vam_debug.h>
#include "cfgapi.h"

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
        printf("\nUsage: test-cfg-update database_name update_filename\n\n");
        exit(-1);
    }

    /* Initialize the logging facility */
    int loglevel = LOG_DEBUG;

    syslog_facility_open(LOG_LIBCFG, LOG_CONS);
    syslog_facility_filter_set(LOG_LIBCFG, loglevel);

    VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_MGR);
    VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_DB);
    VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_CHANNEL);
    //VQE_CFG_SET_DEBUG_FLAG(CFG_DEBUG_SDP);

    /* Initialize the configuration module */
    if (cfg_init(argv[1]) != CFG_SUCCESS) {
        vam_debug_error("main:: Configuration module initialization failed.\n");
        exit(-1);
    }

    /* Get all the channels from the database */
    if (cfg_get_all_channels(&handles_p, &total_channels) != CFG_SUCCESS) {
        vam_debug_error("main:: Failed to get all the channels.\n");
        exit(-1);
    }

    printf("main:: Total number of channels = %d\n\n",
           total_channels);

    for ( i = 0; i < total_channels; i++ ) {
        channel_p = cfg_get_channel_cfg_from_hdl(*handles_p);
        cfg_print_channel_cfg(channel_p);
        handles_p++;
    }

    /* Update the channels */
    if (cfg_update(argv[2]) != CFG_SUCCESS ) {
        vam_debug_error("main:: Failed to update the channels.\n");
        exit(-1);
    }

    if (cfg_commit_update() == CFG_SUCCESS) {
        if (cfg_get_all_channels(&handles_p, &total_channels) != CFG_SUCCESS) {
            vam_debug_error("main:: Failed to get all the channels after updates.\n");
            exit(-1);
        }

        printf("main:: Total number of channels after updates = %d\n\n",
               total_channels);

        for ( i = 0; i < total_channels; i++ ) {
            channel_p = cfg_get_channel_cfg_from_hdl(*handles_p);
            if (channel_p && channel_p->active) {
                cfg_print_channel_cfg(channel_p);
            }
            handles_p++;
        }
    }

    if (cfg_shutdown() != CFG_SUCCESS) {
        vam_debug_error("main:: Configuration module shutdown failed.\n");
        exit(-1);
    }

    exit(0);
}
