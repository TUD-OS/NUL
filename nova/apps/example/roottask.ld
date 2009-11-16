OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

/* Without this _start is not linked into the binary, if it is in a
   library! */

EXTERN(_start)
ENTRY(_start)

PHDRS {
      text	PT_LOAD FLAGS(5) FILEHDR PHDRS;
      data 	PT_LOAD FLAGS(6);
}

SECTIONS
{

  . = 0x00400000;
  .text ALIGN(4K):
  {
    KEEP(*(.text._start));
    *(.text .text.* .gnu.linkonce.t.*);
  } : text

  .data ALIGN(4K):
  {
    *(.rodata .rodata.* .gnu.linkonce.r.*)
    *(.data .data.* .gnu.linkonce.d.*)
    /* Nova does not allow a real BSS. */
    KEEP(*(.bss .bss.* .gnu.linkonce.b.*))
    KEEP(*(.stack))
  } : data

  /DISCARD/ :
  {
     *(.note.GNU-stack)
     *(.comment)
  }
}
