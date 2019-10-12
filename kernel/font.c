#include "font.h"

#include <stddef.h>

uint8_t *font_glyph(font_t *font, int c)
{
  return &font->glyphs[c * font->header.charsize];
}
