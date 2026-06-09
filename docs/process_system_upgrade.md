# Process System Upgrade Plan

## Goals

- Support more user processes
- Support programs from two distinct sources (PRG ROM and PRG RAM)
- Eliminate relocation
- Enable C, C++, and Rust user programs via LLVM MOS
- Fix the syscall register race condition

---

## 1. Two Program Sources, Fixed Load Addresses

Programs originate from exactly one of two sources. Because the load address is fixed per source, relocation is unnecessary and will be removed entirely.

| Source | Load address | Bank window |
|---|---|---|
| PRG ROM | `$8000` | Switchable PRG ROM bank (`$8000–$BFFF`) |
| PRG RAM | `$6000` | Switchable PRG RAM bank (`$6000–$7FFF`) |

All ROM addresses and ROM-relative pointers are authored at `$8000`. All RAM program addresses are authored at `$6000`. No patching at load time.

## 2. PRG Header Simplification

Remove the relocation fields from the `.prg` header. New layout:

| offset | size | meaning |
|---|---|---|
| 0–1 | 2 bytes | magic `'P' 'R'` |
| 2 | 1 byte | version |
| 3 | 1 byte | source type: `0` = PRG ROM, `1` = PRG RAM |
| 4–5 | u16 LE | `entry_offset` |

`reloc_table_size`, `flags`, and the relocation table itself are gone. `mkprg.py`, `prg_header.mac`, and the format docs all need updating.

---

## 3. PRG RAM Bank Layout

SXROM provides 4 × 8 KB PRG RAM banks (`$6000–$7FFF`).

| PRG RAM bank | Purpose |
|---|---|
| 0–2 | Loadable RAM programs — one bank per active RAM process |
| 3 | WRAM pages for ROM processes (32 × 256-byte pages; only 3 slots ever needed) |

**ROM processes** run from `$8000` in their ROM bank. Mutable storage is a single 256-byte WRAM page in PRG RAM bank 3, assigned at `start_process` time by slot index (`slot × $100`).

**RAM processes** are copied into a free bank (0–2) at load time and run from `$6000`. The remainder of the bank after the program image is available as heap.

---

## 4. Process Count

The binding constraint is the **hardware stack** (`$0100–$01FF`, 256 bytes), not zero page.

| Bytes/slot | Total slots | User processes |
|---|---|---|
| 64 (current) | 4 | 3 |
| **32 (target)** | **8** | **7** |

32 bytes per slot is the safe floor given worst-case syscall depth (`start_process` → `fs_open` → `strncmp` plus IRQ entry frame ≈ 15–20 bytes minimum). Zero page has 205 bytes free (`$33–$FF`) and is not a concern.

`PROC_MAX` changes from 4 to 8. `ProcEntry` gains `source_type` (ROM/RAM), `wram_page`, and `ram_bank` fields. The `proc_base_sp` table extends to cover all 8 slots.

---

## 5. Context Switch — NMI Entry and Kernel Stack

**The CPU pushes before you can act.** When NMI fires, the CPU unconditionally pushes PCH, PCL, and P onto whatever SP points to before the first instruction of the handler runs. This cannot be avoided or pre-empted.

Those three bytes are pushed into the currently-running process's stack region — which is correct. They are part of that process's frame and will be included when the stack is copied out.

**RTI does not restore SP.** It only pops P and PC off whatever the stack is pointing at when it executes. If SP is still at the kernel base when RTI fires, it pops garbage as the return address and flags. SP must be restored to the process frame before RTI.

**Correct entry sequence.** Push A, X, Y to the process stack first — they become part of the process frame. Then snapshot SP (all 6 bytes accounted for), then redirect to the kernel stack:

```asm
pha              ; A → process stack
txa
pha              ; X → process stack
tya
pha              ; Y → process stack
tsx              ; X = saved_sp (SP after all 6 bytes pushed)
stx _sched_tmp   ; park saved_sp in kernel ZP before X is clobbered
ldx #$FF
txs              ; SP → kernel stack, safe to JSR freely now
```

The snapshotted value in `_sched_tmp` is what gets stored into `proc_table[cur_pid].saved_sp`. On exit, SP must be restored to `saved_sp` before `pla/tay / pla/tax / pla / rti` so that the pops and RTI operate on the correct process frame.

---

## 6. Context Switch — Hardware Stack Copy

