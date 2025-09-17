#include <stdint.h>

/* base addr of the PL011 UART in qemu's 'virt' machine */
#define UART0_DR (*(volatile uint32_t*)0x09000000)

/* prototypes for assembly funcs */
unsigned long get_el(void);
void hang(void);
void __write_hcr_el2  (uint64_t val);
void __write_cptr_el2 (uint64_t val);
void __write_sctlr_el2(uint64_t val);

/* HCR_EL2 bits 
    `-> HCR_EL2_RW_BIT: set EL1 to be arm64 */
#define HCR_EL2_RW_BIT (1UL << 31)

static void uart_init(void) { }

/* write a single char to UART */
static void uart_putc(char c)
{
    UART0_DR = c;
}

/* write a null-terminated string to UART */
static void uart_puts(const char *s)
{
    while (*s) {
            uart_putc(*s++);
    }
}

/* print an UL as hex string */
void uart_put_hex(unsigned long n)
{
    char buf[17];
    int i = 15;
    buf[16] = '\0';

    if (n == 0) {
            uart_putc('0');
            return;
    }

    /* write '0x' prefix */
    int start_idx = 0;
    buf[start_idx++] = '0';
    buf[start_idx++] = 'x';
    i = 15;     /* reset index */

    while (n > 0) {
        unsigned long digit = n % 16;
        if (digit < 10) {
                buf[i] = '0' + digit;
        } else {
                buf[i] = 'a' + (digit - 10);
        }
        n /= 16;
        i--;
    }

    uart_puts("0x");
    uart_puts(&buf[i + 1]);
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

    unsigned long el = get_el();
    uart_puts("icevmm: current EL: ");
    uart_put_hex(el);
    uart_puts("\n");

    /* check if the returned EL is not 2 
    ...it should be */
    if (el != 2) {
            uart_puts("icevmm:      not running in EL2. halting!!!\n");
            hang();
    }

    uart_puts("icevmm: running in EL2\n");

    el2_setup();

    hang();


    // /* do nothing for now */
    // while (1) { /* inf loop */ }
}