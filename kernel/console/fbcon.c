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
#define DEBUG_BLIT 0
#define BLIT_MINIMISE_TRANSFERS 1

static fbcon_t instance;

void memcpy32(uint32_t *dst, uint32_t *src, size_t len)
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

int fbcon_init(fbcon_t *fbcon)
{
  fbcon->fb = (uint32_t *)graphics_mode.framebuffer;
  fbcon->fb_size = graphics_mode.height * graphics_mode.width;
  fbcon->width = graphics_mode.width / graphics_font.header.width;
  fbcon->height = graphics_mode.height / graphics_font.header.height;
  fbcon->dirty = (rect_t) { 0, 0, fbcon->width, fbcon->height };

  /* allocate memory for back buffer */
  uint64_t frame =
    frames_alloc(&kernel_frames, fbcon->fb_size * sizeof(uint32_t));
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

static inline unsigned ypos(fbcon_t *fbcon, int y)
{
  return (y % fbcon->height) * graphics_font.header.height;
}

static inline uint32_t *at(fbcon_t *fbcon, point_t p)
{
  return fbcon->fb2 +
    p.x * graphics_font.header.width +
    ypos(fbcon, p.y) * graphics_mode.width;
}

void clear_line_console(fbcon_t *fbcon, int line, int length)
{
  rect_t r = (rect_t) {
    0, line * graphics_font.header.height,
    length * graphics_font.header.width,
    graphics_font.header.height
  };
#if DEBUG_BLIT
  serial_printf("[fbcon] clearing %u,%u (%ux%u)\n",
                r.x, r.y, r.width, r.height);
#endif

  uint32_t *dst = fbcon->fb2 + r.x + r.y * graphics_mode.width;
  for (int y = 0; y < r.height; y++) {
    for (int x = 0; x < r.width; x++) {
      dst[x] = 0;
    }
    dst += graphics_mode.width;
  }
}

static void invalidate(void *data, console_t *console, point_t p)
{
  fbcon_t *fbcon = data;
  rect_t cell = (rect_t) { p.x, p.y % fbcon->height, 1, 1 };
  rect_bounding(&fbcon->dirty, &cell);
}

static void scroll(void *data, console_t *console)
{
  fbcon_t *fbcon = data;
  /* clear_line_console(fbcon, */
  /*                    (console->offset - 1) % fbcon->height, */
  /*                    fbcon->width); */
  rect_t line = (rect_t)
    { 0, (console->offset - 1) % fbcon->height,
      fbcon->width, 1 };
  rect_bounding(&fbcon->dirty, &line);
}

static void set_geometry(void *data, int *width, int *height)
{
  *width = graphics_mode.width / graphics_font.header.width;
  *height = graphics_mode.height / graphics_font.header.height;
}

void render_char(fbcon_t *fbcon, console_t *console,
                 point_t p, char c, uint32_t fg, uint32_t bg)
{
  int pitch = graphics_mode.width - graphics_font.header.width;
  uint32_t *pos = at(fbcon, p);

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
  uint32_t *pos = at(fbcon, console->cur);
  uint32_t fg = console->fg_buffer[point_index(console, console->cur)];
  static const int cursor_height = 2;
  pos += (graphics_font.header.height - cursor_height) * graphics_mode.width;

  unsigned int y = ypos(fbcon, console->cur.y);
  for (int i = 0; i < cursor_height; i++) {
    for (size_t x = 0; x < graphics_font.header.width; x++) {
      *pos++ = fg;
    }
    pos += graphics_mode.width - graphics_font.header.width;
  }
}

static void blit(fbcon_t *fbcon, rect_t *rect, point_t p)
{
  if (rect->width == 0 || rect->height == 0) return;

#if DEBUG_BLIT
  serial_printf("    [blit] %d,%d => %d,%d (%d x %d)\n",
                rect->x, rect->y, p.x, p.y,
                rect->width, rect->height);
#endif
  uint32_t *src = fbcon->fb2 + rect->x + rect->y * graphics_mode.width;
  uint32_t *dst = fbcon->fb + p.x + p.y * fbcon->pitch;

  for (int y = 0; y < rect->height; y++) {
    for (int x = 0; x < rect->width; x++) {
#if BLIT_MINIMISE_TRANSFERS
      if (src[x] == dst[x]) continue;
#endif
      dst[x] = src[x];
    }
    src += graphics_mode.width;
    dst += fbcon->pitch;
  }
}

static void _blit_console(fbcon_t *fbcon, rect_t *rect, point_t p)
{
  if (rect->width == 0 || rect->height == 0) return;
#if DEBUG_BLIT
  serial_printf("  [_blit_console] %d,%d => %d,%d (%d x %d)\n",
                rect->x, rect->y, p.x, p.y,
                rect->width, rect->height);
#endif
  rect->x *= graphics_font.header.width;
  rect->width *= graphics_font.header.width;
  rect->y *= graphics_font.header.height;
  rect->height *= graphics_font.header.height;
  p.x *= graphics_font.header.width;
  p.y *= graphics_font.header.height;

  blit(fbcon, rect, p);
}

static void blit_console(fbcon_t *fbcon, rect_t *rect, console_t *console)
{
#if DEBUG_BLIT
  serial_printf("[blit_console] %d,%d (%d x %d)\n",
                rect->x, rect->y, rect->width, rect->height);
#endif

  int off = console->offset % fbcon->height;

  /* top */
  rect_t r = (rect_t) {
    0,
    off,
    fbcon->width,
    fbcon->height - off
  };
  rect_intersection(&r, rect);
  point_t p = (point_t) { r.x, r.y - off };
  _blit_console(fbcon, &r, p);

  /* bottom */
  r = (rect_t) { 0, 0, fbcon->width, off };
  rect_intersection(&r, rect);
  p = (point_t) { r.x, r.y + fbcon->height - off };
  _blit_console(fbcon, &r, p);
}

static void render_buffer(void *data, console_t *console)
{
  fbcon_t *fbcon = data;

  /* render invalidated cells */
  for (int y = fbcon->dirty.y; y < fbcon->dirty.y + fbcon->dirty.height; y++) {
    for (int x = fbcon->dirty.x; x < fbcon->dirty.x + fbcon->dirty.width; x++) {
      point_t p = (point_t) { x, y };
      unsigned int index = point_index(console, p);
      uint8_t c = console->buffer[index];
      uint32_t fg = console->fg_buffer[index];
      uint32_t bg = console->bg_buffer[index];

      render_char(fbcon, console, p, c, fg, bg);
    }
  }

  /* render cursor */
  render_cursor(fbcon, console);

  rect_t r;
  if (1) {
    if (console->offset == fbcon->last_offset) {
      blit_console(fbcon, &fbcon->dirty, console);
    }
    else {
      serial_printf("[fbcon] scrolling\n");
      rect_t r = (rect_t) { 0, 0, fbcon->width, 1 };
      for (; r.y < fbcon->height; r.y++) {
        blit_console(fbcon, &r, console);
      }
    }
  }
  else {
    r = (rect_t) { 0, 0, graphics_mode.width, graphics_mode.height };
    blit(fbcon, &r, (point_t) {0, 0});
  }

  fbcon->last_offset = console->offset;
  fbcon->dirty = (rect_t) { 0, 0, 0, 0 };
}

static console_ops_t ops = {
  .repaint = render_buffer,
  .set_geometry = set_geometry,
  .invalidate = invalidate,
  .scroll = scroll,
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
