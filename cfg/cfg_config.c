/*
 *------------------------------------------------------------------
 * Top level for configuration module
 *
 * May 2006, Dong Hsu
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <unistd.h>

#include "cfgapi.h"
#include "cfg_channel.h"
#include "cfg_database.h"
#include <utils/vam_debug.h>
#include <sdp.h>
#include <utils/vam_time.h>
#include <utils/strl.h>

/*
 * Among all the C files including it, only ONE C file (say the one where main()
 * is defined) should instantiate the message definitions described in
 * XXX_syslog_def.h. To do so, the designated C file should define the symbol
 * SYSLOG_DEFINITION before including the file. 
 */
#define SYSLOG_DEFINITION
#include <log/vqe_cfg_syslog_def.h>
#undef SYSLOG_DEFINITION

/* Include base definitions, e.g. boolean flags, enum codes */
#include <log/vqe_cfg_debug.h>

/* Declare the debug array struct */
#define __DECLARE_DEBUG_ARR__
#include <log/vqe_cfg_debug_flags.h>

typedef struct configuration_mgr_ {
    cfg_database_t      config_db;
    channel_mgr_t       channel_mgr;
    boolean             initialized;
} configuration_mgr_t;

static configuration_mgr_t system_cfg;
static channel_mgr_t new_channel_mgr;

static uint32_t g_lastUsedChannel = 0;

static boolean file_exists(const char* filename);

static boolean parse_sdp_o_line(const char* o_line,
                                char* username,
                                char* session_id,
                                char* nettype,
                                char* addrtype,
                                char* creator_addr);

#define SESSION_KEY_BUCKETS 256
#define MAX_COMMAND_LENGTH 2*MAX_DB_NAME_LENGTH + 4

/* sa_ignore DISABLE_RETURNS strlcpy */

static uint32_t cfg_session_key_hash_func(uint8_t *key,
                                          int32_t len,
                                          int32_t mask);

/* Function:    cfg_init
 * Description: Initialize the configuration module
 * Parameters:  cfg_db_name     Configuration database name
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_init (const char* cfg_db_name)
{
    char message[MAX_LINE_LENGTH];

    VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                  "cfg_init:: Opening the database %s\n", cfg_db_name);

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_init:: Configuration module is already "
                      "initialized.\n");
        return CFG_ALREADY_INIT;
    }

    /* Check to make sure that the cfg_db_name is given */
    if (cfg_db_name) {
        strlcpy(system_cfg.config_db.db_name, cfg_db_name, MAX_DB_NAME_LENGTH);
    }
    else {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_init:: No channel configuration file name "
                      "is specified.\n");
    }

    system_cfg.config_db.db_file_p = NULL;

    /* Make sure all the initialization is done */
    system_cfg.channel_mgr.total_num_channels = 0;
    system_cfg.channel_mgr.num_parsed = 0;
    system_cfg.channel_mgr.num_syntax_errors = 0;
    system_cfg.channel_mgr.num_validated = 0; 
    system_cfg.channel_mgr.num_old_version = 0;
    system_cfg.channel_mgr.num_invalid_ver = 0;
    system_cfg.channel_mgr.num_input_channels = 0;

    /* Initialize the hash tables */
    init_vqe_hash_elem_module();
    init_vqe_hash_module();
    system_cfg.channel_mgr.session_keys = 
        vqe_hash_create(SESSION_KEY_BUCKETS,
                        cfg_session_key_hash_func,
                        NULL);

    if (system_cfg.channel_mgr.session_keys == NULL) {
        syslog_print(CFG_CREATE_HASH_TABLE_CRIT);
        return CFG_FAILURE;
    }

    int i;
    for (i = 0; i < CHAN_MAP_DB_HASH_SIZE; i++) {
        system_cfg.channel_mgr.channel_map.chan_cache_head[i] = NULL;
        system_cfg.channel_mgr.fbt_map.chan_cache_head[i] = NULL;
    }

    /* Create a handle manager */
    system_cfg.channel_mgr.handle_mgr = id_create_new_table(100, 100);

    /* Set new channel manager to zero */
    new_channel_mgr.total_num_channels = 0;

    if (file_exists(cfg_db_name) == TRUE) {
        /* Open the database */
        if (cfg_db_open(&system_cfg.config_db, cfg_db_name, DB_FILE)
            != CFG_DB_SUCCESS) {
            syslog_print(CFG_FILE_OPEN_CRIT, cfg_db_name);
            return CFG_OPEN_FAILED;
        }

        /* Read the channel info from database */
        if (cfg_db_read_channels(&system_cfg.config_db,
                                 &system_cfg.channel_mgr) != CFG_DB_SUCCESS) {
            syslog_print(CFG_FILE_READ_CRIT, cfg_db_name);
            return CFG_FAILURE;
        }
        
        /* Clean up the channel strucuture if not all the channels pass
           checking. */
        if (system_cfg.channel_mgr.num_input_channels != 
            system_cfg.channel_mgr.total_num_channels) {
            if (system_cfg.channel_mgr.num_input_channels >= MAX_CHANNELS) {
                snprintf(message, MAX_LINE_LENGTH,
                         "the total number of channels %d reaching "
                         "the maximum channels %d allowed",
                         system_cfg.channel_mgr.num_input_channels,
                         MAX_CHANNELS);
            }
            else {
                snprintf(message, MAX_LINE_LENGTH,
                         "%d out of %d channels failed the validation",
                         (system_cfg.channel_mgr.num_input_channels
                          - system_cfg.channel_mgr.num_validated),
                         system_cfg.channel_mgr.num_input_channels);
            }
            syslog_print(CFG_FILE_PROCESS_ERR, message);

            /* Close the database */
            if (cfg_db_close(&system_cfg.config_db) != CFG_DB_SUCCESS) {
                syslog_print(CFG_FILE_CLOSE_ERR, system_cfg.config_db.db_name);
            }

            /* Clean up all the channels */
            if (cfg_channel_destroy_all(&system_cfg.channel_mgr) 
                != CFG_CHANNEL_SUCCESS) {
                syslog_print(CFG_DESTROY_ALL_ERR);
            }

            int i;
            for (i = 0; i < CHAN_MAP_DB_HASH_SIZE; i++) {
                system_cfg.channel_mgr.channel_map.chan_cache_head[i] = NULL;
                system_cfg.channel_mgr.fbt_map.chan_cache_head[i] = NULL;
            }
        }
        else {
            /* Remeber the time the configuration data is read */
            system_cfg.channel_mgr.timestamp = get_sys_time();
        }
    }
    else if (cfg_db_name) {
        syslog_print(CFG_NO_FILE_WARN, cfg_db_name);
    } else {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_init::static channel db name is null\n");
    }

    system_cfg.initialized = TRUE;

    VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                  "cfg_init:: Total number of channels in the data file "
                  "is %d\n", system_cfg.channel_mgr.total_num_channels);

    syslog_print(CFG_INIT_COMPLETED_INFO);

    return CFG_SUCCESS;
}


/* Function:    cfg_shutdown
 * Description: Clean up the configuration module
 * Parameters:  N/A
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_shutdown (void)
{
    cfg_ret_e status = CFG_SUCCESS;

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_shutdown:: Configuration module is not "
                      "initialized.\n");
        return CFG_UNINITIALIZED;
    }

    /* Close the database */
    if (cfg_db_close(&system_cfg.config_db) != CFG_DB_SUCCESS) {
        syslog_print(CFG_FILE_CLOSE_ERR, system_cfg.config_db.db_name);
        status = CFG_FAILURE;
    }

    /* Clean up the memory */
    memset(system_cfg.config_db.db_name, 0, MAX_DB_NAME_LENGTH);

    VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                  "cfg_shutdown: Shutdowning the configuration module\n");

    /* Clean up all the channels */
    if (cfg_channel_destroy_all(&system_cfg.channel_mgr) 
        != CFG_CHANNEL_SUCCESS) {
        syslog_print(CFG_DESTROY_ALL_ERR);
    }

    /* Delete the ID manager */
    id_destroy_table(system_cfg.channel_mgr.handle_mgr);
    system_cfg.channel_mgr.handle_mgr = ID_MGR_TABLE_KEY_ILLEGAL;
    system_cfg.initialized = FALSE;

    int i;
    for (i = 0; i < CHAN_MAP_DB_HASH_SIZE; i++) {
        system_cfg.channel_mgr.channel_map.chan_cache_head[i] = NULL;
        system_cfg.channel_mgr.fbt_map.chan_cache_head[i] = NULL;
    }

    if (system_cfg.channel_mgr.session_keys) {
        vqe_hash_destroy(system_cfg.channel_mgr.session_keys);
        system_cfg.channel_mgr.session_keys = NULL;
    }

    if (status == CFG_SUCCESS) {
        syslog_print(CFG_CLOSE_COMPLETED_INFO);
    }

    return status;
}


