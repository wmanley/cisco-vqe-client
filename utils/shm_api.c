/*------------------------------------------------------------------
 * Shared Memory API
 * 
 * Copyright (c) 2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
# include <sys/shm.h>
#include <sys/sem.h>
#include <stddef.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <memory.h>

#include "utils/queue_plus.h"
#include "utils/shm_api.h"

#define smlog(fmt, args...)	 /*fprintf(stderr, fmt "\n", ## args)*/
#define smerr(fmt, args...)	 /*fprintf(stderr, fmt "\n", ## args)*/

#define FCN_ENTRY do { \
	smlog("%s", __FUNCTION__);  \
} while (0)

#define PUBLIC
#define PRIVATE static

#define SM_SUCCESS	0
#define SM_FAIL		-1
#define NSEMS		1
#define SEM_ID		0
#define LOCK		-1
#define UNLOCK		1
#define SM_SEG_MAGIC	0xCAFEFACE
#define PERMIT_ALL	00666
#define SEG_ATTACH	1
#define SEG_DETACH	2
#define SEG_STAT		4
#define INVALID_TID	-1
#define INVALID_PID	-1

struct sm_seg_hdl
{
	int	magic;
	key_t	key;
	void *	base; 
	int	r_size;
	int	t_size;
	pid_t	pid;
	pthread_t	tid;
	int	refcnt;
};

struct sm_seg_hdr
{
	int	magic;
	key_t	key;
	int	r_size;
	int	t_size;
	pid_t	creat_pid;
	pthread_t	creat_tid;
	int	data_offset;
	int	refcnt;
	pthread_mutex_t s_mutex;
};

struct sm_mutex_thread
{
	pthread_t	tid;
	pid_t	pid;
	int	count;
};

struct sm_sem_t
{
	key_t	key;
	pid_t	creat_pid;
	pthread_t	creat_tid;
	int	refcnt;
	VQE_LIST_ENTRY(sm_sem_t) next;
};

struct sm_seg_item
{
	struct sm_seg_hdr	seg_h;
	void *		base;
	int		destroy;
	int		chown;
	VQE_LIST_ENTRY(sm_seg_item) next;
};

VQE_LIST_HEAD(, sm_seg_item)	sm_seglist;
VQE_LIST_HEAD(, sm_sem_t)	sm_semlist;
pthread_mutex_t		g_mutex;
pthread_mutexattr_t	g_mattr;
int			__init;
struct sm_mutex_thread	g_mthread;
pid_t			g_creatpid = INVALID_PID;

#define ALIGN_64(x) (((x) + 64  - 1) & ~(64 - 1))
#define HDR_SIZE ALIGN_64(sizeof(struct sm_seg_hdr))
#define INIT_SIZE(h, r) do {   \
		(h)->r_size = r; \
		(h)->t_size = r + HDR_SIZE;\
	} while (0);

PRIVATE int
sm_lock_destroy (key_t key)
{
	int id;
	struct sm_sem_t *sem = NULL, *sem_n = NULL;

	FCN_ENTRY;

	VQE_LIST_FOREACH_SAFE(sem, &sm_semlist, next, sem_n) {
		if (sem->key == key) {
			VQE_LIST_REMOVE(sem, next);
			if (sem->creat_pid != getpid() ||
			    sem->creat_tid != pthread_self()) 
				smerr("attempting semaphore destroy from a "
				      "thread other than creator (%d/%d/%ld)",
				      key, getpid(), pthread_self());
			if (--sem->refcnt != 0)
				smerr("semaphore (%d) refcnt g.t. 0", key);
			break;
		}
	}

	if (sem != NULL) {
		if (sem->key != key) {
			smerr("semaphore (%d/ %d) destroy, but semaphore  "
			      "not created in this process", sem->key, key);
		}
		free(sem);
	}

	id = semget(key, 0, 0);
	if (id == - 1 && (errno == EIDRM || errno == ENOENT)) {
		smlog("semaphore (%d) already removed (%d)", key, errno);
		return (SM_SUCCESS);
	} else if (id == -1) {
		smerr("semaphore (%d) insufficient delete or access "
		      "privilge (%d)", key, errno);
		return (SM_FAIL);
	}
	if (semctl(id, 0, IPC_RMID) != 0) {
		smerr("semaphore (%d) not deleted (%d)!", key, errno);
		return (SM_FAIL);
	}
	
	return (SM_SUCCESS);
}

PRIVATE int 
sm_lock_creat (key_t key)
{
	int id;
	struct sm_sem_t *sem = NULL;

	FCN_ENTRY;

	if (sm_lock_destroy(key) != SM_SUCCESS) {
		return (SM_FAIL);
	}
	sem = malloc(sizeof(*sem));
	if (sem == NULL) {
		smerr("insufficient memory to create semaphore (%d)", key);
		goto fail;
	}
	id = semget(key, NSEMS, IPC_CREAT | PERMIT_ALL);
	if (id == -1) {
		smerr("semaphore (%d) not created (%d)", key, errno);
		goto fail;
	}

	sem->key = key;
	sem->creat_pid = getpid();
	sem->creat_tid = pthread_self();
	sem->refcnt++;
	VQE_LIST_INSERT_HEAD(&sm_semlist, sem, next);

	return (SM_SUCCESS);

  fail:
	if (sem != NULL)
		free(sem);
	return (SM_FAIL);
}

PRIVATE  int
sm_lock (key_t key)
{
	int id, rv;
	struct sembuf sb;

	FCN_ENTRY;

	id = semget(key, 0, 0);
	if (id == -1) {
		smerr("semaphore (%d) not present (%d)", key, errno);
		return (SM_FAIL);
	}

	sb.sem_num = SEM_ID;
	sb.sem_op = LOCK;
	sb.sem_flg = 0;

	while (1) {
		rv = semop(id, &sb, NSEMS);
		if (rv == EINTR)
			continue;
		if (rv == EIDRM) {
			smerr("Semaphore (%d) deleted!", key);
			return (SM_FAIL);
		}
		break;
	}

	smlog("semaphore (%d) acquired (%d/%ld)'", key, getpid(), pthread_self());
	return (SM_SUCCESS);
}

PRIVATE int sm_unlock (key_t key)
{
	int id;
	struct sembuf sb;

	FCN_ENTRY;

	id = semget(key, 0, 0);
	if (id == -1) {
		smerr("semaphore (%d) not present (%d)", key, errno);
		return (SM_FAIL);
	}

	sb.sem_num = SEM_ID;
	sb.sem_op = UNLOCK;
	sb.sem_flg = 0;

	if (semop(id, &sb, NSEMS) == EIDRM) {
		smerr("Semaphore (%d) deleted!", key);
		return (SM_FAIL);
	}

	smlog("semaphore (%d) released (%d/%ld)'", key, getpid(), pthread_self());
	return (SM_SUCCESS);
}

PRIVATE int
sm_seg_mark_destroy (key_t key)
{
	struct sm_seg_item *seg = NULL;

	FCN_ENTRY;

	VQE_LIST_FOREACH(seg, &sm_seglist, next) {
		if (seg->seg_h.key == key) {
			seg->destroy++;
			return (SM_SUCCESS);
		}
	}
	
	return (SM_FAIL);
}

PRIVATE int
sm_seg_delet_item (key_t key)
{
	struct sm_seg_item *seg = NULL, *seg_n = NULL;
	int rv = SM_FAIL;

	FCN_ENTRY;

	VQE_LIST_FOREACH_SAFE(seg, &sm_seglist, next, seg_n) {
		if (seg->seg_h.key == key) {
			if (seg->seg_h.creat_tid != pthread_self()) {
				smlog("segment (%d) item delet from a thread "
				      "different than creator (%ld/%ld)",  
				      key, seg->seg_h.creat_tid, pthread_self());
			}
			if (seg->seg_h.refcnt != 0) {
				smerr("segment (%d) has attached threads", key);
			} else {
				VQE_LIST_REMOVE(seg, next);
				free(seg);
			}
			rv = SM_SUCCESS;
			break;
		}
	}

	return (rv);
}

PRIVATE struct sm_seg_hdl *
sm_seg_gethdl (key_t key, int size)
{
	struct sm_seg_hdl *h;
	int h_size;

	FCN_ENTRY;

	h_size = ALIGN_64(sizeof(struct sm_seg_hdr));
	h = malloc(sizeof(*h));
	if (h == NULL) {
		smerr("seg (%d) insufficient mem "
		      "to create handle", key);		
	} else {
		memset(h, 0, sizeof(*h));
		h->magic = SM_SEG_MAGIC;
		h->key = key;
		if (size != 0) {
			INIT_SIZE(h, size);
		}
		h->pid = getpid();
		h->tid = pthread_self();
		h->refcnt++; 
	}

	return (h);
}

