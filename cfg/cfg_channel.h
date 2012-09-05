/*
 *------------------------------------------------------------------
 * Channel access and management functions for configuration module
 *
 * May 2006, Dong Hsu
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _CFG_CHANNEL_H_
#define _CFG_CHANNEL_H_

/*! \file cfg_channel.h
    \brief Channel access component for Configuration Module.

    Internal channel strcuture and access functions.
*/

#include <stdio.h>
#include "cfgapi.h"
#include <utils/vam_time.h>
#include <utils/vqe_hash.h>

#define DEFAULT_PORT      50000
#define MAX_IP_ADDR_CHAR  20

/**
 * CHAN_MAP_DB hash function containing CHAN_MAP_DB_HASH_SIZE buckets.
 */
#define CHAN_MAP_DB_HASH_SIZE      256

typedef struct channel_hash_key_ {
    in_addr_t addr;
    in_port_t port;
} channel_hash_key;

/**
 * channel_handle_t - defines the per tv channel data for reverse
 * look up operation
 */
typedef struct channel_handle_ {
    struct channel_handle_ *next;  /**< hash collision */
    struct in_addr address;        /**< all addresses and ports stored 
                                    *   in network byte order
                                    */
    uint16_t port;
    idmgr_id_t handle;             /**< handle to channel_cfg structure */
} channel_handle_t;

/**
 * Channel hash map
 */
typedef struct channel_map_db_ {
    channel_handle_t *chan_cache_head[CHAN_MAP_DB_HASH_SIZE];
} channel_map_db_t;

/**
 * Adds a channel to the channel map database.
 */
boolean channel_add_map(channel_map_db_t *map,
                        struct in_addr address,
                        in_port_t port,
                        idmgr_id_t hanndle);

/**
 * Removes a channel to the channel map database.
 */
boolean channel_remove_map(channel_map_db_t *map,
                           struct in_addr address,
                           in_port_t port,
                           idmgr_id_t hanndle);

/**
 * Channel look up
 */
boolean channel_lookup(channel_map_db_t *map,
                       struct in_addr address, 
                       in_port_t port,
                       idmgr_id_t *handle);


/**
 * Internal channel structure
 */
typedef enum {
    CFG_CHANNEL_SUCCESS,
    CFG_CHANNEL_FAILURE,
    CFG_CHANNEL_EXCEED_MAX,
    CFG_CHANNEL_EXIST,
    CFG_CHANNEL_MALLOC_ERR,
    CFG_CHANNEL_FAILED_GET_INFO,
    CFG_CHANNEL_INVALID_CHANNEL,
    CFG_CHANNEL_INVALID_HANDLE,
    CFG_CHANNEL_FAILED_ADD_MAP,
    CFG_CHANNEL_MAX
} cfg_channel_ret_e;


/**
 * channel_mgr_cfg_t contains the information for all the channels
 */
typedef struct channel_mgr_ {
    uint16_t            total_num_channels;
    idmgr_id_t          handles[MAX_CHANNELS];
    vqe_hash_t          *session_keys;
    channel_map_db_t    channel_map;
    channel_map_db_t    fbt_map;
    id_table_key_t      handle_mgr;

    /* system-wide parameters */
    uint32_t            gop_size;
    uint8_t             burst_rate;

    /* system-wide stats */
    uint32_t            num_parsed;
    uint32_t            num_syntax_errors;
    uint32_t            num_validated;
    uint32_t            num_old_version;
    uint32_t            num_invalid_ver;
    uint32_t            num_input_channels;

    abs_time_t          timestamp;
} channel_mgr_t;

/**
 * Add a channel to the channel manager
 */
extern cfg_channel_ret_e cfg_channel_add(void *sdp_p,
                                         channel_mgr_t *channel_mgr_p,
                                         idmgr_id_t *channel_handle,
                                         uint32_t checksum);

/**
 * Delete a channel from the channel manager
 */
extern cfg_channel_ret_e cfg_channel_delete(channel_mgr_t *channel_mgr_p,
                                            idmgr_id_t channle_handle,
                                            boolean remove_from_map);

/**
 * Insert a channel to the channel manager
 */
extern channel_cfg_t *cfg_channel_insert(channel_mgr_t *channel_mgr_p,
                                         channel_cfg_t *channle_p);

/**
 * Add a channel to the channel map
 */
extern cfg_channel_ret_e cfg_channel_add_map(channel_mgr_t *channel_mgr_p,
                                             channel_cfg_t *channle_p);


/**
 * Remove a channel from the channel map
 */
extern cfg_channel_ret_e cfg_channel_del_map(channel_mgr_t *channel_mgr_p,
                                             channel_cfg_t *channle_p);

/**
 * Get a channel from the channel manager based on the channel handle
 */
extern channel_cfg_t *cfg_channel_get(channel_mgr_t *channel_mgr_p,
                                      idmgr_id_t channel_handle);

/**
 * Print out a channel configuration info
 */
extern cfg_channel_ret_e cfg_channel_print(channel_cfg_t *channel_p);

/**
 * Compare two channels
 */
extern boolean cfg_channel_compare(channel_cfg_t *channel1_p,
                                   channel_cfg_t *channel2_p);

/**
 * Copy the content of source channel to destination channel
 */
extern cfg_channel_ret_e cfg_channel_copy(channel_cfg_t *dest_channel_p,
                                          channel_cfg_t *src_channel_p);

/**
 * Destroy all cahnnel info from the channel manager
 */
extern cfg_channel_ret_e cfg_channel_destroy_all(channel_mgr_t *channel_mgr_p);

/**
 * Get the channel manager
 */
extern channel_mgr_t *cfg_get_channel_mgr(void);

/**
 * Parse session_key
 */
extern void cfg_channel_parse_session_key(channel_cfg_t *channel,
                                          char *user_name,
                                          char *session_id,
                                          char *creator_addr);

/**
 * Extract and validate channel information
 */
extern boolean cfg_channel_extract(void *sdp_p,
                                   channel_cfg_t *channel_p,
                                   cfg_chan_type_e chan_type);

#endif /* _CFG_CHANNEL_H_ */
