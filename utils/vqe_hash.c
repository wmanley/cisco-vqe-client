/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 *
 ******************************************************************************
 *
 * File: vqe_hash.c
 *
 * Description: Generic hash-table implementation. 
 *
 * Documents:
 *
 *****************************************************************************/

#include <string.h>
#include <vqe_hash.h>
#include <log/vqe_utils_syslog_def.h>
#include <vqe_port_macros.h>

#define HASH_ERR_STRLEN 256

typedef 
enum hash_errtype_t_
{
    HASH_OK,
    HASH_ELEM_IS_NULL,
    HASH_BUCKET_LIMIT,
    HASH_TABLE_IS_NULL,
    HASH_KEY_IS_NULL,
    HASH_KEYVAL_IS_NULL,
    HASH_NO_SUCH_ELEM, 
    HASH_INVALID_KEYLEN,
    HASH_DUPLICATE_KEY,
    HASH_MISSING_HASH_FUNC,
    HASH_MALLOC_FAILED,
} hash_errtype_t;

typedef
struct hash_err_t_
{
    char *f;
    hash_errtype_t type;
} hash_err_t;

#define HASH_SET_ERR(s, t)         \
    s.f = (char *)__FUNCTION__;    \
    s.type = t;

#define HASH_GET_ERR_TYPE(s)   \
    s.type

/*!
 * hash element class
 */
static vqe_hash_elem_fcns_t vqe_hash_elem_fcn_table;
/*!
 * hash class
 */
static vqe_hash_fcns_t vqe_hash_fcn_table;
/*!
  Static variable to tell hash elem module is initialized or not.
 */
static boolean vqe_hash_elem_module_initialized = FALSE;
/*!
  Static variable to tell hash module is initialized or not.
*/
static boolean vqe_hash_module_initialized = FALSE;

/*!
 * Log errors.
 */
static 
void vqe_hash_log_err (hash_err_t *err)
{
    char buf[HASH_ERR_STRLEN];

    switch (err->type) {
    case HASH_ELEM_IS_NULL:
        snprintf(buf, HASH_ERR_STRLEN, "%s - %s -", err->f, 
                 "Hash element ptr is null");
        syslog_print(VQE_HASH_BAD_ARG, buf);
        return;

    case HASH_BUCKET_LIMIT:
        snprintf(buf, HASH_ERR_STRLEN, "%s - %s -", err->f, 
                 "Hash bucket limit exceeded");
        syslog_print(VQE_HASH_BAD_ARG, buf);
        return;

    case HASH_TABLE_IS_NULL:
        snprintf(buf, HASH_ERR_STRLEN, "%s - %s -", err->f, 
                 "Hash table ptr is null");
        syslog_print(VQE_HASH_BAD_ARG, buf);
        return;

    case HASH_KEY_IS_NULL:
        snprintf(buf, HASH_ERR_STRLEN, "%s - %s -", err->f, 
                 "Hash key ptr is null");
        syslog_print(VQE_HASH_BAD_ARG, buf);
        return;

    case HASH_NO_SUCH_ELEM:
        snprintf(buf, HASH_ERR_STRLEN, "%s - %s -", err->f, 
                 "Queried element not in hash-table");
        syslog_print(VQE_HASH_REMOVE_FAILURE, buf);
        return;

    case HASH_INVALID_KEYLEN:
        snprintf(buf, HASH_ERR_STRLEN, "%s - %s -", err->f, 
                 "Key length mismatch or invalid length");
        syslog_print(VQE_HASH_BAD_ARG, buf);
        return;

    case HASH_DUPLICATE_KEY:
        snprintf(buf, HASH_ERR_STRLEN, "%s - %s -", err->f, 
                 "Another element with key already exists");
        syslog_print(VQE_HASH_INSERT_FAILURE, buf);
        return;

    case HASH_MISSING_HASH_FUNC:
        snprintf(buf, HASH_ERR_STRLEN, "%s - %s -", err->f, 
                 "No hash function specified");
        syslog_print(VQE_HASH_BAD_ARG, buf);
        return;

    case HASH_KEYVAL_IS_NULL:
        snprintf(buf, HASH_ERR_STRLEN, "%s - %s -", err->f, 
                 "Key's value ptr is null");
        syslog_print(VQE_HASH_BAD_ARG, buf);
        return;    
    
    case HASH_MALLOC_FAILED:
        snprintf(buf, HASH_ERR_STRLEN, "%s - %s -", err->f, 
                 "Memory allocation failed");
        syslog_print(VQE_HASH_MALLOC_FAILURE, buf);
        return;

    default:
        return;
    }
}

