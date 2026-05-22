#ifndef NES_H
#define NES_H

#define PPU_CTRL   (*(volatile unsigned char *)0x2000)
#define PPU_MASK   (*(volatile unsigned char *)0x2001)
#define PPU_STATUS (*(volatile unsigned char *)0x2002)
#define PPU_ADDR   (*(volatile unsigned char *)0x2006)
#define PPU_DATA   (*(volatile unsigned char *)0x2007)
#define APU_STATUS (*(volatile unsigned char *)0x4015)
#define APU_FRAME  (*(volatile unsigned char *)0x4017)
#define PPU_SCROLL (*(volatile unsigned char *)0x2005)
#define JOYPAD1    (*(volatile unsigned char *)0x4016)

#define CTRL_NMI_ON    0x80
#define MASK_SHOW_SPR  0x10
#define MASK_SHOW_BG   0x08
#define MASK_LCLIP_SPR 0x04
#define MASK_LCLIP_BG  0x02

#endif
