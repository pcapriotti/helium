#ifndef CONSOLE_H
#define CONSOLE_H

#include "stdint.h"

typedef struct {
  uint32_t white, red, green, blue;
  uint32_t *fb;
  int pitch;
} console_t;

extern console_t console;

int console_init(void);
uint32_t *console_at(int x, int y);
void console_print_char(uint32_t *pos, char c, uint32_t fg);


#endif /* CONSOLE_H */
