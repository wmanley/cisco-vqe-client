/**------------------------------------------------------------------------
 * @brief
 * Extensions of the tree.h rbtree implementation.
 *
 * @file
 * tree_plus.h
 *
 * April 2007, Mike Lague.
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *-------------------------------------------------------------------------
 */

#ifndef __TREE_PLUS_H__
#define __TREE_PLUS_H__

#include "../add-ons/include/tree.h"

/* 
 * Extend the rbtree implementation to have additional functions:
 *  - a function to return the entry with the smallest key greater than
 *    or equal to a specified key.
 *  - a function to return the entry with the greatest key less than
 *    or equal to a specified key.
 *
 * Macros which "override" existing macros in the tree.h package have the
 * prefix "RBP_", for "red/black plus".
 */

#define VQE_RBP_PROTOTYPE(name, type, field, cmp)                    \
    VQE_RB_PROTOTYPE(name, type, field, cmp);                        \
    struct type *name##_RB_FINDGE(struct name *, struct type *); \
    struct type *name##_RB_FINDLE(struct name *, struct type *); 

#define VQE_RBP_GENERATE(name, type, field, cmp)                            \
    VQE_RB_GENERATE(name, type, field, cmp)                                 \
                                                                        \
/* Finds the node with the smallest key greater than or equal to elm */ \
struct type *								\
name##_RB_FINDGE(struct name *head, struct type *elm)			\
{									\
	struct type *tmp = VQE_RB_ROOT(head);				\
    struct type *best = NULL;                                       \
	int comp;							\
	while (tmp) {							\
		comp = cmp(elm, tmp);					\
		if (comp < 0) {                                         \
            best = tmp;   /* our key less than curr node */             \
            /* nodes less than ours are on the left */                  \
            tmp = VQE_RB_LEFT(tmp, field);                                  \
        } else if (comp > 0) {                                          \
            tmp = VQE_RB_RIGHT(tmp, field);                                 \
        } else {                                                        \
            return (tmp);                                               \
        }                                                               \
	}								\
	return (best);							\
}									                                    \
\
/* Finds the node with the greatest key less than or equal to elm */    \
struct type *								\
name##_RB_FINDLE(struct name *head, struct type *elm)			\
{									\
	struct type *tmp = VQE_RB_ROOT(head);				\
    struct type *best = NULL;                                       \
	int comp;							\
	while (tmp) {							\
		comp = cmp(elm, tmp);					\
		if (comp > 0) {                                         \
            best = tmp;   /* our key greater than curr node */          \
            /* nodes greater than ours are on the right */              \
            tmp = VQE_RB_RIGHT(tmp, field);                                 \
        } else if (comp < 0) {                                          \
            tmp = VQE_RB_LEFT(tmp, field);                                  \
        } else {                                                        \
            return (tmp);                                               \
        }                                                               \
	}								\
	return (best);							\
}									

#define VQE_RB_FINDGE(name, x, y)	name##_RB_FINDGE(x, y)
#define VQE_RB_FINDLE(name, x, y)	name##_RB_FINDLE(x, y)

#endif /* __TREE_PLUS_H__ */
