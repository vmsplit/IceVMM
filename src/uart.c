#include "../include/uart.h"
#include <stdint.h>

void uart_init(void)
{
    // cfg'd by QEMU's boot firmware
}

void uart_putc(char c)
{
    UART0_DR = c;
}

void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

void uart_put_hex(uint64_t n)
{
    char buffer[17];
    char *p = buffer + 16;
    const char hex_chars[] = "0123456789abcdef";

    *p = '\0';

    if (n == 0) {
        uart_putc('0');
        return;
    }

    while (n > 0) {
        *--p = hex_chars[n & 0xF];
        n >>= 4;
    }

    uart_puts(p);
}
