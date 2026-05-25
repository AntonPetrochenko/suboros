## 7. The entry point and minimal program

Minimal program skeleton:

```asm
.include "../include/os_abi.inc"
.include "../include/prg_header.mac"

.segment "CODE"

    PRG_HEADER_ROM          ; emits 8 bytes at $8000

entry:                      ; $8008 — OS jumps here
    ; ... your code ...
    EXIT_PROCESS
```

On entry your process has:

- Its own MMC1 PRG bank mapped at `$8000`.
- A clean stack frame (A = X = Y = P = 0) and ~58 bytes of usable hardware stack (base SP − 6).
- No allocated RAM; no open file handles.
- Preemption enabled — the next NMI may context-switch you out at any non-syscall boundary.

You **must** terminate by calling `SYS_EXIT_PROCESS`. Falling off the end of your code reaches the `$FF`-filled tail of the bank — on the 2A03 (NMOS 6502) `$FF` is an undocumented opcode (`ISC $FFFF,X`), so execution wanders into undefined behavior, almost certainly a crash.
