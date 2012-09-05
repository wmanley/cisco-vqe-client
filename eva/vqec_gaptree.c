/*
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * The VQE-C gaptree is a module with functions for working with gap ranges in
 * a red-black tree.  There are functions provided to add to, remove from, and
 * extract the minimum element from the tree.  These APIs are used by the PCM
 * module to keep track of the current gaps it has seen in the packet stream.
 */

#include <tree_plus.h>
#include "vqec_gaptree.h"
#include "vqec_seq_num.h"
#include <string.h>

/*
 * Compares to gap ranges based on their start_seq values.
 * Takes into account wrap-around.
 */
static vqec_gaptree_cmpres_t vqec_gap_compare(vqec_gap_t *a, vqec_gap_t *b)
{
    if (vqec_seq_num_lt(a->start_seq, b->start_seq)) {
        return VQEC_GAPTREE_LT;
    } else if (vqec_seq_num_gt(a->start_seq, b->start_seq)) {
        return VQEC_GAPTREE_GT;
    } else {
        return VQEC_GAPTREE_EQ;
    }
}

VQE_RBP_GENERATE(vqec_gap_tree, vqec_gap_, rb_node, vqec_gap_compare);

/*
 * Initialize the gaptree.
 */
vqec_gaptree_error_t vqec_gaptree_init (vqec_gap_tree_t *tree)
{
    if (!tree) {
        return VQEC_GAPTREE_ERR_INVALIDARGS;
    }

    VQE_RB_INIT(tree);

    return VQEC_GAPTREE_OK;
}

/*
 * Deinitialize the gaptree.
 */
vqec_gaptree_error_t vqec_gaptree_deinit (vqec_gap_tree_t *tree)
{
    return VQEC_GAPTREE_OK;
}

/*
 * Add a gap range to the gaptree.
 */
vqec_gaptree_error_t vqec_gaptree_add_gap (vqec_gap_tree_t *tree,
                                           vqec_gap_t gap,
                                           vqec_gap_pool_t *pool)
{
    vqec_gap_t *new_gap, *tmp, split_gap;
    vqec_gaptree_error_t err = VQEC_GAPTREE_OK;

    if (!tree || !pool || (gap.extent > VQEC_GAPTREE_MAX_EXTENT)) {
        return VQEC_GAPTREE_ERR_INVALIDARGS;
    }

    new_gap = zone_acquire(pool);
    if (!new_gap) {
        return VQEC_GAPTREE_ERR_ZONEALLOCFAILURE;
    }
    memset(new_gap, 0, sizeof(vqec_gap_t));

    /* 
     * if a gap "wraps around" back to the beginning of the seq. num. space,
     * then we split it into two gaps and add them both separately 
     */
    /* check for upper bound of gap going beyond sequence number space */
    if (((uint64_t)gap.start_seq + (uint64_t)gap.extent) > 
        VQEC_GAPTREE_MAX_SEQNUM) {
        /* add "lower" portion of gap to end of seq num space */
        split_gap.start_seq = gap.start_seq;
        split_gap.extent = VQEC_GAPTREE_MAX_SEQNUM - gap.start_seq;
        err = vqec_gaptree_add_gap(tree, split_gap, pool);
        if (err != VQEC_GAPTREE_OK) {
            return err;
        }
        /* add "higher" portion of gap to beginning of seq num space */
        split_gap.start_seq = 0;
        split_gap.extent = gap.extent - split_gap.extent - 1;
        return vqec_gaptree_add_gap(tree, split_gap, pool);
    }

    new_gap->start_seq = gap.start_seq;
    new_gap->extent = gap.extent;

    /* check for duplicate and overlapping gap range from below */
    tmp = VQE_RB_FINDLE(vqec_gap_tree, tree, new_gap);
    if (tmp) {
        if (vqec_seq_num_eq(new_gap->start_seq, tmp->start_seq)) {
            return VQEC_GAPTREE_ERR_DUPLICATE;
        } else if (vqec_seq_num_le((new_gap->start_seq),
                            (tmp->start_seq + tmp->extent))) {
            return VQEC_GAPTREE_ERR_OVERLAPBELOW;
        }
    }

    /* check for overlapping gap range to above */
    tmp = VQE_RB_FINDGE(vqec_gap_tree, tree, new_gap);
    if (tmp) {
        if (vqec_seq_num_ge((new_gap->start_seq + new_gap->extent),
                            (tmp->start_seq))) {
            return VQEC_GAPTREE_ERR_OVERLAPABOVE;
        }
    }

    if (VQE_RB_INSERT(vqec_gap_tree, tree, new_gap)) {
        return VQEC_GAPTREE_ERR_UNKNOWN;
    }

    return VQEC_GAPTREE_OK;
}

