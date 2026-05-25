## 8. Syscall calling convention

Source: [src/nmi.asm](src/nmi.asm) (`irq_handler`), [programs/include/os_abi.inc](programs/include/os_abi.inc).

To call a syscall:

1. Write the syscall number to `SC_NUM` (`$1A`).
2. Write any input parameters to `SC_P0` (`$1B`), `SC_P1` (`$1C`), … up to `SC_P5` (`$20`).
3. Execute `brk #0`. (`brk` is a 2-byte instruction on the 6502; the operand byte is ignored by the hardware but conventionally `0`.)
4. After return, read outputs from `SC_RV0` (`$21`), `SC_RV1`, `SC_RV2`, `SC_RV3` (`$24`) as documented for the syscall.

The kernel's `irq_handler` distinguishes BRK from a hardware IRQ by checking the B flag in the stacked P register. On a BRK it dispatches by `SC_NUM` and returns via `RTI`. A/X/Y are pushed and pulled by the dispatcher, so your A/X/Y are preserved across the call. **There is no kernel hardware stack — the BRK dispatcher and every syscall handler run on your process's stack.** `SC_NUM` and the `SC_P*` slots are caller-owned ZP — they keep whatever you wrote unless a particular syscall is documented as clobbering them (`SYS_START_PROCESS` reuses `SC_P*` internally to call `fs_open`/`fs_read`, so treat its `SC_P*` as clobbered).

Hardware-stack budget per syscall is **6 bytes minimum**: 3 for the CPU-pushed BRK frame (PCH, PCL, P) plus 3 for the dispatcher's A/X/Y pushes — and on top of that the handler itself nests `jsr` calls (the C handlers can easily go 3–6 deep). With only 64 bytes of stack per slot, deep `jsr` chains that BRK inside leaf functions can run out of stack mid-syscall. Keep call depth shallow before invoking syscalls.
