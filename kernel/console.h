#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

#include "semaphore.h"

typedef struct { int x, y; } point_t;
typedef struct { point_t start, end; } span_t;

typedef struct {
  int pitch;
  span_t dirty;
  point_t cur;
  uint32_t fg, bg;

  uint32_t *fb;
  int width, height;
  int offset;
  uint8_t *buffer;
  uint32_t *fg_buffer;
  uint32_t *bg_buffer;

  semaphore_t write_sem;
  semaphore_t paint_sem;
} console_t;

extern console_t console;

int console_init(void);
uint32_t *console_at(point_t point);
void console_print_str(const char *s);
void console_print_char(char c);
void console_delete_char(point_t p);
void console_set_cursor(point_t c);

void console_render_char(uint32_t *pos, char c, uint32_t fg, uint32_t bg);
void console_render_buffer(void);

void console_debug_print_char(char c);
void console_start_background_task();

void console_set_fg(uint32_t fg);
void console_reset_fg();
void console_set_bg(uint32_t bg);
void console_reset_bg();

#endif /* CONSOLE_H */
