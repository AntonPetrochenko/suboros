; hello/main.asm — "Hello PRG!" demo for SuborOS.
;
; Demonstrates: SC_ALLOC, ZP-indirect copy into heap buffer,
; SYS_PUT_STRING from that buffer, SC_FREE, SC_EXIT_PROCESS.
;
; Entry point: $8008 (immediately after the 8-byte PRG header).
; No RAM segment — all data lives in this ROM bank or in the OS heap.

.include "../include/os_abi.inc"
.include "../include/prg_header.mac"

.segment "CODE"

    PRG_HEADER_ROM          ; 8-byte header at $8000; code begins at $8008

entry:                      ; $8008 — OS jumps here

    ; Allocate a buffer large enough for the message (11 bytes + nul = 12).
    ALLOC 12
    lda SC_RV0
    ora SC_RV1
    beq exit_now            ; OOM — nothing to print, just exit

    ; Store allocated pointer into _usr_ptr ($2A/$2B).
    lda SC_RV0
    sta $2A                 ; _usr_ptr lo
    lda SC_RV1
    sta $2B                 ; _usr_ptr hi

    ; Copy null-terminated message from ROM into the heap buffer.
    ldy #0
copy_loop:
    lda msg, y
    sta ($2A), y
    beq copy_done
    iny
    bne copy_loop           ; Y can't wrap — message is < 256 bytes
copy_done:

    ; SYS_PUT_STRING: x=2, y=12, ptr=buffer.
    lda #SYS_PUT_STRING
    sta SC_NUM
    lda #2
    sta SC_P0               ; tile column
    lda #12
    sta SC_P1               ; tile row
    lda $2A
    sta SC_P2               ; string ptr lo
    lda $2B
    sta SC_P3               ; string ptr hi
    brk #0

    ; Return heap buffer to OS.
    FREE $2A, $2B

exit_now:
    EXIT_PROCESS

; Message data — stays in ROM, copied to heap before printing.
msg:
    .byte "HELLO PRG!", 0
