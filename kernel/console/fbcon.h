#ifndef CONSOLE_FBCON_H
#define CONSOLE_FBCON_H

#include <stdint.h>

typedef struct fbcon {
  uint32_t *fb;
  int pitch;
} fbcon_t;

int fbcon_init(fbcon_t *fbcon);
fbcon_t *fbcon_get(void);
struct console_backend *fbcon_backend_get(void);

#endif /* CONSOLE_FBCON_H */
