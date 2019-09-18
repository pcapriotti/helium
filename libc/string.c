#include "string.h"
#include <stdint.h>

void *memset(void *s, int c, size_t n)
{
  uint8_t *x = s;
  for (size_t i = 0; i < n; i++) x[i] = c;
  return s;
}

void *memmove(void *dst, const void *src, size_t n)
{
  const uint8_t *x = src;
  uint8_t *y = dst;

  if (x > y) {
    for (size_t i = 0; i < n; i++)
      y[i] = x[i];
  }
  else if (x < y) {
    for (size_t i = n; i != 0; i--) {
      y[i - 1] = x[i - 1];
    }
  }
  return y;
}
