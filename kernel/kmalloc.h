#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

struct frames;

int kmalloc_init();

void *kmalloc(size_t bytes);
void kfree(void *p);

#endif /* KMALLOC_H */
