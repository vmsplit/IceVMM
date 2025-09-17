#include <stdint.h>

/* base addr of the PL011 UART in qemu's 'virt' machine */
#define UART0_DR    (*(volatile uint32_t*)0x09000000)

/* prototypes for assembly funcs */
unsigned long get_el(void);
void hang(void);

/*      read/write sysregs      */
void __write_tcr_el2  (uint64_t val);
void __write_hcr_el2  (uint64_t val);
void __write_cptr_el2 (uint64_t val);
void __write_ttbr0_el2(uint64_t val);
void __write_sctlr_el2(uint64_t val);
uint64_t __read_sctlr_el2(void);

/* PTDs */
/*      L1-3D descrip table      */
#define PTE_VALID     (1UL << 0)
#define PTE_TABLE     (1UL << 1)

/*      L1-2 descrip block      
          `-> PTE_MEM_ATTR_IDX(): MAIR idx 
          `-> PTE_AF: access flag
          `-> PTE_SH_IS: inner shareable
          `-> PTE_AP_RW_EL2: EL2 R/W      */
#define PTE_BLOCK     (0UL << 1)
#define PTE_MEM_ATTR_IDX(x) ((x) << 2)
#define PTE_AF        (1UL << 10)
#define PTE_SH_IS     (3UL << 8)
#define PTE_AP_RW_EL2 (0UL << 6)

/* mem attribs fo MAIR_EL2 
    `-> ATTR0: device-nGnRnE mem
    `-> ATTR1: normal, in/outer WB/WA/RA */
#define MAIR_ATTR0_DEV    (0x00)
#define MAIR_ATTR1_NORM   (0xff)

/* TCR_EL2 registers */
#define TCR_EL2_T0SZ(x)   ((x) & 0x3F)      /* TnSZ = 64 - x */
#define TCR_EL2_PS_40_BIT (2UL << 16)
#define TCR_EL2_TG0_4K    (0UL << 14)
#define TCR_EL2_SH0_IS    (3UL << 12)
#define TCR_EL2_ORGN0_WB  (1UL << 10)
#define TCR_EL2_IRGN0_WB  (1UL << 8)

/* HCR_EL2 bits 
    `-> HCR_EL2_RW_BIT: set EL1 to be arm4 */
#define HCR_EL2_RW_BIT (1UL << 31)

/* page tables
    create identity map for the first 2GB of mem
    L0 -> L1 -> 1GB block 
    L0 tbl (1entr) -> L1 tbl
    L1 tbl (2entr) -> 2 1GB blocks */
__attribute__((aligned(4096))) uint64_t l0_tbl[512];
__attribute__((aligned(4096))) uint64_t l1_tbl[512];

/* UART */
static void uart_init(void) { }
static void uart_putc(char c) { UART0_DR = c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
void uart_put_hex(uint64_t n) 
{
    char buf[17]; int i = 15;
    buf[16] = '\0';
    if (n == 0) { uart_puts("0x0"); return; }
    while (n > 0) {
        uint64_t d = n % 16;
        buf[i--] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        n /= 16;
    }
    uart_puts("0x"); uart_puts(&buf[i + 1]);
}

/* setup MMU */
static void mmu_init(void)
{
    /* L0 tbl -> L1 tbl 
           L0 entry covers the first 512GB of vaddr space 
           it's a ptr to our L1 tbl */
    l0_tbl[0] = PTE_VALID | PTE_TABLE | (uint64_t)l1_tbl;
    /* L1 tbl -> 2GB of phys main mem 
           create two 1GB block descripts for an identtiy map
           an L1 block descript maps a 1GB region

           VA 0x00_0000_0000 -> PA 0x00_0000_0000 (1GB) 
           covers our UART addr @ 0x09000000*/
    l0_tbl[0] = PTE_VALID | PTE_BLOCK | PTE_MEM_ATTR_IDX(1) | 
                   PTE_AF | PTE_SH_IS | PTE_AP_RW_EL2 | 0x00000000;
    /*     VA 0x00_2000_0000 -> PA 0x00_2000_0000 
           covers our hypv code in main mem */
    l1_tbl[1] = PTE_VALID | PTE_BLOCK | PTE_MEM_ATTR_IDX(1) | 
                   PTE_AF | PTE_SH_IS | PTE_AP_RW_EL2 | 0x40000000;

    /* configure TCR_EL2
           39-bit VA space (512GB), 4K granule, inner shareable, 
           WB cacheable, 40-bit PA space */
    uint64_t __tcr = TCR_EL2_T0SZ(25) | TCR_EL2_TG0_4K | TCR_EL2_SH0_IS | TCR_EL2_ORGN0_WB | 
                                      TCR_EL2_IRGN0_WB | TCR_EL2_PS_40_BIT;
    __write_tcr_el2(__tcr);

    /* configure MAIR_EL2
           ATTR0-Device, ATTR1-Normal
           NOTE: isb is just for synchronising */
    __asm__ volatile("msr mair_el2, %0" : : "r"((uint64_t) (MAIR_ATTR1_NORM << 8) | MAIR_ATTR0_DEV));
    /* set page table base addr */
    __write_ttbr0_el2((uint64_t) l0_tbl);
    __asm__ volatile("isb");
    /* enable MMU */
    uint64_t __sctlr = __read_sctlr_el2();
    __sctlr |= 1;       /* set 'M' bit to enable */
    __write_sctlr_el2(__sctlr);
    __asm__ volatile("isb");
}

/* configure core EL2 regs
        1. set EL1 to arm64
        2. set a known-clean state for sysctrls
        3. disable traps for SIMD/FP */
static void el2_setup(void)
{
    uart_puts("icevmm: configing EL2\n");
    /* 1 */
    __write_hcr_el2(HCR_EL2_RW_BIT);
    /* 2 */
    __write_sctlr_el2(0);
    /* 3 */
    __write_cptr_el2(0);
    uart_puts("icevmm: EL2 config'd");
}

/* C entrypoint, called from start.S */
void main(void)
{
    uart_init();
    uart_puts("distant meows from baremetal aarch64 !!!\n\n");

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

    uart_puts("icevmm: configing MMU\n");
    mmu_init();
    uart_puts("icevmm: MMU enabled !!!\n");

    hang();


    // /* do nothing for now */
    // while (1) { /* inf loop */ }
}