/*!
 * Compare two hash keys.
 */
static 
boolean vqe_hash_key_compare (const vqe_hash_key_t *key1, 
                               const vqe_hash_key_t *key2)
{
    if (key1 && key2) {
        if (key1->len && (key1->len == key2->len) &&
            key1->valptr && key2->valptr) {
            return (!memcmp(key1->valptr, key2->valptr, key1->len));
        }
    }

    return (FALSE);
}

/*!
 * Initialize a hash element's list header.
 */
static 
void vqe_hash_elem_init_list_hdr (struct vqe_hash_elem_ *elem) 
{
    memset(&elem->next, 0, sizeof(elem->next));
}

/*!
 * Display a hash element -
 * Since the function is invoked via the mcall interface, elem
 * cannot be null, since the function is invoked by dereferencing
 * the element in the first place.
 */
void vqe_hash_elem_display (struct vqe_hash_elem_ * elem) 
{
    CONSOLE_PRINTF("Key ptr (%p) Key len (%d) Data ptr (%p)\n",
                   elem->key.valptr, elem->key.len, elem->data);
}

/*!
  Set the function table of hash elem module
  @param[in] table hash elem module function table.
 */
void vqe_hash_elem_set_fcns (vqe_hash_elem_fcns_t * table)
{
    table->vqe_hash_elem_display = vqe_hash_elem_display;
}

/*!  
 * Init hash elem module. Basically it just sets the hash elem module
 * function table.  
 */
void init_vqe_hash_elem_module (void) 
{
    if (vqe_hash_elem_module_initialized) {
        return;
    }
    
    vqe_hash_elem_set_fcns(&vqe_hash_elem_fcn_table);
    vqe_hash_elem_module_initialized = TRUE;
}

/*! 
 * Create hash elem. 
 * @return Returns a pointer to a newly created hash element. A
 * null pointer is returned if the allocation fails. It's the caller's
 * responsibility to handle the error.
 */
struct vqe_hash_elem_ *vqe_hash_elem_create (const vqe_hash_key_t *key,
                                               const void *data)
{
    vqe_hash_elem_t *elem = NULL;
    hash_err_t err;

    HASH_SET_ERR(err, HASH_OK);

    if (key && key->valptr && key->len) {
        elem = (vqe_hash_elem_t *)calloc(1, sizeof(vqe_hash_elem_t));	
        if (elem && 
            (*(int8_t **)&elem->key.valptr = 
             (int8_t *)calloc(1, sizeof(int8_t) * key->len))) {

            *(uint8_t *)&elem->key.len = key->len;
            *(void **)&elem->data = (void *)data;
            elem->__func_table = &vqe_hash_elem_fcn_table;
            memcpy(elem->key.valptr, key->valptr, key->len);
        } else {
            if (elem) {
                free(elem);
                elem = NULL;
            }
            HASH_SET_ERR(err, HASH_MALLOC_FAILED);
        }
    } else {
        if (!key) {
            HASH_SET_ERR(err, HASH_KEY_IS_NULL);
        } else if (!key->valptr) {
            HASH_SET_ERR(err, HASH_KEYVAL_IS_NULL);
        } else if (!key->len) {
            HASH_SET_ERR(err, HASH_INVALID_KEYLEN);
        }
    }

    vqe_hash_log_err(&err); 
    return (elem);
}

/*! 
 * Init hash elem. 
 * @return None 
 * Initializes an element with key and data. This function is used when
 * the hash element is embedded in some other data structure. 
 */
void vqe_hash_elem_init (struct vqe_hash_elem_ *elem,
                         const vqe_hash_key_t *key,
                         const void *data)
{
    hash_err_t err;

    HASH_SET_ERR(err, HASH_OK);

    if (key && key->valptr && key->len) {
        elem->__func_table = &vqe_hash_elem_fcn_table;
        *(uint8_t *)&elem->key.len = key->len;
        *(int8_t **)&elem->key.valptr = key->valptr; 
        *(void **)&elem->data = (void *)data;
    } else {
        if (!key) {
            HASH_SET_ERR(err, HASH_KEY_IS_NULL);
        } else if (!key->valptr) {
            HASH_SET_ERR(err, HASH_KEYVAL_IS_NULL);
        } else if (!key->len) {
            HASH_SET_ERR(err, HASH_INVALID_KEYLEN);
        }
    }

    vqe_hash_log_err(&err); 
    return;
}


/*! 
 * Destroy hash elem. 
 * @return Destroys an existing hash element. Returns true if 
 * the element is destroyed without errors, false otherwise. It's 
 * the caller's responsibility to handle the error.
 */
