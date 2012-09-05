/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 *
 ******************************************************************************
 *
 * File: test_vqec_utest_gaptree.c
 *
 * Description: Unit tests for the vqec_gaptree module.
 *
 * Documents:
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "vqec_gaptree.h"
#include "test_vqec_utest_main.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"


#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

/*
 *  Unit tests for vqec_gaptree
 */

vqec_gap_tree_t vqec_gaptree;
vqec_gap_pool_t *test_gap_pool;

#define VQEC_GAPTREE_NUM_GAPS 1000
#define VQEC_GAPTREE_SEQ_NUMS 65536

int gap_array[VQEC_GAPTREE_SEQ_NUMS];
int res_gap_array[VQEC_GAPTREE_SEQ_NUMS];

/* statistics counters */
uint64_t remove_fail, remove_succ;
uint64_t insert_fail, insert_succ, if_args, if_alloc, if_olb, if_ola, if_d;

/* init */
int test_vqec_gaptree_init (void)
{
    remove_fail = 0;
    remove_succ = 0;
    insert_fail = 0;
    insert_succ = 0;
    memset(gap_array, 0, sizeof(gap_array));

    test_gap_pool = zone_instance_get_loc("TestGapPool",
                                          ZONE_FLAGS_STATIC,
                                          sizeof(vqec_gap_t),
                                          VQEC_GAPTREE_NUM_GAPS * 2,
                                          zone_ctor_no_zero, NULL);

    return 0;
}

void test_vqec_gaptree_init_func (void)
{
    CU_ASSERT(VQEC_GAPTREE_OK == vqec_gaptree_init(&vqec_gaptree));
}

/* verify gaps with reference list */
void test_vqec_gaptree_verify_gaps (void)
{
    vqec_gap_t *gap;
    int j;
    uint64_t ver_fail = 0;
    uint64_t ver_pass = 0;

    memset(res_gap_array, 0, sizeof(res_gap_array));

    printf("verifying %d seq_nums in gap space...\n", VQEC_GAPTREE_SEQ_NUMS);
    VQE_RB_FOREACH(gap, vqec_gap_tree, &vqec_gaptree) {
        for (j = gap->start_seq; j <= gap->start_seq + gap->extent; j++) {
            res_gap_array[j] = 1;
        }
    }
    for (j = 0; j < VQEC_GAPTREE_SEQ_NUMS; j++) {
        if (res_gap_array[j] == gap_array[j]) {
            ver_pass++;
        } else {
            printf(" %d is %d in gaptree, when it should be %d\n",
                   j, res_gap_array[j], gap_array[j]);
            ver_fail++;
        }
    }
    printf("result:\n correct:\t\t%llu\n incorrect: \t\t%llu\n", ver_pass, ver_fail);

    CU_ASSERT(ver_pass == VQEC_GAPTREE_SEQ_NUMS);
    CU_ASSERT(ver_fail == 0);
}

/* traverse and print gap tree */
void test_vqec_gaptree_print_tree (void)
{
    vqec_gaptree_print(&vqec_gaptree, printf);
}

