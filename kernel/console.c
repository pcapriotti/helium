#include "console.h"
#include "core/debug.h"
#include "font.h"
#include "graphics.h"
#include "memory.h"
#include "scheduler.h"
#include "timer.h"

#include <string.h>

#define CONSOLE_DEBUG 0
#if CONSOLE_DEBUG
#define TRACE(...) serial_printf(__VA_ARGS__)
#else
#define TRACE(...) do {} while(0)
#endif

#define PIXEL_SIZE 4 /* 32 bit graphics only for now */

console_t console = {0};

int point_equal(point_t p, point_t q)
{
  return p.x == q.x && p.y == q.y;
}

int point_le(point_t p, point_t q)
{
  if (p.y == q.y) {
    return p.x < q.x;
  }
  else {
    return p.y < q.y;
  }
}

point_t point_next(point_t p)
{
  if (p.x >= console.width - 1) {
    return (point_t) { 0, p.y + 1 };
  }
  else {
    return (point_t) { p.x + 1, p.y };
  }
}

int span_is_empty(span_t *span)
{
  return point_equal(span->start, span->end);
}

int span_equal(span_t *a, span_t *b)
{
  return point_equal(a->start, b->start) &&
    point_equal(a->end, b->end);
}

void span_include_point(span_t *s, point_t p)
{
  if (span_is_empty(s)) {
    s->start = p;
    s->end = point_next(p);
    return;
  }

  if (point_le(p, s->start)) {
    s->start = p;
  }
  else if (point_le(s->end, p)) {
    s->end = p;
  }
}

static inline uint32_t mask(uint8_t size, uint8_t position)
{
  return ((~0u) << position) & ((~0u) >> size);
}

static inline uint32_t *at(point_t p)
{
  return console.fb +
    p.x * graphics_font.header.width +
    (p.y - console.offset) * console.pitch * graphics_font.header.height;
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

  console.width = graphics_mode.width / graphics_font.header.width;
  console.height = graphics_mode.height / graphics_font.header.height;
  console.offset = 0;
  if (console.width <= 0 || console.height <= 0) return -1;

  console.buffer = (uint16_t *) falloc(console.width * console.height * sizeof(uint16_t));

  for (int i = 0; i < console.width * console.height; i++) {
    console.buffer[i] = 0;
  }

  console.dirty.end.y = console.height;

  sem_init(&console.write_sem, 1);
  sem_init(&console.paint_sem, 0);

  return 0;
}

void console_start_background_task()
{
  sched_spawn_task(console_renderer);
}

void console_renderer(void)
{
  while (1) {
    sem_wait(&console.write_sem);
    console_render_buffer();
    sem_signal(&console.write_sem);

    sem_wait(&console.paint_sem);
  }
}

uint32_t *console_at(point_t p)
{
  return at(p);
}

void console_render_cursor(uint32_t fg)
{
  uint32_t *pos = at(console.cur);
  static const int cursor_height = 2;
  pos += (graphics_font.header.height - cursor_height) * console.pitch;
  for (int i = 0; i < cursor_height; i++) {
    for (size_t x = 0; x < graphics_font.header.width; x++) {
      *pos++ = fg;
    }
    pos += console.pitch - graphics_font.header.width;
  }
}

void console_render_char(uint32_t *pos, char c, uint32_t fg, uint32_t bg)
{
  int pitch = console.pitch - graphics_font.header.width;

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

uint32_t palette[8] = {
  0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff,
  0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
};

void console_render_buffer()
{
  TRACE("start rendering\n");
  point_t p = console.dirty.start;
  point_t p1 = console.dirty.end;
  uint32_t *pos = at(p);

  while (point_le(p, p1)) {
    uint16_t c = console.buffer
      [(p.x + (p.y % console.height) * console.width)];
    uint32_t fg = palette[(c >> 8) & 0x7];
    uint32_t bg = palette[(c >> 12) & 0x7];
    console_render_char(pos, c, fg, bg);

    pos += graphics_font.header.width;
    if (p.x == console.width - 1) {
      pos += graphics_font.header.height * console.pitch - graphics_font.header.width * console.width;
    }
    p = point_next(p);
  }
  console_render_cursor(0x00ffffff);

  console.dirty.end = console.dirty.start;
  TRACE("done rendering\n");
}

void console_clear_line(int y)
{
  uint16_t *p = console.buffer +
    (y % console.height) * console.width;
  memset(p, 0, console.width * sizeof(uint16_t));
}

/* print a character, return whether a redraw is needed */
static inline void _console_putchar_at(point_t p, char c, uint8_t colour)
{
  if (c < 0x20 || c > 0x7e) return;

  uint16_t *dst = console.buffer +
    p.x + (p.y % console.height) * console.width;
  uint16_t col = colour << 8;
  if (p.x < 80) {
    *dst = col | c;
    span_include_point(&console.dirty, p);
  }
}

void _console_set_cursor(point_t p)
{
  span_include_point(&console.dirty, console.cur);
  console.cur = p;
  span_include_point(&console.dirty, console.cur);
  while (console.cur.y - console.offset >= console.height) {
    console_clear_line(console.offset + console.height);
    console.offset++;
    console.dirty.start.x = 0;
    console.dirty.start.y = console.offset;
    console.dirty.end.x = 0;
    console.dirty.end.y = console.offset + console.height;
  }
}

void _console_advance_cursor(void)
{
  _console_set_cursor(point_next(console.cur));
}

void _console_print_char(char c, uint8_t colour)
{
  _console_putchar_at(console.cur, c, colour);
  if (c == '\n') {
    _console_set_cursor((point_t) { 0, console.cur.y + 1 });
  }
  else {
    _console_advance_cursor();
  }
}

#define CONSOLE_LOCK_BEGIN() \
  sem_wait(&console.write_sem); \
  span_t _dirty = console.dirty; \
  do { } while(0)
#define CONSOLE_LOCK_END() do { \
  sem_signal(&console.write_sem); \
  if (!span_equal(&_dirty, &console.dirty)) \
    sem_signal(&console.paint_sem); \
  } while (0)

void console_print_char(char c, uint8_t colour)
{
  CONSOLE_LOCK_BEGIN();

  _console_print_char(c, colour);

  CONSOLE_LOCK_END();
}

void console_delete_char(point_t c)
{
  CONSOLE_LOCK_BEGIN();

  uint16_t *p = console.buffer + c.x +
    (c.y % console.height) * console.width;
  *p = 0;
  span_include_point(&console.dirty, c);

  CONSOLE_LOCK_END();
}

void console_set_cursor(point_t c) {
  CONSOLE_LOCK_BEGIN();
  _console_set_cursor(c);
  CONSOLE_LOCK_END();
}

void console_print_str(const char *s, uint8_t colour)
{
  CONSOLE_LOCK_BEGIN();

  while (1) {
    char c = *s++;
    if (!c) break;
    _console_print_char(c, colour);
  }

  CONSOLE_LOCK_END();
}

void console_debug_print_char(char c)
{
  console_print_char(c, 7);
}
