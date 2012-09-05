/*------------------------------------------------------------------
 * VQEC Cache Manager Packet
 *
 * April 2006, Josh Gahm
 *
 * This module declares APIs for packet pools and packets.
 *
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __VQEC_PAK_H__
#define __VQEC_PAK_H__

#include "vam_types.h"
#include "rtp.h"
#include "vam_time.h"
#include "zone_mgr.h"

#include "vqec_pak_hdr.h"

/**
 * vqec_pak_alloc_no_particle()
 *
 * Allocates a packet (header only) from the given packet pool.
 * A "packet" here refers to a header and associated buffer.
 *
 * @param[out] vqec_pak_t*  Returns a pointer to the allocated packet header.
 */
vqec_pak_t *
vqec_pak_alloc_no_particle(void);

/**
 * vqec_pak_alloc_with_particle()
 *
 * Allocates a packet from the given packet pool.
 * A "packet" here refers to a header and associated buffer.
 *
 * @param[out] vqec_pak_t*  Returns a pointer to the allocated packet.
 */
static inline vqec_pak_t *
vqec_pak_alloc_with_particle (void) 
{
    vqec_pak_t *pak;

    pak = vqec_pak_alloc_no_particle();

    /* set the pak buffer pointer */
    if (pak) {
        pak->buff = (char *)(pak + 1);
    }

    return (pak);
}

/**
 * Associate a packet buffer with a given packet header.
 *
 * @param[in] pak  Pointer to the packet.
 * @param[in] buff  Pointer to the buffer to be associated with the packet.
 */
boolean vqec_pak_add_buffer(vqec_pak_t *pak, char *buf);

/*
 * Decrease the reference count of a pak and call its free method if
 * there are no refs left to it.
 *
 * @param[in] pak  Pointer to the packet.
 */
static inline void vqec_pak_free (vqec_pak_t *pak) 
{
    if (!pak) {
        return;
    }

    if (--pak->ref_count <= 0) {
        zone_release(pak->zone_ptr, pak);
    }
}

/*
 * Free the skb associated with a packet, if any.
 * In user-mode this is a no-op.
 *
 * @param[in] pak  Pointer to the packet.
 */
static inline void vqec_pak_free_skb (vqec_pak_t *pak)
{
    return;
}

/* 
 * Claim a reference on a pak.
 *
 * @param[in] pak  Pointer to the packet.
 */
static inline void vqec_pak_ref (vqec_pak_t *pak)
{
    if (!pak) {
        return;
    }

    pak->ref_count++;
}

/**
 * Get the length of the packet buffer.
 *
 * @param[in] pak  Pointer to the packet.
 * @param[out] uint32_t  Returns the effective length of the packet buffer.
 */
static inline uint32_t vqec_pak_get_content_len (const vqec_pak_t *pak)
{
    return (pak->buff_len - pak->head_offset);
}

/**
 * Set the length of the packet buffer.
 *
 * @param[in] pak  Pointer to the packet.
 * @param[in] len  Length to set for the packet buffer.
 * @param[out] boolean  Returns TRUE on success; FALSE otherwise.
 */
static inline boolean vqec_pak_set_content_len (vqec_pak_t *pak, uint32_t len)
{
    uint32_t buflen;

    buflen = pak->head_offset + len;

    if (buflen > pak->alloc_len) {
        /* not enough space in pak buffer */
        return (FALSE);
    }

    pak->buff_len = buflen;
    return (TRUE);
}

/**
 * Get the head of the packet buffer.
 *
 * @param[in] pak  Pointer to the packet.
 * @param[out] char *  Returns a pointer to the head of the packet buffer.
 */
static inline char *vqec_pak_get_head_ptr (vqec_pak_t *pak)
{
    return ((char *)pak->buff + pak->head_offset);
}

/**
 * Adjust the pointer of the buffer head within the packet data.
 *
 * @param[in] pak  Pointer to the packet.
 * @param[in] offset  Length of the adjustment to make, in bytes.
 * @param[out] boolean  Returns true if the adjustment succeeds.
 */
static inline boolean vqec_pak_adjust_head_ptr (vqec_pak_t *pak, int16_t offset)
{
#define ABS_VAL(x) (((x) < 0) ? -(x) : (x))

    boolean success = TRUE;
    uint16_t new_offset;

    new_offset = pak->head_offset + offset;

    /* sanity check the resultant head_offset */
    if (!((new_offset < pak->buff_len) && (ABS_VAL(offset) < pak->buff_len))) {
        success = FALSE;
        goto done;
    }

done:
    if (success) {
        pak->head_offset = new_offset;
    }
    return (success);
}

/**
 * Get a pointer to the beginning of the RTP header.
 *
 * @param[in] pak Pointer to the packet.
 * @param[out] rtpfasttype_t *  Returns a pointer to the RTP header.
 */
static inline rtpfasttype_t *vqec_pak_get_rtp_hdr (vqec_pak_t *pak)
{
    return (pak->rtp);
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
int16_t vqec_pak_copy_content(vqec_pak_t *dest_pak, vqec_pak_t *src_pak);

#endif /*  __VQEC_PAK_H__ */
