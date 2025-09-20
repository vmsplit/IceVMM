#include <stdint.h>

#include "vm.h"

const uint32_t _guest_payload[] = {
    0x580000a1,     /* ldr x1, #20   */
    0x52800922,     /* mov w2, #'G'  */
    0x39000022,     /* strb w2, [x1] */
    0xd4000002,     /* hvc #0        */
    0x14000000,     /* b .           */
    0x09000000,     /* literal for UART addr */
};

/* base addr of the PL011 UART in qemu's 'virt' machine */
#define UART0_DR    (*(volatile uint32_t*)0x09000000)

/* linker exports */
extern uint64_t __stack_top;
extern uint64_t __text_start,   __text_end;
extern uint64_t __rodata_start, __rodata_end;
extern uint64_t __data_start,   __data_end;
extern uint64_t __bss_start,    __bss_end;

/* structs */
vm_t guest_vm;

/* prototypes */
void uart_init(void);
void uart_puts(const char *s);
void uart_putc(char c);
void uart_put_hex(uint64_t n);

/* prototypes for assembly funcs */
unsigned long get_el(void);
void hang(void);
void vcpu_run(vcpu_t *vcpu);
extern void __exception_vectors(void);
// uint64_t va_2_pa_el2(uint64_t va);

/*      write sysregs      */
void __write_vbar_el2  (uint64_t val);
void __write_tcr_el2   (uint64_t val);
void __write_hcr_el2   (uint64_t val);
void __write_cptr_el2  (uint64_t val);
void __write_ttbr0_el2 (uint64_t val);
void __write_sctlr_el2 (uint64_t val);
void __write_vttbr_el2 (uint64_t val);
void __write_vtcr_el2  (uint64_t val);

/*      tlb flush/invalidate      */
void __tlbi_vmalle1(void);

/*      read sysregs       */
uint64_t __read_sctlr_el2(void);

/* PT data . stage 1 (for hypv) */
#define PTE_VALID             (1UL << 0)
#define PTE_TABLE             (1UL << 1)
#define PTE_BLOCK             (0UL << 1)
#define PTE_MEM_ATTR_IDX(x)   ((x) << 2)
#define PTE_AF                (1UL << 10)
#define PTE_SH_IS             (3UL << 8)
#define PTE_AP_RW_EL2         (0UL << 6)

/* mem attribs for MAIR_EL2 
    `--> ATTR0: device-nGnRE mem
    `--> ATTR1: normal, in/outer WB/WA/RA */
#define MAIR_ATTR0_DEV        (0x04)
#define MAIR_ATTR1_NORM       (0xff)

/* TCR_EL2 registers */
#define TCR_T0SZ(x)           ((x) & 0x3F)
#define TCR_PS_40_BIT         (2UL << 16)
#define TCR_TG0_4K            (0UL << 14)
#define TCR_SH0_IS            (3UL << 12)
#define TCR_ORGN0_WB          (1UL << 10)
#define TCR_IRGN0_WB          (1UL << 8)

/* HCR_EL2 bit to enable stage 2 translation 
   for guest*/
#define HCR_EL2_RW_BIT        (1UL << 31)
#define HCR_EL2_VM_BIT        (1UL << 0)
// #define HCR_EL2_TGE_BIT (1UL << 27)
// #define HCR_EL2_TWI_BIT (1UL << 20)

/* SCTLR_EL2 bits */
#define SCTLR_EL2_M           (1UL << 0)
#define SCTLR_EL2_C           (1UL << 2)
#define SCTLR_EL2_I           (1UL << 12)

/* stage 2 guest PT descripts */
#define PTE_S2_VALID          (1UL << 0)
#define PTE_S2_TABLE          (1UL << 1)
#define PTE_S2_BLOCK          (0UL << 1)
#define PTE_S2_AP_RW          (3UL << 6)
#define PTE_S2_SH_IS          (3UL << 8)
#define PTE_S2_AF             (1UL << 10)
#define PTE_S2_MEMATTR_IDX(x) ((x) << 2)


/* VTCR_EL2 s2 translation ctrl bits */
#define VTCR_EL2_T0SZ(x)      ((x) & 0x3F)
#define VTCR_EL2_PS_40_BIT    (2UL << 16)
#define VTCR_EL2_TG0_4K       (0UL << 14)
#define VTCR_EL2_SH0_IS       (3UL << 12)
#define VTCR_EL2_ORGN0_WB     (1UL << 10)
#define VTCR_EL2_IRGN0_WB     (1UL << 8)

/* hypv s1 PTs and guest s2 PTs 
   giving at least 1GB of mem */
__attribute__((section(".page_tables"), aligned(4096))) uint64_t hyp_l1_tbl[512];
__attribute__((section(".page_tables"), aligned(4096))) uint64_t guest_l1_pt[512];
/* UART */
void uart_init(void) { /* memory-mapped, no impl needed 4 qemu */ }
void uart_putc(char c) { if (c == '\n') { UART0_DR = '\r'; } UART0_DR = c; }
void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
void uart_put_hex(uint64_t n) 
{
    char somedigits[] = "0123456789abcdef";
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uart_putc(somedigits[(n >> i) & 0xf]);
    }
}

/* setup MMU */
static void mmu_init(void)
{
    /* i hope this formatting hurts your eyes */
    hyp_l1_tbl[0] = PTE_VALID | PTE_BLOCK | PTE_MEM_ATTR_IDX(0) |   /* ATTR0 -> Device */
                       PTE_AF | PTE_SH_IS | PTE_AP_RW_EL2 | 0x00000000;
    hyp_l1_tbl[1] = PTE_VALID | PTE_BLOCK | PTE_MEM_ATTR_IDX(1) |   /* ATTR1 -> Normal */
                       PTE_AF | PTE_SH_IS | PTE_AP_RW_EL2 | 0x40000000;

    /* configure TCR_EL2
           39-bit VA space (512GB), 4K granule, inner shareable,
           WB cacheable, 40-bit PA space */
    uint64_t __tcr = TCR_EL2_T0SZ(25) | TCR_EL2_TG0_4K    | TCR_EL2_SH0_IS   | 
                     TCR_EL2_ORGN0_WB |  TCR_EL2_IRGN0_WB | TCR_EL2_PS_40_BIT;
    __write_tcr_el2(__tcr);

    /* configure MAIR_EL2
           ATTR0-Device, ATTR1-Normal */
    __asm__ volatile("msr mair_el2, %0" : : "r"((uint64_t) (MAIR_ATTR1_NORM << 8) | MAIR_ATTR0_DEV));

    /* set page table base addr */
    __write_ttbr0_el2((uint64_t) hyp_l1_tbl);

    /* ensure all page table writes and config
       are actually committed */
    __asm__ volatile("dsb sy");
    __asm__ volatile("isb");

    /* enable MMU, icache & dcache */
    uint64_t __sctlr = __read_sctlr_el2();
    /* icache, dcache, MMU */
    __sctlr |= (1 << 12) | (1 << 2) | 1;
    __write_sctlr_el2(__sctlr);
    __asm__ volatile("isb");
}

static void guest_mmu_init(void)
{
    uart_puts("icevmm: setting up stage 2 MMU for guest...\n");

    /* ok so we give guest just a simple 1:1 identity mapping for 
       the first 1GB
       
       this will require at least 2MB block entries in our L2 tbl
       IPA 0x00000000 -> PA 0x00000000 (covers UART @ 0x09000000) 
       IPA 0x40000000 -> PA 0x40000000 (covers our code @ 0x4000xxxx) */

    /* map the first 2MB block as device mem for UART */
    guest_l1_pt[0] = PTE_S2_VALID | PTE_S2_BLOCK | PTE_S2_MEMATTR_IDX(0) |  /* ATTR0 -> Device */
                     PTE_S2_AP_RW | PTE_S2_SH_IS | PTE_S2_AF | 0x00000000;

    /* map a another 2MB block where our hypv code is in normal mem 
       we can calc the L2 tbl index for addr 0x40000000 
       0x40000000 >> 21 = 512 */
    guest_l1_pt[1] = PTE_S2_VALID | PTE_S2_BLOCK | PTE_S2_MEMATTR_IDX(1) |  /* ATTR1 -> Normal */
                     PTE_S2_AP_RW | PTE_S2_SH_IS | PTE_S2_AF | 0x40000000;
    
    /* config VTCR_EL2 */
    uint64_t __vtcr = VTCR_EL2_T0SZ(25) | VTCR_EL2_TG0_4K   | VTCR_EL2_SH0_IS   |
                    VTCR_EL2_ORGN0_WB   | VTCR_EL2_IRGN0_WB | VTCR_EL2_PS_40_BIT;
    __write_vtcr_el2(__vtcr);

    /* set the s2 PT base addr */
    __write_vttbr_el2((uint64_t) guest_l1_pt);

    /* invalidate all tlb entries for guest VMID */
    __tlbi_vmalle1();
    
    uart_puts("icevmm: guest stage 2 MMU configured !!!\n");
}

/* test func
        for testing mmu purpooses only  */

// static void mmu_test(void)
// {
//     uart_puts("icevmm: mmu translation test\n");

//     uint64_t va_uart = 0x09000000;
//     uint64_t pa_uart = va_2_pa_el2(va_uart);
//     uart_puts("     VA[UART]: "); uart_put_hex(va_uart);
//     uart_puts("       \n      `------> PA["); uart_put_hex(pa_uart); uart_puts("]\n");

//     uint64_t va_code = 0x40020000;
//     uint64_t pa_code = va_2_pa_el2(va_code);
//     uart_puts("     VA[CODE]: "); uart_put_hex(va_code);
//     uart_puts("       \n      `------> PA["); uart_put_hex(pa_code); uart_puts("]\n");

//     uint64_t va_bad = 0x8000000000; /* not mapped in our addr space, but it's valid */
//     uint64_t pa_bad = va_2_pa_el2(va_bad);
//     uart_puts("     VA[BAD]:  "); uart_put_hex(va_bad);
//     uart_puts("       \n      `------> PA["); uart_put_hex(pa_bad); uart_puts("]\n");

//     if (pa_bad & 1) {
//             uart_puts("icevmm: translation for VA[BAD] correctly failed !!!\n");
//     }

//     uart_puts("icevmm: end test...\n");
// }




/* configure core EL2 regs */
static void el2_setup(void)
{
    uart_puts("icevmm: configuring EL2\n");
    uint64_t __hcr = HCR_EL2_RW_BIT | HCR_EL2_VM_BIT;
    __write_hcr_el2(__hcr);
    __write_cptr_el2(0);
    __write_vbar_el2((uint64_t) __exception_vectors);
    uart_puts("icevmm: written vectors to vbar_el2 reg\n");
    uart_puts("icevmm: EL2 configured !!!\n");
}

static void vm_create(void)
{
    uart_puts("icevmm: creating vm...\n");

    /* fucking hope this works.. */
    guest_mmu_init();

    /* initialise vcpu regs and setup the SPSR_EL2
       which determines the state of the cpu when we
       eret to guest
       
       also set guest's stack ptr, give it the top
       of our stack for the moment.. */
    vcpu_t *vcpu = &guest_vm.vcpu;
    vcpu->regs.elr_el2 = (uint64_t) _guest_payload;
    vcpu->regs.spsr_el2 = 0x3c5;    /* EL1h, all interrupts masked */
    vcpu->regs.sp_el1 =  (uint64_t) __stack_top;
}

void handle_trap(vcpu_t *vcpu)
{
    uint64_t __esr;
    __asm__ volatile("mrs %0, esr_el2" : "=r"(__esr));

    uart_puts("\n!!! TRAP HANDLER INVOKED !!!\n");
    uart_puts("     ESR_EL2[");
    uart_put_hex(__esr);
    uart_puts("]\n");

    /* HVC handler (finally) */
    uint32_t exception_class = __esr >> 26;
    if (exception_class == 0x16) {
            vcpu->regs.elr_el2 += 4;
    } else {
            uart_puts("icevmm:      unhandled exception. halting !!!\n");
            hang();
    }
}

/* C entrypoint, called from start.S */
void main(void)
{
    uart_init();
    uart_puts("\nicevmm: distant meows from baremetal aarch64 !!!\n");
    unsigned long __el = get_el();
    uart_puts("icevmm: current EL: ");
    uart_put_hex(__el);
    uart_puts("\n");
    /* check if the returned EL is not 2 
    ...it should be */
    if (__el != 2) {
            uart_puts("icevmm:      not running in EL2. halting!!!\n");
            hang();
    }
    uart_puts("icevmm: running in EL2\n");
    el2_setup();
    uart_puts("icevmm: enabling MMU...\n");
    mmu_init();
    uart_puts("icevmm: MMU enabled !!!\n");
    vm_create();
    uart_puts("icevmm: running vm...\n");
    vcpu_run(&guest_vm.vcpu);
    hang();
}