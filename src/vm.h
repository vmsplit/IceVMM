#ifndef   __VH_H__
#define   __VH_H__

#include <stdint.h>

typedef struct _mem_reg_t {
        uint64_t ipa;
        uint64_t pa;
        uint64_t size;
        uint64_t attribs;
} mem_reg_t;

typedef struct _vcpu_regs_t {
    /* x0-x30 */
    uint64_t x[31];
    uint64_t sp_el1;
    /* ELR(Exception Link Reg) */
    uint64_t elr_el2;
    /* SPSR(Saved Program Status Reg) */
    uint64_t spsr_el2;
} vcpu_regs_t;

typedef struct _vcpu {
    vcpu_regs_t regs;
} vcpu_t;

#define MAX_MEM_REGS    16

typedef struct _vm {
    uint32_t vmid;
    vcpu_t vcpu;
    mem_reg_t mem_regs[MAX_MEM_REGS];
    int num_mem_regs;
} vm_t;

#endif // __VH_H__
