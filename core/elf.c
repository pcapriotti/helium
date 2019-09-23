#include "elf.h"
#include "vfs.h"

#define DEBUG_ELF 1

#define ELF_MAGIC 0x464c457f

#if DEBUG_ELF
# include "debug.h"
#endif

void *elf_load_exe(vfs_file_t *file)
{
  elf_header_t header;
  vfs_read(file, &header, sizeof(elf_header_t));

  if (header.magic != ELF_MAGIC) return 0; /* not an ELF file */
  if (header.arch != ELF_ARCH_X86) return 0; /* wrong arch */
  if (!header.program_header_offset) return 0; /* no program header */

  elf_program_entry_t entry;
  vfs_move(file, header.program_header_offset);
  for (int i = 0; i < header.program_entry_count; i++) {
    vfs_read(file, &entry, sizeof(elf_program_entry_t));
    kprintf("loading from offset %p to address %p\n",
            entry.offset, entry.vaddr);
    vfs_move_rel(file, header.program_entry_size - sizeof(elf_program_entry_t));
  }

  return 0;
}

int elf_test(unsigned char *buf, size_t size)
{
  elf_header_t *header = (elf_header_t*)buf;
  if (size < sizeof(elf_header_t)) {
#if DEBUG_ELF
    kprintf("ELF header does not fit in the given buffer\n");
#endif
    return -1;
  }
  if (header->magic != ELF_MAGIC) {
#if DEBUG_ELF
    kprintf("Not an ELF file\n");
#endif
    return -1;
  } else if (header->arch != ELF_ARCH_X86) {
#if DEBUG_ELF
    kprintf("Only x86 binaries are supported\n");
#endif
    return -1;
  }

#if DEBUG_ELF
  kprintf("header size: %#x\n", sizeof(elf_header_t));
  kprintf("program entry size: %#x\n", sizeof(elf_program_entry_t));

#define DEBUG(x) kprintf(#x ": %x\n", header->x);

  DEBUG(magic);
  DEBUG(bits);
  DEBUG(endianness);
  DEBUG(version);
  DEBUG(os_abi);
  DEBUG(type);
  DEBUG(arch);
  DEBUG(elf_version);
  DEBUG(entry);
  DEBUG(program_header_offset);
  DEBUG(section_header_offset);
  DEBUG(flags);
  DEBUG(size);
  DEBUG(program_entry_size);
  DEBUG(program_entry_count);
  DEBUG(section_entry_size);
  DEBUG(section_entry_count);
  DEBUG(section_name_index);

#undef DEBUG
#endif

  elf_program_entry_t *table
    = (elf_program_entry_t*)(buf + header->program_header_offset);
  if ((uint8_t *)(table + header->program_entry_count) - buf > (int) size) {
    kprintf("program headers do not fit in the given buffer\n");
    return -1;
  }
  (void)table;
  for (int i = 0; i < header->program_entry_count; i++) {
#if DEBUG_ELF
    kprintf("program entry %u\n", i);

#define DEBUG(x) kprintf(#x ": %#x\n", table[i].x);
    DEBUG(type);
    DEBUG(offset);
    DEBUG(vaddr);
    DEBUG(_undefined);
    DEBUG(file_size);
    DEBUG(mem_size);
    DEBUG(flags);
    DEBUG(align);
#undef DEBUG
#endif
  }

  return 0;
}