boolean vqe_hash_elem_destroy (struct vqe_hash_elem_ * elem) 
{
    hash_err_t err;

    HASH_SET_ERR(err, HASH_OK);

    if (elem) {
        if (elem->key.valptr) {
            free(elem->key.valptr);
        } else {
            HASH_SET_ERR(err, HASH_KEY_IS_NULL);
        }
        free(elem);
    } else {
        HASH_SET_ERR(err, HASH_ELEM_IS_NULL);
    } 

    vqe_hash_log_err(&err);
    return (HASH_GET_ERR_TYPE(err) == HASH_OK);
}

/*! 
 * Create hash table. 
 * @return Returns a pointer to the hash-table if creation was 
 * successful. Returns null if errors were encountered during creation.
 * It's the caller's responsibility to handle the error case.
 */ 
vqe_hash_t *vqe_hash_create (int32_t buckets,
                               vqe_hash_func func,
                               vqe_hash_key_compare_func cf)
{
    vqe_hash_t *h_table = NULL;
    hash_err_t err;
    int i;

    HASH_SET_ERR(err, HASH_OK);

    if (buckets <= VQE_HASH_BUCKETS_MAX && func) {
        h_table = (vqe_hash_t *)calloc(1, sizeof(vqe_hash_t));
        if (h_table) {
            h_table->buckets = buckets;
            h_table->hash_func = func;
            h_table->mask = 0;
            h_table->__func_table = &vqe_hash_fcn_table;
            if (cf) {
                h_table->comp_func = cf;
            } else {
                h_table->comp_func = vqe_hash_key_compare;
            }
            h_table->bkts = (vqe_hash_tqh_t *)calloc(h_table->buckets, 
                                                     sizeof(vqe_hash_tqh_t));
            if (h_table->bkts) {
                for (i=0; i<buckets; i++) {
                    VQE_TAILQ_INIT(&h_table->bkts[i]);
                }
            } else {
                HASH_SET_ERR(err, HASH_MALLOC_FAILED);
            }
        } else {
            HASH_SET_ERR(err, HASH_MALLOC_FAILED);
        }
    } else {
        if (buckets > VQE_HASH_BUCKETS_MAX) {
            HASH_SET_ERR(err, HASH_BUCKET_LIMIT);
        } else if (!func) {
            HASH_SET_ERR(err, HASH_MISSING_HASH_FUNC);
        }
    }

    vqe_hash_log_err(&err);
    return (h_table);
}
 
/*! 
 * Destroy an existing hash table and optionally destroy its elements 
 * @return Returns true if the hash table existed, and was 
 * successfully destroyed. False is returned otherwise. It's 
 * the caller's responsibility to handle the error case.
 */ 
boolean vqe_hash_destroy_x (struct vqe_hash_ *h_table, boolean destroy_elem_flag) 
{
    int32_t i;
    vqe_hash_elem_t *cel, *nel;
    hash_err_t err;

    HASH_SET_ERR(err, HASH_OK);

    if (h_table && h_table->bkts) {
        for (i = 0; i < h_table->buckets; i++) {
            VQE_TAILQ_FOREACH_SAFE(cel, &h_table->bkts[i], next, nel) {
                VQE_TAILQ_REMOVE(&h_table->bkts[i], cel, next);
                if (destroy_elem_flag) {
                    (void)vqe_hash_elem_destroy(cel);
                }

            }
        }
        free(h_table->bkts);
        free(h_table);
    } else {
        HASH_SET_ERR(err, HASH_TABLE_IS_NULL);
    }

    vqe_hash_log_err(&err);
    return (HASH_GET_ERR_TYPE(err) == HASH_OK);
}

/*! 
 * Destroy an existing hash table and all its elements
 * @return Returns true if the hash table existed, and was 
 * successfully destroyed. False is returned otherwise. It's 
 * the caller's responsibility to handle the error case.
 */ 
boolean vqe_hash_destroy (struct vqe_hash_ *h_table) 
{
    boolean destroy_elem_flag = TRUE; 
    return(vqe_hash_destroy_x(h_table, destroy_elem_flag)); 
}

/*! 
 * Search hash table for elem with key.
 * @returns Returns a pointer to the hash-element which has the given key 
 * on success. If no such element exists or there is an error in the search
 * the function returns a null pointer. hash_get_elem is invoked via
 * the mcall interface, and from within this file - in either case we
 * assume that the hash-table cannot be null since it is part of the mcall.
 */ 
