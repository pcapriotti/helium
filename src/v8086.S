.section .text1
.code32

#include "v8086.h"

.globl v8086_enter
v8086_enter: /* (interrupt, regs) */
  /* save clobbered */
  push %ebx
  push %ebp

  lea 8(%esp), %ebp

  /* set tss esp and eip */
  mov %esp, kernel_tss + 4
  mov (%ebp), %eax
  mov %eax, kernel_tss + 32

  /* push segment registers */
  xor %eax, %eax
  mov 0x10(%ebp), %ax
  push %eax
  mov 0x12(%ebp), %ax
  push %eax
  mov 0x14(%ebp), %ax
  push %eax
  mov 0x16(%ebp), %ax
  push %eax

  /* get interrupt vector */
  mov 0x4(%ebp), %eax
  xor %ecx, %ecx
  mov (%ecx,%eax,4), %eax

  /* push sp, flags and ip */
  push $0
  push $V8086_STACK_BASE
  pushf
  orl $(1 << 17), (%esp)
  mov %eax, %ecx
  sar $16, %eax
  push %eax
  push %ecx

  movw 0x8(%ebp), %ax
  movw 0xc(%ebp), %cx
  movw 0xe(%ebp), %dx
  movw 0xa(%ebp), %bx
  iret

.globl v8086_exit
v8086_exit:
  /* restore stack */
  mov kernel_tss + 4, %esp

  /* restore registers */
  pop %ebp
  pop %ebx

  /* return from v8086 */
  mov kernel_tss + 32, %eax
  jmp *%eax
