.code16

.set vga_segment, 0xb800
.set sectors_per_track, 18

/* stage0 loader: this has to fit into 512 bytes */
stage0:
  mov $_stack_base, %sp
  ljmp $0, $1f
1:
  /* local variables:
    0: drive
    2: num sectors remaining
    4: current offset in sectors
    6: num sectors for current read */
  sub $8, %sp
  mov %sp, %si
  xor %ax, %ax
  mov %ax, %es
  mov %ax, %ds
  mov %dx, (%si) /* save drive information */

  /* enable A20 gate */
  mov $0x1, %al
  mov $0x24, %ah
  int $0x15
  jc hang

  /* reset floppy */
  xor %ax, %ax
  mov $0, %dl
  int $0x13

  /* get drive geometry */
  mov $0x800, %ax
  mov (%si), %dx
  xor %di, %di
  mov %di, %es
  int $0x13
  jc hang
  movzx %cl, %ax
  and $0x3f, %ax
  mov %ax, sectors_per_track
  inc %dh
  mov %dh, tracks_per_cylinder

  xor %ax, %ax
  mov %ax, %es
  /* total sectors to read */
  movw $loader_num_sectors, 2(%si)
  /* current offset */
  movw $1, 4(%si)
  /* destination */
  mov $_loader_start, %di

load_loop:
  mov 4(%si), %ax
  add sectors_per_track, %ax
  mov %ax, %cx
  mov sectors_per_track, %bx
  xor %dx, %dx
  div %bx
  sub %dx, %cx /* cx = next sector reached */
  mov 4(%si), %ax /* ax = start sector */
  mov %cx, 4(%si)
  sub %ax, %cx
  mov 2(%si), %dx
  mov %dx, %bx
  sub %cx, %dx
  jge 1f
  mov %bx, %cx
  mov $0, %dx
1:
  mov %dx, 2(%si)
  mov %cx, 6(%si) /* num sectors */
  mov sectors_per_track, %bx
  xor %dx, %dx
  div %bx
  inc %dx
  mov %dx, %cx /* cl = sector */

  xor %dx, %dx
  mov tracks_per_cylinder, %bx
  and $0xf, %bx
  div %bx

  mov %dl, %dh /* dh = head */
  mov %al, %ch
  sal $6, %ah
  or %ah, %cl /* cl[6:7] ch = cyl */
  mov 6(%si), %al /* al = num sectors */
  mov (%si), %dl /* dl = drive */
  mov %di, %bx /* bx = destination */

  mov $2, %ah
  int $0x13
  jc load_loop_end

  /* increment destination */
  mov 6(%si), %ax
  mov $512, %bx
  mul %bx
  add %ax, %di
  mov 2(%si), %ax
  test %ax, %ax
  jz load_loop_end
  jmp load_loop
load_loop_end:

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
  mov $_stack_base, %esp

  movzxw (%si), %edx
  push %edx

  call _stage1

.code16

hang:
  cli
  pusha
  mov $vga_segment, %ax
  mov %ax, %ds
  xor %di, %di
  movw $(0x4000 | ' '), (%di)
1:
  hlt
  jmp 1b

sectors_per_track: .short 18
tracks_per_cylinder: .short 2
stage0_end:
