#ifndef CONSOLE_H
#define CONSOLE_H

#include "stdint.h"

typedef struct { int x, y; } point_t;

typedef struct {
  uint32_t white, red, green, blue;
  uint32_t *fb;
  int pitch;

  int width, height;
  uint16_t *buffer;
  point_t cur;
} console_t;

extern console_t console;

int console_init(void);
uint32_t *console_at(point_t point);
void console_set_buffer(uint16_t *buf);
void console_print_str(const char *s, uint8_t colour);

void console_render_char(uint32_t *pos, char c, uint32_t fg);

#endif /* CONSOLE_H */
