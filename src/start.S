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
    str     xzr, [x1], #8       /* store the xzr aka zero reg at the addr, then increment by 8 */
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