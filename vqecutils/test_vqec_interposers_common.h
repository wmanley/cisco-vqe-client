/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file contains support for interposers that are to be shared
 * between the control and data plane.
 */

#if !defined (_VQEC_UTEST_INTERPOSERS) && !defined (_VQEC_DP_UTEST)
#error Only include test_vqec_interposers_common.h when building unit tests!!!
#endif

#include <sys/socket.h>
#include <stdio.h>
#include "../include/utils/zone_mgr.h"
#include "vqec_event.h"

/*
 *  macros to overload the respective function calls
 *  
 *  whenever the module code makes calls to the functions that are
 *  redefined here, the function call will be redirected to the corresponding
 *  "interposing" test_function instead, which we can write to do whatever we
 *  want it to do
 */

#define recvmsg(arg1, arg2, arg3) test_recvmsg(arg1, arg2, arg3)
#define malloc(arg) test_malloc(arg)
#define free(arg) test_free(arg)
#define zone_acquire(arg) test_zone_acquire(arg)
#define event_add(arg1, arg2) test_event_add(arg1, arg2)
#define event_del(arg1) test_event_del(arg1)
#define event_init test_event_init
#define event_init_with_lock test_event_init_with_lock
#define event_base_dispatch test_event_base_dispatch
#define event_base_loopexit test_event_base_loopexit
#define calloc(arg1, arg2) test_calloc(arg1, arg2)

/*
 *  test_recvmsg() - the interposer for recvmsg()
 *  fail_err = 0: work normally
 *  fail_err = 1: fail with general errno (-1)
 *  fail_err = 2: set first CMSG_HDR to NULL but pretend to work
 */
ssize_t test_recvmsg(int socket, struct msghdr *message, int flags);
void test_recvmsg_make_fail(unsigned int fail_err);

/*
 *  test_malloc() - the interposer for malloc()
 *  fail_err = 0 && cache = 0: work normally
 *  fail_err = 0 && cache = 1: work normally, but keep track of malloc'ed pointers in
 *                  in an array
 *  fail_err != 0: return NULL and set errno = fail_err
 */
void *test_malloc(size_t size);
void test_malloc_make_fail(unsigned int fail_err);
void test_malloc_make_cache(unsigned int cache);
void test_malloc_fail_nth_alloc(int n);

/*
 *  test_free() - the interposer for free()
 *  works normally, but removes each pointer from the array of malloc'ed
 *                  pointers as it's freed
 */
void test_free(void *ptr);

/*
 *  helper function
 *  returns the number of malloc pointers that have not been freed
 */
int diff_malloc_free(void);

/*
 *  test_zone_acquire() - the interposer for zone_acquire()
 *  fail_err = 0: work normally
 *  fail_err = 1: return NULL
 */
void *test_zone_acquire(struct vqe_zone *z);
void test_zone_acquire_make_fail(unsigned int fail_err);

/*
 *  test_event_add() - the interposer for event_add()
 *  fail_err = 0: work normally
 *  fail_err = 1: return -1
 */
struct event;
struct event_base;
struct event_mutex;
int test_event_add(struct event *ev, struct timeval *tv);
void test_event_add_make_fail(unsigned int fail_err);
int test_event_del(struct event *ev);
void test_event_del_make_fail(unsigned int fail_err);
void *test_event_init(void);
void *test_event_init_with_lock(struct event_mutex *m);
void test_event_init_make_fail(unsigned int fail_err);
void test_assert(char *f, int32_t l);
int32_t event_base_dispatch(struct event_base *base);
int32_t event_base_loopexit(struct event_base *base, struct timeval *tv);
void test_event_dispatch_make_fail(unsigned int fail_err);
void test_event_loopexit_make_fail(unsigned int fail_err);
void *test_calloc(size_t memb, size_t size);
void test_calloc_make_fail(unsigned int fail_err);

/*
 * connect() interposer
 */
#define connect(arg1, arg2, arg3) test_connect(arg1, arg2, arg3)
/*
 * test_connect_make_fail()
 *
 * @param[in] fail_err  - non-zero to make interposer fail by returning -1
 *                                 and setting errno to "fail_err", or
 *                        0 to make interposer call connect()
 */
void test_connect_make_fail(unsigned int fail_err);
int test_connect(int socket, const struct sockaddr *address,
                 socklen_t address_len);