PRIVATE int
sm_seg_op_item (key_t key, int flags, struct sm_seg_item *useg)
{
	struct sm_seg_item *seg = NULL;
	int rv = SM_FAIL;

	FCN_ENTRY;

	VQE_LIST_FOREACH(seg, &sm_seglist, next) {
		if (seg->seg_h.key == key) {
			if (flags & SEG_ATTACH) {
				seg->seg_h.refcnt++; 
				smlog("seg (%d) refcnt increment (%d)", 
				      key, seg->seg_h.refcnt);
			} else if (flags & SEG_DETACH) {
				seg->seg_h.refcnt--;
				smlog("seg (%d) refcnt decrement (%d)", 
				      key, seg->seg_h.refcnt);
			}
			*useg = *seg;
			rv = SM_SUCCESS;
			goto done;
		}
	}

  done:
	return (rv);
}

PRIVATE int
sm_seg_creat_item (struct sm_seg_hdl *hdl,  struct sm_seg_hdr *segh, int flags)
{
	struct sm_seg_item *seg = NULL;

	FCN_ENTRY;
	
	VQE_LIST_FOREACH(seg, &sm_seglist, next) {
		if (seg->seg_h.key == hdl->key) {
			smerr("seg (%d) already registered by thread %ld", 
			      hdl->key, seg->seg_h.creat_tid);
			return (SM_FAIL);
		       
		}
	}	

	seg = malloc(sizeof(*seg));
	if (seg == NULL) {
		smerr("insufficient mem to create seg (%d) item", hdl->key);
		return (SM_FAIL);
	}
	memset(seg, 0, sizeof(*seg));
	seg->seg_h = *segh;
	seg->base = hdl->base;
	seg->chown = flags & O_CREAT;
	VQE_LIST_INSERT_HEAD(&sm_seglist, seg, next);

	return (SM_SUCCESS);
}

PRIVATE void
sm_init_segh (struct sm_seg_hdr *segh, struct sm_seg_hdl *hdl)
{
	memset(segh, 0, sizeof(*segh));
	segh->magic = SM_SEG_MAGIC;
	segh->key = hdl->key;
	segh->r_size = hdl->r_size;
	segh->t_size = hdl->t_size;
	segh->data_offset = hdl->t_size - hdl->r_size;
	segh->creat_pid = getpid();
	segh->creat_tid = pthread_self();
	segh->refcnt = 0;
	pthread_mutex_init(&segh->s_mutex, &g_mattr);
}

PRIVATE int
__sm_seg_stat (key_t key)
{
	int id;
	
	FCN_ENTRY;

	id = shmget(key, 0, 0);
	return (id);
}

PRIVATE void
sm_hdl_reinit (struct sm_seg_hdl *hdl, struct sm_seg_hdr *segh, void *base)
{
	INIT_SIZE(hdl, segh->r_size);
	hdl->base = base;
}

