#include "nes.h"
#include "ppu.h"

extern unsigned char no_sched;
#pragma zpsym ("no_sched")

#pragma bss-name (push, "PPU_QUEUE")
unsigned char ppu_queue[PPU_QUEUE_SIZE];
#pragma bss-name (pop)

#define OP_END  0
#define OP_COPY 1
#define OP_FILL 2

void ppu_q_init(void) {
    ppu_q_tail   = 0;
    ppu_q_busy   = 0;
    ppu_q_writes = 0;
    ppu_queue[0] = OP_END;
}

void ppu_q_flush(void) {
    /* NMI drainer zeroes ppu_q_tail when it consumes the queue. */
    while (ppu_q_tail) { }
}

/* Returns 1 iff a command with the given queue footprint AND PPU-write count
   can be enqueued without exceeding either limit this frame. */
static unsigned char fits(unsigned char queue_bytes, unsigned int ppu_bytes) {
    /* Reserve 1 byte at end of queue for the trailing END marker. */
    if ((unsigned int)ppu_q_tail   + queue_bytes >= PPU_QUEUE_SIZE) return 0;
    if ((unsigned int)ppu_q_writes + ppu_bytes   >  PPU_FRAME_BUDGET) return 0;
    return 1;
}

void ppu_q_copy(unsigned int ppu_addr, const unsigned char *src, unsigned char n) {
    unsigned char i;
    unsigned char rem;

    if (n == 0) return;

    /* Acquire-and-enqueue atomically against the NMI scheduler: with
       proc_user_count > 0 the kernel and user processes can both call
       ppu_q_copy, and a context switch between fits() and the actual
       enqueue would interleave their writes into the queue.  no_sched
       blocks the scheduler; ppu_q_busy blocks the drainer. */
    for (;;) {
        no_sched = 1;
        if (fits((unsigned char)(4 + n), n)) break;
        no_sched = 0;
        ppu_q_flush();
    }

    ppu_q_busy = 1;
    i = ppu_q_tail;
    ppu_queue[i++] = OP_COPY;
    ppu_queue[i++] = (unsigned char)(ppu_addr >> 8);
    ppu_queue[i++] = (unsigned char)(ppu_addr & 0xFF);
    ppu_queue[i++] = n;
    rem = n;
    while (rem--) ppu_queue[i++] = *src++;
    ppu_queue[i]  = OP_END;
    ppu_q_tail    = i;
    ppu_q_writes += n;
    ppu_q_busy    = 0;
    no_sched      = 0;
}

void ppu_q_fill(unsigned int ppu_addr, unsigned char value, unsigned int count) {
    unsigned char chunk;
    unsigned char i;
    unsigned int  room;

    while (count) {
        /* Hold no_sched across the fits() check + enqueue to keep them
           atomic vs. the NMI scheduler.  See ppu_q_copy for the rationale. */
        no_sched = 1;

        room = (unsigned int)PPU_FRAME_BUDGET - (unsigned int)ppu_q_writes;
        if (room == 0) {
            no_sched = 0;
            ppu_q_flush();
            continue;
        }
        if (room > 255u) room = 255u;
        chunk = (count > room) ? (unsigned char)room : (unsigned char)count;

        /* Queue space for a single FILL command = 5 bytes. */
        if (!fits(5, chunk)) {
            no_sched = 0;
            ppu_q_flush();
            continue;
        }

        ppu_q_busy = 1;
        i = ppu_q_tail;
        ppu_queue[i++] = OP_FILL;
        ppu_queue[i++] = (unsigned char)(ppu_addr >> 8);
        ppu_queue[i++] = (unsigned char)(ppu_addr & 0xFF);
        ppu_queue[i++] = chunk;
        ppu_queue[i++] = value;
        ppu_queue[i]   = OP_END;
        ppu_q_tail     = i;
        ppu_q_writes  += chunk;
        ppu_q_busy     = 0;
        no_sched       = 0;

        ppu_addr += chunk;
        count    -= chunk;
    }
}
