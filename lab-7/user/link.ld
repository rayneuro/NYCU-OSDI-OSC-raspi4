ENTRY(_start)
SECTIONS
{
    . = 0;
    .text : { KEEP(*(.text.entry)) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    /* Flat binaries have no BSS metadata: materialize it in the image. */
    .bss : { *(.bss .bss.*) *(COMMON) }
    .image_end : { BYTE(0) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
