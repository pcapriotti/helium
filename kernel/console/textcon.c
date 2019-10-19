#include "console/console.h"
#include "console/point.h"
#include "console/textcon.h"
#include "core/debug.h"

#define WIDTH 80
#define HEIGHT 25

uint16_t *text_buffer = (uint16_t *) 0xb8000;

static void repaint(void *data, console_t *console)
{
  serial_printf("repaint\n");

  const unsigned int index =
    point_index(console, (point_t) { 0, console->offset });
  const unsigned int bufsize = console->width * console->height;
  for (unsigned int i = 0; i < bufsize; i++) {
    text_buffer[i] = 0x700 | console->buffer[index % bufsize];
  }
}

static void set_geometry(void *data, int *width, int *height)
{
  *width = WIDTH;
  *height = HEIGHT;
}

console_ops_t ops = {
  .repaint = repaint,
  .set_geometry = set_geometry,
};

console_backend_t backend = {
  .ops = &ops,
  .ops_data = 0,
};

console_backend_t *textcon_backend_get(void)
{
  return &backend;
}
