##
## entrypoint of our hypv
##
ENTRY(_start)

SECTIONS
{
    . = 0x40000000;

    .text : {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }

    ##
    ## read-only data sect
    ##
    .rodata : {
        *(.rodata .rodata.*)
    }

    ##
    ## read-write data sect
    ##
    .data : {
        *(.data .data.*)
    }

    . = ALIGN(4096);
    .page_tables : {
        KEEP(*(.page_tables))
    }

    ##
    ## the bss section for uninitialised data
    ## create symbols to mark its start and end
    ## so our startup code can zero it all out
    ##
    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        . = ALIGN(8);
        __bss_end = .;
    }

    ##
    ## define a symbol for the top of our stack
    ## the stack grows downwards obviously, so we
    ## place the stack ptr here at the beginning
    ## allocate 64KB for the initial stack
    ##
    . = ALIGN(4096);
    __stack_start = .;
    . += 0x1000;
    __stack_top = .;
}
