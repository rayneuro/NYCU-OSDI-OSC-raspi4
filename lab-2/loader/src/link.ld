SECTIONS
{
    . = 0x60000;                    /* same as config.txt kernel_address */

    .text.boot : {
        KEEP(*(.text.boot))
    }

    . = ALIGN(0x10);

    .text   : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data .data.*) }

    . = ALIGN(0x10);
    __bss_start = .;
    .bss (NOLOAD) : {
        *(.bss .bss.*)
        *(COMMON)
    }
    __bss_end = .;

    _end = .;

    /* Stack 必須放在 bootloader 上方，但 kernel 下方 */
    PROVIDE(_stack_top = 0x78000);   /* 建議值：給約 32KB stack */
}