vqe_hash_elem_t *vqe_hash_get_elem_x (struct vqe_hash_ *h_table,
                                      const vqe_hash_key_t *key,
                                      vqe_hash_elem_t *elem) 
{
    uint32_t hash;
    vqe_hash_elem_t *cel, *nel, *result = NULL;
    hash_err_t err;

    HASH_SET_ERR(err, HASH_OK);

    if (key && key->valptr && key->len) {
        hash = h_table->hash_func((uint8_t *)key->valptr, key->len, 0);
        if (hash < h_table->buckets) {
            VQE_TAILQ_FOREACH_REVERSE_SAFE(cel, &h_table->bkts[hash], 
                                       vqe_hash_tqh_, next, nel) {
                if (h_table->comp_func(key, &cel->key) && 
                    (!elem || (elem == cel))) {
                    result = cel;
                    break;
                }
            }
        } else {
            HASH_SET_ERR(err, HASH_BUCKET_LIMIT);
        }
    } else {
        if (!key) {
            HASH_SET_ERR(err, HASH_KEY_IS_NULL);
        } else if (!key->valptr) {
            HASH_SET_ERR(err, HASH_KEYVAL_IS_NULL);
        } else if (!key->len) {
            HASH_SET_ERR(err, HASH_INVALID_KEYLEN);
        }
    }
    vqe_hash_log_err(&err);
    return (result);
}

/*! 
 * Search hash table for elem with key.
 * @returns Returns a pointer to the hash-element which has the given key 
 * on success. If no such element exists or there is an error in the search
 * the function returns a null pointer. hash_get_elem is invoked via
 * the mcall interface, and from within this file - in either case we
 * assume that the hash-table cannot be null since it is part of the mcall.
 */ 
vqe_hash_elem_t *vqe_hash_get_elem (struct vqe_hash_ *h_table,
                                    const vqe_hash_key_t *key) 
{
    return(vqe_hash_get_elem_x (h_table, key, NULL));
}


/*! 
 * Search hash table for elem with key.
 * @returns Returns a pointer to the hash-element which has the given key 
 * on success. If no such element exists or there is an error in the search
 * the function returns a null pointer. hash_get_elem is invoked via
 * the mcall interface, and from within this file - in either case we
 * assume that the hash-table cannot be null since it is part of the mcall.
 */ 
vqe_hash_elem_t *vqe_hash_get_elem_exact (struct vqe_hash_ *h_table,
                                          vqe_hash_elem_t *elem) 
{
    vqe_hash_key_t key; 
    vqe_hash_get_elem_key(&key, elem); 
    return(vqe_hash_get_elem_x (h_table, &key, elem));
}

/*! 
 * Return the prev element .
 */ 
vqe_hash_elem_t *vqe_hash_get_prev_elem (struct vqe_hash_ *h_table,
                                         vqe_hash_elem_t *elem) 
{
    vqe_hash_elem_t *prev_el = VQE_TAILQ_PREV(elem, vqe_hash_tqh_, next);
    return(prev_el);
}



/*! 
 * Add elem to hash table. 
 * @return Returns true if the operation succeeds, otherwise
 * returns a false. It's the caller's responsbility to take
 * appropriate action in case of failure. Since the function
 * is invoked via a mcall on a hash-table object, the hash-table
 * ptr cannot be null.
 */ 
#define HASH_FLAGS_DUPS_OK  0x0001
boolean vqe_hash_add_elem_x (struct vqe_hash_ *h_table,
                             vqe_hash_elem_t *elem, uint32_t flags,
                             boolean *dup_flag_ptr) 
{
    uint32_t hash;
    hash_err_t err;
    vqe_hash_elem_t *dup_elem;
    boolean dups_ok = ((flags & HASH_FLAGS_DUPS_OK) != 0); 

    HASH_SET_ERR(err, HASH_OK);

    if (elem && elem->key.valptr && elem->key.len) {
        hash = h_table->hash_func((uint8_t *)elem->key.valptr, 
                                  elem->key.len, 0);
        if (hash < h_table->buckets) {
            dup_elem = vqe_hash_get_elem(h_table, &elem->key);
            if (dups_ok || !dup_elem) {
                VQE_TAILQ_INSERT_HEAD(&h_table->bkts[hash], elem, next);
                if (dup_flag_ptr) {
                    *dup_flag_ptr = dup_elem ? TRUE : FALSE;
                }
            } else {
                HASH_SET_ERR(err, HASH_DUPLICATE_KEY);
            }
        } else {
            HASH_SET_ERR(err, HASH_BUCKET_LIMIT);
        }
    } else {
        if (!elem) {
            HASH_SET_ERR(err, HASH_ELEM_IS_NULL);
        } else if (!elem->key.valptr) {
            HASH_SET_ERR(err, HASH_KEYVAL_IS_NULL);
        } else if (!elem->key.len) {
            HASH_SET_ERR(err, HASH_INVALID_KEYLEN);
        }
    }

    vqe_hash_log_err(&err);
    return (HASH_GET_ERR_TYPE(err) == HASH_OK);
}

