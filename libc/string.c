#include "string.h"
#include <stdint.h>

void *memset(void *s, int c, size_t n)
{
  uint8_t *x = s;
  for (size_t i = 0; i < n; i++) x[i] = c;
  return s;
}
