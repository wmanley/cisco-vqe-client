/*------------------------------------------------------------------
 * VQE-C Packet Structure
 *
 * This module defines the structure for packet headers.
 *
 * Copyright (c) 2006-2009 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __VQEC_PAK_HDR_H__
#define __VQEC_PAK_HDR_H__

#include "vqec_seq_num.h"
#include "queue_plus.h"

typedef struct vqec_pak_ {
    struct vqe_zone *zone_ptr;
    const uint32_t alloc_len;
    int32_t ref_count;
    rtpfasttype_t *rtp;

    uint32_t buff_len;
    struct in_addr src_addr;  /* network byte-order */
    struct in_addr dst_addr;  /* network byte-order */
    uint16_t  src_port;  /* network byte-order */
    uint16_t  dst_port;  /* network byte-order */

    /* list-components to put the packet on an "inorder" list in pak_seq */
    VQE_TAILQ_ENTRY(vqec_pak_) inorder_obj;
    /* list-components to hold TS packet from decoded APP packet */
    VQE_TAILQ_ENTRY(vqec_pak_) ts_pkts;
    vqec_seq_num_t seq_num;
    uint32_t mpeg_payload_offset;
    uint16_t type;  /* packet type */
    uint16_t flags;  /* packet flags */

    uint32_t rtp_ts;  /* rtp timestamp */
    /* system time at which pkt was received */
    abs_time_t rcv_ts;
    abs_time_t pred_ts;  /* "predicted" receive timestamp */
    rel_time_t app_cpy_delay;  /** 
                                * packet-specific delay, resultant from APP
                                * replication
                                */
    uint16_t head_offset;       /* used to compute the head ptr of buff */
    uint16_t fec_touched;       /**
                                 * there are two usages of this flag
                                 * for FEC packets, it means how many times the
                                 * fec pak is touched in 2-D iteration.
                                 * for fec corrected packet, it will be set to
                                 * a value shows the packet is corrected by fec
                                 * when inserted to PCM
                                 */

    struct vqec_fec_hdr_  *fec_hdr;
    void *skb;  /* this is used in kernel-mode for the skbuff */
    void *sk;  /* this is used in kernel-mode for the kernel socket */
    char *buff; 

} vqec_pak_t;

/************************************************************************
 * Packet pools support
 */

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
vqec_pak_pool_create(char *name,
                     uint32_t buff_size,
                     uint32_t max_buffs);

/**
 * vqec_pak_pool_destroy()
 *
 * Frees the specified packet pool.
 */
void
vqec_pak_pool_destroy(void);

/*
 * Return codes for pak pool APIs
 */
typedef enum vqec_pak_pool_err_ {
    VQEC_PAK_POOL_ERR_OK,
    VQEC_PAK_POOL_ERR_NOTFOUND,
    VQEC_PAK_POOL_ERR_INVALIDARGS,
    VQEC_PAK_POOL_ERR_INTERNAL,
} vqec_pak_pool_err_t;

/*
 * Status information for a packet pool.
 */
typedef struct vqec_pak_pool_status_t_ {
    int max;        /* Max packets in the pool */
    int used;       /* Num allocated packets */
    int hiwat;      /* High water mark for allocated packets */
    int alloc_fail; /* Number of failed packet allocations */
} vqec_pak_pool_status_t;

/**
 * vqec_pak_pool_get_status()
 *
 * Retrieves information about a packet pool.
 *
 * @param[in]  id              Pool whose information is requestd
 * @param[out] status          Information about requested pool.
 * @param[out] vqec_pak_pool_err_t VQEC_PAK_POOL_ERR_OK upon success, or 
 *                                  failure code otherwise
 */
vqec_pak_pool_err_t
vqec_pak_pool_get_status(vqec_pak_pool_status_t *status);


/************************************************************************
 * Packet flags support
 */

typedef enum vqec_pak_flags_pos_
{
    VQEC_PAK_FLAGS_RX_REORDERED = 0,
    VQEC_PAK_FLAGS_RX_DISCONTINUITY,
    VQEC_PAK_FLAGS_ON_INORDER_Q,
    VQEC_PAK_FLAGS_AFTER_EC,
} vqec_pak_flags_pos_t;

enum
{
    VQEC_PAK_TYPE_APP = 0,
    VQEC_PAK_TYPE_PRIMARY,
    VQEC_PAK_TYPE_REPAIR,
    VQEC_PAK_TYPE_FEC,  /* added for use by FEC */
    VQEC_PAK_TYPE_UDP,
};

#define VQEC_PAK_FLAGS_MASK(pos)      (1 << (pos))
#define VQEC_PAK_FLAGS_ISSET(fl, pos) ((*(fl)) & (1 << (pos)))
#define VQEC_PAK_FLAGS_SET(fl, pos)   ((*(fl)) |= (1 << (pos)))
#define VQEC_PAK_FLAGS_RESET(fl, pos) ((*(fl)) &= ~(1 << (pos)))

#endif /*  __VQEC_PAK_HDR_H__ */
