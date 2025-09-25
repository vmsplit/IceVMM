#include <stdint.h>
#include <stddef.h>
#include "../include/uart.h"
#include "../include/vm.h"
#include "../include/mm.h"
#include "../include/sched.h"
#include "../include/timer.h"


/* linker exports */
extern char _guest_bin_start[];
extern char _guest_bin_end[];
// extern char __exception_vectors[];


/* global VM obj */
// referenced by symbol name in exception.S
vm_t guest_vm;


/* asm func prototypes */
void hang(void);
unsigned long get_el(void);
void vcpu_run(vcpu_t *vcpu);
//  writesa
void __write_sctlr_el2(uint64_t val);
void __write_hcr_el2(uint64_t val);
void __write_cptr_el2(uint64_t val);
void __write_vbar_el2(uint64_t val);
void __write_tcr_el2(uint64_t val);
void __write_ttbr0_el2(uint64_t val);
void __write_vttbr_el2(uint64_t val);
void __write_vtcr_el2(uint64_t val);
void __write_mair_el2(uint64_t val);
//  reads
uint64_t __read_sctlr_el2(void);
uint64_t __read_esr_el2(void);
uint64_t __read_far_el2(void);
//  tlb stuff
void __tlbi_vmalle1(void);  /* flush TLB*/


/* stage 1 MMU for hypv */
#define S1_PTE_VALID        (1UL << 0)
#define S1_PTE_TABLE        (1UL << 1)
#define S1_PTE_BLOCK        (0UL << 1)
#define S1_PTE_MEM_ATTR(x)  ((x) << 2)
#define S1_PTE_AP_RW_EL2    (0UL << 6)
#define S1_PTE_SH_IS        (3UL << 8)
#define S1_PTE_AF           (1UL << 10)
#define S1_MAIR_ATTR0_DEV   (0x04)
#define S1_MAIR_ATTR1_NORM  (0xff)
#define S1_TCR_T0SZ(x)      ((x) & 0x3F)
#define S1_TCR_PS_40_BIT    (2UL << 16)
#define S1_TCR_TG0_4K       (0UL << 14)
#define S1_TCR_SH0_IS       (3UL << 12)
#define S1_TCR_ORGN0_WB     (1UL << 10)
#define S1_TCR_IRGN0_WB     (1UL << 8)
#define S1_SCTLR_M          (1UL << 0)
#define S1_SCTLR_C          (1UL << 2)
#define S1_SCTLR_I          (1UL << 12)


static uint64_t __attribute__((aligned(4096))) s1_l1_tbl[512];


static void s1_mmu_init(void)
{
    s1_l1_tbl[0] = 0x00000000 | S1_PTE_VALID     | S1_PTE_BLOCK |
           S1_PTE_MEM_ATTR(0) | S1_PTE_AP_RW_EL2 | S1_PTE_SH_IS | S1_PTE_AF;

    s1_l1_tbl[1] = 0x40000000 | S1_PTE_VALID     | S1_PTE_BLOCK |
           S1_PTE_MEM_ATTR(1) | S1_PTE_AP_RW_EL2 | S1_PTE_SH_IS | S1_PTE_AF;

    s1_l1_tbl[2] = 0x80000000 | S1_PTE_VALID     | S1_PTE_BLOCK |
           S1_PTE_MEM_ATTR(1) | S1_PTE_AP_RW_EL2 | S1_PTE_SH_IS | S1_PTE_AF;

    uint64_t __mair = (S1_MAIR_ATTR1_NORM << 8) | S1_MAIR_ATTR0_DEV;
    __write_mair_el2(__mair);

    uint64_t __tcr = S1_TCR_T0SZ(25) | S1_TCR_PS_40_BIT | S1_TCR_TG0_4K |
                       S1_TCR_SH0_IS | S1_TCR_ORGN0_WB  | S1_TCR_IRGN0_WB;
    __write_tcr_el2(__tcr);
    __write_ttbr0_el2((uint64_t) s1_l1_tbl);
    __asm__ volatile("dsb sy");
    __asm__ volatile("isb");
    uint64_t __sctlr = __read_sctlr_el2();
    __sctlr |= S1_SCTLR_M | S1_SCTLR_I | S1_SCTLR_C;
    __write_sctlr_el2(__sctlr);
    __asm__ volatile("isb");
}


