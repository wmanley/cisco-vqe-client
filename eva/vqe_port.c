/*
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "vqe_port_macros.h"
#include "vqec_cli.h"

static struct vqec_cli_def *g_vqec_cli;

struct vqec_cli_def *vqec_get_cli_def (void)
{
    return g_vqec_cli;
}

void vqec_set_cli_def (struct vqec_cli_def *cli)
{
    g_vqec_cli = cli;
}
