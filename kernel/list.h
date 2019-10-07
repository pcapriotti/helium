#ifndef LIST_H
#define LIST_H

#include <assert.h>
#include <stddef.h>

#define LIST_ENTRY(item, ty, member) \
  ((ty *)((void *)(item) - (size_t) (&((ty *)0)->member)))

typedef struct list {
  struct list *next, *prev;
} list_t;

/* insert item before an element of a list */
static inline void list_insert(list_t *list, list_t *item)
{
  assert(list);
  list_t *last = list->prev;
  last->next = item;
  item->prev = last;
  list->prev = item;
  item->next = list;
}

/* insert item at the end of a list */
static inline void list_add(list_t **list, list_t *item)
{
  if (*list) {
    list_insert(*list, item);
  }
  else {
    item->next = item;
    item->prev = item;
    *list = item;
  }
}

/* insert item at the front of a list */
static inline void list_push(list_t **list, list_t *item)
{
  list_add(list, item);
  *list = item;
}

/* remove item from a list */
static inline list_t *list_take(list_t **list, list_t *item)
{
  if (item->next == item) {
    *list = 0;
    return item;
  }

  list_t *prev = item->prev;
  list_t *next = item->next;
  prev->next = next;
  next->prev = prev;

  if (item == *list) {
    *list = next;
  }
  return item;
}

/* remove item from the front of a list */
static inline list_t *list_pop(list_t **list)
{
  if (*list == 0) return 0;
  return list_take(list, *list);
}

/* splice a list before a given element of a list */
static inline void list_splice_at(list_t *item, list_t *list)
{
  assert(item);
  if (list == 0) return;

  list_t *last1 = item->prev;
  list_t *last2 = list->prev;

  last1->next = list;
  list->prev = last1;
  last2->next = item;
  item->prev = last2;
}

/* splice a list at the end of the given one */
static inline void list_splice(list_t **list1, list_t *list2)
{
  if (*list1) {
    list_splice_at(*list1, list2);
  }
  else {
    *list1 = list2;
  }
}

#endif /* LIST_H */
