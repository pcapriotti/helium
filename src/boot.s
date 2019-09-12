.code16

.set vga_segment, 0xb800
.set stack_base, 0x7b00

/* stage0 loader: this has to fit into 512 bytes */

_stage0:
  mov $stack_base, %sp
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

  mov $_stage1_end, %ax
  sub $0x7c01, %ax
  xor %dx, %dx
  mov $0x200, %bx
  div %bx /* ax = num sectors to read */

  mov $2, %ah
  mov $2, %cx
  pop %dx
  mov $0, %dh
  mov $0x7e00, %bx
  int $0x13
  jc _stage0

  call graphics_init

  /* enable protected mode */
  cli
  lgdt kernel_gdtp
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
  mov $stack_base, %esp

  jmp _stage1

hang:
  hlt
1:
  jmp 1b

_stage0_end:
