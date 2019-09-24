#include "core/debug.h"
#include "console.h"

void task_a()
{
  int i = 0;
  while (1) {
    console.buffer[console.offset * console.width] = 0x400 | ('0' + i);
    i = (i + 1) % 10;
    console_render_buffer();
  }
}

void task_b()
{
  int i = 0;
  while (1) {
    console.buffer[console.offset * console.width + 1] = 0x200 | ('0' + i);
    i = (i + 1) % 10;
    console_render_buffer();
  }
}
