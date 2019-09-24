#ifndef __ELF_H__
#define __ELF_H__

#include <stdint.h>
#include <stddef.h>

enum {
  ELF_BITS_32 = 1,
  ELF_BITS_64 = 2
};

enum {
  ELF_LITTLE_ENDIAN = 1,
  ELF_BIG_ENDIAN = 2
};

enum {
  ELF_TYPE_REL = 1,
  ELF_TYPE_EXE,
  ELF_TYPE_SHARED,
  ELF_TYPE_CORE
};

enum {
  ELF_ARCH_NONE = 0,
  ELF_ARCH_SPARC = 2,
  ELF_ARCH_X86 = 3,
  ELF_ARCH_MIPS = 8,
  ELF_ARCH_PPC = 20,
  ELF_ARCH_ARM = 40,
  ELF_ARCH_SH = 42,
  ELF_ARCH_IA64 = 50,
  ELF_ARCH_X86_64 = 62,
  ELF_ARCH_AARCH64 = 183
};

typedef struct
{
  uint32_t magic;
  uint8_t bits;
  uint8_t endianness;
  uint8_t version;
  uint8_t os_abi;
  uint8_t _padding[8];
  uint16_t type;
  uint16_t arch;
  uint32_t elf_version;
  void *entry;
  uint32_t program_header_offset;
  uint32_t section_header_offset;
  uint32_t flags;
  uint16_t size;
  uint16_t program_entry_size;
  uint16_t program_entry_count;
  uint16_t section_entry_size;
  uint16_t section_entry_count;
  uint16_t section_name_index;
} __attribute__((packed)) elf_header_t;

typedef struct
{
  uint32_t type;
  uint32_t offset;
  void *vaddr;
  uint32_t _undefined;
  uint32_t file_size;
  uint32_t mem_size;
  uint32_t flags;
  uint32_t align;
} __attribute__((packed)) elf_program_entry_t;

struct vfs_file;

int elf_test(unsigned char *buf, size_t size);
void *elf_load_exe(struct vfs_file *file);

#endif /* __ELF_H__ */
