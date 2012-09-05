/*------------------------------------------------------------------
 * VAM Packet Abstraction
 *
 * April 2006, Josh Gahm
 *
 * Copyright (c) 2006 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */


#ifndef __VAM_PAK_H__
#define __VAM_PAK_H__


#include <sys/socket.h>
#include <netinet/in.h>

#include "vam_types.h"
#include "zone_mgr.h"


/*****************************************************************
 * vam_pak class
 *
 * Abstract class for representing a packet.  Note: currently not
 * used except in CM and should be moved there?
 *
 * Includes abstract interface for getting the buffer associated with
 * a packet, getting the length of the packet, freeing the packet.
 * There is also a reference count so that a packet is actually freed
 * back to its heap (by a call to free_me) only when there are no refs
 * left. 
 */
#define __vam_pak_fcns \
    void (*free_me)(void * the_packet_to_free, void * free_arg); \
    /* get_buffer.  Retrieve the packet's buffer as a scatter decriptor */ \
    /* start and len allow a sub-segment of the buffer to be found.  If *len */ \
    /* is 0 then get the whole thing. *len returns actual length returned. */ \
    void (*get_buffer)(const struct vam_pak_ * pak, uint32_t * num_desc, \
           vam_scatter_desc_t * descs, uint32_t start, uint32_t *len); \
    uint32_t (*get_length)(const struct vam_pak_ * pak); 

#define __vam_pak_members \
    int32_t ref_count __attribute__ ((aligned (8))); \
    void * free_arg;       


DEFCLASS(vam_pak)

/*     
 * vam_pak_free
 *
 * Decrease the reference count of a pak and call its free method if
 * there are no refs left to it.
 */
static inline void vam_pak_free(vam_pak_t * pak) 
{
    if (! pak)
        return;

    if (--pak->ref_count <= 0)
        MCALL(pak, free_me, pak->free_arg);
}

/*
 * vam_pak_ref
 * 
 * Claim a reference on a pak.
 */
static inline void vam_pak_ref(vam_pak_t * pak)
{
    if (! pak)
        return;

    pak->ref_count++;
}

#endif /* __VAM_PAK_H__ */
