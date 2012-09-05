/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Kernel zone manager.
 *
 * Documents: 
 *
 *****************************************************************************/

#include <utils/zone_mgr.h>
#include <utils/queue_plus.h>
#include <linux/version.h>

struct vqe_zone
{
    /**
     * Name of the zone.
     */
    char name[ZONE_MAX_NAME_LEN + 1];
    /**
     * Pointer to the associated slab-cache.
     */
    struct kmem_cache *kcache;
    /**
     * Requested object size in bytes.
     */
    uint32_t req_size;
    /**
     * Adjusted object size used by the slab-cache.
     */
    uint32_t obj_size;
    /**
     * Maximum number of objects that can be allocated:  a cache with
     * without a maximum outstanding element bound is not supported.
     */
    uint32_t max;
    /**
     * Objects currently allocated.
     */
    uint32_t used;
    /**
     * High water mark for allocation.
     */
    uint32_t hiwat;
    /**
     * Failed allocations.
     */
    uint32_t alloc_fail;
    /**
     * Constructor for allocated objects. 
     */
    void (*ctor)(void *data);
    /**
     * Destructor for de-allocated objects.
     */
    void (*dtor)(void *data);   
    /**
     * Zone linked list entry element.
     */
    VQE_SLIST_ENTRY(vqe_zone) list_ptr;
};

static struct kmem_cache *s_zone_kcache;
static char *s_zone_cache_name;
static VQE_SLIST_HEAD(, vqe_zone) s_zone_list;
static uint32_t s_num_alloc_zones;

#define VQE_ZONE_KCACHE_FLAGS SLAB_HWCACHE_ALIGN
#define VQE_ZONE_KCACHE_MINALIGN 8

#define VQE_ZONE_ERROR(format,...) \
    printk(KERN_ERR "<vqe-zone>" format "\n", ##__VA_ARGS__);


/**---------------------------------------------------------------------------
 * Initialize a slab-cache to keep zone pointers.
 *
 * @param[out] boolean Returns true if the creation of the slab-cache
 * succeeds, or if the cache already exists.
 *---------------------------------------------------------------------------*/
#define VQE_ZONE_CACHE_NAME "__vqe_zone_cache"
static boolean
vqe_zone_cache_init (void)
{
    s_zone_cache_name = kmalloc(sizeof(VQE_ZONE_CACHE_NAME) + 1,
                                GFP_KERNEL);
    if (!s_zone_cache_name) {
        return (FALSE);
    }
    strlcpy(s_zone_cache_name, VQE_ZONE_CACHE_NAME,
            sizeof(VQE_ZONE_CACHE_NAME) + 1);
    s_zone_kcache = kmem_cache_create(s_zone_cache_name,
                                      sizeof(struct vqe_zone),
                                      VQE_ZONE_KCACHE_MINALIGN,
                                      VQE_ZONE_KCACHE_FLAGS, 
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)) 
                                      NULL,
#endif
                                      NULL);
    return (s_zone_kcache != NULL);
}


/**---------------------------------------------------------------------------
 * Allocate a new zone pointer from the zone slab-cache.
 *
 * @param[out] zone* Pointer to a zone object.
 *---------------------------------------------------------------------------*/
static struct vqe_zone *
vqe_zone_cache_alloc (void)
{
    struct vqe_zone *z;

    if (!s_num_alloc_zones &&
        !vqe_zone_cache_init()) {
        VQE_ZONE_ERROR("Zone cache initialization failed");
        return (NULL);
    }
    
    ASSERT(s_zone_kcache != NULL);
    z = kmem_cache_alloc(s_zone_kcache, GFP_KERNEL);
    if (z) {
        s_num_alloc_zones++;
        memset(z, 0, sizeof(*z));
        VQE_SLIST_INSERT_HEAD(&s_zone_list, z, list_ptr);
    }

    return (z);
}


