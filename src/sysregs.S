.section ".text"

.globl get_el
get_el:
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    ret

##
## stage 1 MMU
##
.globl __write_sctlr_el2
__write_sctlr_el2:
    msr     sctlr_el2, x0
    ret

.globl __read_sctlr_el2
__read_sctlr_el2:
    mrs     x0, sctlr_el2
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
## hypv cfg
##
.globl __write_hcr_el2
__write_hcr_el2:
    msr     hcr_el2, x0
    ret

.globl __write_cptr_el2
__write_cptr_el2:
    msr     cptr_el2, x0
    ret

.globl __write_vbar_el2
__write_vbar_el2:
    msr     vbar_el2, x0
    ret

##
## stage 2 MMU
##
.globl __write_vttbr_el2
__write_vttbr_el2:
    msr     vttbr_el2, x0
    ret

.globl __write_vtcr_el2
__write_vtcr_el2:
    msr     vtcr_el2, x0
    ret

.globl __tlbi_vmalle1
__tlbi_vmalle1:
    tlbi    vmalle1
    dsb     sy
    isb
    ret