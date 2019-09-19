#include "console.h"
#include "debug.h"
#include "graphics.h"
#include "memory.h"

#define PIXEL_SIZE 4 /* 32 bit graphics only for now */
#define FONT_WIDTH 8
#define FONT_HEIGHT 16

console_t console;

static inline uint32_t mask(uint8_t size, uint8_t position)
{
  return ((~0u) << position) & ((~0u) >> size);
}

static inline uint32_t *at(point_t p)
{
  return console.fb +
    p.x * 8 +
    p.y * console.pitch;
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

  console.width = graphics_mode.width / FONT_WIDTH;
  console.height = graphics_mode.height / FONT_HEIGHT;
  if (console.width <= 0 || console.height <= 0) return -1;

  console.buffer = (uint16_t *) falloc(console.width * console.height * sizeof(uint16_t));
  for (int i = 0; i < console.width * console.height; i++) {
    console.buffer[i] = 0;
  }

  console.cur = (point_t){0, 0};
  return 0;

}

uint32_t *console_at(point_t p)
{
  return at(p);
}

void console_render_char(uint32_t *pos, char c, uint32_t fg)
{
  if (!c || c == 0x20) return;
  if (c < 0) return;
  glyph_t *glyph = &graphics_font[(int) c];
  int pitch = console.pitch - 8;
  for (int i = 0; i < FONT_HEIGHT; i++) {
    uint8_t line = glyph->lines[i];
    for (int j = 0; j < FONT_WIDTH; j++) {
      if (line & 0x80) *pos = fg;

      pos++;
      line <<= 1;
    }
    pos += pitch;
  }
}

uint32_t palette[8] = {
  0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff,
  0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
};

void console_render_buffer()
{
  uint32_t *pos = at((point_t) {0, 0});
  for (int i = 0; i < console.height * console.width; i++) {
    uint16_t c = console.buffer[i];
    uint32_t fg = palette[(c >> 8) & 0x7];
    console_render_char(pos, c, fg);

    pos += FONT_WIDTH;
    if (i % console.width == console.width - 1) {
      pos += FONT_HEIGHT * console.pitch - FONT_WIDTH * console.width;
    }
  }
}

void console_print_char(char c, uint8_t colour)
{
  uint16_t *p = console.buffer + console.cur.x + console.cur.y * console.width;
  uint16_t col = colour << 8;
  if (c == '\n') {
    p += console.width - console.cur.x;

    console.cur.y++;
    console.cur.x = 0;
  }
  else if (console.cur.x < 80) {
    *p++ = col | c;
    console.cur.x++;
  }
  if (console.cur.y == console.height) {
    int last_line = console.width * (console.height - 1);
    for (int i = 0; i < last_line; i++) {
      console.buffer[i] = console.buffer[i + console.width];
    }
    for (int i = 0; i < console.width; i++) {
      vga_text[last_line + i] = 0;
    }
    console.cur.y--;
  }
}

void console_print_str(const char *s, uint8_t colour)
{
  char c;
  while ((c = *s++)) {
    console_print_char(c, colour);
  }
}
