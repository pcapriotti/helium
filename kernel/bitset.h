#ifndef BITSET_H
#define BITSET_H

/* operations on bitvectors defined as arrays of uint32_t */
#define BIT_WORD(v, index) ((v)[(index) >> 5])
#define GET_BIT(v, index) \
  (BIT_WORD(v, index) & (1UL << ((index) & 0x1f)))
#define SET_BIT(v, index) \
  (BIT_WORD(v, index) |= (1UL << ((index) & 0x1f)))
#define UNSET_BIT(v, index) \
  (BIT_WORD(v, index) &= ~(1UL << ((index) & 0x1f)))
#define FLIP_BIT(v, index) \
  (BIT_WORD(v, index) ^= (1UL << ((index) & 0x1f)))

#endif /* BITSET_H */
