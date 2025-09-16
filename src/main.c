#include <stdint.h>

/* base addr of the PL011 UART in qemu's 'virt' machine */
#define UART0_DR (*(volatile uint32_t*)0x09000000)

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

/* C entrypoint, called from start.S */
void main(void)
{
    uart_puts("distant meows from baremetal aarch64 !!!\n");
    /* do nothing for now */
    while (1) { /* inf loop */ }
}