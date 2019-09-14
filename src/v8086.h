#ifndef V8086_H
#define V8086_H

#define V8086_STACK_BASE 0x2000

#if !__ASSEMBLER__
typedef struct {
  uint16_t ax, bx, cx, dx;
  uint16_t gs, fs, es, ds;
} regs16_t;

void v8086_enter(uint32_t interrupt, regs16_t regs);
void v8086_exit();
int vme_supported();
#endif

#endif /* V8086_H */
