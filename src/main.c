#include "nes.h"
#include "syscall.h"
#include "fs.h"
#include "proc.h"
#include "alloc.h"
#include "ppu.h"

extern unsigned char no_sched;
#pragma zpsym ("no_sched")

#define VERSION 5
#define TOSTR_(x) #x
#define TOSTR(x)  TOSTR_(x)

volatile unsigned char nmi_ready;
volatile unsigned int  frame_count;

static unsigned char joy1_held;
static unsigned char joy1_prev;
unsigned char joy1_pressed;


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

static void load_chr_from_fs(void) {
    static unsigned char chr_va[8];
    static const char chr_name[] = "CHR.BIN";
    static unsigned char buf[256];
    unsigned int  remaining;
    unsigned int  got;
    unsigned char j;

    sc_p0 = 0;
    PTR_UNPACK(chr_name, sc_p1, sc_p2);
    PTR_UNPACK(chr_va,   sc_p3, sc_p4);
    fs_open();
    if (sc_rv0 == FS_HANDLE_EMPTY) return;

    (void)PPU_STATUS;
    PPU_ADDR = 0x00;
    PPU_ADDR = 0x00;

    /* Bulk-read 256 bytes at a time and slam them into PPU_DATA.  NMI and
       rendering are both off at this point in boot, so direct writes are
       safe and far faster than per-byte fs_getbyte syscalls. */
    remaining = 8192;
    while (remaining) {
        PTR_UNPACK(chr_va, sc_p0, sc_p1);
        PTR_UNPACK(buf,    sc_p2, sc_p3);
        sc_p4 = 0;
        sc_p5 = 1;                  /* request 256 bytes */
        fs_read();
        got = (unsigned int)sc_rv1 << 8 | sc_rv0;
        if (got == 0) break;
        if (got == 256) {
            /* Full chunk: write all 256 with an 8-bit wrap-around loop. */
            j = 0;
            do { PPU_DATA = buf[j]; j++; } while (j != 0);
            remaining -= 256;
        } else {
            /* Short read at EOF (got <= 255). */
            for (j = 0; j < (unsigned char)got; j++) PPU_DATA = buf[j];
            break;
        }
    }

    PTR_UNPACK(chr_va, sc_p0, sc_p1);
    fs_close();
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

/* Direct nametable-0 clear — caller MUST have rendering disabled
   (PPU_MASK = 0).  Used at boot and just before enabling NMI; runtime
   clears go through ppu_q_fill so the queue drainer handles them. */
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
   $C000=CHR1, $E000=PRG.  Wraps the five writes in _no_sched so the NMI
   scheduler (which itself issues MMC1 writes to $E000) cannot corrupt the
   shift sequence by firing between two of our writes. */
void mmc1_write_reg(unsigned int addr, unsigned char val) {
    volatile unsigned char *p = (volatile unsigned char *)addr;
    no_sched = 1;
    *p = val & 1; val >>= 1;
    *p = val & 1; val >>= 1;
    *p = val & 1; val >>= 1;
    *p = val & 1; val >>= 1;
    *p = val & 1;
    no_sched = 0;
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

static unsigned char is_prg_file(const char *name) {
    unsigned char i;
    for (i = 0; i < FS_NAME_LEN && name[i]; i++);
    if (i < 3) return 0;
    return name[i-3] == 'P' && name[i-2] == 'R' && name[i-1] == 'G';
}

static void view_file(unsigned char slot, const char *name) {
    unsigned char va[8];
    unsigned char line[32];
    unsigned char x = 0, y = 0;
    static const char done_msg[] = "DONE";

    /* Clear the screen through the queue.  Drainer handles it across a
       handful of frames; rendering stays on so no flash. */
    ppu_q_fill(0x2000, 0, 1024);
    ppu_q_flush();

    SC_FS_OPEN(slot, name, va);
    if (sc_rv0 == FS_SLOT_EMPTY) { while (1); }

    while (1) {
        unsigned char n = 0;

        /* Fill a one-row line buffer (up to 32 chars) — but flush at end-of
           line, end-of-file, OR if we've consumed the per-frame byte budget. */
        while (n < (unsigned char)(32 - x)) {
            SC_FS_GETBYTE(va);
            if (sc_rv1 == FS_ERR_EOF) break;
            line[n++] = sc_rv0;
        }

        if (n > 0) {
            ppu_q_copy(0x2000u + (unsigned int)y * 32u + x, line, n);
            x += n;
            if (x >= 32) {
                x = 0;
                y++;
                if (y >= 30) {
                    /* Wrap back to top — clear so the new page is fresh. */
                    ppu_q_flush();
                    ppu_q_fill(0x2000, 0, 1024);
                    ppu_q_flush();
                    y = 0;
                }
            }
        }

        if (sc_rv1 == FS_ERR_EOF) break;
    }

    ppu_q_flush();
    ppu_q_copy(0x2000u + 29u * 32u, (const unsigned char *)done_msg, 4);
    ppu_q_flush();
    while (1);
}

void main(void) {
    unsigned char i;
    volatile unsigned char *apu = (volatile unsigned char *)0x4000;

    fs_init();
    proc_init();
    alloc_init();
    ppu_q_init();

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

    for (i = 0; i <= 0x10; i++) {
        apu[i] = 0;
    }
    APU_STATUS = 0x0F;
    APU_FRAME  = 0x40;

    ppu_wait_vblank();
    ppu_wait_vblank();

    /* Rendering and NMI both off: do the big direct PPU loads. */
    load_chr_from_fs();
    load_palette();
    ppu_clear_nt0();

    /* Reset scroll latch before enabling NMI + rendering. */
    (void)PPU_STATUS;
    PPU_SCROLL = 0;
    PPU_SCROLL = 0;

    /* Enable NMI and rendering NOW so that SC_PRINT (queue-based) works
       from the very first banner line.  proc_user_count is still 0 so the
       NMI scheduler is a no-op; the drainer is what we care about. */
    ppu_wait_vblank();
    PPU_CTRL = CTRL_NMI_ON;
    PPU_MASK = MASK_SHOW_BG | MASK_LCLIP_BG;

    SC_PRINT(1, 1, "> CHR OK");
    SC_PRINT(1, 2, "> PRG RAM ");
    SC_PRINT(1, 3, "> EXT ");
    SC_PRINT(1, 4, "> VER " TOSTR(VERSION));

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

    SC_BEEP(0xFF, 127);

    browser_enumerate();

    /* Clear screen through the queue (rendering stays on, no flash) and
       draw the file browser. */
    ppu_q_fill(0x2000, 0, 1024);
    ppu_q_flush();

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
                if (is_prg_file(browser_files[sel].name)) {
                    static unsigned char err_buf[5];
                    SC_START_PROCESS(browser_files[sel].slot,
                                     browser_files[sel].name);
                    if (sc_rv0 == 0xFF) {
                        err_buf[0] = 'E';
                        err_buf[1] = hex_nibble(sc_rv1 >> 4);
                        err_buf[2] = hex_nibble(sc_rv1);
                        err_buf[3] = 0;
                        SC_PRINT(0, 29, err_buf);
                    }
                } else {
                    view_file(browser_files[sel].slot, browser_files[sel].name);
                }
            }
        }
    }
}
