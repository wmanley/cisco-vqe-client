/**-----------------------------------------------------------------
 * @brief
 * VQE Feedback target address utility functions
 *
 * @file
 * vqes_fbt_addr.h
 *
 * June 2007, Anne McCormick
 * July 2008, Donghai Ma (moved out of CM(DP))
 * 
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __VQES_FBT_ADDR_H__
#define __VQES_FBT_ADDR_H__

#include <stdint.h>
#include <netinet/in.h>
#include <linux/rtnetlink.h>

/* FBT address utility counters */
typedef struct vqes_fbt_cnts_ {
    /* FBT address added */
    uint64_t fbt_nl_add;
    
    /* FBT address add failed */
    uint64_t fbt_nl_add_failed;

    /* FBT address deleted */
    uint64_t fbt_nl_del;

    /* FBT address delete failed */
    uint64_t fbt_nl_del_failed;

    /* FBT addresses flushed */
    uint64_t fbt_nl_flush;

    /* FBT address flush failed */
    uint64_t fbt_nl_flush_failed;

    /* FBT address status retrieved */
    uint64_t fbt_nl_status;

    /* FBT address status failed */
    uint64_t fbt_nl_status_failed;

    /* FBT netlink send tmo reset failed */
    uint64_t fbt_nl_send_tmo_reset_failed;

    /* FBT netlink receive tmo reset failed */
    uint64_t fbt_nl_recv_tmo_reset_failed;

    /* FBT netlink send failed - timeout */
    uint64_t fbt_nl_send_failed_tmo;

    /* FBT netlink send failed - other */
    uint64_t fbt_nl_send_failed_other;

    /* FBT netlink receive failed - timeout */
    uint64_t fbt_nl_recv_failed_tmo;

    /* FBT netlink receive failed - other */
    uint64_t fbt_nl_recv_failed_other;

    /* FBT netlink received invalid header */
    uint64_t fbt_nl_recv_invalid_hdr;

    /* FBT netlink received invalid add message */
    uint64_t fbt_nl_recv_invalid_add_msg;

    /* FBT netlink duplicate add request */
    uint64_t fbt_nl_dup_add;

    /* FBT netlink received invalid delete message */
    uint64_t fbt_nl_recv_invalid_del_msg;

    /* FBT netlink duplicate delete request */
    uint64_t fbt_nl_dup_del;

    /* FBT netlink received lower sequence number */
    uint64_t fbt_nl_recv_lower_seq;

    /* FBT netlink received higher sequence number */
    uint64_t fbt_nl_recv_higher_seq;

    /* FBT netlink request retries */
    uint64_t fbt_nl_req_retries;

    /* FBT netlink request is out of retries */
    uint64_t fbt_nl_req_no_more_retries;
} vqes_fbt_cnts_t;

/* Retrieve the counters */
extern void 
vqes_fbt_get_cnts(vqes_fbt_cnts_t *cnts);



/* */
/**
 * vqes_fbt_status_t
 * Status of an Feedback Target Address
 */
typedef enum vqes_fbt_status_
{
    /**@brief
     * Address not present */
    VQE_FBT_ADDR_MISSING,

    /**@brief
     * Address present with global scope */
    VQE_FBT_ADDR_GLOBAL,
    
    /**@brief
     * Address present with host scope */
    VQE_FBT_ADDR_HOST,
    
} vqes_fbt_status_t;


extern int
vqes_fbt_nl_init(void);

extern int
vqes_fbt_nl_deinit(void);

extern int
vqes_fbt_addr_add(struct in_addr addr, int scope);

extern int
vqes_fbt_addr_del(struct in_addr addr);

extern int
vqes_fbt_addr_flush_all(void);

extern int
vqes_fbt_addr_get_status(struct in_addr addr, vqes_fbt_status_t * addr_status);

extern const char *
vqes_fbt_addr_scope_to_str(int scope);

extern const char *
vqes_fbt_addr_status_to_str(vqes_fbt_status_t status);

#endif /* __VQES_FBT_ADDR_H__ */
