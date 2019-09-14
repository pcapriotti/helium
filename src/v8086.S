.section .text1
.code32

#include "v8086.h"

.globl v8086_enter
v8086_enter: /* (interrupt, ax, bx, cx, dx) */
  /* save clobbered registers */
  push %ebx

  /* set tss esp and eip */
  mov %esp, kernel_tss + 4
  mov 4(%esp), %eax
  mov %eax, kernel_tss + 32

  /* get interrupt vector */
  mov 0x8(%esp), %eax
  xor %ecx, %ecx
  mov (%ecx,%eax,4), %eax

  /* prepare for iret */
  push $0
  push $V8086_STACK_BASE
  pushf
  orl $(1 << 17), (%esp)
  mov %eax, %ecx
  sar $16, %eax
  push %eax
  push %ecx

  mov 0x20(%esp), %ax
  mov 0x22(%esp), %bx
  mov 0x24(%esp), %dx
  mov 0x26(%esp), %cx
  iret

.globl v8086_exit
v8086_exit:
  /* restore stack */
  mov kernel_tss + 4, %esp

  /* restore registers */
  pop %ebx

  /* return from v8086 */
  mov kernel_tss + 32, %eax
  jmp *%eax
