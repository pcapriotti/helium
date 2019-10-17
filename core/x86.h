#ifndef X86_H
#define X86_H

#include <stdint.h>

#define CR_SET(cr, value) \
  __asm__ volatile \
    ("mov %0, %%cr" #cr : : "r"(value))

#define CR_GET(cr) \
  ({ uint32_t value; \
    __asm__ volatile ("mov %%cr" #cr ", %0" : "=r"(value)); \
    value;})

enum {
  CR4_PSE = 1 << 4,
  CR4_PAE = 1 << 5,
};

enum {
  CR0_PG = 1 << 31,
};

enum {
  EFLAGS_CF = 1 << 0,
  EFLAGS_IF = 1 << 9,
  EFLAGS_VM = 1 << 17,
  EFLAGS_ID = 1 << 21,
};

static inline void paging_enable(void) {
  uint32_t value = CR_GET(0);
  CR_SET(0, value | CR0_PG);
}

static inline int paging_disable(void) {
  uint32_t value = CR_GET(0);
  int enabled = value & CR0_PG;
  if (enabled) CR_SET(0, value & ~CR0_PG);
  return enabled;
}

enum {
  CPUID_EFEAT_SSE3 = 1 << 0,
  CPUID_EFEAT_PCLMUL = 1 << 1,
  CPUID_EFEAT_DTES64 = 1 << 2,
  CPUID_EFEAT_MONITOR = 1 << 3,
  CPUID_EFEAT_DS_CPL = 1 << 4,
  CPUID_EFEAT_VMX = 1 << 5,
  CPUID_EFEAT_SMX = 1 << 6,
  CPUID_EFEAT_EST = 1 << 7,
  CPUID_EFEAT_TM2 = 1 << 8,
  CPUID_EFEAT_SSSE3 = 1 << 9,
  CPUID_EFEAT_CID = 1 << 10,
  CPUID_EFEAT_FMA = 1 << 12,
  CPUID_EFEAT_CX16 = 1 << 13,
  CPUID_EFEAT_ETPRD = 1 << 14,
  CPUID_EFEAT_PDCM = 1 << 15,
  CPUID_EFEAT_PCIDE = 1 << 17,
  CPUID_EFEAT_DCA = 1 << 18,
  CPUID_EFEAT_SSE4_1 = 1 << 19,
  CPUID_EFEAT_SSE4_2 = 1 << 20,
  CPUID_EFEAT_x2APIC = 1 << 21,
  CPUID_EFEAT_MOVBE = 1 << 22,
  CPUID_EFEAT_POPCNT = 1 << 23,
  CPUID_EFEAT_AES = 1 << 25,
  CPUID_EFEAT_XSAVE = 1 << 26,
  CPUID_EFEAT_OSXSAVE = 1 << 27,
  CPUID_EFEAT_AVX = 1 << 28,
};

enum {
  CPUID_FEAT_FPU = 1 << 0,
  CPUID_FEAT_VME = 1 << 1,
  CPUID_FEAT_DE = 1 << 2,
  CPUID_FEAT_PSE = 1 << 3,
  CPUID_FEAT_TSC = 1 << 4,
  CPUID_FEAT_MSR = 1 << 5,
  CPUID_FEAT_PAE = 1 << 6,
  CPUID_FEAT_MCE = 1 << 7,
  CPUID_FEAT_CX8 = 1 << 8,
  CPUID_FEAT_APIC = 1 << 9,
  CPUID_FEAT_SEP = 1 << 11,
  CPUID_FEAT_MTRR = 1 << 12,
  CPUID_FEAT_PGE = 1 << 13,
  CPUID_FEAT_MCA = 1 << 14,
  CPUID_FEAT_CMOV = 1 << 15,
  CPUID_FEAT_PAT = 1 << 16,
  CPUID_FEAT_PSE36 = 1 << 17,
  CPUID_FEAT_PSN = 1 << 18,
  CPUID_FEAT_CLF = 1 << 19,
  CPUID_FEAT_DTES = 1 << 21,
  CPUID_FEAT_ACPI = 1 << 22,
  CPUID_FEAT_MMX = 1 << 23,
  CPUID_FEAT_FXSR = 1 << 24,
  CPUID_FEAT_SSE = 1 << 25,
  CPUID_FEAT_SSE2 = 1 << 26,
  CPUID_FEAT_SS = 1 << 27,
  CPUID_FEAT_HTT = 1 << 28,
  CPUID_FEAT_TM1 = 1 << 29,
  CPUID_FEAT_IA64 = 1 << 30,
  CPUID_FEAT_PBE = 1 << 31
};

int cpuid_is_supported(void);
void cpuid_vendor(char *vendor);
uint32_t cpuid_features(void);
int cpuid_check_features(uint32_t mask);

uint32_t cpu_flags();

#endif /* X86_H */
