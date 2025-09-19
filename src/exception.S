.section ".text.exceptions"
.global __exception_vectors

##
## vector table has to be 
## 2048-byte 0x800 aligned
##
.align 11
__exception_vectors:
    ##
    ## each vector entry is 0x80 or 128 bytes long
    ## for now we just use an inf loop
    ##

    ##
    ## current el with SP0: sync, IRQ, FIQ, SError
    ##
    .align 7; b .;
    .align 7; b .;
    .align 7; b .;
    .align 7; b .;
    ##
    ## current el with SPx: sync, IRQ, FIQ, SError
    ##
    .align 7; b .;
    .align 7; b .;
    .align 7; b .;
    .align 7; b .;

    ##
    ## lower el using aarch64: sync, IRQ, FIQ, SError
    ## HVC from el1 lands here
    ##
    .align 7
__lower_el_sync_handler:
    ##
    ## x0 contains ptr to the vcpu struct
    ## passed from vcpu_run
    ##
    ## save it and use x19, 
    ## a callee-saved reg
    ##
    mov     x19, x0

    ##
    ## save all guest's GPRs x0-x30
    ## into the vCPU struct
    ##
   stp     x0,   x1,  [x19, #(16 * 0)]
    stp     x2,  x3,  [x19, #(16 * 1)]
    stp     x4,  x5,  [x19, #(16 * 2)]
    stp     x6,  x7,  [x19, #(16 * 3)]
    stp     x8,  x9,  [x19, #(16 * 4)]
    stp     x10, x11, [x19, #(16 * 5)]
    stp     x12, x13, [x19, #(16 * 6)]
    stp     x14, x15, [x19, #(16 * 7)]
    stp     x16, x17, [x19, #(16 * 8)]
    stp     x18, x20, [x19, #(16 * 9)]
    stp     x21, x22, [x19, #(16 * 10)]
    stp     x23, x24, [x19, #(16 * 11)]
    stp     x25, x26, [x19, #(16 * 12)]
    stp     x27, x28, [x19, #(16 * 13)]
    stp     x29, x30, [x19, #(16 * 14)]


    ##
    ## save sp, elr, spsr el1/2 regs
    ##
    mrs     x20, sp_el1
    str     x20, [x19, #(8 * 31)]   // regs.sp_el1
    mrs     x20, elr_el2
    str     x20, [x19, #(8 * 32)]   // regs.elr_el2
    mrs     x20, spsr_el2
    str     x20, [x19, #(8 * 33)]   // regs.spsr_el2


    ##
    ## call our trap handler
    ##
    ## vcpu_t* arg is passed in
    ## x0 according to ABI
    ##
    ## our ptr still resides in x19
    ##
    mov     x0, x19
    bl      handle_trap

    ##
    ## restore guest's state from
    ## vcpu struct and resume it
    ##
    ldp     x0,  x1,  [x19, #(16 * 0)]
    ldp     x2,  x3,  [x19, #(16 * 1)]
    ldp     x4,  x5,  [x19, #(16 * 2)]
    ldp     x6,  x7,  [x19, #(16 * 3)]
    ldp     x8,  x9,  [x19, #(16 * 4)]
    ldp     x10, x11, [x19, #(16 * 5)]
    ldp     x12, x13, [x19, #(16 * 6)]
    ldp     x14, x15, [x19, #(16 * 7)]
    ldp     x16, x17, [x19, #(16 * 8)]
    ldp     x18, x20, [x19, #(16 * 9)]
    ldp     x21, x22, [x19, #(16 * 10)]
    ldp     x23, x24, [x19, #(16 * 11)]
    ldp     x25, x26, [x19, #(16 * 12)]
    ldp     x27, x28, [x19, #(16 * 13)]
    ldp     x29, x30, [x19, #(16 * 14)]

    ldr     x20, [x19, #8*31]  /* regs.sp_el1 */
    msr     sp_el1,   x20
    ldr     x20, [x19, #8*32]  /* regs.elr_el2 
                                    (the incremented val!) */
    msr     elr_el2,  x20
    ldr     x20, [x19, #8*33]  /* regs.spsr_el2 */
    msr     spsr_el2, x20

    eret

    ##
    ## lower el using aarch64: IRQ, FIQ, SError
    ##
    .align 7; b .;
    .align 7; b .;
    .align 7; b .;
    ##
    ## lower el using aarch32: sync, IRQ, FIQ, SError
    ## unused
    ##
    .align 7; b .;
    .align 7; b .;
    .align 7; b .;
    .align 7; b .;