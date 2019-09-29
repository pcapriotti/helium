#ifndef ATOMIC_H
#define ATOMIC_H

#define barrier() __asm__ volatile("" : : : "memory")
#define sti() __asm__ volatile("sti" : : : "memory")
#define cli() __asm__ volatile("cli" : : : "memory")

#endif /* ATOMIC_H */
