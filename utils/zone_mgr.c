/*------------------------------------------------------------------
 * Zone manager.
 * Derived from Jonathan Lemon's zone impl.
 *
 * March 2006, Atif Faheem
 *
 * Copyright (c) 2006-2011 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
# include <sys/shm.h>
#include <stddef.h>
#include <pthread.h>

#include "utils/shm_api.h"
#include "utils/queue_plus.h"
#include "utils/queue_shm.h"
#include "utils/vam_debug.h"
#include "utils/zone_mgr.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define zlog(fmt, args...)	 /* fprintf(stderr,fmt "\n", ## args) */
#define zerr(fmt, args...)          /* fprintf(stderr,fmt "\n", ## args) */

#define MSG_INFO(z, fmt, args...) do { \
	zlog("==%s=="fmt, z->name, args);  \
} while (0)

#define MSG_FAIL(z, fmt, args...) do { \
	zerr("==%s=="fmt, z->name, args); \
} while (0)

#define FCN_ENTRY do { \
	zlog("%s", __FUNCTION__);  \
} while (0)

#define strlcpy(dst, src, len) do { \
	strncpy(dst, src, len); \
	dst[len - 1] = '\0'; \
} while (0)

#define strlcat(dst, src, len) do { \
	strncat(dst, src, len); \
	dst[len - 1] = '\0'; \
} while (0)

#define ALIGN_BYTES	8
#define PRIVATE		static
#define PUBLIC

struct zone_item {
	union {
		VQE_SLIST_ENTRY(zone_item) next_l;
		SM_SLIST_ENTRY(zone_item) next_s;
	};
	int			chunk_magic;
	int			refcnt;
	union {
		char			data[0];
	} data_area __attribute__ ((aligned (8)));
};

struct zone_chunk
{
	VQE_SLIST_ENTRY(zone_chunk) next;
};

struct zh_loc 
{
	VQE_SLIST_HEAD(, zone_item)	freelist; 
	VQE_SLIST_HEAD(, zone_chunk)	chunklist;
	struct zone_chunk		chunks;
};

struct zh_sm
{
	SM_SLIST_HEAD(, zone_item)	freelist; 
	SM_SLIST_HEAD(, zone_chunk)	chunklist;	 	
};

struct zones_list
{
	VQE_SLIST_ENTRY(zones_list)	next;
	struct vqe_zone *		z;
	int			refcnt;
	int			id;
	struct zone_oper *		oper;
	void			(*ctor)(void *data);
	void			(*dtor)(void *data);
	struct sm_seg_hdl *		h;
};

struct vqe_zone
{
	int			magic;
	int			type;
	key_t			key;
	int			id;
	int			el_size;
	int			el_size_pad;
	int			max;
	int			flags;
	int			allocated;
	int			used;
	int			hiwat;
	int			alloc_fail;
	char 			name[ZONE_MAX_NAME_LEN];
	struct zones_list		*zl_ptr;
	union {
		struct zh_loc	loc;
		struct zh_sm	sm;
	};
	struct zone_item		item[0] __attribute__ ((aligned (8)));
};

VQE_SLIST_HEAD(, zones_list) g_zonelist;
PRIVATE int g_zid;
PRIVATE int g_zinit;
PRIVATE pthread_mutex_t g_zmmutex = PTHREAD_MUTEX_INITIALIZER;

#define ALIGN(size, align)		(((size) + (align) - 1) & ~((align) - 1))
#define ALIGN_PTR(ptr, align)	(((ptrdiff_t)(ptr) + (align) - 1) & ~((align) - 1))
#define CHUNKMAGIC		0x15A3C78B
#define FREECHUNKMAGIC		0xEF4321CD
#define ZONEMAGIC		0xCAFEFEED
#define ZONEDELETE		0xDEADBEEF

struct zone_fcn 
{
        struct vqe_zone *(*alloc)(char *name, int size, int max, int flags, 
                                  void (*ctor)(void *), void (*dtor)(void *),
                                  key_t key, ptrdiff_t offset);
        void *(*acquire)(struct vqe_zone *z);
        int (*release)(struct vqe_zone *z, void *data);
        int (*destroy)(struct vqe_zone *z);
};

