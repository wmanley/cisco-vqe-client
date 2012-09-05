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
 * Description: Kernel vam util tests.
 *
 * Documents: 
 *
 *****************************************************************************/

#include "utils/vam_util.h"

void
vqec_test_util (void)
{
#define VQE_IPV4_ADDR_FORMAT_STRING "255.255.255.255"
    char buf[sizeof(VQE_IPV4_ADDR_FORMAT_STRING) + 1];
    char *out;
    uint32_t addr = 0xFFFFFF01;

    out = uint32_ntoa_r(addr, buf, sizeof(buf));
    if (out) {
        printk("Address is %s\n", out);
    } else {
        printk(KERN_ERR "uint32_ntoa_r failed\n");
        return;
    }
    addr = 0;
    out = uint32_ntoa_r(addr, buf, sizeof(buf));
    if (out) {
        printk("Address is %s\n", out);
    } else {
        printk(KERN_ERR "uint32_ntoa_r failed\n");
        return;
    }
    addr = 0xF0F1F2F3;
    out = uint32_ntoa_r(addr, buf, sizeof(buf));
    if (out) {
        printk("Address is %s\n", out);
    } else {
        printk(KERN_ERR "uint32_ntoa_r failed\n");
        return;
    }
    out = uint32_htoa_r(addr, buf, sizeof(buf));
    if (out) {
        printk("Address is %s\n", out);
    } else {
        printk(KERN_ERR "uint32_htoa_r failed\n");
        return;
    }
}
