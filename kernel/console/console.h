#ifndef CONSOLE_CONSOLE_H
#define CONSOLE_CONSOLE_H

#include <stdint.h>

#include "semaphore.h"
#include "console/point.h"

struct console;

typedef struct console_ops {
  void (*repaint)(void *data, struct console *console);
  void (*set_geometry)(void *data, int* width, int *height);
  void (*invalidate)(void *data, struct console *console, point_t p);
  void (*scroll)(void *data, struct console *console);
} console_ops_t;

typedef struct console_backend {
  console_ops_t *ops;
  void *ops_data;
} console_backend_t;

typedef struct console {
  point_t cur;
  uint32_t fg, bg;
  int width, height;
  int *lengths; /* length of each line */
  int offset;
  semaphore_t write_sem;
  semaphore_t paint_sem;
  int needs_repaint;

  uint8_t *buffer;
  uint32_t *fg_buffer;
  uint32_t *bg_buffer;

  console_backend_t *backend;
} console_t;

extern console_t console;

int console_init(console_backend_t *backend);
void console_print_str(const char *s);
void console_print_char(char c);
void console_delete_char(point_t p);
void console_set_cursor(point_t c);

void console_render_buffer(void);

void console_debug_print_char(char c);
void console_start_background_task();

void console_set_fg(uint32_t fg);
void console_reset_fg();
void console_set_bg(uint32_t bg);
void console_reset_bg();

#endif /* CONSOLE_CONSOLE_H */