/* stage 2 MMU for guest */
static uint64_t *s2_l1_tbl;
#define PTE_PER_TBL     512
#define PAGE_SZ        4096


static void s2_map(uint64_t ipa, uint64_t pa, uint64_t attr)
{
    uint64_t *l2_tbl, *l3_tbl;
    uint64_t l1_idx, l2_idx, l3_idx;
    uint64_t pte;

    l1_idx = (ipa >> 30) & 0x1ff;
    l2_idx = (ipa >> 21) & 0x1ff;
    l3_idx = (ipa >> 12) & 0x1ff;

    /* l1 */
    pte = s2_l1_tbl[l1_idx];
    if ((pte & S2_PTE_VALID) == 0) {
            l2_tbl = palloc();
            pte = (uint64_t) l2_tbl | S2_PTE_VALID | S2_PTE_TABLE;
            s2_l1_tbl[l1_idx] = pte;
    }
    else {
            l2_tbl = (uint64_t *) (pte & 0x0000fffffffff000);
    }

    /* l2 */
    pte = l2_tbl[l2_idx];
    if ((pte & S2_PTE_VALID) == 0) {
            l3_tbl = palloc();
            pte = (uint64_t) l3_tbl | S2_PTE_VALID | S2_PTE_TABLE;
            l2_tbl[l2_idx] = pte;
    }
    else {
            l3_tbl = (uint64_t *) (pte & 0x0000fffffffff000);
    }

    /* l3 */
    pte = pa | S2_PTE_VALID | S2_PTE_PAGE
             | S2_PTE_AF    | attr;
    l3_tbl[l3_idx] = pte;
}


static void s2_mmu_init(vm_t *vm)
{
    /* alloc root l1 PT  */
    s2_l1_tbl = palloc();
    /* setup MAIR_EL2  */
    uint64_t __mair = S2_MAIR_ATTR(0, S2_MAIR_ATTR0_DEV) |
                      S2_MAIR_ATTR(1, S2_MAIR_ATTR1_NORM);
    __write_mair_el2(__mair);

    // identity map the first 2GiB for guest
    //      GPA 0x00000000 -> PA 0x00000000 (device, for UART)
    //      GPA 0x40000000 -> PA 0x40000000 (normal, for code)
    // s2_l1_tbl[0] = 0x00000000 | S2_PTE_VALID | S2_PTE_BLOCK | S2_PTE_MEM_ATTR(0)
    //                           | S2_PTE_AP_RW | S2_PTE_SH_IS | S2_PTE_AF;
    // unmap first entry for now
    // s2_l1_tbl[0] = 0;
    // s2_l1_tbl[1] = 0x40000000 | S2_PTE_VALID | S2_PTE_BLOCK | S2_PTE_MEM_ATTR(1)
    //                           | S2_PTE_AP_RW | S2_PTE_SH_IS | S2_PTE_AF;

    uint64_t __vtcr = S2_VTCR_PS(1)         |
                      S2_VTCR_TG0(0)        |
                      S2_VTCR_SH0(3)        |
                      S2_VTCR_ORGN0(1)      |
                      S2_VTCR_IRGN0(1)      |
                      S2_VTCR_SL0(1)        |
                      S2_VTCR_T0SZ(24);
    __write_vtcr_el2(__vtcr);
    /* set baddr of l1 tbl  */
    __write_vttbr_el2((uint64_t) s2_l1_tbl);

    /* map all mem regs for vm */
    for (int i = 0; i < vm->num_mem_regs; i++) {
            mem_reg_t *region = &vm->mem_regs[i];
            for (uint64_t offset = 0; offset < region->size; offset += PAGE_SZ) {
                    s2_map(region->ipa + offset, region->pa +
                        offset, region->attribs);
            }
    }

    __tlbi_vmalle1();
}


/* ESR_EL2 EC values we care about */
#define ESR_EC_UNKNOWN        0x00
#define ESR_EC_WFI_WFE        0x01
#define ESR_EC_SVC64          0x15
#define ESR_EC_HVC64          0x16
#define ESR_EC_SMC64          0x17
#define ESR_EC_SYSREG         0x18
#define ESR_EC_INSTR_ABORT    0x20
#define ESR_EC_DATA_ABORT     0x24


