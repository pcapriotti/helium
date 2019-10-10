#ifndef PAGING_LEGACY_H
#define PAGING_LEGACY_H

typedef uint32_t pg_legacy_entry_t;
typedef pg_legacy_entry_t pg_legacy_table_t[1 << (PAGE_BITS - 2)];

typedef struct paging_legacy {
  page_t *perm;
  page_t *temp;
  pg_legacy_entry_t *dir_table;
  pg_legacy_entry_t *tmp_table;
} paging_legacy_t;

int paging_legacy_init(paging_legacy_t *pg);
void paging_legacy_init_ops(pg_ops_t *ops);

#endif /* PAGING_LEGACY_H */
