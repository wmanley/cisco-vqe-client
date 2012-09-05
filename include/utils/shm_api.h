/*
 * Copyright (c) 2007-2011 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifndef __SM_API_H__
#define __SM_API_H__

#include <fcntl.h>

struct sm_seg_hdl;

struct sm_seg_hdl *
sm_seg_instance_get (key_t key, int size, int flags);
int
sm_seg_instance_put (struct sm_seg_hdl *hdl);
int
sm_seg_size (struct sm_seg_hdl *hdl);
int
sm_mutex_lock (struct sm_seg_hdl *hdl) ;
int
sm_mutex_unlock (struct sm_seg_hdl *hdl) ;
int
sm_seg_stat (key_t key);
void *
sm_seg_base (struct sm_seg_hdl *hdl);

#define sptrerr(fmt, args...)	fprintf(stderr, fmt "\n", ## args)

static inline void *
sm_getptr_err (int offset)
{
	sptrerr("invalid offset: (%d)", offset);
	return (NULL);
}

static inline void *
sm_setptr_err (void *ptr)
{
	sptrerr("invalid pointer: (%p)", ptr);
	return (NULL);
}

static inline struct sm_seg_hdl *
sm_seg_instance_create (key_t key, int size) 
{
	return (sm_seg_instance_get(key, size, O_CREAT | O_TRUNC));
}

static inline int
sm_seg_instance_delete (struct sm_seg_hdl *h) 
{
	return (sm_seg_instance_put(h));
}

#define	SMPTR(type) \
	struct { \
		type *sm_ptr; \
	} 
		

#define	SMPTR_MKPTR(sptr, h) ({ \
			(((sptr).sm_ptr) == (void *) 0) ? NULL : \
				(__typeof__ (((sptr).sm_ptr))) \
				((ptrdiff_t)(((sptr).sm_ptr)) + ((ptrdiff_t)(sm_seg_base((h)))) - (ptrdiff_t)1); \
		})

#if defined __x86_64__
#define	SMPTR_GET(sptr, h) ({ \
			(ptrdiff_t)((sptr).sm_ptr) > (ptrdiff_t)sm_seg_size((h)) ? \
				sm_getptr_err((int64_t)((sptr).sm_ptr)) : \
				SMPTR_MKPTR((sptr), (h)); \
		})
#else
#define	SMPTR_GET(sptr, h) ({ \
			(ptrdiff_t)((sptr).sm_ptr) > (ptrdiff_t)sm_seg_size((h)) ? \
				sm_getptr_err((int)((sptr).sm_ptr)) : \
				SMPTR_MKPTR((sptr), (h)); \
		})
#endif

#define	EL(ptr) ((ptr).sm_ptr)

#define	SMPTR_OFFS(ptr, h, __s) ({  \
			if ((ptr) == NULL) { \
				__s.sm_ptr = (void *) 0; \
			} else { \
				if (((ptrdiff_t)((ptr)) >= (ptrdiff_t)sm_seg_base((h)) + (ptrdiff_t)sm_seg_size((h))) || \
				    ((ptrdiff_t)(ptr) < (ptrdiff_t)sm_seg_base((h)))) { \
					__s.sm_ptr = sm_setptr_err((ptr)); \
				} else { \
					__s.sm_ptr = (__typeof__ ((ptr))) ((ptrdiff_t)((ptr)) + (ptrdiff_t)1 - (ptrdiff_t)(sm_seg_base((h)))); \
				} \
			} \
		})

#define	SMPTR_SET(ptr, h, s) ({	 \
			__typeof__ (EL(s)) __el;	\
			__typeof__ ((s)) __s; \
			__s.sm_ptr = NULL; \
			if (__builtin_types_compatible_p(__typeof__ ((__el)),  __typeof((ptr)))) {\
				SMPTR_OFFS((ptr), (h), __s); \
			} else { \
				fprintf(stderr, "incompatible types\n"); \
				assert(0); \
			} \
			__s; \
		})

#endif // __SM_API_H__

