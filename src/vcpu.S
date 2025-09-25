#include "asm_utils.h"

.extern handle_trap
.extern handle_irq

.macro save_regs
    sub     sp, sp, #(34 * 8)
    stp     x0, x1, [sp, #(0 * 16)]
    stp     x2, x3, [sp, #(1 * 16)]
    stp     x4, x5, [sp, #(2 * 16)]
    stp     x6, x7, [sp, #(3 * 16)]
    stp     x8, x9, [sp, #(4 * 16)]
    stp     x10, x11, [sp, #(5 * 16)]
    stp     x12, x13, [sp, #(6 * 16)]
    stp     x14, x15, [sp, #(7 * 16)]
    stp     x16, x17, [sp, #(8 * 16)]
    stp     x18, x19, [sp, #(9 * 16)]
    stp     x20, x21, [sp, #(10 * 16)]
    stp     x22, x23, [sp, #(11 * 16)]
    stp     x24, x25, [sp, #(12 * 16)]
    stp     x26, x27, [sp, #(13 * 16)]
    stp     x28, x29, [sp, #(14 * 16)]
    str     x30, [sp, #(15 * 16)]
    mrs     x0, sp_el1
    mrs     x1, elr_el2
    mrs     x2, spsr_el2
    stp     x0, x1, [sp, #(16 * 16)]
    str     x2, [sp, #(17 * 16)]
.endm

.macro restore_regs
    ldp     x0, x1, [sp, #(16 * 16)]
    ldr     x2, [sp, #(17 * 16)]
    msr     sp_el1, x0
    msr     elr_el2, x1
    msr     spsr_el2, x2
    ldp     x0, x1, [sp, #(0 * 16)]
    ldp     x2, x3, [sp, #(1 * 16)]
    ldp     x4, x5, [sp, #(2 * 16)]
    ldp     x6, x7, [sp, #(3 * 16)]
    ldp     x8, x9, [sp, #(4 * 16)]
    ldp     x10, x11, [sp, #(5 * 16)]
    ldp     x12, x13, [sp, #(6 * 16)]
    ldp     x14, x15, [sp, #(7 * 16)]
    ldp     x16, x17, [sp, #(8 * 16)]
    ldp     x18, x19, [sp, #(9 * 16)]
    ldp     x20, x21, [sp, #(10 * 16)]
    ldp     x22, x23, [sp, #(11 * 16)]
    ldp     x24, x25, [sp, #(12 * 16)]
    ldp     x26, x27, [sp, #(13 * 16)]
    ldp     x28, x29, [sp, #(14 * 16)]
    ldr     x30, [sp, #(15 * 16)]
    add     sp, sp, #(34 * 8)
.endm


.globl __lower_el_sync_handler
__lower_el_sync_handler:
    save_regs
    mov     x0, sp
    bl      handle_trap
    restore_regs
    eret

.globl __lower_el_irq_handler
__lower_el_irq_handler:
    save_regs
    mov     x0, sp
    bl      handle_irq
    // handle_irq -> sched -> vcpu_run does not return here
    // the context for the *next* vcpu is restored by vcpu_run
    b       .

.globl vcpu_run
vcpu_run:
    ldr     x1, [x0, #8]
    ldp     x0, x1, [x0, #0]
    ldp     x2, x3, [x0, #(1 * 16)]
    ldp     x4, x5, [x0, #(2 * 16)]
    ldp     x6, x7, [x0, #(3 * 16)]
    ldp     x8, x9, [x0, #(4 * 16)]
    ldp     x10, x11, [x0, #(5 * 16)]
    ldp     x12, x13, [x0, #(6 * 16)]
    ldp     x14, x15, [x0, #(7 * 16)]
    ldp     x16, x17, [x0, #(8 * 16)]
    ldp     x18, x19, [x0, #(9 * 16)]
    ldp     x20, x21, [x0, #(10 * 16)]
    ldp     x22, x23, [x0, #(11 * 16)]
    ldp     x24, x25, [x0, #(12 * 16)]
    ldp     x26, x27, [x0, #(13 * 16)]
    ldp     x28, x29, [x0, #(14 * 16)]
    ldr     x30, [x0, #(15 * 16)]

    add     x0, x0, #(31 * 8)
    ldp     x1, x2, [x0, #0]
    ldr     x3, [x0, #16]

    msr     sp_el1, x1
    msr     elr_el2, x2
    msr     spsr_el2, x3

    eret
