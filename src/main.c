#include "nes.h"
#include "syscall.h"
#include "fs.h"

#define VERSION 5
#define TOSTR_(x) #x
#define TOSTR(x)  TOSTR_(x)

volatile unsigned char nmi_ready;
volatile unsigned int  frame_count;

static unsigned char ppu_ctrl_shadow;
static unsigned char ppu_mask_shadow;
static unsigned char joy1_held;
static unsigned char joy1_prev;
unsigned char joy1_pressed;

extern const unsigned char ascii_chr_data[];

static const unsigned char palette_data[32] = {
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
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
void mmc1_write_reg(unsigned int addr, unsigned char val) {
    volatile unsigned char *p = (volatile unsigned char *)addr;
    *p = val & 1; val >>= 1;
    *p = val & 1; val >>= 1;
    *p = val & 1; val >>= 1;
    *p = val & 1; val >>= 1;
    *p = val & 1;
}

/* ---- File browser ---- */

#define BROWSER_MAX_FILES 16

static struct {
    unsigned char slot;
    char          name[FS_NAME_LEN + 1];
} browser_files[BROWSER_MAX_FILES];
static unsigned char browser_file_count;

static void browser_enumerate(void) {
    unsigned char slot, j;
    browser_file_count = 0;
    for (slot = 0; slot < FS_MAX_MOUNTS; slot++) {
        unsigned char *entry;
        if (FS_MOUNT_TABLE[slot].device_num == FS_SLOT_EMPTY) continue;
        entry = (unsigned char *)
            ((unsigned int)FS_MOUNT_TABLE[slot].toc_hi << 8 |
                           FS_MOUNT_TABLE[slot].toc_lo);
        while (entry[0] != '\0' && browser_file_count < BROWSER_MAX_FILES) {
            browser_files[browser_file_count].slot = slot;
            for (j = 0; j < FS_NAME_LEN; j++) {
                browser_files[browser_file_count].name[j] = (char)entry[j];
            }
            browser_files[browser_file_count].name[FS_NAME_LEN] = '\0';
            browser_file_count++;
            /* Advance past name[8] + extent list + sentinel */
            entry += FS_NAME_LEN;
            while (entry[0] != FS_SLOT_EMPTY) entry += 5;
            entry += 5;
        }
    }
}

static void browser_draw_line(unsigned char idx, unsigned char selected) {
    char line[12];  /* ">S FILENAME\0" = 12 chars */
    unsigned char j;
    line[0] = selected ? '>' : ' ';
    line[1] = '0' + browser_files[idx].slot;
    line[2] = ' ';
    for (j = 0; j < FS_NAME_LEN; j++)
        line[3 + j] = browser_files[idx].name[j];
    line[11] = '\0';
    SC_PRINT(0, (unsigned char)(idx + 2), line);
}

static unsigned char count_mounts(void) {
    unsigned char count = 0, slot;
    for (slot = 0; slot < FS_MAX_MOUNTS; slot++) {
        if (FS_MOUNT_TABLE[slot].device_num != FS_SLOT_EMPTY) count++;
    }
    return count;
}

static void browser_draw_all(unsigned char sel) {
    unsigned char i;
    char header[16];
    unsigned char mount_count = count_mounts();

    header[0] = 'M';
    header[1] = 'O';
    header[2] = 'U';
    header[3] = 'N';
    header[4] = 'T';
    header[5] = 'S';
    header[6] = ':';
    header[7] = ' ';
    header[8] = '0' + mount_count;
    header[9] = '\0';

    SC_PRINT(0, 0, header);
    SC_PRINT(0, 1, "SLOT FILE");
    for (i = 0; i < browser_file_count; i++)
        browser_draw_line(i, i == sel);
}

static void view_file(unsigned char slot, const char *name) {
    unsigned char va[8];
    unsigned char c;
    unsigned char x = 0, y = 0;
    char buf[2];

    PPU_MASK = 0;
    ppu_clear_nt0();
    PPU_MASK = ppu_mask_shadow;

    SC_FS_OPEN(slot, name, va);
    if (sc_rv0 == FS_SLOT_EMPTY) { while (1); }

    buf[1] = '\0';
    while (1) {
        while (!nmi_ready);
        nmi_ready = 0;

        SC_FS_GETBYTE(va);
        if (sc_rv1 == FS_ERR_EOF) break;
        c = sc_rv0;

        if (x == 0 && y == 0) {
            PPU_MASK = 0;
            ppu_clear_nt0();
            PPU_MASK = ppu_mask_shadow;
        }

        buf[0] = (char)c;
        SC_PRINT(x, y, buf);

        x++;
        if (x >= 32) {
            x = 0;
            y++;
            if (y >= 30) y = 0;
        }
    }

    SC_PRINT(0, 29, "DONE");
    while (1);
}

void main(void) {
    unsigned char i;
    volatile unsigned char *apu = (volatile unsigned char *)0x4000;

    fs_init();

    /* Mount any ROM filesystems embedded by tools/mkfs.py. */
    {
        const unsigned char *p = fs_rom_mount_table;
        unsigned char *toc;
        while (p[2] != FS_SLOT_EMPTY) {
            toc = (unsigned char *)((unsigned int)p[1] << 8 | p[0]);
            SC_FS_MOUNT(toc, p[2]);
            p += 3;
        }
    }

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

    SC_PRINT(1, 1, "> CHR OK");
    SC_PRINT(1, 2, "> PRG RAM ");
    SC_PRINT(1, 3, "> EXT ");
    SC_PRINT(1, 4, "> VER " TOSTR(VERSION));

    /* Set scroll and enable BG so the labels above are visible. */
    (void)PPU_STATUS;
    PPU_SCROLL = 0;
    PPU_SCROLL = 0;
    PPU_CTRL = 0;
    ppu_wait_vblank();
    PPU_MASK = MASK_SHOW_BG;

    SC_BEEP(0xFF, 127);

    /* Read joypad; only run POST if any button is held. */
    read_joypad();

    if (joy1_held) {
    /* ---- PRG RAM POST ---- */
    {
        unsigned char bank, page, pass, page_global;
        char buf[3];
        volatile unsigned char *ram = (volatile unsigned char *)0x6000;

        /* Reset MMC1 shift register, then configure:
           Control $0F = horizontal mirror | PRG fix-last-16KB | CHR 8KB
           PRG bank 0 with PRG RAM enabled (bit 4 = 0) */
        *(volatile unsigned char *)0x8000 = 0x80;
        mmc1_write_reg(0x8000, 0x0F);
        mmc1_write_reg(0xE000, 0x00);

        pass = 1;
        page_global = 0;

        /* Pass 1: write all banks before reading any back. */
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

        /* Pass 2: verify; update counter on screen each page. */
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

                buf[0] = hex_nibble(page_global >> 4);
                buf[1] = hex_nibble(page_global);
                buf[2] = '\0';
                SC_PRINT(11, 2, buf);
            }
        }

        mmc1_write_reg(0xA000, 0x00);
        SC_PRINT(13, 2, pass ? " OK" : " FAIL");
    }
    } /* end if (joy1_held) */

    browser_enumerate();

    /* Clear screen and draw the file browser. */
    PPU_MASK = 0;
    ppu_clear_nt0();
    PPU_MASK = MASK_SHOW_BG;

    ppu_ctrl_shadow = CTRL_NMI_ON;
    PPU_CTRL = CTRL_NMI_ON;
    ppu_mask_shadow = MASK_SHOW_BG | MASK_LCLIP_BG;
    PPU_MASK = ppu_mask_shadow;

    {
        unsigned char sel = 0;
        browser_draw_all(sel);

        while (1) {
            while (!nmi_ready);
            nmi_ready = 0;
            read_joypad();

            if (joy1_pressed & BTN_UP) {
                if (sel > 0) {
                    browser_draw_line(sel, 0);
                    sel--;
                    browser_draw_line(sel, 1);
                }
            }
            if (joy1_pressed & BTN_DOWN) {
                if (browser_file_count > 0 && sel < browser_file_count - 1) {
                    browser_draw_line(sel, 0);
                    sel++;
                    browser_draw_line(sel, 1);
                }
            }
            if ((joy1_pressed & BTN_A) && browser_file_count > 0) {
                view_file(browser_files[sel].slot, browser_files[sel].name);
            }
        }
    }
}
