/*	$OpenBSD: tree.h,v 1.7 2002/10/17 21:51:54 art Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_SYS_TREE_H_
#define	_SYS_TREE_H_

/*
 * This file defines data structures for different types of trees:
 * splay trees and red-black trees.
 *
 * A splay tree is a self-organizing data structure.  Every operation
 * on the tree causes a splay to happen.  The splay moves the requested
 * node to the root of the tree and partly rebalances it.
 *
 * This has the benefit that request locality causes faster lookups as
 * the requested nodes move to the top of the tree.  On the other hand,
 * every lookup causes memory writes.
 *
 * The Balance Theorem bounds the total access time for m operations
 * and n inserts on an initially empty tree as O((m + n)lg n).  The
 * amortized cost for a sequence of m accesses to a splay tree is O(lg n);
 *
 * A red-black tree is a binary search tree with the node color as an
 * extra attribute.  It fulfills a set of conditions:
 *	- every search path from the root to a leaf consists of the
 *	  same number of black nodes,
 *	- each red node (except for the root) has a black parent,
 *	- each leaf node is black.
 *
 * Every operation on a red-black tree is bounded as O(lg n).
 * The maximum height of a red-black tree is 2lg (n+1).
 */

#define SPLAY_HEAD(name, type)						\
struct name {								\
	struct type *sph_root; /* root of the tree */			\
}

#define SPLAY_INITIALIZER(root)						\
	{ NULL }

#define SPLAY_INIT(root) do {						\
	(root)->sph_root = NULL;					\
} while (0)

#define SPLAY_ENTRY(type)						\
struct {								\
	struct type *spe_left; /* left element */			\
	struct type *spe_right; /* right element */			\
}

#define SPLAY_LEFT(elm, field)		(elm)->field.spe_left
#define SPLAY_RIGHT(elm, field)		(elm)->field.spe_right
#define SPLAY_ROOT(head)		(head)->sph_root
#define SPLAY_EMPTY(head)		(SPLAY_ROOT(head) == NULL)

/* SPLAY_ROTATE_{LEFT,RIGHT} expect that tmp hold SPLAY_{RIGHT,LEFT} */
#define SPLAY_ROTATE_RIGHT(head, tmp, field) do {			\
	SPLAY_LEFT((head)->sph_root, field) = SPLAY_RIGHT(tmp, field);	\
	SPLAY_RIGHT(tmp, field) = (head)->sph_root;			\
	(head)->sph_root = tmp;						\
} while (0)
	
#define SPLAY_ROTATE_LEFT(head, tmp, field) do {			\
	SPLAY_RIGHT((head)->sph_root, field) = SPLAY_LEFT(tmp, field);	\
	SPLAY_LEFT(tmp, field) = (head)->sph_root;			\
	(head)->sph_root = tmp;						\
} while (0)

#define SPLAY_LINKLEFT(head, tmp, field) do {				\
	SPLAY_LEFT(tmp, field) = (head)->sph_root;			\
	tmp = (head)->sph_root;						\
	(head)->sph_root = SPLAY_LEFT((head)->sph_root, field);		\
} while (0)

#define SPLAY_LINKRIGHT(head, tmp, field) do {				\
	SPLAY_RIGHT(tmp, field) = (head)->sph_root;			\
	tmp = (head)->sph_root;						\
	(head)->sph_root = SPLAY_RIGHT((head)->sph_root, field);	\
} while (0)

#define SPLAY_ASSEMBLE(head, node, left, right, field) do {		\
	SPLAY_RIGHT(left, field) = SPLAY_LEFT((head)->sph_root, field);	\
	SPLAY_LEFT(right, field) = SPLAY_RIGHT((head)->sph_root, field);\
	SPLAY_LEFT((head)->sph_root, field) = SPLAY_RIGHT(node, field);	\
	SPLAY_RIGHT((head)->sph_root, field) = SPLAY_LEFT(node, field);	\
} while (0)

/* Generates prototypes and inline functions */

#define SPLAY_PROTOTYPE(name, type, field, cmp)				\
void name##_SPLAY(struct name *, struct type *);			\
void name##_SPLAY_MINMAX(struct name *, int);				\
struct type *name##_SPLAY_INSERT(struct name *, struct type *);		\
struct type *name##_SPLAY_REMOVE(struct name *, struct type *);		\
									\
