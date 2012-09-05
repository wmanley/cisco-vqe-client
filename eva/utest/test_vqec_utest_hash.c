/*
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "test_vqec_utest_interposers.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

#include <vqe_hash.h>
#include <stdint.h>

#include "vqec_recv_socket.h"
#include "vqec_error.h"
#include "vqec_debug.h"
#include <arpa/inet.h>
#include <log/vqe_utils_syslog_def.h>

/*
 * Unit tests for vqe_hash
 */

vqe_hash_t *test_htable;
vqe_hash_elem_t *test_helem1, *test_helem2, *test_helem3, *test_helem4,
    *test_helem5, *test_helem6, *test_helem7, *test_helem8, *test_helem9;
vqe_hash_key_t *test_key;

#define NUM_BUCKETS 12

static uint32_t test_vqe_hash_test_func (uint8_t *key,
                                          int32_t len, int32_t mask) {
    return *key % NUM_BUCKETS;
}

static uint32_t test_vqe_hash_bad_test_func (uint8_t *key,
                                              int32_t len, int32_t mask) {
    return NUM_BUCKETS + 1;
}

static boolean test_vqe_hash_key_compare_func (const vqe_hash_key_t *key1, 
                                                const vqe_hash_key_t *key2)
{
    return FALSE;
}

int test_vqe_hash_init (void) {
    test_malloc_make_fail(0);
    init_vqe_hash_elem_module();
    init_vqe_hash_module();
    return 0;
}

int test_vqe_hash_clean (void) {
    return 0;
}

static void test_vqe_hash_create (void) {
    /* 
     * A. Make malloc fail.
     */
    /*
      Remove this test since vqec_hash is now vqe_hash.
      test_calloc_make_fail(1);
      test_htable = 
      vqe_hash_create(NUM_BUCKETS, test_vqe_hash_test_func, NULL);
      test_calloc_make_fail(0);
      CU_ASSERT(test_htable == NULL);
    */


    /*  
     * B. Working malloc, invalid buckets.
     */
    test_htable = vqe_hash_create(500000, test_vqe_hash_test_func, NULL);
    CU_ASSERT(test_htable == NULL);

    /*
     * C. Working malloc, valid buckets, no hash function.
     */
    test_htable = vqe_hash_create(NUM_BUCKETS, NULL, NULL);
    CU_ASSERT(test_htable == NULL);

    /*
     * D. Working malloc, valid buckets, hash function.
     */
    test_htable = vqe_hash_create(NUM_BUCKETS, test_vqe_hash_test_func, NULL);
    CU_ASSERT_FALSE(test_htable == NULL);
    vqe_hash_destroy(test_htable);
    
    /*
     * E. Register a different compare function.
     */
    test_htable = vqe_hash_create(NUM_BUCKETS, test_vqe_hash_test_func, test_vqe_hash_key_compare_func);
    CU_ASSERT_FALSE(test_htable == NULL);
    CU_ASSERT(test_htable->comp_func == test_vqe_hash_key_compare_func);
    vqe_hash_destroy(test_htable);

    test_htable = vqe_hash_create(NUM_BUCKETS, test_vqe_hash_test_func, NULL);
    CU_ASSERT_FALSE(test_htable == NULL);

}