/* Function:    cfg_save
 * Description: Save the configuration data to the database
 * Parameters:  cfg_db_name     The name of configuration file to be saved
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_save (const char* cfg_db_name)
{
    cfg_ret_e status = CFG_SUCCESS;

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_save:: Configuration module is not initialized.\n");
        return CFG_UNINITIALIZED;
    }

    /* Remeber the time the configuration data is written */
    system_cfg.channel_mgr.timestamp = get_sys_time();

    /* Save the configuration to the databse */
    if (cfg_db_name == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_save:: No configuration database name is "
                      "specified.\n");
        status = CFG_FAILURE;
    }
    else {
        strlcpy(system_cfg.config_db.db_name, cfg_db_name, MAX_DB_NAME_LENGTH);
    }

    if (cfg_db_write_channels(&system_cfg.config_db, &system_cfg.channel_mgr)
        != CFG_DB_SUCCESS) {
        syslog_print(CFG_FILE_WRITE_ERR, system_cfg.config_db.db_name);
        status = CFG_FAILURE;
    }

    /* Close the database */
    if (cfg_db_close(&system_cfg.config_db) != CFG_DB_SUCCESS) {
        syslog_print(CFG_FILE_CLOSE_ERR, system_cfg.config_db.db_name);
        status = CFG_FAILURE;
    }

    return status;
}


/* Function:    cfg_save_from
 * Description: Save the configuration data from a given file
 * Parameters:  cfg_db_name     The name of configuration file
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_save_from (const char* cfg_db_name)
{
    cfg_ret_e status = CFG_SUCCESS;
    char command[MAX_COMMAND_LENGTH];

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_save_from:: Configuration module is not "
                      "initialized.\n");
        return CFG_UNINITIALIZED;
    }

    /* Remeber the time the configuration data is written */
    system_cfg.channel_mgr.timestamp = get_sys_time();

    /* Save all the contents from a given configuration file */
    if (file_exists(cfg_db_name) == TRUE) {
        snprintf(command, MAX_COMMAND_LENGTH,
                 "cp %s %s", cfg_db_name, system_cfg.config_db.db_name);
        if (system(command)) {
            syslog_print(CFG_SAVE_ERR, system_cfg.config_db.db_name);
            status = CFG_FAILURE;
        }
    }
    else {
        syslog_print(CFG_FILE_OPEN_CRIT, cfg_db_name);
        status = CFG_FAILURE;
    }

    return status;
}
    

/* Function:    cfg_is_savable
 * Description: Check whether the configuration file is savable or not
 * Parameters:  N/A
 * Returns:     TRUE or FALSE
 */
boolean cfg_is_savable (void)
{
    FILE *fp;

    if (file_exists(system_cfg.config_db.db_name) == TRUE) {
        if (access(system_cfg.config_db.db_name, W_OK) == 0) {
            return TRUE;
        }
        else {
            syslog_print(CFG_SAVE_ERR, system_cfg.config_db.db_name);
            return FALSE;
        }
    }
    else {
        fp = fopen(system_cfg.config_db.db_name, "write");
        if (fp) {
            fclose(fp);

            return TRUE;
        }
        else {
            syslog_print(CFG_SAVE_ERR, system_cfg.config_db.db_name);
            return FALSE;
        }
    }
}


/* Function:    cfg_set_db_name
 * Description: Set the name of db in the configuration module
 * Parameters:  cfg_db_name     Configuration database name
 * Returns:     N/A
 */
void cfg_set_db_name (const char* cfg_db_name)
{
    /* Check to make sure that the cfg_db_name is given */
    if (cfg_db_name) {
        strlcpy(system_cfg.config_db.db_name, cfg_db_name, MAX_DB_NAME_LENGTH);
    }
    else {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_set_db_name:: No channel configuration file name "
                      "is specified.\n");
    }
}


/* Function:    cfg_update
 * Description: Update the internal configuration data based on new file
 * Parameters:  cfg_db_name     New configuration data filename
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_update (const char* cfg_db_name)
{
    cfg_database_t      temp_config_db;
    
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_update:: Configuration module is not "
                      "initialized.\n");
        return CFG_UNINITIALIZED;
    }

    /* The newly arrived data will be parsed and validated but managed by a
       temporary channel manager */
    cfg_cleanup_update();

    /* Use the same id manager as system_cfg */
    new_channel_mgr.total_num_channels = 0;
    new_channel_mgr.num_parsed = 0;
    new_channel_mgr.num_syntax_errors = 0;
    new_channel_mgr.num_validated = 0;
    new_channel_mgr.num_old_version = 0;
    new_channel_mgr.num_invalid_ver = 0;
    new_channel_mgr.num_input_channels = 0;
    new_channel_mgr.handle_mgr = system_cfg.channel_mgr.handle_mgr;

    new_channel_mgr.burst_rate = system_cfg.channel_mgr.burst_rate;
    new_channel_mgr.gop_size = system_cfg.channel_mgr.gop_size;


    new_channel_mgr.session_keys = 
        vqe_hash_create(SESSION_KEY_BUCKETS,
                        cfg_session_key_hash_func,
                        NULL);

    if (new_channel_mgr.session_keys == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_update:: Failed to create the "
                      "hash table in new channel manager.\n");
        return CFG_FAILURE;
    }

    int i;
    for (i = 0; i < CHAN_MAP_DB_HASH_SIZE; i++) {
        new_channel_mgr.channel_map.chan_cache_head[i] = NULL;
        new_channel_mgr.fbt_map.chan_cache_head[i] = NULL;
    }

    /* Open the newly arrived channel configuration file */
    temp_config_db.db_file_p = (void *) fopen(cfg_db_name, "read");
    if (temp_config_db.db_file_p == NULL) {
        syslog_print(CFG_FILE_READ_CRIT, cfg_db_name);
        return CFG_OPEN_FAILED;
    }
    else {
        strlcpy(temp_config_db.db_name, cfg_db_name,
                MAX_DB_NAME_LENGTH);
        temp_config_db.type = DB_FILE;
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_update:: Opening the newly arrived data file %s "
                      "with file pointer %p\n",
                      cfg_db_name, temp_config_db.db_file_p);
    }


    /* Read the channel info from database */
    if (cfg_db_read_channels(&temp_config_db, &new_channel_mgr) 
        != CFG_DB_SUCCESS) {
        syslog_print(CFG_FILE_READ_CRIT, cfg_db_name);
        return CFG_FAILURE;
    }

    VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                  "cfg_update:: Total number of channels in the new data file "
                  "is %d\n", new_channel_mgr.total_num_channels);

    /* Close the database */
    if (cfg_db_close(&temp_config_db) != CFG_DB_SUCCESS) {
        syslog_print(CFG_FILE_CLOSE_ERR, cfg_db_name);
        return CFG_FAILURE;
    }

    /* Validate the entire set of channels to be correct
     *
     * The definition of being correct is
     *
     * 1) all the channels pass syntax and semantic checking;
     * 2) if there is a channel existing in the old set, the version
     *    must be greater than or equal to the older one. If the same
     *    version number, the checksum must be the same.
     *
     */
    if (cfg_validate_all() == TRUE) {
        /* Reset the global value of last used channel index */
        g_lastUsedChannel = 0;

        syslog_print(CFG_UPDATE_COMPLETED_INFO,
                     "Configuration data is successfully loaded.");

        return CFG_SUCCESS;
    }
    else {
        return CFG_VALIDATION_ERR;
    }
}


#define DATE_AND_TIME_LENGTH_INCLUDING_NULL 20

/* Function:    cfg_get_timestamp
 * Description: Retrieve the current system timestamp in ISO-8601 format
 * Parameters:  buff            The buffer for storing the timestamp
 *              buffer_len      The buffer length
 * Returns:     Pointer to the timestamp buffer
 */
const char* cfg_get_timestamp (char * buff,
                               uint32_t buff_len)
{
    if (! buff || buff_len == 0) {
        return NULL;
    }

    if (IS_ABS_TIME_ZERO(system_cfg.channel_mgr.timestamp)) {
        snprintf(buff, buff_len, "<unavailable>");
    } else {
        abs_time_to_str_secs(system_cfg.channel_mgr.timestamp, buff, buff_len);
    }
    return (buff);
}


/* Function:    cfg_get_all_channels
 * Description: Retrieve an array of channel handles
 * Parameters:  handles         Array of channel handles
 *              total_channels  Total number of channels in array
 * Returns:     Success
 */
cfg_ret_e cfg_get_all_channels (idmgr_id_t **handles,
                                uint16_t *total_channels)
{
    *total_channels = system_cfg.channel_mgr.total_num_channels;
    *handles = system_cfg.channel_mgr.handles;

    return CFG_SUCCESS;
}


