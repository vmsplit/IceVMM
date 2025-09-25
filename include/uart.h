#ifndef   __UART_H__
#define   __UART_H__


#pragma once
#include <stdint.h>

/* UART */
#define UART0_DR    (*(volatile uint32_t*)0x09000000)

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_put_hex(uint64_t n);


#endif // __UART_H__
