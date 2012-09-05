/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef VQEC_ELOG_H
#define VQEC_ELOG_H

#include <utils/vam_types.h>

void vqec_monitor_elog_set(boolean);
boolean vqec_monitor_elog_get(void);

void vqec_monitor_elog_reset(void);
void vqec_monitor_elog_dump(char *);

#endif /* VQEC_ELOG_H */
