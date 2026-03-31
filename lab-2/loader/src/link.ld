SECTIONS
{
    . = 0x80000;     /* Kernel load address for AArch64 */

    .text : { KEEP(*(.text.boot)) *(.text .text.* .gnu.linkonce.t*) }
    /* Loader 主體搬到高位址 */
    //. = 0x200000;
    PROVIDE(_loader_start = .);
    
    .rodata : { *(.rodata .rodata.* .gnu.linkonce.r*) }
    PROVIDE(_data = .);
    .data : { *(.data .data.* .gnu.linkonce.d*) }
    .bss (NOLOAD) : {
        . = ALIGN(16);
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }
    PROVIDE(_loader_end = .);
    PROVIDE(_end = .);
   
    __loader_size = (_loader_end - _loader_start) /8;   /*  8 bytes unit */
    
    __bss_size = (__bss_end - __bss_start) /8;

    PROVIDE(__stack_top = 0x400000);     /* 4MB (0x400000) lots of space*/


   /DISCARD/ : { *(.comment) *(.gnu*) *(.note*) *(.eh_frame*) }
}
