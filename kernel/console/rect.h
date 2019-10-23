#ifndef CONSOLE_RECT_H
#define CONSOLE_RECT_H

#include "console/point.h"

typedef struct rect {
  int x, y, width, height;
} rect_t;

void rect_expand(rect_t *rect, point_t p);

/* extend the first rect so that it contains the second one */
void rect_bounding(rect_t *rect1, rect_t *rect2);

int rect_intersects(rect_t *rect1, rect_t *rect2);

#endif /* CONSOLE_RECT_H */
