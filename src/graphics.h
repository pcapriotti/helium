#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "gdt.h"

typedef struct vbe_mode_t {
  uint16_t index;
  uint16_t number;
  uint16_t width;
  uint16_t height;
  uint16_t bpp;
  uint16_t pitch;
  uint8_t *framebuffer;
} __attribute__((packed)) vbe_mode_t;

int graphics_init(vbe_mode_t *req_mode);

#endif /* GRAPHICS_H */
