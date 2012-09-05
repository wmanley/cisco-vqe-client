/*
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifndef __VQE_HEAP_H__
#define __VQE_HEAP_H__

#include <vam_types.h>

/*
 * vqe_heap_init
 * Initialize the heap.  
 *
 * @param[in]   heap           pointer to the heap
 * @param[in]   max_size       maximum number of elements the heap can contain
 *
 * @return                     VQE_HEAP_OK on success; otherwise, the following
 *                             failures may occur:
 *
 *     <I>VQE_HEAP_ERR_INVALIDARGS</I><BR>
 *     <I>VQE_HEAP_ERR_ALREADYINITTED</I><BR>
 *     <I>VQE_HEAP_ERR_MALLOCFAILURE</I><BR>
 */

/*
 * vqe_heap_deinit
 * Deinitialize the heap.  
 *
 * @param[in]   heap           pointer to the heap
 *
 * @return                     VQE_HEAP_OK on success; otherwise, the following
 *                             failures may occur:
 *
 *     <I>VQE_HEAP_ERR_HEAPNOTEMPTY</I><BR>
 *     <I>VQE_HEAP_ERR_HEAPNOTINITTED</I><BR>
 */

/*
 * vqe_heap_heapify
 * PRIVATE
 * Re-organize the heap to maintain the heap property.
 *
 * @param[in]   heap    pointer to the heap
 * @param[in]   parent  array index (in the heap's elems array) of the node
 *                      to start heapification from - usually should be 0
 *                      (meaning root) from external API calls
 *
 * @return                     VQE_HEAP_OK on success; otherwise, the following
 *                             failures may occur:
 *
 *     <I>VQE_HEAP_ERR_INVALIDARGS</I><BR>
 *     <I>VQE_HEAP_ERR_INVALIDPOLICY</I><BR>
 */

/*
 * vqe_heap_insert
 * Insert a packet into the heap.
 *
 * @param[in]   heap    pointer to the heap
 * @param[in]   elem    pointer to the element to insert into the heap
 *
 * @return                     VQE_HEAP_OK on success; otherwise, the following
 *                             failures may occur:
 *
 *     <I>VQE_HEAP_ERR_INVALIDARGS</I><BR>
 *     <I>VQE_HEAP_ERR_INVALIDPOLICY</I><BR>
 */

/*
 * vqe_heap_extract_head
 * Retrieve and remove the element with least key from the heap.
 *
 * @param[in]   heap    pointer to the heap
 * @return              pointer to the head element in the heap
 */

/*
 * vqe_heap_is_empty
 * Return whether or not the heap is empty.
 *
 * @param[in]   heap    pointer to the heap
 * @return              TRUE if empty; FALSE if not
 */

/*
 * vqe_heap_is_full
 * Return whether or not the heap has no more space left in it.
 *
 * @param[in]   heap    pointer to the heap
 * @return              TRUE if full; FALSE if not
 */

/*
 * vqe_heap_print
 * Print out the contents of the heap for debugging purposes.
 *
 * @param[in]   heap    pointer to the heap
 * @param[in]   hprint  pointer to function used to print the index and pointer
 *                      of each element
 * @param[in]   display_elem
 *                      pointer to function used to print contents of an elem -
 *                      can be NULL
 *
 * @return                     VQE_HEAP_OK on success; otherwise, the following
 *                             failures may occur:
 *
 *     <I>VQE_HEAP_ERR_INVALIDARGS</I><BR>
 */

/*
 * VQE_HEAP_HEAP_REMOVE_FOREACH(name, x, y) { ... }
 * Iterate through the heap and remove each element one-by-one, placing the
 * removed element in y with each iteration.
 *
 * @param[in]   name    same name as used in VQE_HEAP_GENERATE
 * @param[in]   x       pointer to the heap
 * @param[in]   y       variable in which to store each removed element
 */

#define VQE_HEAP_ROOT 0
#define VQE_HEAP_POLICY_MIN 0
#define VQE_HEAP_POLICY_MAX 1

