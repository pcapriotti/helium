#include "console/console.h"
#include "console/point.h"
#include "console/textcon.h"
#include "core/debug.h"
#include "core/v8086.h"

#define WIDTH 80
#define HEIGHT 25

uint16_t *text_buffer = (uint16_t *) 0xb8000;
static int cursor_enabled = 0;

static uint8_t quantize(uint32_t colour)
{
  uint8_t red = (colour >> 16) & 0xff;
  uint8_t green = (colour >> 8) & 0xff;
  uint8_t blue = colour & 0xff;
  int bright = red >= 0x55 && green >= 0x55 && red >= 0x55;

#define Q(x) (x > 0x80)
  return (bright << 3) | (Q(red) << 2) | (Q(green) << 1) | Q(blue);
#undef Q
}

static void repaint(void *data, console_t *console)
{
  if (!cursor_enabled) {
    regs16_t regs;
    regs.eax = 0x0100;
    regs.ecx = 0x0e0f;
    bios_int(0x10, &regs);
    cursor_enabled = 1;
  }

  unsigned int index =
    point_index(console, (point_t) { 0, console->offset });
  const unsigned int bufsize = console->width * console->height;
  for (unsigned int i = 0; i < bufsize; i++) {
    uint8_t fg = quantize(console->fg_buffer[index % bufsize]);
    uint8_t bg = quantize(console->bg_buffer[index % bufsize]);
    text_buffer[i] = (bg << 12) | (fg << 8) | console->buffer[index % bufsize];
    index++;
  }

  /* set cursor position */
  {
    regs16_t regs;
    regs.eax = 0x0200;
    regs.ebx = 0;
    regs.edx = (console->cur.y << 8) |
      (console->cur.x & 0xff);
    bios_int(0x10, &regs);
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
