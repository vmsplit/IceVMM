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
    ## affinity register. the lower bits are the core ID
    ##
    mrs     x0, mpidr_el1
    and     x0, x0, #3          /* mask to get core ID                 */
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
    ## BL(branch w/ link)
    ## stores the return addr in the link reg aka x30
    ##
    bl      main

.globl hang

hang:
    ##
    ## enter inf loop is main rets
    ## or if we need to halt
    ##
    wfi
    b       hang