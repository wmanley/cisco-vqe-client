/*
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifndef __VQEC_LIBCLI_REGISTER_H__
#define __VQEC_LIBCLI_REGISTER_H__

/**
 * Structure to work with debug flags as they relate to both the CP and DP.
 */
typedef struct vqec_cli_debug_type_ {
    int cp;     /* CP debug flag */
    int dp;     /* DP debug flag */
} vqec_cli_debug_type_t;

/**
 * Invalid debug flag.
 */
#define VQEC_DEBUG_FLAG_INVALID (-1)

void * vqec_cli_startup(void);
int vqec_cli_loop_wrapper(uint16_t cli_port, in_addr_t cli_if_addr);
int vqec_get_cli_fd(void *cli_handle);
void vqec_destroy_cli(void);

#endif
