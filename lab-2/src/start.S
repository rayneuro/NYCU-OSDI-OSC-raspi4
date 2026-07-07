.section ".text.boot"

.global _start

_start:
    // read cpu id, stop slave cores
    ldr     x1, =_dtb_ptr
    str     x0, [x1]
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbz     x1, 2f
    // cpu id > 0, stop
1:  wfe
    b       1b
2:  // cpu id == 0



    // set stack before calling C code
    ldr     x1, =boot_stack_top
    mov     sp, x1

    // clear bss
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
3:  cmp     x1, x2
    b.hs    4f
    strb    wzr, [x1], #1
    b       3b

    // jump to C code, should not return
4:  bl      main
    // for failsafe, halt this core too
    b       1b

.global _dtb_ptr	//define a global variable _dtb_ptr
.section .data		//_dtb_ptr is in data section
.align 3
_dtb_ptr: .dc.a 0x0	//it defines _dtb_ptr to be a 8-byte constant with a value of 0x0

.section .bss
.align 12
boot_stack:
    .skip 4096
boot_stack_top:
