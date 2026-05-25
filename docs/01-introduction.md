# Writing user programs for suboros

This document describes the contract a user program (`.prg`) must honor to be loaded and run by the suboros kernel. It is the only place where the public ABI for user code is collected; the authoritative source files are listed under each section so the doc can be cross-checked when something changes.

Current status (2026-05-25): user programs can be written in **ca65 assembly only**. There is no C toolchain target for user code yet — the cc65 runtime, its zero-page, and BSS are reserved entirely for the kernel.

## 1. What you're building

A user program is a single `.prg` file whose contents live in one 16 KB MMC1 PRG bank, mapped at `$8000–$BFFF` when the program is the running process. The kernel always occupies the fixed bank at `$C000–$FFFF`, so syscalls are reachable no matter which user bank is currently mapped at `$8000`.

The pipeline that turns your source into a running process is:

```
programs/<name>/main.asm                --ca65-->  <name>.o
programs/<name>/<name>.cfg              --ld65-->  build/prg/<name>.prg
                                        --copy-->  in_fs/0/<name>.prg
in_fs/0/                       --tools/mkfs.py-->  ROM filesystem (banks 0..6 + TOC in bank 7)
ROM cartridge image        --emulator/hardware-->  suboros
SC_START_PROCESS(slot=0, "name.prg")  --kernel-->  running process
```

`tools/mkprg.py` exists to wrap a raw assembled binary with the 8-byte PRG header; in practice the macro `PRG_HEADER_ROM` (see §3) embeds the header inside the binary, so `mkprg.py` is not currently invoked by `programs/build.bat`.