Rather than mapping all per-process stack storage inside `$0100–$01FF`, inactive processes have their hardware stack region copied out to per-process storage on switch-out and copied back on switch-in. The copy size is dynamic: `base_sp - saved_sp` bytes, derived from the already-stored `saved_sp` value. Shallow processes (idle loops, simple wait states) cost proportionally less.

**Cost at 14 cycles/byte (NTSC, 29 829 cycles/frame):**

| Stack depth | Copy (one direction) | Round-trip (save + restore) |
|---|---|---|
| 16 bytes | ~224 cycles | ~450 cycles |
| 32 bytes | ~450 cycles | ~900 cycles |
| 128 bytes (worst) | ~1 800 cycles | ~3 600 cycles (~12% of frame) |

Recursion increases copy cost. This is accepted.

The scheduler (`nmi.asm`) performs save-out and load-in inline. The bottom half of the hardware stack (`$0100–$017F`) remains kernel-private and is never copied.

---

## 7. Context Switch — Full Step-by-Step Sequence

The entry/exit concerns above, combined with the ZP race fix (§9) and hardware stack copy (§6), produce non-obvious ordering constraints. The following traces a complete A→B→A switch with every assumption made explicit. Identified breaks are marked inline and addressed in the implementation notes that follow.

---

**Process A doing its things**
- SP in process A's stack region
- ZP `$1A–$52` contains process A's live state
- Process A's ROM/RAM bank selected

---

**NMI fires**
- CPU pushes PCH, PCL, P onto process A's stack (SP -= 3) — automatic, unavoidable ✓
- CPU jumps to NMI vector in fixed bank `$C000` ✓

---

**NMI handler entry**
- `pha / txa,pha / tya,pha` — A, X, Y pushed to process A's stack (SP -= 3 more) ✓
- `tsx` — snapshots process A's SP (6 below pre-NMI) into X ✓
- `stx _sched_tmp` — parks that SP value in kernel ZP ✓
- `ldx #$FF / txs` — SP redirected to kernel stack ✓

---

**Kernel does its things (PPU drain, frame counter)**
- All JSRs, pushes use kernel stack safely ✓

---

**Scheduler: save process A's state**
- Computes `proc_table[cur_pid]` address into `_sched_ptr` ✓
- `tsx / txa / sta (_sched_ptr),y` to save SP into `proc_table[A].saved_sp` ← **that's where we break!!!** — `tsx` now gives the KERNEL stack SP, not process A's SP. Should be reading `_sched_tmp` instead.
- Saves `prg_bank_cur` into `proc_table[A].rom_bank` ✓
- Saves PRG RAM bank into `proc_table[A].ram_bank` ✓

---

**Scheduler: copy process A's hardware stack to save buffer**
- Byte count = `proc_A_base_sp - _sched_tmp` (dynamic length) ✓
- Copy from `$0100 + _sched_tmp + 1` up through `$0100 + proc_A_base_sp` to process A's BSS save buffer ✓
- A, X, Y, P, PCL, PCH all sitting in there safely ✓

---

**Scheduler: save process A's ZP window**
- Copy `$1A–$24` to process A's ZP save buffer ✓
- Skip `$25–$32` — kernel-owned (scheduler vars, PPU queue flags), not per-process ← **that's where we break!!!** if we naively copy the whole `$1A–$52` range as one block, we save and later restore kernel scheduler state mid-operation, clobbering `_sched_ptr`, `_sched_tmp` etc. while we still need them. Must be three separate chunks: `$1A–$24`, `$2A–$2F`, `$33–$52`.
- Copy `$2A–$2F` to save buffer ✓
- Copy `$33–$52` to save buffer ✓

---

**Scheduler: find next active process (process B)**
- Round-robin scan of `proc_table` via `_sched_ptr` ✓
- Updates `_sched_cur_pid` to B ✓

---

**Scheduler: restore process B's banks**
- Write `proc_table[B].rom_bank` to MMC1 `$E000` ✓
- Write `proc_table[B].ram_bank` to MMC1 `$A000` ✓

---

**Scheduler: restore process B's ZP window**
- Copy process B's saved `$1A–$24` → ZP ✓
- Copy process B's saved `$2A–$2F` → ZP ✓
- Copy process B's saved `$33–$52` → ZP ← **that's where we break!!!** — `_sched_ptr` lives at `$27–$28`, inside the kernel-owned `$25–$32` gap. If we're using `_sched_ptr` to drive the ZP copy loop itself, we can't have already restored process B's copy of that range. Kernel ZP (`$25–$32`) must remain untouched throughout. Fine as long as the chunks are correct, but the loop machinery itself must only use kernel ZP bytes.

