#include "../include/mm.h"
#include "../include/uart.h"


/* see linker.ld  */
extern uint64_t __bss_end;
extern uint64_t __stack_top;


#define PAGE_SZ 4096


static uint64_t next_free_pg;


/* initialise sample bump allocator
   find end of hypv bin and align to page
   boundary which is where our heap will start */
void palloc_init(void)
{
    /* alloc from the page after hypv's bss sect  */
    next_free_pg = (uint64_t) &__stack_top;
    /* align to next page */
    if (next_free_pg % PAGE_SZ != 0) {
            next_free_pg = (next_free_pg + PAGE_SZ) & ~(PAGE_SZ - 1);
    }

    uart_puts("icevmm: initialised phys page allocator\n");
}


/* alloc single page of physmem
   and return a void ptr to the start of the alloc'd page */
void *palloc(void)
{
    /* we can assume we have enough mem and we're not starved
       but later on we could check mem lim */
    void *allocd_page = (void*) next_free_pg;
    next_free_pg += PAGE_SZ;

    /* zero out page */
    uint64_t *page_ptr = (uint64_t *) allocd_page;
    for (size_t i = 0; i < (PAGE_SZ / sizeof(uint64_t)); i++) {
            page_ptr[i] = 0;
    }

    return allocd_page;
}


/* internal memcpy */
void *memcpy(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}
