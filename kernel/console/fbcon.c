#include "bitset.h"
#include "console/console.h"
#include "console/point.h"
#include "core/debug.h"
#include "fbcon.h"
#include "font.h"
#include "frames.h"
#include "graphics.h"
#include "memory.h"

#include <assert.h>
#include <string.h>

#define PIXEL_SIZE 4 /* 32 bit graphics only for now */
#define FAST_MEMCPY32 1

static fbcon_t instance;

static void memcpy32(uint32_t *dst, uint32_t *src, size_t len)
{
#if FAST_MEMCPY32
  __asm__ volatile
    ("rep movsl\n"
     : "=S"(src), "=D"(dst), "=c"(len)
     : "S"(src), "D"(dst), "c"(len)
     : "memory");
#else
  memcpy(dst, src, len << 2);
#endif
}

/* static void memset32(uint32_t *dst, uint32_t value, size_t len) */
/* { */
/*   for (size_t i = 0; i < len; i++) { */
/*     dst[i] = value; */
/*   } */
/* } */

static void flip_buffers(fbcon_t *fbcon)
{
  unsigned int yoffset = fbcon->dirty.y * fbcon->pitch;
  uint32_t *src = fbcon->fb2 + yoffset;
  uint32_t *dst = fbcon->fb + yoffset;

  for (int i = 0; i < fbcon->dirty.height; i++) {
    memcpy32(dst + fbcon->dirty.x, src + fbcon->dirty.x, fbcon->dirty.width);
    dst += fbcon->pitch;
    src += fbcon->pitch;
  }
}

int fbcon_init(fbcon_t *fbcon)
{
  fbcon->fb = (uint32_t *)graphics_mode.framebuffer;
  fbcon->fb_size = graphics_mode.height * graphics_mode.pitch;
  fbcon->dirty = (rect_t) { 0, 0, graphics_mode.width, graphics_mode.height };

  /* allocate memory for back buffer */
  uint64_t frame =
    frames_alloc(&kernel_frames, fbcon->fb_size);
  assert(frame < KERNEL_MEMORY_END);
  fbcon->fb2 = (uint32_t *) (size_t) frame;

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

static inline unsigned ypos(console_t *console, int y)
{
  return (y - console->offset) * graphics_font.header.height;
}

static inline uint32_t *at(fbcon_t *fbcon, console_t *console, point_t p)
{
  return fbcon->fb2 +
    p.x * graphics_font.header.width +
    ypos(console, p.y) * fbcon->pitch;
}

static void invalidate(void *data, console_t *console, point_t p)
{
}

static void set_geometry(void *data, int *width, int *height)
{
  *width = graphics_mode.width / graphics_font.header.width;
  *height = graphics_mode.height / graphics_font.header.height;
}

void render_char(fbcon_t *fbcon, console_t *console,
                 point_t p, uint32_t *pos, char c,
                 uint32_t fg, uint32_t bg)
{
  int pitch = fbcon->pitch - graphics_font.header.width;

  if (!c || c == ' ') {
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

  unsigned int y = ypos(console, console->cur.y);
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

  point_t p = (point_t) {0, console->offset};
  point_t p1 = (point_t) {0, console->height + console->offset};
  uint32_t *pos = at(fbcon, console, p);

  while (point_le(p, p1)) {
    unsigned int index = point_index(console, p);
    uint8_t c = console->buffer[index];
    uint32_t fg = console->fg_buffer[index];
    uint32_t bg = console->bg_buffer[index];
    render_char(fbcon, console, p, pos, c, fg, bg);

    pos += graphics_font.header.width;
    if (p.x == console->width - 1) {
      pos += graphics_font.header.height * fbcon->pitch - graphics_font.header.width * console->width;
    }
    p = point_next(console, p);
  }

  render_cursor(fbcon, console);
  flip_buffers(fbcon);
}

static console_ops_t ops = {
  .repaint = render_buffer,
  .set_geometry = set_geometry,
  .invalidate = invalidate,
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
