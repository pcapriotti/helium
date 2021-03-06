#include "core/debug.h"
#include "core/v8086.h"
#include "font.h"
#include "frames.h"
#include "graphics.h"
#include "memory.h"
#include "paging/paging.h"

#define GRAPHICS_DEBUG 0

typedef struct {
  uint32_t signature;
  uint16_t version;
  ptr16_t oem;
  uint32_t capabilities;
  ptr16_t modes;
  uint16_t memory; /* in 64 kB blocks */
  uint16_t sw_version;
  ptr16_t vendor;
  ptr16_t product;
  ptr16_t revision;
  uint16_t vbe_version;
  ptr16_t accel_modes;
  uint8_t reserved[216];
  uint8_t scratchpad[256];
} __attribute__((packed)) vbe_info_t;


font_t graphics_font;
vbe_mode_t graphics_mode = {0};

typedef struct vbe_mode_info_t {
  uint16_t attributes; uint8_t win_a_attrs; uint8_t win_b_attrs;
  uint16_t win_granularity; uint16_t win_size;
  uint16_t win_a_start_seg; uint16_t win_b_start_seg;
  ptr16_t win_position_func;
  uint16_t pitch; uint16_t width; uint16_t height;
  uint8_t character_width; uint8_t character_height;
  uint8_t num_planes; uint8_t bpp; uint8_t num_banks; uint8_t mem_model;
  uint8_t bank_size; uint8_t num_pages; uint8_t reserved;
  vbe_colour_info_t colour_info;
  uint16_t reserved1; uint8_t direct_color_info;
  uint8_t *framebuffer; uint8_t *framebuffer2;
  uint16_t offscreen_mem; uint16_t pitch_linear;
  uint8_t num_images_banked; uint8_t num_images_linear;
  vbe_colour_info_t colour_info_linear;
  uint16_t reserved2; uint32_t max_clock;
  uint8_t reserved3[190];
} __attribute__((packed)) vbe_mode_info_t;

int get_graphics_info(vbe_info_t *info)
{
  info->signature = 0x32454256;
  ptr16_t infop = linear_to_ptr16((uint32_t) info);

  regs16_t regs;
  regs.eax = 0x4f00;
  regs.es = infop.segment;
  regs.edi = infop.offset;

#if GRAPHICS_DEBUG
  serial_printf("get_graphics_info at %04x:%#04x (%p)\n",
          infop.segment, infop.offset, info);
#endif
  bios_int(0x10, &regs);
#if GRAPHICS_DEBUG
  {
    int count = 0;
    uint16_t *modes0 = (uint16_t *)ptr16_to_linear(info->modes);
    uint16_t *modes = modes0;
    while (*modes != 0xffff) {
      count++;
      modes++;
    }
    serial_printf("found %d modes at %#04x:%#04x (%p)\n", count,
            info->modes.segment, info->modes.offset, modes0);
  }
#endif

  if ((regs.eax & 0xffff) != 0x004f)
    return -1;

  return 0;
}

int get_mode(uint16_t number, vbe_mode_info_t *info)
{
  if (number == 0) return -1;

  regs16_t regs;
  ptr16_t infop = linear_to_ptr16((uint32_t) info);

#if GRAPHICS_DEBUG
    serial_printf("requesting info for mode %#x at %#04x:%#04x\n",
            number, infop.segment, infop.offset);
#endif

  regs.eax = 0x4f01;
  regs.ecx = number;
  regs.es = infop.segment;
  regs.edi = infop.offset;

  bios_int(0x10, &regs);

#if GRAPHICS_DEBUG
    serial_printf("results for %#x: %ux%u %u bits, eax = %x\n",
            (uint32_t) number,
            (uint32_t) info->width,
            (uint32_t) info->height,
            (uint32_t) info->bpp,
            regs.eax);
#endif

  if ((regs.eax & 0xffff) != 0x004f)
    return -1;

  return 0;
}

