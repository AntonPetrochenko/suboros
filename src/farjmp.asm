; farjmp.asm — fixed-bank trampoline for calling into the OS extended bank (bank 6).
;
; void __fastcall__ bank6_call(void (*fn)(void));
;
; cc65 __fastcall__ passes the 16-bit argument with lo byte in X, hi byte in A.
;
; Sequence:
;   1. Save function pointer in ptr1 (cc65 zero-page temp).
;   2. SEI — prevents NMI from corrupting the MMC1 shift register.
;   3. Save prg_bank_cur; switch to bank 6 (MMC1 PRG register at $E000).
;   4. JSR through ptr1.
;   5. Restore original bank, CLI, RTS.
;
; MMC1 serial protocol: write 5 bytes to $E000-$FFFF; only bit 0 of each
; byte counts.  Five writes latch the register (LSB first).  Bit 7 set on
; any write resets the shift register — our values (0-7) never set bit 7.

.importzp ptr1, tmp1
.import   _prg_bank_cur

.export _bank6_call
.segment "CODE"

; Switch MMC1 PRG bank to value in A. Updates _prg_bank_cur. Destroys A, X.
; Interrupts must already be disabled by the caller.
.proc do_prg_switch
    sta _prg_bank_cur
    ldx #5
:   sta $E000       ; MMC1 only looks at bit 0; subsequent bits come via LSR
    lsr a
    dex
    bne :-
    rts
.endproc

.proc _bank6_call
    ; A = fn_hi, X = fn_lo  (cc65 __fastcall__ 16-bit parameter)
    stx ptr1
    sta ptr1+1

    sei

    lda _prg_bank_cur
    sta tmp1                ; save original bank

    lda #6
    jsr do_prg_switch       ; switch to bank 6

    jsr do_call             ; JSR (ptr1): push next-instr addr, jmp through ptr1

    ; called function has returned via RTS to here
    lda tmp1
    jsr do_prg_switch       ; restore original bank

    cli
    rts

do_call:
    jmp (ptr1)              ; tail-call: fn's RTS returns to caller of do_call
.endproc