---

**Scheduler: restore process B's hardware stack**
- Copy from process B's BSS save buffer back into `$0100` region at the correct offset ✓
- `proc_table[B].saved_sp` gives the destination and byte count ✓

---

**NMI handler exit**
- `ldx proc_table[B].saved_sp / txs` — SP now points into process B's stack frame ✓
- `pla/tay / pla/tax / pla` — restores process B's Y, X, A from its stack ✓
- `rti` — pops P, PCL, PCH from process B's stack ✓

---

**Process B doing its things**
- SP in process B's region ✓
- ZP `$1A–$52` contains process B's live state ✓
- Process B's banks selected ✓

---

*...time passes, NMI fires again, same sequence, eventually B is saved and A is restored...*

---

**Process A doing its things again** ✓

---

**Breaks to iron out:**
1. `tsx` in the scheduler's SP-save must be replaced with a read of `_sched_tmp`
2. ZP save/restore must be three non-contiguous chunks, never touching `$25–$32`
3. ZP copy loop machinery must only use bytes from the kernel-owned gap — can't use registers that overlap the ranges being written

---

## 8. LLVM MOS for User Programs

User programs may be written in C, C++, or Rust via [LLVM MOS](https://llvm-mos.org). The kernel remains cc65. The two toolchains coexist cleanly because user programs are separate binaries that communicate with the kernel exclusively via BRK — they never link against the kernel.

LLVM MOS uses a ZP imaginary-register file (default 32 bytes, `__rc0`–`__rc31`). Its software stack pointer lives in this file. The kernel assigns LLVM MOS the ZP window starting immediately after the syscall registers (see §9). User programs are compiled with the ZP base offset to that window.

Syscall glue is a header (`os_abi.h`) wrapping each syscall as an inline function using inline asm to load ZP `$1A–$24` and fire BRK — equivalent to the existing `os_abi.inc` for assembly programs.

ROM processes in C should be written shallow: their WRAM page is 256 bytes, which is tight for non-trivial stack frames. RAM processes have the remainder of their 8 KB bank as heap.

---

## 9. Per-Process ZP Window — Syscall Race Fix

**The race:** syscall parameter setup requires multiple instructions before BRK. An NMI can fire between any of them, switch to another process, which overwrites the shared syscall registers (`$1A–$24`) with its own parameters. When the original process resumes and fires BRK, it dispatches with corrupted arguments.

**The fix:** the ZP range `$1A–$52` is per-process state, saved on switch-out and restored on switch-in by the scheduler.

| Range | Contents |
|---|---|
| `$1A–$24` | Syscall registers (`_sc_num`, `_sc_p0–p5`, `_sc_rv0–rv3`) |
| `$25–$32` | Scheduler scratch + user scratch + PPU queue flags (kernel-owned, not saved per-process) |
| `$33–$52` | LLVM MOS imaginary register file (`__rc0`–`__rc31`) |

The kernel reads and writes `$1A–$24` at fixed ZP addresses as before. Correctness is guaranteed because by the time any process fires BRK, the scheduler has already restored that process's ZP window into those addresses.

**ZP save cost addition:** `$1A–$52` = 57 bytes × 14 cycles ≈ 800 cycles per context switch, added to the hardware stack copy.

---

## 10. Changes Summary

| Area | Change |
|---|---|
| `src/proc.h` | `PROC_MAX` → 8; add `source_type`, `wram_page`, `ram_bank` fields to `ProcEntry` |
| `src/proc.c` | `proc_base_sp` extended; `start_process` forks on ROM vs RAM path; WRAM page assigned by slot |
| `src/nmi.asm` | Entry rewritten: `tsx/txa/ldx #$FF/txs` before any pushes to claim kernel stack; hardware stack copy (dynamic length) on context switch; ZP window `$1A–$52` save/restore; PRG RAM bank (`$A000`) save/restore added alongside ROM bank (`$E000`) |
| `suboros.cfg` | No structural change; linker config for RAM programs is a new separate template |
| `tools/mkprg.py` | Remove reloc fields; emit `source_type` byte |
| `programs/include/prg_header.mac` | `PRG_HEADER_ROM` / `PRG_HEADER_RAM` macros; remove reloc fields |
| `programs/include/os_abi.h` | New C/C++ syscall header for LLVM MOS programs |
| `docs/03-format.md` | Update header layout; remove relocation section |
| `docs/06-linker.md` | Add RAM program linker config template (`start=$6000, size=$2000`) |
