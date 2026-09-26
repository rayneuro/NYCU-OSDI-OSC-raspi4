.text
.align 2
.global switch_to
.type switch_to, %function
switch_to:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]
    mov x9, sp
    str x9, [x0, #96]
    // AAPCS64 preserves the low 64 bits of v8-v15, including scalar FP.
    stp d8, d9, [x0, #104]
    stp d10, d11, [x0, #120]
    stp d12, d13, [x0, #136]
    stp d14, d15, [x0, #152]
    mrs x9, fpcr
    mrs x10, fpsr
    stp x9, x10, [x0, #168]

    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]
    ldr x9, [x1, #96]
    ldp d8, d9, [x1, #104]
    ldp d10, d11, [x1, #120]
    ldp d12, d13, [x1, #136]
    ldp d14, d15, [x1, #152]
    mov sp, x9
    ldp x9, x10, [x1, #168]
    msr fpcr, x9
    msr fpsr, x10
    msr tpidr_el1, x1
    ret
.size switch_to, . - switch_to

.global get_current
.type get_current, %function
get_current:
    mrs x0, tpidr_el1
    ret
.size get_current, . - get_current
.section .note.GNU-stack,"",%progbits
