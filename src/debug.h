#ifndef DEBUG_H
#define DEBUG_H

#define DEBUG_EAX(x) __asm__ volatile("" : : "a"(x))
#define DEBUG_REGS(a, b, c, d) __asm__ volatile("" : : "a"(a), "b"(b), "c"(c), "d"(d))

void panic(void);

#endif /* DEBUG_H */
