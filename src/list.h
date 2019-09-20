#ifndef LIST_H
#define LIST_H

typedef struct list {
  struct list *next, *prev;
} list_t;

#define LIST_HEAD(name) \
  list_t name = { &(name), &(name) }

static inline void list_add(list_t *x, list_t *list)
{
  list_t *next = list->next;

  list->next = x;
  x->prev = list;

  next->prev = x;
  x->next = next;
}

#define list_entry(x, ty, member) \
  (ty *)((uint8_t *)(x) - (uint8_t *)&((ty *)0)->member)

#define list_foreach_entry(p, h, member) \
  for (p = list_entry((h)->next, typeof(*p), member);   \
       &p->member != (h);                               \
       p = list_entry(p->member.next, typeof(*p), member))

#endif /* LIST_H */
