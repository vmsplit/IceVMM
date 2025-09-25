#ifndef   __MM_H__
#define   __MM_H__

#include <stdint.h>
#include <string.h>


void palloc_init(void);
void *palloc(void);
void *memcpy(void *dest, const void *src, size_t n);

#endif // __MM_H__
