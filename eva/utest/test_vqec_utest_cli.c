/*
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vam_util.h"
#include "vqec_cli_register.h"
#include "vqec_tuner.h"
#include "vqec_lock_defs.h"
#include "vqec_assert_macros.h"
#include "vqec_tuner.h"
#include "vqec_debug.h"
#include "vqec_stream_output_thread_mgr.h"
#include "vqec_syscfg.h"

#include "vqec_gap_reporter.h"
#include "vqec_cli.h"
#include "vqec_pthread.h"
#include "vqec_channel_api.h"
#include "vqec_channel_private.h"

extern void vqec_str_to_lower(char *outputString, char *inputString);
extern int vqec_str_match(char *s, char *params[]);
extern int vqec_check_args_for_help_char(char *argv[], int argc);
extern void vqec_print_callback(struct vqec_cli_def *cli, char *string, int
                                string_len);
extern int vqec_cmd_show_tuner(struct vqec_cli_def *cli, char *command, 
                               char *argv[], int argc);
extern int vqec_cmd_show_channel(struct vqec_cli_def *cli, char *command, 
                                 char *argv[], int argc);
extern int vqec_cmd_show_drop(struct vqec_cli_def *cli, char *command, 
                              char *argv[], int argc);
extern int vqec_cmd_show_error_repair(struct vqec_cli_def *cli, 
                                      char *command, char *argv[], int argc);
extern int vqec_cmd_show_counters(struct vqec_cli_def *cli, 
                                  char *command, char *argv[], int argc);
extern int vqec_cmd_show_debug(struct vqec_cli_def *cli, 
                               char *command, char *argv[], int argc);
extern int vqec_cmd_show_system_config(struct vqec_cli_def *cli, 
                                       char *command, char *argv[], int argc);
extern int vqec_cmd_drop_interval(struct vqec_cli_def *cli, 
                                  char *command, char *argv[], int argc);
extern int vqec_cmd_drop_percentage(struct vqec_cli_def *cli, 
                                    char *command, char *argv[], int argc);
extern int vqec_cmd_drop_enable(struct vqec_cli_def *cli, 
                                char *command, char *argv[], int argc);
extern int vqec_cmd_drop_disable(struct vqec_cli_def *cli, 
                                 char *command, char *argv[], int argc);
extern int vqec_cmd_error_repair_enable(struct vqec_cli_def *cli, 
                                        char *command, char *argv[], int argc);
extern int vqec_cmd_error_repair_disable(struct vqec_cli_def *cli, 
                                         char *command, char *argv[], int argc);
extern int vqec_cmd_tuner_bind(struct vqec_cli_def *cli, 
                               char *command, char *argv[], int argc);
extern int vqec_cmd_tuner_unbind(struct vqec_cli_def *cli, 
                                 char *command, char *argv[], int argc);
extern int vqec_cmd_tuner_create(struct vqec_cli_def *cli, 
                                 char *command, char *argv[], int argc);
extern int vqec_cmd_tuner_destroy(struct vqec_cli_def *cli, 
                                  char *command, char *argv[], int argc);
extern int vqec_cmd_channel_update(struct vqec_cli_def *cli, 
                                   char *command, 
                                   char *argv[], int argc);
extern int vqec_cmd_update(struct vqec_cli_def *cli, 
                           char *command, char *argv[], int argc);
extern int vqec_cmd_clear_counters(struct vqec_cli_def *cli, 
                                   char *command, char *argv[], int argc);
extern int vqec_cmd_channel_tr135(struct vqec_cli_def *cli, char *command, 
                                  char *argv[], int argc);
extern int vqec_debug_func(struct vqec_cli_def *cli, 
                           char *command, char *argv[], int argc);
extern int vqec_cmd_proxy_igmp_join(struct vqec_cli_def *cli, 
                                    char *command, char *argv[], int argc);
extern int vqec_cmd_stream_output(struct vqec_cli_def *cli, 
                                  char *command, char *argv[], int argc);
extern int vqec_cmd_monitor_elog(struct vqec_cli_def *cli, char *command, 
                                 char *argv[], int argc);
extern int vqec_cmd_monitor_outputsched_show(struct vqec_cli_def *cli,
                                             char *command, 
                                             char *argv[], int argc);
extern int vqec_cmd_monitor_outputsched_off(struct vqec_cli_def *cli,
                                            char *command, 
                                            char *argv[], int argc);
extern int vqec_cmd_monitor_outputsched_on(struct vqec_cli_def *cli,
                                           char *command, 
                                           char *argv[], int argc);
extern int vqec_cmd_monitor_outputsched_reset (struct vqec_cli_def *cli,
                                               char *command, 
                                               char *argv[], int argc);
extern int vqec_cmd_error_repair_policer_enable(struct vqec_cli_def *cli, 
                                                char *command, 
                                                char *argv[], int argc);
extern int vqec_cmd_error_repair_policer_disable(struct vqec_cli_def *cli, 
                                                 char *command, char *argv[], 
                                                 int argc);
extern int vqec_cmd_error_repair_policer_rate(struct vqec_cli_def *cli, 
                                              char *command, 
                                              char *argv[], int argc);
extern int vqec_cmd_error_repair_policer_burst(struct vqec_cli_def *cli, 
                                               char *command, 
                                               char *argv[], int argc);
extern int vqec_cmd_show_fec(struct vqec_cli_def *cli, 
                             char *command, char *argv[], int argc);
extern int vqec_cmd_fec_enable(struct vqec_cli_def *cli, 
                               char *command,char *argv[], int argc);
extern int vqec_cmd_fec_disable(struct vqec_cli_def *cli, 
                                char *command, char *argv[], int argc);
extern int vqec_cli_help_func(struct vqec_cli_def *cli,
                              char *command, char *argv[], int argc);
extern int vqec_cmd_show_tech_support(struct vqec_cli_def *cli,
                                      char *command, char *argv[], int argc);
extern vqec_chan_t *
vqec_chan_find (in_addr_t ip, in_port_t port);

/*
 * Unit tests for vqec_cli
 */

