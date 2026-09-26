.text
.arch armv8-a
.global user_register_probe
.type user_register_probe, %function
user_register_probe:
    str x19, [sp, #-16]!
    mov x19, #0x1919
    mov x10, #0x1010
    movi v0.16b, #0x55
    movi v31.16b, #0xaa
    mov x9, #0x1000000
1:  subs x9, x9, #1
    b.ne 1b
    mov x11, #0x1919
    cmp x19, x11
    b.ne 2f
    mov x11, #0x1010
    cmp x10, x11
    b.ne 2f
    umov x11, v0.d[1]
    mov x12, #0x5555555555555555
    cmp x11, x12
    b.ne 2f
    umov x11, v31.d[1]
    mov x12, #0xaaaaaaaaaaaaaaaa
    cmp x11, x12
    b.ne 2f
    mov x0, #1
    b 3f
2:  mov x0, #0
3:  ldr x19, [sp], #16
    ret
.section .note.GNU-stack,"",%progbits