struct zone_oper
{
	int type;
	struct zone_fcn *fcns;
};

struct zone_chunk_dynamic
{
	struct zone_chunk chunk;
	struct zone_item item[0] __attribute__ ((aligned (8)));
};

static struct zone_oper zone_list[ZONE_CLASS_MAX];

PRIVATE int 
zone_item_size (int size)
{
	int s;
	
	s = ALIGN(sizeof(struct zone_item), ALIGN_BYTES); 
	return (ALIGN(size + s, ALIGN_BYTES));
}

PRIVATE void
zone_init (void)
{
	FCN_ENTRY;

	VQE_SLIST_INIT(&g_zonelist);	
}

PRIVATE void
zone_populate_freelist (struct vqe_zone *z, void *data, int cnt)
{
	struct zone_item *zi;
	int i;

	zi = (struct zone_item *)data;
	for (i = 0; i < cnt; i++) {
		zi->chunk_magic = FREECHUNKMAGIC;
		VQE_SLIST_INSERT_HEAD(&z->loc.freelist, zi, next_l);
		MSG_INFO(z, "Added %p to freelist", zi);
#if defined __x86_64__
		zi =  (struct zone_item *) ((int64_t)zi + z->el_size_pad);
#else
		zi =  (struct zone_item *) ((int)zi + z->el_size_pad);
#endif
		z->allocated++;
	}
}

PRIVATE void
zone_populate_sm_freelist (struct vqe_zone *z, struct zones_list *zl, 
			   void *data, int cnt)
{
	struct zone_item *zi;
	int i;

	zi = (struct zone_item *)data;
	for (i = 0; i < cnt; i++) {
		zi->chunk_magic = FREECHUNKMAGIC;
		SM_SLIST_INSERT_HEAD(&z->sm.freelist, zi, next_s, zl->h);
		MSG_INFO(z, "Added %p to freelist", zi);
#if defined __x86_64__
		zi =  (struct zone_item *) ((int64_t)zi + z->el_size_pad);
#else
		zi =  (struct zone_item *) ((int)zi + z->el_size_pad);
#endif
		z->allocated++;
	}
}

PRIVATE void *
zone_create_chunk_dynamic (struct vqe_zone *z, int cnt)
{
	struct zone_chunk_dynamic *zi;
	int alloc_size;
      
	alloc_size = sizeof(*zi);
	alloc_size += zone_item_size(z->el_size) * cnt;
	
	zi = malloc(alloc_size);
	if (zi == NULL)
		return (NULL);
	memset(zi, 0, alloc_size);
	VQE_SLIST_INSERT_HEAD(&z->loc.chunklist, &zi->chunk, next);
	MSG_INFO(z, "Added %p to chunklist", zi); 

	return (&zi->item[0]);
}

PRIVATE struct zones_list *
zm_get_zone (char *zone_name, key_t key) 
{
	struct zones_list *zl;

	FCN_ENTRY;

	VQE_SLIST_FOREACH (zl, &g_zonelist, next) {
            if ((strncmp(zl->z->name, zone_name, strlen(zl->z->name)) == 0) &&
                (key == zl->z->key)) {
                return (zl);
            }
	}

	return (NULL);
}

PRIVATE struct vqe_zone *
zm_loc_alloc (char *zone_name, int size, int max, int flags,
	      void (*ctor)(void *), void (*dtor)(void *), 
	      key_t key, ptrdiff_t offset)
{    
	int alloc_size;
	struct vqe_zone *z;
	struct zones_list *zl;
	void *data;

	FCN_ENTRY;

	if ((zl = zm_get_zone(zone_name, key)) != NULL) {
		zlog("zone %s exists!", zone_name);
		zl->refcnt++;
		return (zl->z);
	}
		
	alloc_size = sizeof(*z);
	alloc_size += zone_item_size(size) * max;

	z = malloc(alloc_size);
	if (z == NULL) {
		zerr("Cannot allocate zone");
		return (NULL);
	}
	zl = malloc(sizeof(*zl));
	if (zl == NULL) {
		zerr("Cannot allocate zone");
		free(z);
		return (NULL);
	}
	memset(z, 0, alloc_size);
	memset(zl, 0, sizeof(*zl));
	strlcpy(z->name, zone_name, sizeof(z->name));	
	z->magic = ZONEMAGIC;
	z->type = ZONE_CLASS_LOCAL;
	z->el_size = size;
	z->el_size_pad = zone_item_size(size);
	z->max = max;
	z->flags = flags;
	zl->ctor = ctor;
	zl->dtor = dtor;
	zl->oper = &zone_list[ZONE_CLASS_LOCAL];
	zl->refcnt++;
	zl->z = z;
	/* for local zones backdoor to find zone metadata  */
	z->zl_ptr = zl;         
	z->id = zl->id = ++g_zid;	
	VQE_SLIST_INSERT_HEAD(&g_zonelist, zl, next);
	
	data =  &z->item[0];
	zone_populate_freelist(z, data, max);

	MSG_INFO(z, "Successfully created %p(%d) %d/%d/%d", 
		 z, alloc_size, z->el_size, z->max, z->allocated);
	return (z);
}

PRIVATE void *
zm_loc_acq (struct vqe_zone *z)
{
	struct zone_item *zi;
	struct zones_list *zl;
	void *data;
	int cnt = 1;

	FCN_ENTRY;

	if (!VQE_SLIST_EMPTY(&z->loc.freelist)) {
		zi = VQE_SLIST_FIRST(&z->loc.freelist);
		VQE_SLIST_REMOVE_HEAD(&z->loc.freelist, next_l);
		goto crit_success;
	} else {
		if (!(z->flags & O_APPEND)) {
			MSG_FAIL(z, "zone is not dynamic (%d)", 
				 z->flags);
			goto crit_fail;
		}
		data = zone_create_chunk_dynamic(z, cnt);
		if (data == NULL) {
			MSG_FAIL(z, "unable to allocate zone chunk (%d)", cnt);
			goto crit_fail;
		}
		zone_populate_freelist(z, data, cnt); 
		zi = VQE_SLIST_FIRST(&z->loc.freelist);
		VQE_SLIST_REMOVE_HEAD(&z->loc.freelist, next_l);
	}
  crit_success:
	z->used++;

	z->hiwat = MAX(z->hiwat, z->used);
	assert(zi->chunk_magic == FREECHUNKMAGIC);
	assert(zi->refcnt == 0);
	zi->chunk_magic = CHUNKMAGIC;
	zi->refcnt++;
	zl = z->zl_ptr;
	assert(zl != NULL);	
	if (zl->ctor)
		zl->ctor(&zi->data_area.data[0]);
        else
            	memset(&zi->data_area.data[0], 0, z->el_size);
	MSG_INFO(z, "Acquired chunk %p/%p (%d/%d)", zi, &zi->data_area.data[0],
		 z->allocated, z->used);
	return ((void *)&zi->data_area.data[0]);

  crit_fail:
	return (NULL);
}

PRIVATE int
zm_loc_rel (struct vqe_zone *z, void *data)
{
	struct zone_item *zi;
	struct zones_list *zl;

	FCN_ENTRY;

	zl = z->zl_ptr;
	assert(zl != NULL);
	if (zl->dtor)
		zl->dtor(data);
	zi = (struct zone_item *) 
#if defined __x86_64__
		((int64_t)data - offsetof(struct zone_item, data_area.data[0]));
#else
		((int)data - offsetof(struct zone_item, data_area.data[0]));
#endif
	assert(zi->chunk_magic == CHUNKMAGIC);
	assert(zi->refcnt == 1);
	zi->chunk_magic = FREECHUNKMAGIC;
	zi->refcnt--;
	VQE_SLIST_INSERT_HEAD(&z->loc.freelist, zi, next_l);
	z->used--;
	MSG_INFO(z, "Released chunk %p/%p", zi, &zi->data_area.data[0]);
	return (0);
}

