#ifndef UTIL_H
#define UTIL_H

#define ALIGN_MASK(bits) (~0UL << (bits))
#define ALIGNED(x, bits) (((uint32_t) (x)) & ALIGN_MASK(bits))
#define ALIGNED_UP(x, bits) ALIGNED((((uint32_t) (x)) + ~ALIGN_MASK(bits)), bits)
#define IS_ALIGNED(x, bits) ((((uint32_t) (x)) & ~ALIGN_MASK(bits)) == 0)

#define ALIGNED64(x, bits) (((uint64_t) (x)) & (~0ULL << (bits)))

#define ROUND(a, i) (((uint32_t)a + (1 << (i)) - 1) >> i)
#define ROUND64(a, i) (((uint64_t)a + (1 << (i)) - 1) >> i)

static inline int max(int x, int y) {
  return (x > y) ? x : y;
}

#endif /* UTIL_H */
