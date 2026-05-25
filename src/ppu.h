#ifndef PPU_H
#define PPU_H

/* VBlank-deferred PPU write queue.
 *
 * Producers (kernel C / user syscalls) enqueue COPY or FILL commands; the
 * NMI handler drains the queue at the top of VBlank, then performs the
 * frame-counter update and scheduler.  After this header is in use, no
 * code outside the NMI handler should touch $2006 / $2007 while rendering
 * is enabled.
 *
 * Command stream (in ppu_queue[]):
 *   [opcode][ppu_addr_hi][ppu_addr_lo][len][payload...]
 *     opcode 0  END               (no header/payload bytes)
 *     opcode 1  COPY              payload = len data bytes
 *     opcode 2  FILL              payload = 1 value byte (written len times)
 *
 * len = 0 is reserved — callers never enqueue with len = 0.
 *
 * Queue capacity: 256 bytes (PPU_QUEUE segment at $0600-$06FF).  In addition
 * to the queue-byte limit, the producer also caps total PPU bytes per frame
 * at PPU_FRAME_BUDGET so the drainer finishes inside VBlank. */

#define PPU_QUEUE_SIZE   256

/* Per-frame cap on PPU writes the drainer will perform.  VBlank ~2273 cycles;
   a tight inline drain costs ~18 cycles/byte for COPY, ~12 cycles/byte for
   FILL, plus per-command overhead and scroll restore.  96 leaves headroom. */
#define PPU_FRAME_BUDGET 96

extern unsigned char ppu_queue[PPU_QUEUE_SIZE];

extern unsigned char ppu_q_tail;
extern unsigned char ppu_q_busy;
extern unsigned char ppu_q_writes;
#pragma zpsym ("ppu_q_tail")
#pragma zpsym ("ppu_q_busy")
#pragma zpsym ("ppu_q_writes")

void ppu_q_init(void);
void ppu_q_copy(unsigned int ppu_addr, const unsigned char *src, unsigned char n);
void ppu_q_fill(unsigned int ppu_addr, unsigned char value, unsigned int count);
void ppu_q_flush(void);

#endif
