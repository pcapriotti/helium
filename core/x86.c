#include "x86.h"

enum {
  CPUID_VENDOR = 0,
  CPUID_GETFEATURES = 1,
};

int cpuid_is_supported(void)
{
  uint32_t old, new;
  __asm__
    ("pushf\n"
     "mov (%%esp), %1\n"
     "pushf\n"
     "xorl %2, (%%esp)\n"
     "popf\n"
     "pushf\n"
     "pop %0\n"
     "popf\n"
     : "=r"(old), "=r"(new)
     : "i"(EFLAGS_ID));

  return (old ^ new) & EFLAGS_ID;
}

void cpuid_vendor(char *vendor)
{
  uint32_t *ret = (uint32_t *) vendor;
  uint32_t highest;
  __asm__
    ("cpuid"
     : "=b"((uint32_t) ret[0]),
       "=d"((uint32_t) ret[1]),
       "=c"((uint32_t) ret[2]),
       "=a"(highest)
     : "a"(CPUID_VENDOR));

  vendor[12] = '\0';
}

uint32_t cpuid_features(void)
{
  uint32_t eax, ret;

  __asm__
    ("cpuid\n"
     : "=d"(ret),
       "=a"(eax)
     : "a"(CPUID_GETFEATURES)
     : "ebx", "ecx");
  return ret;
}

int cpuid_check_features(uint32_t mask)
{
  if (!cpuid_is_supported()) return 0;
  return (cpuid_features() & mask) == mask;
}

uint32_t cpu_flags()
{
  uint32_t flags;
  __asm__
    ("pushf\n"
     "pop %0\n"
     : "=r"(flags));
  return flags;
}
