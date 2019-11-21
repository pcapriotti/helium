#ifndef UTIL_H
#define UTIL_H

#include <math.h>

#define ALIGN_MASK(bits) (~0UL << (bits))
#define ALIGN_BITS(x, bits) ((x) & ALIGN_MASK(bits))
#define ALIGN_UP_BITS(x, bits) ALIGN_BITS((x) + ~ALIGN_MASK(bits), bits)
#define ALIGNED_BITS(x, bits) (((x) & ~ALIGN_MASK(bits)) == 0)

#define DIV(x, m) ((x) / (m))
#define MOD(x, m) ((x) % (m))
#define ALIGN(x, m) ((x) - MOD(x, m))
#define ALIGN_UP(x, m) ALIGN((x) + (m) - 1, (m))
#define ALIGNED(x, m) (MOD(x, m) == 0)
#define DIV_UP(a, b) DIV((a) + (b) - 1, b)

#ifdef _HELIUM
# define DIV64(x, m) div64sd((x), (m))
# define MOD64(x, m) mod64sd((x), (m))
# define ALIGN64(x, m) ((x) - MOD64(x, m))
# define ALIGN64_UP(x, m) ((x) + (m) - 1 - MOD64(x, m))
# define ALIGNED64(x, m) (MOD64(x, m) == 0)
# define DIV64_UP(x, m) DIV64((x) + (m) - 1, (m))
#else
# define DIV64 DIV
# define MOD64 MOD
# define ALIGN64 ALIGN
# define ALIGN64_UP ALIGN_UP
# define ALIGNED64 ALIGNED
# define DIV64_UP DIV_UP
#endif

static inline int max(int x, int y) {
  return (x > y) ? x : y;
}

static inline int min(int x, int y) {
  return (x < y) ? x : y;
}

#endif /* UTIL_H */
