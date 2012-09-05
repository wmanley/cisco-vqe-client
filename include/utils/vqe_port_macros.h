/*
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef __VQE_PORT_MACROS__H
#define __VQE_PORT_MACROS__H

#ifdef __VQEC__
#include "../../eva/vqec_cli.h"
#include "vam_types.h"
#include <log/syslog.h>

struct vqec_cli_def *vqec_get_cli_def(void);
int vqec_get_cli_fd(void *cli_handle);
void vqec_set_cli_def(struct vqec_cli_def *cli);

extern void vqes_syslog_cli(int pri, const char *fmt, ...);

#define CONSOLE_PRINTF(format, args...)     vqes_syslog_cli(1, format, ##args)

#else
#define CONSOLE_PRINTF printf
#endif

#endif
