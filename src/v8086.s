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
  mov %sp, %ax
  push %ss
  push %ax
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

/* execute a BIOS interrupt */
.globl v8086_int
v8086_int: /* (cs, irq) */
  xor %si, %si
  mov %si, %ds

  /* prepare for iret */
  mov %sp, %si
  push %ss
  push %si
  pushf
  push (%si)
  push 4(%si)

  mov 8(%si), %ax
  xor %dx, %dx
  mov $4, %si
  mul %si
  mov %ax, %si

  ljmp *(%si)

.code32

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
v8086_exit: /* (tss.esp0, tss.eip) */
  mov 8(%esp), %eax
  mov 4(%esp), %esp

  jmp *%eax

.globl v8086_restore_segments
v8086_restore_segments:
  mov $0x10, %ax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs
  ret
