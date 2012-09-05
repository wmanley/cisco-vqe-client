/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 *
 ******************************************************************************
 *
 * File: vqe_hash.h
 *
 * Description: Generic hash-table implementation. 
 *
 * Documents:
 *
 *****************************************************************************/
#ifndef _VQE_HASH_H_
#define _VQE_HASH_H_

#include <utils/vam_types.h>
#include <utils/queue_plus.h>

#ifdef __cplusplus
extern "C" 
{
#endif /* __cplusplus */

/*!
 * Hash key descriptor.
 */
typedef 
struct vqe_hash_key_
{
    int8_t *const valptr;
    const uint8_t len;
} vqe_hash_key_t;

#define VQE_HASH_MAKE_KEY(kptr, vptr, l)               \
    *(int8_t **)&((kptr)->valptr) = (int8_t *)(vptr);   \
    *(uint8_t *)&((kptr)->len) = (l);

#define VQE_HASH_ELEM_INSTANCE struct vqe_hash_elem_ *elem
/*!
 * Hash elem base class methods.
 */
#define VQE_HASH_ELEM_METHODS \
    /* \
       Display elem headers. */ \
    void (*vqe_hash_elem_display)(VQE_HASH_ELEM_INSTANCE); 


/*!
 * Hash elem base class members.
 */
#define VQE_HASH_ELEM_MEMBERS \
    VQE_TAILQ_ENTRY(vqe_hash_elem_) next;		   /* list chain */ \
    vqe_hash_key_t key;                           /* hash key */ \
    void *const data;                              /* data */

#define __vqe_hash_elem_fcns VQE_HASH_ELEM_METHODS
#define __vqe_hash_elem_members VQE_HASH_ELEM_MEMBERS

/*! 
 * Create hash elem. 
 */
struct vqe_hash_elem_ *vqe_hash_elem_create(const vqe_hash_key_t *key, 
                                              const void *data);

/*! 
 * Init hash elem. 
 */
void vqe_hash_elem_init (struct vqe_hash_elem_ *elem,
                         const vqe_hash_key_t *key,
                         const void *data);

/*! 
 * Destroy hash elem.
 */
boolean vqe_hash_elem_destroy(VQE_HASH_ELEM_INSTANCE);

/*!
 * Hash elem base class.
 */
DEFCLASS(vqe_hash_elem);

typedef uint32_t (*vqe_hash_func)(uint8_t *key, int32_t len, int32_t mask);
typedef boolean  (*vqe_hash_key_compare_func)(const vqe_hash_key_t *key1, 
                                               const vqe_hash_key_t *key2);

#define VQE_HASH_INSTANCE struct vqe_hash_ *h_table

/*!
 *  Methods for hash table class.
 */
#define VQE_HASH_METHODS \
    /* \
       Add elem to hash table. */ \
    boolean (*vqe_hash_add_elem)(VQE_HASH_INSTANCE,           \
                                  vqe_hash_elem_t *el);        \
    /* \                                                        
       Del elem from hash table. */ \
    boolean (*vqe_hash_del_elem)(VQE_HASH_INSTANCE,    \
                                  vqe_hash_elem_t *el); \
    /* \
       Search hash table for elem with key */ \
    vqe_hash_elem_t *(*vqe_hash_get_elem)(VQE_HASH_INSTANCE,         \
                                            const vqe_hash_key_t *key); \
    /* \
       Display hash table entries. */ \
    void (*vqe_hash_display)(VQE_HASH_INSTANCE); \
    /* \
       Add elem to hash table; allow dups */ \
    boolean (*vqe_hash_add_elem_dups_ok)(VQE_HASH_INSTANCE,           \
                                         vqe_hash_elem_t *el,         \
                                         boolean *dup_flag);           \
    /* \
       Search hash table for elem with key; requires match of elem ptr */ \
    vqe_hash_elem_t *(*vqe_hash_get_elem_exact)(VQE_HASH_INSTANCE,         \
                                                  vqe_hash_elem_t *el); \
    vqe_hash_elem_t *(*vqe_hash_get_prev_elem)(struct vqe_hash_ *h_table,\
                                                 vqe_hash_elem_t *elem);


typedef  VQE_TAILQ_HEAD(vqe_hash_tqh_, vqe_hash_elem_)vqe_hash_tqh_t;
static inline void* vqe_hash_get_elem_data (vqe_hash_elem_t *elem)
{
    return(elem->data);
}
static inline void vqe_hash_get_elem_key (vqe_hash_key_t *key_ptr,
                                           vqe_hash_elem_t *elem)
{
    VQE_HASH_MAKE_KEY(key_ptr, elem->key.valptr, elem->key.len);
    return;
}

#define VQE_HASH_BUCKETS_MAX 256000

/*!
 * Hash table class members.
 */
#define VQE_HASH_MEMBERS \
    int32_t buckets;                                /* power-of-2 buckets */ \
    int32_t mask;                                   /* bit-mask */ \
    vqe_hash_func hash_func;                       /* hash func pointer */ \
    vqe_hash_key_compare_func comp_func;           /* key compare func */ \
    vqe_hash_tqh_t *bkts;                          /* hash bucket ptrs */

#define __vqe_hash_fcns VQE_HASH_METHODS
#define __vqe_hash_members VQE_HASH_MEMBERS

/*!
 * Hash-table base class.
 */
DEFCLASS(vqe_hash);

/*! 
 * Create hash table. 
 */
struct vqe_hash_ * vqe_hash_create(int32_t buckets, 
                                     vqe_hash_func func,
                                     vqe_hash_key_compare_func cf);

/*! 
 * Destroy hash table.
 */
boolean vqe_hash_destroy(VQE_HASH_INSTANCE);
boolean vqe_hash_destroy_x(VQE_HASH_INSTANCE, boolean destroy_elem_flag);


void init_vqe_hash_elem_module(void);
void init_vqe_hash_module(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _VQE_HASH_H_ */
