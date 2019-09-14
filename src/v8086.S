.section .text1
.code32

#include "v8086.h"

.globl v8086_enter
v8086_enter: /* (interrupt, ptr regs) */
  /* save clobbered */
  push %ebx
  push %ebp

  lea 8(%esp), %ebp
  mov 8(%ebp), %ebx

  /* set tss esp */
  mov %esp, kernel_tss + 4

  /* push segment registers */
  xor %eax, %eax
  mov 0x8(%ebx), %ax
  push %eax
  mov 0xa(%ebx), %ax
  push %eax
  mov 0xc(%ebx), %ax
  push %eax
  mov 0xe(%ebx), %ax
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

  movw 0x0(%ebx), %ax
  movw 0x4(%ebx), %cx
  movw 0x6(%ebx), %dx
  movw 0x2(%ebx), %bx
  iret

.globl v8086_exit
v8086_exit:
  /* save pointer to context */
  mov 4(%esp), %edx

  /* restore stack */
  mov kernel_tss + 4, %esp

  /* copy final state of registers */
  mov 0x10(%esp), %eax
  mov 0x2c(%edx), %ecx
  mov %cx, (%eax) /* ax */
  mov 0x20(%edx), %ecx
  mov %cx, 0x2(%eax) /* bx */
  mov 0x28(%edx), %ecx
  mov %cx, 0x4(%eax) /* cx */
  mov 0x24(%edx), %ecx
  mov %cx, 0x6(%eax) /* dx */
  mov (%edx), %ecx
  mov %cx, 0x8(%eax) /* gs */
  mov 0x4(%edx), %ecx
  mov %cx, 0xa(%eax) /* fs */
  mov 0x8(%edx), %ecx
  mov %cx, 0xc(%eax) /* es */
  mov 0xc(%edx), %ecx
  mov %cx, 0xe(%eax) /* ds */

  /* restore registers */
  pop %ebp
  pop %ebx
  ret
