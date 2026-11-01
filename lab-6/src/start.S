.section ".text.boot"

.global _start

_start:
    msr daifset, #0xf
    mov x20, x0                 // firmware DTB physical address
    mrs x1, mpidr_el1
    and x1, x1, #3
    cbz x1, 2f
1:  wfe
    b 1b
2:  mrs x0, CurrentEL
    cmp x0, #8
    b.ne 3f
    mov x0, #(1 << 31)
    msr hcr_el2, x0
    mov x0, #3
    msr cnthctl_el2, x0
    msr cntvoff_el2, xzr
    mov x0, #0x3c5
    msr spsr_el2, x0
    adr x0, 3f
    msr elr_el2, x0
    eret
3:  // PC-relative addresses remain physical until MMU activation.
    adrp x1, __bss_start
    add x1, x1, :lo12:__bss_start
    adrp x2, __bss_end
    add x2, x2, :lo12:__bss_end
4:  cmp x1, x2
    b.hs 5f
    str xzr, [x1], #8
    b 4b
5:  adrp x0, boot_pgd
    adrp x1, boot_pud
    orr x2, x1, #3
    str x2, [x0]
    adrp x2, boot_pmd
    mov x3, #0
6:  orr x4, x2, #3
    str x4, [x1, x3, lsl #3]
    add x2, x2, #4096
    add x3, x3, #1
    cmp x3, #4
    b.lo 6b
    adrp x1, boot_pmd
    mov x2, #0
    ldr x5, =0xfc000000         // Pi 4 peripheral window
    ldr x6, =0x40000000000705   // normal NC, inner shareable, AF, EL0 XN
    ldr x7, =0x60000000000401   // device nGnRnE, AF, PXN/UXN
7:  cmp x2, x5
    csel x3, x6, x7, lo
    orr x3, x3, x2
    str x3, [x1], #8
    add x2, x2, #0x200000
    lsr x4, x2, #32
    cbz x4, 7b
    mov x1, #0x4400
    msr mair_el1, x1
    ldr x1, =0x0000000280100010 // 48-bit VA, 4 KiB, 40-bit PA, no ASID
    msr tcr_el1, x1
    msr ttbr0_el1, x0
    msr ttbr1_el1, x0
    dsb sy
    isb
    tlbi vmalle1
    dsb sy
    isb
    mrs x1, sctlr_el1
    bic x1, x1, #(1 << 12)      // instruction cache disabled
    bic x1, x1, #(1 << 2)       // data cache disabled
    bic x1, x1, #(1 << 19)      // WXN: raw user binaries contain text/data
    bic x1, x1, #(1 << 1)
    orr x1, x1, #1
    msr sctlr_el1, x1
    isb
    ldr x1, =boot_high
    br x1
boot_high:
    ldr x1, =boot_stack_top
    mov sp, x1
    ldr x1, =_dtb_ptr
    mov x2, #0xffff000000000000
    add x20, x20, x2
    str x20, [x1]
    ldr x0, =empty_pgd
    sub x0, x0, x2
    msr ttbr0_el1, x0           // remove the temporary identity map
    dsb ish
    tlbi vmalle1is
    dsb ish
    isb
    ldr x0, =exception_vector_table
    msr vbar_el1, x0
    mrs x0, cpacr_el1
    orr x0, x0, #(3 << 20)
    msr cpacr_el1, x0
    mov x0, #3
    msr cntkctl_el1, x0         // user video player reads physical counter
    isb
    bl main
    b 1b

.global enter_el0
.type enter_el0, %function
enter_el0:
    // Do not let an IRQ overwrite the exception-return registers.
    msr daifset, #0xf
    msr sp_el0, x1
    msr elr_el1, x0
    mov x2, #0
    msr spsr_el1, x2
    // A user entry point must not return into its EL1 caller.
    mov x30, xzr
    isb
    eret
.size enter_el0, . - enter_el0

// save general registers to stack
.macro save_all
    // GPRs, exception return state, all FP/SIMD registers and FPCR/FPSR.
    sub sp, sp, 50 * 16
    stp x0, x1, [sp ,16 * 0]
    stp x2, x3, [sp ,16 * 1]
    stp x4, x5, [sp ,16 * 2]
    stp x6, x7, [sp ,16 * 3]
    stp x8, x9, [sp ,16 * 4]
    stp x10, x11, [sp ,16 * 5]
    stp x12, x13, [sp ,16 * 6]
    stp x14, x15, [sp ,16 * 7]
    stp x16, x17, [sp ,16 * 8]
    stp x18, x19, [sp ,16 * 9]
    stp x20, x21, [sp ,16 * 10]
    stp x22, x23, [sp ,16 * 11]
    stp x24, x25, [sp ,16 * 12]
    stp x26, x27, [sp ,16 * 13]
    stp x28, x29, [sp ,16 * 14]
    str x30, [sp, 16 * 15]
    mrs x0, sp_el0
    str x0, [sp, #248]

    // A nested exception overwrites these system registers, so preserve them.
    mrs x0, elr_el1
    mrs x1, spsr_el1
    stp x0, x1, [sp, 16 * 16]
    // Interrupts can arrive with caller-saved FP/SIMD values live.
    stp q0, q1, [sp, #272]
    stp q2, q3, [sp, #304]
    stp q4, q5, [sp, #336]
    stp q6, q7, [sp, #368]
    stp q8, q9, [sp, #400]
    stp q10, q11, [sp, #432]
    stp q12, q13, [sp, #464]
    stp q14, q15, [sp, #496]
    stp q16, q17, [sp, #528]
    stp q18, q19, [sp, #560]
    stp q20, q21, [sp, #592]
    stp q22, q23, [sp, #624]
    stp q24, q25, [sp, #656]
    stp q26, q27, [sp, #688]
    stp q28, q29, [sp, #720]
    stp q30, q31, [sp, #752]
    mrs x0, fpcr
    mrs x1, fpsr
    str x0, [sp, #784]
    str x1, [sp, #792]
.endm

// load general registers from stack
.macro load_all
    // Restore the interrupted FP/SIMD state before restoring GPRs.
    ldp q0, q1, [sp, #272]
    ldp q2, q3, [sp, #304]
    ldp q4, q5, [sp, #336]
    ldp q6, q7, [sp, #368]
    ldp q8, q9, [sp, #400]
    ldp q10, q11, [sp, #432]
    ldp q12, q13, [sp, #464]
    ldp q14, q15, [sp, #496]
    ldp q16, q17, [sp, #528]
    ldp q18, q19, [sp, #560]
    ldp q20, q21, [sp, #592]
    ldp q22, q23, [sp, #624]
    ldp q24, q25, [sp, #656]
    ldp q26, q27, [sp, #688]
    ldp q28, q29, [sp, #720]
    ldp q30, q31, [sp, #752]
    ldr x0, [sp, #784]
    ldr x1, [sp, #792]
    msr fpcr, x0
    msr fpsr, x1
    // Restore the exception return state before restoring the original x0/x1.
    ldp x0, x1, [sp, 16 * 16]
    msr elr_el1, x0
    msr spsr_el1, x1

    ldr x0, [sp, #248]
    msr sp_el0, x0
    ldp x0, x1, [sp ,16 * 0]
    ldp x2, x3, [sp ,16 * 1]
    ldp x4, x5, [sp ,16 * 2]
    ldp x6, x7, [sp ,16 * 3]
    ldp x8, x9, [sp ,16 * 4]
    ldp x10, x11, [sp ,16 * 5]
    ldp x12, x13, [sp ,16 * 6]
    ldp x14, x15, [sp ,16 * 7]
    ldp x16, x17, [sp ,16 * 8]
    ldp x18, x19, [sp ,16 * 9]
    ldp x20, x21, [sp ,16 * 10]
    ldp x22, x23, [sp ,16 * 11]
    ldp x24, x25, [sp ,16 * 12]
    ldp x26, x27, [sp ,16 * 13]
    ldp x28, x29, [sp ,16 * 14]
    ldr x30, [sp, 16 * 15]
    add sp, sp, 50 * 16
.endm

.global process_return
.type process_return, %function
process_return:
    msr daifset, #2
    mov sp, x0
    load_all
    eret

exception_handler:
    save_all
    mov x0, sp
    bl except_handler_c
    load_all
    eret

irq_exception_handler:
	save_all
    mov x0, sp
	bl irq_except_handler_c
	load_all
	eret

.align 11 // vector table should be aligned to 0x800
.global exception_vector_table
exception_vector_table:
  b exception_handler // branch to a handler function.
  .align 7 // entry size is 0x80, .align will pad 0
  b exception_handler
  .align 7
  b exception_handler
  .align 7
  b exception_handler
  .align 7

  b exception_handler
  .align 7
  b irq_exception_handler
  .align 7
  b exception_handler
  .align 7
  b exception_handler
  .align 7

  b exception_handler
  .align 7
  b irq_exception_handler
  .align 7
  b exception_handler
  .align 7
  b exception_handler
  .align 7

  b exception_handler
  .align 7
  b exception_handler
  .align 7
  b exception_handler
  .align 7
  b exception_handler
  .align 7


.global _dtb_ptr	//define a global variable _dtb_ptr
.section .data		//_dtb_ptr is in data section
.align 3
_dtb_ptr: .dc.a 0x0	//it defines _dtb_ptr to be a 8-byte constant with a value of 0x0

.section .bss
.align 12
boot_stack:
    .skip 16384
boot_stack_top:

.align 12
.global boot_pgd, empty_pgd
boot_pgd: .skip 4096
boot_pud: .skip 4096
boot_pmd: .skip 16384
empty_pgd: .skip 4096
.section .note.GNU-stack,"",%progbits
