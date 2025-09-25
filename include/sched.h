#ifndef   __SCHED_H__
#define   __SCHED_H__

#include "vm.h"

void sched_init(void);
void sched_add_vcpu(vcpu_t *vcpu);
void sched(vcpu_regs_t *regs);

#endif // __SCHED_H__
