#ifndef ALLOC_H
#define ALLOC_H

#include "syscall.h"

/* 256-byte first-fit heap pool in OS BSS.
 * Block layout: [size:1][used:1][data:size]  — used=0x00 free, 0x01 in-use.
 * size==0 marks the end sentinel.
 * User programs must not access OS RAM directly; use SC_ALLOC/SC_FREE. */

void alloc_init(void);

/* Syscall handler functions (called from IRQ dispatcher). */
void sys_alloc(void);
void sys_free(void);

#define SC_ALLOC(size) \
    sc_num = SYS_ALLOC; \
    sc_p0  = (unsigned char)(size); \
    __asm__("brk #%b", 0)
/* On return: sc_rv0=ptr_lo, sc_rv1=ptr_hi.  Both 0 means out-of-memory. */

#define SC_FREE(ptr_lo, ptr_hi) \
    sc_num = SYS_FREE; \
    sc_p0  = (unsigned char)(ptr_lo); \
    sc_p1  = (unsigned char)(ptr_hi); \
    __asm__("brk #%b", 0)

#endif