/**---------------------------------------------------------------------------
 * Free a zone pointer to the zone slab cache. There must not be any 
 * outstanding allocations from this zone before the object can be destroyed.
 *
 * @param[in] z Pointer to the zone object.
 *---------------------------------------------------------------------------*/
static void
vqe_zone_cache_free (struct vqe_zone *z)
{
    ASSERT(s_zone_kcache != NULL);
    ASSERT(s_zone_cache_name != NULL);
    ASSERT(z != NULL);
    ASSERT(s_num_alloc_zones > 0);

    VQE_SLIST_REMOVE(&s_zone_list, z, vqe_zone, list_ptr);
    kmem_cache_free(s_zone_kcache, z);
    s_num_alloc_zones--;
    if (!s_num_alloc_zones) { 
        kmem_cache_destroy(s_zone_kcache);
        kfree(s_zone_cache_name);
        s_zone_kcache = NULL;
        s_zone_cache_name = NULL;
    }
}


/**---------------------------------------------------------------------------
 * Public interface to allocate a new zone.
 *
 * @param[in] zone_name A unique name for the zone. If a zone with the
 * specified name already exists, the method will return null. This parameter
 * must be non-null.
 * @param[in] flags Used for static or dynamic zones and are ignored in this
 * implementation. All zones are dynamic.
 * @param[in] size The size of elements in the zone.
 * @param[in] max Maximum number of elements that can be allocated from
 * the zone; this parameter must be non-zero.
 * @param[in] ctor A method which is invoked on each object of the zone
 * after allocation; may be specified as null.
 * @param[out] dtor A method which is invoked on each object of the zone
 * prior to de-allocation; may be specified as null.
 * @param[out] zone* Pointer to the allocated zone.
 *---------------------------------------------------------------------------*/
struct vqe_zone *
zone_instance_get_loc (char *zone_name, int flags,
		       int size, int max,
		       void (*ctor)(void *), void (*dtor)(void *))
{
    struct vqe_zone *z = NULL, *zl;
    struct kmem_cache *kcache;

    if (!zone_name || 
        !size || 
        !max ||
        (strnlen(zone_name, ZONE_MAX_NAME_LEN+1) == ZONE_MAX_NAME_LEN+1)) {
        VQE_ZONE_ERROR("Invalid arguments (%s)", __FUNCTION__);
        goto done;
    }

    VQE_SLIST_FOREACH(zl, &s_zone_list, list_ptr) {
        if (!strcmp(zone_name, zl->name)) {
            VQE_ZONE_ERROR("Zone %s already exists", zone_name);
            goto done;
        }
    }

    z = vqe_zone_cache_alloc();
    if (!z) {
        VQE_ZONE_ERROR("Zone cache object allocation failed");
        goto done;
    }

    strlcpy(z->name, zone_name, sizeof(z->name));
    kcache = kmem_cache_create(z->name, 
                               size, 
                               VQE_ZONE_KCACHE_MINALIGN,
                               VQE_ZONE_KCACHE_FLAGS, 
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)) 
                               NULL,
#endif
                               NULL);
    if (!kcache) {
        VQE_ZONE_ERROR("Creation of slab-cache failed");
        goto done;
    }

    z->kcache = kcache;
    z->req_size = size;
    z->obj_size = kmem_cache_size(kcache);
    z->max = max;
    z->ctor = ctor;
    z->dtor = dtor;
    return (z);

  done:
    if (z != NULL) {
        vqe_zone_cache_free(z);
    }
    return (NULL);
}


/**---------------------------------------------------------------------------
 * Public interface to destroy a zone. 
 *
 * @param[in] z Pointer to the zone object.
 * @param[out] int32_t Returns 0 on success, -1 on failure.
 *---------------------------------------------------------------------------*/
