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

/* UART */
void uart_init(void) { /* memory-mapped, no impl needed 4 qemu */ }
void uart_putc(char c) 
{ 
    if (c == '\n') { 
            UART0_DR = '\r'; 
    } 
    UART0_DR = c; 
}
void uart_puts(const char *s) 
{ 
    while (*s) 
        uart_putc(*s++); 
}
void uart_put_hex(uint64_t n) 
{
    char somedigits[] = "0123456789abcdef";
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uart_putc(somedigits[(n >> i) & 0xf]);
    }
}

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

/* stage 2 PT structure 
        for now we only need a single L1 tbl, as indicated
        by our VTCR_EL2 config */
static uint64_t __attribute__((aligned(4096))) __s2_l1_tbl[512];

static void guest_mmu_init(void)
{
    uart_puts("icevmm: setting up stage 2 MMU for guest...\n");

    /* map the first 1GB of IPA space (0x0-0x3FFFFFFF) to the first 1GB of PA space 
       this should cover guest main mem and the UART device */
    __s2_l1_tbl[0] = (0x000000000UL) | PTE_VALID | PTE_BLOCK | PTE_MEM_ATTR_IDX(1) | PTE_AF | PTE_SH_IS;

    /* map a another 1GB of IPA space (0x40000000-0x7FFFFFFF) to the second 1GB of PA space 
       covers the guest code loc */
    __s2_l1_tbl[1] = (0x400000000UL) | PTE_VALID | PTE_BLOCK | PTE_MEM_ATTR_IDX(1) | PTE_AF | PTE_SH_IS;
    
    __write_vtcr_el2(
        TCR_T0SZ(25)    |   /* 39-bit IPA, starts lookup @ L1 */
        TCR_PS_40_BIT   |   /* 40-bit PA                      */
        TCR_TG0_4K      |   /* 4KiB granule                   */
        TCR_SH0_IS      |   /* inner shareable                */
        TCR_ORGN0_WB    |   /* outer WB                       */
        TCR_IRGN0_WB        /* inner WB                       */
    );

    __write_vttbr_el2((uint64_t) __s2_l1_tbl);

    uart_puts("icevmm: guest stage 2 MMU configured !!!\n");
}

/* map first 2GB of phys mem */
static uint64_t __attribute__((aligned(4096))) pgd[512];
static uint64_t __attribute__((aligned(4096))) pud[2][512];

/* setup MMU */
static void mmu_init(void)
{
    /* map first 2GB with 1GB sects */
    pgd[0] = (uint64_t) pud[0] | PTE_VALID | PTE_TABLE;
    pgd[1] = (uint64_t) pud[1] | PTE_VALID | PTE_TABLE;

    pud[0][0] = 0x000000000 | PTE_VALID | PTE_BLOCK | PTE_MEM_ATTR_IDX(1) | PTE_AF | PTE_SH_IS;
    pud[0][1] = 0x40000000  | PTE_VALID | PTE_BLOCK | PTE_MEM_ATTR_IDX(1) | PTE_AF | PTE_SH_IS;

    /* set page table base addr */
    __write_ttbr0_el2((uint64_t) pgd);

    /* configure MAIR_EL2
            ATTR0-Device, ATTR1-Normal */
    uint64_t __mair_el2 = (MAIR_ATTR0_DEV << 0) | (MAIR_ATTR1_NORM << 8);
    __asm__ volatile("msr mair_el2, %0" : : "r"(__mair_el2));

    __write_vtcr_el2(
        TCR_T0SZ(25)    |
        TCR_PS_40_BIT   |
        TCR_TG0_4K      |
        TCR_SH0_IS      |
        TCR_ORGN0_WB    |
        TCR_IRGN0_WB
    );
    __asm__ volatile("isb");

    /* enable MMU, icache & dcache */
    uint64_t __sctlr_el2 = __read_sctlr_el2();
    /* icache, dcache, MMU */
    // __sctlr_el2 |= (1 << 12) | (1 << 2) | 1;
    __write_sctlr_el2(__sctlr_el2 | SCTLR_EL2_M | SCTLR_EL2_I | SCTLR_EL2_C);
    __asm__ volatile("isb");
}

/* configure core EL2 regs */
static void el2_setup(void)
{
    uart_puts("icevmm: configuring EL2\n");
    __write_vbar_el2((uint64_t) &__exception_vectors);

    uint64_t __hcr_el2 = HCR_EL2_RW_BIT;
    __write_hcr_el2(__hcr_el2);
    __write_cptr_el2(0);
    uart_puts("icevmm: written vectors to vbar_el2 reg\n");
    uart_puts("icevmm: EL2 configured !!!\n");
}

void vm_create(void)
{
    uart_puts("icevmm: creating vm...\n");
    guest_vm.vmid = 0;
    
    guest_mmu_init();

    /* must invalidate tlb after new stage 2 translation
       regime has been config'd */
    __tlbi_vmalle1();
    
    /* enable stage 2 translation */
    uint64_t hcr_el2 = HCR_EL2_RW_BIT | HCR_EL2_VM_BIT;
    __write_hcr_el2(hcr_el2);

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

    /* HVC handler */
    uint32_t exception_class = __esr >> 26;
    if (exception_class == 0x16) {  /* 0x16 HVC instr */
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