/* find the best matching mode among the supported ones */
int find_mode(void *low_heap, vbe_mode_t *req_mode, uint16_t *modes)
{
  vbe_mode_info_t *info = low_heap;

  int num_modes = 0;

  int best_score = -1;
  int best = -1;

  int req_width = req_mode->width;
  int req_height = req_mode->height;
  int req_bpp = req_mode->bpp;

  for (int i = 0; modes[i] != 0xffff; i++) {
    num_modes++;
    if (best_score == 0) continue;

    if (get_mode(modes[i], info) == -1) continue;
    if (!(info->attributes & 0x1)) continue; /* mode not supported */

    int score = 0;
    if (req_width > 0) {
      int x = req_width - info->width;
      x *= x;
      score += x;
    }
    if (req_height > 0) {
      int x = req_height - info->height;
      x *= x;
      score += x;
    }
    if (req_bpp > 0) {
      int x = req_bpp - info->bpp;
      x *= 200;
      x *= x;
      score += x;
    }

/* #if GRAPHICS_DEBUG */
/*     serial_printf("score: %u\n", score); */
/* #endif */

    if (best == -1 || score < best_score) {
      best_score = score;
      best = i;
      req_mode->index = i;
      req_mode->number = modes[i];
      req_mode->width = info->width;
      req_mode->height = info->height;
      req_mode->bpp = info->bpp;
      req_mode->pitch = info->pitch;
      req_mode->framebuffer = info->framebuffer;
      req_mode->colour_info = info->colour_info;
    }
  }

  if (best == -1) return -1;
  return num_modes;
}

int get_font(font_t *font)
{
  /* use hack font */
  memcpy(font, &font_terminus, sizeof(font_t));
  return 0;
}

int get_bios_font(font_t *font)
{
  regs16_t regs;

  regs.eax = 0x1130;
  regs.ebx = 0x0600;
  bios_int(0x10, &regs);

  bios_font_t *src = (bios_font_t *)
    seg_off_to_linear(regs.es, regs.ebp);

  font->header.magic = PSF2_MAGIC;
  font->header.version = 0;
  font->header.headersize = sizeof(psf2_header_t);
  font->header.flags = 0;
  font->header.length = 256;
  font->header.charsize = 16;
  font->header.height = 16;
  font->header.width = 8;

  memcpy(font->glyphs, src, 16 * 256);

  return 0;
}

static int init_and_find_mode(vbe_info_t *graphics_info,
                              vbe_mode_info_t *tmp_mode_info,
                              vbe_mode_t *req_mode)
{
  if (get_graphics_info(graphics_info) == -1)
    return -1;

  if (req_mode->number != 0) {
    if (get_mode(req_mode->number, tmp_mode_info) == -1)
      return -1;

    req_mode->index = 0;
    req_mode->width = tmp_mode_info->width;
    req_mode->height = tmp_mode_info->height;
    req_mode->bpp = tmp_mode_info->bpp;
    req_mode->pitch = tmp_mode_info->pitch;
    req_mode->framebuffer = tmp_mode_info->framebuffer;
    req_mode->colour_info = tmp_mode_info->colour_info;
  }
  else {
    uint16_t *modes = (uint16_t *)
      ptr16_to_linear(graphics_info->modes);

    /* find best matching mode */
    if (find_mode(tmp_mode_info, req_mode, modes) == -1)
      return -1;
  }

  return 0;
}

int graphics_init(vbe_mode_t *req_mode, uint16_t *debug_buf)
{
  void *low_heap = (void *)(size_t)frames_alloc
    (&dma_frames, sizeof(vbe_info_t) + sizeof(vbe_mode_info_t));
  vbe_info_t *graphics_info = low_heap;
  vbe_mode_info_t *tmp_mode_info = (void *) (graphics_info + 1);

  int ret = init_and_find_mode(graphics_info, tmp_mode_info, req_mode);
  size_t info_mem = graphics_info->memory;

  frames_free(&dma_frames, (size_t) low_heap);

  if (ret == -1) return -1;
  if (req_mode->bpp != 32) {
    serial_printf("unsupported mode %#x: %ux%u %u bits\n",
            (uint32_t) req_mode->number,
            (uint32_t) req_mode->width,
            (uint32_t) req_mode->height,
            (uint32_t) req_mode->bpp);
    return -1;
  }

  /* map framebuffer into virtual memory */
  req_mode->fb_size = info_mem << 16;

  /* only map one page of the framebuffer */
  req_mode->framebuffer = paging_perm_map_pages
    ((size_t) req_mode->framebuffer,
     req_mode->height * req_mode->pitch);

  /* save debug console */
  for (int i = 0; i < 25; i++) {
    int p = 80 * i;
    for (int j = 0; j < 80; j++) {
      debug_buf[p] = vga_text[p];
      p++;
    }
  }

  /* enable mode */
  regs16_t regs;
  regs.eax = 0x4f02;
  regs.ebx = 0x4000 | req_mode->number;
  bios_int(0x10, &regs);
  if ((regs.eax & 0xff) != 0x4f) return -1;
  graphics_mode = *req_mode;

  /* load font */
  if (get_font(&graphics_font) == -1) return -1;

  return 0;
}