/* Function:    cfg_get_system_params
 * Description: Retrieve the value of GOP size and burst rate.
 * Parameters:  gop_size        GOP size
 *              burst_rate      Burst rate
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_get_system_params (uint32_t *gop_size,
                                 uint8_t *burst_rate)
{
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_system_params:: Configuration module is not "
                      "initialized.\n");
        return CFG_UNINITIALIZED;
    }

    *gop_size = system_cfg.channel_mgr.gop_size;
    *burst_rate = system_cfg.channel_mgr.burst_rate;

    return CFG_SUCCESS;
}


/* Function:    cfg_get_cfg_stats
 * Description: Retrieve the channel configuration parsing and validation
 *              statistics
 * Parameters:  parsed          Number of successfully parsed channels
 *              validated       Number of successfully validated channels
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_get_cfg_stats (uint32_t *parsed,
                             uint32_t *validated,
                             uint32_t *total)
{
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_cfg_stats:: Configuration module is not "
                      "initialized.\n");
        return CFG_UNINITIALIZED;
    }

    *parsed = system_cfg.channel_mgr.num_parsed;
    *validated = system_cfg.channel_mgr.num_validated;
    *total = system_cfg.channel_mgr.num_input_channels;

    return CFG_SUCCESS;
}


/* Function:    cfg_set_system_params
 * Description: Set the system parameters
 * Parameters:  gop_size        GOP size
 *              burst_rate      Burst rate
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_set_system_params (uint32_t gop_size,
                                 uint8_t burst_rate)
{
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_set_system_params:: Configuration module is not "
                      "initialized.\n");
        return CFG_UNINITIALIZED;
    }

    system_cfg.channel_mgr.gop_size = gop_size;
    system_cfg.channel_mgr.burst_rate = burst_rate;

    return CFG_SUCCESS;
}


/* Function:    cfg_get_channel_cfg_from_hdl
 * Description: Retrieve a channel configuration based on channel handle
 * Parameters:  handle  Channel configuration handle
 * Returns:     Pointer to the valid channel configuration structure or NULL
 */
channel_cfg_t *cfg_get_channel_cfg_from_hdl (idmgr_id_t handle)
{
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_channel_cfg_from_hdl:: Configuration module "
                      "is not initialized.\n");
        return NULL;
    }

    return cfg_channel_get(&system_cfg.channel_mgr, handle);
}


/* Function:    cfg_getchannel_cfg_from_idx
 * Description: Retrieve a channel configuration based on channel index
 * Parameters:  index   Channel index
 * Returns:     Pointer to the valid channel configuration structure or NULL
 */
channel_cfg_t *cfg_get_channel_cfg_from_idx (uint16_t index)
{
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_channel_cfg_from_idx:: Configuration module "
                      "is not initialized.\n");
        return NULL;
    }

    if (index > MAX_CHANNELS-1) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_channel_cfg_from_idx:: Channel index is "
                      "greater than MAX_CHANNELS %d\n", MAX_CHANNELS);
        return NULL;
    }

    return cfg_channel_get(&system_cfg.channel_mgr,
                           system_cfg.channel_mgr.handles[index]);
}


/* Function:    cfg_get_channel_cfg_from_orig_src_addr
 * Description: Retrieve a channel configuration based on original
 *              source address and port
 * Parameters:  source_addr     Original source address
 *              port            RTP/UDP port
 * Returns:     Pointer to the valid channel configuration structure or NULL
 */
channel_cfg_t *cfg_get_channel_cfg_from_orig_src_addr (
    struct in_addr source_addr,
    in_port_t port)
{
    channel_mgr_t *channel_mgr_p;
    idmgr_id_t   handle;
    channel_cfg_t *channel_p;

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_channel_cfg_from_orig_src_addr:: Configuration "
                      "module is not initialized.\n");
        return NULL;
    }

    channel_mgr_p = &system_cfg.channel_mgr;

    if (!channel_lookup(&channel_mgr_p->channel_map,
                        source_addr, port, &handle))
    {
        char tmp[INET_ADDRSTRLEN];
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_channel_cfg_from_orig_src_addr:: No channel "
                      "info for ip %s, port %d in channel map.\n",
                      inet_ntop(AF_INET, &source_addr, tmp, INET_ADDRSTRLEN), 
                      ntohs(port));
        return NULL;
    }

    channel_p = cfg_channel_get(channel_mgr_p, handle);

    return channel_p;
}


/* Function:    cfg_get_channel_cfg_from_origin
 * Description: Retrieve the channel configuration based on the information
 *              provided from the origin line o= in a SDP session
 * Parameters:  username        Username string
 *              session_id      SDP session id
 *              nettype         Network type
 *              addrtype        IP address type
 *              creator_addr    IP address of the SDP creator
 * Returns:     Pointer to the valid channel configuration structure or NULL
 */
channel_cfg_t *cfg_get_channel_cfg_from_origin (const char *username,
                                                const char *session_id,
                                                const char *nettype,
                                                const char *addrtype,
                                                const char *creator_addr)
{
    char session_key[MAX_KEY_LENGTH];
    vqe_hash_key_t hkey;
    vqe_hash_elem_t *elem;
    channel_cfg_t *channel_p;

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_channel_cfg_from_origin:: Configuration "
                      "module is not initialized.\n");
        return NULL;
    }

    /* Construct a globally unique identifier */
    snprintf(session_key, MAX_KEY_LENGTH, "%s%s#%s#%s#%s",
             nettype, addrtype, username, session_id, creator_addr);

    VQE_HASH_MAKE_KEY(&hkey, session_key, strlen(session_key));
    elem = MCALL(system_cfg.channel_mgr.session_keys,
                 vqe_hash_get_elem, &hkey);

    if (elem) {
        channel_p = (channel_cfg_t *)elem->data;
        if (channel_p && channel_p->active) {
            return channel_p;
        }
        else {
            return NULL;
        }
    }
    else {
        return NULL;
    }
}


/* Function:    cfg_get_channel_cfg_from_SDP_hdl
 * Description: Retrieve the channel configuration based on the sdp handle
 * Parameters:  handle   a structure that defines the per channel globally
 *              unique identifier
 *              - this field is really a NULL terminated text string 
 *              - in the form of a valid "o=" SDP description.
 *              - See RFC 4456 for a description of the "o=" syntax.
 * Returns:     Pointer to the valid channel configuration structure or NULL
 */
channel_cfg_t *cfg_get_channel_cfg_from_SDP_hdl (
    channel_cfg_sdp_handle_t handle)
{
    char username[SDP_MAX_STRING_LEN];
    char sessionid[SDP_MAX_STRING_LEN];
    char nettype[SDP_MAX_TYPE_LEN];
    char addrtype[SDP_MAX_TYPE_LEN];
    char creator_addr[SDP_MAX_STRING_LEN];

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_channel_cfg_from_SDP_hdl:: Configuration "
                      "module is not initialized.\n");
        return NULL;
    }

    if (handle == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_channel_cfg_from_SDP_hdl:: sdp handle is NULL");
        return NULL;
    }

    /* Call cfg_get_channel_cfg_from_origin() */
    memset(username, 0, SDP_MAX_STRING_LEN);
    memset(sessionid, 0, SDP_MAX_STRING_LEN);
    memset(nettype, 0, SDP_MAX_TYPE_LEN);
    memset(addrtype, 0, SDP_MAX_TYPE_LEN);
    memset(creator_addr, 0, SDP_MAX_STRING_LEN);
    if (parse_sdp_o_line(handle, username, sessionid, nettype,
                         addrtype, creator_addr)) {
        return cfg_get_channel_cfg_from_origin(username, sessionid,
                                               nettype, addrtype,
                                               creator_addr);
    }

    return NULL;
}


/* Function:    cfg_alloc_SDP_handle_from_orig_src_addr
 * Description: Create a SDP handle based on original source address and
 *              port
 * Parameters:  source_addr     Original source address
 *              port            RTP/UDP port
 * Returns:     Pointer to a SDP handle or NULL
 */
channel_cfg_sdp_handle_t cfg_alloc_SDP_handle_from_orig_src_addr (
    stream_proto_e protocol,
    struct in_addr source_addr,
    in_port_t port)
{
    channel_cfg_t *channel_p;
    channel_cfg_sdp_handle_t sdp_handle = NULL;
    char username[SDP_MAX_STRING_LEN];
    char sessionid[SDP_MAX_STRING_LEN];
    char creator_addr[SDP_MAX_STRING_LEN];

    channel_p = cfg_get_channel_cfg_from_orig_src_addr(source_addr, port);
    if (channel_p && channel_p->source_proto == protocol) {
        sdp_handle = (channel_cfg_sdp_handle_t) malloc(MAX_LINE_LENGTH);

        if (sdp_handle) {
            memset(username, 0, SDP_MAX_STRING_LEN);
            memset(sessionid, 0, SDP_MAX_STRING_LEN);
            memset(creator_addr, 0, SDP_MAX_STRING_LEN);
            cfg_channel_parse_session_key(channel_p,
                                          username, 
                                          sessionid,
                                          creator_addr);

            /* Reconstruct the o= line in SDP syntax */
            snprintf(sdp_handle, MAX_KEY_LENGTH, "o=%s %s %llu IN IP4 %s",
                     username, sessionid, channel_p->version, creator_addr);
        }
        else {
            VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                          "cfg_alloc_SDP_handle_from_orig_src_addr:: Memory "
                          "allocation failed.\n");
        }
    }

    return sdp_handle;
}


