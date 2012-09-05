//------------------------------------------------------------------
// VQEC. Assert macros.
//
//
// Copyright (c) 2007 by cisco Systems, Inc.
// All rights reserved.
//------------------------------------------------------------------

#ifndef __VQEC_ASSERT_MACROS_H__
#define __VQEC_ASSERT_MACROS_H__

#include <assert.h>

#define VQEC_ASSERT(expr)                               \
    assert((expr))

#define VQEC_FATAL_ERROR(fmtstr, args...)                       \
    fprintf(stderr, "FATAL @ %s:%d\n", __FUNCTION__, __LINE__); \
    fprintf(stderr, fmtstr, ##args);                            \
    abort();

#endif // __VQEC_ASSERT_MACROS__
