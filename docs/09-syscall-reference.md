## 9. Syscall reference

Sources: [src/syscall.h](src/syscall.h), [programs/include/os_abi.inc](programs/include/os_abi.inc), [src/fs.h](src/fs.h), [src/alloc.h](src/alloc.h), [src/proc.h](src/proc.h).

| # | name | inputs | outputs | notes |
|---|---|---|---|---|
| 0 | `SYS_PUT_STRING` | P0 = column, P1 = row, P2/P3 = ptr lo/hi to a null-terminated string | — | Enqueued into the VBlank PPU queue; the caller blocks until the queue drains. |
| 1 | `SYS_BEEP` | P0 = frequency, P1 = duration | — | Plays one APU pulse 1 tone, timed by a spin loop (consumes wall-clock time). |
| 2 | `SYS_SET_PRG_BANK` | P0 = PRG-RAM bank 0–3 | — | Kernel-facing; maps one 8 KB PRG-RAM bank at `$6000–$7FFF`. User programs should not call this. |
| 3 | `SYS_FS_MOUNT` | P0/P1 = TOC ptr, P2 = device number | RV0 = mount slot, or `$FF` on failure | |
| 4 | `SYS_FS_OPEN` | P0 = mount slot, P1/P2 = name ptr, P3/P4 = ptr to caller's 8-byte VA buffer | RV0 = handle (0–3), or `$FF`; writes initial VA into the caller buffer | |
| 5 | `SYS_FS_UNMOUNT` | P0 = mount slot | RV0 = 0 ok, `$FF` bad slot | Also frees any open handles on that slot. |
| 6 | `SYS_FS_STAT` | P0 = mount slot, P1/P2 = name ptr | RV0/RV1 = file size lo/hi (sum of extents); `$FF` on error | |
| 7 | `SYS_FS_CLOSE` | P0/P1 = VA ptr | RV0 = 0 ok, `$FF` bad handle | Zeroes the VA. |
| 8 | `SYS_FS_SEEK` | P0/P1 = VA ptr, P2/P3/P4 = 24-bit linear offset | — | Resolves the offset against the extent list and rewrites the entire 8-byte VA. |
| 9 | `SYS_FS_READ` | P0/P1 = VA ptr, P2/P3 = dest ptr, P4/P5 = byte count | RV0/RV1 = bytes actually copied | May be less than requested at EOF. Updates all 8 VA bytes. |
| 10 | `SYS_FS_GETBYTE` | P0/P1 = VA ptr | RV0 = byte value, RV1 = `$00` ok / `$FF` EOF | Updates VA. |
| 11 | `SYS_START_PROCESS` | P0 = mount slot, P1/P2 = name ptr | RV0 = PID (1–3) or `$FF`, RV1 = error code on failure | Errors: 1 = no free slot, 2 = file not found, 3 = bad handle after open, 4 = header read returned < 8 bytes, 5 = bad PRG magic. |
| 12 | `SYS_IPC_WRITE` | P0 = target PID, P1/P2/P3/P4 = four bytes | — | Writes into `proc_table[pid].ipc[0..3]`. Silently no-ops on invalid or inactive PID. |
| 13 | `SYS_IPC_GET` | P0 = source PID | RV0–RV3 = `proc_table[pid].ipc[0..3]`; all `$FF` if PID invalid or inactive | |
| 14 | `SYS_EXIT_PROCESS` | — | never returns | Clears your slot's active flag, decrements `proc_user_count`, context-switches to the next active process. Handled directly in assembly because it must rewrite SP without ever returning. |
| 15 | `SYS_ALLOC` | P0 = size in bytes | RV0/RV1 = ptr lo/hi (both `$00` means out-of-memory) | First-fit over a single 256-byte pool shared by all processes. |
| 16 | `SYS_FREE` | P0/P1 = ptr lo/hi | — | Idempotent. Coalescing of adjacent free blocks happens on the next `SYS_ALLOC`, not here. |

### Notes per syscall

**`SYS_PUT_STRING` (0).** The string is enqueued as a COPY command into a deferred PPU write queue. The producer (your syscall call) returns once the NMI drainer has emptied the queue. The per-frame budget is bounded — printing very large amounts of text in a tight loop can span multiple frames.

**`SYS_BEEP` (1).** Spin-loops for the duration of the tone, so this is effectively a `sleep` from your point of view. Other processes still run via NMI preemption.

**`SYS_FS_*` (3–10).** A *VA* (virtual address) is an 8-byte caller-owned cursor that uniquely identifies a position within an open file. Its layout is documented in `src/fs.h`; user code does not need to interpret its bytes, only allocate 8 bytes for it (heap, ZP scratch, or RODATA-copied-to-heap) and pass the pointer to every FS call. Open handles are a global pool of 4 entries shared across all processes — releasing them with `SYS_FS_CLOSE` is mandatory.

**`SYS_START_PROCESS` (11).** P0 names the *mount slot* to load from — the kernel mounts the ROM filesystem at slot 0 during boot, so passing `0` selects the on-cart filesystem. Names are the 8-character filename stem packed by `tools/mkfs.py` (the same names the file browser shows). On success RV0 is the new PID; on failure RV0 is `$FF` and RV1 carries the error code (1–5 as above).

**`SYS_IPC_WRITE` / `SYS_IPC_GET` (12/13).** The IPC channel is 4 bytes per process, owned by that process — anyone can write any process's IPC bytes, and any process can read them. This is intentionally minimal: there is no synchronization primitive, no ringbuffer, and no notification. Build whatever protocol you need on top of these 4 bytes.

**`SYS_EXIT_PROCESS` (14).** Never returns. Implemented in assembly (`do_exit_process` in `nmi.asm`) because it must clear `PROC_FLAG_ACTIVE`, advance to the next process, and switch SP without ever returning to the dying process.

**`SYS_ALLOC` / `SYS_FREE` (15/16).** The kernel heap is a single 256-byte first-fit pool shared by every process. Each allocation has 2 bytes of overhead. Free blocks are coalesced lazily on the next allocation attempt. Failure (out-of-memory) returns `RV0 = RV1 = 0`, which corresponds to a null pointer.