/* Function:    cfg_free_SDP_handle
 * Description: Free the memory held by a SDP handle
 * Parameters:  sdp_handle      A SDP handle
 * Returns:     N/A
 */
void cfg_free_SDP_handle (channel_cfg_sdp_handle_t sdp_handle)
{
    if (sdp_handle) {
        free((char *)sdp_handle);
    }
}


/* Function:    cfg_clone_SDP_handle
 * Description: Clone a given SDP handle
 * Parameters:  handle  A SDP handle
 * Returns:     A cloned SDP handle
 */
channel_cfg_sdp_handle_t cfg_clone_SDP_handle (
    const channel_cfg_sdp_handle_t handle)
{
    channel_cfg_sdp_handle_t new_handle = NULL;

    if (handle == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_clone_SDP_handle:: sdp handle is NULL.\n");
        return NULL;
    }

    new_handle = (channel_cfg_sdp_handle_t) malloc(strlen(handle)+1);
    if (new_handle) {
        strncpy(new_handle, handle, (strlen(handle)+1));
    }
    else {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_clone_SDP_handle:: Memory alloc failed.\n");
    }
    
    return new_handle;
}


/* Function:    cfg_SDP_handle_equivalent
 * Description: Compare two SPD handles
 * Parameters:  sdp_handle1     First SDP handle
 *              sdp_handle2     Second SDP handle
 * Returns:     True or false
 */
boolean cfg_SDP_handle_equivalent (const channel_cfg_sdp_handle_t sdp_handle1,
                                   const channel_cfg_sdp_handle_t sdp_handle2)

{
    char username1[SDP_MAX_STRING_LEN];
    char sessionid1[SDP_MAX_STRING_LEN];
    char nettype1[SDP_MAX_TYPE_LEN];
    char addrtype1[SDP_MAX_TYPE_LEN];
    char creator_addr1[SDP_MAX_STRING_LEN];

    char username2[SDP_MAX_STRING_LEN];
    char sessionid2[SDP_MAX_STRING_LEN];
    char nettype2[SDP_MAX_TYPE_LEN];
    char addrtype2[SDP_MAX_TYPE_LEN];
    char creator_addr2[SDP_MAX_STRING_LEN];

    memset(username1, 0, SDP_MAX_STRING_LEN);
    memset(sessionid1, 0, SDP_MAX_STRING_LEN);
    memset(nettype1, 0, SDP_MAX_TYPE_LEN);
    memset(addrtype1, 0, SDP_MAX_TYPE_LEN);
    memset(creator_addr1, 0, SDP_MAX_STRING_LEN);

    memset(username2, 0, SDP_MAX_STRING_LEN);
    memset(sessionid2, 0, SDP_MAX_STRING_LEN);
    memset(nettype2, 0, SDP_MAX_TYPE_LEN);
    memset(addrtype2, 0, SDP_MAX_TYPE_LEN);
    memset(creator_addr2, 0, SDP_MAX_STRING_LEN);

    if (parse_sdp_o_line(sdp_handle1, username1, sessionid1,
                         nettype1, addrtype1, creator_addr1) &&
        parse_sdp_o_line(sdp_handle2, username2, sessionid2,
                         nettype2, addrtype2, creator_addr2)) {
        if ((strcmp(username1, username2) == 0) &&
            (strcmp(sessionid1, sessionid2) == 0) &&
            (strcmp(nettype1, nettype2) == 0) &&
            (strcmp(addrtype1, addrtype2) == 0) &&
            (strcmp(creator_addr1, creator_addr2) == 0)) {
            return TRUE;
        }
        else {
            return FALSE;
        }
    }
    else {
        return FALSE;
    }
}


/* Function:    cfg_get_total_num_channels
 * Description: Retrieve the total number of channels in configuration module
 * Parameters:  N/A
 * Returns:     The total number of channels
 */
uint16_t cfg_get_total_num_channels (void)
{
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_total_num_channels:: Configuration module "
                      "is not initialized.\n");
        return 0;
    }

    return system_cfg.channel_mgr.total_num_channels;
}


/* Function:    cfg_get_total_num_active_channels
 * Description: Retrieve the total number of active channels in config module
 * Parameters:  N/A
 * Returns:     The total number of active channels
 */
uint16_t cfg_get_total_num_active_channels (void)
{
    int i, total = 0;

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_total_num_channels:: Configuration module "
                      "is not initialized.\n");
        return 0;
    }

    for (i = 0; i < system_cfg.channel_mgr.total_num_channels; i++) {
        if (cfg_get_channel_cfg_from_idx(i)) {
            total++;
        }
    }

    return total;
}


/* Function:    cfg_get_total_num_input_channels
 * Description: Retrieve the total number of input channels in config module
 * Parameters:  N/A
 * Returns:     The total number of input channels
 */
uint16_t cfg_get_total_num_input_channels (void)
{
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_total_num_input_channels:: Configuration "
                      "module is not initialized.\n");
        return 0;
    }

    return system_cfg.channel_mgr.num_input_channels;
}


/* Function:    cfg_get_all_channel_data
 * Description: return all the channel data in a buffer
 * Parameters:  buffer  The allocated buffer for the channel data
 *              size    The size of the buffer
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_get_all_channel_data (char *buffer,
                                    uint32_t size)
{
    uint16_t i;
    uint32_t total_size = 0;
    channel_cfg_t *channel_p;
    char sdp_data[MAX_SESSION_SIZE];

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_all_channel_data:: Configuration module is not "
                      "initialized.\n");
        return CFG_UNINITIALIZED;
    }

    if (buffer == NULL || size == 0) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_all_channel_data:: No memory is being allocated "
                      "or buffer size is zero.\n");
        return CFG_MALLOC_REQ;
    }

    /* Clean up the memory first */
    memset(buffer, 0, size);

    for (i = 0; i < system_cfg.channel_mgr.total_num_channels; i++) {
        channel_p = cfg_get_channel_cfg_from_idx(i);

        if (channel_p) {
            memset(sdp_data, 0, MAX_SESSION_SIZE);
            if (cfg_db_create_SDP(channel_p, sdp_data, MAX_SESSION_SIZE)
                != CFG_DB_SUCCESS) {
                VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                              "cfg_get_all_channel_data:: failed to create SDP "
                              "seesion for channel %d\n",
                              i);
            }
            else {
                total_size += strlen(sdp_data);

                if (total_size + 20 < size) {
                    strncat(buffer, sdp_data, strlen(sdp_data));
                    if (i != system_cfg.channel_mgr.total_num_channels-1) {
                        strncat(buffer, SESSION_SEPARATOR, 
                                strlen(SESSION_SEPARATOR));
                    }
                }
                else {
                    syslog_print(CFG_EXCEED_BUFFER_SIZE_WARN, size);
                }
            }
        }
    }

    VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                  "cfg_get_all_channel_data:: data = \n%s\n",
                  buffer);

    return CFG_SUCCESS;
}


/* Function:    cfg_print_channel_cfg
 * Description: Print out the content in channel configuration data structure
 * Parameters:  channel_p       Pointer to the channel configuration data
 *                              strucuture
 * Returns:     N/A
 */
void cfg_print_channel_cfg (channel_cfg_t *channel_p)
{
    if (cfg_channel_print(channel_p) != CFG_CHANNEL_SUCCESS) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_print_channel_cfg:: error occurred in "
                      "channel print\n");
    }
}


/* Function:    cfg_get_client_source_address
 * Description: Get the source address(address from where the packet was sent
 *              from the client perspective).
 * Parameters:  channel - channel from which to find the src address
 *              src_addr - pointer to the source address
 * Returns:     true if successful and src_addr has been modified.
 */
/*
 * 
 */
boolean
cfg_get_client_source_address (const channel_cfg_t *channel, 
                               struct in_addr *src_addr)
{
    boolean rv = TRUE;
    if (!channel || !src_addr) {
        return FALSE;
    }
    switch (channel->mode) {
    case LOOKASIDE_MODE:
    case RECV_ONLY_MODE:
        memcpy(src_addr, &channel->src_addr_for_original_source,
               sizeof (struct in_addr));
        break;
    case SOURCE_MODE:
        memcpy(src_addr, &channel->fbt_address,
               sizeof (struct in_addr));
        break;
    default:
        rv = FALSE;
        ASSERT(0, "Undefined channel mode"); /* This should never happen */
        break;
    }
    return rv;
}