typedef enum {
    VQE_HEAP_OK,
    VQE_HEAP_ERR_ALREADYINITTED,
    VQE_HEAP_ERR_MALLOCFAILURE,
    VQE_HEAP_ERR_HEAPNOTEMPTY,
    VQE_HEAP_ERR_HEAPNOTINITTED,
    VQE_HEAP_ERR_INVALIDARGS,
    VQE_HEAP_ERR_INVALIDPOLICY,
} vqe_heap_error_t;

/* Declares the heap structure */
#define VQE_HEAP(type)                                                      \
{                                                                           \
    struct type **elems;                                                     \
    uint32_t cur_size;   /* current size */                                 \
    uint32_t max_size;   /* maximum possible size */                        \
}

/* Generates prototypes and globals for heap functions */
#define VQE_HEAP_PROTOTYPE(name, type) \
vqe_heap_error_t name##_vqe_heap_init(struct name *, uint32_t);     \
vqe_heap_error_t name##_vqe_heap_deinit(struct name *);             \
vqe_heap_error_t name##_vqe_heap_insert(struct name *, struct type *); \
struct type *name##_vqe_heap_extract_head(struct name *);           \
int name##_vqe_heap_is_empty(struct name *);                        \
int name##_vqe_heap_is_full(struct name *);                         \
vqe_heap_error_t name##_vqe_heap_print(struct name *,               \
                                       int (*)(const char *, ...),  \
                                       int (*)(struct type *));

/* Main vqe_heap operation */
/*
 * The element comparison function, cmp(elem_a, elem_b), should return
 * one of the following:
 *  <0: the element elem_a should be closer to the root of the heap than elem_b
 *   0: the elements are equal and can stay in place
 *  >0: the element elem_a should be further from the root of the heap than
 *      elem_b
 */

#define VQE_HEAP_GENERATE(name, type, cmp, policy)                             \
/*                                                                      \
 * Initialize the heap.                                                 \
 */                                                                     \
vqe_heap_error_t name##_vqe_heap_init (struct name *heap, uint32_t max_size)    \
{                                                                       \
    if (!heap) {                                                        \
        return VQE_HEAP_ERR_INVALIDARGS;                              \
    }                                                                   \
                                                                        \
    if (heap && heap->elems) {                                          \
        return VQE_HEAP_ERR_ALREADYINITTED;                           \
    } else {                                                            \
        memset(heap, 0, sizeof(struct name));                           \
        heap->elems = malloc(max_size * sizeof(struct type *));         \
        if (!heap->elems) {                                             \
            return VQE_HEAP_ERR_MALLOCFAILURE;                          \
        }                                                               \
        heap->cur_size = 0;                                             \
        heap->max_size = max_size;                                      \
        return VQE_HEAP_OK;                                             \
    }                                                                   \
}                                                                       \
                                                                        \
/*                                                                      \
 * Deinitialize the heap.                                               \
 */                                                                     \
vqe_heap_error_t name##_vqe_heap_deinit (struct name *heap)                         \
{                                                                       \
    if (heap && heap->elems) {                                          \
        if (!name##_vqe_heap_is_empty(heap)) {                          \
            return VQE_HEAP_ERR_HEAPNOTEMPTY;                         \
        }                                                               \
        free(heap->elems);                                              \
        return VQE_HEAP_OK;                                             \
    } else {                                                            \
        return VQE_HEAP_ERR_HEAPNOTINITTED;                           \
    }                                                                   \
}                                                                       \
                                                                        \
/*                                                                      \
 * Re-organize the heap to maintain the heap property.                  \
 */                                                                     \
vqe_heap_error_t name##_vqe_heap_heapify (struct name *heap, int parent)            \
{                                                                       \
    struct type *tmp;                                                   \
    int left = 2 * parent + 1;   /* left child */                       \
    int right = 2 * parent + 2;  /* right child */                      \
    /*                                                                  \
     * in a min-heap, the variable min_max will store the index of the  \
     * element determined to be the least of the parent, left child,    \
     * and right child.                                                 \
     *                                                                  \
     * in a max-heap, this variable will store the index of the         \
     * greatest of the parent, left child, and right child.             \
     */                                                                 \
    int min_max = -1;  /* invalid index */                              \
                                                                        \
    if (!heap || !(heap->elems) || parent >= heap->cur_size) {          \
        return VQE_HEAP_ERR_INVALIDARGS;                              \
    }                                                                   \
                                                                        \
    switch (policy) {                                                   \
        case VQE_HEAP_POLICY_MIN:                                       \
            /* find smaller between parent and left child */            \
            if ((left < heap->cur_size) &&                              \
                ((cmp)(heap->elems[left], heap->elems[parent]) < 0)) {  \
                min_max = left;                                         \
            } else {                                                    \
                min_max = parent;                                       \
            }                                                           \
            /* find smallest between smaller of parent/left             \
             * child and right child */                                 \
            if ((right < heap->cur_size) &&                             \
                ((cmp)(heap->elems[right], heap->elems[min_max]) < 0)) { \
                min_max = right;                                        \
            }                                                           \
            break;                                                      \
        case VQE_HEAP_POLICY_MAX:                                       \
            /* find greater between parent and left child */            \
            if ((left < heap->cur_size) &&                              \
                ((cmp)(heap->elems[left], heap->elems[parent]) > 0)) {  \
                min_max = left;                                         \
            } else {                                                    \
                min_max = parent;                                       \
            }                                                           \
            /* find greatest between greater of parent/left             \
             * child and right child */                                 \
            if ((right < heap->cur_size) &&                             \
                ((cmp)(heap->elems[right], heap->elems[min_max]) > 0)) { \
                min_max = right;                                        \
            }                                                           \
            break;                                                      \
        default:                                                        \
            return VQE_HEAP_ERR_INVALIDPOLICY;                        \
    }                                                                   \
                                                                        \
    if (min_max != parent) {                                            \
        /* exchange smallest and parent and recurse */                  \
        tmp = heap->elems[min_max];                                     \
        heap->elems[min_max] = heap->elems[parent];                     \
        heap->elems[parent] = tmp;                                      \
        name##_vqe_heap_heapify(heap, min_max);                         \
    }                                                                   \
                                                                        \
    return VQE_HEAP_OK;                                                 \
}                                                                       \
                                                                        \
/*                                                                      \
 * Insert an element into the heap.                                     \
 */                                                                     \
vqe_heap_error_t name##_vqe_heap_insert (struct name *heap, struct type *elem)              \
{                                                                       \
    unsigned int i, parent;                                             \
    struct type *tmp;                                                   \
                                                                        \
    if (!heap || !(heap->elems) || name##_vqe_heap_is_full(heap)) {     \
        return VQE_HEAP_ERR_INVALIDARGS;                              \
    }                                                                   \
                                                                        \
    /*                                                                  \
     * array is organized so that the children of a node with index n   \
     * are 2n+1 and 2n+2, so to find the parent, we take the index of   \
     * the last node BEFORE where we'll be placed, and divide by 2      \
     */                                                                 \
    heap->elems[heap->cur_size] = elem;  /* insert the element at the bottom */ \
    heap->cur_size++;                                                   \
    i = heap->cur_size - 1;  /* index of the element we just inserted */ \
                                                                        \
    if (heap->cur_size == 1) {                                          \
        return VQE_HEAP_OK;                                             \
    }                                                                   \
                                                                        \
    /* percolate the element up to where it should be */                \
    parent = (i - 1) / 2;                                               \
    switch (policy) {                                                   \
        case VQE_HEAP_POLICY_MIN:                                       \
            while ((i > 0) && ((cmp)(heap->elems[parent], elem) > 0)) { \
                tmp = heap->elems[i];                                   \
                heap->elems[i] = heap->elems[parent];                   \
                heap->elems[parent] = tmp;                              \
                i = parent;                                             \
                parent = (i - 1) / 2;                                   \
            }                                                           \
            break;                                                      \
        case VQE_HEAP_POLICY_MAX:                                       \
            while ((i > 0) && ((cmp)(heap->elems[parent], elem) < 0)) { \
                tmp = heap->elems[i];                                   \
                heap->elems[i] = heap->elems[parent];                   \
                heap->elems[parent] = tmp;                              \
                i = parent;                                             \
                parent = (i - 1) / 2;                                   \
            }                                                           \
            break;                                                      \
        default:                                                        \
            return VQE_HEAP_ERR_INVALIDPOLICY;                        \
    }                                                                   \
    return VQE_HEAP_OK;                                                 \
}                                                                       \
                                                                        \
