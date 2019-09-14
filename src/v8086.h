#ifndef V8086_H
#define V8086_H

#define V8086_STACK_BASE 0x2000

#if !__ASSEMBLER__
void v8086_enter();
void v8086_exit();
int vme_supported();
#endif

#endif /* V8086_H */
