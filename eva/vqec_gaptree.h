/*
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file contains APIs to operate the VQE-C gap tree.  Basic functions
 * are init, deinit, add, remove, and extract_min.
 */

#ifndef __VQEC_GAPTREE_H__
#define __VQEC_GAPTREE_H__

#include <tree_plus.h>
#include <vam_types.h>
#include "zone_mgr.h"
#include "vqec_seq_num.h"

typedef enum {
    VQEC_GAPTREE_OK,
    VQEC_GAPTREE_ERR_INVALIDARGS,
    VQEC_GAPTREE_ERR_MALLOCFAILURE,
    VQEC_GAPTREE_ERR_ZONEALLOCFAILURE,
    VQEC_GAPTREE_ERR_OVERLAPBELOW,
    VQEC_GAPTREE_ERR_OVERLAPABOVE,
    VQEC_GAPTREE_ERR_DUPLICATE,
    VQEC_GAPTREE_ERR_GAPNOTFOUND,
    VQEC_GAPTREE_ERR_TREEISEMPTY,
    VQEC_GAPTREE_ERR_UNKNOWN,
} vqec_gaptree_error_t;

typedef enum {
    VQEC_GAPTREE_LT = -1,
    VQEC_GAPTREE_EQ = 0,
    VQEC_GAPTREE_GT = 1
} vqec_gaptree_cmpres_t;

/*
 * Note about definition of gaps:
 * A gap range of (start_seq, extent), as defined by vqec_gap_t, refers to
 * the sequence numbers beginning with start_seq, and running through
 * extent more sequence numbers.  Some examples of this are below:
 *   (start_seq, extent) ==> sequence numbers referenced by gap
 *   (234,0)             ==> 234
 *   (123,1)             ==> 123, 124
 *   (567,3)             ==> 567, 568, 569, 570
 * start_seq must be a valid sequence number in a 32-bit sequence number space
 * extent must be greater than or equal to 0 and less than 2^15
 */                      
typedef struct vqec_gap_ {
    uint32_t start_seq;  /* must be a valid sequence number */
    uint32_t extent;     /* must be >= 0 and < 2^16 */
    VQE_RB_ENTRY (vqec_gap_) rb_node;
} vqec_gap_t;

typedef struct vqe_zone vqec_gap_pool_t;
typedef int32_t message_type;
typedef uint32_t srcid;
#define VQEC_SRCID_INVALID -1
#define VQEC_GAPTREE_MAX_EXTENT 32767
#define VQEC_GAPTREE_MAX_SEQNUM ((uint64_t)((uint32_t)(-1))) /*
                                             * With 32-bit seq_num space, this
                                             * evaluates to 4294967295, which as
                                             * a decimal constant is unsigned
                                             * only in ISO C90.
                                             */

VQE_RB_HEAD(vqec_gap_tree, vqec_gap_);
/* expands to:
 *   struct vqec_gap_tree {
 * 	     struct vqec_gap_ *rbh_root;
 *   };
 */

typedef struct vqec_gap_tree vqec_gap_tree_t;

VQE_RBP_PROTOTYPE(vqec_gap_tree,
                  vqec_gap_,
                  rb_node,
                  vqec_gap_compare);

/*
 * vqec_gaptree_init
 * Initialize the gaptree.
 * @param[in]    tree    Pointer to the gaptree.
 *
 * @return               VQE_GAPTREE_OK on success; otherwise the following
 *                       failures may occur:
 *
 *     <I>VQE_GAPTREE_ERR_INVALIDARGS</I><BR>
 */
vqec_gaptree_error_t vqec_gaptree_init(vqec_gap_tree_t *tree);

/*
 * vqec_gaptree_deinit
 * Deinitialize the gap exporter.
 * @param[in]    tree    Pointer to the gaptree.
 *
 * @return               VQE_GAPTREE_OK
 */
vqec_gaptree_error_t vqec_gaptree_deinit(vqec_gap_tree_t *tree);

