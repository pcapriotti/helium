#ifndef CONSOLE_POINT_H
#define CONSOLE_POINT_H

#include "console.h"

static inline int point_equal(point_t p, point_t q)
{
  return p.x == q.x && p.y == q.y;
}

static inline int point_le(point_t p, point_t q)
{
  if (p.y == q.y) {
    return p.x < q.x;
  }
  else {
    return p.y < q.y;
  }
}

static inline int span_is_empty(span_t *span)
{
  return point_equal(span->start, span->end);
}

static inline int span_equal(span_t *a, span_t *b)
{
  return point_equal(a->start, b->start) &&
    point_equal(a->end, b->end);
}

point_t point_next(console_t *console, point_t p);
void span_include_point(console_t *console, span_t *s, point_t p);
unsigned int point_index(console_t *console, point_t p);


#endif /* CONSOLE_POINT_H */
