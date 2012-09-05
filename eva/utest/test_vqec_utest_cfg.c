/*
 * Copyright (c) 2008-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
#include "../include/utils/vam_types.h"
#include <arpa/inet.h>
#include "vqec_ifclient.h"
#include "vqec_ifclient_defs.h"
#include "vqec_syscfg.h"
#include "vqec_gap_reporter.h"

extern void 
vqec_syscfg_dump(const vqec_syscfg_t *v_cfg,
                 boolean cfg_display[]);
extern void
vqec_syscfg_set_defaults(vqec_syscfg_t *cfg);

/*
 * Unit tests for vqec_syscfg
 */

int test_vqec_cfg_init (void) {
    return 0;
}

int test_vqec_cfg_clean (void) {

    /* Ensure VQE-C is deinitialized */
    vqec_ifclient_deinit();

    return 0;
}

#define TRUE 1
#define FALSE 0

static char * test_channel_lineup = "data/utest_tuner.cfg";

boolean compare_config(vqec_syscfg_t *cfg1, vqec_syscfg_t *cfg2)
{
    boolean ret = TRUE;
    
    // fixme add more comparisions here:
    if (cfg1->max_tuners != cfg2->max_tuners) {
        ret = FALSE;
    }

    return ret;
}

static void test_vqec_cfg_not_found (void) {
    vqec_error_t err;
    vqec_syscfg_t v_cfg;
    const char *cfg_nat;
    /* 
     * Test that a config file that can't be found
     * returns the correct error code.
     */
    err = vqec_syscfg_init("data/does_not_exist.cfg");
    CU_ASSERT_EQUAL(err, VQEC_ERR_CONFIG_NOT_FOUND);

    err = vqec_syscfg_init(NULL);
    CU_ASSERT_EQUAL(err, VQEC_ERR_INVALIDARGS);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.max_tuners, VQEC_SYSCFG_DEFAULT_MAX_TUNERS);
    CU_ASSERT_STRING_EQUAL(v_cfg.channel_lineup, VQEC_SYSCFG_DEFAULT_CHANNEL_LINEUP);
    CU_ASSERT_EQUAL(v_cfg.jitter_buff_size, VQEC_SYSCFG_DEFAULT_JITTER_BUFF_SIZE);
    CU_ASSERT_EQUAL(v_cfg.repair_trigger_point_abs,
                    VQEC_SYSCFG_DEFAULT_REPAIR_TRIGGER_POINT_ABS);
    CU_ASSERT_EQUAL(v_cfg.reorder_delay_abs,
                    VQEC_SYSCFG_DEFAULT_REORDER_DELAY_ABS);
    CU_ASSERT_EQUAL(v_cfg.pakpool_size, VQEC_SYSCFG_DEFAULT_PAKPOOL_SIZE);
    CU_ASSERT_EQUAL(v_cfg.strip_rtp, VQEC_SYSCFG_DEFAULT_STRIP_RTP);
    CU_ASSERT((strlen(vqec_get_cname()) > 0));
    CU_ASSERT_STRING_EQUAL(v_cfg.input_ifname, VQEC_SYSCFG_DEFAULT_INPUT_IFNAME);
    if (v_cfg.sig_mode == VQEC_SM_NAT) {
        cfg_nat = VQEC_SYSCFG_SIG_MODE_NAT;
    } else {
        cfg_nat = VQEC_SYSCFG_SIG_MODE_STD;
    }
    CU_ASSERT_STRING_EQUAL(cfg_nat, VQEC_SYSCFG_DEFAULT_SIG_MODE);
    CU_ASSERT_EQUAL(v_cfg.nat_binding_refresh_interval, 
                    VQEC_SYSCFG_DEFAULT_NAT_BINDING_REFRESH_INTERVAL);
    CU_ASSERT_EQUAL(v_cfg.num_byes, VQEC_SYSCFG_DEFAULT_NUM_BYES);
    CU_ASSERT_EQUAL(v_cfg.bye_delay, VQEC_SYSCFG_DEFAULT_BYE_DELAY);
    vqec_syscfg_get(NULL);
}

static void test_vqec_cfg_set_defaults (void) {
    vqec_syscfg_t v_cfg;
    char *cfg_nat;

    memset(&v_cfg, 0, sizeof(vqec_syscfg_t));
    vqec_syscfg_set_defaults(&v_cfg);
    vqec_syscfg_dump(&v_cfg, NULL);
    vqec_syscfg_dump(NULL, NULL);  /* make sure dump handles NULL config */
    CU_ASSERT_EQUAL(v_cfg.max_tuners, VQEC_SYSCFG_DEFAULT_MAX_TUNERS);
    CU_ASSERT_STRING_EQUAL(v_cfg.channel_lineup, VQEC_SYSCFG_DEFAULT_CHANNEL_LINEUP);
    CU_ASSERT_EQUAL(v_cfg.jitter_buff_size, VQEC_SYSCFG_DEFAULT_JITTER_BUFF_SIZE);
    CU_ASSERT_EQUAL(v_cfg.repair_trigger_point_abs,
                    VQEC_SYSCFG_DEFAULT_REPAIR_TRIGGER_POINT_ABS);
    CU_ASSERT_EQUAL(v_cfg.reorder_delay_abs,
                    VQEC_SYSCFG_DEFAULT_REORDER_DELAY_ABS);
    CU_ASSERT_EQUAL(v_cfg.pakpool_size, VQEC_SYSCFG_DEFAULT_PAKPOOL_SIZE);
    CU_ASSERT_EQUAL(v_cfg.strip_rtp, VQEC_SYSCFG_DEFAULT_STRIP_RTP);
    CU_ASSERT((strlen(vqec_get_cname()) > 0));
    CU_ASSERT_STRING_EQUAL(v_cfg.input_ifname, VQEC_SYSCFG_DEFAULT_INPUT_IFNAME);
    if (v_cfg.sig_mode == VQEC_SM_NAT) {
        cfg_nat = VQEC_SYSCFG_SIG_MODE_NAT;
    } else {
        cfg_nat = VQEC_SYSCFG_SIG_MODE_STD;
    }
    CU_ASSERT_STRING_EQUAL(cfg_nat, VQEC_SYSCFG_DEFAULT_SIG_MODE);
    CU_ASSERT_EQUAL(v_cfg.nat_binding_refresh_interval, 
                    VQEC_SYSCFG_DEFAULT_NAT_BINDING_REFRESH_INTERVAL);
    CU_ASSERT_EQUAL(v_cfg.num_byes, VQEC_SYSCFG_DEFAULT_NUM_BYES);
    CU_ASSERT_EQUAL(v_cfg.bye_delay, VQEC_SYSCFG_DEFAULT_BYE_DELAY);
}

static void test_vqec_cfg_all_params_valid (void) {
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT_EQUAL(err, VQEC_OK);
    vqec_syscfg_get(&v_cfg);
    
    CU_ASSERT_EQUAL(v_cfg.max_tuners,27);
    CU_ASSERT_STRING_EQUAL(v_cfg.channel_lineup, test_channel_lineup);
    CU_ASSERT_EQUAL(v_cfg.jitter_buff_size,398);
    CU_ASSERT_EQUAL(v_cfg.pakpool_size,1234);
    CU_ASSERT_EQUAL(v_cfg.strip_rtp, 0);
    CU_ASSERT_STRING_EQUAL(v_cfg.input_ifname, "eth0");
    CU_ASSERT_EQUAL(v_cfg.sig_mode, VQEC_SM_NAT);
    CU_ASSERT_EQUAL(v_cfg.nat_binding_refresh_interval,2);
    CU_ASSERT_EQUAL(v_cfg.max_paksize,8000);
    CU_ASSERT_EQUAL(v_cfg.cdi_enable, 0);
    CU_ASSERT_EQUAL(v_cfg.cli_telnet_port, 9999);
    CU_ASSERT_EQUAL(v_cfg.output_pakq_limit, 400);
    CU_ASSERT_EQUAL(v_cfg.num_byes, 2);
    CU_ASSERT_EQUAL(v_cfg.bye_delay, 40);
}

static void test_vqec_cfg_all_params_valid2 (void) {
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_all_params_valid2.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_OK);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.max_tuners,22);
    CU_ASSERT_STRING_EQUAL(v_cfg.channel_lineup, test_channel_lineup);
    CU_ASSERT_EQUAL(v_cfg.jitter_buff_size,199);
    CU_ASSERT_EQUAL(v_cfg.pakpool_size,4321);
    CU_ASSERT_EQUAL(v_cfg.strip_rtp, 1);
    CU_ASSERT_STRING_EQUAL(v_cfg.input_ifname, "eth1");
    CU_ASSERT_EQUAL(v_cfg.sig_mode, VQEC_SM_NAT);
}

static void test_vqec_cfg_all_params_invalid (void) {
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_all_params_invalid.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    
    /* Verify that each parameter has retained its default value */
    CU_ASSERT_EQUAL(v_cfg.repair_trigger_point_abs,
                    VQEC_SYSCFG_DEFAULT_REPAIR_TRIGGER_POINT_ABS);
    CU_ASSERT_EQUAL(v_cfg.reorder_delay_abs,
                    VQEC_SYSCFG_DEFAULT_REORDER_DELAY_ABS);
    CU_ASSERT_EQUAL(v_cfg.update_interval_max,
                    VQEC_SYSCFG_DEFAULT_UPDATE_INTERVAL_MAX);
}