/*
 * vqec_gaptree_add_gap
 * Add a gap range to the gaptree.
 * @param[in]    tree    Pointer to the gaptree.
 * @param[in]    gap     Gap structure to be added to the gaptree.  Sequence
 *                       number + extent wrapping is supported.
 * @param[in]    pool    Pointer to the zone used for allocating vqec_gap_t's.
 *
 * @return               VQE_GAPTREE_OK on success; otherwise, the following
 *                       failures may occur:
 *
 *     <I>VQE_GAPTREE_ERR_INVALIDARGS</I><BR>
 *     <I>VQE_GAPTREE_ERR_ZONEALLOCFAILURE</I><BR>
 *     <I>VQE_GAPTREE_ERR_OVERLAPBELOW</I><BR>
 *     <I>VQE_GAPTREE_ERR_OVERLAPABOVE</I><BR>
 *     <I>VQE_GAPTREE_ERR_DUPLICATE</I><BR>
 *     <I>VQE_GAPTREE_ERR_UNKNOWN</I><BR>
 */
vqec_gaptree_error_t vqec_gaptree_add_gap(vqec_gap_tree_t *tree,
                                          vqec_gap_t gap,
                                          vqec_gap_pool_t *pool);

/*
 * vqec_gaptree_remove_seq_num
 * Remove a single sequence number from the gaptree.
 * @param[in]    tree    Pointer to the gaptree.
 * @param[in]    seq     Sequence number to be removed from the gaptree.
 * @param[in]    pool    Pointer to the zone used for allocating vqec_gap_t's.
 *
 * @return               VQE_GAPTREE_OK on success; otherwise, the following
 *                       failures may occur:
 *
 *     <I>VQE_GAPTREE_ERR_INVALIDARGS</I><BR>
 *     <I>VQE_GAPTREE_ERR_GAPNOTFOUND</I><BR>
 *     <I>VQE_GAPTREE_ERR_ZONEALLOCFAILURE</I><BR>
 */
vqec_gaptree_error_t vqec_gaptree_remove_seq_num(vqec_gap_tree_t *tree,
                                                 vqec_seq_num_t seq,
                                                 vqec_gap_pool_t *pool);

/*
 * vqec_gaptree_extract_min
 * Extract the minimum element of the gaptree.
 * @param[in]    tree    Pointer to the gaptree.
 * @param[out]   gap     The minimum element of the gaptree.
 * @param[in]    pool    Pointer to the zone used for allocating vqec_gap_t's.
 *
 * @return               VQE_GAPTREE_OK on success; otherwise, the following
 *                       failures may occur:
 *
 *     <I>VQE_GAPTREE_ERR_INVALIDARGS</I><BR>
 *     <I>VQE_GAPTREE_ERR_TREEISEMPTY</I><BR>
 */
vqec_gaptree_error_t vqec_gaptree_extract_min(vqec_gap_tree_t *tree,
                                              vqec_gap_t *gap,
                                              vqec_gap_pool_t *pool);

/*
 * vqec_gaptree_print
 * Print the contents of the gaptree for debugging.
 * @param[in]    tree    Pointer to the gaptree.
 * @param[in]    gprint  Pointer to the function to be used to print.
 *
 * @return               VQE_GAPTREE_OK on success; otherwise, the following
 *                       failures may occur:
 *
 *     <I>VQE_GAPTREE_ERR_INVALIDARGS</I><BR>
 *     <I>VQE_GAPTREE_ERR_MALLOCFAILURE</I><BR>
 */
vqec_gaptree_error_t vqec_gaptree_print(vqec_gap_tree_t *tree,
                                        int32_t (*gprint)(const char *, ...));

/*
 * vqec_gaptree_is_empty
 * Return whether or not the gaptree has no elements in it.
 * @param[in]    tree    Pointer to the gaptree.
 *
 * @return               TRUE if the tree is empty; FALSE if it is not
 */
boolean vqec_gaptree_is_empty(vqec_gap_tree_t *tree);

#endif /*  __VQEC_GAPTREE_H__ */
