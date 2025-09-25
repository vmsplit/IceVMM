#include <stddef.h>
#include "../include/sched.h"
#include "../include/uart.h"
#include "../include/mm.h"


#define MAX_SCHED_VCPUS     8


static vcpu_t *vcpu_queue[MAX_SCHED_VCPUS];
static int vcpu_cnt = 0;
static int curr_vcpu_idx = -1;

extern void vcpu_run(vcpu_t *vcpu);


void sched_init(void)
{
    vcpu_cnt = 0;
    curr_vcpu_idx = -1;
}


void sched_add_vcpu(vcpu_t *vcpu)
{
    if (vcpu_cnt < MAX_SCHED_VCPUS) {
        vcpu_queue[vcpu_cnt++] = vcpu;
        vcpu->state = VCPU_STATE_RUNNABLE;
    }
}


void sched(vcpu_regs_t *regs)
{
    if (curr_vcpu_idx != -1) {
        vcpu_t *last_vcpu = vcpu_queue[curr_vcpu_idx];
            if (last_vcpu->state == VCPU_STATE_RUNNING) {
                    /* save ctx of preempted vcpu */
                    memcpy(&last_vcpu->regs, regs, sizeof(vcpu_regs_t));
                    last_vcpu->state = VCPU_STATE_RUNNABLE;
            }
    }

    curr_vcpu_idx = (curr_vcpu_idx + 1) % vcpu_cnt;
    vcpu_t *next_vcpu = vcpu_queue[curr_vcpu_idx];

    uart_puts("icevmm: scheduling vcpu=");
    uart_put_hex(next_vcpu->vcpu_id);
    uart_puts("\n");

    next_vcpu->state = VCPU_STATE_RUNNING;
    vcpu_run(next_vcpu);
}
