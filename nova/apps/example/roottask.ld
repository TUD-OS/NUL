OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

/* Without this _start is not linked into the binary, if it is in a
library! */
EXTERN(_start)

PHDRS {
      text PT_LOAD ;
      gcdata   PT_LOAD ;
      userdata PT_LOAD ;
}

SECTIONS
{
  .  = 4K;			/* Skip first page to trap NULL pointer dereference */
  PROVIDE (_utcb_start = .);
  /* XXX This is defined in nova/memory.h */
  . += 1024*4096;
  PROVIDE (_utcb_end = .);

  /* Leave space after UTCBs to catch stupid mistakes. */
  . += 4K;

  /* VGA memory */
  PROVIDE (_vga_start = .);
  . += 4K;

  PROVIDE (_img_begin = .);

  .gcdata ALIGN(4K):
  {
    KEEP(*(.gcdata));
    LONG(1);
  } : gcdata

  .text ALIGN(4K):
  {
    KEEP(*(.text._start));
    *(.text .text.* .gnu.linkonce.t.*);
  } : text

  .data ALIGN(4K):
  {
    PROVIDE (_data_begin = .);
    PROVIDE (_ctors_start = .);
    KEEP (*(.ctors))
    KEEP (*(SORT(.ctors.*)))
    PROVIDE (_ctors_end = .);
    *(.rodata .rodata.* .gnu.linkonce.r.*)

    *(.data .data.* .gnu.linkonce.d.*)
    /* Nova does not allow a real BSS. */
    KEEP(*(.bss .bss.* .gnu.linkonce.b.*))

    . = ALIGN(4K);
    PROVIDE(_stack_top = .);
    KEEP(*(.mainstack))		/* Stack of initial thread */
    PROVIDE(_stack_bottom = .);
    PROVIDE (_img_end = .);
  } : userdata

  . = 0x10000000; /* ALIGN(4K); */
  PROVIDE (_heap_start = .);
  . = 0xC0000000;		/* ~3GB */
  PROVIDE (_heap_end = .);
  
  /DISCARD/ :
  {
     *(.note.GNU-stack)
     *(.comment)
  }
}