static const char *esr_ec_str(uint32_t ec)
{
    switch (ec) {
    case ESR_EC_UNKNOWN:     return "Unknown";
    case ESR_EC_WFI_WFE:     return "WFI/WFE";
    case ESR_EC_SVC64:       return "SVC (AArch64)";
    case ESR_EC_HVC64:       return "HVC (AArch64)";
    case ESR_EC_SMC64:       return "SMC (AArch64)";
    case ESR_EC_SYSREG:      return "MSR/MRS (sysreg)";
    case ESR_EC_INSTR_ABORT:  return "Instruction Abort (EL1)";
    case ESR_EC_DATA_ABORT:  return "Data Abort (EL1)";
    default:                 return "Unhandled/Unknown EC";
    }
}


/* trap handling */
static void trap_dump(uint64_t __esr)
{
    uint32_t ec  = (__esr >> 26) & 0x3f;
    uint32_t iss = __esr & 0x01ffffff;
    uart_puts("  reason: ");
    uart_puts(esr_ec_str(ec));
    uart_puts("\n  EC[0x");
    uart_put_hex(ec);
    uart_puts("]\n  ISS[0x");
    uart_put_hex(iss);
    uart_puts("]\n");
    if (ec == ESR_EC_DATA_ABORT || ec == ESR_EC_INSTR_ABORT) {
        uint64_t __far = __read_far_el2();
        uart_puts("  FAR_EL2[0x");
        uart_put_hex(__far);
        uart_puts("]\n");
    }
}

static void handle_mmio(vcpu_regs_t *regs);
void handle_trap(vcpu_regs_t *regs);


static void handle_mmio(vcpu_regs_t *regs)
{
    uint64_t __esr = __read_esr_el2();
    uint64_t __far = __read_far_el2();
    uint32_t rt = (__esr >> 5) & 0x1f;
    int is_write = (__esr & (1 << 6));

    if (__far == 0x09000000) {
            if (is_write) {
                    uart_putc((char) regs->x[rt]);
            }
            else {
                    regs->x[rt] = 0;
            }
        regs->elr_el2 += 4;
        return;
    }
    else {
            uart_puts("icevmm: unhandled MMIO access for ");
            uart_put_hex(__far);
            uart_puts("\n");
            hang();
    }
}

// void handle_irq(vcpu_regs_t *regs)
// {
//     sched(regs);
// }

void handle_trap(vcpu_regs_t *regs)
{
    uint64_t __esr = __read_esr_el2();
    uint32_t ec = (__esr >> 26) & 0x3f;

    uart_puts("icevmm: trap:\n");
    uart_puts("  ESR_EL2[");
    uart_put_hex(__esr);
    uart_puts("]\n");

    trap_dump(__esr);

    /* 0x16 HVC from aarch64 */
    // if (ec == ESR_EC_HVC64) {
    //         uart_puts("icevmm: guest called HVC (handled)\n");
    //         vcpu->regs.elr_el2 += 4;    /* move past HVC instr */
    //         return;
    // }
    if (ec == ESR_EC_DATA_ABORT || ec == ESR_EC_INSTR_ABORT) {
            handle_mmio(regs);
            return;
    }
    // else if (ec == ESR_EC_INSTR_ABORT) {
    //         handle_mmio(vcpu);
    // }
    // else if (ec == ESR_EC_DATA_ABORT) {
    //         uint64_t __far = __read_far_el2();
    //         if (__far == 0x09000000) {
    //                 handle_mmio(vcpu);
    //                 return;
    //         }
    // }
    else if (ec == ESR_EC_SYSREG) {
            uart_puts("icevmm: sysreg trap (not implemented)\n");
            hang();
    }

    uart_puts("icevmm: unhandled EC\n");
    hang();
}


