/*------------------------------------------------------------------
 * Zone manager.
 * Derived from Jonathan Lemon's zone impl.
 *
 * March 2006, Atif Faheem
 *
 * Copyright (c) 2006-2008, 2012 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef ZONE_MGR_H
#define ZONE_MGR_H

#include "vam_types.h"

#define ZONE_CLASS_LOCAL	0
#define ZONE_CLASS_SHARED	1
#define ZONE_CLASS_MAX		ZONE_CLASS_SHARED + 1

#define ZONE_FLAGS_STATIC	0
#define ZONE_FLAGS_DYNAMIC	1

#define ZONE_MAX_NAME_LEN (32)

struct vqe_zone;

struct vqe_zone *
zone_instance_get_loc (char *zone_name, int flags,
		       int size, int max,
		       void (*ctor)(void *), void (*dtor)(void *));
struct vqe_zone *
zone_instance_get_sm (char *zone_name, key_t key, 
		      ptrdiff_t offset, int flags,
		      int size, int max, int seg_size,
		      void (*ctor)(void *), void (*dtor)(void *));
int 
zone_instance_put (struct vqe_zone *z);
void *
zone_acquire (struct vqe_zone *z);
void
zone_release (struct vqe_zone *z, void *data);
int 
zm_zone_size (int el_size, int num);
int 
zm_elem_size (const struct vqe_zone *z);

void 
zone_ctor_no_zero(void *arg);

static inline struct vqe_zone *
zone_alloc (char *zone_name, int type, int flags,
	    int size, int max, int proj_id,
	    void (*ctor)(void *), void (*dtor)(void *)) 
{
	int n_flags = O_CREAT;
	if (type == ZONE_CLASS_LOCAL) {
		VQE_FPRINTF(stderr, 
			"This API is deprecated; replace with "
			"zone_instance_get_loc()\n");
		if (flags == ZONE_FLAGS_DYNAMIC)
			n_flags |= O_APPEND;
		return (zone_instance_get_loc(zone_name, n_flags, 
					      size, max, ctor, dtor));
	} else {
		VQE_FPRINTF(stderr, 
			"Use zone_instance_get_sm() API ...\n");
		return (NULL);
	}
}

static inline void
zone_destroy (struct vqe_zone *z) 
{
	VQE_FPRINTF(stderr, 
		"This API is deprecated; replace with "
		"zone_instance_put()\n");
	zone_instance_put(z);
}
typedef struct zone_info_ {
    int max;   /* Max entries in the zone */
    int used;  /* Num used entries */
    int hiwat; /* High water mark used entries */
    int alloc_fail; /* num_failed_allocs */
} zone_info_t;

int 
zm_zone_get_info(const struct vqe_zone *z,
                 zone_info_t *zi);

void
zone_shrink_cache(struct vqe_zone *z);

#endif // ZONE_MGR_H
