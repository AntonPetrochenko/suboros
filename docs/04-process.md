

## 4. How a process runs

Source: [src/proc.h](src/proc.h), [src/proc.c](src/proc.c), [src/nmi.asm](src/nmi.asm).

### Process slots

The kernel maintains a 4-entry process table. Slot 0 is the kernel; slots 1–3 are user processes. At most three user processes may exist simultaneously.

### Hardware stack partition (64 bytes per slot)

| slot | range | base SP |
|---|---|---|
| 0 (kernel) | `$01C0–$01FF` | `$FF` |
| 1 | `$0180–$01BF` | `$BF` |
| 2 | `$0140–$017F` | `$7F` |
| 3 | `$0100–$013F` | `$3F` |

A user program therefore has **64 bytes of hardware stack**. That is enough for a moderate `jsr` depth and a handful of pushed registers, but recursion is not viable. The kernel never relocates your stack frames between slots — your stack lives in the slot you were assigned and stays there until you exit.

### ROM banking

Each process owns the MMC1 PRG ROM bank that its `.prg` file body sits in. When the kernel loads your program (see §12), it reads the bank number of the first extent of your file (the file system places the entire file in a single bank — see §13 on the single-extent constraint) and stores that in `proc_table[pid].rom_bank`. On every context switch the scheduler in `nmi.asm` re-issues the MMC1 5-write sequence to map your bank back at `$8000` before resuming you. Your code is therefore allowed to assume `$8000–$BFFF` is its own bank at all times.

The kernel runs in fixed bank 7 at `$C000–$FFFF`. Switching the `$8000` window has no effect on it, so syscalls (which live in the kernel) work regardless of which user bank is currently mapped.

### Scheduling

The NMI handler runs ~60 Hz. After draining the PPU write queue and bumping the frame counter it consults `proc_user_count` and the ZP byte `_no_sched`:

- If `proc_user_count == 0`, no scheduling happens.
- If `_no_sched != 0`, the current process keeps running (critical section).
- Otherwise the scheduler saves the current process's SP and rom_bank into its `proc_table[]` entry, finds the next slot whose `PROC_FLAG_ACTIVE` is set (round-robin 0→1→2→3→0), restores that process's rom_bank, sets `SP = next.saved_sp`, and `RTI`s into it.

The scheduler is **preemptive**. Any non-syscall instruction in your program may be interrupted by an NMI. Syscalls themselves are atomic from your perspective: the BRK dispatcher in `irq_handler` runs to completion before the scheduler gets a turn at the next NMI.

`_no_sched` is the ZP byte at `$29`; writing non-zero to it defers context switching across a critical section, and writing zero re-enables it. User code rarely needs this — there are no user-visible kernel data structures that a context switch would corrupt mid-update — and touching it is itself a violation of "don't touch kernel ZP" (§5), so leave it alone unless you have a specific reason.

### Initial register state on first dispatch

When a freshly created process is scheduled in for the first time, the synthesized stack frame restores:

- `PC = $8000 + entry_offset` (typically `$8008`)
- `A = 0`, `X = 0`, `Y = 0`, `P = 0` (no flags set, including I — interrupts enabled)
- `SP = base − 6` (base from the table above; the 6 bytes were used by the synthesized PCH/PCL/P/A/X/Y frame)

