#ifndef   __VH_H__
#define   __VH_H__

#include <stdint.h>

/* s2 VTCR_EL2 cfg */
#define S2_VTCR_T0SZ(x)         (((x) & 0x3f) << 0)
#define S2_VTCR_SL0(x)          (((x) & 0x3) << 6)
#define S2_VTCR_IRGN0(x)        (((x) & 0x3) << 8)
#define S2_VTCR_ORGN0(x)        (((x) & 0x3) << 10)
#define S2_VTCR_SH0(x)          (((x) & 0x3) << 12)
#define S2_VTCR_TG0(x)          (((x) & 0x3) << 14)  /* granule size */
#define S2_VTCR_PS(x)           (((x) & 0x7) << 16)  /* paddr size */

/* S2 MAIR_EL2 attributes */
#define S2_MAIR_ATTR(idx, val)  ((val) << ((idx) * 8))
#define S2_MAIR_ATTR0_DEV       (0x04)
#define S2_MAIR_ATTR1_NORM      (0xff)

/* S2 PTE attributes */
#define S2_PTE_VALID        (1UL << 0)
#define S2_PTE_TABLE        (1UL << 1)  /* block/page bit is 0 */
#define S2_PTE_PAGE         (1UL << 1)  /* block/table bit is 1 */
#define S2_PTE_BLOCK        (0UL << 1)
#define S2_PTE_MEM_ATTR(x)  (((x) & 0xf) << 2)
#define S2_PTE_S2AP(x)      (((x) & 0x3) << 6)
#define S2_PTE_S2AP_WO      0
#define S2_PTE_S2AP_RO      1
#define S2_PTE_S2AP_WO2     2 // write-only?
#define S2_PTE_S2AP_RW      3
#define S2_PTE_SH(x)        (((x) & 0x3) << 8)
#define S2_PTE_SH_NS        0
#define S2_PTE_SH_OS        2
#define S2_PTE_SH_IS        3
#define S2_PTE_AF           (1UL << 10)

typedef enum _vcpu_state {
    VCPU_STATE_RUNNING,
    VCPU_STATE_RUNNABLE,
    VCPU_STATE_BLOCKED,
} vcpu_state_t;

typedef struct _mem_reg_t {
    uint64_t ipa;
    uint64_t pa;
    uint64_t size;
    uint64_t attribs;
} mem_reg_t;

typedef struct _vcpu_regs_t {
    /* x0-x30 */
    uint64_t x[31];
    /* ELR(Exception Link Reg) */
    uint64_t elr_el2;
    /* SPSR(Saved Program Status Reg) */
    uint64_t spsr_el2;
    uint64_t sp_el1;
} vcpu_regs_t;

typedef struct _vcpu {
    uint32_t     vcpu_id;
    vcpu_regs_t  regs;
    vcpu_state_t state;
    struct _vm   *vm;
} vcpu_t;

#define MAX_MEM_REGS    16
#define MAX_VCPUS       1

typedef struct _vm {
    uint32_t  vmid;
    vcpu_t    vcpus[MAX_VCPUS];
    mem_reg_t mem_regs[MAX_MEM_REGS];
    int       num_mem_regs;
} vm_t;

// This function is defined in main.c but used by the scheduler
void vcpu_run(vcpu_t *vcpu);

#endif // __VH_H__
