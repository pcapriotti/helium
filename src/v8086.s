.section .text1
.code32

.set v8086_stack_base, 0x2000

/* virtual 8086 mode */
.code16
v8086_entry:
  xor %ax, %ax
  mov %ax, %ds
  mov %ax, %es

  /* prepare for iret */
  pushf
  push %cs
  push $1f

  /* copy first sector into a buffer */
  mov $0x0201, %ax
  mov $0, %dx
  mov $1, %cx
  mov $0x2100, %bx

  mov $(0x13 * 4), %si
  ljmp *(%si)
1:
  int $0xd

.code32

.globl v8086_enter
v8086_enter:
  /* set tss esp and eip */
  mov %esp, kernel_tss + 4
  mov (%esp), %eax
  mov %eax, kernel_tss + 32

  /* prepare for iret */
  push $0
  push $v8086_stack_base
  pushf
  orl $(1 << 17), (%esp)
  push $0
  push $v8086_entry
  iret

.globl v8086_exit
v8086_exit:
  mov kernel_tss + 4, %esp
  mov kernel_tss + 32, %eax
  jmp *%eax
