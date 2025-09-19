.section ".text"

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