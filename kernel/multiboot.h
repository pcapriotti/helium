#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MB_MAGIC_EAX 0x2badb002

#define MB_INFO_MEM (1 << 0)
#define MB_INFO_BOOTDEV (1 << 1)
#define MB_INFO_CMDLINE (1 << 2)
#define MB_INFO_MODS (1 << 3)
#define MB_INFO_SYMS_AOUT (1 << 4)
#define MB_INFO_SYMS_ELF (1 << 5)
#define MB_INFO_MMAP (1 << 6)

typedef struct mb_aout_symbols {
  uint32_t tabsize;
  uint32_t strsize;
  uint32_t addr;
  uint32_t reserved;
} __attribute__((packed)) mb_aout_symbols_t;

typedef struct mb_elf_symbols {
  uint32_t num;
  uint32_t size;
  uint32_t addr;
  uint32_t shndx;
} __attribute__((packed)) mb_elf_symbols_t;

typedef struct mb_module {
  void *start;
  void *end;
  char *string;
  uint32_t reserved;
} __attribute__((packed)) mb_module_t;

typedef struct mb_mmap_entry {
  uint32_t size;
  uint64_t base;
  uint64_t length;
  uint32_t type;
} __attribute__((packed)) mb_mmap_entry_t;

typedef struct multiboot {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  char *cmdline;
  uint32_t mods_count;
  mb_module_t* mods;
  union {
    mb_aout_symbols_t aout;
    mb_elf_symbols_t elf;
  } symbols;
  uint32_t mmap_length;
  mb_mmap_entry_t* mmap;
} __attribute__((packed)) multiboot_t;

void mb_print_mmap(multiboot_t *multiboot);

#endif /* MULTIBOOT_H */
