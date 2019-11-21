#ifndef UTIL_H
#define UTIL_H

#define UINT32(x) (x)

#define ALIGN_MASK(bits) (~0UL << (bits))
#define ALIGNED(x, bits) (UINT32(x) & ALIGN_MASK(bits))
#define ALIGNED_UP(x, bits) ALIGNED(UINT32(x) + ~ALIGN_MASK(bits), bits)
#define IS_ALIGNED(x, bits) ((UINT32(x) & ~ALIGN_MASK(bits)) == 0)

#define ALIGNED64(x, bits) (((uint64_t) (x)) & (~0ULL << (bits)))

#define DIV_UP(a, b) (((a) + (b) - 1) / (b))

static inline int max(int x, int y) {
  return (x > y) ? x : y;
}

static inline int min(int x, int y) {
  return (x < y) ? x : y;
}

#endif /* UTIL_H */
