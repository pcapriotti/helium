#include "core/interrupts.h"
#include "gdt.h"

__attribute__((aligned(8))) gdt_entry_t kernel_gdt[] = {
  { 0, 0, 0, 0, 0, 0 },
  { 0xffff, 0, 0, 0x9a, 0xcf, 0 }, /* code segment */
  { 0xffff, 0, 0, 0x92, 0xcf, 0 }, /* data segment */
  { 0, 0, 0, 0, 0, 0 }, /* placeholder for task descriptor */
};

gdtp_t kernel_gdtp = {
  sizeof(kernel_gdt) - 1,
  kernel_gdt
};

void gdt_set_entry(gdt_entry_t *entry,
                   uint32_t base,
                   uint32_t limit,
                   uint8_t flags,
                   uint8_t granularity)
{
  entry->limit_low = limit & 0xffff;
  entry->granularity = ((granularity << 4) & 0xf0) |
    ((limit >> 16) & 0x0f);
  entry->base_low = base & 0xffff;
  entry->base_mid = (base >> 16) & 0xff;
  entry->base_high = (base >> 24) & 0xff;
  entry->flags = flags;
}

void gdt_init(void)
{
  gdt_set_entry(&kernel_gdt[GDT_TASK],
                (uint32_t)&kernel_tss,
                sizeof(kernel_tss),
                0x89, 0);
  __asm__ volatile
    ("lgdt %0\n"
     "mov %1, %%ds\n"
     "mov %1, %%es\n"
     "mov %1, %%fs\n"
     "mov %1, %%gs\n"
     "mov %1, %%ss\n"
     "ljmp %2, $1f\n"
     "1:\n"
     : :
       "m"(kernel_gdtp),
       "a"(GDT_SEL(GDT_DATA)),
       "i"(GDT_SEL(GDT_CODE)));

  kernel_tss.tss.ss0 = GDT_SEL(GDT_DATA);
  kernel_tss.tss.iomap_base = sizeof(tss_t);
  __asm__ volatile("ltr %0" : : "r"(GDT_SEL(GDT_TASK)));
}
