#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define GDT_SEL(i) ((uint16_t)((i) * sizeof(gdt_entry_t)))

typedef struct {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t flags;
  uint8_t granularity;
  uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
  uint16_t size;
  void *offset;
} __attribute__((packed)) gdtp_t;

void set_gdt_entry(gdt_entry_t *entry,
                   uint32_t base,
                   uint32_t limit,
                   uint8_t flags,
                   uint8_t granularity);

extern gdtp_t kernel_gdtp;
extern __attribute__((aligned(8))) gdt_entry_t kernel_gdt[];

enum {
  GDT_NULL = 0,
  GDT_CODE,
  GDT_DATA,
  GDT_TASK,

  GDT_NUM_ENTRIES,
};

#endif /* GDT_H */