/*                                                                      \
 * Retrieve and remove the element with least key from the heap.        \
 */                                                                     \
struct type *name##_vqe_heap_extract_head (struct name *heap)           \
{                                                                       \
    struct type *head_elem;                                             \
                                                                        \
    if (!heap || !(heap->elems) || name##_vqe_heap_is_empty(heap)) {    \
        return NULL;                                                    \
    }                                                                   \
                                                                        \
    if (heap->cur_size == 1) {                                          \
        heap->cur_size--;                                               \
        return heap->elems[VQE_HEAP_ROOT];                              \
    }                                                                   \
                                                                        \
    head_elem = heap->elems[VQE_HEAP_ROOT];                             \
                                                                        \
    /* move the last element to root, shrink the heap, and re-heapify */ \
    heap->elems[VQE_HEAP_ROOT] = heap->elems[heap->cur_size - 1];       \
    heap->cur_size--;                                                   \
    name##_vqe_heap_heapify(heap, VQE_HEAP_ROOT);                       \
    return head_elem;                                                   \
}                                                                       \
                                                                        \
/*                                                                      \
 * Return whether or not the heap is empty.                             \
 */                                                                     \
int name##_vqe_heap_is_empty (struct name *heap)                               \
{                                                                       \
        return (heap && heap->cur_size == 0);                           \
}                                                                       \
                                                                        \
/*                                                                      \
 * Return whether or not the heap has no more space left in it.         \
 */                                                                     \
int name##_vqe_heap_is_full (struct name *heap)                                \
{                                                                       \
    return (heap && heap->cur_size >= heap->max_size);                  \
}                                                                       \
                                                                        \
/*                                                                      \
 * Print the contents of the heap.                                      \
 */                                                                     \
vqe_heap_error_t name##_vqe_heap_print (struct name *heap,                      \
                                        int (*hprint)(const char *, ...), \
                                        int (*print_elem)(struct type *elem)) \
{                                                                       \
    int i;                                                              \
    if (!heap || !(heap->elems) || !hprint) {                           \
        return VQE_HEAP_ERR_INVALIDARGS;                                \
    }                                                                   \
                                                                        \
    hprint("heap (cur_size = %d; max_size = %d) contents:\n",           \
           heap->cur_size, heap->max_size);                             \
    for (i = VQE_HEAP_ROOT; i < heap->cur_size; i++) {                  \
        hprint(" [%d] %p\n", i, heap->elems[i]);                        \
        if (print_elem) {                                               \
            print_elem(heap->elems[i]);                                 \
        }                                                               \
    }                                                                   \
                                                                        \
    return VQE_HEAP_OK;                                                 \
}

#define VQE_HEAP_INIT(name, x, y) name##_vqe_heap_init(x, y)
#define VQE_HEAP_DEINIT(name, x) name##_vqe_heap_deinit(x)
#define VQE_HEAP_HEAPIFY(name, x, y) name##_vqe_heap_heapify(x, y)
#define VQE_HEAP_INSERT(name, x, y)	name##_vqe_heap_insert(x, y)
#define VQE_HEAP_EXTRACT_HEAD(name, x) name##_vqe_heap_extract_head(x)
#define VQE_HEAP_IS_EMPTY(name, x) name##_vqe_heap_is_empty(x)
#define VQE_HEAP_IS_FULL(name, x) name##_vqe_heap_is_full(x)
#define VQE_HEAP_PRINT(name, x, y, z) name##_vqe_heap_print(x, y, z)
#define VQE_HEAP_REMOVE_FOREACH(name, x, y)     \
    for (y = name##_vqe_heap_extract_head(x);    \
         y;                                    \
         y = name##_vqe_heap_extract_head(x))

#endif /*  __VQE_HEAP_H__ */