static void test_vqec_cfg_read (void)
{
    vqec_syscfg_t v_cfg;
    const vqec_syscfg_t *syscfg;
    vqec_error_t err;

    /*
     * Load a start-up configuration, and then merge with a network file
     * (as an update)
     */
    err = vqec_syscfg_init(
        "data/cfg_test_all_params_valid_with_attributes_all_valid.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_OK);
    vqec_syscfg_read(&v_cfg);
    vqec_syscfg_set(&v_cfg, TRUE);
    syscfg = vqec_syscfg_get_ptr();

    /* These parameters should match the start-up file */
    CU_ASSERT_EQUAL(syscfg->pakpool_size, 1234);
    
    /* These parameters should match the network file */
    CU_ASSERT_EQUAL(syscfg->qoe_enable, TRUE);
    CU_ASSERT_EQUAL(syscfg->error_repair_enable, FALSE);
    CU_ASSERT_EQUAL(syscfg->fec_enable, FALSE);
    CU_ASSERT_EQUAL(syscfg->rcc_enable, FALSE);
    CU_ASSERT_EQUAL(syscfg->max_receive_bandwidth_sd, 1500);
    CU_ASSERT_EQUAL(syscfg->max_receive_bandwidth_hd, 1600);
    CU_ASSERT_EQUAL(syscfg->min_hd_stream_bitrate, 1800);


    /* 
     * Load a start-up configuration, and then merge with a network file
     * (during VQE-C initialization) having VQE-C disabled
     */
    err = vqec_syscfg_init(
        "data/cfg_test_all_params_valid_with_attributes_vqec_disabled.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_OK);
    vqec_syscfg_read(&v_cfg);
    vqec_syscfg_set(&v_cfg, FALSE);

    /* These parameters should match the network file */
    CU_ASSERT_EQUAL(syscfg->qoe_enable, FALSE);
    CU_ASSERT_EQUAL(syscfg->pakpool_size, 2345);

    /*
     * Load a start-up configuration, and then merge with a network file 
     * (as an update) with parameters not supported as attributes.  
     * Make sure parameters that are not supported as attributes 
     * have not been overwritten.
     *
     * These parameters should match the start-up file
     */
    err = vqec_syscfg_init(
        "data/cfg_test_all_params_valid_with_attributes_unsupported.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_OK);
    vqec_syscfg_read(&v_cfg);
    vqec_syscfg_set(&v_cfg, TRUE);

    CU_ASSERT(strcmp(syscfg->channel_lineup, "/etc/channels.cfg"));
    CU_ASSERT(strcmp(syscfg->network_cfg_pathname, "/etc/attributes.cfg"));
    CU_ASSERT(strcmp(syscfg->domain_name_override, "domain"));
    CU_ASSERT(syscfg->log_level != 1);
    CU_ASSERT(syscfg->vcds_server_ip != 
              htonl(((1 << 24) | (1 << 16) | (1 << 8) | 1)));
    CU_ASSERT(syscfg->vcds_server_port != htons(1024));
    CU_ASSERT(vqec_ifclient_tuner_get_id_by_name("foo_foo") == 
              VQEC_TUNERID_INVALID);
    CU_ASSERT(syscfg->repair_trigger_point_abs == 20);
    CU_ASSERT(syscfg->reorder_delay_abs == 20);
                     

    /* 
     * Load a start-up configuration, and then merge with a network file
     * and override file (during VQE-C initialization).  Make sure
     * the order of precedence is override file, network file, then
     * start-up file (from highest to lowest).
     */
    err = vqec_syscfg_init("data/cfg_params_with_network_and_override.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_OK);
    vqec_syscfg_read(&v_cfg);
    vqec_syscfg_set(&v_cfg, FALSE);

    /* These parameters should match the start-up file */
    CU_ASSERT_EQUAL(syscfg->max_tuners, 27);
    CU_ASSERT_EQUAL(syscfg->pakpool_size, 1234);
    CU_ASSERT_EQUAL(syscfg->log_level, 4);
    CU_ASSERT_EQUAL(syscfg->so_rcvbuf, 129000);

    /* These parameters should match the network file */
    CU_ASSERT_EQUAL(syscfg->max_receive_bandwidth_sd, 1500);

    /* These parameters should match the override file */
    CU_ASSERT_EQUAL(syscfg->qoe_enable, FALSE);
    CU_ASSERT_EQUAL(syscfg->error_repair_enable, TRUE);    
    CU_ASSERT_EQUAL(syscfg->fec_enable, TRUE);    
    CU_ASSERT_EQUAL(syscfg->rcc_enable, TRUE);    
    CU_ASSERT_EQUAL(syscfg->jitter_buff_size, 877);    
    CU_ASSERT_EQUAL(syscfg->max_receive_bandwidth_hd, 2600);
    CU_ASSERT_EQUAL(syscfg->min_hd_stream_bitrate, 2800);

    vqec_syscfg_dump_overrides();
    vqec_syscfg_dump_attributes();
    vqec_syscfg_dump_system();
    vqec_syscfg_dump_defaults();
}

/*
 * Loads a file into a memory buffer allocatd by this function.
 *
 * Note:  caller must free the allocated buffer.
 *
 * @param[in]  filename   - file to read
 * @param[out] buffer     - contains file contents (NULL-terminated)
 * @param[out] int        - 0 or greater - successful, size of buffer
 *                          -1           - error, file not found
 *                          -2           - error, could not read file
 *                          -3           - error, could not allocate buffer
 */
static int
load_to_memory (const char *filename, char **buffer)
{
    int size = 0;
    FILE *fp = fopen(filename, "r");

    if (fp == NULL) {
        *buffer = NULL;
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *buffer = (char *)malloc(size+1);
    if (!*buffer) {
        fclose(fp);
        return -3;
    }
    if (size != fread(*buffer, sizeof(char), size, fp)) {
        free(*buffer);
        fclose(fp);
        return -2;
    }
    fclose(fp);
    (*buffer)[size] = '\0';
    
    return (size+1);
}

/*
 * For testing VQE-C event callback notifications
 */
static boolean test_vqec_config_register_passed = FALSE;
static void
test_vqec_config_cb (vqec_ifclient_config_event_params_t *params)
{
    if ((params->config == VQEC_IFCLIENT_CONFIG_NETWORK) &&
        (params->event == VQEC_IFCLIENT_CONFIG_EVENT_CONFIG_INVALID)) {
        test_vqec_config_register_passed = TRUE;
    }
}

/*
 * Test config delivery functionality
 */
static void test_vqec_cfg_delivery (void)
{
    const vqec_syscfg_t *syscfg;
    vqec_error_t status;
    char *buffer = NULL;
    vqec_ifclient_config_update_params_t params_update;
    vqec_ifclient_config_override_params_t params_override;
    vqec_ifclient_config_status_params_t params_status;
    vqec_ifclient_config_register_params_t params_register;
#define STRLEN_MAX 80
    char cmd_corrupt[STRLEN_MAX];

    /* Ensure VQE-C is deinitialized */
    vqec_ifclient_deinit();
    sleep(5);

    /* Remove any left-over configuration updates */
    remove("data/temp_network.cfg");
    remove("data/temp_override.cfg");
    remove("data/temp_index.cfg");
    remove("data/temp_channel.cfg");

    /*
     * Initialize VQE-C API behavior with VQE-C disabled.
     */
    status = vqec_ifclient_init("data/cfg_test_qoe_enable_false.cfg");
    printf("error = %s\n", vqec_err2str(status));
    CU_ASSERT_EQUAL(status, VQEC_ERR_DISABLED);
    syscfg = vqec_syscfg_get_ptr();

    /* Perform an update of network file, verify it was accepted */
    CU_ASSERT(load_to_memory("data/attributes_qoe_enable_true.cfg", 
                             &buffer) > 0);
    params_update.config = VQEC_IFCLIENT_CONFIG_NETWORK;
    params_update.buffer = buffer;
    status = vqec_ifclient_config_update(&params_update);
    CU_ASSERT_EQUAL(status, VQEC_OK);
    CU_ASSERT_EQUAL(params_update.persisted, TRUE);
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }
    params_status.config = VQEC_IFCLIENT_CONFIG_NETWORK;
    status = vqec_ifclient_config_status(&params_status);
    CU_ASSERT_EQUAL(status, VQEC_OK);
    CU_ASSERT(params_status.persistent);
    /* Note:  changes in copyright date of the file below can cause failure */
    CU_ASSERT(!strcmp("e30ffaae07b8ac0cf594474672699438", params_status.md5));
    CU_ASSERT(strcmp("<unavailable>", params_status.last_update_timestamp));
    CU_ASSERT_EQUAL(VQEC_IFCLIENT_CONFIG_STATUS_NA, params_status.status);

    /* Ensure VQE-C is deinitialized */
    vqec_ifclient_deinit();
    sleep(5);

    /* Remove any left-over configuration updates */
    remove("data/temp_network.cfg");
    remove("data/temp_override.cfg");
    remove("data/temp_index.cfg");
    remove("data/temp_channel.cfg");

    /*
     * Initialize VQE-C using only a start-up configuration
     * (no other persisted configurations exist)
     */
    status = vqec_ifclient_init("data/cfg_test_index.cfg");
    printf("error = %s\n", vqec_err2str(status));
    CU_ASSERT_EQUAL(status, VQEC_OK);
    syscfg = vqec_syscfg_get_ptr();

    /* Register a callback for corruptions to the network config */
    params_register.config = VQEC_IFCLIENT_CONFIG_NETWORK;
    params_register.event_cb = test_vqec_config_cb;
    status = vqec_ifclient_config_register(&params_register);
    CU_ASSERT_EQUAL(status, VQEC_OK);
    
    /* Perform an update of network file */
    CU_ASSERT(load_to_memory("data/attributes_all_valid.cfg", &buffer) > 0);
    params_update.config = VQEC_IFCLIENT_CONFIG_NETWORK;
    params_update.buffer = buffer;
    status = vqec_ifclient_config_update(&params_update);
    CU_ASSERT_EQUAL(status, VQEC_OK);
    CU_ASSERT_EQUAL(params_update.persisted, TRUE);
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }

    /* Verify it has been incorporated into the system config */
    CU_ASSERT_EQUAL(syscfg->jitter_buff_size, 800);

    /* Verify the network file status information */
    params_status.config = VQEC_IFCLIENT_CONFIG_NETWORK;
    status = vqec_ifclient_config_status(&params_status);
    CU_ASSERT_EQUAL(status, VQEC_OK);
    CU_ASSERT(params_status.persistent);
    /* Note:  changes in copyright date of the file below can cause failure */
    CU_ASSERT(!strcmp("fa6e6ffdcccaa966e4b67ea39c55b95a", params_status.md5));
    CU_ASSERT(strcmp("<unavailable>", params_status.last_update_timestamp));
    CU_ASSERT_EQUAL(VQEC_IFCLIENT_CONFIG_STATUS_VALID, params_status.status);


    /* Corrupt the network file on the filesystem */
    snprintf(cmd_corrupt, STRLEN_MAX, 
             "sed s/800/900/g %s > %s", syscfg->network_cfg_pathname,
             syscfg->network_cfg_pathname);
    system(cmd_corrupt);
    
    /* Verify the network file status information (while corrupted) */
    params_status.config = VQEC_IFCLIENT_CONFIG_NETWORK;
    status = vqec_ifclient_config_status(&params_status);
    CU_ASSERT_EQUAL(status, VQEC_OK);
    CU_ASSERT(params_status.persistent);
    CU_ASSERT_EQUAL(VQEC_IFCLIENT_CONFIG_STATUS_INVALID, 
                    params_status.status);

    /* Perform an update of override tags */
    char *tags[] = {"error_repair_enable", "rcc_enable", NULL};
    char *values[] = {"false", "false", NULL};
    params_override.tags = (const char **)tags;
    params_override.values = (const char **)values;
    status = vqec_ifclient_config_override_update(&params_override);
    CU_ASSERT_EQUAL(status, VQEC_OK);

    /*
     * Verify that
     *   1. the corrupt network file is now excluded from the system config
     *      (jitter_buff_size from the start-up config bleeds through)
     *   2. overrides have been incorporated into the system config
     */
    CU_ASSERT_EQUAL(syscfg->jitter_buff_size, 398);
    CU_ASSERT_EQUAL(syscfg->error_repair_enable, FALSE);
    CU_ASSERT_EQUAL(syscfg->rcc_enable, FALSE);

    /*
     * Verify that the registered callback for events on the network
     * configuration was called, due to an attempt at reading the
     * corrupt network file after processing the override config update.
     */
    CU_ASSERT(test_vqec_config_register_passed);

    /* Verify the override file status information */
    params_status.config = VQEC_IFCLIENT_CONFIG_OVERRIDE;
    status = vqec_ifclient_config_status(&params_status);
    CU_ASSERT_EQUAL(status, VQEC_OK);
    CU_ASSERT(params_status.persistent);
    CU_ASSERT(!strcmp("248f4116ba881186d05f8ebbcd4075b3", params_status.md5));
    CU_ASSERT(strcmp("<unavailable>", params_status.last_update_timestamp));
    CU_ASSERT_EQUAL(VQEC_IFCLIENT_CONFIG_STATUS_VALID, 
                    params_status.status);

    /* Perform an update of channel file */
    CU_ASSERT(load_to_memory("data/utest_tuner.cfg", &buffer) > 0);
    params_update.config = VQEC_IFCLIENT_CONFIG_CHANNEL;
    params_update.buffer = buffer;
    status = vqec_ifclient_config_update(&params_update);
    CU_ASSERT_EQUAL(status, VQEC_OK);
    CU_ASSERT_EQUAL(params_update.persisted, FALSE);
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }

    /* 
     * Verify the channel config status information 
     * (none persisted, but last update time indicates an update)
     */
    params_status.config = VQEC_IFCLIENT_CONFIG_CHANNEL;
    status = vqec_ifclient_config_status(&params_status);
    CU_ASSERT_EQUAL(status, VQEC_OK);
    CU_ASSERT(!params_status.persistent);
    CU_ASSERT(!strcmp("<file not available>", params_status.md5));
    CU_ASSERT(strcmp("<unavailable>", params_status.last_update_timestamp));
    CU_ASSERT_EQUAL(VQEC_IFCLIENT_CONFIG_STATUS_NA, 
                    params_status.status);

    /* Clean up configuration files */
    remove(syscfg->index_cfg_pathname);
    remove(syscfg->channel_lineup);
    remove(syscfg->network_cfg_pathname);
    remove(syscfg->override_cfg_pathname);
}

static void test_vqec_cfg_range_err_max_tuners (void) {
    /*
     * configuration file contains an invalid number of tuners
     */
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_range_err_max_tuners1.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    /* default value for max_tuners, not configured value 0 */
    CU_ASSERT_EQUAL(v_cfg.max_tuners,1);
    CU_ASSERT_STRING_EQUAL(v_cfg.channel_lineup, test_channel_lineup);
    CU_ASSERT_EQUAL(v_cfg.jitter_buff_size,200);
    CU_ASSERT_EQUAL(v_cfg.pakpool_size,4321);
    CU_ASSERT_EQUAL(v_cfg.strip_rtp, 0);
    CU_ASSERT_STRING_EQUAL(v_cfg.input_ifname, "eth1");
    CU_ASSERT_EQUAL(v_cfg.sig_mode, VQEC_SM_NAT);

    /* Assert that we get VQEC_ERR_PARAMRANGEINVALID error for this case */
    err = vqec_syscfg_init("data/cfg_test_range_err_max_tuners2.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    /* Assert max_tuners value is unchanged from what is in the file */
    CU_ASSERT_EQUAL(v_cfg.max_tuners, 2);
}

static void test_vqec_cfg_range_err_jitter_buff (void) {
    /*
     * configuration file contains jitter_buffer_size of 20001 which is greater than
     * VQEC_SYSCFG_MAX_JITTER_BUFF_SIZE.
     */
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_range_err_jitter_buff.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.max_tuners,22);
    CU_ASSERT_STRING_EQUAL(v_cfg.channel_lineup, test_channel_lineup);
    CU_ASSERT_EQUAL(v_cfg.jitter_buff_size,VQEC_SYSCFG_DEFAULT_JITTER_BUFF_SIZE);  /* get default not configured jitter buffer size */
    CU_ASSERT_EQUAL(v_cfg.pakpool_size,4321);
    CU_ASSERT_EQUAL(v_cfg.strip_rtp, 1);
    CU_ASSERT_STRING_EQUAL(v_cfg.input_ifname, "eth1");
    CU_ASSERT_EQUAL(v_cfg.sig_mode, VQEC_SM_NAT);
}

static void test_vqec_cfg_range_err_repair_trigger (void) {
    /*
     * configuration file contains repair_trigger_point value of 101
     * which is greater than VQEC_SYSCFG_MAX_REPAIR_TRIGGER_POINT.
     */
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_range_err_repair_trigger.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.max_tuners,22);
    CU_ASSERT_STRING_EQUAL(v_cfg.channel_lineup, test_channel_lineup);
    CU_ASSERT_EQUAL(v_cfg.jitter_buff_size,899);  
    CU_ASSERT_EQUAL(v_cfg.pakpool_size,4321);
    CU_ASSERT_EQUAL(v_cfg.strip_rtp, 1);
    CU_ASSERT_STRING_EQUAL(v_cfg.input_ifname, "eth1");
    CU_ASSERT_EQUAL(v_cfg.sig_mode, VQEC_SM_NAT);
}

static void test_vqec_cfg_range_err_pakpool_size (void) {
    /*
     * configuration file contains an invalid number of pakpool_size.
     */
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_range_err_pakpool_size.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.max_tuners,22);
    CU_ASSERT_STRING_EQUAL(v_cfg.channel_lineup, test_channel_lineup);
    CU_ASSERT_EQUAL(v_cfg.jitter_buff_size,899);  
    CU_ASSERT_EQUAL(v_cfg.pakpool_size,VQEC_SYSCFG_DEFAULT_PAKPOOL_SIZE);     /* get default, not configured pakpool_size point */
    CU_ASSERT_EQUAL(v_cfg.strip_rtp, 1);
    CU_ASSERT_STRING_EQUAL(v_cfg.input_ifname, "eth1");
    CU_ASSERT_EQUAL(v_cfg.sig_mode, VQEC_SM_NAT);
}