/*
 * Remove a single seq_num gap from the gaptree.
 */
vqec_gaptree_error_t vqec_gaptree_remove_seq_num (vqec_gap_tree_t *tree,
                                                  vqec_seq_num_t seq,
                                                  vqec_gap_pool_t *pool)
{
    vqec_gap_t *tmp, new_gap;
    vqec_gaptree_error_t ret = VQEC_GAPTREE_ERR_GAPNOTFOUND;
    uint32_t tmp_start, tmp_ext;
    
    memset(&new_gap, 0, sizeof(vqec_gap_t));
    new_gap.start_seq = seq;
    new_gap.extent = 0;

    if (!tree || !pool) {
        return VQEC_GAPTREE_ERR_INVALIDARGS;
    }

    /* find the gap starting less than or equal to the target seq_num */
    tmp = VQE_RB_FINDLE(vqec_gap_tree, tree, &new_gap);
    if (tmp && (vqec_seq_num_le(seq, tmp->start_seq + tmp->extent))) {
        /* gap is in range */
        tmp_start = tmp->start_seq;
        tmp_ext = tmp->extent;
        if (seq == tmp_start) {
            if (tmp_ext) {
                /* trim the start_seq of the gap range */
                tmp->start_seq++;
                tmp->extent--;
            } else {
                /* extent == 0, so this is a single gap; just remove it */
                VQE_RB_REMOVE(vqec_gap_tree, tree, tmp);
                zone_release(pool, tmp);
            }
            ret = VQEC_GAPTREE_OK;
        } else {
            /* trim the extent and re-add anything necessary */
            tmp->extent = seq - tmp_start - 1;
            ret = VQEC_GAPTREE_OK;
            if (vqec_seq_num_lt(seq, tmp_start + tmp_ext)) {
                /* end of tmp > target seq */
                /* need to add back portion of gap that's beyond the target seq */
                new_gap.start_seq = seq + 1;
                new_gap.extent = (tmp_start + tmp_ext) - seq - 1;
                ret = vqec_gaptree_add_gap(tree, new_gap, pool);
            }
        }
    }

    return ret;
}

/*
 * Get and remove the minimal element from the gaptree.
 */
vqec_gaptree_error_t vqec_gaptree_extract_min(vqec_gap_tree_t *tree,
                                              vqec_gap_t *gap,
                                              vqec_gap_pool_t *pool)
{
    vqec_gap_t *min_gap;

    if (!tree || !gap || !pool) {
        return VQEC_GAPTREE_ERR_INVALIDARGS;
    } else if (vqec_gaptree_is_empty(tree)) {
        return VQEC_GAPTREE_ERR_TREEISEMPTY;
    }

    min_gap = VQE_RB_MIN(vqec_gap_tree, tree);
    if (min_gap) {
        *gap = *min_gap;
        VQE_RB_REMOVE(vqec_gap_tree, tree, min_gap);
        zone_release(pool, min_gap);
    }

    return VQEC_GAPTREE_OK;
}

/*
 * Print contents of gaptree for debugging.
 */
vqec_gaptree_error_t vqec_gaptree_print(vqec_gap_tree_t *tree,
                                        int32_t (*gprint)(const char *, ...))
{
    vqec_gap_t *gap, prev_gap;
    boolean first_gap = TRUE;

    if (!tree || !gprint) {
        return VQEC_GAPTREE_ERR_INVALIDARGS;
    }

    memset(&prev_gap, 0, sizeof(vqec_gap_t));

    VQE_RB_FOREACH(gap, vqec_gap_tree, tree) {
        gprint(" (%5u,%1u) ==> %u - %u",
               gap->start_seq, gap->extent,
               gap->start_seq, gap->start_seq + gap->extent);
        if (vqec_seq_num_lt(gap->start_seq, prev_gap.start_seq) &&
            !first_gap) {   /* out of order */
            gprint("\tSEQ ERROR!!!\n");
        } else if (vqec_seq_num_ge((prev_gap.start_seq + prev_gap.extent),
                                   gap->start_seq) &&
                   !first_gap) {   /* overlapping range */
            gprint("\tOVERLAP!!!\n");
        } else {   /* all ok - just print it */
            gprint("\n");
        }

        prev_gap = *gap;
        first_gap = FALSE;
    }

    return VQEC_GAPTREE_OK;
}

/*
 * Check to see if the gaptree is empty.
 */
boolean vqec_gaptree_is_empty(vqec_gap_tree_t *tree)
{
    return (tree && !VQE_RB_ROOT(tree));
}
