/*
 *------------------------------------------------------------------
 * I D _ M A N A G E R . C
 *
 * October 1997
 * Copyright (c) 1997-1998, 2001-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

/*
 *  I n c l u d e  F i l e s
 */

#include "id_manager_private.h"
#include "utils/id_manager.h"
#include "utils/queue_plus.h"

/* Define the following to get devel debug printouts.  Will spoil
 * real-time behavior.*/
#define PRINTF(args...)

/*
 * Multiple ID manager tables can be created upon request for exlusive
 * use by different components. A maximun of ID_MGR_TABLE_KEY_MAX is
 * allowed.
 */

entry_t **g_id_manager_table[ID_MGR_TABLE_KEY_MAX];
uint numTableSlot_max[ID_MGR_TABLE_KEY_MAX];
uint numIDsPerSLot_max[ID_MGR_TABLE_KEY_MAX];
static ushort id_table_flags[ID_MGR_TABLE_KEY_MAX];

/* 
 * id_table_flags can be:
 */
#define ID_TABLE_NO_RANDOM_FLAG 0x1

/* Added for VAM random number generation */
static uint rand_seed = 0x12345678;
#define random_gen() VQE_RAND_R(&rand_seed)

static boolean id_manager_inited = FALSE;
static VQE_TAILQ_HEAD(, _entry_t) g_free_q[ID_MGR_TABLE_KEY_MAX];

/* Not defined for VAM */
#define SAVE_CALLER()

VQE_MUTEX_DEFINE(gl_idmgr_mutex);

static inline entry_t * id_get_next_entry_p (id_table_key_t table_key)
{
    entry_t * result;

    result = (entry_t *) VQE_TAILQ_FIRST(&g_free_q[table_key]);
    if (result)
        VQE_TAILQ_REMOVE(&g_free_q[table_key], result, list_obj);
    return result;
}

static inline void id_enqueue_entry_p (entry_t * entry_p,
				       id_table_key_t table_key)
{
    VQE_TAILQ_INSERT_TAIL(&g_free_q[table_key], entry_p, list_obj);
}

static inline boolean id_table_randomize_id (id_table_key_t table_key)
{
    return (!(id_table_flags[table_key] & ID_TABLE_NO_RANDOM_FLAG));
}

boolean id_table_set_no_randomize (id_table_key_t table_key)
{
    LOCK_IDMGR();
    if ((table_key >= ID_MGR_TABLE_KEY_MAX) || 
       (!g_id_manager_table[table_key])) {
        UNLOCK_IDMGR();
	return (FALSE);
    }
    id_table_flags[table_key] |= ID_TABLE_NO_RANDOM_FLAG;
    UNLOCK_IDMGR();
    return (TRUE);
}

static boolean id_manager_module_init (id_table_key_t table_key,
				       uint numTableSlot,
				       uint numIDsPerSLot)
{
   uint i;
   entry_t **id_mgr_table;

   VQE_TAILQ_INIT(&g_free_q[table_key]);

   id_mgr_table = (entry_t **)VQE_MALLOC_ATOMIC(sizeof(entry_t *) * numTableSlot);
   if (!id_mgr_table) {
       PRINTF("IDMGR: Malloc failure - %s\n", __FUNCTION__); 
      return(FALSE);
   }

   g_id_manager_table[table_key] = id_mgr_table;
   numTableSlot_max[table_key] = numTableSlot;
   numIDsPerSLot_max[table_key] = numIDsPerSLot;
   id_table_flags[table_key] = 0; /* Clear All Flags */

   for (i = 0; i < numTableSlot; i++) {
       id_mgr_table[i] = (entry_t *)NULL;
   }

   return(TRUE);
}


/* 
 * Update the entry_t and return the ID.
 */

static idmgr_id_t id_assign_ptr (entry_t *entry_p, void *p, id_table_key_t table_key)
{

   entry_p->u.user_p = p;
   if (id_table_randomize_id(table_key)) {
       return ((entry_p->tableSequence << 24) |
	       (entry_p->tableNumber << 16) |
	       (entry_p - g_id_manager_table[table_key][entry_p->tableNumber]));
   } else {
	/* 
	 * Always Zero for non-randomized ID Table users 
	 */
       uchar actual_tableSequence = 0; 
       /*
	* Also mark that this ID is in use now
	*/
       entry_p->tableSequence |= ID_IN_USE_BIT;
       return ((actual_tableSequence << 24) |
	       (entry_p->tableNumber << 16) |
	       (entry_p - g_id_manager_table[table_key][entry_p->tableNumber]));
   }
}