static void test_vqec_cfg_range_err_sig_mode (void) {
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_range_err_sig_mode.cfg");
    printf("error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.max_tuners,22);
    CU_ASSERT_STRING_EQUAL(v_cfg.channel_lineup, test_channel_lineup);
    CU_ASSERT_EQUAL(v_cfg.jitter_buff_size,899);  
    CU_ASSERT_EQUAL(v_cfg.pakpool_size,10000);     
    CU_ASSERT_EQUAL(v_cfg.strip_rtp, 1);
    CU_ASSERT_STRING_EQUAL(v_cfg.input_ifname, "eth1");
    CU_ASSERT_EQUAL(v_cfg.sig_mode, VQEC_SM_NAT); /* get default, not configured sig_mode value */
}

static void test_vqec_cfg_range_err_nat_binding_refresh_interval (void) {
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_range_err_nat_binding.cfg");
    printf("nat_binding_refresh_interval range error = %d\n", err);
    
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.nat_binding_refresh_interval,
                    VQEC_SYSCFG_DEFAULT_NAT_BINDING_REFRESH_INTERVAL);
    CU_ASSERT_NOT_EQUAL(VQEC_SYSCFG_DEFAULT_NAT_BINDING_REFRESH_INTERVAL, 
                        111);
    CU_ASSERT_EQUAL(v_cfg.cli_telnet_port, 18888);
}

