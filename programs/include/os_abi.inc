; os_abi.inc — SuborOS ABI constants for user programs.
;
; These are the zero-page addresses of the OS syscall registers as laid out
; in src/zp.asm.  Keep in sync with that file if ZP layout ever changes.
;
; Usage in your program:
;   .include "../include/os_abi.inc"
;
; Then use constants like SC_NUM, SC_P0 .. SC_P5, SC_RV0 .. SC_RV3 directly
; as zero-page addresses:
;   lda #SYS_EXIT_PROCESS
;   sta SC_NUM
;   brk #0

; --- Syscall number and parameter registers (ZP addresses) ---
SC_NUM = $1A    ; write syscall number here before BRK
SC_P0  = $1B    ; input parameters
SC_P1  = $1C
SC_P2  = $1D
SC_P3  = $1E
SC_P4  = $1F
SC_P5  = $20
SC_RV0 = $21    ; return values (read after BRK returns)
SC_RV1 = $22
SC_RV2 = $23
SC_RV3 = $24

; --- Syscall numbers ---
SYS_PUT_STRING    = 0
SYS_BEEP          = 1
SYS_SET_PRG_BANK  = 2
; 3-10: filesystem (see fs.h)
SYS_START_PROCESS = 11
SYS_IPC_WRITE     = 12
SYS_IPC_GET       = 13
SYS_EXIT_PROCESS  = 14
SYS_ALLOC         = 15
SYS_FREE          = 16

; --- Convenience macros ---
; Note: ca65 does not allow 'x' or 'y' as macro parameter names (register
; names are reserved) and does not support ':' multi-statement inside macros.

; Print null-terminated string at (ptr_lo, ptr_hi) to tile column col, row row.
; Destroys A.
.macro PRINT col, row, ptr_lo, ptr_hi
    lda #SYS_PUT_STRING
    sta SC_NUM
    lda #(col)
    sta SC_P0
    lda #(row)
    sta SC_P1
    lda #(ptr_lo)
    sta SC_P2
    lda #(ptr_hi)
    sta SC_P3
    brk #0
.endmacro

; Allocate N bytes.  Result ptr in SC_RV0 (lo) / SC_RV1 (hi); both 0 = OOM.
.macro ALLOC n
    lda #SYS_ALLOC
    sta SC_NUM
    lda #(n)
    sta SC_P0
    brk #0
.endmacro

; Free pointer whose lo/hi bytes are in zp_lo, zp_hi (ZP addresses of variables).
.macro FREE zp_lo, zp_hi
    lda #SYS_FREE
    sta SC_NUM
    lda zp_lo
    sta SC_P0
    lda zp_hi
    sta SC_P1
    brk #0
.endmacro

; Exit current process (never returns).
.macro EXIT_PROCESS
    lda #SYS_EXIT_PROCESS
    sta SC_NUM
    brk #0
.endmacro
