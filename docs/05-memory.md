
## 5. Memory map (what user code may touch)

Sources: [src/zp.asm](src/zp.asm), [programs/build.bat](programs/build.bat) (BSS rejection).

### Zero page — user-program scratch

These slots exist precisely for user programs and are **not** touched by the kernel C runtime, so their contents survive context switches:

| address | symbol | size | purpose |
|---|---|---|---|
| `$2A–$2B` | `_usr_ptr` | 2 | indirect pointer (works with `lda (zp),y` etc.) |
| `$2C–$2F` | `_usr_tmp` | 4 | scratch bytes |

Hardcoding these addresses in your `.asm` is the recommended pattern. `os_abi.inc` does not re-export these names; reference them as raw addresses `$2A`–`$2F`, which is what `programs/hello/main.asm` does today.

### Zero page — syscall registers

Write parameters here before `brk`, read return values after:

| address | symbol | direction |
|---|---|---|
| `$1A` | `SC_NUM` | in |
| `$1B–$20` | `SC_P0 .. SC_P5` | in |
| `$21–$24` | `SC_RV0 .. SC_RV3` | out |

The symbols are defined in [programs/include/os_abi.inc](programs/include/os_abi.inc).

### Zero page — off-limits to user programs

Every ZP byte that is **not** a syscall slot (`$1A–$24`) and **not** in the user-scratch window (`$2A–$2F`) belongs to the kernel:

| range | what's there |
|---|---|
| `$00–$19` | cc65 runtime: `c_sp`, `sreg`, `regsave`, `regbank`, `ptr1`–`ptr4`, `tmp1`–`tmp4`. Used by every kernel C function. |
| `$25–$29` | Scheduler state: `_sched_cur_pid` (`$25`), `_sched_tmp` (`$26`), `_sched_ptr` (`$27–$28`), `_no_sched` (`$29`). Modified inside the NMI handler. |
| `$30–$32` | PPU write queue flags: `_ppu_q_tail` (`$30`), `_ppu_q_busy` (`$31`), `_ppu_q_writes` (`$32`). Touched by every `SYS_PUT_STRING` and every NMI. |

Touching any of these will at best corrupt the kernel and at worst crash the system. The simplest mental rule: **only `$1A–$24` (syscall slots) and `$2A–$2F` (user scratch) are yours.**

### Hardware stack

Use only the 64-byte window assigned to your slot (see §4). The kernel sets up SP correctly before your entry runs; just don't push or pull past your slot's boundaries.

### RAM ($0200–$07FF and $6000–$7FFF)

The entire `$0200–$07FF` region is OS BSS — process table, file-system mount/handle tables, the 256-byte heap pool, and assorted kernel variables. User programs may **not** define static RAM there. The only legal way to obtain mutable RAM is `SYS_ALLOC`, which returns a pointer into the kernel heap.

PRG RAM at `$6000–$7FFF` is reserved for the kernel (its filesystem may map any of four 8 KB PRG-RAM banks here via `SYS_SET_PRG_BANK`). User programs should not assume it is reachable, addressable, or stable.

### ROM ($8000–$BFFF)

This is your own bank — code (`CODE` segment) and read-only data (`RODATA` segment) only. The bank is read-only on the cartridge, so you can't use it for variables.