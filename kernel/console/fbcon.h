#ifndef CONSOLE_FBCON_H
#define CONSOLE_FBCON_H

#include <stdint.h>
#include <stddef.h>

#include "console/rect.h"

typedef struct fbcon {
  uint32_t *fb;
  uint32_t *fb2;
  size_t fb_size;

  rect_t dirty;

  int pitch;
} fbcon_t;

int fbcon_init(fbcon_t *fbcon);
fbcon_t *fbcon_get(void);
struct console_backend *fbcon_backend_get(void);

#endif /* CONSOLE_FBCON_H */