static void test_vqe_hash_elem_create (void) {
    vqe_hash_key_t key;
    int32_t kval;
    int32_t *tdata = malloc(sizeof(int));


    /*
     * A. Make malloc Fail.
     */
    /* kval = 10;
       VQE_HASH_MAKE_KEY(&key, &kval, sizeof(int32_t));
       test_calloc_make_fail(1);
       test_helem1 = vqe_hash_elem_create(&key, NULL);
       test_calloc_make_fail(0);
       CU_ASSERT(test_helem1 == NULL);
    */

    /* 
     * B. Use NULL key.
     */
    test_helem1 = vqe_hash_elem_create(NULL, NULL);
    CU_ASSERT(test_helem1 == NULL);

    /* 
     * C. Use key with 0 length.
     */
    VQE_HASH_MAKE_KEY(&key, &kval, 0);
    test_helem1 = vqe_hash_elem_create(&key, NULL);
    CU_ASSERT(test_helem1 == NULL);

    /* 
     * D. Use key with no value pointer..
     */
    VQE_HASH_MAKE_KEY(&key, 0, sizeof(int));
    test_helem1 = vqe_hash_elem_create(&key, NULL);
    CU_ASSERT(test_helem1 == NULL);

    /* 
     * E. Create with working malloc.
     */
    kval = 3;
    *tdata = 5;
    VQE_HASH_MAKE_KEY(&key, &kval, sizeof(int));
    test_helem1 = vqe_hash_elem_create(&key, tdata);
    CU_ASSERT_FALSE(test_helem1 == NULL);
    CU_ASSERT(test_helem1->data == tdata);

    kval += 3;
    *tdata += 5;
    VQE_HASH_MAKE_KEY(&key, &kval, sizeof(int));
    test_helem2 = vqe_hash_elem_create(&key, tdata);
    CU_ASSERT_FALSE(test_helem2 == NULL);
    CU_ASSERT(test_helem2->data == tdata);

    kval += 3;
    *tdata += 5;
    VQE_HASH_MAKE_KEY(&key, &kval, sizeof(int));
    test_helem3 = vqe_hash_elem_create(&key, tdata);
    CU_ASSERT_FALSE(test_helem3 == NULL);
    CU_ASSERT(test_helem3->data == tdata);

    kval += 3;
    *tdata += 5;
    VQE_HASH_MAKE_KEY(&key, &kval, sizeof(int));
    test_helem4 = vqe_hash_elem_create(&key, tdata);
    CU_ASSERT_FALSE(test_helem4 == NULL);
    CU_ASSERT(test_helem4->data == tdata);

    kval += 3;
    *tdata += 5;
    VQE_HASH_MAKE_KEY(&key, &kval, sizeof(int));
    test_helem5 = vqe_hash_elem_create(&key, tdata);
    CU_ASSERT_FALSE(test_helem5 == NULL);
    CU_ASSERT(test_helem5->data == tdata);

    kval += 3;
    *tdata += 5;
    VQE_HASH_MAKE_KEY(&key, &kval, sizeof(int));
    test_helem6 = vqe_hash_elem_create(&key, tdata);
    CU_ASSERT_FALSE(test_helem6 == NULL);
    CU_ASSERT(test_helem6->data == tdata);

    kval += 3;
    *tdata += 5;
    VQE_HASH_MAKE_KEY(&key, &kval, sizeof(int));
    test_helem7 = vqe_hash_elem_create(&key, tdata);
    CU_ASSERT_FALSE(test_helem7 == NULL);
    CU_ASSERT(test_helem7->data == tdata);

    kval += 3;
    *tdata += 5;
    VQE_HASH_MAKE_KEY(&key, &kval, sizeof(int));
    test_helem8 = vqe_hash_elem_create(&key, tdata);
    CU_ASSERT_FALSE(test_helem8 == NULL);
    CU_ASSERT(test_helem8->data == tdata);

    kval += 3;
    *tdata += 5;
    VQE_HASH_MAKE_KEY(&key, &kval, sizeof(int));
    test_helem9 = vqe_hash_elem_create(&key, tdata);
    CU_ASSERT_FALSE(test_helem9 == NULL);
    CU_ASSERT(test_helem9->data == tdata);
}

static void test_vqe_hash_add_elem (void) {
    boolean status;
    status = MCALL(test_htable,vqe_hash_add_elem,test_helem1);
    CU_ASSERT(status);

    status = MCALL(test_htable,vqe_hash_add_elem,test_helem2);
    CU_ASSERT(status);

    status = MCALL(test_htable,vqe_hash_add_elem,test_helem3);
    CU_ASSERT(status);

    status = MCALL(test_htable,vqe_hash_add_elem,test_helem4);
    CU_ASSERT(status);

    status = MCALL(test_htable,vqe_hash_add_elem,test_helem5);
    CU_ASSERT(status);

    status = MCALL(test_htable,vqe_hash_add_elem,test_helem6);
    CU_ASSERT(status);

    status = MCALL(test_htable,vqe_hash_add_elem,test_helem7);
    CU_ASSERT(status);

    status = MCALL(test_htable,vqe_hash_add_elem,test_helem8);
    CU_ASSERT(status);

    status = MCALL(test_htable,vqe_hash_add_elem,test_helem9);
    CU_ASSERT(status);

    /*
     * A. Check null element insertion.
     */
    status = MCALL(test_htable,vqe_hash_add_elem,NULL);
    CU_ASSERT_FALSE(status);

    /*
     * B. Check invalid buckets. 
     */
    test_htable->hash_func = test_vqe_hash_bad_test_func;
    status = MCALL(test_htable,vqe_hash_add_elem, test_helem1);
    CU_ASSERT_FALSE(status);
    test_htable->hash_func = test_vqe_hash_test_func;

    /*
     * C. Check double insertion. 
     */
    status = MCALL(test_htable,vqe_hash_add_elem,test_helem9);
    CU_ASSERT_FALSE(status);

    /*
     * D. Check invalid key length insertion.
     */
    *(uint8_t *)&test_helem1->key.len = 100;
    status = MCALL(test_htable,vqe_hash_add_elem, test_helem1);
    CU_ASSERT_FALSE(status);
    *(uint8_t *)&test_helem1->key.len = sizeof(int32_t);

    /*
     * D. Check null value ptr.
     */
    int8_t *val = (int8_t *)test_helem1->key.valptr;
    *(int8_t **)&test_helem1->key.valptr = (int8_t *)NULL;
    status = MCALL(test_htable,vqe_hash_add_elem, test_helem1);
    CU_ASSERT_FALSE(status);
    *(int8_t **)&test_helem1->key.valptr = val;
}

