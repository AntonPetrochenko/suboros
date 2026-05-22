.importzp c_sp
.import   _main
.import   nmi_handler, irq_handler

.segment "CODE"

.proc reset_handler
    sei
    cld
    ldx #$FF
    txs

    lda #0
    sta $2000       ; PPUCTRL = 0 (NMI off)
    sta $2001       ; PPUMASK = 0 (rendering off)

    ; Wait for first vblank
:   bit $2002
    bpl :-

    ; Zero all RAM $0000-$07FF
    lda #0
    tax
:   sta $000,x
    sta $100,x
    sta $200,x
    sta $300,x
    sta $400,x
    sta $500,x
    sta $600,x
    sta $700,x
    inx
    bne :-

    ; Wait for second vblank
:   bit $2002
    bpl :-

    ; Init CC65 software stack (grows down from top of RAM)
    lda #$00
    sta c_sp
    lda #$08        ; c_sp = $0800
    sta c_sp+1

    jsr _main

:   jmp :-          ; main() should never return
.endproc

.segment "VECTORS"
    .word nmi_handler
    .word reset_handler
    .word irq_handler
