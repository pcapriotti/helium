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

  /* enable protected mode */
  cli
  lgdt gdtp
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

.p2align 3
/* prefilled gdt */
gdt:
  /* null descriptor */
  .short 0, 0
  .byte 0, 0, 0, 0

  /* code descriptor */
  .short 0xffff /* limit low */
  .short 0      /* base low */
  .byte 0       /* base middle */
  .byte 0x9a    /* code segment, ring 0 */
  .byte 0xcf    /* limit middle */
  .byte 0       /* base high */

  /* data descriptor */
  .short 0xffff
  .short 0
  .byte 0
  .byte 0x92    /* data segment, ring 0 */
  .byte 0xcf
  .byte 0
gdtp:
  .short 8 * 3 - 1
  .long gdt

_stage0_end:
