#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

struct frames;

int kmalloc_init(struct frames *frames);

void *kmalloc(size_t bytes);
void kfree(void *p);

#endif /* KMALLOC_H */