/* Function:    cfg_get_channel_mgr
 * Description: Return the configuration manager
 * Parameters:  N/A
 * Returns:     Pointer to the channel manager or NULL
 */
channel_mgr_t *cfg_get_channel_mgr (void)
{
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_get_channel_mgr:: Configuration module is not "
                      "initialized.\n");
        return NULL;
    }

    return &system_cfg.channel_mgr;
}


/* Function:    read_line
 * Description: Return a line from the input buffer
 * Parameters:  line_buffer     Returned line buffer
 *              max_length      The length of the buffer
 *              whole_buffer    The input buffer to be parsed
 *              total_length    Length of whole_buffer
 *              start_idx       Starting index to be parsed in input buffer
 * Returns:     The index after the line being parsed
 */
int read_line (char *line_buffer, 
               int max_length, 
               char *whole_buffer, 
               int total_length,
               int start_idx)
{
    int res =0;
    int count = 0;

    if (start_idx >= total_length) {
        return -1;
    }

    /* We support "\r\n" and "\n" line termination */
    memset(line_buffer, 0, max_length);
    while ((start_idx + count < total_length) &&
           (count < max_length)) {
        if (whole_buffer[start_idx+count] == '\n') { 
            line_buffer[count] = '\n';
            count+=1;
            res = count;
            break;
        }
        
        if (whole_buffer[start_idx+count] == '\r') { 
            line_buffer[count] = '\n';
            count+=2;
            res = count;
            break;
        }

        line_buffer[count] = whole_buffer[start_idx+count];
        count++;
    }

    /* This means that we did not find the line termination mark at the end */
    if (res == 0) {
        if (start_idx + count == total_length) {
            /* If there is still a space, add the null termination */
            if (count < max_length) {
                line_buffer[count] = '\0';
            }
            return total_length; /* no more data */
        }
    }

    return start_idx+res;
}


/* Function:    cfg_parse_all_channel_data
 * Description: Parse all the channel data in a buffer
 * Parameters:  buffer  The buffer contains channel data in SDP syntax
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_parse_all_channel_data (char *buffer)
{
    cfg_channel_ret_e status;
    int         start_idx = 0;
    char        line_buffer[MAX_LINE_LENGTH];
    char        session_buffer[MAX_SESSION_SIZE];
    void        *sdp_cfg_p;
    void        *sdp_p;
    idmgr_id_t  handle;
    boolean     in_session = FALSE;
    char        id_buffer[MAX_LINE_LENGTH];
    uint32_t    total_session_length = 0;
    boolean     skip = FALSE;
    uint32_t    chksum;
    int         total_buffer_length;
    boolean     syntax_error = FALSE;

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_parse_all_channel_data:: Configuration module is "
                      "not initialized.\n");
        return CFG_UNINITIALIZED;
    }

    /* The data in buffer will be parsed and validated but managed by a
       temporary channel manager */
    cfg_cleanup_update();

    /* Use the same id manager as system_cfg */
    new_channel_mgr.total_num_channels = 0;
    new_channel_mgr.num_parsed = 0;
    new_channel_mgr.num_syntax_errors = 0;
    new_channel_mgr.num_validated = 0;
    new_channel_mgr.num_old_version = 0;
    new_channel_mgr.num_invalid_ver = 0;
    new_channel_mgr.num_input_channels = 0;
    new_channel_mgr.handle_mgr = system_cfg.channel_mgr.handle_mgr;

    new_channel_mgr.burst_rate = system_cfg.channel_mgr.burst_rate;
    new_channel_mgr.gop_size = system_cfg.channel_mgr.gop_size;

    new_channel_mgr.session_keys = 
        vqe_hash_create(SESSION_KEY_BUCKETS,
                        cfg_session_key_hash_func,
                        NULL);

    if (new_channel_mgr.session_keys == NULL) {
        syslog_print(CFG_CREATE_HASH_TABLE_CRIT);
        return CFG_FAILURE;
    }

    int i;
    for (i = 0; i < CHAN_MAP_DB_HASH_SIZE; i++) {
        new_channel_mgr.channel_map.chan_cache_head[i] = NULL;
        new_channel_mgr.fbt_map.chan_cache_head[i] = NULL;
    }

    if (buffer == NULL || strlen(buffer) == 0) {
        /* We will still return success even there is no data in data buffer */
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_parse_all_channel_data:: No data in input buffer\n");

        /* Remeber the time the configuration data is read */
        new_channel_mgr.timestamp = get_sys_time();

        return CFG_SUCCESS;
    }

    /* Set up a default SDP configuration for all the sessions */
    sdp_cfg_p = sdp_init_config();

    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_TRACE, FALSE);
    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_WARNINGS, FALSE);
    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_ERRORS, FALSE);

    sdp_require_version(sdp_cfg_p, TRUE);
    sdp_require_owner(sdp_cfg_p, TRUE);
    sdp_require_session_name(sdp_cfg_p, TRUE);
    sdp_require_timespec(sdp_cfg_p, TRUE);

    sdp_media_supported(sdp_cfg_p, SDP_MEDIA_VIDEO, TRUE);

    sdp_nettype_supported(sdp_cfg_p, SDP_NT_INTERNET, TRUE);    
    sdp_addrtype_supported(sdp_cfg_p, SDP_AT_IP4, TRUE);
   
    sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_UDP, TRUE);
    sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_RTPAVP, TRUE);
    sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_RTPAVPF, TRUE);
    
    sdp_allow_choose(sdp_cfg_p, SDP_CHOOSE_CONN_ADDR, TRUE);
    sdp_allow_choose(sdp_cfg_p, SDP_CHOOSE_PORTNUM, TRUE);

    /* Parse all the data contents */
    memset(session_buffer, 0, MAX_SESSION_SIZE);
    memset(id_buffer, 0, MAX_LINE_LENGTH);
    total_session_length = 0;
    total_buffer_length = strlen(buffer);
    while ((start_idx = read_line(line_buffer, 
                                  MAX_LINE_LENGTH, 
                                  buffer, 
                                  total_buffer_length,
                                  start_idx))
           != -1) {
        if (line_buffer[0] == '-' && line_buffer[1] == '-') {
            /* Found the session boundary */
            VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                          "cfg_parse_all_channel_data:: Session = \n%s\n",
                          session_buffer);
            in_session = FALSE;

            /* We will not count the empty session as one channel */
            if (session_buffer[0] == '\0') {
                continue;
            }

            new_channel_mgr.num_input_channels++;

            /* Create a SDP structure for this session */
            sdp_p  = sdp_init_description(sdp_cfg_p);
            VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                          "cfg_parse_all_channel_data:: Creating SDP session "
                          "%p\n", sdp_p);

            if (sdp_p == NULL) {
                syslog_print(CFG_MALLOC_ERR);
                return CFG_MALLOC_ERR;
            }

            /* Parse and validate SDP description in the session */
            if(cfg_db_parse_SDP(sdp_p, session_buffer, &syntax_error)) {
                /* Increase number of channels passing the parser */
                new_channel_mgr.num_parsed++;

                /* Compute MD5 checksum */
                chksum = cfg_db_checksum(session_buffer,
                                         strlen(session_buffer));
                        
                /* Store it in the channel manager */
                status = cfg_channel_add(sdp_p, &new_channel_mgr,
                                         &handle, chksum);
                if (status != CFG_CHANNEL_SUCCESS) {
                    if (status != CFG_CHANNEL_EXIST) {
                        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                                      "cfg_parse_all_channel_data:: Failed to "
                                      "add the channel.\n");
                        VQE_CFG_DEBUG(CFG_DEBUG_SDP, NULL,
                                      "SDP Session = \n%s\n",
                                      session_buffer);
                    }
                }
                else {
                    /* Increase number of channels passing */
                    /* the validation */
                    new_channel_mgr.num_validated++;

                    VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                                  "cfg_parse_all_channel_data:: New channel "
                                  "0x%lx is being added\n",
                                  handle);
                }
            }
            else {
                if (syntax_error) {
                    new_channel_mgr.num_syntax_errors++;
                }
                VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                              "cfg_parse_all_channel_data:: Failed to create "
                              "SDP session for the channel.\n");
                VQE_CFG_DEBUG(CFG_DEBUG_SDP, NULL,
                              "Raw data = \n%s\n", 
                              session_buffer);
            }

            /* Delete the SDP data */
            if (sdp_p) {
                VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                              "cfg_parse_all_channel_data:: Deleting SDP "
                              "session %p\n",
                              sdp_p);
                sdp_free_description(sdp_p);
            }

            /* Reset the seesion buffer for new session */
            memset(session_buffer, 0, MAX_SESSION_SIZE);
            memset(id_buffer, 0, MAX_LINE_LENGTH);
            total_session_length = 0;
            skip = FALSE;
        }
        else {
            /* Strip out the commented lines or blank lines */
            if (line_buffer[0] != '#' &&
                line_buffer[0] != '\n' &&
                line_buffer[0] != '\b' &&
                line_buffer[0] != '\r' &&
                line_buffer[0] != ' ') {
                if (line_buffer[0] == 'v' && line_buffer[1] == '=') {
                    if (in_session == TRUE) {
                        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                                      "cfg_parse_all_channel_data:: "
                                      "No session divider is found. "
                                      "A session might be skipped.\n");
                    }

                    in_session = TRUE;
                }

                /* Remember the o= line */
                if (line_buffer[0] == 'o' && line_buffer[1] == '=') {
                    strncpy(id_buffer, line_buffer,
                            strlen(line_buffer)-1);
                }

                total_session_length += strlen(line_buffer);
                if (total_session_length >= MAX_SESSION_SIZE) {
                    if (skip == FALSE) {
                        syslog_print(CFG_EXCEED_BUFFER_SIZE_WARN,
                                     id_buffer, MAX_SESSION_SIZE);
                    }
                    skip = TRUE;
                }
                else {
                    strncat(session_buffer, line_buffer, strlen(line_buffer));
                }
            }
        }
    }

    /* Process the last session */
    if (session_buffer[0] != '\n' &&
        session_buffer[0] != '\r' &&
        session_buffer[0] != '\0') {
        new_channel_mgr.num_input_channels++;
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_parse_all_channel_data:: Session = \n%s\n",
                      session_buffer);

        /* Create a SDP structure for this session */
        sdp_p  = sdp_init_description(sdp_cfg_p);
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_parse_all_channel_data:: Creating SDP session "
                      "%p\n", sdp_p);
        
        if (sdp_p == NULL) {
            syslog_print(CFG_MALLOC_ERR);
            return CFG_MALLOC_ERR;
        }

        /* Parse and validate SDP description in the session */
        if (cfg_db_parse_SDP(sdp_p, session_buffer, &syntax_error)) {
            /* Increase number of channels passing the parser */
            new_channel_mgr.num_parsed++;

            /* Compute MD5 checksum */
            chksum = cfg_db_checksum(session_buffer,
                                     strlen(session_buffer));
                        
           /* Store it in the channel manager */
            status = cfg_channel_add(sdp_p, &new_channel_mgr, &handle, chksum);
            if (status != CFG_CHANNEL_SUCCESS) {
                if (status != CFG_CHANNEL_EXIST) {
                    VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                                  "cfg_parse_all_channel_data:: Failed to "
                                  "add the channel.\n");
                    VQE_CFG_DEBUG(CFG_DEBUG_SDP, NULL, 
                                  "SDP Session = \n%s\n", 
                                  session_buffer);
                }
            }
            else {
                /* Increase number of channels passing */
                /* the validation */
                new_channel_mgr.num_validated++;

                VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                              "cfg_parse_all_channel_data:: New channel "
                              "0x%lx is being added\n",
                              handle);
            }
        }
        else {
            if (syntax_error) {
                new_channel_mgr.num_syntax_errors++;
            }
            VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                          "cfg_parse_all_channel_data:: Failed to create SDP "
                          "session for the channel.\n");
            VQE_CFG_DEBUG(CFG_DEBUG_SDP, NULL, 
                          "Raw data = \n%s\n", 
                          session_buffer);
        }

        /* Delete the SDP data */
        if (sdp_p) {
            VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                          "cfg_parse_all_channel_data:: Deleting SDP "
                          "session %p\n",
                          sdp_p);
            sdp_free_description(sdp_p);
        }
    }

    /* Free up stuff */
    my_free(sdp_cfg_p);

    /* Remeber the time the configuration data is read */
    new_channel_mgr.timestamp = get_sys_time();

    return CFG_SUCCESS;
}


