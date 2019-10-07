#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void *memset(void *s, int c, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

char *strchr(const char *s, int c);
size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strtok_r(char *str, const char *delim, char **saveptr);
int strcmp(const char *s1, const char *s2);

#endif /* STRING_H */