void test_vqec_gaptree_rollover (void)
{
    vqec_gap_t gap, min_gap;
    vqec_gaptree_error_t err;
    vqec_seq_num_t maxn = (-1);

    /* add a gap that wraps around */
    gap.start_seq = maxn - 5; /* 2^32 - 5 */
    gap.extent = 9;
    err = vqec_gaptree_add_gap(&vqec_gaptree, gap, test_gap_pool);
    CU_ASSERT(err == VQEC_GAPTREE_OK);
    vqec_gaptree_print(&vqec_gaptree, printf);

    /* we should have (4294967290,9) ==> (4294967290,5), (0,3) */
    vqec_gaptree_extract_min(&vqec_gaptree, &min_gap, test_gap_pool);
    CU_ASSERT(min_gap.start_seq == maxn - 5);
    CU_ASSERT(min_gap.extent == 5);
    vqec_gaptree_extract_min(&vqec_gaptree, &min_gap, test_gap_pool);
    CU_ASSERT(min_gap.start_seq == 0);
    CU_ASSERT(min_gap.extent == 3);
    CU_ASSERT(vqec_gaptree_is_empty(&vqec_gaptree));
    CU_ASSERT(VQEC_GAPTREE_ERR_TREEISEMPTY ==
              vqec_gaptree_extract_min(&vqec_gaptree, &min_gap, test_gap_pool));
    
    /* add it back */
    err = vqec_gaptree_add_gap(&vqec_gaptree, gap, test_gap_pool);
    CU_ASSERT(err == VQEC_GAPTREE_OK);

    /* remove things around the boundaries */
    err = vqec_gaptree_remove_seq_num(&vqec_gaptree, maxn - 5, test_gap_pool);
    CU_ASSERT(err == VQEC_GAPTREE_OK);
    err = vqec_gaptree_remove_seq_num(&vqec_gaptree, maxn - 3, test_gap_pool);
    CU_ASSERT(err == VQEC_GAPTREE_OK);
    err = vqec_gaptree_remove_seq_num(&vqec_gaptree, 0, test_gap_pool);
    CU_ASSERT(err == VQEC_GAPTREE_OK);
    err = vqec_gaptree_remove_seq_num(&vqec_gaptree, 2, test_gap_pool);
    CU_ASSERT(err == VQEC_GAPTREE_OK);
    err = vqec_gaptree_remove_seq_num(&vqec_gaptree, maxn, test_gap_pool);
    CU_ASSERT(err == VQEC_GAPTREE_OK);

    /* so now we should have: (1,0), (3,0), (4294967291,0), (4294967293,1) */
    vqec_gaptree_extract_min(&vqec_gaptree, &min_gap, test_gap_pool);
    CU_ASSERT(min_gap.start_seq == maxn - 4);
    CU_ASSERT(min_gap.extent == 0);
    vqec_gaptree_extract_min(&vqec_gaptree, &min_gap, test_gap_pool);
    CU_ASSERT(min_gap.start_seq == maxn - 2);
    CU_ASSERT(min_gap.extent == 1);
    vqec_gaptree_extract_min(&vqec_gaptree, &min_gap, test_gap_pool);
    CU_ASSERT(min_gap.start_seq == 1);
    CU_ASSERT(min_gap.extent == 0);
    vqec_gaptree_extract_min(&vqec_gaptree, &min_gap, test_gap_pool);
    CU_ASSERT(min_gap.start_seq == 3);
    CU_ASSERT(min_gap.extent == 0);
    CU_ASSERT(vqec_gaptree_is_empty(&vqec_gaptree));
    CU_ASSERT(VQEC_GAPTREE_ERR_TREEISEMPTY ==
              vqec_gaptree_extract_min(&vqec_gaptree, &min_gap, test_gap_pool));

    /* make sure gap tree is empty for rest of tests */
    while (!vqec_gaptree_is_empty(&vqec_gaptree)) {
        vqec_gaptree_extract_min(&vqec_gaptree,
                                 &min_gap,
                                 test_gap_pool);
    }
}

void test_vqec_gaptree_add_gap (void)
{
    int i, j;
    vqec_gap_t gap, min_gap;
    vqec_gaptree_error_t err;

    memset(&gap, 0, sizeof(vqec_gap_t));
    memset(&min_gap, 0, sizeof(vqec_gap_t));
    min_gap.start_seq = VQEC_GAPTREE_SEQ_NUMS + 1;

    /* verify tree is empty */
    CU_ASSERT(vqec_gaptree_is_empty(&vqec_gaptree));
    CU_ASSERT(VQEC_GAPTREE_ERR_TREEISEMPTY ==
              vqec_gaptree_extract_min(&vqec_gaptree, &min_gap, test_gap_pool));

    for (i = 0; i < VQEC_GAPTREE_NUM_GAPS; i++) {
        gap.start_seq = rand() % VQEC_GAPTREE_SEQ_NUMS;
        gap.extent = rand() % 10;
        err = vqec_gaptree_add_gap(&vqec_gaptree,
                                   gap,
                                   test_gap_pool);

        switch (err) {
            case VQEC_GAPTREE_OK:
                insert_succ++;
                for (j = gap.start_seq; j <= gap.start_seq + gap.extent; j++) {
                    if (j < VQEC_GAPTREE_SEQ_NUMS) {
                        gap_array[j] = 1;
                    }
                }
                break;
            case VQEC_GAPTREE_ERR_INVALIDARGS:
                insert_fail++;
                if_args++;
                break;
            case VQEC_GAPTREE_ERR_ZONEALLOCFAILURE:
                insert_fail++;
                if_alloc++;
                break;
            case VQEC_GAPTREE_ERR_OVERLAPBELOW:
                insert_fail++;
                if_olb++;
                break;
            case VQEC_GAPTREE_ERR_OVERLAPABOVE:
                insert_fail++;
                if_ola++;
                break;
            case VQEC_GAPTREE_ERR_DUPLICATE:
                insert_fail++;
                if_d++;
                break;
            default:
                insert_fail++;
                printf("unknown gap_add failure\n");
                break;
        }
    }

    /* verify tree is not empty */
    CU_ASSERT(!vqec_gaptree_is_empty(&vqec_gaptree));
    CU_ASSERT(VQEC_GAPTREE_OK ==
              vqec_gaptree_extract_min(&vqec_gaptree,
                                       &min_gap,
                                       test_gap_pool));

    CU_ASSERT(min_gap.start_seq != VQEC_GAPTREE_SEQ_NUMS + 1);
    for (j = min_gap.start_seq; j <= min_gap.start_seq + min_gap.extent; j++) {
        gap_array[j] = 0;
    }

}