/* Function:    cfg_parse_single_channel_data
 * Description: Parse the channel data in a buffer
 * Parameters:  buffer      The buffer contains channel data in SDP syntax
                channel     Channel configuration for extracting data
                chan_type   SDP type (VoD or Linear)
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_parse_single_channel_data (char *buffer,
                                         channel_cfg_t *chan_cfg,
                                         cfg_chan_type_e chan_type)
{
    cfg_ret_e   err = CFG_SUCCESS;
    void        *sdp_cfg_p = NULL;
    void        *sdp_p = NULL;

    /* Initialize channel configuration memory to zero */
    if (chan_cfg) {
        memset(chan_cfg, 0, sizeof(channel_cfg_t));
    }

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_parse_single_channel_data:: "
                      "Configuration module is not initialized.\n");
        return CFG_UNINITIALIZED;
    }
    
    if (buffer == NULL || strlen(buffer) == 0) {
        /* We will still return success even there is no data in data buffer */
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_parse_single_channel_data:: "
                      "No data in input buffer\n");
        return CFG_SUCCESS;
    }
    
    VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                  "cfg_parse_single_channel_data:: Session = \n%s\n",
                  buffer);

    /* Set up a default SDP configuration for all the sessions */
    sdp_cfg_p = sdp_init_config();

    if (sdp_cfg_p == NULL) {
        syslog_print(CFG_MALLOC_ERR);
        err = CFG_MALLOC_ERR;
        goto done;
    }

    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_TRACE, FALSE);
    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_WARNINGS, FALSE);
    sdp_appl_debug(sdp_cfg_p, SDP_DEBUG_ERRORS, FALSE);

    sdp_require_version(sdp_cfg_p, TRUE);
    sdp_require_owner(sdp_cfg_p, TRUE);
    sdp_require_session_name(sdp_cfg_p, TRUE);
    sdp_require_timespec(sdp_cfg_p, TRUE);

    sdp_media_supported(sdp_cfg_p, SDP_MEDIA_VIDEO, TRUE);

    sdp_nettype_supported(sdp_cfg_p, SDP_NT_INTERNET, TRUE);    
    sdp_addrtype_supported(sdp_cfg_p, SDP_AT_IP4, TRUE);
  
    if (chan_type == CFG_LINEAR) {
        sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_UDP, TRUE);
    } 
    sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_RTPAVP, TRUE);
    sdp_transport_supported(sdp_cfg_p, SDP_TRANSPORT_RTPAVPF, TRUE);
    
    sdp_allow_choose(sdp_cfg_p, SDP_CHOOSE_CONN_ADDR, TRUE);
    sdp_allow_choose(sdp_cfg_p, SDP_CHOOSE_PORTNUM, TRUE);

    /* Create a SDP structure for this session */
    sdp_p  = sdp_init_description(sdp_cfg_p);
    VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                  "cfg_parse_single_channel_data:: Creating SDP session "
                  "%p\n", sdp_p);
    
    if (sdp_p == NULL) {
        syslog_print(CFG_MALLOC_ERR);
        err = CFG_MALLOC_ERR;
        goto done;
    }

    /* Parse and validate SDP description in the session */
    if (cfg_db_parse_SDP(sdp_p, buffer, NULL)) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_parse_single_channel_data:: Parsed SDP "
                      "data for the channel successfully.\n");
        
        /* Validate and extract channel information */
        if (cfg_channel_extract(sdp_p, chan_cfg, chan_type) == FALSE) {
            VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                          "cfg_parse_single_channel_data:: Failed to extract "
                          "data for the channel.\n");
            err = CFG_FAILURE;
            goto done;
        }
    }
    else {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_parse_single_channel_data:: Failed to parse SDP "
                      "data for the channel.\n");
        VQE_CFG_DEBUG(CFG_DEBUG_SDP, NULL, 
                      "Raw data = \n%s\n", 
                      buffer);
        err = CFG_FAILURE;
        goto done;
    }

  done:
    /* Delete the SDP data */
    if (sdp_p) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_parse_all_channel_data:: Deleting SDP "
                      "session %p\n",
                      sdp_p);
        sdp_free_description(sdp_p);
    }
        
    /* Free SDP cfg */
    my_free(sdp_cfg_p);

    return err;
}


/* Function:    cfg_commit_update()
 * Description: Replace current channel manager with new channel manager
 * Parameters:  N/A
 * Returns:     Success or failure codes
 */