static void test_vqe_hash_elem_display (void) {
    MCALL(test_helem1,vqe_hash_elem_display);
}

static void test_vqe_hash_display (void) {
    MCALL(test_htable,vqe_hash_display);
}

static void test_vqe_hash_del_elem (void) {
    boolean status;

    /*
     * A. Use null element. 
     */
    status = MCALL(test_htable,vqe_hash_del_elem, NULL);
    CU_ASSERT_FALSE(status);

    status = MCALL(test_htable,vqe_hash_del_elem,test_helem1);
    CU_ASSERT(status);
    status = MCALL(test_htable,vqe_hash_del_elem,test_helem1);
    CU_ASSERT_FALSE(status);
   
    status = MCALL(test_htable,vqe_hash_del_elem,test_helem2);
    CU_ASSERT(status);
    status = MCALL(test_htable,vqe_hash_del_elem,test_helem2);
    CU_ASSERT_FALSE(status);

    status = MCALL(test_htable,vqe_hash_del_elem,test_helem5);
    CU_ASSERT(status);
    status = MCALL(test_htable,vqe_hash_del_elem,test_helem5);
    CU_ASSERT_FALSE(status);

    status = MCALL(test_htable,vqe_hash_del_elem,test_helem8);
    CU_ASSERT(status);
    status = MCALL(test_htable,vqe_hash_del_elem,test_helem8);
    CU_ASSERT_FALSE(status);

    MCALL(test_htable,vqe_hash_display);

    /*
     * At this point the remaining elements should be 3, 4, 6, 7, 9 */
}

static void test_vqe_hash_get_elem (void) {

    vqe_hash_elem_t *found_elem;
    vqe_hash_key_t key;

    found_elem = MCALL(test_htable,vqe_hash_get_elem,&test_helem4->key);
    CU_ASSERT(found_elem == test_helem4);

    found_elem = MCALL(test_htable,vqe_hash_get_elem,&test_helem5->key);
    CU_ASSERT(found_elem == NULL);

    /* 
     * A. Use null key. 
     */
    found_elem = MCALL(test_htable,vqe_hash_get_elem,NULL);
    CU_ASSERT(found_elem == NULL);
    
    /*
     * B. Check invalid key length.
     */
    int32_t kval = 12;
    VQE_HASH_MAKE_KEY(&key, &kval, 100);
    found_elem = MCALL(test_htable,vqe_hash_get_elem, &key);
    CU_ASSERT(found_elem == NULL);

    /*
     * C. Check null value ptr.
     */
    VQE_HASH_MAKE_KEY(&key, NULL, sizeof(int32_t));
    found_elem = MCALL(test_htable,vqe_hash_get_elem, &key);
    CU_ASSERT(found_elem == NULL);

    /*
     * D. Invalid hash bucket. 
     */
    test_htable->hash_func = test_vqe_hash_bad_test_func;
    found_elem = MCALL(test_htable,vqe_hash_get_elem,&test_helem4->key);
    CU_ASSERT(found_elem == NULL);
    test_htable->hash_func = test_vqe_hash_test_func;
}

static void test_vqe_hash_elem_destroy (void) {
    boolean status;

    /*
     * A. Use null element. 
     */
    status = vqe_hash_elem_destroy(NULL);
    CU_ASSERT_FALSE(status);
    
    status = vqe_hash_elem_destroy(test_helem1);
    CU_ASSERT(status);
}

static void test_vqe_hash_destroy (void) {
    boolean status;

    /*
     * A. Use null table. 
     */
    status = vqe_hash_destroy(NULL);
    CU_ASSERT_FALSE(status);

    status = vqe_hash_destroy(test_htable); 
    CU_ASSERT(status);
}

CU_TestInfo test_array_hash[] = {
    {"test vqe_hash_create",test_vqe_hash_create},
    {"test vqe_hash_elem_create",test_vqe_hash_elem_create},
    {"test vqe_hash_add_elem",test_vqe_hash_add_elem},
    {"test vqe_hash_elem_display",test_vqe_hash_elem_display},
    {"test vqe_hash_display",test_vqe_hash_display},
    {"test vqe_hash_del_elem",test_vqe_hash_del_elem},
    {"test vqe_hash_get_elem",test_vqe_hash_get_elem},
    {"test vqe_hash_elem_destroy",test_vqe_hash_elem_destroy},
    {"test vqe_hash_destroy",test_vqe_hash_destroy},
    CU_TEST_INFO_NULL,
};

