/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file contains support for interposers that are to be shared
 * between the control and data plane.
 */

#if defined (_VQEC_UTEST_INTERPOSERS) || defined (_VQEC_DP_UTEST)

#include <stdio.h>
#include "zone_mgr.h"
#include <dlfcn.h>
#include <netinet/in.h>
#include "vqec_recv_socket.h"
#include "sys/event.h"

#include "test_vqec_interposers_common.h"

#undef recvmsg
#undef malloc
#undef free
#undef zone_acquire
#undef event_add
#undef event_del
#undef event_init
#undef event_init_with_lock
#undef event_base_loopexit
#undef event_base_dispatch
#undef calloc
#undef connect

unsigned int recvmsg_fail = 0;
unsigned int malloc_fail = 0;
unsigned int malloc_cache = 0;
unsigned int malloc_counter = 0;
unsigned int free_counter = 0;
unsigned int zone_acquire_fail = 0;
unsigned int event_add_fail = 0;
unsigned int event_del_fail = 0;
unsigned int event_init_fail = 0;
unsigned int event_dispatch_fail = 0;
unsigned int event_loopexit_fail = 0;
unsigned int calloc_fail = 0;
unsigned int connect_fail = 0;


static void *mallocs[1000];

ssize_t test_recvmsg (int socket, struct msghdr *message, int flags) {
    //printf("overloaded recvmsg() called!\n");
/*    int bytes_recvd;
    int pak_stream;
    int sock_fid;
    char pak_buf[1370];
    pak_stream = open("vam_stream.pak", O_RDONLY);
    socket();
    //AF_UNIX,SOCK_DGRAM,0);
    read(pak_stream, &pak_buf, 1370);
    write(sock_fid, &pak_buf, 1370);
    bytes_recvd = recvmsg(sock_fid, message, 0);
    printf("\nreceived %d bytes!\n", bytes_recvd);
    return bytes_recvd;
*/
    //struct msghdr msg;
    struct cmsghdr *cmsg;
    int ret;
    switch (recvmsg_fail) {
        default:
        case 0: //work normally
            return recvmsg(socket, message, flags);
        case 1: //fail with general errno
            errno = recvmsg_fail;
            return -1;
        case 2: //set first CMSG_HDR to NULL but pretend to work
            ret = recvmsg(socket, message, flags);
            cmsg = CMSG_FIRSTHDR(message);
            if (cmsg) {
                cmsg->cmsg_level = 1234;
            }
            //message = &msg;
            return ret;
    }
}

void test_recvmsg_make_fail (unsigned int fail_err) {
    recvmsg_fail = fail_err;
}


void *test_malloc_cache (size_t size) {
    void *malloc_ptr;
    malloc_ptr = malloc(size);
    mallocs[malloc_counter] = malloc_ptr;
    //printf("stored malloc pointer in mallocs[%u]\n", malloc_counter);
    malloc_counter++;
    return malloc_ptr;
}

static int malloc_fail_index, malloc_fail_count; 
void *test_malloc (size_t size) {
    //printf("overloaded malloc() has been called! count=%u\n", malloc_counter+1);
    if (!malloc_fail ||
        (malloc_fail_index && (malloc_fail_index == malloc_fail_count++))) { 
        if (malloc_cache == 0) {
            return malloc(size);
        } else {
            return test_malloc_cache(size);
        }
    } else {
        errno = malloc_fail;
        return NULL;
    }
}

void test_malloc_make_fail (unsigned int fail_err) {
    malloc_fail = fail_err;
}

void test_malloc_make_cache (unsigned int cache) {
    malloc_cache = cache;
}

void
test_malloc_fail_nth_alloc (int n)
{
    malloc_fail_index = n;
    malloc_fail_count = 0;
}

