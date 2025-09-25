#include <stdint.h>
#include "../include/timer.h"


uint64_t read_cntfrq_el0(void);
void write_cntv_tval_el0(uint32_t val);
void write_cntv_ctl_el0(uint32_t val);


void timer_init(void)
{
    /* use virtual timer CNT_V, which is controlled from el2 via
       CNTH_CTL_EL2

    traps are config'd in vm_create and the guest will be interrupted
    by a vIRQ */
    uint64_t cntfrq = read_cntfrq_el0();
    /* set timer interval to 10ms (freq/100) */
    write_cntv_tval_el0(cntfrq / 100);
    /* now enable timer */
    write_cntv_ctl_el0(1);
}
