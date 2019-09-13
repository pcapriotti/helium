#ifndef GDT_H
#define GDT_H

#include "stdint.h"

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

typedef struct {
  uint16_t offset_low;
  uint16_t segment;
  uint16_t flags;
  uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
  uint32_t prev;
  uint32_t esp0, ss0;
  uint32_t esp1, ss1;
  uint32_t esp2, ss2;
  uint32_t cr3, eip, eflags;
  uint32_t eax, ecx, edx, ebx;
  uint32_t esp, ebp, esi, edi;
  uint32_t es, cs, ss, ds, fs, gs;
  uint32_t ldt;
  uint16_t trap;
  uint16_t iomap_base;
} __attribute__((packed)) tss_t;

enum {
  EFLAGS_VM = 1 << 17,
};

#endif /* GDT_H */
