; NMI, IRQ/BRK dispatch, and preemptive scheduler.
;
; Scheduler design:
;   NMI fires every ~60 Hz.  After updating frame_count / nmi_ready it checks
;   _proc_user_count and _no_sched.  If user processes are runnable and no
;   critical section is active it performs a round-robin context switch:
;     1. Save current process SP (+ rom_bank) into proc_table[_sched_cur_pid].
;     2. Advance _sched_cur_pid to the next ACTIVE slot (wrap 0-3).
;     3. Restore the next process's rom_bank (inline MMC1 $E000 write, no JSR).
;     4. Set SP = next.saved_sp, then pla/tay/pla/tax/pla/rti.
;
;   SYS_EXIT_PROCESS (14) is handled entirely in assembly because it must
;   manipulate SP directly to switch to the next process without returning to
;   the dying process.
;
; Stack partition (64 bytes each):
;   Slot 0 (kernel): $01C0-$01FF   base SP=$FF
;   Slot 1:          $0180-$01BF   base SP=$BF
;   Slot 2:          $0140-$017F   base SP=$7F
;   Slot 3:          $0100-$013F   base SP=$3F
;
; ProcEntry layout (8 bytes, must match proc.h):
;   +0 flags    +1 saved_sp    +2 rom_bank    +3 pad    +4..7 ipc[4]

.import _nmi_ready, _frame_count
.import _put_string, _beep, _set_prg_bank
.import _fs_mount, _fs_open, _fs_unmount, _fs_stat
.import _fs_close, _fs_seek, _fs_read, _fs_getbyte
.import _start_process, _ipc_write, _ipc_get
.import _sys_alloc, _sys_free
.import _proc_table, _proc_user_count
.import _prg_bank_cur
.import _ppu_queue
.importzp _sc_num
.importzp _sched_cur_pid, _sched_tmp, _sched_ptr, _no_sched
.importzp _ppu_q_tail, _ppu_q_busy, _ppu_q_writes

.export nmi_handler, irq_handler

; ProcEntry field offsets (must match proc.h typedef):
PROC_FLAGS       = 0
PROC_SAVEDSP     = 1
PROC_ROMBANK     = 2
PROC_FLAG_ACTIVE = $01

.segment "CODE"

; ---------------------------------------------------------------------------
.proc nmi_handler
    pha
    txa
    pha
    tya
    pha

    ; --- PPU queue drain ----------------------------------------------------
    ; Drain the deferred PPU write queue while we are in VBlank.  Producers
    ; outside the NMI handler enqueue COPY/FILL commands instead of touching
    ; $2006/$2007 directly; we walk the buffer here and perform the actual
    ; PPU writes.  The producer caps total PPU bytes per frame at
    ; PPU_FRAME_BUDGET (ppu.h) so this loop finishes inside VBlank.
    bit $2002                   ; reset write-toggle latch (clears bit 7 too)
    lda _ppu_q_busy
    bne drain_done              ; producer mid-enqueue; skip this frame
    lda _ppu_q_tail
    beq drain_done              ; queue empty

    ldx #0                      ; X = byte cursor into ppu_queue
drain_loop:
    lda _ppu_queue, x
    beq drain_finish            ; OP_END
    cmp #1
    beq do_copy
    cmp #2
    beq do_fill
    jmp drain_finish            ; unknown opcode → bail out safely

do_copy:
    inx                         ; past opcode
    lda _ppu_queue, x           ; addr hi
    sta $2006
    inx
    lda _ppu_queue, x           ; addr lo
    sta $2006
    inx
    lda _ppu_queue, x           ; len
    sta _sched_tmp
    inx
copy_byte:
    lda _ppu_queue, x
    sta $2007
    inx
    dec _sched_tmp
    bne copy_byte
    jmp drain_loop

do_fill:
    inx                         ; past opcode
    lda _ppu_queue, x           ; addr hi
    sta $2006
    inx
    lda _ppu_queue, x           ; addr lo
    sta $2006
    inx
    lda _ppu_queue, x           ; len
    sta _sched_tmp
    inx
    lda _ppu_queue, x           ; value
    inx                         ; advance past value byte for next command
fill_byte:
    sta $2007
    dec _sched_tmp
    bne fill_byte
    jmp drain_loop

