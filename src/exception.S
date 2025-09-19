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

__lower_el_sync_handler:
    ##
    ## x19 still holds the vcpu ptr from before 
    ## the eret in vcpu_run. guest has trapped 
    ## so we save its state
    ##
    ## offset is calc'd from the start of the
    ## vcpu_t struct, which contains the 
    ## vcpu_regs_t struct
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

    ##
    ## save sp, elr, spsr el1/2 regs
    ##
    mrs     x20, sp_el1
    str     x20, [x19, #8*31]
    mrs     x20, elr_el2
    str     x20, [x19, #8*32]
    mrs     x20, spsr_el2
    str     x20, [x19, #8*33]

    ##
    ## call our trap handler
    ##
    ## vcpu_t* arg is passed in
    ## x0 according to ABI
    ##
    ## our ptr still resides in x19
    ##
    mov     x0, x19
    bl      __handle_trap

    ##
    ## restore guest state from
    ## vcpu struct and resume it
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
    /* skip x19 again again */
    ldp     x18, x20, [x19, #8*18]
    ldp     x21, x22, [x19, #8*21]
    ldp     x23, x24, [x19, #8*23]
    ldp     x25, x26, [x19, #8*25]
    ldp     x27, x28, [x19, #8*27]
    ldp     x29, x30, [x19, #8*29]

    /* restore sp, elr, spsr el1/2 regs for guest */
    ldr     x20, [x19, #8*31]
    msr     sp_el1,   x20
    ldr     x20, [x19, #8*32]
    msr     elr_el2,  x20
    ldr     x20, [x19, #8*33]
    msr     spsr_el2, x20

    eret