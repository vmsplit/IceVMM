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

.globl __read_esr_el2
__read_esr_el2:
    mrs     x0, esr_el2
    ret

.globl __read_far_el2
__read_far_el2:
    mrs     x0, far_el2
    ret

.globl __write_mair_el2
__write_mair_el2:
    msr     mair_el2, x0
    ret

.globl __tlbi_vmalle1
__tlbi_vmalle1:
    tlbi    vmalle1
    dsb     sy
    isb
    ret

##
## GICv3 & timer regs
##
.globl __read_icc_iar1_el1
__read_icc_iar1_el1:
    mrs     x0, icc_iar1_el1
    ret

.globl __write_icc_eoir1_el1
__write_icc_eoir1_el1:
    msr     icc_eoir1_el1, x0
    ret

.globl __write_icc_sre_el2
__write_icc_sre_el2:
    msr     icc_sre_el2, x0
    isb
    ret

.globl __write_ich_hcr_el2
__write_ich_hcr_el2:
    msr     ich_hcr_el2, x0
    isb
    ret

.globl __write_ich_lr0_el2
__write_ich_lr0_el2:
    msr     ich_lr0_el2, x0
    isb
    ret

.globl __read_cntp_ctl_el0
__read_cntp_ctl_el0:
    mrs     x0, cntp_ctl_el0
    ret

.globl __write_cntp_ctl_el0
__write_cntp_ctl_el0:
    msr     cntp_ctl_el0, x0
    ret

.globl __read_cntp_cval_el0
__read_cntp_cval_el0:
    mrs     x0, cntp_cval_el0
    ret

.globl __write_cntp_cval_el0
__write_cntp_cval_el0:
    msr     cntp_cval_el0, x0
    ret
