#include "console/console.h"
#include "console/point.h"
#include "core/debug.h"
#include "font.h"
#include "graphics.h"
#include "kmalloc.h"
#include "memory.h"
#include "scheduler.h"
#include "timer.h"

#include <string.h>

#define CONSOLE_DEBUG 0
#if CONSOLE_DEBUG
#define TRACE(fmt, ...) serial_printf("[console] " fmt __VA_OPT__(,) __VA_ARGS__)
#else
#define TRACE(...) do {} while(0)
#endif

#define DEFAULT_FG 0x00aaaaaa
#define DEFAULT_BG 0

console_t console = {0};

static inline uint32_t mask(uint8_t size, uint8_t position)
{
  return ((~0u) << position) & ((~0u) >> size);
}

static void schedule_repaint(console_t *console)
{
  sem_wait(&console->write_sem);
  if (!console->needs_repaint) {
    console->needs_repaint = 1;
    sem_signal(&console->paint_sem);
  }
  sem_signal(&console->write_sem);
}

static void invalidate(console_t *console, point_t p)
{
  assert(console->backend->ops->invalidate);
  console->backend->ops->invalidate
    (console->backend->ops_data, console, p);
}

static void scroll_up(console_t *console)
{
  assert(console->backend->ops->scroll);
  console->backend->ops->scroll
    (console->backend->ops_data, console);
}

int console_init(console_backend_t *backend)
{
  console.backend = backend;
  console.needs_repaint = 0;

  backend->ops->set_geometry(backend->ops_data,
                             &console.width,
                             &console.height);
  console.offset = 0;
  if (console.width <= 0 || console.height <= 0) return -1;

  console.buffer = (uint8_t *) falloc(console.width * console.height * sizeof(uint8_t));
  console.fg_buffer = (uint32_t *) falloc(console.width * console.height * sizeof(uint32_t));
  console.bg_buffer = (uint32_t *) falloc(console.width * console.height * sizeof(uint32_t));

  console.lengths = kmalloc(console.height * sizeof(int));
  for (int i = 0; i < console.height; i++) {
    console.lengths[i] = 0;
  }

  for (int i = 0; i < console.width * console.height; i++) {
    console.buffer[i] = 0;
    console.fg_buffer[i] = DEFAULT_FG;
    console.bg_buffer[i] = DEFAULT_BG;
  }
  console.fg = DEFAULT_FG;
  console.bg = DEFAULT_BG;

  sem_init(&console.write_sem, 1);

  return 0;
}

static void console_renderer(void)
{
  while (1) {
    sem_wait(&console.write_sem);
    unsigned int ticks = timer_get_tick();
    console.backend->ops->repaint
      (console.backend->ops_data, &console);
    serial_printf("[console] renderered in %u ticks\n",
                  timer_get_tick() - ticks);
    console.needs_repaint = 0;
    sem_signal(&console.write_sem);

    sem_wait(&console.paint_sem);
  }
}

void console_start_background_task()
{
  sched_spawn_task(console_renderer);
}

void console_clear_line(int y)
{
  point_t begin = (point_t) { 0, y };
  unsigned index = point_index(&console, begin);
  int length = console.lengths[y % console.height];
  console.lengths[y % console.height] = 0;
  memset(&console.buffer[index], 0,
         length * sizeof(uint8_t));
  memset(&console.fg_buffer[index], DEFAULT_FG,
         length * sizeof(uint32_t));
  memset(&console.fg_buffer[index], DEFAULT_FG,
         length * sizeof(uint32_t));
  for (int i = 0; i < length; i++) {
    invalidate(&console, (point_t) { i, y });
  }
}

/* print a character */
static inline void _console_putchar_at(point_t p, char c, uint32_t fg, uint32_t bg)
{
  if (p.x > console.lengths[p.y % console.height]) {
    console.lengths[p.y % console.height] = p.x + 1;
  }

  if (c < 0x20 || c > 0x7e) return;

  unsigned int index = point_index(&console, p);
  if (p.x < console.width) {
    console.buffer[index] = c;
    console.fg_buffer[index] = fg;
    console.bg_buffer[index] = bg;
    invalidate(&console, p);
  }
}

void _console_set_cursor(point_t p)
{
  if (point_equal(p, console.cur)) return;

  invalidate(&console, console.cur);
  invalidate(&console, p);

  console.cur = p;
  while (console.cur.y - console.offset >= console.height) {
    console_clear_line(console.offset + console.height);
    console.offset++;
    scroll_up(&console);
  }
}

void _console_advance_cursor(void)
{
  _console_set_cursor(point_next(&console, console.cur));
}

void _console_print_char(char c)
{
  _console_putchar_at(console.cur, c, console.fg, console.bg);
  if (c == '\n') {
    _console_set_cursor((point_t) { 0, console.cur.y + 1 });
  }
  else {
    _console_advance_cursor();
  }
}

void console_print_char(char c)
{
  sem_wait(&console.write_sem);
  _console_print_char(c);
  sem_signal(&console.write_sem);

  schedule_repaint(&console);
}

void _console_delete_char(point_t c)
{
  uint8_t *p = console.buffer + c.x +
    (c.y % console.height) * console.width;
  if (*p == 0) return;
  *p = 0;
  invalidate(&console, c);
}

void console_delete_char(point_t p)
{
  sem_wait(&console.write_sem);
  _console_delete_char(p);
  sem_signal(&console.write_sem);
  schedule_repaint(&console);
}

void console_set_cursor(point_t c) {
  sem_wait(&console.write_sem);
  _console_set_cursor(c);
  sem_signal(&console.write_sem);
  schedule_repaint(&console);
}

void console_print_str(const char *s)
{
  while (1) {
    char c = *s++;
    if (!c) break;
    sem_wait(&console.write_sem);
    _console_print_char(c);
    sem_signal(&console.write_sem);
  }

  schedule_repaint(&console);
}

void console_debug_print_char(char c)
{
  console_print_char(c);
}

void console_set_fg(uint32_t fg)
{
  console.fg = fg;
}

void console_reset_fg(void)
{
  console.fg = DEFAULT_FG;
}

void console_set_bg(uint32_t bg)
{
  console.fg = bg;
}

void console_reset_bg(void)
{
  console.fg = DEFAULT_BG;
}

void console_render_buffer(void)
{
  console.backend->ops->repaint
    (console.backend->ops_data, &console);
}
