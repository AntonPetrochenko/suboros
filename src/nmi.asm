.import _nmi_ready
.import _frame_count
.import _put_string
.import _beep
.import _set_prg_bank
.importzp _sc_num

.export nmi_handler, irq_handler

.segment "CODE"

.proc nmi_handler
    pha
    txa
    pha
    tya
    pha

    inc _frame_count
    bne :+
    inc _frame_count+1
:
    lda #1
    sta _nmi_ready

    pla
    tay
    pla
    tax
    pla
    rti
.endproc

; BRK and hardware IRQ share this vector. Distinguish by the B flag
; (bit 4) in the saved P register, which the CPU sets for BRK but not IRQ.
; After tsx, saved P is at $0104,X:
;   CPU pushed PCH/PCL/P (SP-=3), we push A/X/Y (SP-=3) → offset = 3+1 = 4
;   lda $0104,X  ≡  load from $0100 + original_SP - 2  = where P lives.
.proc irq_handler
    pha
    txa
    pha
    tya
    pha
    tsx
    lda $0104,x             ; saved P register
    and #$10                ; B flag set = BRK
    beq irq_restore         ; pure IRQ — nothing to do

    lda _sc_num
    cmp #0                  ; SYS_PUT_STRING
    bne check_beep
    jsr _put_string
    jmp irq_restore
check_beep:
    cmp #1                  ; SYS_BEEP
    bne check_set_prg_bank
    jsr _beep
    jmp irq_restore
check_set_prg_bank:
    cmp #2                  ; SYS_SET_PRG_BANK
    bne irq_restore
    jsr _set_prg_bank
irq_restore:
    pla
    tay
    pla
    tax
    pla
    rti
.endproc
