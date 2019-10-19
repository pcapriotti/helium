#ifndef CONSOLE_FBCON_H
#define CONSOLE_FBCON_H

#include <stdint.h>
#include <stddef.h>

typedef struct fbcon {
  uint32_t *fb;
  uint32_t *fb2;
  size_t fb_size;

  /* bitmap of lines that need repainting */
  uint32_t *dirty_lines;
  size_t max_dirty_col;

  int pitch;
} fbcon_t;

int fbcon_init(fbcon_t *fbcon);
fbcon_t *fbcon_get(void);
struct console_backend *fbcon_backend_get(void);

#endif /* CONSOLE_FBCON_H */
