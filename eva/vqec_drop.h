/*
 * vqec_drop.h 
 * routines for forcing dropped packets.
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef __VQEC_DROP_H__
#define __VQEC_DROP_H__
#include "vqec_pak.h"
#include <sys/types.h>
#include "vqe_port_macros.h"
#include "vqec_error.h"
#include "vqec_dp_api_types.h"
#include "vqec_cli.h"

/* Members and methods for packet dropping */
typedef struct vqec_drop_t { 
    uint32_t drop_packet_interval; 
    uint32_t drop_continuous_packets; 
    boolean drop_packet; 
    uint32_t drop_packet_ratio;
} vqec_drop_t;

void vqec_set_drop_interval(vqec_dp_input_stream_type_t session,
                            uint32_t continuous_drops, uint32_t drop_interval);
void vqec_set_drop_ratio(vqec_dp_input_stream_type_t session,
                         uint32_t drop_ratio);
void vqec_set_drop_enable(vqec_dp_input_stream_type_t session,
                          boolean enable);
void vqec_drop_dump(void);

/* methods for en/diable RCC */
#if HAVE_FCC
boolean vqec_get_rcc(boolean *demo_set);
void vqec_set_rcc (boolean b);

/*
 * vqec_rcc_show()
 *
 * Displays information about rcc.
 *
 * Params:
 *   @param[in]  cli  CLI context on which output should be displayed
 */
void
vqec_rcc_show(struct vqec_cli_def *cli);

boolean vqec_get_fast_fill(boolean *demo_set);
void vqec_set_fast_fill(boolean b);

/*
 * vqec_fast_fill_show()
 *
 * Displays information about fast fill.
 *
 * Params:
 *   @param[in]  cli  CLI context on which output should be displayed
 */
void
vqec_fast_fill_show(struct vqec_cli_def *cli);

#endif /* HAVE_FCC */

#endif /* __VQEC_DROP_H__ */
