#include "console/console.h"
#include "console/point.h"
#include "fbcon.h"
#include "font.h"
#include "graphics.h"

#include <stddef.h>

#define PIXEL_SIZE 4 /* 32 bit graphics only for now */

static fbcon_t instance;

int fbcon_init(fbcon_t *fbcon)
{
  fbcon->fb = (uint32_t *)graphics_mode.framebuffer;

  if (graphics_mode.bpp != 32) return -1;
  if (graphics_mode.pitch % PIXEL_SIZE != 0) return -1;
  fbcon->pitch = graphics_mode.pitch / PIXEL_SIZE;
  return 0;
}

fbcon_t *fbcon_get(void)
{
  if (!instance.fb) {
    if (fbcon_init(&instance) == 1) {
      return 0;
    }
  }
  return &instance;
}

static inline uint32_t *at(fbcon_t *fbcon, console_t *console, point_t p)
{
  return fbcon->fb +
    p.x * graphics_font.header.width +
    (p.y - console->offset) * fbcon->pitch * graphics_font.header.height;
}

static void set_geometry(void *data, int *width, int *height)
{
  *width = graphics_mode.width / graphics_font.header.width;
  *height = graphics_mode.height / graphics_font.header.height;
}

void render_char(fbcon_t *fbcon, console_t *console,
                 uint32_t *pos, char c,
                 uint32_t fg, uint32_t bg)
{
  int pitch = fbcon->pitch - graphics_font.header.width;

  if (!c) {
    /* just draw background */
    for (size_t i = 0; i < graphics_font.header.height; i++) {
      for (size_t j = 0; j < graphics_font.header.width; j++) {
        *pos++ = bg;
      }
      pos += pitch;
    }
    return;
  }

  uint8_t *glyph = font_glyph(&graphics_font, c);
  for (size_t i = 0; i < graphics_font.header.height; i++) {
    uint8_t line = glyph[i];
    for (size_t j = 0; j < graphics_font.header.width; j++) {
      *pos++ = (line & 0x80) ? fg : bg;
      line <<= 1;
    }
    pos += pitch;
  }
}


static void render_cursor(fbcon_t *fbcon, console_t *console)
{
  uint32_t *pos = at(fbcon, console, console->cur);
  uint32_t fg = console->fg_buffer[point_index(console, console->cur)];
  static const int cursor_height = 2;
  pos += (graphics_font.header.height - cursor_height) * fbcon->pitch;
  for (int i = 0; i < cursor_height; i++) {
    for (size_t x = 0; x < graphics_font.header.width; x++) {
      *pos++ = fg;
    }
    pos += fbcon->pitch - graphics_font.header.width;
  }
}

static void render_buffer(void *data, console_t *console)
{
  fbcon_t *fbcon = data;

  point_t p = console->dirty.start;
  point_t p1 = console->dirty.end;
  uint32_t *pos = at(fbcon, console, p);

  while (point_le(p, p1)) {
    unsigned int index = point_index(console, p);
    uint8_t c = console->buffer[index];
    uint32_t fg = console->fg_buffer[index];
    uint32_t bg = console->bg_buffer[index];
    render_char(fbcon, console, pos, c, fg, bg);

    pos += graphics_font.header.width;
    if (p.x == console->width - 1) {
      pos += graphics_font.header.height * fbcon->pitch - graphics_font.header.width * console->width;
    }
    p = point_next(console, p);
  }

  render_cursor(fbcon, console);
}

static console_ops_t ops = {
  .repaint = render_buffer,
  .set_geometry = set_geometry,
};

static console_backend_t backend;

console_backend_t *fbcon_backend_get(void)
{
  if (!backend.ops) {
    backend.ops = &ops;
    backend.ops_data = fbcon_get();
  }
  return &backend;
};
