/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)queue.h	8.5 (Berkeley) 8/20/94
 * $FreeBSD: src/sys/sys/queue.h,v 1.32.2.7 2002/04/17 14:21:02 des Exp $
 */

/* Cisco Note: This file has been copied from a later version of sys/queue.h
 * in a later version of linux because it has some macros not present in 
 * current system version.  Back out to sys/queue.h when necessary macros present. 
 * (This is a shared memory implementation of various data structures)
 */

#ifndef _UTILS_QUEUE_SHM_H_
#define	_UTILS_QUEUE_SHM_H_

// #include <machine/ansi.h>	/* for __offsetof */

/*
 * This file defines five types of data structures: singly-linked lists,
 * singly-linked tail queues, lists, tail queues, and circular queues.
 *
 * A singly-linked list is headed by a single forward pointer. The elements
 * are singly linked for minimum space and pointer manipulation overhead at
 * the expense of O(n) removal for arbitrary elements. New elements can be
 * added to the list after an existing element or at the head of the list.
 * Elements being removed from the head of the list should use the explicit
 * macro for this purpose for optimum efficiency. A singly-linked list may
 * only be traversed in the forward direction.  Singly-linked lists are ideal
 * for applications with large datasets and few or no removals or for
 * implementing a LIFO queue.
 *
 * A singly-linked tail queue is headed by a pair of pointers, one to the
 * head of the list and the other to the tail of the list. The elements are
 * singly linked for minimum space and pointer manipulation overhead at the
 * expense of O(n) removal for arbitrary elements. New elements can be added
 * to the list after an existing element, at the head of the list, or at the
 * end of the list. Elements being removed from the head of the tail queue
 * should use the explicit macro for this purpose for optimum efficiency.
 * A singly-linked tail queue may only be traversed in the forward direction.
 * Singly-linked tail queues are ideal for applications with large datasets
 * and few or no removals or for implementing a FIFO queue.
 *
 * A list is headed by a single forward pointer (or an array of forward
 * pointers for a hash table header). The elements are doubly linked
 * so that an arbitrary element can be removed without a need to
 * traverse the list. New elements can be added to the list before
 * or after an existing element or at the head of the list. A list
 * may only be traversed in the forward direction.
 *
 * A tail queue is headed by a pair of pointers, one to the head of the
 * list and the other to the tail of the list. The elements are doubly
 * linked so that an arbitrary element can be removed without a need to
 * traverse the list. New elements can be added to the list before or
 * after an existing element, at the head of the list, or at the end of
 * the list. A tail queue may be traversed in either direction.
 *
 * A circle queue is headed by a pair of pointers, one to the head of the
 * list and the other to the tail of the list. The elements are doubly
 * linked so that an arbitrary element can be removed without a need to
 * traverse the list. New elements can be added to the list before or after
 * an existing element, at the head of the list, or at the end of the list.
 * A circle queue may be traversed in either direction, but has a more
 * complex end of list detection.
 *
 * For details on the use of these macros, see the queue(3) manual page.
 *
 *
 *			SLIST	LIST	STAILQ	TAILQ	CIRCLEQ
 * _HEAD		+	+	+	+	+
 * _HEAD_INITIALIZER	+	+	+	+	+
 * _ENTRY		+	+	+	+	+
 * _INIT		+	+	+	+	+
 * _EMPTY		+	+	+	+	+
 * _FIRST		+	+	+	+	+
 * _NEXT		+	+	+	+	+
 * _PREV		-	-	-	+	+
 * _LAST		-	-	+	+	+
 * _FOREACH		+	+	+	+	+
 * _FOREACH_REVERSE	-	-	-	+	+
 * _INSERT_HEAD		+	+	+	+	+
 * _INSERT_BEFORE	-	+	-	+	+
 * _INSERT_AFTER	+	+	+	+	+
 * _INSERT_TAIL		-	-	+	+	+
 * _REMOVE_HEAD		+	-	+	-	-
 * _REMOVE		+	+	+	+	+
 *
 */

/*
 * Singly-linked List declarations.
 */
#define	SM_SLIST_HEAD(name, type)  \
struct name { \
	struct type *sm_slh_first;	/* first element */ \
}

#define	SM_SLIST_HEAD_INITIALIZER(head) \
	{ (void *) 0 }
 
#define	SM_SLIST_ENTRY(type) \
struct { \
	struct type *sm_sle_next;	/* next element */   \
}

#define sm_mk_sptr(x, y) ({ \
			((x) == (void *) 0) ? NULL : (__typeof__ ((x))) ((ptrdiff_t)((x)) + ((ptrdiff_t)(sm_seg_base((y)))) - (ptrdiff_t)1); \
		})

#define sm_mk_soffs(x, y) ({ \
			((x) == NULL) ? (void *) 0 : (__typeof__ ((x))) ((ptrdiff_t)((x)) + (ptrdiff_t)1 - ((ptrdiff_t)(sm_seg_base((y))))); \
		})

/*
 * Singly-linked List functions.
 */
#define	SM_SLIST_EMPTY(head) ((head)->sm_slh_first == (void *) 0)

#define	SM_SLIST_FIRST(head) ((head)->sm_slh_first)
#define	SM_SLIST_FIRST_PTR(head, b) (sm_mk_sptr(((head)->sm_slh_first), (b)))

#define	SM_SLIST_FOREACH(var, head, field, b) \
	for ((var) = SM_SLIST_FIRST_PTR((head), (b));	\
	     (var); \
	     (var) = SM_SLIST_NEXT_PTR((var), field, (b)))

#define	SM_SLIST_INIT(head) do { \
		SM_SLIST_FIRST((head)) = (void *) 0; \
} while (0)

#define	SM_SLIST_INSERT_AFTER(slistelm, elm, field, b) do { \
		SM_SLIST_NEXT((elm), field) = SM_SLIST_NEXT((slistelm), field); \
		SM_SLIST_NEXT((slistelm), field) = sm_mk_soffs((elm), (b)); \
} while (0)

#define	SM_SLIST_INSERT_HEAD(head, elm, field, b) do { \
		SM_SLIST_NEXT((elm), field) = sm_mk_soffs(SM_SLIST_FIRST_PTR((head), (b)), (b)); \
		SM_SLIST_FIRST((head)) = sm_mk_soffs((elm), (b));	\
} while (0)

#define	SM_SLIST_NEXT_PTR(elm, field, b) (sm_mk_sptr((elm)->field.sm_sle_next, (b)))
#define	SM_SLIST_NEXT(elm, field) ((elm)->field.sm_sle_next)

#define SM_SLIST_REMOVE(head, elm, type, field, b) do { \
		if (SM_SLIST_FIRST((head)) == sm_mk_soffs((elm), (b))) { \
			    SM_SLIST_REMOVE_HEAD((head), field, (b));	\
		} else { \
			struct type *curelm = SM_SLIST_FIRST_PTR((head), (b)); \
			while (SM_SLIST_NEXT(curelm, field) != sm_mk_soffs((elm), (b))) \
				curelm = SM_SLIST_NEXT_PTR(curelm, field, (b)); \
			SM_SLIST_NEXT(curelm, field) = \
				SM_SLIST_NEXT(sm_mk_sptr(SM_SLIST_NEXT(curelm, field), (b)), field); \
		} \
} while (0)

#define	SM_SLIST_REMOVE_HEAD(head, field, b) do {  \
		SM_SLIST_FIRST((head)) = SM_SLIST_NEXT(SM_SLIST_FIRST_PTR((head), (b)), field); \
} while (0)

/*
 * List declarations.
 */
#define	SM_LIST_HEAD(name, type)	 \
struct name { \
	struct type *sm_lh_first;	/* first element */ \
}

#define	SM_LIST_HEAD_INITIALIZER(head) \
	{ (void *) 0 }

#define	SM_LIST_ENTRY(type) \
struct { \
	struct type *sm_le_next;	/* next element */     \
	struct type **sm_le_prev;	/* address of previous next element */ \
}

/*
 * List functions.
 */

#define	SM_LIST_EMPTY(head)	((head)->sm_lh_first == (void *) 0)

#define	SM_LIST_FIRST(head)	((head)->sm_lh_first)
#define	SM_LIST_FIRST_PTR(head, b) (sm_mk_sptr(((head)->sm_lh_first), (b)))

#define	SM_LIST_FOREACH(var, head, field, b) \
	for ((var) = SM_LIST_FIRST_PTR((head), (b)); \
	     (var); \
	     (var) = SM_LIST_NEXT_PTR((var), field, (b)))

#define	SM_LIST_INIT(head) do { \
		SM_LIST_FIRST((head)) = (void *) 0; \
} while (0)

