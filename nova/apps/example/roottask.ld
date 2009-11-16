OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

/* Without this _start is not linked into the binary, if it is in a
   library! */

EXTERN(_start)
ENTRY(_start)

PHDRS
{
	text PT_LOAD FILEHDR PHDRS;
	data PT_LOAD;
}

SECTIONS
{

  . = 0x00400000 + SIZEOF_HEADERS;

  .text :
  {
    KEEP(*(.text._start))
    *(.text .text.*)
  } : text

  .data :
  {
    *(.bss .bss.*)
    /* Nova does not allow a real BSS for the roottask, so force the
    linker allocate space for the BSS. */
    LONG(1)
    *(.rodata .rodata.*)
    *(.data .data.*) } : data

  /DISCARD/ :
  {
     *(.note.GNU-stack)
     *(.comment)
  }
}
