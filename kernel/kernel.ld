SECTIONS
{
  . = 0x100000;
  .text :
  {
    _kernel_start = .;
    *(.text*)
  }

  .data : { *(.data*) *(.rodata*) }

  _bss_start = .;
  .bss : { *(.bss) }
  _bss_end = .;

  _kernel_end = .;
}
