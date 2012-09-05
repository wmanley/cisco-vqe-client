/*------------------------------------------------------------------
 * VAM Debug Test
 *
 * Copyright (c) 2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdint.h>
#include <features.h>
#include <string.h>
#include <assert.h>

#include "utils/queue_plus.h"
#include "utils/queue_shm.h"
#include "utils/shm_api.h"
#include "utils/zone_mgr.h"

struct mp_tsrapdsc
{
	uint32_t	h_esrap; 
	uint32_t	bufdsc_cnt; 
	SM_SLIST_ENTRY(mp_tsrapdsc) next;
	SM_TAILQ_ENTRY(mp_tsrapdsc) tq_next;
};

struct mp_lsrapdsc
{
	uint32_t	h_esrap; 
	uint32_t	bufdsc_cnt; 
	SM_LIST_ENTRY(mp_lsrapdsc) next;
	SM_TAILQ_ENTRY(mp_lsrapdsc) tq_next;
};

SM_TAILQ_DEFINE(tsrap_head, mp_tsrapdsc) tsrap_tQ;
SM_TAILQ_DEFINE(lsrap_head, mp_lsrapdsc) lsrap_tQ;

struct cm_shm
{
	SM_SLIST_HEAD(, mp_tsrapdsc) tsrap_list;
	SM_LIST_HEAD(, mp_lsrapdsc) lsrap_list;

	SM_TAILQ_HEAD(tsrap_head) tsrap_tQ;
	SM_TAILQ_HEAD(lsrap_head) lsrap_tQ;

	struct mp_tsrapdsc tsdesc[10];
	struct mp_lsrapdsc lsdesc[10];
	SMPTR(char)	cptr;
	SMPTR(char *)  cptrptr;
	SMPTR(char **)	cp3;
	char c1[100];
	SMPTR(struct mp_tsrapdsc)	tsrap_ptr;
	SMPTR(struct mp_tsrapdsc *)	tsrap_ptrptr;

};

void
exec_child (void)
{
	struct sm_seg_hdl *h_1, *h_2;
	struct mp_tsrapdsc *dsc;
	struct mp_lsrapdsc *dscl;

	// Child

	if ((h_2 = sm_seg_instance_get(1001, 1024, O_CREAT|O_TRUNC)) == (struct sm_seg_hdl *)0) {
		fprintf(stderr, "failed");
	} else {
		fprintf(stderr, "segment created successfully");
	}

	sleep(2);
	if ((h_1 = sm_seg_instance_get(1000, 0, 0)) == (struct sm_seg_hdl *)0) {
		fprintf(stderr, "failed");
	} else {
		fprintf(stderr, "child segment created successfully\n");
	}
	struct cm_shm *cm = (struct cm_shm *)sm_seg_base(h_1);
	sm_mutex_lock(h_1);
	if (!SM_SLIST_EMPTY(&cm->tsrap_list)) {
		SM_SLIST_FOREACH(dsc, &cm->tsrap_list, next, h_1) {
			fprintf(stderr, "h_esrap is (%d)\n", dsc->h_esrap);
		}
	} else {
		fprintf(stderr, "Empty list\n");
	}
	if (!SM_LIST_EMPTY(&cm->lsrap_list)) {
		SM_LIST_FOREACH(dscl, &cm->lsrap_list, next, h_1) {
			fprintf(stderr, "list: h_esrap is (%d)\n", dscl->h_esrap);
		}
	} else {
		fprintf(stderr, "Empty list\n");
	}	
	char *ptr = SMPTR_GET(cm->cptr, h_1);
	int i;
	for (i = 0; i < 10; i++)
		fprintf(stderr, "%c", *ptr++);

	char **ptrptr = SMPTR_GET(cm->cptrptr, h_1);
	fprintf(stderr, "(%p/%p)", &cm->cptr, ptrptr);

	char ***ptrptrptr = SMPTR_GET(cm->cp3, h_1);
	fprintf(stderr, "(%p/%p)", &cm->cptrptr, ptrptrptr);

	sm_mutex_unlock(h_1);

	struct vqe_zone *z1= zone_instance_get_sm("testZoneSm_1", 1000, (ptrdiff_t)1024, 0, 0, 0, 0, NULL, NULL);
	z1 = z1;
	struct vqe_zone *z2= zone_instance_get_sm("testZoneSm_2", 1000, (ptrdiff_t)1024+zm_zone_size(60,10), 0, 0, 0, 0, NULL, NULL);
	z2 = z2;

	void *ptrs[6];
	int j;
	for (j = 0; j < 4; j++) {
		for (i = 0; i < 6 ; i++) {
			ptrs[i] = zone_acquire(z1);
		}
		for (i = 0; i < 6 ; i++) {
			zone_release(z1, ptrs[i]);
		}
	}
	for (j = 0; j < 4; j++) {
		for (i = 0; i < 6 ; i++) {
			ptrs[i] = zone_acquire(z2);
		}
		for (i = 0; i < 6 ; i++) {
			zone_release(z2, ptrs[i]);
		}
	}

	zone_instance_put(z1);
        while (1) {
            fprintf(stderr, "child got lock\n");			
            sleep(10);
        }

}

int
main(int argc, char **argv)
{
	int rv;
	struct sm_seg_hdl *h_1, *h_2;
	struct mp_tsrapdsc *dsc;
	struct mp_lsrapdsc *dscl;

	int i;

	fprintf(stderr, "main() %d\n", argc);

	if (argc != 1) {
		exec_child();
		return 1;
	}

	if ((h_1 = sm_seg_instance_get(1000, 1024, O_CREAT|O_TRUNC)) == (struct sm_seg_hdl *)0) {
		fprintf(stderr, "failed");
	} else {
		fprintf(stderr, "segment created successfully");
	}
	
	if ((h_2 = sm_seg_instance_get(1001, 1024, O_CREAT|O_TRUNC)) == (struct sm_seg_hdl *)0) {
		fprintf(stderr, "failed");
	} else {
		fprintf(stderr, "segment created successfully");
	}
	
	if (sm_seg_instance_put(h_2) == -1) {
		fprintf(stderr, "failed");
	} else {
		fprintf(stderr, "segment put-back successfully");
	}

	if (sm_seg_instance_put(h_1) == -1) {
		fprintf(stderr, "failed");
	} else {
		fprintf(stderr, "segment put-back successfully");
	}

	struct vqe_zone *z1= zone_instance_get_loc("testZone", O_CREAT|O_APPEND, 60, 10, NULL, NULL);
	struct vqe_zone *z2= zone_instance_get_loc("testZone", 0, 0, 0, NULL, NULL);
	for (i = 0; i < 6 ; i++) {
		zone_acquire(z1);
		zone_acquire(z2);
	}
	for (i = 0; i < 6 ; i++) {
		void *ptr = zone_acquire(z1);
		zone_release(z1, ptr);
	}
	fprintf(stderr, "item size (%d)\n", zm_elem_size(z1));
	fprintf(stderr, "zone size (%d)\n", zm_zone_size(60, 10));
	zone_instance_put(z1);
	zone_instance_put(z2);

	struct vqe_zone *z3 = zone_alloc("oldZone", ZONE_CLASS_LOCAL,
				     ZONE_FLAGS_STATIC,
				     101,
				     20,
				     0, NULL, NULL);
	z3 = z3;
	zone_destroy(z3);

	int pid;
	if ((pid = fork())) {
		if ((h_1 = sm_seg_instance_get(1000, 1024 + (2*zm_zone_size(60, 10)), O_CREAT|O_TRUNC)) == (struct sm_seg_hdl *)0) {
			fprintf(stderr, "failed");
		} else {
			fprintf(stderr, "segment created successfully\n");
		}

		// Parent
		struct vqe_zone *z1= zone_instance_get_sm("testZoneSm_1", 1000, (ptrdiff_t)1024, O_CREAT, 60, 10, zm_zone_size(60, 10), NULL, NULL);
		z1 = z1;
		struct vqe_zone *z3 = zone_instance_get_sm("testZoneSm_2", 1000, (ptrdiff_t)1024+zm_zone_size(60, 10), O_CREAT, 60, 10, zm_zone_size(60, 10), NULL, NULL);
		z3 = z3;
		for (i = 0; i < 4 ; i++) {
			zone_acquire(z1);
			zone_acquire(z3);
		}

		sm_mutex_lock(h_1);
		fprintf(stderr, "got lock\n");
		struct cm_shm *cm = (struct cm_shm *)sm_seg_base(h_1);
		SM_SLIST_INIT(&cm->tsrap_list);
		for (i = 0; i < 10; i++) {
			dsc = &cm->tsdesc[i];
			dsc->h_esrap = i + 1;
			SM_SLIST_INSERT_HEAD(&cm->tsrap_list, dsc, next, h_1);
			fprintf(stderr, "dsc is %p", dsc);
		}
		
		SM_SLIST_FOREACH(dsc, &cm->tsrap_list, next, h_1) {
			fprintf(stderr, "h_esrap is (%d)\n", dsc->h_esrap);
		}

		int de[10] = { 9, 6, 0, 3, 5, 2, 7, 1, 8, 4 };
		for (i = 0; i < 10; i++) {
			SM_SLIST_REMOVE(&cm->tsrap_list, &cm->tsdesc[de[i]], mp_tsrapdsc, next, h_1);
			SM_SLIST_FOREACH(dsc, &cm->tsrap_list, next, h_1) {
				fprintf(stderr, "%i: h_esrap is (%d)\n", i, dsc->h_esrap);
			}
		}

		SM_SLIST_INSERT_HEAD(&cm->tsrap_list, &cm->tsdesc[de[0]], next, h_1);
		for (i = 1; i < 10; i++) {
			dsc = &cm->tsdesc[de[i]];
			SM_SLIST_INSERT_AFTER(&cm->tsdesc[de[0]], dsc, next, h_1);
			SM_SLIST_FOREACH(dsc, &cm->tsrap_list, next, h_1) {
				fprintf(stderr, "%i-insert: h_esrap is (%d)\n", i, dsc->h_esrap);
			}
		}

		for (i = 0; i < 10; i++) {
			if (!SM_SLIST_EMPTY(&cm->tsrap_list)) {
				SM_SLIST_REMOVE_HEAD(&cm->tsrap_list, next, h_1);
				SM_SLIST_FOREACH(dsc, &cm->tsrap_list, next, h_1) {
					fprintf(stderr, "h_esrap is (%d)\n", dsc->h_esrap);
				}
			}
		}

		SM_LIST_INIT(&cm->lsrap_list);
		for (i = 0; i < 10; i++) {
			dscl = &cm->lsdesc[i];
			dscl->h_esrap = i + 1;
			SM_LIST_INSERT_HEAD(&cm->lsrap_list, dscl, next, h_1);
		}
		SM_LIST_FOREACH(dscl, &cm->lsrap_list, next, h_1) {
			fprintf(stderr, "list: h_esrap is (%d)\n", dscl->h_esrap);
		}
		for (i = 0; i < 10; i++) {
			SM_LIST_REMOVE(&cm->lsdesc[de[i]], next, h_1);
			SM_LIST_FOREACH(dscl, &cm->lsrap_list, next, h_1) {
				fprintf(stderr, "listdel-%i: h_esrap is (%d)\n", i, dscl->h_esrap);
			}
		}
		SM_LIST_INSERT_HEAD(&cm->lsrap_list, &cm->lsdesc[de[0]], next, h_1);
		for (i = 1; i < 10; i++) {
			dscl = &cm->lsdesc[de[i]];
			SM_LIST_INSERT_AFTER(&cm->lsdesc[de[0]], dscl, next, h_1);
			SM_LIST_FOREACH(dscl, &cm->lsrap_list, next, h_1) {
				fprintf(stderr, "%i-insert: h_esrap is (%d)\n", i, dscl->h_esrap);
			}
		}
		for (i = 0; i < 10; i++) {
			SM_LIST_REMOVE(&cm->lsdesc[de[i]], next, h_1);
			SM_LIST_FOREACH(dscl, &cm->lsrap_list, next, h_1) {
				fprintf(stderr, "listdel-%i: h_esrap is (%d)\n", i, dscl->h_esrap);
			}
		}

		SM_LIST_INSERT_HEAD(&cm->lsrap_list, &cm->lsdesc[de[0]], next, h_1);
		SM_LIST_INSERT_HEAD(&cm->lsrap_list, &cm->lsdesc[de[1]], next, h_1);
		for (i = 2; i < 10; i++) {
			dscl = &cm->lsdesc[de[i]];
			SM_LIST_INSERT_BEFORE(&cm->lsdesc[de[1]], dscl, next, h_1);
			SM_LIST_FOREACH(dscl, &cm->lsrap_list, next, h_1) {
				fprintf(stderr, "%i-insert-before: h_esrap is (%d)\n", i, dscl->h_esrap);
			}
		}



		SM_TAILQ_INIT(&cm->lsrap_tQ, h_1);
		for (i = 0; i < 10; i++) {
			dscl = &cm->lsdesc[i];
			dscl->h_esrap = i + 1;
			SM_TAILQ_INSERT_HEAD(&cm->lsrap_tQ, dscl, tq_next, h_1);
		}
		SM_TAILQ_FOREACH(dscl, &cm->lsrap_tQ, tq_next, h_1) {
			fprintf(stderr, "tQ: h_esrap is (%d)\n", dscl->h_esrap);
		}
		for (i = 0; i < 10; i++) {
			SM_TAILQ_REMOVE(&cm->lsrap_tQ, &cm->lsdesc[de[i]], tq_next, h_1);
			SM_TAILQ_FOREACH(dscl, &cm->lsrap_tQ, tq_next, h_1) {
				fprintf(stderr, "tQdel-%i: h_esrap is (%d)\n", i, dscl->h_esrap);
			}
		}
		SM_TAILQ_INSERT_HEAD(&cm->lsrap_tQ, &cm->lsdesc[de[0]], tq_next, h_1);
		for (i = 1; i < 10; i++) {
			dscl = &cm->lsdesc[de[i]];
			SM_TAILQ_INSERT_AFTER(&cm->lsrap_tQ, &cm->lsdesc[de[0]], dscl, tq_next, h_1);
			SM_TAILQ_FOREACH(dscl, &cm->lsrap_tQ, tq_next, h_1) {
				fprintf(stderr, "%i-tQinsert: h_esrap is (%d)\n", i, dscl->h_esrap);
			}
		}
		for (i = 0; i < 10; i++) {
			SM_TAILQ_REMOVE(&cm->lsrap_tQ, &cm->lsdesc[de[i]], tq_next, h_1);
			SM_TAILQ_FOREACH(dscl, &cm->lsrap_tQ, tq_next, h_1) {
				fprintf(stderr, "tQdel-%i: h_esrap is (%d)\n", i, dscl->h_esrap);
			}
		}

		SM_TAILQ_INSERT_HEAD(&cm->lsrap_tQ, &cm->lsdesc[de[0]], tq_next, h_1);
		SM_TAILQ_INSERT_HEAD(&cm->lsrap_tQ, &cm->lsdesc[de[1]], tq_next, h_1);
		for (i = 2; i < 10; i++) {
			dscl = &cm->lsdesc[de[i]];
			SM_TAILQ_INSERT_BEFORE(&cm->lsdesc[de[1]], dscl, tq_next, h_1);
			SM_TAILQ_FOREACH(dscl, &cm->lsrap_tQ, tq_next, h_1) {
				fprintf(stderr, "%i-tQinsert-before: h_esrap is (%d)\n", i, dscl->h_esrap);
			}
		}
		SM_TAILQ_FOREACH_REVERSE(dscl, &cm->lsrap_tQ, lsrap_head, tq_next, h_1) {
			fprintf(stderr, "tQ-reverse: h_esrap is (%d)\n", dscl->h_esrap);
		}
		for (i = 0; i < 10; i++) {
			SM_TAILQ_REMOVE(&cm->lsrap_tQ, &cm->lsdesc[de[i]], tq_next, h_1);
			SM_TAILQ_FOREACH(dscl, &cm->lsrap_tQ, tq_next, h_1) {
				fprintf(stderr, "tQdel-%i: h_esrap is (%d)\n", i, dscl->h_esrap);
			}
		}
		for (i = 0; i < 10; i++) {
			dscl = &cm->lsdesc[i];
			dscl->h_esrap = i + 1;
			SM_TAILQ_INSERT_TAIL(&cm->lsrap_tQ, dscl, tq_next, h_1);
		}
		SM_TAILQ_FOREACH(dscl, &cm->lsrap_tQ, tq_next, h_1) {
			fprintf(stderr, "tQ-tail: h_esrap is (%d)\n", dscl->h_esrap);
		}

		
		cm->cptr = SMPTR_SET(&cm->c1[0], h_1, cm->cptr);
		char *ptr = SMPTR_GET(cm->cptr, h_1);
		memset(cm->c1, 'a', sizeof(cm->c1));
		fprintf(stderr, "(%p/%p)", &cm->c1[0], ptr);

		cm->cptrptr =  SMPTR_SET(&EL(cm->cptr), h_1, cm->cptrptr);
		char **ptrptr = SMPTR_GET(cm->cptrptr, h_1);
		fprintf(stderr, "(%p/%p)", &cm->cptr, ptrptr);

		cm->cp3 = SMPTR_SET(&EL(cm->cptrptr), h_1, cm->cp3);
		char ***ptrptrptr = SMPTR_GET(cm->cp3, h_1);
		fprintf(stderr, "(%p/%p)", &cm->cptrptr, ptrptrptr);

		cm->tsrap_ptr = SMPTR_SET(&cm->tsdesc[0], h_1, cm->tsrap_ptr);
		dsc = SMPTR_GET(cm->tsrap_ptr, h_1);
		fprintf(stderr, "(%p/%p)", &cm->tsdesc[0], dsc);

		cm->tsrap_ptrptr = SMPTR_SET(&EL(cm->tsrap_ptr), h_1, cm->tsrap_ptrptr);
		struct mp_tsrapdsc *dscdsc = SMPTR_GET(cm->tsrap_ptrptr, h_1);
		fprintf(stderr, "(%p/%p)", &cm->tsrap_ptr, dscdsc);

		sm_mutex_unlock(h_1);
		
		sleep(20);
		//sm_mutex_unlock(h_1);
		int status;
		while (1) {
			rv = waitpid(pid, &status, 0);
			if (WIFEXITED(status))
				break;
			else
				fprintf(stderr, "status %d/%d/%d\n", rv, pid, status);
			sleep(10);
		}
	} else {
		rv = execl("/users/afaheem/vam/acme/vam/obj/utils/vam_prog_1", "1", "2", NULL);
		fprintf(stderr, "child done (%d/%d)\n)", rv, errno);
	}
		
	
	return 1;
}