void *cli_handle = 0;
int vqec_cli_port = 8181;
in_addr_t vqec_cli_if_addr = INADDR_ANY;

/* elog status report */
extern void status_report(void);

void *vqec_utest_cli_thread(void* arg)
{
    cli_handle = vqec_cli_startup();
    vqec_cli_loop_wrapper(vqec_cli_port, vqec_cli_if_addr);
    return NULL;
}

int test_vqec_cli_init (void) {
    pthread_t vqec_cli_thread_id;
    vqec_tunerid_t deftuner;
    vqec_pthread_create(&vqec_cli_thread_id,vqec_utest_cli_thread, NULL);
    printf("please telnet to port %d to cover telnet session code...\n", 
           vqec_cli_port);
    sleep(1);

    vqec_ifclient_init("data/cfg_test_all_params_valid.cfg");
    vqec_stream_output_thread_mgr_module_init();
  
    vqec_ifclient_tuner_create(&deftuner, "deftuner");
    return 0;
}

int test_vqec_cli_clean (void) {
    vqec_stream_output_thread_mgr_module_deinit();    
    vqec_ifclient_deinit();
    return 0;
}

static void test_vqec_cli_helper_funcs (void) {
    char *varlist[] = { "first", "second", "third", "fourth", "fifth", 
                        (char *)0 };
    if(varlist);
    CU_ASSERT(vqec_str_match("first", varlist) == 0);
    CU_ASSERT(vqec_str_match("second", varlist) == 1);
    CU_ASSERT(vqec_str_match("THird", varlist) == 2);
    CU_ASSERT(vqec_str_match("four", varlist) == 3);
    CU_ASSERT(vqec_str_match("fiFth", varlist) == 4);
    CU_ASSERT(vqec_str_match("dfirst", varlist) == -1);
    CU_ASSERT(vqec_str_match("fi", varlist) == VQEC_CLI_ERROR);
    CU_ASSERT(vqec_str_match(NULL, varlist) == VQEC_CLI_ERROR);

    char *argv[] = {"arg1","arg2"};
    char *argv2[] = {"ar?"};
    CU_ASSERT(vqec_check_args_for_help_char(argv, 2) == FALSE);
    CU_ASSERT(vqec_check_args_for_help_char(argv2, 1) == TRUE);

    vqec_print_callback(cli_handle, "hello world!", strlen("hello world!"));

    int test_cli_fd;
    test_cli_fd = vqec_get_cli_fd(cli_handle);
    CU_ASSERT(test_cli_fd);
}