/*
 * id_create_new_table
 *
 * Randomly create a new ID table key and allocate a new ID table. The new
 * table can hold (numTableSlot x numIDsPerSLot) many ID entries
 * and would allcoate numIDsPerSLot entries each time when the new table
 * runs out of free ID entries. 
 */
id_table_key_t id_create_new_table (uint numTableSlot,
				    uint numIDsPerSLot)
{
   id_table_key_t new_table_key;
   uint i;

   SAVE_CALLER();

   if ((numTableSlot > NUMBER_OF_TABLE_SLOTS_MAX) || (numTableSlot == 0) ||
       (numIDsPerSLot > NUMBER_OF_ENTRIES_PER_SLOT_MAX) ||
       (numIDsPerSLot < 2)) {
      PRINTF("IDMGR: Invalid table size - %s\n", __FUNCTION__);
      return(ID_MGR_TABLE_KEY_ILLEGAL);
   }

   LOCK_IDMGR();

   /*
    * Initiate the master table of ID tables.
    */
   if (!id_manager_inited) {
      for (i = 0; i < ID_MGR_TABLE_KEY_MAX; i++) {
	 g_id_manager_table[i] = (entry_t **)NULL;
      }
      id_manager_inited = TRUE;
   }

   /*
    * Randomly find the next unused table ID entry.
    */
   new_table_key = ((id_table_key_t)random_gen()) % ID_MGR_TABLE_KEY_MAX;
   for (i = 0; i < ID_MGR_TABLE_KEY_MAX; i++) {
      if (new_table_key == ID_MGR_TABLE_KEY_MAX) {
	 new_table_key = 0;
      }
      if ((new_table_key != ID_MGR_TABLE_KEY_DEFAULT) &&
	  (!g_id_manager_table[new_table_key])) {
	 break;
      }
      new_table_key++;
   }
   if (i >= ID_MGR_TABLE_KEY_MAX) {
      PRINTF("IDMGR: No more entries - %s\n", __FUNCTION__);
      UNLOCK_IDMGR();
      return(ID_MGR_TABLE_KEY_ILLEGAL);
   }

   if (!id_manager_module_init(new_table_key,
			       numTableSlot,
			       numIDsPerSLot)) {
       UNLOCK_IDMGR();
       return(ID_MGR_TABLE_KEY_ILLEGAL);
   }

   UNLOCK_IDMGR();
   return(new_table_key);
}

void id_destroy_table (id_table_key_t table_key)
{
   uint i;
   entry_t **id_mgr_table;
   uint numTableSlot;
   entry_t *entry_p;

   LOCK_IDMGR();

   if ((table_key >= ID_MGR_TABLE_KEY_MAX) || 
       (!g_id_manager_table[table_key])) {
      PRINTF("IDMGR: Bad table id - %s\n", __FUNCTION__);
      UNLOCK_IDMGR();
      return;
   }

   while ((entry_p = VQE_TAILQ_FIRST(&g_free_q[table_key])))
       VQE_TAILQ_REMOVE(&g_free_q[table_key], entry_p, list_obj);

   numTableSlot = numTableSlot_max[table_key];
   id_mgr_table = g_id_manager_table[table_key];
   for (i = 0; i < numTableSlot; i++) {
      if (*id_mgr_table) {
	 VQE_FREE(*id_mgr_table);
      }
      id_mgr_table++;
   }

   VQE_FREE(g_id_manager_table[table_key]);

   g_id_manager_table[table_key] = (entry_t **)NULL;
   numTableSlot_max[table_key] = 0;
   numIDsPerSLot_max[table_key] = 0;

   UNLOCK_IDMGR();
}

