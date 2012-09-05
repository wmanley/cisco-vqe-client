/*
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * The VQE bitmap is a module with functions for working with bit ranges in
 * a direct-indexed array of bits.  There are functions provided to create,
 * destroy, add to, remove from, and retrieve bits from the bitmap.
 */

#include "vam_types.h"
#include "vqe_bitmap.h"

/* internal defines - not for use outside of this file */
#define VQE_BITMAP_ALL_ONES (uint32_t)(-1)
#define VQE_BITMAP_IDX_MAX (map->size - 1)
#define VQE_BITMAP_IDX(x) ((x) & VQE_BITMAP_IDX_MAX)

/* opaque data structure */
struct vqe_bitmap_ {
    uint32_t *data;
    uint32_t size;
};

/**
 * Create a bitmap.  Will return NULL on failure.
 */
vqe_bitmap_t *vqe_bitmap_create (uint32_t size)
{
    vqe_bitmap_t *map;

    if (size > VQE_BITMAP_MAX_SIZE) {
        return NULL;
    }

    /* allocate bitmap structure */
    map = VQE_MALLOC(sizeof(vqe_bitmap_t));
    if(!map) {
        return NULL;
    }
    memset(map, 0, sizeof(vqe_bitmap_t));

    /* allocate data array */
    map->data = VQE_MALLOC(sizeof(uint32_t) * (size >> VQE_BITMAP_WORD_SIZE_BITS));
    if (!map->data) {
        VQE_FREE(map);
        return NULL;
    }

    map->size = size;

    vqe_bitmap_flush(map);

    return map;
}

/**
 * Destroy a bitmap.
 */
vqe_bitmap_error_t vqe_bitmap_destroy (vqe_bitmap_t *map)
{
    if (!map || !map->data) {
        return VQE_BITMAP_ERR_INVALIDARGS;
    }

    VQE_FREE(map->data);
    VQE_FREE(map);

    return VQE_BITMAP_OK;
}

/**
 * Flush the contents of the bit array.
 */
vqe_bitmap_error_t vqe_bitmap_flush (vqe_bitmap_t *map)
{
    if (!map || !map->data) {
        return VQE_BITMAP_ERR_INVALIDARGS;
    }

    memset(map->data, 0,
           sizeof(uint32_t) * (map->size >> VQE_BITMAP_WORD_SIZE_BITS));

    return VQE_BITMAP_OK;
}

/**
 * Set a range of bits in the bitmap.
 */
vqe_bitmap_error_t vqe_bitmap_modify_bitrange (vqe_bitmap_t *map,
                                               uint32_t startbit,
                                               uint32_t endbit,
                                               boolean value)
{
    uint16_t b1 = startbit, b2 = endbit;
    uint16_t block_num;  /* index of relevant block in data array */
    uint16_t block_offset;  /* bit offset within relevant block */
    uint16_t extent;  /* length of the bit range */
    uint32_t block;  /* 32-bit block of the data array */

    if (!map) {
        return VQE_BITMAP_ERR_INVALIDARGS;
    }

    b1 = VQE_BITMAP_IDX(b1);
    b2 = VQE_BITMAP_IDX(b2);

    if (b2 < b1) {  /* detect wrap-around */
        /* handle wrap-around case recursively */
        vqe_bitmap_modify_bitrange(map, b1, VQE_BITMAP_IDX_MAX, value);
        vqe_bitmap_modify_bitrange(map, 0, b2, value);
        return VQE_BITMAP_OK;
    }

    while (b1 <= b2) {
        block_num = b1 >> VQE_BITMAP_WORD_SIZE_BITS;  /* the 11 bits after
                                                         the first 16 */
        block_offset = (b1 & 0x1f);  /* last 5 bits */
        extent = ((b2 >> VQE_BITMAP_WORD_SIZE_BITS) - block_num) ?
            31 - block_offset : (b2 - b1);
        block = (VQE_BITMAP_ALL_ONES >> block_offset) &
            VQE_BITMAP_ALL_ONES << (31 - block_offset - extent);
        if (value) {  /* set the range to 1's */
            map->data[block_num] |= block;
        } else {  /* set the range to 0's (clear the range) */
            map->data[block_num] &= ~block;
        }
        if (block_num == 
            (uint16_t)VQE_BITMAP_ALL_ONES >> VQE_BITMAP_WORD_SIZE_BITS) {
            break;  /* prevent infinite loop */
        }
        b1 = (block_num + 1) << VQE_BITMAP_WORD_SIZE_BITS;
    }

    return VQE_BITMAP_OK;
}

/**
 * Get the value of the bit at a given bit index.
 */
vqe_bitmap_error_t vqe_bitmap_get_bit (vqe_bitmap_t *map,
                                       boolean *value,
                                       uint32_t bit)
{
    uint32_t block;

    if (!map || !value) {
        return VQE_BITMAP_ERR_INVALIDARGS;
    }

    /* convert bit to proper range */
    bit = VQE_BITMAP_IDX(bit);

    block = map->data[bit >> VQE_BITMAP_WORD_SIZE_BITS];
    *value = (block >> (31 - (bit & 0x1f))) & 1;

    return VQE_BITMAP_OK;
}

/**
 * Set a single bit in the bitmap.
 */
vqe_bitmap_error_t vqe_bitmap_set_bit (vqe_bitmap_t *map,
                                       uint32_t bit)
{
    uint16_t block_num;

    if (!map) {
        return VQE_BITMAP_ERR_INVALIDARGS;
    }

    /* convert bit to proper range */
    bit = VQE_BITMAP_IDX(bit);

    block_num = bit >> VQE_BITMAP_WORD_SIZE_BITS;  /* first 27 bits */
    map->data[block_num] |= 1 << (31 - (bit & 0x1f));

    return VQE_BITMAP_OK;
}

/**
 * Clear a single bit in the bitmap.
 */
vqe_bitmap_error_t vqe_bitmap_clear_bit (vqe_bitmap_t *map,
                                         uint32_t bit)
{
    uint16_t block_num;

    if (!map) {
        return VQE_BITMAP_ERR_INVALIDARGS;
    }

    /* convert bit to proper range */
    bit = VQE_BITMAP_IDX(bit);

    block_num = bit >> VQE_BITMAP_WORD_SIZE_BITS;  /* first 27 bits */
    map->data[block_num] &= ~(1 << (31 - (bit & 0x1f)));

    return VQE_BITMAP_OK;
}

/**
 * Get a single 32-bit block containing a given bit index.
 */
vqe_bitmap_error_t vqe_bitmap_get_block (vqe_bitmap_t *map,
                                         uint32_t *block,
                                         uint32_t bit)
{
    if (!map || !block) {
        return VQE_BITMAP_ERR_INVALIDARGS;
    }

    /* convert bit to proper range */
    bit = VQE_BITMAP_IDX(bit);

    *block = map->data[bit >> VQE_BITMAP_WORD_SIZE_BITS];

    return VQE_BITMAP_OK;
}

/**
 * Clear the entire 32-bit block containing a given bit index.
 */
vqe_bitmap_error_t vqe_bitmap_clear_block (vqe_bitmap_t *map,
                                           uint32_t bit)
{
    if (!map) {
        return VQE_BITMAP_ERR_INVALIDARGS;
    }

    /* convert bit to proper range */
    bit = VQE_BITMAP_IDX(bit);

    map->data[bit >> VQE_BITMAP_WORD_SIZE_BITS] = 0;

    return VQE_BITMAP_OK;
}