static void test_vqec_cli_show_cmds (void) {
    int cmdstat = 0;
    char *args1[] = { };
    char *args2[] = { "deftuner" };
    char *args3[] = { "deftuner?" };
    char *args4[] = { "invalidtuner" };
    char *args5[] = { "name", "deftuner" };
    char *args6[] = { "cumulative" };

    char *shtargs1[] = { "all" };
    char *shtargs2[] = { "name", "deftuner" };
    char *shtargs3[] = { "name", "deftuner?" };
    char *shtargs4[] = { "name", "invalidtuner" };
    char *shtargs5[] = { "join-delay" };

    char *shcargs1[] = { "counters" };
    char *shcargs2[] = { "counters", "all" };
    char *shcargs3[] = { "counters", "url" };
    char *shcargs4[] = { "config" };
    char *shcargs5[] = { "config" , "all" };
    char *shcargs6[] = { "config", "url" };
    char *shcargs7[] = { "counters", "all", "cumulative" };

    /*
     * The vqec_cmd_show_system_config() function requires that the array 
     * arguments be writable, because they are passed to strtok_r().  
     * So the arrays of string literals cannot be used, and an array of
     * pointers to writable memory is needed instead.  Since this an issue
     * for just one unit test, the variables below will be used.
     */
    char deftuner[] = "deftuner";
    char *args2_writable[] = { deftuner };

    /* show tuner */
    cmdstat = vqec_cmd_show_tuner(cli_handle, "show tuner", shtargs1, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_tuner(cli_handle, "show tuner", shtargs2, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_tuner(cli_handle, "show tuner", shtargs3, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_show_tuner(cli_handle, "show tuner", shtargs4, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_show_tuner(cli_handle, "show tuner", shtargs5, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* show channels */
    cmdstat = vqec_cmd_show_channel(cli_handle, "show channel", args1, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_channel(cli_handle, "show channel", args2, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_show_channel(cli_handle, "show channel", shcargs1, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_show_channel(cli_handle, "show channel", shcargs2, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_channel(cli_handle, "show channel", shcargs3, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_show_channel(cli_handle, "show channel", shcargs4, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_show_channel(cli_handle, "show channel", shcargs5, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_channel(cli_handle, "show channel", shcargs6, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_show_channel(cli_handle, "show channel", shcargs7, 3);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* show drop */
    cmdstat = vqec_cmd_show_drop(cli_handle, "show drop", args1, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_drop(cli_handle, "show drop", args2, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    /* show error-repair */
    cmdstat = vqec_cmd_show_error_repair(cli_handle, "show error_repair", 
                                         args1, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_error_repair(cli_handle, "show error_repair", 
                                         args2, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    /*show fec */
    cmdstat = vqec_cmd_show_fec(cli_handle, "show fec", args1, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_fec(cli_handle, "show fec", args2, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    /* show counters */
    cmdstat = vqec_cmd_show_counters(cli_handle, "show counters", args1, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_counters(cli_handle, "show counters", args5, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_counters(cli_handle, "show counters", args3, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_show_counters(cli_handle, "show counters", args4, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_show_counters(cli_handle, "show counters", args6, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* show debug */
    cmdstat = vqec_cmd_show_debug(cli_handle, "show debug", args1, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_debug(cli_handle, "show debug", args2, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    /* show system-config */
    cmdstat = vqec_cmd_show_system_config(cli_handle, 
                                          "show system-config", args1, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_show_system_config(cli_handle, 
                                          "show system-config", 
                                           args2_writable, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    /* show tech-support */
    cmdstat = vqec_cmd_show_tech_support(cli_handle, 
                                         "show tech-support", args1, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    /* Pass some argument to see the function return an error */
    cmdstat = vqec_cmd_show_tech_support(cli_handle, 
                                         "show tech-support", args2, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
}

static void test_vqec_cli_global_cmds (void) {
    int cmdstat = 0;
    uint32_t rate, burst;
    vqec_dp_drop_sim_state_t state;
    boolean policer_enabled;

    char *args11[] = { };
    char *args12[] = { "?" };

    char *args31[] = { "1" };
    char *args32[] = { "1", "2" };


    /* error-repair */
    CU_ASSERT(vqec_get_error_repair(NULL));  /* enabled by default */

    cmdstat = vqec_cmd_error_repair_disable(cli_handle, 
                                            "error-repair disable", args11, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!vqec_get_error_repair(NULL));

    cmdstat = vqec_cmd_error_repair_enable(cli_handle, 
                                           "error-repair enable", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(!vqec_get_error_repair(NULL));

    cmdstat = vqec_cmd_error_repair_enable(cli_handle, 
                                           "error-repair enable", args11, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(vqec_get_error_repair(NULL));

    cmdstat = vqec_cmd_error_repair_disable(cli_handle, 
                                            "error-repair disable", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(vqec_get_error_repair(NULL));

   /* fec */
    boolean fec;

    /* enabled by default */
    CU_ASSERT(vqec_dp_get_fec(&fec, NULL) == VQEC_DP_ERR_OK);  

    cmdstat = vqec_cmd_fec_disable(cli_handle, "fec disable", args11, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(vqec_dp_get_fec(&fec, NULL) == VQEC_DP_ERR_OK);

    cmdstat = vqec_cmd_fec_enable(cli_handle, "fec enable", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(vqec_dp_get_fec(&fec, NULL) == VQEC_DP_ERR_OK);

    cmdstat = vqec_cmd_fec_enable(cli_handle, "fec enable", args11, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(vqec_dp_get_fec(&fec, NULL) == VQEC_DP_ERR_OK);

    cmdstat = vqec_cmd_fec_disable(cli_handle, "fec disable", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(vqec_dp_get_fec(&fec, NULL) == VQEC_DP_ERR_OK);

    /* error repair policer enable */
    cmdstat = vqec_cmd_error_repair_policer_enable(
        cli_handle, "error-repair policer enable", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_error_repair_policer_enable(
        cli_handle, "error-repair policer enable", args11, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    vqec_error_repair_policer_parameters_get(&policer_enabled, NULL,
                                             NULL, NULL,
                                             NULL, NULL);
    CU_ASSERT(policer_enabled);

    /* error repair policer disable */
    cmdstat = vqec_cmd_error_repair_policer_disable(
        cli_handle, "error-repair policer disable", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_error_repair_policer_disable(
        cli_handle, "error-repair policer disable", args11, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    vqec_error_repair_policer_parameters_get(&policer_enabled, NULL,
                                             NULL, NULL,
                                             NULL, NULL);
    CU_ASSERT(!policer_enabled);

    /* error repair policer rate */
    cmdstat = vqec_cmd_error_repair_policer_rate(
        cli_handle, "error-repair policer rate", args11, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_error_repair_policer_rate(
        cli_handle, "error-repair policer rate", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_error_repair_policer_rate
        (cli_handle, "error-repair policer rate", args31, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    vqec_error_repair_policer_parameters_get(&policer_enabled, NULL,
                                             &rate, NULL,
                                             &burst, NULL);
    CU_ASSERT_EQUAL(rate, 1);

    /* error repair policer burst */
    cmdstat = vqec_cmd_error_repair_policer_burst(
        cli_handle, "error-repair policer burst", args11, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_error_repair_policer_burst(
        cli_handle, "error-repair policer burst", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_error_repair_policer_burst(
        cli_handle, "error-repair policer burst", args31, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    vqec_error_repair_policer_parameters_get(&policer_enabled, NULL,
                                             NULL, NULL,
                                             &burst, NULL);
    CU_ASSERT_EQUAL(burst, 1);

    /* drop default settings */
    memset(&state, 0, sizeof(state));
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(!state.en);  /* disabled by default */

    memset(&state, 0, sizeof(state));
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_REPAIR, &state);
    CU_ASSERT(!state.en);  /* disabled by default */

    /* drop session {primary | repair} {enable | disable} */
    cmdstat = vqec_cmd_drop_enable(cli_handle,
                                   "drop session primary enable",
                                   args11, 0);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(state.en);
    cmdstat = vqec_cmd_drop_disable(cli_handle,
                                    "drop session primary disable",
                                    args11, 0);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!state.en);

    cmdstat = vqec_cmd_drop_enable(cli_handle,
                                   "drop session repair enable",
                                   args11, 0);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_REPAIR, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(state.en);

    cmdstat = vqec_cmd_drop_disable(cli_handle,
                                    "drop session repair disable",
                                    args11, 0);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_REPAIR, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!state.en);

    /* drop { enable | disable */
    cmdstat = vqec_cmd_drop_enable(cli_handle, "drop enable", args11, 0);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(state.en);

    cmdstat = vqec_cmd_drop_enable(cli_handle, "drop enable", args11, 0);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(state.en);

    cmdstat = vqec_cmd_drop_disable(cli_handle, "drop disable", args12, 1);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(state.en);
    
    cmdstat = vqec_cmd_drop_disable(cli_handle, "drop disable", args11, 0);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!state.en);

    cmdstat = vqec_cmd_drop_enable(cli_handle, "drop enable", args12, 1);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(!state.en);

    /* drop-interval */
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(state.desc.num_cont_drops == 0);  /* default */
    CU_ASSERT(state.desc.interval_span == 0);  /* default */

    cmdstat = vqec_cmd_drop_interval(cli_handle, "drop interval", args11, 0);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(state.desc.num_cont_drops == 0);
    CU_ASSERT(state.desc.interval_span == 0);

    cmdstat = vqec_cmd_drop_interval(cli_handle, "drop interval", args12, 1);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(state.desc.num_cont_drops == 0);
    CU_ASSERT(state.desc.interval_span == 0);

    cmdstat = vqec_cmd_drop_interval(cli_handle, "drop interval", args31, 1);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(state.desc.num_cont_drops == 0);
    CU_ASSERT(state.desc.interval_span == 0);

    cmdstat = vqec_cmd_drop_interval(cli_handle, "drop interval", args32, 2);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(state.desc.num_cont_drops == 1);
    CU_ASSERT(state.desc.interval_span == 2);

    /* drop-percentage */
    CU_ASSERT(state.desc.drop_percent == 0);  /* default */

    cmdstat = vqec_cmd_drop_percentage(cli_handle, "drop percentage", args11, 0);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(state.desc.drop_percent == 0);

    cmdstat = vqec_cmd_drop_percentage(cli_handle, "drop percentage", args12, 1);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(state.desc.drop_percent == 0);

    cmdstat = vqec_cmd_drop_percentage(cli_handle, "drop percentage", args32, 2);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    CU_ASSERT(state.desc.drop_percent == 0);

    cmdstat = vqec_cmd_drop_percentage(cli_handle, "drop percentage", args31, 1);
    vqec_dp_get_drop_params(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY, &state);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(state.desc.drop_percent == 1);
}

static void test_vqec_cli_test_cmds (void) {
    int cmdstat = 0;

    char *args0[] = { };
    char *args11[] = { "?" };
    char *args12[] = { "invalidtuner" };
    char *args13[] = { "deftuner" };
    char *args14[] = { "newtuner" };
    char *args21[] = { "deftuner", "url?" };
    char *args22[] = { "deftuner", "udp://224.1.1.1:50000" };
    char *args23[] = { "deftuner", "udp://224.56.3.33:50000" };
    char *args24[] = { "deftuner", "udp://224.1.1.1:50000", "tr-135",  
                       "gmin", "2", "slmd", "3" };
    char *args31[] = { "udp://224.1.1.1:50000", "gmin", "1", "slmd", "5" }; 
    char *args32[] = { "udp://224.56.3.33:50000", "gmin", "1", "slmd", "5" }; 
    char primary_dest_addr[INET_ADDRSTRLEN];
    char *args_bind[] = {"deftuner", "chan-params", "list",    /* er & rcc */
                         "primary-dest-addr", primary_dest_addr,
                         "primary-dest-port", "60000",
                         "primary-dest-rtcp-port", "60011",
                         "primary-src-addr", "127.0.0.1",
                         "primary-src-port", "32993",
                         "primary-src-rtcp-port", "55001",
                         "primary-payload-type", "96",
                         "primary-bit-rate", "4000000",
                         "primary-rtcp-sndr-bw", "53",
                         "primary-rtcp-rcvr-bw", "530000",
                         "primary-rtcp-per-rcvr-bw", "53",
                         "fbt-addr", "127.0.0.1",
                         "er_enable", "rcc_enable",
                         "rtx-src-addr", "127.0.0.1",
                         "rtx-src-port", "55002",
                         "rtx-src-rtcp-port", "55003",
                         "rtx-dest-addr", "0.0.0.0",
                         "rtx-dest-port", "0",
                         "rtx-dest-rtcp-port", "0",
                         "rtx-payload-type", "99",
                         "rtx-rtcp-sndr-bw", "53",
                         "rtx-rtcp-rcvr-bw", "53"};
    char *args_bind2[] = {"deftuner", "chan-params", "list", /* w/o er & rcc */
                          "primary-dest-addr", primary_dest_addr,
                          "primary-dest-port", "60000",
                          "primary-dest-rtcp-port", "60011",
                          "primary-src-addr", "127.0.0.1",
                          "primary-src-port", "32993",
                          "primary-src-rtcp-port", "55001",
                          "primary-payload-type", "96",
                          "primary-bit-rate", "4000000",
                          "primary-rtcp-sndr-bw", "53",
                          "primary-rtcp-rcvr-bw", "530000",
                          "primary-rtcp-per-rcvr-bw", "53",
                          "fbt-addr", "127.0.0.1"};
    /* args_update1 - invalid (src port supplied w/o src address) */
    char *args_update1[] = {"primary-dest-addr", primary_dest_addr,
                            "primary-dest-port", "60000",
                            "primary-src-addr", "0.0.0.0",
                            "primary-src-port", "32993",
                            "fbt-addr", "127.0.0.1",
                            "primary-src-rtcp-port", "56001",
                            "rtx-src-addr", "127.0.0.1",
                            "rtx-src-port", "56002",
                            "rtx-src-rtcp-port", "56003"};
    /* args_update2 - invalid (FBT addr and rtx src addr don't match) */
    char *args_update2[] = {"primary-dest-addr", primary_dest_addr,
                            "primary-dest-port", "60000",
                            "primary-src-addr", "127.0.0.1",
                            "primary-src-port", "32993",
                            "fbt-addr", "127.0.0.1",
                            "primary-src-rtcp-port", "56001",
                            "rtx-src-addr", "0.0.0.0",
                            "rtx-src-port", "56002",
                            "rtx-src-rtcp-port", "56003"};
    /* args_update3 - valid */
    char *args_update3[] = {"primary-dest-addr", primary_dest_addr,
                            "primary-dest-port", "60000",
                            "primary-src-addr", "127.0.0.2",
                            "primary-src-port", "32994",
                            "fbt-addr", "127.0.0.3",
                            "primary-src-rtcp-port", "56005",
                            "rtx-src-addr", "127.0.0.3",
                            "rtx-src-port", "56004",
                            "rtx-src-rtcp-port", "56003"};
    /* args_update4 - valid */
    char *args_update4[] = {"primary-dest-addr", primary_dest_addr,
                            "primary-dest-port", "60000",
                            "primary-src-addr", "127.0.0.1",
                            "primary-src-port", "32993",
                            "fbt-addr", "127.0.0.1",
                            "primary-src-rtcp-port", "56001"};
    char *args_update_config1[] = {"file", 
                                   "data/override_tags.txt",
                                   "type",
                                   "override"};
    in_addr_t dst_ip, src_ip;
    uint16_t dst_port, src_port;
    vqec_chan_t *chan;


    /* tuner bind */
    cmdstat = vqec_cmd_tuner_bind(cli_handle, "tuner bind", args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_bind(cli_handle, "tuner bind", args11, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_bind(cli_handle, "tuner bind", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_bind(cli_handle, "tuner bind", args13, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_bind(cli_handle, "tuner bind", args21, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_bind(cli_handle, "tuner bind", args22, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_tuner_bind(cli_handle, "tuner bind", args23, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    /* tuner unbind */
    cmdstat = vqec_cmd_tuner_unbind(cli_handle, "tuner unbind", args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_unbind(cli_handle, "tuner unbind", args11, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_unbind(cli_handle, "tuner unbind", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_unbind(cli_handle, "tuner unbind", args13, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* tuner create */
    cmdstat = vqec_cmd_tuner_create(cli_handle, "tuner create", args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_create(cli_handle, "tuner create", args11, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_create(cli_handle, "tuner create", args13, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_create(cli_handle, "tuner create", args14, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* tuner bind and unbind for TR-135 */
    cmdstat = vqec_cmd_tuner_bind(cli_handle, "tuner bind", args24, 7);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_tuner_unbind(cli_handle, "tuner unbind", args13, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* tuner destroy */
    cmdstat = vqec_cmd_tuner_destroy(cli_handle, "tuner destroy", args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_destroy(cli_handle, "tuner destroy", args11, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_destroy(cli_handle, "tuner destroy", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_tuner_destroy(cli_handle, "tuner destroy", args14, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* channel tr-135 */
    cmdstat = vqec_cmd_tuner_bind(cli_handle, "tuner bind", args24, 7);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_channel_tr135(cli_handle, "channel tr-135", args31, 5);
    CU_ASSERT(cmdstat == VQEC_CLI_OK); 
    cmdstat = vqec_cmd_channel_tr135(cli_handle, "channel tr-135", args32, 5);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR); 
    cmdstat = vqec_cmd_tuner_unbind(cli_handle, "tuner unbind", args13, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* 
     * channel update (w/err & rcc)
     *
     * Test steps:
     *  1. bind a tuner to a unicast channel using chan params.
     *  2. verify some invalid source changes are rejected
     *  3. verify for a valid source change that the active channel's
     *     config reflects the new source information
     */    
    CU_ASSERT(get_ip_address_by_if((char *)vqec_syscfg_get_ptr()->input_ifname,
                                   primary_dest_addr,
                                   INET_ADDRSTRLEN));
    printf("Starting channel update tests with primary stream dest %s\n",
           primary_dest_addr);
    cmdstat = vqec_cmd_tuner_bind(cli_handle, "tuner bind", args_bind,
                                  sizeof(args_bind)/sizeof(char *));
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    /* supply invalid channel args */
    cmdstat = vqec_cmd_channel_update(cli_handle, "channel update",
                                      args_update1,
                                      sizeof(args_update1)/sizeof(char *));
    CU_ASSERT(cmdstat != VQEC_CLI_OK);
    /* supply invalid channel args */
    cmdstat = vqec_cmd_channel_update(cli_handle, "channel update",
                                      args_update2,
                                      sizeof(args_update2)/sizeof(char *));
    CU_ASSERT(cmdstat != VQEC_CLI_OK);
    /* supply valid channel args */
    cmdstat = vqec_cmd_channel_update(cli_handle, "channel update",
                                      args_update3,
                                      sizeof(args_update3)/sizeof(char *));
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    /* verify the active channel's config has changed */
    CU_ASSERT(inet_pton(AF_INET, primary_dest_addr, &dst_ip) > 0);
    dst_port = htons(60000);
    chan = vqec_chan_find(dst_ip, dst_port);
    CU_ASSERT(chan != NULL);
    if (chan) {
        /* primary stream */
        CU_ASSERT(inet_pton(AF_INET, "127.0.0.2", &src_ip));
        src_port = htons(32994);
        CU_ASSERT(chan->cfg.primary_source_addr.s_addr == src_ip);
        CU_ASSERT(chan->cfg.primary_source_port == src_port);
        /* repair stream */
        CU_ASSERT(inet_pton(AF_INET, "127.0.0.3", &src_ip));
        src_port = htons(56004);
        CU_ASSERT(chan->cfg.rtx_source_addr.s_addr == src_ip);
        CU_ASSERT(chan->cfg.rtx_source_port == src_port);
    }
    cmdstat = vqec_cmd_tuner_unbind(cli_handle, "tuner unbind", args13, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* 
     * channel update (w/o er & rcc)
     */
    cmdstat = vqec_cmd_tuner_bind(cli_handle, "tuner bind", args_bind2,
                                  sizeof(args_bind2)/sizeof(char *));
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    /* supply valid channel args */
    cmdstat = vqec_cmd_channel_update(cli_handle, "channel update",
                                      args_update4,
                                      sizeof(args_update4)/sizeof(char *));
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* verify the active channel's config has changed back */
    CU_ASSERT(inet_pton(AF_INET, primary_dest_addr, &dst_ip) > 0);
    dst_port = htons(60000);
    chan = vqec_chan_find(dst_ip, dst_port);
    CU_ASSERT(chan != NULL);
    if (chan) {
        /* primary stream */
        CU_ASSERT(inet_pton(AF_INET, "127.0.0.1", &src_ip));
        src_port = htons(32993);
        CU_ASSERT(chan->cfg.primary_source_addr.s_addr == src_ip);
        CU_ASSERT(chan->cfg.primary_source_port == src_port);
    }
    cmdstat = vqec_cmd_tuner_unbind(cli_handle, "tuner unbind", args13, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* update [file <filename> type {network|channel|override-tags}] */
    cmdstat = vqec_cmd_update(cli_handle, "update", args_update_config1, 3);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_update(cli_handle, "update", args_update_config1, 4);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
}

static void test_vqec_cli_clear_cmds (void) {
    int cmdstat = 0;

    char *args0[] = { };
    char *args11[] = { "?" };
    char *args12[] = { "deftu?" };
    char *args13[] = { "invalidtuner" };
    char *args14[] = { "deftuner" };

    /* clear counters */
    cmdstat = vqec_cmd_clear_counters(cli_handle, "clear counters", args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cmd_clear_counters(cli_handle, "clear counters", args11, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_clear_counters(cli_handle, "clear counters", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_clear_counters(cli_handle, "clear counters", args13, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cmd_clear_counters(cli_handle, "clear counters", args14, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
}

static void test_vqec_cli_debug_cmds (void) {
    int cmdstat = 0;

    char command[100];

    char *args0[] = { };
    char *args11[] = { "?" };
    char *args12[] = { "rc?" };
    char *args13[] = { "rcc" };

    memset(command, 0, sizeof(command));

    /* invalid command/args */
    strncpy(command, "debug <type>", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    strncpy(command, "debug ?", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args11, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    strncpy(command, "debug rcc enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    strncpy(command, "debug rcc enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args13, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);


    /* channel */
    printf("trying debug channel enable...\n");
    strncpy(command, "debug channel enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CHANNEL));

    strncpy(command, "debug channel disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CHANNEL));

    /* pcm */
    strncpy(command, "debug pcm enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_PCM));

    strncpy(command, "debug pcm disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_PCM));


    /* input */
    strncpy(command, "debug input enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_INPUT));

    strncpy(command, "debug input disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_INPUT));


    /* output */
    strncpy(command, "debug output enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_OUTPUT));

    strncpy(command, "debug output disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_OUTPUT));


    /* event */
    strncpy(command, "debug event enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_EVENT));

    strncpy(command, "debug event disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_EVENT));


    /* igmp */
    strncpy(command, "debug igmp enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_IGMP));

    strncpy(command, "debug igmp disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_IGMP));


    /* tuner */
    strncpy(command, "debug tuner enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TUNER));

    strncpy(command, "debug tuner disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TUNER));


    /* recv-socket */
    strncpy(command, "debug recv-socket enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RECV_SOCKET));

    strncpy(command, "debug recv-socket disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RECV_SOCKET));


    /* timer */
    strncpy(command, "debug timer enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TIMER));

    strncpy(command, "debug timer disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TIMER));


    /* rtcp */
    strncpy(command, "debug rtcp enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RTCP));

    strncpy(command, "debug rtcp disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RTCP));


    /* error-repair */
    strncpy(command, "debug error-repair enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_ERROR_REPAIR));

    strncpy(command, "debug error-repair disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_ERROR_REPAIR));


    /* rcc */
    strncpy(command, "debug rcc enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC));

    strncpy(command, "debug rcc disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC));


    /* nat */
    strncpy(command, "debug nat enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_NAT));

    strncpy(command, "debug nat disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_NAT));


    /* chan_cfg */
    strncpy(command, "debug chan_cfg enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CHAN_CFG));

    strncpy(command, "debug chan_cfg disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CHAN_CFG));


    /* updater */
    strncpy(command, "debug updater enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_UPDATER));

    strncpy(command, "debug updater disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_UPDATER));


    /* all */
    strncpy(command, "debug all enable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CHANNEL));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_PCM));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_INPUT));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_OUTPUT));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_EVENT));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_IGMP));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TUNER));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RECV_SOCKET));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TIMER));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RTCP));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_ERROR_REPAIR));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_NAT));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CHAN_CFG));
    CU_ASSERT(VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_UPDATER));

    strncpy(command, "debug all disable", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CHANNEL));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_PCM));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_INPUT));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_OUTPUT));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_EVENT));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_IGMP));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TUNER));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RECV_SOCKET));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_TIMER));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RTCP));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_ERROR_REPAIR));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_RCC));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_NAT));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_CHAN_CFG));
    CU_ASSERT(!VQEC_GET_DEBUG_FLAG(VQEC_DEBUG_UPDATER));

    /* ? */
    strncpy(command, "debug ?", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    strncpy(command, "debug rcc ?", 50);
    cmdstat = vqec_debug_func(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
}

static void test_vqec_cli_hag_cmds (void) {
    int cmdstat = 0;

    char *args0[] = { };
    char *args11[] = { "?" };
    char *args12[] = { "deftu?" };
    char *args21[] = { "deftuner", "eth0" };
    char *args31[] = { "deftuner", "eth0", "192.168.1.128" };

    char *args32[] = { "deftuner", "eth0", "udp://192.168.1.128:50000" };
    char *args33[] = { "deftuner", "eth0", "udp://192.368.1.128:50000" };

    char *args41[] = { "deftuner", "eth0", "192.168.1.128", "0.0.240.0" };
#ifdef HAVE_HAG_MODE
    char *args42[] = { "deftuner", "eth0", "192.168.1.256", "0.0.240.0" };
    char *args43[] = { "deftuner", "eth0", "192.168.1.128", "0.0.288.0" };
    char *args44[] = { "deftuner", "eth0", "192.168.1.128", "0.4.240.0" };

    char *args51[] = { "deftuner", "eth0", "192.168.1.128", "0.0.240.0", 
                       "extraarg"};
#endif /* HAVE_HAG_MODE */

#ifdef HAVE_HAG_MODE
    /* proxy-igmp-join */
    cmdstat = vqec_cmd_proxy_igmp_join(cli_handle, "proxy-igmp-join", args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_proxy_igmp_join(cli_handle, "proxy-igmp-join", 
                                       args11, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_proxy_igmp_join(cli_handle, "proxy-igmp-join", 
                                       args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    /* Need to run this in root mode, so failed in socket create */
    cmdstat = vqec_cmd_proxy_igmp_join(cli_handle, "proxy-igmp-join", 
                                       args31, 3);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_proxy_igmp_join(cli_handle, "proxy-igmp-join", 
                                       args41, 4);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_proxy_igmp_join(cli_handle, "proxy-igmp-join", 
                                       args42, 4);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_proxy_igmp_join(cli_handle, "proxy-igmp-join", 
                                       args43, 4);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_proxy_igmp_join(cli_handle, "proxy-igmp-join", 
                                       args44, 4);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_proxy_igmp_join(cli_handle, "proxy-igmp-join", 
                                       args51, 5);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
#endif /* HAVE_HAG_MODE */


    /* stream-output */
    cmdstat = vqec_cmd_stream_output(cli_handle, "stream-output", args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_stream_output(cli_handle, "stream-output", args11, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_stream_output(cli_handle, "stream-output", args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_stream_output(cli_handle, "stream-output", args21, 2);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_stream_output(cli_handle, "stream-output", args31, 3);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_stream_output(cli_handle, "stream-output", args32, 3);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    cmdstat = vqec_cmd_stream_output(cli_handle, "stream-output", args33, 3);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_stream_output(cli_handle, "stream-output", args41, 4);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
}

static void test_vqec_cli_monitor_cmds (void) {
    int cmdstat = 0;

    char command[100];

    char *args0[] = { };
    char *args11[] = { "?" };
    char *args12[] = { "file?" };
    char *args13[] = { "utest_elog_dump_file.tmp" };

    memset(command, 0, sizeof(command));

    /* invalid command/args */
    strncpy(command, "monitor elog <3rdparam>", 50);
    cmdstat = vqec_cmd_monitor_elog(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    strncpy(command, "monitor elog dump", 50);
    cmdstat = vqec_cmd_monitor_elog(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    strncpy(command, "monitor elog on", 50);
    cmdstat = vqec_cmd_monitor_elog(cli_handle, command, args11, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    /* monitor elog on */
    strncpy(command, "monitor elog on", 50);
    cmdstat = vqec_cmd_monitor_elog(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* monitor elog off */
    strncpy(command, "monitor elog off", 50);
    cmdstat = vqec_cmd_monitor_elog(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* monitor elog reset */
    strncpy(command, "monitor elog reset", 50);
    cmdstat = vqec_cmd_monitor_elog(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* monitor elog dump */
    strncpy(command, "monitor elog dump", 50);
    cmdstat = vqec_cmd_monitor_elog(cli_handle, command, args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    strncpy(command, "monitor elog dump", 50);
    cmdstat = vqec_cmd_monitor_elog(cli_handle, command, args11, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    strncpy(command, "monitor elog dump", 50);
    cmdstat = vqec_cmd_monitor_elog(cli_handle, command, args12, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    strncpy(command, "monitor elog dump", 50);
    cmdstat = vqec_cmd_monitor_elog(cli_handle, command, args13, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    /* print elog status report */
    status_report();

    /* Output Scheduling commands */
    cmdstat = vqec_cmd_stream_output(cli_handle, "monitor output-sched",
                                     args11, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cmd_monitor_outputsched_on(cli_handle,
                                              "monitor output-sched on",
                                              args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    cmdstat = vqec_cmd_monitor_outputsched_off(cli_handle,
                                               "monitor output-sched off",
                                               args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    cmdstat = vqec_cmd_monitor_outputsched_reset(cli_handle,
                                                 "monitor output-sched reset",
                                                 args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);

    cmdstat = vqec_cmd_monitor_outputsched_show(cli_handle,
                                                "monitor output-sched show",
                                                args0, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
}

/* Test exit, quit, disable, ?, special commands */
static void test_vqec_cli_special_cmds (void) 
{
    int cmdstat = 0;

    char *args1[] = { "?" };
    char *args2[] = { "enable" };
    char *args3[] = { "disable" };
    char *args4[] = { "logout" };
    char *args5[] = { "exit" };
    char *args6[] = { "quit" }; 
    char *args7[] = { "history" }; 

    cmdstat = vqec_cli_help_func(cli_handle, "?", args1, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    /* Enter the privileged mode */
    cmdstat = vqec_cli_help_func(cli_handle, "enable", args2, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cli_help_func(cli_handle, "disable", args3, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cli_help_func(cli_handle, "logout", args4, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cli_help_func(cli_handle, "exit", args5, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);
    cmdstat = vqec_cli_help_func(cli_handle, "quit", args6, 1);
    CU_ASSERT(cmdstat == VQEC_CLI_ERROR);

    cmdstat = vqec_cli_help_func(cli_handle, "?", args1, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    /* Enter the privileged mode */
    cmdstat = vqec_cli_help_func(cli_handle, "enable", args2, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cli_help_func(cli_handle, "disable", args3, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cli_help_func(cli_handle, "logout", args4, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cli_help_func(cli_handle, "exit", args5, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cli_help_func(cli_handle, "quit", args6, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
    cmdstat = vqec_cli_help_func(cli_handle, "history", args7, 0);
    CU_ASSERT(cmdstat == VQEC_CLI_OK);
}

void fixerrs(void){
    test_vqec_cli_monitor_cmds();
}

CU_TestInfo test_array_cli[] = {
    {"test vqec_cli_helper_funcs",test_vqec_cli_helper_funcs},
    {"test vqec_cli_show_cmds",test_vqec_cli_show_cmds},
    {"test vqec_cli_global_cmds",test_vqec_cli_global_cmds},
    {"test vqec_cli_test_cmds",test_vqec_cli_test_cmds},
    {"test vqec_cli_clear_cmds",test_vqec_cli_clear_cmds},
    {"test vqec_cli_debug_cmds",test_vqec_cli_debug_cmds},
    {"test vqec_cli_hag_cmds",test_vqec_cli_hag_cmds},
    {"test vqec_cli_monitor_cmds",test_vqec_cli_monitor_cmds},
    {"test vqec_cli_special_cmds",test_vqec_cli_special_cmds},
    CU_TEST_INFO_NULL,
};
