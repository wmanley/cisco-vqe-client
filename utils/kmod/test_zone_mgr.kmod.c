/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Kernel zone manager tests.
 *
 * Documents: 
 *
 *****************************************************************************/

#include "utils/vam_types.h"
#include "utils/zone_mgr.h"

void
vqec_test_zones (void)
{
#define ZELEMS 32
#define ZELEM_SIZE 256
    struct vqe_zone *z, *z_dup;
    int32_t i;
    void *buff[32], *last;
    zone_info_t zi;

    z = zone_instance_get_loc("Foo", 0, ZELEM_SIZE, ZELEMS, NULL, NULL);
    if (!z) {
        printk(KERN_ERR "non-null zone\n");
        return;
    }
    z_dup = zone_instance_get_loc("Foo", 0, 1330, 1024, NULL, NULL);
    if (z_dup) { 
        printk(KERN_ERR "did not detect duplicate zone\n");
        return;
    }
    if (zm_elem_size(z) != ZELEM_SIZE) {
        printk(KERN_ERR "zone element size check failed\n");
        return;
    } else {
        printk("SUCCESS checking zone elem size\n");
    }
    for (i = 0; i < ZELEMS; i++) {
        buff[i] = zone_acquire(z);
        if (!buff[i]) {
            printk(KERN_ERR "allocation failed\n");
            return;
        } else {
            printk("%u:%p:%p\n", i, &buff[i], buff[i]);
            memset(buff[i], 0, ZELEM_SIZE);
        }
    }
    last = zone_acquire(z);
    if (last) {
        printk(KERN_ERR "limit enforcement failure\n");
        return;
    } else {
        printk("SUCCESS in limit enforcement\n");
    }
    zone_shrink_cache(z);
    i = zm_zone_get_info(z, &zi);
    if (!i) {
        if ((zi.max != ZELEMS) ||
            (zi.used != ZELEMS) ||
            (zi.hiwat != ZELEMS) ||
            (zi.alloc_fail != 1)) {
            printk(KERN_ERR "bad zone info block\n");
            return;
        } else {
            printk("SUCCESS verifying zone info block\n");
        }
    } else {
        printk(KERN_ERR "zone info retrieval failure\n");
        return;
    }
    for (i = 0; i < ZELEMS; i++) {
        printk("%u:%p:%p\n", i, &buff[i], buff[i]);
        zone_release(z, buff[i]);
    }
    i = zm_zone_get_info(z, &zi);
    if (!i) {
        if ((zi.max != ZELEMS) ||
            (zi.used != 0) ||
            (zi.hiwat != ZELEMS) ||
            (zi.alloc_fail != 1)) {
            printk(KERN_ERR "bad zone info block\n");
            return;
        } else {
            printk("SUCCESS verifying zone info block\n");
        }
    } else {
        printk(KERN_ERR "zone info retrieval failure\n");
        return;
    }
    if (zone_instance_put(z)) {
        printk(KERN_ERR "failure to free zone\n");
        return;
    }
}
