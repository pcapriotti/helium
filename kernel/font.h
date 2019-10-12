#ifndef FONT_H
#define FONT_H

#include <stdint.h>

#define PSF2_MAGIC 0x864ab572

enum {
  PSF2_HAS_UNICODE_TABLE = 1,
};

typedef struct psf2_header {
  uint32_t magic;
  uint32_t version;
  uint32_t headersize;
  uint32_t flags;
  uint32_t length;
  uint32_t charsize;
  uint32_t height, width;
} __attribute__((packed)) psf2_header_t;

typedef struct font {
  psf2_header_t header;
  uint8_t *glyphs;
} font_t;

uint8_t *font_glyph(font_t *font, int c);

#endif /* FONT_H */
