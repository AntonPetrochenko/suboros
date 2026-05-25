## 2. Toolchain & build

Source: [programs/build.bat](programs/build.bat).

`programs/build.bat` discovers every subdirectory of `programs/` except `include/` and, for each subdirectory `<name>/`, expects:

- `programs/<name>/main.asm` — the source file
- `programs/<name>/<name>.cfg` — an `ld65` linker config

It runs:

```
ca65 -I programs/include -o build/prg/<name>.o  programs/<name>/main.asm
ld65 -C programs/<name>/<name>.cfg -o build/prg/<name>.prg  build/prg/<name>.o
copy build/prg/<name>.prg  in_fs/0/<name>.prg
```

The result is picked up by the top-level `build.bat` → `tools/mkfs.py` step and packed into the ROM image.

**Hard build-time rule:** the script refuses to build any `.cfg` file that contains a `BSS` segment, with the message:

```
ERROR: <cfg> contains a BSS segment - user programs must not occupy OS RAM.
```

That's the build-side enforcement of the "no static RAM" rule (see §5).