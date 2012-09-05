/*------------------------------------------------------------------
 * VQE-C Packet
 *
 * April 2006, Josh Gahm
 *
 * This module defines APIs for packet pools and packets.
 *
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

/* included for unit testing purposes */
#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif

#include "vqec_pak.h"
#include "vqec_syslog_def.h"
#include "zone_mgr.h"


/************************************************************************
 * Packet pools support
 */

typedef struct vqe_zone  vqec_pak_pool_t;
vqec_pak_pool_t *s_pak_pool = NULL;
uint32_t s_pak_pool_elem_buff_size;

/**
 * vqec_pak_pool_get_elem_size()
 *
 * Returns the size of the buffer element associated with a given packet.
 *
 * @param[out]  uint32_t  Size of the buffer.
 */
uint32_t vqec_pak_pool_get_elem_size (void)
{
    return (s_pak_pool_elem_buff_size);
}

/**
 * vqec_pak_pool_create()
 *
 * Creates a pool of packets, and returns an ID for use in future requests
 * for allocating packets from pool.
 *
 * NOTE:   Currently, a maximum of one pak pool is supported.
 *         This can be extended as needed in the future.
 *
 * @param[in] name       Descriptive name of pool (for use in displays)
 * @param[in] buff_size  Size of packet's buffer (must hold RTP header 
 *                        + MPEG payload)
 * @param[in] max_buffs  Number of packets to create in pool
 */
void
vqec_pak_pool_create (char *name,
                      uint32_t buff_size,
                      uint32_t max_buffs)
{
    uint32_t elem_size;

    if (s_pak_pool) {
        /*
         * If a pool exists, we have hit the current limit of one pool.
         */
        return;
    }    

#ifdef __KERNEL__
    elem_size = sizeof(vqec_pak_t);
#else
    elem_size = sizeof(vqec_pak_t) + buff_size;
#endif

    s_pak_pool = zone_instance_get_loc(name,
                                       ZONE_FLAGS_STATIC,
                                       elem_size,
                                       max_buffs,
                                       zone_ctor_no_zero, NULL);
    if (!s_pak_pool) {
        return;
    }

    s_pak_pool_elem_buff_size = buff_size;
}

/**
 * vqec_pak_pool_destroy()
 *
 * Frees the packet pool.
 */
void
vqec_pak_pool_destroy (void)
{
    /* Nothing to do if pool is not allocated */
    if (!s_pak_pool) {
        goto done;
    }
    if (zone_instance_put(s_pak_pool) != 0) {
        syslog_print(VQEC_ERROR, "pak pool destroy failed\n");
    }
    s_pak_pool = NULL;
    s_pak_pool_elem_buff_size = 0;
done:
    return;
}

/**
 * vqec_pak_pool_get_status()
 *
 * Retrieves information about a packet pool.
 *
 * @param[out] status          Information about requested pool.
 * @param[out] vqec_pak_pool_err_t VQEC_PAK_POOL_ERR_OK upon success, or 
 *                                  failure code otherwise
 */
vqec_pak_pool_err_t
vqec_pak_pool_get_status (vqec_pak_pool_status_t *status)
{
    zone_info_t zi;
    struct vqe_zone *z;
    int z_ret;
    vqec_pak_pool_err_t err = VQEC_PAK_POOL_ERR_OK;

    /* Validate arguments */
    if (!status) {
        err = VQEC_PAK_POOL_ERR_INVALIDARGS;
        goto done;
    }
    if (!s_pak_pool) {
        err = VQEC_PAK_POOL_ERR_INTERNAL;
        goto done;
    }

    /* Attempt to retrieve status from the zone manager */
    z = (struct vqe_zone *)s_pak_pool;
    z_ret = zm_zone_get_info(z, &zi);
    if (z_ret == -1) {
        err = VQEC_PAK_POOL_ERR_INTERNAL;
        goto done;
    }

    /* Load retrieved data */
    memset(status, 0, sizeof(vqec_pak_pool_status_t));
    status->max = zi.max;
    status->used = zi.used;
    status->hiwat = zi.hiwat;
    status->alloc_fail = zi.alloc_fail;

done:
    return (err);
}

/************************************************************************
 * Packet support
 */

static inline vqec_pak_t *vqec_pak_alloc_internal (void)
{
    vqec_pak_t *pak = NULL;

    if (!s_pak_pool) {
        goto done;
    }

    pak = zone_acquire(s_pak_pool);
    if (!pak) {
        goto done;
    }

    /* Clear the pak metadata but not the buffer */
    memset(pak, 0, sizeof(vqec_pak_t));

    pak->ref_count = 1;
    pak->zone_ptr = s_pak_pool;
    *((int32_t *)&pak->alloc_len) =
        zm_elem_size(s_pak_pool) - sizeof(vqec_pak_t);

done:
    return pak;
}

/**
 * vqec_pak_alloc_no_particle()
 *
 * Allocates a packet (header only) from the given packet pool.
 * A "packet" here refers to a header and associated buffer.
 *
 * @param[in] id  ID of packet pool from which pak is to be allocated
 * @param[out] vqec_pak_t*  Returns a pointer to the allocated packet header.
 */
vqec_pak_t *
vqec_pak_alloc_no_particle (void)
{
    vqec_pak_t *pak;

    pak = vqec_pak_alloc_internal();

    return (pak);
}

/**
 * Associate a packet buffer with a given packet header.
 *
 * @param[in] pak  Pointer to the packet.
 * @param[in] buff  Pointer to the buffer to be associated with the packet.
 */
boolean vqec_pak_add_buffer (vqec_pak_t *pak, char *buf)
{
    if (!pak || pak->buff || !buf) {
        return FALSE;
    }

    pak->buff = buf;
    pak->head_offset = 0;

    return TRUE;
}

/**
 * Copy a VQE-C packet's contents. The destination packet must have
 * a data buffer that's greater than or equal in size to the source's
 * data buffer. No metadata is copied.
 *
 * @param[in] dest_pak  Destination packet.
 * @param[in] src_pak   Source packet.
 * @param[out] int16_t  Returns the length of the packet buffer copied; or
 *                      (-1) on error.
 */
int16_t vqec_pak_copy_content (vqec_pak_t *dest_pak, vqec_pak_t *src_pak)
{
    if (!src_pak || (dest_pak->alloc_len < src_pak->buff_len)) {
        return (-1);
    }

    if (dest_pak->buff && src_pak->buff) {
        memcpy(dest_pak->buff, src_pak->buff, dest_pak->buff_len);
    } else if (src_pak->buff) {
        /* source has buff ptr but dest does not */
        return (-1);
    }

    return (dest_pak->buff_len);
}

