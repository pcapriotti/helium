#include "rect.h"

void rect_expand(rect_t *rect, point_t p)
{
  if (rect->height == 0 || rect->width == 0) {
    rect->x = p.x;
    rect->y = p.y;
  }

  if (p.x < rect->x) {
    rect->x = p.x;
  }
  else if (p.x >= rect->x + rect->width) {
    rect->width = p.x - rect->x + 1;
  }

  if (p.y < rect->y) {
    rect->y = p.y;
  }
  else if (p.y >= rect->y + rect->height) {
    rect->height = p.y - rect->y + 1;
  }
}

void rect_bounding(rect_t *rect1, rect_t *rect2)
{
  if (rect2->height == 0 || rect2->height == 0) return;

  if (rect1->height == 0 || rect1->width == 0) {
    rect1->x = rect2->x;
    rect1->y = rect2->y;
  }

  if (rect2->x < rect1->x) {
    rect1->width += rect1->x - rect2->x;
    rect1->x = rect2->x;
  }
  if (rect2->x + rect2->width > rect1->x + rect1->width) {
    rect1->width = rect2->x + rect2->width - rect1->x;
  }

  if (rect2->y < rect1->y) {
    rect1->height += rect1->y - rect2->y;
    rect1->y = rect2->y;
  }
  if (rect2->y + rect2->height > rect1->y + rect1->height) {
    rect1->height = rect2->y + rect2->height - rect1->y;
  }
}

int rect_intersects(rect_t *rect1, rect_t *rect2)
{
  return ((rect2->x >= rect1->x && rect2->x < rect1->x + rect1->width)
          || (rect2->x + rect2->width >= rect1->x &&
              rect2->x + rect2->width < rect1->x + rect1->width)) &&
    ((rect2->y >= rect1->y && rect2->y < rect1->y + rect1->height)
     || (rect2->y + rect2->height >= rect1->y &&
         rect2->y + rect2->height < rect1->y + rect1->height));
}

void rect_intersection(rect_t *rect1, rect_t *rect2)
{
  int x0 = rect1->x;
  if (x0 < rect2->x)
    x0 = rect2->x;
  int x1 = rect1->x + rect1->width;
  if (x1 > rect2->x + rect2->width)
    x1 = rect2->x + rect2->width;

  int y0 = rect1->y;
  if (y0 < rect2->y)
    y0 = rect2->y;
  int y1 = rect1->y + rect1->height;
  if (y1 > rect2->y + rect2->height)
    y1 = rect2->y + rect2->height;

  rect1->x = x0;
  rect1->width = x1 - x0;
  rect1->y = y0;
  rect1->height = y1 - y0;

  if (rect1->width < 0) rect1->width = 0;
  if (rect1->height < 0) rect1->height = 0;
}
