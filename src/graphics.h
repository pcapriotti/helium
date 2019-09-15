#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "stage1.h"

typedef struct {
  uint8_t lines[16];
} __attribute__((packed)) glyph_t;
typedef glyph_t font_t[512];

typedef struct {
  uint8_t red_mask; uint8_t red_field;
  uint8_t green_mask; uint8_t green_field;
  uint8_t blue_mask; uint8_t blue_field;
} __attribute__((packed)) vbe_colour_info_t;

typedef struct {
  uint16_t index;
  uint16_t number;
  uint16_t width;
  uint16_t height;
  uint16_t bpp;
  uint16_t pitch;
  uint8_t *framebuffer;
  vbe_colour_info_t colour_info;
} __attribute__((packed)) vbe_mode_t;

int graphics_init(vbe_mode_t *req_mode);

extern vbe_mode_t graphics_mode;
extern font_t graphics_font;

static inline void graphics_plot(uint8_t *pos, uint32_t colour)
{
  switch (graphics_mode.bpp) {
  case 8:
    *pos = colour;
    break;
  case 16:
    *((uint16_t *)pos) = colour;
    break;
  case 32:
    *((uint32_t *)pos) = colour;
    break;
  }
}

#endif /* GRAPHICS_H */