static void test_vqec_cfg_range_err_max_paksize (void) {
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_range_err_max_paksize.cfg");
    printf("max_paksize range error = %d\n", err);
    
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.max_paksize,
                    VQEC_SYSCFG_DEFAULT_MAX_PAKSIZE);
    CU_ASSERT_NOT_EQUAL(VQEC_SYSCFG_DEFAULT_MAX_PAKSIZE, 
                        111);
    CU_ASSERT_EQUAL(v_cfg.cli_telnet_port, 18888);
}

static void test_vqec_cfg_range_err_cli_telnet_port (void) {
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_range_err_libcli_telnet_port.cfg");
    printf("libcli_telnet_port range error = %d\n", err);
    
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.cli_telnet_port,
                    VQEC_SYSCFG_DEFAULT_CLI_TELNET_PORT);
    CU_ASSERT_NOT_EQUAL(VQEC_SYSCFG_DEFAULT_CLI_TELNET_PORT, 
                        111);
    CU_ASSERT_EQUAL(v_cfg.nat_binding_refresh_interval, 11111);
}

static void test_vqec_cfg_range_err_output_pakq_limit (void) {
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_range_err_output_pakq_limit.cfg");
    printf("output_pakq_limit range error = %d\n", err);
    
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.output_pakq_limit,
                    VQEC_SYSCFG_DEFAULT_OUTPUT_PAKQ_LIMIT);
    CU_ASSERT_NOT_EQUAL(VQEC_SYSCFG_DEFAULT_OUTPUT_PAKQ_LIMIT, 
                        111);
    CU_ASSERT_EQUAL(v_cfg.cli_telnet_port, 18888);
}

