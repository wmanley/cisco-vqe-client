/*
 *------------------------------------------------------------------
 * Configuration Test
 *
 * May 2006, Dong Hsu
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include "cfgapi.h"
#include "cfg_channel.h"

/*Include CFG Library syslog header files */
#include <log/vqe_cfg_syslog_def.h>
#include <log/vqe_cfg_debug.h>

int main (int argc, char *argv[])
{
    uint16_t i;
    uint16_t total_channels;
    idmgr_id_t *handles_p = NULL;
    channel_cfg_t *channel_p;

    if (argc < 2) {
        printf("\nUsage: test-config database_name [-p] [-r]\n\n");
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
        printf("main:: Configuration module initialization failed.\n");
        exit(-1);
    }

    /* Get all the channels from the database */
    if (cfg_get_all_channels(&handles_p, &total_channels) != CFG_SUCCESS) {
        printf("main:: Configuration module failed to get all the channels.\n");
        exit(-1);
    }
    printf("main:: Total number of channels = %d\n\n",
           total_channels);

    if ((argv[2] && (strcmp(argv[2], "-p") == 0)) ||
        (argv[3] && (strcmp(argv[3], "-p") == 0))) {
        for ( i = 0; i < total_channels; i++ ) {
            channel_p = cfg_get_channel_cfg_from_hdl(*handles_p);
            if (channel_p && channel_p->active) {
                cfg_print_channel_cfg(channel_p);
            }
            handles_p++;
        }
    }

    if ((argv[2] && (strcmp(argv[2], "-r") == 0)) ||
        (argv[3] && (strcmp(argv[3], "-r") == 0))) {
        cfg_remove_redundant_FBTs();
    }

    uint32_t parsed = 0, validated = 0, total = 0;
    if (cfg_get_cfg_stats(&parsed, &validated, &total) == CFG_SUCCESS) {
        printf("No. of channels passed syntax checking = %d, "
               "No. of channels passed semantic checking = %d, "
               "No. of input channels = %d\n",
               parsed, validated, total);
    }

    /* Memory usage */
    printf("\nMemory usage of channel configuration data:\n");
    printf("One channel cfg structure = %d bytes\n", sizeof(channel_cfg_t));
    printf("channel manager structure = %d bytes\n", 
           sizeof(channel_mgr_t));

    if (cfg_shutdown() != CFG_SUCCESS) {
        printf("main:: Configuration module shutdown failed.\n");
        exit(-1);
    }

    exit(0);
}
