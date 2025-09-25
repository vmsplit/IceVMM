#include "asm_utils.h"

.globl get_el
get_el:
    mrs x0, CurrentEL
    lsr x0, x0, #2
    ret

.globl __read_vbar_el2
__read_vbar_el2:
    mrs     x0, vbar_el2
    ret

.globl __write_vbar_el2
__write_vbar_el2:
    msr     vbar_el2, x0
    ret

.globl __read_hcr_el2
__read_hcr_el2:
    mrs     x0, hcr_el2
    ret

.globl __write_hcr_el2
__write_hcr_el2:
    msr     hcr_el2, x0
    ret

.globl __read_sctlr_el2
__read_sctlr_el2:
    mrs     x0, sctlr_el2
    ret

.globl __write_sctlr_el2
__write_sctlr_el2:
    msr     sctlr_el2, x0
    ret

.globl __read_esr_el2
__read_esr_el2:
    mrs     x0, esr_el2
    ret

.globl __read_elr_el2
__read_elr_el2:
    mrs     x0, elr_el2
    ret

.globl __read_far_el2
__read_far_el2:
    mrs     x0, far_el2
    ret

.globl __write_cptr_el2
__write_cptr_el2:
    msr     cptr_el2, x0
    ret

.globl __write_ttbr0_el2
__write_ttbr0_el2:
    msr     ttbr0_el2, x0
    ret

.globl __write_tcr_el2
__write_tcr_el2:
    msr     tcr_el2, x0
    ret

.globl __write_mair_el2
__write_mair_el2:
    msr     mair_el2, x0
    ret

.globl __tlbi_vmalle1
__tlbi_vmalle1:
    tlbi    vmalle1is
    dsb     ish
    isb
    ret

.globl __write_vttbr_el2
__write_vttbr_el2:
    msr     vttbr_el2, x0
    ret

.globl __write_vtcr_el2
__write_vtcr_el2:
    msr     vtcr_el2, x0
    ret

.globl __write_cnthp_ctl_el2
__write_cnthp_ctl_el2:
    msr     cnthp_ctl_el2, x0
    ret

.globl __write_cnthp_cval_el2
__write_cnthp_cval_el2:
    msr     cnthp_cval_el2, x0
    ret

.globl read_cntfrq_el0
read_cntfrq_el0:
    mrs     x0, cntfrq_el0
    ret

.globl write_cntv_tval_el0
write_cntv_tval_el0:
    msr     cntv_tval_el0, x0
    ret

.globl write_cntv_ctl_el0
write_cntv_ctl_el0:
    msr     cntv_ctl_el0, x0
    ret