void test_vqec_gaptree_remove_gap (void)
{
    int i, j;
    vqec_gap_t gap, min_gap;

    memset(&gap, 0, sizeof(vqec_gap_t));

    for (i = 0; i < VQEC_GAPTREE_NUM_GAPS; i++) {
        gap.start_seq = rand() % VQEC_GAPTREE_SEQ_NUMS;

        if (VQEC_GAPTREE_OK != vqec_gaptree_remove_seq_num(&vqec_gaptree,
                                                           gap.start_seq,
                                                           test_gap_pool)) {
            remove_fail++;
        } else {
            remove_succ++;
            gap_array[gap.start_seq] = 0;
        }
    }

    /* extract the minimum half of gaps */
    for (i = 0; i < VQEC_GAPTREE_NUM_GAPS / 2; i++) {
        CU_ASSERT(VQEC_GAPTREE_OK == 
                  vqec_gaptree_extract_min(&vqec_gaptree, &min_gap, test_gap_pool));
        for (j = min_gap.start_seq; j <= min_gap.start_seq + min_gap.extent; j++) {
            gap_array[j] = 0;
        }
    }
}

void test_vqec_gaptree_print_stats (void)
{
    /* print out some stats from the tests here */

    zone_info_t zi;

    (void)zm_zone_get_info(test_gap_pool, &zi);
    printf("global gap pool stats:\n");
    printf(" max entries:\t\t%d\n", zi.max);
    printf(" used entries:\t\t%d\n", zi.used);
    printf(" high water entries:\t%d\n", zi.hiwat);
    printf(" fail gap alloc drops:\t%d\n",zi.alloc_fail);
    printf("operation stats:\n"
           " insert success:\t%llu\n"
           " insert failed:\t\t%llu\n"
           "  invalid args:\t\t%llu\n"
           "  alloc failed:\t\t%llu\n"
           "  overlap below:\t%llu\n"
           "  overlap above:\t%llu\n"
           "  duplicate:\t\t%llu\n"
           " removal success:\t%llu\n"
           " removal failed:\t%llu\n",
           insert_succ, insert_fail, if_args, if_alloc, if_olb, if_ola, if_d,
           remove_succ, remove_fail);
}

int test_vqec_gaptree_clean (void)
{
    zone_instance_put(test_gap_pool);

    return 0;
}

CU_TestInfo test_array_gaptree[] = {
    {"test vqec_gaptree_init_func",test_vqec_gaptree_init_func},
    {"test vqec_gaptree_rollover_funcs",test_vqec_gaptree_rollover},
    {"test vqec_gaptree_add_gap",test_vqec_gaptree_add_gap},
    {"test vqec_gaptree_print_tree",test_vqec_gaptree_print_tree},
    {"test vqec_gaptree_remove_gap",test_vqec_gaptree_remove_gap},
    {"test vqec_gaptree_verify_gaps",test_vqec_gaptree_verify_gaps},
    {"test vqec_gaptree_print_stats",test_vqec_gaptree_print_stats},
    CU_TEST_INFO_NULL,
};