/* Finds the node with the same key as elm */				\
static __inline struct type *						\
name##_SPLAY_FIND(struct name *head, struct type *elm)			\
{									\
	if (SPLAY_EMPTY(head))						\
		return(NULL);						\
	name##_SPLAY(head, elm);					\
	if ((cmp)(elm, (head)->sph_root) == 0)				\
		return (head->sph_root);				\
	return (NULL);							\
}									\
									\
static __inline struct type *						\
name##_SPLAY_NEXT(struct name *head, struct type *elm)			\
{									\
	name##_SPLAY(head, elm);					\
	if (SPLAY_RIGHT(elm, field) != NULL) {				\
		elm = SPLAY_RIGHT(elm, field);				\
		while (SPLAY_LEFT(elm, field) != NULL) {		\
			elm = SPLAY_LEFT(elm, field);			\
		}							\
	} else								\
		elm = NULL;						\
	return (elm);							\
}									\
									\
static __inline struct type *						\
name##_SPLAY_MIN_MAX(struct name *head, int val)			\
{									\
	name##_SPLAY_MINMAX(head, val);					\
        return (SPLAY_ROOT(head));					\
}

/* Main splay operation.
 * Moves node close to the key of elm to top
 */
#define SPLAY_GENERATE(name, type, field, cmp)				\
struct type *								\
name##_SPLAY_INSERT(struct name *head, struct type *elm)		\
{									\
    if (SPLAY_EMPTY(head)) {						\
	    SPLAY_LEFT(elm, field) = SPLAY_RIGHT(elm, field) = NULL;	\
    } else {								\
	    int __comp;							\
	    name##_SPLAY(head, elm);					\
	    __comp = (cmp)(elm, (head)->sph_root);			\
	    if(__comp < 0) {						\
		    SPLAY_LEFT(elm, field) = SPLAY_LEFT((head)->sph_root, field);\
		    SPLAY_RIGHT(elm, field) = (head)->sph_root;		\
		    SPLAY_LEFT((head)->sph_root, field) = NULL;		\
	    } else if (__comp > 0) {					\
		    SPLAY_RIGHT(elm, field) = SPLAY_RIGHT((head)->sph_root, field);\
		    SPLAY_LEFT(elm, field) = (head)->sph_root;		\
		    SPLAY_RIGHT((head)->sph_root, field) = NULL;	\
	    } else							\
		    return ((head)->sph_root);				\
    }									\
    (head)->sph_root = (elm);						\
    return (NULL);							\
}									\
									\
struct type *								\
name##_SPLAY_REMOVE(struct name *head, struct type *elm)		\
{									\
	struct type *__tmp;						\
	if (SPLAY_EMPTY(head))						\
		return (NULL);						\
	name##_SPLAY(head, elm);					\
	if ((cmp)(elm, (head)->sph_root) == 0) {			\
		if (SPLAY_LEFT((head)->sph_root, field) == NULL) {	\
			(head)->sph_root = SPLAY_RIGHT((head)->sph_root, field);\
		} else {						\
			__tmp = SPLAY_RIGHT((head)->sph_root, field);	\
			(head)->sph_root = SPLAY_LEFT((head)->sph_root, field);\
			name##_SPLAY(head, elm);			\
			SPLAY_RIGHT((head)->sph_root, field) = __tmp;	\
		}							\
		return (elm);						\
	}								\
	return (NULL);							\
}									\
									\
void									\
name##_SPLAY(struct name *head, struct type *elm)			\
{									\
	struct type __node, *__left, *__right, *__tmp;			\
	int __comp;							\
\
	SPLAY_LEFT(&__node, field) = SPLAY_RIGHT(&__node, field) = NULL;\
	__left = __right = &__node;					\
\
	while ((__comp = (cmp)(elm, (head)->sph_root))) {		\
		if (__comp < 0) {					\
			__tmp = SPLAY_LEFT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if ((cmp)(elm, __tmp) < 0){			\
				SPLAY_ROTATE_RIGHT(head, __tmp, field);	\
				if (SPLAY_LEFT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKLEFT(head, __right, field);		\
		} else if (__comp > 0) {				\
			__tmp = SPLAY_RIGHT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if ((cmp)(elm, __tmp) > 0){			\
				SPLAY_ROTATE_LEFT(head, __tmp, field);	\
				if (SPLAY_RIGHT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKRIGHT(head, __left, field);		\
		}							\
	}								\
	SPLAY_ASSEMBLE(head, &__node, __left, __right, field);		\
}									\
									\
/* Splay with either the minimum or the maximum element			\
 * Used to find minimum or maximum element in tree.			\
 */									\
void name##_SPLAY_MINMAX(struct name *head, int __comp) \
{									\
	struct type __node, *__left, *__right, *__tmp;			\
\
	SPLAY_LEFT(&__node, field) = SPLAY_RIGHT(&__node, field) = NULL;\
	__left = __right = &__node;					\
\
	while (1) {							\
		if (__comp < 0) {					\
			__tmp = SPLAY_LEFT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if (__comp < 0){				\
				SPLAY_ROTATE_RIGHT(head, __tmp, field);	\
				if (SPLAY_LEFT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKLEFT(head, __right, field);		\
		} else if (__comp > 0) {				\
			__tmp = SPLAY_RIGHT((head)->sph_root, field);	\
			if (__tmp == NULL)				\
				break;					\
			if (__comp > 0) {				\
				SPLAY_ROTATE_LEFT(head, __tmp, field);	\
				if (SPLAY_RIGHT((head)->sph_root, field) == NULL)\
					break;				\
			}						\
			SPLAY_LINKRIGHT(head, __left, field);		\
		}							\
	}								\
	SPLAY_ASSEMBLE(head, &__node, __left, __right, field);		\
}

#define SPLAY_NEGINF	-1
#define SPLAY_INF	1

#define SPLAY_INSERT(name, x, y)	name##_SPLAY_INSERT(x, y)
#define SPLAY_REMOVE(name, x, y)	name##_SPLAY_REMOVE(x, y)
#define SPLAY_FIND(name, x, y)		name##_SPLAY_FIND(x, y)
#define SPLAY_NEXT(name, x, y)		name##_SPLAY_NEXT(x, y)
#define SPLAY_MIN(name, x)		(SPLAY_EMPTY(x) ? NULL	\
					: name##_SPLAY_MIN_MAX(x, SPLAY_NEGINF))
#define SPLAY_MAX(name, x)		(SPLAY_EMPTY(x) ? NULL	\
					: name##_SPLAY_MIN_MAX(x, SPLAY_INF))

#define SPLAY_FOREACH(x, name, head)					\
	for ((x) = SPLAY_MIN(name, head);				\
	     (x) != NULL;						\
	     (x) = SPLAY_NEXT(name, head, x))

/* Macros that define a red-back tree */
#define VQE_RB_HEAD(name, type)						\
struct name {								\
	struct type *rbh_root; /* root of the tree */			\
}

#define VQE_RB_INITIALIZER(root)						\
	{ NULL }

#define VQE_RB_INIT(root) do {						\
	(root)->rbh_root = NULL;					\
} while (0)

#define VQE_RB_BLACK	0
#define VQE_RB_RED		1
#define VQE_RB_ENTRY(type)							\
struct {								\
	struct type *rbe_left;		/* left element */		\
	struct type *rbe_right;		/* right element */		\
	struct type *rbe_parent;	/* parent element */		\
	int rbe_color;			/* node color */		\
}

#define VQE_RB_LEFT(elm, field)		(elm)->field.rbe_left
#define VQE_RB_RIGHT(elm, field)		(elm)->field.rbe_right
#define VQE_RB_PARENT(elm, field)		(elm)->field.rbe_parent
#define VQE_RB_COLOR(elm, field)		(elm)->field.rbe_color
#define VQE_RB_ROOT(head)			(head)->rbh_root
#define VQE_RB_EMPTY(head)			(VQE_RB_ROOT(head) == NULL)

#define VQE_RB_SET(elm, parent, field) do {					\
	VQE_RB_PARENT(elm, field) = parent;					\
	VQE_RB_LEFT(elm, field) = VQE_RB_RIGHT(elm, field) = NULL;		\
	VQE_RB_COLOR(elm, field) = VQE_RB_RED;					\
} while (0)

#define VQE_RB_SET_BLACKRED(black, red, field) do {				\
	VQE_RB_COLOR(black, field) = VQE_RB_BLACK;				\
	VQE_RB_COLOR(red, field) = VQE_RB_RED;					\
} while (0)

#ifndef VQE_RB_AUGMENT
#define VQE_RB_AUGMENT(x)
#endif

#define VQE_RB_ROTATE_LEFT(head, elm, tmp, field) do {			\
	(tmp) = VQE_RB_RIGHT(elm, field);					\
	if ((VQE_RB_RIGHT(elm, field) = VQE_RB_LEFT(tmp, field))) {		\
		VQE_RB_PARENT(VQE_RB_LEFT(tmp, field), field) = (elm);		\
	}								\
	VQE_RB_AUGMENT(elm);						\
	if ((VQE_RB_PARENT(tmp, field) = VQE_RB_PARENT(elm, field))) {		\
		if ((elm) == VQE_RB_LEFT(VQE_RB_PARENT(elm, field), field))	\
			VQE_RB_LEFT(VQE_RB_PARENT(elm, field), field) = (tmp);	\
		else							\
			VQE_RB_RIGHT(VQE_RB_PARENT(elm, field), field) = (tmp);	\
	} else								\
		(head)->rbh_root = (tmp);				\
	VQE_RB_LEFT(tmp, field) = (elm);					\
	VQE_RB_PARENT(elm, field) = (tmp);					\
	VQE_RB_AUGMENT(tmp);						\
	if ((VQE_RB_PARENT(tmp, field)))					\
		VQE_RB_AUGMENT(VQE_RB_PARENT(tmp, field));			\
} while (0)

#define VQE_RB_ROTATE_RIGHT(head, elm, tmp, field) do {			\
	(tmp) = VQE_RB_LEFT(elm, field);					\
	if ((VQE_RB_LEFT(elm, field) = VQE_RB_RIGHT(tmp, field))) {		\
		VQE_RB_PARENT(VQE_RB_RIGHT(tmp, field), field) = (elm);		\
	}								\
	VQE_RB_AUGMENT(elm);						\
	if ((VQE_RB_PARENT(tmp, field) = VQE_RB_PARENT(elm, field))) {		\
		if ((elm) == VQE_RB_LEFT(VQE_RB_PARENT(elm, field), field))	\
			VQE_RB_LEFT(VQE_RB_PARENT(elm, field), field) = (tmp);	\
		else							\
			VQE_RB_RIGHT(VQE_RB_PARENT(elm, field), field) = (tmp);	\
	} else								\
		(head)->rbh_root = (tmp);				\
	VQE_RB_RIGHT(tmp, field) = (elm);					\
	VQE_RB_PARENT(elm, field) = (tmp);					\
	VQE_RB_AUGMENT(tmp);						\
	if ((VQE_RB_PARENT(tmp, field)))					\
		VQE_RB_AUGMENT(VQE_RB_PARENT(tmp, field));			\
} while (0)

/* Generates prototypes and inline functions */
#define VQE_RB_PROTOTYPE(name, type, field, cmp)				\
void name##_RB_INSERT_COLOR(struct name *, struct type *);	\
void name##_RB_REMOVE_COLOR(struct name *, struct type *, struct type *);\
struct type *name##_RB_REMOVE(struct name *, struct type *);		\
struct type *name##_RB_INSERT(struct name *, struct type *);		\
struct type *name##_RB_FIND(struct name *, struct type *);		\
struct type *name##_RB_NEXT(struct type *);				\
struct type *name##_RB_MINMAX(struct name *, int);			\
									\

/* Main rb operation.
 * Moves node close to the key of elm to top
 */
#define VQE_RB_GENERATE(name, type, field, cmp)				\
void									\
name##_RB_INSERT_COLOR(struct name *head, struct type *elm)		\
{									\
	struct type *parent, *gparent, *tmp;				\
	while ((parent = VQE_RB_PARENT(elm, field)) &&			\
	    VQE_RB_COLOR(parent, field) == VQE_RB_RED) {			\
		gparent = VQE_RB_PARENT(parent, field);			\
		if (parent == VQE_RB_LEFT(gparent, field)) {		\
			tmp = VQE_RB_RIGHT(gparent, field);			\
			if (tmp && VQE_RB_COLOR(tmp, field) == VQE_RB_RED) {	\
				VQE_RB_COLOR(tmp, field) = VQE_RB_BLACK;	\
				VQE_RB_SET_BLACKRED(parent, gparent, field);\
				elm = gparent;				\
				continue;				\
			}						\
			if (VQE_RB_RIGHT(parent, field) == elm) {		\
				VQE_RB_ROTATE_LEFT(head, parent, tmp, field);\
				tmp = parent;				\
				parent = elm;				\
				elm = tmp;				\
			}						\
			VQE_RB_SET_BLACKRED(parent, gparent, field);	\
			VQE_RB_ROTATE_RIGHT(head, gparent, tmp, field);	\
		} else {						\
			tmp = VQE_RB_LEFT(gparent, field);			\
			if (tmp && VQE_RB_COLOR(tmp, field) == VQE_RB_RED) {	\
				VQE_RB_COLOR(tmp, field) = VQE_RB_BLACK;	\
				VQE_RB_SET_BLACKRED(parent, gparent, field);\
				elm = gparent;				\
				continue;				\
			}						\
			if (VQE_RB_LEFT(parent, field) == elm) {		\
				VQE_RB_ROTATE_RIGHT(head, parent, tmp, field);\
				tmp = parent;				\
				parent = elm;				\
				elm = tmp;				\
			}						\
			VQE_RB_SET_BLACKRED(parent, gparent, field);	\
			VQE_RB_ROTATE_LEFT(head, gparent, tmp, field);	\
		}							\
	}								\
	VQE_RB_COLOR(head->rbh_root, field) = VQE_RB_BLACK;			\
}									\
									\
void									\
name##_RB_REMOVE_COLOR(struct name *head, struct type *parent, struct type *elm) \
{									\
	struct type *tmp;						\
	while ((elm == NULL || VQE_RB_COLOR(elm, field) == VQE_RB_BLACK) &&	\
	    elm != VQE_RB_ROOT(head)) {					\
		if (VQE_RB_LEFT(parent, field) == elm) {			\
			tmp = VQE_RB_RIGHT(parent, field);			\
			if (VQE_RB_COLOR(tmp, field) == VQE_RB_RED) {		\
				VQE_RB_SET_BLACKRED(tmp, parent, field);	\
				VQE_RB_ROTATE_LEFT(head, parent, tmp, field);\
				tmp = VQE_RB_RIGHT(parent, field);		\
			}						\
			if ((VQE_RB_LEFT(tmp, field) == NULL ||		\
			    VQE_RB_COLOR(VQE_RB_LEFT(tmp, field), field) == VQE_RB_BLACK) &&\
			    (VQE_RB_RIGHT(tmp, field) == NULL ||		\
			    VQE_RB_COLOR(VQE_RB_RIGHT(tmp, field), field) == VQE_RB_BLACK)) {\
				VQE_RB_COLOR(tmp, field) = VQE_RB_RED;		\
				elm = parent;				\
				parent = VQE_RB_PARENT(elm, field);		\
			} else {					\
				if (VQE_RB_RIGHT(tmp, field) == NULL ||	\
				    VQE_RB_COLOR(VQE_RB_RIGHT(tmp, field), field) == VQE_RB_BLACK) {\
					struct type *oleft;		\
					if ((oleft = VQE_RB_LEFT(tmp, field)))\
						VQE_RB_COLOR(oleft, field) = VQE_RB_BLACK;\
					VQE_RB_COLOR(tmp, field) = VQE_RB_RED;	\
					VQE_RB_ROTATE_RIGHT(head, tmp, oleft, field);\
					tmp = VQE_RB_RIGHT(parent, field);	\
				}					\
				VQE_RB_COLOR(tmp, field) = VQE_RB_COLOR(parent, field);\
				VQE_RB_COLOR(parent, field) = VQE_RB_BLACK;	\
				if (VQE_RB_RIGHT(tmp, field))		\
					VQE_RB_COLOR(VQE_RB_RIGHT(tmp, field), field) = VQE_RB_BLACK;\
				VQE_RB_ROTATE_LEFT(head, parent, tmp, field);\
				elm = VQE_RB_ROOT(head);			\
				break;					\
			}						\
		} else {						\
			tmp = VQE_RB_LEFT(parent, field);			\
			if (VQE_RB_COLOR(tmp, field) == VQE_RB_RED) {		\
				VQE_RB_SET_BLACKRED(tmp, parent, field);	\
				VQE_RB_ROTATE_RIGHT(head, parent, tmp, field);\
				tmp = VQE_RB_LEFT(parent, field);		\
			}						\
			if ((VQE_RB_LEFT(tmp, field) == NULL ||		\
			    VQE_RB_COLOR(VQE_RB_LEFT(tmp, field), field) == VQE_RB_BLACK) &&\
			    (VQE_RB_RIGHT(tmp, field) == NULL ||		\
			    VQE_RB_COLOR(VQE_RB_RIGHT(tmp, field), field) == VQE_RB_BLACK)) {\
				VQE_RB_COLOR(tmp, field) = VQE_RB_RED;		\
				elm = parent;				\
				parent = VQE_RB_PARENT(elm, field);		\
			} else {					\
				if (VQE_RB_LEFT(tmp, field) == NULL ||	\
				    VQE_RB_COLOR(VQE_RB_LEFT(tmp, field), field) == VQE_RB_BLACK) {\
					struct type *oright;		\
					if ((oright = VQE_RB_RIGHT(tmp, field)))\
						VQE_RB_COLOR(oright, field) = VQE_RB_BLACK;\
					VQE_RB_COLOR(tmp, field) = VQE_RB_RED;	\
					VQE_RB_ROTATE_LEFT(head, tmp, oright, field);\
					tmp = VQE_RB_LEFT(parent, field);	\
				}					\
				VQE_RB_COLOR(tmp, field) = VQE_RB_COLOR(parent, field);\
				VQE_RB_COLOR(parent, field) = VQE_RB_BLACK;	\
				if (VQE_RB_LEFT(tmp, field))		\
					VQE_RB_COLOR(VQE_RB_LEFT(tmp, field), field) = VQE_RB_BLACK;\
				VQE_RB_ROTATE_RIGHT(head, parent, tmp, field);\
				elm = VQE_RB_ROOT(head);			\
				break;					\
			}						\
		}							\
	}								\
	if (elm)							\
		VQE_RB_COLOR(elm, field) = VQE_RB_BLACK;			\
}									\
									\
struct type *								\
name##_RB_REMOVE(struct name *head, struct type *elm)			\
{									\
	struct type *child, *parent, *old = elm;			\
	int color;							\
	if (VQE_RB_LEFT(elm, field) == NULL)				\
		child = VQE_RB_RIGHT(elm, field);				\
	else if (VQE_RB_RIGHT(elm, field) == NULL)				\
		child = VQE_RB_LEFT(elm, field);				\
	else {								\
		struct type *left;					\
		elm = VQE_RB_RIGHT(elm, field);				\
		while ((left = VQE_RB_LEFT(elm, field)))			\
			elm = left;					\
		child = VQE_RB_RIGHT(elm, field);				\
		parent = VQE_RB_PARENT(elm, field);				\
		color = VQE_RB_COLOR(elm, field);				\
		if (child)						\
			VQE_RB_PARENT(child, field) = parent;		\
		if (parent) {						\
			if (VQE_RB_LEFT(parent, field) == elm)		\
				VQE_RB_LEFT(parent, field) = child;		\
			else						\
				VQE_RB_RIGHT(parent, field) = child;	\
			VQE_RB_AUGMENT(parent);				\
		} else							\
			VQE_RB_ROOT(head) = child;				\
		if (VQE_RB_PARENT(elm, field) == old)			\
			parent = elm;					\
		(elm)->field = (old)->field;				\
		if (VQE_RB_PARENT(old, field)) {				\
			if (VQE_RB_LEFT(VQE_RB_PARENT(old, field), field) == old)\
				VQE_RB_LEFT(VQE_RB_PARENT(old, field), field) = elm;\
			else						\
				VQE_RB_RIGHT(VQE_RB_PARENT(old, field), field) = elm;\
			VQE_RB_AUGMENT(VQE_RB_PARENT(old, field));		\
		} else							\
			VQE_RB_ROOT(head) = elm;				\
		VQE_RB_PARENT(VQE_RB_LEFT(old, field), field) = elm;		\
		if (VQE_RB_RIGHT(old, field))				\
			VQE_RB_PARENT(VQE_RB_RIGHT(old, field), field) = elm;	\
		if (parent) {						\
			left = parent;					\
			do {						\
				VQE_RB_AUGMENT(left);			\
			} while ((left = VQE_RB_PARENT(left, field)));	\
		}							\
		goto color;						\
	}								\
	parent = VQE_RB_PARENT(elm, field);					\
	color = VQE_RB_COLOR(elm, field);					\
	if (child)							\
		VQE_RB_PARENT(child, field) = parent;			\
	if (parent) {							\
		if (VQE_RB_LEFT(parent, field) == elm)			\
			VQE_RB_LEFT(parent, field) = child;			\
		else							\
			VQE_RB_RIGHT(parent, field) = child;		\
		VQE_RB_AUGMENT(parent);					\
	} else								\
		VQE_RB_ROOT(head) = child;					\
color:									\
	if (color == VQE_RB_BLACK)						\
		name##_RB_REMOVE_COLOR(head, parent, child);		\
	return (old);							\
}									\
									\
/* Inserts a node into the RB tree */					\
struct type *								\
name##_RB_INSERT(struct name *head, struct type *elm)			\
{									\
	struct type *tmp;						\
	struct type *parent = NULL;					\
	int comp = 0;							\
	tmp = VQE_RB_ROOT(head);						\
	while (tmp) {							\
		parent = tmp;						\
		comp = (cmp)(elm, parent);				\
		if (comp < 0)						\
			tmp = VQE_RB_LEFT(tmp, field);			\
		else if (comp > 0)					\
			tmp = VQE_RB_RIGHT(tmp, field);			\
		else							\
			return (tmp);					\
	}								\
	VQE_RB_SET(elm, parent, field);					\
	if (parent != NULL) {						\
		if (comp < 0)						\
			VQE_RB_LEFT(parent, field) = elm;			\
		else							\
			VQE_RB_RIGHT(parent, field) = elm;			\
		VQE_RB_AUGMENT(parent);					\
	} else								\
		VQE_RB_ROOT(head) = elm;					\
	name##_RB_INSERT_COLOR(head, elm);				\
	return (NULL);							\
}									\
									\
/* Finds the node with the same key as elm */				\
struct type *								\
name##_RB_FIND(struct name *head, struct type *elm)			\
{									\
	struct type *tmp = VQE_RB_ROOT(head);				\
	int comp;							\
	while (tmp) {							\
		comp = cmp(elm, tmp);					\
		if (comp < 0)						\
			tmp = VQE_RB_LEFT(tmp, field);			\
		else if (comp > 0)					\
			tmp = VQE_RB_RIGHT(tmp, field);			\
		else							\
			return (tmp);					\
	}								\
	return (NULL);							\
}									\
									\
struct type *								\
name##_RB_NEXT(struct type *elm)					\
{									\
	if (VQE_RB_RIGHT(elm, field)) {					\
		elm = VQE_RB_RIGHT(elm, field);				\
		while (VQE_RB_LEFT(elm, field))				\
			elm = VQE_RB_LEFT(elm, field);			\
	} else {							\
		if (VQE_RB_PARENT(elm, field) &&				\
		    (elm == VQE_RB_LEFT(VQE_RB_PARENT(elm, field), field)))	\
			elm = VQE_RB_PARENT(elm, field);			\
		else {							\
			while (VQE_RB_PARENT(elm, field) &&			\
			    (elm == VQE_RB_RIGHT(VQE_RB_PARENT(elm, field), field)))\
				elm = VQE_RB_PARENT(elm, field);		\
			elm = VQE_RB_PARENT(elm, field);			\
		}							\
	}								\
	return (elm);							\
}									\
									\
struct type *								\
name##_RB_MINMAX(struct name *head, int val)				\
{									\
	struct type *tmp = VQE_RB_ROOT(head);				\
	struct type *parent = NULL;					\
	while (tmp) {							\
		parent = tmp;						\
		if (val < 0)						\
			tmp = VQE_RB_LEFT(tmp, field);			\
		else							\
			tmp = VQE_RB_RIGHT(tmp, field);			\
	}								\
	return (parent);						\
}

#define VQE_RB_NEGINF	-1
#define VQE_RB_INF	1

#define VQE_RB_INSERT(name, x, y)	name##_RB_INSERT(x, y)
#define VQE_RB_REMOVE(name, x, y)	name##_RB_REMOVE(x, y)
#define VQE_RB_FIND(name, x, y)	name##_RB_FIND(x, y)
#define VQE_RB_NEXT(name, x, y)	name##_RB_NEXT(y)
#define VQE_RB_MIN(name, x)		name##_RB_MINMAX(x, VQE_RB_NEGINF)
#define VQE_RB_MAX(name, x)		name##_RB_MINMAX(x, VQE_RB_INF)

#define VQE_RB_FOREACH(x, name, head)					\
	for ((x) = VQE_RB_MIN(name, head);					\
	     (x) != NULL;						\
	     (x) = name##_RB_NEXT(x))

#endif	/* _SYS_TREE_H_ */
