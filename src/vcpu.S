.section ".text"

.globl vcpu_run
vcpu_run:
    ##
    ## x0 holds the ptr to the vcpu struct
    ## save hypv ctx (callee-saved registers 
    ##                & link register)
    ##
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
    stp     x29, x30, [sp, #-16]!

    ##
    ## the exception handler needs to know where the vcpu struct is
    ## we pass it in x19 (callee-saved reg), but we gotta save the orig hypv x19
    ## first...
    ## it's already been saved to the stack, so the handler will use this same ptr
    ##
    mov     x19, x0

    ##
    ## restore guest's special regs
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
    ##
    ## skip restoring x19 cuz it's our ctx ptr
    ## guest's x19 will be restored right before eret
    ##
    ldp     x18, x20, [x19, #(16 * 9)]
    ldp     x21, x22, [x19, #(16 * 10)]
    ldp     x23, x24, [x19, #(16 * 11)]
    ldp     x25, x26, [x19, #(16 * 12)]
    ldp     x27, x28, [x19, #(16 * 13)]
    ldp     x29, x30, [x19, #(16 * 14)]

    ##
    ## restore special regs
    ##
    ldr     x21, [x19, #(8 * 31)]   // offset of sp_el1
    msr     sp_el1, x21
    ldr     x21, [x19, #(8 * 32)]   // offset of elr_el2
    msr     elr_el2, x21
    ldr     x21, [x19, #(8 * 33)]   // offset of spsr_el2
    msr     spsr_el2, x21
    ##
    ## restore guest's x19 and drop into guest
    ##
    ldr     x19, [x19, #(8 * 19)]   // offset of    x[19]
    eret