uint id_reserve (void * p, const id_table_key_t table_key, idmgr_id_t id_arg)
{
    entry_t *entry_p, *compare_entry_p, *tmp_p;
    uchar   tableSequence;
    uchar   tableNumber;
    ushort  tableIndex;
    idmgr_id_t actual_id;
    entry_t **id_mgr_table;
    uint numIDsPerSLot;
    entry_t *last_p;
    idmgr_id_t id;
    boolean found = FALSE;

    SAVE_CALLER();

    actual_id = id_arg - APP_ID_OFFSET;

    if ((id_arg == 0) || (table_key >= ID_MGR_TABLE_KEY_MAX)){
        PRINTF("Bad id - %s (0x%lx)\n", __FUNCTION__, id_arg);
	return (ID_MGR_INVALID_ID);
    }

    tableSequence = (actual_id >> 24);
    tableNumber   = (actual_id >> 16) & 0xFF;
    tableIndex   = (actual_id & 0xFFFF);

    if ((tableNumber >= numTableSlot_max[table_key]) ||
        (tableIndex >= numIDsPerSLot_max[table_key])) {
        PRINTF("Bad id - %s (0x%lx)\n", __FUNCTION__, id_arg);
        return (ID_MGR_INVALID_ID);
    }

    LOCK_IDMGR();

    /*
     * For non-randomized id tables, tableSequence has to be zero !!
     */
    if ((!id_table_randomize_id(table_key) && (tableSequence != 0))) {
        PRINTF("Id out of range - %s (0x%lx)\n", __FUNCTION__, id_arg);
        UNLOCK_IDMGR();
	return (ID_MGR_INVALID_ID);
    }

    id_mgr_table = g_id_manager_table[table_key];

    if (id_mgr_table == NULL) {
        /* 
         * Table should have been created first.
         */
        PRINTF("Unassigned table id - %s (0x%lx)\n", __FUNCTION__, id_arg);
        UNLOCK_IDMGR();
        return (ID_MGR_UNINITIALIZED);
    }

    entry_p = id_mgr_table[tableNumber];
    numIDsPerSLot = numIDsPerSLot_max[table_key];
    
    if (entry_p == NULL) {
        id_mgr_table[tableNumber] = 
            VQE_MALLOC_ATOMIC(sizeof(entry_t) * numIDsPerSLot);
        if (id_mgr_table[tableNumber] == NULL) {
            PRINTF("Malloc failure - %s (0x%lx)\n", __FUNCTION__, id_arg);
            UNLOCK_IDMGR();
            return (ID_MGR_RET_ERROR);
        }

        last_p = &id_mgr_table[tableNumber][numIDsPerSLot];
        entry_p = id_mgr_table[tableNumber];

        compare_entry_p = entry_p + tableIndex;

        /* Now, we link all the entries onto the free list */
        for ( ; entry_p < last_p; entry_p++) {

            entry_p->u.next = NULL;
            entry_p->tableSequence = tableSequence;
            entry_p->tableNumber = tableNumber;
	    /*
	     * Can be optimized to avoid comparing every time
	     */
            if (compare_entry_p != entry_p) {
                id_enqueue_entry_p(entry_p, table_key);
            }
        }
    } else {
        compare_entry_p = entry_p + tableIndex;

	if (!id_table_randomize_id(table_key)) {
	    if (compare_entry_p->tableSequence & ID_IN_USE_BIT) {
                UNLOCK_IDMGR();
		return (ID_IN_USE);
	    }
            VQE_TAILQ_FOREACH(tmp_p, &g_free_q[table_key], list_obj)
            {
                if (tmp_p == compare_entry_p)
                {
                    found = TRUE;
                    break;
                }
            }
            if (found)
            {
                VQE_TAILQ_REMOVE(&g_free_q[table_key], 
				 compare_entry_p, list_obj);
            }
            else
            {
                PRINTF("Id is not in use, but not in free Q ?!!? - %s (0x%lx)\n", 
                       __FUNCTION__, id_arg);
                UNLOCK_IDMGR();
		return (ID_IN_USE);
	    }
	} else {
            VQE_TAILQ_FOREACH(tmp_p, &g_free_q[table_key], list_obj)
            {
                if (tmp_p == compare_entry_p)
                {
                    found = TRUE;
                    break;
                }
            }
            if (found)
            {
                VQE_TAILQ_REMOVE(&g_free_q[table_key], 
				 compare_entry_p, list_obj);
            }
            else 
            {
                UNLOCK_IDMGR();
		return (ID_IN_USE);
	    }
	    /*
	     * Ok, we got the entry_p out of free_q, now, set the
	     * sequence to the reserving sequence
	     */
	    compare_entry_p->tableSequence = tableSequence;
	}
    }

    id = id_assign_ptr(compare_entry_p, p, table_key);
    UNLOCK_IDMGR();
    return (ID_MGR_RET_OK);
}
/*
 * Allocate a spot in the ID table, and install the
 * pointer. Give back the idmgr_id_t, used to quickly
 * retrieve the pointer.
 */

