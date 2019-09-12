.code16
.globl graphics_init
.set vbe_mode, 0x0101
.section .text

graphics_init:
  push %ds
  push %es

  /* get vbe info */
  xor %ax, %ax
  mov %ax, %es
  mov $vbe_info, %di
  mov $0x4f00, %ax
  int $0x10

  cmp $0x004f, %al
  jne graphics_init_error

  /* get mode info */
  mov $mode_info, %di
  mov $0x4f01, %ax
  mov $vbe_mode, %cx
  int $0x10

  cmp $0x004f, %al
  jne graphics_init_error

  /* set vbe mode */
  mov $0x4f02, %ax
  mov $(0x4000 | vbe_mode), %bx
  int $0x10
  cmp $0x004f, %al
  jne graphics_init_error

  /* get font bitmaps */
  mov $0x1130, %ax
  mov $6, %bh
  int $0x10

  /* copy bitmaps */
  mov %es, %ax
  mov %ax, %ds
  xor %ax, %ax
  mov %ax, %es
  mov %bp, %si
  mov $vga_font, %di
  mov $(256 * 4), %cx
  rep movsd

  mov $0, %ax
  jmp graphics_init_done

graphics_init_error:
  mov $1, %ax
graphics_init_done:
  pop %es
  pop %ds
  ret

.section .data
vbe_info:
    signature: .ascii "VBE2"
	version: .short 0
    oem: .long 0
    caps: .long 0
    modes: .long 0
    mem: .short 0 /* mem in 64K blocks */
    sw_rev: .short 0
    vendor: .long 0
    product: .long 0
    prod_rev: .long 0
    .skip 222
    oem_data: .skip 256

.section .bss
vga_font:
    .skip 4096

mode_info:
  attributes: .short 0
  .skip 14
  vesa_pitch: .short 0
  vesa_width: .short 0
  vesa_height: .short 0
  .skip 3
  vesa_bpp: .byte 0
  num_banks: .byte 0
  mem_model: .byte 0
  bank_size: .byte 0
  .skip 2
  red_info: .short 0
  green_info: .short 0
  blue_info: .short 0
  .skip 3
  vesa_framebuffer: .long 0
  .skip 212

.globl vesa_framebuffer
.globl vesa_pitch
.globl vesa_width
.globl vesa_height
