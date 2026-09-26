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
     // cpu id == 0
1:  wfe
    b       1b
2: 
    // set stack before calling C code
    ldr     x1, =boot_stack_top
    bl from_el2_to_el1
    mov     sp, x1

    // clear bss
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
3:  cmp     x1, x2
    b.hs    4f
    strb    wzr, [x1], #1
    b       3b
    # the next instruction runs in EL1

    // Install the EL1 exception vector table before enabling interrupts.
4:  ldr     x0, =exception_vector_table
    msr     vbar_el1, x0 // vbar_el1
    isb

    // Enable EL1 FP/SIMD
    mrs x0, cpacr_el1
    orr x0, x0, #(3 << 20)   // FPEN = 0b11
    msr cpacr_el1, x0
    isb
    // jump to C code, should not return
    bl      main
    // for failsafe, halt this core too
    b       1b

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

from_el2_to_el1:
    mov x0, (1 << 31) // EL1 uses aarch64
    msr hcr_el2, x0
    mov x0, 0x3c5 // EL1h (SPSel = 1) with interrupt disabled  (DIAF, RES0, M[4], M[3:0] )
    msr spsr_el2, x0
    msr elr_el2, lr
    eret // return to EL1

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
    .skip 4096
boot_stack_top:
