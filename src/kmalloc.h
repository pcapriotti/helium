#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

#define ROUND(a, i) (((uint32_t)a + (1 << (i)) - 1) >> i)

void *kmalloc(size_t bytes);
void kfree(void *p);

#endif /* KMALLOC_H */
