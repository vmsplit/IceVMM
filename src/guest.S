.section ".rodata"
.globl _guest_payload

_guest_payload:
    .word   0xd2800000      // mov x0,  #0
    .word   0x580000a1      // ldr x1,  uart_addr_lit
    .word   0x52800922      // mov w2,  #'G'
    .word   0x39000022      // strb w2, [x1]
    .word   0xd4000002      // hvc #0
    .word   0x14000000      // b .
uart_addr_lit:
    .word   0x09000000      // UART addr lit

.section ".text"
