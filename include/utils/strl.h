/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifndef STRL_H
#define STRL_H

#ifndef HAVE_STRLFUNCS

#include "vam_types.h"

size_t strlcat(char *dst, const char *src, size_t siz);
size_t strlcpy(char *dst, const char *src, size_t siz);

#else

/* if there is already a header file with strl{cat,cpy}(), #include it here */

#endif // HAVE_STRLFUNCS

#endif // STRL_H
