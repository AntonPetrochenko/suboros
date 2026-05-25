## 13. Constraints & gotchas (cheat sheet)

- **16 KB hard cap.** Code + RODATA + reloc table + header ≤ 16 384 bytes. The ld65 `PRG` MEMORY area enforces this at link time; `mkprg.py` re-enforces it.
- **Single bank.** Programs run in-place from one MMC1 PRG bank. There is no concept of a multi-bank user program; if you outgrow 16 KB, you need to split into cooperating processes communicating via IPC.
- **64-byte hardware stack.** No recursion. Watch `jsr` depth, especially when leaf functions invoke `brk`: a syscall consumes ≥ 6 bytes of your stack (BRK frame + dispatcher A/X/Y pushes) plus the kernel handler's own `jsr` nesting on top.
- **No static RAM.** Every byte of mutable storage must be in the ZP user-scratch window (`$2A–$2F`), on the hardware stack, or obtained via `SYS_ALLOC`. A `BSS` segment in your `.cfg` is a build error.
- **Off-limits ZP.** Do not touch `$00–$19` — that's kernel and scheduler state. Do not touch ZP outside `$2A–$2F` and the syscall slots `$1A–$24`.
- **Off-limits RAM.** Do not read or write `$0200–$07FF` or `$6000–$7FFF` — both are kernel territory.
- **Preemption.** Every non-syscall instruction can be interrupted by an NMI context-switch. Treat any shared state with another process (i.e., the 4 IPC bytes) as the only contract. Within your own process you do not need to worry about races, since only one core exists and your registers/SP are saved and restored together.
- **Always `EXIT_PROCESS`.** Falling off the end of code executes the `$FF` padding and almost certainly crashes the system.
- **No reloc loader.** The kernel does not honor the reloc-table flag yet. `PRG_HEADER_ROM` correctly emits no relocations; don't try to add any.
- **`brk #0`, not `brk`.** ca65 will accept either, but the 6502 BRK is a 2-byte instruction — the operand byte is silently consumed by the CPU regardless. Stick with `brk #0` for clarity. (The convenience macros already do this.)

More importantly:

- **C user programs.** Only the kernel currently uses cc65. There is no per-program C startup, no segregated cc65 ZP, and no link configuration for user-program C. User programs are ca65 assembly only.
- **Reloc loader.** The header field exists; the kernel does not consume it. Programs cannot currently be loaded into RAM.
- **Input.** There is no syscall to read the joypad or any keyboard. User programs cannot read input. (The kernel has a polling routine but it isn't exposed.)
- **Synchronization primitives.** Inter-process communication is four raw bytes per process. No locks, no semaphores, no event delivery.
- **Filesystem writes.** The filesystem is read-only at runtime. New files can only enter the system via `tools/mkfs.py` on the host.
- **Multitasking guarantees.** Round-robin preemption is the only scheduling policy. No priorities, no sleep, no yield, no wait-on-IPC.

When these gaps are filled, this document needs an update.