int32_t 
zone_instance_put (struct vqe_zone *z)
{
    int32_t rv = 0;

    ASSERT(z != NULL);
    ASSERT(z->kcache != NULL);

    if (z->used) {
        VQE_ZONE_ERROR("Zone \"%s\" has outstanding allocations (%u) "
                       "cannot be destroyed", z->name, z->used);
        rv = -1;
    } else {
        kmem_cache_destroy(z->kcache);
        vqe_zone_cache_free(z);
    } 
    return (rv);
}


/**---------------------------------------------------------------------------
 * Public interface to allocate an object from a zone. 
 *
 * @param[in] z Pointer to the zone object.
 * @param[out] void* Pointer to the allocated object.
 *---------------------------------------------------------------------------*/
void *
zone_acquire (struct vqe_zone *z)
{
    void *data = NULL;

    ASSERT(z != NULL);

    if (unlikely(z->used == z->max)) {
        z->alloc_fail++;
    } else {
        data = kmem_cache_alloc(z->kcache, GFP_ATOMIC);
        if (likely(data)) {
            if (unlikely(z->ctor)) {
                (*z->ctor)(data);
            }
            z->used++;
            if (unlikely(z->used > z->hiwat)) {
                z->hiwat = z->used;
            }
        } else {
            z->alloc_fail++; 
        }
    }

    return (data);
}


/**---------------------------------------------------------------------------
 * Public interface to release an allocated object to it's parent zone. 
 *
 * @param[in] z Pointer to the zone object.
 * @param[in] data Pointer to the allocated object.
 *---------------------------------------------------------------------------*/
void
zone_release (struct vqe_zone *z, void *data)
{
    ASSERT(z != NULL);
    ASSERT(data != NULL);
    
    if (unlikely(z->dtor)) {
        (*z->dtor)(data);
    }
    z->used--;
    ASSERT(z->used >= 0);    
    kmem_cache_free(z->kcache, data);
}


/**---------------------------------------------------------------------------
 * Public interface to get the total static allocated size for all objects in
 * a zone - unsupported for kernel zone.
 *
 * @param[in] el_size Requested object size in bytes.
 * @param[in] num Number of objects to be created.
 * @param[out] int32_t Bytes statically allocated for the zone.
 *---------------------------------------------------------------------------*/
int32_t 
zm_zone_size (int32_t el_size, int32_t num)
{    
    VQE_ZONE_ERROR("Unsupported API");
    return (0);
}


/**---------------------------------------------------------------------------
 * Public interface to get the "actual" size of a zone's objects after HW
 * cache-line alignment / hidden fields.
 *
 * @param[in] z Pointer to the zone.
 * @param[out] int32_t Actual allocated size of the objects in the zone.
 *---------------------------------------------------------------------------*/
int32_t
zm_elem_size (const struct vqe_zone *z)
{
    ASSERT(z != NULL);
    return (z->obj_size);
}


/**---------------------------------------------------------------------------
 * Public interface to retrieve zone statistics.
 *
 * @param[in] z Pointer to the zone.
 * @param[out] zi Zone information pointer.
 * @param[out] Returns 0 on success, -1 on failure.
 *---------------------------------------------------------------------------*/
int32_t
zm_zone_get_info (const struct vqe_zone *z,
                  zone_info_t *zi)
{
    if (!z || !zi) {
        VQE_ZONE_ERROR("Invalid arguments (%s)", __FUNCTION__);
        return (-1);
    }

    zi->max = z->max;
    zi->used = z->used;
    zi->hiwat = z->hiwat;
    zi->alloc_fail = z->alloc_fail;
    return (0);
}


/**---------------------------------------------------------------------------
 * Public interface to shrink the slab-cache associated with a zone.
 * 
 * @param[in] z Pointer to the zone.
 *---------------------------------------------------------------------------*/
void
zone_shrink_cache (struct vqe_zone *z)
{
    ASSERT(z != NULL);    
    kmem_cache_shrink(z->kcache);
}


/**---------------------------------------------------------------------------
 * STUB.
 *---------------------------------------------------------------------------*/
void
zone_ctor_no_zero (void *arg)
{
}

