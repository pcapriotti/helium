#ifndef STORAGE_STORAGE_H
#define STORAGE_STORAGE_H

typedef struct storage_ops {
  void *(*read)(void *data, void *buf,
                uint32_t offset,
                uint32_t bytes);
  int (*write)(void *data, void *buf,
               uint32_t offset,
               uint32_t bytes);
} storage_ops_t;

#endif /* STORAGE_STORAGE_H */
