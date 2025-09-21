.section ".text.exceptions"
.global __exception_vectors

##
## vector table has to be 
## 2048-byte 0x800 aligned
##
.align 11
__exception_vectors:
    .align 7; b .;  /* current EL with SP0:  Sync   */
    .align 7; b .;  /* current EL with SP0:  IRQ    */
    .align 7; b .;  /* current EL with SP0:  FIQ    */
    .align 7; b .;  /* current EL with SP0:  SError */

    .align 7; b .;  /* current EL with SPx:  Sync   */
    .align 7; b .;  /* current EL with SPx:  IRQ    */
    .align 7; b .;  /* current EL with SPx:  FIQ    */
    .align 7; b .;  /* current EL with SPx:  SError */

    .align 7
__lower_el_sync_handler:
    ##
    ## a hardware trap from lower EL doesn't pass args in regs so we must
    ## load the addr of our vCPU ctx from a known global loc.
    ##
    ## we use x19 as a callee-saved reg to hold the ptr
    ##
    ## so we can load the addr of the global `guest_vm` into x19
    ##
    adrp    x19, guest_vm
    add     x19, x19, :lo12:guest_vm
    /* note: guest_vm is the vm_t struct, vcpu_t is the first member
             so the addr will be the same */
    ##
    ## now that x19 holds our ctx ptr, save all guest regs
    ##      all regs x0-x30 contain the guest's state at the time of trap
    ##
    stp     x0,  x1,  [x19, #(16 * 0)]
    stp     x2,  x3,  [x19, #(16 * 1)]
    stp     x4,  x5,  [x19, #(16 * 2)]
    stp     x6,  x7,  [x19, #(16 * 3)]
    stp     x8,  x9,  [x19, #(16 * 4)]
    stp     x10, x11, [x19, #(16 * 5)]
    stp     x12, x13, [x19, #(16 * 6)]
    stp     x14, x15, [x19, #(16 * 7)]
    stp     x16, x17, [x19, #(16 * 8)]
    stp     x18, x19, [x19, #(16 * 9)]
    stp     x20, x21, [x19, #(16 * 10)]
    stp     x22, x23, [x19, #(16 * 11)]
    stp     x24, x25, [x19, #(16 * 12)]
    stp     x26, x27, [x19, #(16 * 13)]
    stp     x28, x29, [x19, #(16 * 14)]
    str     x30,      [x19, #(8 * 30)]
    ##
    ## now we can save any special regs
    ##
    mrs     x20, sp_el1
    str     x20, [x19, #(8 * 31)]   /* regs.sp_el1   */
    mrs     x20, elr_el2
    str     x20, [x19, #(8 * 32)]   /* regs.elr_el2  */
    mrs     x20, spsr_el2
    str     x20, [x19, #(8 * 33)]   /* regs.spsr_el2 */
    ##
    ## now we call our C trap handler
    ## x0 must point to the vcpu struct
    ##
    mov     x0, x19
    bl      handle_trap
    ##
    ## restore guest's state & resume
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
    ldp     x18, x19, [x19, #(16 * 9)]
    ldp     x20, x21, [x19, #(16 * 10)]
    ldp     x22, x23, [x19, #(16 * 11)]
    ldp     x24, x25, [x19, #(16 * 12)]
    ldp     x26, x27, [x19, #(16 * 13)]
    ldp     x28, x29, [x19, #(16 * 14)]
    ldr     x30,      [x19, #(8 * 30)]
    ##
    ## restore special regs (and the vals 
    ##          should be modified by handler)
    ##
    ldr     x20, [x19, #(8 * 31)]
    msr     sp_el1,    x20
    ldr     x20, [x19, #(8 * 32)]
    msr     elr_el2,   x20
    ldr     x20, [x19, #(8 * 33)]
    msr     spsr_el2,  x20

    eret

    .align 7; b .; /* lower el using aarch64:  IRQ, FIQ, SError  */
    .align 7; b .;
    .align 7; b .;
    .align 7; b .; /* lower el using aarch32:  unused            */
    .align 7; b .;
    .align 7; b .;
    .align 7; b .;