#include "stdint.h"

extern volatile uint8_t *vesa_framebuffer;
extern unsigned short vesa_pitch;

typedef struct {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t access;
  uint8_t granularity;
  uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

static __attribute__((aligned(8))) gdt_entry_t kernel_gdt[] = {
  { 0, 0, 0, 0, 0, 0 },
  { 0xffff, 0, 0, 0x9a, 0xcf, 0 }, /* code segment */
  { 0xffff, 0, 0, 0x92, 0xcf, 0 }, /* code segment */
};

typedef struct {
  uint16_t size;
  gdt_entry_t *gdt;
} __attribute__((packed)) gdtp_t;

gdtp_t kernel_gdtp = {
  sizeof(kernel_gdt) - 1,
  kernel_gdt
};

void _stage1()
{
  for (int j = 0; j < 10; j++) {
    for (int i = 0; i < 10; i++) {
      vesa_framebuffer[i + j * vesa_pitch] = 4;
    }
  }

  __asm__ volatile("hlt");
  while (1);
}
