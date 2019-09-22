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

int memcmp(const void* a, const void* b, size_t n)
{
  const unsigned char *x = a;
  const unsigned char *y = b;
  for (size_t i = 0; i < n; i++) {
    if (x[i] < y[i]) return -1;
    if (y[i] < x[i]) return 1;
  }
  return 0;
}

int strcmp(const char* a, const char* b)
{
  size_t i;
  for (i = 0; a[i] && b[i]; i++) {
    if (a[i] < b[i]) return -1;
    if (b[i] < a[i]) return 1;
  }
  if (a[i])
    return 1;
  else if (b[i])
    return -1;
  return 0;
}

size_t strlen(const char *s)
{
  size_t i = 0;
  while (s[i]) i++;
  return i;
}

char *strncpy(char *dest, const char *src, size_t n)
{
  size_t i;
  for (i = 0; i < n && src[i]; i++)
    dest[i] = src[i];
  for ( ; i < n; i++)
    dest[i] = '\0';
  return dest;
}

char *strcpy(char *dest, const char *src)
{
  size_t i;
  for (i = 0; src[i]; i++) {
    dest[i] = src[i];
  }
  dest[i] = '\0';
  return dest;
}

char *strtok_r(char *str, const char *delim, char **saveptr)
{
  char *x = str;
  if (!str) x = (char*)(*saveptr);

  if (!x) return 0;

  /* skip initial delimiters */
  while (*x && strchr(delim, *x)) x++;

  /* find delimiter */
  for (int i = 0; x[i]; i++) {
    if (strchr(delim, x[i])) {
      x[i] = '\0';
      *saveptr = &x[i + 1];
      return x;
    }
  }

  *saveptr = 0;
  return x;
}

char *strchr(const char *s, int c)
{
  char *x = (char*)s;
  while (*x) {
    if (c == *x) return x;
    x++;
  }
  return 0;
}
