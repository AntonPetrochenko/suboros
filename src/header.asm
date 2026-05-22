; NES 2.0 header for suboros
; SXROM (MMC1), 8x16KB PRG ROM = 128KB, 8KB CHR RAM, 32KB battery-backed PRG RAM
.segment "HEADER"
    .byte "NES", $1A    ; magic
    .byte 8             ; byte 4: PRG ROM 8 x 16KB = 128KB
    .byte 0             ; byte 5: CHR ROM 0 (CHR RAM)
    .byte $12           ; byte 6: mapper low nibble=1, battery, horizontal mirror
    .byte $08           ; byte 7: mapper high nibble=0, NES 2.0 id (bits 3-2 = 10)
    .byte $00           ; byte 8: submapper=0, mapper bits 11-8=0
    .byte $00           ; byte 9: PRG/CHR ROM size MSB
    .byte $90           ; byte 10: battery PRG RAM 32KB (shift=9 in bits 7-4), no volatile PRG RAM
    .byte $07           ; byte 11: CHR RAM 8KB (shift=7 in bits 3-0), no battery CHR RAM
    .byte $00           ; byte 12: NTSC timing
    .byte $00           ; byte 13: VS/extend console type
    .byte $00           ; byte 14: miscellaneous ROMs
    .byte $00           ; byte 15: default expansion device
