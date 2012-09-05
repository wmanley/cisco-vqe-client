/*
 * Copyright (c) 2006-2007 by Cisco Systems, Inc.
 * All rights reserved.
 */

/*
 * this is the temporary file to make test program happy in rtspclient
 * without linking with vqec library.
 */

#define SYSLOG_DEFINITION
#ifndef __VQEC__
#define __VQEC__
#endif
#include "vqec_debug.h"
#undef SYSLOG_DEFINITION

