/*
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file contains APIs to operate the VQE bitmap.  Basic functions
 * are create, destroy, add, remove, and retrieve.
 */

#ifndef __VQEC_BITMAP_H__
#define __VQEC_BITMAP_H__

#include "vam_types.h"

/* 
 * The VQE bitmap is a data structure that is simply an array of bits (a simple
 * binary digit, 1 or 0).  The structure is stored in memory as an array of
 * 32-bit integers, and each 32-bit uint32_t element in the bitmap array is
 * referred to as a single "block."
 */

#define VQE_BITMAP_MAX_SIZE 65536
#define VQE_BITMAP_WORD_SIZE_BITS 5

typedef struct vqe_bitmap_ vqe_bitmap_t;

typedef enum {
    VQE_BITMAP_OK,
    VQE_BITMAP_ERR_INVALIDARGS,
} vqe_bitmap_error_t;

/**
 * vqe_bitmap_create
 * Create a bitmap.
 *
 * @param[in]    size   Size (in number of bits) of bitmap to create.  The size
 *                      must be a power of 2, and no less than 32.
 *
 * @return       Returns a pointer to the created bitmap, or NULL if failed.
 */
vqe_bitmap_t *vqe_bitmap_create(uint32_t size);

/**
 * vqe_bitmap_destroy
 * Destroy a bitmap.
 *
 * @param[in]    map    Pointer to the bitmap to destroy.
 * @return              VQE_BITMAP_OK on success; otherwise the following
 *                      failure may occur:
 *
 *     <I>VQE_BITMAP_ERR_INVALIDARGS</I><BR>
 */
vqe_bitmap_error_t vqe_bitmap_destroy(vqe_bitmap_t *map);

/**
 * vqe_bitmap_flush
 * Flush the contents of the bit array.
 * @param[in]    map    Pointer to the bitmap to flush.
 * @return              VQE_BITMAP_OK on success; otherwise the following
 *                      failure may occur:
 *
 *     <I>VQE_BITMAP_ERR_INVALIDARGS</I><BR>
 */
vqe_bitmap_error_t vqe_bitmap_flush(vqe_bitmap_t *map);


/**
 * vqe_bitmap_set_bitrange
 * Set a range of bits in the bitmap.  Wrap-around is not supported.
 * 
 * @param[in]    map       Pointer to the bitmap.
 * @param[in]    startbit  Start bit index of range.
 * @param[in]    endbit    End bit index of range.
 * @param[in]    value     If TRUE, bitrange will be set to 1's; if FALSE,
 *                         bitrange will be set to 0's (cleared).
 * @return                 VQE_BITMAP_OK on success; otherwise the following
 *                         failure may occur:
 *
 *     <I>VQE_BITMAP_ERR_INVALIDARGS</I><BR>
 */
vqe_bitmap_error_t vqe_bitmap_modify_bitrange(vqe_bitmap_t *map,
                                              uint32_t startbit,
                                              uint32_t endbit,
                                              boolean value);
/**
 * vqe_bitmap_get_bit
 * Get the value of the bit at a given bit index.
 *
 * @param[in]    map    Pointer to the bitmap.
 * @param[out]   value  Pointer to a boolean to store the value of the bit.
 * @param[in]    bit    Bit index to be retrieved.
 * @return              VQE_BITMAP_OK on success; otherwise the following
 *                      failure may occur:
 *
 *     <I>VQE_BITMAP_ERR_INVALIDARGS</I><BR>
 */
vqe_bitmap_error_t vqe_bitmap_get_bit(vqe_bitmap_t *map,
                                      boolean *value,
                                      uint32_t bit);

/**
 * vqe_bitmap_set_bit
 * Set a single bit in the bitmap.
 * 
 * @param[in]    map    Pointer to the bitmap.
 * @param[in]    bit    Index of the bit to set.
 * @return              VQE_BITMAP_OK on success; otherwise the following
 *                      failure may occur:
 *
 *     <I>VQE_BITMAP_ERR_INVALIDARGS</I><BR>
 */
vqe_bitmap_error_t vqe_bitmap_set_bit(vqe_bitmap_t *map,
                                      uint32_t bit);

/**
 * vqe_bitmap_clear_bit
 * Clear a single bit in the bitmap.
 * 
 * @param[in]    map    Pointer to the bitmap.
 * @param[in]    bit    Index of the bit to clear.
 * @return              VQE_BITMAP_OK on success; otherwise the following
 *                      failure may occur:
 *
 *     <I>VQE_BITMAP_ERR_INVALIDARGS</I><BR>
 */
vqe_bitmap_error_t vqe_bitmap_clear_bit(vqe_bitmap_t *map,
                                        uint32_t bit);

/**
 * vqe_bitmap_get_block
 * Get a single 32-bit block containing a given bit index.
 *
 * @param[in]    map    Pointer to the bitmap.
 * @param[out]   block  Pointer to 32-bit buffer where block should be stored.
 * @param[in]    bit    Bit index within block to be retrieved.
 * @return              VQE_BITMAP_OK on success; otherwise the following
 *                      failure may occur:
 *
 *     <I>VQE_BITMAP_ERR_INVALIDARGS</I><BR>
 */
vqe_bitmap_error_t vqe_bitmap_get_block(vqe_bitmap_t *map,
                                        uint32_t *block,
                                        uint32_t bit);

/**
 * vqe_bitmap_clear_block
 * Clear the entire 32-bit block containing a given bit index.
 *
 * @param[in]    map    Pointer to the bitmap.
 * @param[in]    bit    Bit index within block to be cleared.
 * @return              VQE_BITMAP_OK on success; otherwise the following
 *                      failure may occur:
 *
 *     <I>VQE_BITMAP_ERR_INVALIDARGS</I><BR>
 */
vqe_bitmap_error_t vqe_bitmap_clear_block(vqe_bitmap_t *map,
                                          uint32_t bit);

#endif /*  __VQEC_BITMAP_H__ */
