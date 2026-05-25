#include "nes.h"
#include "syscall.h"
#include "ppu.h"

extern unsigned char no_sched;
#pragma zpsym ("no_sched")

/* SC_PUT_STRING
 * sc_p0 = x column, sc_p1 = y row
 * sc_p2 = string ptr lo, sc_p3 = string ptr hi
 * Enqueues a COPY into the VBlank PPU queue then waits for the drainer. */
void put_string(void) {
    const unsigned char *str = (const unsigned char *)
        ((unsigned int)sc_p3 << 8 | sc_p2);
    unsigned int addr = 0x2000u + (unsigned int)sc_p1 * 32u + sc_p0;
    unsigned char len = 0;

    while (str[len] != '\0') len++;
    if (len == 0) return;

    ppu_q_copy(addr, str, len);
    ppu_q_flush();
}

/* SC_SET_PRG_BANK — map one of the four 8 KB PRG RAM banks at $6000-$7FFF.
 * sc_p0 = bank (0-3); writes bits [3:2] of the MMC1 CHR0 register ($A000).
 * Also updates prg_bank_cur so fs_read can save/restore correctly.
 *
 * Wraps the 5-write MMC1 sequence in _no_sched so the NMI scheduler cannot
 * fire MMC1 writes of its own into the middle of the shift sequence. */
void set_prg_bank(void) {
    extern unsigned char prg_bank_cur;
    no_sched = 1;
    mmc1_write_reg(0xA000, (unsigned char)(sc_p0 << 2));
    prg_bank_cur = sc_p0;
    no_sched = 0;
}

/* SC_BEEP — Pulse 1 beep followed by silence, timed with a spin loop. */
void beep(void) {
    unsigned char i;
    unsigned char j;

    APU_P1_CTRL  = 0xBF;
    APU_P1_SWEEP = 0x08;
    APU_P1_LO    = sc_p0;
    APU_P1_HI    = 0x00;

    for (j = 0; j != sc_p1; j++) {
        for (i = 0; i != 255; i++);
    }

    APU_P1_CTRL = 0x30;

    for (j = 0; j != 255; j++) {
        for (i = 0; i != 255; i++);
    }
}