static void test_vqec_cfg_domain_name_override (void) {
    vqec_syscfg_t v_cfg;
    vqec_error_t err;
    err = vqec_syscfg_init("data/cfg_test_domain_name_override.cfg");
    printf("domain_name_override range error = %d\n", err);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_STRING_EQUAL(v_cfg.domain_name_override, 
                           "mydomain");
    CU_ASSERT_EQUAL(v_cfg.cli_telnet_port, 18888);
}

static void test_vqec_cfg_error_repair_policer (void) {
    vqec_syscfg_t v_cfg;
    vqec_error_t err;

    /*
     * Verify illegal policer values rejected, and that default values 
     * were used in place of rejected values.  This is done for three
     * sets of illegal values (below the supported min, above the
     * supported max, and well above the supported max).
     */
    err = vqec_syscfg_init("data/cfg_test_range_err_repair_policer1.cfg");
    CU_ASSERT(err == VQEC_ERR_PARAMRANGEINVALID);    
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.error_repair_policer_rate,
                    VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_RATE);
    CU_ASSERT_EQUAL(v_cfg.error_repair_policer_burst,
                    VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_BURST);

    err = vqec_syscfg_init("data/cfg_test_range_err_repair_policer2.cfg");
    CU_ASSERT(err == VQEC_ERR_PARAMRANGEINVALID);    
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.error_repair_policer_rate,
                    VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_RATE);
    CU_ASSERT_EQUAL(v_cfg.error_repair_policer_burst,
                    VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_BURST);

    err = vqec_syscfg_init("data/cfg_test_range_err_repair_policer3.cfg");
    CU_ASSERT(err == VQEC_ERR_PARAMRANGEINVALID);    
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.error_repair_policer_rate,
                    VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_RATE);
    CU_ASSERT_EQUAL(v_cfg.error_repair_policer_burst,
                    VQEC_SYSCFG_DEFAULT_ERROR_REPAIR_POLICER_BURST);

    /*
     * Verify that legal values are accepted and match expected values.
     */
    err = vqec_syscfg_init("data/cfg_test_all_params_valid.cfg");
    CU_ASSERT_EQUAL(err, VQEC_OK);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.error_repair_policer_rate, 4);
    CU_ASSERT_EQUAL(v_cfg.error_repair_policer_burst, 1);
}

