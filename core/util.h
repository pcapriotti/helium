#ifndef UTIL_H
#define UTIL_H

#define ALIGN_MASK(bits) (~0UL << (bits))
#define ALIGN_BITS(x, bits) ((x) & ALIGN_MASK(bits))
#define ALIGN_UP_BITS(x, bits) ALIGN_BITS((x) + ~ALIGN_MASK(bits), bits)
#define ALIGNED_BITS(x, bits) (((x) & ~ALIGN_MASK(bits)) == 0)

#define ALIGN(x, m) ((x) - ((x) % m))
#define ALIGN_UP(x, m) ((x) + (m) - 1 - ((x) % m))
#define IS_ALIGNED(x, m) (((x) % (m)) == 0)
#define DIV_UP(a, b) (((a) + (b) - 1) / (b))

static inline int max(int x, int y) {
  return (x > y) ? x : y;
}

static inline int min(int x, int y) {
  return (x < y) ? x : y;
}

#endif /* UTIL_H */
