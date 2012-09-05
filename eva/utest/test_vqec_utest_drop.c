/*
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include "vqec_drop.h"
#include "vqec_gap_reporter.h"

int32_t 
test_vqec_drop_init (void) 
{
    return (0);
}

int32_t
test_vqec_drop_clean (void)
{
    return (0);
}

static void 
test_vqec_drop_dump (void) 
{
    vqec_drop_dump();
}

static void 
test_vqec_error_repair (void) 
{
    vqec_set_error_repair(FALSE);
    CU_ASSERT(!vqec_get_error_repair(NULL));
    vqec_set_error_repair(TRUE);
    CU_ASSERT(vqec_get_error_repair(NULL));
}

static void 
test_vqec_fast_fill (void)
{
    CU_ASSERT(vqec_get_fast_fill(NULL));
    vqec_set_fast_fill(FALSE);
    CU_ASSERT(!vqec_get_fast_fill(NULL));
    vqec_set_fast_fill(TRUE);
    CU_ASSERT(vqec_get_fast_fill(NULL));
}

CU_TestInfo test_array_drop[] = {
    {"test vqec_drop_dump",test_vqec_drop_dump},
    {"test vqec_{get,set}_error_repair",test_vqec_error_repair},
    {"test vqec_{get,set}_fast_fill",test_vqec_fast_fill},
    CU_TEST_INFO_NULL,
};