PRIVATE int
zm_loc_destroy (struct vqe_zone *z)
{
	struct zone_chunk *zc, *zc_n;
	struct zones_list *zl;

	FCN_ENTRY;

	zl = zm_get_zone(z->name, z->key);
	assert(zl != NULL);
	if (--zl->refcnt != 0) {
		MSG_INFO(z, "zone has other references (%d)", 
			 zl->refcnt);
		return (-1);
	}

	if (z->used != 0)
		MSG_FAIL(z, "%d items unfreed in zone", 
			 z->allocated);
	
	VQE_SLIST_FOREACH_SAFE(zc, &z->loc.chunklist, next, zc_n) {
		VQE_SLIST_REMOVE_HEAD(&z->loc.chunklist, next);
		free(zc);
		MSG_INFO(z, "Freed chunk %p", zc);
	}
	VQE_SLIST_REMOVE(&g_zonelist, zl, zones_list, next);
	z->magic = ZONEDELETE;

	free(z);
	free(zl);
	return (0);
}

PRIVATE struct vqe_zone *
zm_sm_alloc (char *zone_name, int size, int max, int flags,
	     void (*ctor)(void *), void (*dtor)(void *), 
	     key_t key, ptrdiff_t offset)
{    
	int alloc_size;
	struct vqe_zone *z;
	struct zones_list *zl;
	struct sm_seg_hdl *hdl;
	void *data;

	FCN_ENTRY;

	hdl = sm_seg_instance_get(key, 0, 0);
	if (hdl == NULL) {
		return (NULL);
	}
	(void)sm_mutex_lock(hdl); 
	if ((zl = zm_get_zone(zone_name, key)) != NULL) {
		zlog("zone %s exists!", zone_name);
		zl->refcnt++;
		(void)sm_mutex_unlock(hdl);
		(void)sm_seg_instance_put(hdl);
		return (zl->z);
	}

	zl = malloc(sizeof(*zl));
	if (zl == NULL) {
		zerr("Cannot allocate zone");
		(void)sm_mutex_unlock(hdl);
		(void)sm_seg_instance_put(hdl);
		return (NULL);
	}

	z = (struct vqe_zone *)((ptrdiff_t)sm_seg_base(hdl) + offset);
	if (flags & O_CREAT) {
		alloc_size = zm_zone_size(size, max);
		memset(z, 0, alloc_size);
		strlcpy(z->name, zone_name, sizeof(z->name)); 
		z->magic = ZONEMAGIC;
		z->type = ZONE_CLASS_SHARED;
		z->el_size = size;
		z->el_size_pad = zone_item_size(size);
		z->max = max;
		z->flags = flags;
		z->id = ++g_zid;	
	} else {
		size = z->el_size;
		max = z->max;
		alloc_size = zm_zone_size(size, max);
	}
	memset(zl, 0, sizeof(*zl));
	zl->ctor = ctor;
	zl->dtor = dtor;
	zl->oper = &zone_list[ZONE_CLASS_SHARED];
	zl->refcnt++;
	zl->z = z;
	zl->id = z->id;
	zl->h = hdl;
	VQE_SLIST_INSERT_HEAD(&g_zonelist, zl, next);

	if (flags & O_CREAT) {	
		data =  &z->item[0];
		zone_populate_sm_freelist(z, zl, data, max);
	}

	(void)sm_mutex_unlock(hdl);
	MSG_INFO(z, "Successfully created %p(%d) %d/%d/%d", 
		 z, alloc_size, z->el_size, z->max, z->allocated);
	return (z);
}

