## 12. How a program is loaded and launched (end to end)

Source: [src/proc.c](src/proc.c) (`start_process`).

When some kernel code (for example the file browser) calls `SYS_START_PROCESS(slot, name)`:

1. The kernel scans slots 1–3 for an inactive entry. If all three are taken, it returns `RV0 = $FF`, `RV1 = 1`.
2. It calls `fs_open()` against the given mount slot and name. If the file is missing, returns `RV0 = $FF`, `RV1 = 2`.
3. It reads the first extent of the opened handle to determine which PRG ROM bank the file body lives in. This bank number becomes the process's permanent `rom_bank` — **the program is not copied; it runs in-place from wherever `mkfs.py` packed it.** This is also why programs are limited to 16 KB (one MMC1 bank): a program that spanned two banks would have two `rom_bank` values and the scheduler can only switch one at a time. The packer (`tools/mkfs.py`) places each file in a single bank, satisfying this constraint.
4. It reads the first 8 bytes (the PRG header) into a local buffer. If `fs_read` returns fewer than 8 bytes, returns `RV0 = $FF`, `RV1 = 4`.
5. It closes the file handle. (The kernel keeps no per-process handle; once the process is running it can open its own files via `SYS_FS_OPEN`.)
6. It validates the magic `'P','R'`. If wrong, returns `RV0 = $FF`, `RV1 = 5`.
7. It reads `entry_offset` from header bytes 4–5 (LE) and computes `PC = $8000 + entry_offset`.
8. It writes a synthesized stack frame at the top of the process's hardware-stack slot (PCH, PCL, P=0, A=0, X=0, Y=0), sets `saved_sp = base − 6`, sets `rom_bank` to the value captured in step 3, sets `flags = PROC_FLAG_ACTIVE`, clears the four IPC bytes, and increments `proc_user_count`.
9. On the very next NMI, the scheduler sees `proc_user_count > 0`, round-robins to your slot, restores your `rom_bank`, sets SP to `saved_sp`, pulls Y/X/A, and `RTI`s into your entry point. You are now running.

From your perspective inside the program: you wake up at `$8008` with zeroed registers and the world is as described in §4 and §7.

When you eventually `SYS_EXIT_PROCESS`: the assembly handler `do_exit_process` decrements `proc_user_count`, clears your slot's `PROC_FLAG_ACTIVE`, finds the next active slot (round-robin), restores that slot's ROM bank and SP, and `RTI`s into it. Control never returns to your code.