idmgr_id_t id_get (void * p, id_table_key_t table_key)
{
   entry_t **id_mgr_table;
   uint tabSlot;
   uint numIDsPerSLot;
   entry_t *entry_p;
   entry_t *last_p;
   idmgr_id_t id;
   idmgr_id_t id_for_app;
   uint i;

   SAVE_CALLER();

   if (table_key >= ID_MGR_TABLE_KEY_MAX) {
       PRINTF("IDMGR: Bad table id - %s\n", __FUNCTION__);
      return(ID_MGR_INVALID_HANDLE);
   }

   LOCK_IDMGR();

   /*
    * Initiate the master table of ID tables.
    */
   if (!id_manager_inited) {
      for (i = 0; i < ID_MGR_TABLE_KEY_MAX; i++) {
	 g_id_manager_table[i] = (entry_t **)NULL;
      }
      id_manager_inited = TRUE;
   }

   if (g_id_manager_table[table_key] == NULL) {
      /*
       * Initiate the default ID table if it is to be accessed.
       */
      if (table_key == ID_MGR_TABLE_KEY_DEFAULT) {
	 if (!id_manager_module_init(table_key, 
				     NUMBER_OF_TABLE_SLOTS_DEFAULT,
				     NUMBER_OF_ENTRIES_PER_SLOT_DEFAULT)) {
             UNLOCK_IDMGR();
	    return(ID_MGR_INVALID_HANDLE);
	 }
      } else {
	 /*
	  * Other ID tables must be explicitly initialized.
	  */
          PRINTF("IDMGR: Unassigned table id - %s\n", __FUNCTION__);
          UNLOCK_IDMGR();
	 return(ID_MGR_INVALID_HANDLE);
      }
   }
   id_mgr_table = g_id_manager_table[table_key];
   numIDsPerSLot = numIDsPerSLot_max[table_key];
   tabSlot = numTableSlot_max[table_key];

   entry_p = id_get_next_entry_p(table_key);
   if (entry_p == NULL) {
      /* We need to allocate an entry table */
      
      for (i = 0; i < tabSlot; i++) {
          if (id_mgr_table[i] == NULL) {
             /* Found an empty slot */
             id_mgr_table[i] = VQE_MALLOC_ATOMIC(sizeof(entry_t) * numIDsPerSLot);
             if (id_mgr_table[i] == NULL) {
                 PRINTF("IDMGR: Malloc failure - %s\n", __FUNCTION__);
                break;
             } else {
                  /* If we got some memory */
                  last_p = &id_mgr_table[i][numIDsPerSLot];
                  entry_p = id_mgr_table[i];

                  /* Now, we link all the entries onto the free list */
                  for (; entry_p < last_p; entry_p++) {
                      entry_p->u.next = NULL;
		      if (id_table_randomize_id(table_key)) {
                          entry_p->tableSequence = random_gen() % 0x100;
                      } else {
                          entry_p->tableSequence = 0;
                      }
                      entry_p->tableNumber = i;
                      id_enqueue_entry_p(entry_p, table_key);
                  }
  
                  /* Now, try to allocate one after we've packed the list */
                  entry_p = id_get_next_entry_p(table_key);

		  break;
             }
          }
      } 

	/*
	 * Exhausted our table or memory.
	 */
	if (entry_p == NULL) {
            PRINTF("IDMGR: Out of Ids! - %s\n", __FUNCTION__);
            UNLOCK_IDMGR();
	    return (ID_MGR_INVALID_HANDLE);
	}
    }

    id = id_assign_ptr(entry_p, p, table_key);

    id_for_app = id + APP_ID_OFFSET;
    if (id_for_app < id) {
	// Wrap around has occurred
	// Meaning, we are out of ids
        PRINTF("IDMGR: Out of Ids! - %s\n", __FUNCTION__);
        UNLOCK_IDMGR();
	return ID_MGR_INVALID_HANDLE;
    }

    id = id_for_app;
    
    /*
     * Do this auto incr. of tableSeq. only for randomized id tables
     */
    if (id_mgr_is_invalid_handle(id) && id_table_randomize_id(table_key)) {
	/*
	 * Increment idmgr_id_t to ensure that ID_MGR_INVALID_HANDLE can never be
	 * returned i.e.
	 *
	 *     id_assign_ptr() returns 0x00000000 above
	 *
	 * we would increment the table sequence number so that
	 *
	 *     id_assign_ptr() returns 0x01000000 below
	 */
	entry_p->tableSequence++;
	id = id_assign_ptr(entry_p, p, table_key);
	id += APP_ID_OFFSET;
    }
    UNLOCK_IDMGR();
    return (id);
}

/*
 * Return the pointer associated with the idmgr_id_t passed
 * in.
 */