void test_free (void *ptr) {
    if (malloc_cache == 0) {
        free(ptr);
    } else {
        //printf("overloaded free() has been called! count=%u\n", free_counter+1);
        /* search the mallocs array for our pointer!! */
        int i;
        boolean found = 0;
        for(i = 0; i < malloc_counter; i++){
            if (mallocs[i] == ptr) {
                found = 1;
                //printf("found malloc pointer in mallocs[%u]; removing and freeing...", i);
                mallocs[i] = NULL;
                free_counter++;
                free(ptr);
                break;
                //printf("done\n");
            }
        }
        //if (!found) { printf("can't find malloc pointer to free!\n"); }
    }
}

int diff_malloc_free (void) {
    return malloc_counter - free_counter;
}


void *test_zone_acquire (struct vqe_zone *z) {
    if (zone_acquire_fail == 0) {
        return zone_acquire(z);
    } else {
        return NULL;
    }
}

void test_zone_acquire_make_fail (unsigned int fail_err) {
    zone_acquire_fail = fail_err;
}

int test_event_add (struct event *ev, struct timeval *tv) {
    if (event_add_fail == 0) {
        return event_add(ev,tv);
    } else {
        return -1;
    }
}

void test_event_add_make_fail (unsigned int fail_err) {
    event_add_fail = fail_err;
}

int test_event_del (struct event *ev) {
    if (event_del_fail == 0) {
        return event_del(ev);
    } else {
        return -1;
    }
}

void test_event_del_make_fail (unsigned int fail_err) {
    event_del_fail = fail_err;
}

void test_event_init_make_fail (unsigned int fail_err) {
    event_init_fail = fail_err;
}

void *test_event_init (void) {
    if (event_init_fail == 0) {
        return event_init();
    } else {
        return NULL;
    }
}

void *test_event_init_with_lock (struct event_mutex *m) {
    if (event_init_fail == 0) {
        return event_init_with_lock(m);
    } else {
        return NULL;
    }
}

/*
 *
 * recvmsg()
 * read from file vam_stream.pak instead of socket
 *
 */
/*
ssize_t recvmsg(int socket, struct msghdr *message, int flags){
    #define BUF_LEN 1316
    printf("overloaded recvmsg() called!\n");
    int pak_stream;
    pak_stream = open("vam_cap.pak", O_RDONLY);
    char read_buf[BUF_LEN];
    uint32_t buf_len = BUF_LEN;
    read(pak_stream, &read_buf, buf_len);
    close(pak_stream);
    //PRINT_BUF_HEX("buf",buf,buf_len);
    printf("returning %u\n", buf_len);
    return buf_len;
}
*/

int32_t test_event_base_dispatch(struct event_base *base)
{
    if (event_dispatch_fail)
        return (-1);
    else
        return (event_base_dispatch(base));
}

int32_t test_event_base_loopexit(struct event_base *base, struct timeval *tv)
{
    if (event_loopexit_fail)
        return (-1);
    else
        return (event_base_loopexit(base, tv));
}

void test_event_dispatch_make_fail (unsigned int fail_err) {
    event_dispatch_fail = fail_err;
}

void test_event_loopexit_make_fail (unsigned int fail_err) {
    event_loopexit_fail = fail_err;
}

void *test_calloc (size_t memb, size_t size) {
    if (calloc_fail == 0) {
        return calloc(memb, size);
    } else {
        errno = calloc_fail;
        return NULL;
    }
}

void test_calloc_make_fail (unsigned int fail_err) {
    calloc_fail = fail_err;
}
/*
 * test_connect_make_fail()
 *
 * @param[in] fail_err  - non-zero to make interposer fail by returning -1
 *                                 and setting errno to "fail_err", or
 *                        0 to make interposer call connect()
 */
void test_connect_make_fail (unsigned int fail_err) {
    connect_fail = fail_err;
}
int test_connect(int socket, const struct sockaddr *address,
                  socklen_t address_len)
{
    if (!connect_fail) {
        return connect(socket, address, address_len);
    } else {
        errno = connect_fail;
        return (-1);
    }
}

#endif /* defined (_VQEC_UTEST_INTERPOSERS) || defined (_VQEC_DP_UTEST) */
