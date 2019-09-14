#include "graphics.h"
#include "v8086.h"

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

int get_graphics_info(vbe_info_t *info)
{
  info->signature = 0x32454256;

  regs16_t regs;
  regs.ax = 0x4f00;
  regs.es = 0;
  regs.di = (uint32_t) info;

  v8086_enter(0x10, &regs);

  if ((regs.ax & 0xff) != 0x4f) {
    return -1;
  }

  return 0;
}

typedef struct vbe_mode_info_t {
  uint16_t attributes; uint8_t win_a_attrs; uint8_t win_b_attrs;
  uint16_t win_granularity; uint16_t win_size;
  uint16_t win_a_start_seg; uint16_t win_b_start_seg;
  ptr16_t win_position_func;
  uint16_t pitch; uint16_t width; uint16_t height;
  uint8_t character_width; uint8_t character_height;
  uint8_t num_planes; uint8_t bpp; uint8_t num_banks; uint8_t mem_model;
  uint8_t bank_size; uint8_t num_pages; uint8_t reserved;
  uint8_t red_mask; uint8_t red_field;
  uint8_t green_mask; uint8_t green_field;
  uint8_t blue_mask; uint8_t blue_field;
  uint16_t reserved1; uint8_t direct_color_info;
  uint8_t *framebuffer; uint8_t *framebuffer2;
  uint16_t offscreen_mem; uint16_t pitch_linear;
  uint8_t num_images_banked; uint8_t num_images_linear;
  uint8_t red_mask_linear; uint8_t red_field_linear;
  uint8_t green_mask_linear; uint8_t green_field_linear;
  uint8_t blue_mask_linear; uint8_t blue_field_linear;
  uint16_t reserved2; uint32_t max_clock;
  uint8_t reserved3[190];
} __attribute__((packed)) vbe_mode_info_t;

/* find the best matching mode among the supported ones */
int graphics_init(vbe_mode_t *req_mode)
{
  vbe_info_t graphics_info;

  if (get_graphics_info(&graphics_info) == -1)
    return -1;

  uint16_t *modes = (uint16_t *)
    ptr16_to_linear(graphics_info.modes);

  vbe_mode_info_t info;

  regs16_t regs;
  int num_modes = 0;

  int best_score = -1;
  int best = -1;

  int req_width = req_mode->width;
  int req_height = req_mode->height;
  int req_bpp = req_mode->bpp;

  for (int i = 0; modes[i] != 0xffff; i++) {
    num_modes++;
    if (best_score == 0) continue;

    regs.ax = 0x4f01;
    regs.cx = modes[i];
    regs.es = 0;
    regs.di = (uint32_t) &info;

    v8086_enter(0x10, &regs);
    if ((regs.ax & 0xff) != 0x4f) return -1;
    /* if (info.mem_model != 0x04) continue; */

    int score = 0;
    if (req_width > 0) {
      int x = req_width - info.width;
      x *= x;
      score += x;
    }
    if (req_height > 0) {
      int x = req_height - info.height;
      x *= x;
      score += x;
    }
    if (req_bpp > 0) {
      int x = req_bpp - info.bpp;
      x *= 200;
      x *= x;
      score += x;
    }


    if (best == -1 || score < best_score) {
      best_score = score;
      best = i;
      req_mode->index = i;
      req_mode->number = modes[i];
      req_mode->width = info.width;
      req_mode->height = info.height;
      req_mode->bpp = info.bpp;
      req_mode->pitch = info.pitch;
      req_mode->framebuffer = info.framebuffer;
    }
  }

  if (best == -1) return -1;
  return num_modes;
}
