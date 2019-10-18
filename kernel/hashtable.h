#ifndef HASHTABLE_H
#define HASHTABLE_H

typedef const char *ht_key_t;

struct hashtable_struct_t;
typedef struct hashtable_struct_t hashtable_t;

hashtable_t *ht_new();
void ht_insert(hashtable_t *ht, ht_key_t key, void *value);
void *ht_get(hashtable_t *ht, ht_key_t key);
void ht_del(hashtable_t *ht);
int ht_size(hashtable_t *ht);

#endif /* HASHTABLE_H */
