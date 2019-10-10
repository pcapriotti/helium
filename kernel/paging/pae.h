#ifndef PAGING_PAE_H
#define PAGING_PAE_H

typedef uint64_t pg_pae_entry_t;
typedef pg_pae_entry_t pg_pae_table_t[1 << (PAGE_BITS - 3)];

typedef struct paging_pae {
  page_t *perm;
  page_t *temp;
  pg_pae_entry_t *table3; /* level 3 page */
  pg_pae_entry_t *table2; /* first level 2 page */
  pg_pae_entry_t *tmp_table; /* page of temp mappings */
} paging_pae_t;

int paging_pae_init(paging_pae_t *pg);
void paging_pae_init_ops(pg_ops_t *ops);

#endif /* PAGING_PAE_H */
