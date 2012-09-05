/*
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
#include "vqe_port_macros.h"
#include "vqec_cli.h"

extern void vqec_set_cli_def(struct vqec_cli_def *cli);

/*
 * Unit tests for vqec_port
 */

/* global variables for test_vqec_port */
struct vqec_cli_def *testcli;

/*
struct vqec_cli_def *vqec_get_cli_def(void);
*/
extern boolean vqec_remove_trailing_newline(char *format);

int test_vqec_port_init (void) {
    test_malloc_make_fail(0);  //make sure we're using the real malloc
    testcli = malloc(10);
    return 0;
}

int test_vqec_port_clean (void) {
    return 0;
}

static void test_vqec_port_cli_def (void) {
    vqec_set_cli_def(testcli);
    CU_ASSERT(vqec_get_cli_def() == testcli);
    vqec_set_cli_def(NULL);
}

static void test_vqec_remove_trailing_newline (void) {
    char *format1 = NULL;
    char *format2 = "";
    char format3[30];
    char format4[30];

    sprintf(format3, "%s", "this is a test\n");
    sprintf(format4, "%s", "this is another test");

    CU_ASSERT(vqec_remove_trailing_newline(format1) == FALSE);
    CU_ASSERT(vqec_remove_trailing_newline(format2) == FALSE);
    CU_ASSERT(vqec_remove_trailing_newline(format3) == TRUE);
    CU_ASSERT(!strncmp(format3,"this is a test", 30));
    CU_ASSERT(vqec_remove_trailing_newline(format4) == FALSE);
}

CU_TestInfo test_array_port[] = {
    {"test vqec_port_cli_def",test_vqec_port_cli_def},
    {"test vqec_port_remove_trailing_newline",test_vqec_remove_trailing_newline},
    CU_TEST_INFO_NULL,
};

