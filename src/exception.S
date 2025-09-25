#include "asm_utils.h"

.section ".text.exceptions"
.align 11 // 2^11 = 2048-byte alignment for the vector table

.globl __exception_vectors
__exception_vectors:
    // Exception from current EL with SP0
    .align 7
    b handle_sync_exception
    .align 7
    b handle_irq
    .align 7
    b . // FIQ
    .align 7
    b . // SError

    // Exception from current EL with SPx
    .align 7
    b handle_sync_exception
    .align 7
    b handle_irq
    .align 7
    b . // FIQ
    .align 7
    b . // SError

    // Exception from lower EL using AArch64
    .align 7
    b . // Sync
    .align 7
    b . // IRQ
    .align 7
    b . // FIQ
    .align 7
    b . // SError

    // Exception from lower EL using AArch32
    .align 7
    b . // Sync
    .align 7
    b . // IRQ
    .align 7
    b . // FIQ
    .align 7
    b . // SError


// Generic handler for synchronous exceptions from EL2
.globl handle_sync_exception
handle_sync_exception:
    // We don't expect any, so just print registers and hang.
    mrs x0, esr_el2
    mrs x1, elr_el2
    mrs x2, far_el2
    bl  dump_registers_and_hang // Defined in main.c for debugging
    b   .


// IRQ handler for EL2 (will be used for the timer)
.globl handle_irq
handle_irq:
    sub sp, sp, #256  // Allocate stack space (16 regs * 16 bytes)
    save_all
    bl sched       // Call the C scheduler function
    restore_all
    add sp, sp, #256  // Deallocate stack space
    eret              // Return from exception