#define	SM_LIST_INSERT_AFTER(listelm, elm, field, b) do { \
		if ((SM_LIST_NEXT((elm), field) = SM_LIST_NEXT((listelm), field)) != (void *) 0) \
			SM_LIST_NEXT_PTR((listelm), field, (b))->field.sm_le_prev = \
				sm_mk_soffs(&SM_LIST_NEXT((elm), field), (b)); \
		SM_LIST_NEXT((listelm), field) = sm_mk_soffs((elm), (b)); \
		(elm)->field.sm_le_prev = sm_mk_soffs(&SM_LIST_NEXT((listelm), field), (b)); \
} while (0)

#define	SM_LIST_INSERT_BEFORE(listelm, elm, field, b) do { \
		(elm)->field.sm_le_prev = (listelm)->field.sm_le_prev; \
		SM_LIST_NEXT((elm), field) = sm_mk_soffs((listelm), (b)); \
		*(sm_mk_sptr((listelm)->field.sm_le_prev, (b))) = sm_mk_soffs((elm), (b)); \
		(listelm)->field.sm_le_prev = sm_mk_soffs(&SM_LIST_NEXT((elm), field), (b)); \
} while (0)

#define	SM_LIST_INSERT_HEAD(head, elm, field, b) do { \
		if ((SM_LIST_NEXT((elm), field) = SM_LIST_FIRST((head))) != (void *) 0) \
			SM_LIST_FIRST_PTR((head), (b))->field.sm_le_prev = sm_mk_soffs(&SM_LIST_NEXT((elm), field), (b)); \
		SM_LIST_FIRST((head)) = sm_mk_soffs(elm, (b));	\
		(elm)->field.sm_le_prev = sm_mk_soffs(&SM_LIST_FIRST((head)), (b)); \
} while (0)

#define	SM_LIST_NEXT_PTR(elm, field, b) (sm_mk_sptr((elm)->field.sm_le_next, (b)))
#define	SM_LIST_NEXT(elm, field)	((elm)->field.sm_le_next)

#define	SM_LIST_REMOVE(elm, field, b) do { \
		if (SM_LIST_NEXT((elm), field) != (void *) 0) \
			sm_mk_sptr(SM_LIST_NEXT((elm), field), (b))->field.sm_le_prev = \
				(elm)->field.sm_le_prev; \
		*(sm_mk_sptr((elm)->field.sm_le_prev, (b))) = SM_LIST_NEXT((elm), field); \
} while (0)

/*
 * Tail queue declarations.
 */
#define	SM_TAILQ_DEFINE(name, type)					\
	struct name {							\
		struct type *sm_tqh_first;	/* first element */	\
		struct type **sm_tqh_last;	/* addr of last next element */ \
	}

#define	SM_TAILQ_HEAD(name)						\
	struct name

#define	SM_TAILQ_HEAD_INITIALIZER(head)		\
	{ NULL, &(head).sm_tqh_first }

#define	SM_TAILQ_ENTRY(type)						\
	struct {							\
		struct type *sm_tqe_next;	/* next element */	\
		struct type **sm_tqe_prev;	/* address of previous next element */ \
	}

/*
 * Tail queue functions.
 */
#define	SM_TAILQ_EMPTY(head)	((head)->sm_tqh_first == (void *) 0)

#define	SM_TAILQ_FIRST(head)	((head)->sm_tqh_first)
#define	SM_TAILQ_FIRST_PTR(head, b) (sm_mk_sptr(((head)->sm_tqh_first), (b)))

#define	SM_TAILQ_NEXT(elm, field) ((elm)->field.sm_tqe_next)
#define	SM_TAILQ_NEXT_PTR(elm, field, b) (sm_mk_sptr((elm)->field.sm_tqe_next, (b)))

#define	SM_TAILQ_LAST(head, headname, b)					\
	(*(sm_mk_sptr(((struct headname *)(sm_mk_sptr((head)->sm_tqh_last, (b))))->sm_tqh_last, (b))))

#define	SM_TAILQ_LAST_PTR(head, headname, b)				\
	(sm_mk_sptr(SM_TAILQ_LAST((head), headname, (b)), (b)))

#define	SM_TAILQ_PREV(elm, headname, field, b)	\
	(*(sm_mk_sptr(((struct headname *)(sm_mk_sptr((elm)->field.sm_tqe_prev, (b))))->sm_tqh_last, (b))))		

#define	SM_TAILQ_PREV_PTR(elm, headname, field, b)		\
	sm_mk_sptr((SM_TAILQ_PREV((elm), headname, field, (b))), (b))

#define	SM_TAILQ_FOREACH(var, head, field, b)				\
	for ((var) = SM_TAILQ_FIRST_PTR((head), (b));			\
	     (var);							\
	     (var) = SM_TAILQ_NEXT_PTR((var), field, (b)))

#define	SM_TAILQ_FOREACH_REVERSE(var, head, headname, field, b)		\
	for ((var) = SM_TAILQ_LAST_PTR((head), headname, (b));		\
	     (var);							\
	     (var) = SM_TAILQ_PREV_PTR((var), headname, field, (b)))

#define	SM_TAILQ_INIT(head, b) do {					\
		SM_TAILQ_FIRST((head)) = (void *) 0;			\
		(head)->sm_tqh_last = sm_mk_soffs(&SM_TAILQ_FIRST((head)), (b)); \
	} while (0)

#define	SM_TAILQ_INSERT_AFTER(head, listelm, elm, field, b) do {		\
		if ((SM_TAILQ_NEXT((elm), field) = SM_TAILQ_NEXT((listelm), field)) != (void *) 0) \
			SM_TAILQ_NEXT_PTR((elm), field, (b))->field.sm_tqe_prev = \
				sm_mk_soffs(&SM_TAILQ_NEXT((elm), field), (b)); \
		else							\
			(head)->sm_tqh_last = sm_mk_soffs(&SM_TAILQ_NEXT((elm), field), (b)); \
		SM_TAILQ_NEXT((listelm), field) = sm_mk_soffs((elm), (b)); \
		(elm)->field.sm_tqe_prev = sm_mk_soffs(&SM_TAILQ_NEXT((listelm), field), (b)); \
	} while (0)

#define	SM_TAILQ_INSERT_BEFORE(listelm, elm, field, b) do {		\
		(elm)->field.sm_tqe_prev = (listelm)->field.sm_tqe_prev; \
		SM_TAILQ_NEXT((elm), field) = sm_mk_soffs((listelm), (b)); \
		*(sm_mk_sptr((listelm)->field.sm_tqe_prev, (b))) = sm_mk_soffs((elm), (b)); \
		(listelm)->field.sm_tqe_prev = sm_mk_soffs(&SM_TAILQ_NEXT((elm), field), (b)); \
	} while (0)

#define	SM_TAILQ_INSERT_HEAD(head, elm, field, b) do {			\
		if ((SM_TAILQ_NEXT((elm), field) = SM_TAILQ_FIRST((head))) != (void *) 0) \
			SM_TAILQ_FIRST_PTR((head), (b))->field.sm_tqe_prev = \
				sm_mk_soffs(&SM_TAILQ_NEXT((elm), field), (b)); \
		else							\
			(head)->sm_tqh_last = sm_mk_soffs(&SM_TAILQ_NEXT((elm), field), (b)); \
		SM_TAILQ_FIRST((head)) = sm_mk_soffs((elm), (b));	\
		(elm)->field.sm_tqe_prev = sm_mk_soffs(&SM_TAILQ_FIRST((head)), (b)); \
	} while (0)

#define	SM_TAILQ_INSERT_TAIL(head, elm, field, b) do {			\
		SM_TAILQ_NEXT((elm), field) = (void *) 0;		\
		(elm)->field.sm_tqe_prev = (head)->sm_tqh_last;		\
		*(sm_mk_sptr((head)->sm_tqh_last, (b))) = sm_mk_soffs((elm), (b)); \
		(head)->sm_tqh_last = sm_mk_soffs(&SM_TAILQ_NEXT((elm), field), (b)); \
	} while (0)

#define	SM_TAILQ_REMOVE(head, elm, field, b) do {				\
		if ((SM_TAILQ_NEXT((elm), field)) != (void *) 0)		\
			sm_mk_sptr(SM_TAILQ_NEXT((elm), field), (b))->field.sm_tqe_prev = \
				(elm)->field.sm_tqe_prev;		\
		else							\
			(head)->sm_tqh_last = (elm)->field.sm_tqe_prev;	\
		*(sm_mk_sptr((elm)->field.sm_tqe_prev, (b))) = SM_TAILQ_NEXT((elm), field); \
	} while (0)

#endif /* !_UTILS_QUEUE_SHM_H_ */
