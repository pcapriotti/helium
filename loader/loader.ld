SECTIONS
{

  _stack_base = 0x7b00;
  . = 0x7c00;
  _loader0_start = .;
  .text0 :
  {
    boot.o
    . = 0x1fe;
    SHORT(0xaa55)
  }
  _loader0_end = .;
  _loader_start = .;
  .text :
  {
    *(.text*)
  }

  .data : { *(.data*) *(.rodata*) }
  _loader_end = .;

  ASSERT(_loader_end - _loader0_start < 512 * 72,
    "Loader should be smaller than 72 sectors (36 kB)")

  _bss_start = .;
  .bss : { *(.bss*) }
  _bss_end = .;

  kernel_gdtp_segment = kernel_gdtp >> 16;
  kernel_gdtp_offset = kernel_gdtp & 0xffff;
  loader_num_sectors = (_loader_end - _loader_start + 511) / 512;

  _kernel_start = 0x100000;
}
