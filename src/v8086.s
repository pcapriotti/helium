.section .text1

/* virtual 8086 mode */
.code16
v8086_entry:
  /* int $0x13 */
  xor %ax, %ax
  mov %ax, %ds

  /* prepare for iret */
  mov %sp, %ax
  push %ss
  push %ax
  pushf
  push %cs
  push $1f

  mov $(0x13 * 4), %si
  ljmp *(%si)
1:
  mov $0xbadb12e8, %ecx
2:
  jmp 2b

.code32

.globl vme_supported
vme_supported:
  mov $1, %eax
  cpuid
  sar $1, %edx
  and $1, %edx
  mov %edx, %eax
  ret

.globl enter_v8086_mode
enter_v8086_mode:
  /* iopl = 3 */
  pushf
  orl $(0x3 << 12), (%esp)
  popf

  /* prepare for iret */
  push $0
  lea -0x40(%esp), %eax
  push %eax
  pushf
  orl $(1 << 17), (%esp)
  push $0
  push $v8086_entry
  iret