PRIVATE void *
zm_sm_acq (struct vqe_zone *z)
{
	struct zone_item *zi;
	struct zones_list *zl;

	FCN_ENTRY;

	assert(z != NULL);
	zl = zm_get_zone(z->name, z->key);
	assert(zl != NULL);	

	(void)sm_mutex_lock(zl->h);
	if (!SM_SLIST_EMPTY(&zl->z->sm.freelist)) {
		zi = SM_SLIST_FIRST_PTR(&z->sm.freelist, zl->h);
		SM_SLIST_REMOVE_HEAD(&z->sm.freelist, next_s, zl->h);
		goto crit_success;
	} else {
		MSG_FAIL(z, "no more elems! - static (%d)", 
			 z->flags);
		goto crit_fail;
	}

  crit_success:
	z->used++;
	z->hiwat = MAX(z->hiwat, z->used);
	assert(zi->chunk_magic == FREECHUNKMAGIC &&
	       zi->refcnt == 0);
	zi->chunk_magic = CHUNKMAGIC;
	zi->refcnt++;
	memset(&zi->data_area.data[0], 0, z->el_size);
	if (zl->ctor)
		zl->ctor(&zi->data_area.data[0]);
	(void)sm_mutex_unlock(zl->h);
	MSG_INFO(z, "Acquired chunk %p/%p (%d/%d)", zi, &zi->data_area.data[0],
		 z->allocated, z->used);
	return ((void *)&zi->data_area.data[0]);

  crit_fail:
	(void)sm_mutex_unlock(zl->h);
	return (NULL);
}

PRIVATE int
zm_sm_rel (struct vqe_zone *z, void *data)
{
	struct zone_item *zi;
	struct zones_list *zl;

	FCN_ENTRY;

	zl = zm_get_zone(z->name, z->key);
	assert(zl != NULL);

	(void)sm_mutex_lock(zl->h);
	if (zl->dtor)
		zl->dtor(data);
	zi = (struct zone_item *) 
#if defined __x86_64__
		((int64_t)data - offsetof(struct zone_item, data_area.data[0]));
#else
		((int)data - offsetof(struct zone_item, data_area.data[0]));
#endif
	assert(zi->chunk_magic == CHUNKMAGIC &&
	       zi->refcnt == 1);
	zi->chunk_magic = FREECHUNKMAGIC;
	zi->refcnt--;
	SM_SLIST_INSERT_HEAD(&z->sm.freelist, zi, next_s, zl->h);
	z->used--;
	(void)sm_mutex_unlock(zl->h);

	MSG_INFO(z, "Released chunk %p/%p", zi, &zi->data_area.data[0]);
	return (0);
}

PRIVATE int
zm_sm_destroy (struct vqe_zone *z)
{
	struct zones_list *zl;
	FCN_ENTRY;

	zl = zm_get_zone(z->name, z->key);
	assert(zl != NULL);

	(void)sm_mutex_lock(zl->h);
	if (--zl->refcnt != 0) {
		MSG_INFO(z, "zone has other references (%d)", 
			 zl->refcnt);
		sm_mutex_unlock(zl->h);		
		return (0);
	}

	if (z->used != 0)
		MSG_FAIL(z, "%d items unfreed in zone", 
			 z->allocated);
	
	VQE_SLIST_REMOVE(&g_zonelist, zl, zones_list, next);
	z->magic = ZONEDELETE;
	(void)sm_mutex_unlock(zl->h);
	(void)sm_seg_instance_put(zl->h);
	
	free(zl);
	return (0);
}

PUBLIC int 
zm_zone_size (int el_size, int num)
{

	FCN_ENTRY;
	
	if (g_zinit++ == 0) 
		zone_init();
	assert(num != 0 && el_size != 0);
	return ((zone_item_size(el_size) * num) + ALIGN_BYTES + 
		sizeof(struct vqe_zone));
}

PUBLIC int 
zm_elem_size (const struct vqe_zone *z)
{

	FCN_ENTRY;

	assert(z->magic == ZONEMAGIC);
	assert(g_zinit != 0);
	return (z->el_size);
}

PUBLIC struct vqe_zone *
zone_instance_get_loc (char *zone_name, int flags,
		       int size, int max,
		       void (*ctor)(void *), void (*dtor)(void *))
{
	struct vqe_zone *z;

	FCN_ENTRY;

	assert(size != 0 || (size == 0 && flags == 0));
	assert(max != 0 || (max == 0 && (flags & O_APPEND) == 0));
	if (g_zinit++ == 0) 
		zone_init();
	pthread_mutex_lock(&g_zmmutex);
	z = (*zone_list[ZONE_CLASS_LOCAL].fcns->alloc)(zone_name, size, max, 
						       flags, ctor, dtor, 0, 0);
	pthread_mutex_unlock(&g_zmmutex);
	return (z);
}

