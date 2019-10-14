/* The following function enters virtual-8086 mode with the given 16
bit register configuration. It works by taking a stack parameter,
populating it with values that would be pushed before an interrupt
from v8086 to protected mode, and issuing an iret instruction, which
will jump to the given entry point. */

.globl v8086_enter
v8086_enter:
  /* save registers and flags */
  pushf
  push %ebp
  push %ebx
  push %edi

  /* save stack pointer to tss */
  mov %esp, %eax
  push %eax
  call v8086_save_esp
  add $0x4, %esp

  /* set up registers */
  mov 0x14(%esp), %ecx
  mov 0x0(%ecx), %eax
  mov 0x4(%ecx), %ebx
  mov 0xc(%ecx), %edx
  mov 0x10(%ecx), %edi
  mov 0x14(%ecx), %ebp
  mov 0x8(%ecx), %ecx /* this has to be last */

  /* point stack pointer to v8086 frame */
  add $0x18, %esp

  /* jump to v8086 mode */
  iret

.globl v8086_exit
v8086_exit:
  /* retrieve stack pointer */
  call v8086_restore_esp
  mov %eax, %ebx

  /* save context registers */
  push 0x4(%esp)
  push 0x14(%ebx)
  call v8086_save_ctx

  /* reset stack pointer */
  mov %ebx, %esp

  /* restore registers and flags */
  pop %edi
  pop %ebx
  pop %ebp
  popf

  ret