void * id_to_ptr (idmgr_id_t id, uint *ret_code, const id_table_key_t table_key)
{
    entry_t *entry_p;
    uchar   tableSequence;
    uchar   tableNumber;
    ushort  tableIndex;
    idmgr_id_t actual_id;

    actual_id = id - APP_ID_OFFSET;

    if ((id == 0) || (actual_id > id)) {
        PRINTF("Bad id - %s (0x%lx)\n", __FUNCTION__, id);
	*ret_code = ID_MGR_INVALID_ID;
	return (NULL);
    }

    LOCK_IDMGR();

    if ((table_key >= ID_MGR_TABLE_KEY_MAX) || 
	(!g_id_manager_table[table_key])) {
        PRINTF("Bad table id - %s (0x%lx)\n", __FUNCTION__, id);
	if (ret_code) {
	    *ret_code = ID_MGR_INVALID_ID;
	}
        UNLOCK_IDMGR();
        return (NULL);
    }

    tableSequence = (actual_id >> 24);
    tableNumber   = (actual_id >> 16) & 0xFF;
    tableIndex   = (actual_id & 0xFFFF);

    /* Do some sanity checks */
    entry_p = g_id_manager_table[table_key][tableNumber];

    if ((tableNumber >= numTableSlot_max[table_key]) || (entry_p == NULL) ||
        (tableIndex >= numIDsPerSLot_max[table_key])) {
        PRINTF("Bad id - %s (0x%lx)\n", __FUNCTION__, id);
        /* This is a bad ID, since the table entry should never be NULL */
	if (ret_code) {
	    *ret_code = ID_MGR_RET_ERROR;
	}
        UNLOCK_IDMGR();
        return (NULL);
    }
    entry_p += tableIndex;
    if (id_table_randomize_id(table_key)) {
	if (entry_p->tableSequence != tableSequence) {
	    /* This is the situation where the ID has been retired */
	    if (ret_code) {
		*ret_code = ID_MGR_RET_RETIRED;
	    }
            UNLOCK_IDMGR();
	    return (NULL);
	}
    } else {
	/*
	 * For non-randomized IDs, we need to check the ID_IN_USE_BIT
	 * to say whether the ID has been retired or not
	 */
	if (!(entry_p->tableSequence & ID_IN_USE_BIT)) {
	    if (ret_code) {
		*ret_code = ID_MGR_RET_RETIRED;
	    }
            UNLOCK_IDMGR();
	    return (NULL);
	}
    }
    if (ret_code) {
	*ret_code = ID_MGR_RET_OK;
    }
    UNLOCK_IDMGR();
    return (entry_p->u.user_p);
}

/*
 * Make the pointer inaccessible using this ID.
 */

void  id_delete (idmgr_id_t id, id_table_key_t table_key)
{
   entry_t * entry_p;
   uchar tableSequence;
   uchar tableNumber;
   ushort tableIndex;
   idmgr_id_t actual_id;

   actual_id = id - APP_ID_OFFSET;

   if ((id == 0) || (actual_id > id)) {
       PRINTF("Bad id - %s (0x%lx)\n", __FUNCTION__, id);
	return;
   }
   id = actual_id;

   LOCK_IDMGR();

   if ((table_key >= ID_MGR_TABLE_KEY_MAX) || 
       (!g_id_manager_table[table_key])) {
       PRINTF("Bad table key - %s (0x%lx)\n", __FUNCTION__, id);
       UNLOCK_IDMGR();
      return;
   }

   tableSequence = (id >> 24);
   tableNumber   = (id >> 16) & 0xFF;
   tableIndex   = (id & 0xFFFF);

   /* Do some sanity checks */


    entry_p = g_id_manager_table[table_key][tableNumber];
    if ((tableNumber >= numTableSlot_max[table_key]) || (entry_p == NULL) ||
        (tableIndex >= numIDsPerSLot_max[table_key])) {
        PRINTF("Bad id - %s (0x%lx)\n", __FUNCTION__, id);
        UNLOCK_IDMGR();
        return;
    }

    entry_p += tableIndex;
    if (id_table_randomize_id(table_key)) {
	if (entry_p->tableSequence == tableSequence) {
	    /* Its a valid ID, so we can retire it */
	    entry_p->u.user_p = NULL;
	    entry_p->tableSequence++;
	    id_enqueue_entry_p(entry_p, table_key);
	}
    } else {
	/*
	 * Check whether this entry is in use
	 */
	if (entry_p->tableSequence & ID_IN_USE_BIT) {
	    /*
	     * For non-randomized ID Table Users, we need to
	     * clear the ID_IN_USE_BIT - to indicate that this
	     * id is retired.
	     */
	    entry_p->tableSequence &= ~((uchar)ID_IN_USE_BIT);
	    entry_p->u.user_p = NULL;
	    entry_p->tableSequence++;
	    id_enqueue_entry_p(entry_p, table_key);
	}
    }
    UNLOCK_IDMGR();
}
