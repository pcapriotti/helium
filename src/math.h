#ifndef MATH_H
#define MATH_H

/* unsigned 64bit dividend, divisor d is such that d + 1 fits in 16 bits */
static inline uint64_t div64sd(uint64_t a, uint64_t b)
{
  /* Let N = 2^32. Write a = a1 N + a0 with a0, a1: uint32_t.
     Now divide everything by b:
     a1 = b d1 + r1
     a0 = b d0 + r0
     N - 1 = b u + v

     therefore:

     a1 N + a0 = (b d1 + r1) (b u + v + 1) + b d0 + r0
               = b (b d1 u + d1 (v + 1) + r1 u + d0) + r1 (v + 1) + r0

     thanks to the smallness of b, we know that the remainder is
     within 32 bits, therefore we can divide again:

     r1 (v + 1) + r0 = b q + s

     and the final result is

     b d1 u + d1 (v + 1) + r1 u + d0 + q
   */

  uint32_t b0 = b;

  uint32_t N1 = (uint32_t) ((1ULL << 32) - 1);
  uint32_t a1 = a >> 32;
  uint32_t a0 = a;

  uint64_t d1 = a1 / b0;
  uint64_t d0 = a0 / b0;
  uint64_t r1 = a1 % b0;
  uint64_t r0 = a0 % b0;
  uint64_t u = N1 / b0;
  uint64_t v1 = N1 % b0 + 1;
  uint64_t q = (uint32_t)(r1 * v1 + r0) / b0;

  return b * d1 * u + d1 * v1 + r1 * u + d0 + q;
}

static inline uint64_t mod64sd(uint64_t a, uint64_t b)
{
  return a - b * div64sd(a, b);
}

#endif /* MATH_H */
