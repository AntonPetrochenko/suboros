## 6. Linker config

Source: [programs/hello/hello.cfg](programs/hello/hello.cfg).

Copy this verbatim for new programs and rename only the file:

```
MEMORY {
    PRG:  start = $8000, size = $4000, fill = yes, fillval = $FF, file = %O;
    ZP:   start = $00,   size = $100,  type = rw,  file = "";
}
SEGMENTS {
    CODE:     load = PRG, type = ro;
    RODATA:   load = PRG, type = ro, optional = yes;
    ZEROPAGE: load = ZP,  type = zp, optional = yes;
}
```

What each part does:

- `PRG` is the single 16 KB MMC1 PRG bank, filled to `$FF` so trailing space is padded predictably. `%O` is the output file.
- `ZP` is a dummy MEMORY area (`file = ""`, not emitted) that exists only so any ZP symbols referenced by your assembly resolve cleanly during linking.
- `CODE` and `RODATA` both go into `PRG`.
- `ZEROPAGE` maps to the dummy `ZP` area, marked `optional` so you can omit it entirely.

You will notice this config has **no `BSS` segment**. Adding one is a build-time error (§2). If you need mutable storage, allocate it via `SYS_ALLOC` at runtime.

You can declare local labels in your `ZEROPAGE` segment if you wish, but only `$2A–$2F` (the user-scratch window) is safe to actually use; the linker won't enforce that. The simplest and recommended pattern is to skip `ZEROPAGE` entirely and reference `$2A`–`$2F` as bare hexadecimal addresses, as in `programs/hello/main.asm`.
