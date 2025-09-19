.section ".text"

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

.globl __write_vttbr_el2

__write_vttbr_el2:
    msr     vttbr_el2, x0
    ret

.globl __write_vtcr_el2

__write_vtcr_el2:
    msr     vtcr_el2, x0
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