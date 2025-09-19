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