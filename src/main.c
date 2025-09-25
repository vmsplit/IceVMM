#include <stdint.h>
#include <stddef.h>
#include "vm.h"


/* linker exports */
extern char _guest_payload[];
extern char __exception_vectors[];


/* global VM obj */
// referenced by symbol name in exception.S
vm_t guest_vm;


/* UART */
#define UART0_DR    (*(volatile uint32_t*)0x09000000)
static void uart_putc(char c)
{
    if (c == '\n') UART0_DR = '\r';
    UART0_DR = c;
}
static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}
static void uart_put_hex(uint64_t n)
{
    static const char hexdigits[] = "0123456789abcdef";
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) uart_putc(hexdigits[(n >> i) & 0xf]);
}


/* asm func prototypes */
void hang(void);
unsigned long get_el(void);
void vcpu_run(vcpu_t *vcpu);
//  writes
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
           S1_PTE_MEM_ATTR(0) | S1_PTE_AP_RW_EL2 | S1_PTE_SH_IS |
           S1_PTE_AF;

    s1_l1_tbl[1] = 0x40000000 | S1_PTE_VALID     | S1_PTE_BLOCK |
           S1_PTE_MEM_ATTR(1) | S1_PTE_AP_RW_EL2 | S1_PTE_SH_IS |
           S1_PTE_AF;

    uint64_t __mair = (S1_MAIR_ATTR1_NORM << 8) | S1_MAIR_ATTR0_DEV;
    __write_mair_el2(__mair);

    uint64_t __tcr = S1_TCR_T0SZ(25) | S1_TCR_PS_40_BIT | S1_TCR_TG0_4K |
                     S1_TCR_SH0_IS   | S1_TCR_ORGN0_WB  | S1_TCR_IRGN0_WB;
    __write_tcr_el2(__tcr);
    __write_ttbr0_el2((uint64_t) s1_l1_tbl);
    __asm__ volatile("isb");
    uint64_t __sctlr = __read_sctlr_el2();
    __sctlr |= S1_SCTLR_M | S1_SCTLR_I | S1_SCTLR_C;
    __write_sctlr_el2(__sctlr);
    __asm__ volatile("isb");
}


/* stage 2 MMU for guest */
#define S2_PTE_VALID        (1UL << 0)
#define S2_PTE_TABLE        (1UL << 1)
#define S2_PTE_BLOCK        (0UL << 1)
#define S2_PTE_MEM_ATTR(x)  ((x) << 2)
#define S2_PTE_AP_RW        (3UL << 6)
#define S2_PTE_SH_IS        (3UL << 8)
#define S2_PTE_AF           (1UL << 10)
#define S2_MAIR_ATTR0_DEV   (0x04)
#define S2_MAIR_ATTR1_NORM  (0xff)
#define S2_VTCR_T0SZ(x)     ((x) & 0x3F)
#define S2_VTCR_PS_40_BIT   (2UL << 16)
#define S2_VTCR_TG0_4K      (0UL << 14)
#define S2_VTCR_SL0(x)      (((x) & 3) << 6)
#define S2_VTCR_ORGN0_WB    (1UL << 10)
#define S2_VTCR_IRGN0_WB    (1UL << 8)


static uint64_t __attribute__((aligned(4096))) s2_l1_tbl[512];

static void s2_mmu_init(vm_t *vm)
{
    /* clear s2 PT  */
    for (int i = 0; i < 512; i++) {
            s2_l1_tbl[i] = 0;
    }

    /* map mem regs for vm  */
    for (int i = 0; i < vm->num_mem_regs; i++) {
        mem_reg_t *region = &vm->mem_regs[i];
        uint64_t ipa  = region->ipa;
        uint64_t pa   = region->pa;
        uint64_t size = region->size;
        uint64_t attribs = region->attribs;

        /* for now use 1GB block mappings &
           assume ipa, pa, and size are 1GB aligned */
        uint64_t l1_idx = ipa >> 30;
        s2_l1_tbl[l1_idx] = pa | S2_PTE_VALID | S2_PTE_BLOCK | attribs;
    }

    // identity map the first 2GiB for guest
    //      GPA 0x00000000 -> PA 0x00000000 (device, for UART)
    //      GPA 0x40000000 -> PA 0x40000000 (normal, for code)
    // s2_l1_tbl[0] = 0x00000000 | S2_PTE_VALID | S2_PTE_BLOCK | S2_PTE_MEM_ATTR(0)
    //                           | S2_PTE_AP_RW | S2_PTE_SH_IS | S2_PTE_AF;
    // unmap first entry for now
    // s2_l1_tbl[0] = 0;
    // s2_l1_tbl[1] = 0x40000000 | S2_PTE_VALID | S2_PTE_BLOCK | S2_PTE_MEM_ATTR(1)
    //                           | S2_PTE_AP_RW | S2_PTE_SH_IS | S2_PTE_AF;

    uint64_t __vtcr = S2_VTCR_T0SZ(25) | S2_VTCR_PS_40_BIT | S2_VTCR_TG0_4K |
                      S2_VTCR_SL0(1)   | S2_VTCR_ORGN0_WB  | S2_VTCR_IRGN0_WB;
    __write_vtcr_el2(__vtcr);
    __write_vttbr_el2((uint64_t) s2_l1_tbl);
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

static void handle_mmio(vcpu_t *vcpu);
void handle_trap(vcpu_t *vcpu);


static void handle_mmio(vcpu_t *vcpu)
{
    uint64_t __esr = __read_esr_el2();
    uint64_t __far = __read_far_el2();
    uint32_t rt  = (__esr >> 5) & 0x1f;
    int is_write = (__esr & (1 << 6));

    if (__far == 0x09000000) {
            if (is_write) {
                    uart_putc((char) vcpu->regs.x[rt]);
            }
            else {
                    vcpu->regs.x[rt] = 0;
            }
        vcpu->regs.elr_el2 += 4;
        return;
    }
    else {
        uart_puts("icevmm: unhandled MMIO access for ");
        uart_put_hex(__far);
        uart_puts("\n");
        hang();
    }
    vcpu->regs.elr_el2 += 4;
}


void handle_trap(vcpu_t *vcpu)
{
    uint64_t __esr = __read_esr_el2();
    uint32_t ec = (__esr >> 26) & 0x3f;

    uart_puts("icevmm: trap:\n");
    uart_puts("  ESR_EL2[");
    uart_put_hex(__esr);
    uart_puts("]\n");

    trap_dump(__esr);

    /* 0x16 HVC from aarch64 */
    if (ec == ESR_EC_HVC64) {
            uart_puts("icevmm: guest called HVC (handled)\n");
            vcpu->regs.elr_el2 += 4;    /* move past HVC instr */
            return;
    }
    else if (ec == ESR_EC_INSTR_ABORT) {
            handle_mmio(vcpu);
    }
    else if (ec == ESR_EC_DATA_ABORT) {
            uint64_t __far = __read_far_el2();
            if (__far == 0x09000000) {
                    handle_mmio(vcpu);
                    return;
            }
    }
    else if (ec == ESR_EC_SYSREG) {
            uart_puts("icevmm: sysreg trap (not implemented)\n");
            hang();
    }

    uart_puts("icevmm: unhandled EC\n");
    hang();
}


/* vm setup */
#define HCR_EL2_RW_BIT (1UL << 31)
void vm_create(void)
{
    guest_vm.num_mem_regs = 2;

    /* dev mem | uart mem region */
    guest_vm.mem_regs[0].ipa  = 0x09000000;
    guest_vm.mem_regs[0].pa   = 0x09000000;
    guest_vm.mem_regs[0].size = 0x1000;
    guest_vm.mem_regs[0].attribs = S2_PTE_MEM_ATTR(S2_MAIR_ATTR0_DEV) | S2_PTE_AP_RW |
                                   S2_PTE_SH_IS | S2_PTE_AF;

    /* normal mem | guest code/data region  */
    guest_vm.mem_regs[1].ipa  = 0x40000000;
    guest_vm.mem_regs[1].pa   = 0x40000000;
    guest_vm.mem_regs[1].size = 0x10000000;
    guest_vm.mem_regs[1].attribs = S2_PTE_MEM_ATTR(S2_MAIR_ATTR1_NORM) | S2_PTE_AP_RW |
                                   S2_PTE_SH_IS | S2_PTE_AF;

    // /* set guest entrypoint to payload linked in hypv bin */
    // guest_vm.vcpu.regs.elr_el2 = (uint64_t) &_guest_payload;

    // /* setup guest stack ptr */
    // guest_vm.vcpu.regs.sp_el1 = 0x40080000;

    /* set SPSR: EL1h (run in el1, use SP_el1),
       mask all interrupts */
    guest_vm.vcpu.regs.spsr_el2 = (0x5);    /* PSTATE.M[4:0] = 0b00101 */
}

#define HCR_EL2_RW   (1UL << 31)
#define HCR_EL2_VM   (1UL << 0)
#define HCR_EL2_IMO  (1UL << 4)
#define HCR_EL2_DEFAULT (HCR_EL2_VM | HCR_EL2_RW | HCR_EL2_IMO)
/* main hypv entrypoint */
void main(void)
{
    uart_puts("\nicevmm: distant meows from baremetal aarch64 !!!\n");

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
    vm_create();
    uart_puts("icevmm: guest created.\n");

    /* setup & enable stage 2 MMU for guest */
    uart_puts("icevmm: enabling S2 MMU...\n");
    s2_mmu_init(&guest_vm);
    uart_puts("icevmm: S2 MMU enabled !!!\n");

    /* launch guest */
    uart_puts("icevmm: running vm...\n");
    vcpu_run(&guest_vm.vcpu);

    /* i mean this shouldn't ever be reached but here anyways */
    uart_puts("icevmm: vcpu_run returned unexpectedly!\n");
    hang();
}