boolean vqe_hash_add_elem (struct vqe_hash_ *h_table,
                             vqe_hash_elem_t *elem) 
{
    return(vqe_hash_add_elem_x (h_table, elem, 0, NULL)); 
}


/*! 
 * Add elem to hash table; allow dups 
 * @return Returns true if the operation succeeds, otherwise
 * returns a false. It's the caller's responsbility to take
 * appropriate action in case of failure. Since the function
 * is invoked via a mcall on a hash-table object, the hash-table
 * ptr cannot be null.
 */ 
boolean vqe_hash_add_elem_dups_ok (struct vqe_hash_ *h_table,
                                   vqe_hash_elem_t *elem, 
                                   boolean *dup_flag) 
{
    uint32_t flags = HASH_FLAGS_DUPS_OK;
    return(vqe_hash_add_elem_x (h_table,elem, flags, dup_flag)); 
}


/*! 
 * Del elem from hash table.
 * @return Returns true if the operation succeeds, otherwise
 * returns a false. It's the caller's responsbility to take
 * appropriate action in case of failure. Since the function
 * is invoked via a mcall on a hash-table object, the hash-table
 * ptr cannot be null.
 */ 
boolean vqe_hash_del_elem (struct vqe_hash_ *h_table,
                            vqe_hash_elem_t *elem) 
{
    uint32_t hash;
    hash_err_t err;

    HASH_SET_ERR(err, HASH_OK);

   if (elem && elem->key.valptr && elem->key.len) { 
        hash = h_table->hash_func((uint8_t *)elem->key.valptr, 
                                  elem->key.len, 0);
        if (vqe_hash_get_elem_exact(h_table, elem)) {
            VQE_TAILQ_REMOVE(&h_table->bkts[hash], elem, next);
            vqe_hash_elem_init_list_hdr(elem);
        } else {
            HASH_SET_ERR(err, HASH_NO_SUCH_ELEM);
        }
     } else {
        if (!elem) {
            HASH_SET_ERR(err, HASH_ELEM_IS_NULL);
        } else if (!elem->key.valptr) {
            HASH_SET_ERR(err, HASH_KEYVAL_IS_NULL);
        } else if (!elem->key.len) {
            HASH_SET_ERR(err, HASH_INVALID_KEYLEN);
        }
    }

    vqe_hash_log_err(&err);
    return (HASH_GET_ERR_TYPE(err) == HASH_OK);
}  

/*! 
 * Display hash table entries. 
 * Since the function is invoked via the mcall interface, h_table
 * cannot be null, since the function is invoked by dereferencing
 * h_table in the first place.
 */ 
void vqe_hash_display (struct vqe_hash_ *h_table) 
{
    int i;
    vqe_hash_elem_t *cel, *nel;

    CONSOLE_PRINTF("-- Hash Table Entries --\n");
    
    for (i = 0; i < h_table->buckets; i++) {
        CONSOLE_PRINTF("\t\t Bucket %d \n", i);        
        VQE_TAILQ_FOREACH_SAFE(cel, &h_table->bkts[i], next, nel) {
            MCALL(cel, vqe_hash_elem_display);
        }
    }
}

/*!
  Set the function table of hash module
  @param[in] table hash module function table.
 */
void vqe_hash_set_fcns (vqe_hash_fcns_t * table)
{
    table->vqe_hash_add_elem = vqe_hash_add_elem;
    table->vqe_hash_del_elem = vqe_hash_del_elem;
    table->vqe_hash_get_elem = vqe_hash_get_elem;
    table->vqe_hash_display  = vqe_hash_display;
    table->vqe_hash_add_elem_dups_ok = vqe_hash_add_elem_dups_ok;
    table->vqe_hash_get_elem_exact = vqe_hash_get_elem_exact;
    table->vqe_hash_get_prev_elem = vqe_hash_get_prev_elem;

}

/*!
  Init hash module. Basically it just sets the hash module function table.
*/
void init_vqe_hash_module (void) 
{
    if (vqe_hash_module_initialized) {
        return;
    }
    
    vqe_hash_set_fcns(&vqe_hash_fcn_table);
    vqe_hash_module_initialized = TRUE;
}