cfg_ret_e cfg_commit_update (void)
{
    channel_mgr_t *channel_mgr_p;
    int i;
    channel_cfg_t *channel_p;

    VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                  "cfg_commit_update:: Replacing current contents with new "
                  "updates\n");

    channel_mgr_p = cfg_get_channel_mgr();
    if (channel_mgr_p) {
        /* Clean up the current channel manager in system_config */
        if (cfg_channel_destroy_all(channel_mgr_p) != CFG_CHANNEL_SUCCESS) {
            syslog_print(CFG_DESTROY_ALL_ERR);
        }
        else {
            for (i = 0; i < new_channel_mgr.total_num_channels; i++) {
                channel_p = cfg_channel_get(&new_channel_mgr,
                                            new_channel_mgr.handles[i]);
                if (channel_p && channel_p->active) {
                    if (cfg_insert_channel(channel_p) == NULL) {
                        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                                      "cfg_commit_update:: Failed to insert "
                                      "channel 0x%lx to channel manager.\n",
                                      new_channel_mgr.handles[i]);

                        return CFG_FAILURE;
                    }
                }
            }

            channel_mgr_p->num_parsed = new_channel_mgr.num_parsed;
            channel_mgr_p->num_syntax_errors = 
                new_channel_mgr.num_syntax_errors;
            channel_mgr_p->num_validated = new_channel_mgr.num_validated;
            channel_mgr_p->num_input_channels = 
                new_channel_mgr.num_input_channels;
            channel_mgr_p->timestamp = new_channel_mgr.timestamp;
        }

        return CFG_SUCCESS;
    }
    else {
        return CFG_FAILURE;
    }

}


/* Function:    file_exists()
 * Description: Check whether the file exists or not
 * Parameters:  filename        The filename to be checked
 * Returns:     True or false
 */
boolean file_exists (const char * filename)
{
    FILE *fp = NULL;

    if (!filename) {
        return FALSE;
    }

    fp = fopen(filename, "r");
    if (fp != NULL) {
        fclose(fp);
        return TRUE;
    }

    return FALSE;
}


/* Function:    cfg_remove_redundant_FBTs
 * Description: Remove all the channels with redundant FBT addresses
 * Parameters:  N/A
 * Returns:     None
 */
void cfg_remove_redundant_FBTs (void)
{
    channel_map_db_t fbt_map;
    
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_remove_redundant_FBTs:: Configuration module is "
                      "not initialized.\n");
    }
    else {
        int i;
        for (i = 0; i < CHAN_MAP_DB_HASH_SIZE; i++) {
            fbt_map.chan_cache_head[i] = NULL;
        }

        channel_mgr_t *channel_mgr_p;
        if (new_channel_mgr.total_num_channels != 0) {
            channel_mgr_p = &new_channel_mgr;
        }
        else {
            channel_mgr_p = cfg_get_channel_mgr();
        }

        if (channel_mgr_p) {
            int total_channels = channel_mgr_p->total_num_channels;
            channel_cfg_t *channel_p;
            for (i = 0; i < total_channels; i++) {
                channel_p = cfg_channel_get(channel_mgr_p,
                                            channel_mgr_p->handles[i]);
                if (channel_p && channel_p->active) {
                    /* Add each channel to the fbt_map. If failed, remove it */
                    /* completely.*/
                    if (channel_add_map(&fbt_map,
                                        channel_p->fbt_address,
                                        htons(5000),
                                        channel_p->handle) == FALSE) {
                        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                                 "cfg_remove_redundant_FBTs:: channel %s is "
                                 "removed due to duplicated FBT addresses.\n",
                                 channel_p->name);
                        syslog_print(CFG_DUPLICATED_FBT_ADDR_WARN,
                                     channel_p->session_key);

                        /* Need to be removed from all the channel maps */
                        if (cfg_channel_del_map(channel_mgr_p,
                                                channel_p)
                            != CFG_CHANNEL_SUCCESS) {
                            syslog_print(CFG_REMOVE_FROM_MAP_WARN,
                                         channel_p->session_key);
                        }

                        channel_p->active = FALSE;
                        channel_mgr_p->num_validated--;
                    }
                }
            }

            /* Free up the ftb map */
            for (i = 0; i < total_channels; i++) {
                channel_p = cfg_channel_get(channel_mgr_p,
                                            channel_mgr_p->handles[i]);
                if (channel_p && channel_p->active) {
                    if (channel_remove_map(&fbt_map,
                                           channel_p->fbt_address,
                                           htons(5000),
                                           channel_p->handle) == FALSE) {
                        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                                 "cfg_remove_redundant_FBTs:: failed to "
                                 "remove channel %s from FBT map.\n",
                                 channel_p->name);
                        syslog_print(CFG_REMOVE_FROM_MAP_WARN,
                                     channel_p->session_key);
                    }
                }
            }
        }
    }
}


/* Function:    parse_sdp_o_line
 * Description: Extract out the fields of username, session version, and
 *              creator's address from SDP o= line
 * Parameters:  username        User name
 *              session_id      Session identifier
 *              nettype         Network type
 *              addrtype        Address type
 *              creator_addr    Address for the session creator
 * Returns:     Success or failed
 */
static boolean parse_sdp_o_line (const char* o_line,
                                 char* username,
                                 char* session_id,
                                 char* nettype,
                                 char* addrtype,
                                 char* creator_addr)
{
    int i;
    char *sep = " ";
    char *token, *brkt;
    char buffer[MAX_LINE_LENGTH];

    if (o_line == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "parse_sdp_o_line:: Input o= line is NULL.\n");
        return FALSE;
    }

    memset(buffer, 0, MAX_LINE_LENGTH);
    strlcpy(buffer, o_line, MAX_LINE_LENGTH);
    VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                  "parse_sdp_o_line:: Input o= line is %s\n",
                  buffer);

    for (i = 1, token = strtok_r(buffer, sep, &brkt);
         token;
         i++, token = strtok_r(NULL, sep, &brkt))
    {
        switch(i) {
            case 1:
                strlcpy(username, &token[2], SDP_MAX_STRING_LEN);
                break;
        
            case 2:
                strlcpy(session_id, token, SDP_MAX_STRING_LEN);
                break;

            case 3:
                break;

            case 4:
                strlcpy(nettype, token, SDP_MAX_TYPE_LEN);
                break;

            case 5:
                strlcpy(addrtype, token, SDP_MAX_TYPE_LEN);
                break;

            case 6:
                strlcpy(creator_addr, token, SDP_MAX_STRING_LEN);
                break;

            default:
                VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                              "parse_sdp_o_line:: illegal o= line %s\n",
                              o_line);
                return FALSE;
        }

        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "parse_sdp_o_line:: field %d in o= line = %s\n",
                      i, token);
    }

    if (i < 6) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "parse_sdp_o_line:: illegal o= line %s\n",
                      o_line);
        return FALSE;
    }
    else {
        return TRUE;
    }
}


/* Function:    cfg_session_key_hash_func
 * Description: Create a hash based on ISO hash function
 * Parameters:  key     The key to be hashed
 *              len     Length of the key
 *              mask    
 * Returns:     Hash key
 */
static uint32_t cfg_session_key_hash_func (uint8_t *key,
                                           int32_t len,
                                           int32_t mask)
{
    if (key == NULL) {
        ASSERT(0, "cfg_session_key_hash_func:: Null pointer for key\n");
    }

    register int acc = 0;
    register uint8_t *p = (void *)key;
    int i = len;
    int htbl_size = SESSION_KEY_BUCKETS - 1;
    while (i--) {
        acc += *p++;
        while (acc > htbl_size) {
            acc = (acc & htbl_size) + (acc >> 8);
        }
    }

    return (acc % SESSION_KEY_BUCKETS);
}


/* Function:    cfg_find_in_update
 * Description: Check whether the given channel configuration is in update
 * Parameters:  channel         Pointer to a channel cfg data structure
 * Returns:     Pointer to a channel configuration data structure
 */
channel_cfg_t *cfg_find_in_update (const channel_cfg_t *channel)
{
    idmgr_id_t   handle;
    channel_cfg_t *channel_p;

    /* Check whether the configuration manager has any data */
    if (new_channel_mgr.total_num_channels) {
        if (channel && channel_lookup(&new_channel_mgr.channel_map,
                                      channel->original_source_addr,
                                      channel->original_source_port,
                                      &handle))
        {
            channel_p = cfg_channel_get(&new_channel_mgr, handle);
            /* Make sure that there are indeed the same */
            if (channel_p) {
                if (strcmp(channel->session_key, channel_p->session_key)
                    == 0) {
                    /* Set to TRUE so we won't use it again. */
                    channel_p->used = TRUE;

                    return channel_p;
                }
            }
        }
    }

    return NULL;
}


/* Function:    cfg_remove_channel
 * Description: Remove a given channel configuration from internal manager
 * Parameters:  channel         Pointer to a channel cfg data structure
 * Returns:     N/A
 */
void cfg_remove_channel (const channel_cfg_t *channel)
{
    char name[SDP_MAX_STRING_LEN];

    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_remove_channel:: Configuration module is not "
                      "initialized.\n");
    }
    else {
        if (channel) {
            /* Before deleting the channel, copy the name string */
            strncpy(name, channel->name, SDP_MAX_STRING_LEN);

            if (cfg_channel_delete(&system_cfg.channel_mgr,
                                   channel->handle,
                                   TRUE) != CFG_CHANNEL_SUCCESS) {
                VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                              "cfg_remove_channel:: delete channel failed\n");
            }
            else {
                VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                              "cfg_remove_channel:: delete the channel %s\n",
                              name);
            }
        }
    }
}


