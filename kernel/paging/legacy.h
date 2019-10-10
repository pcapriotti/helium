#ifndef PAGING_LEGACY_H
#define PAGING_LEGACY_H

typedef struct paging_legacy {
  page_t *perm;
  page_t *temp;
  pt_entry_t *dir_table;
  pt_entry_t *tmp_table;
} paging_legacy_t;

int paging_legacy_init(paging_legacy_t *pg);
void paging_legacy_init_ops(pg_ops_t *ops);

#endif /* PAGING_LEGACY_H */
