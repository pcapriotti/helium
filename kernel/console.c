#include "console.h"
#include "core/debug.h"
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
#define FONT_WIDTH 8
#define FONT_HEIGHT 16

console_t console = {0};

static inline uint32_t mask(uint8_t size, uint8_t position)
{
  return ((~0u) << position) & ((~0u) >> size);
}

static inline uint32_t *at(point_t p)
{
  return console.fb +
    p.x * FONT_WIDTH +
    p.y * console.pitch * FONT_HEIGHT;
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
  console.offset = 0;
  if (console.width <= 0 || console.height <= 0) return -1;

  console.buffer = (uint16_t *) falloc(console.width * console.height * sizeof(uint16_t));

  for (int i = 0; i < console.width * console.height; i++) {
    console.buffer[i] = 0;
  }

  console.cur = (point_t){0, 0};
  console.dirty = 0;

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
  pos += (FONT_HEIGHT - cursor_height) * console.pitch;
  for (int i = 0; i < cursor_height; i++) {
    for (int x = 0; x < FONT_WIDTH; x++) {
      *pos++ = fg;
    }
    pos += console.pitch - FONT_WIDTH;
  }
}

void console_render_char(uint32_t *pos, char c, uint32_t fg)
{
  int pitch = console.pitch - FONT_WIDTH;

  if (!c) {
    /* just draw background */
    for (int i = 0; i < FONT_HEIGHT; i++) {
      for (int j = 0; j < FONT_WIDTH; j++) {
        *pos++ = 0;
      }
      pos += pitch;
    }
    return;
  }

  glyph_t *glyph = &graphics_font[(int) c];
  for (int i = 0; i < FONT_HEIGHT; i++) {
    uint8_t line = glyph->lines[i];
    for (int j = 0; j < FONT_WIDTH; j++) {
      *pos++ = (line & 0x80) ? fg : 0;
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
  if (!console.dirty) return;
  TRACE("start rendering\n");
  uint32_t *pos = at((point_t) {0, 0});
  int coffset = console.offset * console.width;
  int num_chars = console.height * console.width;
  for (int i = 0; i < num_chars; i++) {
    uint16_t c = console.buffer[(i + coffset) % num_chars];
    uint32_t fg = palette[(c >> 8) & 0x7];
    console_render_char(pos, c, fg);

    pos += FONT_WIDTH;
    if (i % console.width == console.width - 1) {
      pos += FONT_HEIGHT * console.pitch - FONT_WIDTH * console.width;
    }
  }
  console_render_cursor(0x00ffffff);

  console.dirty = 0;
  TRACE("done rendering\n");
}

void console_clear_line(int y)
{
  uint16_t *p = console.buffer +
    console.cur.x +
    (y % console.height) * console.width;
  memset(p, 0, console.width * sizeof(uint16_t));
}

/* print a character, return whether a redraw is needed */
int _console_print_char(char c, uint8_t colour)
{
  int dirty0 = console.dirty;

  uint16_t *p = console.buffer +
    console.cur.x +
    (console.cur.y % console.height) *
    console.width;
  uint16_t col = colour << 8;
  if (c == '\n') {
    console.cur.y++;
    console.cur.x = 0;
  }
  else if (console.cur.x < 80) {
    *p = col | c;
    console.cur.x++;
    console.dirty = 1;
  }
  while (console.cur.y - console.offset >= console.height) {
    console_clear_line(console.offset + console.height);
    console.offset++;
    console.dirty = 1;
  }

  return console.dirty ^ dirty0;
}

void console_print_char(char c, uint8_t colour)
{
  sem_wait(&console.write_sem);
  int redraw = _console_print_char(c, colour);
  sem_signal(&console.write_sem);

  if (redraw) sem_signal(&console.paint_sem);
}

void console_print_str(const char *s, uint8_t colour)
{
  int redraw = 0;

  char c;
  while ((c = *s++)) {
    sem_wait(&console.write_sem);
    redraw = redraw || _console_print_char(c, colour);
    sem_signal(&console.write_sem);
  }

  if (redraw) sem_signal(&console.paint_sem);
}

void console_debug_print_char(char c)
{
  console_print_char(c, 7);
}
