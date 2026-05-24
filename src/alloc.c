#include "syscall.h"
#include "alloc.h"

#define POOL_SIZE 256

static unsigned char alloc_pool[POOL_SIZE];

void alloc_init(void) {
    alloc_pool[0] = POOL_SIZE - 2;  /* one free block covering rest of pool */
    alloc_pool[1] = 0x00;           /* free */
    alloc_pool[2] = 0x00;           /* end sentinel for after any split */
}

/* SYS_ALLOC: sc_p0 = size → sc_rv0/rv1 = ptr lo/hi (0/0 on OOM). */
void sys_alloc(void) {
    unsigned char n = sc_p0;
    unsigned char *p = alloc_pool;
    unsigned char *next;
    unsigned int addr;

    if (n == 0) {
        sc_rv0 = 0;
        sc_rv1 = 0;
        return;
    }

    while (p[0] != 0) {
        if (p[1] == 0x00 && p[0] >= n) {
            /* Split block only if remainder is at least 3 bytes (2 header + 1 data). */
            if (p[0] > (unsigned char)(n + 2)) {
                next = p + 2 + n;
                next[0] = p[0] - n - 2;
                next[1] = 0x00;
            }
            p[0] = n;
            p[1] = 0x01;
            addr = (unsigned int)(p + 2);
            sc_rv0 = (unsigned char)(addr & 0xFF);
            sc_rv1 = (unsigned char)(addr >> 8);
            return;
        }
        /* Opportunistic coalesce: merge two adjacent free blocks. */
        if (p[1] == 0x00) {
            next = p + 2 + p[0];
            if (next[0] != 0 && next[1] == 0x00) {
                p[0] = p[0] + 2 + next[0];
                continue;  /* re-evaluate same block with larger size */
            }
        }
        p += (unsigned int)(p[0]) + 2;
    }

    sc_rv0 = 0;
    sc_rv1 = 0;
}

/* SYS_FREE: sc_p0/p1 = ptr lo/hi. */
void sys_free(void) {
    unsigned char *p;
    unsigned int addr = (unsigned int)sc_p1 << 8 | sc_p0;
    if (addr < 2) return;           /* obviously invalid */
    p = (unsigned char *)addr - 2;  /* back to block header */
    if (p[1] == 0x01) p[1] = 0x00; /* mark free (idempotent otherwise) */
}
