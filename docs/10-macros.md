
---

## 10. Convenience macros (`os_abi.inc`)

The include file [programs/include/os_abi.inc](programs/include/os_abi.inc) defines four macros to make common patterns concise:

```asm
; Print null-terminated string at (ptr_lo, ptr_hi) to tile column col, row row.
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

; Allocate N bytes. Result ptr in SC_RV0 (lo) / SC_RV1 (hi); both 0 = OOM.
.macro ALLOC n
    lda #SYS_ALLOC
    sta SC_NUM
    lda #(n)
    sta SC_P0
    brk #0
.endmacro

; Free pointer whose lo/hi bytes are in zp_lo, zp_hi (ZP addresses).
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
```

Two ca65 gotchas to be aware of when extending this file:

- The names `x` and `y` are reserved (they are register names) and cannot be used as macro parameter names. The existing `ALLOC n` and `PRINT col, row, …` avoid them.
- ca65 macros do not support `:` as a multi-statement separator inside the macro body — each statement must be on its own line.

There are no convenience macros yet for `SYS_BEEP`, `SYS_FS_*`, `SYS_START_PROCESS`, or `SYS_IPC_*`. Add new macros to `os_abi.inc` as patterns emerge.

---