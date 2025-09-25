.section ".rodata"
.global _guest_payload_start
.global _guest_payload_end

_guest_payload_start:
    // a very simple guest that prints 'G' to the UART forever
    mov     x1, 0x09000000  // UART address
loop:
    mov     w0, #'G'
    str     w0, [x1]        // write to UART
    // simple delay loop
    mov     x2, 0x1000000
delay:
    sub     x2, x2, #1
    cbnz    x2, delay
    b       loop

_guest_payload_end:
