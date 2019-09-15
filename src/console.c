#include "console.h"
#include "debug.h"
#include "graphics.h"

/* 32 bit graphics only for now */

#define PIXEL_SIZE 4

console_t console;

static inline uint32_t mask(uint8_t size, uint8_t position)
{
  return ((~0u) << position) & ((~0u) >> size);
}

static inline uint32_t *at(int x, int y)
{
  return console.fb +
    x * 8 +
    y * console.pitch;
}

int console_init(void)
{
  if (graphics_mode.bpp != 32) return -1;

  console.red = mask(graphics_mode.colour_info.red_mask,
                     graphics_mode.colour_info.red_field);
  console.green = mask(graphics_mode.colour_info.green_mask,
                       graphics_mode.colour_info.green_field);
  console.blue = mask(graphics_mode.colour_info.blue_mask,
                      graphics_mode.colour_info.blue_field);
  console.white = console.red + console.green + console.blue;
  console.fb = (uint32_t *)graphics_mode.framebuffer;

  if (graphics_mode.pitch % PIXEL_SIZE != 0) return -1;
  console.pitch = graphics_mode.pitch / PIXEL_SIZE;

  return 0;
}

uint32_t *console_at(int x, int y)
{
  return at(x, y);
}

void console_print_char(uint32_t *pos, char c, uint32_t fg)
{
  glyph_t *glyph = &graphics_font[(int) c];
  int pitch = console.pitch - 8;
  for (int i = 0; i < 16; i++) {
    uint8_t line = glyph->lines[i];
    for (int j = 0; j < 8; j++) {
      if (line & 0x80) *pos = fg;

      pos++;
      line <<= 1;
    }
    pos += pitch;
  }
}
