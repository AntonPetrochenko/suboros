#include "nes.h"

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

    (void)PPU_STATUS;
    PPU_ADDR = 0x20;
    PPU_ADDR = 0x21;
    PPU_DATA = '>';
    PPU_DATA = ' ';
    PPU_DATA = 'C';
    PPU_DATA = 'H';
    PPU_DATA = 'R';
    PPU_DATA = ' ';
    PPU_DATA = 'O';
    PPU_DATA = 'K';

    PPU_SCROLL = 0;
    PPU_SCROLL = 0;

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