/* vm setup */
void create_guest_vm(void)
{
    /* vm's mem regs  */
    guest_vm.vmid = 0;
    guest_vm.num_mem_regs = 2;

    /* dev mem | uart mem region */
    guest_vm.mem_regs[0].ipa  = 0x09000000;
    guest_vm.mem_regs[0].pa   = 0x09000000;
    guest_vm.mem_regs[0].size = 0x1000;
    guest_vm.mem_regs[0].attribs = S2_PTE_MEM_ATTR(0) | S2_PTE_S2AP(S2_PTE_S2AP_RW);

    /* normal mem | guest code/data region  */
    // uint64_t guest_mem_sz = 0x10000;
    uint64_t guest_mem_pa = (uint64_t) palloc(); /* get one page */
    guest_vm.mem_regs[1].ipa  = 0x40000000;
    guest_vm.mem_regs[1].pa   = guest_mem_pa;
    guest_vm.mem_regs[1].size = PAGE_SZ;
    guest_vm.mem_regs[1].attribs = S2_PTE_MEM_ATTR(1) | S2_PTE_S2AP(S2_PTE_S2AP_RW);

    // /* set guest entrypoint to payload linked in hypv bin */
    // guest_vm.vcpu.regs.elr_el2 = guest_vm.mem_regs[1].ipa;
    vcpu_t *vcpu = &guest_vm.vcpus[0];
    vcpu->vcpu_id = 0;
    vcpu->vm = &guest_vm;
    vcpu->regs.elr_el2 = guest_vm.mem_regs[1].ipa;
    vcpu->regs.spsr_el2 = 0x3c5;

    /* setup guest payload for guest's mem  */
    char  *_payload_start = _guest_bin_start;
    char  *_payload_end   = _guest_bin_end;
    size_t _payload_sz    = _payload_end - _payload_start;

    uart_puts("icevmm: loading guest payload !!! size=");
    uart_put_hex(_payload_sz);
    uart_puts(" bytes...\n");

    memcpy((void *) guest_mem_pa, _payload_start, _payload_sz);

    // /* setup guest stack ptr */
    // guest_vm.vcpu.regs.sp_el1 = 0x40080000;

    /* set SPSR: EL1h (run in el1, use SP_el1),
       mask all interrupts */
    // guest_vm.vcpu.regs.spsr_el2 = 0x3c5;
}

void dump_registers_and_hang(uint64_t __esr, uint64_t __elr, uint64_t __far)
{
    uart_puts("FATAL HYPERVISOR EXCEPTION:\n");
    uart_puts(" ESR_EL2: 0x");
    uart_put_hex(__esr);
    uart_puts("\n ELR_EL2: 0x");
    uart_put_hex(__elr);
    uart_puts("\n FAR_EL2: 0x");
    uart_put_hex(__far);
    uart_puts("\n");

    // Infinite loop to hang the core
    while (1) {}
}

#define HCR_EL2_RW   (1UL << 31)
#define HCR_EL2_VM   (1UL << 0)
#define HCR_EL2_IMO  (1UL << 4) // Trap virtual IRQs
#define HCR_EL2_DEFAULT (HCR_EL2_VM | HCR_EL2_RW | HCR_EL2_IMO)
/* main hypv entrypoint */
void main(void)
{
    extern char __exception_vectors;
    uart_init();
    uart_puts("\nicevmm: distant meows from baremetal aarch64 !!!\n");

    palloc_init();

    unsigned long el = get_el();
    if (el != 2) hang();
    uart_puts("icevmm: running in EL2\n");

    /* cfg hypv controls */
    __write_vbar_el2((uint64_t)&__exception_vectors);
    __write_hcr_el2(HCR_EL2_DEFAULT);
    __write_cptr_el2(0); /* disable simd/fp traps to el2 */

    /* setup & enable stage 1 MMU for hypv */
    uart_puts("icevmm: enabling S1 MMU...\n");
    s1_mmu_init();
    uart_puts("icevmm: S1 MMU enabled !!!\n");

    /* create the actual guest VM */
    create_guest_vm();
    uart_puts("icevmm: guest created.\n");

    /* setup & enable stage 2 MMU for guest */
    uart_puts("icevmm: enabling S2 MMU...\n");
    s2_mmu_init(&guest_vm);
    uart_puts("icevmm: S2 MMU enabled !!!\n");

    sched_init();
    sched_add_vcpu(&guest_vm.vcpus[0]);

    timer_init();

    uart_puts("icevmm: running scheduler...\n");
    sched(NULL);

    // /* launch guest */
    // vcpu_run(&guest_vm.vcpu);

    /* i mean this shouldn't ever be reached but here anyways */
    uart_puts("icevmm: scheduler returned unexpectedly!\n");
    hang();
}