PUBLIC struct vqe_zone *
zone_instance_get_sm (char *zone_name, key_t key, 
		      ptrdiff_t offset, int flags,
		      int size, int max, int seg_size,
		      void (*ctor)(void *), void (*dtor)(void *))
{
	struct vqe_zone *z;

	FCN_ENTRY;

	assert(size != 0 || (size == 0 && flags == 0));
	assert(max != 0 || (max == 0 && flags == 0));
	assert(key != 0);
  	assert((size == 0 && max == 0 && flags == 0) || 
	       (seg_size == zm_zone_size(size, max)));
	if (g_zinit++ == 0) 
		zone_init();
	pthread_mutex_lock(&g_zmmutex);
	z = (*zone_list[ZONE_CLASS_SHARED].fcns->alloc)(zone_name, size, max, 
							flags, ctor, dtor, key, offset);
	pthread_mutex_unlock(&g_zmmutex);
	return (z);
}

PUBLIC int 
zone_instance_put (struct vqe_zone *z) 
{
	int rv;

	FCN_ENTRY;

	assert(z->magic == ZONEMAGIC);
	assert(z->type == ZONE_CLASS_LOCAL || 
	       z->type == ZONE_CLASS_SHARED);
	assert(g_zinit != 0);

	pthread_mutex_lock(&g_zmmutex);
	rv = (*zone_list[z->type].fcns->destroy)(z);
	pthread_mutex_unlock(&g_zmmutex);
	
	return (rv);
}

PUBLIC void *
zone_acquire (struct vqe_zone *z)
{
	void *ptr;

	FCN_ENTRY;
        if (!z) {
            return NULL;
        }
	
	assert(z->magic == ZONEMAGIC);
	assert(z->type == ZONE_CLASS_LOCAL || 
	       z->type == ZONE_CLASS_SHARED);
	assert(g_zinit != 0);

	pthread_mutex_lock(&g_zmmutex);
	ptr = (*zone_list[z->type].fcns->acquire)(z);
        if (!ptr) {
            z->alloc_fail++;
        }
	pthread_mutex_unlock(&g_zmmutex);
	
	return (ptr);
}

PUBLIC void
zone_release (struct vqe_zone *z, void *data)
{
	int rv;

	FCN_ENTRY;
	
	assert(z->magic == ZONEMAGIC);
	assert(z->type == ZONE_CLASS_LOCAL || 
	       z->type == ZONE_CLASS_SHARED);
	assert(g_zinit != 0);

	pthread_mutex_lock(&g_zmmutex);
	rv = (*zone_list[z->type].fcns->release)(z, data);
	pthread_mutex_unlock(&g_zmmutex);
	
}

PUBLIC void 
zone_ctor_no_zero(void *arg) {}

static struct zone_fcn zone_loc_fcn =
{
	alloc:	zm_loc_alloc,
	acquire:	zm_loc_acq,
	release:	zm_loc_rel,
	destroy:	zm_loc_destroy
};

static struct zone_fcn zone_shm_fcn =
{
	alloc:	zm_sm_alloc,
	acquire:	zm_sm_acq,
	release:	zm_sm_rel,
	destroy:	zm_sm_destroy
};

static struct zone_oper zone_list[ZONE_CLASS_MAX] =
{ 
	{ 
		ZONE_CLASS_LOCAL, 
		&zone_loc_fcn
	},
	{ 
		ZONE_CLASS_SHARED, 
		&zone_shm_fcn 
	}
};

int
zm_zone_get_info (const struct vqe_zone *z,
                  zone_info_t *zi)
{
    int ret = -1;
    pthread_mutex_lock(&g_zmmutex);
    if (z && zi) {
        zi->max = z->max;
        zi->used = z->used;
        zi->hiwat = z->hiwat;
        zi->alloc_fail = z->alloc_fail;
        ret = 0;
    }
    pthread_mutex_unlock(&g_zmmutex);    
    return ret;
}
