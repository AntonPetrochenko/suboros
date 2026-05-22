; iNES header for suboros
; SXROM (MMC1), 8x16KB PRG ROM = 128KB, CHR RAM, 32KB battery-backed PRG RAM
.segment "HEADER"
    .byte "NES", $1A        ; magic number
    .byte 8                 ; PRG ROM size: 8 x 16KB = 128KB
    .byte 0                 ; CHR ROM size: 0 = 8KB CHR RAM
    .byte $12               ; flags 6: mapper low nibble=1, battery PRG RAM set, horizontal mirror
    .byte $10               ; flags 7: mapper high nibble=1 (mapper = 1)
    .byte 4                 ; flags 8: 4 x 8KB PRG RAM banks = 32KB
    .byte $00               ; flags 9: NTSC
    .byte $00               ; flags 10
    .byte 0,0,0,0,0         ; padding to 16 bytes
