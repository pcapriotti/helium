#ifndef CONSOLE_FBCON_H
#define CONSOLE_FBCON_H

#include <stdint.h>
#include <stddef.h>

#include "console/rect.h"

typedef struct fbcon {
  /* front buffer */
  uint32_t *fb;

  /* back buffer */
  uint32_t *fb2;

  /* number of pixels in the back buffer */
  size_t fb_size;

  /* size in cell units */
  int width, height;

  /* cells that need to be rerendered */
  rect_t dirty;

  /* offset of last repaint */
  int last_offset;

  /* number of pixels per row */
  int pitch;
} fbcon_t;

int fbcon_init(fbcon_t *fbcon);
fbcon_t *fbcon_get(void);
struct console_backend *fbcon_backend_get(void);

#endif /* CONSOLE_FBCON_H */
