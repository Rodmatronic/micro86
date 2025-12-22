ENTRY(_start)

SECTIONS
{
  . = 0x15000;

  .text : {
      *(.text .text.*)
      *(.init)      /* <- pull init here */
      *(.fini)
  }

  .rodata : {
      *(.rodata .rodata.*)
  }

  .data : {
      *(.data .data.*)
  }

  .bss : {
      *(.bss .bss.*)
      *(COMMON)
  }
}
