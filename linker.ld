##
## entrypoint of our hypv
##
ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : ALIGN(4K) {
        *(.text.boot)   /* boot code at beginning */
        *(.text*)       /* all other code         */
    }

    ##
    ## read-only data sect
    ##
    .rodata : ALIGN(4K) {
        *(.rodata*)
    }

    ##
    ## read-write data sect
    ##
    .data : ALIGN(4K) {
        *(.data*)
    }

    ##
    ## the bss section for uninitialised data
    ## create symbols to mark its start and end
    ## so our startup code can zero it all out
    ##
    .bss : ALIGN(4K) {
        __bss_start = .;
        *(.bss*)
        __bss_end   = .;
    }

    ##
    ## define a symbol for the top of our stack
    ## the stack grows downwards obviously, so we
    ## place the stack ptr here at the beginning
    ## allocate 64KB for the initial stack
    ##
    . = ALIGN(16);
    . += 64K;
    __stack_top = .;

    END = .;
}