drain_finish:
    lda #0
    sta _ppu_q_tail
    sta _ppu_q_writes
    sta _ppu_queue              ; ensure ppu_queue[0] = OP_END for safety

    ; Restore PPU state so the next frame renders with NT base 0, scroll
    ; (0,0).  PPU_ADDR writes during the drain clobbered t's NT-base and
    ; fine-Y bits; PPU_SCROLL writes also clear coarse X/Y.
    lda #$80                    ; CTRL_NMI_ON, NT base 0, VRAM inc +1
    sta $2000
    lda #0
    sta $2005
    sta $2005

drain_done:
    ; --- Frame counter / nmi_ready -----------------------------------------
    inc _frame_count
    bne :+
    inc _frame_count+1
:
    lda #1
    sta _nmi_ready

    ; --- Scheduler ---
    lda _proc_user_count
    bne :+
    jmp nmi_done        ; no user processes
:
    lda _no_sched
    beq :+
    jmp nmi_done        ; critical section in progress
:
    ; 1. Compute index: _sched_cur_pid * 8 → A
    lda _sched_cur_pid
    asl
    asl
    asl                 ; A = cur_pid * 8

    ; Load address of proc_table[cur_pid] into ZP pointer _sched_ptr
    clc
    adc #<_proc_table
    sta _sched_ptr
    lda #>_proc_table
    adc #0
    sta _sched_ptr+1

    ; Save SP into proc_table[cur_pid].saved_sp
    tsx
    txa
    ldy #PROC_SAVEDSP
    sta (_sched_ptr), y

    ; Save current ROM bank into proc_table[cur_pid].rom_bank
    lda _prg_bank_cur
    ldy #PROC_ROMBANK
    sta (_sched_ptr), y

    ; 2. Find next active process (round-robin 0-3).
    lda _sched_cur_pid
find_next_loop:
    clc
    adc #1
    and #3              ; wrap 0-3
    sta _sched_tmp      ; candidate PID

    ; Load address of proc_table[candidate] into _sched_ptr
    asl
    asl
    asl                 ; A = candidate * 8
    clc
    adc #<_proc_table
    sta _sched_ptr
    lda #>_proc_table
    adc #0
    sta _sched_ptr+1

    ; Check flags.bit0 (PROC_FLAG_ACTIVE)
    ldy #PROC_FLAGS
    lda (_sched_ptr), y
    and #PROC_FLAG_ACTIVE
    bne found_next

    ; Try next candidate — wrap check so we don't spin forever
    lda _sched_tmp
    cmp _sched_cur_pid
    bne find_next_loop
    jmp nmi_done        ; looped all the way back: only 1 active slot

found_next:
    ; Update current PID
    lda _sched_tmp
    sta _sched_cur_pid

    ; 3. Restore next process's ROM bank (inline MMC1 $E000 write, no JSR).
    ;    _sched_ptr already points to proc_table[next].
    ldy #PROC_ROMBANK
    lda (_sched_ptr), y
    sta _prg_bank_cur       ; keep tracking variable in sync
    sta _sched_tmp          ; copy for 5-write bit-serial shift
    sta $E000               ; write bit 0
    lsr _sched_tmp
    lda _sched_tmp
    sta $E000
    lsr _sched_tmp
    lda _sched_tmp
    sta $E000
    lsr _sched_tmp
    lda _sched_tmp
    sta $E000
    lsr _sched_tmp
    lda _sched_tmp
    sta $E000               ; bit 4

    ; 4. Switch hardware SP to next process's saved_sp.
    ldy #PROC_SAVEDSP
    lda (_sched_ptr), y
    tax
    txs

    ; Restore Y, X, A from next process's stack frame, then RTI.
    pla
    tay
    pla
    tax
    pla
    rti

nmi_done:
    pla
    tay
    pla
    tax
    pla
    rti
.endproc

; ---------------------------------------------------------------------------
; BRK and hardware IRQ share this vector.  Distinguish by B flag (bit 4) in
; the saved P register set by the CPU for BRK but not hardware IRQ.
; After tsx, saved P is at $0104,X (CPU pushed PCH/PCL/P then we pushed A/X/Y).
.proc irq_handler
    pha
    txa
    pha
    tya
    pha
    tsx
    lda $0104,x
    and #$10            ; B flag set → BRK (syscall)
    bne is_brk
    jmp irq_restore     ; hardware IRQ — nothing to do, just RTI
