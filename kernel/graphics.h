#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

typedef struct {
  uint8_t lines[16];
} __attribute__((packed)) glyph_t;

typedef struct {
  glyph_t glyphs[512];
} __attribute__((aligned(4), packed)) font_t;

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
} vbe_mode_t;

int graphics_init(void *low_heap, vbe_mode_t *req_mode);

extern vbe_mode_t graphics_mode;
extern font_t graphics_font;

#endif /* GRAPHICS_H */
