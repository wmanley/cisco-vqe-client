/*
 *------------------------------------------------------------------
 * I D _ M A N A G E R _ P R I V A T E . H
 *
 * October 1997
 * Copyright (c) 1997-2002, 2005, 2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __ID_MANAGER_PRIVATE_H__
#define __ID_MANAGER_PRIVATE_H__

#include "utils/vam_types.h"
#include "utils/queue_plus.h"

extern vqe_mutex_t gl_idmgr_mutex;
#define LOCK_IDMGR() VQE_MUTEX_LOCK(&gl_idmgr_mutex)
#define UNLOCK_IDMGR() VQE_MUTEX_UNLOCK(&gl_idmgr_mutex)

#define NUMBER_OF_ENTRIES_PER_SLOT_MAX  	(1 << 16)
#define NUMBER_OF_ENTRIES_PER_SLOT_DEFAULT  	1024

#define NUMBER_OF_TABLE_SLOTS_MAX		(1 << 8)
#define NUMBER_OF_TABLE_SLOTS_DEFAULT		32

typedef unsigned char uchar;

/*
 * This bit is used for non-randomized ID Table
 * users, to validate lookup of deleted IDs
 *
 * This Bit is stored in the tableSequence, Which will
 * be always be 0x00 for the non-randomized ID Table users.
 */
#define ID_IN_USE_BIT 0x80

typedef struct _entry_t {
   union {
      struct _entry_t *next;
      void  *user_p;
   } u;
   uchar tableSequence;
   uchar tableNumber;
   VQE_TAILQ_ENTRY(_entry_t) list_obj;
} entry_t;

#endif /* __ID_MANAGER_PRIVATE_H_ */
