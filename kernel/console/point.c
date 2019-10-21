#include "console/console.h"
#include "console/point.h"

void span_include_point(console_t *console, span_t *s, point_t p)
{
  if (span_is_empty(s)) {
    s->start = p;
    s->end = point_next(console, p);
    return;
  }

  if (point_le(p, s->start)) {
    s->start = p;
  }
  else if (point_le(s->end, p)) {
    s->end = p;
  }
}

point_t point_next(console_t *console, point_t p)
{
  if (p.x >= console->width - 1) {
    return (point_t) { 0, p.y + 1 };
  }
  else {
    return (point_t) { p.x + 1, p.y };
  }
}

unsigned int point_index(console_t *console, point_t p)
{
  return p.x + (p.y % console->height) * console->width;
}
