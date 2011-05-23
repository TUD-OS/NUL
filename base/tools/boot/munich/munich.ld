SECTIONS
{
  ENTRY(__start)
  . = 0x18000;

  .text :
  {
    KEEP(*(.text.__mbheader));
    KEEP(*(.text.__start));
    *(.text .text.*);
    *(.rodata .rodata.*);
  }

  .bss :
  {
     *(.bss);
  }


  .debug :
  {
     *(.debug*);
  }


  /DISCARD/ :
  {
    *(.comment)
  }
}