PRIVATE int
sm_seg_map (struct sm_seg_hdl *hdl, int flags)
{
	int id;
	struct shmid_ds ds;
	struct sm_seg_hdr *segh;
	void *base = NULL;
	struct sm_seg_item si;

	FCN_ENTRY;
	
	if (sm_seg_op_item(hdl->key, SEG_STAT, &si) != SM_FAIL) {
		smlog("seg (%d) already mapped", hdl->key);
		if (!(flags & O_CREAT)) {
			sm_hdl_reinit(hdl, &si.seg_h, si.base);
			return (sm_seg_op_item(hdl->key, SEG_ATTACH, &si));
		} else {
			return (SM_FAIL);
		}
	}

	id = __sm_seg_stat(hdl->key);
	if (id != -1) {
		smerr("shared segment (%d) already exists", hdl->key);
		if ((flags & O_CREAT) && (flags & O_TRUNC)) {
			if (shmctl(id, IPC_RMID, 0) == -1) {
				smerr("shmctl error (%d)", errno);
				return (SM_FAIL);
			}
		} else if (flags & O_CREAT) {
			return (SM_FAIL);
		}
	} 

	if (flags & O_CREAT) {
		if (sm_lock_creat(hdl->key) == SM_FAIL)
			return (SM_FAIL);

		if ((id = 
		     shmget(hdl->key, hdl->t_size, IPC_CREAT | PERMIT_ALL)) == -1) {
			smerr("unable to allocate segment-key %d(%d)", 
			      hdl->key, hdl->r_size);
			goto lock_del;
		} else if (shmctl(id, IPC_STAT, &ds) != 0) {
			smerr("shmctl error (%d)", errno);
		} else if (ds.shm_segsz != hdl->t_size) {
			smerr("IPCSTAT shows size mismatch: %d vs %d\n",
			      ds.shm_segsz, hdl->t_size);
			goto sm_put;
		}  else if ((base = shmat(id, NULL, 0)) == (void *)-1) {
			smerr("shmat error (%d)", errno);
			goto sm_put;
		}
		segh = (struct sm_seg_hdr *)base;
		sm_init_segh(segh, hdl);
	} else {
		if (sm_lock(hdl->key) == SM_FAIL)
			return (SM_FAIL);

		if ((base = shmat(id, NULL, 0)) == (void *)-1) {
			smerr("shmat error (%d)", errno);
			goto sm_put;
		}
		segh = (struct sm_seg_hdr *)base;
		sm_hdl_reinit(hdl, segh, NULL);
	}

	assert(base != NULL);
	hdl->base = base;
	if (sm_seg_creat_item(hdl, segh, flags) == SM_FAIL)
		goto sm_dt;
	if (sm_seg_op_item(hdl->key, SEG_ATTACH, &si) == SM_FAIL)
		goto clean;
	if (sm_unlock(hdl->key) != SM_SUCCESS)
		goto clean;

	smlog("Shared segment (%d) attached at (%p)", hdl->key, base);
	return (SM_SUCCESS);

  clean:
	if ((sm_seg_op_item(hdl->key, SEG_DETACH, &si) == SM_FAIL) ||
	    (sm_seg_delet_item(hdl->key) == SM_FAIL)) {
		smerr("clean error");
	} 
  sm_dt:
	if (shmdt(base) == -1) 
		smerr("smdt error (%d)", errno);
  sm_put:
	if (flags & O_CREAT) {
		if (shmctl(id, IPC_RMID, 0) == -1)
			smerr("shmctl error (%d)", errno);
	}
  lock_del:
	if (flags & O_CREAT) {
		if (sm_lock_destroy(hdl->key) == SM_FAIL)
			smerr("lock_destroy error");
	}
	return (SM_FAIL);
}

PRIVATE int
sm_seg_unmap (struct sm_seg_hdl *hdl, int flags)
{
	int id;
	struct sm_seg_item si;

	FCN_ENTRY;

	if (sm_seg_op_item(hdl->key, SEG_DETACH, &si) == SM_FAIL)
		return (SM_FAIL);
	if (si.chown == 0 && si.seg_h.refcnt == 0) {
		if (sm_seg_delet_item(hdl->key) == SM_FAIL) {
			return (SM_FAIL);
		} else {
			if (shmdt(hdl->base) == -1) {
				smerr("smdt error (%d)", errno); 
				return (SM_FAIL);
			}
			return (SM_SUCCESS);
		}
	} else if (si.chown != 0 || si.destroy) {
		if (si.seg_h.refcnt != 0) {
			if (sm_seg_mark_destroy(hdl->key) == SM_FAIL)
				return(SM_FAIL);
			return (SM_SUCCESS);
		}

		if (sm_seg_delet_item(hdl->key) == SM_FAIL)
			return (SM_FAIL);

		if (sm_lock(hdl->key) == SM_FAIL)
			smerr("sm_lock error");

		id = __sm_seg_stat(hdl->key);
		if (id == -1) {
			smerr("shared segment (%d) does not exist", hdl->key);
			goto unlock;
		} 
		if (shmdt(hdl->base) == -1) {
			smerr("smdt error (%d)", errno); 
			goto unlock;
		}
		if (shmctl(id, IPC_RMID, 0) == -1) {
			smerr("shmctl error (%d)", errno);
			goto unlock;
		}
		if (sm_lock_destroy(hdl->key) == SM_FAIL) {
			smerr("lock_destroy error");
			goto unlock;
		}		
	}

	return (SM_SUCCESS);

  unlock:
	if (sm_unlock(hdl->key) == SM_FAIL)
		smerr("sm_unlock error");
	return (SM_FAIL); 
}

PRIVATE void
sm_init (void) 
{
	int rv;

	FCN_ENTRY;

	pthread_mutex_init(&g_mutex, NULL);
	if ((rv = pthread_mutexattr_init(&g_mattr)) != 0)
		smerr("mutexattr init error (%d)", rv);
	if ((rv = pthread_mutexattr_setpshared(&g_mattr, PTHREAD_PROCESS_SHARED)) != 0)
		smerr("mutexattr set error (%d)", rv);
	VQE_LIST_INIT(&sm_seglist);
	VQE_LIST_INIT(&sm_semlist);
	g_mthread.tid = INVALID_TID;       
	g_mthread.pid = INVALID_PID; 
	g_creatpid = getpid();
}

PUBLIC struct sm_seg_hdl *
sm_seg_instance_get (key_t key, int size, int flags)
{
	struct sm_seg_hdl *h;

	FCN_ENTRY;

	assert(key != 0);
	assert(flags == 0 || (flags & O_CREAT) || 
	       ((flags & O_CREAT) && (flags & O_TRUNC)));
	assert((size == 0 && flags == 0) || (size >= 0));;

	if (__init == 0) {
		sm_init();
		__init++;
	} else if (g_creatpid != getpid()) {
		g_mthread.tid = INVALID_TID;       
		g_mthread.pid = INVALID_PID; 
		g_creatpid = getpid();
	}
	h = sm_seg_gethdl(key, size);
	if (h == NULL)
		return (NULL);

	pthread_mutex_lock(&g_mutex);
	if (sm_seg_map(h, flags) == SM_FAIL) {
		free(h);
		h = NULL;
	}
	pthread_mutex_unlock(&g_mutex);
	return (h);
}

PUBLIC int
sm_seg_instance_put (struct sm_seg_hdl *hdl)
{
	int rv;

	FCN_ENTRY;

	assert(hdl != NULL && hdl->magic == SM_SEG_MAGIC);
	assert(__init != 0);

	pthread_mutex_lock(&g_mutex);
	rv = sm_seg_unmap(hdl, 0);
	pthread_mutex_unlock(&g_mutex);
	free (hdl);

	return (rv);
}

PUBLIC int
sm_seg_size (struct sm_seg_hdl *hdl)
{
	FCN_ENTRY;

	assert(hdl != NULL && hdl->magic == SM_SEG_MAGIC);
	assert(__init != 0);
	return (hdl->r_size);
}

PUBLIC void *
sm_seg_base (struct sm_seg_hdl *hdl)
{
	ptrdiff_t ptr;

	assert(hdl != NULL && hdl->magic == SM_SEG_MAGIC);
	assert(__init != 0);
	ptr = (ptrdiff_t)hdl->base + hdl->t_size - hdl->r_size;
	return ((void *)ptr);
}

PUBLIC int
sm_seg_stat (key_t key) 
{
	int id;

	FCN_ENTRY;

	assert(key != 0);
	pthread_mutex_lock(&g_mutex);
	id = __sm_seg_stat(key);
	pthread_mutex_unlock(&g_mutex);

	return (id);
}

PUBLIC int
sm_mutex_lock (struct sm_seg_hdl *hdl) 
{
	int rv;

	struct sm_seg_hdr *segh;
	
	FCN_ENTRY;

	assert(hdl != NULL && hdl->magic == SM_SEG_MAGIC);
	assert(__init != 0);
	segh = hdl->base;
	if (g_mthread.tid == pthread_self() && g_mthread.pid == g_creatpid) {
		g_mthread.count++;
		smerr("Recursive sm mutex lock");
		return (0);
	}
	rv = pthread_mutex_lock(&segh->s_mutex);
	if (rv == 0) {
		g_mthread.tid = pthread_self();
		g_mthread.pid = g_creatpid;
		g_mthread.count++;
	}
	return (rv);
}

PUBLIC int
sm_mutex_unlock (struct sm_seg_hdl *hdl) 
{
	struct sm_seg_hdr *segh;
	int rv = -1;

	FCN_ENTRY;

	assert(hdl != NULL && hdl->magic == SM_SEG_MAGIC);
	assert(__init != 0);
	segh = hdl->base;
	if (g_mthread.tid == pthread_self() && g_mthread.pid == g_creatpid) {
		if (--g_mthread.count == 0) {
			g_mthread.tid = INVALID_TID;
			g_mthread.pid = INVALID_PID;
			rv = pthread_mutex_unlock(&segh->s_mutex);
		} else {
			smlog("Recursive lock; sm mutex still held (%d)", 
			      g_mthread.count);
			rv = 0;
		}
	} else {
		smerr("unlock but sm mutex not held!");
	}

	return (rv);
}

