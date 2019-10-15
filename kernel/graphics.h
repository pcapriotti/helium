#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

#define BIOS_FONT_WIDTH 8
#define BIOS_FONT_HEIGHT 16

typedef struct {
  uint8_t lines[16];
} __attribute__((packed)) bios_glyph_t;

typedef struct {
  bios_glyph_t glyphs[256];
} __attribute__((aligned(4), packed)) bios_font_t;

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

int graphics_init(vbe_mode_t *req_mode, uint16_t *debug_buf);

extern vbe_mode_t graphics_mode;
extern struct font graphics_font;

#endif /* GRAPHICS_H */
