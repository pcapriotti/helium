.code16

.set vga_segment, 0xb800

/* stage0 loader: this has to fit into 512 bytes */

_stage0:
  mov $_stack_base, %sp
  ljmp $0, $1f
1:
  push %dx /* save drive information */

  /* enable A20 gate */
  mov $0x1, %al
  mov $0x24, %ah
  int $0x15
  jc _stage0

  /* load stage1 */
  xor %ax, %ax
  mov %ax, %es

  xor %ax, %ax
  mov $0, %dl
  int $0x13

  mov $_loader_end, %eax
  sub $0x7c01, %eax
  xor %edx, %edx
  mov $0x200, %ebx
  div %ebx /* ax = num sectors to read */

  mov $2, %ah
  mov $2, %cx
  mov (%esp), %dx
  mov $0, %dh
  mov $0x7e00, %bx
  int $0x13
  jc _stage0

  /* enable protected mode */
  cli
  mov $kernel_gdtp_segment, %ax
  mov %ax, %ds
  lgdt kernel_gdtp_offset
  mov %cr0, %eax
  or $1, %eax
  mov %eax, %cr0
  ljmp $0x8, $1f
1:

.code32

  mov $0x10, %eax
  mov %ax, %ds
  mov %ax, %es
  mov %ax, %fs
  mov %ax, %gs
  mov %ax, %ss

  xor %ebp, %ebp
  xor %edx, %edx
  pop %dx
  mov $_stack_base, %esp

  push %edx
  call _stage1

.globl hang_system
hang_system:
  cli
  hlt
1:
  jmp 1b

_stage0_end:
