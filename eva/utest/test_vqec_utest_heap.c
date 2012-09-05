/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 *
 ******************************************************************************
 *
 * File: test_vqec_utest_heap.c
 *
 * Description: Unit tests for the vqe_heap structure and functions.
 *
 * Documents:
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <vqe_heap.h>
#include "test_vqec_utest_main.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"


#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

/*
 *  Unit tests for vqe_heap
 */

#define TEST_VQEC_HEAP_MAX_HEAP_SIZE 1000
#define TEST_VQEC_HEAP_EXCESS_PACKETS 10
#define TEST_VQEC_HEAP_NUM_PACKETS \
    TEST_VQEC_HEAP_MAX_HEAP_SIZE + TEST_VQEC_HEAP_EXCESS_PACKETS

typedef struct testpak_ {
    uint seq;
    char *pak_data;
} testpak_t;

/*
 * compare function to be used to create a min-heap (instead of the default min-heap)
 */
static int test_vqe_heap_compare(testpak_t *a, testpak_t *b)
{
    if (a->seq < b->seq) {
        return (-1);
    } else if (a->seq > b->seq) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * function to print contents of an element
 */
static int test_vqe_heap_print_elem(testpak_t *elem)
{
    printf("   seq_num = %d\n", elem->seq);
    return 0;
}

static struct test_heap VQE_HEAP(testpak_) heap;

VQE_HEAP_PROTOTYPE(test_heap,
                   testpak_);

VQE_HEAP_GENERATE(test_heap,
                  testpak_,
                  test_vqe_heap_compare,
                  VQE_HEAP_POLICY_MIN);

testpak_t test_paks[TEST_VQEC_HEAP_NUM_PACKETS];
struct timeval tv;
uint32_t before, after;

int test_vqec_heap_init (void)
{
    int i;

    for (i = 0; i < TEST_VQEC_HEAP_NUM_PACKETS; i++) {
        test_paks[i].seq = rand() % 65536;
    }

    return 0;
}

int test_vqec_heap_clean (void)
{
    return 0;
}

static void test_vqec_heap_init_func (void)
{
    CU_ASSERT(VQE_HEAP_OK == VQE_HEAP_INIT(test_heap, &heap, TEST_VQEC_HEAP_MAX_HEAP_SIZE));
    CU_ASSERT(VQE_HEAP_ERR_ALREADYINITTED == 
              VQE_HEAP_INIT(test_heap, &heap, TEST_VQEC_HEAP_MAX_HEAP_SIZE));
}

static void test_vqec_heap_insertion (void)
{
    int i;
    int num_failed = 0;
    vqe_heap_error_t ret = VQE_HEAP_OK;

    gettimeofday(&tv, NULL);
    before = tv.tv_sec * 1000000 + tv.tv_usec;

    for (i = 0; i < TEST_VQEC_HEAP_NUM_PACKETS; i++) {
        if (VQE_HEAP_OK != VQE_HEAP_INSERT(test_heap, &heap, &(test_paks[i]))) {
            printf("failed to insert pak p[%d]\n", i);
            num_failed++;
        }
    }

    gettimeofday(&tv, NULL);
    after = tv.tv_sec * 1000000 + tv.tv_usec;
    printf("insert time elapsed:  %d usec\n", after - before);

    CU_ASSERT(num_failed == TEST_VQEC_HEAP_EXCESS_PACKETS);

    /* test printing */
    ret = VQE_HEAP_PRINT(test_heap, &heap, printf, test_vqe_heap_print_elem);
    CU_ASSERT(ret == VQE_HEAP_OK);
    ret = VQE_HEAP_PRINT(test_heap, &heap, printf, NULL);
    CU_ASSERT(ret == VQE_HEAP_OK);
    ret = VQE_HEAP_PRINT(test_heap, &heap, NULL, NULL);
    CU_ASSERT(ret == VQE_HEAP_ERR_INVALIDARGS);

}

static void test_vqec_heap_extract_min (void)
{
    uint cur_seq = 0, prev_seq = 0;
    uint32_t before, after;
    int num_failed = 0, num_passed = 0, num_nulls = 0;
    testpak_t *pak;

    gettimeofday(&tv, NULL);
    before = tv.tv_sec * 1000000 + tv.tv_usec;

    while (!num_nulls) {
        prev_seq = cur_seq;
        pak = VQE_HEAP_EXTRACT_HEAD(test_heap, &heap);
        if (!pak) {
            num_nulls++;
            continue;
        }
		cur_seq = pak->seq;
        if (cur_seq < prev_seq) {
            printf("prev = %i, cur = %i\tSEQ ERROR!!!\n", prev_seq, cur_seq);
            num_failed++;
        } else {
            num_passed++;
        }
    }

    gettimeofday(&tv, NULL);
    after = tv.tv_sec * 1000000 + tv.tv_usec;
    printf("extract_min time elapsed:  %d usec\n", after - before);

    CU_ASSERT(num_passed == 
              TEST_VQEC_HEAP_NUM_PACKETS - TEST_VQEC_HEAP_EXCESS_PACKETS);
    CU_ASSERT(num_failed == 0);
    CU_ASSERT(num_nulls == 1);
}

static void test_vqec_heap_deinit (void)
{
    int i, heap_highwater, num_flushed = 0;
    testpak_t *tmp;

    /* repopulate heap */
    for (i = 0; i < TEST_VQEC_HEAP_NUM_PACKETS; i++) {
        VQE_HEAP_INSERT(test_heap, &heap, &(test_paks[i]));
    }

    heap_highwater = heap.cur_size;

    /* should fail - heap is not empty */
    CU_ASSERT(VQE_HEAP_ERR_HEAPNOTEMPTY ==
              VQE_HEAP_DEINIT(test_heap, &heap));

    VQE_HEAP_REMOVE_FOREACH(test_heap, &heap, tmp) {
        if (tmp) {
            num_flushed++;
        }
    }
    CU_ASSERT(num_flushed == heap_highwater);
    CU_ASSERT(VQE_HEAP_IS_EMPTY(test_heap, &heap));

    CU_ASSERT(VQE_HEAP_OK == VQE_HEAP_DEINIT(test_heap, &heap));
}

CU_TestInfo test_array_heap[] = {
    {"test vqec_heap_init_func",test_vqec_heap_init_func},
    {"test vqec_heap_insertion",test_vqec_heap_insertion},
    {"test vqec_heap_extract_min",test_vqec_heap_extract_min},
    {"test vqec_heap_deinit",test_vqec_heap_deinit},
    CU_TEST_INFO_NULL,
};
