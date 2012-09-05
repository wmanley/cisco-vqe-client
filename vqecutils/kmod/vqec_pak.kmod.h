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
#include <linux/skbuff.h>
#include "linux/udp.h"
#include "linux/ip.h"
#include <linux/version.h>

#include "vqec_pak_hdr.h"

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
        /* free the associated skbuff particle */
        if (pak->skb) {
            if (pak->sk) {
                lock_sock((struct sock *)pak->sk);
                skb_free_datagram((struct sock *)pak->sk,
                                  (struct sk_buff *)pak->skb);
                release_sock((struct sock *)pak->sk);
                pak->sk = NULL;
            } else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
                kfree_skb((struct sk_buff *)pak->skb);
#else  /* kernels 2.6.30 and newer */
                consume_skb((struct sk_buff *)pak->skb);
#endif  /* LINUX_VERSION_CODE */
            }
            pak->skb = NULL;
        }

        /* free the rest of the pak structure */
        zone_release(pak->zone_ptr, pak);
    }
}

/*
 * Free the skb associated with a packet, if any.
 *
 * @param[in] pak  Pointer to the packet.
 */
static inline void vqec_pak_free_skb (vqec_pak_t *pak)
{
    if (!pak || !pak->skb) {
        return;
    }

    /* free the associated skbuff particle */
    if (pak->sk) {
        lock_sock((struct sock *)pak->sk);
        skb_free_datagram((struct sock *)pak->sk,
                          (struct sk_buff *)pak->skb);
        release_sock((struct sock *)pak->sk);
        pak->sk = NULL;
    } else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
        kfree_skb((struct sk_buff *)pak->skb);
#else  /* kernels 2.6.30 and newer */
        consume_skb((struct sk_buff *)pak->skb);
#endif  /* LINUX_VERSION_CODE */
    }
    pak->skb = NULL;
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
    struct sk_buff *skb;

/* 
 * SKB_USER_LEN is the length of the data excluding the UDP header, as
 * currently, the UDP header is considered part of the packet buffer by the
 * skb.
 */
#define SKB_USER_LEN (skb->len - sizeof(struct udphdr))

    buflen = pak->head_offset + len;

    if (buflen > pak->alloc_len) {
        /* not enough space in pak buffer */
        return (FALSE);
    }

    /* the code below simply adjusts the size of the skb buffer */
    skb = (struct sk_buff *)pak->skb;
    if (skb) {
        if (buflen < SKB_USER_LEN) {
            skb_trim(skb, (buflen + sizeof(struct udphdr)));
        } else if (buflen > SKB_USER_LEN) {
            if (buflen > (SKB_USER_LEN + skb_tailroom(skb))) {
                /* not enough space in skb buffer */
                return (FALSE);
            }
            skb_put(skb, buflen - SKB_USER_LEN);
        }
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
static inline boolean vqec_pak_adjust_head_ptr (vqec_pak_t *pak,
                                                int16_t offset)
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

    /* adjust the attached skb if there is one */
    if (pak->skb) {
        if (offset > 0 && !pskb_pull(pak->skb, offset)) {
            success = FALSE;
            goto done;
        }

        if (offset < 0 && !skb_push(pak->skb, -offset)) {
            success = FALSE;
            goto done;
        }
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
 * Allocate a new skbuff.  This new skbuff will have complete but NULL IP and
 * UDP header information.  The data pointer will be set at the beginning of
 * the UDP header, so as to recreate the setup of a live skbuff as it would be
 * received from a socket.
 *
 * @param[in] size  Size of the packet buffer to be allocated for the skbuff.
 * @param[out]  struct sk_buff  Returns the pointer to the allocated skbuff, or
 *                              NULL if unsuccessful.
 */
static inline struct sk_buff *vqec_pak_alloc_skb (uint size)
{
    struct sk_buff *skb;
    struct udphdr *uh;
    struct iphdr *iph;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
#define IPHDR_PTR_RAW (skb->nh.raw)
#define IPHDR_PTR (skb->nh.iph)
#define UDPHDR_PTR_RAW (skb->h.raw)
#define UDPHDR_PTR (skb->h.uh)
#else
#define IPHDR_PTR_RAW (skb->network_header)
#define IPHDR_PTR ((struct iphdr *)skb->network_header)
#define UDPHDR_PTR_RAW (skb->transport_header)
#define UDPHDR_PTR ((struct udphdr *)skb->transport_header)
#endif  /* LINUX_VERSION_CODE */

    /*
     * It is required that sk_buff's have valid IP and UDP headers before they
     * are accepted into a UDP socket queue.  These headers are constructed and
     * added to the sk_buff's buffer such that the following pointers are
     * aligned as follows in relation to the beginning of the originally
     * allocated buffer:
     *
     *  IPHDR_PTR           = 0
     *  UDPHDR_PTR          = 20  (when IP header length = 20 bytes)
     *  data                = 20
     *  (user added data)   = 28  (when UDP header length = 8 bytes)
     */

    /* alloc an empty skb */
    skb = alloc_skb(size + sizeof(struct udphdr) + sizeof(struct iphdr),
                    GFP_KERNEL);
    if (!skb) {
        return (NULL);
    }

    /* construct and put an IP header onto the skb */
    IPHDR_PTR_RAW = skb_put(skb, sizeof(struct iphdr));
    iph = IPHDR_PTR;
    iph->version = 4;
    iph->ihl = 5;
    iph->tot_len = htons(skb->len);
    iph->frag_off = 0;
    iph->protocol = IPPROTO_UDP;

    /* construct and put a UDP header onto the skb */
    UDPHDR_PTR_RAW = skb_put(skb, sizeof(struct udphdr));
    uh = UDPHDR_PTR;
    uh->len = htons(size);
    uh->check = 0;

    /* pull IP header since it's not part of the data */
    skb_pull(skb, sizeof(struct iphdr));  /* moves data ptr to correct pos */

    /* create space for packet data to reside */
    skb_put(skb, size); 

    return (skb);
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

/**
 * vqec_pak_pool_get_elem_size()
 *
 * Returns the size of the buffer element associated with a given packet.
 *
 * @param[out]  uint32_t  Size of the buffer.
 */
uint32_t vqec_pak_pool_get_elem_size(void);

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
    if (pak) {

        /* allocate an skb particle to go with this pak */
        pak->skb = (void *)vqec_pak_alloc_skb(vqec_pak_pool_get_elem_size());
        if (!pak->skb) {
            goto bail;
        }

        /* update the pak->alloc_len to reflect newly allocated sk_buff */
        *((uint32_t *)&pak->alloc_len) = vqec_pak_pool_get_elem_size();

        /* set the buff pointer to point to the particle's buffer */
        pak->buff = ((struct sk_buff *)pak->skb)->data + sizeof(struct udphdr);
    }

    return (pak);

bail:
    vqec_pak_free(pak);
    return (NULL);
}

#endif /*  __VQEC_PAK_H__ */
