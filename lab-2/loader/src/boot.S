.section ".text.boot"
.global _start
_start:
    // Check processor ID is zero (executing on main core), else hang
    mrs x1, mpidr_el1
    and x1, x1, #3
    cbnz x1, 1f

    /* Set stack */
    ldr x1, =_stack_top
    mov sp, x1

    /* clean BSS section*/
    ldr x1, =__bss_start
    ldr x2, =__bss_end
bss_clear:
    cmp x1, x2
    b.ge main_entry
    str xzr, [x1], #8
    b bss_clear

main_entry:
    /* 儲存 start4.elf 傳來的參數（主要是 x0 = DTB） */
    mov x10, x0
    mov x11, x1
    mov x12, x2
    mov x13, x3

    bl main          /* 跳到你的 C main() */

    /* 如果 main() 回傳，就停住 */
1:  wfe
    b 1b