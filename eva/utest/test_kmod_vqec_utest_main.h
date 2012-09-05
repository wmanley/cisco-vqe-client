/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifndef __VQEC_UTEST_MAIN_H__
#define __VQEC_UTEST_MAIN_H__

#include <unistd.h>
#include <stdio.h>
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"

/* useful macro for printing buffers to screen in hex */
#define PRINT_BUF_HEX(buf_name,buf,buf_len){ \
        printf("\n%s[%u]:", buf_name, buf_len); \
        int i; \
        for (i = 0; i < buf_len; i++) \
        { \
          if (i % 8 == 0) \
            printf(" "); \
          if (i % 24 == 0) \
            printf("\n"); \
          printf("%02hx ",buf[i] & 0x00ff); \
        } \
}

/* unit tests for rpc */
int test_vqec_rpc_init(void);
int test_vqec_rpc_clean(void);
extern CU_TestInfo test_array_rpc[];

#endif
