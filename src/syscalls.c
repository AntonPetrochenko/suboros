#include "nes.h"
#include "syscall.h"


/* SC_PUT_STRING
 * sc_p0 = x column, sc_p1 = y row
 * sc_p2 = string ptr lo, sc_p3 = string ptr hi
 * Waits for VBlank then writes the null-terminated string into nametable 0. */
void put_string(void) {
    const char *str = (const char *)((unsigned int)sc_p3 << 8 | sc_p2);
    unsigned int addr = 0x2000u + (unsigned int)sc_p1 * 32u + sc_p0;
    unsigned char ch;

    while (!(PPU_STATUS & 0x80));   /* wait for VBlank */
    (void)PPU_STATUS;               /* reset address latch (w = 0) */

    PPU_ADDR = (unsigned char)(addr >> 8);
    PPU_ADDR = (unsigned char)addr;
    while ((ch = *str++) != '\0')
        PPU_DATA = ch;

    PPU_SCROLL = 0;
    PPU_SCROLL = 0;
}

/* SC_SET_PRG_BANK — map one of the four 8 KB PRG RAM banks at $6000-$7FFF.
 * sc_p0 = bank (0-3); writes bits [3:2] of the MMC1 CHR0 register ($A000). */
void set_prg_bank(void) {
    mmc1_write_reg(0xA000, (unsigned char)(sc_p0 << 2));
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
