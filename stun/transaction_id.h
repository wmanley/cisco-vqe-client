
#ifndef __TRANSACTION_ID_H__
#define __TRANSACTION_ID_H__ 1

typedef struct transaction_id_t {
  struct transaction_id_t *next_tid;

  uint8_t tid[12];
} transaction_id_t;

transaction_id_t *transaction_id_create(void);

void transaction_id_generate(uint8_t tid[12]);

bool transaction_id_match(const uint8_t *, const uint8_t *);

transaction_id_t *transaction_id_find_in_list(transaction_id_t **head, 
					      const uint8_t *match_tid,
					      bool remove_from_list);
#endif
