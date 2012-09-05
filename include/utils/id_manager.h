/*
 *------------------------------------------------------------------
 * I D _ M A N A G E R . H
 *
 * October 1997
 * Copyright (c) 1997-2006, 2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef	_ID_MANAGER_H_
#define _ID_MANAGER_H_

#include "vam_types.h"
#include "../../utils/id_manager_private.h"

#define ILLEGAL_ID                  ((unsigned long int) 0xFFFFFFFF)
#define RETIRED_ID                  ((unsigned long int) 0xFFFFFFFE)
#define ID_OK                       ((unsigned long int) 0x0)
#define APP_ID_OFFSET               (0x1)
#define NULL_ID                     ((unsigned long int) 0x0)

#define ID_MGR_TABLE_KEY_DEFAULT    0
#define ID_MGR_TABLE_KEY_ILLEGAL    0xFFFF	/* Larger than any valid key */
//#define ID_MGR_TABLE_KEY_MAX   	    512
#define ID_MGR_TABLE_KEY_MAX        10

extern entry_t **g_id_manager_table[ID_MGR_TABLE_KEY_MAX];
extern unsigned int numTableSlot_max[ID_MGR_TABLE_KEY_MAX];
extern unsigned int numIDsPerSLot_max[ID_MGR_TABLE_KEY_MAX];

/*
 * id_t has been renamed to idmgr_id_t in order to avoid
 * conflict with regular UNIX (solaris, neutrino) id_t definition.
 */
typedef unsigned long int idmgr_id_t;
typedef unsigned short id_table_key_t;

/*
 * Possible return codes for id_to_ptr()s "ret_code":
 */
typedef enum id_mgr_ret_t {
    ID_MGR_RET_OK,
    ID_MGR_RET_ERROR,
    ID_MGR_RET_RETIRED,
    ID_MGR_INVALID_ID,
    ID_MGR_UNINITIALIZED,
    ID_IN_USE,
} id_mgr_ret;

extern void *id_to_ptr(idmgr_id_t id,
		       id_mgr_ret *ret_code,
		       id_table_key_t table_key);
/*
 * Routines provided to test the validity of handles returned.
 */
static const idmgr_id_t ID_MGR_INVALID_HANDLE = 0;

static inline boolean id_mgr_is_invalid_handle (const idmgr_id_t id)
{
    return (id == ID_MGR_INVALID_HANDLE);
}

/*
 * id_to_ptr_inline
 *
 * id_to_ptr_inline() is a faster version of id_to_ptr() and is more suitable
 * for inlining. However it has limitations over id_to_ptr():
 *
 * - no sanity checks performed on table_key
 * - no error reporting
 * - no return of error code (other than NULL pointer)
 *
 * Ideally, in your inline client code, if id_to_ptr_inline() fails, you will
 * break out to call the slower non inlined id_to_ptr(). Speed isn't an issue
 * in the failure case, but speed is, for the normal success case.
 */
static inline void *id_to_ptr_inline (const idmgr_id_t id,
				      const id_table_key_t table_key)
{
    const idmgr_id_t actual_id = id - APP_ID_OFFSET;
    const uchar tableSequence = (actual_id >> 24);
    const uchar tableNumber   = (actual_id >> 16) & 0xFF;
    const unsigned short tableIndex   = (actual_id & 0xFFFF);
    entry_t *entry_p;

    LOCK_IDMGR();

    entry_p = g_id_manager_table[table_key][tableNumber];

    if ((tableNumber >= numTableSlot_max[table_key]) || (entry_p == NULL) ||
        (tableIndex >= numIDsPerSLot_max[table_key])) {
        /* This is a bad ID, since the table entry should never be NULL */
        UNLOCK_IDMGR();
        return (NULL);
    }
    entry_p += tableIndex;
    if (entry_p->tableSequence != tableSequence) {
        /* This is the situation where the ID has been retired */
        UNLOCK_IDMGR();
        return (NULL);
    }
    UNLOCK_IDMGR();
    return (entry_p->u.user_p);
}

extern id_table_key_t id_create_new_table(unsigned int, unsigned int);
extern void id_destroy_table(id_table_key_t);
extern idmgr_id_t id_get(void * p, id_table_key_t);
extern void id_delete(idmgr_id_t, id_table_key_t);

extern unsigned int id_reserve(void *, const id_table_key_t, idmgr_id_t);
extern boolean id_table_set_no_randomize(id_table_key_t table_key);
#endif
