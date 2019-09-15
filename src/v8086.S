.section .text1
.code32

#include "v8086.h"

.globl v8086_enter
v8086_enter: /* (interrupt, ptr regs) */
  /* save clobbered */
  push %ebx
  push %ebp
  push %esi
  push %edi

  lea 0x10(%esp), %ebp
  mov 8(%ebp), %ebx

  /* set tss esp */
  mov %esp, kernel_tss + 4

  /* push segment registers */
  xor %eax, %eax
  mov 0x14(%ebx), %ax
  push %eax
  mov 0x12(%ebx), %ax
  push %eax
  mov 0x10(%ebx), %ax
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
  movw 0x8(%ebx), %si
  movw 0xa(%ebx), %di
  movw 0xc(%ebx), %bp
  movw 0x2(%ebx), %bx /* set bx last */
  iret

.globl v8086_exit
v8086_exit:
  /* save pointer to context */
  mov 4(%esp), %edx

  /* restore stack */
  mov kernel_tss + 4, %esp

  /* copy final state of registers */
  mov 0x18(%esp), %eax
  mov 0x1c(%edx), %ecx
  mov %cx, (%eax) /* ax */
  mov 0x10(%edx), %ecx
  mov %cx, 0x2(%eax) /* bx */
  mov 0x18(%edx), %ecx
  mov %cx, 0x4(%eax) /* cx */
  mov 0x14(%edx), %ecx
  mov %cx, 0x6(%eax) /* dx */
  mov 0x04(%edx), %ecx
  mov %cx, 0x8(%eax) /* si */
  mov 0x00(%edx), %ecx
  mov %cx, 0xa(%eax) /* di */
  mov 0x08(%edx), %ecx
  mov %cx, 0xc(%eax) /* bp */
  mov 0x3c(%edx), %ecx
  mov %cx, 0xe(%eax) /* es */
  mov 0x40(%edx), %ecx
  mov %cx, 0x10(%eax) /* ds */
  mov 0x44(%edx), %ecx
  mov %cx, 0x12(%eax) /* fs */
  mov 0x48(%edx), %ecx
  mov %cx, 0x14(%eax) /* gs */

  /* restore registers */
  pop %edi
  pop %esi
  pop %ebp
  pop %ebx

  /* return flags */
  mov 0x30(%edx), %eax
  ret
