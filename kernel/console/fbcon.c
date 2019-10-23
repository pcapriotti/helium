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
  uint32_t *src = fbcon->fb2 + fbcon->dirty.y * graphics_mode.width;
  uint32_t *dst = fbcon->fb + fbcon->dirty.y * fbcon->pitch;

  serial_printf("dirty: %u %u (%u x %u)\n",
                fbcon->dirty.x,
                fbcon->dirty.y,
                fbcon->dirty.width,
                fbcon->dirty.height);

  /* intersect dirty rect with screen */
  if (fbcon->dirty.x + fbcon->dirty.width > graphics_mode.width) {
    fbcon->dirty.width = graphics_mode.width - fbcon->dirty.x;
  }
  if (fbcon->dirty.y + fbcon->dirty.height > graphics_mode.height) {
    fbcon->dirty.height = graphics_mode.height - fbcon->dirty.y;
  }

  for (int i = 0; i < fbcon->dirty.height; i++) {
    memcpy32(dst + fbcon->dirty.x, src + fbcon->dirty.x, fbcon->dirty.width);
    dst += fbcon->pitch;
    src += graphics_mode.width;
  }

  fbcon->dirty = (rect_t) { 0, 0, 0, 0 };
  fbcon->scroll = 0;
}

int fbcon_init(fbcon_t *fbcon)
{
  fbcon->fb = (uint32_t *)graphics_mode.framebuffer;
  fbcon->fb_size = graphics_mode.height * graphics_mode.width;
  fbcon->dirty = (rect_t) { 0, 0, graphics_mode.width, graphics_mode.height };

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

static inline unsigned ypos(console_t *console, int y)
{
  return (y - console->offset) * graphics_font.header.height;
}

static inline uint32_t *at(fbcon_t *fbcon, console_t *console, point_t p)
{
  return fbcon->fb2 +
    p.x * graphics_font.header.width +
    ypos(console, p.y) * graphics_mode.width;
}

static void invalidate(void *data, console_t *console, point_t p)
{
  fbcon_t *fbcon = data;
  rect_t cell = (rect_t) { p.x, p.y - console->offset, 1, 1 };
  cell.x *= graphics_font.header.width;
  cell.width *= graphics_font.header.width;
  cell.y *= graphics_font.header.height;
  cell.height *= graphics_font.header.height;
  rect_bounding(&fbcon->dirty, &cell);
}

static void scroll(void *data, console_t *console)
{
  fbcon_t *fbcon = data;
  fbcon->scroll++;
}

static void set_geometry(void *data, int *width, int *height)
{
  *width = graphics_mode.width / graphics_font.header.width;
  *height = graphics_mode.height / graphics_font.header.height;
}

void render_char(fbcon_t *fbcon, console_t *console,
                 point_t p, char c, uint32_t fg, uint32_t bg)
{
  rect_t cell;
  cell.x = p.x * graphics_font.header.width;
  cell.y = (p.y - console->offset) * graphics_font.header.height;
  cell.width = graphics_font.header.width;
  cell.height = graphics_font.header.height;

  if (!rect_intersects(&fbcon->dirty, &cell)) return;

  int pitch = graphics_mode.width - graphics_font.header.width;
  uint32_t *pos = fbcon->fb2 + cell.x + cell.y * graphics_mode.width;

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
  pos += (graphics_font.header.height - cursor_height) * graphics_mode.width;

  unsigned int y = ypos(console, console->cur.y);
  for (int i = 0; i < cursor_height; i++) {
    for (size_t x = 0; x < graphics_font.header.width; x++) {
      *pos++ = fg;
    }
    pos += graphics_mode.width - graphics_font.header.width;
  }
}

static void render_buffer(void *data, console_t *console)
{
  fbcon_t *fbcon = data;

  point_t p = (point_t) {0, console->offset};
  point_t p1 = (point_t) {0, console->height + console->offset};

  /* scroll */
  if (fbcon->scroll) {
    size_t scroll_offset = graphics_mode.width *
      fbcon->scroll * graphics_font.header.height;
    memmove(fbcon->fb2, fbcon->fb2 + scroll_offset,
            fbcon->fb_size * sizeof(uint32_t) - scroll_offset);
  }

  while (point_le(p, p1)) {
    unsigned int index = point_index(console, p);
    uint8_t c = console->buffer[index];
    uint32_t fg = console->fg_buffer[index];
    uint32_t bg = console->bg_buffer[index];
    render_char(fbcon, console, p, c, fg, bg);
    p = point_next(console, p);
  }
  render_cursor(fbcon, console);

  rect_t scroll_rect = (rect_t) { 0, 0, 0, 0 };
  scroll_rect.width = graphics_mode.width;
  scroll_rect.height = fbcon->scroll * graphics_font.header.height;
  rect_bounding(&fbcon->dirty, &scroll_rect);

  flip_buffers(fbcon);
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