static void test_vqec_cfg_error_2nd_bye (void) {
    vqec_syscfg_t v_cfg;
    vqec_error_t err;

    err = vqec_syscfg_init("data/cfg_test_range_err_2nd_bye.cfg");
    printf("2nd BYE range error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.num_byes, VQEC_SYSCFG_DEFAULT_NUM_BYES);
    CU_ASSERT_EQUAL(v_cfg.bye_delay, VQEC_SYSCFG_DEFAULT_BYE_DELAY);
 
    err = vqec_syscfg_init("data/cfg_test_range_err_2nd_bye1.cfg");
    printf("2nd BYE range error = %d\n", err);
    CU_ASSERT_EQUAL(err, VQEC_ERR_PARAMRANGEINVALID);
    vqec_syscfg_get(&v_cfg);
    CU_ASSERT_EQUAL(v_cfg.num_byes, VQEC_SYSCFG_DEFAULT_NUM_BYES);
    CU_ASSERT_EQUAL(v_cfg.bye_delay, VQEC_SYSCFG_DEFAULT_BYE_DELAY);
}

void
test_vqec_cfg_dump (void)
{
    vqec_syscfg_dump_startup();
    vqec_syscfg_dump_attributes();
    /* if we got here, no crash during display */
    CU_ASSERT(1);
}

CU_TestInfo test_array_cfg[] = {
    {"test vqec_cfg_not_found",test_vqec_cfg_not_found},
    {"test vqec_cfg_set_defaults",test_vqec_cfg_set_defaults},
    {"test vqec_cfg_all_params_valid",test_vqec_cfg_all_params_valid},
    {"test vqec_cfg_all_params_valid2",test_vqec_cfg_all_params_valid2},
    {"test vqec_cfg_all_params_invalid",test_vqec_cfg_all_params_invalid},
    {"test vqec_cfg_read",test_vqec_cfg_read},
    {"test_vqec_cfg_delivery",test_vqec_cfg_delivery},
    {"test vqec_cfg_range_err_max_tuners",test_vqec_cfg_range_err_max_tuners},
    {"test vqec_cfg_range_err_jitter_buff",test_vqec_cfg_range_err_jitter_buff},
    {"test vqec_cfg_range_err_repair_trigger",test_vqec_cfg_range_err_repair_trigger},
    {"test vqec_cfg_range_err_pakpool_size",test_vqec_cfg_range_err_pakpool_size},
    {"test vqec_cfg_range_err_sig_mode",test_vqec_cfg_range_err_sig_mode},
    {"test vqec_cfg_range_err_nat_binding_refresh_interval",
     test_vqec_cfg_range_err_nat_binding_refresh_interval},
    {"test vqec_cfg_range_err_max_paksize",
     test_vqec_cfg_range_err_max_paksize},
    {"test vqec_cfg_range_err_cli_telnet_port",
     test_vqec_cfg_range_err_cli_telnet_port},
    {"test vqec_cfg_range_err_output_pakq_limit",
     test_vqec_cfg_range_err_output_pakq_limit},
    {"test vqec_cfg_domain_name_override",
     test_vqec_cfg_domain_name_override},
    {"test_vqec_cfg_error_repair_policer",
     test_vqec_cfg_error_repair_policer},
    {"test_vqec_cfg_error_2nd_bye", test_vqec_cfg_error_2nd_bye},
    {"test_vqec_cfg_dump", test_vqec_cfg_dump},
    CU_TEST_INFO_NULL,
};



