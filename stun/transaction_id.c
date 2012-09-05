#include "stun_includes.h"
#include "transaction_id.h"

void transaction_id_generate (uint8_t tid[12])
{
  uint ix;

#if 0
  struct timeval tv;

  if (gettimeofday(&tv, NULL) < 0) {
    ix = 0;
  } else {
    ix = sizeof(tv);
    ix = MIN(ix, 12);
    memcpy(tid, sizeof(tv) - ix + (uint8_t *)&tv, ix);
  }
#else
  ix = 0;
#endif
  for (; ix < 11; ix++) {
    do {
      tid[ix] = random();
    } while (!isalpha(tid[ix]));
  }
  tid[11] = '\0';
}

transaction_id_t *transaction_id_create (void)
{
  transaction_id_t *ret;

  ret = MALLOC_STRUCTURE(transaction_id_t);
  if (!ret) {
      return NULL;
  }

  transaction_id_generate(ret->tid);

  ret->next_tid = NULL;
  return ret;
}

bool transaction_id_match (const uint8_t *left, const uint8_t *right)
{
  return memcmp(left, right, 12) == 0;
}

transaction_id_t *transaction_id_find_in_list (transaction_id_t **head, 
					       const uint8_t *match_tid,
					       bool remove_from_list)
{
  transaction_id_t *ptid, *qtid;

  qtid = NULL;
  ptid = *head;
  while (ptid != NULL) {
    if (transaction_id_match(ptid->tid, match_tid)) {
      if (remove_from_list) {
	if (qtid == NULL) {
	  *head = ptid->next_tid;
	} else {
	  qtid->next_tid = ptid->next_tid;
	}
      }
      return ptid;
    }
    qtid = ptid;
    ptid = ptid->next_tid;
  }
  return NULL;
}
  
