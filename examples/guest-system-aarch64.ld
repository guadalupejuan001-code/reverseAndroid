ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); }
SECTIONS {
  . = 0x40080000;
  .text : { *(.text*) *(.rodata*) } :text
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
