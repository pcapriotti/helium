.section .text1
.code32

.set v8086_stack_base, 0x2000

/* virtual 8086 mode */
.code16
v8086_entry:

  /* int $0x13 */
  xor %ax, %ax
  mov %ax, %ds
  mov %ax, %es

  /* prepare for iret */
  mov %sp, %ax
  push %ss
  push %ax
  pushf
  push %cs
  push $1f

  /* reset floppy */
  xor %ax, %ax
  mov $0, %dl

  mov $(0x13 * 4), %si
  ljmp *(%si)
1:
  int $0xd

.code32

.globl vme_supported
vme_supported:
  mov $1, %eax
  cpuid
  sar $1, %edx
  and $1, %edx
  mov %edx, %eax
  ret

.globl v8086_enter
v8086_enter: /* (ptr tss.esp0, ptr tss.eip) */
  /* set tss esp */
  mov 4(%esp), %eax
  mov %esp, (%eax)

  mov 8(%esp), %eax
  mov (%esp), %ebx
  movl %ebx, (%eax)

  /* iopl = 3 */
  pushf
  orl $(0x3 << 12), (%esp)
  popf

  /* prepare for iret */
  push $0
  push $v8086_stack_base
  pushf
  orl $(1 << 17), (%esp)
  push $0
  push $v8086_entry
  iret

.globl v8086_exit
v8086_exit: /* (tss.eip) */
  mov 4(%esp), %eax
  jmp *%eax

.globl v8086_restore_segments
v8086_restore_segments:
  mov $0x10, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs
  ret
