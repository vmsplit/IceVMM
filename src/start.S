##
## file must be placed in the .text.boot sect
## to ensure it's at the very beginning of the
## executable
##
.section ".text.boot"

##
## declare the global entrypoint symbol, as specified
## in the linker script
##
.globl _start

_start:
    ##
    ## find out which core we are
    ##
    ## the MPIDR_EL1 reg holds the multi-processor
    ## affinity register
    ## the lower bits are the core ID
    ##
    mrs     x0, mpidr_el1
    and     x0, x0, #3          /* mask to get core ID */
    cbz     x0, .primary_core   /* if core ID is 0, it is primary core */

.secondary_core_loop:
    ##
    ## other cores will just wait for an interrupt
    ## indefinitely
    ##
    wfi
    b       .secondary_core_loop

.primary_core:
    ##
    ## set up the stack ptr
    ##
    ## we load the addr of __stack_top, which we defined
    ## in our linker script, and set the SP reg
    ##
    ldr     x1, =__stack_top
    mov     sp, x1

    ##
    ## clear the BSS sect
    ##
    ## critical because C expects all uninitialised global
    ## vars to be zero
    ##
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
.bss_loop:
    cmp     x1, x2              /* compare current addr with the end addr */
    b.ge    .bss_done           /* if start>=end, we're done              */
    str     xzr, [x1], #8       /* store the xzr aka zero reg at the addr, 
                                   then increment by 8                    */
.bss_done:
    ##
    ## jump to our C entrypoint
    ##
    ## bl -> branch with link
    ## stores the return addr in the link reg aka x30
    ##
    bl      main

.globl get_el

get_el:
    ##
    ## read the CurrentEL reg
    ## the EL is within bits 2:3
    ##
    mrs     x0, CurrentEL
    ##
    ## shift the val right by 2 bits to
    ## isolate the EL num.
    ##
    lsr     x0, x0, #2
    ret

.globl hang

hang:
    ##
    ## enter inf loop is main rets
    ## or if we need to halt
    ##
    wfi
    b       hang

// .section .rodata
// .Ltrap_msg:
//     .asciz      "!!! TRAP HANDLER INVOKED !!!\n"

.section ".text"
##
## vector table has to be 
## 2048-byte 0x800 aligned
##
.align 11

.globl __exception_vectors

__exception_vectors:
    ##
    ## current EL with SP0
    ##
    .align 7; b       hang  /* synchronous */
    .align 7; b       hang  /*     IRQ     */
    .align 7; b       hang  /*     FIQ     */
    .align 7; b       hang  /*    SError   */
    ##
    ## current EL with SPx
    ##
    .align 7; b       hang  /* synchronous */
    .align 7; b       hang  /*     IRQ     */
    .align 7; b       hang  /*     FIQ     */
    .align 7; b       hang  /*    SError   */
    ##
    ## lower EL using aarch64
    ##
    .align 7; b       __lower_el_sync_handler  /* synchronous */
    .align 7; b       hang                     /*     IRQ     */
    .align 7; b       hang                     /*     FIQ     */
    .align 7; b       hang                     /*    SError   */
    ##
    ## lower EL using aarch32
    ##
    .align 7; b       hang  /* synchronous */
    .align 7; b       hang  /*     IRQ     */
    .align 7; b       hang  /*     FIQ     */
    .align 7; b       hang  /*    SError   */

.globl vcpu_run
vcpu_run:
    ##
    ## save hypv state (callee-saved regs)
    ##
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
    stp     x29, x30, [sp, #-16]!

    ##
    ## x0 contains the vcpu ptr
    ##
    ## save it in callee-saved register
    ## for access in trap handler
    ##
    mov     x19, x0

    ##
    ## restore guest's GPRs from vcpu_t struct
    ## (see src/vm.h)
    ##
    ldp     x0,  x1,  [x19, #8*0]
    ldp     x2,  x3,  [x19, #8*2]
    ldp     x4,  x5,  [x19, #8*4]
    ldp     x6,  x7,  [x19, #8*6]
    ldp     x8,  x9,  [x19, #8*8]
    ldp     x10, x11, [x19, #8*10]
    ldp     x12, x13, [x19, #8*12]
    ldp     x14, x15, [x19, #8*14]
    ldp     x16, x17, [x19, #8*16]
    /* skip x19, our vcpu ptr, 
       we restore from another reg later */
    ldp     x18, x20, [x19, #8*18] 
    ldp     x21, x22, [x19, #8*21]
    ldp     x23, x24, [x19, #8*23]
    ldp     x25, x26, [x19, #8*25]
    ldp     x27, x28, [x19, #8*27]
    ldp     x29, x30, [x19, #8*29]

    ##
    ## restore sp, elr, spsr 
    ## el1/el2 guest regs
    ##
    ldr     x20, [x19, #8*31]
    msr     sp_el1,   x20
    ldr     x20, [x19, #8*32]
    msr     elr_el2,  x20
    ldr     x20, [x19, #8*33]
    msr     spsr_el2, x20

    eret

__lower_el_sync_handler:
    ##
    ## x19 still holds the vcpu ptr from before 
    ## the eret. guest has trapped so we save its state
    ##
    stp     x0,  x1,  [x19, #8*0]
    stp     x2,  x3,  [x19, #8*2]
    stp     x4,  x5,  [x19, #8*4]
    stp     x6,  x7,  [x19, #8*6]
    stp     x8,  x9,  [x19, #8*8]
    stp     x10, x11, [x19, #8*10]
    stp     x12, x13, [x19, #8*12]
    stp     x14, x15, [x19, #8*14]
    stp     x16, x17, [x19, #8*16]
    /* skip x19 again */
    stp     x18, x20, [x19, #8*18]
    stp     x21, x22, [x19, #8*21]
    stp     x23, x24, [x19, #8*23]
    stp     x25, x26, [x19, #8*25]
    stp     x27, x28, [x19, #8*27]
    stp     x29, x30, [x19, #8*29]

    mrs     x20, sp_el1
    str     x20, [x19, #8*31]
    mrs     x20, elr_el2
    str     x20, [x19, #8*32]
    mrs     x20, spsr_el2
    str     x20, [x19, #8*33]

    ldp     x29, x30, [sp], #16
    ldp     x27, x28, [sp], #16
    ldp     x25, x26, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16

    ret

// .globl __trap_handler

// __trap_handler:
//     ldr     x0, =.Ltrap_msg
//     bl      uart_puts
//     b       hang




##
## C-callable funcs to write to sys regs
##
.globl __write_sctlr_el2

__write_sctlr_el2:
    msr     sctlr_el2, x0
    ret

.globl __read_sctlr_el2

__read_sctlr_el2:
    mrs     x0, sctlr_el2
    ret

.globl __write_hcr_el2

__write_hcr_el2:
    msr     hcr_el2, x0
    ret

.globl __write_cptr_el2

__write_cptr_el2:
    msr     cptr_el2, x0
    ret

.globl __write_tcr_el2

__write_tcr_el2:
    msr     tcr_el2, x0
    ret

.globl __write_ttbr0_el2

__write_ttbr0_el2:
    msr     ttbr0_el2, x0
    ret

.globl __write_vbar_el2

__write_vbar_el2:
    msr     vbar_el2, x0
    ret

##
## testing translation
##
.globl va_2_pa_el2
va_2_pa_el2:
    ##
    ## translate for el2 read and move
    ## result from PAR_EL1 into our
    ## x0 return reg
    ##
    at      s1e2r, x0
    isb
    mrs     x0, par_el1
    ret