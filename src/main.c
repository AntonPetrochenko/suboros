#include "nes.h"

#define VERSION 5

volatile unsigned char nmi_ready;
volatile unsigned int  frame_count;

static unsigned char ppu_ctrl_shadow;
static unsigned char ppu_mask_shadow;
static unsigned char joy1_held;
static unsigned char joy1_prev;
unsigned char joy1_pressed;

extern const unsigned char ascii_chr_data[];

static const unsigned char palette_data[32] = {
    0x0F, 0x30, 0x10, 0x00,
    0x0F, 0x30, 0x10, 0x00,
    0x0F, 0x30, 0x10, 0x00,
    0x0F, 0x30, 0x10, 0x00,
    0x0F, 0x30, 0x10, 0x00,
    0x0F, 0x30, 0x10, 0x00,
    0x0F, 0x30, 0x10, 0x00,
    0x0F, 0x30, 0x10, 0x00,
};

static void ppu_wait_vblank(void) {
    while (!(PPU_STATUS & 0x80));
}

static void chr_load(unsigned int vram_addr,
                     const unsigned char *src,
                     unsigned int count) {
    (void)PPU_STATUS;
    PPU_ADDR = (unsigned char)(vram_addr >> 8);
    PPU_ADDR = (unsigned char)vram_addr;
    while (count--) {
        PPU_DATA = *src++;
    }
}

static void load_palette(void) {
    unsigned char i;
    (void)PPU_STATUS;
    PPU_ADDR = 0x3F;
    PPU_ADDR = 0x00;
    for (i = 0; i < 32; i++) {
        PPU_DATA = palette_data[i];
    }
}

static void ppu_clear_nt0(void) {
    unsigned int i;
    (void)PPU_STATUS;
    PPU_ADDR = 0x20;
    PPU_ADDR = 0x00;
    for (i = 0; i < 1024; i++) {
        PPU_DATA = 0;
    }
}

static void read_joypad(void) {
    unsigned char i, held = 0;
    joy1_prev = joy1_held;
    JOYPAD1 = 1;
    JOYPAD1 = 0;
    for (i = 0; i < 8; i++) {
        held = (unsigned char)((held << 1) | (JOYPAD1 & 1));
    }
    joy1_held = held;
    joy1_pressed = (unsigned char)(held & ~joy1_prev);
}

static unsigned char hex_nibble(unsigned char n) {
    n &= 0x0F;
    return (n < 10) ? (unsigned char)('0' + n) : (unsigned char)('A' + n - 10);
}

/* Write a 5-bit value to an MMC1 register via the serial shift register.
   The register is chosen by the address range: $8000=Control, $A000=CHR0,
   $C000=CHR1, $E000=PRG.  No NMI must fire between the five writes. */
static void mmc1_write_reg(unsigned int addr, unsigned char val) {
    volatile unsigned char *p = (volatile unsigned char *)addr;
    *p = val & 1; val >>= 1;
    *p = val & 1; val >>= 1;
    *p = val & 1; val >>= 1;
    *p = val & 1; val >>= 1;
    *p = val & 1;
}

void main(void) {
    unsigned char i;
    volatile unsigned char *apu = (volatile unsigned char *)0x4000;

    PPU_CTRL = 0;
    PPU_MASK = 0;
    ppu_ctrl_shadow = 0;
    ppu_mask_shadow = 0;

    for (i = 0; i <= 0x10; i++) {
        apu[i] = 0;
    }
    APU_STATUS = 0x0F;
    APU_FRAME  = 0x40;

    ppu_wait_vblank();
    ppu_wait_vblank();

    PPU_MASK = 0;

    chr_load(0x0000, ascii_chr_data, 8192);
    load_palette();
    ppu_clear_nt0();

    /* Write all static label content while rendering is off — free VRAM access. */

    /* row 1 col 1 = $2021 */
    (void)PPU_STATUS;
    PPU_ADDR = 0x20; PPU_ADDR = 0x21;
    PPU_DATA = '>'; PPU_DATA = ' ';
    PPU_DATA = 'C'; PPU_DATA = 'H'; PPU_DATA = 'R';
    PPU_DATA = ' '; PPU_DATA = 'O'; PPU_DATA = 'K';

    /* row 2 col 1 = $2041: label only; counter ($204B) and result ($204D) filled live */
    (void)PPU_STATUS;
    PPU_ADDR = 0x20; PPU_ADDR = 0x41;
    PPU_DATA = '>'; PPU_DATA = ' ';
    PPU_DATA = 'P'; PPU_DATA = 'R'; PPU_DATA = 'G';
    PPU_DATA = ' '; PPU_DATA = 'R'; PPU_DATA = 'A'; PPU_DATA = 'M';
    PPU_DATA = ' ';

    /* row 3 col 1 = $2061: label only; KB result ($2067) filled live */
    (void)PPU_STATUS;
    PPU_ADDR = 0x20; PPU_ADDR = 0x61;
    PPU_DATA = '>'; PPU_DATA = ' ';
    PPU_DATA = 'E'; PPU_DATA = 'X'; PPU_DATA = 'T';
    PPU_DATA = ' ';

    /* row 4 col 1 = $2081: version is static */
    (void)PPU_STATUS;
    PPU_ADDR = 0x20; PPU_ADDR = 0x81;
    PPU_DATA = '>'; PPU_DATA = ' ';
    PPU_DATA = 'V'; PPU_DATA = 'E'; PPU_DATA = 'R';
    PPU_DATA = ' '; PPU_DATA = (unsigned char)('0' + VERSION);

    /* Enable BG rendering now so the tests are visible.
       NMI stays off — no interrupt can corrupt MMC1 shift register writes.
       All VRAM updates below are gated to VBlank via ppu_wait_vblank(). */
    (void)PPU_STATUS;
    PPU_SCROLL = 0;
    PPU_SCROLL = 0;
    PPU_CTRL = 0;
    ppu_wait_vblank();
    PPU_MASK = MASK_SHOW_BG;

    /* ---- PRG RAM POST ---- */
    {
        unsigned char bank, page, pass, page_global;
        volatile unsigned char *ram = (volatile unsigned char *)0x6000;

        /* Reset MMC1 shift register, then configure:
           Control $0F = horizontal mirror | PRG fix-last-16KB | CHR 8KB
           PRG bank 0 with PRG RAM enabled (bit 4 = 0) */
        *(volatile unsigned char *)0x8000 = 0x80;
        mmc1_write_reg(0x8000, 0x0F);
        mmc1_write_reg(0xE000, 0x00);

        pass = 1;
        page_global = 0;

        /* Pass 1: write all banks before reading any back.
           If two banks are mirrored, the later write will corrupt the earlier
           one's data, which the verify pass will then catch. */
        for (bank = 0; bank < 4; bank++) {
            mmc1_write_reg(0xA000, (unsigned char)(bank << 2));
            for (page = 0; page < 32; page++) {
                unsigned int base = (unsigned int)page << 8;
                i = 0;
                do {
                    ram[base + (unsigned int)i] = (unsigned char)(i ^ page ^ bank);
                    i++;
                } while (i != 0);
            }
        }

        /* Pass 2: verify all banks; update counter on screen each page. */
        for (bank = 0; bank < 4 && pass; bank++) {
            mmc1_write_reg(0xA000, (unsigned char)(bank << 2));
            for (page = 0; page < 32 && pass; page++) {
                unsigned int base = (unsigned int)page << 8;
                unsigned char val;
                i = 0;
                do {
                    val = ram[base + (unsigned int)i];
                    if (val != (unsigned char)(i ^ page ^ bank)) {
                        pass = 0;
                        break;
                    }
                    i++;
                } while (i != 0);
                if (pass) page_global++;

                /* counter update at $204B during VBlank.
                   ppu_wait_vblank's last PPU_STATUS read resets the latch. */
                ppu_wait_vblank();
                PPU_ADDR = 0x20; PPU_ADDR = 0x4B;
                PPU_DATA = hex_nibble(page_global >> 4);
                PPU_DATA = hex_nibble(page_global);
                PPU_SCROLL = 0; PPU_SCROLL = 0;
                PPU_CTRL = 0;
            }
        }

        mmc1_write_reg(0xA000, 0x00);

        /* write OK/FAIL at $204D during VBlank */
        ppu_wait_vblank();
        PPU_ADDR = 0x20; PPU_ADDR = 0x4D;
        PPU_DATA = ' ';
        if (pass) {
            PPU_DATA = 'O'; PPU_DATA = 'K';
        } else {
            PPU_DATA = 'F'; PPU_DATA = 'A'; PPU_DATA = 'I'; PPU_DATA = 'L';
        }
        PPU_SCROLL = 0; PPU_SCROLL = 0;
        PPU_CTRL = 0;
    }

    /* ---- Extended PRG RAM survey ---- */
    /* Probe banks 4-7 (CHR bit 4) to measure RAM beyond the required 32KB.
       Never fails; just counts. For each candidate, we save the byte 0 of the
       bank it would mirror (ext_bank & 3), write a sentinel, then check whether
       the mirror bank's canary was disturbed. */
    {
        volatile unsigned char *ram = (volatile unsigned char *)0x6000;
        unsigned char ext_bank, ext_count;
        unsigned char extra_kb;

        ext_count = 0;
        for (ext_bank = 4; ext_bank < 8; ext_bank++) {
            unsigned char canary_bank = ext_bank & 3;
            unsigned char saved_canary;
            unsigned char sentinel = (unsigned char)(0xA5 ^ ext_bank);

            mmc1_write_reg(0xA000, (unsigned char)(canary_bank << 2));
            saved_canary = ram[0];

            mmc1_write_reg(0xA000, (unsigned char)(ext_bank << 2));
            ram[0] = sentinel;

            if (ram[0] != sentinel) break;          /* open bus — no RAM here */

            mmc1_write_reg(0xA000, (unsigned char)(canary_bank << 2));
            if (ram[0] != saved_canary) {           /* mirror detected */
                ram[0] = saved_canary;              /* restore clobbered byte */
                break;
            }

            ext_count++;
        }

        mmc1_write_reg(0xA000, 0x00);

        /* write ##K at $2067 during VBlank */
        extra_kb = (unsigned char)(ext_count << 3);
        ppu_wait_vblank();
        PPU_ADDR = 0x20; PPU_ADDR = 0x67;
        PPU_DATA = hex_nibble(extra_kb >> 4);
        PPU_DATA = hex_nibble(extra_kb);
        PPU_DATA = 'K';
        PPU_SCROLL = 0; PPU_SCROLL = 0;
        PPU_CTRL = 0;
    }

    ppu_ctrl_shadow = CTRL_NMI_ON;
    PPU_CTRL = CTRL_NMI_ON;
    ppu_mask_shadow = MASK_SHOW_BG | MASK_SHOW_SPR | MASK_LCLIP_BG | MASK_LCLIP_SPR;
    PPU_MASK = ppu_mask_shadow;

    while (1) {
        while (!nmi_ready);
        nmi_ready = 0;
        read_joypad();
    }
}