is_brk:

    lda _sc_num
    cmp #0
    bne chk1
    jsr _put_string
    jmp irq_restore
chk1:
    cmp #1
    bne chk2
    jsr _beep
    jmp irq_restore
chk2:
    cmp #2
    bne chk3
    jsr _set_prg_bank
    jmp irq_restore
chk3:
    cmp #3
    bne chk4
    jsr _fs_mount
    jmp irq_restore
chk4:
    cmp #4
    bne chk5
    jsr _fs_open
    jmp irq_restore
chk5:
    cmp #5
    bne chk6
    jsr _fs_unmount
    jmp irq_restore
chk6:
    cmp #6
    bne chk7
    jsr _fs_stat
    jmp irq_restore
chk7:
    cmp #7
    bne chk8
    jsr _fs_close
    jmp irq_restore
chk8:
    cmp #8
    bne chk9
    jsr _fs_seek
    jmp irq_restore
chk9:
    cmp #9
    bne chk10
    jsr _fs_read
    jmp irq_restore
chk10:
    cmp #10
    bne chk11
    jsr _fs_getbyte
    jmp irq_restore
chk11:
    cmp #11
    bne chk12
    jsr _start_process
    jmp irq_restore
chk12:
    cmp #12
    bne chk13
    jsr _ipc_write
    jmp irq_restore
chk13:
    cmp #13
    bne chk14
    jsr _ipc_get
    jmp irq_restore
chk14:
    cmp #14
    bne chk15               ; not EXIT_PROCESS
    jmp do_exit_process     ; unlimited range jump to handler below
chk15:
    cmp #15
    bne chk16
    jsr _sys_alloc
    jmp irq_restore
chk16:
    cmp #16
    bne irq_restore
    jsr _sys_free

irq_restore:
    pla
    tay
    pla
    tax
    pla
    rti

; SYS_EXIT_PROCESS — does not return via irq_restore.
; Mark current slot inactive, find next active slot, context-switch into it.
; Placed after irq_restore to avoid branch-range constraints on chk14.
do_exit_process:
    ; Decrement proc_user_count.
    lda _proc_user_count
    beq exit_no_users       ; shouldn't happen, but be safe
    sec
    sbc #1
    sta _proc_user_count

    ; Clear PROC_FLAG_ACTIVE for current pid.
    lda _sched_cur_pid
    asl
    asl
    asl
    clc
    adc #<_proc_table
    sta _sched_ptr
    lda #>_proc_table
    adc #0
    sta _sched_ptr+1
    ldy #PROC_FLAGS
    lda #0
    sta (_sched_ptr), y     ; flags = 0 (inactive)

    ; Find next active process.
    lda _sched_cur_pid
exit_find_next:
    clc
    adc #1
    and #3
    sta _sched_tmp          ; candidate
    asl
    asl
    asl
    clc
    adc #<_proc_table
    sta _sched_ptr
    lda #>_proc_table
    adc #0
    sta _sched_ptr+1
    ldy #PROC_FLAGS
    lda (_sched_ptr), y
    and #PROC_FLAG_ACTIVE
    bne exit_restore
    lda _sched_tmp
    cmp _sched_cur_pid
    bne exit_find_next      ; keep searching

    ; Every slot (including kernel) is inactive — panic: hang.
exit_no_users:
    jmp exit_no_users

exit_restore:
    lda _sched_tmp
    sta _sched_cur_pid

    ; Restore ROM bank (inline MMC1 $E000 write).
    ldy #PROC_ROMBANK
    lda (_sched_ptr), y
    sta _prg_bank_cur
    sta _sched_tmp
    sta $E000
    lsr _sched_tmp
    lda _sched_tmp
    sta $E000
    lsr _sched_tmp
    lda _sched_tmp
    sta $E000
    lsr _sched_tmp
    lda _sched_tmp
    sta $E000
    lsr _sched_tmp
    lda _sched_tmp
    sta $E000

    ; Restore hardware SP and context of next process, then RTI.
    ldy #PROC_SAVEDSP
    lda (_sched_ptr), y
    tax
    txs
    pla
    tay
    pla
    tax
    pla
    rti
.endproc
