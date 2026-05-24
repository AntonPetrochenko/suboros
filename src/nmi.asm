.import _nmi_ready
.import _frame_count
.import _put_string
.import _beep
.import _set_prg_bank
.import _fs_mount
.import _fs_open
.import _fs_unmount
.import _fs_stat
.import _fs_close
.import _fs_seek
.import _fs_read
.import _fs_getbyte
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
    bne check_fs_mount
    jsr _set_prg_bank
    jmp irq_restore
check_fs_mount:
    cmp #3                  ; SYS_FS_MOUNT
    bne check_fs_open
    jsr _fs_mount
    jmp irq_restore
check_fs_open:
    cmp #4                  ; SYS_FS_OPEN
    bne check_fs_unmount
    jsr _fs_open
    jmp irq_restore
check_fs_unmount:
    cmp #5                  ; SYS_FS_UNMOUNT
    bne check_fs_stat
    jsr _fs_unmount
    jmp irq_restore
check_fs_stat:
    cmp #6                  ; SYS_FS_STAT
    bne check_fs_close
    jsr _fs_stat
    jmp irq_restore
check_fs_close:
    cmp #7                  ; SYS_FS_CLOSE
    bne check_fs_seek
    jsr _fs_close
    jmp irq_restore
check_fs_seek:
    cmp #8                  ; SYS_FS_SEEK
    bne check_fs_read
    jsr _fs_seek
    jmp irq_restore
check_fs_read:
    cmp #9                  ; SYS_FS_READ
    bne check_fs_getbyte
    jsr _fs_read
    jmp irq_restore
check_fs_getbyte:
    cmp #10                 ; SYS_FS_GETBYTE
    bne irq_restore
    jsr _fs_getbyte
irq_restore:
    pla
    tay
    pla
    tax
    pla
    rti
.endproc
