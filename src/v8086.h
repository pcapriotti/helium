#ifndef V8086_H
#define V8086_H

#define V8086_STACK_BASE 0x2000
#define V8086_HEAP 0x2800

#if !__ASSEMBLER__
typedef struct {
  uint16_t ax, bx, cx, dx;
  uint16_t es, ds, fs, gs;
} regs16_t;

void v8086_enter(uint32_t interrupt, regs16_t *regs);
void v8086_exit(void *stack);
int vme_supported();
#endif

#endif /* V8086_H */
