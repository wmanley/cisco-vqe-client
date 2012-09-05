/*
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vqec_config_parser.h"

int32_t 
test_vqec_configparser_init (void) 
{
    return (0);
}

int32_t
test_vqec_configparser_clean (void)
{
    return (0);
}

vqec_config_t cfg1, cfg2, cfg3, cfg4, cfg5, cfg6;

static void test_vqec_config_init (void)
{
    boolean status;

    status = vqec_config_init(&cfg1);
    if (!status) {
        printf("init test 1 failed!\n");
        return;
    }

    status = vqec_config_init(&cfg2);
    if (!status) {
        printf("init test 2 failed!\n");
        return;
    }

    status = vqec_config_init(&cfg3);
    if (!status) {
        printf("init test 3 failed!\n");
        return;
    }

    status = vqec_config_init(&cfg4);
    if (!status) {
        printf("init test 4 failed!\n");
        return;
    }

    status = vqec_config_init(&cfg5);
    if (!status) {
        printf("init test 5 failed!\n");
        return;
    }

    status = vqec_config_init(&cfg6);
    if (!status) {
        printf("init test 6 failed!\n");
        return;
    }
}

static void test_vqec_config_read_file (void)
{
    char *filepath;
    boolean status;

    filepath = "data/cfg_test_massive.cfg";
    status = vqec_config_read_file(&cfg1, filepath);
    printf("filepath:  \"%s\"\n", filepath);
    CU_ASSERT(status);
    printf("parsing status:  %d\n  error_line = %d\n  error_text = \"%s\"\n",
           (int)status, cfg1.error_line, cfg1.error_text);

    filepath = "data/cfg_test_multituner.cfg";
    status = vqec_config_read_file(&cfg2, filepath);
    printf("filepath:  \"%s\"\n", filepath);
    CU_ASSERT(status);
    printf("parsing status:  %d\n  error_line = %d\n  error_text = \"%s\"\n",
           (int)status, cfg2.error_line, cfg2.error_text);

    filepath = "data/cfg_test_list_error.cfg";
    status = vqec_config_read_file(&cfg3, filepath);
    printf("filepath:  \"%s\"\n", filepath);
    CU_ASSERT(status == FALSE);
    CU_ASSERT(cfg3.error_line == 48);
    printf("parsing status:  %d\n  error_line = %d\n  error_text = \"%s\"\n",
           (int)status, cfg3.error_line, cfg3.error_text);

    filepath = "data/cfg_test_no_assign.cfg";
    status = vqec_config_read_file(&cfg4, filepath);
    printf("filepath:  \"%s\"\n", filepath);
    CU_ASSERT(status == FALSE);
    CU_ASSERT(cfg4.error_line == 11);
    printf("parsing status:  %d\n  error_line = %d\n  error_text = \"%s\"\n",
           (int)status, cfg4.error_line, cfg4.error_text);

    filepath = "data/cfg_test_invalid_param.cfg";
    status = vqec_config_read_file(&cfg5, filepath);
    printf("filepath:  \"%s\"\n", filepath);
    CU_ASSERT(status == FALSE);
    CU_ASSERT(cfg5.error_line == 7);
    printf("parsing status:  %d\n  error_line = %d\n  error_text = \"%s\"\n",
           (int)status, cfg5.error_line, cfg5.error_text);

    filepath = "data/cfg_test_duplicate.cfg";
    status = vqec_config_read_file(&cfg6, filepath);
    printf("filepath:  \"%s\"\n", filepath);
    CU_ASSERT(status == FALSE);
    CU_ASSERT(cfg6.error_line == 20);
    printf("parsing status:  %d\n  error_line = %d\n  error_text = \"%s\"\n",
           (int)status, cfg6.error_line, cfg6.error_text);

}

static void test_vqec_config_lookup (void)
{
    vqec_config_setting_t *tmp;

    tmp = vqec_config_lookup(&cfg1, "error_repair_enable");
    CU_ASSERT((int)tmp);
    if (tmp) {
        printf("found setting with name = \"%s\"\n", tmp->name);
    } else {
        printf("lookup returned NULL\n");
    }

    tmp = vqec_config_lookup(&cfg1, "error_repair_enabled");
    CU_ASSERT(!tmp);
    if (tmp) {
        printf("found setting with name = \"%s\"\n", tmp->name);
    } else {
        printf("lookup returned NULL\n");
    }

    tmp = vqec_config_lookup(&cfg1, "tuner_list");
    CU_ASSERT((int)tmp);
    if (tmp) {
        printf("found setting with name = \"%s\"\n", tmp->name);
    } else {
        printf("lookup returned NULL\n");
    }



    tmp = vqec_config_lookup(&cfg1, "error_repair_policer.rate");
    CU_ASSERT((int)tmp);
    if (tmp) {
        printf("found setting with name = \"%s\"\n", tmp->name);
    } else {
        printf("lookup returned NULL\n");
    }

    tmp = vqec_config_lookup(&cfg1, "group1.group2.parm22");
    CU_ASSERT((int)tmp);
    if (tmp) {
        printf("found setting with name = \"%s\"\n", tmp->name);
    } else {
        printf("lookup returned NULL\n");
    }
}

static void test_vqec_config_setting_type (void)
{
    vqec_config_setting_t *tmp;

    tmp = vqec_config_lookup(&cfg1, "error_repair_enable");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) == VQEC_CONFIG_SETTING_TYPE_BOOLEAN);
    printf((tmp && vqec_config_setting_type(tmp) == VQEC_CONFIG_SETTING_TYPE_BOOLEAN) ?
           "PASS\n" : "FAILED\n");
    
    tmp = vqec_config_lookup(&cfg1, "max_tuners");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) == VQEC_CONFIG_SETTING_TYPE_INT);
    printf((tmp && vqec_config_setting_type(tmp) == VQEC_CONFIG_SETTING_TYPE_INT) ?
           "PASS\n" : "FAILED\n");
    
    tmp = vqec_config_lookup(&cfg1, "sig_mode");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) == VQEC_CONFIG_SETTING_TYPE_STRING);
    printf((tmp && vqec_config_setting_type(tmp) == VQEC_CONFIG_SETTING_TYPE_STRING) ?
           "PASS\n" : "FAILED\n");
    
    tmp = vqec_config_lookup(&cfg1, "invalid_param");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) == VQEC_CONFIG_SETTING_TYPE_INVALID);
    printf((tmp && vqec_config_setting_type(tmp) == VQEC_CONFIG_SETTING_TYPE_INVALID) ?
           "PASS\n" : "FAILED\n");
    
}

static void test_vqec_config_setting_length (void)
{
    vqec_config_setting_t *tmp;

    tmp = vqec_config_lookup(&cfg1, "error_repair_policer");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_length(tmp) == 3);
    printf((tmp && vqec_config_setting_length(tmp) == 3) ?
           "PASS\n" : "FAILED\n");
    tmp = vqec_config_lookup(&cfg1, "group1");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_length(tmp) == 4);
    printf((tmp && vqec_config_setting_length(tmp) == 4) ?
           "PASS\n" : "FAILED\n");
    tmp = vqec_config_lookup(&cfg1, "tuner_list");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_length(tmp) == 1);
    printf((tmp && vqec_config_setting_length(tmp) == 1) ?
           "PASS\n" : "FAILED\n");
    tmp = vqec_config_lookup(&cfg1, "sig_mode");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_length(tmp) == 0);
    printf((tmp && vqec_config_setting_length(tmp) == 0) ?
           "PASS\n" : "FAILED\n");
}

static void test_vqec_config_setting_value (void)
{
    vqec_config_setting_t *tmp;

    tmp = vqec_config_lookup(&cfg1, "error_repair_enable");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_get_bool(tmp) == TRUE);
    printf((tmp && vqec_config_setting_get_bool(tmp) == TRUE) ?
           "PASS\n" : "FAILED\n");

    tmp = vqec_config_lookup(&cfg1, "max_tuners");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_get_int(tmp) == 10);
    printf((tmp && vqec_config_setting_get_int(tmp) == 10) ?
           "PASS\n" : "FAILED\n");

    tmp = vqec_config_lookup(&cfg1, "pakpool_size");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_get_int(tmp) == 1000);
    printf((tmp && vqec_config_setting_get_int(tmp) == 1000) ?
           "PASS\n" : "FAILED\n");

    tmp = vqec_config_lookup(&cfg1, "sig_mode");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && !strncmp(vqec_config_setting_get_string(tmp), "nat", 5));
    printf((tmp && !strncmp(vqec_config_setting_get_string(tmp), "nat", 5)) ?
           "PASS\n" : "FAILED\n");

    tmp = vqec_config_lookup(&cfg1, "invalid_param");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_get_int(tmp) == 0);
    printf((tmp && vqec_config_setting_get_int(tmp) == 0) ?
           "PASS\n" : "FAILED\n");

}

static void test_vqec_config_setting_group (void)
{
    vqec_config_setting_t *tmp, *erp, *grp1, *grp2;

    printf("erp...\n");
    erp = vqec_config_lookup(&cfg1, "error_repair_policer");
    CU_ASSERT((int)erp);
    CU_ASSERT((int)erp && vqec_config_setting_type(erp) ==
              VQEC_CONFIG_SETTING_TYPE_GROUP);
    printf((erp && vqec_config_setting_type(erp) ==
            VQEC_CONFIG_SETTING_TYPE_GROUP) ?
           "PASS\n" : "FAILED\n");
    tmp = vqec_config_setting_get_member(erp, "burst");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_INT);
    printf((tmp && vqec_config_setting_type(tmp) ==
            VQEC_CONFIG_SETTING_TYPE_INT) ?
           "PASS\n" : "FAILED\n");
    tmp = vqec_config_setting_get_member(erp, "enable");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_BOOLEAN);
    printf((tmp && vqec_config_setting_type(tmp) ==
            VQEC_CONFIG_SETTING_TYPE_BOOLEAN) ?
           "PASS\n" : "FAILED\n");

    printf("grp1...\n");
    grp1 = vqec_config_lookup(&cfg1, "group1");
    CU_ASSERT((int)grp1);
    CU_ASSERT((int)grp1 && vqec_config_setting_type(grp1) ==
              VQEC_CONFIG_SETTING_TYPE_GROUP);
    printf((grp1 && vqec_config_setting_type(grp1) ==
            VQEC_CONFIG_SETTING_TYPE_GROUP) ?
           "PASS\n" : "FAILED\n");
    tmp = vqec_config_setting_get_member(grp1, "parm11");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_BOOLEAN);
    CU_ASSERT((int)tmp && vqec_config_setting_get_bool(tmp) == TRUE);
    printf((tmp && vqec_config_setting_type(tmp) ==
            VQEC_CONFIG_SETTING_TYPE_BOOLEAN) ?
           "PASS\n" : "FAILED\n");
    printf((tmp && vqec_config_setting_get_bool(tmp) == TRUE) ?
           "PASS\n" : "FAILED\n");
    tmp = vqec_config_setting_get_member(grp1, "parm12");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_INT);
    CU_ASSERT((int)tmp && vqec_config_setting_get_int(tmp) == 5);
    printf((tmp && vqec_config_setting_type(tmp) ==
            VQEC_CONFIG_SETTING_TYPE_INT) ?
           "PASS\n" : "FAILED\n");
    printf((tmp && vqec_config_setting_get_int(tmp) ==
            5) ?
           "PASS\n" : "FAILED\n");

    printf("grp2...\n");
    grp2 = vqec_config_setting_get_member(grp1, "group2");
    CU_ASSERT((int)grp2);
    CU_ASSERT((int)grp2 && vqec_config_setting_type(grp2) ==
              VQEC_CONFIG_SETTING_TYPE_GROUP);
    printf((grp2 && vqec_config_setting_type(grp2) ==
            VQEC_CONFIG_SETTING_TYPE_GROUP) ?
           "PASS\n" : "FAILED\n");
    tmp = vqec_config_setting_get_member(grp2, "parm21");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_STRING);
    CU_ASSERT((int)tmp && !strncmp(vqec_config_setting_get_string(tmp),
                              "test_string", 20));
    printf((tmp && vqec_config_setting_type(tmp) ==
            VQEC_CONFIG_SETTING_TYPE_STRING) ?
           "PASS\n" : "FAILED\n");
    printf((tmp &&
            !strncmp(vqec_config_setting_get_string(tmp), "test_string", 20)) ?
           "PASS\n" : "FAILED\n");
    tmp = vqec_config_setting_get_member(grp2, "parm22");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_BOOLEAN);
    CU_ASSERT((int)tmp && vqec_config_setting_get_bool(tmp) == FALSE);
    printf((tmp && vqec_config_setting_type(tmp) ==
            VQEC_CONFIG_SETTING_TYPE_BOOLEAN) ?
           "PASS\n" : "FAILED\n");
    printf((tmp && vqec_config_setting_get_bool(tmp) == FALSE) ?
           "PASS\n" : "FAILED\n");
}

static void test_vqec_config_setting_list (void)
{
    vqec_config_setting_t *tmp, *lst1, *lst2, *lst21,
        *lst22, *lst23, *lst231, *lst232;

    printf("lst1...\n");
    lst1 = vqec_config_lookup(&cfg1, "list1");
    CU_ASSERT((int)lst1);
    CU_ASSERT((int)lst1 && vqec_config_setting_type(lst1) ==
              VQEC_CONFIG_SETTING_TYPE_LIST);
    tmp = vqec_config_setting_get_elem(lst1, 0);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_INT);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_get_int(tmp) == 2);
    tmp = vqec_config_setting_get_elem(lst1, 3);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_INT);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_get_int(tmp) == 11);
    tmp = vqec_config_setting_get_elem(lst1, 4);
    CU_ASSERT(!tmp);


    printf("lst2...\n");
    lst2 = vqec_config_lookup(&cfg1, "list2");
    CU_ASSERT((int)lst2);
    CU_ASSERT((int)lst2 && vqec_config_setting_type(lst2) ==
              VQEC_CONFIG_SETTING_TYPE_LIST);
    lst21 = vqec_config_setting_get_elem(lst2, 0);
    CU_ASSERT((int)lst21);
    CU_ASSERT((int)lst21 && vqec_config_setting_type(lst21) ==
              VQEC_CONFIG_SETTING_TYPE_LIST);
    tmp = vqec_config_setting_get_elem(lst21, 0);
    CU_ASSERT((int)lst21);
    CU_ASSERT((int)lst21 && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_INT);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_get_int(tmp) == 3);

    lst22 = vqec_config_setting_get_elem(lst2, 1);
    CU_ASSERT((int)lst22);
    CU_ASSERT((int)lst22 && vqec_config_setting_type(lst22) ==
              VQEC_CONFIG_SETTING_TYPE_LIST);
    tmp = vqec_config_setting_get_elem(lst22, 0);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_BOOLEAN);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_get_bool(tmp) == FALSE);

    lst23 = vqec_config_setting_get_elem(lst2, 2);
    CU_ASSERT((int)lst23);
    CU_ASSERT((int)lst23 && vqec_config_setting_type(lst23) ==
              VQEC_CONFIG_SETTING_TYPE_LIST);
    lst231 = vqec_config_setting_get_elem(lst23, 0);
    CU_ASSERT((int)lst231);
    CU_ASSERT((int)lst231 && vqec_config_setting_type(lst231) ==
              VQEC_CONFIG_SETTING_TYPE_LIST);
    tmp = vqec_config_setting_get_elem(lst231, 0);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_INT);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_get_int(tmp) == 10);
    lst232 = vqec_config_setting_get_elem(lst23, 1);
    CU_ASSERT((int)lst232);
    CU_ASSERT((int)lst232 && vqec_config_setting_type(lst232) ==
              VQEC_CONFIG_SETTING_TYPE_LIST);
    tmp = vqec_config_setting_get_elem(lst232, 1);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_INT);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_get_int(tmp) == 15);


}

static void test_vqec_config_setting_string (void)
{
    vqec_config_setting_t *tmp;

    tmp = vqec_config_lookup(&cfg1, "test_str_concat");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && !strcmp(vqec_config_setting_get_string(tmp),
                             "this stringshould be concatenatedwhen"
                             " it is interpreted"));
    if (tmp) {
        printf("test_str_concat =\n%s\n", vqec_config_setting_get_string(tmp));
    }

    tmp = vqec_config_lookup(&cfg1, "test_str_escapes");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && !strcmp(vqec_config_setting_get_string(tmp),
                             "this\nstring\ruses\\the\"escape\t"
                             "character\fsequences"));
    if (tmp) {
        printf("test_str_escapes =\n%s\n", vqec_config_setting_get_string(tmp));
    }
}

void test_vqec_config_read_buffer (void)
{
    char buffer[1000] = "parm1 = \"val1\";\nparm2 = (2,3,4);parm3=true;";
    boolean status;
    vqec_config_setting_t *tmp, *tmp2;

    status = vqec_config_read_buffer(&cfg2, buffer);
    if (status) {
        CU_ASSERT(TRUE);
    } else {
        printf("parsing status:  FAILURE\n  error_line = %d\n  error_text = \"%s\"\n",
               cfg2.error_line, cfg2.error_text);
        CU_ASSERT(FALSE);
    }

    
    printf("buffer lookup test...\n");
    tmp = vqec_config_lookup(&cfg2, "parm1");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_STRING); 

    tmp = vqec_config_lookup(&cfg2, "parm2");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_LIST);

    tmp2 = vqec_config_setting_get_elem(tmp, 0);
    CU_ASSERT((int)tmp2);
    CU_ASSERT((int)tmp2 && vqec_config_setting_type(tmp2) ==
              VQEC_CONFIG_SETTING_TYPE_INT);
    CU_ASSERT((int)tmp2);
    CU_ASSERT((int)tmp2 && vqec_config_setting_get_int(tmp2) == 2);

    tmp2 = vqec_config_setting_get_elem(tmp, 1);
    CU_ASSERT((int)tmp2);
    CU_ASSERT((int)tmp2 && vqec_config_setting_type(tmp2) ==
              VQEC_CONFIG_SETTING_TYPE_INT);
    CU_ASSERT((int)tmp2);
    CU_ASSERT((int)tmp2 && vqec_config_setting_get_int(tmp2) == 3);
    tmp2 = vqec_config_setting_get_elem(tmp, 2);
    CU_ASSERT((int)tmp2);
    CU_ASSERT((int)tmp2 && vqec_config_setting_type(tmp2) ==
              VQEC_CONFIG_SETTING_TYPE_INT);
    CU_ASSERT((int)tmp2);
    CU_ASSERT((int)tmp2 && vqec_config_setting_get_int(tmp2) == 4);

    tmp2 = vqec_config_setting_get_elem(tmp, 3);
    CU_ASSERT(!tmp2);

    tmp = vqec_config_lookup(&cfg2, "parm3");
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_type(tmp) ==
              VQEC_CONFIG_SETTING_TYPE_BOOLEAN);
    CU_ASSERT((int)tmp);
    CU_ASSERT((int)tmp && vqec_config_setting_get_bool(tmp) == TRUE);

}

void test_vqec_config_destroy (void)
{
    boolean status;

    status = vqec_config_destroy(&cfg1);
    if (!status) {
        printf("destroy test 1 failed!\n");
    }


    status = vqec_config_destroy(&cfg2);
    if (!status) {
        printf("destroy test 2 failed!\n");
    }


    status = vqec_config_destroy(&cfg3);
    if (!status) {
        printf("destroy test 3 failed!\n");
    }


    status = vqec_config_destroy(&cfg4);
    if (!status) {
        printf("destroy test 4 failed!\n");
    }


    status = vqec_config_destroy(&cfg5);
    if (!status) {
        printf("destroy test 5 failed!\n");
    }


    status = vqec_config_destroy(&cfg6);
    if (!status) {
        printf("destroy test 6 failed!\n");
    }

}

CU_TestInfo test_array_configparser[] = {
    {"test vqec_config_init",test_vqec_config_init},
    {"test vqec_config_read_file",test_vqec_config_read_file},
    {"test vqec_config_lookup",test_vqec_config_lookup},
    {"test vqec_config_setting_type",test_vqec_config_setting_type},
    {"test vqec_config_setting_length",test_vqec_config_setting_length},
    {"test vqec_config_setting_value",test_vqec_config_setting_value},
    {"test vqec_config_setting_group",test_vqec_config_setting_group},
    {"test vqec_config_setting_list",test_vqec_config_setting_list},
    {"test vqec_config_setting_string",test_vqec_config_setting_string},
    {"test vqec_config_read_buffer",test_vqec_config_read_buffer},
    {"test vqec_config_destroy",test_vqec_config_destroy},
    CU_TEST_INFO_NULL,
};