/* Function:    cfg_has_changed
 * Description: Check whether the version has increased or not
 * Parameters:  channel1        Pointer to a channel cfg data structure
 *              channel2        Pointer to a channel cfg data structure
 * Returns:     True or false
 */
boolean cfg_has_changed (channel_cfg_t *channel1,
                         channel_cfg_t *channel2)
{
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_has_changed:: Configuration module is not "
                      "initialized.\n");
        return FALSE;
    }

    if (strcmp(channel1->session_key, channel2->session_key) == 0) {
        if (channel2->version > channel1->version) {
            return TRUE;
        }
        else {
            return FALSE;
        }
    }
    else {
        return FALSE;
    }
}


/* Function:    cfg_is_identical
 * Description: Check whether the channels are identical except the version
 * Parameters:  channel1        Pointer to a channel cfg data structure
 *              channel2        Pointer to a channel cfg data structure
 * Returns:     True or false
 */
boolean cfg_is_identical (channel_cfg_t *channel1,
                          channel_cfg_t *channel2)
{
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_is_identical:: Configuration module is not "
                      "initialized.\n");
        return FALSE;
    }

    return cfg_channel_compare(channel1, channel2);
}


/* Function:    cfg_get_next_in_update
 * Description: Get next unused channel
 * Parameters:  N/A
 * Returns:     Pointer to a channel configuration data structure
 */
channel_cfg_t *cfg_get_next_in_update (void)
{
    int i;
    channel_cfg_t *channel_p;

    /* Check whether the configuration manager has any data */
    for (i = g_lastUsedChannel; i < new_channel_mgr.total_num_channels; i++) {
        channel_p = cfg_channel_get(&new_channel_mgr,
                                    new_channel_mgr.handles[i]);
        if (channel_p && !channel_p->used) {
            g_lastUsedChannel = i;
            channel_p->used = TRUE;
            return channel_p;
        }
    }

    return NULL;
}


/* Function:    cfg_insert_channel
 * Description: Insert a channel to the internal manager
 * Parameters:  channel         Pointer to a channel cfg data structure
 * Returns:     New channel pointer in channel manager
 */
channel_cfg_t *cfg_insert_channel (channel_cfg_t *channel)
{
    /* Check whether the configuration manager has been initialized or not */
    if (system_cfg.initialized == FALSE) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_insert_channel:: Configuration module is not "
                      "initialized.\n");
        return NULL;
    }

    return cfg_channel_insert(&system_cfg.channel_mgr, channel);
}


/* Function:    cfg_cleanup_update
 * Description: Clean up the contents in new channel manager
 * Parameters:  N/A
 * Returns:     N/A
 */
void cfg_cleanup_update (void)
{
    if (new_channel_mgr.total_num_channels != 0) {
        VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                      "cfg_cleanup_update:: Cleaning up the contents in "
                      "new channel manager.\n");

        /* Clean up the contents in new channel manager */
        if (cfg_channel_destroy_all(&new_channel_mgr) != CFG_CHANNEL_SUCCESS) {
            syslog_print(CFG_DESTROY_ALL_ERR);
            VQE_CFG_DEBUG(CFG_DEBUG_MGR, NULL,
                          "cfg_cleanup_update:: Failed to delete the "
                          "channels in new channel manager.\n");
        }
    }

    /* Need to call vqe_hash_destroy() even there is no channel
       because the change of vqe_hash_create() */
    if (new_channel_mgr.session_keys) {
        vqe_hash_destroy(new_channel_mgr.session_keys);
        new_channel_mgr.session_keys = NULL;
    }

    new_channel_mgr.total_num_channels = 0;
}


/* Function:    cfg_copy_cfg_stats
 * Description: Copy the stats from new channel manager to global manager
 * Parameters:  N/A
 * Returns:     N/A
 */
void cfg_copy_cfg_stats (void)
{
    system_cfg.channel_mgr.num_parsed = new_channel_mgr.num_parsed;
    system_cfg.channel_mgr.num_syntax_errors =
        new_channel_mgr.num_syntax_errors;
    system_cfg.channel_mgr.num_validated = new_channel_mgr.num_validated;
    system_cfg.channel_mgr.num_old_version = new_channel_mgr.num_old_version;
    system_cfg.channel_mgr.num_invalid_ver = new_channel_mgr.num_invalid_ver;
    system_cfg.channel_mgr.num_input_channels 
        = new_channel_mgr.num_input_channels;
}


/* Function:    cfg_validate_all
 * Description: Validate all the channels in new_channel_mgr to be correct
 * Parameters:  N/A
 * Returns:     True or false
 */
boolean cfg_validate_all (void)
{
    int i;
    channel_cfg_t *channel_p, *old_p;
    idmgr_id_t handle;
    char message[MAX_LINE_LENGTH];

    /* Check to see whether all the channels pass the syntax and semantic
       checking */
    if (new_channel_mgr.num_input_channels != 
        new_channel_mgr.total_num_channels) {
        if (new_channel_mgr.num_input_channels >= MAX_CHANNELS) {
            snprintf(message, MAX_LINE_LENGTH,
                     "the total number of channels %d reaching "
                     "the maximum channels %d allowed",
                     new_channel_mgr.num_input_channels,
                     MAX_CHANNELS);
        }
        else {
            snprintf(message, MAX_LINE_LENGTH,
                     "%d out of %d channels failed the validation",
                     (new_channel_mgr.num_input_channels
                      - new_channel_mgr.num_validated),
                     new_channel_mgr.num_input_channels);
        }
        
        syslog_print(CFG_UPDATE_ERR, message);

        return FALSE;
    }
        
    /* Check whether the configuration manager has any data */
    for (i = 0; i < new_channel_mgr.total_num_channels; i++) {
        channel_p = cfg_channel_get(&new_channel_mgr,
                                    new_channel_mgr.handles[i]);
        if (channel_p && channel_lookup(&system_cfg.channel_mgr.channel_map,
                                        channel_p->original_source_addr,
                                        channel_p->original_source_port,
                                        &handle)) {
            old_p = cfg_channel_get(&system_cfg.channel_mgr, handle);
            if (old_p) {
            /* Compare the key first */
                if (strcmp(channel_p->session_key, old_p->session_key) == 0) {
                    if (channel_p->version == old_p->version) {
                        /* Compare the checksum */
                        if (channel_p->chksum != old_p->chksum) {
                            snprintf(message, MAX_LINE_LENGTH,
                                     "having different checksum %u "
                                     "from the existing one %u",
                                     channel_p->chksum, old_p->chksum);
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key, message);
                            new_channel_mgr.num_invalid_ver++;
                        }
                    }
                    else if (channel_p->version < old_p->version) {
                        snprintf(message, MAX_LINE_LENGTH,
                                 "having older version %llu "
                                 "from the existing one %llu",
                                 channel_p->version, old_p->version);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key, message);
                        new_channel_mgr.num_old_version++;
                    }
                }
            }
        }
    }
        

    if (new_channel_mgr.num_old_version != 0) {
        snprintf(message, MAX_LINE_LENGTH,
                 "%d channels having the old version.",
                 new_channel_mgr.num_old_version);
        syslog_print(CFG_UPDATE_ERR, message);

        return FALSE;
    }
    else if (new_channel_mgr.num_invalid_ver != 0) {
        snprintf(message, MAX_LINE_LENGTH,
                 "%d channels having the same version but contents "
                 "being changed",
                 new_channel_mgr.num_invalid_ver);
        syslog_print(CFG_UPDATE_ERR, message);

        return FALSE;
    }
    else {
        return TRUE;
    }
}


/* Function:    cfg_get_update_stats
 * Description: Get the validation stats in update config
 * Parameters:  string  Buffer for stats message
 *              length  buffer length
 * Returns:     N/A
 */
void cfg_get_update_stats (char *string, int length)
{
    if (string == NULL || length == 0) {
        return;
    }

    snprintf(string, length,
             "%d out of %d channels failed to validate; %d channels having "
             "the old version; and %d channels having the same version but "
             "contents being changed",
             (new_channel_mgr.num_input_channels
              - new_channel_mgr.num_validated),
             new_channel_mgr.num_input_channels,
             new_channel_mgr.num_old_version,
             new_channel_mgr.num_invalid_ver);
}

/* Function:    cfg_get_update_stats_values
 * Description: Get the validation stat values in update config
 * Parameters:  string  Buffer for stats message
 *              length  buffer length
 * Returns:     N/A
 */
void
cfg_get_update_stats_values (cfg_stats_t *stats)
{
    if (!stats) {
        return;
    }
    stats->num_syntax_errors = new_channel_mgr.num_syntax_errors;
}
